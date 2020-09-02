#ifndef ARTIFEX_EXTRACT_XML
#define ARTIFEX_EXTRACT_XML

#include "astring.h"

#include <stdio.h>


/* Things for representing XML. */

typedef struct {
    char*   name;
    char*   value;
} extract_xml_attribute_t;

/* Represents a single <...> XML tag plus trailing text. */
typedef struct {
    char*                       name;
    extract_xml_attribute_t*    attributes;
    int                         attributes_num;
    extract_astring_t           text;
} extract_xml_tag_t;

/* Sets all fields to NULL, so will cause memory leaks if fields have not been
freed. */
void extract_xml_tag_init(extract_xml_tag_t* tag);

void extract_xml_tag_free(extract_xml_tag_t* tag);


/* extract_xml_pparse_*(): simple XML 'pull' parser.

extract_xml_pparse_init() merely consumes the initial '<'. Thereafter extract_xml_pparse_next()
consumes the next '<' before returning the previous tag. */

/* Opens specified file.

If first_line is not NULL, we check that it matches the first line in the file.

Returns NULL with errno set if error. */
FILE* extract_xml_pparse_init(const char* path, const char* first_line);


/* Returns the next XML tag.

Returns 0 with *out containing next tag; or -1 with errno set if error; or +1
with errno=ESRCH if EOF.

*out is initially passed to extract_xml_tag_free(), so *out must have been initialised,
e.g. by by extract_xml_tag_init(). */
int extract_xml_pparse_next(FILE* in, extract_xml_tag_t* out);

/* Returns pointer to value of specified attribute, or NULL if not found. */
char* extract_xml_tag_attributes_find(extract_xml_tag_t* tag, const char* name);

/* Finds float value of specified attribute, returning error if not found. We
use atof() and don't check for non-numeric attribute value. */
int extract_xml_tag_attributes_find_float(
        extract_xml_tag_t*  tag,
        const char* name,
        float*      o_out
        );

int extract_xml_tag_attributes_find_int(
        extract_xml_tag_t*  tag,
        const char* name,
        int*        o_out
        );

#endif
