#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


_Noreturn void fatal_errno(const char *file, const char *func, const size_t line, int err_code, int exit_code)
{
    const char *msg;

    msg = strerror(err_code);                                                                  // NOLINT(concurrency-mt-unsafe)
    fprintf(stderr, "Error (%s @ %s:%zu %d) - %s\n", file, func, line, err_code, msg);  // NOLINT(cert-err33-c)
    exit(exit_code);                                                                            // NOLINT(concurrency-mt-unsafe)
}
