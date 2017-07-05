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
#ifndef __IECSOCK_INTERNAL_H
#define __IECSOCK_INTERNAL_H

#ifdef __cplusplus
extern  "C" {
#endif

#define DEFAULT_W	8
#define DEFAULT_K	12
#define DEFAULT_T0	30
#define DEFAULT_T1	15
#define DEFAULT_T2	10
#define DEFAULT_T3	20
#define IEC_APDU_MAX	253
#define IEC_APDU_MIN	4


#define t3_timer_stop(__s)	evtimer_del(&(__s)->t3_timer)
#define	t1_timer_stop(__s)	evtimer_del(&(__s)->t1_timer)
#define t1_timer_pending(__s)	evtimer_pending(&(__s)->t1_timer, NULL)
#define t2_timer_stop(__s)	evtimer_del(&(__s)->t2_timer)
#define t2_timer_pending(__s)	evtimer_pending(&(__s)->t2_timer, NULL)
#define t3_timer_pending(__s)	evtimer_pending(&(__s)->t2_timer, NULL)
#define t0_timer_stop(__s)	evtimer_del(&(__s)->t0_timer)


enum frame_type {
	FRAME_TYPE_I,
	FRAME_TYPE_S,
	FRAME_TYPE_U
};

enum uframe_func {
	STARTACT,
	STARTCON,
	STOPACT,
	STOPCON,
	TESTACT,
	TESTCON
};

static inline enum frame_type frame_type(struct iechdr *h)
{
	if (!(h->raw[0] & 0x1))
		return FRAME_TYPE_I;
	else if (!(h->raw[0] & 0x2))
		return FRAME_TYPE_S;
	else
		return FRAME_TYPE_U; 
}

static inline enum uframe_func uframe_func(struct iechdr *h)
{
	if (h->raw[0] & 0x4)
		return STARTACT;
	else if (h->raw[0] & 0x8)
		return STARTCON;
	else if (h->raw[0] & 0x10)
		return STOPACT;
	else if (h->raw[0] & 0x20)
		return STOPCON;
	else if (h->raw[0] & 0x40)
		return TESTACT;
	else
		return TESTCON;
}

static inline char * uframe_func_to_string(enum uframe_func func)
{
	switch (func) {
	case STARTACT:
		return "STARTACT";
	case STARTCON:
		return "STARTCON";
	case STOPACT:
		return "STOPACT";
	case STOPCON:
		return "STOPCON";
	case TESTACT:
		return "TESTACT";
	case TESTCON:
		return "TESTCON";
	default:
		return "UNKNOWN";
	}
}

static inline char * frame_to_string(struct iechdr *h)
{
	switch (frame_type(h)) {
	case FRAME_TYPE_I:
		return "I";
	case FRAME_TYPE_S:
		return "S";
	case FRAME_TYPE_U:
		return "U";
	default:
		return "0";
	}
}

#ifdef __cplusplus
}
#endif

#endif
