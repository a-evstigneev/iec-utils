/*
 * Copyright (C) 2005 by Grigoriy A. Sitkarev                            
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

#ifndef __IEC104_TYPES_H
#define __IEC104_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif
 
#define M_SP_NA_1	1
#define M_BO_NA_1	7
#define M_ME_NA_1	9
#define M_ME_NB_1	11
#define M_ME_NC_1	13
#define M_SP_TB_1	30
#define M_BO_TB_1	33
#define M_ME_TD_1	34
#define M_ME_TE_1	35
#define M_ME_TF_1	36

/* 32-bit string state and change data unit */
struct iec_stcd {
	u_short		st[0];
	u_char		st1	:1;
	u_char		st2	:1;
	u_char		st3	:1;
	u_char		st4	:1;
	u_char		st5	:1;
	u_char		st6	:1;
	u_char		st7	:1;
	u_char		st8	:1;
	u_char		st9	:1;
	u_char		st10	:1;
	u_char		st11	:1;
	u_char		st12	:1;
	u_char		st13	:1;
	u_char		st14	:1;
	u_char		st15	:1;
	u_char		st16	:1;
	u_short		cd[0];
	u_char		cd1	:1;
	u_char		cd2	:1;
	u_char		cd3	:1;
	u_char		cd4	:1;
	u_char		cd5	:1;
	u_char		cd6	:1;
	u_char		cd7	:1;
	u_char		cd8	:1;
	u_char		cd9	:1;
	u_char		cd10	:1;
	u_char		cd11	:1;
	u_char		cd12	:1;
	u_char		cd13	:1;
	u_char		cd14	:1;
	u_char		cd15	:1;
	u_char		cd16	:1;
}__attribute__((__packed__));

/* CP56Time2a timestamp */
typedef struct cp56time2a {
	u_short		msec;
	u_char		min	:6;
	u_char		res1	:1;
	u_char		iv	:1;
	u_char		hour	:5;
	u_char		res2	:2;
	u_char		su	:1;
	u_char		mday	:5;
	u_char		wday	:3;
	u_char		month	:4;
	u_char		res3	:4;
	u_char		year	:7;
	u_char		res4	:1;	
} cp56time2a __attribute__((__packed__));

/* M_SP_NA_1 - single point information with quality description */
struct iec_type1 {
	u_char		sp	:1; /* single point information */
	u_char		res	:3;
	u_char		bl	:1; /* blocked/not blocked */
	u_char		sb	:1; /* substituted/not substituted */
	u_char		nt	:1; /* not topical/topical */
	u_char		iv	:1; /* valid/invalid */
} __attribute__((__packed__));

/* M_BO_NA_1 - state and change information bit string */
struct iec_type7 {
	struct iec_stcd	stcd;
	u_char		ov	:1; /* overflow/no overflow */
	u_char		res	:3;
	u_char		bl	:1; /* blocked/not blocked */
	u_char		sb	:1; /* substituted/not substituted */
	u_char		nt	:1; /* not topical/topical */
	u_char		iv	:1; /* valid/invalid */
}__attribute__((__packed__));

/* M_ME_NA_1 - normalized measured value */
struct iec_type9 {
	u_short		mv;	/* normalized value */
	u_char		ov	:1; /* overflow/no overflow */
	u_char		res	:3;
	u_char		bl	:1; /* blocked/not blocked */
	u_char		sb	:1; /* substituted/not substituted */
	u_char		nt	:1; /* not topical/topical */
	u_char		iv	:1; /* valid/invalid */
}__attribute__((__packed__));

/* M_ME_NB_1 - scaled measured value */
struct iec_type11 {
	u_short		mv;	/* scaled value */
	u_char		ov	:1; /* overflow/no overflow */
	u_char		res	:3;
	u_char		bl	:1; /* blocked/not blocked */
	u_char		sb	:1; /* substituted/not substituted */
	u_char		nt	:1; /* not topical/topical */
	u_char		iv	:1; /* valid/invalid */
}__attribute__((__packed__));

/* M_ME_NC_1 - short floating point measured value */
struct iec_type13 {
	float		mv;
	u_char		ov	:1; /* overflow/no overflow */
	u_char		res	:3;
	u_char		bl	:1; /* blocked/not blocked */
	u_char		sb	:1; /* substituted/not substituted */
	u_char		nt	:1; /* not topical/topical */
	u_char		iv	:1; /* valid/invalid */
}__attribute__((__packed__));

/* M_SP_TB_1 - single point information with quality description and time tag */
struct iec_type30 {
	u_char		sp	:1; /* single point information */
	u_char		res	:3;
	u_char		bl	:1; /* blocked/not blocked */
	u_char		sb	:1; /* substituted/not substituted */
	u_char		nt	:1; /* not topical/topical */
	u_char		iv	:1; /* valid/invalid */
	cp56time2a	time;
} __attribute__((__packed__));

/* M_BO_TB_1 - state and change information bit string and time tag  */
struct iec_type33 {
	struct iec_stcd	stcd;
	u_char		ov	:1; /* overflow/no overflow */
	u_char		res	:3;
	u_char		bl	:1; /* blocked/not blocked */
	u_char		sb	:1; /* substituted/not substituted */
	u_char		nt	:1; /* not topical/topical */
	u_char		iv	:1; /* valid/invalid */
	cp56time2a	time;
}__attribute__((__packed__));

/* M_ME_TD_1 - scaled measured value with time tag */
struct iec_type34 {
	u_short		mv;	/* scaled value */
	u_char		ov	:1; /* overflow/no overflow */
	u_char		res	:3;
	u_char		bl	:1; /* blocked/not blocked */
	u_char		sb	:1; /* substituted/not substituted */
	u_char		nt	:1; /* not topical/topical */
	u_char		iv	:1; /* valid/invalid */
	cp56time2a	time;
}__attribute__((__packed__));

/* M_ME_TE_1 - scaled measured value with time tag*/
struct iec_type35 {
	u_short		mv;	/* scaled value */
	u_char		ov	:1; /* overflow/no overflow */
	u_char		res	:3;
	u_char		bl	:1; /* blocked/not blocked */
	u_char		sb	:1; /* substituted/not substituted */
	u_char		nt	:1; /* not topical/topical */
	u_char		iv	:1; /* valid/invalid */
	cp56time2a	time;
}__attribute__((__packed__));

/* M_ME_TF_1 - short floating point measurement value and time tag */
struct iec_type36 {
	float		mv;
	u_char		ov	:1; /* overflow/no overflow */
	u_char		res	:3;
	u_char		bl	:1; /* blocked/not blocked */
	u_char		sb	:1; /* substituted/not substituted */
	u_char		nt	:1; /* not topical/topical */
	u_char		iv	:1; /* valid/invalid */
	cp56time2a	time;
}__attribute__((__packed__));

/* Data unit identifier block - ASDU header */
struct iec_unit_id {
	u_char		type;	/* type identification */
	u_char		num	:7; /* number of information objects */
	u_char		sq	:1; /* sequenced/not sequenced address */
	u_char		cause	:6; /* cause of transmission */
	u_char		t	:1; /* test */
	u_char		pn	:1; /* positive/negative app. confirmation */
	u_short		ca;	/* common address of ASDU */
}__attribute__((__packed__));

#ifdef __cplusplus
}
#endif

#endif	/* __IEC104_TYPES_H */
