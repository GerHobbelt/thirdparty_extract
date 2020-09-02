#pragma once

/*
Unless otherwise stated, all functions return 0 on success or -1 with errno
set.
*/


typedef struct extract_document_t extract_document_t;
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
        const char*             path,
        extract_document_t**    o_document,
        int                     autosplit
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
        int                 spacing,
        char**              o_content,
        int*                o_content_length
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
        const char*         content,
        int                 content_length,
        const char*         path_template,
        const char*         path_out,
        int                 preserve_dir
        );
