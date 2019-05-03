/*
 * Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/unistd.h>
#include "LPUART.h"
#include <errno.h>

char* getcwd(char* buf, size_t size)
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

int mkdir(const char* path, mode_t mode)
{
    (void) path;
    (void) mode;
    errno = EACCES;
    return -1;
}

// our replacement for the libc call.
__attribute__((visibility("default"))) int _gettimeofday(struct timeval* tp, void* tzp)
{
    (void)tzp;
    if (tp)
    {
        // TODO: use systick
        tp->tv_sec  = 0;
        tp->tv_usec = 0;
    }
    else
    {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

int _write(int fd, const void* buf, size_t count)
{
    (void)fd;
    if (count == 0 || buf == 0)
    {
        errno = EINVAL;
        return -1;
    }
    LPUART1_transmit_string_len(buf, count);
    return 0;
}

int _open(const char* filename, int oflag, int pmode)
{
    (void) filename;
    (void) oflag;
    (void) pmode;
    errno = ENOENT;
    return -1;
}

int _close(int fd)
{
    (void)fd;
    errno = EIO;
    return -1;
}

void _putchar(int c)
{
    LPUART1_transmit_char((char)c);
}
