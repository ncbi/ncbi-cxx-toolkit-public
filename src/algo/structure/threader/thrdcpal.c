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
* File Name:  thrdcpal.c
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

/* List pairwise side-chain contacts by segment for the current core, with */
/* a pointer to the corresponding level of the pairwise component of the */
/* contact potential.  Accumulate the hydrophobic component of side-chain to */
/* side-chain contacts, side-chain to peptide and side-chain to fixed-residue */
/* energies for each residue of a segment, for each possible residue type. */
/* This segment "profile" is used to compute the linear component of */
/* alternative-alignment energies, given the current core definition. */

/* Note that the profile terms include the hydrophobic component energy for */
/* contacts involving a fixed group, either peptide or fixed-type side chain. */
/* These terms are constant across alternative sequence assigments, and will */
/* cancel when subracting the reference state energy sums, but are included */
/* that the same reference state sum may be used for core segment aligment */
/* location sampling. */

/*int*/ void cpal(Rcx_Ptl* pmf, Cxl_Los** cpr, Cur_Loc* sli, Cxl_Als** cpa) {
/*-------------------------------------------------------*/
/* pmf:  Potential of mean force as a 3-d lookup table   */
/* cpr:  Contacts by segment, largest possible set       */
/* sli:	 Current locations of core segments in the motif */
/* cpa:  Contacts and profile terms at current location  */
/*-------------------------------------------------------*/

int	ppi;		/* Index of peptide group in contact potential */
int	nmt;		/* Number of motif residue positions */
int	nsc;		/* Number of threaded core segments */
int	nrt;		/* Number of residue types in potential */
int	i,j,k,l,n;	/* Counters */
int	r1,r2,d,t2;	/* Motif residue positions */
int	s1,s2;		/* Core segment indices */
int	*prt;		/* Pointer to columns in the contact potential */
int	*ert;		/* Pointer to columns in contact list energy profiles */
Cxl_Los *cr;	/* Pointer to and segment contact lists */
Cxl_Als *ca;	/* Pointer to and segment contact lists */


/* Parameters */

nsc=sli->nsc;
nmt=sli->nmt;
ppi=pmf->ppi;
nrt=pmf->nrt;

/* printf("nmt %d\n",nmt);
printf("ppi %d\n",ppi);
printf("nrt %d\n",nrt);
printf("nsc %d\n",nsc);  */


/* Zero pair counts */

for(i=0; i<nsc; i++) { ca=cpa[i]; ca->r.n=0; ca->rr.n=0; }


/* Identify core segment residue indices and zero profile energies */

for(i=0; i<nmt; i++) if(sli->cr[i]>=0) {
	ca=cpa[sli->cr[i]];
	n=ca->r.n;
	ca->r.r[n]=i;
	ert=ca->r.e[n]; for(j=0; j<nrt; j++) ert[j]=0;
	ca->r.n++;
	/* printf("position i:%d segment:%d count:%d\n",i, sli->cr[i],n);
	for(j=0; j<nrt; j++) printf("%d ",ert[j]); printf("ca->r.e[%d]\n",n); */
	}


/* Loop over core segments */
for(i=0; i<nsc; i++) {
	ca=cpa[i];
	cr=cpr[i];
	/* printf("segment list:%d\n",i); */

	/* Loop over residue-residue contacts in the reference list */
	for(j=0; j<cr->rr.n; j++) {

		/* Test that contact is present in the current core */
		r1=cr->rr.r1[j];
		s1=sli->cr[r1];
		if(s1<0) continue;
		r2=cr->rr.r2[j];
		s2=sli->cr[r2];
		if(s2<0) continue;
		d=cr->rr.d[j];

		/* Copy pairwise contact energy pointer to the pair list */
		k=ca->rr.n;
		ca->rr.r1[k]=r1;
		ca->rr.r2[k]=r2;
		ca->rr.e[k]=pmf->rre[d];
		ca->rr.n++;
		/* printf("rr pair:%d r1:%d s1:%d r2:%d s2:%d d:%d\n",
			j,r1,s1,r2,s2,d); */

		/* Add hydrophobic contact energy to the segment profile */
		prt=pmf->re[d];
		for(k=0;k<ca->r.n;k++) {
			if(ca->r.r[k]==r1) {
				ert=ca->r.e[k];
				for(l=0;l<nrt;l++) ert[l]+=prt[l];
				/* for(l=0; l<nrt; l++) printf("%d ",ert[l]);
				printf("ca->r.e[%d]\n",k); */
				}
			if(ca->r.r[k]==r2) {
				ert=ca->r.e[k];
				for(l=0;l<nrt;l++) ert[l]+=prt[l];
				/* for(l=0; l<nrt; l++) printf("%d ",ert[l]);
				printf("ca->r.e[%d]\n",k); */
				}
			}
		}



	/* Loop over residue-peptide contacts in the reference list */
	for(j=0; j<cr->rp.n; j++) {

		/* Test that the contact is present in the current core */
		r1=cr->rp.r1[j];
		s1=sli->cr[r1];
		if(s1<0) continue;
		r2=cr->rp.p2[j];
		s2=sli->cr[r2];
		if(s2<0) continue;
		d=cr->rp.d[j];
		/* printf("rp pair:%d r1:%d s1:%d r2:%d s2:%d d:%d\n",
			j,r1,s1,r2,s2,d); */

		/* Add total peptide contact energy to the segment profile */
		for(k=0;k<ca->r.n;k++) if(ca->r.r[k]==r1) {
				ert=ca->r.e[k];
				prt=pmf->rrt[d][ppi];
				for(l=0;l<nrt;l++) ert[l]+=prt[l];
				/* for(l=0; l<nrt; l++) printf("%d ",ert[l]);
				printf("ca->r.e[%d]\n",k); */
				} }


	/* Loop over residue-fixed contacts in the reference list */
	for(j=0; j<cr->rf.n; j++) {

		/* Test that the contact is present in the current core */
		r1=cr->rf.r1[j];
		s1=sli->cr[r1];
		if(s1<0) continue;
		t2=cr->rf.t2[j];
		d=cr->rf.d[j];
		/* printf("rp pair:%d r1:%d s1:%d t2:%d d:%d\n",
			j,r1,s1,t2,d); */

		/* Add fixed contact energy to the segment profile */
		for(k=0;k<ca->r.n;k++) if(ca->r.r[k]==r1) {
			ert=ca->r.e[k];
			prt=pmf->rrt[d][t2];
			for(l=0;l<nrt;l++) ert[l]+=prt[l];
			/* for(l=0; l<nrt; l++) printf("%d ",ert[l]);
			printf("ca->r.e[%d]\n",k); */
			} }

	}
}
