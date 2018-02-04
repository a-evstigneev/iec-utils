#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <time.h>
#include <assert.h>

#include "xalloc.h"
#include "logging.h"

int
mvfile(const char *src, const char *dst)
{
	int ret, srcdirfd, dstdirfd;
	char *srcdir, *dstdir;
	char *srcpath = strdup(src);
	char *dstpath = strdup(dst);

	srcdir = dirname(srcpath);
	dstdir = dirname(dstpath);

	if ( (srcdirfd = open(srcdir, O_RDONLY|O_NONBLOCK|O_DIRECTORY|O_CLOEXEC)) < 0)
		ret = -1;
	else if ( (dstdirfd = open(dstdir, O_RDONLY|O_NONBLOCK|O_DIRECTORY|O_CLOEXEC)) < 0)
		ret = -1;
	else if (rename(src, dst) == 0) {
		if (fsync(srcdirfd) < 0)
			ret = -1; 
		if (fsync(dstdirfd) < 0)
			ret = -1;
		close(srcdirfd);
		close(dstdirfd);
		ret = 0;
	} 
	else
		ret = -1;

	xfree(srcpath);
	xfree(dstpath);

	return ret;
}

int
cpfile(const char *src, const char *dst)
{
	int ret, srcdirfd, dstdirfd;
	char *srcdir, *dstdir;
	char *srcpath = strdup(src);
	char *dstpath = strdup(dst);

//	srcdir = dirname(srcpath);
//	dstdir = dirname(dstpath);

	ret = link(srcpath, dstpath);
	
	
//	if ( (srcdirfd = open(srcdir, O_RDONLY|O_NONBLOCK|O_DIRECTORY|O_CLOEXEC)) < 0)
//		ret = -1;
//	else if ( (dstdirfd = open(dstdir, O_RDONLY|O_NONBLOCK|O_DIRECTORY|O_CLOEXEC)) < 0)
//		ret = -1;
//	else if (rename(src, dst) == 0) {
//		if (fsync(srcdirfd) < 0)
//			ret = -1; 
//		if (fsync(dstdirfd) < 0)
//			ret = -1;
//		close(srcdirfd);
//		close(dstdirfd);
//		ret = 0;
//	} 
//	else
//		ret = -1;

	xfree(srcpath);
	xfree(dstpath);

	return ret;
}

int
fsyncdir(const char *dir)
{
	int dirfd, ret = 0;

	if ( (dirfd = open(dir, O_RDONLY|O_NONBLOCK|O_DIRECTORY|O_CLOEXEC)) < 0)
		ret = -1;
	else if (fsync(dirfd) < 0)
		ret = -1;
	else if (close(dirfd) < 0)
		ret = -1;

	return ret;
}

int
calctimeout(const char *dir)
{
	int n, h, m, s, timeout;
	char hour[NAME_MAX] = {0}, min[NAME_MAX] = {0}, sec[NAME_MAX] = {0};

	if ( (n = sscanf(dir, "%2[0-9]h%2[0-9]m%2[0-9]s", hour, min, sec)) != 3) {
		return -1; 
	}
	h = atoi(hour);
	m = atoi(min);
	s = atoi(sec);
	timeout = h*60*60 + m*60 + s;

	return timeout;
}

struct timeval *
getmonotime(struct timeval *tv) 
{
	struct timespec ts; 
	
	assert(tv != NULL);
	
	if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
		ERR_SYS("error clock_gettime()");
	tv->tv_sec = ts.tv_sec;
	tv->tv_usec = ts.tv_nsec / 1000;
	
	return tv; 
}
