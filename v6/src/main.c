#include "conversion.h"
#include "copy.h"
#include "error.h"
#include <dc_posix/arpa/dc_inet.h>
#include <dc_posix/dc_fcntl.h>
#include <dc_posix/dc_libgen.h>
#include <dc_posix/dc_stdio.h>
#include <dc_posix/dc_stdlib.h>
#include <dc_posix/dc_string.h>
#include <dc_posix/dc_unistd.h>
#include <dc_posix/sys/dc_socket.h>
#include <dc_util/networking.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>


struct options
{
    bool verbose;
    bool show_help;
    char *file_name;
    char *ip_in;
    char *ip_out;
    in_port_t port_in;
    in_port_t port_out;
    int fd_in;
    int fd_out;
    size_t buffer_size;
};


static _Noreturn void usage(const struct dc_posix_env *env, const char *binary_path);
static void options_init(const struct dc_posix_env *env, struct options *opts);
static void parse_arguments(const struct dc_posix_env *env, struct dc_error *err, int argc, char *argv[], struct options *opts);
static void options_process(const struct dc_posix_env *env, struct dc_error *err, struct options *opts);
static void cleanup(const struct dc_posix_env *env, struct dc_error *err, const struct options *opts);
static void set_signal_handling(struct sigaction *sa);
static void signal_handler(int sig);


#define DEFAULT_BUF_SIZE 1024
#define DEFAULT_PORT 5000
#define BACKLOG 5


static volatile sig_atomic_t running;   // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)


int main(int argc, char *argv[])
{
    struct dc_posix_env env;
    struct dc_error err;
    struct options opts;
    int exit_code;

    dc_posix_env_init(&env, NULL);
//    dc_posix_env_init(&env, dc_posix_default_tracer);
    options_init(&env, &opts);
    parse_arguments(&env, &err, argc, argv, &opts);

    if(opts.verbose)
    {
        dc_posix_env_set_trace(&env, dc_posix_default_tracer);
    }

    if(opts.show_help)
    {
        usage(&env, argv[0]);
    }

    options_process(&env, &err, &opts);

    if(opts.ip_in)
    {
        struct sigaction sa;

        set_signal_handling(&sa);
        running = 1;

        while(running)
        {
            int fd;
            struct sockaddr_in accept_addr;
            socklen_t accept_addr_len;
            char *accept_addr_str;
            in_port_t accept_port;

            accept_addr_len = sizeof(accept_addr);
            fd = dc_accept(&env, &err, opts.fd_in, (struct sockaddr *)&accept_addr, &accept_addr_len);

            if(dc_error_has_error(&err))
            {
                if(errno == EINTR)
                {
                    dc_error_reset(&err);
                    break;
                }

                exit_code = 1;
                goto DONE;
            }

            accept_addr_str = dc_inet_ntoa(&env, accept_addr.sin_addr);  // NOLINT(concurrency-mt-unsafe)
            accept_port = dc_ntohs(&env, accept_addr.sin_port);
            printf("Accepted from %s:%d\n", accept_addr_str, accept_port);
            copy(&env, &err, fd, opts.fd_out, opts.buffer_size);

            if(dc_error_has_error(&err))
            {
                exit_code = 2;
            }

            printf("Closing %s:%d\n", accept_addr_str, accept_port);
            dc_close(&env, &err, fd);

            if(dc_error_has_error(&err))
            {
                exit_code = 3;
                goto DONE;
            }
        }
    }
    else
    {
        copy(&env, &err, opts.fd_in, opts.fd_out, opts.buffer_size);

        if(dc_error_has_error(&err))
        {
            exit_code = 4;
            goto DONE;
        }
    }

    DONE:
    cleanup(&env, &err, &opts);

    return EXIT_SUCCESS;
}


static _Noreturn void usage(const struct dc_posix_env *env, const char *binary_path)
{
    char *binary_name;

    binary_name = dc_basename(env, binary_name);

    fprintf(stderr, "%s [OPTIONS] [FILE]\n", binary_name);
    fprintf(stderr, "-i ip address      input IP address\n");
    fprintf(stderr, "-o ip address      output IP address\n");
    fprintf(stderr, "-p port            input port\n");
    fprintf(stderr, "-P port            output port\n");
    fprintf(stderr, "-b buffer size     size of the read/write buffer\n");
    fprintf(stderr, "-v                 verbose\n");
    fprintf(stderr, "-h                 help\n");

    exit(EXIT_SUCCESS);
}

static void options_init(const struct dc_posix_env *env, struct options *opts)
{
    DC_TRACE(env);
    dc_memset(env, opts, 0, sizeof(struct options));
    opts->fd_in       = STDIN_FILENO;
    opts->fd_out      = STDOUT_FILENO;
    opts->port_in     = DEFAULT_PORT;
    opts->port_out    = DEFAULT_PORT;
    opts->buffer_size = DEFAULT_BUF_SIZE;
}


static void parse_arguments(const struct dc_posix_env *env, struct dc_error *err, int argc, char *argv[], struct options *opts)
{
    int c;

    DC_TRACE(env);

    while((c = dc_getopt(env, err, argc, argv, ":i:o:p:P:b:v:h")) != -1)   // NOLINT(concurrency-mt-unsafe)
    {
        switch(c)
        {
            case 'i':
            {
                opts->ip_in = optarg;
                break;
            }
            case 'o':
            {
                opts->ip_out = optarg;
                break;
            }
            case 'p':
            {
                opts->port_in = parse_port(optarg, 10); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
                break;
            }
            case 'P':
            {
                opts->port_out = parse_port(optarg, 10); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
                break;
            }
            case 'b':
            {
                opts->buffer_size = parse_size_t(optarg, 10); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
                break;
            }
            case 'v':
            {
                opts->verbose = true;
                break;
            }
            case 'h':
            {
                opts->show_help = true;
                break;
            }
            case ':':
            {
                fatal_message(__FILE__, __func__ , __LINE__, "\"Option requires an operand\"", 5); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
                break;
            }
            case '?':
            {
                fatal_message(__FILE__, __func__ , __LINE__, "Unknown", 6); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
            }
            default:
            {
                abort();
            };
        }
    }

    if(optind < argc)
    {
        opts->file_name = argv[optind];
    }
}


static void options_process(const struct dc_posix_env *env, struct dc_error *err, struct options *opts)
{
    DC_TRACE(env);

    if(opts->file_name && opts->ip_in)
    {
        fatal_message(__FILE__, __func__ , __LINE__, "Can't pass -i and a filename", 2);
    }

    if(opts->file_name)
    {
        opts->fd_in = dc_open(env, err, opts->file_name, O_RDONLY);

        if(dc_error_has_error(err))
        {
            goto DONE;
        }
    }

    if(opts->ip_in)
    {
        struct sockaddr_in addr;

        opts->fd_in = dc_socket(env, err, AF_INET, SOCK_STREAM, 0);

        if(dc_error_has_error(err))
        {
            goto SOCKET_ERROR;
        }

        dc_setsockopt_socket_REUSEADDR(env, err, opts->fd_in, true);

        if(dc_error_has_error(err))
        {
            goto SOCKOPT_ERROR;
        }

        addr.sin_family = AF_INET;
        addr.sin_port = dc_htons(env, opts->port_in);
        addr.sin_addr.s_addr = dc_inet_addr(env, err, opts->ip_in);

        if(dc_error_has_error(err))
        {
            goto ADDRESS_ERROR;
        }

        dc_bind(env, err, opts->fd_in, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));

        if(dc_error_has_error(err))
        {
            goto BIND_ERROR;
        }

        dc_listen(env, err, opts->fd_in, BACKLOG);

        if(dc_error_has_error(err))
        {
            goto LISTEN_ERROR;
        }
    }

    if(opts->ip_out)
    {
        int result;
        struct sockaddr_in addr;

        opts->fd_out = socket(AF_INET, SOCK_STREAM, 0);

        if(opts->fd_out == -1)
        {
            fatal_errno(__FILE__, __func__ , __LINE__, errno, 2);
        }

        addr.sin_family = AF_INET;
        addr.sin_port = htons(opts->port_out);
        addr.sin_addr.s_addr = dc_inet_addr(env, err, opts->ip_out);

        if(addr.sin_addr.s_addr ==  (in_addr_t)-1)
        {
            fatal_errno(__FILE__, __func__ , __LINE__, errno, 2);
        }

        result = connect(opts->fd_out, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));

        if(result == -1)
        {
            fatal_errno(__FILE__, __func__ , __LINE__, errno, 2);
        }
    }

    LISTEN_ERROR:
    BIND_ERROR:
    SOCKOPT_ERROR:
    ADDRESS_ERROR:
    SOCKET_ERROR:
    DONE:
    {
    }
}


static void cleanup(const struct dc_posix_env *env, struct dc_error *err, const struct options *opts)
{
    DC_TRACE(env);

    if((opts->file_name || opts->ip_in) && opts->fd_in != -1)
    {
        dc_close(env, err, opts->fd_in);
    }

    if(opts->ip_out && opts->fd_out != -1)
    {
        dc_close(env, err, opts->fd_out);
    }
}


static void set_signal_handling(struct sigaction *sa)
{
    int result;

    sigemptyset(&sa->sa_mask);
    sa->sa_flags = 0;
    sa->sa_handler = signal_handler;
    result = sigaction(SIGINT, sa, NULL);

    if(result == -1)
    {
        fatal_errno(__FILE__, __func__ , __LINE__, errno, 2);
    }
}


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
static void signal_handler(int sig)
{
    running = 0;
}
#pragma GCC diagnostic pop

