/* 
 * HISTORY
 * 09-Nov-86  John Seamons (jks) at NeXT
 *	Ported to NeXT.
 */ 

#ifndef _REG_
#define _REG_

#import <sys/types.h>

/*
 * processor exception frame definitions
 */

/* format codes */
#define EF_NORMAL4	0x0
#define	EF_THROWAWAY	0x1
#define	EF_NORMAL6	0x2
#define	EF_FLOATPT	0x3	/* 040 only */
#define	EF_ACCESS	0x7	/* 040 only */
#define	EF_COPROC	0x9	/* 030 only */
#define	EF_SHORTBUS	0xa	/* 030 only */
#define	EF_LONGBUS	0xb	/* 030 only */

#ifndef ASSEMBLER
struct excp_frame {
	short	e_fsize;	/* exception frame extension size */
	short	e_sr;
	int	e_pc;
	short	e_format : 4,
		e_vector : 12;
};

struct special_status {
	u_short	ss_faultc : 1,
		ss_faultb : 1,
		ss_rerunc : 1,
		ss_rerunb : 1,
			: 3,
		ss_faultrerund : 1,
		ss_rmw : 1,
		ss_read : 1,
		ss_cyclesize : 2,
			: 1,
		ss_fcode : 3;
};

#define	e_faultc	e_ss.ss_faultc
#define	e_faultb	e_ss.ss_faultb
#define	e_rerunc	e_ss.ss_rerunc
#define	e_rerunb	e_ss.ss_rerunb
#define	e_faultd	e_ss.ss_faultrerund
#define	e_rerund	e_ss.ss_faultrerund
#define	e_rmw		e_ss.ss_rmw
#define	e_read		e_ss.ss_read
#define	e_cyclesize	e_ss.ss_cyclesize
#define	e_fcode		e_ss.ss_fcode
#endif	ASSEMBLER

/* bit fields as masks */
#define	SS_FAULTC	0x8000
#define	SS_FAULTB	0x4000
#define	SS_FAULTD	0x0100
#define	SS_READ		0x0040
#define	SS_FCODE	0x0007

#ifndef	ASSEMBLER
struct special_status_040 {
	u_short	ss_cp : 1,
		ss_cu : 1,
		ss_ct : 1,
		ss_cm : 1,
		ss_ma : 1,
		ss_atc : 1,
		ss_lk : 1,
		ss_rw : 1,
		: 1,
		ss_size : 2,
		ss_tt : 2,
		ss_tm : 3;
};

#define	e_cp		e_ss.ss_cp
#define	e_cu		e_ss.ss_cu
#define	e_ct		e_ss.ss_ct
#define	e_cm		e_ss.ss_cm
#define	e_ma		e_ss.ss_ma
#define	e_atc		e_ss.ss_atc
#define	e_lk		e_ss.ss_lk
#define	e_rw		e_ss.ss_rw
#define	e_size		e_ss.ss_size
#define	e_tt		e_ss.ss_tt
#define	e_tm		e_ss.ss_tm
#endif	ASSEMBLER

/* bit fields as masks */
#define	SS_CP		0x8000
#define	SS_CU		0x4000
#define	SS_CT		0x2000
#define	SS_CM		0x1000
#define	SS_MA		0x0800
#define	SS_ATC		0x0400
#define	SS_LK		0x0200
#define	SS_RW		0x0100
#define	SS_SIZE		0x0060
#define	SS_TT		0x0018
#define	SS_TM		0x0007

#ifndef	ASSEMBLER
struct writeback_status {
	u_short	: 8,
		wbs_v : 1,
		wbs_size : 2,
		wbs_tt : 2,
		wbs_tm : 3;
};

struct excp_normal6 {
	int	e_ia;
};

struct excp_coproc {
	int	e_ia;
	short	e_internal;
	short	e_op;
	int	e_effaddr;
};

struct excp_floatpt {
	int	e_effaddr;
};

struct excp_access {
	int	e_effaddr;
	struct	special_status_040 e_ss;
	struct	writeback_status e_wb3s, e_wb2s, e_wb1s;
	int	e_faultaddr;
	int	e_wb3a, e_wb3d;
	int	e_wb2a, e_wb2d;
	int	e_wb1a, e_wb1d_pd0;
 	int	e_pd1, e_pd2, e_pd3;
};

struct excp_shortbus {
	short	e_internal1;
	struct	special_status e_ss;
	short	e_ipipec;
	short	e_ipipeb;
	int	e_faultaddr;
	short	e_internal2[2];
	int	e_dob;
	short	e_internal3[2];
};

struct excp_longbus {
	short	e_internal1;
	struct	special_status e_ss;
	short	e_ipipec;
	short	e_ipipeb;
	int	e_faultaddr;
	short	e_internal2[2];
	int	e_dob;
	short	e_internal3[4];
	int	e_stagebaddr;
	short	e_internal4[2];
	int	e_dib;
	short	e_internal5[22];
};

struct regs {
	int	r_dreg[8];
#define	r_d0	r_dreg[0]
	int	r_areg[8];
#define	r_sp	r_areg[7]	/* system sp saved after switch to SR_SUPER */
#if MONITOR
	int	r_usp;		/* user sp */
	int	r_isp;
	int	r_msp;
	int	r_sfc, r_dfc;
	int	r_vbr;
	int	r_caar, r_cacr;
	int	r_crph, r_crpl;
	int	r_srph, r_srpl;
	int	r_tc;
	int	r_tt0, r_tt1;
	int	r_ts;
#endif MONITOR
	short	pad;		/* pad to keep stack aligned */
	struct	excp_frame r_evec;
#define	r_fsize		r_evec.e_fsize
#define	r_sr		r_evec.e_sr
#define	r_pc		r_evec.e_pc
#define	r_format	r_evec.e_format
#define	r_vector	r_evec.e_vector
};

/* offset definitions into u.u_ar0 for machine independent code */
#define	R0	0
#define	R1	1
#define	SP	15
#define	PS	17
#define	PC	17
#endif ASSEMBLER

#endif _REG_
