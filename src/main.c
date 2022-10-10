#include "conversion.h"
#include "copy.h"
#include <dc_posix/arpa/dc_inet.h>
#include <dc_posix/dc_fcntl.h>
#include <dc_posix/dc_libgen.h>
#include <dc_posix/dc_signal.h>
#include <dc_posix/dc_stdio.h>
#include <dc_posix/dc_stdlib.h>
#include <dc_posix/dc_string.h>
#include <dc_posix/dc_unistd.h>
#include <dc_posix/sys/dc_socket.h>
#include <dc_util/networking.h>


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


static _Noreturn void usage(const struct dc_posix_env *env, struct dc_error *err, const char *binary_path);
static void options_init(const struct dc_posix_env *env, struct options *opts);
static void parse_arguments(const struct dc_posix_env *env, struct dc_error *err, int argc, char *argv[], struct options *opts);
static void options_process(const struct dc_posix_env *env, struct dc_error *err, struct options *opts);
static void open_input_file(const struct dc_posix_env *env, struct dc_error *err, struct options *opts);
static void open_input_socket(const struct dc_posix_env *env, struct dc_error *err, struct options *opts);
static void open_output_socket(const struct dc_posix_env *env, struct dc_error *err, struct options *opts);
static void handle_client(const struct dc_posix_env *env, struct dc_error *err, struct options *opts);
static void cleanup(const struct dc_posix_env *env, struct dc_error *err, const struct options *opts);
static void set_signal_handling(const struct dc_posix_env *env, struct dc_error *err, struct sigaction *sa);
static void signal_handler(int sig);


#define DEFAULT_BUF_SIZE 1024
#define DEFAULT_PORT 5000
#define BACKLOG 5


static volatile sig_atomic_t running;   // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)


int main(int argc, char *argv[])
{
    struct dc_error *err;
    struct dc_posix_env *env;
    struct options opts;
    struct sigaction sa;
    int exit_code;

    err = dc_error_create(true);

    if(err == NULL)
    {
        exit_code = EXIT_FAILURE;
        goto ERROR_CREATE;
    }

    env = dc_posix_env_create(err, true, NULL);

    if(dc_error_has_error(err))
    {
        goto ENV_CREATE;
    }

    options_init(env, &opts);
    parse_arguments(env, err, argc, argv, &opts);

    if(opts.verbose)
    {
        dc_posix_env_set_tracer(env, dc_posix_default_tracer);
    }

    if(opts.show_help)
    {
        usage(env, err, argv[0]);
    }

    options_process(env, err, &opts);

    if(dc_error_has_error(err))
    {
        goto PROCESS_ERROR;
    }

    set_signal_handling(env, err, &sa);
    running = 1;

    if(opts.ip_in)
    {
        handle_client(env, err, &opts);
    }
    else
    {
        copy(env, err, opts.fd_in, opts.fd_out, opts.buffer_size);
    }

    PROCESS_ERROR:
    cleanup(env, err, &opts);
    free(env);
    ENV_CREATE:

    if(dc_error_has_error(err))
    {
        const char *message;

        message = dc_error_get_message(err);
        fprintf(stderr, "Error: %s\n", message);      // NOLINT(cert-err33-c)
        exit_code = EXIT_FAILURE;
    }
    else
    {
        exit_code = EXIT_SUCCESS;
    }

    dc_error_reset(err);
    free(err);
    ERROR_CREATE:

    return exit_code;
}

static void handle_client(const struct dc_posix_env *env, struct dc_error *err, struct options *opts)
{
    DC_TRACE(env);

    while(running)
    {
        int fd;
        struct sockaddr_in accept_addr;
        socklen_t accept_addr_len;
        char *accept_addr_str;
        in_port_t accept_port;

        accept_addr_len = sizeof(accept_addr);
        fd = dc_accept(env, err, opts->fd_in, (struct sockaddr *)&accept_addr, &accept_addr_len);

        if(dc_error_has_error(err))
        {
            if(dc_error_is_errno(err, EINTR))
            {
                dc_error_reset(err);
            }

            goto DONE;
        }

        accept_addr_str = dc_inet_ntoa(env, accept_addr.sin_addr);  // NOLINT(concurrency-mt-unsafe)
        accept_port = dc_ntohs(env, accept_addr.sin_port);
        printf("Accepted from %s:%d\n", accept_addr_str, accept_port);
        copy(env, err, fd, opts->fd_out, opts->buffer_size);
        printf("Closing %s:%d\n", accept_addr_str, accept_port);
        dc_close(env, err, fd);

        DONE:
        {
        }
    }
}

static _Noreturn void usage(const struct dc_posix_env *env, struct dc_error *err, const char *binary_path)
{
    char *dup_path;
    char *binary_name;

    DC_TRACE(env);
    dup_path = dc_strdup(env, err, binary_path);
    binary_name = dc_basename(env, dup_path);

    // NOLINTBEGIN(cert-err33-c)
    fprintf(stderr, "%s [OPTIONS] [FILE]\n", binary_name);
    fprintf(stderr, "-i ip address      input IP address\n");
    fprintf(stderr, "-o ip address      output IP address\n");
    fprintf(stderr, "-p port            input port\n");
    fprintf(stderr, "-P port            output port\n");
    fprintf(stderr, "-b buffer size     size of the read/write buffer\n");
    fprintf(stderr, "-v                 verbose\n");
    fprintf(stderr, "-h                 help\n");
    // NOLINTEND(cert-err33-c)

    exit(EXIT_SUCCESS);     // NOLINT(concurrency-mt-unsafe)
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

    while((c = dc_getopt(env, err, argc, argv, ":i:o:p:P:b:vh")) != -1)   // NOLINT(concurrency-mt-unsafe)
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
                opts->port_in = parse_port(env, err, optarg, 10); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

                if(dc_error_has_error(err))
                {
                }

                break;
            }
            case 'P':
            {
                opts->port_out = parse_port(env, err, optarg, 10); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

                if(dc_error_has_error(err))
                {
                }

                break;
            }
            case 'b':
            {
                opts->buffer_size = parse_size_t(env, err, optarg, 10); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

                if(dc_error_has_error(err))
                {
                }

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
                DC_ERROR_RAISE_USER(err, "", 1);
                break;
            }
            case '?':
            {
                DC_ERROR_RAISE_USER(err, "", 2);
                break;
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
        DC_ERROR_RAISE_USER(err, "", 2);
        goto INPUT_ERROR;
    }

    if(opts->file_name)
    {
        open_input_file(env, err, opts);

        if(dc_error_has_error(err))
        {
            goto INPUT_FILE_ERROR;
        }
    }

    if(opts->ip_in)
    {
        open_input_socket(env, err, opts);

        if(dc_error_has_error(err))
        {
            goto INPUT_SOCKET_ERROR;
        }
    }

    if(opts->ip_out)
    {
        open_output_socket(env, err, opts);

        if(dc_error_has_error(err))
        {
            goto OUTPUT_SOCKET_ERROR;
        }
    }

    OUTPUT_SOCKET_ERROR:
    INPUT_SOCKET_ERROR:
    INPUT_FILE_ERROR:
    INPUT_ERROR:
    {
    }
}

static void open_input_file(const struct dc_posix_env *env, struct dc_error *err, struct options *opts)
{
    DC_TRACE(env);
    opts->fd_in = dc_open(env, err, opts->file_name, O_RDONLY);
}

static void open_input_socket(const struct dc_posix_env *env, struct dc_error *err, struct options *opts)
{
    struct sockaddr_in addr;

    DC_TRACE(env);
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

    LISTEN_ERROR:
    BIND_ERROR:
    SOCKOPT_ERROR:
    ADDRESS_ERROR:
    SOCKET_ERROR:
    {
    }
}

static void open_output_socket(const struct dc_posix_env *env, struct dc_error *err, struct options *opts)
{
    struct sockaddr_in addr;

    DC_TRACE(env);
    opts->fd_out = socket(AF_INET, SOCK_STREAM, 0);

    if(dc_error_has_error(err))
    {
        goto SOCKET_ERROR;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = dc_htons(env, opts->port_out);
    addr.sin_addr.s_addr = dc_inet_addr(env, err, opts->ip_out);

    if(dc_error_has_error(err))
    {
        goto INET_ADDR_ERROR;
    }

    dc_connect(env, err, opts->fd_out, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));

    if(dc_error_has_error(err))
    {
        goto CONNECT_ERROR;
    }

    CONNECT_ERROR:
    INET_ADDR_ERROR:
    SOCKET_ERROR:
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


static void set_signal_handling(const struct dc_posix_env *env, struct dc_error *err, struct sigaction *sa)
{
    DC_TRACE(env);
    dc_sigemptyset(env, err, &sa->sa_mask);
    sa->sa_flags = 0;
    sa->sa_handler = signal_handler;
    dc_sigaction(env, err, SIGINT, sa, NULL);
}


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
static void signal_handler(int sig)
{
    running = 0;
}
#pragma GCC diagnostic pop

