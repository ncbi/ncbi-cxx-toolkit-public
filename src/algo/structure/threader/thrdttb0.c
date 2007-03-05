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
* File Name:  thrdttb0.c
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

/* Initialize thread table, a linked list containing the minimum energy */
/* threads found in sampling */

/*int*/ void ttb0(Thd_Tbl* ttb) {
/*---------------------------------------------------------*/
/* ttb:  Tables to hold Results of Gibbs sampled threading */
/*---------------------------------------------------------*/

int	i;		/* Counter */
int	n;		/* Maximum thread index */

n=ttb->n-1;

/* Set thread frequencies to zero and initialize linked list. */
ttb->tf[0]=0; ttb->ts[0]=0; ttb->nx[0]=1; ttb->tg[0]=BIGNEG;
for(i=1; i<n; i++) {
	ttb->tf[i]=0; ttb->ts[i]=0; ttb->tg[i]=BIGNEG;
	ttb->ps[i]=BIGNEG; ttb->ms[i]=BIGNEG;
	ttb->cs[i]=BIGNEG; ttb->lps[i]=BIGNEG;
	/*ttb->g0[i]=BIGNEG; ttb->zsc[i]=0.0;*/
	ttb->pr[i]=i-1; ttb->nx[i]=i+1; }
ttb->tf[n]=0; ttb->ts[n]=0; ttb->pr[n]=n-1; ttb->tg[n]=BIGNEG;
ttb->mx=0;
ttb->mn=n;

}
