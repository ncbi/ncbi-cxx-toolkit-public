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
* File Name:  thrdsal0.c
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

/* Initial core segment alignment */

/* Find limiting values for alignment of a core segment, given the extents */
/* of all segments, loop length ranges, the length of the query sequence, */
/* any explicit alignment constraints, and the alignment of other segments */
/* that have been assigned already.  The query sequence is assumed to be */
/* unaligned or only partially aligned with the core motif, and constraints */
/* are therefore derived from consieration of the overall length of the */
/* query sequence as compared to that required for all other core segments */
/* and loops.  Explicit alignement constraints or previous alignment of */
/* other core segments are also considered, and will further restrict the */
/* range of alignments allowed for the current segment */

int sal0(Cor_Def* cdf, Qry_Seq* qsq, Cur_Loc* sli, Cur_Aln* sai, int cs, int* mn, int* mx) {
/*-----------------------------------------------------------*/
/* cdf:    Contains specified loop length limits             */
/* qsq:    Sequence to thread with alignment contraints      */
/* sli:    Contains current segment location values          */
/* sai:    Contains current segment alignment values         */
/* cs:     The segment for which constraints are to be found */
/* mn,mx:  The range of allowed aligments for this segment   */
/*-----------------------------------------------------------*/

int	i;		/* Counter */
int	nsc;		/* Number of core segments */
int	sl;		/* Core segment lengths */
int     ec;             /* Alignment limit due to explicit constraint */
int	lm1,lm2;	/* Loop length limits */
int	ntmn,ntmx;	/* Alignment range due to n-termnal constraints */
int	ctmn,ctmx;	/* Alignment range due to c-termnal constraints */


/* Number of core segments */
nsc=cdf->sll.n;

/* Find alignment constraints arising from segments n-terminal to this one. */
/* These are indices of the most c-terminal query sequence residue assigned */
/* to an n-terminal segment or loop. The minimum index assumes minimum loop */
/* lengths and explicitly constrained segments at their most n-terminal */
/* positions.  The maximum index assumes maximum loop lengths and explicitly */
/* constrained segments at their most c-terminal positions. */

/* Allow for the minimum length of an n-terminal tail  */
ntmn=-1+cdf->lll.llmn[0];

/* Initialize the maximum alignment allowed by n-terminal segments to the */
/* end of the sequence.  They constrain alignment only one of the n-terminal */
/* segments is already aligned or explicitly constrained. */
ntmx=qsq->n;

/* Loop over n-terminal segments  */
/* printf("initial ntmn:%d initial ntmx:%d\n",ntmn,ntmx); */
/* printf("cs:%d\n",cs); */
for(i=0; i<cs; i++) {
	
	/* Add segment length */	
	sl=sli->no[i]+sli->co[i]+1;
	ntmn+=sl; 
	ntmx+=sl;
	/* printf("sl:%d ntmn:%d ntmx:%d\n",sl,ntmn,ntmx); */

	/* Reset limits to reflect any explicit constraints */ 	
	if(qsq->sac.mn[i]>0) {
		ec=qsq->sac.mn[i]+sli->co[i];
		ntmn=(ec>ntmn) ? ec : ntmn;}
	if(qsq->sac.mx[i]>0) {
		ec=qsq->sac.mx[i]+sli->co[i];
		ntmx=(ec<ntmx) ? ec : ntmx;}

	/* Reset limits if this segment has been aligned already */
	if(sai->al[i]>=0) {
		ntmn=ntmx=sai->al[i]+sli->co[i]; 
		/* printf("ntmx=ntmn:%d\n",ntmn); */
		}

	/* Add length limits of the following loop */
	lm1=sli->lp[i+1];
	lm2=cdf->lll.llmn[i+1];
	ntmn+= (lm1>lm2) ? lm1 : lm2;
	/* printf("lm1:%d lm2:%d ntmn:%d\n",lm1,lm2,ntmn); */
	lm2=cdf->lll.llmx[i+1];
	ntmx+= (lm1>lm2) ? lm1 : lm2;  
 	/* printf("lm1:%d lm2:%d ntmx:%d\n",lm1,lm2,ntmx); */
 	}

/* Add n-terminal offset of current segment to give alignment limits */
ntmn+=sli->no[cs]+1;
ntmx+=sli->no[cs]+1;
/* printf("final ntmn: %d final ntmx: %d\n",ntmn,ntmx); */


/* Find alignment constraints arising from segments c-terminal to this one. */

/* Allow for the minimum length of a tail.  */
ctmx=qsq->n-cdf->lll.llmn[nsc];

/* Initialize the maximum alignment allowed by c-terminal segments to the */
/* start of the sequence.  They constrain alignment only one of the */
/* c-terminal segments is already aligned or explicitly constrained. */
ctmn=-1;

/* Loop over all c-terminal segments  */
/* printf("initial ctmn:%d initial ctmx:%d\n",ctmn,ctmx); */
for(i=nsc-1; i>cs; i--) {
	
	/* Add segment length */	
	sl=sli->no[i]+sli->co[i]+1;		
	ctmn-=sl; 
	ctmx-=sl;
	/* printf("sl:%d ctmn:%d ctmx:%d\n",sl,ctmn,ctmx); */

	/* Reset limits to reflect any explicit constraints */ 	
	if(qsq->sac.mn[i]>0) {
		ec=qsq->sac.mn[i]-sli->no[i];
		ctmn=(ec>ctmn) ? ec : ctmn;}
	if(qsq->sac.mx[i]>0) {
		ec=qsq->sac.mx[i]-sli->no[i];
		ctmx=(ec<ctmx) ? ec : ctmx;}

	/* Reset limits if this segment has been aligned already */
	if(sai->al[i]>=0) {
		ctmn=ctmx=sai->al[i]-sli->no[i]; 
		/* printf("ctmx=ctmn:%d\n",ctmn); */
		}

	/* Subtract lengths of the preceeding loop */
	lm1=sli->lp[i];
	lm2=cdf->lll.llmn[i];
	ctmx-= (lm1>lm2) ? lm1 : lm2;
	/* printf("lm1:%d lm2:%d ctmx:%d\n",lm1,lm2,ctmx); */
	lm2=cdf->lll.llmx[i];
	ctmn-= (lm1>lm2) ? lm1 : lm2;  
	/* printf("lm1:%d lm2:%d ctmn:%d\n",lm1,lm2,ctmn); */
	}

/* Subtract c-terminal offset of current segment to give alignment limits */
ctmn-=sli->co[cs]+1;
ctmx-=sli->co[cs]+1;
/* printf("final ctmn:%d final ctmx:%d\n",ctmn,ctmx); */


/* Signal an error if no alignment falls within the limits identified. */
if(ntmn>ctmx||ntmx<ctmn) return(0);

/* Reconcile n-termnal and c-termnal aligment limits: alignment must */
/* satisfy all the constraints */
*mn=(ntmn>ctmn) ? ntmn : ctmn;
*mx=(ntmx<ctmx) ? ntmx : ctmx;
/* printf("mn:%d mx:%d\n",*mn,*mx); */

/* Further reconcile any explict limits on the current segment alignment, */
/* and signal an error if these constraints cannot be satisfied. */
/* printf("qsq->sac.mn[%d]: %d\n",cs,qsq->sac.mn[cs]); */
if(qsq->sac.mn[cs]>=0) {	
	lm1=qsq->sac.mn[cs]; 
	if(lm1>*mx) return(0);
	if(lm1>*mn) *mn=lm1;}
/* printf("qsq->sac.mx[%d]: %d\n",cs,qsq->sac.mx[cs]); */
if(qsq->sac.mx[cs]>=0) {	
	lm1=qsq->sac.mx[cs];
	if(lm1<*mn) return(0);
	if(lm1<*mx) *mx=lm1; }

/* printf("mn:%d mx:%d\n",*mn,*mx); */
return(1);
}
