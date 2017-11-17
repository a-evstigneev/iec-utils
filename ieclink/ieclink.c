/*
 * Copyright (C) 2005, Grigoriy Sitkarev                                 
 * sitkarev@komi.tgk-9.ru                                                
 *                                                                       
 * This program is free software; you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation; either version 2 of the License, or     
 * (at your option) any later version.                                   
 *                                                                       
 * This program is distributed in the hope that it will be useful,       
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         
 * GNU General Public License for more details.                          
 *                                                                       
 * You should have received a copy of the GNU General Public License     
 * along with this program; if not, write to the                         
 * Free Software Foundation, Inc.,                                       
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <event.h>
#include <sys/queue.h>
#include <ctype.h>

#include "iecsock.h"
#include "iecsock_internal.h"
#include "xalloc.h"

#define BACKLOG	10

struct iechooks default_hooks;

struct asduhead {
	struct asduhead *next;
	short len;
};

static unsigned char asdubuf[IEC104_ASDU_MAX];
static int asdulen;
static int limlistsize = 12; // Предельный Размер списка asdulist, при превышении которого перестаем читать с stdin 
static unsigned char rbuf[1024];
static unsigned int rbuflen;
static struct asduhead *asdulist = NULL, **asdulistlast = &asdulist;
static int curlistsize = 0; // Текущий размер списка asdulist

int endmsg = 0; // Пришел весь пакет asdu со стандартного ввода (получен символ ">")

static void iec_asdu_send(struct iecsock *s, unsigned char *asdubuf, const int len)
{
	struct iec_buf *b;
	static int i = 0;
	b = calloc(1, sizeof(struct iec_buf) + len);
	if (!b)
		return;
	b->data_len = len;
	memcpy(b->data, asdubuf, len);
	iecsock_prepare_iframe(b);
	TAILQ_INSERT_TAIL(&s->write_q, b, head);
	iecsock_run_write_queue(s);
}

static void stdin_read(int fd, short events, void *opaque)
{
	struct iecsock *s = (struct iecsock *) opaque;
	char c, *cp, *end, *pbuf;
	int i, v, n;
	int left;

	while ( (n = read(fd, rbuf + rbuflen, sizeof(rbuf) - rbuflen)) > 0) {
		rbuflen += n;
		pbuf = rbuf;
		left = rbuflen;
		
		while (pbuf < rbuf + rbuflen) {
			if ( (end = memchr(pbuf, '\n', left)) == NULL) {
				break;
			}
			
			cp = pbuf;

			while (cp + 1 < end) {
				v = 0;
				for (i = 0; i < 2; i++) {
					c = tolower(*cp);
					if (c >= '0' && c <= '9')
						v = v * 16 + c - '0';
					else if (c >= 'a' && c <= 'f')
						v = v * 16 + 10 + c - 'a';
					else {
						goto discard;
					}
					cp++;
				}
				if (asdulen < sizeof(asdubuf))
					asdubuf[asdulen++] = v;
			}
			discard:
				if (cp == end && asdulen != 0) {
					if (iecsock_can_queue(s)) {
						iec_asdu_send(s, asdubuf, asdulen);
					}
					else {
						fprintf(stderr, "%s [%d]: window size exhausted, save ASDU in asdulist\n", __FUNCTION__, getpid());
						struct asduhead *h;
						h = xmalloc(sizeof(*h) + asdulen);
						h->next = NULL;
						h->len = asdulen;
						memcpy((unsigned char *)h + sizeof(*h), asdubuf, asdulen);
						*asdulistlast = h;
						asdulistlast = &h->next;
						++curlistsize;
					}
				}
			
			if (*pbuf == '>') {
				rbuflen = 0;
				endmsg = 1;
				fprintf(stderr, "%s [%d]: end of message \">\" reached\n", __FUNCTION__, getpid());	
				return;	
			}
			
			asdulen = 0;
			left -= end - pbuf + 1; 
			pbuf = end + 1;
		}
		
		if (!left) {
			memset(rbuf, 0, 1024);
			rbuflen = left;
		}
		
		if (left && pbuf != rbuf) {
			memmove(rbuf, pbuf, left);
			rbuflen = left;		
		}
		
		if (rbuflen == sizeof(rbuf))
			rbuflen = 0;
		
		if (curlistsize == limlistsize) {
			fprintf(stderr, "%s [%d]: limit size of asdulist reached, disable reading from stdin\n", __FUNCTION__, getpid());
			event_del(&s->user);
			return;
		}
	}
}

static void iec_kick_callback(struct iecsock *s)
{
	fprintf(stderr, "%s [%d]: Sucess 0x%lu\n", __FUNCTION__, getpid(), (unsigned long) s);
	struct asduhead *p = NULL, *next = NULL;
	
	for (p = asdulist; p != NULL; p = next) {
		if (iecsock_can_queue(s)) {
			iec_asdu_send(s, (unsigned char *)p + sizeof(*p), p->len);
			next = p->next;
			xfree(p);
		}
		else
			break;
	}
	asdulist = p;
	
	if ( (p == NULL) && (endmsg == 0)) {
		asdulistlast = &asdulist;
		event_add(&s->user, NULL);
		fprintf(stderr, "%s [%d]: asdulist is empty, set event_add(&s->user, NULL)\n", __FUNCTION__, getpid());
	}
	else if ( (p == NULL) && (endmsg == 1)) {
		fprintf(stdout, "<\n");	
		fflush(stdout);
		event_add(&s->user, NULL);
		endmsg = 0;
	}
	
	return;
}

void disconnect_hook(struct iecsock *s, short reason)
{	
	fprintf(stderr, "%s [%d]: what=0x%02x\n", __FUNCTION__, getpid(), reason);

	fprintf(stdout, "-\n"); 
	fflush(stdout);
	
	return;
}

void data_received_hook(struct iecsock *s, struct iec_buf *b)
{	
	int i;

	fprintf(stderr, "%s [%d]: data_len=%d Success\n", __FUNCTION__, getpid(), b->data_len);
	
	for (i = 0; i < b->data_len; ++i) {
		fprintf(stderr, "%d ", b->data[i]);
		//fprintf(stderr, "%02X", b->data[i]);
	
	}
	fprintf(stderr, "\n");
	fflush(stderr);
	free(b);

	return;
}

void activation_hook(struct iecsock *s)
{
	struct timeval tv;
	u_long flags;
	int ret;

	fprintf(stderr, "%s [%d]: Sucess 0x%lu\n", __FUNCTION__, getpid(), (unsigned long) s);
	
	event_set(&s->user, 0, EV_READ, stdin_read, s);
	event_add(&s->user, NULL);
	
	fprintf(stdout, "+\n");	
	fflush(stdout);
	
	return;
}

void connect_hook(struct iecsock *s)
{	
	fprintf(stderr, "%s [%d]: Sucess 0x%lu\n", __FUNCTION__, getpid(), (unsigned long) s);
}

int main(int argc, char **argv)
{
	struct sockaddr_in addr;
	
	event_init();
	
	default_hooks.disconnect_indication = disconnect_hook;
	default_hooks.connect_indication = connect_hook;
	default_hooks.data_indication = data_received_hook;
	default_hooks.activation_indication = activation_hook;
	default_hooks.transmit_wakeup = iec_kick_callback;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(IEC_PORT_DEFAULT);
	
	if (argc > 1 && inet_pton(AF_INET, argv[1], &addr.sin_addr) > 0) 
		iecsock_connect(&addr);
	else
		iecsock_connect(NULL);
	
	event_dispatch();
	
	return EXIT_SUCCESS;
}
