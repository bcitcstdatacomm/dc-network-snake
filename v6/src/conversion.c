#include "conversion.h"
#include <dc_posix/dc_inttypes.h>
#include <dc_posix/dc_stdlib.h>
#include <limits.h>
#include <stdint.h>


in_port_t parse_port(const struct dc_posix_env *env, struct dc_error *err, const char *buff, int radix)
{
    char *end;
    long sl;
    in_port_t port;
    const char *msg;

    DC_TRACE(env);
    sl = dc_strtol(env, err, buff, &end, radix);

    if(end == buff)
    {
        msg = "not a decimal number";
    }
    else if(*end != '\0')
    {
        msg = "%s: extra characters at end of input";
    }
    else if((sl == LONG_MIN || sl == LONG_MAX) && ERANGE == errno)
    {
        msg = "out of range of type long";
    }
    else if(sl > UINT16_MAX)
    {
        msg = "greater than UINT16_MAX";
    }
    else if(sl < 0)
    {
        msg = "less than 0";
    }
    else
    {
        msg = NULL;
    }

    if(msg)
    {
        DC_ERROR_RAISE_USER(err, msg, 3);
    }

    port = (in_port_t)sl;

    return port;
}


size_t parse_size_t(const struct dc_posix_env *env, struct dc_error *err, const char *buff, int radix)
{
    char *end;
    uintmax_t max;
    size_t ret_val;
    const char *msg;

    DC_TRACE(env);
    errno = 0;
    max = dc_strtoumax(env, err, buff, &end, radix);

    if(end == buff)
    {
        msg = "not a decimal number";
    }
    else if(*end != '\0')
    {
        msg = "%s: extra characters at end of input";
    }
    else if((max == UINTMAX_MAX || max == 0) && ERANGE == errno)
    {
        msg = "out of range of type uintmax_t";
    }
    else
    {
        msg = NULL;
    }

    if(msg)
    {
        DC_ERROR_RAISE_USER(err, msg, 3);
    }

    ret_val = (size_t)max;

    return ret_val;
}
