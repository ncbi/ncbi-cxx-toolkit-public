/* $Id$
*===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
* File Name:  thrdatd.h
*
* Author:  Stephen Bryant
*
* Initial Version Creation Date: 08/16/2000
*
* $Revision$
*
* File Description: threader
*/

/* atd.h - structure and function declarations for adaptive threading  */
			/* Steve Bryant, 6/93 */

#if !defined(THRDATD_H)
#define THRDATD_H

#ifdef __cplusplus
extern "C" {
#endif

/* Argument data structures */

typedef struct _Fld_Mtf {		/* Folding motif data structure */
  int	n;		/* Number of residues in the structure */
  struct {		/* Residue-residue contact list */
    int	*r1;	/* Index of first residue of in a pair */
    int	*r2;	/* Index second residue in a pair */
    int	*d;	/* Distance interval in a pair */
    int	n; 	/* Number of contact pairs */ 
		} rrc;
  struct  {		/* Residue-peptide contact list */
    int	*r1;	/* Index of side-chain residue in a pair */
    int	*p2;	/* Index of the peptide group in a pair */
    int	*d;	/* Distance interval of a contact pair */
    int	n; 	/* Number of contact pairs */ 
		} rpc;
  int **mll;		/* Minimum loop lengths by residue pair */ 
} Fld_Mtf;


typedef struct _Cor_Def {	/* Core definition data structure */
  struct {		/* Core segment location limits */
    int	*rfpt;	/* Index of central reference residue */
    int	*nomn;	/* Minimun offset of core segment N-termini */
    int	*nomx;	/* Maxixmum offset of core segment N-termini */
    int	*comn;	/* Minimun offset of core segment C-termini */
    int	*comx;	/* Maxixmum offset of core segment C-termini */
    int	n;	/* Number of core segments */
		} sll;		
  struct { 		/* Loop length limits */
    int	*llmn;	/* Minimum lengths, including tails */
    int	*llmx;	/* Maximum lengths, including tails */
    int	*lrfs;	/* Maximun number of residues in ref. state */
    int	n;	/* Number of loops, one more than core segs */
		} lll;
  struct {		/* Fixed-sequence segment locations */
    int	*nt;	/* Fixed segment N-termini */
    int	*ct;	/* Fixed segment C-termini */
    int	*sq;	/* Motif sequence containing fixed segs */
    int	n;	/* Number of fixed segments */
		} fll;
} Cor_Def;


typedef struct _Qry_Seq { 		/* Query sequence data structure */ 
  int 	*sq;		/* The sequence as an integer array */
  int	n;		/* Number of residues in that sequence */
  struct {		/* Alignment contstraints for this sequence */
    int	*mn;	/* Minimum and maximum query sequence indices */
    int	*mx;	/* to be aligned with each core segment */
    int	n;	/* Number of constraints */
		} sac;
} Qry_Seq; 


typedef struct _Rcx_Ptl {		/* Pairwise contact potential */
  int	***rre;		/* Pair energies by type and distance */
  int	**re;		/* Hydrophobic energies by type and distance */
  int	***rrt;		/* Sum of pair plus hydrophobic energies */
  int	nrt;		/* Number or residue types */
  int	ndi;		/* Number of distance intervals */
  int	ppi;		/* Index of the peptide group */
} Rcx_Ptl;


typedef struct _Gib_Scd { 		/* Gibbs schedule parameters */
  int ntp;    /* number of trajectory points */
  int	nrs;		/* Number of random starts */
  int	nts;		/* Number of temperature steps */
  int	crs;		/* Number of starts before convergence test */
  int	cfm;		/* Top thread frequency convergence criterion */
  int	csm;		/* Top thread start convergence criterion */
  int	cet;		/* Temperature for convergence test ensemble */
  int	cef;		/* Percent of ensemble defining top threads */
  int	isl;		/* Code for choice of starting locations */
  int	iso;		/* Code for choice of segment sample order */
  int	ito;		/* Code for choice of terminus sample order */
  int	rsd;		/* Seeds for random number generator */
  int	als;		/* Code for choice of alignment record style */
  int	trg;		/* Code for choice of trajectory record */
  int	*nti;		/* Number of iterations per tempeature step */ 	
  int	*nac;		/* Number of alignment cycles per iteration */
  int	*nlc;		/* Number of location cycles per iteration */
  int	*tma;		/* Temperature steps for alignment sampling */ 	
  int	*tml;		/* Temperature steps for location sampling */ 	
  int	*lms;		/* Iterations before local minimum test */
  int	*lmw;		/* Iterations in local min test interval */
  int	*lmf;		/* Percent of top score indicating local min */
} Gib_Scd;


typedef struct _Thd_Tbl {		/* Thread table data structure */
  float	*tg;		/* Energy of this thread  */
  float	*ps;		/* Potential energy */
  float	*ms;		/* Motif energy */
  float   *cs;		/* Conservation energy */
  float	*lps;		/* Loopout energy */
  float   *zsc;           /* Z-score */
  float   *g0;		/* Average total energy of shuffled seq */
  float   *m0;		/* Average motif score of shuffled seq */
  float   *errm;		/* Standard error of random motif scores */
  float   *errp;		/* Standard error of random contact scores */
  int	*tf;		/* Frequency of this thread in sampling */
  int	*ts;		/* Number of starts in which it was found */
  int	*ls;		/* Last start in which this thread was found */ 
  int	**al;		/* Alignment of query to each core segment */
  int	**no;		/* N-terminal offset of each core segment */
  int	**co;		/* C-terminal offset of each core segment */
  int	*pr;		/* Index of next-higher energy thread */
  int	*nx;		/* Index of next-lower energy thread */
  int	mx;		/* Index of lowest energy thread in list */
  int	mn;		/* Index of highest energy thread in list */
  int	n;		/* Maximum number of threads in linked list */
  int	nsc;	/* Number of core segments */
} Thd_Tbl;


typedef struct _Thd_Tst {		/* Storage for local min and convergence test */
  float   *bw;            /* Boltzmann weight of this thread */
  int	nw;		/* Number of Boltzmann weight values */
  float	*ib;		/* Best energy found for each iteration */
  int	nb;		/* Number of best energy values */
  float	gb;		/* Best energy across iterations */
  int	lm;		/* Flag for local minimum */
  int	tf;		/* Top-thread best alignment frequency */
  int	ts;		/* Top-thread best random start count */
} Thd_Tst;


/* Tracking data structures for Gibb's sampling */

typedef struct _Cur_Loc {		/* Location of core segments in motif */
  int	*no;		/* N-terminus offset from reference point */
  int	*co;		/* C-terminus offset from reference point */
  int	nsc;		/* Number of core segments */
  int	*lp;		/* Minimum loop length derived from motif */
  int	nlp;		/* Number of loops */
  int	*cr;		/* Core segment index by residue position */
  int	nmt;		/* Number of residue positions in motif */
} Cur_Loc;


typedef struct _Cur_Aln {		/* Aligment of query sequence with core */
  int	*al;		/* Query sequence index aligned with rfpt  */
  int	nsc;		/* Number of core segments */
  int	*sq;		/* Residue types currently assigned to motif */
  int	*cf;		/* Core segment indices at maximun extent */ 
  int	nmt;		/* Number of residue positions in motif */
} Cur_Aln;


typedef struct _Seg_Nsm {		/* Number of contacts by segment pair */
  int	***nrr;		/* Res-res counts by seg by seg by dis */
  int	*srr;		/* Total res-res counts by distance interval */
  int	***nrp;		/* Res-pep counts by seg by seg by dis */
  int	*srp;		/* Total res-pep counts by distance interval */
  int	***nrf;		/* Res-fixed contacts by seg by type by dis */
  int	**frf;		/* Total res-fixed contacts by type by dis */
  int	*srf;		/* Total res-fixed contacts by dis */
  int	trf;		/* Total res-fixed contacts */
  int	nsc;		/* Number of core segments */
  int	ndi;		/* Number of distance intervals */
  int	nrt;		/* Number of residue types */
} Seg_Nsm;


typedef struct _Seg_Cmp {		/* Segment pair amino acid composition */
  int	**srt;		/* Residue type counts by core segment */
  int	nsc;		/* Number of core segments */
  int	nrt;		/* Number of residue types */
  int	**lrt;		/* Residue type counts by loop segment */
  int	nlp;		/* Number of loop segments */
  int	*rt;		/* Total residue type counts */
  int	*rto;		/* Residue type counts from previous thread */
} Seg_Cmp;


typedef struct _Seg_Gsm {                /* Current contact energies by segmenmt pair */
  int     **gss;          /* Potential energies by core segment pair */
  int     *gs;            /* Potential energies by core segment */
  int     *ms;		/* Motif energies by core segment */
  int	*cs;		/* Conservation energies by core segment */
  int	*ls;		/* Loopout energies by by core segment */
  int     *s0;		/* Expected motif energies by core segment */
  int     nsc;            /* The number of segments */
} Seg_Gsm;


typedef struct _Thd_Cxe {		/* Expected energy of pairwise contacts */
  float	*rp;		/* Residue frequencies, from type counts */
  float	**rrp;		/* Mass action residue pair probabilities */
  int	nrt;		/* Number of residue types */
  float	*rre;		/* Expected energy of a res-res contact */
  float	*rpe;		/* Expected energy of a res-pep contact */
  float	*rfe;		/* Expected energy of a res-fix contact */
  int	ndi;		/* Number of distance intervals */
} Thd_Cxe;


typedef struct _Thd_Gsm {		/* Total thread energy */
  int	g;		/* G(r|m), sum of potentials */
  float	g0;		/* G0(r|m), reference state energy for potential */
  float	dg;		/* Overall thread energy */
  float	ps;		/* G(r|m)-G0(r|m)-overall potential energy */
  float	ms;		/* Overall motif energy */
  float m0;   /* Average motif score for random assignment*/
  float	cs;		/* Overall conservation energy */
  float	ls;		/* Overall loopout energy */
} Thd_Gsm;


typedef struct _Cxl_Als {		/* Contacts by segment for alignment sampling */
  struct {		/* Residue-residue contact list */
    int	*r1;	/* Index of first residue in a pair */
    int	*r2;	/* Index second residue in a pair */
    int	***e;	/* Energy of each type pair at these pos. */
    int	n; 	/* Number of contact pairs */ 
		} rr;
  struct {		/* Residue-hfo/pep/fix contact list */
    int	*r;	/* Index of a position with contacts */
    int	**e;	/* Energy of each residue type at this pos. */
    int	n; 	/* Number of residue-peptide contact terms */ 
		} r;
} Cxl_Als;


typedef struct _Cxl_Los {		/* Contacts by segment for extent sampling */
  struct {		/* Residue-residue contact list */
    int	*r1;	/* Index of first residue in a pair */
    int	*r2;	/* Index second residue in a pair */
    int	*d;	/* Contact distance interval */
    int	*e;	/* Energy of a contact at these positions */
    int	n; 	/* Number of contact pairs */ 
		} rr;
  struct {		/* Residue-peptide contact list */
    int	*r1;	/* Index of first residue in a pair */
    int	*p2;	/* Index second residue in a pair */
    int	*d;	/* Contact distance interval */
    int	*e;	/* Energy of the pair, given alignment */
    int	n; 	/* Number of contact pairs */ 
		} rp;
  struct  {		/* Residue-fixed environment contact list */
    int	*r1;	/* Index of a position with contacts */
    int	*t2;	/* Type of second residue in the pair */
    int	*d;	/* Contact distance interval */
    int	*e;	/* Energy of the pair, given alignment */
    int	n; 	/* Number of residue-peptide contact terms */ 
		} rf;
} Cxl_Los;


typedef struct _Seg_Ord {		/* Sampling order for core segments */
  int	*si;		/* Indices of variables for sampling */
  int	*so;		/* Sample order for variables */
  int	*to;		/* Flags first terminus for extent sampling */
  int	nsc;		/* Number of core segments */
} Seg_Ord;


typedef struct _Rnd_Smp {		/* Random-sampling and permutation parameters */
  int	n;		/* Number of values a variable may adopt */
  float	*p;		/* Probabilitites associated with each value */
  float	*e;		/* Energies associated with each value */
  int     *lsg;		/* Start/stop sites of loop segments in structure */
  int     *aqi;		/* Indeces of the residues to be shuffled */
  int     *r,*o;		/* Shuffled values and order permutation */
  int     *sq;		/* Permuted sequence */
} Rnd_Smp;


typedef struct _Seq_Mtf {		/* Sequence motif parameters */
  int	**ww;	        /* Weights */
  int **freqs;      /* residue frequencies */
  int	n;		        /* Number of residues in structure */
  int AlphabetSize; /* Number of letters in alphabet */
  float ww0;        /* Expected energy of template profile term */
} Seq_Mtf;


/*----------------------------------------------------------------------------*/
/* Constants                                                                  */
/*----------------------------------------------------------------------------*/
#define BIGNEG -1000000000.
#define NERZRO .000001
#undef	ATD_DEBUG
#undef	SLO0_DEBUG
#define SGI
#undef	SUN

#ifdef __cplusplus
}
#endif

#endif
