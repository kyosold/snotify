#include <stdio.h>
#include <unistd.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "utils.h"

char *(qsargs[]) = {"sn-send", 0};
char *(qrargs[]) = {"sn-rspawn", 0};

void die()
{
    _exit(111);
}

int pi3[2];
int pi4[2];

void close23456()
{
    close(3);
    close(4);
}

void close_pipes()
{
    close(pi3[0]);
    close(pi3[1]);
    close(pi4[0]);
    close(pi4[1]);
}

int main(int argc, char **argv)
{
    // if (chdir(SNOTIFY_PATH) == -1)
    if (chdir("/") == -1)
        die();
    umask(077);

    if (fd_copy(3, 0) == -1)
        die();
    if (fd_copy(4, 0) == -1)
        die();

    if (pipe(pi3) == -1)
        die();
    if (pipe(pi4) == -1)
        die();

    switch (fork())
    {
    case -1:
        die();
    case 0:
        if (fd_copy(0, pi3[0]) == -1)
            die();
        if (fd_copy(1, pi4[1]) == -1)
            die();
        close23456();
        close_pipes();
        execvp(*qrargs, qrargs);
        die();
    }

    if (fd_copy(0, 1) == -1)
        die();
    if (fd_copy(3, pi3[1]) == -1)
        die();
    if (fd_copy(4, pi4[0]) == -1)
        die();

    close_pipes();
    execvp(*qsargs, qsargs);

    die();

    return 0;
}