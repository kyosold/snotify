#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/syslog.h>
#include <fcntl.h>
#include <uuid/uuid.h>
#include <stdint.h>

int fd_copy(int to, int from)
{
    if (to == from)
        return 0;
    if (fcntl(from, F_GETFL, 0) == -1)
        return -1;
    close(to);
    if (fcntl(from, F_DUPFD, to) == -1)
        return -1;
    return 0;
}

int fd_move(int to, int from)
{
    if (to == from)
        return 0;
    if (fd_copy(to, from) == -1)
        return -1;
    close(from);
    return 0;
}

int wait_nohang(int *wstat)
{
    return waitpid(-1, wstat, WNOHANG);
}

int ndelay_on(int fd)
{
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}

int ndelay_off(int fd)
{
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) & ~O_NONBLOCK);
}

// return: != 16失败
int get_uuid(char *uuid, size_t uuid_size)
{
    uuid_t _uuid;
    uuid_generate(_uuid);
    unsigned char *p = _uuid;
    int i;
    char ch[5] = {0};
    for (i = 0; i < sizeof(uuid_t); i++, p++)
    {
        snprintf(ch, sizeof(ch), "%02X", *p);
        uuid[i * 2] = ch[0];
        uuid[i * 2 + 1] = ch[1];
    }
    return i;
}

void idx_char_to_uint(char *c2, int *delnum)
{
    *delnum = (((uint8_t)c2[0]) << 8) + (uint8_t)c2[1];
}

void idx_uint_to_char(int delnum, char *c2)
{
    c2[0] = (delnum >> 8) & 0xff;
    c2[1] = delnum & 0xff;
}

void byte_copy(char *to, int n, char *from)
{
    for (;;)
    {
        if (!n)
            return;
        *to++ = *from++;
        --n;
        if (!n)
            return;
        *to++ = *from++;
        --n;
        if (!n)
            return;
        *to++ = *from++;
        --n;
        if (!n)
            return;
        *to++ = *from++;
        --n;
    }
}
