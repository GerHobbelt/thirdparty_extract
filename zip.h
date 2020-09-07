#ifndef ARTIFEX_EXTRACT_ZIP
#define ARTIFEX_EXTRACT_ZIP

#include <stddef.h>


/* Functions for creating zip files.

Content is uncompressed.

Unless otherwise stated, all functions return 0 on success or -1 with errno
set.
*/

typedef struct extract_zip_t extract_zip_t;
/* Abstract handle for zipfile state. */


int extract_zip_open(
        const char*     path,
        const char*     mode,
        extract_zip_t** o_zip
        );
/* Opens new zip file.

path:
    Path of zip file.
mode:
    Passed to fopen(), should be "w" (intended to allow reading/writing in
    the future, but unnecessary for now).
o_zip:
    Out-param.
*/


int extract_zip_write_file(
        extract_zip_t*  zip,
        const void*     data,
        size_t          data_length,
        const char*     name
        );
/* Writes specified data into the zip file.

zip:
    From extract_zip_open().
data:
    File contents.
data_length:
    Length in bytes of file contents.
name:
    Name of file within the zip file.
*/


int extract_zip_close(extract_zip_t* zip);
/* Finishes writing the zip file (e.g. appends Central directory file headers
and End of central directory record.

zip:
    From extract_zip_open().
*/

#endif
