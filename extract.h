#pragma once

/*
Unless otherwise stated, all functions return 0 on success or -1 with errno
set.
*/

/* A simple string struct that reallocs as required. */
typedef struct
{
    char*   chars;      /* NULL or zero-terminated. */
    int     chars_num;  /* Length of string pointed to by .chars. */
} extract_string_t;

void extract_string_init(extract_string_t* string);

void extract_string_free(extract_string_t* string);

int extract_string_catl(extract_string_t* string, const char* s, int s_len);

int extract_string_catc(extract_string_t* string, char c);

int extract_string_cat(extract_string_t* string, const char* s);

typedef struct
{
    float a;
    float b;
    float c;
    float d;
    float e;
    float f;
} extract_matrix_t;

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
} extract_char_t;

/* Array of chars that have same font and are usually adjacent.
*/
typedef struct extract_span_t
{
    extract_matrix_t    ctm;
    extract_matrix_t    trm;
    char*               font_name;
    
    /* font size is matrix_expansion(trm). */
    
    struct {
        int font_bold   : 1;
        int font_italic : 1;
        int wmode       : 1;
    };
    
    extract_char_t*     chars;
    int                 chars_num;
} extract_span_t;

/* Array of pointers to spans that are aligned on same line.
*/
typedef struct
{
    extract_span_t**    spans;
    int                 spans_num;
} extract_line_t;

/* Array of pointers to lines that are aligned and adjacent to each other so as
to form a paragraph. */
typedef struct
{
    extract_line_t**    lines;
    int                 lines_num;
} extract_paragraph_t;

/* A page. Contains different representations of the same list of spans.
*/
typedef struct
{
    extract_span_t**    spans;
    int                 spans_num;

    /* .lines[] refers to items in .spans. */
    extract_line_t**    lines;
    int                 lines_num;

    /* .paragraphs[] refers to items in .lines. */
    extract_paragraph_t**   paragraphs;
    int                     paragraphs_num;
} extract_page_t;

/* Array of pointers to pages.
*/
typedef struct {
    extract_page_t**    pages;
    int                 pages_num;
} extract_document_t;

void extract_document_init(extract_document_t* document);

void extract_document_free(extract_document_t* document);


/* Reads from intermediate format file into a document.

path;
    Path of file containg intermediate format.
document:
    Is populated with pages from intermediate format. Each page will have
    spans, but no lines or paragraphs; use extract_document_join() to create
    lines and paragraphs.
autosplit:
    If true, we split spans when the y coordinate changes, in order to stress
    out joining algorithms.

Returns with *document's pages containing spans, but no lines or paragraphs.
*/
int extract_intermediate_to_document(
        const char*         path,
        extract_document_t* document,
        int                 autosplit
        );

/* Finds lines and paragraphs in document (e.g. from
extract_intermediate_to_document()).

document:
    Should have spans, but no lines or paragraphs.
    
Returns with *document containing lines and paragraphs.
*/
int extract_document_join(extract_document_t* document);


/* Reads from document and converts into docx content.

document:
    Should contain paragraphs e.g. from extract_document_join().
content:
    Out-param. On return will contain docx content.
spacing:
    If non-zero, we add extra vertical space between paragraphs.
*/
int extract_document_to_docx_content(
        extract_document_t* document,
        extract_string_t*   content,
        int                 spacing
        );


/* Writes docx content (e.g. from extract_document_to_docx_content()) into a
new .docx file.

content:
    E.g. from extract_document_to_docx_content().
path_template:
    Name of .docx file to use as a template.
path_out:
    Name of .docx file to create. Must not contain single-quote character.
preserve_dir:
    If true, we don't delete the temporary directory <path_out>.dir containing
    unzipped .docx content.

Returns 0 on success or -1 with errno set.

Uses the 'zip' and 'unzip' commands internally.
*/
int extract_docx_content_to_docx(
        extract_string_t*   content,
        const char*         path_template,
        const char*         path_out,
        int                 preserve_dir
        );
