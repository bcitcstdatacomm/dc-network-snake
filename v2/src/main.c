#include "copy.h"
#include "error.h"
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>


#define BUF_SIZE 1024


int main(int argc, char *argv[])
{
    int fd;
    bool must_close;

    if(argc > 1)
    {
        fd = open(argv[1], O_RDONLY);

        if(fd == -1)
        {
            fatal_errno(__FILE__, __func__ , __LINE__, errno, 2);
        }

        must_close = true;
    }
    else
    {
        fd = STDIN_FILENO;
        must_close = false;
    }

    copy(fd, STDOUT_FILENO, BUF_SIZE);

    if(must_close)
    {
        close(fd);
    }

    return EXIT_SUCCESS;
}
