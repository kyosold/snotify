#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include "trigger.h"
#include "utils.h"
#include "slog.h"

static char *todo_path = "todo";
static char *queue_path = "queue";
static char *mess_path = "mess";

int main(int argc, char **argv)
{

    if (chdir(SNOTIFY_PATH) == -1)
    {
        printf("cannot start: unable to switch to home directory");
        exit(111);
    }

    char mess_fn[1024] = {0};
    char todo_fn[1024] = {0};
    char file_name[1024] = {0};
    char uuid[1024] = {0};
    get_uuid(uuid, sizeof(uuid));

    FILE *todo_fp = NULL;
    int i = 0;
    int fd = -1;
    static int seq = 0;
    (void)umask(033);
    srandom(time(NULL));
    for (i = 0; i < 100; i++)
    {
        snprintf(file_name, sizeof(file_name) - 1, "%.5ld%.5u%.3d",
                 time(NULL), getpid(), seq++);
        snprintf(todo_fn, sizeof(todo_fn) - 1, "%s/%s",
                 todo_path, file_name);
        fd = open(todo_fn, O_WRONLY | O_EXCL | O_CREAT, 0644);
        if (fd != -1)
        {
            todo_fp = fdopen(fd, "w");
            if (todo_fp)
            {
                break;
            }
            close(fd);
        }
        slog_info("open file(%s) error(%d):%s", todo_fn, errno, strerror(errno));
    }

    if (todo_fp == NULL)
    {
        slog_info("inqueue fail(%d):%s", errno, strerror(errno));
        close(fd);
        return 1;
    }

    char todo_buf[2048] = {0};
    int n = snprintf(todo_buf, sizeof(todo_buf) - 1, "%s", uuid);
    fwrite(todo_buf, 1, n, todo_fp);
    fclose(todo_fp);

    snprintf(mess_fn, sizeof(mess_fn) - 1, "%s/%s", mess_path, file_name);
    FILE *fp = fopen(mess_fn, "w+");

    char buf[4096] = {0};
    while (NULL != fgets(buf, sizeof(buf) - 1, stdin))
    {
        fwrite(buf, 1, strlen(buf), fp);
    }
    // fwrite(argv[1], 1, strlen(argv[1]), fp);
    fclose(fp);

    trigger_pull();

    char out_buf[2048] = {0};
    n = snprintf(out_buf, sizeof(out_buf) - 1, "250 queue id %s\r\n", uuid);
    write(1, out_buf, n);

    return 0;
}