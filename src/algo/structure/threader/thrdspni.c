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
* File Name:  thrdspni.c
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

/* Sum contact counts for the n-th core segment. Use these sums to recompute */
/* total contact counts. */

/*int*/ void spni(Cxl_Los** cpl, Cur_Loc* sli, int n, Seg_Nsm* spn) {
/*-------------------------------------------------------*/
/* cpl:  Contacts by segment, given current alignment    */
/* sli:  Current locations of core segments in the motif */
/* n:    Update composition for this core segment only   */
/* spn:  Partial sums of contact counts by segment pair  */
/*-------------------------------------------------------*/

int	nsc;		/* Number of core segments */
int	ndi;		/* Number of distance intervals */
int	nrt;		/* Number of residue types in potential */
int	i,j,k;		/* Counters */
int	s1,s2;		/* Core segment indices */
int	t2;		/* Residue type index */
Cxl_Los *cl;	/* Pointer to contact list for current segment */
int	**nni,*nnj;	/* Pointers to contact counts in pair tables */
int	*nsi;		/* Pointer to sum of contact counts */
int	rn;		/* Residue count */

/* Parameters */

nsc=spn->nsc;
nrt=spn->nrt;
ndi=spn->ndi;


/* Set pointer to contact list of current segment */

cl=cpl[n];

/* printf("n:%d\n",n);
printf("rr:%d rp:%d rf:%d cpl[%d]\n",cl->rr.n,cl->rp.n,cl->rf.n,n); */

/* printf("rr:\n");
for(j=0; j<cl->rr.n; j++) {
	printf("%d %d %d\n",cl->rr.r1[j],cl->rr.r2[j],cl->rr.d[j]); }
printf("rp:\n");
for(j=0; j<cl->rp.n; j++) {
	printf("%d %d %d\n",cl->rp.r1[j],cl->rp.p2[j],cl->rp.d[j]); }
printf("rf:\n");
for(j=0; j<cl->rf.n; j++) {
	printf("%d %d %d\n",cl->rf.r1[j],cl->rf.t2[j],cl->rf.d[j]); }
for(i=0;i<sli->nmt;i++) printf("%d ",sli->cr[i]); printf("sli->cr\n"); */

/* Zero residue-residue contact counts */

for(i=0;i<ndi;i++) {
	nni=spn->nrr[i];
	for(j=0; j<nsc; j++) { nni[n][j]=0; nni[j][n]=0; } }


/* printf("rr before %d:\n",n); for(k=0; k<ndi; k++) for(j=0; j<nsc; j++) {
	for(i=0; i<nsc; i++) printf("%d ",spn->nrr[k][j][i]);
	printf("spn->nrr[%d][%d]\n",k,j); } */

/* Loop over residue-residue contacts in the reference list */

for(i=0; i<cl->rr.n; i++) {

	/* Increment counts if contact is within the current core */
	s1=sli->cr[cl->rr.r1[i]];
	/* printf("i:%d s1:%d\n",i,s1); */
	if(s1<0) continue;
	s2=sli->cr[cl->rr.r2[i]];
	/* printf("s2:%d\n",s2); */
	if(s2<0) continue;
	spn->nrr[cl->rr.d[i]][s1][s2]++;
	/* printf("spn->nrr[%d][%d][%d]:%d\n",cl->rr.d[i],s1,s2,
		spn->nrr[cl->rr.d[i]][s1][s2]); */
	}

/* for(k=0; k<ndi; k++) for(j=0; j<nsc; j++) {
	for(i=0; i<nsc; i++) printf("%d ",spn->nrr[k][j][i]);
	printf("spn->nrr[%d][%d]\n",k,j); } */


/* Zero residue-peptide contact counts */

for(i=0;i<ndi;i++) {
	nni=spn->nrp[i];
	for(j=0; j<nsc; j++) { nni[n][j]=0; nni[j][n]=0; } }

/* printf("rp before %d:\n",n); for(k=0; k<ndi; k++) for(j=0; j<nsc; j++) {
	for(i=0; i<nsc; i++) printf("%d ",spn->nrp[k][j][i]);
	printf("spn->nrp[%d][%d]\n",k,j); } */

/* Loop over residue-peptide contacts in the reference list */

for(i=0; i<cl->rp.n; i++) {

	/* Increment counts if contact is within the current core */
	s1=sli->cr[cl->rp.r1[i]];
	/* printf("i:%d s1:%d\n",i,s1); */
	if(s1<0) continue;
	s2=sli->cr[cl->rp.p2[i]];
	/* printf("s2:%d\n",s2);  */
	if(s2<0) continue;
	spn->nrp[cl->rp.d[i]][s1][s2]++;
	/* printf("spn->nrp[%d][%d][%d]:%d\n",cl->rp.d[i],s1,s2,
		spn->nrp[cl->rp.d[i]][s1][s2]);  */
	}

/* for(k=0; k<ndi; k++) for(j=0; j<nsc; j++) {
	for(i=0; i<nsc; i++) printf("%d ",spn->nrp[k][j][i]);
	printf("spn->nrp[%d][%d]\n",k,j); } */


/* Sum residue-residue contact counts */

for(i=0;i<ndi;i++) {
	rn=0;
	nni=spn->nrr[i];
	for(j=0;j<nsc;j++) {
		nnj=nni[j];
		for(k=0;k<nsc;k++) rn+=nnj[k]; }
	spn->srr[i]=rn; }

/* for(k=0; k<ndi; k++) printf("%d ",spn->srr[k]); printf("spn->srr\n");  */


/* Sum residue-peptide contact counts */

for(i=0;i<ndi;i++) spn->srp[i]=0;
for(i=0;i<ndi;i++) {
	rn=0;
	nni=spn->nrp[i];
	for(j=0;j<nsc;j++) {
		nnj=nni[j];
		for(k=0;k<nsc;k++) rn+=nnj[k]; }
	spn->srp[i]=rn; }

/* for(k=0; k<ndi; k++) printf("%d ",spn->srp[k]); printf("spn->srp\n"); */


/* Zero residue-fixed contact counts */

for(i=0;i<ndi;i++) {
	nni=spn->nrf[i];
	for(j=0; j<nrt; j++) nni[n][j]=0; }


/* Loop over residue-fixed contacts in the reference list */

for(i=0; i<cl->rf.n; i++) {

	/* Increment counts if contact is within the current core */
	s1=sli->cr[cl->rf.r1[i]];
	if(s1<0) continue;
	t2=cl->rf.t2[i];
	spn->nrf[cl->rf.d[i]][s1][t2]++; }

/* for(k=0; k<ndi; k++) for(j=0; j<nsc; j++) {
	for(i=0; i<nrt; i++) printf("%d ",spn->nrf[k][j][i]);
	printf("spn->nrf[%d][%d]\n",k,j); } */


/* Sum residue-fixed contact counts */

for(i=0;i<ndi;i++) {
	nsi=spn->frf[i];
	for(j=0;j<nrt;j++) nsi[j]=0; }

for(i=0;i<ndi;i++) {
	nni=spn->nrf[i];
	nsi=spn->frf[i];
	for(j=0;j<nsc;j++) {
		nnj=nni[j];
		for(k=0;k<nrt;k++) nsi[k]+=nnj[k]; } }

/* for(k=0; k<ndi; k++) {
	for(i=0; i<nrt; i++) printf("%d ",spn->frf[k][i]);
	printf("spn->frf[%d][%d]\n",k,j); } */


spn->trf=0; for(i=0; i<ndi; i++) {
	nsi=spn->frf[i];
	rn=0; for(j=0; j<nrt; j++) rn+=nsi[j];
	spn->srf[i]=rn;
	spn->trf+=rn; }

/* for(k=0; k<ndi; k++) printf("%d ",spn->srf[k]); printf("spn->srf\n");  */

}
