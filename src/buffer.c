#include "../include/extract_buffer.h"

#include "outf.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>


int extract_buffer_open(
        void*                   handle,
        extract_buffer_fn_get   get,
        extract_buffer_fn_close close,
        extract_buffer_t**      o_buffer
        )
{
    int e = -1;
    extract_buffer_t* buffer = malloc(sizeof(*buffer));
    if (!buffer) goto end;
    
    buffer->handle = handle;
    buffer->get = get;
    buffer->close = close;
    buffer->data = NULL;
    buffer->data_length = 0;
    buffer->data_pos = 0;
    e = 0;
    
    end:
    if (e) {
        free(buffer);
        *o_buffer = NULL;
    }
    else {
        *o_buffer = buffer;
    }
    return e;
}

void extract_buffer_close(extract_buffer_t* buffer)
{
    if (!buffer) {
        return;
    }
    if (buffer->close) {
        buffer->close(buffer->handle);
    }
    free(buffer);
}

int extract_buffer_internal_get_more(extract_buffer_t* buffer)
{
    assert(buffer);
    assert(buffer->data_pos == buffer->data_length);
    int e = buffer->get(buffer->handle, &buffer->data, &buffer->data_length);
    if (e) {
        buffer->data_length = 0;
        buffer->data_pos = 0;
        return e;
    }
    if (buffer->data_length == 0) {
        buffer->data_pos = 0;
        return +1;
    }
    buffer->data_pos = 0;
    return 0;
}

int extract_buffer_read(extract_buffer_t* buffer, char* out, int out_len)
{
    int out_pos = 0;
    for(;;) {
        if (out_pos == out_len) {
            return 0;
        }
        int n = buffer->data_length - buffer->data_pos;
        if (n == 0) {
            if (extract_buffer_internal_get_more(buffer)) return -1;
            if (buffer->data_length == 0) {
                /* EOF */
                return +1;
            }
            n = buffer->data_length - buffer->data_pos;
        }
        if (n > out_len - out_pos) {
            n = out_len - out_pos;
        }
        memcpy(out + out_pos, buffer->data + buffer->data_pos, n);
        out_pos += n;
        buffer->data_pos += n;
    }
}

/* Implementation of extract_buffer_file_*. */

typedef struct
{
    int     fd;
    char    cache[4096];
} extract_buffer_file_t;

static int extract_buffer_file_get(void* handle, char** o_data, int* o_data_size)
{
    extract_buffer_file_t* file = handle;
    ssize_t e = read(file->fd, file->cache, sizeof(file->cache));
    if (e < 0) return e;
    *o_data = file->cache;
    *o_data_size = e;
    return 0;
}

static void extract_buffer_file_close(void* handle)
{
    extract_buffer_file_t* file = handle;
    if (!file) return;
    close(file->fd);
    free(file);
}

int extract_buffer_file_open(const char* path, extract_buffer_t** o_buffer)
{
    int e = -1;
    extract_buffer_file_t* file = malloc(sizeof(*file));
    if (!file) goto end;
    
    file->fd = open(path, O_RDONLY, 0);
    if (file->fd < 0) {
        outf("failed to open '%s': %s", path, strerror(errno));
        goto end;
    }
    
    if (extract_buffer_open(
            file /*handle*/,
            extract_buffer_file_get,
            extract_buffer_file_close,
            o_buffer
            )) goto end;
    
    e = 0;
    
    end:
    if (e) {
        if (file && file->fd >= 0) {
            close(file->fd);
        }
        free(file);
        *o_buffer = NULL;
    }
    return e;
}

int extract_buffer_getc_internal(extract_buffer_t* buffer, char* out)
/* Called by extract_buffer_getc() if we are at end of buffer->data. */
{
    if (buffer->data_length == buffer->data_pos) {
        int e = extract_buffer_internal_get_more(buffer);
        if (e) return e;
    }
    assert(buffer->data_pos < buffer->data_length);
    *out = buffer->data[buffer->data_pos];
    buffer->data_pos += 1;
    return 0;
}
