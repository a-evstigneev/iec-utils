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
#include <signal.h>
#include <sys/ioctl.h>

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

#define ADDTIMER(queind, inode) \
do { \
	getmonotime(&now); \
	now.tv_sec += quelist[queind].timeout; \
	heap_insert(timerheap, now, queind, inode); \
} while (0)

enum quenum {ACT, IN};

extern char *progname;
extern FILE *logstream;
extern int daemon_proc;

extern struct msglist *quelist;
int countq;

pid_t childpid;

struct heap *timerheap;

int debuglevel;

char *drop	= "./drop/";
char *fail	= "./fail/";
char *df	= "./df/";
char *act	= "./act/";
char *in	= "./in/";

int signalpipe[2];
int nsig;

int pipefd1[2], pipefd2[2], flags;
FILE *pipestream = NULL;

int
initqueue(void)
{
	DIR *workdir = NULL;
	struct dirent *direntry = NULL;
	struct stat buf;
	char dir[PATH_MAX] = {0};
	int i, j, timeout;
	struct timeval now;
	
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
	for (i = 0; i < countq+1; ++i) { // i начинается с 1. act востанавливать не надо, так как при выходе мы его чистим. 
		queue_restore(quelist+i, i);
		queue_sort(quelist+i);
		if (i != IN) {
			for (j = 0; j < quelist[i].total; ++j) {
				getmonotime(&now);
				heap_insert(timerheap, now, i, 0); // Надо сюда будет забивать корректные inode при востановлении
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
			return 1;
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
				ADDTIMER(state+2, inode); 
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
				ADDTIMER(state+1, inode); 
				LOG_MSG(2, "timer for queue %d added", state+1);
			}
			break;
	}
	return 0;
}

void
flag_signal(const char c)
{
	char ch = c;

	if (write(signalpipe[1], &ch, 1) < 0) {
		exit(EXIT_FAILURE);
		; // обработка ошибок с выводом в stderr
	}
}

void 
sig_term(int signum)
{
	char src[PATH_MAX] = {0};
	int n = quelist[ACT].total;

//	while (n >= 0) { // Удаляем файлы из каталога act, так как это временный буфер для отправки файлов
//		snprintf(src, PATH_MAX, "%s/%ld", act, quelist[ACT].msg[n]->inode);
//		unlink(src);
//		--n;
//	}
	
	if (childpid > 0) 
		kill(childpid, SIGTERM);
	else 
		exit(EXIT_SUCCESS);
		// Очистить очередь ACT, чтобы в каталоге act не было сообщений
}

void 
sig_chld(int signum)
{
	pid_t pid;
	int stat;

	pid = waitpid(-1, &stat, WNOHANG);
	
	if (pid == childpid) {
		childpid = 0;
		raise(SIGTERM);
	}
}

void 
sig_hup(int signum) 
{
	flag_signal('H'); 
}

void 
sig_usr1(int signum) 
{
	flag_signal('U'); 
}

int
flush_queue(void)
{

	char src[PATH_MAX] = {0};
	char dst[PATH_MAX] = {0};
	ino_t dfinode, rinode;
	int queuesend, code, n;
	char description[DESCMAX] = {0};	
	
	n = 0;
	queuesend = countq;

	while (queuesend > 1) {
		while (quelist[queuesend].total > 0) {
			if ( (quelist[queuesend].send < WINSIZE) && (quelist[queuesend].send < quelist[queuesend].total) && (quelist[queuesend].send == quelist[queuesend].conf)) { 
				dfinode = GETFILENAME(queuesend);
				MOVEFILE(queuesend, ACT, dfinode);
				LOG_MSG(2, "Нашли файл %d в очереди %d", dfinode, queuesend);
				elem_add(quelist+ACT, elem_new(dfinode, (struct timeval){0, 0}, queuesend));
				
				if (dprintf(pipefd1[1], "%ld\n", dfinode) < 0) 
					ERR_SYS("error write %ld to pipefd1", dfinode);
		
				LOG_MSG(2, "Отправили файл %d в pipefd1", dfinode);
				quelist[queuesend].send++;
				
				LOG_MSG(2, "queue=%d, total=%d, send=%d, conf=%d", queuesend, quelist[queuesend].total, quelist[queuesend].send, quelist[queuesend].conf);
			}

			while (fscanf(pipestream, "%ld %d %[^\n]", &rinode, &code, description) != EOF)    
				if (check_response(rinode, code, description) > 0)
					++n;
		}
		queuesend--;
	}

	return n;
}

int
main(int argc, char *argv[])
{
	struct pollfd fds[3];
	int trigfd;
	
	char *workdir, *trigger, *crts_name, *worker;
		
	int gopt, code, indq, ret;
	
	char description[DESCMAX] = {0};	
	char src[PATH_MAX] = {0};
	char dst[PATH_MAX] = {0};
	
	ino_t inode, dfinode, rinode; 
	
	int nearest, wait = -1;
	struct timeval now;
	struct heapnode timer = {0};
	struct heapnode *minnode = NULL;
	
	struct sigaction sa, saterm, sachld, sahup, sausr1;
	sigset_t oldset;

	progname = argv[0];

	while ( (gopt = getopt(argc, argv, ":n:d:t:s:l:")) != -1) {
		switch(gopt) {
			case 'n':
				crts_name = optarg;
				break;
			case 'd':
				workdir = optarg;
				break;
			case 't':
				trigger = optarg;
				break;
			case 's':
				worker = optarg;
				break;
			case 'l':
				debuglevel = atoi(optarg); 
				break;
			case '?':
			default:
				break;
		}
	}

	logstream = stderr;
	daemon_proc = 1;
	
	if (chdir(workdir) < 0)
		ERR_SYS("failed to change working directory to %s", workdir);
	
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);	
	sigaddset(&sa.sa_mask, SIGTERM);
	sigaddset(&sa.sa_mask, SIGCHLD);
	sigaddset(&sa.sa_mask, SIGHUP);
	sigaddset(&sa.sa_mask, SIGUSR1);

//	sigprocmask(SIG_BLOCK, &sa.sa_mask, &oldset);

	sa.sa_handler = sig_term;
	sigaction(SIGTERM, &sa, &saterm);
	sa.sa_handler = sig_chld;
	sigaction(SIGCHLD, &sa, &sachld);
	sa.sa_handler = sig_hup;
	sigaction(SIGHUP, &sa, &sahup);
	sa.sa_handler = sig_usr1;
	sigaction(SIGUSR1, &sa, &sausr1);

	initqueue(); 
	
	if (pipe(signalpipe) < 0)
		ERR_SYS("can`t create signalpipe", progname);
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
		
		sigaction(SIGTERM, &saterm, NULL);
		sigaction(SIGCHLD, &sachld, NULL);
		sigaction(SIGHUP,  &sahup,  NULL);
		sigaction(SIGUSR1, &sausr1, NULL);
		
		if (execlp(worker, worker, crts_name, NULL) < 0)
			ERR_SYS("error exec %s", worker); 
		exit(EXIT_FAILURE);
	}
	
	close(pipefd1[0]);
	close(pipefd2[1]);

	if ( (mkfifo(trigger, FILEMODE) < 0) && (errno != EEXIST))
		ERR_SYS("can`t create fifo %s", trigger);
	if ( (trigfd = open(trigger, O_RDONLY | O_NONBLOCK)) < 0)
		ERR_SYS("error open %s", trigger);
	
	if ( (flags = fcntl(pipefd2[0], F_GETFL, 0)) < 0)
		ERR_SYS("error get F_GETFL pipefd2", progname);
	if (fcntl(pipefd2[0], F_SETFL, flags | O_NONBLOCK) < 0)
		ERR_SYS("error set F_SETFL to pipefd2", progname);
	if ( (pipestream = fdopen(pipefd2[0], "r")) == NULL)
		ERR_SYS("error fdopen pipefd2", progname);
	
	fds[0].events = POLLIN;
	fds[1].fd = pipefd2[0];
	fds[1].events = POLLIN;
	fds[2].fd = signalpipe[0];
	fds[2].events = POLLIN;

	for (;;) {
		
		LOG_MSG(2, "queue=%d, total=%d, send=%d, conf=%d", IN, quelist[IN].total, quelist[IN].send, quelist[IN].conf);
		if ( (quelist[IN].send < WINSIZE) && (quelist[IN].send < quelist[IN].total) && (quelist[IN].send == quelist[IN].conf)) {
			fds[0].fd = -1;
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
	
	signal_received:
		ret = poll(fds, 3, wait);
		if (ret < 0 && errno != EINTR) { 
			ERR_SYS("error poll", progname);
		}
		else if (ret < 0) {
			goto signal_received;
		}
		else if (ret == 0) {
			int tempinode = minnode->inode;
			indq = minnode->indq;
			
			LOG_MSG(2, "timeout expired\n");
			wait = -1;
			
			if (elem_find(quelist+indq, tempinode) < 0) { // Элемент не найден, извлекаем таймер из кучи (возможно можно упростить)
				heap_extract_min(timerheap);
				LOG_MSG(2, "Inode в отложенных очередях не найден, удаляем таймер из кучи");
				continue;
			}
							
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
		else if (ret > 0) {
			LOG_MSG(2, "Один из дескрипторов готов на чтение");
			
			if (fds[0].revents & POLLIN) {
				LOG_MSG(2, "trigger changed state");
				close(trigfd);
				if (unlink(trigger) < 0)
					ERR_SYS("error delete file %s", trigger);
				if ( (mkfifo(trigger, FILEMODE) < 0) && (errno != EEXIST))
					ERR_SYS("can`t create fifo %s", trigger);
				if ( (trigfd = open(trigger, O_RDONLY | O_NONBLOCK)) < 0)
					ERR_SYS("error open %s", trigger);
				scan_drop();
			}

			if (fds[1].revents & POLLIN) {
				LOG_MSG(2, "data in the pipe are available");
				while (fscanf(pipestream, "%ld %d %[^\n]", &rinode, &code, description) != EOF)    
					check_response(rinode, code, description);
			}

			if (fds[2].revents & POLLIN) {
				if (ioctl(signalpipe[0], FIONREAD, &nsig) != 0) {
					exit(EXIT_FAILURE);
					; // Обработка ошибок
				}
				while (--nsig >= 0) {
					char c;
					int n;

					if (read(signalpipe[0], &c, 1) < 0 ) {
						exit(EXIT_FAILURE);
						; // Обработка ошибок
					}
					
					switch(c) {
						case 'H': /* sighup */
							LOG_MSG(2, "signal SIGHUP received");
							wait = -1;	
							fds[0].fd = -1;
							fds[1].fd = -1;
							goto signal_received; // Выключаем все файловые дескрипторы кроме, fd[2].fd
							break;
						case 'U': /* sigusr1 */
							
							LOG_MSG(2, "signal SIGUSR1 received");
							fds[0].fd = trigfd;
							fds[1].fd = pipefd2[0];
							if ( (n = flush_queue()) > 0) 
								LOG_MSG(2, "Successfully sent %d messages from pending queues", n);
							break;
					}
				}
			}
		} 
	}
	
	return 0;
}
