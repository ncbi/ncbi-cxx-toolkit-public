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
* File Name:  thrdatd.c
*
* Author:  Stephen Bryant
*
* Initial Version Creation Date: 08/16/2000
*
* $Revision$
*
* File Description: threader
*/

#ifdef _MSC_VER
#pragma warning(disable:4244)   // disable double->float warning in MSVC
#pragma warning(disable:4018)   // disable signed/unsigned mismatch warning in MSVC
#endif

/*----------------------------------------------------------------------------*/
/* atd - adaptive threading of protein sequence through folding motif         */
/*                             Steve Bryant, 2/94                             */
/*----------------------------------------------------------------------------*/
/* Uses the Gibb's sampling algorithm to find optimal alignments of a	      */
/* query sequence and folding motif, and to locate the optimal core 	      */
/* structure, given a set of constraints on the locations of each core	      */
/* segment.								      */
/*----------------------------------------------------------------------------*/
/* Test version of 8/96: Sets alignment-only flag to ttbi.c.                  */
/* Also passes trajectory data back to calling routine, satd.c.               */
/* Also has test code to stop iterations in apparent local minima             */
/*----------------------------------------------------------------------------*/

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <algo/structure/threader/thrdatd.h>
#include <algo/structure/threader/thrddecl.h>

typedef char Boolean;
#define TRUE 1
#define FALSE 0

int atd(Fld_Mtf* mtf, Cor_Def* cdf, Qry_Seq* qsq, Rcx_Ptl* pmf,
        Gib_Scd* gsp, Thd_Tbl* ttb, Seq_Mtf* psm, float* trg, int zscs,
        double ScalingFactor, float PSSM_Weight) {
/*------------------------------------------------------------------*/
/* mtf:  Contact matrices defining the folding motif                */
/* cdf:  Core segment locations and loop length limits              */
/* qsq:  Sequence to thread with alignment contraints               */
/* pmf:  Potential of mean force as a 3-d lookup table              */
/* gsp:  Various control parameters for Gibb's sampling             */
/* ttb:  Tables to hold Results of Gibbs sampled threading          */
/* psm:  Motif matching parameters                                  */
/* trg:  Will store trajectory energy values                        */
/* zscs: which z scores calculated? 0=none, 1=lowest energy, 2=all  */
/*------------------------------------------------------------------*/

  Cur_Loc *sli;	/* Current locations of core segments in the motif    */
  Cur_Aln *sai;	/* Current alignment of query sequence with core      */
  Seg_Nsm *spn;	/* Partial sums of contact counts by segment pair     */
  Seg_Cmp *spc;	/* Partial sums of residue counts by segment pair     */
  Thd_Cxe *cxe;	/* Expected energy of a contact in current thread     */
  Seg_Gsm *spe;	/* Partial sums of contact energies by segment pair   */
  Thd_Gsm *tdg;	/* Total energies for the current threaded sequence   */
  Cxl_Los **cpr;	/* Contacts by segment, largest possible set          */
  Cxl_Los **cpl;	/* Contacts by segment, given current alignment       */
  Cxl_Als **cpa;	/* Contacts by segment, given current location        */
  Seg_Ord *sgo;	/* Segment samping order and related quantities       */
  Rnd_Smp *pvl;	/* Alignment-location parameter value probabilities   */
  Thd_Tst *tts;	/* Parameters for local min and convergence tests     */

/*----------------------------------------------------------------------------*/
/* Parameters from argument data structures                                   */
/*----------------------------------------------------------------------------*/
 int	nsc; 		/* Number of threaded segments in core definition     */
 int	nmt;		/* Number of residue positions in the folding motif   */
 int	nlp;		/* Number of loops, including n- and c-terminal tails */
 /*int	nrr;*/		/* Number of residue-residue contacts in motif        */
 /*int	nrp;*/		/* Number of residue-peptide contacts in motif        */
 int	nrt;		/* Number of residue types in contact potential       */
 int	ndi;		/* Number of distance intervals in potential          */

/*----------------------------------------------------------------------------*/
/* Countdown timers for Gibbs annealing schedule                              */
/*----------------------------------------------------------------------------*/
 int	nrs;		/* Number of random starts for Gibb's sampling        */
 int	nts;		/* Number of temperature steps                        */
 int	nti;		/* Number of iterations per tempeature step */
 int	nac;		/* Number of alignment cycles per iteration */
 int	nlc;		/* Number of extent/location cycles per iteration */
 int	ngs;		/* Number of trajectory points recorded */

/*----------------------------------------------------------------------------*/
/* Other local variables                                                      */
/*----------------------------------------------------------------------------*/
 int	i,j,k,n;	/* Counters                                           */
 Cxl_Als	*ca;	/* Pointer to alignment sampling pair lists           */
 Cxl_Los	*cl;	/* Pointer to location sampling lists                 */
 Cxl_Los	*cr;	/* Pointer to contact pair lists                      */
 int	spcd;		/* Flags a change in threaded sequence composition    */
 int	mx,mn;		/* Range of segment alignment or location             */
 int	cs;		/* Current segment in alignment or location sampling  */
 int	ct=0;		/* Current terminus for segment location sampling     */
 int	al;		/* Current alignment of a core segment                */
 int	of;		/* Current terminus offset from reference position    */
 /*int	rf;*/		/* Reference position for a core element              */
 int  tmp;  /* temp holder for ttb->mx */
 int  dist, dist2; /* for loop distances */
 /*float hh;*/

/*----------------------------------------------------------------------------*/
/* Function declarations for routines returning non-integer values            */
/*----------------------------------------------------------------------------*/
/* float  dgri(); */


/*----------------------------------------------------------------------------*/
/*  if 2 aligned regions on the query sequence are constrained to match       */
/*  2 adjacent core regions, then make sure the loop constraints are          */
/*  sufficiently big.                                                         */
/*  this over-rides the automatically determined loop constraints.            */
/*----------------------------------------------------------------------------*/
  /* for each loop between core blocks */
  for (i=0; i<(cdf->sll.n-1); i++) {
    /* if 2 aligned blocks on query sequence are constrained to adjacent core blocks */
    if ((qsq->sac.mn[i] > 0) && (qsq->sac.mn[i+1] > 0)) {
      /* calculate loop distance of query sequence */
      dist = (qsq->sac.mn[i+1] - cdf->sll.nomn[i+1]) - (qsq->sac.mn[i] + cdf->sll.comn[i]) - 1;
      /* if 1.5 times this dist is greater than the loop constraint, then relax the loop constraint */
      dist = (int) ((double)dist * 1.5);
      if (dist > cdf->lll.llmx[i+1]) {
        cdf->lll.llmx[i+1] = dist;
      }
    }
  }


/*----------------------------------------------------------------------------*/
/*  if 2 adjacent aligned regions on the query sequence are constrained to    */
/*  match 2 core regions that are separated by one intervening core region,   */
/*  then make sure the loop constraints are sufficiently big.                 */
/*  this over-rides the automatically determined loop constraints.            */
/*----------------------------------------------------------------------------*/
  /* for each pair of core blocks separated by a core block */
  for (i=0; i<(cdf->sll.n-2); i++) {
    /* if 2 adjacent aligned blocks on the query sequence are constrained */
    /* to 2 core blocks separated by an intervening core block */
    if ((qsq->sac.mn[i] > 0) && (qsq->sac.mn[i+1] <= 0) && (qsq->sac.mn[i+2] > 0)) {
      /* calculate loop distance of query sequence */
      dist = (qsq->sac.mn[i+2] - cdf->sll.nomn[i+2]) - (qsq->sac.mn[i] + cdf->sll.comn[i]) - 1;
      /* calculate length of intervening aligned block */
      dist2 = cdf->sll.nomn[i+1] + cdf->sll.comn[i+1] + 1;
      /* relax loop constraints between 2 core blocks and intervening core block */
      if ((dist - dist2) > cdf->lll.llmx[i+1]) {
        cdf->lll.llmx[i+1] = dist - dist2;
      }
      if ((dist - dist2) > cdf->lll.llmx[i+2]) {
        cdf->lll.llmx[i+2] = dist - dist2;
      }
    }
  }


/*----------------------------------------------------------------------------*/
/* Retrieve parameters                                                        */
/*----------------------------------------------------------------------------*/
  nsc=cdf->sll.n;
  nmt=mtf->n;
  nlp=cdf->lll.n;
  /*nrr=mtf->rrc.n;*/
  /*nrp=mtf->rpc.n;*/
  nrt=pmf->nrt;
  ndi=pmf->ndi;

#ifdef ATD_DEBUG
  printf("nsc %d\n",nsc);
  printf("nmt %d\n",nmt);
  printf("nlp %d\n",nlp);
  /*printf("nrr %d\n",nrr);*/
  /*printf("nrp %d\n",nrp);*/
  printf("nrt %d\n",nrt);
  printf("ndi %d\n",ndi);
#endif


/*----------------------------------------------------------------------------*/
/* Allocate storage for threading data structures                             */
/*----------------------------------------------------------------------------*/
  sli=calloc(1,sizeof(Cur_Loc));
  sai=calloc(1,sizeof(Cur_Aln));
  spn=calloc(1,sizeof(Seg_Nsm));
  spc=calloc(1,sizeof(Seg_Cmp));
  cxe=calloc(1,sizeof(Thd_Cxe));
  spe=calloc(1,sizeof(Seg_Gsm));
  tdg=calloc(1,sizeof(Thd_Gsm));
  cpr=calloc(nsc,sizeof(Cxl_Los *));
  for(i=0;i<nsc;i++) cpr[i]=calloc(1,sizeof(Cxl_Los));
  cpa=calloc(nsc,sizeof(Cxl_Als *));
  for(i=0;i<nsc;i++) cpa[i]=calloc(1,sizeof(Cxl_Als));
  cpl=calloc(nsc,sizeof(Cxl_Los *));
  for(i=0;i<nsc;i++) cpl[i]=calloc(1,sizeof(Cxl_Los));
  sgo=calloc(1,sizeof(Seg_Ord));
  pvl=calloc(1,sizeof(Rnd_Smp));
  tts=calloc(1,sizeof(Thd_Tst));

#ifdef ATD_DEBUG
  printf("allocated threading data structures\n");
#endif


/*----------------------------------------------------------------------------*/
/* Allocate storage for current segment locations, type Cur_Loc               */
/*----------------------------------------------------------------------------*/
  sli->nsc=nsc;
  sli->nlp=nlp;
  sli->nmt=nmt;
  sli->no=(int *)calloc(nsc,sizeof(int));
  sli->co=(int *)calloc(nsc,sizeof(int));
  sli->lp=(int *)calloc(nsc+1,sizeof(int));
  sli->cr=(int *)calloc(nmt,sizeof(int));

#ifdef ATD_DEBUG
printf("sli->nsc:%d nlp:%d nmt%d\n",sli->nsc,sli->nlp,sli->nmt);
for(i=0;i<nsc;i++) printf("%d ",sli->no[i]); printf("sli->no\n");
for(i=0;i<nsc;i++) printf("%d ",sli->co[i]); printf("sli->co\n");
for(i=0;i<nlp;i++) printf("%d ",sli->lp[i]); printf("sli->lp\n");
for(i=0;i<nmt;i++) printf("%d ",sli->cr[i]); printf("sli->cr\n");
#endif

/* Allocate storage for current segment alignment and sequence, type Cur_Aln */
sai->nsc=nsc;
sai->nmt=nmt;
sai->al=(int *)calloc(nsc,sizeof(int));
sai->sq=(int *)calloc(nmt,sizeof(int));
sai->cf=(int *)calloc(nmt,sizeof(int));

#ifdef ATD_DEBUG
printf("sai->nsc:%d nmt%d\n",sai->nsc,sai->nmt);
for(i=0;i<nsc;i++) printf("%d ",sai->al[i]); printf("sai->al\n");
for(i=0;i<nmt;i++) printf("%d ",sai->sq[i]); printf("sai->sq\n");
#endif


/* Allocate storage for current segment pair energies, type Seg_Gsm */
spe->nsc=nsc;
spe->gss=calloc(nsc,sizeof(int *));
	for(i=0;i<nsc;i++) spe->gss[i]=calloc(nsc,sizeof(int));
spe->gs=calloc(nsc,sizeof(int));
spe->ms=calloc(nsc,sizeof(int));
spe->cs=calloc(nsc,sizeof(int));
spe->ls=calloc(nsc,sizeof(int));
spe->s0=calloc(nsc,sizeof(int));

#ifdef ATD_DEBUG
for(i=0; i<nsc; i++) printf("%d ",spe->gs[i]); printf("spe->gs\n");
for(j=0; j<nsc; j++) {
	for(i=0; i<nsc; i++) printf("%d ",spe->gss[j][i]);
	printf("spe->gss[%d]\n",j); }
#endif

/* Allocate storage for current segment pair contact counts, type Seg_Nsm */
spn->ndi=ndi;
spn->nsc=nsc;
spn->nrt=nrt;
spn->nrr=calloc(ndi,sizeof(int **));
	for(i=0; i<ndi; i++) spn->nrr[i]=calloc(nsc,sizeof(int *));
	for(i=0; i<ndi; i++) for (j=0; j<nsc; j++)
		spn->nrr[i][j]=calloc(nsc,sizeof(int));
spn->srr=calloc(ndi,sizeof(int));
spn->nrp=calloc(ndi,sizeof(int **));
	for(i=0; i<ndi; i++) spn->nrp[i]=calloc(nsc,sizeof(int *));
	for(i=0; i<ndi; i++) for (j=0; j<nsc; j++)
		spn->nrp[i][j]=calloc(nsc,sizeof(int));
spn->srp=calloc(ndi,sizeof(int));
spn->nrf=calloc(ndi,sizeof(int **));
	for(i=0; i<ndi; i++) spn->nrf[i]=calloc(nsc,sizeof(int *));
	for(i=0; i<ndi; i++) for (j=0; j<nsc; j++)
		spn->nrf[i][j]=calloc(nrt,sizeof(int));
spn->frf=calloc(ndi,sizeof(int *));
for(i=0;i<ndi;i++) spn->frf[i]=calloc(nrt,sizeof(int));
spn->srf=calloc(ndi,sizeof(int));

#ifdef ATD_DEBUG
printf("spn->ndi:%d nsc:%d nrt%d\n",spn->ndi,spn->nsc,spn->nrt);
for(k=0; k<ndi; k++) for(j=0; j<nsc; j++) {
	for(i=0; i<nsc; i++) printf("%d ",spn->nrr[k][j][i]);
	printf("spn->nrr[%d][%d]\n",k,j); }
for(i=0; i<ndi; i++) printf("%d ",spn->srr[i]); printf("spn->srr\n");
for(k=0; k<ndi; k++) for(j=0; j<nsc; j++) {
	for(i=0; i<nsc; i++) printf("%d ",spn->nrp[k][j][i]);
	printf("spn->nrp[%d][%d]\n",k,j); }
for(i=0; i<ndi; i++) printf("%d ",spn->srp[i]); printf("spn->srp\n");
for(k=0; k<ndi; k++) for(j=0; j<nsc; j++) {
	for(i=0; i<nrt; i++) printf("%d ",spn->nrf[k][j][i]);
	printf("spn->nrf[%d][%d]\n",k,j); }
for(j=0; j<ndi; j++) {
	for(i=0;i<nrt;i++) printf("%d ",spn->frf[j][i]);
	printf("spn->frf[%d]\n",j); }
for(i=0; i<ndi; i++) printf("%d ",spn->srf[i]); printf("spn->srf\n");
#endif

/* Allocate storage for threaded segment compostion, type Seg_Cmp */
spc->nsc=nsc;
spc->nrt=nrt;
spc->srt=calloc(nsc,sizeof(int *));
	for(i=0; i<nsc; i++) spc->srt[i]=calloc(nrt,sizeof(int));
spc->nlp=nlp;
spc->lrt=calloc(nlp,sizeof(int *));
	for(i=0; i<nlp; i++) spc->lrt[i]=calloc(nrt,sizeof(int));
spc->rt=calloc(nrt,sizeof(int));
spc->rto=calloc(nrt,sizeof(int));

#ifdef ATD_DEBUG
printf("spc->nsc:%d nrt:%d nlp:%d\n",spc->nsc,spc->nrt,spc->nlp);
for(j=0; j<nsc; j++) {
	for(i=0;i<nrt;i++) printf("%d ",spc->srt[j][i]);
	printf("spc->srt[%d]\n",j); }
for(j=0; j<nlp; j++) {
	for(i=0;i<nrt;i++) printf("%d ",spc->lrt[j][i]);
	printf("spc->lrt[%d]\n",j); }
for(i=0; i<nrt; i++) printf("%d ",spc->rt[i]); printf("spc->rt\n");
for(i=0; i<nrt; i++) printf("%d ",spc->rto[i]); printf("spc->rto\n");
#endif

/* Allocate storage for expected contact energies, type Thd_Cxe */
cxe->nrt=nrt;
cxe->ndi=ndi;
cxe->rp=calloc(nrt,sizeof(float));
cxe->rrp=calloc(nrt,sizeof(float *));
	for(i=0; i<nrt; i++) cxe->rrp[i]=calloc(i+1,sizeof(float));
cxe->rre=calloc(ndi,sizeof(float));
cxe->rpe=calloc(ndi,sizeof(float));
cxe->rfe=calloc(ndi,sizeof(float));

#ifdef ATD_DEBUG
printf("cxe->nrt:%d ndi:%d\n",cxe->nrt,cxe->ndi);
for(i=0; i<nrt; i++) printf("%.4f ",cxe->rp[i]); printf("cxe->rp\n");
for(j=0; j<nrt; j++) {
	for(i=0;i<nrt;i++) printf("%.4f ",cxe->rrp[j][i]);
	printf("cxe->rrp[%d]\n",j); }
for(i=0; i<ndi; i++) printf("%.4f ",cxe->rre[i]); printf("cxe->rre\n");
for(i=0; i<ndi; i++) printf("%.4f ",cxe->rpe[i]); printf("cxe->rpe\n");
for(i=0; i<ndi; i++) printf("%.4f ",cxe->rfe[i]); printf("cxe->rfe\n");
#endif

/* Allocate storage for segment sampling order, type Seg_Ord */
sgo->nsc=nsc;
sgo->si=(int *)calloc(nsc,sizeof(int));
sgo->so=(int *)calloc(nsc,sizeof(int));
sgo->to=(int *)calloc(nsc,sizeof(int));

#ifdef ATD_DEBUG
printf("sgo->nsc:%d\n",sgo->nsc);
for(i=0; i<nsc; i++) printf("%d ",sgo->si[i]); printf("sgo->si\n");
for(i=0; i<nsc; i++) printf("%d ",sgo->so[i]); printf("sgo->so\n");
for(i=0; i<nsc; i++) printf("%d ",sgo->to[i]); printf("sgo->to\n");
#endif


/* Allocate storage for random sampling, type Rnd_Smp */
/* Need at most one value per residue in the query sequence */
pvl->p=(float *)calloc(qsq->n,sizeof(float));
pvl->e=(float *)calloc(qsq->n,sizeof(float));
pvl->sq=(int *)calloc(qsq->n,sizeof(int));
pvl->aqi=(int *)calloc(qsq->n,sizeof(int));
pvl->r=(int *)calloc(qsq->n,sizeof(int));
pvl->o=(int *)calloc(qsq->n,sizeof(int));
pvl->lsg=(int *)calloc(nsc+1,sizeof(int));


/* Allocate storage for local minima and convergence tests */
tts->nw=ttb->n;
tts->bw=(float *)calloc(tts->nw,sizeof(float));	/* For Boltzmann weights */
tts->nb=0; for(i=0; i<gsp->nts; i++) {
	if(tts->nb<gsp->nti[i]) tts->nb=gsp->nti[i];
#ifdef ATD_DEBUG
	 printf("nts=%d nti=%d tts->nb=%d\n",i,gsp->nti[i],tts->nb);
#endif
	}
tts->ib=(float *)calloc(tts->nb,sizeof(float)); /* Best energy per iteration */


/* For each core segment identify all possible contact pairs */
cprl(mtf,cdf,pmf,cpr,0);

#ifdef ATD_DEBUG
  for(i=0; i<nsc; i++) {
	cr=cpr[i];
	printf("rr:%d rp:%d rf:%d cpr[%d]\n",cr->rr.n,cr->rp.n,cr->rf.n,i); }
#endif


/* Allocate storage for master contact lists, type Cxl_Los */
for(i=0; i<nsc; i++) {
	cr=cpr[i];
	cr->rr.r1=calloc(cr->rr.n,sizeof(int));
	cr->rr.r2=calloc(cr->rr.n,sizeof(int));
	cr->rr.d=calloc(cr->rr.n,sizeof(int));
	cr->rp.r1=calloc(cr->rp.n,sizeof(int));
	cr->rp.p2=calloc(cr->rp.n,sizeof(int));
	cr->rp.d=calloc(cr->rp.n,sizeof(int));
	if(cr->rf.n>0) {cr->rf.r1=calloc(cr->rf.n,sizeof(int));
			cr->rf.t2=calloc(cr->rf.n,sizeof(int));
			cr->rf.d=calloc(cr->rf.n,sizeof(int)); } }

/* Construct master contact lists */
cprl(mtf,cdf,pmf,cpr,1);

#ifdef ATD_DEBUG
for(i=0; i<nsc; i++) { 
	cr=cpr[i];
	printf("rr:%d rp:%d rf:%d cpr[%d]\n",cr->rr.n,cr->rp.n,cr->rf.n,i); }
#endif

/* Allocate storage for extent-sampling contact lists, type Cxl_Los */
for(i=0; i<nsc; i++) {
	cl=cpl[i]; cr=cpr[i];
	cl->rr.r1=calloc(cr->rr.n,sizeof(int));
	cl->rr.r2=calloc(cr->rr.n,sizeof(int));
	cl->rr.d=calloc(cr->rr.n,sizeof(int));
	cl->rr.e=calloc(cr->rr.n,sizeof(int));
	cl->rp.r1=calloc(cr->rp.n,sizeof(int));
	cl->rp.p2=calloc(cr->rp.n,sizeof(int));
	cl->rp.d=calloc(cr->rp.n,sizeof(int));
	cl->rp.e=calloc(cr->rp.n,sizeof(int));
	cl->rf.r1=calloc(cr->rf.n,sizeof(int));
	cl->rf.t2=calloc(cr->rf.n,sizeof(int));
	cl->rf.d=calloc(cr->rf.n,sizeof(int));
	cl->rf.e=calloc(cr->rf.n,sizeof(int)); }


/* Allocate storage for alignment-sampling contact lists, type Cxl_Als */
for(i=0; i<nsc; i++) {
	ca=cpa[i];
	n=cpr[i]->rr.n;
	ca->rr.r1=calloc(n,sizeof(int));
	ca->rr.r2=calloc(n,sizeof(int));
	ca->rr.e=calloc(n,sizeof(int *));
	n=cdf->sll.nomx[i]+cdf->sll.comx[i]+1;
	ca->r.r=calloc(n,sizeof(int));
	ca->r.e=calloc(n,sizeof(int *));
		for(j=0;j<n;j++) ca->r.e[j]=calloc(nrt,sizeof(int));}


/* Initialize pseudo-random number generation. */
/* srand48((long)gsp->rsd); */
/* RandomSeed((long)gsp->rsd); */
Rand01(&gsp->rsd);

/* Initialize linked list for storage of top threads */
ttb0(ttb);


/* Initialize trajectory record counter */
ngs=0;

#ifdef ATD_DEBUG
printf("ttb->n: %d mx: %d mn: %d\n",ttb->n,ttb->mx,ttb->mn);
for(i=0; i<ttb->n; i++) printf("i:%d tg:%.4f tf:%d pr:%d nx:%d\n",
	i,ttb->tg[i],ttb->tf[i],ttb->pr[i],ttb->nx[i]);
#endif

/* ----------------------------------------------------------------- */
/* Begin sampling.  Loop over the number of random starts specified. */
/* ----------------------------------------------------------------- */

for(nrs=0;nrs<gsp->nrs;nrs++) {
#ifdef ATD_DEBUG
  printf("nrs:%d\n",nrs);
#endif

/* Choose the initial location and extent of core elements */

/* Flag all core segments as missing extent assignment */
for(i=0; i<nsc; i++) { sli->no[i]=-1; sli->co[i]=-1; }

/* Flag all core residue positions as not assigned to any core segment */
for(i=0; i<nmt; i++) sli->cr[i]=-1;

#ifdef ATD_DEBUG
for(i=0;i<nsc;i++) printf("%d ",sli->no[i]); printf("sli->no\n");
for(i=0;i<nsc;i++) printf("%d ",sli->co[i]); printf("sli->co\n");
for(i=0;i<nlp;i++) printf("%d ",sli->lp[i]); printf("sli->lp\n");
for(i=0;i<nmt;i++) printf("%d ",sli->cr[i]); printf("sli->cr\n");
#endif

/* Determine the order of segment location. */ 
sgoi(gsp->iso,gsp->ito,pvl,sgo);
/* sgoi(1,2,pvl,sgo); */

#ifdef ATD_DEBUG
printf("gsp->iso: %d gsp->ito: %d\n",gsp->iso,gsp->ito);
for(i=0;i<nsc;i++) printf("%d ",sgo->si[i]); printf("sgo->si\n");
for(i=0;i<nsc;i++) printf("%d ",sgo->so[i]); printf("sgo->so\n");
for(i=0;i<nsc;i++) printf("%d ",sgo->to[i]); printf("sgo->to\n");
#endif

/* Loop over core segments and terminii */
for(i=0; i<nsc; i++) { 
	cs=sgo->si[i];
	/*rf=cdf->sll.rfpt[cs];*/
#ifdef ATD_DEBUG
	/*printf("cs:%d rf:%d\n",cs,rf);*/
#endif
	for(j=0;j<=1;j++) { 

		switch(j) {	case 0: { ct=sgo->to[cs]; 
						break; }
				case 1: { ct=(sgo->to[cs]==0) ? 1:0; 
						break; } 
				}
	
		/* Find allowed extent range for current terminus */
		/* printf("ct:%d\n",ct); */
		if(!slo0(mtf,cdf,qsq,sli,cs,ct,&mn,&mx)) {
			printf("slo0 failed at nrs:%d cs:%d ct:%d\n",nrs,cs,ct);
			return(0); }
		/* printf("cs:%d ct:%d mn:%d mx:%d\n",cs,ct,mn,mx); */

		/* Assign extent, using the method specified */
		switch(gsp->isl) { 

			case 1: { 	/* Assign minimum extent */
				switch(ct)  {
					case 0: { 
						sli->no[cs]=cdf->sll.nomn[cs]; 
						break; }
					case 1: { 
						sli->co[cs]=cdf->sll.comn[cs]; 
						break;} 
					} 
				break; 
				}

			default: { 	/* Choose extent at random */
				switch(ct) {
					case 0: {
						pvl->n=mx-mn+1; 
						for(k=0;k<pvl->n;k++) 
							pvl->p[k]=1./pvl->n;
						sli->no[cs]=mn+rsmp(pvl);
						break; }
					case 1: {	
						pvl->n=mx-mn+1; 
						for(k=0;k<pvl->n;k++) 
							pvl->p[k]=1./pvl->n;
						sli->co[cs]=mn+rsmp(pvl); 
						break; } 
					}
				break; 
				}

			case 2:	{	/* Choose maximum extent */
				switch(ct) {
					case 0: {

/* disable this test since the assignment algorithm is based on alignment of 
   minimum extent core elements	*/	/*	if(mx<cdf->sll.nomx[cs]) {
		printf( "over maximum extent nrs:%d cs:%d ct:0\n",nrs,cs);
		return(0); } */

						sli->no[cs]=cdf->sll.nomx[cs]; 
						break; }
					case 1: {
					
					/*	if(mx<cdf->sll.comx[cs]) {
		printf("over maximum extent nrs:%d cs:%d ct:1\n",nrs,cs);
		return(0); } */

						sli->co[cs]=cdf->sll.comx[cs]; 
						break; }
					}
				break;	
				} 
			} 

		}
	}

/* for(k=0;k<nsc;k++) printf("%d ",sli->no[k]);printf("sli->no\n");
for(k=0;k<nsc;k++) printf("%d ",sli->co[k]);printf("sli->co\n");*/ 

/* Store the initial location of the core segments */
for(i=0;i<nsc;i++) {
	mn=cdf->sll.rfpt[i]-sli->no[i];
	mx=cdf->sll.rfpt[i]+sli->co[i];
	for(j=mn; j<=mx; j++) sli->cr[j]=i; 
	}

/* for(i=0;i<nmt;i++) printf("%d ",sli->cr[i]); printf("sli->cr\n"); */

/* Store the initial motif-derived loop lengths */
for(i=1;i<nsc;i++) {
	mn=cdf->sll.rfpt[i-1]+sli->co[i-1];
	mx=cdf->sll.rfpt[i]-sli->no[i];
	sli->lp[i]=mtf->mll[mn][mx]; }
/* for(i=0;i<nlp;i++) printf("%d ",sli->lp[i]); printf("sli->lp\n"); */

/* Choose the initial alignment of the query sequence and core motif */

/* Flag all core segments as unaligned */
for(i=0; i<nsc; i++) sai->al[i]=-1;

/* Determine the order of segment alignment. */ 
sgoi(gsp->iso,gsp->ito,pvl,sgo);

/* Randomly align core segments in the order determined */
for(i=0; i<nsc; i++) { cs=sgo->si[i]; 
	
	/* Find alignment constraints arising from previously aligned */
	/* core segment, explicitly constrained core segments, and/or  */
	/* consideration of the lengths of the query sequence and core */
	/* segments and length ranges for loops. */
	if(!sal0(cdf,qsq,sli,sai,cs,&mn,&mx)) {
		printf("failed sal0 nrs:%d cs:%d\n",nrs,cs); 
		return(0);}	
	/* printf("cs:%d mn:%d mx:%d\n",cs,mn,mx); */

	/* Choose an alignment at random, given the min/max constraints */
	pvl->n=mx-mn+1;
	for(j=0;j<pvl->n;j++) pvl->p[j]=1./pvl->n;
	sai->al[cs]=mn+rsmp(pvl);
	}
/* for(k=0;k<nsc;k++) printf("%d ",sai->al[k]);printf("sai->al\n"); */

/* Assign the threaded sequence, i.e. the residue types aligned with each */
/* residue position in the current core model. These are stored in sai->sq. */

for(i=0; i<nsc; i++) {
	mn=sai->al[i]-sli->no[i];
	mx=sai->al[i]+sli->co[i];
	j=cdf->sll.rfpt[i]-sli->no[i];
	for(k=mn; k<=mx; k++) {
		sai->sq[j]=qsq->sq[k];
		j++;} }


/* Compute threaded sequence compostion. */
for(i=0; i<nsc; i++) spci(cdf,qsq,sli,sai,i,spc); 
/* for(i=0;i<nrt;i++) printf("%d ",spc->rt[i]); printf("spc->rt\n"); */

/* 
for(i=0;i<nsc;i++) {
	for(j=0;j<nrt;j++) printf("%d ",spc->srt[i][j]); 
	printf("spc->srt[%d]\n",i); }
for(i=0;i<nlp;i++) {
	for(j=0;j<nrt;j++) printf("%d ",spc->lrt[i][j]); 
	printf("spc->lrt[%d]\n",i); }
for(i=0;i<nrt;i++) printf("%d ",spc->rt[i]); printf("spc->rt\n");
for(i=0;i<nrt;i++) printf("%d ",spc->rto[i]); printf("spc->rto\n");
*/

/* Compute contact counts. */
for(i=0; i<nsc; i++) spni(cpr,sli,i,spn);

/*
printf("spn->ndi:%d nsc:%d nrt%d\n",spn->ndi,spn->nsc,spn->nrt);
for(k=0; k<ndi; k++) for(j=0; j<nsc; j++) { 
	for(i=0; i<nsc; i++) printf("%d ",spn->nrr[k][j][i]); 
	printf("spn->nrr[%d][%d]\n",k,j); }
for(i=0; i<ndi; i++) printf("%d ",spn->srr[i]); printf("spn->srr\n");
for(k=0; k<ndi; k++) for(j=0; j<nsc; j++) { 
	for(i=0; i<nsc; i++) printf("%d ",spn->nrp[k][j][i]); 
	printf("spn->nrp[%d][%d]\n",k,j); }
for(i=0; i<ndi; i++) printf("%d ",spn->srp[i]); printf("spn->srp\n");
for(k=0; k<ndi; k++) for(j=0; j<nsc; j++) { 
	for(i=0; i<nrt; i++) printf("%d ",spn->nrf[k][j][i]); 
	printf("spn->nrf[%d][%d]\n",k,j); }
for(j=0; j<ndi; j++) {
	for(i=0;i<nrt;i++) printf("%d ",spn->frf[j][i]);
	printf("spn->frf[%d]\n",j); }
for(i=0; i<ndi; i++) printf("%d ",spn->srf[i]); printf("spn->srf\n");
*/

/* Compute expected contact energy. */
cxei(spn,spc,pmf,sli,psm,cdf,cxe);


/* printf("cxe->nrt:%d ndi:%d\n",cxe->nrt,cxe->ndi);
for(i=0; i<nrt; i++) printf("%.4f ",cxe->rp[i]); printf("cxe->rp\n");
for(j=0; j<nrt; j++) {
	for(i=0;i<nrt;i++) printf("%.4f ",cxe->rrp[j][i]);
	printf("cxe->rrp[%d]\n",j); } 
for(k=0; k<ndi; k++) printf("%.4f ",cxe->rre[k]); printf("cxe->rre\n");
for(k=0; k<ndi; k++) printf("%.4f ",cxe->rpe[k]); printf("cxe->rpe\n");
for(k=0; k<ndi; k++) printf("%.4f ",cxe->rfe[i]); printf("cxe->rfe\n");  */

/* Construct contact pair lists for extent sampling, including energies */
cpll(cdf,pmf,qsq,cpr,sai,cpl);

/* Compute partial contact energy sums using the extent sampling lists. */
for(i=0; i<nsc; i++) spel(cpl,sai,sli,i,spe,cdf,psm,spc);

/* for(k=0; k<nsc; k++) printf("%d ",spe->gs[k]); printf("spe->gs\n");
for(k=0; k<nsc; k++) { 
	for(l=0; l<nsc; l++) printf("%d ",spe->gss[k][l]); 
	printf("spe->gss[%d]\n",k); } */

/* Compute thread energy for the initial model */
dgri(spe,spn,cxe,tdg,psm,spc); 
/*printf("g:%d g0:%.2f dg:%.2f\n",tdg->g, tdg->g0, tdg->dg); */

/* Push the initial model onto the stack */
ttbi(sai,sli,tdg,ttb,nrs,gsp->als);

/* If indicated, record this value in the trajectory */
if(gsp->trg!=0) { trg[ngs]=tdg->dg; ngs++; }

/* 
for(k=0;k<nsc;k++) printf("%d ",sai->al[k]); printf("sai->al\n");
for(k=0;k<nmt;k++) printf("%d ",sai->sq[k]); printf("sai->sq\n");

printf("ttb->n: %d mx: %d mn: %d\n",ttb->n,ttb->mx,ttb->mn);
for(k=0; k<ttb->n; k++) {
	printf("k:%d tg:%.4f tf:%d ",k,ttb->tg[k],ttb->tf[k]);
	printf("al: "); for(l=0; l<sli->nsc; l++) printf("%d ",ttb->al[l][k]);
	printf("no: "); for(l=0; l<sli->nsc; l++) printf("%d ",ttb->no[l][k]);
	printf("co: "); for(l=0; l<sli->nsc; l++) printf("%d ",ttb->co[l][k]);
	printf("nx:%d pr:%d\n",ttb->nx[k],ttb->pr[k]);  }
*/

/* Loop over the specified number of Gibb's sampling iterations. For each, */
/* perform the specified number of alignment and location sampling cycles, */
/* and push the resulting threads onto the best threads stack. */

/* Iterations may follow an annealing schedule as specified by temperature */
/* parameters associated with that iteration.  The loop over iterations is */
/* therefore nested according to the number of temperature changes, and the */
/* number of iterations to conduct at each temperature.  The temperature */
/* parameters for alignment and location cycles are specified separately. */

for(nts=0;nts<gsp->nts;nts++) { 
	/* printf("nts:%d\n",nts); */

	for(nti=0;nti<gsp->nti[nts];nti++) {

	/* Initialize storage for lowest energy found in this iteration */
	tts->ib[nti]=BIGNEG;

	if(gsp->nac[nts]>0) {

	/* Construct contact pair lists for alignment sampling */
	cpal(pmf,cpr,sli,cpa);

	/* Initialize partial energy sums for alignment sampling */
	for(i=0; i<nsc; i++) spea(cdf,cpa,sai,sli,i,1,spe,psm,spc);

	/* Perform the indicated number of alignment cycles. In each cycle */
	/* a new alignment for each core segments is chosen by the Gibbs */
	/* sampling algorithm, conditioned on the current extent of core */
	/* elements in the folding motif. */
	
	for(nac=0;nac<gsp->nac[nts];nac++) {
		/* printf("nac:%d\n",nac); */

		/* Determine order for segment alignment. */ 
		sgoi(gsp->iso,gsp->ito,pvl,sgo);

		/* Loop over core segments. */
		for(i=0; i<nsc; i++) { cs=sgo->si[i]; 
			
			/* Find allowed alignment range for current segment */
			if(salr(cdf,qsq,sli,sai,cs,&mn,&mx)==0) {
			printf("failed salr nrs:%d, nts:%d nti:%d cs:%d\n",
				nrs,nts,nti,cs);

for(i=0;i<nsc;i++) printf("%d ",sai->al[i]); printf("sai->al\n");
for(i=0;i<sai->nmt;i++) printf("%d ",sai->sq[i]); printf("sai->sq\n");

for(i=0;i<nsc;i++) printf("%d ",sli->no[i]); printf("sli->no\n");
for(i=0;i<nsc;i++) printf("%d ",sli->co[i]); printf("sli->co\n");
for(i=0;i<sli->nlp;i++) printf("%d ",sli->lp[i]); printf("sli->lp\n");
for(i=0;i<sli->nmt;i++) printf("%d ",sli->cr[i]); printf("sli->cr\n");

				return(0); }


			/* Loop over values of the aligment variable. */
			pvl->n=mx-mn+1;
			/* printf("cs:%d mn:%d mx:%d\n",cs,mn,mx); */
			for(j=0; j<=mx-mn; j++) { al=mn+j;
		
				/* Assign the alignment. */
				salu(cdf,qsq,sli,cs,al,sai);
				/* printf("assigned cs:%d al:%d\n",cs,al); */

				/* Update contact energies. */
				spea(cdf,cpa,sai,sli,cs,1,spe,psm,spc);
				/* printf("updated energies cs:%d\n",cs); */

				/* Update composition of threaded sequence */
				spcd=spci(cdf,qsq,sli,sai,cs,spc);
				/* printf("spcd:%d\n",spcd); */

				/* If necessary update expected energy. */
				if(spcd!=0) cxei(spn,spc,pmf,sli,psm,cdf,cxe);

				/* Assign thread energy for this aligment. */
				pvl->e[j]=dgri(spe,spn,cxe,tdg,psm,spc); 
				}

			
			/* Calculate probabilities and choose an alignment. */
			al=mn+algs(pvl,gsp->tma[nts]);
			
			/* Assign the chosen alignment for this segment. */
			salu(cdf,qsq,sli,cs,al,sai);
			/* printf("choose by sampling cs:%d al:%d\n",cs,al); */

			/* Update contact energies. */
			spea(cdf,cpa,sai,sli,cs,1,spe,psm,spc);

			/* Update composition. */
			spcd=spci(cdf,qsq,sli,sai,cs,spc);

			/* If necessary update expected energies. */
			if(spcd) cxei(spn,spc,pmf,sli,psm,cdf,cxe);

			/* Assign thread energy */
			dgri(spe,spn,cxe,tdg,psm,spc); 
			}


		/* Push the sampled thread onto the stack. */
		ttbi(sai,sli,tdg,ttb,nrs,gsp->als);
		
		/* Record the lowest energy found in this iteration */
		if(tdg->dg>tts->ib[nti]) tts->ib[nti]=tdg->dg;

		/* If indicated, record this value in the trajectory */
		if(gsp->trg!=0) { trg[ngs]=tts->ib[nti]; ngs++; }

		} }

/* printf("After alignment cycle\n");

for(k=0;k<nsc;k++) printf("%d ",sai->al[k]); printf("sai->al\n");
for(k=0;k<nmt;k++) printf("%d ",sai->sq[k]); printf("sai->sq\n");

for(k=0; k<nsc; k++) printf("%d ",spe->gs[k]); printf("spe->gs\n");
for(k=0; k<nsc; k++) { 
	for(l=0; l<nsc; l++) printf("%d ",spe->gss[k][l]); 
	printf("spe->gss[%d]\n",k); } 
printf("nts:%d nti:%d\n",nts,nti);
printf("ps:%.2f ms:%.2f dg:%.2f\n",tdg->ps, tdg->ms, tdg->dg);

printf("ttb->n: %d mx: %d mn: %d\n",ttb->n,ttb->mx,ttb->mn);
for(k=0; k<ttb->n; k++) {
	printf("k:%d tg:%.4f tf:%d ",k,ttb->tg[k],ttb->tf[k]);
	printf("al: "); for(l=0; l<sli->nsc; l++) printf("%d ",ttb->al[l][k]);
	printf("no: "); for(l=0; l<sli->nsc; l++) printf("%d ",ttb->no[l][k]);
	printf("co: "); for(l=0; l<sli->nsc; l++) printf("%d ",ttb->co[l][k]);
	printf("nx:%d pr:%d\n",ttb->nx[k],ttb->pr[k]);  } 
		
printf("spn->ndi:%d nsc:%d nrt%d\n",spn->ndi,spn->nsc,spn->nrt);
for(k=0; k<ndi; k++) for(j=0; j<nsc; j++) { 
	for(i=0; i<nsc; i++) printf("%d ",spn->nrr[k][j][i]); 
	printf("spn->nrr[%d][%d]\n",k,j); }
for(i=0; i<ndi; i++) printf("%d ",spn->srr[i]); printf("spn->srr\n");
for(k=0; k<ndi; k++) for(j=0; j<nsc; j++) { 
	for(i=0; i<nsc; i++) printf("%d ",spn->nrp[k][j][i]); 
	printf("spn->nrp[%d][%d]\n",k,j); }
for(i=0; i<ndi; i++) printf("%d ",spn->srp[i]); printf("spn->srp\n");
for(k=0; k<ndi; k++) for(j=0; j<nsc; j++) { 
	for(i=0; i<nrt; i++) printf("%d ",spn->nrf[k][j][i]); 
	printf("spn->nrf[%d][%d]\n",k,j); }
for(j=0; j<ndi; j++) {
	for(i=0;i<nrt;i++) printf("%d ",spn->frf[j][i]);
	printf("spn->frf[%d]\n",j); }
for(i=0; i<ndi; i++) printf("%d ",spn->srf[i]); printf("spn->srf\n");

printf("spc->nsc:%d nrt:%d nlp:%d\n",spc->nsc,spc->nrt,spc->nlp);
for(j=0; j<nsc; j++) {
	for(i=0;i<nrt;i++) printf("%d ",spc->srt[j][i]);
	printf("spc->srt[%d]\n",j); }
for(j=0; j<nlp; j++) {
	for(i=0;i<nrt;i++) printf("%d ",spc->lrt[j][i]);
	printf("spc->lrt[%d]\n",j); }
for(i=0; i<nrt; i++) printf("%d ",spc->rt[i]); printf("spc->rt\n");
for(i=0; i<nrt; i++) printf("%d ",spc->rto[i]); printf("spc->rto\n");

for(k=0; k<ndi; k++) printf("%.4f ",cxe->rre[k]); printf("cxe->rre\n");
for(k=0; k<ndi; k++) printf("%.4f ",cxe->rpe[k]); printf("cxe->rpe\n");
for(k=0; k<ndi; k++) printf("%.4f ",cxe->rfe[i]); printf("cxe->rfe\n");
*/

	if(gsp->nlc[nts]>0) {

	/* Construct contact pair lists for segment extent sampling. */
	cpll(cdf,pmf,qsq,cpr,sai,cpl);

	/* Initialize partial energy sums for segment extent sampling. */
	for(i=0; i<nsc; i++) spel(cpl,sai,sli,i,spe,cdf,psm,spc);
	
	/* Perform the indicated number of segment extent sampling cycles. */
	/* For each cycle, a new extent for the n- and c-termini of each */
	/* core element is chosen by the Gibbs sampling algorithm, */
	/* conditioned on the current aligment of the query sequence. */
	
	for(nlc=0;nlc<gsp->nlc[nts];nlc++) {

		/* Determine the order of segment location. */ 
		sgoi(gsp->iso,gsp->ito,pvl,sgo);

		/* Loop over core segments */
		for(i=0; i<nsc; i++) { cs=sgo->si[i];
		
		/* Loop over termini */	
		for(j=0;j<=1;j++) { 
		
			switch(j) {	case 0: { ct=sgo->to[cs]; 
						break; }
					case 1: { ct=(sgo->to[cs]==0) ? 1:0; 
						break; } 
					}

			/* Find allowed extent range for current terminus. */
			if(slor(mtf,cdf,qsq,sli,sai,cs,ct,&mn,&mx)==0) {
			printf("failed slor nrs:%d nts:%d nti:%d cs:%d ct:%d\n",
				nrs,nts,nti,cs,ct);

for(k=0;k<nsc;k++) printf("%d ",sai->al[k]); printf("sai->al\n");
for(k=0;k<nmt;k++) printf("%d ",sai->sq[k]); printf("sai->sq\n");

for(i=0;i<nsc;i++) printf("%d ",sli->no[i]); printf("sli->no\n");
for(i=0;i<nsc;i++) printf("%d ",sli->co[i]); printf("sli->co\n");
for(i=0;i<nlp;i++) printf("%d ",sli->lp[i]); printf("sli->lp\n");
for(i=0;i<nmt;i++) printf("%d ",sli->cr[i]); printf("sli->cr\n");

				return(0); }
			/* printf("cs:%d ct:%d mn:%d mx:%d\n",cs,ct,mn,mx); */


			/* Loop over possible extent values. */
			pvl->n=mx-mn+1;
			for(k=0; k<=mx-mn; k++) { of=mn+k;
		
				/* Assign the extent. */
				slou(mtf,cdf,cs,ct,of,sli,sai,qsq);
				/* printf("assigned cs:%d ct:%d of:%d\n",
					cs,ct,of); */

				/* Update contact energies. */
				spel(cpl,sai,sli,cs,spe,cdf,psm,spc);

				/* Update composition. */
				spci(cdf,qsq,sli,sai,cs,spc);

				/* Update contact counts. */
        if (PSSM_Weight < 0.9999999) {
  				spni(cpl,sli,cs,spn);
        }

				/* Update expected contact energy. */
                                cxei(spn,spc,pmf,sli,psm,cdf,cxe);

				/* Assign thread energy for this extent. */
				pvl->e[k]=dgri(spe,spn,cxe,tdg,psm,spc); 
				}

			/* Calculate probabilities and choose an extent. */
			of=mn+algs(pvl,gsp->tml[nts]);

			/* Assign the chosen extent for this terminus. */
			slou(mtf,cdf,cs,ct,of,sli,sai,qsq);
			/* printf("chose by sampling cs:%d ct:%d of:%d\n",
				cs,ct,of);  */

			/* Update contact energies. */
			spel(cpl,sai,sli,cs,spe,cdf,psm,spc);

			/* Update composition. */
			spci(cdf,qsq,sli,sai,cs,spc);

			/* Update contact counts. */
      if (PSSM_Weight < 0.9999999) {
  			spni(cpl,sli,cs,spn);
      }

			/* Update expected contact energy. */
			cxei(spn,spc,pmf,sli,psm,cdf,cxe);

			/* Update thread energy */
			dgri(spe,spn,cxe,tdg,psm,spc); } }

		/* Push the sampled thread onto the stack. */
		ttbi(sai,sli,tdg,ttb,nrs,gsp->als); 

		/* Record the lowest energy found in this iteration */
		if(tdg->dg>tts->ib[nti]) {tts->ib[nti]=tdg->dg;
					  /*hh=tdg->ms;*/}

	        /*printf("%d,%d,%d\n",nrs,nts,nti);
		printf("%f,%f\n",tts->ib[nti],hh);*/

		/* printf("tts->ib[%d]=%.2f\n",nti,tts->ib[nti]); */

		/* If indicated, record this value in the trajectory */
		if(gsp->trg!=0) { trg[ngs]=tts->ib[nti]; ngs++; }

		} }

/*
for(k=0;k<nsc;k++) printf("%d ",sai->al[k]); printf("sai->al\n");
for(k=0;k<nmt;k++) printf("%d ",sai->sq[k]); printf("sai->sq\n");

for(k=0; k<nsc; k++) printf("%d ",spe->gs[k]); printf("spe->gs\n");
for(k=0; k<nsc; k++) { 
	for(l=0; l<nsc; l++) printf("%d ",spe->gss[k][l]); 
	printf("spe->gss[%d]\n",k); }
printf("nts:%d nti:%d\n",nts,nti);
printf("ps:%.2f ms:%.2f dg:%.2f\n",tdg->ps, tdg->ms, tdg->dg);

printf("ttb->n: %d mx: %d mn: %d\n",ttb->n,ttb->mx,ttb->mn);
for(k=0; k<ttb->n; k++) {
	printf("k:%d tg:%.4f tf:%d ",k,ttb->tg[k],ttb->tf[k]);
	printf("al: "); for(l=0; l<sli->nsc; l++) printf("%d ",ttb->al[l][k]);
	printf("no: "); for(l=0; l<sli->nsc; l++) printf("%d ",ttb->no[l][k]);
	printf("co: "); for(l=0; l<sli->nsc; l++) printf("%d ",ttb->co[l][k]);
	printf("nx:%d pr:%d\n",ttb->nx[k],ttb->pr[k]);  } 
		
printf("spn->ndi:%d nsc:%d nrt%d\n",spn->ndi,spn->nsc,spn->nrt);
for(k=0; k<ndi; k++) for(j=0; j<nsc; j++) { 
	for(i=0; i<nsc; i++) printf("%d ",spn->nrr[k][j][i]); 
	printf("spn->nrr[%d][%d]\n",k,j); }
for(i=0; i<ndi; i++) printf("%d ",spn->srr[i]); printf("spn->srr\n");
for(k=0; k<ndi; k++) for(j=0; j<nsc; j++) { 
	for(i=0; i<nsc; i++) printf("%d ",spn->nrp[k][j][i]); 
	printf("spn->nrp[%d][%d]\n",k,j); }
for(i=0; i<ndi; i++) printf("%d ",spn->srp[i]); printf("spn->srp\n");
for(k=0; k<ndi; k++) for(j=0; j<nsc; j++) { 
	for(i=0; i<nrt; i++) printf("%d ",spn->nrf[k][j][i]); 
	printf("spn->nrf[%d][%d]\n",k,j); }
for(j=0; j<ndi; j++) {
	for(i=0;i<nrt;i++) printf("%d ",spn->frf[j][i]);
	printf("spn->frf[%d]\n",j); }
for(i=0; i<ndi; i++) printf("%d ",spn->srf[i]); printf("spn->srf\n");

printf("spc->nsc:%d nrt:%d nlp:%d\n",spc->nsc,spc->nrt,spc->nlp);
for(j=0; j<nsc; j++) {
	for(i=0;i<nrt;i++) printf("%d ",spc->srt[j][i]);
	printf("spc->srt[%d]\n",j); }
for(j=0; j<nlp; j++) {
	for(i=0;i<nrt;i++) printf("%d ",spc->lrt[j][i]);
	printf("spc->lrt[%d]\n",j); }
for(i=0; i<nrt; i++) printf("%d ",spc->rt[i]); printf("spc->rt\n");
for(i=0; i<nrt; i++) printf("%d ",spc->rto[i]); printf("spc->rto\n");

for(k=0; k<ndi; k++) printf("%.4f ",cxe->rre[k]); printf("cxe->rre\n");
for(k=0; k<ndi; k++) printf("%.4f ",cxe->rpe[k]); printf("cxe->rpe\n");
for(k=0; k<ndi; k++) printf("%.4f ",cxe->rfe[i]); printf("cxe->rfe\n");
*/

	/* Test for local minimum within this iteration. Set flag if found. */
	tts->lm=0;
	/* printf("tts->ib[%d]=%.2f mx=%.2f\n", 
		nti,tts->ib[nti],ttb->tg[ttb->mx]); */
	if(gsp->lms[nts]>0 && (nti+1)>=gsp->lms[nts]) {
		tts->gb=BIGNEG; for(i=nti-gsp->lmw[nts]+1;i<=nti;i++) 
			if(tts->gb<tts->ib[i]) tts->gb=tts->ib[i];
		if(tts->gb<(((float)gsp->lmf[nts])/100.)*ttb->tg[ttb->mx]) {
		/* printf("gb:%.2f mx:%.2f\n", tts->gb, ttb->tg[ttb->mx]); */
				tts->lm=1; break; }} 

	/* End of loop over iterations per temperature step */
	}


	/* If local minimum flag is set, stop sampling for this random start */
	if(tts->lm==1) break;
	
	/* End of loop over temperature steps */
	}


/* Test for convergence based on recurrence of the same alignments */

/* bwfi(ttb,gsp,tts); printf("nrs:%d ts:%d tf:%d\n",nrs,tts->ts,tts->tf); */
if(nrs>=gsp->crs) {
	bwfi(ttb,gsp,tts);

	/* Stop if the number of starts and frequency crosses a threshold */
	if(tts->tf>=gsp->cfm && tts->ts>=gsp->csm) break; }


/* End of loop over random starts */
}

/* put results in order, from highest score to lowest */
OrderThdTbl(ttb);

/* Calculate z_scores */
switch(zscs) {
  case 0:
    /* don't calculate any z-scores */
    break;
  case 1:
    /* calculate z-score for top alignment */
    zsc(ttb,psm,qsq,cpr,cdf,pmf,spe,sai,pvl,ScalingFactor);
    break;
  default:
    /* calculate z-scores for all top alignments */
    tmp = ttb->mx;
    for (i=0; i<ttb->n; i++) {
      ttb->mx = i;
      zsc(ttb,psm,qsq,cpr,cdf,pmf,spe,sai,pvl,ScalingFactor);
    }
    ttb->mx = tmp;
    break;
}

/* scale energies back down */
ScaleThdTbl(ttb, 1.0/ScalingFactor);

/*

for(i=0;i<ttb->n;i++) {
	printf("i:%d\n",i);
	printf("ttb->tf:%d",ttb->tf[i]);
	printf("ttb->ts:%d",ttb->ts[i]);
	printf("ttb->ls:%d\n",ttb->ls[i]);
	
	for(j=0;j<nsc;j++){
	printf("ttb->al:%d",ttb->al[j][i]);
	printf("ttb->no:%d",ttb->no[j][i]);
	printf("ttb->co:%dn",ttb->co[j][i]);}

	}

*/

/* Free all allocated storage */

free(sli->no);					/* In sli, type Cur_Loc */
free(sli->co);
free(sli->lp);
free(sli->cr);
free(sli);
/* printf("freed sli\n"); */

free(sai->al);					/* In sai, type Cur_Aln */
free(sai->sq);
free(sai->cf);
free(sai);
/* printf("freed sai\n"); */

free(spe->gs);					/* In spe, type Seg_Gsm */
free(spe->ms);
free(spe->cs);
free(spe->ls);
free(spe->s0);
for(i=0;i<nsc;i++) free(spe->gss[i]); free(spe->gss);
free(spe);
/* printf("freed spe\n"); */

free(spn->srr);					/* In spn, type Seg_Nsm */
for(i=0; i<ndi; i++) 
	for (j=0; j<nsc; j++) free(spn->nrr[i][j]);
for(i=0; i<ndi; i++) free(spn->nrr[i]); free(spn->nrr);
free(spn->srp);
for(i=0; i<ndi; i++) 
	for (j=0; j<nsc; j++) free(spn->nrp[i][j]);
for(i=0; i<ndi; i++) free(spn->nrp[i]); free(spn->nrp);
free(spn->srf);
for(i=0; i<ndi; i++) 
	for (j=0; j<nsc; j++) free(spn->nrf[i][j]);
for(i=0; i<ndi; i++) free(spn->nrf[i]); free(spn->nrf);
for(i=0; i<ndi; i++) free(spn->frf[i]); free(spn->frf);
free(spn);
/* printf("freed spn\n"); */


for(i=0;i<nsc;i++) {				/* In cpr, type Cxl_Los */
	cr=cpr[i];
	free(cr->rr.r1);
	free(cr->rr.r2);
	free(cr->rr.d);
	free(cr->rp.r1);
	free(cr->rp.p2);
	free(cr->rp.d);
	free(cr->rf.r1);
	free(cr->rf.t2);
	free(cr->rf.d); }
for(i=0;i<nsc;i++) free(cpr[i]); 
free(cpr);
/* printf("freed cpr\n"); */


for(i=0;i<nsc;i++) {				/* In cpl, type Cxl_Los */
	cl=cpl[i];
	free(cl->rr.r1);
	free(cl->rr.r2);
	free(cl->rr.d);
	free(cl->rr.e);
	free(cl->rp.r1);
	free(cl->rp.p2);
	free(cl->rp.d);
	free(cl->rp.e);
	free(cl->rf.r1);
	free(cl->rf.t2);
	free(cl->rf.d);
	free(cl->rf.e); }
for(i=0;i<nsc;i++) free(cpl[i]); 
free(cpl);
/* printf("freed cpl\n"); */


for(i=0;i<nsc;i++) {				/* In cpa, type Cxl_Als */
	ca=cpa[i];
	free(ca->rr.r1);
	free(ca->rr.r2);
	free(ca->rr.e);
	free(ca->r.r);
	n=cdf->sll.nomx[i]+cdf->sll.comx[i]+1;
	for(j=0;j<n;j++) free(ca->r.e[j]); 
	free(ca->r.e);}
for(i=0;i<nsc;i++) free(cpa[i]); 
free(cpa);
/* printf("freed cpa\n"); */


free(spc->rt);					/* In spc, type Seg_Cmp */
free(spc->rto);
for(i=0; i<nsc; i++) free(spc->srt[i]);
free(spc->srt);
for(i=0; i<nlp; i++) free(spc->lrt[i]);
free(spc->lrt); 
free(spc);
/* printf("freed spc\n"); */

free(cxe->rp);					/* In cxe, type Thd_Cxe */
for(i=0; i<nrt; i++) free(cxe->rrp[i]);
free(cxe->rrp);
free(cxe->rre);
free(cxe->rpe);
free(cxe->rfe);
free(cxe);
/* printf("freed cxe\n"); */

free(sgo->si);					/* In sgo; type Seg_Ord */
free(sgo->so);
free(sgo->to);
free(sgo);
/* printf("freed sgo\n"); */

free(pvl->p);					/* In pvl; type Rnd_Smp */
free(pvl->e);
free(pvl->sq);
free(pvl->aqi);
free(pvl->r);
free(pvl->o);
free(pvl->lsg);
free(pvl);
/* printf("freed pvl\n"); */

free(tts->bw);					/* In tts; type Thd_Tst */
free(tts->ib);
free(tts);
/* printf("freed tts\n"); */

free(tdg);

return(1);  /* successful completion */
}


Cor_Def*  NewCorDef(int NumBlocks) {
/*----------------------------------------------------------*/
/*  allocate space for a new Cor_Def.                       */
/*----------------------------------------------------------*/
  Cor_Def* cdfp;

  cdfp = (Cor_Def*) calloc(1,sizeof(Cor_Def));
  cdfp->sll.rfpt = (int*) calloc(1,sizeof(int) * NumBlocks);
  cdfp->sll.nomn = (int*) calloc(1,sizeof(int) * NumBlocks);
  cdfp->sll.nomx = (int*) calloc(1,sizeof(int) * NumBlocks);
  cdfp->sll.comn = (int*) calloc(1,sizeof(int) * NumBlocks);
  cdfp->sll.comx = (int*) calloc(1,sizeof(int) * NumBlocks);
  cdfp->sll.n    = NumBlocks;
  cdfp->lll.llmn = (int*) calloc(1,sizeof(int) * (NumBlocks+1));
  cdfp->lll.llmx = (int*) calloc(1,sizeof(int) * (NumBlocks+1));
  cdfp->lll.lrfs = (int*) calloc(1,sizeof(int) * (NumBlocks+1));
  cdfp->lll.n    = NumBlocks+1;
  return(cdfp);
}


Cor_Def*  FreeCorDef(Cor_Def* cdfp) {
/*----------------------------------------------------------*/
/*  free Cor_Def.                                           */
/*----------------------------------------------------------*/
  free(cdfp->sll.rfpt);
  free(cdfp->sll.nomn);
  free(cdfp->sll.nomx);
  free(cdfp->sll.comn);
  free(cdfp->sll.comx);
  free(cdfp->lll.llmn);
  free(cdfp->lll.llmx);
  free(cdfp->lll.lrfs);
  free(cdfp);
  return NULL;
}


void PrintCorDef(Cor_Def* cdf, FILE* pFile) {
/*----------------------------------------------------------*/
/* for debugging.  Print CorDef.                            */
/*----------------------------------------------------------*/
  int    i;
  static Boolean FirstPass = TRUE;

  if (FirstPass == FALSE) return;
  FirstPass = FALSE;

  fprintf(pFile, "Core Definition:\n");
  fprintf(pFile, "number of core segments: %4d\n", cdf->sll.n);
  fprintf(pFile, "   nomx   nomn   rfpt   comn   comx\n");
  for (i=0; i<cdf->sll.n; i++) {
    fprintf(pFile, " %6d %6d %6d %6d %6d\n",
      cdf->sll.nomx[i], cdf->sll.nomn[i], cdf->sll.rfpt[i], cdf->sll.comn[i], cdf->sll.comx[i]);
  }
  fprintf(pFile, "number of loops (one more than core segs): %4d\n", cdf->lll.n);
  fprintf(pFile, "   llmn   llmx   lrfs\n");
  for (i=0; i<cdf->lll.n; i++) {
    fprintf(pFile, " %6d %6d %6d\n", cdf->lll.llmn[i], cdf->lll.llmx[i], cdf->lll.lrfs[i]);
  }
  fprintf(pFile, "number of fixed segments: %4d\n", cdf->fll.n);
  fprintf(pFile, "     nt     ct     sq\n");
  for (i=0; i<cdf->fll.n; i++) {
    fprintf(pFile, " %6d %6d %6d\n", cdf->fll.nt[i], cdf->fll.ct[i], cdf->fll.sq[i]);
  }
  fprintf(pFile, "\n");
}


Seq_Mtf*  NewSeqMtf(int NumResidues, int AlphabetSize) {
/*----------------------------------------------------------*/
/*  allocate space for a new pssm.                          */
/*----------------------------------------------------------*/
  int       i;
  Seq_Mtf*  pssm;

  pssm = (Seq_Mtf*) calloc(1,sizeof(Seq_Mtf));
  pssm->n = NumResidues;
  pssm->AlphabetSize = AlphabetSize;
  pssm->ww =    (int**) calloc(1,NumResidues * sizeof(int*));
  pssm->freqs = (int**) calloc(1,NumResidues * sizeof(int*));
  for (i=0; i<NumResidues; i++) {
    pssm->ww[i] =    (int*) calloc(1,AlphabetSize * sizeof(int));
    pssm->freqs[i] = (int*) calloc(1,AlphabetSize * sizeof(int));
  }
  return(pssm);
}


Seq_Mtf*  FreeSeqMtf(Seq_Mtf* pssm) {
/*----------------------------------------------------------*/
/*  free pssm.                                              */
/*----------------------------------------------------------*/
  int  i;
  
  for (i=0; i<pssm->n; i++) {
    free(pssm->ww[i]);
    free(pssm->freqs[i]);
  }
  free(pssm->ww);
  free(pssm->freqs);
  free(pssm);
  return NULL;
}


void PrintSeqMtf(Seq_Mtf* psm, FILE* pFile) {
/*----------------------------------------------------------*/
/*  print the pssm.                                         */
/*----------------------------------------------------------*/
  char OutputOrder[] = "ARNDCQEGHILKMFPSTWYV";
  int  i, j;

  fprintf(pFile, "PSSM:\n");
  fprintf(pFile, "Number of residues: %4d\n", psm->n);
  fprintf(pFile, "Weights:\n");
  for (i=0; i<strlen(OutputOrder); i++) {
    fprintf(pFile, "%8c ", OutputOrder[i]);
  }
  fprintf(pFile, "\n");
  for (i=0; i<psm->n; i++) {
    for (j=0; j<strlen(OutputOrder); j++) {
      fprintf(pFile, "%8d ", psm->ww[i][j]);
    }
    fprintf(pFile, "\n");
  }
  fprintf(pFile, "Frequencies:\n");
  for (i=0; i<strlen(OutputOrder); i++) {
    fprintf(pFile, "%8c ", OutputOrder[i]);
  }
  fprintf(pFile, "\n");
  for (i=0; i<psm->n; i++) {
    for (j=0; j<strlen(OutputOrder); j++) {
      fprintf(pFile, "%8d ", psm->freqs[i][j]);
    }
    fprintf(pFile, "\n");
  }
  fprintf(pFile, "\n");
}


Qry_Seq*  NewQrySeq(int NumResidues, int NumBlocks) {
/*----------------------------------------------------------*/
/*  allocate space for a new query sequence.                */
/*----------------------------------------------------------*/
  Qry_Seq*  qsqp;
  int       i;

  qsqp = (Qry_Seq*) calloc(1,sizeof(Qry_Seq));
  qsqp->n = NumResidues;
  qsqp->sq = (int*) calloc(1,sizeof(int) * NumResidues);
  qsqp->sac.n = NumBlocks;
  qsqp->sac.mn = (int*) calloc(1,sizeof(int) * NumBlocks);
  qsqp->sac.mx = (int*) calloc(1,sizeof(int) * NumBlocks);
  /* for no constraints, set mn and mx to -1 for each block */
  /* make no constraints the default */
  for (i=0; i<NumBlocks; i++) {
    qsqp->sac.mn[i] = -1;
    qsqp->sac.mx[i] = -1;
  }
  return(qsqp);
}


Qry_Seq*  FreeQrySeq(Qry_Seq* qsqp) {
/*----------------------------------------------------------*/
/*  free query sequence.                                    */
/*----------------------------------------------------------*/
  free(qsqp->sac.mn);
  free(qsqp->sac.mx);
  free(qsqp->sq);
  free(qsqp);
  return NULL;
}


void PrintQrySeq(Qry_Seq* qsqp, FILE* pFile) {
/*----------------------------------------------------------*/
/*  for debugging. print query sequence.                    */
/*----------------------------------------------------------*/
  int    i, j, Count;
  char   ResLetters[32];

  /* lookup to give one-letter names of residues */
  strcpy(ResLetters, "ARNDCQEGHILKMFPSTWYV");

  fprintf(pFile, "Query Sequence:\n");
  fprintf(pFile, "number of residues: %4d\n", qsqp->n);
  Count = 0;
  for (i=0; i<=qsqp->n/30; i++) {
    for (j=0; j<30; j++) {
      fprintf(pFile, "%c ", ResLetters[qsqp->sq[i*30 + j]]);
      Count++;
      if (Count == qsqp->n) {
        fprintf(pFile, "\n");
        goto Here;
      }
    }
    fprintf(pFile, "\n");
  }

Here:
  fprintf(pFile, "number of constraints: %4d\n", qsqp->sac.n);
  fprintf(pFile, "     mn     mx\n");
  for (i=0; i<qsqp->sac.n; i++) {
    fprintf(pFile, " %6d %6d\n", qsqp->sac.mn[i], qsqp->sac.mx[i]);
  }
  fprintf(pFile, "\n");
}


Rcx_Ptl*  NewRcxPtl(int NumResTypes, int NumDistances, int PeptideIndex) {
/*----------------------------------------------------------*/
/*  allocate space for the contact potential                */
/*----------------------------------------------------------*/
  Rcx_Ptl*  pmf;
  int       i, j;

  pmf = (Rcx_Ptl*) calloc(1,sizeof(Rcx_Ptl));
  pmf->rre = (int***) calloc(1,NumDistances * sizeof(int**));
  pmf->rrt = (int***) calloc(1,NumDistances * sizeof(int**));
  pmf->re  = (int**)  calloc(1,NumDistances * sizeof(int*));
  for (i=0; i<NumDistances; i++) {
    pmf->rre[i] = (int**) calloc(1,NumResTypes * sizeof(int*));
    pmf->rrt[i] = (int**) calloc(1,NumResTypes * sizeof(int*));
    for (j=0; j<NumResTypes; j++) {
      pmf->rre[i][j] = (int*) calloc(1,NumResTypes * sizeof(int));
      pmf->rrt[i][j] = (int*) calloc(1,NumResTypes * sizeof(int));
    }
    pmf->re[i]  = (int*)  calloc(1,NumResTypes * sizeof(int));
  }
  pmf->nrt = NumResTypes;
  pmf->ndi = NumDistances;
  pmf->ppi = PeptideIndex;
  return(pmf);
}


Rcx_Ptl*  FreeRcxPtl(Rcx_Ptl* pmf) {
/*----------------------------------------------------------*/
/*  free contact potential.                                 */
/*----------------------------------------------------------*/
  int  i, j;

  for (i=0; i<pmf->ndi; i++) {
    for (j=0; j<pmf->nrt; j++) {
      free(pmf->rre[i][j]);
      free(pmf->rrt[i][j]);
    }
    free(pmf->rre[i]);
    free(pmf->rrt[i]);
    free(pmf->re[i]);
  }
  free(pmf->rre);
  free(pmf->rrt);
  free(pmf->re);
  free(pmf);
  return NULL;
}


Gib_Scd*  NewGibScd(int NumTempSteps) {
/*----------------------------------------------------------*/
/*  allocate space for annealing schedule.                  */
/*----------------------------------------------------------*/
  Gib_Scd*  gsp;

  gsp = (Gib_Scd*) calloc(1,sizeof(Gib_Scd));
  gsp->nts = NumTempSteps;
  gsp->nti = (int*) calloc(1,NumTempSteps * sizeof(int));
  gsp->nac = (int*) calloc(1,NumTempSteps * sizeof(int));
  gsp->nlc = (int*) calloc(1,NumTempSteps * sizeof(int));
  gsp->tma = (int*) calloc(1,NumTempSteps * sizeof(int));
  gsp->tml = (int*) calloc(1,NumTempSteps * sizeof(int));
  gsp->lms = (int*) calloc(1,NumTempSteps * sizeof(int));
  gsp->lmw = (int*) calloc(1,NumTempSteps * sizeof(int));
  gsp->lmf = (int*) calloc(1,NumTempSteps * sizeof(int));
  return(gsp);
}


Gib_Scd*  FreeGibScd(Gib_Scd* gsp) {
/*----------------------------------------------------------*/
/*  free the annealing schedule.                            */
/*----------------------------------------------------------*/
  free(gsp->nti);
  free(gsp->nac);
  free(gsp->nlc);
  free(gsp->tma);
  free(gsp->tml);
  free(gsp->lms);
  free(gsp->lmw);
  free(gsp->lmf);
  free(gsp);
  return NULL;
}


Fld_Mtf*  NewFldMtf(int NumResidues, int NumResResContacts, int NumResPepContacts) {
/*-------------------------------------------------------------*/
/*  allocate space for contact lists and minimum loop lengths  */
/*-------------------------------------------------------------*/
  int       i;
  Fld_Mtf*  mtf;

  mtf = (Fld_Mtf*) calloc(1,sizeof(Fld_Mtf));
  mtf->n = NumResidues;
  mtf->rrc.n = NumResResContacts;
  mtf->rrc.r1 = (int*) calloc(1,NumResResContacts * sizeof(int));
  mtf->rrc.r2 = (int*) calloc(1,NumResResContacts * sizeof(int));
  mtf->rrc.d =  (int*) calloc(1,NumResResContacts * sizeof(int));
  mtf->rpc.n = NumResPepContacts;
  mtf->rpc.r1 = (int*) calloc(1,NumResPepContacts * sizeof(int));
  mtf->rpc.p2 = (int*) calloc(1,NumResPepContacts * sizeof(int));
  mtf->rpc.d =  (int*) calloc(1,NumResPepContacts * sizeof(int));
  mtf->mll = (int**) calloc(1,NumResidues * sizeof(int*));
  for (i=0; i<NumResidues; i++) {
    mtf->mll[i] = (int*) calloc(1,NumResidues * sizeof(int));
  }
  return(mtf);
}


Fld_Mtf*  FreeFldMtf(Fld_Mtf* mtf) {
/*----------------------------------------------------------*/
/*  free contact lists and loop lengths                     */
/*----------------------------------------------------------*/
  int  i;

  free(mtf->rrc.r1);
  free(mtf->rrc.r2);
  free(mtf->rrc.d);
  free(mtf->rpc.r1);
  free(mtf->rpc.p2);
  free(mtf->rpc.d);
  for (i=0; i<mtf->n; i++) {
    free(mtf->mll[i]);
  }
  free(mtf->mll);
  free(mtf);
  return NULL;
}


void PrintFldMtf(Fld_Mtf* mtf, FILE* pFile) {
/*----------------------------------------------------------*/
/*  print contact lists and loop lengths.                   */
/*----------------------------------------------------------*/
  int    i, j;

  fprintf(pFile, "Contact Lists, Loop Lengths:\n");
  fprintf(pFile, "Number of residues: %4d\n", mtf->n);
  fprintf(pFile, "Number of residue-residue contacts: %6d\n", mtf->rrc.n);
  fprintf(pFile, "res index 1,  res index 2,  Distance interval\n");
  for (i=0; i<mtf->rrc.n; i++) {
    fprintf(pFile, "  %9d  %12d  %18d\n", mtf->rrc.r1[i], mtf->rrc.r2[i], mtf->rrc.d[i]);
  }
  fprintf(pFile, "Number of residue-peptide contacts: %6d\n", mtf->rpc.n);
  fprintf(pFile, "res index,  peptide index,  Distance interval\n");
  for (i=0; i<mtf->rpc.n; i++) {
    fprintf(pFile, "  %7d  %14d  %18d\n", mtf->rpc.r1[i], mtf->rpc.p2[i], mtf->rpc.d[i]);
  }
  fprintf(pFile, "Minimum loop lengths:\n");
  for (i=0; i<mtf->n; i++) {
    for (j=0; j<mtf->n; j++) {
      fprintf(pFile, " %4d", mtf->mll[i][j]);
    }
    fprintf(pFile, "\n");
  }
  fprintf(pFile, "\n");
}


Thd_Tbl*  NewThdTbl(int NumResults, int NumCoreElements) {
/*----------------------------------------------------------*/
/*  allocate space for results.                             */
/*----------------------------------------------------------*/
  int       i;
  Thd_Tbl*  ttb;

  ttb = (Thd_Tbl*) calloc(1,sizeof(Thd_Tbl));
  ttb->n =   NumResults;
  ttb->nsc = NumCoreElements;
  ttb->tg =   (float*) calloc(1,NumResults * sizeof(float));
  ttb->ps =   (float*) calloc(1,NumResults * sizeof(float));
  ttb->ms =   (float*) calloc(1,NumResults * sizeof(float));
  ttb->cs =   (float*) calloc(1,NumResults * sizeof(float));
  ttb->lps =  (float*) calloc(1,NumResults * sizeof(float));
  ttb->zsc =  (float*) calloc(1,NumResults * sizeof(float));
  ttb->g0 =   (float*) calloc(1,NumResults * sizeof(float));
  ttb->m0 =   (float*) calloc(1,NumResults * sizeof(float));
  ttb->errm = (float*) calloc(1,NumResults * sizeof(float));
  ttb->errp = (float*) calloc(1,NumResults * sizeof(float));
  ttb->tf =   (int*) calloc(1,NumResults * sizeof(int));
  ttb->ts =   (int*) calloc(1,NumResults * sizeof(int));
  ttb->ls =   (int*) calloc(1,NumResults * sizeof(int));
  ttb->pr =   (int*) calloc(1,NumResults * sizeof(int));
  ttb->nx =   (int*) calloc(1,NumResults * sizeof(int));
  ttb->al =   (int**) calloc(1,NumCoreElements * sizeof(int*));
  ttb->no =   (int**) calloc(1,NumCoreElements * sizeof(int*));
  ttb->co =   (int**) calloc(1,NumCoreElements * sizeof(int*));
  for (i=0; i<NumCoreElements; i++) {
    ttb->al[i] = (int*) calloc(1,NumResults *sizeof(int));
    ttb->no[i] = (int*) calloc(1,NumResults *sizeof(int));
    ttb->co[i] = (int*) calloc(1,NumResults *sizeof(int));
  }
  return(ttb);
}


Thd_Tbl*  FreeThdTbl(Thd_Tbl* ttb) {
/*----------------------------------------------------------*/
/*  free results.                                           */
/*----------------------------------------------------------*/
  int  i;

  free(ttb->tg);
  free(ttb->ps);
  free(ttb->ms);
  free(ttb->cs);
  free(ttb->lps);
  free(ttb->zsc);
  free(ttb->g0);
  free(ttb->m0);
  free(ttb->errm);
  free(ttb->errp);
  free(ttb->tf);
  free(ttb->ts);
  free(ttb->ls);
  free(ttb->pr);
  free(ttb->nx);
  for (i=0; i<ttb->nsc; i++) {
    free(ttb->al[i]);
    free(ttb->no[i]);
    free(ttb->co[i]);
  }
  free(ttb->al);
  free(ttb->no);
  free(ttb->co);
  free(ttb);
  return NULL;
}


void PrintThdTbl(Thd_Tbl* ttb, FILE* pFile) {
/*------------------------------------------------------------------------------*/
/* print the results.                                                           */
/*------------------------------------------------------------------------------*/
  int    i, j;

  fprintf(pFile, "Threading Results:\n");
  fprintf(pFile, "number of threads: %6d\n", ttb->n);
  fprintf(pFile, "number of core segments:  %6d\n", ttb->nsc);
  fprintf(pFile, "index of lowest energy thread:  %6d\n", ttb->mn);
  fprintf(pFile, "index of highest energy thread: %6d\n", ttb->mx);
  fprintf(pFile, "for each thread:\n");
  fprintf(pFile, "           tg           ps           ms           cs          lps          zsc\n");
  for (i=0; i<ttb->n; i++) {
    fprintf(pFile, " %12.5e %12.5e %12.5e %12.5e %12.5e %12.5e\n",
      ttb->tg[i], ttb->ps[i], ttb->ms[i], ttb->cs[i], ttb->lps[i], ttb->zsc[i]);
  }
  fprintf(pFile, "           g0           m0         errm         errp\n");
  for (i=0; i<ttb->n; i++) {
    fprintf(pFile, " %12.5e %12.5e %12.5e %12.5e\n",
      ttb->g0[i], ttb->m0[i], ttb->errm[i], ttb->errp[i]);
  }
  fprintf(pFile, "     tf     ts     ls     pr     nx\n");
  for (i=0; i<ttb->n; i++) {
    fprintf(pFile, " %6d %6d %6d %6d %6d\n",
      ttb->tf[i], ttb->ts[i], ttb->ls[i], ttb->pr[i], ttb->nx[i]);
  }
  fprintf(pFile, "threading alignments: %6d\n", i+1);
  fprintf(pFile, "Centers:\n");
  for (i=0; i<ttb->n; i++) {
    for (j=0; j<ttb->nsc; j++) {
      fprintf(pFile, " %4d", ttb->al[j][i] + 1);
    }
    fprintf(pFile, "\n");
  }
  fprintf(pFile, "N offsets:\n");
  for (i=0; i<ttb->n; i++) {
    for (j=0; j<ttb->nsc; j++) {
      fprintf(pFile, " %4d", ttb->no[j][i]);
    }
    fprintf(pFile, "\n");
  }
  fprintf(pFile, "C offsets:\n");
  for (i=0; i<ttb->n; i++) {
    for (j=0; j<ttb->nsc; j++) {
      fprintf(pFile, " %4d", ttb->co[j][i]);
    }
    fprintf(pFile, "\n");
  }
  fprintf(pFile, "\n");
}


int CopyResult(Thd_Tbl* pFromResults, Thd_Tbl* pToResults, int from, int to) {
/*------------------------------------------------------------------------------*/
/* copy the from-th result of pFromResults to the to-th result of pToResults.   */
/*------------------------------------------------------------------------------*/
  int  i;

  pToResults->tg[to] =   pFromResults->tg[from];
  pToResults->ps[to] =   pFromResults->ps[from];
  pToResults->ms[to] =   pFromResults->ms[from];
  pToResults->cs[to] =   pFromResults->cs[from];
  pToResults->lps[to] =  pFromResults->lps[from];
  pToResults->zsc[to] =  pFromResults->zsc[from];
  pToResults->g0[to] =   pFromResults->g0[from];
  pToResults->m0[to] =   pFromResults->m0[from];
  pToResults->errm[to] = pFromResults->errm[from];
  pToResults->errp[to] = pFromResults->errp[from];
  pToResults->tf[to] =   pFromResults->tf[from];
  pToResults->ts[to] =   pFromResults->ts[from];
  pToResults->ls[to] =   pFromResults->ls[from];
  pToResults->pr[to] =   pFromResults->pr[from];
  pToResults->nx[to] =   pFromResults->nx[from];
  for (i=0; i<pFromResults->nsc; i++) {
    pToResults->al[i][to] = pFromResults->al[i][from];
    pToResults->no[i][to] = pFromResults->no[i][from];
    pToResults->co[i][to] = pFromResults->co[i][from];
  }
  return(1);
}


void OrderThdTbl(Thd_Tbl* pResults) {
/*----------------------------------------------------------*/
/*  put the results in order of highest score to lowest.    */
/*  it's O(n**2), but that's ok because n is very small.    */
/*  return the new ordered results, free original results.  */
/*----------------------------------------------------------*/
  int*      Order;
  Boolean*  CheckList;
  int       i, j, SaveIndex, Index;
  double    HighestScore;
  Thd_Tbl*  pOrderedResults;

  /* mem allocation for array where order of results is stored */
  Order = (int*) calloc(1,sizeof(int) * pResults->n);
  /* mem allocation for checklist to tell which results have been ordered */
  CheckList = (Boolean*) calloc(1,sizeof(int) * pResults->n);

  /* look through the list n times */
  for (i=0; i<pResults->n; i++) {
    HighestScore = -9999999999.0;
    SaveIndex = -1;
    /* each time, find the highest remaining score */
    for (j=0; j<pResults->n; j++) {
      if (CheckList[j] == FALSE) {
        if (pResults->tg[j] > HighestScore) {
          HighestScore = pResults->tg[j];
          SaveIndex = j;
        }
      }
    }
    /* save the index, and mark that the score has been ordered */
    Order[i] = SaveIndex;
    CheckList[SaveIndex] = TRUE;
  }

  /* now that the order is determined, do the re-ordering */
  pOrderedResults = NewThdTbl(pResults->n, pResults->nsc);
  for (i=0; i<pResults->n; i++) {
    CopyResult(pResults, pOrderedResults, Order[i], i);
  }
  pOrderedResults->mx = 0;
  pOrderedResults->mn = pResults->n - 1;

  /* now copy results back to original structure */
  for (i=0; i<pResults->n; i++) {
    CopyResult(pOrderedResults, pResults, i, i);
  }

  /* update next and previous arrays to match new order */
  for (i=0; i<pResults->n; i++) {
    /* point to next-lower energy thread */
    pResults->nx[i] = i+1 < pResults->n ? i+1 : 0;
    /* point to next-higher energy thread */
    Index = pResults->n - i - 1;
    pResults->pr[Index] = Index-1 < 0 ? 0 : Index-1;
  }

  /* update min and max indices to match new order */
  pResults->mx = pOrderedResults->mx;
  pResults->mn = pOrderedResults->mn;

  /* free memory allocated for this routine */
  free(Order);
  free(CheckList);
  FreeThdTbl(pOrderedResults);

  return;
}


void ScaleThdTbl(Thd_Tbl* ttb, double ScalingFactor) {
/*----------------------------------------------------------*/
/*  scale the energies in ttb by ScalingFactor              */
/*----------------------------------------------------------*/
  int    i;
  float  SF;
  
  SF = (float) ScalingFactor;

  for (i=0; i<ttb->n; i++) {
    ttb->tg[i]   *= SF;
    ttb->ps[i]   *= SF;
    ttb->ms[i]   *= SF;
    ttb->cs[i]   *= SF;
    ttb->lps[i]  *= SF;
    ttb->zsc[i]  *= SF;
    ttb->g0[i]   *= SF;
    ttb->m0[i]   *= SF;
    ttb->errm[i] *= SF;
    ttb->errp[i] *= SF;
  }
}


int ThrdRound(double Num) {
/*----------------------------------------------------------*/
/*  round fp to nearest int                                 */
/*----------------------------------------------------------*/
  if (Num > 0) {
    return((int)(Num + 0.5));
  }
  else {
    return((int)(Num - 0.5));
  }
}
