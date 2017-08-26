#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <linux/limits.h>
#include <time.h>
#include <sys/time.h>
#include <syslog.h>

#include "xalloc.h"
#include "quecontrol.h"
#include "heapfunc.h"
#include "logging.h"
#include "utils.h"

#define WINSIZE 1
#define WINDEFSIZE 10
#define TIMERHEAPSIZE 16
#define DELTAUSEC 25

#ifdef DEBON
	#define LOG_MSG(macrolevel, ...) \
	debuglevel >= macrolevel ? logmsg(__FILE__, __FUNCTION__, __LINE__, 0, LOG_INFO, __VA_ARGS__) : 0;
#else
	#define LOG_MSG(macrolevel, ...) 
#endif

#define GETFILENAME(numque) quelist[numque].msg[quelist[numque].send]->inode;

#define MOVEFILE(queind_src, queind_dst, file) \
do { \
	snprintf(src, PATH_MAX, "%s/%ld", quelist[queind_src].dir, file); \
	snprintf(dst, PATH_MAX, "%s/%ld", quelist[queind_dst].dir, file); \
	if (mvfile(src, dst) < 0) \
		ERR_SYS("error move file %s to %s", src, dst); \
} while (0)

#define ADDTIMER(queind) \
do { \
	getmonotime(&now); \
	now.tv_sec += quelist[queind].timeout; \
	heap_insert(timerheap, now, queind); \
} while (0)

enum quenum {ACT, IN};

extern char *progname;
extern FILE *logstream;
extern int daemon_proc;

extern struct msglist *quelist;
int countq;

struct heap *timerheap;

char *logfile, *fifolog;
int debuglevel;

char *msgdir, *fifotrig, *brokersend;

char *drop	= "./drop/";
char *fail	= "./fail/";
char *df	= "./df/";
char *act	= "./act/";
char *in	= "./in/";

int
init(void)
{
	DIR *workdir = NULL;
	struct dirent *direntry = NULL;
	struct stat buf;
	char dir[PATH_MAX] = {0};
	int i, j, timeout;
	struct timeval now;
	
	daemon_proc = 1;
	if (logfile && (logstream = fopen(logfile, "w")))
		;
	else if (fifolog && (logstream = fopen(fifolog, "a")))
		;
	else {
		logstream = stderr;
		daemon_proc = 0;
	}
	
	if ( (msgdir = getenv("MSGDIR")) == NULL)
		ERR_QUIT("environment variable MSGDIR is not defined");
	else if ( (fifotrig = getenv("FIFOTRIGGER")) == NULL)
		ERR_QUIT("environment variable FIFOTRIGGER is not defined");
	else if ( (brokersend = getenv("BROKERSEND")) == NULL)
		ERR_QUIT("environment variable BROKERSEND is not defined");
	
	if (chdir(msgdir) < 0)
		ERR_SYS("failed to change working directory to %s", msgdir);
	
	countq = queue_add();
	quelist[countq].dir = act;
	quelist[countq].timeout = 0;
	
	countq = queue_add();
	quelist[countq].dir = in;
	quelist[countq].timeout = 0;
	
	if ( (workdir = opendir(df)) == NULL)
		ERR_SYS("error open directory %s", df);
	while ( (direntry = readdir(workdir)) != NULL) {
		if ((strcmp(direntry->d_name, CURDIR) == 0) || (strcmp(direntry->d_name, PARENTDIR) == 0))
			continue;
		snprintf(dir, PATH_MAX, "%s%s", df, direntry->d_name);
		if (stat(dir, &buf) < 0)
			ERR_SYS("error get file status %s", dir);
		if (S_ISDIR(buf.st_mode)) {
			if ( (timeout = calctimeout(direntry->d_name)) > 0) {
				countq = queue_add();
				quelist[countq].dir = strdup(dir);
				quelist[countq].timeout = timeout;
			}
			else
				LOG_MSG(2, "The directory has an incorrect name format %s", dir);
		}
	}
	closedir(workdir);
	
	qsort(quelist, countq+1, sizeof(struct msglist), queue_comp);
	timerheap = heap_create(TIMERHEAPSIZE);
	for (i = 0; i < countq+1; ++i) { 
		queue_restore(quelist+i, i);
		queue_sort(quelist+i);
		if (i != IN) {
			for (j = 0; j < quelist[i].total; ++j) {
				getmonotime(&now);
				heap_insert(timerheap, now, i);
			}
		}
	}
	LOG_MSG(1, "initialization is successfully completed");
	
	return 0;
}

int
scan_drop(void)
{
	DIR *workdir = NULL;
	struct dirent *direntry = NULL;
	struct stat buf;
	struct timeval tv;
	char src[PATH_MAX] = {0};
	char dst[PATH_MAX] = {0};
	int count = 0;

	if ( (workdir = opendir(drop)) == NULL)
		ERR_SYS("error open directory %s", drop);
	while ( (direntry = readdir(workdir)) != NULL) {
		if ((strcmp(direntry->d_name, CURDIR) == 0) || (strcmp(direntry->d_name, PARENTDIR) == 0))
			continue;
		if (strtoul(direntry->d_name, NULL, 10) == direntry->d_ino) {
			snprintf(src, PATH_MAX, "%s%s", drop, direntry->d_name);
			snprintf(dst, PATH_MAX, "%s%s", quelist[IN].dir, direntry->d_name);
			if (mvfile(src, dst) < 0)
				ERR_SYS("error move file %s to %s", src, dst);
			if (stat(dst, &buf) < 0)
				ERR_SYS("error get file status %s", dst);
			tv.tv_sec = buf.st_mtim.tv_sec;
			tv.tv_usec = buf.st_mtim.tv_nsec/1000;
			elem_add(&quelist[IN], elem_new(buf.st_ino, tv, IN));
			++count;
		}
	}
	closedir(workdir);
	
	if (count) {
		LOG_MSG(2, "%d files were read from %s", count, drop); 
		queue_sort(quelist+IN);
	}

	return 0;
}

int
check_response(ino_t inode, int retval, char *description)
{
	short i;
	int indelem = -1;
	int state;  
	char src[PATH_MAX] = {0};
	char dst[PATH_MAX] = {0};
	struct timeval now;
	
	LOG_MSG(2, "search message with inode %ld, code %d", inode, retval); 
	if ( (indelem = elem_find(quelist+ACT, inode)) < 0) {
		LOG_MSG(2, "message with inode %ld not found", inode); 
		return -1;
	}
	state = quelist[ACT].msg[indelem]->state;

	switch (retval) {
		case 0:
			snprintf(src, PATH_MAX, "%s%ld", act, inode);
			if (unlink(src) < 0)
				ERR_SYS("error delete file %s", src);
			if (fsyncdir(act) < 0)
				ERR_SYS("error synchronize directory %s", act);
			elem_del(quelist+ACT, inode);
			elem_del(quelist+state, inode);
			quelist[state].conf--;
			quelist[state].send--;
			
			LOG_MSG(2, "queue=%d, total=%d, send=%d, conf=%d", state, quelist[state].total, quelist[state].send, quelist[state].conf);
			break;
		case 1:
			quelist[state].conf++;
			LOG_MSG(2, "queue=%d, total=%d, send=%d, conf=%d", state, quelist[state].total, quelist[state].send, quelist[state].conf);
			break;
		default:
			if (state == countq) {
				snprintf(src, PATH_MAX, "%s/%ld", act, inode);
				snprintf(dst, PATH_MAX, "%s/%ld", fail, inode);
				if (mvfile(src, dst) < 0)
					ERR_SYS("error move file %s to %s", src, dst);
				elem_del(quelist+ACT, inode);
				elem_del(quelist+state, inode);
				quelist[state].conf--;
				quelist[state].send--;
				LOG_MSG(2, "message with inode %ld moved to directory %s", inode, fail);
				LOG_MSG(2, "queue=%d, total=%d, send=%d, conf=%d", state, quelist[state].total, quelist[state].send, quelist[state].conf);
			}
			else if (state == ACT) {
				MOVEFILE(ACT, state+2, inode);
				elem_add(quelist+state+2, elem_new(inode, (struct timeval){0, 0}, state+2));
				elem_del(quelist+ACT, inode);
				quelist[state].conf--; 
				quelist[state].send--; 
				LOG_MSG(2, "queue=%d, total=%d, send=%d, conf=%d", state, quelist[state].total, quelist[state].send, quelist[state].conf);
				LOG_MSG(2, "queue=%d, total=%d, send=%d, conf=%d", state+2, quelist[state+2].total, quelist[state+2].send, quelist[state+2].conf);
				ADDTIMER(state+2); 
				LOG_MSG(2, "timer for queue %d added", state+2);
			}
			else {	
				MOVEFILE(ACT, state+1, inode);
				elem_add(quelist+state+1, elem_new(inode, (struct timeval){0, 0}, state+1));
				elem_del(quelist+ACT, inode);
				elem_del(quelist+state, inode);
				quelist[state].conf--; 
				quelist[state].send--; 
				LOG_MSG(2, "queue=%d, total=%d, send=%d, conf=%d", state, quelist[state].total, quelist[state].send, quelist[state].conf);
				LOG_MSG(2, "queue=%d, total=%d, send=%d, conf=%d", state+1, quelist[state+1].total, quelist[state+1].send, quelist[state+1].conf);
				ADDTIMER(state+1); 
				LOG_MSG(2, "timer for queue %d added", state+1);
			}
			break;
	}
	return 0;
}

int
main(int argc, char *argv[])
{
	int pipefd1[2], pipefd2[2], flags;
	FILE *pipestream = NULL;
	
	struct pollfd fds[2];
	int trigfd, ret;

	int childpid, gopt;
	
	int code, indq;
	char description[DESCMAX] = {0};	
	char src[PATH_MAX] = {0};
	char dst[PATH_MAX] = {0};
	ino_t inode, dfinode, rinode; 
	
	int nearest, wait = -1;
	struct timeval now;
	struct heapnode timer = {0};
	struct heapnode *minnode = NULL;
	
	progname = argv[0];

	while ( (gopt = getopt(argc, argv, ":d:l:f:")) != -1) {
		switch(gopt) {
			case 'd':
				debuglevel = atoi(optarg); 
				break;
			case 'l':
				logfile = optarg;
				break;
			case 'f':
				fifolog = optarg;
				break;
			case '?':
			default:
			break;
		}
	}
	
	init();
	
	if ( (pipe(pipefd1)) < 0)
		ERR_SYS("can`t create pipefd1", progname);
	if ( (pipe(pipefd2)) < 0)
		ERR_SYS("can`t create pipefd2", progname);

	if ( (childpid = fork()) < 0) {
		ERR_SYS("error fork", progname);
	}
	else if (childpid == 0) {
		close(pipefd1[1]);
		close(STDIN_FILENO);
		dup(pipefd1[0]);
		close(pipefd1[0]);
			
		close(pipefd2[0]);
		close(STDOUT_FILENO);
		dup(pipefd2[1]);
		close(pipefd2[1]);
		if (execl(brokersend, "brokersend", NULL) < 0)
			ERR_SYS("error exec %s", brokersend); 
	}
	
	close(pipefd1[0]);
	close(pipefd2[1]);

	if ( (mkfifo(fifotrig, FILEMODE) < 0) && (errno != EEXIST))
		ERR_SYS("can`t create fifo %s", fifotrig);
	if ( (trigfd = open(fifotrig, O_RDONLY | O_NONBLOCK)) < 0)
		ERR_SYS("error open %s", fifotrig);
	
	if ( (flags = fcntl(pipefd2[0], F_GETFL, 0)) < 0)
		ERR_SYS("error get F_GETFL pipefd2", progname);
	if (fcntl(pipefd2[0], F_SETFL, flags | O_NONBLOCK) < 0)
		ERR_SYS("error set F_SETFL to pipefd2", progname);
	if ( (pipestream = fdopen(pipefd2[0], "r")) == NULL)
		ERR_SYS("error fdopen pipefd2", progname);


	for (;;) {
		fds[0].fd = -1;
		fds[0].events = POLLIN;
		fds[1].fd = pipefd2[0];
		fds[1].events = POLLIN;
		
		LOG_MSG(2, "queue=%d, total=%d, send=%d, conf=%d", IN, quelist[IN].total, quelist[IN].send, quelist[IN].conf);
		if ( (quelist[IN].send < WINSIZE) && (quelist[IN].send < quelist[IN].total) && (quelist[IN].send == quelist[IN].conf)) {
			inode = GETFILENAME(IN);
			MOVEFILE(IN, ACT, inode);
			elem_add(quelist+ACT, elem_new(inode, (struct timeval){0, 0}, IN));
			if (dprintf(pipefd1[1], "%ld\n", inode) < 0)
				ERR_SYS("error write %ld to pipefd1", inode);
			quelist[IN].send++;
			LOG_MSG(2, "message with inode %ld was written to pipe", inode);
			LOG_MSG(2, "queue=%d, total=%d, send=%d, conf=%d", IN, quelist[IN].total, quelist[IN].send, quelist[IN].conf);
		}
		else if (quelist[IN].total == quelist[IN].send) {
			fds[0].fd = trigfd;
			LOG_MSG(2, "trigger switched on");
		}

		if (wait > 0) {
			LOG_MSG(2, "current timeout is %d", wait);
			minnode = heap_min(timerheap);
			getmonotime(&now);
			nearest = (((minnode->key.tv_sec - now.tv_sec)*1000000L + minnode->key.tv_usec) - now.tv_usec)/1000;
			if (nearest < 0) nearest = DELTAUSEC;
			if (wait > nearest) { 
				wait = nearest;
				LOG_MSG(2, "min timeout of %d is set from the heap", wait);
			}
		}
		else if (timerheap->nodecount) {
			LOG_MSG(2, "number of timers in the heap %d", timerheap->nodecount);
			minnode = heap_min(timerheap);
			getmonotime(&now); 
			wait = (((minnode->key.tv_sec - now.tv_sec)*1000000L + minnode->key.tv_usec) - now.tv_usec)/1000;
			if (wait <= 0) wait = DELTAUSEC;
			LOG_MSG(2, "timeout of %d msec is set to poll", wait);
		} 
		else
			wait = -1;

		ret = poll(fds, 2, wait);
		if (ret < 0 && errno != EINTR) { 
			ERR_SYS("error poll", progname);
		}
		else if (ret == 0) {
			indq = minnode->indq;
			LOG_MSG(2, "timeout expired\n");
			wait = -1;
			if ( (quelist[indq].send < WINDEFSIZE) && (quelist[indq].send < quelist[indq].total) && (quelist[indq].send == quelist[indq].conf)) { 
				LOG_MSG(2, "queue=%d, total=%d, send=%d, conf=%d", indq, quelist[indq].total, quelist[indq].send, quelist[indq].conf);
				dfinode = GETFILENAME(indq);
				if (indq) {
					MOVEFILE(indq, ACT, dfinode);
					elem_add(quelist+ACT, elem_new(dfinode, (struct timeval){0, 0}, indq));
				}
				if (dprintf(pipefd1[1], "%ld\n", dfinode) < 0) 
					ERR_SYS("error write %ld to pipefd1", dfinode);
				quelist[indq].send++;
				LOG_MSG(2, "message with inode %ld was written to pipe", dfinode);
				LOG_MSG(2, "queue=%d, total=%d, send=%d, conf=%d", indq, quelist[indq].total, quelist[indq].send, quelist[indq].conf);
				heap_extract_min(timerheap);
				LOG_MSG(2, "min timer has been deleted from heap");
			} 
		}
		else {
			LOG_MSG(2, "trigger changed state");
			if (fds[0].revents & POLLIN) {
				close(trigfd);
				if (unlink(fifotrig) < 0)
					ERR_SYS("error delete file %s", fifotrig);
				if ( (mkfifo(fifotrig, FILEMODE) < 0) && (errno != EEXIST))
					ERR_SYS("can`t create fifo %s", fifotrig);
				if ( (trigfd = open(fifotrig, O_RDONLY | O_NONBLOCK)) < 0)
					ERR_SYS("error open %s", fifotrig);
				scan_drop();
			}
			if (fds[1].revents & POLLIN) {
				LOG_MSG(2, "data in the pipe are available");
				while (fscanf(pipestream, "%ld %d %[^\n]", &rinode, &code, description) != EOF)    
					check_response(rinode, code, description);
			}
		}
	}
	
	return 0;
}
