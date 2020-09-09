#ifndef ARTIFEX_EXTRACT_BUFFER_H
#define ARTIFEX_EXTRACT_BUFFER_H


typedef struct extract_buffer_t extract_buffer_t;
/* Abstract state for buffer API. */

typedef int (*extract_buffer_fn_get)(void* handle, char** o_data, int* o_data_length);
/* Should get more data and use out-parameters to inform where the data is.

handle:
    As passed to extract_buffer_open().
o_data:
    Out-param, should point to new data.
o_data_length:
    Out-param, should contain length of new data.

On EOF should return 0 with *o_data_length = 0;
*/

typedef void (*extract_buffer_fn_close)(void* handle);
/* Called by extract_buffer_close().

handle:
    As passed to extract_buffer_open().
*/

int extract_buffer_open(
        void*                   handle,
        extract_buffer_fn_get   open,
        extract_buffer_fn_close close,
        extract_buffer_t**      o_buffer
        );
/* Creates an extract_buffer_t that uses specified functions for getting data.

handle:
    Passed to open() and close() callbacks.
openfn:
    Callback used to get more data.
closefn:
    Callback called by extract_buffer_close().
o_buffer:
    Out-param.
*/

void extract_buffer_close(extract_buffer_t* buffer);
/* Closes down an extract_buffer_t and frees all internal resources. */


int extract_buffer_file_open(const char* path, extract_buffer_t** o_buffer);
/* Creates an extract_buffer_t that reads from a file.

path:
    Path of file to read from.
o_buffer:
    Out-param.
*/

int extract_buffer_internal_get_more(extract_buffer_t* buffer);
/* Internal use only. */

int extract_buffer_read(extract_buffer_t* buffer, char* out_buffer, int out_buffer_length);
/* Reads from buffer into out_buffer. Returns +1 if unable to get
out_buffer_length bytes due to EOF.

buffer:
    As returned by earlier call to extract_buffer_open().
out_buffer:
    Location for copied data.
out_buffer_length:
    Length of out_buffer.
*/


/* Implementation details below are to allow extract_buffer_getc() to be
inline. */

struct extract_buffer_t
{
    void*                   handle;
    extract_buffer_fn_get   get;
    extract_buffer_fn_close close;
    char*                   data;
    int                     data_length;
    int                     data_pos;
};
/* Internal only; defined here only so that extract_buffer_getc() can be
inline. */

int extract_buffer_getc_internal(extract_buffer_t* buffer, char* out);
/* Internal only. */

static inline int extract_buffer_getc(extract_buffer_t* buffer, char* out)
/* Inline function to read one character from an extract_buffer_t. Writes next
char to *out and returns zero, or returns -ve error, or returns +1 on EOF. */
{
    if (buffer->data_length == buffer->data_pos) {
        return extract_buffer_getc_internal(buffer, out);
    }
    *out = buffer->data[buffer->data_pos];
    buffer->data_pos += 1;
    return 0;
}


#endif
