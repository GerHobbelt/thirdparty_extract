#include "outf.h"
#include "xml.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>


/* These str_*() functions realloc buffer as required. All return 0 or -1 with
errno set. */

/* Appends first <s_len> chars of string <s> to *p. */
static int str_catl(char** p, const char* s, int s_len)
{
    int p_len = (*p) ? strlen(*p) : 0;
    char* pp = realloc(*p, p_len + s_len + 1);
    if (!pp)    return -1;
    memcpy(pp + p_len, s, s_len);
    pp[p_len + s_len] = 0;
    *p = pp;
    return 0;
}

/* Appends a char.  */
static int str_catc(char** p, char c)
{
    return str_catl(p, &c, 1);
}

/* Unused but usefult o keep code here. */
#if 0
/* Appends a string. */
static int str_cat(char** p, const char* s)
{
    return str_catl(p, s, strlen(s));
}
#endif

char* extract_xml_tag_attributes_find(extract_xml_tag_t* tag, const char* name)
{
    for (int i=0; i<tag->attributes_num; ++i) {
        if (!strcmp(tag->attributes[i].name, name)) {
            char* ret = tag->attributes[i].value;
            return ret;
        }
    }
    outf("Failed to find attribute '%s'",name);
    return NULL;
}

/* Finds float value of specified attribute, returning error if not found. We
use atof() and don't check for non-numeric attribute value. */
int extract_xml_tag_attributes_find_float(
        extract_xml_tag_t*  tag,
        const char* name,
        float*      o_out
        )
{
    const char* value = extract_xml_tag_attributes_find(tag, name);
    if (!value) {
        errno = ESRCH;
        return -1;
    }
    *o_out = atof(value);
    return 0;
}

int extract_xml_tag_attributes_find_int(
        extract_xml_tag_t*  tag,
        const char* name,
        int*        o_out
        )
{
    const char* value = extract_xml_tag_attributes_find(tag, name);
    if (!value) {
        errno = ESRCH;
        return -1;
    }
    *o_out = atoi(value);
    return 0;
}

static int extract_xml_tag_attributes_append(extract_xml_tag_t* tag, char* name, char* value)
{
    extract_xml_attribute_t* a = realloc(
            tag->attributes,
            (tag->attributes_num+1) * sizeof(extract_xml_attribute_t)
            );
    if (!a) return -1;
    tag->attributes = a;
    tag->attributes[tag->attributes_num].name = name;
    tag->attributes[tag->attributes_num].value = value;
    tag->attributes_num += 1;
    return 0;
}

void extract_xml_tag_init(extract_xml_tag_t* tag)
{
    tag->name = NULL;
    tag->attributes = NULL;
    tag->attributes_num = 0;
    extract_astring_init(&tag->text);
}

void extract_xml_tag_free(extract_xml_tag_t* tag)
{
    free(tag->name);
    int i;
    for (i=0; i<tag->attributes_num; ++i) {
        extract_xml_attribute_t* attribute = &tag->attributes[i];
        free(attribute->name);
        free(attribute->value);
    }
    free(tag->attributes);
    extract_astring_free(&tag->text);
    extract_xml_tag_init(tag);
}

/* Unused but useful to keep code here. */
#if 0
/* Like strcmp() but also handles NULL. */
static int extract_xml_strcmp_null(const char* a, const char* b)
{
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    return strcmp(a, b);
}
#endif

/* Unused but usefult o keep code here. */
#if 0
/* Compares tag name, then attributes; returns -1, 0 or +1. Does not compare
extract_xml_tag_t::text members. */
int extract_xml_compare_tags(const extract_xml_tag_t* lhs, const extract_xml_tag_t* rhs)
{
    int d;
    d = extract_xml_strcmp_null(lhs->name, rhs->name);
    if (d)  return d;
    for(int i=0;; ++i) {
        if (i >= lhs->attributes_num || i >= rhs->attributes_num) {
            break;
        }
        const extract_xml_attribute_t* lhs_attribute = &lhs->attributes[i];
        const extract_xml_attribute_t* rhs_attribute = &rhs->attributes[i];
        d = extract_xml_strcmp_null(lhs_attribute->name, rhs_attribute->name);
        if (d)  return d;
        d = extract_xml_strcmp_null(lhs_attribute->value, rhs_attribute->value);
        if (d)  return d;
    }
    if (lhs->attributes_num > rhs->attributes_num) return +1;
    if (lhs->attributes_num < rhs->attributes_num) return -1;
    return 0;
}
#endif


FILE* extract_xml_pparse_init(const char* path, const char* first_line)
{
    FILE* in = NULL;
    char* buffer = NULL;
    int e = 1;

    in = fopen(path, "r");
    if (!in) {
        outf("error: Could not open filename=%s", path);
        goto end;
    }

    if (first_line) {
        size_t first_line_len = strlen(first_line);
        buffer = malloc(first_line_len + 1);
        if (!buffer) goto end;

        ssize_t n = fread(buffer, first_line_len, 1 /*nmemb*/, in);
        if (n != 1) {
            outf("error: fread() failed. n=%zi. path='%s'", n, path);
            goto end;
        }
        buffer[first_line_len] = 0;
        if (strcmp(first_line, buffer)) {
            outf("Unrecognised prefix in path=%s: %s", path, buffer);
            errno = ESRCH;
            goto end;
        }
    }

    for(;;) {
        int c = getc(in);
        if (c == '<') {
            break;
        }
        else if (c == ' ' || c == '\n') {}
        else {
            outf("Expected '<' but found c=%i", c);
            goto end;
        }
    }
    e = 0;

    end:
    free(buffer);
    if (e) {
        if (in) {
            fclose(in);
            in = NULL;
        }
    }
    return in;
}


int extract_xml_pparse_next(FILE* in, extract_xml_tag_t* out)
{
    int ret = -1;
    extract_xml_tag_free(out);

    char*   attribute_name = NULL;
    char*   attribute_value = NULL;

    extract_xml_tag_init(out);
    char c;

    assert(in);

    /* Read tag name. */
    int i = 0;
    for( i=0;; ++i) {
        c = getc(in);
        if (c == EOF) {
            if (i == 0 && feof(in)) {
                /* Legitimate EOF. We provide a reasonable errno value if
                caller isn't expecting EOF and doesn't test explicitly for +1.
                */
                ret = +1;
                errno = ESRCH;
            }
            goto end;
        }
        if (c == '>' || c == ' ')  break;
        if (str_catc(&out->name, c)) goto end;
    }
    if (c == ' ') {

        /* Read attributes. */
        for(;;) {

            /* Read attribute name. */
            for(;;) {
                c = getc(in);
                if (c == EOF) {
                    errno = -ESRCH;
                    goto end;
                }
                if (c == '=' || c == '>' || c == ' ') break;
                if (str_catc(&attribute_name, c)) goto end;
            }
            if (c == '>') break;

            if (c == '=') {
                /* Read attribute value. */
                int quote_single = 0;
                int quote_double = 0;
                for(;;) {
                    c = getc(in);
                    if (c == '\'')      quote_single = !quote_single;
                    else if (c == '"')  quote_double = !quote_double;
                    else if (!quote_single && !quote_double
                            && (c == ' ' || c == '/' || c == '>')
                            ) {
                        /* We are at end of attribute value. */
                        break;
                    }
                    else if (c == '\\') {
                        // Escape next character.
                        c = getc(in);
                        if (c == EOF) {
                            errno = ESRCH;
                            goto end;
                        }
                    }
                    if (str_catc(&attribute_value, c)) goto end;
                }

                /* Remove any enclosing quotes. */
                int l = strlen(attribute_value);
                if (l >= 2) {
                    if (
                            (attribute_value[0] == '"' && attribute_value[l-1] == '"')
                            ||
                            (attribute_value[0] == '\'' && attribute_value[l-1] == '\'')
                            ) {
                        memmove(attribute_value, attribute_value+1, l-2);
                        attribute_value[l-2] = 0;
                    }
                }
            }

            if (extract_xml_tag_attributes_append(out, attribute_name, attribute_value)) goto end;
            attribute_name = NULL;
            attribute_value = NULL;
            if (c == '/') c = getc(in);
            if (c == '>') break;
        }
    }

    /* Read plain text until next '<'. */
    for(;;) {
        c = getc(in);
        if (c == '<' || feof(in)) break;
        if (extract_astring_catc(&out->text, c)) goto end;
    }

    ret = 0;

    end:

    free(attribute_name);
    free(attribute_value);
    if (ret) {
        extract_xml_tag_free(out);
    }
    return ret;
}

