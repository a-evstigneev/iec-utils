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
#ifndef __IECSOCK_H
#define __IECSOCK_H

#ifdef __cplusplus
extern "C" {
#endif

#define IEC104_BUF_LEN		255
#define	IEC_PORT_DEFAULT	2404
#define IEC104_CTRL_LEN		4
#define IEC104_ASDU_MAX		249

#ifndef TAILQ_EMPTY
#define TAILQ_EMPTY(head)       ((head)->tqh_first == NULL)
#endif /* TAILQ_EMPTY */

#ifndef TAILQ_FIRST
#define TAILQ_FIRST(head)       ((head)->tqh_first)
#endif /* TAILQ_FIRST */

#ifndef TAILQ_NEXT
#define TAILQ_NEXT(elm, field) ((elm)->field.tqe_next)
#endif /* TAILQ_NEXT */

enum {
	IEC_SLAVE,
	IEC_MASTER
};

struct iec_buf {
	TAILQ_ENTRY(iec_buf) head;
	u_char	data_len;	/* actual ASDU length */
	
	struct iechdr {
		u_char	start;
		u_char	length;
		u_char	raw[0];
		union {
			struct iec_i {
				u_char	ft:1;
				u_short	ns:15;
				u_char	res:1;
				u_short	nr:15;
			} ic;
			struct iec_s {
				u_char	ft:1;
				u_short	res1:15;
				u_char	res2:1;
				u_short	nr:15;
			} sc;
			struct iec_u {
				u_char	ft:2;
				u_char	start_act:1;
				u_char	start_con:1;
				u_char	stop_act:1;
				u_char	stop_con:1;
				u_char	test_act:1;
				u_char	test_con:1;
				u_char	res1;
				u_short	res2;
			} uc;
		};
	} h;	
	u_char	data[0];
} __attribute__((__packed__));

struct iecsock;

struct iechooks {
	void (*connect_indication)(struct iecsock *s);
	void (*activation_indication)(struct iecsock *s);
	void (*deactivation_indication)(struct iecsock *s);
	void (*disconnect_indication)(struct iecsock *s, short reason);
	void (*data_indication)(struct iecsock *s, struct iec_buf *b);
	void (*transmit_wakeup)(struct iecsock *s);
};

extern struct iechooks default_hooks;

TAILQ_HEAD(iec_buf_queue, iec_buf);

struct iecsock {
	int		sock;		/* socket descriptor */
	u_char		buf[IEC104_BUF_LEN];
	u_char		len;
	u_char		left;
	u_char		type;
	u_char		stopdt:1;	/* monitor direction 0=active 1=inactive */
	u_char		testfr:1;	/* test function 1=active 0=inactive */
	u_short		w, k;
	u_short		va, vr, vs, va_peer;
	u_short		t0, t1, t2, t3;
	struct event	t0_timer;
	struct event	t1_timer;
	struct event	t2_timer;
	struct event	t3_timer;
	struct sockaddr_in addr;	/* socket address */
	struct bufferevent *io;
	struct iec_buf_queue write_q;	/* write queue */
	struct iec_buf_queue ackw_q;	/* acknowledge wait queue */
	
	struct iechooks hooks;
	struct event user;
	void	(*usercb)(struct iecsock *s, void *arg);
	void	*userarg;
	u_long recv_cnt, xmit_cnt;
};

struct iecsock_options {
	u_short		w;
	u_short		k;
	u_short		t0;
	u_short		t1;
	u_short		t2;
	u_short		t3;
};

void iecsock_prepare_iframe(struct iec_buf *buf);
void iecsock_run_write_queue(struct iecsock *s);

int iecsock_listen(struct sockaddr_in *addr, int backlog);
int iecsock_connect(struct sockaddr_in *addr);
size_t iecsock_can_queue(struct iecsock *s);
void iecsock_set_options(struct iecsock *s, struct iecsock_options *opt);
void iecsock_set_hooks(struct iecsock *s, struct iechooks *hooks);
void iecsock_close(struct iecsock *s);
void iecsock_user_timer_set(struct iecsock *s, 
	void (*cb)(struct iecsock *s, void *arg), void *arg);
void iecsock_user_timer_start(struct iecsock *s, struct timeval *tv);
void iecsock_user_timer_stop(struct iecsock *s);

#ifdef __cplusplus
}
#endif

#endif
