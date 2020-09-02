#include "autostring.h"

#include <stdlib.h>
#include <string.h>


void string_init(string_t* string)
{
    string->chars = NULL;
    string->chars_num = 0;
}

void string_free(string_t* string)
{
    free(string->chars);
    string_init(string);
}

int string_catl(string_t* string, const char* s, int s_len)
{
    char* chars = realloc(string->chars, string->chars_num + s_len + 1);
    if (!chars) return -1;
    memcpy(chars + string->chars_num, s, s_len);
    chars[string->chars_num + s_len] = 0;
    string->chars = chars;
    string->chars_num += s_len;
    return 0;
}

int string_catc(string_t* string, char c)
{
    return string_catl(string, &c, 1);
}

int string_cat(string_t* string, const char* s)
{
    return string_catl(string, s, strlen(s));
}

