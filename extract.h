#ifndef ARITFEX_EXTRACT_H
#define ARITFEX_EXTRACT_H


/* Functions for extracting paragraphs of text from intermediate format data
created by these commands:

    mutool draw -F xmltext ...
    gs -sDEVICE=txtwrite -dTextFormat=4 ... 

Unless otherwise stated, all functions return 0 on success or -1 with errno
set.
*/


typedef struct extract_document_t extract_document_t;
/* Contains characters, spans, lines, paragraphs and pages. */

void extract_document_free(extract_document_t* document);
/* Frees a document.
*/


int extract_intermediate_to_document(
        const char*             path,
        int                     autosplit,
        extract_document_t**    o_document
        );
/* Reads from intermediate format file into a document.

path;
    Path of file containg intermediate format. Intermediate format can be
    created with these commands:
        mutool draw -F xmltext ...
        gs -sDEVICE=txtwrite -dTextFormat=4 ...
autosplit:
    If true, we split spans when the y coordinate changes, in order to stress
    the joining algorithms.
o_document:
    Out-param: *o_document is set to internal data populated with pages from
    intermediate format. Each page will have spans, but no lines or paragraphs;
    use extract_document_join() to create lines and paragraphs.

    *o_document should be freed with extract_document_free().

Returns with *o_document set. On error *o_document=NULL.
*/


int extract_document_join(extract_document_t* document);
/* Finds lines and paragraphs in document.

document:
    Should have spans, but no lines or paragraphs, e.g. from
    extract_intermediate_to_document().
    
Returns with document containing lines and paragraphs.
*/


int extract_document_to_docx_content(
        extract_document_t* document,
        int                 spacing,
        char**              o_content,
        int*                o_content_length
        );
/* Reads from document and converts into docx content.

document:
    Should contain paragraphs e.g. from extract_document_join().
spacing:
    If non-zero, we add extra vertical space between paragraphs.
o_content:
    Out param: set to point to zero-terminated text in buffer from malloc().
o_content_length:
    Out-param: set to length of returned string.

On error *o_content=NULL and *o_content_length=0.
*/


int extract_docx_content_to_docx(
        const char* content,
        int         content_length,
        const char* path_template,
        const char* path_out,
        int         preserve_dir
        );
/* Writes docx content (e.g. from extract_document_to_docx_content()) into a
new .docx file.

content:
    E.g. from extract_document_to_docx_content().
content_length:
    Length of content.
path_template:
    Name of .docx file to use as a template.
preserve_dir:
    If true, we don't delete the temporary directory <path_out>.dir containing
    unzipped .docx content.
path_out:
    Name of .docx file to create. Must not contain single-quote character or
    '..'.

Uses the 'zip' and 'unzip' commands internally.
*/

#endif
