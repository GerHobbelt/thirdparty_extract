#ifndef ARTIFEX_OUTF_H
#define ARTIFEX_OUTF_H

/* Only for internal use by extract code.  */

/* Simple printf-style debug output. */
void (outf)(
        const char* file, int line,
        const char* fn,
        int ln,
        const char* format,
        ...
        );

#define outf(format, ...) \
        (outf)(__FILE__, __LINE__, __FUNCTION__, 1 /*ln*/, format, ##__VA_ARGS__)

#define outfx(format, ...)


#endif
