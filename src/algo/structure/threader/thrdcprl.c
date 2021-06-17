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
* File Name:  thrdcprl.c
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

/* Count and/or list largest possible sets of contact indices by segment. */
/* The flag ct specifies whether contacts should only be counted, or whether */
/* their indices should be stored.  The former option is used to determine */
/* the required memory allocation. */

/* Organize residue-residue contacts as a list of paired indices, with an */
/* associated distance interval.  Organize residue peptide contacts as a */
/* similar list.  Organize residue-fixed environment contacts as a list of */
/* indices and types. */

/*int*/ void cprl(Fld_Mtf* mtf, Cor_Def* cdf, Rcx_Ptl* pmf, Cxl_Los** cpr, int ct) {
/*--------------------------------------------------------*/
/* mtf:  Contact matrices defining the folding motif      */
/* cdf:  contains min/max segment locations               */
/* pmf:  Potential of mean force as a 3-d lookup table    */
/* cpr:  indices of contacts by seg, largest possible set */
/* ct:   If 0, count contacts but do not assign indices   */
/*--------------------------------------------------------*/

int	ppi;		/* Index of peptide group in contact potential */
int	nmt;		/* Number of motif residue positions */
int	nsc;		/* Number of threaded core segments */
int	nfs;		/* Number of fixed core segments */
int	*cs;		/* Flag for core residue positions  */
int	*fs;		/* Flag for fixed residue positions */
int	i,j;		/* Counters */
int	mn,mx;		/* Ranges */
int	r1,r2,d;	/* Motif residue positions */
int	s1,s2;		/* Core segment indices */
int	f1,f2;		/* Fixed segment residue positions */
int	t2;		/* Fixed segment residue type */
Cxl_Los *cp;	/* Pointer to segment contact lists */


/* Parameters */

nsc=cdf->sll.n;
nfs=cdf->fll.n;
nmt=mtf->n;
ppi=pmf->ppi;

/*
printf("nsc %d\n",nsc);
printf("nfs %d\n",nfs);
printf("nmt %d\n",nmt);
printf("ppi %d\n",ppi);
*/


/* Local storage for identifying residue positions by segment */

cs=(int *)calloc(nmt,sizeof(int));
fs=(int *)calloc(nmt,sizeof(int));


/* Zero contact counts  */

for(i=0; i<nsc; i++) { cp=cpr[i]; cp->rr.n=0; cp->rp.n=0; cp->rf.n=0; }

/*
for(i=0; i<nsc; i++) {
	cp=cpr[i];
	printf("rr:%d rp:%d rf:%d cpr[%d]\n",cp->rr.n,cp->rp.n,cp->rf.n,i); }
*/

/* Flag residue positions by core segment, at segments maximum extent */

for(i=0; i<nmt; i++) cs[i]=-1;
for(i=0; i<nsc; i++) {
	mn=cdf->sll.rfpt[i]-cdf->sll.nomx[i];
	mx=cdf->sll.rfpt[i]+cdf->sll.comx[i];
	for(j=mn; j<=mx; j++) cs[j]=i; }

/* for(i=0; i<nmt; i++) printf("%d ",cs[i]); printf("cs\n"); */


/* Count and/or copy residue-residue contacts to by-segment contact lists. */

for(i=0; i<mtf->rrc.n; i++) {
	r1=mtf->rrc.r1[i];
	s1=cs[r1];
	if(s1<0) continue;
	r2=mtf->rrc.r2[i];
	s2=cs[r2];
	if(s2<0) continue;
	d=mtf->rrc.d[i];
	cp=cpr[s1];
	cp->rr.n++;
	if(ct!=0) {	j=cp->rr.n-1;
			cp->rr.r1[j]=r1;
			cp->rr.r2[j]=r2;
			cp->rr.d[j]=d; }

	if(s1==s2) continue;

	cp=cpr[s2];
	cp->rr.n++;
	if(ct!=0) {	j=cp->rr.n-1;
			cp->rr.r1[j]=r1;
			cp->rr.r2[j]=r2;
			cp->rr.d[j]=d; } }

/*
for(i=0; i<nsc; i++) {
	cp=cpr[i];
	printf("rr:%d rp:%d rf:%d cpr[%d]\n",cp->rr.n,cp->rp.n,cp->rf.n,i);
	if(ct!=0) for(j=0; j<cp->rr.n; j++) {
		printf("%d %d %d\n",cp->rr.r1[j],cp->rr.r2[j],cp->rr.d[j]); } }
*/

/* Count and/or copy residue-peptide contacts to by segment contact lists. */

for(i=0; i<mtf->rpc.n; i++) {
	r1=mtf->rpc.r1[i];
	s1=cs[r1];
	if(s1<0) continue;
	r2=mtf->rpc.p2[i];
	s2=cs[r2];
	if(s2<0) continue;
	d=mtf->rpc.d[i];
	cp=cpr[s1];
	cp->rp.n++;
	if(ct!=0) {	j=cp->rp.n-1;
			cp->rp.r1[j]=r1;
			cp->rp.p2[j]=r2;
			cp->rp.d[j]=d; }

	if(s1==s2) continue;

	cp=cpr[s2];
	cp->rp.n++;
	if(ct!=0) {	j=cp->rp.n-1;
			cp->rp.r1[j]=r1;
			cp->rp.p2[j]=r2;
			cp->rp.d[j]=d; } }

/*
for(i=0; i<nsc; i++) {
	cp=cpr[i];
	printf("rr:%d rp:%d rf:%d cpr[%d]\n",cp->rr.n,cp->rp.n,cp->rf.n,i);
	if(ct!=0) for(j=0; j<cp->rp.n; j++) {
		printf("%d %d %d\n",cp->rp.r1[j],cp->rp.p2[j],cp->rp.d[j]); } }
*/

/* Flag residue positions within fixed segments */

for(i=0; i<nmt; i++) fs[i]=-1;
for(i=0; i<nfs; i++) {
	mn=cdf->fll.nt[i];
	mx=cdf->fll.ct[i];
	for(j=mn; j<=mx; j++) fs[j]=j; }

/* for(i=0; i<nmt; i++) printf("%d ",fs[i]); printf("fs\n"); */

/* Count and/or copy residue-fixed contacts to by segment contact lists. */

for(i=0; i<mtf->rrc.n; i++) {
	r1=mtf->rrc.r1[i];
	s1=cs[r1]; f1=fs[r1];
	if(s1<0&&f1<0) continue;
	r2=mtf->rrc.r2[i];
	s2=cs[r2]; f2=fs[r2];
	if(s2<0&&f2<0) continue;

	if(f1<0&&f2<0) continue;
	if(f1>0&&f2>0) continue;

	if(s2>0&&f1>0) { s1=s2; r1=r2; f2=f1; }

	d=mtf->rrc.d[i];
	cp=cpr[s1];
	t2=cdf->fll.sq[f2];
	if(t2<0) continue;
	cp->rf.n++;
	if(ct!=0) {	j=cp->rf.n-1;
			cp->rf.r1[j]=r1;
			cp->rf.t2[j]=t2;
			cp->rf.d[j]=d; } }

for(i=0; i<mtf->rpc.n; i++) {
	r1=mtf->rpc.r1[i];
	s1=cs[r1];
	if(s1<0) continue;
	r2=mtf->rpc.p2[i];
	f2=fs[r2];
	if(f2<0) continue;

	d=mtf->rpc.d[i];
	cp=cpr[s1];
	cp->rf.n++;
	if(ct!=0) {	j=cp->rf.n-1;
			cp->rf.r1[j]=r1;
			cp->rf.t2[j]=ppi;
			cp->rf.d[j]=d; } }


/*
for(i=0; i<nsc; i++) {
	cp=cpr[i];
	printf("rr:%d rp:%d rf:%d cpr[%d]\n",cp->rr.n,cp->rp.n,cp->rf.n,i);
	if(ct!=0) for(j=0; j<cp->rf.n; j++) {
		printf("%d %d %d\n",cp->rf.r1[j],cp->rf.t2[j],cp->rf.d[j]); } }

*/

/* Free local storage */

free(fs);
free(cs);

}
