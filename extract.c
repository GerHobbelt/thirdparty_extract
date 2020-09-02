#ifdef MEMENTO
    #include "memento.h"
#endif

#include "extract.h"

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>


/* Simple printf-style debug output. */
static void (outf)(
        const char* file, int line,
        const char* fn,
        int ln,
        const char* format,
        ...
        )
{
    va_list va;
    if (ln) {
        fprintf(stderr, "%s:%i:%s: ", file, line, fn);
    }
    va_start(va, format);
    vfprintf(stderr, format, va);
    va_end(va);
    if (ln) {
        size_t len = strlen(format);
        if (len == 0 || format[len-1] != '\n') {
            fprintf(stderr, "\n");
        }
    }
}

#define outf(format, ...) \
        (outf)(__FILE__, __LINE__, __FUNCTION__, 1 /*ln*/, format, ##__VA_ARGS__)

#define outfx(format, ...)

static const float g_pi = 3.14159265;


/* These local_*() functions should be used to ensure that Memento works. */

static char* local_strdup(const char* text)
{
    size_t l = strlen(text) + 1;
    char* ret = malloc(l);
    if (!ret) return NULL;
    memcpy(ret, text, l);
    return ret;
}


static int local_vasprintf(char** out, const char* format, va_list va0)
{
    va_list va;

    /* Find required length. */
    va_copy(va, va0);
    int len = vsnprintf(NULL, 0, format, va);
    va_end(va);
    assert(len >= 0);
    len += 1; /* For terminating 0. */

    /* Repeat call of vnsprintf() with required buffer. */
    char* buffer = malloc(len);
    if (!buffer) {
        return -1;
    }
    va_copy(va, va0);
    int len2 = vsnprintf(buffer, len, format, va);
    va_end(va);
    assert(len2 + 1 == len);
    *out = buffer;
    return len2;
}


static int local_asprintf(char** out, const char* format, ...)
{
    va_list va;
    va_start(va, format);
    int ret = local_vasprintf(out, format, va);
    va_end(va);
    return ret;
}


/* These str_*() functions realloc buffer as required. All return 0 or -1 with
errno set. */

/* Appends first <s_len> chars of string <s> to *p. */
static int str_catl(char** p, const char* s, int s_len)
{
    int p_len = (*p) ? strlen(*p) : 0;
    char* pp = realloc(*p, p_len + s_len + 1);
    if (!pp)    return -1;
    memcpy(pp + p_len, s, s_len);
    pp[p_len + s_len] = 0;
    *p = pp;
    return 0;
}

/* Appends a char.  */
static int str_catc(char** p, char c)
{
    return str_catl(p, &c, 1);
}

/* Unused but usefult o keep code here. */
#if 0
/* Appends a string. */
static int str_cat(char** p, const char* s)
{
    return str_catl(p, s, strlen(s));
}
#endif



/* A simple string struct that reallocs as required. */
typedef struct
{
    char*   chars;      /* NULL or zero-terminated. */
    int     chars_num;  /* Length of string pointed to by .chars. */
} string_t;

static void string_init(string_t* string)
{
    string->chars = NULL;
    string->chars_num = 0;
}

static void string_free(string_t* string)
{
    free(string->chars);
    string_init(string);
}

static int string_catl(string_t* string, const char* s, int s_len)
{
    char* chars = realloc(string->chars, string->chars_num + s_len + 1);
    if (!chars) return -1;
    memcpy(chars + string->chars_num, s, s_len);
    chars[string->chars_num + s_len] = 0;
    string->chars = chars;
    string->chars_num += s_len;
    return 0;
}

static int string_catc(string_t* string, char c)
{
    return string_catl(string, &c, 1);
}

static int string_cat(string_t* string, const char* s)
{
    return string_catl(string, s, strlen(s));
}


typedef struct
{
    float   a;
    float   b;
    float   c;
    float   d;
    float   e;
    float   f;
} matrix_t;

static float matrix_expansion(matrix_t m)
{
    return sqrtf(fabsf(m.a * m.d - m.b * m.c));
}

/* Unused but usefult o keep code here. */
#if 0
static void matrix_scale(matrix_t* matrix, float scale)
{
    matrix->a *= scale;
    matrix->b *= scale;
    matrix->c *= scale;
    matrix->d *= scale;
    matrix->e *= scale;
    matrix->f *= scale;
}

static void matrix_scale4(matrix_t* matrix, float scale)
{
    matrix->a *= scale;
    matrix->b *= scale;
    matrix->c *= scale;
    matrix->d *= scale;
}
#endif

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


/* A single char in a span.
*/
typedef struct
{
    /* (x,y) before transformation by ctm and trm. */
    float       pre_x;
    float       pre_y;
    
    /* (x,y) after transformation by ctm and trm. */
    float       x;
    float       y;
    
    int         gid;
    unsigned    ucs;
    float       adv;
} char_t;

static void char_init(char_t* item)
{
    item->pre_x = 0;
    item->pre_y = 0;
    item->x = 0;
    item->y = 0;
    item->gid = 0;
    item->ucs = 0;
    item->adv = 0;
}


/* Array of chars that have same font and are usually adjacent.
*/
typedef struct
{
    matrix_t    ctm;
    matrix_t    trm;
    char*       font_name;
    
    /* font size is matrix_expansion(trm). */
    
    struct {
        int font_bold   : 1;
        int font_italic : 1;
        int wmode       : 1;
    };
    
    char_t*     chars;
    int         chars_num;
} span_t;

/* Returns static string containing info about span_t. */
static const char* span_string(span_t* span)
{
    float x0 = 0;
    float y0 = 0;
    float x1 = 0;
    float y1 = 0;
    int c0 = 0;
    int c1 = 0;
    if (span->chars_num) {
        c0 = span->chars[0].ucs;
        x0 = span->chars[0].x;
        y0 = span->chars[0].y;
        c1 = span->chars[span->chars_num-1].ucs;
        x1 = span->chars[span->chars_num-1].x;
        y1 = span->chars[span->chars_num-1].y;
    }
    static string_t ret = {0};
    string_free(&ret);
    char buffer[200];
    snprintf(buffer, sizeof(buffer),
            "span chars_num=%i (%c:%f,%f)..(%c:%f,%f) font=%s:(%f,%f) wmode=%i chars_num=%i: ",
            span->chars_num,
            c0, x0, y0,
            c1, x1, y1,
            span->font_name,
            span->trm.a,
            span->trm.d,
            span->wmode,
            span->chars_num
            );
    string_cat(&ret, buffer);
    int i;
    for (i=0; i<span->chars_num; ++i) {
        snprintf(
                buffer,
                sizeof(buffer),
                " i=%i {x=%f adv=%f}",
                i,
                span->chars[i].x,
                span->chars[i].adv
                );
        string_cat(&ret, buffer);
    }
    string_cat(&ret, ": ");
    string_catc(&ret, '"');
    for (i=0; i<span->chars_num; ++i) {
        string_catc(&ret, span->chars[i].ucs);
    }
    string_catc(&ret, '"');
    return ret.chars;
}

/* Returns static string containing brief info about span_t. */
static const char* span_string2(span_t* span)
{
    static string_t ret = {0};
    string_free(&ret);
    string_catc(&ret, '"');
    int i;
    for (i=0; i<span->chars_num; ++i) {
        string_catc(&ret, span->chars[i].ucs);
    }
    string_catc(&ret, '"');
    return ret.chars;
}

/* Appends new char_t to an span_t with .ucs=c and all other
fields zeroed. */
static int span_append_c(span_t* span, int c)
{
    char_t* items = realloc(
            span->chars,
            sizeof(*items) * (span->chars_num + 1)
            );
    if (!items) return -1;
    span->chars = items;
    char_t* item = &span->chars[span->chars_num];
    span->chars_num += 1;
    char_init(item);
    item->ucs = c;
    return 0;
}

static char_t* span_char_first(span_t* span)
{
    assert(span->chars_num);
    return &span->chars[0];
}

static char_t* span_char_last(span_t* span)
{
    assert(span->chars_num);
    return &span->chars[span->chars_num-1];
}

static float span_angle(span_t* span)
{
    /* Assume ctm is a rotation matix. */
    float ret = atan2f(-span->ctm.c, span->ctm.a);
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

/* Returns total width of span. */
static float span_adv_total(span_t* span)
{
    float dx = span_char_last(span)->x - span_char_first(span)->x;
    float dy = span_char_last(span)->y - span_char_first(span)->y;
    /* We add on the advance of the last item; this avoids us returning zero if
    there's only one item. */
    float adv = span_char_last(span)->adv * matrix_expansion(span->trm);
    return sqrt(dx*dx + dy*dy) + adv;
}

/* Returns distance between end of <a> and beginning of <b>. */
static float spans_adv(
        span_t* a_span,
        char_t* a,
        char_t* b
        )
{
    float delta_x = b->x - a->x;
    float delta_y = b->y - a->y;
    float s = sqrt( delta_x*delta_x + delta_y*delta_y);
    float a_size = a->adv * matrix_expansion(a_span->trm);
    s -= a_size;
    return s;
}

/* Array of pointers to spans that are aligned on same line.
*/
typedef struct
{
    span_t**    spans;
    int         spans_num;
} line_t;

/* Unused but usefult o keep code here. */
#if 0
/* Returns static string containing info about line_t. */
static const char* line_string(line_t* line)
{
    static string_t ret = {0};
    char    buffer[32];
    string_free(&ret);
    snprintf(buffer, sizeof(buffer), "line spans_num=%i:", line->spans_num);
    string_cat(&ret, buffer);
    int i;
    for (i=0; i<line->spans_num; ++i) {
        string_cat(&ret, " ");
        string_cat(&ret, span_string(line->spans[i]));
    }
    return ret.chars;
}
#endif

/* Returns static string containing brief info about line_t. */
static const char* line_string2(line_t* line)
{
    static string_t ret = {0};
    char    buffer[256];
    string_free(&ret);
    snprintf(buffer, sizeof(buffer), "line x=%f y=%f spans_num=%i:",
            line->spans[0]->chars[0].x,
            line->spans[0]->chars[0].y,
            line->spans_num
            );
    string_cat(&ret, buffer);
    int i;
    for (i=0; i<line->spans_num; ++i) {
        string_cat(&ret, " ");
        string_cat(&ret, span_string2(line->spans[i]));
    }
    return ret.chars;
}

/* Returns first span in a line. */
static span_t* line_span_last(line_t* line)
{
    assert(line->spans_num > 0);
    return line->spans[line->spans_num - 1];
}

/* Returns list span in a line. */
static span_t* line_span_first(line_t* line)
{
    assert(line->spans_num > 0);
    return line->spans[0];
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

/* Returns angle of <line>. */
static float line_angle(line_t* line)
{
    /* All spans in a line must have same angle, so just use the first span. */
    assert(line->spans_num > 0);
    return span_angle(line->spans[0]);
}


/* Array of pointers to lines that are aligned and adjacent to each other so as
to form a paragraph. */
typedef struct
{
    line_t**    lines;
    int         lines_num;
} paragraph_t;

static const char* paragraph_string(paragraph_t* paragraph)
{
    static string_t ret = {0};
    string_free(&ret);
    string_cat(&ret, "paragraph: ");
    if (paragraph->lines_num) {
        string_cat(&ret, line_string2(paragraph->lines[0]));
        if (paragraph->lines_num > 1) {
            string_cat(&ret, "..");
            string_cat(
                    &ret,
                    line_string2(paragraph->lines[paragraph->lines_num-1])
                    );
        }
    }
    return ret.chars;
}

/* Returns first line in paragraph. */
static line_t* paragraph_line_first(
        const paragraph_t* paragraph
        )
{
    assert(paragraph->lines_num);
    return paragraph->lines[0];
}

/* Returns last line in paragraph. */
static line_t* paragraph_line_last(
        const paragraph_t* paragraph
        )
{
    assert(paragraph->lines_num);
    return paragraph->lines[ paragraph->lines_num-1];
}


/* A page. Contains different representations of the same list of spans.
*/
typedef struct
{
    span_t**    spans;
    int         spans_num;

    /* .lines[] refers to items in .spans. */
    line_t**    lines;
    int         lines_num;

    /* .paragraphs[] refers to items in .lines. */
    paragraph_t**   paragraphs;
    int             paragraphs_num;
} page_t;

static void page_init(page_t* page)
{
    page->spans = NULL;
    page->spans_num = 0;
    page->lines = NULL;
    page->lines_num = 0;
    page->paragraphs = NULL;
    page->paragraphs_num = 0;
}

static void page_free(page_t* page)
{
    if (!page) return;

    int s;
    for (s=0; s<page->spans_num; ++s) {
        span_t* span = page->spans[s];
        if (span) {
            free(span->chars);
            free(span->font_name);
        }
        free(span);
    }
    free(page->spans);

    int l;
    for (l=0; l<page->lines_num; ++l) {
        line_t* line = page->lines[l];
        free(line->spans);
        free(line);
        /* We don't free line->spans->chars[] because already freed via
        page->spans. */
    }
    free(page->lines);

    int p;
    for (p=0; p<page->paragraphs_num; ++p) {
        paragraph_t* paragraph = page->paragraphs[p];
        if (paragraph) free(paragraph->lines);
        free(paragraph);
    }
    free(page->paragraphs);
}

/* Appends new empty span_ to an page_t; returns NULL with errno set on error.
*/
static span_t* page_span_append(page_t* page)
{
    span_t* span = malloc(sizeof(*span));
    if (!span) return NULL;
    span->font_name = NULL;
    span->chars = NULL;
    span->chars_num = 0;
    span_t** s = realloc(
            page->spans,
            sizeof(*s) * (page->spans_num + 1)
            );
    if (!s) {
        free(span);
        return NULL;
    }
    page->spans = s;
    page->spans[page->spans_num] = span;
    page->spans_num += 1;
    return span;
}


/* Array of pointers to pages.
*/
struct extract_document_t
{
    page_t**    pages;
    int         pages_num;
};

/* Appends new empty page_t to an extract_document_t; returns NULL with errno
set on error. */
static page_t* document_page_append(extract_document_t* document)
{
    page_t* page = malloc(sizeof(page_t));
    if (!page) return NULL;
    page->spans = NULL;
    page->spans_num = 0;
    page->lines = NULL;
    page->lines_num = 0;
    page->paragraphs = NULL;
    page->paragraphs_num = 0;
    page_t** pages = realloc(
            document->pages,
            sizeof(page_t*) * (document->pages_num + 1)
            );
    if (!pages) {
        free(page);
        return NULL;
    }
    document->pages = pages;
    page_init(page);
    document->pages[document->pages_num] = page;
    document->pages_num += 1;
    return page;
}

void extract_document_free(extract_document_t* document)
{
    int p;
    for (p=0; p<document->pages_num; ++p) {
        page_t* page = document->pages[p];
        page_free(page);
        free(page);
    }
    free(document->pages);
    document->pages = NULL;
    document->pages_num = 0;
}


/* Reads bytes until EOF and returns zero-terminated string in memory allocated
with realloc(). If error, we return NULL with errno set. */
static char* read_all(FILE* in)
{
    char*   ret = NULL;
    int     len = 0;
    size_t  delta = 128;
    for(;;) {
        char* p = realloc(ret, len + delta + 1);
        if (!p) {
            free(ret);
            return NULL;
        }
        ret = p;
        ssize_t n = fread(ret + len, 1 /*size*/, delta /*nmemb*/, in);
        len += n;
        if (feof(in)) {
            ret[len] = 0;
            return ret;
        }
        if (ferror(in)) {
            /* It's weird that fread() and ferror() don't set errno. */
            errno = EIO;
            free(ret);
            return NULL;
        }
    }
}


/* Things for representing XML. */

typedef struct {
    char*   name;
    char*   value;
} xml_attribute_t;

/* Represents a single <...> XML tag plus trailing text. */
typedef struct {
    char*               name;
    xml_attribute_t*    attributes;
    int                 attributes_num;
    string_t    text;
} xml_tag_t;

/* Returns pointer to value of specified attribute, or NULL if not found. */
static char* xml_tag_attributes_find(xml_tag_t* tag, const char* name)
{
    for (int i=0; i<tag->attributes_num; ++i) {
        if (!strcmp(tag->attributes[i].name, name)) {
            char* ret = tag->attributes[i].value;
            return ret;
        }
    }
    outf("Failed to find attribute '%s'",name);
    return NULL;
}

/* Finds float value of specified attribute, returning error if not found. We
use atof() and don't check for non-numeric attribute value. */
static int xml_tag_attributes_find_float(
        xml_tag_t*  tag,
        const char* name,
        float*      o_out
        )
{
    const char* value = xml_tag_attributes_find(tag, name);
    if (!value) {
        errno = ESRCH;
        return -1;
    }
    *o_out = atof(value);
    return 0;
}

static int xml_tag_attributes_find_int(
        xml_tag_t*  tag,
        const char* name,
        int*        o_out
        )
{
    const char* value = xml_tag_attributes_find(tag, name);
    if (!value) {
        errno = ESRCH;
        return -1;
    }
    *o_out = atoi(value);
    return 0;
}

static int xml_tag_attributes_append(xml_tag_t* tag, char* name, char* value)
{
    xml_attribute_t* a = realloc(
            tag->attributes,
            (tag->attributes_num+1) * sizeof(xml_attribute_t)
            );
    if (!a) return -1;
    tag->attributes = a;
    tag->attributes[tag->attributes_num].name = name;
    tag->attributes[tag->attributes_num].value = value;
    tag->attributes_num += 1;
    return 0;
}

/* Sets all fields to NULL, so will cause memory leaks if fields have not been
freed. */
static void xml_tag_init(xml_tag_t* tag)
{
    tag->name = NULL;
    tag->attributes = NULL;
    tag->attributes_num = 0;
    string_init(&tag->text);
}

static void xml_tag_free(xml_tag_t* tag)
{
    free(tag->name);
    int i;
    for (i=0; i<tag->attributes_num; ++i) {
        xml_attribute_t* attribute = &tag->attributes[i];
        free(attribute->name);
        free(attribute->value);
    }
    free(tag->attributes);
    string_free(&tag->text);
    xml_tag_init(tag);
}

/* Unused but useful to keep code here. */
#if 0
/* Like strcmp() but also handles NULL. */
static int xml_strcmp_null(const char* a, const char* b)
{
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    return strcmp(a, b);
}
#endif

/* Unused but usefult o keep code here. */
#if 0
/* Compares tag name, then attributes; returns -1, 0 or +1. Does not compare
xml_tag_t::text members. */
static int xml_compare_tags(const xml_tag_t* lhs, const xml_tag_t* rhs)
{
    int d;
    d = xml_strcmp_null(lhs->name, rhs->name);
    if (d)  return d;
    for(int i=0;; ++i) {
        if (i >= lhs->attributes_num || i >= rhs->attributes_num) {
            break;
        }
        const xml_attribute_t* lhs_attribute = &lhs->attributes[i];
        const xml_attribute_t* rhs_attribute = &rhs->attributes[i];
        d = xml_strcmp_null(lhs_attribute->name, rhs_attribute->name);
        if (d)  return d;
        d = xml_strcmp_null(lhs_attribute->value, rhs_attribute->value);
        if (d)  return d;
    }
    if (lhs->attributes_num > rhs->attributes_num) return +1;
    if (lhs->attributes_num < rhs->attributes_num) return -1;
    return 0;
}
#endif


/* xml_pparse_*(): simple XML 'pull' parser.

xml_pparse_init() merely consumes the initial '<'. Thereafter xml_pparse_next()
consumes the next '<' before returning the previous tag. */

/* Opens specified file.

If first_line is not NULL, we check that it matches the first line in the file.

Returns NULL with errno set if error. */
static FILE* xml_pparse_init(const char* path, const char* first_line)
{
    FILE* in = NULL;
    char* buffer = NULL;
    int e = 1;

    in = fopen(path, "r");
    if (!in) {
        outf("error: Could not open filename=%s", path);
        goto end;
    }

    if (first_line) {
        size_t first_line_len = strlen(first_line);
        buffer = malloc(first_line_len + 1);
        if (!buffer) goto end;

        ssize_t n = fread(buffer, first_line_len, 1 /*nmemb*/, in);
        if (n != 1) {
            outf("error: fread() failed. n=%zi. path='%s'", n, path);
            goto end;
        }
        buffer[first_line_len] = 0;
        if (strcmp(first_line, buffer)) {
            outf("Unrecognised prefix in path=%s: %s", path, buffer);
            errno = ESRCH;
            goto end;
        }
    }

    {
        int c = getc(in);
        if (c != '<') {
            outf("Expected '<' but found c=%i", c);
            goto end;
        }
    }
    e = 0;

    end:
    free(buffer);
    if (e) {
        if (in) {
            fclose(in);
            in = NULL;
        }
    }
    return in;
}

/* Returns the next XML tag.

Returns 0 with *out containing next tag; or -1 with errno set if error; or +1
with errno=ESRCH if EOF.

*out is initially passed to xml_tag_free(), so *out must have been initialised,
e.g. by by xml_tag_init(). */
static int xml_pparse_next(FILE* in, xml_tag_t* out)
{
    int ret = -1;
    xml_tag_free(out);

    char*   attribute_name = NULL;
    char*   attribute_value = NULL;

    xml_tag_init(out);
    char c;

    assert(in);

    /* Read tag name. */
    int i = 0;
    for( i=0;; ++i) {
        c = getc(in);
        if (c == EOF) {
            if (i == 0 && feof(in)) {
                /* Legitimate EOF. We provide a reasonable errno value if
                caller isn't expecting EOF and doesn't test explicitly for +1.
                */
                ret = +1;
                errno = ESRCH;
            }
            goto end;
        }
        if (c == '>' || c == ' ')  break;
        if (str_catc(&out->name, c)) goto end;
    }
    if (c == ' ') {

        /* Read attributes. */
        for(;;) {

            /* Read attribute name. */
            for(;;) {
                c = getc(in);
                if (c == EOF) {
                    errno = -ESRCH;
                    goto end;
                }
                if (c == '=' || c == '>' || c == ' ') break;
                if (str_catc(&attribute_name, c)) goto end;
            }
            if (c == '>') break;

            if (c == '=') {
                /* Read attribute value. */
                int quote_single = 0;
                int quote_double = 0;
                for(;;) {
                    c = getc(in);
                    if (c == '\'')      quote_single = !quote_single;
                    else if (c == '"')  quote_double = !quote_double;
                    else if (!quote_single && !quote_double
                            && (c == ' ' || c == '/' || c == '>')
                            ) {
                        /* We are at end of attribute value. */
                        break;
                    }
                    else if (c == '\\') {
                        // Escape next character.
                        c = getc(in);
                        if (c == EOF) {
                            errno = ESRCH;
                            goto end;
                        }
                    }
                    if (str_catc(&attribute_value, c)) goto end;
                }

                /* Remove any enclosing quotes. */
                int l = strlen(attribute_value);
                if (l >= 2) {
                    if (
                            (attribute_value[0] == '"' && attribute_value[l-1] == '"')
                            ||
                            (attribute_value[0] == '\'' && attribute_value[l-1] == '\'')
                            ) {
                        memmove(attribute_value, attribute_value+1, l-2);
                        attribute_value[l-2] = 0;
                    }
                }
            }

            if (xml_tag_attributes_append(out, attribute_name, attribute_value)) goto end;
            attribute_name = NULL;
            attribute_value = NULL;
            if (c == '/') c = getc(in);
            if (c == '>') break;
        }
    }

    /* Read plain text until next '<'. */
    for(;;) {
        c = getc(in);
        if (c == '<' || feof(in)) break;
        if (string_catc(&out->text, c)) goto end;
    }

    ret = 0;

    end:

    free(attribute_name);
    free(attribute_value);
    if (ret) {
        xml_tag_free(out);
    }
    return ret;
}

/* Returns +1, 0 or -1 depending on sign of x. */
static int s_sign(float x)
{
    if (x < 0)  return -1;
    if (x > 0)  return +1;
    return 0;
}

/* Unused but usefult o keep code here. */
#if 0
/* Returns zero if *lhs and *rhs are equal, otherwise +/- 1. */
static int matrix_cmp(
        const matrix_t* lhs,
        const matrix_t* rhs
        )
{
    int ret;
    ret = s_sign(lhs->a - rhs->a);  if (ret) return ret;
    ret = s_sign(lhs->b - rhs->b);  if (ret) return ret;
    ret = s_sign(lhs->c - rhs->c);  if (ret) return ret;
    ret = s_sign(lhs->d - rhs->d);  if (ret) return ret;
    ret = s_sign(lhs->e - rhs->e);  if (ret) return ret;
    ret = s_sign(lhs->f - rhs->f);  if (ret) return ret;
    return 0;
}
#endif

/* Returns zero if first four members of *lhs and *rhs are equal, otherwise
+/-1. */
static int matrix_cmp4(
        const matrix_t* lhs,
        const matrix_t* rhs
        )
{
    int ret;
    ret = s_sign(lhs->a - rhs->a);  if (ret) return ret;
    ret = s_sign(lhs->b - rhs->b);  if (ret) return ret;
    ret = s_sign(lhs->c - rhs->c);  if (ret) return ret;
    ret = s_sign(lhs->d - rhs->d);  if (ret) return ret;
    return 0;
}

typedef struct
{
    float x;
    float y;
} point_t;

static point_t multiply_matrix_point(matrix_t m, point_t p)
{
    float x = p.x;
    p.x = m.a * x + m.c * p.y;
    p.y = m.b * x + m.d * p.y;
    return p;
}

static int s_matrix_read(const char* text, matrix_t* matrix)
{
    if (!text) {
        outf("text is NULL in s_matrix_read()");
        errno = EINVAL;
        return -1;
    }
    int n = sscanf(text,
            "%f %f %f %f %f %f",
            &matrix->a,
            &matrix->b,
            &matrix->c,
            &matrix->d,
            &matrix->e,
            &matrix->f
            );
    if (n != 6) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

/* Unused but usefult o keep code here. */
#if 0
/* Like s_matrix_read() but only expects four values, and sets .e and .g to
zero. */
static int s_matrix_read4(const char* text, matrix_t* matrix)
{
    if (!text) {
        outf("text is NULL in s_matrix_read4()");
        errno = EINVAL;
        return -1;
    }
    int n = sscanf(text,
            "%f %f %f %f",
            &matrix->a,
            &matrix->b,
            &matrix->c,
            &matrix->d
            );
    if (n != 4) {
        errno = EINVAL;
        return -1;
    }
    matrix->e = 0;
    matrix->f = 0;
    return 0;
}
#endif


/* These docx_*() functions generate docx content. Caller must call things in a
sensible order to create valid content - e.g. don't call docx_paragraph_start()
twice without intervening call to docx_paragraph_finish(). */

static int docx_paragraph_start(string_t* content)
{
    return string_cat(content, "\n\n<w:p>");
}

static int docx_paragraph_finish(string_t* content)
{
    return string_cat(content, "\n</w:p>");
}

/* Starts a new run. Caller must ensure that docx_run_finish() was called to
terminate any previous run. */
static int docx_run_start(
        string_t* content,
        const char* font_name,
        float font_size,
        int bold,
        int italic
        )
{
    int e = 0;
    if (!e) e = string_cat(content, "\n<w:r><w:rPr><w:rFonts w:ascii=\"");
    if (!e) e = string_cat(content, font_name);
    if (!e) e = string_cat(content, "\" w:hAnsi=\"");
    if (!e) e = string_cat(content, font_name);
    if (!e) e = string_cat(content, "\"/>");
    if (!e && bold) e = string_cat(content, "<w:b/>");
    if (!e && italic) e = string_cat(content, "<w:i/>");
    {
        char   font_size_text[32];
        if (0) font_size = 10;

        if (!e) e = string_cat(content, "<w:sz w:val=\"");
        snprintf(font_size_text, sizeof(font_size_text), "%f", font_size * 2);
        string_cat(content, font_size_text);
        string_cat(content, "\"/>");

        if (!e) e = string_cat(content, "<w:szCs w:val=\"");
        snprintf(font_size_text, sizeof(font_size_text), "%f", font_size * 1.5);
        string_cat(content, font_size_text);
        string_cat(content, "\"/>");
    }
    if (!e) e = string_cat(content, "</w:rPr><w:t xml:space=\"preserve\">");
    assert(!e);
    return e;

}
static int docx_run_finish(string_t* content)
{
    return string_cat(content, "</w:t></w:r>");
}

static int docx_char_append_string(string_t* content, char* text)
{
    return string_cat(content, text);
}

static int docx_char_append_char(string_t* content, char c)
{
    return string_catc(content, c);
}

/* Append an empty paragraph. */
static int docx_paragraph_empty(string_t* content)
{
    int e = -1;
    if (docx_paragraph_start(content)) goto end;
    /* It seems like our choice of font size here doesn't make any difference
    to the ammount of vertical space, unless we include a non-space
    character. Presumably something to do with the styles in the template
    document. */
    if (docx_run_start(
            content,
            "OpenSans",
            10 /*font_size*/,
            0 /*font_bold*/,
            0 /*font_italic*/
            )) goto end;
    //docx_char_append_string(content, "&#160;");   /* &#160; is non-break space. */
    if (docx_run_finish(content)) goto end;
    if (docx_paragraph_finish(content)) goto end;
    e = 0;
    end:
    return e;
}

/* Removes last <len> chars. */
static int docx_char_truncate(string_t* content, int len)
{
    assert(len <= content->chars_num);
    content->chars_num -= len;
    content->chars[content->chars_num] = 0;
    return 0;
}

/* Removes last char if it is <c>. */
static int docx_char_truncate_if(string_t* content, char c)
{
    if (content->chars_num && content->chars[content->chars_num-1] == c) {
        docx_char_truncate(content, 1);
    }
    return 0;
}

/* Like system() but takes printf-style format and args. */
static int systemf(const char* format, ...)
{
    char* command;
    va_list va;
    va_start(va, format);
    int e = local_vasprintf(&command, format, va);
    va_end(va);
    if (e < 0) return e;
    outf("running: %s", command);
    e = system(command);
    free(command);
    return e;
}

int extract_docx_content_to_docx(
        const char* content,
        int         content_length,
        const char* path_template,
        const char* path_out,
        int         preserve_dir
        )
{
    assert(path_out);
    assert(path_template);

    /* This gets set to zero only if everything succeeds. */
    int ret = -1;

    char*   path_tempdir = NULL;
    char*   word_document_xml = NULL;
    char*   original = NULL;
    FILE*   f = NULL;

    int e;

    if (strchr(path_out, '\'')) {
        outf("path_out contains single-quote character: %s", path_out);
        errno = EINVAL;
        goto end;
    }

    if (local_asprintf(&path_tempdir, "%s.dir", path_out) < 0) goto end;
    if (systemf("rm -r '%s' 2>/dev/null", path_tempdir) < 0) goto end;

    if (mkdir(path_tempdir, 0777)) {
        outf("Failed to create directory: %s", path_tempdir);
        goto end;
    }

    outfx("Unzipping template document '%s' to tempdir: %s",
            path_template, path_tempdir);
    e = systemf("unzip -q -d %s %s", path_tempdir, path_template);
    if (e) {
        outf("Failed to unzip %s into %s",
                path_template, path_tempdir);
        if (e > 0) errno = EIO;
        goto end;
    }

    if (local_asprintf(
            &word_document_xml,
            "%s/word/document.xml",
            path_tempdir) < 0) goto end;

    outfx("Reading tempdir's word/document.xml object");
    f = fopen(word_document_xml, "r");
    if (!f) {
        outf("Failed to open docx object: %s", word_document_xml);
        goto end;
    }
    original = read_all(f);
    if (!original) goto end;
    if (fclose(f)) goto end;
    f = NULL;

    const char* original_marker = "<w:body>";
    const char* original_pos = strstr(original, original_marker);
    if (!original_pos) {
        outf("error: could not find '%s' in docx object: %s",
                original_marker, word_document_xml);
        errno = ESRCH;
        goto end;
    }
    original_pos += strlen(original_marker);

    outfx("Writing tempdir's word/document.xml file");
    f = fopen(word_document_xml, "w");
    if (!f) {
        outf("error: Failed to open .docx for writing: %s",
                word_document_xml);
        goto end;
    }
    if (0
            || fwrite(original, original_pos - original, 1 /*nmemb*/, f) != 1
            || fwrite(content, content_length, 1 /*nmemb*/, f) != 1
            || fwrite(original_pos, strlen(original_pos), 1 /*nmemb*/, f) != 1
            || fclose(f) < 0
            ) {
        outf("error: Failed to write to: %s", word_document_xml);
        goto end;
    }
    f = NULL;

    outf("Zipping tempdir to create %s", path_out);
    const char* path_out_leaf = strrchr(path_out, '/');
    if (!path_out_leaf) path_out_leaf = path_out;
    e = systemf("cd %s && zip -q -r ../%s .", path_tempdir, path_out_leaf);
    if (e) {
        outf("Zip command failed to convert '%s' directory into output file: %s",
                path_tempdir, path_out);
        if (e > 0) errno = EIO;
        goto end;
    }

    if (!preserve_dir) {
        if (strchr(path_tempdir, '\'') || strstr(path_tempdir, "..")) {
            outf("Refusing to delete path_tempdir=%s because not safe for shell");
        }
        e = systemf("rm -r '%s'", path_tempdir);
        if (e) {
            outf("error: Failed to delete tempdir: %s", path_tempdir);
            if (e > 0) errno = EIO;
            goto end;
        }
    }

    ret = 0;

    end:
    if (path_tempdir)   free(path_tempdir);
    if (word_document_xml)  free(word_document_xml);
    if (original)   free(original);
    if (f)  fclose(f);

    return ret;
}



/* Things for direct conversion of text spans into lines and paragraphs. */

/* Returns 1 if lines have same wmode and are at the same angle, else 0. */
static int lines_are_compatible(
        line_t* a,
        line_t* b,
        float           angle_a,
        int             verbose
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
    float angle_b = span_angle(line_span_first(b));
    if (angle_b != angle_a) {
        outfx("%s:%i: angles differ");
        return 0;
    }
    return 1;
}


/* Creates representation of span_t's that consists of a list of
line_t's, with each line_t containins pointers to a list of
span_t's.

We only join spans that are at the same angle and are aligned.

On entry:
    Original value of *o_lines and *o_lines_num are ignored.

    <spans> points to array of <spans_num> span_t*'s, each pointing to
    an span_t.

On exit:
    If we succeed, we return 0, with *o_lines pointing to array of *o_lines_num
    line_t*'s, each pointing to an line_t.

    Otherwise we return -1 with errno set. *o_lines and *o_lines_num are
    undefined.
*/
static int make_lines(
        span_t**    spans,
        int                 spans_num,
        line_t***   o_lines,
        int*                o_lines_num
        )
{
    int ret = -1;

    /* Make an line_t for each span. Then we will join some of these
    line_t's together before returning. */
    int     lines_num = spans_num;
    line_t** lines = NULL;

    lines = malloc(sizeof(*lines) * lines_num);
    if (!lines) goto end;

    int a;
    /* Ensure we can clean up after error. */
    for (a=0; a<lines_num; ++a) {
        lines[a] = NULL;
    }
    for (a=0; a<lines_num; ++a) {
        lines[a] = malloc(sizeof(line_t));
        if (!lines[a])  goto end;
        lines[a]->spans_num = 0;
        lines[a]->spans = malloc(sizeof(span_t*) * 1);
        if (!lines[a]->spans)   goto end;
        lines[a]->spans_num = 1;
        lines[a]->spans[0] = spans[a];
        outfx("initial line a=%i: %s", a, line_string(lines[a]));
    }

    int num_compatible = 0;

    /* For each line, look for nearest aligned line, and append if found. */
    int num_joins = 0;
    for (a=0; a<lines_num; ++a) {

        line_t* line_a = lines[a];
        if (!line_a) {
            continue;
        }

        int verbose = 0;
        if (0 && a < 1) verbose = 1;
        outfx("looking at line_a=%s", line_string2(line_a));
        line_t* nearest_line = NULL;
        int nearest_line_b = -1;
        float nearest_adv = 0;

        span_t* span_a = line_span_last(line_a);
        float angle_a = span_angle(span_a);
        if (verbose) outf("a=%i angle_a=%lf ctm=%s: %s",
                a,
                angle_a * 180/g_pi,
                matrix_string(&span_a->ctm),
                line_string2(line_a)
                );

        int b;
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
                outf("a=%i b=%i: nearest_line_b=%i nearest_adv=%lf",
                        a,
                        b,
                        nearest_line_b,
                        nearest_adv
                        );
                outf("    line_a=%s", line_string2(line_a));
                outf("    line_b=%s", line_string2(line_b));
            }
            if (!lines_are_compatible(line_a, line_b, angle_a, 0*verbose)) {
                if (verbose) outf("not compatible");
                continue;
            }

            num_compatible += 1;

            /* Find angle between last glyph of span_a and first glyph of
            span_b. This detects whether the lines are lined up with each other
            (as opposed to being at the same angle but in different lines). */
            span_t* span_b = line_span_first(line_b);
            float dx = span_char_first(span_b)->x - span_char_last(span_a)->x;
            float dy = span_char_first(span_b)->y - span_char_last(span_a)->y;
            float angle_a_b = atan2(-dy, dx);
            if (verbose) {
                outf("delta=(%f %f) alast=(%f %f) bfirst=(%f %f): angle_a=%lf angle_a_b=%lf",
                        dx,
                        dy,
                        span_char_last(span_a)->x,
                        span_char_last(span_a)->y,
                        span_char_first(span_b)->x,
                        span_char_first(span_b)->y,
                        angle_a * 180 / g_pi,
                        angle_a_b * 180 / g_pi
                        );
            }
            /* Might want to relax this when we test on non-horizontal lines.
            */
            const float angle_tolerance_deg = 1;
            if (fabs(angle_a_b - angle_a) * 180 / g_pi <= angle_tolerance_deg) {
                /* Find distance between end of line_a and beginning of line_b. */
                float adv = spans_adv(
                        span_a,
                        span_char_last(span_a),
                        span_char_first(span_b)
                        );
                if (verbose) outf("nearest_adv=%lf. angle_a_b=%lf adv=%lf",
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
                        "angle beyond tolerance: span_a last=(%f,%f) span_b first=(%f,%f) angle_a_b=%lg angle_a=%lg span_a.trm{a=%f b=%f}",
                        span_char_last(span_a)->x,
                        span_char_last(span_a)->y,
                        span_char_first(span_b)->x,
                        span_char_first(span_b)->y,
                        angle_a_b * 180 / g_pi,
                        angle_a * 180 / g_pi,
                        span_a->trm.a,
                        span_a->trm.b
                        );
            }
        }

        if (nearest_line) {
            /* line_a and nearest_line are aligned so we can move line_b's
            spans on to the end of line_a. */
            b = nearest_line_b;
            if (verbose) outf("found nearest line. a=%i b=%i", a, b);
            span_t* span_b = line_span_first(nearest_line);

            if (1
                    && span_char_last(span_a)->ucs != ' '
                    && span_char_first(span_b)->ucs != ' '
                    ) {
                /* Find average advance of the two adjacent spans in the two
                lines we are considering joining, so that we can decide whether
                the distance between them is large enough to merit joining with
                a space character). */
                float average_adv = (
                        (span_adv_total(span_a) + span_adv_total(span_b))
                        /
                        (span_a->chars_num + span_b->chars_num)
                        );

                int insert_space = (nearest_adv > 0.25 * average_adv);
                if (insert_space) {
                    /* Append space to span_a before concatenation. */
                    if (verbose) {
                        outf("(inserted space) nearest_adv=%lf average_adv=%lf",
                                nearest_adv,
                                average_adv
                                );
                        outf("    a: %s", span_string(span_a));
                        outf("    b: %s", span_string(span_b));
                    }
                    char_t* p = realloc(
                            span_a->chars,
                            (span_a->chars_num + 1) * sizeof(char_t)
                            );
                    if (!p) goto end;
                    span_a->chars = p;
                    char_t* item = &span_a->chars[span_a->chars_num];
                    span_a->chars_num += 1;
                    bzero(item, sizeof(*item));
                    item->ucs = ' ';
                    item->adv = nearest_adv;
                }

                if (verbose) {
                    outf("Joining spans a=%i b=%i:", a, b);
                    outf("    %s", span_string2(span_a));
                    outf("    %s", span_string2(span_b));
                }
                if (0) {
                    /* Show details about what we're joining. */
                    outf(
                            "joining line insert_space=%i a=%i (y=%f) to line b=%i (y=%f). nearest_adv=%lf average_adv=%lf",
                            insert_space,
                            a,
                            span_char_last(span_a)->y,
                            b,
                            span_char_first(span_b)->y,
                            nearest_adv,
                            average_adv
                            );
                    outf("a: %s", span_string(span_a));
                    outf("b: %s", span_string(span_b));
                }
            }

            /* We might end up with two adjacent spaces here. But removing a
            space could result in an empty line_t, which could break various
            assumptions elsewhere. */

            if (verbose) {
                outf("Joining spans a=%i b=%i:", a, b);
                outf("    %s", span_string2(span_a));
                outf("    %s", span_string2(span_b));
            }
            span_t** s = realloc(
                    line_a->spans,
                    sizeof(span_t*)
                        * (line_a->spans_num + nearest_line->spans_num)
                    );
            if (!s) goto end;
            line_a->spans = s;
            int k;
            for (k=0; k<nearest_line->spans_num; ++k) {
                line_a->spans[ line_a->spans_num + k] = nearest_line->spans[k];
            }
            line_a->spans_num += nearest_line->spans_num;

            /* Ensure that we ignore nearest_line from now on. */
            free(nearest_line->spans);
            free(nearest_line);
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

    /* Remove empty lines left behind after we appended pairs of lines. */
    int from;
    int to;
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
    lines_num = to;
    line_t** l = realloc(lines, sizeof(line_t*) * lines_num);
    /* Should always succeed because we're not increasing allocation size. */
    assert(l);

    lines = l;

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
        for (a=0; a<lines_num; ++a) {
            if (lines[a])   free(lines[a]->spans);
            free(lines[a]);
        }
    }
    return ret;
}


/* Returns max font size of all span_t's in an line_t. */
static float line_font_size_max(line_t* line)
{
    float   size_max = 0;
    int i;
    for (i=0; i<line->spans_num; ++i) {
        span_t* span = line->spans[i];
        int size = matrix_expansion(span->trm);
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
static float line_distance(
        float ax,
        float ay,
        float bx,
        float by,
        float angle
        )
{
    float dx = bx - ax;
    float dy = by - ay;

    return dx * sin(angle) + dy * cos(angle);
}


/* A comparison function for use with qsort(), for sorting paragraphs within a
page. */
static int paragraphs_cmp(const void* a, const void* b)
{
    paragraph_t* const* a_paragraph = a;
    paragraph_t* const* b_paragraph = b;
    line_t* a_line = paragraph_line_first(*a_paragraph);
    line_t* b_line = paragraph_line_first(*b_paragraph);

    span_t* a_span = line_span_first(a_line);
    span_t* b_span = line_span_first(b_line);

    /* If ctm matrices differ, always return this diff first. Note that we
    ignore .e and .f because if data is from ghostscript then .e and .f vary
    for each span, and we don't care about these differences. */
    int d = matrix_cmp4(&a_span->ctm, &b_span->ctm);
    if (d)  return d;

    float a_angle = line_angle(a_line);
    float b_angle = line_angle(b_line);
    if (fabs(a_angle - b_angle) > 3.14/2) {
        /* Give up if more than 90 deg. */
        return 0;
    }
    float angle = (a_angle + b_angle) / 2;
    float ax = line_item_first(a_line)->x;
    float ay = line_item_first(a_line)->y;
    float bx = line_item_first(b_line)->x;
    float by = line_item_first(b_line)->y;
    float distance = line_distance(ax, ay, bx, by, angle);
    if (distance > 0)   return -1;
    if (distance < 0)   return +1;
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
        line_t**        lines,
        int                     lines_num,
        paragraph_t***  o_paragraphs,
        int*                    o_paragraphs_num
        )
{
    int ret = -1;
    paragraph_t** paragraphs = NULL;

    /* Start off with an paragraph_t for each line_t. */
    int paragraphs_num = lines_num;
    paragraphs = malloc(sizeof(*paragraphs) * paragraphs_num);
    if (!paragraphs) goto end;
    int a;
    /* Ensure we can clean up after error when setting up. */
    for (a=0; a<paragraphs_num; ++a) {
        paragraphs[a] = NULL;
    }
    /* Set up initial paragraphs. */
    for (a=0; a<paragraphs_num; ++a) {
        paragraphs[a] = malloc(sizeof(paragraph_t));
        if (!paragraphs[a]) goto end;
        paragraphs[a]->lines_num = 0;
        paragraphs[a]->lines = malloc(sizeof(line_t*) * 1);
        if (!paragraphs[a]->lines) goto end;
        paragraphs[a]->lines_num = 1;
        paragraphs[a]->lines[0] = lines[a];
    }

    int num_joins = 0;
    for (a=0; a<paragraphs_num; ++a) {

        paragraph_t* paragraph_a = paragraphs[a];
        if (!paragraph_a) {
            /* This paragraph is empty - already been appended to a different
            paragraph. */
            continue;
        }

        paragraph_t* nearest_paragraph = NULL;
        int nearest_paragraph_b = -1;
        float nearest_paragraph_distance = -1;
        assert(paragraph_a->lines_num > 0);

        line_t* line_a = paragraph_line_last(paragraph_a);
        float angle_a = line_angle(line_a);

        int verbose = 0;

        /* Look for nearest paragraph_t that could be appended to
        paragraph_a. */
        int b;
        for (b=0; b<paragraphs_num; ++b) {
            paragraph_t* paragraph_b = paragraphs[b];
            if (!paragraph_b) {
                /* This paragraph is empty - already been appended to a different
                paragraph. */
                continue;
            }
            line_t* line_b = paragraph_line_first(paragraph_b);
            if (!lines_are_compatible(line_a, line_b, angle_a, 0)) {
                continue;
            }

            float ax = line_item_last(line_a)->x;
            float ay = line_item_last(line_a)->y;
            float bx = line_item_first(line_b)->x;
            float by = line_item_first(line_b)->y;
            float distance = line_distance(ax, ay, bx, by, angle_a);
            if (verbose) {
                outf(
                        "angle_a=%lf a=(%lf %lf) b=(%lf %lf) delta=(%lf %lf) distance=%lf:",
                        angle_a * 180 / g_pi,
                        ax, ay,
                        bx, by,
                        bx - ax,
                        by - ay,
                        distance
                        );
                outf("    line_a=%s", line_string2(line_a));
                outf("    line_b=%s", line_string2(line_b));
            }
            if (distance > 0) {
                if (nearest_paragraph_distance == -1
                        || distance < nearest_paragraph_distance) {
                    if (verbose) {
                        outf("updating nearest. distance=%lf:", distance);
                        outf("    line_a=%s", line_string2(line_a));
                        outf("    line_b=%s", line_string2(line_b));
                    }
                    nearest_paragraph_distance = distance;
                    nearest_paragraph_b = b;
                    nearest_paragraph = paragraph_b;
                }
            }
        }

        if (nearest_paragraph) {
            line_t* line_b = paragraph_line_first(nearest_paragraph);
            (void) line_b; /* Only used in outfx(). */
            float line_b_size = line_font_size_max(
                    paragraph_line_first(nearest_paragraph)
                    );
            if (nearest_paragraph_distance < 1.5 * line_b_size) {
                if (verbose) {
                    outf(
                            "joing paragraphs. a=(%lf,%lf) b=(%lf,%lf) nearest_paragraph_distance=%lf line_b_size=%lf",
                            line_item_last(line_a)->x,
                            line_item_last(line_a)->y,
                            line_item_first(line_b)->x,
                            line_item_first(line_b)->y,
                            nearest_paragraph_distance,
                            line_b_size
                            );
                    outf("    %s", paragraph_string(paragraph_a));
                    outf("    %s", paragraph_string(nearest_paragraph));
                    outf("paragraph_a ctm=%s",
                            matrix_string(&paragraph_a->lines[0]->spans[0]->ctm)
                            );
                    outf("paragraph_a trm=%s",
                            matrix_string(&paragraph_a->lines[0]->spans[0]->trm)
                            );
                }
                /* Join these two paragraph_t's. */
                span_t* a_span = line_span_last(line_a);
                if (span_char_last(a_span)->ucs == '-') {
                    /* remove trailing '-' at end of prev line. char_t doesn't
                    contain any malloc-heap pointers so this doesn't leak. */
                    a_span->chars_num -= 1;
                }
                else {
                    /* Insert space before joining adjacent lines. */
                    if (span_append_c(line_span_last(line_a), ' ')) goto end;
                }

                int a_lines_num_new = paragraph_a->lines_num + nearest_paragraph->lines_num;
                line_t** l = realloc(
                        paragraph_a->lines,
                        sizeof(line_t*) * a_lines_num_new
                        );
                if (!l) goto end;
                paragraph_a->lines = l;
                int i;
                for (i=0; i<nearest_paragraph->lines_num; ++i) {
                    paragraph_a->lines[paragraph_a->lines_num + i]
                        = nearest_paragraph->lines[i];
                }
                paragraph_a->lines_num = a_lines_num_new;

                /* Ensure that we skip nearest_paragraph in future. */
                free(nearest_paragraph->lines);
                free(nearest_paragraph);
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
                        "Not joining paragraphs. nearest_paragraph_distance=%lf line_b_size=%lf",
                        nearest_paragraph_distance,
                        line_b_size
                        );
            }
        }
    }

    /* Remove empty paragraphs. */
    int from;
    int to;
    for (from=0, to=0; from<paragraphs_num; ++from) {
        if (paragraphs[from]) {
            paragraphs[to] = paragraphs[from];
            to += 1;
        }
    }
    outfx("paragraphs_num=%i => %i", paragraphs_num, to);
    paragraphs_num = to;
    paragraph_t** p = realloc(
            paragraphs,
            sizeof(paragraph_t*) * paragraphs_num
            );
    /* Should always succeed because we're not increasing allocation size. */
    assert(p);

    paragraphs = p;

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
        for (a=0; a<paragraphs_num; ++a) {
            if (paragraphs[a])   free(paragraphs[a]->lines);
            free(paragraphs[a]);
        }
        free(paragraphs);
    }
    return ret;
}


static void extract_document_init(extract_document_t* document)
{
    document->pages = NULL;
    document->pages_num = 0;
}

/* Does preliminary processing of the end of the last spen in a page; intended
to be called as we load span information.

Looks at last two char_t's in last span_t of <page>, and either
leaves unchanged, or removes space in last-but-one position, or moves last
char_t into a new span_t. */
static int page_span_end_clean(page_t* page)
{
    int ret = -1;
    assert(page->spans_num);
    span_t* span = page->spans[page->spans_num-1];
    assert(span->chars_num);

    /* Last two char_t's are char_[-2] and char_[-1]. */
    char_t* char_ = &span->chars[span->chars_num];

    if (span->chars_num == 1) {
        return 0;
    }

    float font_size = matrix_expansion(span->trm)
            * matrix_expansion(span->ctm);

    point_t dir;
    if (span->wmode) {
        dir.x = 0;
        dir.y = 1;
    }
    else {
        dir.x = 1;
        dir.y = 0;
    }
    dir = multiply_matrix_point(span->trm, dir);

    float x = char_[-2].pre_x + char_[-2].adv * dir.x;
    float y = char_[-2].pre_y + char_[-2].adv * dir.y;

    float err_x = (char_[-1].pre_x - x) / font_size;
    float err_y = (char_[-1].pre_y - y) / font_size;

    if (span->chars_num >= 2 && span->chars[span->chars_num-2].ucs == ' ') {
        int remove_penultimate_space = 0;
        if (err_x < -span->chars[span->chars_num-2].adv / 2
                && err_x > -span->chars[span->chars_num-2].adv
                ) {
            remove_penultimate_space = 1;
        }
        if ((char_[-1].pre_x - char_[-2].pre_x) / font_size < char_[-1].adv / 10) {
            outfx(
                    "removing penultimate space because space very narrow:"
                    "char_[-1].pre_x-char_[-2].pre_x=%f font_size=%f"
                    " char_[-1].adv=%f",
                    char_[-1].pre_x - char_[-2].pre_x,
                    font_size,
                    char_[-1].adv
                    );
            remove_penultimate_space = 1;
        }
        if (remove_penultimate_space) {
            /* This character overlaps with previous space
            character. We discard previous space character - these
            sometimes seem to appear in the middle of words for some
            reason. */
            outfx("removing space before final char in: %s",
                    span_string(span));
            span->chars[span->chars_num-2] = span->chars[span->chars_num-1];
            span->chars_num -= 1;
            outfx("span is now:                         %s", span_string(span));
            return 0;
        }
    }
    else if (fabs(err_x) > 0.01 || fabs(err_y) > 0.01) {
        /* This character doesn't seem to be a continuation of
        previous characters, so split into two spans. This often
        splits text incorrectly, but this is corrected later when
        we join spans into lines. */
        outfx(
                "Splitting last char into new span. font_size=%f dir.x=%f"
                " char[-1].pre=(%f, %f) err=(%f, %f): %s",
                font_size,
                dir.x,
                char_[-1].pre_x,
                char_[-1].pre_y,
                err_x,
                err_y,
                span_string2(span)
                );
        span_t* span2 = page_span_append(page);
        if (!span2) goto end;
        *span2 = *span;
        span2->font_name = local_strdup(span->font_name);
        if (!span2->font_name) goto end;
        span2->chars_num = 1;
        span2->chars = malloc(sizeof(char_t) * span2->chars_num);
        if (!span2->chars) goto end;
        span2->chars[0] = char_[-1];
        span->chars_num -= 1;
        return 0;
    }
    ret = 0;
    end:
    return ret;
}

int extract_intermediate_to_document(
        const char*             path,
        int                     autosplit,
        extract_document_t**    o_document
        )
{
    int ret = -1;

    FILE* in = NULL;
    int num_spans = 0;

    /* Number of extra spans from page_span_end_clean(). */
    int num_spans_split = 0;

    /* Num extra spans from autosplit=1. */
    int num_spans_autosplit = 0;

    xml_tag_t   tag;
    xml_tag_init(&tag);

    extract_document_t* document = malloc(sizeof(**o_document));
    if (!document) goto end;
    extract_document_init(document);
    
    in = xml_pparse_init(path, NULL);
    if (!in) {
        outf("Failed to open: %s", path);
        goto end;
    }
    /* Data read from <path> is expected to be XML looking like:

    <page>
        <span>
            <char ...>
            <char ...>
            ...
        </span>
        <span>
            ...
        </span>
        ...
    </page>
    <page>
        ...
    </page>
    ...

    We convert this into a list of page_t's, each containing a list of
    span_t's, each containing a list of char_t's.

    While doing this, we do some within-span processing by calling
    page_span_end_clean():
        Remove spurious spaces.
        Split spans in two where there seem to be large gaps between glyphs.
    */
    for(;;) {
        int e = xml_pparse_next(in, &tag);
        if (e == 1) break; /* EOF. */
        if (e) goto end;
        if (!strcmp(tag.name, "?xml")) {
            /* We simply skip this if we find it. As of 2020-07-31, mutool adds
            this header to mupdf raw output, but gs txtwrite does not include
            it. */
            continue;
        }
        if (strcmp(tag.name, "page")) {
            outf("Expected <page> but tag.name='%s'", tag.name);
            errno = ESRCH;
            goto end;
        }
        outfx("loading spans for page %i...", document->pages_num);
        page_t* page = document_page_append(document);
        if (!page) goto end;

        for(;;) {
            if (xml_pparse_next(in, &tag)) goto end;
            if (!strcmp(tag.name, "/page")) {
                num_spans += page->spans_num;
                break;
            }
            if (strcmp(tag.name, "span")) {
                outf("Expected <span> but tag.name='%s'", tag.name);
                errno = ESRCH;
                goto end;
            }

            span_t* span = page_span_append(page);
            if (!span) goto end;

            if (s_matrix_read(xml_tag_attributes_find(&tag, "ctm"), &span->ctm)) goto end;
            if (s_matrix_read(xml_tag_attributes_find(&tag, "trm"), &span->trm)) goto end;

            char* f = xml_tag_attributes_find(&tag, "font_name");
            if (!f) {
                outf("Failed to find attribute 'font_name'");
                goto end;
            }
            char* ff = strchr(f, '+');
            if (ff)  f = ff + 1;
            span->font_name = local_strdup(f);
            if (!span->font_name) {
                outf("Attribute 'font_name' is bad: %s", f);
                goto end;
            }
            span->font_bold = strstr(span->font_name, "-Bold") ? 1 : 0;
            span->font_italic = strstr(span->font_name, "-Oblique") ? 1 : 0;
            
            {
                /* Need to use temporary int because span->wmode is a bitfield. */
                int wmode;
                if (xml_tag_attributes_find_int(&tag, "wmode", &wmode)) {
                    outf("Failed to find attribute 'wmode'");
                    goto end;
                }
                span->wmode = wmode;
            }

            float   offset_x = 0;
            float   offset_y = 0;
            for(;;) {
                if (xml_pparse_next(in, &tag)) {
                    outf("Failed to find <char or </span");
                    goto end;
                }
                if (!strcmp(tag.name, "/span")) {
                    break;
                }
                if (strcmp(tag.name, "char")) {
                    errno = ESRCH;
                    outf("Expected <char> but tag.name='%s'", tag.name);
                    goto end;
                }

                float char_pre_x;
                float char_pre_y;
                if (xml_tag_attributes_find_float(&tag, "x", &char_pre_x)) goto end;
                if (xml_tag_attributes_find_float(&tag, "y", &char_pre_y)) goto end;

                if (autosplit && char_pre_y - offset_y != 0) {
                    outfx("autosplit: char_pre_y=%f offset_y=%f",
                            char_pre_y, offset_y);
                    float e = span->ctm.e + span->ctm.a * (char_pre_x-offset_x)
                            + span->ctm.b * (char_pre_y - offset_y);
                    float f = span->ctm.f + span->ctm.c * (char_pre_x-offset_x)
                            + span->ctm.d * (char_pre_y - offset_y);
                    offset_x = char_pre_x;
                    offset_y = char_pre_y;
                    outfx(
                            "autosplit: changing ctm.{e,f} from (%f, %f) to (%f, %f)",
                            span->ctm.e,
                            span->ctm.f,
                            e, f
                            );
                    if (span->chars_num > 0) {
                        /* Create new span. */
                        num_spans_autosplit += 1;
                        span_t* span0 = span;
                        span = page_span_append(page);
                        if (!span) goto end;
                        *span = *span0;
                        span->chars = NULL;
                        span->chars_num = 0;
                        span->font_name = local_strdup(span0->font_name);
                        if (!span->font_name) goto end;
                    }
                    span->ctm.e = e;
                    span->ctm.f = f;
                    outfx("autosplit: char_pre_y=%f offset_y=%f",
                            char_pre_y, offset_y);
                }

                if (span_append_c(span, 0 /*c*/)) goto end;
                char_t* char_ = &span->chars[ span->chars_num-1];
                char_->pre_x = char_pre_x - offset_x;
                char_->pre_y = char_pre_y - offset_y;
                if (char_->pre_y) {
                    outfx("char_->pre=(%f %f)",
                            char_->pre_x, char_->pre_y);
                }

                char_->x = span->ctm.a * char_->pre_x + span->ctm.b * char_->pre_y;
                char_->y = span->ctm.c * char_->pre_x + span->ctm.d * char_->pre_y;

                if (xml_tag_attributes_find_float(&tag, "adv", &char_->adv)) goto end;

                if (xml_tag_attributes_find_int(&tag, "ucs", &char_->ucs)) goto end;

                char    trm[64];
                snprintf(trm, sizeof(trm), "%s", matrix_string(&span->trm));
               char_->x += span->ctm.e;
               char_->y += span->ctm.f;

                outfx(
                        "ctm=%s trm=%s ctm*trm=%f pre=(%f %f) =>"
                        " xy=(%f %f) [orig xy=(%f %f)]",
                        matrix_string(&span->ctm),
                        trm,
                        span->ctm.a * span->trm.a,
                        char_->pre_x,
                        char_->pre_y,
                        char_->x,
                        char_->y,
                        x, y
                        );

                int page_spans_num_old = page->spans_num;
                if (page_span_end_clean(page)) goto end;
                span = page->spans[page->spans_num-1];
                if (page->spans_num != page_spans_num_old) {
                    num_spans_split += 1;
                }
            }
            xml_tag_free(&tag);
        }
        outf("page=%i page->num_spans=%i",
                document->pages_num, page->spans_num);
    }

    outf("num_spans=%i num_spans_split=%i num_spans_autosplit=%i",
            num_spans,
            num_spans_split,
            num_spans_autosplit
            );

    ret = 0;

    end:
    xml_tag_free(&tag);
    if (in) {
        fclose(in);
        in = NULL;
    }

    if (ret) {
        outf("read_spans_raw() returning error");
        extract_document_free(document);
        free(document);
        *o_document = NULL;
    }
    else {
        *o_document = document;
    }

    return ret;
}


static float matrices_to_font_size(matrix_t* ctm, matrix_t* trm)
{
    float font_size = matrix_expansion(*trm)
            * matrix_expansion(*ctm);
    /* Round font_size to nearest 0.01. */
    font_size = (int) (font_size * 100 + 0.5) / 100.0;
    return font_size;
}

/* Writes paragraphs from extract_document_t into docx content. On return *content
points to zero-terminated content, allocated by realloc().

spacing: if true, we insert extra vertical space between paragraphs. */
int extract_document_to_docx_content(
        extract_document_t* document,
        int                 spacing,
        char**              o_content,
        int*                o_content_length
        )
{
    int ret = -1;
    
    string_t    content;
    string_init(&content);

    /* Write paragraphs into <content>. */
    int p;
    for (p=0; p<document->pages_num; ++p) {
        page_t* page = document->pages[p];

        const char* font_name = NULL;
        float       font_size = 0;
        int         font_bold = 0;
        int         font_italic = 0;
        matrix_t*   ctm_prev = NULL;
        int p;
        for (p=0; p<page->paragraphs_num; ++p) {
            paragraph_t* paragraph = page->paragraphs[p];
            if (spacing
                    && ctm_prev
                    && paragraph->lines_num
                    && paragraph->lines[0]->spans_num
                    && matrix_cmp4(
                            ctm_prev,
                            &paragraph->lines[0]->spans[0]->ctm
                            )
                    ) {
                /* Extra vertical space between paragraphs that were at
                different angles in the original document. */
                if (docx_paragraph_empty(&content)) goto end;
            }

            if (spacing) {
                /* Extra vertical space between paragraphs. */
                if (docx_paragraph_empty(&content)) goto end;
            }
            if (docx_paragraph_start(&content)) goto end;

            int l;
            for (l=0; l<paragraph->lines_num; ++l) {
                line_t* line = paragraph->lines[l];
                int s;
                for (s=0; s<line->spans_num; ++s) {
                    span_t* span = line->spans[s];
                    ctm_prev = &span->ctm;
                    float font_size_new = matrices_to_font_size(
                            &span->ctm, &span->trm
                            );
                    if (!font_name
                            || strcmp(span->font_name, font_name)
                            || span->font_bold != font_bold
                            || span->font_italic != font_italic
                            || font_size_new != font_size
                            ) {
                        if (font_name) {
                            if (docx_run_finish(&content)) goto end;
                        }
                        font_name = span->font_name;
                        font_bold = span->font_bold;
                        font_italic = span->font_italic;
                        font_size = font_size_new;
                        if (docx_run_start(
                                &content,
                                font_name,
                                font_size,
                                font_bold,
                                font_italic
                                )) goto end;
                    }

                    int si;
                    for (si=0; si<span->chars_num; ++si) {
                        char_t* char_ = &span->chars[si];
                        int c = char_->ucs;

                        if (0) {}

                        /* Escape XML special characters. */
                        else if (c == '<')  docx_char_append_string(&content, "&lt;");
                        else if (c == '>')  docx_char_append_string(&content, "&gt;");
                        else if (c == '&')  docx_char_append_string(&content, "&amp;");
                        else if (c == '"')  docx_char_append_string(&content, "&quot;");
                        else if (c == '\'') docx_char_append_string(&content, "&apos;");

                        /* Expand ligatures. */
                        else if (c == 0xFB00) {
                            if (docx_char_append_string(&content, "ff")) goto end;
                        }
                        else if (c == 0xFB01) {
                            if (docx_char_append_string(&content, "fi")) goto end;
                        }
                        else if (c == 0xFB02) {
                            if (docx_char_append_string(&content, "fl")) goto end;
                        }
                        else if (c == 0xFB03) {
                            if (docx_char_append_string(&content, "ffi")) goto end;
                        }
                        else if (c == 0xFB04) {
                            if (docx_char_append_string(&content, "ffl")) goto end;
                        }

                        /* Output ASCII verbatim. */
                        else if (c >= 32 && c <= 127) {
                            if (docx_char_append_char(&content, c)) goto end;
                        }

                        /* Escape all other characters. */
                        else {
                            char    buffer[32];
                            snprintf(buffer, sizeof(buffer), "&#x%x;", c);
                            if (docx_char_append_string(&content, buffer)) goto end;
                        }
                    }
                    /* Remove any trailing '-' at end of line. */
                    if (docx_char_truncate_if(&content, '-')) goto end;
                }
            }
            if (font_name) {
                if (docx_run_finish(&content)) goto end;
                font_name = NULL;
            }
            if (docx_paragraph_finish(&content)) goto end;
        }
    }
    ret = 0;

    end:

    if (ret) {
        string_free(&content);
        *o_content = NULL;
        *o_content_length = 0;
    }
    else {
        *o_content = content.chars;
        *o_content_length = content.chars_num;
        content.chars = NULL;
        content.chars_num = 0;
    }

    return ret;
}

int extract_document_join(extract_document_t* document)
{
    int ret = -1;

    /* Now for each page we join spans into lines and paragraphs. A line is a
    list of spans that are at the same angle and on the same line. A paragraph
    is a list of lines that are at the same angle and close together. */
    int p;
    for (p=0; p<document->pages_num; ++p) {
        page_t* page = document->pages[p];
        outf("processing page %i: num_spans=%i", p, page->spans_num);

        if (make_lines(
                page->spans,
                page->spans_num,
                &page->lines,
                &page->lines_num
                )) goto end;

        if (make_paragraphs(
                page->lines,
                page->lines_num,
                &page->paragraphs,
                &page->paragraphs_num
                )) goto end;
    }

    ret = 0;

    end:

    return ret;
}
