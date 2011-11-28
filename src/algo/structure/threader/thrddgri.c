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
* File Name:  thrddgri.c
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

/* Compute sum of contact potentials G(r|m) */

int g(Seg_Gsm* spe, Seq_Mtf* psm, Thd_Gsm* tdg) {
/*--------------------------------------------------------*/
/* spe:  Partial sums of contact energies by segment pair */
/* psm:  Sequence motif parameters                        */
/* tdg:  Total thread energy                              */
/*--------------------------------------------------------*/

int     nsc;            /* Number of threaded segments in core definition */
int	i,j;		/* Counters */
int	g;		/* Partial energy sum */
int	*gi;		/* Pointer to current row in segment pair table */
int	ms,cs,ls;
/*int	nrt;*/		/* Number of residue types */
/*int	ntot;*/		/* Number of residues in the alignment */
/*int	s0;*/

/*nrt=spc->nrt;*/
/*ntot=0;*/


/*for(i=0;i<nrt-1;i++)ntot+=spc->rt[i];*/

/* Sum energies for segments and segment pairs */

nsc=spe->nsc; g=0; ms=0; cs=0; ls=0; /*s0=0;*/
for(i=0;i<nsc;i++) {
	
	g+=spe->gs[i];
	ms+=spe->ms[i];
	cs+=spe->cs[i];
	ls+=spe->ls[i];

	gi=spe->gss[i];
	for(j=0;j<nsc;j++) g+=gi[j]; }

	tdg->ms=(float)ms-psm->ww0;
	tdg->cs=(float)cs;
  tdg->ls=(float)ls;
  tdg->m0=psm->ww0;

return(g);
}


/* Compute reference state energy G0(r|m) */

float g0(Seg_Nsm* spn, Thd_Cxe* cxe) {
/*------------------------------------------------------*/
/* spn:  Partial sums of contact counts by segment pair */
/* cxe:  Expected energy of a contact in current thread */
/*------------------------------------------------------*/

int     ndi;            /* Number of distance intervals in potential */
int	i;		/* Counters */
float	g0;		/* Reference state energy */

ndi=spn->ndi;

/* Sum expected contact energy per contact times number of contacts */
g0=0.; for(i=0; i<ndi; i++) {

	/* Expected side-chain to side-chain contact energy */
	g0+=((float)spn->srr[i])*cxe->rre[i];

	/* Expected side-chain to peptide contact energy */
	g0+=((float)spn->srp[i])*cxe->rpe[i];

	/* Expected energy of side-chain to fixed-residue contacts */
	g0+=((float)spn->srf[i])*cxe->rfe[i]; }

return(g0);

}


/* Compute thread energy  */

float dgri(Seg_Gsm* spe, Seg_Nsm* spn, Thd_Cxe* cxe, Thd_Gsm* tdg,
           Seq_Mtf* psm, Seg_Cmp* spc) {
/*--------------------------------------------------------*/
/* spe:  Partial sums of contact energies by segment pair */
/* spn:  Partial sums of contact counts by segment pair   */
/* cxe:  Expected energy of a contact in current thread   */
/* tdg:  Energy of the current alignment, current core    */
/* psm:  Sequence motif parameters                        */
/* spc:  Residue type composition of current threaded seq */
/*--------------------------------------------------------*/

/* int  g(); */
/* float  g0(); */

/* Find overall energy coming from the potential */

tdg->g=g(spe,psm,tdg);
tdg->g0=g0(spn,cxe);
tdg->ps=((float)tdg->g)-tdg->g0;

/* Find overall threading energy */

tdg->dg=tdg->ps+tdg->ms+tdg->cs+tdg->ls;

return(tdg->dg);

}
