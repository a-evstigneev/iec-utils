#ifndef UTILS_H
#define UTILS_H

int mvfile(const char *src, const char *dst);
int cpfile(const char *src, const char *dst);
int fsyncdir(const char *dir);
int calctimeout(const char *dir);
struct timeval *getmonotime(struct timeval *tv);

#endif
