#include <sys/stat.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/times.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <stdio.h>

#include "wup.h"


char *__env[1] = { 0 };
char **environ = __env;


int _open(const char *name, int flags, ...)
{
    return -1;
}

int _close(int file)
{
    return -1;
}

int _lseek(int file, int ptr, int dir)
{
    return 0;
}

int _read(int file, char *ptr, int len)
{
    return 0;
}

int _write(int file, char *ptr, int len)
{
    return 0;
}

int _isatty(int file)
{
    return 1;
}

int _fstat(int file, struct stat *st)
{
    st->st_mode = S_IFCHR;
    return 0;
}

int _stat(const char *file, struct stat *st)
{
    st->st_mode = S_IFCHR;
    return 0;
}

int _link(char *old, char *new)
{
    errno = EMLINK;
    return -1;
}

int _unlink(char *name)
{
    errno = ENOENT;
    return -1;
}

void _exit()
{
    // TODO.
    for (;;);
}


int _execve(char *name, char **argv, char **env)
{
    errno = ENOMEM;
    return -1;
}

int _fork()
{
    errno = EAGAIN;
    return -1;
}

int _getpid()
{
    return 1;
}


int _kill(int pid, int sig)
{
    errno = EINVAL;
    return -1;
}

caddr_t _sbrk(int incr)
{
#if 0
    extern char _end;		/* Defined by the linker */
    static char *heap_end;
    char *prev_heap_end;

    if (heap_end == 0) {
        heap_end = &_end;
    }
    prev_heap_end = heap_end;
    if (heap_end + incr > stack_ptr) {
        write (1, "Heap and stack collision\n", 25);
        abort ();
    }

    heap_end += incr;
    return (caddr_t) prev_heap_end;
#endif
    // TODO
    return NULL;
}


clock_t _times(struct tms *buf)
{
    return -1;
}

/*int gettimeofday(struct timeval *p, struct timezone *z)
{
    return -1;
}*/


int _wait(int *status)
{
    errno = ECHILD;
    return -1;
}
