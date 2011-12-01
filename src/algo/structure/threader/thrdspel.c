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
* File Name:  thrdspel.c
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

/* Update segment pair energies as they are stored during location sampling. */
/* The segment pairwise term contains the sum of the pairwise and hydrophobic */
/* components of the side-chain to side-chain contact potential, and the sum */
/* of side-chain to peptide contact potential.  The "profile" term contains */
/* only the sum of side-chain to fixed-residue contact potential. */

/*int*/ void spel(Cxl_Los** cpl, Cur_Aln* sai, Cur_Loc* sli, int cs, Seg_Gsm* spe,
         Cor_Def* cdf, Seq_Mtf* psm, Seg_Cmp* spc) {
/*--------------------------------------------------------*/
/* cpl:  Contacts by segment, given current location      */
/* sai:  Current alignment of query sequence with core    */
/* sli:  Current locations of core segments in the motif  */
/* cs:   Segment for which partial sums are to be updated */
/* spe:  Partial sums of contact energies by segment pair */
/* cdf:  Core segment locations and loop length limits    */
/* psm:  Sequence motif parameters                        */
/* spc:  Residue type composition of current threaded seq */
/*--------------------------------------------------------*/

int	nsc;		/* Number of core segments */
int	i,j;		/* Counters */
int	s1,s2;		/* Segment indices */
Cxl_Los *cl;	/* Pointer to contact list of the current segment */
int	gs;		/* Profile energy sum */
int     ms;		/* Motif energy sum */
int     css;             /* Conservation energies sum */
int     s0;             /* Expected motif energy sum */
int     ls;             /* Loopout energies sum */
int     mn, mx;
/*int     nrt;*/            /* Number of residue types in potential */
int	t1;

/* Parameters */

nsc=sai->nsc;
/*nrt=spc->nrt;*/

/* for(i=0; i<nsc; i++) printf("%d ",spe->gs[i]); printf("spe->gs\n");
for(j=0; j<nsc; j++) {
	for(i=0; i<nsc; i++) printf("%d ",spe->gss[j][i]);
		printf("spe->gss[%d]\n",j); } */


/* Set pointer to current segment contact list */

cl=cpl[cs];
/* printf("cs:%d\n",cs); */

/* Zero segment pairwise sums for the current segment */

for(i=0;i<nsc;i++) { spe->gss[cs][i]=0; spe->gss[i][cs]=0; }

/* for(j=0; j<nsc; j++) {
	for(i=0; i<nsc; i++) printf("%d ",spe->gss[j][i]);
		printf("spe->gss[%d]\n",j); } */


/* Sum potential for residue-residue contacts */

for(i=0; i<cl->rr.n; i++) {
	s1=sli->cr[cl->rr.r1[i]];
	if(s1<0) continue;
	s2=sli->cr[cl->rr.r2[i]];
	if(s2<0) continue;
	spe->gss[s1][s2]+=cl->rr.e[i]; }

/* for(j=0; j<nsc; j++) {
	for(i=0; i<nsc; i++) printf("%d ",spe->gss[j][i]);
		printf("spe->gss[%d]\n",j); } */



/* Sum potential for residue-peptide contacts */

for(i=0; i<cl->rp.n; i++) {
	s1=sli->cr[cl->rp.r1[i]];
	if(s1<0) continue;
	s2=sli->cr[cl->rp.p2[i]];
	if(s2<0) continue;
	spe->gss[s1][s2]+=cl->rp.e[i]; }

/* for(j=0; j<nsc; j++) {
	for(i=0; i<nsc; i++) printf("%d ",spe->gss[j][i]);
		printf("spe->gss[%d]\n",j); } */


/* Sum energies for "profile" terms */

/* Sum potential hydr/pept/fixed energies */

gs=0;
for(i=0;i<cl->rf.n;i++) {
	if(sli->cr[cl->rf.r1[i]]<0) continue;
	gs+=cl->rf.e[i]; }
spe->gs[cs]=gs;

/* for(i=0; i<nsc; i++) printf("%d ",spe->gs[i]); printf("spe->gs\n"); */

/* Sum motif energies */

ms=0; s0=0;

	mn=cdf->sll.rfpt[cs]-sli->no[cs];
	mx=cdf->sll.rfpt[cs]+sli->co[cs];

	for(j=mn;j<=mx;j++) {
		t1=sai->sq[j];
		if(t1<0) continue;
		ms+=psm->ww[j][t1];

/*		for(k=0;k<psm->AlphabetSize;k++){
		s0+=psm->ww[j][k]*spc->rt[k]; }*/

	}

	spe->ms[cs]=ms;
	spe->s0[cs]=s0;

/* Sum conservation energies */

css=0;
spe->cs[cs]=css;

/* Sum loopout energies */

ls=0;
spe->ls[cs]=ls;


}
