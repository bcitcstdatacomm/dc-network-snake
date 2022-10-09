#include "copy.h"
#include <stdlib.h>
#include <unistd.h>


#define BUF_SIZE 1024


int main(void)
{
    copy(STDIN_FILENO, STDOUT_FILENO, BUF_SIZE);

    return EXIT_SUCCESS;
}
