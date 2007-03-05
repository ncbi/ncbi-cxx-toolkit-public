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
* File Name:  thrdalgs.c
*
* Author:  Stephen Bryant
*
* Initial Version Creation Date: 08/16/2000
*
* $Revision$
*
* File Description: threader
*/



/* Choose an aligment by Gibbs sampling */

#include <algo/structure/threader/thrdatd.h>
#include <algo/structure/threader/thrddecl.h>
#include <math.h>

int algs(Rnd_Smp* pvl, int tm) {
/*----------------------------------------------------------*/
/* pvl:   Alignment-location parameter value probabilities  */
/* tm:    Temperature for Boltzmann sampling                */
/*----------------------------------------------------------*/

int	i;		/* Counter */
float	gm;		/* Minimum energy value for scaling of exponents */
float	g;		/* Energy value */
float	b,sb;		/* Boltzmann factor and sum of Boltzmann factors */
float	tf;		/* Temperature */

/* printf("tm:%d\n",tm);
for(i=0; i<pvl->n; i++) printf("%.2f ",pvl->e[i]); printf("pvl->e\n"); */

tf=(float)tm;
/* printf("tf:%.2f\n",tf); */

/* Find minimum energy, equal to maximum score */
gm=pvl->e[0];
for(i=1;i<pvl->n;i++) {
	g=pvl->e[i];
	if(g>gm) gm=g; }
/* printf("gm:%.2f\n",gm); */

/* Subtract minimum energy, compute Boltzmann factors, and sum */
sb=0.; for(i=0;i<pvl->n;i++) {
	g=pvl->e[i]-gm;
	b=(float) exp(g/tf);
	pvl->p[i]=b;
	sb+=b; 
	/* printf("g:%.2f b:%.4f sb:%.4f\n",g,b,sb); */
	}
	
/* Compute Boltzmann probabilities */
for(i=0; i<pvl->n; i++) pvl->p[i]=pvl->p[i]/sb;
/* for(i=0; i<pvl->n; i++) printf(".4f ",pvl->p[i]); printf("pvl->p\n"); */


/* Choose a state at random given this distribution */
return(rsmp(pvl));

}
