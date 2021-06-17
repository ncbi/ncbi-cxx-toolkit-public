/*  $Id$
* ===========================================================================
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
* Author:  Richard Desper
*
* File Description:  NNI.cpp
*
*    A part of the Miminum Evolution algorithm
*
*/

#include <ncbi_pch.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "graph.h"
#include "fastme.h"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(fastme)

boolean leaf(meNode *v);
meEdge *siblingEdge(meEdge *e);
meEdge *depthFirstTraverse(meTree *T, meEdge *e);
meEdge *findBottomLeft(meEdge *e);
meEdge *topFirstTraverse(meTree *T, meEdge *e);
meEdge *moveUpRight(meEdge *e);
double wf(double lambda, double D_LR, double D_LU, double D_LD, 
	  double D_RU, double D_RD, double D_DU);
/*NNI functions for unweighted OLS topological switches*/

/*fillTableUp fills all the entries in D associated with
  e->head,f->head and those edges g->head above e->head*/
void fillTableUp(meEdge *e, meEdge *f, double **A, double **D, meTree *T)
{
  meEdge *g,*h;
  if (T->root == f->tail)
    {
      if (leaf(e->head))
	A[e->head->index][f->head->index] = 
	  A[f->head->index][e->head->index] = 
	  D[e->head->index2][f->tail->index2];
      else
	{
	  g = e->head->leftEdge;
	  h = e->head->rightEdge;
	  A[e->head->index][f->head->index] = 
	    A[f->head->index][e->head->index] =  
	    (g->bottomsize*A[f->head->index][g->head->index]
	     + h->bottomsize*A[f->head->index][h->head->index])
	    /e->bottomsize;  
	}
    }
  else 
    {
      g = f->tail->parentEdge;
      fillTableUp(e,g,A,D,T); /*recursive call*/
      h = siblingEdge(f);
      A[e->head->index][f->head->index] = 
	A[f->head->index][e->head->index] =  
	(g->topsize*A[e->head->index][g->head->index]
	 + h->bottomsize*A[e->head->index][h->head->index])/f->topsize;    
    }
}


void makeOLSAveragesTable(meTree *T, double **D, double **A);

double **buildAveragesTable(meTree *T, double **D)
{
  int i,j;
  double **A;
  A = (double **) malloc(T->size*sizeof(double *));
  for(i = 0; i < T->size;i++)
    {
      A[i] = (double *) malloc(T->size*sizeof(double));
      for(j=0;j<T->size;j++)
	A[i][j] = 0.0;
    }
  makeOLSAveragesTable(T,D,A);
  return(A);
}

double wf2(double lambda, double D_AD, double D_BC, double D_AC, double D_BD,
	   double D_AB, double D_CD)
{
  double weight;
  weight = 0.5*(lambda*(D_AC + D_BD) + (1 - lambda)*(D_AD + D_BC)
		+ (D_AB + D_CD));
  return(weight);
}

int NNIEdgeTest(meEdge *e, meTree *T, double **A, double *weight)
{
  int a,b,c,d;
  meEdge *f;
  double *lambda;
  double D_LR, D_LU, D_LD, D_RD, D_RU, D_DU;
  double w1,w2,w0;
  
  if ((leaf(e->tail)) || (leaf(e->head)))
    return(NONE);
  lambda = (double *)malloc(3*sizeof(double));
  a = e->tail->parentEdge->topsize;
  f = siblingEdge(e);
  b = f->bottomsize;  
  c = e->head->leftEdge->bottomsize;
  d = e->head->rightEdge->bottomsize;

  lambda[0] = ((double) b*c + a*d)/((a + b)*(c+d));
  lambda[1] = ((double) b*c + a*d)/((a + c)*(b+d));    
  lambda[2] = ((double) c*d + a*b)/((a + d)*(b+c));
  
  D_LR = A[e->head->leftEdge->head->index][e->head->rightEdge->head->index];
  D_LU = A[e->head->leftEdge->head->index][e->tail->index];
  D_LD = A[e->head->leftEdge->head->index][f->head->index];
  D_RU = A[e->head->rightEdge->head->index][e->tail->index];
  D_RD = A[e->head->rightEdge->head->index][f->head->index];
  D_DU = A[e->tail->index][f->head->index];

  w0 = wf2(lambda[0],D_RU,D_LD,D_LU,D_RD,D_DU,D_LR);
  w1 = wf2(lambda[1],D_RU,D_LD,D_DU,D_LR,D_LU,D_RD);
  w2 = wf2(lambda[2],D_DU,D_LR,D_LU,D_RD,D_RU,D_LD);
  free(lambda);
  if (w0 <= w1)
    {
      if (w0 <= w2) /*w0 <= w1,w2*/
	{
	  *weight = 0.0;
	  return(NONE);
	}
      else /*w2 < w0 <= w1 */
	{
	  *weight = w2 - w0;
	  if (verbose)
	    {
	      printf("Swap across %s. ",e->label);
	      printf("Weight dropping by %lf.\n",w0 - w2);
	      printf("New weight should be %lf.\n",T->weight + w2 - w0);
	    }
	  return(RIGHT);
	}
    }
  else if (w2 <= w1) /*w2 <= w1 < w0*/
    {
      *weight = w2 - w0;
      if (verbose)
	{
	  printf("Swap across %s. ",e->label);
	  printf("Weight dropping by %lf.\n",w0 - w2);
	  printf("New weight should be %lf.\n",T->weight + w2 - w0);
	}
      return(RIGHT);
    }
  else /*w1 < w2, w0*/
    {
      *weight = w1 - w0;
      if (verbose)
	{
	  printf("Swap across %s. ",e->label);
	  printf("Weight dropping by %lf.\n",w0 - w1);
	  printf("New weight should be %lf.\n",T->weight + w1 - w0);
	}
      return(LEFT);	
    }
}
 

int *initPerm(int size);

void NNIupdateAverages(double **A, meEdge *e, meEdge *par, meEdge *skew, 
		       meEdge *swap, meEdge *fixed, meTree *T)
{
  meNode *v;
  meEdge *elooper;
  v = e->head;
  /*first, v*/
  A[e->head->index][e->head->index] =  
    (swap->bottomsize* 
     ((skew->bottomsize*A[skew->head->index][swap->head->index]
       + fixed->bottomsize*A[fixed->head->index][swap->head->index]) 
      / e->bottomsize) +
     par->topsize*
     ((skew->bottomsize*A[skew->head->index][par->head->index]
       + fixed->bottomsize*A[fixed->head->index][par->head->index]) 
      / e->bottomsize)
     ) / e->topsize; 
  
  elooper = findBottomLeft(e); /*next, we loop over all the edges 
				 which are below e*/
  while (e != elooper)  
    {
      A[e->head->index][elooper->head->index] = 
	A[elooper->head->index][v->index] 
	= (swap->bottomsize*A[elooper->head->index][swap->head->index] +
	   par->topsize*A[elooper->head->index][par->head->index]) 
	/ e->topsize;
      elooper = depthFirstTraverse(T,elooper);
    }
  elooper = findBottomLeft(swap); /*next we loop over all the edges below and
				    including swap*/  
  while (swap != elooper)
  {
    A[e->head->index][elooper->head->index] = 
      A[elooper->head->index][e->head->index]
      = (skew->bottomsize * A[elooper->head->index][skew->head->index] + 
	 fixed->bottomsize*A[elooper->head->index][fixed->head->index]) 
      / e->bottomsize;
    elooper = depthFirstTraverse(T,elooper);
  }
  /*now elooper = skew */
  A[e->head->index][elooper->head->index] = 
    A[elooper->head->index][e->head->index]
    = (skew->bottomsize * A[elooper->head->index][skew->head->index] + 
       fixed->bottomsize* A[elooper->head->index][fixed->head->index])	
    / e->bottomsize;
  
  /*finally, we loop over all the edges in the meTree 
    on the far side of parEdge*/ 
  elooper = T->root->leftEdge;
  while ((elooper != swap) && (elooper != e)) /*start a top-first traversal*/
    {
      A[e->head->index][elooper->head->index] = 
	A[elooper->head->index][e->head->index]
	= (skew->bottomsize * A[elooper->head->index][skew->head->index] 
	   + fixed->bottomsize* A[elooper->head->index][fixed->head->index]) 
	/ e->bottomsize;
      elooper = topFirstTraverse(T,elooper);
    }

  /*At this point, elooper = par.
    We finish the top-first traversal, excluding the submeTree below par*/
  elooper = moveUpRight(par);
  while (NULL != elooper)
    {
      A[e->head->index][elooper->head->index] 
	= A[elooper->head->index][e->head->index]
	= (skew->bottomsize * A[elooper->head->index][skew->head->index] + 
	   fixed->bottomsize* A[elooper->head->index][fixed->head->index]) 
	/ e->bottomsize;
      elooper = topFirstTraverse(T,elooper);
    }
  
}


void NNItopSwitch(meTree *T, meEdge *e, int direction, double **A)
{
  meEdge *par, *fixed;
  meEdge *skew, *swap;
  
  if (verbose)
    printf("Branch swap across meEdge %s.\n",e->label);

  if (LEFT == direction)
    swap = e->head->leftEdge;
  else
    swap = e->head->rightEdge;
  skew = siblingEdge(e);
  fixed = siblingEdge(swap);
  par = e->tail->parentEdge;
  
  if (verbose)
    {
      printf("Branch swap: switching edges %s and %s.\n",skew->label,swap->label);
    }
  /*perform topological switch by changing f from (u,b) to (v,b)
    and g from (v,c) to (u,c), necessitatates also changing parent fields*/
  
  swap->tail = e->tail;
  skew->tail = e->head;
  
  if (LEFT == direction)
    e->head->leftEdge = skew;
  else
    e->head->rightEdge = skew;
  if (skew == e->tail->rightEdge)
    e->tail->rightEdge = swap;
  else
    e->tail->leftEdge = swap;

  /*both topsize and bottomsize change for e, but nowhere else*/

  e->topsize = par->topsize + swap->bottomsize;
  e->bottomsize = fixed->bottomsize + skew->bottomsize;
  NNIupdateAverages(A, e, par, skew, swap, fixed,T);

} /*end NNItopSwitch*/

void reHeapElement(int *p, int *q, double *v, int length, int i);
void pushHeap(int *p, int *q, double *v, int length, int i);
void popHeap(int *p, int *q, double *v, int length, int i);


void NNIRetestEdge(int *p, int *q, meEdge *e,meTree *T, double **avgDistArray, 
		double *weights, int *location, int *possibleSwaps)
{
  int tloc;
  tloc = location[e->head->index+1];
  location[e->head->index+1] = 
    NNIEdgeTest(e,T,avgDistArray,weights + e->head->index+1);
  if (NONE == location[e->head->index+1])
    {
      if (NONE != tloc)
	popHeap(p,q,weights,(*possibleSwaps)--,q[e->head->index+1]);	  
    }
  else
    {
      if (NONE == tloc)
	pushHeap(p,q,weights,(*possibleSwaps)++,q[e->head->index+1]);
      else
	reHeapElement(p,q,weights,*possibleSwaps,q[e->head->index+1]);
    }
}

void permInverse(int *p, int *q, int length);

int makeThreshHeap(int *p, int *q, double *v, int arraySize, double thresh);


void NNI(meTree *T, double **avgDistArray, int *count)
{
  meEdge *e, *centerEdge;
  meEdge **edgeArray;
  int *location;
  int *p,*q;
  int i;
  int possibleSwaps;
  double *weights;
  p = initPerm(T->size+1);
  q = initPerm(T->size+1);
  edgeArray = (meEdge **) malloc((T->size+1)*sizeof(meEdge*));
  weights = (double *) malloc((T->size+1)*sizeof(double));
  location = (int *) malloc((T->size+1)*sizeof(int));
  for (i=0;i<T->size+1;i++)
    {
      weights[i] = 0.0;
      location[i] = NONE;
    }
  e = findBottomLeft(T->root->leftEdge); 
  /* *count = 0; */
  while (NULL != e)
    {
      edgeArray[e->head->index+1] = e;
      location[e->head->index+1] = 
	NNIEdgeTest(e,T,avgDistArray,weights + e->head->index + 1);
      e = depthFirstTraverse(T,e);
    } 
  possibleSwaps = makeThreshHeap(p,q,weights,T->size+1,0.0);
  permInverse(p,q,T->size+1);
  /*we put the negative values of weights into a heap, indexed by p
    with the minimum value pointed to by p[1]*/
  /*p[i] is index (in edgeArray) of meEdge with i-th position 
    in the heap, q[j] is the position of meEdge j in the heap */
  /*NOTE: the loop below should test that weights[p[1]] < 0, but
    if compiled with optimization it is possible that weights[p[1]]
    ends up negative and very small, so that changes to the heap
    become cyclic and the loop becomes infinite. To avoid this
    behavior, stop the loop short of 0.0. This is a workaround 
    until the roundoff sensitivity is removed algorithmically */
  while (weights[p[1]] < -1e-8)
    {
      centerEdge = edgeArray[p[1]];
      (*count)++;
      T->weight = T->weight + weights[p[1]];
      NNItopSwitch(T,edgeArray[p[1]],location[p[1]],avgDistArray);
      location[p[1]] = NONE;
      weights[p[1]] = 0.0;  /*after the NNI, this meEdge is in optimal
			      configuration*/
      popHeap(p,q,weights,possibleSwaps--,1);
      /*but we must retest the other four edges*/
      e = centerEdge->head->leftEdge;
      NNIRetestEdge(p,q,e,T,avgDistArray,weights,location,&possibleSwaps);
      e = centerEdge->head->rightEdge;
      NNIRetestEdge(p,q,e,T,avgDistArray,weights,location,&possibleSwaps);     
      e = siblingEdge(centerEdge);
      NNIRetestEdge(p,q,e,T,avgDistArray,weights,location,&possibleSwaps);
      e = centerEdge->tail->parentEdge;
      NNIRetestEdge(p,q,e,T,avgDistArray,weights,location,&possibleSwaps);
    }
  free(p);
  free(q);
  free(location);
  free(edgeArray);
}

void NNIwithoutMatrix(meTree *T, double **D, int *count)
{
  double **avgDistArray;
  avgDistArray = buildAveragesTable(T,D);
  NNI(T,avgDistArray,count);
}

void NNIWithPartialMatrix(meTree *T,double **D,double **A,int *count)
{
  makeOLSAveragesTable(T,D,A);
  NNI(T,A,count);
}


END_SCOPE(fastme)
END_NCBI_SCOPE
