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
* File Name:  thrdrsmp.c
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

/* Choose randomly from a multinomial distribution */

#include <algo/structure/threader/thrdatd.h>
#include <algo/structure/threader/thrddecl.h>
#include <stdio.h>
#include <math.h>

int rsmp(Rnd_Smp* pvl) {
/*----------------------------------------------------*/
/* pvl:  Number and probabilities of parameter values */
/*----------------------------------------------------*/

int	i;		/* The i-th offset parameter value will be the choice */
float	c;		/* Cumulative probabilites across parameter values */
float	r;		/* A uniform random number on the interval 0 - 1 */
int idum = 1;

/* for(i=0;i<pvl->n;i++) printf("%.4f ",pvl->p[i]); printf("pvl->p\n");  */

/* r=drand48(); c=0.; */
r = Rand01(&idum);
c = 0.0;

/* printf("r: %.4f\n",r); */

for(i=0; i<pvl->n; i++) {
	c=c+pvl->p[i];
	/* printf("c: %.4f\n",c); */
	if(c>=r) return(i); }

return(i-1);
}


#define  M1  259200
#define IA1    7141
#define IC1   54773
#define RM1 (1.0/M1)
#define  M2  134456
#define IA2    8121
#define IC2   28411
#define RM2 (1.0/M2)
#define  M3  243000
#define IA3    4561
#define IC3   51349

float Rand01(int* idum) {
/*----------------------------------------------------------------------------
// Returns a uniform deviate between 0.0 and 1.0.
// Set idum to any negative value to initialize or reinitialize the sequence.
//
// This is copied from Numerical Recipes, chapter 7.1, 1988.
// (ISBN 0-521-35465-X)
//--------------------------------------------------------------------------*/
  static long   ix1,ix2,ix3;
  static float  r[98];
  float         temp;
  static int    iff=0;
  int           j;

  if (*idum<0 || iff==0) {
    iff=1;
    ix1=(IC1-*idum) % M1;
    ix1=(IA1*ix1+IC1) % M1;
    ix2=ix1 % M2;
    ix1=(IA1*ix1+IC1) % M1;
    ix3=ix1 % M3;
    for (j=1; j<=97; j++) {
      ix1=(IA1*ix1+IC1) % M1;
      ix2=(IA2*ix2+IC2) % M2;
      r[j]=(ix1+ix2*RM2)*RM1;
    }
    *idum=1;
  }
  ix1=(IA1*ix1+IC1) % M1;
  ix2=(IA2*ix2+IC2) % M2;
  ix3=(IA3*ix3+IC3) % M3;
  j=1 + ((97*ix3)/M3);
  temp=r[j];
  r[j]=(ix1+ix2*RM2)*RM1;
  return(temp);
}
