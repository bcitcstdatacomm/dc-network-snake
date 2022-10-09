#ifndef DC_NETWORK_SNAKE_COPY_H
#define DC_NETWORK_SNAKE_COPY_H


#include <dc_posix/dc_posix_env.h>
#include <stddef.h>


void copy(const struct dc_posix_env *env, struct dc_error *err, int from_fd, int to_fd, size_t count);


#endif //DC_NETWORK_SNAKE_COPY_H
