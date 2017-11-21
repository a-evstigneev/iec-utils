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

#define BUF_SIZE 512
#define BACKLOG 1 
#define FD_MAX 3

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
		perror("socket() error");
		exit(EXIT_FAILURE);
	}

	memset(&sockun, 0, sizeof(struct sockaddr_un));
	sockun.sun_family = AF_UNIX;
	strncpy(sockun.sun_path, path, sizeof(sockun.sun_path) - 1);

	ret = bind(sockfd, (const struct sockaddr *) &sockun, sizeof(struct sockaddr_un)); 
	if ( ret == -1) {
		perror("bind() error");
		exit(EXIT_FAILURE);
	}

	ret = listen(sockfd, BACKLOG);
	if (ret == -1) {
        perror("listen() error");
        exit(EXIT_FAILURE);
    }
	
	return sockfd;
}

#if 0
void sig_chld(int signo) 
{
	pid_t pid;
	int stat;

	pid = wait(&stat);
	
	exit(2);
}
#endif


int
main(int argc, char *argv[])
{
	char *unixsock, *ieclink, *iecserver, *iecport;
	int sockfd = -1, connfd = -1;
    
	int i, n, nready, gopt, ret;
	
	int pipefd1[2], pipefd2[2];
	int childpid;
	
	char buf[BUF_SIZE] = {0};
	
	struct pollfd fdread[FD_MAX];

	// s - path of unix socket, d - ip address of iecserver
	while ( (gopt = getopt(argc, argv, ":u:s:p:l:")) != -1) { 
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
			case '?':
			default:
				break;
		}
	}
	
	if (unixsock == NULL) 
		if ( (unixsock = getenv("IECSOCK")) == NULL) {
			perror("environment variable IECSOCK not defined");
			exit(EXIT_FAILURE);
		}
	if (iecserver == NULL) 
		if ( (iecserver = getenv("IECSERVER"))) {
			perror("environment variable IECSERVER not defined");
			exit(EXIT_FAILURE);
		}
	if (iecport == NULL) 
		if ( (iecport = getenv("IECPORT"))) {
			perror("environment variable IECPORT not defined");
			exit(EXIT_FAILURE);
		}
	if (ieclink == NULL) 
		if ( (ieclink = getenv("IECLINK")) == NULL) {
			perror("environment variable IECLINK not defined");
			exit(EXIT_FAILURE);
		}
	
	/* delete old unix socket */
    unlink(unixsock);

	/* start iecclient */
	if ( (pipe(pipefd1)) < 0) {
		perror("can`t create pipefd1");
		exit(EXIT_FAILURE);
	}
	if ( (pipe(pipefd2)) < 0) {
		perror("can`t create pipefd2");
		exit(EXIT_FAILURE);
	}
	if ( (childpid = fork()) < 0) {
		perror("fork() error");
		exit(EXIT_FAILURE);
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
		fprintf(stderr, "%s %s\n", iecserver, iecport);		
		if (execl(ieclink, "ieclink", iecserver, iecport, NULL) < 0) 
			perror("exec() error"); 
		exit(EXIT_FAILURE);
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
				switch (buf[0]) {
					case '+':
						fdread[1].fd = new_usocket(unixsock);
						fdread[1].events = POLLIN;
						break;
					case '-':
						if (connfd > 0) {
							dprintf(connfd, "-\n");
							close(connfd);
							connfd = -1;
						}
						close(fdread[1].fd);
						unlink(unixsock);
						fdread[1].fd = -1;
						break;
					case '<':
						dprintf(connfd, "<\n");
						close(connfd);
						connfd = -1;
						break;
					default:
						;
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
			if (fdread[2].fd < 0)
				continue;
			
			while ( (n = read(fdread[2].fd, buf, BUF_SIZE)) > 0) { 
				write(pipefd1[1], buf, n);
			}
			
			if (n < 0) {
				if (errno == ECONNRESET) {
					close(fdread[2].fd);
					fdread[2].fd = -1;
				} 
				else
					perror("read() error");
			}				
			dprintf(pipefd1[1], ">\n");
			fdread[2].fd = -1;	
		}
	}

	return 0;	
}
