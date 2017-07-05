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

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <arpa/inet.h>
#include <event.h>

#include "iecsock.h"
#include "iecsock_internal.h"

#include "iec104.h"

#define MODNAME "iecsock"
#define debug(format, arg...)						\
fprintf(stderr, "%s: " format ": %s\n", MODNAME, ##arg, strerror(errno))
#define proto_debug(format, arg...)					\
fprintf(stderr, "%s: " format "\n", MODNAME, ##arg)

void bufferevent_setwatermark(struct bufferevent *bufev, short events, size_t lowmark, size_t highmark);

static void iecsock_uframe_send(struct iecsock *s, enum uframe_func func)
{
	struct iechdr h;
	
	memset(&h, 0, sizeof(struct iechdr));
	
/*	proto_debug("TX U %s stopdt:%i", uframe_func_to_string(func), s->stopdt);*/
	
	h.start = 0x68;
	h.length = sizeof(struct iec_u);
	h.uc.ft = 3;
	if (func == STARTACT)
		h.uc.start_act = 1;
	else if (func == STARTCON)
		h.uc.start_con = 1;
	else if (func == STOPACT)
		h.uc.stop_act = 1;
	else if (func == STOPCON)
		h.uc.stop_con = 1;
	else if (func == TESTACT)
		h.uc.test_act = 1;
	else if (func == TESTCON)
		h.uc.test_con = 1;
	
	bufferevent_write(s->io, &h, sizeof(h));	
	s->xmit_cnt++;
}

static void iecsock_sframe_send(struct iecsock *s)
{
	struct iechdr h;
	
	memset(&h, 0, sizeof(h));
	
/*	proto_debug("TX S N(r)=%i", s->vr); */
	
	h.start = 0x68;
	h.length = sizeof(struct iec_s);
	h.sc.ft = 1;
	h.sc.nr = s->vr;	
	bufferevent_write(s->io, &h, sizeof(h));
	s->xmit_cnt++;
}

void iecsock_prepare_iframe(struct iec_buf *buf)
{
	struct iechdr *h;
	
	h = &buf->h;
	h->start = 0x68;
	h->length = buf->data_len + sizeof(struct iec_i);
	h->ic.ft = 0;
}

static void t1_timer_run(int nofd, short what, void *arg)
{
	struct iecsock *s = (struct iecsock *) arg;
	s->io->errorcb(s->io, what, s);
}

static void t1_timer_start(struct iecsock *s)
{
	struct timeval tv;
	tv.tv_usec = 0;
	tv.tv_sec = s->t1;
	evtimer_add(&s->t1_timer, &tv);
}

static void t2_timer_run(int nofd, short what, void *arg)
{
	struct iecsock *s = (struct iecsock *) arg;
	iecsock_sframe_send(s);
	s->va_peer = s->vr;
}

static void t2_timer_start(struct iecsock *s)
{
	struct timeval tv;
	tv.tv_usec = 0;
	tv.tv_sec = s->t2;
	evtimer_add(&s->t2_timer, &tv);
}

static void t3_timer_start(struct iecsock *s)
{
	struct timeval tv;
	tv.tv_usec = 0;
	tv.tv_sec = s->t3;
	evtimer_add(&s->t3_timer, &tv);
}

static void t3_timer_run(int nofd, short what, void *arg)
{
	struct iecsock *s = (struct iecsock *) arg;
	t1_timer_start(s);
	s->testfr = 1;
	iecsock_uframe_send(s, TESTACT);
}

static inline void flush_queue(struct iec_buf_queue *q)
{
	struct iec_buf *b;
	
	while (!(TAILQ_EMPTY(q))) {
		b = TAILQ_FIRST(q);
		TAILQ_REMOVE(q, b, head);
		free(b);
	}
}

static inline void iecsock_flush_queues(struct iecsock *s)
{
	flush_queue(&s->write_q);
	flush_queue(&s->ackw_q);
}

void iecsock_run_write_queue(struct iecsock *s)
{
	struct iechdr *h;
	struct iec_buf *b;
	
	if (s->type == IEC_SLAVE && s->stopdt)
		return;
	
	while ( !(TAILQ_EMPTY(&s->write_q)) && (s->vs != (s->va + s->k) % 32767) ) {

		b = TAILQ_FIRST(&s->write_q);
		TAILQ_REMOVE(&s->write_q, b, head);
		h = &b->h;
		h->ic.nr = s->vr;
		h->ic.ns = s->vs;
		
/*		proto_debug("TX I V(s)=%d V(a)=%d N(s)=%d N(r)=%d", s->vs, s->va, 
					h->ic.ns, h->ic.nr);*/
		if (t1_timer_pending(s))
			t1_timer_stop(s);
		t1_timer_start(s);
		
		bufferevent_write(s->io, h, h->length + 2);
		TAILQ_INSERT_TAIL(&s->ackw_q, b, head);
		
		s->vs = (s->vs + 1) % 32767;
		s->xmit_cnt++;
	}
	
/*	if (s->vs == (s->va + s->k) % 32767)
		proto_debug("reached k, no frames will be sent"); */
}

static void iecsock_run_ackw_queue(struct iecsock *s, unsigned short nr)
{
	struct iec_buf *b;
	
	/* if (s->type == IEC_SLAVE) proto_debug("received ack for N(s)=%d-1", nr); */
	
	while (!(TAILQ_EMPTY(&s->ackw_q))) {
		b = TAILQ_FIRST(&s->ackw_q);
		if (b->h.ic.ns == nr) break;
		TAILQ_REMOVE(&s->ackw_q, b, head);
		free(b);
	}
}

static inline int check_nr(struct iecsock *s, unsigned short nr)
{
	return ((nr - s->va + 32767) % 32767 <= (s->vs - s->va + 32767) % 32767);
}

static inline int check_ns(struct iecsock *s, unsigned short ns)
{
	return (ns == s->vr);
}

static int iecsock_iframe_recv(struct iecsock *s, struct iec_buf *buf)
{
	struct iechdr *h;
	h = &buf->h;
	buf->data_len = h->length - 2;
	
	if (!check_nr(s, h->ic.nr))
		return -1;
	
/*	iecsock_run_ackw_queue(s, h->ic.nr); */
	s->va = h->ic.nr;
	if (s->va == s->vs) {
		t1_timer_stop(s);
		if (s->hooks.transmit_wakeup)
			s->hooks.transmit_wakeup(s);
		else if (default_hooks.transmit_wakeup)
			default_hooks.transmit_wakeup(s);
	}
	t2_timer_stop(s);
	t2_timer_start(s);
	
	if (!check_ns(s, h->ic.ns))
		return -1;

	s->vr = (s->vr + 1) % 32767;
	if ((s->vr - s->va_peer + 32767) % 32767 == s->w) {
		iecsock_sframe_send(s);
		s->va_peer = s->vr;
	}

	if (s->hooks.data_indication)
		s->hooks.data_indication(s, buf);
	else if (default_hooks.data_indication)
		default_hooks.data_indication(s, buf);
	else free(buf);

	return 0;
}

static int iecsock_sframe_recv(struct iecsock *s, struct iec_buf *buf)
{
	struct iechdr *h;
	h = &buf->h;
	buf->data_len = h->length - 2;
	
	if (!check_nr(s, h->ic.nr))
		return -1;
	
	iecsock_run_ackw_queue(s, h->ic.nr);
	s->va = h->ic.nr;
	if (s->va == s->vs) {
		t1_timer_stop(s);
		if (s->hooks.transmit_wakeup)
			s->hooks.transmit_wakeup(s);
		else if (default_hooks.transmit_wakeup)
			default_hooks.transmit_wakeup(s);
	}
	
	return 0;
}

static int iecsock_uframe_recv(struct iecsock *s, struct iec_buf *buf)
{
	struct iechdr *h;
	h = &buf->h;

	switch(uframe_func(h)) {
	case STARTACT:
		if (s->type != IEC_SLAVE)
			return -1;
/*		proto_debug("STARTACT changed stopdt to 0"); */
		s->stopdt = 0;
		iecsock_uframe_send(s, STARTCON);
		iecsock_run_write_queue(s);
		if (s->hooks.activation_indication)
			s->hooks.activation_indication(s);
		else if (default_hooks.activation_indication)
			default_hooks.activation_indication(s);
	break;
	case STARTCON:
		if (s->type != IEC_MASTER)
			return -1;
		t1_timer_stop(s);
/*		proto_debug("STARTCON changed stopdt to 0");*/
		s->stopdt = 0;
		if (s->hooks.activation_indication)
			s->hooks.activation_indication(s);
		else if (default_hooks.activation_indication)
			default_hooks.activation_indication(s);
	break;
	case STOPACT:
		if (s->type != IEC_SLAVE)
			return -1;
		s->stopdt = 1;
		iecsock_uframe_send(s, STOPCON);
		if (s->hooks.deactivation_indication)
			s->hooks.deactivation_indication(s);
		else if (default_hooks.deactivation_indication)
			default_hooks.deactivation_indication(s);
	break;
	case STOPCON:
		if (s->type != IEC_MASTER)
			return -1;
		s->stopdt = 1;
		if (s->hooks.deactivation_indication)
			s->hooks.deactivation_indication(s);
		else if (default_hooks.deactivation_indication)
			default_hooks.deactivation_indication(s);
	break;
	case TESTACT:
		iecsock_uframe_send(s, TESTCON);
		/* SLAVE must not send TESTFR while recieving them from MASTER */
		if (s->type == IEC_SLAVE && !s->testfr)
			t3_timer_stop(s);
	break;
	case TESTCON:
		if (!s->testfr)
			return -1;
		t1_timer_stop(s);
		s->testfr = 0;
	break;
	}

	return 0;
}

static int iecsock_frame_recv(struct iecsock *s)
{
	int ret = 0;
	struct iechdr *h;
	struct iec_buf *buf;
	
	h = (struct iechdr *) &s->buf[0];
	buf = calloc(1, s->len + sizeof(struct iec_buf) - sizeof(struct iechdr));
	if (!buf)
		return -1;
	
	t3_timer_stop(s);
	t3_timer_start(s);
	
	memcpy(&buf->h, h, h->length + 2);
	
	switch (frame_type(h)) {
	case FRAME_TYPE_I:
		if (s->type == IEC_SLAVE && s->stopdt) {
/*			proto_debug("RX I monitor direction not active");*/
			free(buf);
			break;
		}
/*		proto_debug("RX I len=%d V(r)=%d V(s)=%d V(a)=%d V(a_peer)=%d " 
		"N(r)=%d N(s)=%d", s->len, s->vr, s->vs, s->va, s->va_peer, 
		h->ic.nr, h->ic.ns);*/
		ret = iecsock_iframe_recv(s, buf);
	break;
	case FRAME_TYPE_S:
/*		proto_debug("RX S V(r)=%d V(s)=%d V(a)=%d V(a_peer)=%d " 
		"N(r)=%d", s->vr, s->vs, s->va, s->va_peer, h->sc.nr);*/
		ret = iecsock_sframe_recv(s, buf);
		free(buf);
	break;
	case FRAME_TYPE_U:
/*		proto_debug("RX U %s stopdt:%i", uframe_func_to_string(uframe_func(h)), s->stopdt);*/
		ret = iecsock_uframe_recv(s, buf);
		free(buf);
	break;
	}
	
	return ret;
}

static void iecsock_buffer_read(struct iecsock *s, int (*frame_recv)(struct iecsock *s))
{
	int ret;
	u_char	wm_read;
	struct iechdr *h;
	
	if (!s->left) {
		ret = bufferevent_read(s->io, s->buf, sizeof(struct iechdr));
		assert(ret == sizeof(struct iechdr));
		h = (struct iechdr *) &s->buf[0];
		if (h->start != 0x68 || 
		    h->length < IEC_APDU_MIN || h->length > IEC_APDU_MAX)
			iecsock_close(s);
		s->left = h->length - IEC104_CTRL_LEN;
		s->len = sizeof(struct iechdr);
		wm_read = s->left;
		ret = 0;
		if (!s->left && !(ret = frame_recv(s))) {
			wm_read = sizeof(struct iechdr);
			s->left = s->len = 0;
		}
		bufferevent_setwatermark(s->io, EV_READ, wm_read, 0);
		if (ret)
			iecsock_close(s);
	} else {
		ret = bufferevent_read(s->io, &s->buf[s->len], s->left);
		s->left -= ret;
		s->len += ret;
		if (s->left)
			return;
		if (frame_recv(s))
			iecsock_close(s);
		wm_read = sizeof(struct iechdr);
		s->left = s->len = 0;
		bufferevent_setwatermark(s->io, EV_READ, wm_read, 0);
	}
}

static void bufreadcb(struct bufferevent *bufev, void *arg)
{
	struct iecsock *s = (struct iecsock *) arg;
	
	while (EVBUFFER_LENGTH(s->io->input))
		iecsock_buffer_read(s, iecsock_frame_recv);
	return;
}

static void bufwritecb(struct bufferevent *bufev, void *arg)
{
	return;
}

static void buferrorcb(struct bufferevent *bufev, short what, void *arg)
{
	struct iecsock *s = (struct iecsock *) arg;
	
	if (s->hooks.disconnect_indication)
		s->hooks.disconnect_indication(s, what);
	else if (default_hooks.disconnect_indication)	
		default_hooks.disconnect_indication(s, what);

	iecsock_close(s);
}

static void iecsock_set_defaults(struct iecsock *s)
{
	s->t0	= DEFAULT_T0;
	s->t1	= DEFAULT_T1;
	s->t2	= DEFAULT_T2;
	s->t3	= DEFAULT_T3;
	s->w 	= DEFAULT_W;
	s->k	= DEFAULT_K;
	TAILQ_INIT(&s->write_q);
	TAILQ_INIT(&s->ackw_q);
	evtimer_set(&s->t1_timer, t1_timer_run, s);
	evtimer_set(&s->t2_timer, t2_timer_run, s);
	evtimer_set(&s->t3_timer, t3_timer_run, s);
}

static void t0_timer_run(int nofd, short what, void *arg)
{
	struct iecsock *s = (struct iecsock *) arg;
	iecsock_connect(&s->addr);
	free(s);
}

static void connect_writecb(int sock, short what, void *arg)
{
	socklen_t slen;
	int ret, opt;
	struct iecsock *s = (struct iecsock *) arg;
	struct timeval tv;
	
	if (what & EV_TIMEOUT) {
		tv.tv_sec = s->t0;
		tv.tv_usec = 0;
		event_add(&s->t0_timer, &tv);
		return;
	}
	
	slen = sizeof(opt);
	ret = getsockopt(s->sock, SOL_SOCKET, SO_ERROR, &opt, &slen);
	if (opt != 0) {
		while(close(s->sock) != 0 && errno == EINTR);
		tv.tv_sec = s->t0;
		tv.tv_usec = 0;
		evtimer_set(&s->t0_timer, t0_timer_run, s);
		evtimer_add(&s->t0_timer, &tv);
		return;
	}
		
	t0_timer_stop(s);
	
	s->io = bufferevent_new(s->sock, bufreadcb, bufwritecb, buferrorcb, s);
	if (!s->io) {
		while(close(s->sock) != 0 && errno == EINTR);
		free(s);
	}
	
	bufferevent_setwatermark(s->io, EV_READ, sizeof(struct iechdr), 0);
	bufferevent_enable(s->io, EV_READ);
	
	s->stopdt = 1;
	iecsock_uframe_send(s, STARTACT);
	t1_timer_start(s);
	
	if (default_hooks.connect_indication)
		default_hooks.connect_indication(s);
	return;
}

static void listen_readcb(int sock, short what, void *arg)
{
	int opt, ret;
	u_long sflags;
	socklen_t slen;
	struct iecsock *s;
	struct event *evt = (struct event *) arg;
	
	event_add(evt, NULL);
	
	s = calloc(1, sizeof(struct iecsock));
	if (!s)
		return;
	
	iecsock_set_defaults(s);	
	s->type = IEC_SLAVE;
	s->stopdt = 1;
		
	slen = sizeof(s->addr);
	s->sock = accept(sock, (struct sockaddr *) &s->addr, &slen);
	if (s->sock == -1) {
		free(s);
		return;
	}
	
	opt = 1;
	sflags = fcntl(s->sock, F_GETFL);
	ret = fcntl(s->sock, F_SETFL, O_NONBLOCK | sflags);
	ret = setsockopt(s->sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (ret == -1)
		goto error_bufev;
	
	s->io = bufferevent_new(s->sock, bufreadcb, bufwritecb, buferrorcb, s);
	if (!s->io)
		goto error_bufev;
	
	bufferevent_setwatermark(s->io, EV_READ, sizeof(struct iechdr), 0);
	bufferevent_enable(s->io, EV_READ);
	
	if (default_hooks.connect_indication)
		default_hooks.connect_indication(s);
		
	return;
error_bufev:
	while (close(s->sock) != 0 && errno == EINTR);			
	free(s);
	return;
}

/**
 * User can use his own specific timer and register a callback on it's arrival.
 */
 
static void iecsock_user_timer_run(int nofd, short what, void *arg)
{
	struct iecsock *s = (struct iecsock *) arg;
	if (s->usercb)
		s->usercb(s, s->userarg);
}

void iecsock_user_timer_set(struct iecsock *s, 
	void (*cb)(struct iecsock *s, void *arg), void *arg)
{
	if (cb == NULL)
		return;
	s->usercb = cb;
	s->userarg = arg;
	evtimer_set(&s->user, iecsock_user_timer_run, s);
}

void iecsock_user_timer_start(struct iecsock *s, struct timeval *tv)
{
	if (evtimer_initialized(&s->user))
		evtimer_add(&s->user, tv);
}

void iecsock_user_timer_stop(struct iecsock *s)
{
	evtimer_del(&s->user);
}

/**
 * iecsock_can_queue - check current window size
 * @param s : iecsock session
 * @return : number of acceptable ASDU's for transmission
 */
size_t iecsock_can_queue(struct iecsock *s)
{
	return (s->k - ((s->vs - s->va + 32767) % 32767));
}

/**
 * iecsock_set_options - set IEC 870-5-104 specific options
 * @param s : iecsock session
 * @param opt: protocol specific options
 *
 * Please, refere to protocol specification!
 */
void iecsock_set_options(struct iecsock *s, struct iecsock_options *opt)
{
	s->t0 = opt->t0;
	s->t1 = opt->t1;
	s->t2 = opt->t2;
	s->t3 = opt->t3;
	s->w = opt->w;
	s->k = opt->k;
}

/**
 * iecsock_set_hooks - set session specific hooks
 * @param s : iecsock session
 * @param hooks : session specific hooks to call on event
 *
 * User can provide his own specific hooks for each event he is interested in.
 * NULL-pointers in hooks structure mean that default hooks are prefered.
 *
 * connect_indication - called when link layer connection is established.
 *
 * disconnect_indication - called when link layer connection terminates.
 *
 * activation_indication - called when monitor direction activates with 
 * STARTACT/STARTCON S-frames.
 *
 * deactivation_indication - called when monitor direction deactivates 
 * with STOPACT/STOPCON S-frames.
 *
 * data_activation - called when ASDU was received, buf points to 
 * allocated structure. It is user responsibility to free allocated resources.
 *
 * transmit_wakeup - called when all frames from transmition queue were
 * sent, acknowledged and iecsock can accept more.
 */
void iecsock_set_hooks(struct iecsock *s, struct iechooks *hooks)
{
	s->hooks.connect_indication = hooks->connect_indication;
	s->hooks.disconnect_indication = hooks->disconnect_indication;
	s->hooks.activation_indication = hooks->activation_indication;
	s->hooks.deactivation_indication = hooks->deactivation_indication;
	s->hooks.data_indication = hooks->data_indication;
	s->hooks.transmit_wakeup = hooks->transmit_wakeup;
}

/**
 * iecsock_connect - register master station connection
 * @param addr : slave station host address.
 * @return : 0 on success -1 on error
 *
 * iecsock_connect is used to register master station connection based on 
 * buffered events. Many iecsock_connect connections can be registered. 
 * iecsock_connect will try to connect to slave station host even if it 
 * refuses connection or timeout occures after DEFAULT_T3 seconds and 
 * stays persistent untill explicitely disconnected by iecsock_close().
 *
 * After all the master station connections have been registered, user 
 * calls event_dispatch() to process events.
 */
int iecsock_connect(struct sockaddr_in *addr)
{
	int ret;
	socklen_t slen;
	u_long flags;
	struct iecsock *s;
	struct timeval tv;
	
	s = calloc(1, sizeof(struct iecsock));
	if (!s)
		return -1;
	
	if ((s->sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		free(s);
		return -1; 
	}
	
	flags = fcntl(s->sock, F_GETFL);
	ret = fcntl(s->sock, F_SETFL, O_NONBLOCK | flags);
	if (ret == -1)
		goto error_sock;
	
	iecsock_set_defaults(s);
	s->type = IEC_MASTER;
	
	if (addr) {
		s->addr.sin_family = addr->sin_family;
		s->addr.sin_port = addr->sin_port;
		s->addr.sin_addr.s_addr = addr->sin_addr.s_addr;
	} else {
		slen = sizeof(struct sockaddr_in);
		s->addr.sin_family = AF_INET;
		s->addr.sin_port = htons(IEC_PORT_DEFAULT);
		inet_pton(AF_INET, "127.0.0.1", &s->addr.sin_addr);
	}
	
	ret = connect(s->sock, (struct sockaddr *) &s->addr, sizeof(struct sockaddr_in));
	if (ret != -1 && errno != EINPROGRESS)
		goto error_sock;

	tv.tv_sec = s->t0;
	tv.tv_usec = 0;
	event_set(&s->t0_timer, s->sock, EV_WRITE, connect_writecb, s);
	event_add(&s->t0_timer, &tv);
	
	return 0;
error_sock:
	while (close(s->sock) != 0 && errno == EINTR);
	free(s);
	return -1;
}

/**
 * iecsock_close - close iecsock session and free occupied resources
 * @param s : iecsock session to close
 * 
 * iecsock_close MUST be called carefully! It is intended to be called only 
 * in bufferevent callbacks when nobody references iecsock session pointer.
 *
 */
void iecsock_close(struct iecsock *s)
{
	iecsock_flush_queues(s);
	evtimer_del(&s->t0_timer);
	evtimer_del(&s->t1_timer);
	evtimer_del(&s->t2_timer);
	evtimer_del(&s->t3_timer);
	evtimer_del(&s->user);
	bufferevent_disable(s->io, EV_READ);
	bufferevent_free(s->io);
	while (close(s->sock) != 0 && errno == EINTR);
	if (s->type == IEC_MASTER)
		iecsock_connect(&s->addr);
	free(s);
}

/**
 * iecsock_listen - register slave station listener
 * @param addr : slave station address and port. If NULL provided, listens on 
 * all available addresses.
 * @param backlog : backlog socket parameter.
 * @return : 0 on success -1 on error
 *
 * iecsock_listen is used to register slave station listening process based on
 * buffered events. Each new master connection creates new socket and new 
 * buffered event. User can later register his own hooks in events he is 
 * interested.
 *
 * After all the master station connections have been registered, user 
 * calls event_dispatch() to process events.
 */
int iecsock_listen(struct sockaddr_in *addr, int backlog)
{
	int sock, opt, ret;
	u_long flags;
	struct sockaddr_in sock_addr;
	struct event *evt;

	evt = calloc(1, sizeof(struct event));
	if (!evt)
		return -1;
	
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		free(evt);
		return -1;
	}
	
	flags = fcntl(sock, F_GETFL);
	ret = fcntl(sock, F_SETFL, O_NONBLOCK | flags);
	if (ret == -1)	
		goto error_sock;
	
	opt = 1;
	ret = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (ret == -1)
		goto error_sock;
	
	if (!addr) {
		memset(&sock_addr, 0, sizeof(sock_addr));
		sock_addr.sin_family = AF_INET;
		sock_addr.sin_port = htons(IEC_PORT_DEFAULT);
		sock_addr.sin_addr.s_addr = INADDR_ANY;
	} else
		sock_addr = *addr;
	
	ret = bind(sock, (struct sockaddr *) &sock_addr, sizeof(sock_addr));
	if (ret == -1)
		goto error_sock;

	ret = listen(sock, backlog);
	if (ret == -1)
		goto error_sock;
	
	event_set(evt, sock, EV_READ, listen_readcb, evt);
	event_add(evt, NULL);
	
	return 0;
error_sock:
	while (close(sock) != 0 && errno == EINTR);
	free(evt);
	return -1;
}
