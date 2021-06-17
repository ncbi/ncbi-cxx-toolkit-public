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
* File Name:  thrdzsc.c
*
* Author:  Stephen Bryant
*
* Initial Version Creation Date: 08/16/2000
*
* $Revision$
*
* File Description: threader
*/

#ifdef _MSC_VER
#pragma warning(disable:4244)   // disable double->float warning in MSVC
#endif

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <util/random_gen.hpp>

static ncbi::CRandom randGenerator;
static void RandomSeed(int s)
{
    randGenerator.SetSeed(s);
}
static int RandomNum(void)
{
    return randGenerator.GetRand();
}

extern "C" {

#include <algo/structure/threader/thrdatd.h>
#include <algo/structure/threader/thrddecl.h>
#include <math.h>

/* Sorts the stored energies in descending order, picks up the alignment.
Calculates the Z-score by shuffling the aliged region 10000 times. */

void zsc(Thd_Tbl* ttb, Seq_Mtf* psm, Qry_Seq* qsq, Cxl_Los** cpr, Cor_Def* cdf,
          Rcx_Ptl* pmf, Seg_Gsm* spe, Cur_Aln* sai, Rnd_Smp* pvl, double ScalingFactor) {
/*---------------------------------------------------------*/
/* ttb:  Tables to hold Results of Gibbs sampled threading */
/* qsq:  Sequence to thread with alignment contraints      */
/* cpr:  Contacts by segment, largest possible set         */
/* cdf:  Core segment locations and loop length limits     */
/* pmf:  Potential of mean force as a 3-d lookup table     */
/* psm:  Sequence motif parameters                         */
/* spe:  Partial sums of contact energies by segment pair  */
/* sai:  Current alignment of query sequence with core     */
/* pvl:  Storage for sequence permutation parameters       */
/*---------------------------------------------------------*/

int	i,j,k,nmt,
	itr,ii,jj;	/* Counters */
int	i_max;		/* Index of the top hit */
int	ntr;		/* Number of positions to be shuffled */
int	nsc;		/* Number of core segments */
int	ppi;		/* Index of the peptide group in potential */
int	n_perm;		/* Number of permutations */
int	mn,mx;
int	n_thr;		/* Number of top threads to calculate Z-score for */
int	to,from,
	tlf=0,tls=0,dln;
int	*cf;		/* Flags residues within the core */
int     t1,t2;          /* Motif residue types */
int     r1,r2;          /* Motif residue positions */
int     s1,s2;          /* Core segment indices */
int     d;              /* Distance interval */
int	ms,cs,
	ls; 		/* Energy terms */
float	avg,avg2,
	avgm,avgp,
	avgm2,avgp2;    /* Averages */
int	nl,ot;
float	disp,dispm,dispp;/* Square root of variance */
float	tg,tgp,tgm;	        /* Energy of shuffled sequence */
float   tg_max=0.0f;		/* Energy of a given thread */
int	*gi;
int     *lsg,*aqi,
	*r,*o,*sq;
int	g;

Cxl_Los *cr;    /* Pointer to segment reference contact lists */

/* Parameters */

nsc=cdf->sll.n;
n_perm=10000;
n_thr=1; 		/* Should be less than ttb->n */
ppi=pmf->ppi;
nmt=psm->n;
cf=sai->cf;
sq=pvl->sq;
aqi=pvl->aqi;
r=pvl->r;
o=pvl->o;
lsg=pvl->lsg;


/* Start loop over n_thr top threads */

i_max=ttb->mx;
for(ii=0;ii<n_thr;ii++){

  avg = avg2 = avgm = avgp = avgm2 = avgp2 = 0.0;
  ntr=0;

	for(jj=0;jj<qsq->n;jj++)sq[jj]=qsq->sq[jj];

/* Collect sequence indices of threaded residues of sequence */

for(i=0;i<=nsc;i++) lsg[i]=1;

itr=0;

for(j=0;j<nsc;j++){

	mn=ttb->al[j][i_max]-ttb->no[j][i_max];
	mx=ttb->al[j][i_max]+ttb->co[j][i_max];

	for(k=mn;k<=mx;k++){
		aqi[itr]=k;
		itr++;}

	if(j<nsc-1){
	to=ttb->al[j+1][i_max]-ttb->no[j+1][i_max];
	from=ttb->al[j][i_max]+ttb->co[j][i_max];
	dln=to-from-1;
	if(dln > cdf->lll.lrfs[j+1] || dln < 1) lsg[j+1]=0;}

	}


for(j=0;j<=nsc;j++){

		if(lsg[j]==0) continue;

	   if(j==0){tlf=ttb->al[j][i_max]-ttb->no[j][i_max]-1;
			tls=tlf-cdf->lll.lrfs[j]+1;
			if(tls<1) tls=1;}

	   if(j==nsc){tls=ttb->al[j-1][i_max]+ttb->co[j-1][i_max]+1;
			   tlf=tls+cdf->lll.lrfs[j]-1;
			   if(tlf>qsq->n) tlf=qsq->n;}

	   if(j!=0 && j!=nsc){tls=ttb->al[j-1][i_max]+ttb->co[j-1][i_max]+1;
			   tlf=ttb->al[j][i_max]-ttb->no[j][i_max]-1;}


	for(k=tls;k<=tlf;k++){
		aqi[itr]=k;
		itr++; }  }

		ntr=itr-1;


/* Flag residues within the cores */

for(i=0;i<nmt; i++) cf[i]=(-1);
for(i=0; i<nsc; i++) {

	mn=cdf->sll.rfpt[i]-ttb->no[i][i_max];
	mx=cdf->sll.rfpt[i]+ttb->co[i][i_max];

	for(j=mn; j<=mx; j++) cf[j]=i;

				}

/* Loop over n_perm permutations */

for(k=0; k<=n_perm; k++) {

/* srand48(k); */
  RandomSeed(k);

/* Perform the permutation of the residues of aligned part together with the
intercore loops. Tail loops are not included. */

/* for(i=0;i<=ntr;i++)r[i]=lrand48(); */
for(i=0;i<=ntr;i++)r[i]=RandomNum();
for(i=0;i<=ntr;i++)o[i]=i;

for(i=ntr; i>0; ) {nl=i;
		   i=0;
		   for(j=0; j<nl; j=j+1){
		      if(r[o[j]]>r[o[j+1]]){
		      ot=o[j];
		      o[j]=o[j+1];
		      o[j+1]=ot;
		      i=j;
		      }}}

if(k!=0){
	for(i=0;i<qsq->n;i++) sq[i]=-1;

	for(i=0;i<=ntr;i++){r[i]=aqi[o[i]];
		    sq[aqi[i]]=qsq->sq[r[i]];}
		    }

/* Calculate the energy for a given permuted sequence aligned with
the structure*/

/* Loop over core segments */

for(i=0; i<nsc; i++) {

	cr=cpr[i];

	spe->gs[i]=0; spe->ms[i]=0;

	for(j=0;j<nsc;j++) { spe->gss[i][j]=0; spe->gss[j][i]=0;}

	/* Loop over residue-residue contacts in the reference list */

	for(j=0; j<cr->rr.n; j++) {

	    /* Test that the contact is within the allowed extent range */
	    r1=cr->rr.r1[j];
	    s1=cf[r1];
	    if(s1<0) continue;
	    t1=sq[ttb->al[s1][i_max]-(cdf->sll.rfpt[s1]-r1)];
	    if(t1<0) continue;
	    r2=cr->rr.r2[j];
	    s2=cf[r2];
	    if(s2<0) continue;
	    t2=sq[ttb->al[s2][i_max]-(cdf->sll.rfpt[s2]-r2)];
	    if(t2<0) continue;
	    d=cr->rr.d[j];

	    spe->gss[s1][s2]+=pmf->rrt[d][t1][t2];

	    }


	/*Loop over residue-peptide contacts in the refernce list */

	for(j=0; j<cr->rp.n; j++) {

	   /* Test that the contact is present in the current core */

	   r1=cr->rp.r1[j];
	   s1=cf[r1];
	   if(s1<0) continue;
	   t1=sq[ttb->al[s1][i_max]-(cdf->sll.rfpt[s1]-r1)];
	   if(t1<0) continue;
	   r2=cr->rp.p2[j];
	   s2=cf[r2];
	   if(s2<0) continue;
	   d=cr->rp.d[j];

	   spe->gss[s1][s2]+=pmf->rrt[d][t1][ppi];

	   }

	/* Loop over residue-fixed contacts in the reference list */

	   for(j=0; j<cr->rf.n; j++) {

	  /* Test that the contact is present in the current list */
	    r1=cr->rf.r1[j];
	    s1=cf[r1];
	    if(s1<0) continue;
	    t1=sq[ttb->al[s1][i_max]-(cdf->sll.rfpt[s1]-r1)];
	    if(t1<0) continue;
	    t2=cr->rf.t2[j];
	    d=cr->rf.d[j];

	    spe->gs[i]+=pmf->rrt[d][t1][t2];

	    }

/* Sum motif energies */

	mn=cdf->sll.rfpt[i]-ttb->no[i][i_max];
	mx=cdf->sll.rfpt[i]+ttb->co[i][i_max];

	for(j=mn; j<=mx; j++) {

		t1=sq[ttb->al[i][i_max]-(cdf->sll.rfpt[i]-j)];
		if(t1<0) continue;
		spe->ms[i]+=psm->ww[j][t1]; }


}

g=0; ms=0; cs=0; ls=0;
for(i=0;i<nsc;i++) {

	g+=spe->gs[i];
	ms+=spe->ms[i];
	cs+=spe->cs[i];
	ls+=spe->ls[i];

	gi=spe->gss[i];
	for(j=0;j<nsc;j++) g+=gi[j]; }

	/*ms=ms-psm->score0;*/

/* Entire energy for a current permutation */

	if(k!=0) {
		tg=((float)(g+ms+cs+ls))/ScalingFactor;
		tgm=((float)(ms))/ScalingFactor;
		tgp=((float)(g))/ScalingFactor;}

  else {
    tg = tgm = tgp = 0.0;
		tg_max=((float)(g+ms+cs+ls))/ScalingFactor;}

	 avg+=tg;
	 avg2+=tg*tg;

	 avgm+=tgm;
	 avgm2+=tgm*tgm;

	 avgp+=tgp;
	 avgp2+=tgp*tgp;

	} /* End of loop over permutations */

/* Calculate the mean, variance and Z-score */

	disp=sqrt(((float)avg2 - (avg*avg)/n_perm)/(n_perm-1));
	dispm=sqrt(((float)avgm2 - (avgm*avgm)/n_perm)/(n_perm-1));
	dispp=sqrt(((float)avgp2 - (avgp*avgp)/n_perm)/(n_perm-1));

	avg=avg/n_perm;
	avgm=avgm/n_perm;
	avgp=avgp/n_perm;

	ttb->zsc[i_max]=ScalingFactor*(tg_max-avg)/disp;
	ttb->g0[i_max]=avg*ScalingFactor;
	ttb->m0[i_max]=avgm*ScalingFactor;
	ttb->errm[i_max]=dispm*ScalingFactor;
	ttb->errp[i_max]=dispp*ScalingFactor;

	i_max=ttb->nx[i_max];

	}   /* End of loop over threads */



}

} // extern "C"
