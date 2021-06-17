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
* File Name:  thrdsalu.c
*
* Author:  Stephen Bryant
*
* Initial Version Creation Date: 08/16/2000
*
* $Revision$
*
* File Description: threader
*/



/* Update core segment alignment */

#include <algo/structure/threader/thrdatd.h>
#include <algo/structure/threader/thrddecl.h>

/*int*/ void salu(Cor_Def* cdf, Qry_Seq* qsq, Cur_Loc* sli, int cs, int al, Cur_Aln* sai) {
/*-------------------------------------------------------*/
/* cdf:  Core segment locations and loop length limits   */
/* qsq:  Sequence to thread with alignment contraints    */
/* sli:  Current locations of core segments in the motif */
/* cs:   Current segment                                 */
/* al:   New alignment of current segment                */
/* sai:  Contains segment alignment values to be set     */
/*-------------------------------------------------------*/

int	ci;		/* Residue index in core motif */
int	si;		/* Residue index in query sequence */
int	mx,mn;		/* Aligment range */


sai->al[cs]=al;
mn=sai->al[cs]-sli->no[cs];
mx=sai->al[cs]+sli->co[cs];
ci=cdf->sll.rfpt[cs]-sli->no[cs];
for(si=mn; si<=mx; si++) {
	sai->sq[ci]=qsq->sq[si];
	ci++; }

/* printf("sai->nsc:%d nmt%d\n",sai->nsc,sai->nmt);
for(i=0;i<sai->nsc;i++) printf("%d ",sai->al[i]); printf("sai->al\n");
for(i=0;i<sai->nmt;i++) printf("%d ",sai->sq[i]); printf("sai->sq\n"); */


}
