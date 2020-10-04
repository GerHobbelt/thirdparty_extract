#include "../include/extract_buffer.h"

#include "outf.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>


struct extract_buffer_t
{
    extract_buffer_data_t   data;
    void*                   handle;
    extract_buffer_fn_get   get;
    extract_buffer_fn_close close;
};


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
    buffer->data.data = NULL;
    buffer->data.length = 0;
    buffer->data.pos = 0;
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

static int extract_buffer_internal_get_more(extract_buffer_t* buffer)
/* Returns +1 on EOF. */
{
    assert(buffer);
    assert(buffer->data.pos == buffer->data.length);
    buffer->data.pos = 0;
    int e = buffer->get(buffer->handle, &buffer->data.data, &buffer->data.length);
    if (e) {
        buffer->data.data = NULL;
        buffer->data.length = 0;
        return e;
    }
    if (buffer->data.length == 0) {
        return +1;
    }
    return 0;
}

int extract_buffer_read(
        extract_buffer_t*   buffer,
        char*               out,
        int                 out_length,
        int*                out_actual
        )
{
    int e = 0;
    int out_pos = 0;
    for(;;) {
        if (out_pos == out_length) {
            break;
        }
        int n = buffer->data.length - buffer->data.pos;
        if (n == 0) {
            if (extract_buffer_internal_get_more(buffer)) return -1;
            if (buffer->data.length == 0) {
                /* EOF */
                e = +1;
                break;
            }
            n = buffer->data.length - buffer->data.pos;
        }
        if (n > out_length - out_pos) {
            n = out_length - out_pos;
        }
        memcpy(out + out_pos, buffer->data.data + buffer->data.pos, n);
        out_pos += n;
        buffer->data.pos += n;
    }
    
    if (out_actual) {
        *out_actual = out_pos;
    }
    return e;
}

static int s_extract_buffer_open_simple_get(
        void* handle,
        char** o_data,
        int* o_data_length
        )
{
    /* Indicate EOF. */
    (void) handle;
    *o_data = NULL;
    *o_data_length = 0;
    return 0;
}

int extract_buffer_open_simple(
        char*               data,
        int                 data_length,
        extract_buffer_t**  o_buffer
        )
{
    extract_buffer_t* buffer = malloc(sizeof(*buffer));
    if (!buffer) return -1;
    buffer->data.data = data;
    buffer->data.length = data_length;
    buffer->data.pos = 0;
    buffer->handle = NULL;
    buffer->get = s_extract_buffer_open_simple_get;
    buffer->close = NULL;
    *o_buffer = buffer;
    return 0;
}


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

int extract_buffer_open_file(const char* path, extract_buffer_t** o_buffer)
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
    outf("buffer->data.length=%i buffer->data.pos=%i",
            buffer->data.length,
            buffer->data.pos
            );
    if (buffer->data.length == buffer->data.pos) {
        outf("calling extract_buffer_internal_get_more()");
        int e = extract_buffer_internal_get_more(buffer);
        if (e) return e;
    }
    /* We could call extract_buffer_getc() without fear of recursion, but seems
    simpler to do things explicitly. */
    assert(buffer->data.pos < buffer->data.length);
    *out = buffer->data.data[buffer->data.pos];
    buffer->data.pos += 1;
    return 0;
}
