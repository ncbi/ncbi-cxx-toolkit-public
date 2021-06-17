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
* File Name:  thrdbwfi.c
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
#include <math.h>

/*float*/ void bwfi(Thd_Tbl* ttb, Gib_Scd* gsp, Thd_Tst* tts) {
/*---------------------------------------------------------*/
/* ttb:   Table holding results of Gibbs sampled threading */
/* gsp:   Various control parameters for Gibb's sampling   */
/* tts:   Pameters for local min and convergence tests     */
/*---------------------------------------------------------*/

int	i;		/* Counter */
int	ct;		/* Index of current thread in list traversal */
int	nz=0;		/* Number of threads with non-zero Boltzmann weight */
int	nt=0;		/* Number of threads within specified ensemble cutoff */
float	tm;		/* Temperature for calculation of Boltzmann weight */
float	gm;		/* Minimum energy value for scaling of exponents */
float	g;		/* Energy value */
float	b;		/* Boltzmann exponent exponents */
float	sb;		/* Sum of Boltzmann exponents */
float	cw;		/* Cumulative sum of Boltzmann factors */


/* Identify the top threads of the current ensemble */

/* Calculate and sum Boltzmann exponents*/
gm=ttb->tg[ttb->mx]; tm=(float)gsp->cet;
sb=0.; ct=ttb->mx; for(i=0; i<ttb->n; i++) {
	if(ttb->tf[ct]==0) {nz=i; break;}
	g=ttb->tg[ct]-gm;
	b=(float) exp(g/tm);
	tts->bw[ct]=b;
	sb+=b;
	/* printf("gm:%.2f sb:%.5f i:%d ct:%d g:%.2f b:%.5f\n",
		gm,sb,i,ct,ttb->tg[ct],b); */
	if(b<NERZRO) {nz=i; break;}
	ct=ttb->nx[ct];}

/* Cumulate Boltzmann weights to identify top threads */
cw=0.; ct=ttb->mx; for(i=0; i<=nz; i++) {
	b=tts->bw[ct]/sb;
	tts->bw[ct]=b;
	cw+=b;
	/* printf("gm:%.2f sb:%.5f i:%d ct:%d g:%.2f b:%.5f cw:%.5f\n",
		gm,sb,i,ct,ttb->tg[ct],b,cw); */
	if(cw>((float)gsp->cef)/100.) {nt=i; break;}
	ct=ttb->nx[ct];}


/* Find the highest random-start count and frequecy for any top thread */

tts->ts=-1; tts->tf=-1;
ct=ttb->mx; for(i=0; i<=nt; i++) {
	if(ttb->ts[ct]>tts->ts) {
		tts->ts=ttb->ts[ct];
		tts->tf=ttb->tf[ct]; }
	else if(ttb->ts[ct]==tts->ts && ttb->tf[ct]>tts->tf)
		tts->tf=ttb->tf[ct];
		/* printf("ensi:%d ct:%d dgi:%.1f bfi:%.4f tsi:%d ts:%d
			tfi: %d tf:%d\n",
			i,ct,ttb->tg[ct],tts->bw[ct],ttb->ts[ct],tts->ts,
			ttb->tf[ct],tts->tf); */
	ct=ttb->nx[ct];}

}
