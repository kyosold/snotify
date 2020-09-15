#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/epoll.h>
#include "confparser.h"
#include "dictionary.h"
#include "sig.h"
#include "utils.h"
#include "slog.h"
#include "stralloc.h"
#include "alloc.h"

int log_level = 8;
char exec_prog[1024] = {0};
int auto_spawn = 300000;

#define MAX_CONCURRENCY 65535

int wfd = 1;
int rfd = 0;

char delnum_str[10] = {0};
int delnum_to_string(int num)
{
    return snprintf(delnum_str, sizeof(delnum_str), "%05d", num);
}

dictionary *conf_dict = NULL;

int stage = -1;
int flag_abort = 0;
int delnum;
stralloc messid = {0};
stralloc uuid = {0};

// epoll
int epoll_fd = -1;
int epoll_nfds = -1;
int epoll_event_num = 0;
struct epoll_event *epoll_evts = NULL;
int epoll_num_running = 0;

void epoll_clean();
int epoll_del_evt(int epoll_fd, int fd);
int epoll_add_evt(int epoll_fd, int fd, int state);

struct delivery
{
    int used;
    int fdin;  /* pipe input */
    int fdout; /* pipe output, -1 if !pid; delays eof until after death */
    int pid;   /* zero if child is dead */
    int wstat; /* if !pid: status of child */
    stralloc output;
    stralloc msgid;
};

struct delivery *d;

dictionary *read_conf(char *conf_fs)
{
    int i = 0;
    int ret = 0;

    struct conf_int_config conf_int_all_array[] = {
        {"log_level", &log_level},
        {"max_concurrency", &auto_spawn},
        {0, 0}};

    struct conf_str_config conf_str_all_array[] = {
        {"notify_prog", exec_prog},
        {0, 0}};

    dictionary *dict = open_conf_file(conf_fs);
    if (dict == NULL)
    {
        slog_error("open config file(%s) fail", conf_fs);
        return NULL;
    }
    ret = parse_conf_file(dict, "rspawn", conf_int_all_array, conf_str_all_array);
    if (ret != 0)
    {
        slog_error("parse config file section[rspawn] fail");
        return NULL;
    }

    return dict;
}

void sigchld()
{
    int wstat;
    int pid;
    int i;
    while ((pid = wait_nohang(&wstat)) > 0)
    {
        for (i = 0; i < auto_spawn; ++i)
        {
            if (d[i].used)
            {
                if (d[i].pid == pid)
                {
                    close(d[i].fdout);
                    d[i].fdout = -1;
                    d[i].wstat = wstat;
                    d[i].pid = 0;
                }
            }
        }
    }
}

int swrite(int fd, char *buf, int n)
{
    int w;
    w = write(fd, buf, n);
    slog_debug("fd(%d) w(%d) buf(%d):%s", fd, w, n, buf);
    if (w != -1)
        return w;
    if (errno == EINTR)
        return -1;

    slog_debug("write to fd(%d) n(%d) buf(%s)", fd, n, buf);

    return n;
}

int swrite_str(int fd, char *str)
{
    return swrite(fd, str, strlen(str));
}

// int spawn(int mfd, int fdout, char *mid, char *mesid)
int spawn(int fdout, char *mid, char *mesid)
{
    int f;
    char *(args[4]);
    // args[0] = exec_name;
    args[0] = exec_prog;

    if (mid && *mid)
    {
        args[1] = mid;
    }
    else
    {
        args[1] = "";
    }
    if (mesid && *mesid)
    {
        char mess_fn[2048] = {0};
        snprintf(mess_fn, sizeof(mess_fn) - 1, "%s/%s", SNOTIFY_PATH, mesid);
        args[2] = mess_fn;
    }
    else
    {
        args[2] = "";
    }
    args[3] = 0;

    if (!(f = fork()))
    {
        // if (fd_move(0, mfd) == -1)
        // _exit(111);
        if (fd_move(1, fdout) == -1)
            _exit(111);
        if (fd_copy(2, 1) == -1)
            _exit(111);
        slog_info("Execvp:%s %s %s", args[0], args[1], args[2]);
        execvp(*args, args);
        if ((errno == EINTR) || (errno == EIO) || (errno == EAGAIN))
            _exit(111);
        else
            _exit(100);
    }
    return f;
}

#define err(s)                                 \
    do                                         \
    {                                          \
        int m = delnum_to_string(delnum);      \
        swrite(wfd, delnum_str, m);            \
        swrite(wfd, s, strlen(s));             \
        swrite(wfd, "", 1);                    \
        slog_error("delnum:%u %s", delnum, s); \
    } while (0)

void docmd()
{
    int i, j, f, fdmess;
    int pi[2];
    struct stat st;

    if (flag_abort)
    {
        err("Zrspawn out of memory");
        return;
    }
    if (delnum < 0)
    {
        err("ZInternal error: delnum negative.");
        return;
    }
    if (delnum >= auto_spawn)
    {
        err("Zinternal error: delnum too big.");
        return;
    }
    if (d[delnum].used)
    {
        err("ZInternal error: delnum in use.");
        return;
    }

    if (!stralloc_copys(&d[delnum].output, ""))
    {
        err("Zrspawn out of memeory.");
        return;
    }
    if (!stralloc_copys(&d[delnum].msgid, ""))
    {
        err("Zrspawn out of memory.");
        return;
    }
    if (!stralloc_copys(&d[delnum].msgid, uuid.s))
    {
        err("Zrspawn out of memory.");
        return;
    }

    /*
    fdmess = open(messid.s, O_RDONLY | O_NDELAY);
    if (fdmess == -1)
    {
        err("Zrspawn unable to open message.");
        return;
    }
    if (fstat(fdmess, &st) == -1)
    {
        close(fdmess);
        err("Zrspawn unable to fstat message.");
        return;
    }
    if ((st.st_mode & S_IFMT) != S_IFREG)
    {
        close(fdmess);
        err("Zsorry, message has wrong type.n");
        return;
    }
    */

    if (pipe(pi) == -1)
    {
        close(fdmess);
        err("Zrspawn unable to create pipe.");
        return;
    }
    // close on exec
    fcntl(pi[0], F_SETFD, 1);

    // f = spawn(fdmess, pi[1], uuid.s, messid.s);
    f = spawn(pi[1], uuid.s, messid.s);
    // close(fdmess);

    if (f == -1)
    {
        close(pi[0]);
        close(pi[1]);
        err("Zrspawn unable to fork.");
        return;
    }

    d[delnum].fdin = pi[0];
    d[delnum].fdout = pi[1];
    // close on exec
    fcntl(pi[1], F_SETFD, 1);
    d[delnum].pid = f;
    d[delnum].used = 1;

    // add to event
    int state;
    if (ndelay_on(d[delnum].fdin) == -1)
    {
        slog_error("%s setnonblocking fd(%d) fail.", uuid.s, d[delnum].fdin);
        state = EPOLLIN;
    }
    else
    {
        state = EPOLLIN | EPOLLET;
    }
    if (epoll_add_evt(epoll_fd, d[delnum].fdin, state) > 0)
    {
        slog_error("%s add delnum(%d) to epoll event fail.", uuid.s, delnum);
    }
    slog_debug("uuid:%s messid:%s delnum:%d fdin:%d fdout:%d add to event",
               d[delnum].msgid.s, messid.s, delnum, d[delnum].fdin, d[delnum].fdout);
}

char cmdbuf[4096];
void getcmd(int fd)
{
    int i, r;
    char ch;

    memset(cmdbuf, 0, sizeof(cmdbuf));

    r = read(fd, cmdbuf, sizeof(cmdbuf) - 1);
    if (r == 0)
    {
        slog_error("read send fail(%d):%s", errno, strerror(errno));
        epoll_del_evt(epoll_fd, fd);
        return;
    }

    if (r == -1)
    {
        if (errno != EINTR && errno != EAGAIN)
        {
            // delete event from epoll
            slog_error("read send fail(%d):%s", errno, strerror(errno));
            epoll_del_evt(epoll_fd, fd);
        }
        return;
    }
    slog_debug("Read comm_buf(%d)", r);

    // delnum + fn.s + , + uuid + ""
    char c5[5] = {0, 0, 0, 0, 0};
    int m = 0;
    int stage = -1;
    for (i = 0; i < r; i++)
    {
        ch = cmdbuf[i];
        if (stage < 4)
        {
            c5[m++] = ch;
            stage++;
            continue;
        }
        /*
        if (stage == -1)
        {
            delnum = (uint8_t)ch;
            slog_debug("cmd delnum 0(%d)", delnum);
            stage++;
            continue;
        }
        else if (stage == 0)
        {
            delnum = (delnum << 8) + (uint8_t)ch;
            uuid.len = 0;
            stage++;
            continue;
        }
        */
        else if (stage == 4)
        {
            delnum = atoi(c5);
            uuid.len = 0;
            do
            {

                if (ch != ',')
                {
                    if (!stralloc_catb(&uuid, &ch, 1))
                    {
                        flag_abort = 1;
                        break;
                    }
                    i++;
                    ch = cmdbuf[i];
                }
            } while (ch != ',');
            slog_debug("cmd uuid(%s)", uuid.s);
            messid.len = 0;
            stage++;
            continue;
        }
        else if (stage == 5)
        {
            do
            {
                if (ch != '\0')
                {
                    if (!stralloc_catb(&messid, &ch, 1))
                    {
                        flag_abort = 1;
                        break;
                    }
                    i++;
                    ch = cmdbuf[i];
                }
            } while (ch != '\0');
            slog_debug("cmd messid(%s)", messid.s);

            slog_info("delnum:%d uuid:%s messid:%s", delnum, uuid.s, messid.s);
            docmd();
            flag_abort = 0;

            stage = -1;
        }
    }
}

int epoll_add_evt(int epoll_fd, int fd, int state)
{
    struct epoll_event ev;
    ev.events = state;
    ev.data.fd = fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1)
    {
        slog_error("add event to epoll fail(%d):%s", errno, strerror(errno));
        return 1;
    }
    slog_debug("add event to epoll succ. fd:%d event:%d", fd, state);

    epoll_num_running++;

    return 0;
}

void report(int fd, int wstat, char *s, int len)
{
    int j, k, result, orr;

    if (wait_crashed(wstat))
    {
        swrite_str(fd, "Zremote crashed.");
        return;
    }

    switch (wait_exitcode(wstat))
    {
    case 0:
        break;
    case 111:
        swrite_str(fd, "Zunable to run remote.");
        return;
    default:
        swrite_str(fd, "Dunable to run remote.");
        return;
    }

    if (!len)
    {
        swrite_str(fd, "Zremote produced no output.");
        return;
    }

    swrite(fd, s, len);
}

int get_delnum_with_fd(int fd)
{
    int i = -1;
    for (i = 0; i < (auto_spawn + 10); ++i)
    {
        // slog_debug("i:%d fdin:%d fd:%d used:%d", i, d[i].fdin, fd, d[i].used);
        if ((d[i].fdin == fd) && (d[i].used == 1))
        {
            break;
        }
    }
    slog_debug("delnum:%d fdin:%d fd:%d used:%d", i, d[i].fdin, fd, d[i].used);
    return i;
}

int epoll_del_evt(int epoll_fd, int fd)
{
    struct epoll_event ev;
    ev.data.fd = fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, 0) == -1)
    {
        slog_error("delete event from epoll fail(%d):%s", errno, strerror(errno));
        return 1;
    }
    slog_debug("delete event from epoll succ. fd:%d", fd);

    epoll_num_running--;

    return 0;
}

void epoll_clean()
{
    if (epoll_evts != NULL)
    {
        free(epoll_evts);
        epoll_evts = NULL;
    }
    if (epoll_fd > 0)
    {
        close(epoll_fd);
        epoll_fd = -1;
    }

    epoll_event_num = 0;
}

char inbuf[4096] = {0};

int main(int argc, char **argv)
{
    int i, r;

    openlog("sn-rsapwn", LOG_PID | LOG_NDELAY, LOG_MAIL);
    slog_info("Daemon Start...");

    if (chdir(SNOTIFY_PATH) == -1)
    {
        slog_alert("cannot start: unable to switch to home directory");
        _exit(111);
    }

    if (!stralloc_copys(&uuid, ""))
        _exit(111);
    if (!stralloc_copys(&messid, ""))
        _exit(111);

    conf_dict = read_conf("conf/snotify.ini");
    if (conf_dict == NULL)
    {
        slog_info("read config fail");
        return 1;
    }
    slog_level = log_level;

    slog_info("log_level: %d", log_level);
    slog_info("max_concurrency: %d", auto_spawn);
    slog_info("exec_prog: %s", exec_prog);

    d = (struct delivery *)alloc((auto_spawn + 10) * sizeof(struct delivery));
    if (!d)
        _exit(1101);

    sig_pipeignore();
    sig_childcatch(sigchld);

    if (auto_spawn > MAX_CONCURRENCY)
    {
        auto_spawn = MAX_CONCURRENCY;
        slog_warning("set max concurrency too big, use default: %d", auto_spawn);
    }

    int n = delnum_to_string(auto_spawn);
    slog_info("pdelnum(%d):%s", n, delnum_str);
    swrite(wfd, delnum_str, 5);

    for (i = 0; i < auto_spawn; ++i)
    {
        d[i].used = 0;
        d[i].output.s = 0;
    }

    // epoll init
    epoll_event_num = auto_spawn + 11;
    epoll_evts = NULL;
    epoll_fd = -1;
    epoll_nfds = -1;

    int epoll_i = 0;
    epoll_evts = (struct epoll_event *)alloc(epoll_event_num * sizeof(struct epoll_event));
    if (epoll_evts == NULL)
    {
        slog_info("alloc epoll event fail");
        _exit(111);
    }

    // epoll create fd
    epoll_fd = epoll_create(epoll_event_num);
    if (epoll_fd <= 0)
    {
        slog_info("create epoll fd fail(%d):%s", errno, strerror(errno));
        epoll_clean();
        _exit(111);
    }

    // add rfd to epoll for sn-send
    struct epoll_event send_evt;
    send_evt.events = EPOLLIN;
    send_evt.data.fd = rfd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, send_evt.data.fd, &send_evt) == -1)
    {
        slog_info("add rfd event(EPOLLIN) to epoll fail(%d):%s", errno, strerror(errno));
        epoll_clean();
        _exit(111);
    }
    epoll_num_running = 1;

    for (;;)
    {
        // get epoll event
        epoll_nfds = epoll_wait(epoll_fd, epoll_evts, epoll_event_num, -1);
        if (epoll_nfds == -1)
        {
            if (errno == EINTR)
                continue;
            _exit(112);
        }
        else if (epoll_nfds > 0)
        {
            slog_debug("epoll_num_running:%d nfds:%d", epoll_num_running, epoll_nfds);
            for (epoll_i = 0; epoll_i < epoll_nfds; epoll_i++)
            {
                sig_childblock();

                int evt_fd = epoll_evts[epoll_i].data.fd;
                slog_debug("epoll_id:%d fd:%d event:%d", epoll_i, evt_fd, epoll_evts[epoll_i].events);

                if (epoll_evts[epoll_i].data.fd == send_evt.data.fd)
                {
                    // Send has events
                    if (epoll_evts[epoll_i].events & EPOLLIN)
                    {
                        // read data
                        slog_debug("read data from send");
                        getcmd(send_evt.data.fd);
                    }
                }
                else if (epoll_evts[epoll_i].events & EPOLLIN)
                {
                    // remote 有可读信息
                    int di = get_delnum_with_fd(evt_fd);
                    slog_debug("read data from remote event: epoll_i(%d) di(%d) fd(%d) uuid(%s), used(%d)",
                               epoll_i, di, d[di].fdin, d[di].msgid.s, d[di].used);
                    do
                    {
                        r = read(d[di].fdin, inbuf, sizeof(inbuf));
                        slog_debug("read remote %s data(%d):%s", d[di].msgid.s, r, inbuf);

                        if (r == -1)
                        {
                            if (errno == EAGAIN || errno == EINTR)
                                continue;
                            slog_error("read remote %s data fail(%d):%s", d[di].msgid.s, errno, strerror(errno));
                        }
                        else if (r > 0)
                        {
                            while (!stralloc_readyplus(&d[di].output, r))
                                sleep(10);

                            // XXX
                            // byte_copy(d[di].output.s + d[di].output.len, r, inbuf);
                            // d[di].output.len += r;
                            if (!stralloc_catb(&d[di].output, inbuf, r))
                            {
                                slog_error("stralloc_append fail");
                                break;
                            }

                            slog_info("read remote %s output(%d):%s", d[di].msgid.s, d[di].output.len, d[di].output.s);
                        }
                        else
                        {
                            break;
                        }
                        // } while (r == sizeof(inbuf));
                    } while (1);
                }
                else if ((epoll_evts[epoll_i].events & EPOLLHUP) && (epoll_evts[epoll_i].data.fd != send_evt.data.fd))
                {
                    int di = get_delnum_with_fd(evt_fd);
                    slog_debug("remote %s hup di(%d)", d[di].msgid.s, di);

                    delnum_to_string(di);
                    slog_info("Write to send data(%d):%s%s", d[di].output.len, delnum_str, d[di].output.s);
                    swrite(wfd, delnum_str, 5);
                    report(wfd, d[di].wstat, d[di].output.s, d[di].output.len);
                    swrite(wfd, "", 1);

                    epoll_del_evt(epoll_fd, d[di].fdin);

                    close(d[di].fdin);
                    d[di].used = 0;

                    slog_debug("%s epoll_num_running:%d/%d", d[di].msgid.s, epoll_num_running, epoll_nfds);
                }
                else
                {
                    slog_info("get other epoll event:%d", epoll_evts[epoll_i].events);
                }
                sig_childunblock();
            }
        }
    }

    return 0;
}
