#include "copy.h"
#include "error.h"
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


struct options
{
    char *file_name;
    int fd_in;
    int fd_out;
};


static void options_init(struct options *opts);
static void parse_arguments(int argc, char *argv[], struct options *opts);
static void options_process(struct options *opts);
static void cleanup(const struct options *opts);


#define BUF_SIZE 1024


int main(int argc, char *argv[])
{
    struct options opts;

    options_init(&opts);
    parse_arguments(argc, argv, &opts);
    options_process(&opts);
    copy(opts.fd_in, opts.fd_out, BUF_SIZE);
    cleanup(&opts);

    return EXIT_SUCCESS;
}


static void options_init(struct options *opts)
{
    memset(opts, 0, sizeof(struct options));
    opts->fd_in = STDIN_FILENO;
    opts->fd_out = STDOUT_FILENO;
}


static void parse_arguments(int argc, char *argv[], struct options *opts)
{
    int c;

    while((c = getopt(argc, argv, "")) != -1)   // NOLINT(concurrency-mt-unsafe)
    {
    }

    if(optind < argc)
    {
        opts->file_name = argv[optind];
    }
}


static void options_process(struct options *opts)
{
    if(opts->file_name)
    {
        opts->fd_in = open(opts->file_name, O_RDONLY);

        if(opts->fd_in == -1)
        {
            fatal_errno(__FILE__, __func__ , __LINE__, errno, 2);
        }
    }
}


static void cleanup(const struct options *opts)
{
    if(opts->file_name)
    {
        close(opts->fd_in);
    }
}
