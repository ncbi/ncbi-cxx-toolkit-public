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
* File Name:  thrdslor.c
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

/* Find allowed extent range for a core segment */

/* Find limiting values for location of a core segment terminus, given the */
/* extent and alignment of other segments, loop length ranges, and the */
/* length of the query sequence. */

/* The algorithm used begins by defining the interval on the query sequence */
/* which is available for assignment to the loop plus extenstion of the core */
/* segment beyond it's minimum extent.  Beginning at the current segment */
/* extent, it moves the segment endpoint towards it's minimum, until either */
/* the minimum extent is reached, or a loop length inconsistent with motif- */
/* derived limits is encounterd (see comment below).  Beginning again with */
/* the current extent, the algorithm then moves the segment endpoint towards */
/* its maximum, until either the maxixmum extent is reached, or the portion */
/* of the query sequence interval remaining for the loop decreases below it's */
/* explicit or motif-derived limit.  The resulting range is continuous, and */
/* contains at least one value, the current or starting extent. */

int slor(Fld_Mtf* mtf, Cor_Def* cdf, Qry_Seq* qsq, Cur_Loc* sli, Cur_Aln* sai,
         int cs, int ct, int* mn, int*mx) {
/*------------------------------------------------------------*/
/* mtf:    Contact matrices defining the folding motif        */
/* cdf:    Contains specified loop length limits              */
/* qsq:    Sequence to thread with alignment contraints       */
/* sli:    Contains current segment location values           */
/* sai:    Contains current segment alignment values          */
/* cs:     The segment for which constraints are to be found  */
/* ct:     The terminus for which constraints are to be found */
/* mn,mx:  The range of allowed aligments for this segment    */
/*------------------------------------------------------------*/

int	nsc;		/* Number of core segments */
int	i;		/* Counter */
int	nr;		/* Endpoint residue position in neighboring segment */
int	qs,qf;		/* Start and finish of a query sequence interval */
int	ql;		/* Limit on loop plus extentsion length by query seq */
int	ns;		/* Neighbor segment index */
int	mxo,mno;	/* Explicit maximum and minimum segment extent */
int	cuo;		/* Current segment extent */
int	cua;		/* Current segment alignment */
int	*mp=NULL;	/* Motif-derived loop length limits */
int	l1;		/* Minimum loop length allowed by core definition */
int	l2;		/* Minimum loop length allowed by folding motif */
int	ll;		/* Loop length implied by a given segment extent */
int	lm;		/* Reconciled loop length limit */
int	cr;		/* Central residue of this core segment */
int	q1,q2;		/* Limits on query sequence interval finish */

/* Number of core segments */
nsc=cdf->sll.n;

/* for(i=0;i<nsc;i++) printf("%d ",sai->al[i]); printf("sai->al\n");
for(i=0;i<sai->nmt;i++) printf("%d ",sai->sq[i]); printf("sai->sq\n");
for(i=0;i<nsc;i++) printf("%d ",sli->no[i]); printf("sli->no\n");
for(i=0;i<nsc;i++) printf("%d ",sli->co[i]); printf("sli->co\n");
for(i=0;i<sli->nlp;i++) printf("%d ",sli->lp[i]); printf("sli->lp\n");
for(i=0;i<sli->nmt;i++) printf("%d ",sli->cr[i]); printf("sli->cr\n"); */

/* Current alignment of this segment */
cua=sai->al[cs];

/* Central residue of this segment */
cr=cdf->sll.rfpt[cs];

switch(ct) {
	
	case 0: {	/* N-terminal limits */

		if(cs>0) { ns=cs-1;
		
			/* Start of query sequence interval */
			qs=sai->al[ns]+sli->co[ns]+1;
		
			/* Finish of query sequence interval */
			q1=cua-cdf->sll.nomn[cs]-1;	
			q2=qs+cdf->lll.llmx[cs]-1;
			qf=(q2<q1) ? q2 : q1;

			/* Last position of neighboring core segment */
			nr=cdf->sll.rfpt[ns]+sli->co[ns]; 
			/* printf("q1:%d q2:%d nr:%d\n",q1,q2,nr); */
			
			/* Pointer to motif-derived loop length limits */
			mp=mtf->mll[nr];
			
			}

		else {	
			/* Start of query sequence interval */
			/* qs=0+cdf->lll.llmn[cs]; */
			qs=0;

			/* Finish of query sequence interval */
			qf=cua-cdf->sll.nomn[cs]-1;	
	
			/* Flag value for last position of preceding segment */
			nr=-1; }
		

		/* Minimum extent allowed by alignment and explicit limit */
		mno=cua-qf-1; 
		
		/* Maximum extent allowed by core definition */
		mxo=cdf->sll.nomx[cs];

		/* Explicit minimum loop length */
		l1=cdf->lll.llmn[cs]; 
	
		/* Length of query sequence interval */
		ql=qf-qs+1;

		/* Current extent of this segment */
		cuo=sli->no[cs];

		/* Check current extent against motif loop limits */
		l2=(nr<0) ? l1 : mp[cr-cuo];
		lm=(l1>l2) ? l1:l2;
		ll=ql-(cuo-mno);
/* printf("cs:%d ct:0 qs:%d qf:%d cuo:%d mno:%d mxo:%d l1:%d l2:%d ll:%d\n",
	cs,qs,qf,cuo,mno,mxo,l1,l2,ll); */
		if(ll<lm) return(0);
		
		/* Find the minimum extent consistent with motif loop limits. */
		/* This is expected to be the minimum extent given explicitly */
		/* in the core definition, since by the triangle inequality */
		/* one may expect to remove residues from the terminus of the */
		/* segment and add them to the loop, and always have a loop */
		/* long enough to reach the neighboring segment.  This must */
		/* be checked, however, since coordinate imprecision allows */
		/* occasional violations of this inequality in the matrix of */
		/* motif-derived loop limits.  The minimum identified here */
		/* will define a continuous extent range that includes the */
		/* present extent, and has at least this one valid value. */

		*mn=cuo;
		for(i=cuo-1; i>=mno; i--) {

			/* Length of loop implied by this extent */
			ll=ql-(i-mno);

			/* Motif-derived minimum loop length */
			l2=(nr<0) ? l1 : mp[cr-i];

			/* Greater of fixed and motif-derived loop limits */
			lm=(l1>l2) ? l1:l2;
			/* printf("mn:%d ll:%d l1:%d l2:%d lm:%d nr2:%d\n",
				i,ll,l1,l2,lm,cr-i); */
		
			/* Set minimum extent if loop length is allowed */
			if(ll>=lm) *mn=i; else break;
			
			}
		

		/* Find the maximum extent consistent with motif and  */
		/* explicit minimum loop limits.  This maxixum varies with */
		/* the aligment, which will have assigned varying numbers */
		/* of residues to the extent-plus-loop interval on the query */
		/* sequence. */

		*mx=cuo;
		for(i=cuo+1; i<=mxo; i++) {
	
			/* Length of adjacent loop implied by this extent */
			ll=ql-(i-mno);
	
			/* Motif-derived minimum loop length */
			l2=(nr<0) ? l1 : mp[cr-i];

			/* Greater of fixed and motif-derived loop limits */
			lm=(l1>l2) ? l1:l2;
			/* printf("mx:%d ll:%d l1:%d l2:%d lm:%d nr2:%d\n",
				i,ll,l1,l2,lm,cr-i); */

			/* If loop length is above minimum, increase extent */ 
			if(ll>=lm) *mx=i; else break; } 
	
		break;
		}


	case 1: {	/* C-terminal limits */
			

		if(cs<(nsc-1)) { ns=cs+1;

			/* Finish of query sequence interval */
			qf=sai->al[ns]-sli->no[ns]-1;
		
			/* Start of query sequence interval */
			q1=cua+cdf->sll.comn[cs]+1;	
			q2=qf-cdf->lll.llmx[ns]+1;
			qs=(q2>q1) ? q2 : q1;

			/* Last position of following core segment */
			nr=cdf->sll.rfpt[ns]-sli->no[ns]; 
			/* printf("q1:%d q2:%d nr:%d\n",q1,q2,nr); */
			
			/* Pointer to motif-derived loop length limits */
			mp=mtf->mll[nr];
			}
		
		else {	
			/* Start of query sequence interval */	
			/* qf=qsq->n-1-cdf->lll.llmn[nsc]; */
			qf=qsq->n-1;

			/* Finish of query sequence interval */
			qs=cua+cdf->sll.comn[cs]+1;	
	
			/* Flag value for last position of preceding segment */
			nr=-1; }

		/* Length of query sequence interval */
		ql=qf-qs+1;
		
		/* Minimum extent allowed by alignment and explicit limit */
		mno=qs-cua-1;

		/* Maximum extent allowed by core definition */
		mxo=cdf->sll.comx[cs];

		/* Explicit minimum loop length */
		l1=cdf->lll.llmn[cs+1]; 
		
		/* Current extent of this segment */
		cuo=sli->co[cs];

		/* Check current extent against motif loop limits */
		l2=(nr<0) ? l1 : mp[cr+cuo];
		lm=(l1>l2) ? l1:l2;
		ll=ql-(cuo-mno);
/* printf("cs:%d ct:1 qs:%d qf:%d cuo:%d mno:%d mxo:%d l1:%d l2:%d ll:%d\n",
	cs,qs,qf,cuo,mno,mxo,l1,l2,ll); */
		if(ll<lm) return(0);
		
		/* Find the minimum extent consistent with motif loop limits. */
		/* This is expected to be the minimum extent given explicitly */
		/* in the core definition, since by the triangle inequality */
		/* one may expect to remove residues from the terminus of the */
		/* segment and add them to the loop, and always have a loop */
		/* long enough to reach the neighboring segment.  This must */
		/* be checked, however, since coordinate imprecision allows */
		/* occasional violations of this inequality in the matrix of */
		/* motif-derived loop limits.  The minimum identified here */
		/* will define a continuous extent range that includes the */
		/* present extent, and has at least this one valid value. */

		*mn=cuo;
		for(i=cuo-1; i>=mno; i--) {

			/* Length of loop implied by this extent */
			ll=ql-(i-mno);

			/* Motif-derived minimum loop length */
			l2=(nr<0) ? l1 : mp[cr+i];

			/* Greater of fixed and motif-derived loop limits */
			lm=(l1>l2) ? l1:l2;
			/* printf("mn:%d ll:%d l1:%d l2:%d lm:%d nr2:%d\n",
				i,ll,l1,l2,lm,cr+i); */
		
			/* Set minimum extent if loop length is allowed */
			if(ll>=lm) *mn=i; else break;
			
			}
		
		
		/* Find the maximum extent consistent with motif and  */
		/* explicit minimum loop limits.  This maxixum varies with */
		/* the aligment, which will have assigned varying numbers */
		/* of residues to the extent-plus-loop interval on the query */
		/* sequence. */

		*mx=cuo; 
		for(i=cuo+1; i<=mxo; i++) {
	
			/* Length of adjacent loop implied by this extent */
			ll=ql-(i-mno);
	
			/* Motif-derived minimum loop length */
			l2=(nr<0) ? l1 : mp[cr+i];

			/* Greater of fixed and motif-derived loop limits */
			lm=(l1>l2) ? l1:l2;
			/* printf("mx:%d ll:%d l1:%d l2:%d lm:%d nr2:%d\n",
				i,ll,l1,l2,lm,cr+i); */

			/* If loop length is above minimum, increase extent */ 
			if(ll>=lm) *mx=i; else break; } 
		
		break;
		}

	}


/* printf("mn:%d mx:%d\n",*mn,*mx); */

return(1);
}
