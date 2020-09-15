#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>

char *SNOTIFY_PATH = "/var/snotify";

int fd_copy(int to, int from);
int fd_move(int to, int from);

int wait_nohang(int *wstat);

#define wait_crashed(w) ((w)&127)
#define wait_exitcode(w) ((w) >> 8)
#define wait_stopsig(w) ((w) >> 8)
#define wait_stopped(w) (((w)&127) == 127)

int ndelay_on(int fd);
int ndelay_off(int fd);

int get_uuid(char *uuid, size_t uuid_size);

void idx_char_to_uint(char *c2, int *delnum);
void idx_uint_to_char(int delnum, char *c2);

void byte_copy(char *to, int n, char *from);

#endif