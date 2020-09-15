#ifndef SLOG_H
#define SLOG_H

#include <sys/syslog.h>

extern int slog_level;
extern char slog_sid[1024];

#define SLOG_DEBUG 8
#define SLOG_INFO 7
#define SLOG_NOTICE 6
#define SLOG_WARNING 5
#define SLOG_ERROR 4
#define SLOG_CRIT 3
#define SLOG_ALERT 2
#define SLOG_EMERG 1

void slog_open(const char *ident, int opt, int facility);

#define slog_emerg(fmt, ...)                                                                \
    {                                                                                       \
        if (slog_level >= SLOG_EMERG)                                                       \
        {                                                                                   \
            if (*slog_sid == '\0')                                                          \
            {                                                                               \
                syslog(LOG_EMERG, "[EMERG] %s " fmt, __func__, ##__VA_ARGS__);              \
            }                                                                               \
            else                                                                            \
            {                                                                               \
                syslog(LOG_EMERG, "[EMERG] %s %s " fmt, __func__, slog_sid, ##__VA_ARGS__); \
            }                                                                               \
        }                                                                                   \
    }

#define slog_alert(fmt, ...)                                                                \
    {                                                                                       \
        if (slog_level >= SLOG_ALERT)                                                       \
        {                                                                                   \
            if (*slog_sid == '\0')                                                          \
            {                                                                               \
                syslog(LOG_ALERT, "[ALERT] %s " fmt, __func__, ##__VA_ARGS__);              \
            }                                                                               \
            else                                                                            \
            {                                                                               \
                syslog(LOG_ALERT, "[ALERT] %s %s " fmt, __func__, slog_sid, ##__VA_ARGS__); \
            }                                                                               \
        }                                                                                   \
    }

#define slog_crit(fmt, ...)                                                               \
    {                                                                                     \
        if (slog_level >= SLOG_CRIT)                                                      \
        {                                                                                 \
            if (*slog_sid == '\0')                                                        \
            {                                                                             \
                syslog(LOG_CRIT, "[CRIT] %s " fmt, __func__, ##__VA_ARGS__);              \
            }                                                                             \
            else                                                                          \
            {                                                                             \
                syslog(LOG_CRIT, "[CRIT] %s %s " fmt, __func__, slog_sid, ##__VA_ARGS__); \
            }                                                                             \
        }                                                                                 \
    }

#define slog_error(fmt, ...)                                                              \
    {                                                                                     \
        if (slog_level >= SLOG_ERROR)                                                     \
        {                                                                                 \
            if (*slog_sid == '\0')                                                        \
            {                                                                             \
                syslog(LOG_ERR, "[ERROR] %s " fmt, __func__, ##__VA_ARGS__);              \
            }                                                                             \
            else                                                                          \
            {                                                                             \
                syslog(LOG_ERR, "[ERROR] %s %s " fmt, __func__, slog_sid, ##__VA_ARGS__); \
            }                                                                             \
        }                                                                                 \
    }

#define slog_warning(fmt, ...)                                                                  \
    {                                                                                           \
        if (slog_level >= SLOG_WARNING)                                                         \
        {                                                                                       \
            if (*slog_sid == '\0')                                                              \
            {                                                                                   \
                syslog(LOG_WARNING, "[WARNING] %s " fmt, __func__, ##__VA_ARGS__);              \
            }                                                                                   \
            else                                                                                \
            {                                                                                   \
                syslog(LOG_WARNING, "[WARNING] %s %s " fmt, __func__, slog_sid, ##__VA_ARGS__); \
            }                                                                                   \
        }                                                                                       \
    }

#define slog_notice(fmt, ...)                                                                 \
    {                                                                                         \
        if (slog_level >= SLOG_NOTICE)                                                        \
        {                                                                                     \
            if (*slog_sid == '\0')                                                            \
            {                                                                                 \
                syslog(LOG_NOTICE, "[NOTICE] %s " fmt, __func__, ##__VA_ARGS__);              \
            }                                                                                 \
            else                                                                              \
            {                                                                                 \
                syslog(LOG_NOTICE, "[NOTICE] %s %s " fmt, __func__, slog_sid, ##__VA_ARGS__); \
            }                                                                                 \
        }                                                                                     \
    }

#define slog_info(fmt, ...)                                                               \
    {                                                                                     \
        if (slog_level >= SLOG_INFO)                                                      \
        {                                                                                 \
            if (*slog_sid == '\0')                                                        \
            {                                                                             \
                syslog(LOG_INFO, "[INFO] %s " fmt, __func__, ##__VA_ARGS__);              \
            }                                                                             \
            else                                                                          \
            {                                                                             \
                syslog(LOG_INFO, "[INFO] %s %s " fmt, __func__, slog_sid, ##__VA_ARGS__); \
            }                                                                             \
        }                                                                                 \
    }

#define slog_debug(fmt, ...)                                                                \
    {                                                                                       \
        if (slog_level >= SLOG_DEBUG)                                                       \
        {                                                                                   \
            if (*slog_sid == '\0')                                                          \
            {                                                                               \
                syslog(LOG_DEBUG, "[DEBUG] %s " fmt, __func__, ##__VA_ARGS__);              \
            }                                                                               \
            else                                                                            \
            {                                                                               \
                syslog(LOG_DEBUG, "[DEBUG] %s %s " fmt, __func__, slog_sid, ##__VA_ARGS__); \
            }                                                                               \
        }                                                                                   \
    }

#endif