#include "copy.h"
#include <dc_error/error.h>
#include <dc_posix/dc_stdlib.h>
#include <dc_posix/dc_unistd.h>


void copy(const struct dc_posix_env *env, struct dc_error *err, int from_fd, int to_fd, size_t count)
{
    char *buffer;
    ssize_t rbytes;

    buffer = dc_malloc(env, err, count);

    if(dc_error_has_error(err))
    {
        goto MALLOC_FAIL;
    }

    while((rbytes = dc_read(env, err, from_fd, buffer, count)) > 0)
    {
        dc_write(env, err, to_fd, buffer, rbytes);

        if(dc_error_has_error(err))
        {
            goto IO_FAIL;
        }
    }

IO_FAIL:
    dc_free(env, buffer, count);

MALLOC_FAIL:
    {
    }
}
