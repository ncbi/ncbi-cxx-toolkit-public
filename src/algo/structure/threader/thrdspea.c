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
* File Name:  thrdspea.c
*
* Author:  Stephen Bryant
*
* Initial Version Creation Date: 08/16/2000
*
* $Revision$
*
* File Description: threader
*/



#include <algo/structure/threader/thrdatd.h>
#include <algo/structure/threader/thrddecl.h>

/* Update segment pair energies as they are stored during alignment sampling. */
/* The segment pairwise term contains the sum of the pairwise components of */
/* the side-chain to side-chain contact potential.  The segment term contains */
/* the sum of side-chain "profile" terms, including side-chain to side-chain */
/* hydrophobic components, side-chain to peptide hydrophobic and pairwise */
/* components, and side-chain to fixed-residue pairwise and hydrohobic */
/* components. */

/*int*/ void spea(Cor_Def* cdf, Cxl_Als** cpa, Cur_Aln* sai, Cur_Loc* sli,
         int n, int h, Seg_Gsm* spe, Seq_Mtf* psm, Seg_Cmp* spc) {
/*--------------------------------------------------------*/
/* cpa:  Contacts by segment, given current location      */
/* sli:  Current locations of core segments in the motif  */
/* sai:  Current alignment of query sequence with core    */
/* n:    Segment for which partial sums are to be updated */
/* h:    If zero, include only hydropohobic/profile terms */
/* spe:  Partial sums of contact energies by segment pair */
/* psm:  Sequence motif parameters                        */
/* cdf:  Core definition data structure                   */
/* spc:  Residue type composition of current threaded seq */
/*--------------------------------------------------------*/

int     nsc;            /* Number of threaded segments in core definition */
int	i,j;		/* Counters */
int	r1,r2;		/* Residue indices in core motif */
int	s1,s2;		/* Segment indices in core motif */
int	t1,t2;		/* Residue type indices */
Cxl_Als *ca;	/* Pointer to contact list of the current segment */
int	gs;		/* Potential profile energy sum */
int	ms;		/* Motif energy sum */
int	mn, mx;
/*int     nrt;*/            /* Number of residue types in potential */


/* Parameters */

nsc=sai->nsc;
/*nrt=spc->nrt;*/

/* Set pointer to current contact list */

ca=cpa[n];
/* printf("nsc:%d n:%d\n",nsc,n); */


/* Zero segment pairwise sums for the current segment */

/* if(h!=0) for(i=0;i<nsc;i++) { spe->gss[n][i]=0; spe->gss[i][n]=0; } */
for(i=0;i<nsc;i++) { spe->gss[n][i]=0; spe->gss[i][n]=0; }


/* Sum potential for residue-residue contacts */

if(h!=0) for(i=0; i<ca->rr.n; i++) {
	r1=ca->rr.r1[i];
	t1=sai->sq[r1];
	if(t1<0) continue;
	s1=sli->cr[r1];
	r2=ca->rr.r2[i];
	t2=sai->sq[r2];
	if(t2<0) continue;
	s2=sli->cr[r2];
	/* printf("i:%d r1:%d r2:%d s1:%d s2:%d t1:%d t2:%d\n",
		i,r1,r2,s1,s2,t1,t2); */
	spe->gss[s1][s2]+= ca->rr.e[i][t1][t2];
	/* printf("spe->gss[%d][%d]:%d\n",s1,s2,spe->gss[s1][s2]); */
	}


/* Sum energies for "profile" terms */

/* Sum potential hydr/pept/fixed energies */

gs=0;
for(i=0;i<ca->r.n;i++) {
	t1=sai->sq[ca->r.r[i]];
	if(t1<0) continue;
	gs+=ca->r.e[i][sai->sq[ca->r.r[i]]];
	}
spe->gs[n]=gs;

/* for(j=0; j<nsc; j++) {
	for(i=0; i<nsc; i++) printf("%d ",spe->gss[j][i]);
			printf("spe->gss[%d]\n",j); }
for(i=0; i<nsc; i++) printf("%d ",spe->gs[i]); printf("spe->gs\n"); */

/* Sum motif energies */

ms=0; /*s0=0;*/
	mn=cdf->sll.rfpt[n]-sli->no[n];
	mx=cdf->sll.rfpt[n]+sli->co[n];

/*printf("\nAligned segment number:");printf("%d\n",n);
printf("sll.rfpt[n],cdf->sll.nomn[n],cdf->sll.comn[n]:");
printf("%d,%d,%d\n",cdf->sll.rfpt[n],
cdf->sll.nomn[n],cdf->sll.comn[n]);
printf("sai->al[n]:");printf("%d\n",sai->al[n]);
printf("mn,mx:");printf("%d,%d\n",mn,mx);*/


for(j=mn;j<=mx;j++) {
	t1=sai->sq[j];
	if(t1<0) continue;
	ms+=psm->ww[j][t1];

/*	for(k=0;k<psm->AlphabetSize;k++){
	s0+=psm->ww[j][k]*spc->rt[k]; } */

	}

/*printf("Sequence of aligned segment");printf("%d,%d\n",sai->sq[j],ms);}*/


spe->ms[n]=ms;

/* Sum conservation energies */

/*
cs=0;
spe->cs[n]=cs;
*/

/* Sum loopout energies */

/*
ls=0;
spe->ls[n]=ls;
*/

}
