#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "logging.h"

#define BUF_SIZE 512
#define BACKLOG 1 
#define FD_MAX 3
#define END_INIT	 "46010400000000000000" 
#define ACT_INTERROG "64010600FFFF00000014"
#define CON_INTERROG "64010700FFFF00000014"
#define END_INTERROG "64010A00FFFF00000014"

#ifdef DEBON
	#define LOG_MSG(macrolevel, ...) \
	debuglevel >= macrolevel ? logmsg(__FILE__, __FUNCTION__, __LINE__, 0, LOG_INFO, __VA_ARGS__) : 0;
#else
	#define LOG_MSG(macrolevel, ...) 
#endif

extern char *progname;
extern FILE *logstream;
extern int daemon_proc;

pid_t childpid;
int debuglevel;

ssize_t
readline(int fd, void *buf, size_t maxlen)
{
	ssize_t n, rc;
	char c, *ptr;

	ptr = buf;
	for (n = 1; n < maxlen; n++) {
		again:
			if ( (rc = read(fd, &c , 1)) == 1) {
				*ptr++ = c;
				if (c == '\n')
					break;
			}
			else if (rc == 0) {
				if (n == 1)
					return 0;
				else
					break;
			}
			else {
				if (errno == EINTR)
					goto again;
				return -1;
			}
	}

	*ptr = 0;
	return n;
}
		
int 
new_usocket(const char *path)
{
	int sockfd, ret;
	struct sockaddr_un sockun;
	
	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd == -1) {	
		ERR_SYS("socket() error");
	}

	memset(&sockun, 0, sizeof(struct sockaddr_un));
	sockun.sun_family = AF_UNIX;
	strncpy(sockun.sun_path, path, sizeof(sockun.sun_path) - 1);

	ret = bind(sockfd, (const struct sockaddr *) &sockun, sizeof(struct sockaddr_un)); 
	if ( ret == -1) {
		ERR_SYS("bind() error");
	}

	ret = listen(sockfd, BACKLOG);
	if (ret == -1) {
		ERR_SYS("listen() error");
    }
	
	return sockfd;
}

int
ginterrog(int fdw, const char *script_path)
{
	FILE *fp;
	char asdu[BUF_SIZE] = {0};

	fp = popen(script_path, "r");
	while (fgets(asdu, BUF_SIZE, fp) != NULL) {
		dprintf(fdw, "%s", asdu);
	}
	pclose(fp);
	
	return 0;
}

void sig_term(int signum)
{
	if (childpid > 0)  
		kill(childpid, SIGTERM);
	if (childpid == 0)
		exit(0);
}

void sig_chld(int signum)
{
    pid_t pid;
	int stat;

	pid = waitpid(-1, &stat, WNOHANG);
	
	if (pid == childpid) {
		childpid = 0;
		raise(SIGTERM);
	}
}

int
connect_hook(char *pid)
{
	int n = -1;
	char command[BUF_SIZE] = {0};

	if (pid != NULL) {
		snprintf(command, BUF_SIZE, "%s %s", "connect_hook", pid);
		n = system(command);
	}

	return n;
}

int
disconnect_hook(char *pid)
{
	int n = -1;
	char command[BUF_SIZE] = {0};
	
	if (pid != NULL) {
		snprintf(command, BUF_SIZE, "%s %s", "disconnect_hook", pid);
		n = system(command);
	}

	return n;
}

int
main(int argc, char *argv[])
{
	char *unixsock, *ieclink, *iecserver, *iecport, *gi_script;
	int sockfd = -1, connfd = -1;
    
	char *pid_notify = NULL;

	int i, n, nready, gopt, ret, first_connect = 0, new_sockfd = -1;
		
	int pipefd1[2], pipefd2[2];
	
	char buf[BUF_SIZE] = {0};
	
	struct pollfd fdread[FD_MAX];
	
	// u - path to unix socket, s - ip address of iecserver, p - port of iecserver
	// l - ieclink path, g - general interrogation script, d - debug level
	while ( (gopt = getopt(argc, argv, ":u:s:p:l:g:d:")) != -1) { 
		switch(gopt) {
			case 'u':
				unixsock = optarg; 
				break;
			case 's':
				iecserver = optarg;
				break;
			case 'p':
				iecport = optarg;
				break;
			case 'l':
				ieclink = optarg;
				break;
			case 'g':
				gi_script = optarg;
				break;
			case 'd':
				debuglevel = atoi(optarg);
				break;
			case '?':
			default:
				break;
		}
	}
	
	progname = argv[0];
	logstream = stderr;
	daemon_proc = 1;


	signal(SIGTERM, sig_term);
	signal(SIGCHLD, sig_chld);
	
	if ( (pid_notify = getenv("PID_NOTIFY")) == NULL)
		LOG_MSG(2, "variable PID_NOTIFY not defined");
	
	/* delete old unix socket */
    unlink(unixsock);

	/* start iecclient */
	if ( (pipe(pipefd1)) < 0) {
		ERR_SYS("pipe() fd1 error");
	}
	if ( (pipe(pipefd2)) < 0) {
		ERR_SYS("pipe() fd2 error");
	}
	if ( (childpid = fork()) < 0) {
		ERR_SYS("fork() error");
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

		if (execlp(ieclink, ieclink, iecserver, iecport, NULL) < 0) 
			ERR_SYS("exec() error"); 
	}
	
	close(pipefd1[0]);
	close(pipefd2[1]);

	fdread[0].fd = pipefd2[0];
	fdread[0].events = POLLIN;

	for (i = 1; i < FD_MAX; ++i)
		fdread[i].fd = -1;

/*	signal(SIGCHLD, sig_chld); */

	for (;;) {
		nready = poll(fdread, FD_MAX, -1); 
		
		if (fdread[0].revents & POLLIN) {
			if ( (n = readline(fdread[0].fd, buf, BUF_SIZE)) > 0) {
				
				if (strncmp(END_INIT, buf, 8) == 0) {
					LOG_MSG(2, "received M_EI_NA_1 (cot=4) %s from ieclink", END_INIT);
				}
				else if (strncmp(ACT_INTERROG, buf, n-1) == 0) {
					LOG_MSG(2, "received C_IC_NA_1 (cot=6) %s from ieclink", ACT_INTERROG);
					
					if (first_connect) {
						dprintf(pipefd1[1], "%s\n", CON_INTERROG);
						LOG_MSG(2, "send CON_INTERROG to ieclink, confirmation of activation general interrogation"); // Заменить END_INTERROG на имя ASDU
						fdread[1].fd = new_sockfd;
						fdread[1].events = POLLIN; // Ждем сообщения от менеджера очередей, 
						// или служебного сообщения, что отложенных сообщений нет
						first_connect = 0;		
					}
					else {
						dprintf(pipefd1[1], "%s\n", CON_INTERROG);
						LOG_MSG(2, "send CON_INTERROG to ieclink, confirmation of activation general interrogation"); // Заменить END_INTERROG на имя ASDU
						ginterrog(pipefd1[1], gi_script);
						dprintf(pipefd1[1], "%s\n", END_INTERROG);
						LOG_MSG(2, "send END_INTERROG to ieclink, general interrogation completed"); // Заменить END_INTERROG на имя ASDU
						dprintf(pipefd1[1], ">\n");
						LOG_MSG(2, "send \">\" to ieclink, transfer completed");
					}
				}
				else { 
					switch (buf[0]) {
						case '+':
							LOG_MSG(2, "received \"+\", connection established");
							//fdread[1].fd = new_usocket(unixsock); // Может временной переменной
							new_sockfd = new_usocket(unixsock); // Может временной переменной
							// присвоить значение дескриптора?
						//	fdread[1].events = POLLIN; // Ждем пока придет активация общего опроса
							connect_hook(pid_notify);
							first_connect = 1; // Соединение было установлено только что
							break;
						case '-':
							LOG_MSG(2, "received \"-\" from ieclink, connection to CTS lost");
							if (connfd > 0) {
								dprintf(connfd, "-\n");
								close(connfd);
								connfd = -1;
							}
							close(fdread[1].fd);
							unlink(unixsock);
							fdread[1].fd = -1;
							disconnect_hook(pid_notify);
							break;
						case '<':
							LOG_MSG(2, "received \"<\" from ieclink, ASDU confirmed");
							if (connfd < 0)
								break;
							dprintf(connfd, "<\n");
							LOG_MSG(2, "send \"<\" to asdusend, ASDU confirmed");
							close(connfd);
							connfd = -1;
							break;
						default:
							break;
					}
				}
			}
			if (--nready <= 0)
				continue;
		}
		
		if (fdread[1].revents & POLLIN) {
			connfd = accept(fdread[1].fd, NULL, NULL);
			
			fdread[2].fd = connfd;
			fdread[2].events = POLLIN;
			
			if (--nready <= 0)
				continue;
		}
		
		if (fdread[2].revents & POLLIN) {
			if (fdread[2].fd < 0) // Какая-то хрень
				continue;
			
			while ( (n = read(fdread[2].fd, buf, BUF_SIZE)) > 0) { 
				if (buf[0] == '^') {
					LOG_MSG(2, "receive from quemngr \"^\", there are no more deferred messages");
					
					ginterrog(pipefd1[1], gi_script);
					dprintf(pipefd1[1], "%s\n", END_INTERROG);
					
					LOG_MSG(2, "send END_INTERROG to ieclink, general interrogation completed"); // Заменить END_INTERROG на имя ASDU
					dprintf(fdread[2].fd, "^\n");
					close(fdread[2].fd);
					connfd = -1;
					// Надо сделать break, иначе ошибка read bad file descriptor
				}
				else
					write(pipefd1[1], buf, n);
			}
			
			LOG_MSG(2, "read n = %d\n", n);
		//	fprintf(stderr, "iecproxy: read n = %d\n", n);
			
			if (n < 0) {
				if (errno == ECONNRESET) {
					close(fdread[2].fd);
					fdread[2].fd = -1;
				} 
				else
					perror("iecproxy: read() error");
			}				
			
			dprintf(pipefd1[1], ">\n");
			LOG_MSG(2, "send \">\" to ieclink, transfer completed");
			fdread[2].fd = -1;	
		}
	}

	return 0;	
}
