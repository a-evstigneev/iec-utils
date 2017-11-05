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

char endmsg[] = {'>', '\n', '\0'}; 

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
	char *sockpath, *iecclient, *destination;
	int sockfd = -1, connfd = -1;
    
	int i, n, nready, gopt, ret;
	
	int pipefd1[2], pipefd2[2];
	int childpid;
	
	char buf[BUF_SIZE] = {0};
	
	struct pollfd fdread[FD_MAX];

	// s - path of unix socket, d - ip address of iecserver
	while ( (gopt = getopt(argc, argv, ":s:d:l:")) != -1) { 
		switch(gopt) {
			case 's':
				sockpath = optarg; 
				break;
			case 'd':
				destination = optarg;
				break;
			case 'l':
				iecclient = optarg;
				break;
			case '?':
			default:
				break;
		}
	}
	
	if (sockpath == NULL) 
		if ( (sockpath = getenv("IECSOCK")) == NULL) {
			perror("environment variable IECSOCK not defined");
			exit(EXIT_FAILURE);
		}
	if (destination == NULL) 
		if ( (destination = getenv("IECSEVER"))) {
			perror("environment variable IECSERVER not defined");
			exit(EXIT_FAILURE);
		}
	if (iecclient == NULL) 
		if ( (iecclient = getenv("IECLINK")) == NULL) {
			perror("environment variable IECLINK not defined");
			exit(EXIT_FAILURE);
		}
	
	/* delete old unix socket */
    unlink(sockpath);

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
		
		if (execl(iecclient, "iecclient", destination, NULL) < 0) 
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
			if ( (n = read(fdread[0].fd, buf, 2)) > 0) {
				switch (buf[0]) {
					case '+':
						fdread[1].fd = new_usocket(sockpath);
						fdread[1].events = POLLIN;
						break;
					case '-':
						if (connfd > 0) {
							dprintf(connfd, "-\n");
							close(connfd);
							connfd = -1;
						}
						close(fdread[1].fd);
						unlink(sockpath);
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
