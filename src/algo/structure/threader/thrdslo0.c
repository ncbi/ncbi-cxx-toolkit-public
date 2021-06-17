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
* File Name:  thrdslo0.c
*
* Author:  Stephen Bryant
*
* Initial Version Creation Date: 08/16/2000
*
* $Revision$
*
* File Description: threader
*/



#include <stdlib.h>
#include <algo/structure/threader/thrdatd.h>
#include <algo/structure/threader/thrddecl.h>

/*----------------------------------------------------------------------------*/
/* Find constraints on the initial extent of the n- or c-terminus of a core   */
/* element, given elements already located, the length of the query sequence, */
/* and any explicit alignment constraints.  Aside from explict constraints    */
/* the query sequence is assumed to be as yet unaligned with the core folding */
/* motif.  The constraints on element termini extents are therefore derived   */
/* from the overall length of the query sequence, as compared to the lengths  */
/* of core elements already located, and the minimum lengths of loops and     */
/* core elements not yet located.  Explicit alignment constraints may further */
/* limit possible locations, by restricting the region of the query sequence  */
/* in which remaining core segments must be placed.  Extent constraints as    */
/* specified in the core definition are also considered, and the returned     */
/* minimum and maxixum core segment terminus locations will fall within these */
/* limits.                                                                    */
/*----------------------------------------------------------------------------*/
/* A return code of zero indicates that no valid extent could be found, i.e.  */
/* that the query sequence is too short to allow placement of the the current */
/* core segment, even at its minimum length.                                  */
/*----------------------------------------------------------------------------*/

int slo0(Fld_Mtf* mtf, Cor_Def* cdf, Qry_Seq* qsq, Cur_Loc* sli,
         int cs, int ct, int* mn, int* mx) {
/*----------------------------------------------------------*/
/* mtf:  Contact matrices defining the folding motif        */
/* cdf:  Contains min/max segment locations                 */
/* qsq:  Sequence to thread with alignment contraints       */
/* sli:  Contains segment location values to be set         */
/* cs:   Index of core segment to locate                    */
/* ct:   Flag indicating the n or c-terminus                */
/* mn:   Minimum folding motif index for this terminus      */
/* mx:   Maximum folding motif index for this terminus      */
/*----------------------------------------------------------*/

  int	i;		/* Counters                                           */
  int	nsc;            /* Number of core segments                            */
  int	sl;		/* Segment length                                     */
  int	ec;		/* Alignment limit due to explicit constraint         */
  int	lm1,lm2;	/* Loop length limits                                 */
  int	ntmn,ntmx;	/* Alignment limit due to n-terminal segments         */
  int	ctmn,ctmx;	/* Alignment limit due to c-terminal segments         */
  int	*no,*co;	/* Left and right offsets of core segment terminii    */
  int	qmn,qmx;	/* Combined alignment limits                          */


/*----------------------------------------------------------------------------*/
/* Allocate local storage for segment extents                                 */
/*----------------------------------------------------------------------------*/
  nsc=cdf->sll.n;
  no=(int *)calloc(nsc,sizeof(int));
  co=(int *)calloc(nsc,sizeof(int));

#ifdef SLO0_DEBUG
  printf("cs: %d ct: %d\n",cs,ct);
  for(i=0; i<nsc; i++) printf("%d ",qsq->sac.mn[i]); printf("qsq->sac.mn\n");
  for(i=0; i<nsc; i++) printf("%d ",qsq->sac.mx[i]); printf("qsq->sac.mx\n");
#endif

/*----------------------------------------------------------------------------*/
/* Assume minimum extents for segments not yet located                        */
/*----------------------------------------------------------------------------*/
  for(i=0;i<nsc;i++) {
    no[i]=(sli->no[i]<0) ? cdf->sll.nomn[i] : sli->no[i];
    co[i]=(sli->co[i]<0) ? cdf->sll.comn[i] : sli->co[i];
  }

#ifdef SLO0_DEBUG
  for(i=0;i<nsc;i++) printf("%d ",no[i]); printf("no\n");
  for(i=0;i<nsc;i++) printf("%d ",co[i]); printf("co\n");
#endif

/*----------------------------------------------------------------------------*/
/* Find alignment constraints arising from segments n-terminal to this one.   */
/* These are indices of the most c-terminal query sequence residue which must */
/* be assigned to an n-terminal element or loop. The minimum index assumes    */
/* minimum loop lengths and explicitly constrained segments at their most     */
/* n-terminal positions.  The maximum index assumes maximum loop lengths and  */
/* explicitly constrained segments at their most c-terminal positions.        */
/*----------------------------------------------------------------------------*/
/* Allow for the minimum length of a tail                                     */
/*----------------------------------------------------------------------------*/
  ntmn=-1+cdf->lll.llmn[0];

/*----------------------------------------------------------------------------*/
/* Initialize the maximum alignment allowed by n-terminal segments to the     */
/* end of the sequence.  They constrain alignment only if an n-terminal       */
/* segment is explicitly constrained.                                         */
/*----------------------------------------------------------------------------*/
  ntmx=qsq->n;

/*----------------------------------------------------------------------------*/
/* Loop over n-terminal segments                                              */
/*----------------------------------------------------------------------------*/
#ifdef SLO0_DEBUG
  printf("initial ntmn:%d initial ntmx:%d\n",ntmn,ntmx);
#endif
  for(i=0; i<cs; i++) {

/*----------------------------------------------------------------------------*/
/* Add segment length                                                         */	
/*----------------------------------------------------------------------------*/
    sl=no[i]+co[i]+1;		
    ntmn+=sl; 
    ntmx+=sl;
#ifdef SLO0_DEBUG
    printf("sl:%d ntmn:%d ntmx:%d\n",sl,ntmn,ntmx);
#endif

/*----------------------------------------------------------------------------*/
/* Reset limits to reflect any explicit constraints                           */
/*----------------------------------------------------------------------------*/
    if(qsq->sac.mn[i]>0) {
      ec=qsq->sac.mn[i]+co[i];
      ntmn=(ec>ntmn) ? ec : ntmn;
#ifdef SLO0_DEBUG
      printf("ec:%d ntmn:%d\n",ec,ntmn);
#endif
    }
    if(qsq->sac.mx[i]>0) {
      ec=qsq->sac.mx[i]+co[i];
      ntmx=(ec>ntmx) ? ec : ntmx;
#ifdef SLO0_DEBUG
      printf("ec:%d ntmx:%d\n",ec,ntmx);
#endif
    }

/*----------------------------------------------------------------------------*/
/* Add lengths of the following loop                                          */
/*----------------------------------------------------------------------------*/
    lm1=mtf->mll[cdf->sll.rfpt[i]+co[i]][cdf->sll.rfpt[i+1]-no[i+1]];
    lm2=cdf->lll.llmn[i+1];
    ntmn+= (lm1>lm2) ? lm1 : lm2;
#ifdef SLO0_DEBUG
    printf("lm1:%d lm2:%d ntmn:%d\n",lm1,lm2,ntmn);
#endif
    lm2=cdf->lll.llmx[i+1];
    ntmx+= (lm1>lm2) ? lm1 : lm2;  
#ifdef SLO0_DEBUG
    printf("lm1:%d lm2:%d ntmx:%d\n",lm1,lm2,ntmx);
#endif
  }

/*----------------------------------------------------------------------------*/
/* Add n-terminal offset of current segment to give alignment limits          */
/*----------------------------------------------------------------------------*/
  ntmn+=no[cs]+1;
  ntmx+=no[cs]+1;
#ifdef SLO0_DEBUG
  printf("final ntmn: %d final ntmx: %d\n",ntmn,ntmx);
#endif

/*----------------------------------------------------------------------------*/
/* Find alignment constraints arising from segments c-terminal to this one.   */
/*----------------------------------------------------------------------------*/
/* Allow for the minimum length of a tail                                     */
/*----------------------------------------------------------------------------*/
  ctmx=qsq->n-cdf->lll.llmn[nsc];

/*----------------------------------------------------------------------------*/
/* Initialize the minimum alignment allowed by c-terminal segments to the     */
/* start of the sequence.  They constrain alignment only one of the           */
/* c-terminal segments is already aligned or explicitly constrained.          */
/*----------------------------------------------------------------------------*/
  ctmn=-1;

/*----------------------------------------------------------------------------*/
/* Loop over all c-terminal segments                                          */
/*----------------------------------------------------------------------------*/
#ifdef SLO0_DEBUG
  printf("initial ctmn:%d initial ctmx:%d\n",ctmn,ctmx);
#endif
  for(i=nsc-1; i>cs; i--) {
/*----------------------------------------------------------------------------*/
/* Subtract segment length                                                    */	
/*----------------------------------------------------------------------------*/
    sl=no[i]+co[i]+1;		
    ctmn-=sl; 
    ctmx-=sl;
#ifdef SLO0_DEBUG
    printf("sl:%d ctmn:%d ctmx:%d\n",sl,ctmn,ctmx);
#endif
	
/*----------------------------------------------------------------------------*/
/* Reset limits to reflect any explicit constraints                           */
/*----------------------------------------------------------------------------*/
    if(qsq->sac.mn[i]>0) {
      ec=qsq->sac.mn[i]-no[i];
      ctmn=(ec>ctmn) ? ec : ctmn;
#ifdef SLO0_DEBUG
      printf("ec:%d ctmn:%d\n",ec,ctmn);
#endif
    }
    if(qsq->sac.mx[i]>0) {
      ec=qsq->sac.mx[i]-no[i];
      ctmx=(ec<ctmx) ? ec : ctmx;
#ifdef SLO0_DEBUG
      printf("ec:%d ctmx:%d\n",ec,ctmx);
#endif
    }

/*----------------------------------------------------------------------------*/
/* Subtract lengths of the preceeding loop                                    */
/*----------------------------------------------------------------------------*/
    lm1=mtf->mll[cdf->sll.rfpt[i-1]+co[i-1]][cdf->sll.rfpt[i]-no[i]];
    lm2=cdf->lll.llmn[i];
    ctmx-= (lm1>lm2) ? lm1 : lm2;
#ifdef SLO0_DEBUG
    printf("lm1:%d lm2:%d ctmx:%d\n",lm1,lm2,ctmx);
#endif
    lm2=cdf->lll.llmx[i];
    ctmn-= (lm1>lm2) ? lm1 : lm2;  
#ifdef SLO0_DEBUG
    printf("lm1:%d lm2:%d ctmn:%d\n",lm1,lm2,ctmn);
#endif
  }

/*----------------------------------------------------------------------------*/
/* Subtract c-terminal offset of current segment to give alignment limits     */
/*----------------------------------------------------------------------------*/
  ctmn-=co[cs]+1;
  ctmx-=co[cs]+1;
#ifdef SLO0_DEBUG
  printf("final ctmn:%d final ctmx:%d\n",ctmn,ctmx);
#endif

/*----------------------------------------------------------------------------*/
/* Signal an error if no alignment falls within the limits identified.        */
/* This means the core motif is too big to be threaded by the query sequence. */
/*----------------------------------------------------------------------------*/
  if(ntmn>ctmx||ntmx<ctmn) return(0); 

/*----------------------------------------------------------------------------*/
/* Reconcile n-terminal and c-terminal aligment limits                        */
/*----------------------------------------------------------------------------*/
  qmn=(ntmn>ctmn) ? ntmn : ctmn;
  qmx=(ntmx<ctmx) ? ntmx : ctmx;
#ifdef SLO0_DEBUG
  printf("qmn: %d qmx: %d\n",qmn,qmx);
#endif

/*----------------------------------------------------------------------------*/
/* Further reconcile any explict limits on the current segment alignment,     */
/* and signal an error if these constraints cannot be satisfied.              */
/*----------------------------------------------------------------------------*/

#ifdef SLO0_DEBUG
  printf("qsq->sac.mn[%d]: %d\n",cs,qsq->sac.mn[cs]);
#endif

  if(qsq->sac.mn[cs]>=0) {
    lm1=qsq->sac.mn[cs];
    if(lm1>qmx) return(0);
  }
/*----------------------------------------------------------------------------*/
/* if(lm1>qmn) qmn=lm1;}                                                      */
/*----------------------------------------------------------------------------*/
#ifdef SLO0_DEBUG
  printf("qsq->sac.mx[%d]: %d\n",cs,qsq->sac.mx[cs]);
#endif
  if(qsq->sac.mx[cs]>=0) {
    lm1=qsq->sac.mx[cs];
    if(lm1<qmn) return(0); }
/*----------------------------------------------------------------------------*/
/* if(lm1<qmx) qmx=lm1; }                                                     */
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
/* The maximum change in extent of the current core segment above it's        */
/* assumed minimal extent is the width of the alignment window.               */ 
/*----------------------------------------------------------------------------*/
  sl=qmx-qmn;
#ifdef SLO0_DEBUG
  printf("sl: %d\n",sl);
#endif

/*----------------------------------------------------------------------------*/
/* If this is negative the current segment cannot be fitted onto the query    */
/* sequence, even at it's minimum extent.  Return a zero error code.          */
/*----------------------------------------------------------------------------*/
  if(sl<0) return(0);

/*----------------------------------------------------------------------------*/
/* Define limits on of the current terminus using the maximum change in       */
/* extent. If this is greater than the maximum extent specified in the core   */
/* definition, set it to the latter.  Return the minimum extent as            */
/* specified in the core definition, and the maximum extent as determined.    */
/*----------------------------------------------------------------------------*/
  switch(ct) {
    case 0:{
      *mn=cdf->sll.nomn[cs];
      *mx=cdf->sll.nomx[cs];
      if((sl+(*mn))<*mx) *mx=sl+(*mn);
/*----------------------------------------------------------------------------*/
/*    if(sl<*mx) *mx=sl;                                                      */
/*----------------------------------------------------------------------------*/
      break;
    }
    case 1: {
      *mn=cdf->sll.comn[cs];
      *mx=cdf->sll.comx[cs];
      if((sl+(*mn))<*mx) *mx=sl+(*mn);
/*----------------------------------------------------------------------------*/
/*    if(sl<*mx) *mx=sl;                                                      */
/*----------------------------------------------------------------------------*/
      break;
    }
  }

#ifdef SLO0_DEBUG
  printf("mn: %d mx: %d\n",*mn,*mx);
#endif

/*----------------------------------------------------------------------------*/
/* Free local storage                                                         */
/*----------------------------------------------------------------------------*/
  free(no);
  free(co);

  return(1);
}
