#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <poll.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <string.h>
#include <arpa/inet.h>
#include <signal.h>

#define MAXBUF 2048
#define TIMEOUT 2500 
#define SIZERINGBUF 50

struct ringbuf {
	char *msg[SIZERINGBUF];
	short head;
	short tail;
	short size;
};

struct ringbuf store;

char *usock, *servaddr, *port;
int ufd, sfd;

int
pushmsg(struct ringbuf *st, const char *msg)
{
	short next = st->head + 1;
	if (next >= SIZERINGBUF)
		next = 0;
	
	if (next == st->tail) {
		free(st->msg[st->tail]);
		st->tail++;
		--(st->size);
	}
	
	st->msg[st->head] = strdup(msg);
	st->head = next;
	++(st->size);

	return 0;
}

char *
readtail(struct ringbuf *st)
{
	if (st->head == st->tail)
		return NULL;

	return st->msg[st->tail];
}

int
deltail(struct ringbuf *st)
{
	short next;
	char *msg;

	if (st->head == st->tail)
		return -1;

	next = st->tail + 1;
	if (next >= SIZERINGBUF)
		next = 0;
	
	free(st->msg[st->tail]);
	st->tail = next;
	--(st->size);	
	
	return 0;	
}

int 
connectnb(int sockfd, const struct sockaddr *addr, socklen_t addrlen, int timeout)
{
	int flags, n, error, ret;
	socklen_t len;
	struct pollfd fds[1];
		
	flags = fcntl(sockfd, F_GETFL);
	fcntl(sockfd, F_SETFL, O_NONBLOCK | flags);
	
	error = 0;	
	if ( (n = connect(sockfd, (const struct sockaddr *) addr, addrlen)) < 0)
		if (errno != EINPROGRESS)
			return -1;
	
	if (n == 0)
		goto done;

	fds[0].fd = sockfd;
	fds[0].events = POLLIN | POLLOUT;
	
	if ( (ret = poll(fds, 1, timeout)) == 0) {
		errno = ETIMEDOUT;
		return -1;
	}

	if (ret < 0 && errno != EINTR) {
		return -1;
	}
	else {
		if ( (fds[0].revents & POLLIN) || (fds[0].revents & POLLOUT)) {
			len = sizeof(error);
			if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) 
				return -1;
		}
	}
	done:
		fcntl(sockfd, F_SETFL, flags);
	
	return 0;
}

int
main(int argc, char **argv)
{
	int gopt;
	char buf[MAXBUF] = {0};
	int logrfd, logwfd, ret;
	FILE *logstream;
	char *fifolog;
	char *message;
	struct pollfd fds[1];
	struct sockaddr_un locallog;
	struct sockaddr_in remotelog;
	int flags, rt, n, error, len;
	
	while ( (gopt = getopt(argc, argv, ":u:n:p:")) != -1) {
		switch(gopt) {
		case 'u':
			usock = optarg;
			break;
		case 'n':
			servaddr = optarg;
			break;
		case 'p':
			port = optarg;
			break;
		case '?':
		default:
			break;
		}
	}
	
	if ( (fifolog = getenv("FIFOLOG")) == NULL)
		exit(EXIT_FAILURE);
	if ( (logrfd = open(fifolog, O_RDONLY)) < 0)
		exit(EXIT_FAILURE);
	if ( (logwfd = open(fifolog, O_WRONLY)) < 0)
		exit(EXIT_FAILURE);
	if ( (logstream = fdopen(logrfd, "r")) == NULL)
		exit(EXIT_FAILURE);

#if 0	
	if (usock) {
		memset(&locallog, 0, sizeof(struct sockaddr_un));
		locallog.sun_family = AF_UNIX;
		strncpy(locallog.sun_path, usock, sizeof(locallog.sun_path) - 1);
		ufd = socket(AF_LOCAL, SOCK_DGRAM, 0);
		connect(ufd, (const struct sockaddr *) &locallog, sizeof(struct sockaddr_un));
	}
#endif	
	
	if (servaddr && port) {
		memset(&remotelog, 0, sizeof(struct sockaddr_in));
		remotelog.sin_family = AF_INET;
		remotelog.sin_port = htons(atoi(port));
		inet_pton(AF_INET, servaddr, &remotelog.sin_addr);
	}

	fds[0].fd = logrfd;
	fds[0].events = POLLIN;

	signal(SIGPIPE, SIG_IGN);

	for (;;) {
		ret = poll(fds, 1, -1);
		
		if (ret < 0 && errno != EINTR) 
			;
		else {
			if (fds[0].revents & POLLIN) {
				while (fgets(buf, MAXBUF, logstream) != NULL) {
					sfd = socket(AF_INET, SOCK_STREAM, 0);
					if (connectnb(sfd, (const struct sockaddr *) &remotelog, sizeof(struct sockaddr_in), TIMEOUT) < 0) {
						pushmsg(&store, buf);
						close(sfd);
						continue;
					}
					
					while (store.size > 0) {
						message = readtail(&store);
						if (write(sfd, message, strlen(message)) < 0) 
							break;
						else
							deltail(&store);
					}
					
					if (write(sfd, buf, strlen(buf)) < 0) 
						pushmsg(&store, buf);
										
					close(sfd);
				}	
			}
		}
	}
	
	return 0;
}
