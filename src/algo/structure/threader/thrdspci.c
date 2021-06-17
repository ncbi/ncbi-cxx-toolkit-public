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
* File Name:  thrdspci.c
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

/* Sum residue type counts for the n-th core segment and for loops adjacent */
/* it.  Use these sums to recompute total threaded sequence composition. For */
/* loops, include their residues in the composition sum only if their length */
/* is less than the maximum reference state length specified in the core */
/* definition.  For loops at the terminii, include either this maximum number */
/* of residues or the total length of the assigned sequence segment, */
/* whichever is less. Return 1 if the composition has changed since the */
/* previous call and 0 if it has not. */

int spci(Cor_Def* cdf, Qry_Seq* qsq, Cur_Loc* sli, Cur_Aln* sai, int n, Seg_Cmp* spc) {
/*--------------------------------------------------------*/
/* cdf:  Core segment locations and loop length limits    */
/* qsq:  Sequence to thread with alignment contraints     */
/* sli:  Current locations of core segments in the motif  */
/* sai:  Current alignment of query sequence with core    */
/* n:    Update composition for this core segment only    */
/* spc:  Residue type composition of current threaded seq */
/*--------------------------------------------------------*/

int	nsc;		/* Number of core segments */
int	nlp;		/* Number of loops */
int	nrt;		/* Number of residue types in potential */
int	nqi;		/* Number of residues in query sequence */
int	i,j;		/* Counters */
int	r;		/* Residue type */
int	nt,ct;		/* Core segment or loop terminii */
int	*rtn;		/* Pointer to residue type counts */
int	lm;		/* Maximum loop length to include in counts */

nsc=spc->nsc;
nrt=spc->nrt;
nlp=spc->nlp;
nqi=qsq->n;

/* Assign core segment composition from previous thread */

for(i=0; i<nrt; i++) spc->rto[i]=spc->rt[i];
/* for(i=0;i<nrt;i++) printf("%d ",spc->rto[i]); printf(":spc->rto[%d]\n",n); */

/* Assign current core segment composition */

rtn=spc->srt[n]; for(r=0;r<nrt;r++) rtn[r]=0;
nt=sai->al[n]-sli->no[n];
ct=sai->al[n]+sli->co[n];
/* printf("nt:%d ct:%d\n",nt,ct); */
for(i=nt;i<=ct;i++) {
	r=qsq->sq[i];
	if(r<0) continue;
	/* printf("i:%d r:%d\n",i,r); */
	rtn[r]++; } 
/* for(i=0;i<nrt;i++) printf("%d ",rtn[i]); printf(":spc->srt[%d]\n",n); */

/* Assign n-terminal loop composition */

lm=cdf->lll.lrfs[n];
if(lm>0) {
	rtn=spc->lrt[n];
	for(r=0;r<nrt;r++) rtn[r]=0;
	if(n==0) {
		ct=sai->al[0]-sli->no[0]-1;
		nt=ct-lm+1;
		nt=(nt<0) ? 0 : nt;
		/* printf("nt:%d ct:%d\n",nt,ct); */
		for(i=nt;i<=ct;i++) {
			r=qsq->sq[i];
			if(r<0) continue;
			/* printf("i:%d r:%d\n",i,r); */
			rtn[r]++; } } 
	else {
		ct=sai->al[n]-sli->no[n]-1;
		nt=sai->al[n-1]+sli->co[n-1]+1;
		if((ct-nt)<lm ) {
			/* printf("nt:%d ct:%d\n",nt,ct); */
			for(i=nt;i<=ct;i++) {
				r=qsq->sq[i];
				if(r<0) continue;
				/* printf("i:%d r:%d\n",i,r); */
				rtn[r]++; } } } }

/* for(i=0;i<nrt;i++) printf("%d ",spc->lrt[n][i]); 
printf(":spc->lrt[%d]\n",n); */


/* Assign c-terminal loop composition */

lm=cdf->lll.lrfs[n+1];
if(lm>0) {
	rtn=spc->lrt[n+1];
	for(r=0;r<nrt;r++) rtn[r]=0;
	if((n+1)==nsc) {
		nt=sai->al[n]+sli->co[n]+1;
		ct=nt+lm-1;
		ct=(ct>=nqi) ? nqi-1 : ct;
		/* printf("nt:%d ct:%d\n",nt,ct); */
		for(i=nt;i<=ct;i++) {
			r=qsq->sq[i];
			if(r<0) continue;
			/* printf("i:%d r:%d\n",i,r); */
			rtn[r]++; } } 
	else {
		nt=sai->al[n]+sli->co[n]+1;
		ct=sai->al[n+1]-sli->no[n+1]-1;
		if((ct-nt)<lm ) {
			/* printf("nt:%d ct:%d\n",nt,ct); */
			for(i=nt;i<=ct;i++) {
				r=qsq->sq[i];
				if(r<0) continue;
				/* printf("i:%d r:%d\n",i,r); */
				rtn[r]++; } } } }

/* for(i=0;i<nrt;i++) printf("%d ",spc->lrt[n+1][i]); 
printf(":spc->lrt[%d]\n",n+1); */


/* Assign composition, or total residue type counts */

for(r=0;r<nrt;r++) spc->rt[r]=0; 

/* for(i=0; i<nrt; i++) printf("%d ",spc->rt[i]); printf("spc->rt\n");
for(j=0; j<nsc; j++) {
	for(i=0;i<nrt;i++) printf("%d ",spc->srt[j][i]);
	printf("spc->srt[%d]\n",j); }
for(j=0; j<nlp; j++) {
	for(i=0;i<nrt;i++) printf("%d ",spc->lrt[j][i]);
	printf("spc->lrt[%d]\n",j); } */

for(i=0;i<nsc;i++) {
	rtn=spc->srt[i]; for(r=0;r<nrt;r++) spc->rt[r] += rtn[r]; }
for(i=0;i<nlp;i++) {
	rtn=spc->lrt[i]; for(r=0;r<nrt;r++) spc->rt[r] += rtn[r]; }

/* for(i=0; i<nrt; i++) printf("%d ",spc->rt[i]); printf("spc->rt\n"); */


/* If composition has not changed from the previous thread return 0 */

j=0; for(i=0; i<nrt; i++) if(spc->rt[i]!=spc->rto[i]) { j=1; break; } 
return(j);

}
