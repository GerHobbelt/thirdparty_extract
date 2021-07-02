#include "../include/extract.h"
#include "../include/extract_alloc.h"

#include "astring.h"
#include "document.h"
#include "mem.h"
#include "outf.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>


static char_t* span_char_first(span_t* span)
{
    assert(span->chars_num > 0);
    return &span->chars[0];
}

/* Returns first char_t in a line. */
static char_t* line_item_first(line_t* line)
{
    span_t* span = line_span_first(line);
    return span_char_first(span);
}

/* Returns last char_t in a line. */
static char_t* line_item_last(line_t* line)
{
    span_t* span = line_span_last(line);
    return span_char_last(span);
}

static point_t char_to_point(const char_t* char_)
{
    point_t ret;
    ret.x = char_->x;
    ret.y = char_->y;
    return ret;
}

static const char* matrix_string(const matrix_t* matrix)
{
    static char ret[64];
    snprintf(ret, sizeof(ret), "{%f %f %f %f %f %f}",
            matrix->a,
            matrix->b,
            matrix->c,
            matrix->d,
            matrix->e,
            matrix->f
            );
    return ret;
}

/* Returns total width of span. */
static double span_adv_total(span_t* span)
{
    double dx = span_char_last(span)->x - span_char_first(span)->x;
    double dy = span_char_last(span)->y - span_char_first(span)->y;
    /* We add on the advance of the last item; this avoids us returning zero if
    there's only one item. */
    double adv = span_char_last(span)->adv * matrix_expansion(span->trm);
    return sqrt(dx*dx + dy*dy) + adv;
}

/* Returns distance between end of <a> and beginning of <b>. */
static double spans_adv(
        span_t* a_span,
        char_t* a,
        char_t* b
        )
{
    double delta_x = b->x - a->x;
    double delta_y = b->y - a->y;
    double s = sqrt( delta_x*delta_x + delta_y*delta_y);
    double a_size = a->adv * matrix_expansion(a_span->trm);
    s -= a_size;
    return s;
}

static double span_angle(span_t* span)
{
    /* Assume ctm is a rotation matix. */
    double ret = atan2(-span->ctm.c, span->ctm.a);
    outfx("ctm.a=%f ctm.b=%f ret=%f", span->ctm.a, span->ctm.b, ret);
    return ret;
    /* Not sure whether this is right. Inclined text seems to be done by
    setting the ctm matrix, so not really sure what trm matrix does. This code
    assumes that it also inclines text, but maybe it only rotates individual
    glyphs? */
    /*if (span->wmode == 0) {
        return atan2(span->trm.b, span->trm.a);
    }
    else {
        return atan2(span->trm.d, span->trm.c);
    }*/
}

/* Returns static string containing brief info about span_t. */
static const char* span_string2(extract_alloc_t* alloc, span_t* span)
{
    static extract_astring_t ret = {0};
    int i;
    extract_astring_free(alloc, &ret);
    extract_astring_catc(alloc, &ret, '"');
    for (i=0; i<span->chars_num; ++i) {
        extract_astring_catc(alloc, &ret, (char) span->chars[i].ucs);
    }
    extract_astring_catc(alloc, &ret, '"');
    return ret.chars;
}

/* Returns angle of <line>. */
static double line_angle(line_t* line)
{
    /* All spans in a line must have same angle, so just use the first span. */
    assert(line->spans_num > 0);
    return span_angle(line->spans[0]);
}

/* Returns static string containing brief info about line_t. */
static const char* line_string2(extract_alloc_t* alloc, line_t* line)
{
    static extract_astring_t ret = {0};
    char    buffer[256];
    int i;
    extract_astring_free(alloc, &ret);
    snprintf(buffer, sizeof(buffer), "line x=%f y=%f spans_num=%i:",
            line->spans[0]->chars[0].x,
            line->spans[0]->chars[0].y,
            line->spans_num
            );
    extract_astring_cat(alloc, &ret, buffer);
    for (i=0; i<line->spans_num; ++i) {
        extract_astring_cat(alloc, &ret, " ");
        extract_astring_cat(alloc, &ret, span_string2(alloc, line->spans[i]));
    }
    return ret.chars;
}

/* Array of pointers to lines that are aligned and adjacent to each other so as
to form a paragraph. */
static const char* paragraph_string(extract_alloc_t* alloc, paragraph_t* paragraph)
{
    static extract_astring_t ret = {0};
    extract_astring_free(alloc, &ret);
    extract_astring_cat(alloc, &ret, "paragraph: ");
    if (paragraph->lines_num) {
        extract_astring_cat(alloc, &ret, line_string2(alloc, paragraph->lines[0]));
        if (paragraph->lines_num > 1) {
            extract_astring_cat(alloc, &ret, "..");
            extract_astring_cat(
                    alloc,
                    &ret,
                    line_string2(alloc, paragraph->lines[paragraph->lines_num-1])
                    );
        }
    }
    return ret.chars;
}

/* Returns first line in paragraph. */
static line_t* paragraph_line_first(const paragraph_t* paragraph)
{
    assert(paragraph->lines_num);
    return paragraph->lines[0];
}

/* Returns last line in paragraph. */
static line_t* paragraph_line_last(const paragraph_t* paragraph)
{
    assert(paragraph->lines_num);
    return paragraph->lines[ paragraph->lines_num-1];
}



/* Things for direct conversion of text spans into lines and paragraphs. */

/* Returns 1 if lines have same wmode and are at the same angle, else 0.

todo: allow small epsilon? */
static int lines_are_compatible(
        line_t* a,
        line_t* b,
        double  angle_a,
        int     verbose
        )
{
    if (a == b) return 0;
    if (!a->spans || !b->spans) return 0;
    if (line_span_first(a)->wmode != line_span_first(b)->wmode) {
        return 0;
    }
    if (matrix_cmp4(
            &line_span_first(a)->ctm,
            &line_span_first(b)->ctm
            )) {
        if (verbose) {
            outf("ctm's differ:");
            outf("    %f %f %f %f %f %f",
                    line_span_first(a)->ctm.a,
                    line_span_first(a)->ctm.b,
                    line_span_first(a)->ctm.c,
                    line_span_first(a)->ctm.d,
                    line_span_first(a)->ctm.e,
                    line_span_first(a)->ctm.f
                    );
            outf("    %f %f %f %f %f %f",
                    line_span_first(b)->ctm.a,
                    line_span_first(b)->ctm.b,
                    line_span_first(b)->ctm.c,
                    line_span_first(b)->ctm.d,
                    line_span_first(b)->ctm.e,
                    line_span_first(b)->ctm.f
                    );
        }
        return 0;
    }
    {
        double angle_b = span_angle(line_span_first(b));
        if (angle_b != angle_a) {
            outfx("%s:%i: angles differ");
            return 0;
        }
    }
    return 1;
}

#if 0
static int s_span_inside_rects(extract_alloc_t* alloc, span_t* span, rect_t* rects, int rects_num)
{
    int i;
    point_t p = {span->chars[0].x, span->chars[0].y};
    if (!rects_num) return 1;
    
    for (i=0; i<rects_num; ++i)
    {
        rect_t* rect = &rects[i];
        if (0
                || p.x < rect->min.x
                || p.x >= rect->max.x
                || p.y < rect->min.y
                || p.y >= rect->max.y
                )
        {
            outf("span ctm=(%f %f) trm=(%f %f) p=%s not inside rect %s",
                    span->ctm.e,
                    span->ctm.f,
                    span->trm.e,
                    span->trm.f,
                    point_string(&p),
                    rect_string(rect)
                    );
            return 0;
        }
    }
    outf0("span ctm=(%f %f) trm=(%f %f) p=%s is inside rect %s: %s",
            span->ctm.e,
            span->ctm.f,
            span->trm.e,
            span->trm.f,
            point_string(&p),
            rect_string(&rects[0]),
            span_string(alloc, span)
            );
    return 1;
}
#endif

static const unsigned ucs_NONE = ((unsigned) -1);

static int s_span_inside_rects2(
        extract_alloc_t* alloc,
        span_t* span,
        rect_t* rects,
        int rects_num,
        span_t* o_span
        )
/* Returns with <o_span> containing char_t's from <span> that are inside
rects[], and *span modified to remove any char_t's that we have moved to
<o_span>.

May return with span->chars_num == 0, in which case the caller must remove the
span, because lots of code assumes that there are no empty spans. */
{
    int c;
    *o_span = *span;
    o_span->chars = NULL;
    o_span->chars_num = 0;
    for (c=0; c<span->chars_num; ++c)
    {
        /* For now we just look at whether span's (x, y) is within any
        rects[]. We could instead try to find character's bounding box etc. */
        char_t* char_ = &span->chars[c];
        int r;
        for (r=0; r<rects_num; ++r)
        {
            rect_t* rect = &rects[r];
            if (1
                    && char_->x >= rect->min.x
                    && char_->x < rect->max.x
                    && char_->y >= rect->min.y
                    && char_->y < rect->max.y
                    )
            {
                if (span_append_c(alloc, o_span, char_->ucs))   return -1;
                *span_char_last(o_span) = *char_;
                char_->ucs = ucs_NONE; /* Mark for removal below, so it is not used again. */
                break;
            }
        }
    }

    /* Remove any char_t's that we've used. */
    {
        int cc = 0;
        for (c=0; c<span->chars_num; ++c)
        {
            char_t* char_ = &span->chars[c];
            if (char_->ucs != ucs_NONE)
            {
                span->chars[cc] = span->chars[c];
                cc += 1;
            }
        }
        /* This might set span->chars_num to zero; our caller needs to remove
        the span - lots of code assumes that all spans contain at least one
        character. */
        span->chars_num = cc;
    }

    if (o_span->chars_num)
    {
        //outf0("  span: %s", span_string(alloc, span));
        outf("o_span: %s", span_string(alloc, o_span));
    }
    return 0;
}

/* Creates representation of span_t's that consists of a list of line_t's, with
each line_t contains pointers to a list of span_t's.

We only join spans that are at the same angle and are aligned.

On entry:
    Original value of *o_lines and *o_lines_num are ignored.

    <spans> points to array of <spans_num> span_t*'s, each pointing to
    a span_t.

On exit:
    If we succeed, we return 0, with *o_lines pointing to array of *o_lines_num
    line_t*'s, each pointing to a line_t.
    
    If <rects_num> is zero, each of these line_t's will contain pointers to
    items in <spans>; otherwise each of the line_t's will contain new spans
    which should be freed by the caller (spans are not necessarily wholy inside
    or outside rects[] so we need to create new spams).

    Otherwise we return -1 with errno set. *o_lines and *o_lines_num are
    undefined.
*/
static int make_lines(
        extract_alloc_t*    alloc,
        span_t**            spans,
        int*                spans_num,
        rect_t*             rects,
        int                 rects_num,
        line_t***           o_lines,
        int*                o_lines_num
        )
{
    int ret = -1;

    /* Make an line_t for each span. Then we will join some of these
    line_t's together before returning. */
    int         lines_num = 0;
    line_t**    lines = NULL;
    int         a;
    int         num_compatible;
    int         num_joins;
    
    if (rects_num)
    {
        /* Make <lines> contain new span_t's and char_t's that are inside rects[]. */
        for (a=0; a<*spans_num; ++a)
        {
            span_t* span;
            if (spans[a]->chars_num == 0)   continue; /* In case used for table, */
            if (extract_malloc(alloc, &span, sizeof(*span))) return -1;
            span->chars = NULL;
            span->chars_num = 0;
            if (s_span_inside_rects2(alloc, spans[a], rects, rects_num, span)) return -1;
            if (span->chars_num)
            {
                if (extract_realloc(alloc, &lines, sizeof(*lines) * (lines_num + 1))) goto end;
                if (extract_malloc(alloc, &lines[lines_num], sizeof(line_t))) goto end;
                lines_num += 1;
                if (extract_malloc(alloc, &lines[lines_num-1]->spans, sizeof(span_t*) * 1)) goto end;
                lines[lines_num-1]->spans[0] = span;
                lines[lines_num-1]->spans_num = 1;
            }
            else
            {
                extract_free(alloc, &span);
            }
            
            if (!spans[a]->chars_num)
            {
                /* All characters in this span are inside table, so remove
                entire span, otherwise the same characters will end up being
                output outside the table also. */
                memmove(&spans[a], &spans[a+1], sizeof(*spans) * ((*spans_num) - (a+1)));
                *spans_num -= 1;
                a -= 1;
            }
        }
    }
    else
    {
        /* Make <lines> be a copy of <spans>. */
        lines_num = *spans_num;
        if (extract_malloc(alloc, &lines, sizeof(*lines) * lines_num)) goto end;

        /* Ensure we can clean up after error. */
        for (a=0; a<lines_num; ++a) {
            lines[a] = NULL;
        }
        for (a=0; a<lines_num; ++a) {
            if (extract_malloc(alloc, &lines[a], sizeof(line_t))) goto end;
            lines[a]->spans_num = 0;
            if (extract_malloc(alloc, &lines[a]->spans, sizeof(span_t*) * 1)) goto end;
            lines[a]->spans_num = 1;
            lines[a]->spans[0] = spans[a];
            outfx("initial line a=%i: %s", a, line_string(lines[a]));
        }
    }
    
    num_compatible = 0;

    /* For each line, look for nearest aligned line, and append if found. */
    num_joins = 0;
    for (a=0; a<lines_num; ++a) {
        int b;
        int verbose = 0;
        int nearest_line_b = -1;
        double nearest_adv = 0;
        line_t* nearest_line = NULL;
        span_t* span_a;
        double angle_a;
        
        line_t* line_a = lines[a];
        if (!line_a) {
            continue;
        }

        if (0 && a < 1) verbose = 1;
        outfx("looking at line_a=%s", line_string2(alloc, line_a));

        span_a = line_span_last(line_a);
        angle_a = span_angle(span_a);
        if (verbose) outf("a=%i angle_a=%f ctm=%s: %s",
                a,
                angle_a * 180/pi,
                matrix_string(&span_a->ctm),
                line_string2(alloc, line_a)
                );

        for (b=0; b<lines_num; ++b) {
            line_t* line_b = lines[b];
            if (!line_b) {
                continue;
            }
            if (b == a) {
                continue;
            }
            if (verbose) {
                outf("");
                outf("a=%i b=%i: nearest_line_b=%i nearest_adv=%f",
                        a,
                        b,
                        nearest_line_b,
                        nearest_adv
                        );
                outf("    line_a=%s", line_string2(alloc, line_a));
                outf("    line_b=%s", line_string2(alloc, line_b));
            }
            if (!lines_are_compatible(line_a, line_b, angle_a, 0*verbose)) {
                if (verbose) outf("not compatible");
                continue;
            }

            num_compatible += 1;
            {
                /* Find angle between last glyph of span_a and first glyph of
                span_b. This detects whether the lines are lined up with each other
                (as opposed to being at the same angle but in different lines). */
                span_t* span_b = line_span_first(line_b);
                double dx = span_char_first(span_b)->x - span_char_last(span_a)->x;
                double dy = span_char_first(span_b)->y - span_char_last(span_a)->y;
                double angle_a_b = atan2(-dy, dx);
                const double angle_tolerance_deg = 1;
                if (verbose) {
                    outf("delta=(%f %f) alast=(%f %f) bfirst=(%f %f): angle_a=%f angle_a_b=%f",
                            dx,
                            dy,
                            span_char_last(span_a)->x,
                            span_char_last(span_a)->y,
                            span_char_first(span_b)->x,
                            span_char_first(span_b)->y,
                            angle_a * 180 / pi,
                            angle_a_b * 180 / pi
                            );
                }
                /* Might want to relax this when we test on non-horizontal lines.
                */
                if (fabs(angle_a_b - angle_a) * 180 / pi <= angle_tolerance_deg) {
                    /* Find distance between end of line_a and beginning of line_b. */
                    double adv = spans_adv(
                            span_a,
                            span_char_last(span_a),
                            span_char_first(span_b)
                            );
                    if (verbose) outf("nearest_adv=%f. angle_a_b=%f adv=%f",
                            nearest_adv,
                            angle_a_b,
                            adv
                            );
                    if (!nearest_line || adv < nearest_adv) {
                        nearest_line = line_b;
                        nearest_adv = adv;
                        nearest_line_b = b;
                    }
                }
                else {
                    if (verbose) outf(
                            "angle beyond tolerance: span_a last=(%f,%f) span_b first=(%f,%f) angle_a_b=%g angle_a=%g span_a.trm{a=%f b=%f}",
                            span_char_last(span_a)->x,
                            span_char_last(span_a)->y,
                            span_char_first(span_b)->x,
                            span_char_first(span_b)->y,
                            angle_a_b * 180 / pi,
                            angle_a * 180 / pi,
                            span_a->trm.a,
                            span_a->trm.b
                            );
                }
            }
        }

        if (nearest_line) {
            /* line_a and nearest_line are aligned so we can move line_b's
            spans on to the end of line_a. */
            double average_adv;
            span_t* span_b = line_span_first(nearest_line);
            b = nearest_line_b;
            if (verbose) outf("found nearest line. a=%i b=%i", a, b);

            /* Find average advance of the two adjacent spans in the two
            lines we are considering joining, so that we can decide whether
            the distance between them is large enough to merit joining with
            a space character). */
            average_adv = (
                    (span_adv_total(span_a) + span_adv_total(span_b))
                    /
                    (double) (span_a->chars_num + span_b->chars_num)
                    );

            if (0 && nearest_adv > 5 * average_adv)
            {
                continue;
            }
            
            if (1
                    && span_char_last(span_a)->ucs != ' '
                    && span_char_first(span_b)->ucs != ' '
                    ) {
                int insert_space = (nearest_adv > 0.25 * average_adv);
                if (insert_space) {
                    /* Append space to span_a before concatenation. */
                    char_t* item;
                    if (verbose) {
                        outf("(inserted space) nearest_adv=%f average_adv=%f",
                                nearest_adv,
                                average_adv
                                );
                        outf("    a: %s", span_string(alloc, span_a));
                        outf("    b: %s", span_string(alloc, span_b));
                    }
                    if (extract_realloc2(
                            alloc,
                            &span_a->chars,
                            sizeof(char_t) * span_a->chars_num,
                            sizeof(char_t) * (span_a->chars_num + 1)
                            )) goto end;
                    item = &span_a->chars[span_a->chars_num];
                    span_a->chars_num += 1;
                    extract_bzero(item, sizeof(*item));
                    item->ucs = ' ';
                    item->adv = nearest_adv;
                }

                if (verbose) {
                    outf("Joining spans a=%i b=%i:", a, b);
                    outf("    %s", span_string2(alloc, span_a));
                    outf("    %s", span_string2(alloc, span_b));
                }
                if (0) {
                    /* Show details about what we're joining. */
                    outf(
                            "joining line insert_space=%i a=%i (y=%f) to line b=%i (y=%f). nearest_adv=%f average_adv=%f",
                            insert_space,
                            a,
                            span_char_last(span_a)->y,
                            b,
                            span_char_first(span_b)->y,
                            nearest_adv,
                            average_adv
                            );
                    outf("a: %s", span_string(alloc, span_a));
                    outf("b: %s", span_string(alloc, span_b));
                }
            }

            /* We might end up with two adjacent spaces here. But removing a
            space could result in an empty line_t, which could break various
            assumptions elsewhere. */

            if (verbose) {
                outf("Joining spans a=%i b=%i:", a, b);
                outf("    %s", span_string2(alloc, span_a));
                outf("    %s", span_string2(alloc, span_b));
            }
            if (extract_realloc2(
                    alloc,
                    &line_a->spans,
                    sizeof(span_t*) * line_a->spans_num,
                    sizeof(span_t*) * (line_a->spans_num + nearest_line->spans_num)
                    )) goto end;
            {
                int k;
                for (k=0; k<nearest_line->spans_num; ++k) {
                    line_a->spans[ line_a->spans_num + k] = nearest_line->spans[k];
                }
            }
            line_a->spans_num += nearest_line->spans_num;

            /* Ensure that we ignore nearest_line from now on. */
            extract_free(alloc, &nearest_line->spans);
            extract_free(alloc, &nearest_line);
            outfx("setting line[b=%i] to NULL", b);
            lines[b] = NULL;

            num_joins += 1;

            if (b > a) {
                /* We haven't yet tried appending any spans to nearest_line, so
                the new extended line_a needs checking again. */
                a -= 1;
            }
            outfx("new line is:\n    %s", line_string2(line_a));
        }
    }

    {
        /* Remove empty lines left behind after we appended pairs of lines. */
        int from;
        int to;
        int lines_num_old;
        for (from=0, to=0; from<lines_num; ++from) {
            if (lines[from]) {
                outfx("final line from=%i: %s",
                        from,
                        lines[from] ? line_string(lines[from]) : "NULL"
                        );
                lines[to] = lines[from];
                to += 1;
            }
        }
        lines_num_old = lines_num;
        lines_num = to;
        if (extract_realloc2(
                alloc,
                &lines,
                sizeof(line_t*) * lines_num_old,
                sizeof(line_t*) * lines_num
                )) {
            /* Should always succeed because we're not increasing allocation size. */
            goto end;
        }
    }

    *o_lines = lines;
    *o_lines_num = lines_num;
    ret = 0;

    outf("Turned %i spans into %i lines. num_compatible=%i",
            spans_num,
            lines_num,
            num_compatible
            );

    end:
    if (ret) {
        /* Free everything. */
        if (lines) {
            for (a=0; a<lines_num; ++a) {
                if (lines[a])   extract_free(alloc, &lines[a]->spans);
                extract_free(alloc, &lines[a]);
            }
        }
        extract_free(alloc, &lines);
    }
    return ret;
}


/* Returns max font size of all span_t's in an line_t. */
static double line_font_size_max(line_t* line)
{
    double  size_max = 0;
    int i;
    for (i=0; i<line->spans_num; ++i) {
        span_t* span = line->spans[i];
        /* fixme: <size> should be double, which changes some output. */
        double size = matrix_expansion(span->trm);
        if (size > size_max) {
            size_max = size;
        }
    }
    return size_max;
}



/* Find distance between parallel lines line_a and line_b, both at <angle>.

        _-R
     _-
    A------------_P
     \        _-
      \    _B
       \_-
        Q

A is (ax, ay)
B is (bx, by)
APB and PAR are both <angle>.

AR and QBP are parallel, and are the lines of text a and b
respectively.

AQB is a right angle. We need to find AQ.
*/
static double line_distance_y( double ax, double ay, double bx, double by, double angle)
{
    double dx = bx - ax;
    double dy = by - ay;

    return dx * sin(angle) + dy * cos(angle);
}

/* Returns distance QB in above diagram. */
static double line_distance_x( double ax, double ay, double bx, double by, double angle)
{
    double dx = bx - ax;
    double dy = by - ay;

    return dx * cos(angle) - dy * sin(angle);
}

static double line_distance_xp(point_t a, point_t b, double angle)
{
    return line_distance_x(a.x, a.y, b.x, b.y, angle);
}

static int lines_overlap(point_t a_left, point_t a_right, point_t b_left, point_t b_right, double angle)
{
    if (line_distance_xp(a_left, b_right, angle) < 0)  return 0;
    if (line_distance_xp(a_right, b_left, angle) >= 0) return 0;
    return 1;
}


/* A comparison function for use with qsort(), for sorting paragraphs within a
page. */
static int paragraphs_cmp(const void* a, const void* b)
{
    const paragraph_t* const* a_paragraph = a;
    const paragraph_t* const* b_paragraph = b;
    line_t* a_line = paragraph_line_first(*a_paragraph);
    line_t* b_line = paragraph_line_first(*b_paragraph);

    span_t* a_span = line_span_first(a_line);
    span_t* b_span = line_span_first(b_line);

    /* If ctm matrices differ, always return this diff first. Note that we
    ignore .e and .f because if data is from ghostscript then .e and .f vary
    for each span, and we don't care about these differences. */
    int d = matrix_cmp4(&a_span->ctm, &b_span->ctm);
    if (d)  return d;

    {
        double a_angle = line_angle(a_line);
        double b_angle = line_angle(b_line);
        if (fabs(a_angle - b_angle) > 3.14/2) {
            /* Give up if more than 90 deg. */
            return 0;
        }
        {
            double angle = (a_angle + b_angle) / 2;
            double ax = line_item_first(a_line)->x;
            double ay = line_item_first(a_line)->y;
            double bx = line_item_first(b_line)->x;
            double by = line_item_first(b_line)->y;
            double distance = line_distance_y(ax, ay, bx, by, angle);
            if (distance > 0)   return -1;
            if (distance < 0)   return +1;
        }
    }
    return 0;
}


/* Creates a representation of line_t's that consists of a list of
paragraph_t's.

We only join lines that are at the same angle and are adjacent.

On entry:
    Original value of *o_paragraphs and *o_paragraphs_num are ignored.

    <lines> points to array of <lines_num> line_t*'s, each pointing to
    a line_t.

On exit:
    On sucess, returns zero, *o_paragraphs points to array of *o_paragraphs_num
    paragraph_t*'s, each pointing to an paragraph_t. In the
    array, paragraph_t's with same angle are sorted.

    On failure, returns -1 with errno set. *o_paragraphs and *o_paragraphs_num
    are undefined.
*/
static int make_paragraphs(
        extract_alloc_t*    alloc, 
        line_t**            lines,
        int                 lines_num,
        paragraph_t***      o_paragraphs,
        int*                o_paragraphs_num
        )
{
    int ret = -1;
    int a;
    int num_joins;
    paragraph_t** paragraphs = NULL;

    /* Start off with an paragraph_t for each line_t. */
    int paragraphs_num = lines_num;
    if (extract_malloc(alloc, &paragraphs, sizeof(*paragraphs) * paragraphs_num)) goto end;
    /* Ensure we can clean up after error when setting up. */
    for (a=0; a<paragraphs_num; ++a) {
        paragraphs[a] = NULL;
    }
    /* Set up initial paragraphs. */
    for (a=0; a<paragraphs_num; ++a) {
        if (extract_malloc(alloc, &paragraphs[a], sizeof(paragraph_t))) goto end;
        paragraphs[a]->lines_num = 0;
        if (extract_malloc(alloc, &paragraphs[a]->lines, sizeof(line_t*) * 1)) goto end;
        paragraphs[a]->lines_num = 1;
        paragraphs[a]->lines[0] = lines[a];
    }

    num_joins = 0;
    for (a=0; a<paragraphs_num; ++a) {
        paragraph_t* nearest_paragraph = NULL;
        int nearest_paragraph_b = -1;
        double nearest_paragraph_distance = -1;
        line_t* line_a;
        double angle_a;
        int verbose;
        int b;
        
        paragraph_t* paragraph_a = paragraphs[a];
        if (!paragraph_a) {
            /* This paragraph is empty - already been appended to a different
            paragraph. */
            continue;
        }

        assert(paragraph_a->lines_num > 0);
        line_a = paragraph_line_last(paragraph_a);
        angle_a = line_angle(line_a);
        verbose = 0;

        /* Look for nearest paragraph_t that could be appended to
        paragraph_a. */
        for (b=0; b<paragraphs_num; ++b) {
            paragraph_t* paragraph_b = paragraphs[b];
            line_t* line_b;
            if (!paragraph_b) {
                /* This paragraph is empty - already been appended to a different
                paragraph. */
                continue;
            }
            line_b = paragraph_line_first(paragraph_b);
            if (!lines_are_compatible(line_a, line_b, angle_a, 0)) {
                continue;
            }

            {
                double ax = line_item_last(line_a)->x;
                double ay = line_item_last(line_a)->y;
                double bx = line_item_first(line_b)->x;
                double by = line_item_first(line_b)->y;
                double distance = line_distance_y(ax, ay, bx, by, angle_a);
                if (verbose) {
                    outf(
                            "angle_a=%f a=(%f %f) b=(%f %f) delta=(%f %f) distance=%f:",
                            angle_a * 180 / pi,
                            ax, ay,
                            bx, by,
                            bx - ax,
                            by - ay,
                            distance
                            );
                    outf("    line_a=%s", line_string2(alloc, line_a));
                    outf("    line_b=%s", line_string2(alloc, line_b));
                }
                if (distance > 0)
                {
                    if (nearest_paragraph_distance == -1
                            || distance < nearest_paragraph_distance)
                    {
                        int ok = 1;
                        if (0)
                        {
                            /* Check whether lines overlap horizontally. */
                            point_t a_left = char_to_point(line_item_first(line_a));
                            point_t b_left = char_to_point(line_item_first(line_b));
                            point_t a_right = char_to_point(line_item_last(line_a));
                            point_t b_right = char_to_point(line_item_last(line_b));

                            if (!lines_overlap(a_left, a_right, b_left, b_right, angle_a))
                            {
                                outf0("Not joining lines because not overlapping.");
                                ok = 0;
                            }
                        }

                        if (ok)
                        {
                            if (verbose) {
                                outf("updating nearest. distance=%f:", distance);
                                outf("    line_a=%s", line_string2(alloc, line_a));
                                outf("    line_b=%s", line_string2(alloc, line_b));
                            }

                            nearest_paragraph_distance = distance;
                            nearest_paragraph_b = b;
                            nearest_paragraph = paragraph_b;
                        }
                    }
                }
            }
        }

        if (nearest_paragraph) {
            double line_b_size = line_font_size_max(
                    paragraph_line_first(nearest_paragraph)
                    );
            line_t* line_b = paragraph_line_first(nearest_paragraph);
            (void) line_b; /* Only used in outfx(). */
            if (nearest_paragraph_distance < 1.4 * line_b_size) {
                /* Paragraphs are close together vertically compared to maximum
                font size of first line in second paragraph, so we'll join them
                into a single paragraph. */
                span_t* a_span;
                int a_lines_num_new;
                if (verbose) {
                    outf(
                            "joing paragraphs. a=(%f,%f) b=(%f,%f) nearest_paragraph_distance=%f line_b_size=%f",
                            line_item_last(line_a)->x,
                            line_item_last(line_a)->y,
                            line_item_first(line_b)->x,
                            line_item_first(line_b)->y,
                            nearest_paragraph_distance,
                            line_b_size
                            );
                    outf("    %s", paragraph_string(alloc, paragraph_a));
                    outf("    %s", paragraph_string(alloc, nearest_paragraph));
                    outf("paragraph_a ctm=%s",
                            matrix_string(&paragraph_a->lines[0]->spans[0]->ctm)
                            );
                    outf("paragraph_a trm=%s",
                            matrix_string(&paragraph_a->lines[0]->spans[0]->trm)
                            );
                }
                /* Join these two paragraph_t's. */
                a_span = line_span_last(line_a);
                if (span_char_last(a_span)->ucs == '-') {
                    /* remove trailing '-' at end of prev line. char_t doesn't
                    contain any malloc-heap pointers so this doesn't leak. */
                    a_span->chars_num -= 1;
                }
                else if (span_char_last(a_span)->ucs == ' ')
                {
                }
                else
                {
                    /* Insert space before joining adjacent lines. */
                    char_t* c_prev;
                    char_t* c;
                    if (span_append_c(alloc, line_span_last(line_a), ' ')) goto end;
                    c_prev = &a_span->chars[ a_span->chars_num-2];
                    c = &a_span->chars[ a_span->chars_num-1];
                    c->x = c_prev->x + c_prev->adv * a_span->ctm.a;
                    c->y = c_prev->y + c_prev->adv * a_span->ctm.c;
                }

                a_lines_num_new = paragraph_a->lines_num + nearest_paragraph->lines_num;
                if (extract_realloc2(
                        alloc,
                        &paragraph_a->lines,
                        sizeof(line_t*) * paragraph_a->lines_num,
                        sizeof(line_t*) * a_lines_num_new
                        )) goto end;
                {
                    int i;
                    for (i=0; i<nearest_paragraph->lines_num; ++i) {
                        paragraph_a->lines[paragraph_a->lines_num + i]
                            = nearest_paragraph->lines[i];
                    }
                }
                paragraph_a->lines_num = a_lines_num_new;

                /* Ensure that we skip nearest_paragraph in future. */
                extract_free(alloc, &nearest_paragraph->lines);
                extract_free(alloc, &nearest_paragraph);
                paragraphs[nearest_paragraph_b] = NULL;

                num_joins += 1;
                outfx(
                        "have joined paragraph a=%i to snearest_paragraph_b=%i",
                        a,
                        nearest_paragraph_b
                        );

                if (nearest_paragraph_b > a) {
                    /* We haven't yet tried appending any paragraphs to
                    nearest_paragraph_b, so the new extended paragraph_a needs
                    checking again. */
                    a -= 1;
                }
            }
            else {
                outfx(
                        "Not joining paragraphs. nearest_paragraph_distance=%f line_b_size=%f",
                        nearest_paragraph_distance,
                        line_b_size
                        );
            }
        }
    }

    {
        /* Remove empty paragraphs. */
        int from;
        int to;
        int paragraphs_num_old;
        for (from=0, to=0; from<paragraphs_num; ++from) {
            if (paragraphs[from]) {
                paragraphs[to] = paragraphs[from];
                to += 1;
            }
        }
        outfx("paragraphs_num=%i => %i", paragraphs_num, to);
        paragraphs_num_old = paragraphs_num;
        paragraphs_num = to;
        if (extract_realloc2(
                alloc,
                &paragraphs,
                sizeof(paragraph_t*) * paragraphs_num_old,
                sizeof(paragraph_t*) * paragraphs_num
                )) {
            /* Should always succeed because we're not increasing allocation size, but
            can fail with memento squeeze. */
            goto end;
        }
    }

    /* Sort paragraphs so they appear in correct order, using paragraphs_cmp().
    */
    qsort(
            paragraphs,
            paragraphs_num,
            sizeof(paragraph_t*), paragraphs_cmp
            );

    *o_paragraphs = paragraphs;
    *o_paragraphs_num = paragraphs_num;
    ret = 0;
    outf("Turned %i lines into %i paragraphs",
            lines_num,
            paragraphs_num
            );


    end:

    if (ret) {
        if (paragraphs) {
            for (a=0; a<paragraphs_num; ++a) {
                if (paragraphs[a])   extract_free(alloc, &paragraphs[a]->lines);
                extract_free(alloc, &paragraphs[a]);
            }
        }
        extract_free(alloc, &paragraphs);
    }
    return ret;
}

int extract_document_join_page_rects(
        extract_alloc_t*    alloc,
        page_t*             page,
        rect_t*             rects,
        int                 rects_num,
        line_t***           lines,
        int*                lines_num,
        paragraph_t***      paragraphs,
        int*                paragraphs_num
        )
{
    if (make_lines(
            alloc,
            page->spans,
            &page->spans_num,
            rects,
            rects_num,
            lines,
            lines_num
            )) return -1;

    if (make_paragraphs(
            alloc,
            *lines,
            *lines_num,
            paragraphs,
            paragraphs_num
            )) return -1;
    
    return 0;
}

static int extract_document_join_page(
        extract_alloc_t*    alloc,
        page_t*             page
        )
{
    if (extract_document_join_page_rects(
            alloc,
            page,
            NULL /*rects*/,
            0 /*rects_num*/,
            &page->lines,
            &page->lines_num,
            &page->paragraphs,
            &page->paragraphs_num
            )) return -1;
    
    return 0;
}

int extract_document_join(extract_alloc_t* alloc, document_t* document)
{
    /* For each page in <document> we join spans into lines and paragraphs. A
    line is a list of spans that are at the same angle and on the same line. A
    paragraph is a list of lines that are at the same angle and close together.
    */
    int p;
    for (p=0; p<document->pages_num; ++p) {
        page_t* page = document->pages[p];
        outf("processing page %i: num_spans=%i", p, page->spans_num);

        if (extract_document_join_page(alloc, page)) return -1;
    }

    return 0;
}
