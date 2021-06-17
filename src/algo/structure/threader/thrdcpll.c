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
* File Name:  thrdcpll.c
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

/* Given the current alignment, construct by-segment contact lists for */
/* location sampling.  These lists contain the same information as the */
/* largest-extent or reference contact lists, but some contacts may be */
/* omitted, if loop length limits or sequence length constrain the possible */
/* n- or c-terminal offsets of a segment.  They contain pointers to the */
/* energies of each contact, as well as distance intervals.  */

/*int*/ void cpll(Cor_Def* cdf, Rcx_Ptl* pmf, Qry_Seq* qsq, Cxl_Los** cpr,
         Cur_Aln* sai, Cxl_Los** cpl) {
/*-----------------------------------------------------*/
/* cdf:  Core segment locations and loop length limits */
/* pmf:  Potential of mean force as a 3-d lookup table */
/* qsq:  Sequence to thread with alignment contraints  */
/* cpr:  Contacts by segment, largest possible set     */
/* sai:  Current alignment of query sequence with core */
/* cpl:  Contacts by segment, given current alignment  */
/*-----------------------------------------------------*/

int	nmt;		/* Number of motif residue positions */
int	nsc;		/* Number of threaded core segments */
int	ppi; 		/* Index of peptide group in potential */
/*int	nrt;*/		/* Number of residue types */
int	nqi;		/* Number of residues in query sequence */
int	i,j,k;		/* Counters */
int	t1,t2;		/* Motif residue types */
int	r1,r2=0;	/* Motif residue positions */
int	s1,s2;		/* Core segment indices */
int	d;		/* Distance inteva */
Cxl_Los *cr;	/* Pointer to segment reference contact lists */
Cxl_Los *cl;	/* Pointer to segment location sampling contact lists */
int	*cf;		/* Flags residues possibly within the core. */
int	le;		/* Explicit limit on segment extent, query index */
int	la;		/* Alignment-derived limit on segment extent */
int	mn,mx;		/* Range */

/* Parameters */

nsc=sai->nsc;
nmt=sai->nmt;
ppi=pmf->ppi;
/*nrt=pmf->nrt;*/
nqi=qsq->n;


/* printf("nsc %d\n",nsc);
printf("nmt %d\n",nmt);
printf("ppi %d\n",ppi);
printf("nrt %d\n",nrt);
printf("nqi %d\n",nqi); */



/* Flag residues which may fall in the core, given the current alignment */

cf=sai->cf;
for(i=0;i<nmt;i++) cf[i]=(-1);
/* for(i=0; i<nmt; i++) printf("%d ",cf[i]); printf("cf\n"); */

for(i=0; i<nsc; i++) {

	/* Identify maximum n-terminal extent of this segment */
	le=sai->al[i]-cdf->sll.nomx[i];
	la=(i==0) ? cdf->lll.llmn[0]:
		sai->al[i-1]+cdf->sll.comn[i-1]+cdf->lll.llmn[i]+1;
	/* printf("nt-le:%d nt-la:%d\n",le,la); */
	le=(la>le) ? la : le;
	mn=cdf->sll.rfpt[i]-(sai->al[i]-le);
	/* printf("nt-le:%d nt-mn:%d\n",le,mn); */

	/* Identify maximum c-terminal extent of this segment */
	le=sai->al[i]+cdf->sll.comx[i];
	la=(i==(nsc-1)) ? nqi-1-cdf->lll.llmn[nsc]:
		sai->al[i+1]-cdf->sll.nomn[i+1]-cdf->lll.llmn[i+1]-1;
	/* printf("ct-le:%d ct-la:%d\n",le,la); */
	le=(la<le) ? la : le;
	mx=cdf->sll.rfpt[i]+(le-sai->al[i]);
	/* printf("ct-le:%d ct-mx:%d\n",le,mx); */

	/* Flag possible core residues */
	/* printf("mn:%d mx:%d\n",mn,mx); */
	for(j=mn; j<=mx; j++) cf[j]=i; }

/* for(i=0; i<nmt; i++) printf("%d ",cf[i]); printf("cf\n"); */


/* Zero pair counts */

for(i=0; i<nsc; i++) { cl=cpl[i]; cl->rr.n=0; cl->rp.n=0; cl->rf.n=0;}


/* Loop over core segments */

for(i=0; i<nsc; i++ ) {
	cl=cpl[i];
	cr=cpr[i];

	/* Loop over residue-residue contacts in the reference list */

	for(j=0; j<cr->rr.n; j++) {

		/* Test that contact is within the allowed extent range */
		r1=cr->rr.r1[j];
		s1=cf[r1];
		if(s1<0) continue;
		t1=qsq->sq[sai->al[s1]-(cdf->sll.rfpt[s1]-r1)];
		if(t1<0) continue;
		r2=cr->rr.r2[j];
		s2=cf[r2];
		if(s2<0) continue;
		t2=qsq->sq[sai->al[s2]-(cdf->sll.rfpt[s2]-r2)];
		if(t2<0) continue;
		d=cr->rr.d[j];

		/* Copy contact to the location-sampling pair list */
		k=cl->rr.n;
		cl->rr.r1[k]=r1;
		cl->rr.r2[k]=r2;
		cl->rr.d[k]=d;
		cl->rr.e[k]=pmf->rrt[d][t1][t2];
		cl->rr.n++;
	/* printf("j:%d k:%d s1:%d s2:%d r1:%d r2:%d t1:%d t2:%d d:%d e:%d\n",
		j,k,s1,s2,r1,r2,t1,t2,d,cl->rr.e[k]); */

		}


	/* Loop over residue-peptide contacts in the reference list */
	for(j=0; j<cr->rp.n; j++) {

		/* Test that the contact is present in the current core */
		r1=cr->rp.r1[j];
		s1=cf[r1];
		if(s1<0) continue;
		t1=qsq->sq[sai->al[s1]-(cdf->sll.rfpt[s1]-r1)];
		if(t1<0) continue;
		r2=cr->rp.p2[j];
		s2=cf[r2];
		if(s2<0) continue;
		d=cr->rp.d[j];

		/* Copy contact to the location-sampling pair list */
		k=cl->rp.n;
		cl->rp.r1[k]=r1;
		cl->rp.p2[k]=r2;
		cl->rp.d[k]=d;
		cl->rp.e[k]=pmf->rrt[d][t1][ppi];
		cl->rp.n++;
	/* printf("j:%d k:%d s1:%d s2:%d r1:%d r2:%d t1:%d t2:%d d:%d e:%d\n",
		j,k,s1,s2,r1,r2,t1,ppi,d,cl->rp.e[k]); */
		}


	/* Loop over residue-fixed contacts in the reference list */
	for(j=0; j<cr->rf.n; j++) {

		/* Test that the contact is present in the current core */
		r1=cr->rf.r1[j];
		s1=cf[r1];
		if(s1<0) continue;
		t1=qsq->sq[sai->al[s1]-(cdf->sll.rfpt[s1]-r1)];
		if(t1<0) continue;
		t2=cr->rf.t2[j];
		d=cr->rf.d[j];

		/* Copy contact to the location-sampling pair list */
		k=cl->rf.n;
		cl->rf.r1[k]=r1;
		cl->rf.t2[k]=r2;
		cl->rf.d[k]=d;
		cl->rf.e[k]=pmf->rrt[d][t1][t2];
		cl->rf.n++;
	/* printf("j:%d k:%d s1:%d s2:%d r1:%d r2:%d t1:%d t2:%d d:%d e:%d\n",
		j,k,s1,s2,r1,r2,t1,ppi,d,cl->rf.e[k]); */
		}
	}

}
