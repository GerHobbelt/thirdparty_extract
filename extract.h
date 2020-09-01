#pragma once

/*
Unless otherwise stated, all functions return 0 on success or -1 with errno
set.
*/

/* Simple debug output. */
void (extract_outf)(
        const char* file, int line,
        const char* fn,
        int ln,
        const char* format,
        ...
        )
;

#define extract_outf(format, ...) (extract_outf)(__FILE__, __LINE__, __FUNCTION__, 1 /*ln*/, format, ##__VA_ARGS__)
#define extract_outfx(format, ...)


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

/* A single char in a span. */
typedef struct
{
    /* (x,y) before transformation by ctm and trm. */
    float       pre_x;
    float       pre_y;
    
    float       x;
    float       y;
    
    int         gid;
    unsigned    ucs;
    float       adv;
} extract_char_t;

/* List of chars that have same font. */
typedef struct extract_span_t
{
    extract_matrix_t    ctm;
    extract_matrix_t    trm;
    char*               font_name;
    
    /* font size is matrix_expansion(trm). */
    
    int                 font_bold;
    int                 font_italic;
    int                 wmode;
    extract_char_t*     chars;
    int                 chars_num;
} extract_span_t;

/* List of spans that are aligned on same line. */
typedef struct
{
    extract_span_t**    spans;
    int                 spans_num;
} extract_line_t;

/* A list of lines that are aligned and adjacent to each other so as to form a
paragraph. */
typedef struct
{
    extract_line_t**    lines;
    int                 lines_num;
} extract_paragraph_t;

/* A page. Contains different representations of the same list of spans. */
typedef struct
{
    extract_span_t**    spans;
    int                 spans_num;

    /* .lines[] eventually points to items in .spans. */
    extract_line_t**    lines;
    int                 lines_num;

    /* .paragraphs[] eventually points to items in .lines. */
    extract_paragraph_t**   paragraphs;
    int                     paragraphs_num;
} extract_page_t;

/* List of pages. */
typedef struct {
    extract_page_t**    pages;
    int                 pages_num;
} extract_document_t;

void extract_document_init(extract_document_t* document);

void extract_document_free(extract_document_t* document);


/* Reads from intermediate format in file <path> into document.

path;
    Path of file containng intermediate format.
document:
    Is populated with pages etc from intermediate format.
autosplit:
    If true, we split spans when y coordinate changes, in order to stress out
    joining algorithms.
*/
int extract_read_spans_raw(
        const char*         path,
        extract_document_t* document,
        int                 autosplit
        );

/* Reads from document and converts into docx content.

document:
    Should contain raw intermediate data e.g. from extract_read_spans_raw().
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

/*
Writes docx content into a new .docx document.

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
int extract_docx_create(
        extract_string_t*   content,
        const char*         path_template,
        const char*         path_out,
        int                 preserve_dir
        );
