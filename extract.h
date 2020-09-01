#pragma once

/*
Unless otherwise stated, all functions return 0 on success or -1 with errno
set.
*/

void extract_outf(const char* file, int line, const char* fn, int ln, const char* format, ...);

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

typedef struct
{
    float       pre_x;
    float       pre_y;
    float       x;
    float       y;
    int         gid;
    unsigned    ucs;
    float       adv;
} extract_char_t;

typedef struct extract_span_t
{
    extract_matrix_t    ctm;
    extract_matrix_t    trm;
    char*       font_name;
    /* font size is matrix_expansion(trm). */
    int         font_bold;
    int         font_italic;
    int         wmode;
    extract_char_t*     chars;
    int         chars_num;
} extract_span_t;

/* List of spans that are aligned on same line. */
typedef struct
{
    extract_span_t**    spans;
    int         spans_num;
} extract_line_t;

/* A list of lines that are aligned and adjacent to each other so as to form a
paragraph. */
typedef struct
{
    extract_line_t**    lines;
    int         lines_num;
} extract_paragraph_t;

/* A page. */
typedef struct
{
    extract_span_t**        spans;
    int             spans_num;

    /* .lines[] eventually points to items in .spans. */
    extract_line_t**        lines;
    int             lines_num;

    /* .paragraphs[] eventually points to items in .lines. */
    extract_paragraph_t**   paragraphs;
    int             paragraphs_num;
} extract_page_t;

typedef struct {
    extract_page_t**    pages;
    int         pages_num;
} extract_document_t;

void extract_document_init(extract_document_t* document);

void extract_document_free(extract_document_t* document);


/* Reads from intermediate format in file <path> into document_t.

autosplit:
    If true, we split spans when y coordinate changes.
debugscale:
    If not zero, scale ctm by debugscale and trm by 1/debugscale; intended for
    use with ghostscript output, but this doesn't work yet.
*/
int extract_read_spans_raw(
        const char* path,
        extract_document_t* document,
        int autosplit,
        float debugscale
        );

/* Reads from intermediate data and converts into docx content. On return
*content points to zero-terminated content, allocated by realloc(). */
int extract_document_to_docx_content(
        extract_document_t* document,
        extract_string_t* content,
        int spacing,
        float debugscale
        );

/*
Creates a .docx file based on a template, by inserting <content> into
word/document.xml.

content:
    E.g. from process().
path_out:
    Name of .docx file to create. Must not contain single-quote character.
path_template:
    Name of .docx file to use as a template.
preserve_dir:
    If true, we don't delete the temporary directory <path_out>.dir containing
    unzipped .docx content.

Returns 0 on success or -1 with errno set.

We use the 'zip' and 'unzip' commands.
*/

int extract_docx_create(
        extract_string_t* content,
        const char* path_template,
        const char* path_out,
        int preserve_dir
        );
