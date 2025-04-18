#include <sys/stat.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/times.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <stdio.h>

#include <wup/wup.h>
#include <lwip/sockets.h>

#define _is_socket(fd) (((fd) >= LWIP_SOCKET_OFFSET) && ((fd) < (LWIP_SOCKET_OFFSET + MEMP_NUM_NETCONN)))


char *__env[1] = { 0 };
char **environ = __env;


void send_string(char* str);

int _open(const char *name, int flags, ...)
{
    send_string("_open\n");
    send_string(name);
    return -1;
}

int _close(int file)
{
    if (_is_socket(file))
        return lwip_close(file);

    return -1;
}

int _lseek(int file, int ptr, int dir)
{
    return 0;
}

int _read(int file, char *ptr, int len)
{
    if (_is_socket(file))
        return lwip_read(file, ptr, len);

    return 0;
}

void _SPI_Write(u8* buf, int len)
{
    REG_SPI_CNT = (REG_SPI_CNT & ~SPI_DIR_MASK) | SPI_DIR_WRITE;
    REG_SPI_IRQ_ENABLE |= SPI_IRQ_WRITE;

    for (int i = 0; i < len; i++)
    {
        REG_SPI_DATA = buf[i];
        while (SPI_WRITE_FIFO_LVL == 0);
    }

    // wait for any leftover contents to be transferred
    while (!(REG_SPI_IRQ_STATUS & SPI_IRQ_WRITE));
    REG_SPI_IRQ_STATUS |= SPI_IRQ_WRITE;
    REG_SPI_IRQ_ENABLE &= ~SPI_IRQ_WRITE;
}

int _write(int file, char *ptr, int len)
{
    if (_is_socket(file))
        return lwip_write(file, ptr, len);

    if (file == 1)
    {
        int irqen = DisableIRQ();

        // FPGA debug
#ifdef FPGA_LOG
        if (1)
        {
            const int csize = 0x3FFF;
            for (int i = 0; i < len; i += csize)
            {
                int chunk = csize;
                if ((i + chunk) > len)
                    chunk = len - i;

                u16 header = 0x8000 | (chunk & 0x3FFF);

                u8 buf[3];
                buf[0] = 0xF2;
                buf[1] = header >> 8;
                buf[2] = header & 0xFF;

                SPI_Start(SPI_DEVICE_FLASH, SPI_CLK_ENABLE | SPI_CLK_SOURCE(CLKSRC_32MHZ) | SPI_CLK_DIVIDER(16));
                //SPI_Start(SPI_DEVICE_FLASH, 0x8400);//SPI_SPEED_FLASH);
                _SPI_Write(buf, 3);
                _SPI_Write((u8 *) ptr + i, chunk);
                SPI_Finish();
            }
        }
#endif

        Console_Print(ptr, len);

        RestoreIRQ(irqen);
        return len;
    }
    return len;
}

int _isatty(int file)
{
    if (file == 1)
        return 1;

    return 0;
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
    send_string("_exit\n");
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
    send_string("_kill\n");
    errno = EINVAL;
    return -1;
}

/*void logu32(const char* label, u32 val)
{
    char str[8+8+3];
    for (int i = 0; i < 8; i++)
        str[i] = label[i];
    str[8] = '=';
    for (int i = 9; i < 9+8; i++)
    {
        u32 n = val >> 28;
        val <<= 4;
        if (n <= 9) str[i] = '0'+n;
        else str[i] = 'A'+(n-10);
    }
    str[9+8] = '\n';
    str[9+9] = 0;
    _write(1, str, 8+8+2);
}*/

caddr_t _sbrk(int incr)
{
    extern char __end__;		/* Defined by the linker */
    extern char __stack_start;
    static char *heap_end;
    char *prev_heap_end;

    if (heap_end == 0)
    {
        heap_end = &__end__;
    }
    prev_heap_end = heap_end;
    if ((heap_end + incr) > &__stack_start)
    {
        errno = ENOMEM;
        return (caddr_t)-1;
    }

    heap_end += incr;
    return (caddr_t)prev_heap_end;
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
