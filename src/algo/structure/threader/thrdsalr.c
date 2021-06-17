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
* File Name:  thrdsalr.c
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

/* Find alignment range for a core segment */

/* Find limiting values for alignment of a core segment, given the extent */
/* and alignment of other segments, loop length ranges, the length of the */
/* query sequence, and any explicit alignment constraints */

int salr(Cor_Def* cdf, Qry_Seq* qsq, Cur_Loc* sli, Cur_Aln* sai, int cs, int* mn, int* mx) {
/*-----------------------------------------------------------*/
/* cdf:    Contains specified loop length limits             */
/* qsq:    Sequence to thread with alignment contraints      */
/* sli:    Contains current segment location values          */
/* sai:    Contains current segment alignment values         */
/* cs:     The segment for which constraints are to be found */
/* mn,mx:  The range of allowed aligments for this segment   */
/*-----------------------------------------------------------*/

int	nsc;		/* Number of core segments */
int	lm1,lm2;	/* Loop length limits */
int	ntmn,ntmx;	/* Alignment range due to n-termnal constraints */
int	ctmn,ctmx;	/* Alignment range due to c-termnal constraints */

/* Number of core segments */
nsc=cdf->sll.n;

/* printf("cs:%d ",cs);
for(i=0;i<nsc;i++) printf("%d ",sai->al[i]); printf("sai->al\n"); */

/* Find constraints arising from the segment n-terminal to this one. */
if(cs==0) {	
	ntmn=-1+cdf->lll.llmn[0]; 
	ntmx=qsq->n; }
else  { ntmn=ntmx=sai->al[cs-1]+sli->co[cs-1]; 
	/* printf("ntmn=ntmx:%d\n",ntmn);  */
	lm1=sli->lp[cs];
	/* printf("sli->lp[%d]:%d\n",cs,sli->lp[cs]); */
	lm2=cdf->lll.llmn[cs];
	ntmn+= (lm1>lm2) ? lm1 : lm2;
	/* printf("lm2:%d ntmn:%d\n",lm2,ntmn); */
	lm2=cdf->lll.llmx[cs];
	ntmx+= (lm1>lm2) ? lm1 : lm2;  
	/* printf("lm2:%d ntmx:%d\n",lm2,ntmx); */
	}
ntmn+=sli->no[cs]+1;
ntmx+=sli->no[cs]+1;
/* printf("ntmn:%d ntmx:%d\n",ntmn,ntmx); */

/* Find constraints arising from the segment c-terminal to this one. */
if(cs==(nsc-1)) { 	
	ctmn=-1; 
	ctmx=qsq->n-cdf->lll.llmn[nsc]; }
else {	ctmn=ctmx=sai->al[cs+1]-sli->no[cs+1]; 
	/* printf("ctmn=ctmx:%d\n",ctmn); */
	lm1=sli->lp[cs+1];
	/* printf("sli->lp[%d]:%d\n",cs+1,sli->lp[cs+1]); */
	lm2=cdf->lll.llmn[cs+1];
	ctmx-= (lm1>lm2) ? lm1 : lm2;
	/* printf("lm2:%d ctmx:%d\n",lm2,ctmx); */
	lm2=cdf->lll.llmx[cs+1];
	ctmn-= (lm1>lm2) ? lm1 : lm2;  
	/* printf("lm2:%d ctmn:%d\n",lm2,ctmn); */
	}
ctmn-=sli->co[cs]+1;
ctmx-=sli->co[cs]+1;
/* printf("ctmn:%d ctmx:%d\n",ctmn,ctmx); */


/* Signal an error if no alignment falls within the limits identified. */
if(ntmn>ctmx||ntmx<ctmn) return(0);

/* Reconcile n-termnal and c-termnal aligment limits. */
*mn=(ntmn>ctmn) ? ntmn : ctmn;
*mx=(ntmx<ctmx) ? ntmx : ctmx;
/* printf("mn:%d mx:%d\n",*mn,*mx); */

/* Further reconcile any explict limits on the current segment alignment. */
/* printf("qsq->sac.mn[%d]: %d\n",cs,qsq->sac.mn[cs]); */
if(qsq->sac.mn[cs]>0) {	
	lm1=qsq->sac.mn[cs]; 
	if(lm1>*mx) return(0);
	if(lm1>*mn) *mn=lm1;}
/* printf("qsq->sac.mx[%d]: %d\n",cs,qsq->sac.mx[cs]); */
if(qsq->sac.mx[cs]>0) {	
	lm1=qsq->sac.mx[cs];
	if(lm1<*mn) return(0);
	if(lm1<*mx) *mx=lm1; }

return(1);

}
