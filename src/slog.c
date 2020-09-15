#include <stdio.h>
#include <string.h>
#include "slog.h"

int slog_level = SLOG_INFO;
char slog_sid[1024] = {0};

void slog_open(const char *ident, int opt, int facility)
{
    openlog(ident, opt, facility);
}
