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
* File Name:  thrdslou.c
*
* Author:  Stephen Bryant
*
* Initial Version Creation Date: 08/16/2000
*
* $Revision$
*
* File Description: threader
*/



/* Update core segment location */

#include <algo/structure/threader/thrdatd.h>
#include <algo/structure/threader/thrddecl.h>

/*int*/ void slou(Fld_Mtf* mtf, Cor_Def* cdf, int cs, int ct, int of, Cur_Loc* sli,
    Cur_Aln* sai, Qry_Seq* qsq) {
/*--------------------------------------------------------*/
/* mtf:  Contact matrices defining the folding motif      */
/* cdf:  Core definition contains min/max segment extents */
/* cs:   Current segment                                  */
/* ct:   Current terminus                                 */
/* of:   New alignment of current segment                 */
/* sli:  Current locations of core segments in the motif  */
/*--------------------------------------------------------*/

int	i;		/* Residue indices in core motif */
int     nsc;            /* Number of threaded core segments */
int	mx,mn;		/* Range of motif indices */
int	rf;		/* Reference point for core segment offsets */
int	nt;		/* Terminus of neighboring segment */
int	ns;		/* Neighbor segment index */
int	ci, si;

/* Number of core segments */
nsc=cdf->sll.n;

/* Lower in-core flags for all positions in the current element */
rf=cdf->sll.rfpt[cs];
mn=rf-sli->no[cs];
mx=rf+sli->co[cs];
for(i=mn; i<=mx; i++) sli->cr[i]=-1;


/* Record new element range based on updated n- or c-terminal extent */
switch(ct) {

	case 0: { 	sli->no[cs]=of;
			mn=rf-of;
			ns=cs-1; if(ns>=0) {
				nt=cdf->sll.rfpt[ns]+sli->co[ns];
				sli->lp[cs]=mtf->mll[nt][mn]; }
			break; }

	case 1: { 	sli->co[cs]=of;
			mx=rf+of;
			ns=cs+1; if(ns<nsc) {
				nt=cdf->sll.rfpt[ns]-sli->no[ns];
				sli->lp[cs+1]=mtf->mll[mx][nt]; }
			break; }
		}


/* Raise in-core flags for all positions in the new element */
for(i=mn; i<=mx; i++) sli->cr[i]=cs;

/* update aligned residue types */
mn=sai->al[cs]-sli->no[cs];
mx=sai->al[cs]+sli->co[cs];
ci=cdf->sll.rfpt[cs]-sli->no[cs];
for(si=mn; si<=mx; si++) {
    sai->sq[ci]=qsq->sq[si];
    ci++; }

}
