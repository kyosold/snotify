/*
*
*   进队列:
*       1. 写文件内容到 mess 中并写文件名到到 todo 目录，然后字一空字节到 trigger 中。
*
*   监控队列:
*       1. 监控 trigger 文件是否可读:
*           a. 可读，说明有 todo 目录有新文件进来，读取该文件名写并入到 queue 中然后插入到 prioq 链表中。
*           b. 不可读，说明 todo 目录没有新文件进来，设定 25 分钟后再去遍历一下 todo 目录。
*/

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <utime.h>
#include "confparser.h"
#include "dictionary.h"
#include "stralloc.h"
#include "trigger.h"
#include "utils.h"
#include "sig.h"
#include "prioq.h"
#include "alloc.h"
#include "slog.h"

int log_level = 7;
int lifetime = 604800;

/*
 delivery retry times, for chanskip = 6
 *
 * Try ========= after ============         == delay until next ==
 * times    seconds dd hh mm ss             seconds     dd hh mm ss
 * 0        0       [00 00:00:00]           36          [00 00:00:36]
 * 1        36      [00 00:00:36]           108         [00 00:01:48]
 * 2        144     [00 00:02:24]           180         [00 00:03:00]
 * 3        324     [00 00:05:24]           252         [00 00:04:12]
 * 4        576     [00 00:09:36]           324         [00 00:05:24]
 * 5        900     [00 00:15:00]           396         [00 00:06:36]
 * 6        1296    [00 00:21:36]           468         [00 00:07:48]
 * 7        1764    [00 00:29:24]           540         [00 00:09:00]
 * 8        2304    [00 00:38:24]           612         [00 00:10:12]
 * 9        2916    [00 00:48:36]           684         [00 00:11:24]
 *
 */
int chanskip = 6;

static char *todo_path = "todo";
static char *queue_path = "queue";
static char *mess_path = "mess";

char todo_fn[2048] = {0};
char queue_fn[2048] = {0};
char mess_fn[2048] = {0};
void fnmake_queue(unsigned long id)
{
    snprintf(queue_fn, sizeof(queue_fn) - 1, "%s/%lu", queue_path, id);
}
void fnmake_todo(unsigned long id)
{
    snprintf(todo_fn, sizeof(todo_fn) - 1, "%s/%lu", todo_path, id);
}
void fnmake_mess(unsigned long id)
{
    snprintf(mess_fn, sizeof(mess_fn) - 1, "%s/%lu", mess_path, id);
}

char delnum_str[10] = {0};
int delnum_to_string(int num)
{
    return snprintf(delnum_str, sizeof(delnum_str), "%05d", num);
}

dictionary *conf_dict = NULL;
dictionary *read_conf(char *conf_fs)
{
    int i = 0;
    int ret = 0;

    struct conf_int_config conf_int_all_array[] = {
        {"log_level", &log_level},
        {"lifetime", &lifetime},
        {0, 0}};

    struct conf_str_config conf_str_all_array[] = {
        {0, 0}};

    dictionary *dict = open_conf_file(conf_fs);
    if (dict == NULL)
    {
        slog_error("open config file(%s) fail", conf_fs);
        return NULL;
    }
    ret = parse_conf_file(dict, "send", conf_int_all_array, conf_str_all_array);
    if (ret != 0)
    {
        slog_error("parse config file section[send] fail");
        return NULL;
    }

    return dict;
}

void clean_queue_mess(unsigned long id)
{
    fnmake_queue(id);
    if (unlink(queue_fn) == -1)
    {
        slog_warning("unable to unlink %s, error:%m, will try again later", queue_fn);
    }
    fnmake_mess(id);
    if (unlink(mess_fn) == -1)
    {
        slog_warning("unable to unlink %s, error:%m, will try again later", mess_fn);
    }
}

#define SLEEP_TODO 1500     /* check todo/ every 25 minutes in any case */
#define SLEEP_FOREVER 86400 /* absolute maximum time spent in select() */
#define SLEEP_FUZZ 1        /* slop a bit on sleeps to avoid zeno effect */
#define SLEEP_SYSFAIL 123

prioq pqchan = {0};

int numjobs;

int fdout = 3;
int fdin = 4;
char *tochan = " to remote ";

void nomem()
{
    _exit(51);
}

#define REPORTMAX 10000

long recent;
long nexttodorun;
DIR *todo_dir; /* if 0, have to opendir again */

int flag_exit = 0;
void sigterm()
{
    flag_exit = 1;
    slog_info("flag_exit = 1");
}

unsigned int scan_ulong(register char *s, register unsigned long *u)
{
    register unsigned int pos;
    register unsigned long result;
    register unsigned long c;
    pos = 0;
    result = 0;
    while ((c = (unsigned long)(unsigned char)(s[pos] - '0')) < 10)
    {
        result = result * 10 + c;
        ++pos;
    }
    *u = result;
    return pos;
}

unsigned int concurrency = 10;
unsigned int concurrencyused = 0;

void del_status();
void spawn_died();

// ------ JOB --------------------------------------------------------
struct job
{
    int used; /* if 0, this struct is unused */
    unsigned long id;
    stralloc uuid;
    unsigned long retry;
    unsigned long alive;
};
struct job *jo;

int flag_spawn_alive;
void job_init()
{
    flag_spawn_alive = 1;
    int j;
    while (!(jo = (struct jo *)alloc(numjobs * sizeof(struct job))))
        nomem();
    for (j = 0; j < numjobs; ++j)
    {
        jo[j].used = 0;
        jo[j].id = 0;
        jo[j].uuid.s = 0;
        jo[j].retry = 0;
        jo[j].alive = 0;
    }
    del_status();
}

int job_avali()
{
    int j;
    for (j = 0; j < numjobs; ++j)
    {
        if (!jo[j].used)
            return 1;
    }
    return 0;
}

int job_open(unsigned long id, stralloc *uuid)
{
    int j;
    for (j = 0; j < numjobs; ++j)
        if (!jo[j].used)
            break;

    if (j == numjobs)
        return -1;

    jo[j].used = 1;
    jo[j].id = id;

    // add uuid
    while (!stralloc_copy(&jo[j].uuid, uuid))
        return -1;

    slog_info("jo[%d].uuid:%s", j, jo[j].uuid);

    return j;
}

void job_close(int j)
{
    if (0 < --jo[j].used)
        return;

    jo[j].id = 0;
    jo[j].uuid.s = 0;
    jo[j].retry = 0;
}

// ------ DEL --------------------------------------------------------
// 读取rspawn内容
stralloc dline;
char del_buf[4096];
void del_dochan()
{
    int r;
    char ch;
    int i;
    int delnum;

    r = read(fdin, del_buf, sizeof(del_buf));
    if (r == -1)
        return;
    if (r == 0)
    {
        spawn_died();
        return;
    }
    slog_info("Total Read from rspawn data(%d):%s", r, del_buf);
    slog_info("Old dline len(%d):%s", dline.len, dline.s);

    for (i = 0; i < r; ++i)
    {
        ch = del_buf[i];
        while (!stralloc_append(&dline, &ch))
            nomem();
        if (dline.len > REPORTMAX)
            dline.len = REPORTMAX;

        slog_debug("i(%d) ch(%d)(%c) dline(%d):%s", i, ch, ch, dline.len, dline.s + dline.len - 1);
        if (!ch && (dline.len > 5))
        {
            slog_info("Read from rspawn data(%d):%s", dline.len, dline.s);
            // delnum = (((uint8_t)dline.s[0]) << 8) + (uint8_t)dline.s[1];
            char c5[5] = {0, 0, 0, 0, 0};
            c5[0] = dline.s[0];
            c5[1] = dline.s[1];
            c5[2] = dline.s[2];
            c5[3] = dline.s[3];
            c5[4] = dline.s[4];
            delnum = atoi(c5);

            if ((delnum < 0) || (delnum >= concurrency) || !jo[delnum].used)
            {
                slog_error("delnum(%d) concurrency(%d) used(%d)", delnum, concurrency, jo[delnum].used);
                slog_warning("delivery report out of range, delnum:%u, concurrency:%u",
                             delnum, concurrencyused);
            }
            else
            {
                // Z: 延迟，重试
                // D: 错误，删除
                // K: 成功，删除

                if (dline.s[5] == 'Z')
                {
                    if (jo[delnum].alive)
                    {
                        dline.s[5] = 'D';

                        slog_info("Defer delivery num:%d id:%lu mid:%s deferral:%s",
                                  delnum, jo[delnum].id, jo[delnum].uuid.s, dline.s + 5);

                        slog_info("Expire delivery num:%d id:%lu mid:%s falure: I'm not going to try again; this message has been in the queue too long.",
                                  delnum, jo[delnum].id, jo[delnum].uuid.s);
                    }
                }

                switch (dline.s[5])
                {
                case 'K':
                    slog_info("Succ delivery num:%d id:%lu mid:%s succ",
                              delnum, jo[delnum].id, jo[delnum].uuid.s);

                    clean_queue_mess(jo[delnum].id);
                    job_close(delnum);
                    break;

                case 'Z':
                    slog_info("Defer delivery num:%d id:%lu mid:%s deferral:%s",
                              delnum, jo[delnum].id, jo[delnum].uuid.s, dline.s + 5);

                    struct prioq_elt pe;
                    pe.id = jo[delnum].id;
                    pe.dt = jo[delnum].retry;
                    while (!prioq_insert(&pqchan, &pe))
                        nomem();

                    slog_info("Nextretry:%lu num:%d id:%lu mid:%s", jo[delnum].retry, delnum, jo[delnum].id, jo[delnum].uuid.s);
                    job_close(delnum);
                    break;

                case 'D':
                    slog_info("Final delivery num:%d id:%lu mid:%s failure:%s",
                              delnum, jo[delnum].id, jo[delnum].uuid.s, dline.s + 5);

                    clean_queue_mess(jo[delnum].id);
                    job_close(delnum);
                    break;

                default:
                    slog_info("delivery %d, report mangled, will defer, dline[5]:(%d)(%c)", delnum, dline.s[5], dline.s[5]);
                }
                --concurrencyused;
                del_status();
            }
            dline.len = 0;
            slog_debug("dline.len=0 i(%d)", i);
        }
        if (!ch && dline.len < 5)
        {
            dline.len = 0;
            slog_debug("dline.len=0 i(%d)", i);
        }
    }
}

int del_avail()
{
    return !(flag_spawn_alive & concurrencyused < concurrency);
}

void del_status()
{
    slog_info("status: %u/%u", concurrencyused, concurrency);
}

void del_init()
{
    dline.s = 0;
    while (!stralloc_copys(&dline, ""))
        nomem();
    del_status();
    slog_info("init dline len(%d)", dline.len);
}

void del_select_prep(int *nfds, fd_set *rfds)
{
    if (flag_spawn_alive)
    {
        slog_debug("FD_SET fdin to rfds");
        FD_SET(fdin, rfds);
        if (*nfds < fdin)
            *nfds = fdin + 1;
    }
}

void del_do(fd_set *rfds)
{
    if (flag_spawn_alive)
    {
        if (FD_ISSET(fdin, rfds))
        {
            slog_debug("rspawn is readable, read it");
            del_dochan();
        }
    }
}
// ------ DEL --------------------------------------------------------

// ------ COMM --------------------------------------------------------
stralloc comm_buf = {0};

void comm_write(int delnum, unsigned long id, char *uuid)
{
    // char c2[2] = {0, 0};
    if (comm_buf.s && comm_buf.len)
        return;
    while (!stralloc_copys(&comm_buf, ""))
        nomem();
    /*
    c2[0] = (delnum >> 8) & 0xff;
    c2[1] = delnum & 0xff;

    while (!stralloc_append(&comm_buf, &c2[0]))
        nomem();
    while (!stralloc_append(&comm_buf, &c2[1]))
        nomem();
*/
    delnum_to_string(delnum);
    while (!stralloc_cats(&comm_buf, delnum_str))
        nomem();
    while (!stralloc_cats(&comm_buf, uuid))
        nomem();
    while (!stralloc_cats(&comm_buf, ","))
        nomem();

    fnmake_mess(id);
    while (!stralloc_cats(&comm_buf, mess_fn))
        nomem();
    // while (!stralloc_cats(&comm_buf, ","))
    // nomem();
    while (!stralloc_append(&comm_buf, ""))
        nomem();

    slog_debug("comm_buf(%d):%s", comm_buf.len, comm_buf.s);
}

void comm_select_prep(int *nfds, fd_set *wfds)
{
    if (flag_spawn_alive)
    {
        slog_debug("comm_buf.len(%d)", comm_buf.len);
        if (comm_buf.s && comm_buf.len)
        {
            slog_debug("FD_SET fdout to wfds");
            FD_SET(fdout, wfds);
            if (*nfds <= fdout)
                *nfds = fdout + 1;
        }
    }
}

void comm_do(fd_set *wfds)
{
    if (flag_spawn_alive)
    {
        if (comm_buf.s && comm_buf.len)
        {
            if (FD_ISSET(fdout, wfds))
            {
                slog_debug("rspawn is writeable, write buf");
                int len = 0;
                int left = comm_buf.len;
                char *ptr = comm_buf.s;
                while (left)
                {
                    len = write(fdout, ptr, left);
                    if (len < 0)
                    {
                        if ((len == -1) && (errno == EPIPE))
                            spawn_died();
                        else
                            continue;
                    }
                    else
                    {
                        slog_info("Write to rspawn data(%d):%s", len, ptr);
                        left -= len;
                        ptr += len;
                    }
                }
                comm_buf.len = 0;
                slog_debug("set comm_buf.len = 0");
            }
        }
    }
}

void queue_init()
{
    // 读取之前留在 queue 目录中的消息
    DIR *queue_dir = opendir(queue_path);
    if (!queue_dir)
    {
        slog_info("Alert: unable to opendir(%s)\n", queue_dir);
        _exit(111);
    }
    struct dirent *pd;
    while ((pd = readdir(queue_dir)) != NULL)
    {
        slog_info("readdir dir(%s)", pd->d_name);
        if (strcasecmp(pd->d_name, ".") == 0)
            continue;
        if (strcasecmp(pd->d_name, "..") == 0)
            continue;

        unsigned long id;
        unsigned int len;
        struct prioq_elt pe;
        struct stat st;
        char fn[2048] = {0};

        len = scan_ulong(pd->d_name, &id);
        if (!len || pd->d_name[len])
            continue;

        snprintf(fn, sizeof(fn) - 1, "%s/%s", queue_path, pd->d_name);
        if (stat(fn, &st) == -1)
        {
            slog_error("stat file(%s) fail(%d):%s", fn, errno, strerror(errno));
            pe.id = id;
            pe.dt = time(NULL) + SLEEP_SYSFAIL;
            while (!prioq_insert(&pqchan, &pe))
                nomem();
            continue;
        }

        slog_info("insert prioq file(%s) id(%lu)", fn, id);
        pe.id = id;
        pe.dt = st.st_mtime;
        while (!prioq_insert(&pqchan, &pe))
            nomem();
    }
    closedir(queue_dir);
}

// ------ PASS --------------------------------------------------------
/* result^2 <= x < (result + 1)^2 */
/* x >= 0 */
static long square_root(long x)
{
    long y;
    long yy;
    long y21;

    int j;

    y = 0;
    yy = 0;
    for (j = 15; j >= 0; --j)
    {
        y21 = (y << (j + 1)) + (1 << (j + j));
        if (y21 <= x - yy)
        {
            y += (1 << j);
            yy += y21;
        }
    }
    return y;
}
long next_retry(unsigned long id, unsigned long *dt)
{
    struct stat st;
    char filename[1024] = {0};
    snprintf(filename, sizeof(filename) - 1, "%s/%lu", queue_path, id);

    if (stat(filename, &st))
    {
        slog_info("stat(%s) fail(%d):%s\n", filename, errno, strerror(errno));
    }

    long birth = st.st_mtime;
    *dt = birth;

    //slog_info("birth(%lu) recent(%lu)\n", birth, recent);
    int n;
    if (birth > recent)
        n = 0;
    else
        n = square_root(recent - birth); /* no need to add fuzz to recent */

    // slog_info("birth(%lu) n(%lu)\n", birth, n);
    n += chanskip;
    return birth + n * n;
}

void pass_select_prep(long *wakeup)
{
    struct prioq_elt pe;
    if (del_avail())
    {
        *wakeup = 0;
        return;
    }

    if (job_avali())
    {
        if (prioq_min(&pqchan, &pe))
        {
            if (*wakeup > pe.dt)
            {
                *wakeup = pe.dt;
            }
            slog_info("wakeup:%lu prioq pe: %lu\n", pe.dt, pe.id);
        }
    }
}

void pass_dochan()
{
    if (flag_exit)
        return;

    if (!job_avali())
        return;

    static stralloc uuid = {0};
    // char fn[2048] = {0};
    char buf[4098] = {0};

    struct prioq_elt pe;
    if (!prioq_min(&pqchan, &pe))
        return;

    if (pe.dt > recent)
        return;

    prioq_del_min(&pqchan);
    slog_debug("get pe succ and delete it from pqchan");

    // slog_info("\nBefore Exec file(%lu) dt(%lu)\n", pe.id, pe.dt);

    // 读取 queue 文件内容获取 uuid
    // snprintf(fn, sizeof(fn) - 1, "%s/%lu", queue_path, pe.id);
    // int fd = open(fn, O_RDONLY | O_NDELAY);
    fnmake_queue(pe.id);
    int fd = open(queue_fn, O_RDONLY | O_NDELAY);
    if (fd == -1)
    {
        slog_error("open file(%s) fail(%d):%s", queue_fn, errno, strerror(errno));
        goto trouble;
    }
    int n = read(fd, buf, sizeof(buf) - 1);
    slog_debug("read file(%s) buf(%d):%s", queue_fn, n, buf);
    buf[n] = 0;
    if (!stralloc_copys(&uuid, buf))
    {
        close(fd);
        goto trouble;
    }
    close(fd);
    slog_debug("uuid(%d):%s", uuid.len, uuid.s);

    unsigned long birth;
    int j = job_open(pe.id, &uuid);
    jo[j].retry = next_retry(pe.id, &birth);
    jo[j].alive = (recent > birth + lifetime);
    ++concurrencyused;
    // write to rspawn

    comm_write(j, jo[j].id, jo[j].uuid.s);
    slog_info("starting delivery num:%d id:%lu MID:%s nextretry:%lu",
              j, jo[j].id, jo[j].uuid.s, jo[j].retry);
    del_status();

    return;
    // pe.dt = jo[j].retry;
    // slog_info("Exec file(%lu) next dt(%lu)\n", pe.id, pe.dt);
    // slog_info("Exec fail, In to retry queue\n\n");
    // prioq_insert(&pqchan, &pe);

trouble:
    slog_warning("trouble opening %s, will try again later", queue_fn);
    pe.dt = recent + SLEEP_SYSFAIL;
    while (!prioq_insert(&pqchan, &pe))
        nomem();
}

void pass_do()
{
    slog_debug("pass_dochan");
    pass_dochan();
}
// ------ PASS --------------------------------------------------------

// ------ TODO --------------------------------------------------------
void todo_init()
{
    todo_dir = 0;
    nexttodorun = time(NULL);
    trigger_set();
}

void todo_select_prep(int *nfds, fd_set *rfds, long *wakeup)
{
    trigger_select(nfds, rfds);
    if (todo_dir)
        *wakeup = 0;
    if (*wakeup > nexttodorun)
        *wakeup = nexttodorun;
}

void todo_do(fd_set *rfds, char *queue_dir)
{
    struct prioq_elt pe;
    unsigned long id;
    unsigned int len;
    struct dirent *d;

    if (!todo_dir)
    {
        if (!trigger_pulled(rfds))
        {
            // 不可读, todo目录没有新文件进来
            if (recent < nexttodorun)
                return;
        }
        trigger_set();
        todo_dir = opendir(queue_dir);
        if (!todo_dir)
        {
            slog_info("Alert: unable to opendir(%s), sleep 10 seconds...\n", queue_dir);
            sleep(10);
            return;
        }
        nexttodorun = recent + SLEEP_TODO; // 设定下次运行时间为25分钟后
    }

    d = readdir(todo_dir);
    if (!d)
    {
        slog_info("close todo dir\n");
        closedir(todo_dir);
        todo_dir = 0;
        return;
    }
    slog_info("readdir file(%s)\n", d->d_name);

    if (strcasecmp(d->d_name, ".") == 0)
        return;
    if (strcasecmp(d->d_name, "..") == 0)
        return;

    len = scan_ulong(d->d_name, &id);
    if (!len || d->d_name[len])
        return;

    slog_info("insert prioq file(%s) id(%lu)\n", d->d_name, id);

    // snprintf(todo_fn, sizeof(todo_fn) - 1, "%s/%lu", todo_path, id);
    // snprintf(queue_fn, sizeof(queue_fn) - 1, "%s/%lu", queue_path, id);
    fnmake_todo(id);
    fnmake_queue(id);

    int todo_fd = open(todo_fn, O_RDONLY);
    if (todo_fd == -1)
    {
        slog_info("open file(%s) fail(%d):%s\n", todo_fn, errno, strerror(errno));
        return;
    }
    int queue_fd = open(queue_fn, O_CREAT | O_WRONLY | O_NDELAY, 0644);
    if (queue_fd == -1)
    {
        slog_info("open file(%s) fail(%d):%s\n", queue_fn, errno, strerror(errno));
        close(todo_fd);
        return;
    }

    char buf[1024] = {0};
    for (;;)
    {
        int n = read(todo_fd, buf, sizeof(buf) - 1);
        if (n > 0)
        {
            write(queue_fd, buf, n);
            continue;
        }
        break;
    }

    close(queue_fd);
    close(todo_fd);

    pe.id = id;
    pe.dt = time(NULL);
    while (!prioq_insert(&pqchan, &pe))
        nomem();

    unlink(todo_fn);

    return;
}
// ------ TODO --------------------------------------------------------

void pqfinish()
{
    struct prioq_elt pe;
    time_t ut[2];

    while (prioq_min(&pqchan, &pe))
    {
        prioq_del_min(&pqchan);
        fnmake_queue(pe.id);
        ut[0] = ut[1] = pe.dt;
        if (utime(queue_fn, (const struct utimbuf *)ut) == -1)
            slog_warning("unable to utime %s, error:%m, message will be retried too soon", queue_fn);
    }
}

void spawn_died()
{
    slog_alert("oh no, lost spawn connection! dying...\n");
    flag_spawn_alive = 0;
    flag_exit = 1;
}

void usage(char *prog)
{
    slog_info("Usage:\n");
    slog_info("    %s [todo dir]\n", prog);
    slog_info("Example:\n");
    slog_info("    %s todo\n", prog);
}

int main(int argc, char **argv)
{
    slog_open("sn-send", LOG_PID | LOG_NDELAY, LOG_MAIL);
    slog_info("Daemon Start...");

    slog_level = log_level;

    if (chdir(SNOTIFY_PATH) == -1)
    {
        slog_alert("cannot start: unable to switch to home directory");
        _exit(111);
    }

    conf_dict = read_conf("conf/snotify.ini");
    if (conf_dict == NULL)
    {
        slog_info("read config fail");
        return 1;
    }
    slog_level = log_level;

    slog_info("log_level: %d", log_level);
    slog_info("lifetime: %d", lifetime);

    sig_pipeignore();
    sig_termcatch(sigterm);
    sig_childdefault();
    umask(077);

    fd_set rfds;
    fd_set wfds;
    int nfds;
    struct timeval tv;
    long wakeup;

    numjobs = 0;
    char c2[5] = {0, 0, 0, 0, 0};
    int u, r;
    do
    {
        r = read(fdin, c2, 5);
    } while ((r == -1) && ((errno == EINTR) || (errno == EAGAIN)));

    if (r < 1)
    {
        slog_error("can't start: hath the dameon spawn no fire ?");
        _exit(111);
    }
    // u = ((uint8_t)c2[0] << 8) + (uint8_t)c2[1];
    u = atoi(c2);
    slog_info("u(%d)", u);
    if (concurrency < u)
        concurrency = u;
    numjobs += concurrency;

    if (ndelay_on(fdout) == -1)
    {
        spawn_died();
    }

    slog_info("get numjobs:%d", numjobs);

    queue_init();
    job_init();
    del_init();

    // 初始化todo目录指针 与 打开监控 trigger 文件
    todo_init();

    while (!flag_exit)
    {
        recent = time(NULL);
        wakeup = recent + SLEEP_FOREVER;

        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        nfds = 1;

        // 检查是否comm_buf中有数据需要发送，如果有，则设置wfds
        comm_select_prep(&nfds, &wfds);

        // 监控rspawn的fd可读
        del_select_prep(&nfds, &rfds);

        // 获取 pqchan 中优先级最高的 item 的dt
        pass_select_prep(&wakeup);

        // 设置监控trigger 如果 todo 目录打开的话，wakeup = 0 立刻读取todo目录的文件
        todo_select_prep(&nfds, &rfds, &wakeup);

        if (wakeup <= recent)
            tv.tv_sec = 0;
        else
            tv.tv_sec = wakeup - recent + SLEEP_FUZZ;
        tv.tv_usec = 0;

        slog_info("select timeout:%ld\n", tv.tv_sec);
        if (select(nfds, &rfds, &wfds, (fd_set *)0, &tv) == -1)
        {
            if (errno == EINTR)
            {
                slog_info("select errno(EINTR)");
            }
            else
            {
                slog_info("trouble in select");
            }
        }
        else
        {
            recent = time(NULL);

            // 如果rspawn可写，且comm_buf有内容，则发送内容给rspawn
            comm_do(&wfds);

            // 接收rspawn消息，并处理是否成功、重试、失败
            del_do(&rfds);

            // 检查 todo 目录是否已经打开:
            //  1. 打开，则读取 todo 文件并插入到 prioq 队列中。
            //  2. 未打开:
            //      a. trigger 可读, 设定25分钟后 nexttodorun，打开 todo 目录，并读取 todo 文件插入到 prioq 队列.
            //      b. trigger 不可读，检查当前是否已经过了25分钟:
            //          i. 没到25分钟，直接返回，什么都不做。
            //          ii. 过了25分钟，打开 todo 目录，遍历 todo 目录
            todo_do(&rfds, todo_path);

            // 从队列(prioq)中取优化级最高的item,并从队列删除此item
            // 执行投递此item内容到comm_buf中，准备在对方rspawn可写的情况发送
            pass_do();
        }
    }

    // pqfinish();
    slog_info("status: exiting");
    return 0;
}
