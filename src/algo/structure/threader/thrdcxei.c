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
* File Name:  thrdcxei.c
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

/* Compute the expected energy of a threaded core contact across random */
/* permutations of the threaded sequence.  This is a function of the */
/* composition of the threaded sequence, spc, the contact potential, pmf, */
/* and the type-frequency of fixed-residue contacts, within spn. Expected */
/* energies of side-chain to side-chain, side-chain to peptide, and */
/* side-chain to fixed-residue contacts are stored in cxe.  Pairwise and */
/* hydrophobic components of the potential are included in the sums. */

/*int*/ void cxei(Seg_Nsm* spn, Seg_Cmp* spc, Rcx_Ptl* pmf, Cur_Loc* sli, Seq_Mtf* psm, Cor_Def* cdf, Thd_Cxe* cxe) {
/*--------------------------------------------------------*/
/* spn:  Partial sums of contact counts by segment pair   */
/* spc:  Residue type composition of current threaded seq */
/* pmf:  Potential of mean force as a 3-d lookup table    */
/* cxe:  Expected contact energy for the current thread   */
/*--------------------------------------------------------*/

int     nrt;            /* Number of residue types in potential */
int     ndi;            /* Number of distance intervals in potential */
int     ppi;            /* Index of peptide group in contact potential */
int     i,j,k;		/* Counters */
int     rn;             /* Residue count */
int     rrn;            /* Product of residue counts */
int     ism;            /* Integer sum of residues counts or count products */
float   fsm;            /* Floating sum of counts or count products */
float   *rri;           /* Pointer to residue pair probabilities */
int	**pmd;		/* Pointer to distance level of potential */
int	*pmr;		/* Pointer to a residue-row of the potential */
int     *fnd;           /* Fixed contact counts for a distance interval */
int     fnr;            /* Fixed contact counts for a residue type */
int     nsc;            /* Number of threaded segments in core definition */
int     s0;
int     mn, mx;
int     ntot/*, ms*/;

nrt=cxe->nrt;
ndi=cxe->ndi;
ppi=pmf->ppi;
nsc=spc->nsc;
/* printf("cxe->nrt:%d ndi:%d pmf->ppi:%d\n",cxe->nrt,cxe->ndi,pmf->ppi); */

/* Compute residue type frequency from counts */

ism=0; for(i=0;i<nrt;i++) ism+=spc->rt[i];
fsm=(float)ism;
for(i=0;i<nrt;i++) cxe->rp[i]=((float)spc->rt[i])/fsm;
/* for(i=0; i<nrt; i++) printf("%d ",spc->rt[i]); printf("spc->rt\n"); */
/* for(i=0; i<nrt; i++) printf("%.4f ",cxe->rp[i]); printf("cxe->rp\n"); */


/* Compute pairwice contact probabilities */

ism=0; for(i=0; i<nrt; i++) {
	rn=spc->rt[i];
	rri=cxe->rrp[i];
	for(j=0; j<=i; j++) {
		rrn=rn*spc->rt[j];
		ism+=rrn;
		rri[j]=(float)rrn; } }

/* for(j=0; j<nrt; j++) {
	for(i=0;i<nrt;i++) printf("%.0f ",cxe->rrp[j][i]);
	printf("cxe->rrp[%d]\n",j); } */

fsm=(float)ism;
for(i=0; i<nrt; i++) {
	rri=cxe->rrp[i];
	for(j=0; j<=i; j++) {
		rri[j]=rri[j]/fsm; } }

/* for(j=0; j<nrt; j++) {
	for(i=0;i<nrt;i++) printf("%.4f ",cxe->rrp[j][i]);
	printf("cxe->rrp[%d]\n",j); } */


/* Compute expected residue-peptide contact energy by distance interval. */

for(i=0; i<ndi; i++) {
	if(spn->srp[i]==0) {
		cxe->rpe[i]=0.;
		continue; }
	fsm=0.;
	pmr=pmf->rrt[i][ppi];
	for(j=0;j<nrt;j++) fsm+=((float)pmr[j])*cxe->rp[j];
	cxe->rpe[i]=fsm;
	}
/* for(i=0; i<ndi; i++) printf("%.2f ",cxe->rpe[i]); printf("cxe->rpe\n"); */


/* Compute expected residue-residue contact energy by distance interval. */

for(i=0; i<ndi; i++) {
	if(spn->srr[i]==0) {
		cxe->rre[i]=0.;
		continue; }
	fsm=0;
	pmd=pmf->rrt[i];
	for(j=0; j<nrt; j++) {
		pmr=pmd[j];
		rri=cxe->rrp[j];
		for(k=0; k<=j; k++) fsm+=((float)pmr[k])*rri[k]; }
	cxe->rre[i]=fsm; }
/* for(i=0; i<ndi; i++) printf("%.4f ",cxe->rre[i]); printf("cxe->rre\n"); */


/* Compute expected residue-fixed energy */

for(i=0; i<ndi; i++) {
	if(spn->srf[i]==0) {
		cxe->rfe[i]=0.;
		continue; }
	fsm=0.;
	fnd=spn->frf[i];
	pmd=pmf->rrt[i];
	for(j=0; j<nrt; j++) {
		fnr=fnd[j];
		if(fnr==0) continue;
		pmr=pmd[j];
		for(k=0; k<nrt; k++) fsm+=((float)(fnr*pmr[k]))*cxe->rp[k]; }
	cxe->rfe[i]=fsm/(float)spn->srf[i]; }
/* for(i=0; i<ndi; i++) printf("%.4f ",cxe->rfe[i]); printf("cxe->rfe\n"); */

/* Compute expected energy for template sequence motif and profile-profile term */

/*ms=0;*/ s0=0;
nrt=spc->nrt;

for(i=0;i<nsc;i++){

	mn=cdf->sll.rfpt[i]-sli->no[i];
	mx=cdf->sll.rfpt[i]+sli->co[i];

  for(j=mn;j<=mx;j++) {
    for(k=0;k<(nrt-1);k++){
      s0+=psm->ww[j][k]*spc->rt[k];
    }
  }
}

ntot=0;
for(i=0;i<(nrt-1);i++)ntot+=spc->rt[i];

psm->ww0=((float)s0)/ntot;

}
