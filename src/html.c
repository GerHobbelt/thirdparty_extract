/* These extract_html_*() functions generate docx content and docx zip archive
data.

Caller must call things in a sensible order to create valid content -
e.g. don't call docx_paragraph_start() twice without intervening call to
docx_paragraph_finish(). */

#include "../include/extract.h"

#include "astring.h"
#include "document.h"
#include "html.h"
#include "mem.h"
#include "memento.h"
#include "outf.h"
#include "sys.h"
#include "text.h"
#include "zip.h"

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/stat.h>


static int extract_html_paragraph_start(extract_alloc_t* alloc, extract_astring_t* content)
{
    return extract_astring_cat(alloc, content, "\n\n<p>");
}

static int extract_html_paragraph_finish(extract_alloc_t* alloc, extract_astring_t* content)
{
    return extract_astring_cat(alloc, content, "\n</p>");
}

static int extract_html_run_start(
        extract_alloc_t* alloc,
        extract_astring_t* content,
        const char* font_name,
        double font_size,
        int bold,
        int italic
        )
/* Starts a new run. Caller must ensure that extract_html_run_finish() was
called to terminate any previous run. */
{
    (void) alloc;
    (void) content;
    (void) font_name;
    (void) font_size;
    (void) bold;
    (void) italic;
    int e = 0;
    return e;
}

static int extract_html_run_finish(extract_alloc_t* alloc, extract_astring_t* content)
{
    (void) alloc;
    (void) content;
    return 0;
}

static int extract_html_char_truncate_if(extract_astring_t* content, char c)
/* Removes last char if it is <c>. */
{
    if (content->chars_num && content->chars[content->chars_num-1] == c) {
        extract_astring_truncate(content, 1);
    }
    return 0;
}


typedef struct
{
    const char* font_name;
    double      font_size;
    int         font_bold;
    int         font_italic;
    matrix_t*   ctm_prev;
} content_state_t;
/* Used to keep track of font information when writing paragraphs of docx
content, e.g. so we know whether a font has changed so need to start a new docx
span. */


static int extract_document_to_html_content_paragraph(
        extract_alloc_t*    alloc,
        content_state_t*    state,
        paragraph_t*        paragraph,
        extract_astring_t*  content
        )
/* Append html for <paragraph> to <content>. Updates *state if we change
font. */
{
    int e = -1;
    int l;
    if (extract_html_paragraph_start(alloc, content)) goto end;

    for (l=0; l<paragraph->lines_num; ++l) {
        line_t* line = paragraph->lines[l];
        int s;
        for (s=0; s<line->spans_num; ++s) {
            int si;
            span_t* span = line->spans[s];
            double font_size_new;
            state->ctm_prev = &span->ctm;
            font_size_new = extract_matrices_to_font_size(&span->ctm, &span->trm);
            if (!state->font_name
                    || strcmp(span->font_name, state->font_name)
                    || span->font_bold != state->font_bold
                    || span->font_italic != state->font_italic
                    || font_size_new != state->font_size
                    ) {
                if (state->font_name) {
                    if (extract_html_run_finish(alloc, content)) goto end;
                }
                state->font_name = span->font_name;
                state->font_bold = span->font_bold;
                state->font_italic = span->font_italic;
                state->font_size = font_size_new;
                if (extract_html_run_start(
                        alloc,
                        content,
                        state->font_name,
                        state->font_size,
                        state->font_bold,
                        state->font_italic
                        )) goto end;
            }

            for (si=0; si<span->chars_num; ++si) {
                char_t* char_ = &span->chars[si];
                int c = char_->ucs;
                if (extract_astring_cat_xmlc(alloc, content, c)) goto end;
            }
            /* Remove any trailing '-' at end of line. */
            if (extract_html_char_truncate_if(content, '-')) goto end;
        }
    }
    if (state->font_name) {
        if (extract_html_run_finish(alloc, content)) goto end;
        state->font_name = NULL;
    }
    if (extract_html_paragraph_finish(alloc, content)) goto end;
    
    e = 0;
    
    end:
    return e;
}


int extract_document_to_html_content(
        extract_alloc_t*    alloc,
        document_t*         document,
        int                 rotation,
        int                 images,
        extract_astring_t*  content
        )
{
    int ret = -1;
    //int text_box_id = 0;
    int p;
    
    (void) rotation;
    (void) images;
    
    /* Write paragraphs into <content>. */
    for (p=0; p<document->pages_num; ++p) {
        page_t* page = document->pages[p];
        int p;
        content_state_t state;
        state.font_name = NULL;
        state.font_size = 0;
        state.font_bold = 0;
        state.font_italic = 0;
        state.ctm_prev = NULL;
        
        for (p=0; p<page->paragraphs_num; ++p) {
            paragraph_t* paragraph = page->paragraphs[p];
            const matrix_t* ctm = &paragraph->lines[0]->spans[0]->ctm;
            double rotate = atan2(ctm->b, ctm->a);
            (void) rotate;
            #if 0
            if (rotation && rotate != 0)
            {
            
                /* Find extent of paragraphs with this same rotation. extent
                will contain max width and max height of paragraphs, in units
                before application of ctm, i.e. before rotation. */
                point_t extent = {0, 0};
                int p0 = p;
                int p1;
                
                outf("rotate=%.2frad=%.1fdeg ctm: ef=(%f %f) abcd=(%f %f %f %f)",
                        rotate, rotate * 180 / pi,
                        ctm->e,
                        ctm->f,
                        ctm->a,
                        ctm->b,
                        ctm->c,
                        ctm->d
                        );
                
                {
                    /* We assume that first span is at origin of text
                    block. This assumes left-to-right text. */
                    double rotate0 = rotate;
                    const matrix_t* ctm0 = ctm;
                    point_t origin = {
                            paragraph->lines[0]->spans[0]->chars[0].x,
                            paragraph->lines[0]->spans[0]->chars[0].y
                            };
                    matrix_t ctm_inverse = {1, 0, 0, 1, 0, 0};
                    double ctm_det = ctm->a*ctm->d - ctm->b*ctm->c;
                    if (ctm_det != 0) {
                        ctm_inverse.a = +ctm->d / ctm_det;
                        ctm_inverse.b = -ctm->b / ctm_det;
                        ctm_inverse.c = -ctm->c / ctm_det;
                        ctm_inverse.d = +ctm->a / ctm_det;
                    }
                    else {
                        outf("cannot invert ctm=(%f %f %f %f)",
                                ctm->a, ctm->b, ctm->c, ctm->d);
                    }

                    for (p=p0; p<page->paragraphs_num; ++p) {
                        paragraph = page->paragraphs[p];
                        ctm = &paragraph->lines[0]->spans[0]->ctm;
                        rotate = atan2(ctm->b, ctm->a);
                        if (rotate != rotate0) {
                            break;
                        }

                        /* Update <extent>. */
                        {
                            int l;
                            for (l=0; l<paragraph->lines_num; ++l) {
                                line_t* line = paragraph->lines[l];
                                span_t* span = line_span_last(line);
                                char_t* char_ = span_char_last(span);
                                double adv = char_->adv * matrix_expansion(span->trm);
                                double x = char_->x + adv * cos(rotate);
                                double y = char_->y + adv * sin(rotate);

                                double dx = x - origin.x;
                                double dy = y - origin.y;

                                /* Position relative to origin and before box rotation. */
                                double xx = ctm_inverse.a * dx + ctm_inverse.b * dy;
                                double yy = ctm_inverse.c * dx + ctm_inverse.d * dy;
                                yy = -yy;
                                if (xx > extent.x) extent.x = xx;
                                if (yy > extent.y) extent.y = yy;
                                if (0) outf("rotate=%f p=%i: origin=(%f %f) xy=(%f %f) dxy=(%f %f) xxyy=(%f %f) span: %s",
                                        rotate, p, origin.x, origin.y, x, y, dx, dy, xx, yy, span_string(alloc, span));
                            }
                        }
                    }
                    p1 = p;
                    rotate = rotate0;
                    ctm = ctm0;
                    outf("rotate=%f p0=%i p1=%i. extent is: (%f %f)",
                            rotate, p0, p1, extent.x, extent.y);
                }
                
                /* Paragraphs p0..p1-1 have same rotation. We output them into
                a single rotated text box. */
                
                /* We need unique id for text box. */
                //text_box_id += 1;
                
                {
                    /* Angles are in units of 1/60,000 degree. */
                    int rot = (int) (rotate * 180 / pi * 60000);

                    /* <wp:anchor distT=\.. etc are in EMU - 1/360,000 of a cm.
                    relativeHeight is z-ordering. (wp:positionV:wp:posOffset,
                    wp:positionV:wp:posOffset) is position of origin of box in
                    EMU.

                    The box rotates about its centre but we want to rotate
                    about the origin (top-left). So we correct the position of
                    box by subtracting the vector that the top-left moves when
                    rotated by angle <rotate> about the middle. */
                    double point_to_emu = 12700;    /* https://en.wikipedia.org/wiki/Office_Open_XML_file_formats#DrawingML */
                    int x = (int) (ctm->e * point_to_emu);
                    int y = (int) (ctm->f * point_to_emu);
                    int w = (int) (extent.x * point_to_emu);
                    int h = (int) (extent.y * point_to_emu);
                    int dx;
                    int dy;

                    if (0) outf("rotate: %f rad, %f deg. rot=%i", rotate, rotate*180/pi, rot);

                    h *= 2;
                    /* We can't predict how much space Word will actually
                    require for the rotated text, so make the box have the
                    original width but allow text to take extra vertical
                    space. There doesn't seem to be a way to make the text box
                    auto-grow to contain the text. */

                    dx = (int) ((1-cos(rotate)) * w / 2.0 + sin(rotate) * h / 2.0);
                    dy = (int) ((cos(rotate)-1) * h / 2.0 + sin(rotate) * w / 2.0);
                    outf("ctm->e,f=%f,%f rotate=%f => x,y=%ik %ik dx,dy=%ik %ik",
                            ctm->e,
                            ctm->f,
                            rotate * 180/pi,
                            x/1000,
                            y/1000,
                            dx/1000,
                            dy/1000
                            );
                    x -= dx;
                    y -= -dy;

                    if (extract_document_output_rotated_paragraphs(alloc, page, p0, p1, rot, x, y, w, h, text_box_id, content, &state)) goto end;
                }
                p = p1 - 1;
                //p = page->paragraphs_num - 1;
            }
            else
            #endif
            {
                if (extract_document_to_html_content_paragraph(alloc, &state, paragraph, content)) goto end;
            }
        
        }
        
        #if 0
        if (images) {
            int i;
            for (i=0; i<page->images_num; ++i) {
                extract_document_append_image(alloc, content, &page->images[i]);
            }
        }
        #endif
    }
    ret = 0;

    end:

    return ret;
}
