#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "trigger.h"

static int fd = -1;

static char *trigger_fs = "lock/trigger";

// -------------------------------------
void trigger_pull()
{
    int pfd = open(trigger_fs, O_WRONLY | O_NDELAY);
    if (pfd >= 0)
    {
        ndelay_on(pfd);
        write(pfd, "", 1);
        close(pfd);
    }
}

// -------------------------------------
void trigger_set()
{
    if (fd != -1)
        close(fd);

    fd = open(trigger_fs, O_RDONLY | O_NDELAY);
}

void trigger_select(int *nfds, fd_set *rfds)
{
    if (fd != -1)
    {
        FD_SET(fd, rfds);
        if (*nfds < fd + 1)
            *nfds = fd + 1;
    }
}

int trigger_pulled(fd_set *rfds)
{
    if (fd != -1)
        if (FD_ISSET(fd, rfds))
            return 1;
    return 0;
}
