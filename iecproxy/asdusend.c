#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#define BUF_SIZE 512
#define RES_SIZE 32

int
main(int argc, char **argv)
{
	struct sockaddr_un sockun;
	int sockfd;
	char *sockpath = NULL;
	
	char buf[BUF_SIZE] = {0};
	char res[RES_SIZE] = {0};    

	int n, gopt, ret, status = 0;

	while ( (gopt = getopt(argc, argv, ":s:")) != -1) {
		switch(gopt) {
			case 's':
				sockpath = optarg;
				break;
			default:
				fprintf(stderr, "Use %s -s \"socket_path\"\n", argv[0]);
				exit(4);
				break;
		}	
	}

	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd == -1) {
		perror("Socket() error");
		exit(4);
	}
	
	memset(&sockun, 0, sizeof(struct sockaddr_un));
	sockun.sun_family = AF_UNIX;
	strncpy(sockun.sun_path, sockpath, sizeof(sockun.sun_path) - 1);

	ret = connect(sockfd, (const struct sockaddr *) &sockun, sizeof(struct sockaddr_un));
	if (ret == -1) {
		perror("Proxy-server is down");
		exit(4);
	}

	while ( (n = read(STDIN_FILENO, buf, BUF_SIZE)) > 0)  
		write(sockfd, buf, n);
	
	shutdown(sockfd, SHUT_WR);

	if (read(sockfd, res, RES_SIZE) > 0) {
		switch (*res) {
			case '<':
				status = 0;
				break;
			case '-':
				status = 4;
				break;
			default:
				break;
				
		}
	} 
	else
		status = errno;
	
	return status;
}
