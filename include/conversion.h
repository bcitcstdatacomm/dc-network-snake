#ifndef DC_NETWORK_SNAKE_CONVERSION_H
#define DC_NETWORK_SNAKE_CONVERSION_H


#include <dc_env/env.h>
#include <netinet/in.h>


in_port_t parse_port(const struct dc_env *env, struct dc_error *err, const char *buff, int radix);
size_t parse_size_t(const struct dc_env *env, struct dc_error *err, const char *buff, int radix);


#endif //DC_NETWORK_SNAKE_CONVERSION_H
