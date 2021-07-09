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
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/stat.h>


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

static void content_state_init(content_state_t* state)
{
    state->font_name = NULL;
    state->font_size = 0;
    state->font_bold = 0;
    state->font_italic = 0;
    state->ctm_prev = NULL;
}

int content_state_reset(extract_alloc_t* alloc, content_state_t* state, extract_astring_t* content)
{
    int e = -1;
    if (state->font_bold)
    {
        if (extract_astring_cat(alloc, content, "</b>")) goto end;
        state->font_bold = 0;
    }
    if (state->font_italic)
    {
        if (extract_astring_cat(alloc, content, "</i>")) goto end;
        state->font_italic = 0;
    }
    e = 0;
    
    end:
    return e;
}

static int paragraph_to_html_content(
        extract_alloc_t*    alloc,
        content_state_t*    state,
        paragraph_t*        paragraph,
        int                 single_line,
        extract_astring_t*  content
        )
{
    int e = -1;
    char* endl = (single_line) ? "" : "\n";
    int l;
    if (extract_astring_catf(alloc, content, "%s%s<p>", endl, endl)) goto end;

    for (l=0; l<paragraph->lines_num; ++l)
    {
        line_t* line = paragraph->lines[l];
        int s;
        for (s=0; s<line->spans_num; ++s)
        {
            int c;
            span_t* span = line->spans[s];
            double font_size_new;
            state->ctm_prev = &span->ctm;
            font_size_new = extract_matrices_to_font_size(&span->ctm, &span->trm);
            if (span->font_bold != state->font_bold)
            {
                if (extract_astring_cat(alloc, content,
                        span->font_bold ? "<b>" : "</b>"
                        )) goto end;
                state->font_bold = span->font_bold;
            }
            if (span->font_italic != state->font_italic)
            {
                if ( extract_astring_cat(alloc, content,
                        span->font_italic ? "<i>" : "</i>"
                        )) goto end;
                state->font_italic = span->font_italic;
            }

            for (c=0; c<span->chars_num; ++c)
            {
                char_t* char_ = &span->chars[c];
                if (extract_astring_cat_xmlc(alloc, content, char_->ucs)) goto end;
                //extract_astring_cat(alloc, content, "[eos]");
            }
            /* Remove any trailing '-' at end of line. */
            //extract_astring_cat(alloc, content, "[eos]");
            //if (extract_html_char_truncate_if(content, '-')) goto end;
        }

        if (content->chars_num && l+1 < paragraph->lines_num)
        {
            if (content->chars[content->chars_num-1] == '-')    content->chars_num -= 1;
            else if (content->chars[content->chars_num-1] != ' ')
            {
                extract_astring_catc(alloc, content, ' ');
            }
        }
    }
    /*if (state->font_name)
    {
        if (extract_html_run_finish(alloc, content)) goto end;
        state->font_name = NULL;
    }*/
    if (extract_astring_catf(alloc, content, "%s</p>", endl)) goto end;
    
    e = 0;
    
    end:
    return e;
}


static int paragraphs_to_html_content(
        extract_alloc_t*    alloc,
        content_state_t*    state,
        paragraph_t**       paragraphs,
        int                 paragraphs_num,
        int                 single_line,
        extract_astring_t*  content
        )
/* Append html for paragraphs[] to <content>. Updates *state if we change font
etc. */
{
    int e = -1;
    int p;
    for (p=0; p<paragraphs_num; ++p)
    {
        paragraph_t* paragraph = paragraphs[p];
        if (paragraph_to_html_content(alloc, state, paragraph, single_line, content)) goto end;
    }
    
    if (content_state_reset(alloc, state, content)) goto end;
    e = 0;
    
    end:
    return e;
}

static int append_table(extract_alloc_t* alloc, content_state_t* state, table_t* table, extract_astring_t* content)
{
    int e = -1;
    extract_astring_cat(alloc, content, "\n\n<table border=\"1\" style=\"border-collapse:collapse\">\n");
    int y;
    int i;

    if (0)
    {
        /* Show raw cells info. */
        for (y=0; y<table->cells_num_y; ++y)
        {
            int x;
            for (x=0; x<table->cells_num_x; ++x)
            {
                cell_t* cell = table->cells[y*table->cells_num_x + x];
                fprintf(stderr,
                        " [i=% 4i (% 3i % 3i) l=%i a=%i ix_extend=%i iy_extend=% 3i]",
                        i, cell->ix, cell->iy, cell->left, cell->above, cell->ix_extend, cell->iy_extend
                        );
                /*fprintf(stderr, cell->left ? "|" : " ");
                fprintf(stderr, cell->above ? "-" : " ");
                fprintf(stderr,   " ");*/                      
            }
            fprintf(stderr, "\n");
        }
    }

    outf("table->cells_num_x=%i", table->cells_num_x);
    outf("table->cells_num_y=%i", table->cells_num_y);
    for (y=0; y<table->cells_num_y; ++y)
    {
        int x;
        extract_astring_cat(alloc, content, "    <tr>\n        ");
        for (x=0; x<table->cells_num_x; ++x)
        {
            cell_t* cell = table->cells[y*table->cells_num_x + x];
            if (!cell->above || !cell->left) continue;
            extract_astring_cat(alloc, content, "<td");
            if (cell->ix_extend > 1)
            {
                extract_astring_catf(alloc, content, " colspan=\"%i\"", cell->ix_extend);
            }
            if (cell->iy_extend > 1)
            {
                extract_astring_catf(alloc, content, " rowspan=\"%i\"", cell->iy_extend);
            }
            extract_astring_cat(alloc, content, ">");

            extract_astring_t text = {NULL, 0};
            //if (get_paragraphs_text(alloc, cell->paragraphs, cell->paragraphs_num, &text)) goto end;
            if (paragraphs_to_html_content(alloc, state, cell->paragraphs, cell->paragraphs_num, 1 /* single_line*/, &text)) goto end;
            if (text.chars)
            {
                extract_astring_cat(alloc, content, text.chars);
            }
            extract_astring_cat(alloc, content, "</td>");

            extract_astring_free(alloc, &text);
            if (content_state_reset(alloc, state, content)) goto end;
        }
        extract_astring_cat(alloc, content, "\n    </tr>\n");
    }
    extract_astring_cat(alloc, content, "</table>\n\n");
    e = 0;
    end:
    return e;
}


#if 0
static int get_paragraphs_text(
        extract_alloc_t* alloc,
        paragraph_t** paragraphs,
        int paragraphs_num,
        extract_astring_t* text
        )
{
    int p;
    for (p=0; p<paragraphs_num; ++p)
    {
        paragraph_t* paragraph = paragraphs[p];
        int l;
        for (l=0; l<paragraph->lines_num; ++l)
        {
            line_t* line = paragraph->lines[l];
            int s;
            for (s=0; s<line->spans_num; ++s)
            {
                span_t* span = line->spans[s];
                int c;
                for (c=0; c<span->chars_num; ++c)
                {
                    char_t* char_ = &span->chars[c];
                    int cc = char_->ucs;
                    if (extract_astring_cat_xmlc(alloc, text, cc)) return -1;
                }
            }
            if (extract_astring_catc(alloc, text, ' ')) return -1;
        }
    }
    return 0;
}
#endif

int extract_document_to_html_content(
        extract_alloc_t*    alloc,
        document_t*         document,
        int                 rotation,
        int                 images,
        extract_astring_t*  content
        )
{
    int ret = -1;
    int p;
    
    (void) rotation;
    (void) images;
    
    extract_astring_cat(alloc, content, "<html>\n");
    extract_astring_cat(alloc, content, "<body>\n");
    
    /* Write paragraphs into <content>. */
    for (p=0; p<document->pages_num; ++p)
    {
        page_t* page = document->pages[p];
        content_state_t state;
        content_state_init(&state);
        
        /* Output paragraphs and tables in order of increasing <y> coordinate. */
        int p = 0;
        int t = 0;
        for(;;)
        {
            paragraph_t* paragraph = (p == page->paragraphs_num) ? NULL : page->paragraphs[p];
            table_t* table = (t == page->tables_num) ? NULL : page->tables[t];
            if (!paragraph && !table) break;
            double y_paragraph = (paragraph) ? paragraph->lines[0]->spans[0]->chars[0].y : DBL_MAX;
            double y_table = (table) ? table->pos.y : DBL_MAX;
            if (y_paragraph < y_table)
            {
                if (paragraph_to_html_content(alloc, &state, paragraph, 0 /*single_line*/, content)) goto end;
                if (content_state_reset(alloc, &state, content)) goto end;
                p += 1;
            }
            else
            {
                if (append_table(alloc, &state, table, content)) goto end;
                t += 1;
            }
        }
        #if 0
        if (paragraphs_to_html_content(alloc, &state, page->paragraphs, page->paragraphs_num, 0 /*single_line*/, content)) goto end;
        
        {
            int t;
            outf("page->tables_num=%i", page->tables_num);
            for (t=0; t<page->tables_num; ++t)
            {
                table_t* table = page->tables[t];
                if (append_table(alloc, &state, table, content)) goto end;
            }
        }
        #endif
    }
    extract_astring_cat(alloc, content, "</body>\n");
    extract_astring_cat(alloc, content, "</html>\n");
    ret = 0;

    end:

    return ret;
}
