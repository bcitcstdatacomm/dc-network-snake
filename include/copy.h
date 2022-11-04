#ifndef DC_NETWORK_SNAKE_COPY_H
#define DC_NETWORK_SNAKE_COPY_H


#include <dc_env/env.h>


void copy(const struct dc_env *env, struct dc_error *err, int from_fd, int to_fd, size_t count);


#endif //DC_NETWORK_SNAKE_COPY_H
