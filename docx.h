#ifndef ARTIFEX_DOCX_H
#define ARTIFEX_DOCX_H

/* Starts a new run. Caller must ensure that docx_run_finish() was called to
terminate any previous run. */
int docx_run_start(
        extract_astring_t* content,
        const char* font_name,
        float font_size,
        int bold,
        int italic
        );

int docx_run_finish(extract_astring_t* content);

int docx_paragraph_start(extract_astring_t* content);

int docx_paragraph_finish(extract_astring_t* content);

int docx_char_append_string(extract_astring_t* content, char* text);

int docx_char_append_char(extract_astring_t* content, char c);

/* Removes last char if it is <c>. */
int docx_char_truncate_if(extract_astring_t* content, char c);


/* Append an empty paragraph. */
int docx_paragraph_empty(extract_astring_t* content);


int extract_docx_content_to_docx(
        const char* content,
        int         content_length,
        const char* path_template,
        const char* path_out,
        int         preserve_dir
        );

#endif
