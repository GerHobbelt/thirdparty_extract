/* These extract_odt_*() functions generate odt content and odt zip archive
data.

Caller must call things in a sensible order to create valid content -
e.g. don't call odt_paragraph_start() twice without intervening call to
odt_paragraph_finish(). */

#include "../include/extract.h"

//#include "odt_template.h"

#include "astring.h"
#include "document.h"
#include "odt.h"
#include "mem.h"
#include "memento.h"
#include "outf.h"
#include "zip.h"

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/stat.h>


static int extract_odt_paragraph_start(extract_alloc_t* alloc, extract_astring_t* content)
{
    return extract_astring_cat(alloc, content, "\n\n<text:p>");
}

static int extract_odt_paragraph_finish(extract_alloc_t* alloc, extract_astring_t* content)
{
    return extract_astring_cat(alloc, content, "</text:p>");
}


struct extract_odt_style_t
{
    int         id;
    const char* font_name;
    double      font_size;
    int         font_bold;
    int         font_italic;
};

static int extract_odt_style_compare(extract_odt_style_t* a, extract_odt_style_t*b)
{
    int d;
    double dd;
    if ((d = strcmp(a->font_name, b->font_name)))   return d;
    if ((dd = a->font_size - b->font_size) != 0.0)  return (dd > 0.0) ? 1 : -1;
    if ((d = a->font_bold - b->font_bold))          return d;
    if ((d = a->font_italic - b->font_italic))      return d;
    return 0;
}

static int extract_odt_char_append_stringf(extract_alloc_t* alloc, extract_astring_t* content, const char* format, ...)
{
    char* buffer = NULL;
    int e;
    va_list va;
    va_start(va, format);
    e = extract_vasprintf(alloc, &buffer, format, va);
    va_end(va);
    if (e < 0) return e;
    e = extract_astring_cat(alloc, content, buffer);
    extract_free(alloc, &buffer);
    return e;
}

static int extract_odt_style_append_definition(extract_alloc_t* alloc, extract_odt_style_t* style, extract_astring_t* text)
{
    if (extract_odt_char_append_stringf(alloc, text, "<style:style style:name=\"T%i\" style:family=\"text\">", style->id)) return -1;
    if (extract_odt_char_append_stringf(alloc, text, "<style:text-properties style:font-name=\"%s\""), style->font_name) return -1;
    if (extract_odt_char_append_stringf(alloc, text, " fo:font-size=\"%ipt\"", style->font_size)) return -1;
    if (style->font_bold)
    {
        if (extract_astring_cat(alloc, text, " fo:font-weight=\"bold\"")) return -1;
    }
    if (style->font_italic)
    {
        if (extract_astring_cat(alloc, text, " fo:font-style=\"italic\"")) return -1;
    }
    if (extract_astring_cat(alloc, text, " /></style:style>")) return -1;
    return 0;
}

struct extract_odt_styles_t
{
    extract_odt_style_t*    styles;
    int                     styles_num;
};

void extract_odt_styles_free(extract_alloc_t* alloc, extract_odt_styles_t* styles)
{
    extract_free(alloc, &styles->styles);
}

static int extract_odt_styles_definitions(
        extract_alloc_t*        alloc,
        extract_odt_styles_t*   styles,
        extract_astring_t*      out
        )
{
    int i;
    for (i=0; i<styles->styles_num; ++i)
    {
        if (extract_odt_style_append_definition(alloc, &styles->styles[i], out)) return -1;
    }
    return 0;
}

static int styles_add(
        extract_alloc_t*        alloc,
        extract_odt_styles_t*   styles,
        const char*             font_name,
        double                  font_size,
        int                     font_bold,
        int                     font_italic,
        extract_odt_style_t**   o_style
    )
/* Adds specified style to <styles> if not already present. Sets *o_style to
point to the style_t within <styles>. */
{
    extract_odt_style_t style = {0 /*id*/, font_name, font_size, font_bold, font_italic};
    int i;
    /* We keep styles->styles[] sorted; todo: use bsearch or similar when
    searching. */
    for (i=0; i<styles->styles_num; ++i)
    {
        int d = extract_odt_style_compare(&style, &styles->styles[i]);
        if (d == 0)
        {
            *o_style = &styles->styles[i];
            return 0;
        }
        if (d > 0) break;
    }
    /* Insert at position <i>. */
    if (extract_realloc(alloc, &styles->styles, sizeof(styles->styles[0]) * (styles->styles_num+1))) return -1;
    memmove(&styles->styles[i+1], &styles->styles[i], sizeof(styles->styles[0]) * (styles->styles_num - i));
    styles->styles_num += 1;
    styles->styles[i].id = styles->styles_num + 10; /* Leave space for template's built-in styles. */
    styles->styles[i].font_name = font_name;
    styles->styles[i].font_size = font_size;
    styles->styles[i].font_bold = font_bold;
    styles->styles[i].font_italic = font_italic;
    *o_style = &styles->styles[i];
    return 0;
}

static int extract_odt_run_start(
        extract_alloc_t* alloc,
        extract_astring_t* content,
        extract_odt_styles_t* styles,
        const char* font_name,
        double font_size,
        int bold,
        int italic
        )
/* Starts a new run. Caller must ensure that extract_odt_run_finish() was
called to terminate any previous run. */
{
    extract_odt_style_t* style;
    if (styles_add(alloc, styles, font_name, font_size, bold, italic, &style)) return -1;
    if (extract_odt_char_append_stringf(alloc, content, "<text:span text:style-name=\"T%i\"", style->id)) return -1;
    return 0;
}

static int extract_odt_run_finish(extract_alloc_t* alloc, extract_astring_t* content)
{
    return extract_astring_cat(alloc, content, "</text:span>");
}

static int extract_odt_char_append_string(extract_alloc_t* alloc, extract_astring_t* content, const char* text)
{
    return extract_astring_cat(alloc, content, text);
}

static int extract_odt_char_append_char(extract_alloc_t* alloc, extract_astring_t* content, char c)
{
    return extract_astring_catc(alloc, content, c);
}

static int extract_odt_paragraph_empty(extract_alloc_t* alloc, extract_astring_t* content, extract_odt_styles_t* styles)
/* Append an empty paragraph to *content. */
{
    int e = -1;
    if (extract_odt_paragraph_start(alloc, content)) goto end;
    /* It seems like our choice of font size here doesn't make any difference
    to the ammount of vertical space, unless we include a non-space
    character. Presumably something to do with the styles in the template
    document. */
    if (extract_odt_run_start(
            alloc,
            content,
            styles,
            "OpenSans",
            10 /*font_size*/,
            0 /*font_bold*/,
            0 /*font_italic*/
            )) goto end;
    //docx_char_append_string(content, "&#160;");   /* &#160; is non-break space. */
    if (extract_odt_run_finish(alloc, content)) goto end;
    if (extract_odt_paragraph_finish(alloc, content)) goto end;
    e = 0;
    end:
    return e;
}


/* Removes last <len> chars. */
static int odt_char_truncate(extract_astring_t* content, int len)
{
    assert((size_t) len <= content->chars_num);
    content->chars_num -= len;
    content->chars[content->chars_num] = 0;
    return 0;
}

static int extract_odt_char_truncate_if(extract_astring_t* content, char c)
/* Removes last char if it is <c>. */
{
    if (content->chars_num && content->chars[content->chars_num-1] == c) {
        odt_char_truncate(content, 1);
    }
    return 0;
}


static double matrices_to_font_size(matrix_t* ctm, matrix_t* trm)
{
    double font_size = matrix_expansion(*trm)
            * matrix_expansion(*ctm);
    /* Round font_size to nearest 0.01. */
    font_size = (double) (int) (font_size * 100.0f + 0.5f) / 100.0f;
    return font_size;
}

typedef struct
{
    const char* font_name;
    double      font_size;
    int         font_bold;
    int         font_italic;
    matrix_t*   ctm_prev;
    /* todo: add extract_odt_styles_t member? */
} content_state_t;
/* Used to keep track of font information when writing paragraphs of odt
content, e.g. so we know whether a font has changed so need to start a new odt
span. */


static int extract_document_to_odt_content_paragraph(
        extract_alloc_t*        alloc,
        content_state_t*        state,
        paragraph_t*            paragraph,
        extract_astring_t*      content,
        extract_odt_styles_t*   styles
        )
/* Append odt xml for <paragraph> to <content>. Updates *state if we change
font. */
{
    int e = -1;
    int l;

    if (extract_odt_paragraph_start(alloc, content)) goto end;

    for (l=0; l<paragraph->lines_num; ++l) {
        line_t* line = paragraph->lines[l];
        int s;
        for (s=0; s<line->spans_num; ++s) {
            int si;
            span_t* span = line->spans[s];
            double font_size_new;
            state->ctm_prev = &span->ctm;
            font_size_new = matrices_to_font_size(&span->ctm, &span->trm);
            if (!state->font_name
                    || strcmp(span->font_name, state->font_name)
                    || span->font_bold != state->font_bold
                    || span->font_italic != state->font_italic
                    || font_size_new != state->font_size
                    ) {
                if (state->font_name) {
                    if (extract_odt_run_finish(alloc, content)) goto end;
                }
                state->font_name = span->font_name;
                state->font_bold = span->font_bold;
                state->font_italic = span->font_italic;
                state->font_size = font_size_new;
                if (extract_odt_run_start(
                        alloc,
                        content,
                        styles,
                        state->font_name,
                        state->font_size,
                        state->font_bold,
                        state->font_italic
                        )) goto end;
            }

            for (si=0; si<span->chars_num; ++si) {
                char_t* char_ = &span->chars[si];
                int c = char_->ucs;

                if (0) {}

                /* Escape XML special characters. */
                else if (c == '<')  extract_odt_char_append_string(alloc, content, "&lt;");
                else if (c == '>')  extract_odt_char_append_string(alloc, content, "&gt;");
                else if (c == '&')  extract_odt_char_append_string(alloc, content, "&amp;");
                else if (c == '"')  extract_odt_char_append_string(alloc, content, "&quot;");
                else if (c == '\'') extract_odt_char_append_string(alloc, content, "&apos;");

                /* Expand ligatures. */
                else if (c == 0xFB00) {
                    if (extract_odt_char_append_string(alloc, content, "ff")) goto end;
                }
                else if (c == 0xFB01) {
                    if (extract_odt_char_append_string(alloc, content, "fi")) goto end;
                }
                else if (c == 0xFB02) {
                    if (extract_odt_char_append_string(alloc, content, "fl")) goto end;
                }
                else if (c == 0xFB03) {
                    if (extract_odt_char_append_string(alloc, content, "ffi")) goto end;
                }
                else if (c == 0xFB04) {
                    if (extract_odt_char_append_string(alloc, content, "ffl")) goto end;
                }

                /* Output ASCII verbatim. */
                else if (c >= 32 && c <= 127) {
                    if (extract_odt_char_append_char(alloc, content, (char) c)) goto end;
                }

                /* Escape all other characters. */
                else {
                    char    buffer[32];
                    snprintf(buffer, sizeof(buffer), "&#x%x;", c);
                    if (extract_odt_char_append_string(alloc, content, buffer)) goto end;
                }
            }
            /* Remove any trailing '-' at end of line. */
            if (extract_odt_char_truncate_if(content, '-')) goto end;
        }
    }
    if (state->font_name) {
        if (extract_odt_run_finish(alloc, content)) goto end;
        state->font_name = NULL;
    }
    if (extract_odt_paragraph_finish(alloc, content)) goto end;
    
    e = 0;
    
    end:
    return e;
}

static int extract_document_append_image(
        extract_alloc_t*    alloc,
        extract_astring_t*  content,
        image_t*            image
        )
/* Write reference to image into odt content. */
{
    extract_odt_char_append_string(alloc, content, "\n");
    extract_odt_char_append_string(alloc, content, "     <w:p>\n");
    extract_odt_char_append_string(alloc, content, "       <w:r>\n");
    extract_odt_char_append_string(alloc, content, "         <w:rPr>\n");
    extract_odt_char_append_string(alloc, content, "           <w:noProof/>\n");
    extract_odt_char_append_string(alloc, content, "         </w:rPr>\n");
    extract_odt_char_append_string(alloc, content, "         <w:drawing>\n");
    extract_odt_char_append_string(alloc, content, "           <wp:inline distT=\"0\" distB=\"0\" distL=\"0\" distR=\"0\" wp14:anchorId=\"7057A832\" wp14:editId=\"466EB3FB\">\n");
    extract_odt_char_append_string(alloc, content, "             <wp:extent cx=\"2933700\" cy=\"2200275\"/>\n");
    extract_odt_char_append_string(alloc, content, "             <wp:effectExtent l=\"0\" t=\"0\" r=\"0\" b=\"9525\"/>\n");
    extract_odt_char_append_string(alloc, content, "             <wp:docPr id=\"1\" name=\"Picture 1\"/>\n");
    extract_odt_char_append_string(alloc, content, "             <wp:cNvGraphicFramePr>\n");
    extract_odt_char_append_string(alloc, content, "               <a:graphicFrameLocks xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" noChangeAspect=\"1\"/>\n");
    extract_odt_char_append_string(alloc, content, "             </wp:cNvGraphicFramePr>\n");
    extract_odt_char_append_string(alloc, content, "             <a:graphic xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\">\n");
    extract_odt_char_append_string(alloc, content, "               <a:graphicData uri=\"http://schemas.openxmlformats.org/drawingml/2006/picture\">\n");
    extract_odt_char_append_string(alloc, content, "                 <pic:pic xmlns:pic=\"http://schemas.openxmlformats.org/drawingml/2006/picture\">\n");
    extract_odt_char_append_string(alloc, content, "                   <pic:nvPicPr>\n");
    extract_odt_char_append_string(alloc, content, "                     <pic:cNvPr id=\"1\" name=\"Picture 1\"/>\n");
    extract_odt_char_append_string(alloc, content, "                     <pic:cNvPicPr>\n");
    extract_odt_char_append_string(alloc, content, "                       <a:picLocks noChangeAspect=\"1\" noChangeArrowheads=\"1\"/>\n");
    extract_odt_char_append_string(alloc, content, "                     </pic:cNvPicPr>\n");
    extract_odt_char_append_string(alloc, content, "                   </pic:nvPicPr>\n");
    extract_odt_char_append_string(alloc, content, "                   <pic:blipFill>\n");
    extract_odt_char_append_stringf(alloc, content,"                     <a:blip r:embed=\"%s\">\n", image->id);
    extract_odt_char_append_string(alloc, content, "                       <a:extLst>\n");
    extract_odt_char_append_string(alloc, content, "                         <a:ext uri=\"{28A0092B-C50C-407E-A947-70E740481C1C}\">\n");
    extract_odt_char_append_string(alloc, content, "                           <a14:useLocalDpi xmlns:a14=\"http://schemas.microsoft.com/office/drawing/2010/main\" val=\"0\"/>\n");
    extract_odt_char_append_string(alloc, content, "                         </a:ext>\n");
    extract_odt_char_append_string(alloc, content, "                       </a:extLst>\n");
    extract_odt_char_append_string(alloc, content, "                     </a:blip>\n");
    //extract_docx_char_append_string(alloc, content, "                     <a:srcRect/>\n");
    extract_odt_char_append_string(alloc, content, "                     <a:stretch>\n");
    extract_odt_char_append_string(alloc, content, "                       <a:fillRect/>\n");
    extract_odt_char_append_string(alloc, content, "                     </a:stretch>\n");
    extract_odt_char_append_string(alloc, content, "                   </pic:blipFill>\n");
    extract_odt_char_append_string(alloc, content, "                   <pic:spPr bwMode=\"auto\">\n");
    extract_odt_char_append_string(alloc, content, "                     <a:xfrm>\n");
    extract_odt_char_append_string(alloc, content, "                       <a:off x=\"0\" y=\"0\"/>\n");
    extract_odt_char_append_string(alloc, content, "                       <a:ext cx=\"2933700\" cy=\"2200275\"/>\n");
    extract_odt_char_append_string(alloc, content, "                     </a:xfrm>\n");
    extract_odt_char_append_string(alloc, content, "                     <a:prstGeom prst=\"rect\">\n");
    extract_odt_char_append_string(alloc, content, "                       <a:avLst/>\n");
    extract_odt_char_append_string(alloc, content, "                     </a:prstGeom>\n");
    extract_odt_char_append_string(alloc, content, "                     <a:noFill/>\n");
    extract_odt_char_append_string(alloc, content, "                     <a:ln>\n");
    extract_odt_char_append_string(alloc, content, "                       <a:noFill/>\n");
    extract_odt_char_append_string(alloc, content, "                     </a:ln>\n");
    extract_odt_char_append_string(alloc, content, "                   </pic:spPr>\n");
    extract_odt_char_append_string(alloc, content, "                 </pic:pic>\n");
    extract_odt_char_append_string(alloc, content, "               </a:graphicData>\n");
    extract_odt_char_append_string(alloc, content, "             </a:graphic>\n");
    extract_odt_char_append_string(alloc, content, "           </wp:inline>\n");
    extract_odt_char_append_string(alloc, content, "         </w:drawing>\n");
    extract_odt_char_append_string(alloc, content, "       </w:r>\n");
    extract_odt_char_append_string(alloc, content, "     </w:p>\n");
    extract_odt_char_append_string(alloc, content, "\n");
    return 0;
}


static int extract_document_output_rotated_paragraphs(
        extract_alloc_t*    alloc,
        page_t*             page,
        int                 paragraph_begin,
        int                 paragraph_end,
        int                 rot,
        int                 x,
        int                 y,
        int                 w,
        int                 h,
        int                 text_box_id,
        extract_astring_t*  content,
        extract_odt_styles_t* styles,
        content_state_t*    state
        )
/* Writes paragraph to content inside rotated text box. */
{
    int e = 0;
    int p;
    outf("x,y=%ik,%ik = %i,%i", x/1000, y/1000, x, y);
    extract_odt_char_append_string(alloc, content, "\n");
    extract_odt_char_append_string(alloc, content, "\n");
    extract_odt_char_append_string(alloc, content, "<w:p>\n");
    extract_odt_char_append_string(alloc, content, "  <w:r>\n");
    extract_odt_char_append_string(alloc, content, "    <mc:AlternateContent>\n");
    extract_odt_char_append_string(alloc, content, "      <mc:Choice Requires=\"wps\">\n");
    extract_odt_char_append_string(alloc, content, "        <w:drawing>\n");
    extract_odt_char_append_string(alloc, content, "          <wp:anchor distT=\"0\" distB=\"0\" distL=\"0\" distR=\"0\" simplePos=\"0\" relativeHeight=\"0\" behindDoc=\"0\" locked=\"0\" layoutInCell=\"1\" allowOverlap=\"1\" wp14:anchorId=\"53A210D1\" wp14:editId=\"2B7E8016\">\n");
    extract_odt_char_append_string(alloc, content, "            <wp:simplePos x=\"0\" y=\"0\"/>\n");
    extract_odt_char_append_string(alloc, content, "            <wp:positionH relativeFrom=\"page\">\n");
    extract_odt_char_append_stringf(alloc, content,"              <wp:posOffset>%i</wp:posOffset>\n", x);
    extract_odt_char_append_string(alloc, content, "            </wp:positionH>\n");
    extract_odt_char_append_string(alloc, content, "            <wp:positionV relativeFrom=\"page\">\n");
    extract_odt_char_append_stringf(alloc, content,"              <wp:posOffset>%i</wp:posOffset>\n", y);
    extract_odt_char_append_string(alloc, content, "            </wp:positionV>\n");
    extract_odt_char_append_stringf(alloc, content,"            <wp:extent cx=\"%i\" cy=\"%i\"/>\n", w, h);
    extract_odt_char_append_string(alloc, content, "            <wp:effectExtent l=\"381000\" t=\"723900\" r=\"371475\" b=\"723900\"/>\n");
    extract_odt_char_append_string(alloc, content, "            <wp:wrapNone/>\n");
    extract_odt_char_append_stringf(alloc, content,"            <wp:docPr id=\"%i\" name=\"Text Box %i\"/>\n", text_box_id, text_box_id);
    extract_odt_char_append_string(alloc, content, "            <wp:cNvGraphicFramePr/>\n");
    extract_odt_char_append_string(alloc, content, "            <a:graphic xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\">\n");
    extract_odt_char_append_string(alloc, content, "              <a:graphicData uri=\"http://schemas.microsoft.com/office/word/2010/wordprocessingShape\">\n");
    extract_odt_char_append_string(alloc, content, "                <wps:wsp>\n");
    extract_odt_char_append_string(alloc, content, "                  <wps:cNvSpPr txBox=\"1\"/>\n");
    extract_odt_char_append_string(alloc, content, "                  <wps:spPr>\n");
    extract_odt_char_append_stringf(alloc, content,"                    <a:xfrm rot=\"%i\">\n", rot);
    extract_odt_char_append_string(alloc, content, "                      <a:off x=\"0\" y=\"0\"/>\n");
    extract_odt_char_append_string(alloc, content, "                      <a:ext cx=\"3228975\" cy=\"2286000\"/>\n");
    extract_odt_char_append_string(alloc, content, "                    </a:xfrm>\n");
    extract_odt_char_append_string(alloc, content, "                    <a:prstGeom prst=\"rect\">\n");
    extract_odt_char_append_string(alloc, content, "                      <a:avLst/>\n");
    extract_odt_char_append_string(alloc, content, "                    </a:prstGeom>\n");

    /* Give box a solid background. */
    if (0) {
        extract_odt_char_append_string(alloc, content, "                    <a:solidFill>\n");
        extract_odt_char_append_string(alloc, content, "                      <a:schemeClr val=\"lt1\"/>\n");
        extract_odt_char_append_string(alloc, content, "                    </a:solidFill>\n");
        }

    /* Draw line around box. */
    if (0) {
        extract_odt_char_append_string(alloc, content, "                    <a:ln w=\"175\">\n");
        extract_odt_char_append_string(alloc, content, "                      <a:solidFill>\n");
        extract_odt_char_append_string(alloc, content, "                        <a:prstClr val=\"black\"/>\n");
        extract_odt_char_append_string(alloc, content, "                      </a:solidFill>\n");
        extract_odt_char_append_string(alloc, content, "                    </a:ln>\n");
    }

    extract_odt_char_append_string(alloc, content, "                  </wps:spPr>\n");
    extract_odt_char_append_string(alloc, content, "                  <wps:txbx>\n");
    extract_odt_char_append_string(alloc, content, "                    <w:txbxContent>");

    #if 0
    if (0) {
        /* Output inline text describing the rotation. */
        extract_odt_char_append_stringf(content, "<w:p>\n"
                "<w:r><w:rPr><w:rFonts w:ascii=\"OpenSans\" w:hAnsi=\"OpenSans\"/><w:sz w:val=\"20.000000\"/><w:szCs w:val=\"15.000000\"/></w:rPr><w:t xml:space=\"preserve\">*** rotate: %f rad, %f deg. rot=%i</w:t></w:r>\n"
                "</w:p>\n",
                rotate,
                rotate * 180 / pi,
                rot
                );
    }
    #endif

    /* Output paragraphs p0..p2-1. */
    for (p=paragraph_begin; p<paragraph_end; ++p) {
        paragraph_t* paragraph = page->paragraphs[p];
        if (extract_document_to_odt_content_paragraph(alloc, state, paragraph, content, styles)) goto end;
    }

    extract_odt_char_append_string(alloc, content, "\n");
    extract_odt_char_append_string(alloc, content, "                    </w:txbxContent>\n");
    extract_odt_char_append_string(alloc, content, "                  </wps:txbx>\n");
    extract_odt_char_append_string(alloc, content, "                  <wps:bodyPr rot=\"0\" spcFirstLastPara=\"0\" vertOverflow=\"overflow\" horzOverflow=\"overflow\" vert=\"horz\" wrap=\"square\" lIns=\"91440\" tIns=\"45720\" rIns=\"91440\" bIns=\"45720\" numCol=\"1\" spcCol=\"0\" rtlCol=\"0\" fromWordArt=\"0\" anchor=\"t\" anchorCtr=\"0\" forceAA=\"0\" compatLnSpc=\"1\">\n");
    extract_odt_char_append_string(alloc, content, "                    <a:prstTxWarp prst=\"textNoShape\">\n");
    extract_odt_char_append_string(alloc, content, "                      <a:avLst/>\n");
    extract_odt_char_append_string(alloc, content, "                    </a:prstTxWarp>\n");
    extract_odt_char_append_string(alloc, content, "                    <a:noAutofit/>\n");
    extract_odt_char_append_string(alloc, content, "                  </wps:bodyPr>\n");
    extract_odt_char_append_string(alloc, content, "                </wps:wsp>\n");
    extract_odt_char_append_string(alloc, content, "              </a:graphicData>\n");
    extract_odt_char_append_string(alloc, content, "            </a:graphic>\n");
    extract_odt_char_append_string(alloc, content, "          </wp:anchor>\n");
    extract_odt_char_append_string(alloc, content, "        </w:drawing>\n");
    extract_odt_char_append_string(alloc, content, "      </mc:Choice>\n");

    /* This fallback is copied from a real Word document. Not sure
    whether it works - both Libreoffice and Word use the above
    choice. */
    extract_odt_char_append_string(alloc, content, "      <mc:Fallback>\n");
    extract_odt_char_append_string(alloc, content, "        <w:pict>\n");
    extract_odt_char_append_string(alloc, content, "          <v:shapetype w14:anchorId=\"53A210D1\" id=\"_x0000_t202\" coordsize=\"21600,21600\" o:spt=\"202\" path=\"m,l,21600r21600,l21600,xe\">\n");
    extract_odt_char_append_string(alloc, content, "            <v:stroke joinstyle=\"miter\"/>\n");
    extract_odt_char_append_string(alloc, content, "            <v:path gradientshapeok=\"t\" o:connecttype=\"rect\"/>\n");
    extract_odt_char_append_string(alloc, content, "          </v:shapetype>\n");
    extract_odt_char_append_stringf(alloc, content,"          <v:shape id=\"Text Box %i\" o:spid=\"_x0000_s1026\" type=\"#_x0000_t202\" style=\"position:absolute;margin-left:71.25pt;margin-top:48.75pt;width:254.25pt;height:180pt;rotation:-2241476fd;z-index:251659264;visibility:visible;mso-wrap-style:square;mso-wrap-distance-left:9pt;mso-wrap-distance-top:0;mso-wrap-distance-right:9pt;mso-wrap-distance-bottom:0;mso-position-horizontal:absolute;mso-position-horizontal-relative:text;mso-position-vertical:absolute;mso-position-vertical-relative:text;v-text-anchor:top\" o:gfxdata=\"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&#10;bFBLBQYAAAAABAAEAPMAAAC8BQAAAAA=&#10;\" fillcolor=\"white [3201]\" strokeweight=\".5pt\">\n", text_box_id);
    extract_odt_char_append_string(alloc, content, "            <v:textbox>\n");
    extract_odt_char_append_string(alloc, content, "              <w:txbxContent>");

    for (p=paragraph_begin; p<paragraph_end; ++p) {
        paragraph_t* paragraph = page->paragraphs[p];
        if (extract_document_to_odt_content_paragraph(alloc, state, paragraph, content, styles)) goto end;
    }

    extract_odt_char_append_string(alloc, content, "\n");
    extract_odt_char_append_string(alloc, content, "\n");
    extract_odt_char_append_string(alloc, content, "              </w:txbxContent>\n");
    extract_odt_char_append_string(alloc, content, "            </v:textbox>\n");
    extract_odt_char_append_string(alloc, content, "          </v:shape>\n");
    extract_odt_char_append_string(alloc, content, "        </w:pict>\n");
    extract_odt_char_append_string(alloc, content, "      </mc:Fallback>\n");
    extract_odt_char_append_string(alloc, content, "    </mc:AlternateContent>\n");
    extract_odt_char_append_string(alloc, content, "  </w:r>\n");
    extract_odt_char_append_string(alloc, content, "</w:p>");
    e = 0;
    end:
    return e;
}


int extract_document_to_odt_content(
        extract_alloc_t*    alloc,
        document_t*         document,
        int                 spacing,
        int                 rotation,
        int                 images,
        extract_astring_t*  content,
        extract_odt_styles_t* styles
        )
{
    int ret = -1;
    int text_box_id = 0;
    int p;

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
            
            if (spacing
                    && state.ctm_prev
                    && paragraph->lines_num
                    && paragraph->lines[0]->spans_num
                    && matrix_cmp4(
                            state.ctm_prev,
                            &paragraph->lines[0]->spans[0]->ctm
                            )
                    ) {
                /* Extra vertical space between paragraphs that were at
                different angles in the original document. */
                if (extract_odt_paragraph_empty(alloc, content, styles)) goto end;
            }

            if (spacing) {
                /* Extra vertical space between paragraphs. */
                if (extract_odt_paragraph_empty(alloc, content, styles)) goto end;
            }
            
            if (rotation && rotate != 0) {
            
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
                text_box_id += 1;
                
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

                    if (extract_document_output_rotated_paragraphs(alloc, page, p0, p1, rot, x, y, w, h, text_box_id, content, styles, &state)) goto end;
                }
                p = p1 - 1;
                //p = page->paragraphs_num - 1;
            }
            else {
                if (extract_document_to_odt_content_paragraph(alloc, &state, paragraph, content, styles)) goto end;
            }
        
        }
        
        if (images) {
            int i;
            for (i=0; i<page->images_num; ++i) {
                extract_document_append_image(alloc, content, &page->images[i]);
            }
        }
    }
    ret = 0;

    end:

    return ret;
}



static int systemf(extract_alloc_t* alloc, const char* format, ...)
/* Like system() but takes printf-style format and args. Also, if we return +ve
we set errno to EIO. */
{
    int e;
    char* command;
    va_list va;
    va_start(va, format);
    e = extract_vasprintf(alloc, &command, format, va);
    va_end(va);
    if (e < 0) return e;
    outf("running: %s", command);
    e = system(command);
    extract_free(alloc, &command);
    if (e > 0) {
        errno = EIO;
    }
    return e;
}

static int read_all(extract_alloc_t* alloc, FILE* in, char** o_out)
/* Reads until eof into zero-terminated malloc'd buffer. */
{
    size_t  len = 0;
    size_t  delta = 128;
    for(;;) {
        size_t n;
        if (extract_realloc2(alloc, o_out, len, len + delta + 1)) {
            extract_free(alloc, o_out);
            return -1;
        }
        n = fread(*o_out + len, 1 /*size*/, delta /*nmemb*/, in);
        len += n;
        if (feof(in)) {
            (*o_out)[len] = 0;
            return 0;
        }
        if (ferror(in)) {
            /* It's weird that fread() and ferror() don't set errno. */
            errno = EIO;
            extract_free(alloc, o_out);
            return -1;
        }
    }
}

static int read_all_path(extract_alloc_t* alloc, const char* path, char** o_text)
/* Reads entire file into zero-terminated malloc'd buffer. */
{
    int e = -1;
    FILE* f = NULL;
    f = fopen(path, "rb");
    if (!f) goto end;
    if (read_all(alloc, f, o_text)) goto end;
    e = 0;
    end:
    if (f) fclose(f);
    if (e) extract_free(alloc, &o_text);
    return e;
}

static int write_all(const void* data, size_t data_size, const char* path)
{
    int e = -1;
    FILE* f = fopen(path, "w");
    if (!f) goto end;
    if (fwrite(data, data_size, 1 /*nmemb*/, f) != 1) goto end;
    e = 0;
    end:
    if (f) fclose(f);
    return e;
}

static int extract_odt_content_insert(
        extract_alloc_t*    alloc,
        const char*         original,
        const char*         single_name,
        const char*         mid_begin_name,
        const char*         mid_end_name,
        extract_astring_t*  contentss,
        int                 contentss_num,
        char**              o_out
        )
/* Creates a string consisting of <original> with all strings in <contentss>
inserted into <original>'s <mid_begin_name>...<mid_end_name> region, and
appends this string to *o_out.

If <mid_begin_name> is NULL, we insert into the zero-length region before
<mid_end_name>.

If <mid_end_name> is NULL, we insert into the zero-length region after
<mid_begin_name>.

At least one of <mid_begin_name> and <mid_end_name> must be non-NULL.
*/
{
    int e = -1;
    const char* mid_begin = NULL;
    const char* mid_end = NULL;
    const char* single = NULL;
    extract_astring_t   out;
    extract_astring_init(&out);
    
    assert(single || mid_begin_name || mid_end_name);
    
    if (single_name) single = strstr(original, single_name);
    
    if (single)
    {
        outf("Have found single_name='%s', using in preference to mid_begin_name=%s mid_end_name=%s",
                single_name,
                mid_begin_name,
                mid_end_name
                );
        mid_begin = single;
        mid_end = single + strlen(single_name);
    }
    else
    {
        if (mid_begin_name) {
            mid_begin = strstr(original, mid_begin_name);
            if (!mid_begin) {
                outf("error: could not find '%s' in odt content", mid_begin_name);
                errno = ESRCH;
                goto end;
            }
            mid_begin += strlen(mid_begin_name);
        }
        if (mid_end_name) {
            mid_end = strstr(mid_begin ? mid_begin : original, mid_end_name);
            if (!mid_end) {
                outf("error: could not find '%s' in odt content", mid_end_name);
                e = -1;
                errno = ESRCH;
                goto end;
            }
        }
        if (!mid_begin) {
            mid_begin = mid_end;
        }
        if (!mid_end) {
            mid_end = mid_begin;
        }
    }

    if (extract_astring_catl(alloc, &out, original, mid_begin - original)) goto end;
    {
        int i;
        for (i=0; i<contentss_num; ++i) {
            if (extract_astring_catl(alloc, &out, contentss[i].chars, contentss[i].chars_num)) goto end;
        }
    }
    if (extract_astring_cat(alloc, &out, mid_end)) goto end;
    
    *o_out = out.chars;
    out.chars = NULL;
    e = 0;
    
    end:
    if (e) {
        extract_astring_free(alloc, &out);
        *o_out = NULL;
    }
    return e;
}


#if 0
static int s_find_mid(const char* text, const char* begin, const char* end, const char** o_begin, const char** o_end)
/* Sets *o_begin to end of first occurrence of <begin> in <text>, and *o_end to
beginning of first occurtence of <end> in <text>. */
{
    *o_begin = strstr(text, begin);
    if (!*o_begin) goto fail;
    *o_begin += strlen(begin);
    *o_end = strstr(*o_begin, end);
    if (!*o_end) goto fail;
    return 0;
    fail:
    errno = ESRCH;
    return -1;
}
#endif

int extract_odt_content_item(
        extract_alloc_t*    alloc,
        extract_astring_t*  contentss,
        int                 contentss_num,
        extract_odt_styles_t* styles,
        images_t*           images,
        const char*         name,
        const char*         text,
        char**              text2
        )
{
    int e = -1;
    extract_astring_t   temp;
    extract_astring_init(&temp);
    *text2 = NULL;
    
    (void) images;
    if (0)
    {}
    #if 0
    else if (!strcmp(name, "[Content_Types].xml")) {
        /* Add information about all image types that we are going to use. */
        const char* begin;
        const char* end;
        const char* insert;
        int it;
        extract_astring_free(alloc, &temp);
        outf("text: %s", text);
        if (s_find_mid(text, "<Types ", "</Types>", &begin, &end)) goto end;

        insert = begin;
        insert = strchr(insert, '>');
        assert(insert);
        insert += 1;

        if (extract_astring_catl(alloc, &temp, text, insert - text)) goto end;
        outf("images->imagetypes_num=%i", images->imagetypes_num);
        for (it=0; it<images->imagetypes_num; ++it) {
            const char* imagetype = images->imagetypes[it];
            if (extract_astring_cat(alloc, &temp, "<Default Extension=\"")) goto end;
            if (extract_astring_cat(alloc, &temp, imagetype)) goto end;
            if (extract_astring_cat(alloc, &temp, "\" ContentType=\"image/")) goto end;
            if (extract_astring_cat(alloc, &temp, imagetype)) goto end;
            if (extract_astring_cat(alloc, &temp, "\"/>")) goto end;
        }
        if (extract_astring_cat(alloc, &temp, insert)) goto end;
        *text2 = temp.chars;
        extract_astring_init(&temp);
    }
    else if (!strcmp(name, "word/_rels/document.xml.rels")) {
        /* Add relationships between image ids and image names within odt
        archive. */
        const char* begin;
        const char* end;
        int         j;
        extract_astring_free(alloc, &temp);
        if (s_find_mid(text, "<Relationships", "</Relationships>", &begin, &end)) goto end;
        if (extract_astring_catl(alloc, &temp, text, end - text)) goto end;
        outf("images.images_num=%i", images->images_num);
        for (j=0; j<images->images_num; ++j) {
            image_t* image = &images->images[j];
            if (extract_astring_cat(alloc, &temp, "<Relationship Id=\"")) goto end;
            if (extract_astring_cat(alloc, &temp, image->id)) goto end;
            if (extract_astring_cat(alloc, &temp, "\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/image\" Target=\"media/")) goto end;
            if (extract_astring_cat(alloc, &temp, image->name)) goto end;
            if (extract_astring_cat(alloc, &temp, "\"/>")) goto end;
        }
        if (extract_astring_cat(alloc, &temp, end)) goto end;
        *text2 = temp.chars;
        extract_astring_init(&temp);
    }
    #endif
    else if (!strcmp(name, "content.xml")) {
        /* Insert paragraphs content. */
        char* text_intermediate = NULL;
        extract_astring_t   styles_definitions = {0};

        if (extract_odt_content_insert(
                alloc,
                text,
                NULL /*single*/,
                NULL,
                "</office:text>",
                contentss,
                contentss_num,
                &text_intermediate
                )) goto end;
        outf("text_intermediate: %s", text_intermediate);
        
        if (extract_odt_styles_definitions(alloc, styles, &styles_definitions)) goto end;
        
        e = extract_odt_content_insert(
                alloc,
                text_intermediate,
                "<office:automatic-styles/>" /*single*/,
                NULL,
                "</office:automatic-styles>",
                &styles_definitions,
                1,
                text2
                );
        outf("e=%i errno=%i", e, errno);
        extract_free(alloc, &text_intermediate);
        extract_astring_free(alloc, &styles_definitions);
        outf("e=%i errno=%i", e, errno);
        if (e) goto end;
    }
    else {
        *text2 = NULL;
    }
    e = 0;
    end:
    outf("e=%i errno=%i text2=%s", e, errno, text2);
    if (e) {
        /* We might have set <text2> to new content. */
        extract_free(alloc, text2);
        /* We might have used <temp> as a temporary buffer. */
        extract_astring_free(alloc, &temp);
    }
    extract_astring_init(&temp);
    return e;
}

        

static int check_path_shell_safe(const char* path)
/* Returns -1 with errno=EINVAL if <path> contains sequences that could make it
unsafe in shell commands. */
{
    if (0
            || strstr(path, "..")
            || strchr(path, '\'')
            || strchr(path, '"')
            || strchr(path, ' ')
            ) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

static int remove_directory(extract_alloc_t* alloc, const char* path)
{
    if (check_path_shell_safe(path)) {
        outf("path_out is unsafe: %s", path);
        return -1;
    }
    return systemf(alloc, "rm -r '%s'", path);
}

#ifdef _WIN32
#include <direct.h>
static int s_mkdir(const char* path, int mode)
{
    (void) mode;
    return _mkdir(path);
}
#else
static int s_mkdir(const char* path, int mode)
{
    return mkdir(path, mode);
}
#endif


int extract_odt_write_template(
        extract_alloc_t*    alloc,
        extract_astring_t*  contentss,
        int                 contentss_num,
        //extract_odt_styles_t* styles,
        images_t*           images,
        const char*         path_template,
        const char*         path_out,
        int                 preserve_dir
        )
{
    int     e = -1;
    int     i;
    char*   path_tempdir = NULL;
    FILE*   f = NULL;
    char*   path = NULL;
    char*   text = NULL;
    char*   text2 = NULL;
    extract_odt_styles_t    styles = {0};

    assert(path_out);
    assert(path_template);
    
    if (check_path_shell_safe(path_out)) {
        outf("path_out is unsafe: %s", path_out);
        goto end;
    }

    outf("images->images_num=%i", images->images_num);
    if (extract_asprintf(alloc, &path_tempdir, "%s.dir", path_out) < 0) goto end;
    if (systemf(alloc, "rm -r '%s' 2>/dev/null", path_tempdir) < 0) goto end;

    if (s_mkdir(path_tempdir, 0777)) {
        outf("Failed to create directory: %s", path_tempdir);
        goto end;
    }

    outf("Unzipping template document '%s' to tempdir: %s",
            path_template, path_tempdir);
    if (systemf(alloc, "unzip -q -d '%s' '%s'", path_tempdir, path_template))
    {
        outf("Failed to unzip %s into %s",
                path_template, path_tempdir);
        goto end;
    }

    /* Might be nice to iterate through all items in path_tempdir, but for now
    we look at just the items that we know extract_odt_content_item() will
    modify. */
    
    {
        const char* names[] = {
                "content.xml",
                };
        int names_num = sizeof(names) / sizeof(names[0]);
        for (i=0; i<names_num; ++i) {
            const char* name = names[i];
            extract_free(alloc, &path);
            extract_free(alloc, &text);
            extract_free(alloc, &text2);
            if (extract_asprintf(alloc, &path, "%s/%s", path_tempdir, name) < 0) goto end;
            if (read_all_path(alloc, path, &text)) goto end;
            
            if (extract_odt_content_item(
                    alloc,
                    contentss,
                    contentss_num,
                    &styles,
                    images,
                    name,
                    text,
                    &text2
                    ))
            {
                outf("extract_odt_content_item() failed");
                goto end;
            }

            {
                const char* text3 = (text2) ? text2 : text;
                if (write_all(text3, strlen(text3), path)) goto end;
                outf("have written to path=%s", path);
            }
        }
    }

    /* Copy images into <path_tempdir>/media/. */
    #if 0
    outf("");
    extract_free(alloc, &path);
    if (extract_asprintf(alloc, &path, "%s/word/media", path_tempdir) < 0) goto end;
    if (s_mkdir(path, 0777))
    {
        outf("Failed to mkdir %s", path);
        goto end;
    }
    #endif
    
    outf("");
    for (i=0; i<images->images_num; ++i) {
        image_t* image = &images->images[i];
        extract_free(alloc, &path);
        if (extract_asprintf(alloc, &path, "%s/word/media/%s", path_tempdir, image->name) < 0) goto end;
        if (write_all(image->data, image->data_size, path)) goto end;
    }
    
    outf("Zipping tempdir to create %s", path_out);
    {
        const char* path_out_leaf = strrchr(path_out, '/');
        if (!path_out_leaf) path_out_leaf = path_out;
        if (systemf(alloc, "cd '%s' && zip -q -r -D '../%s' .", path_tempdir, path_out_leaf))
        {
            outf("Zip command failed to convert '%s' directory into output file: %s",
                    path_tempdir, path_out);
            goto end;
        }
    }

    if (!preserve_dir) {
        if (remove_directory(alloc, path_tempdir)) goto end;
    }

    e = 0;

    end:
    outf("e=%i", e);
    extract_free(alloc, &path_tempdir);
    extract_free(alloc, &path);
    extract_free(alloc, &text);
    extract_free(alloc, &text2);
    extract_odt_styles_free(alloc, &styles);
    if (f)  fclose(f);

    if (e) {
        outf("Failed to create %s", path_out);
    }
    return e;
}