#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include "logging.h"

#define BUF_SIZE 512
#define RESPONSE_SIZE 32

#ifdef DEBON
	#define LOG_MSG(macrolevel, ...) \
	debuglevel >= macrolevel ? logmsg(__FILE__, __FUNCTION__, __LINE__, 0, LOG_INFO, __VA_ARGS__) : 0;
#else
	#define LOG_MSG(macrolevel, ...) 
#endif

extern char *progname;
extern FILE *logstream;
extern int daemon_proc;

int debuglevel;

int
main(int argc, char **argv)
{
	struct sockaddr_un sockun;
	int sockfd;
	char *sockpath = NULL;
	
	char buf[BUF_SIZE] = {0};
	char response[RESPONSE_SIZE] = {0};    
	
	ssize_t n, m;
	int gopt, ret, status = 0;

	while ( (gopt = getopt(argc, argv, ":d:s:")) != -1) {
		switch(gopt) {
			case 's':
				sockpath = optarg;
				break;
			case 'd':
				debuglevel = atoi(optarg);
				break;
			default:
				fprintf(stderr, "Use %s -s \"socket_path\"\n", argv[0]);
				exit(4);
				break;
		}	
	}

	progname = argv[0];
	logstream = stderr;
	daemon_proc = 1;
	
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

	if ( (n = read(sockfd, response, RESPONSE_SIZE)) > 0) {
		switch (*response) {
			case '<':
				status = 0;
				break;
			case '-':
				status = 2;
				break;
			case '^':
				status = 3;
				break;
			default:
				break;
		}
	} 
	
	if (n < 0)
		status = errno;
	
	exit_point:
//		LOG_MSG(2, "sockwrite read %c, exit code = %d", *response, status);
		return status;
}
