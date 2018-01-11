#ifndef LOGGING_H
#define LOGGING_H

void logmsg(const char *file, const char *func, int line, int errnoflag, int level, const char *fmt, ...);

#define LOG_ERR	3
#define LOG_INFO 6

#define ERR_QUIT(...) \
do { \
	logmsg(__FILE__, __FUNCTION__, __LINE__, 0, LOG_ERR, __VA_ARGS__); \
	exit(2); \
} while (0)

#define ERR_SYS(...) \
do { \
	logmsg(__FILE__, __FUNCTION__, __LINE__, 1, LOG_ERR, __VA_ARGS__); \
	exit(2); \
} while (0);

#endif /* LOGGING_H */
