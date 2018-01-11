#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <linux/limits.h>

#include "xalloc.h"
#include "quecontrol.h"
#include "logging.h"

extern char *progname;
enum {ARINIT = 4, ARGROW = 2};

struct msglist *quelist;
static short quecount;

static int
mtimecomp(const void *msg1, const void *msg2)
{
	struct msg *m1 = *(struct msg **) msg1;
	struct msg *m2 = *(struct msg **) msg2;
	double diff;
	
	diff = ((m1->modtime.tv_sec - m2->modtime.tv_sec)*1000000L + m1->modtime.tv_usec) - m2->modtime.tv_usec;
	if (diff < 0)
		return -1; 
	else if (diff > 0)
		return 1;
	else
		return 0;
}

struct msg * 
elem_new(ino_t inode, struct timeval modtime, int state)
{
	struct msg *nelem;

	nelem = xmalloc(sizeof(struct msg));
	nelem->inode = inode;
	nelem->modtime = modtime;
	nelem->state = state;
	
	return nelem;
}

int
elem_add(struct msglist *list, struct msg *elem)
{
	struct msg **templist;
	
	if (list->msg == NULL) {
		list->msg = xmalloc(ARINIT * sizeof(struct msg *));
		list->nmax = ARINIT;
		list->total = 0;
	}
	else if (list->total >= list->nmax) {
		templist = xresize(list->msg, ARGROW * list->nmax * sizeof(struct msg *));
		list->nmax *= ARGROW;
		list->msg = templist;
	}

	list->msg[list->total++] = elem;
	
	return list->total;
}

int
elem_find(struct msglist *list, ino_t inode)
{
	int i;

	for (i = 0; i < list->total; ++i) { 
		if (list->msg[i]->inode == inode) 
			return i; 
	}	   
	
	return -1;
}

int 
elem_del(struct msglist *list, ino_t inode)
{
	int i;
	for (i = 0; i < list->total; ++i) {
		if (list->msg[i]->inode == inode) {
			xfree(list->msg[i]);
			memmove(list->msg+i, list->msg+i+1, (list->total - (i+1)) * sizeof(struct msg *));
			list->total--;
			return 1;
		}
	}
	
	return 0;
}

int
queue_add(void) 
{
	struct msglist *tempq = NULL;
	
	if (quelist == NULL) {
		tempq = xmalloc(sizeof(struct msglist));
		memset(tempq, 0, sizeof(struct msglist));
		quecount++;
		quelist = tempq;
	}
	else if ( (quecount > 0) && (quelist != NULL)) {
		tempq = xresize(quelist, (quecount+1) * sizeof(struct msglist));
		memset(tempq+quecount, 0, sizeof(struct msglist));
		quecount++;
		quelist = tempq;
	}

	return quecount-1;
}

int 
queue_restore(struct msglist *list, int index)
{
	DIR *workdir = NULL;
	struct dirent *direntry = NULL;
	struct stat buf;
	struct timeval tv;
	char src[PATH_MAX] = {0};
	
	workdir = opendir(list->dir);
	while ( (direntry = readdir(workdir)) != NULL) {
		if ((strcmp(direntry->d_name, CURDIR) == 0) || (strcmp(direntry->d_name, PARENTDIR) == 0))
			continue;
		sprintf(src, "%s/%s", list->dir, direntry->d_name);
		if (stat(src, &buf) < 0)
			ERR_SYS("error get file status %s", src);
		tv.tv_sec = buf.st_mtim.tv_sec;
		tv.tv_usec = buf.st_mtim.tv_nsec/1000;
		elem_add(list, elem_new(buf.st_ino, tv, index));
	}
	closedir(workdir);

	return list->total;
}

int
queue_comp(const void *queue1, const void *queue2)
{
	struct msglist *q1 = (struct msglist *) queue1;
	struct msglist *q2 = (struct msglist *) queue2;
	int diff;
	
	diff = q1->timeout - q2->timeout;
	if (diff < 0)
		return -1;
	else if (diff > 0)
		return 1;
	else
		return 0;
}

void
queue_sort(struct msglist *list)
{
	qsort(list->msg, list->total, sizeof(struct msg *), mtimecomp);
}
