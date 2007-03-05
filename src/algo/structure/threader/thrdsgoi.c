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
* File Name:  thrdsgoi.c
*
* Author:  Stephen Bryant
*
* Initial Version Creation Date: 08/16/2000
*
* $Revision$
*
* File Description: threader
*/



/* Set core segment sampling order */

#include <algo/structure/threader/thrdatd.h>
#include <algo/structure/threader/thrddecl.h>

/*int*/ void sgoi(int is, int it, Rnd_Smp* pvl, Seg_Ord* sgo) {
/*----------------------------------------------------*/
/* is:   Code to determine order of segment sampling  */
/* it:   Code to determine order of terminus sampling */
/* pvl:	 Storage for segment location probabilities   */
/* sgo:  Segment samping order                        */
/*----------------------------------------------------*/

int	nsc;		/* Number of threaded core segments */
int	i,j,k;		/* Counters */
int	*si;		/* Segment index by order of sampling */
int	*so;		/* Flags segments already assigned an order */
int	*to;		/* Order of terminus sampling, 0=Nterm, 1=Cterm. */
float	pv;		/* P value */


/* Number of core segments */
nsc=sgo->nsc;

/* Initialize alignment order arrays */
si=sgo->si;
so=sgo->so;
for(i=0; i<nsc; i++) so[i]=1;
to=sgo->to;

/* Determine the order of core segment alignment */
/* printf("is:%d\n",is); */
switch(is) {

	default: {	/* Order randomly */
		/* printf("segment order by default random method\n"); */
		pvl->n=nsc;
		for(i=0; i<nsc; i++) {
			pv= 1.0f /(nsc-i);
			for(j=0; j<nsc; j++) {
				if(so[j]==1) pvl->p[j]=pv;
				else pvl->p[j]=0.; }
			k=rsmp(pvl); so[k]=0; si[i]=k; }
			break; }

	case 1: {	/* Order by occurrence in the core definition */

		/* printf("segment order by occurrence, case 1\n"); */
		for(i=0; i<nsc; i++)  si[i]=i;  break; }

		}


/* Select the terminus to sample first, using the specified method */
/* printf("it:%d\n",it); */
switch(it) {

	default: {	/* Choose n- or c-terminus at random */
		/* printf("terminus order by default random method\n");*/
		for(i=0; i<nsc; i++) {
			pvl->n=2; pvl->p[0]=.5; pvl->p[1]=.5;
			to[i]=rsmp(pvl); } break; }

	case 1: {	/* N-terminus first */
		/* printf("segment order n-terminus first, case 1\n"); */
		for(i=0; i<nsc; i++) to[i]=0; break; }

	case 2: {	/* C-terminus first */
		/* printf("segment order c-terminus first, case 2\n"); */
		for(i=0; i<nsc; i++) to[i]=1; break; }

		}

}
