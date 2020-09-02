#ifndef ARTIFEX_AUTOSTRING_XML
#define ARTIFEX_AUTOSTRING_XML

/* A simple string struct that reallocs as required. */
typedef struct
{
    char*   chars;      /* NULL or zero-terminated. */
    int     chars_num;  /* Length of string pointed to by .chars. */
} string_t;

void string_init(string_t* string);

void string_free(string_t* string);

int string_catl(string_t* string, const char* s, int s_len);

int string_catc(string_t* string, char c);

int string_cat(string_t* string, const char* s);

#endif
