#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/syslog.h>
#include <sys/epoll.h>
#include "confparser.h"
#include "dictionary.h"
#include "slog.h"

char uuid[1024] = {0};

int wfd = 1;

int swrite(int fd, char *buf, int n)
{
    int w;
    w = write(fd, buf, n);
    if (w != -1)
        return w;
    if (errno == EINTR)
        return -1;

    // close(fd);
    return n;
}

void out(char *s)
{
    if (swrite(wfd, s, strlen(s)) == -1)
        _exit(0);
}

void zerodie()
{
    // if (swrite(wfd, "\n", 1) == -1)
    if (swrite(wfd, "", 1) == -1)
        _exit(0);

    _exit(0);
}

void quit(char *prepend, char *append)
{
    struct timeval tv;

    out(prepend);
    out(":");
    out(append);
    // out("\n");

    gettimeofday(&tv, NULL);

    slog_info("Time:%u.%06u uuid(%s) to rspwn:%s:%s", (uint32_t)tv.tv_sec, (uint32_t)tv.tv_usec, uuid, prepend, append);

    zerodie();
}

// argv[1]: mid
// argv[2]: message file
int main(int argc, char **argv)
{
    slog_open("sn-remote", LOG_PID | LOG_NDELAY, LOG_MAIL);

    snprintf(uuid, sizeof(uuid) - 1, "%s", argv[1]);
    slog_info("get args: mid(%s) file(%s)", uuid, argv[2]);

    // test temp fail
    // quit("Z", "I test temp failed.\n");

    // test succ
    // quit("K", "ok.");

    // test fail
    //quit("D", "I test failed.\n");

    return 0;
}
