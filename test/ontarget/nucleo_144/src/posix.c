

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/unistd.h>
#include <errno.h>

int init_posix(void) 
{
    errno = 0;
    return 0;
}


char *getcwd(char *buf, size_t size)
{
    if (!buf || size <= 1)
    {
        errno = EINVAL;
        return 0;
    }
    buf[0] = '/';
    buf[1] = 0;
    return buf;
}
