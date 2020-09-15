#ifndef TRIGGER_H
#define TRIGGER_H

#include <sys/select.h>

void trigger_pull();

void trigger_set();
void trigger_select(int *nfds, fd_set *rfds);
int trigger_pulled(fd_set *rfds);

#endif