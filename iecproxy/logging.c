#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <time.h>
#include <syslog.h> 

#define MSGMAX 4096
#define LOG_DAEMON (3<<3) /* 24 */

char *progname;
FILE *logstream; 
int daemon_proc;

void
logmsg(const char *file, const char *func, int line, int errnoflag, int level, const char *fmt, ...)
{
	va_list ap;
	pid_t pid;
	char hostname[HOST_NAME_MAX] = {0};
	int errno_save, n;
	char buf[MSGMAX] = {0};
	char time[32] = {0};
	struct timeval tv;
	struct tm *tm;
	char *monthnames[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

	va_start(ap, fmt);
	errno_save = errno;

	if (daemon_proc) {
		gettimeofday(&tv, NULL);
		tm = localtime(&tv.tv_sec);
		snprintf(time, sizeof(time),"%s %02d %2.2d:%2.2d:%2.2d", monthnames[tm->tm_mon], tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
		pid = getpid();
		gethostname(hostname, HOST_NAME_MAX);
//		snprintf(buf, MSGMAX, "%d %s %s %s[%d]: ", LOG_DAEMON + level, time, hostname, progname, pid);
		snprintf(buf, MSGMAX, "%s %s %s[%d]: ", time, hostname, progname, pid);

	}
	else
		snprintf(buf, MSGMAX, "[ file: %s, func: %s, line: %d ] ", file, func, line);
		
	n = strlen(buf);
	vsnprintf(buf + n, MSGMAX - n, fmt, ap);
	
	if (errnoflag) {
		n = strlen(buf);
		snprintf(buf + n, MSGMAX - n, " (%s)", strerror(errno_save));
	}
	
	fprintf(logstream, "%s\n", buf);
	fflush(logstream);
	va_end(ap);
	
	return;
}
