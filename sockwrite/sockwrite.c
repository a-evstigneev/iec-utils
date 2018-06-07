#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#define BUF_SIZE 512
#define RESPONSE_SIZE 32

int
main(int argc, char **argv)
{
	struct sockaddr_un sockun;
	int sockfd;
	char *sockpath = NULL;
	
	char buf[BUF_SIZE] = {0};
	char res[RESPONSE_SIZE] = {0};    
	
	ssize_t n, m;
	int gopt, ret, status = 0;

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
		perror("socket() error");
		status = errno;
		goto exit_point;
	}
	
	memset(&sockun, 0, sizeof(struct sockaddr_un));
	sockun.sun_family = AF_UNIX;
	strncpy(sockun.sun_path, sockpath, sizeof(sockun.sun_path) - 1);

	ret = connect(sockfd, (const struct sockaddr *) &sockun, sizeof(struct sockaddr_un));
	if (ret == -1) {
		perror("connect() error");
		status = errno;
		goto exit_point;	
	} 
	
	while ( (n = read(STDIN_FILENO, buf, BUF_SIZE)) > 0) {
		if ( (m = write(sockfd, buf, n)) < 0) {
			perror("write() error");
			status = errno;
			goto exit_point;	
		}
	}
	
	shutdown(sockfd, SHUT_WR);

	if ( (n = read(sockfd, res, RESPONSE_SIZE)) > 0) {
		switch (*res) {
			case '<':
				status = 0;
				break;
			case '-':
				status = 2;
				break;
			case '^':
				status = 3; // Стоит ли ожидать ответа об отправке отложенных сообщений?
				break;
			default:
				break;
		}
	} 
	
	if (n < 0)
		status = errno;
	
	exit_point:
		return status;
}
