#ifndef ARTIFEX_EXTRACT_XML
#define ARTIFEX_EXTRACT_XML

/* Only for internal use by extract code.  */

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


void extract_xml_tag_init(extract_xml_tag_t* tag);
/* Initialises tag. Will cause leak if tag contains data - in this case call
extract_xml_tag_free(). */

void extract_xml_tag_free(extract_xml_tag_t* tag);
/* Frees tag and then calls extract_xml_tag_init(). */


FILE* extract_xml_pparse_init(const char* path, const char* first_line);
/* extract_xml_pparse_*(): simple XML 'pull' parser.

extract_xml_pparse_init() merely consumes the initial '<'. Thereafter extract_xml_pparse_next()
consumes the next '<' before returning the previous tag. */

/* Opens specified file.

If first_line is not NULL, we check that it matches the first line in the file.

Returns NULL with errno set if error. */


int extract_xml_pparse_next(FILE* in, extract_xml_tag_t* out);
/* Returns the next XML tag.

Returns 0 with *out containing next tag; or -1 with errno set if error; or +1
with errno=ESRCH if EOF.

*out is initially passed to extract_xml_tag_free(), so *out must have been initialised,
e.g. by by extract_xml_tag_init(). */


char* extract_xml_tag_attributes_find(extract_xml_tag_t* tag, const char* name);
/* Returns pointer to value of specified attribute, or NULL if not found. */

int extract_xml_tag_attributes_find_float(
        extract_xml_tag_t*  tag,
        const char*         name,
        float*              o_out
        );
/* Finds float value of specified attribute, returning error if not found. We
use atof() and don't check for non-numeric attribute value. */


int extract_xml_tag_attributes_find_int(
        extract_xml_tag_t*  tag,
        const char*         name,
        int*                o_out
        );
/* Finds int value of specified attribute, returning error if not found. We
use atof() and don't check for non-numeric attribute value. */

#endif
