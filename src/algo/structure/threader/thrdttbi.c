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
* File Name:  thrdttbi.c
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
#include <math.h>
#define SIGDIF 1

/* Merge a sampled thread into a linked-list stack containing the lowest */
/* energy threads found in sampling up until now.  */

/* Modified 7/96 to optionally consider as new only those threads which */
/* differ in alignment variables. If new location variables give a lower */
/* energy, replace previous values for that alignment. This change is */
/* intended to reduce the storage required for alternative, suboptimal */
/* alignments. It is refered to below as alignment-only mode. */
 
int ttbi(Cur_Aln* sai, Cur_Loc* sli, Thd_Gsm* tdg, Thd_Tbl* ttb, int nrs, int o) {
/*---------------------------------------------------------*/
/* sai:  Current alignment of query sequence with core     */
/* sli:  Current locations of core segments in the motif   */
/* tdg:  Energy of the current alignment, current core     */
/* ttb:  Tables to hold Results of Gibbs sampled threading */
/* nrs:  Random start index for this thread                */
/* o:    If nonzero, only different alignments are stored  */
/*---------------------------------------------------------*/

int	i,j;		/* Counters */
int	ct;		/* Index of current thread in list traversal */
int	in;		/* Index where a new thread will be inserted */
int	pr;		/* Index of previous thread in linked list */
int	nx;		/* Index of next thread in linked list */
int	sm=0;		/* Test if two threads are the same */
int	le;		/* Test if new thread has lower energy */
float	cg;		/* Energy of current thread */


/* Determine the sort point in the linked list, the index of the first thread */
/* whose score is equal to or lower than that of the new thread. Also flag */
/* whether the new thread is identical to this one. */

if(tdg->dg<ttb->tg[ttb->mn]) {
  ct=-1; sm=0;
}

else { 
	/* Decend linked list from the top-scoring thread */
	ct=ttb->mx; for(i=0; i<ttb->n; i++) {

	/* Test if new thread has higher score than the current */
	cg=ttb->tg[ct];
	le=(tdg->dg>=cg) ? 1:0;

	/* Test if the new thread is the same as the current.  If it's */
	/* energy is within roundoff of the current thread, test that */
	/* all location and aligment variables are the same. */
	sm=0; if(fabs((double)((tdg->dg)-cg))<SIGDIF) {
		sm=1; for(j=0; j<sli->nsc; j++) {
			if(sai->al[j]!=ttb->al[j][ct]) {sm=0; break;}
			if(sli->no[j]!=ttb->no[j][ct]) {sm=0; break;}
			if(sli->co[j]!=ttb->co[j][ct]) {sm=0; break;} } }


	/* Continue to next thread if not higher score and not the same */
	if(le==0&&sm==0) {ct=ttb->nx[ct]; continue; } 
	else break; } }


/* If the new thread is identical to that identified in the list search, */
/* increment its frequency counter and return. No other action needed. */

if(sm==1) { 	ttb->tf[ct]++; 
		if(ttb->ls[ct]!=nrs) { ttb->ts[ct]++; ttb->ls[ct]=nrs; }
		return(0); }


/* If in alignment-only mode, check whether a thread with the same alignment */
/* variables exists within the list, regardless of its score. This will be */
/* the replacement point, the index where new thread data will be recorded. */

in=-1; if (o!=0) {
	
	/* Loop over thread table */
	for(i=0; i<=ttb->n; i++) {

		/* Test whether alignment variables are the same */
		in=i; for(j=0; j<sli->nsc; j++) {
			if(sai->al[j]!=ttb->al[j][i]) {in=-1; break;} }

		/* If so, record this thread index */
		if(in>=0) break; } }


/* If not in alignment-only mode, or if the new thread does not match any in */
/* the list, check whether the new thread's score is higher than the current */
/* minimum.  If not, ignore the new thread.  If so, set the replacement point */
/* as the index of the current minimum score thread. Initialize the frequency */
/* counter of the new thread. */

if(o==0 || in<0) { 

	/* If the new thread has score lower than any, no action needed. */
	if(ct<0) return(0);

	/* Otherwise set the replace point as the minimum score thread */
	in=ttb->mn;

	/* And set the new thread's frequency counter to one */
	ttb->tf[in]=1; ttb->ts[in]=1; ttb->ls[in]=nrs;  }


/* If in alignment-only mode, and a matching alignment has been found, */
/* increment it's frequency counter. If the new thread has lower score take */
/* take no further action. */

else {

	/* Increment frequency counter for the matching thread */
	ttb->tf[in]++; 
	if(ttb->ls[in]!=nrs) { ttb->ts[in]++; ttb->ls[in]=nrs; }

	/* If new thread has lower score, no action needed */
	if(tdg->dg<=ttb->tg[in]) return(0);  }


/* Copy score and alignment data of the new thread into the thread table. */


ttb->tg[in]=tdg->dg;
ttb->ps[in]=tdg->ps;
ttb->ms[in]=tdg->ms;
ttb->m0[in]=tdg->m0;
ttb->cs[in]=tdg->cs;
ttb->lps[in]=tdg->ls;
/*printf("%d,%f,%f,%f\n",in,ttb->ps[in],ttb->ms[in],ttb->tg[in]);*/

for(j=0;j<sli->nsc;j++) {	
	ttb->al[j][in]=sai->al[j];
	ttb->no[j][in]=sli->no[j];
	ttb->co[j][in]=sli->co[j]; } 


/* If the sort position of the new thread differs from that of the thread */
/* it replaces, update the linked list pointers. */


if(ct!=in) {	

	/* Pull thread to be replaced from the list */
	pr=ttb->pr[in]; 
	nx=ttb->nx[in];
	if(in==ttb->mn) ttb->mn=pr; else 
		ttb->pr[nx]=pr;
	if(in!=ttb->mx) ttb->nx[pr]=nx;
	
	/* Insert the new thread at its sort position */	
	if(ct==ttb->mx) ttb->mx=in; else {	
		pr=ttb->pr[ct];
		ttb->nx[pr]=in;
		ttb->pr[in]=pr; }
	ttb->nx[in]=ct;
	ttb->pr[ct]=in; }

return(1);
}
