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
* File Description:  bNNI.cpp
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
void WFext(meEdge *e, double **A);
void WFint(meEdge *e, double **A);

void limitedFillTableUp(meEdge *e, meEdge *f, double **A, meEdge *trigger);
void assignBalWeights(meTree *T, double **A);
void updateAveragesMatrix(meTree *T, double **A, meNode *v,int direction);
void bNNItopSwitch(meTree *T, meEdge *e, int direction, double **A);
int bNNIEdgeTest(meEdge *e, meTree *T, double **A, double *weight);
void updatePair(double **A, meEdge *nearEdge, meEdge *farEdge, meNode *closer, meNode *further, double dcoeff, int direction);

int *initPerm(int size);

void reHeapElement(int *p, int *q, double *v, int length, int i);
void pushHeap(int *p, int *q, double *v, int length, int i);
void popHeap(int *p, int *q, double *v, int length, int i);


void bNNIRetestEdge(int *p, int *q, meEdge *e,meTree *T, double **avgDistArray,
		double *weights, int *location, int *possibleSwaps)
{
  int tloc;
  tloc = location[e->head->index+1];
  location[e->head->index+1] =
    bNNIEdgeTest(e,T,avgDistArray,weights + e->head->index+1);
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

int makeThreshHeap(int *p, int *q, double *v, int arraySize, double thresh);

void permInverse(int *p, int *q, int length);

void printMatrix(double **M, int dim, FILE *ofile,meTree *T)
{
  meEdge *e,*f;
  fprintf(ofile,"%d\n",dim-1);
  for(e=depthFirstTraverse(T,NULL);NULL!=e;e=depthFirstTraverse(T,e))
    {
      fprintf(ofile,"%s ",e->head->label);
      for(f=depthFirstTraverse(T,NULL);NULL!=f;f=depthFirstTraverse(T,f))
	fprintf(ofile,"%lf ",M[e->head->index][f->head->index]);
      fprintf(ofile,"\n");
    }
}

void weighTree(meTree *T)
{
  meEdge *e;
  T->weight = 0;
  for(e = depthFirstTraverse(T,NULL);NULL!=e;e=depthFirstTraverse(T,e))
    T->weight += e->distance;
}

void bNNI(meTree *T, double **avgDistArray, int *count)
{
  meEdge *e, *centerEdge;
  meEdge **edgeArray;
  int *p, *location, *q;
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
  if (verbose)
    {
      assignBalWeights(T,avgDistArray);
      weighTree(T);
    }
  e = findBottomLeft(T->root->leftEdge);
  while (NULL != e)
    {
      edgeArray[e->head->index+1] = e;
      location[e->head->index+1] =
	bNNIEdgeTest(e,T,avgDistArray,weights + e->head->index + 1);
      e = depthFirstTraverse(T,e);
    }
  possibleSwaps = makeThreshHeap(p,q,weights,T->size+1,0.0);
  permInverse(p,q,T->size+1);
  /*we put the negative values of weights into a heap, indexed by p
    with the minimum value pointed to by p[1]*/
  /*p[i] is index (in edgeArray) of edge with i-th position
    in the heap, q[j] is the position of edge j in the heap */
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
      if (verbose)
	{
	  T->weight = T->weight + weights[p[1]];
	  printf("New tree weight is %lf.\n",T->weight);
	}
      bNNItopSwitch(T,edgeArray[p[1]],location[p[1]],avgDistArray);
      location[p[1]] = NONE;
      weights[p[1]] = 0.0;  /*after the bNNI, this edge is in optimal
			      configuration*/
      popHeap(p,q,weights,possibleSwaps--,1);
      /*but we must retest the other edges of T*/
      /*CHANGE 2/28/2003 expanding retesting to _all_ edges of T*/
      e = depthFirstTraverse(T,NULL);
      while (NULL != e)
	{
	  bNNIRetestEdge(p,q,e,T,avgDistArray,weights,location,&possibleSwaps);
	  e = depthFirstTraverse(T,e);
	}
    }
  free(p);
  free(q);
  free(location);
  free(edgeArray);
  assignBalWeights(T,avgDistArray);
}



/*This function is the meat of the average distance matrix recalculation*/
/*Idea is: we are looking at the subtree rooted at rootEdge.  The subtree
rooted at closer is closer to rootEdge after the NNI, while the subtree
rooted at further is further to rootEdge after the NNI.  direction tells
the direction of the NNI with respect to rootEdge*/
void updateSubTreeAfterNNI(double **A, meNode *v, meEdge *rootEdge, meNode *closer, meNode *further,
			   double dcoeff, int direction)
{
  meEdge *sib;
  switch(direction)
    {
    case UP: /*rootEdge is below the center edge of the NNI*/
      /*recursive calls to subtrees, if necessary*/
      if (NULL != rootEdge->head->leftEdge)
	updateSubTreeAfterNNI(A, v, rootEdge->head->leftEdge, closer, further, 0.5*dcoeff,UP);
      if (NULL != rootEdge->head->rightEdge)
	updateSubTreeAfterNNI(A, v, rootEdge->head->rightEdge, closer, further, 0.5*dcoeff,UP);
      updatePair(A, rootEdge, rootEdge, closer, further, dcoeff, UP);
      sib = siblingEdge(v->parentEdge);
      A[rootEdge->head->index][v->index] =
	A[v->index][rootEdge->head->index] =
	0.5*A[rootEdge->head->index][sib->head->index] +
	0.5*A[rootEdge->head->index][v->parentEdge->tail->index];
      break;
    case DOWN: /*rootEdge is above the center edge of the NNI*/
      sib = siblingEdge(rootEdge);
      if (NULL != sib)
	updateSubTreeAfterNNI(A, v, sib, closer, further, 0.5*dcoeff, SKEW);
      if (NULL != rootEdge->tail->parentEdge)
	updateSubTreeAfterNNI(A, v, rootEdge->tail->parentEdge, closer, further, 0.5*dcoeff, DOWN);
      updatePair(A, rootEdge, rootEdge, closer, further, dcoeff, DOWN);
      A[rootEdge->head->index][v->index] =
	A[v->index][rootEdge->head->index] =
	0.5*A[rootEdge->head->index][v->leftEdge->head->index] +
	0.5*A[rootEdge->head->index][v->rightEdge->head->index];
      break;
    case SKEW: /*rootEdge is in subtree skew to v*/
      if (NULL != rootEdge->head->leftEdge)
	updateSubTreeAfterNNI(A, v, rootEdge->head->leftEdge, closer, further, 0.5*dcoeff,SKEW);
      if (NULL != rootEdge->head->rightEdge)
	updateSubTreeAfterNNI(A, v, rootEdge->head->rightEdge, closer, further, 0.5*dcoeff,SKEW);
      updatePair(A, rootEdge, rootEdge, closer, further, dcoeff, UP);
      A[rootEdge->head->index][v->index] =
	A[v->index][rootEdge->head->index] =
	0.5*A[rootEdge->head->index][v->leftEdge->head->index] +
	0.5*A[rootEdge->head->index][v->rightEdge->head->index];
      break;
    }
}


/*swapping across edge whose head is v*/
void bNNIupdateAverages(double **A, meNode *v, meEdge *par, meEdge *skew,
			meEdge *swap, meEdge *fixed)
{
  A[v->index][v->index] = 0.25*(A[fixed->head->index][par->head->index] +
				A[fixed->head->index][swap->head->index] +
				A[skew->head->index][par->head->index] +
				A[skew->head->index][swap->head->index]);
  updateSubTreeAfterNNI(A, v, fixed, skew->head, swap->head, 0.25, UP);
  updateSubTreeAfterNNI(A, v, par, swap->head, skew->head, 0.25, DOWN);
  updateSubTreeAfterNNI(A, v, skew, fixed->head, par->head, 0.25, UP);
  updateSubTreeAfterNNI(A, v, swap, par->head, fixed->head, 0.25, SKEW);

}



void bNNItopSwitch(meTree *T, meEdge *e, int direction, double **A)
{
  meEdge *down, *swap, *fixed;
  meNode *u, *v;
  if (verbose)
    {
      printf("Performing branch swap across edge %s ",e->label);
      printf("with ");
      if (LEFT == direction)
	printf("left ");
      else printf("right ");
      printf("subtree.\n");
    }
  down = siblingEdge(e);
  u = e->tail;
  v = e->head;
  if (LEFT == direction)
    {
      swap = e->head->leftEdge;
      fixed = e->head->rightEdge;
      v->leftEdge = down;
    }
  else
    {
      swap = e->head->rightEdge;
      fixed = e->head->leftEdge;
      v->rightEdge = down;
    }
  swap->tail = u;
  down->tail = v;
  if(e->tail->leftEdge == e)
    u->rightEdge = swap;
  else
    u->leftEdge = swap;
  bNNIupdateAverages(A, v, e->tail->parentEdge, down, swap, fixed);
}

double wf5(double D_AD, double D_BC, double D_AC, double D_BD,
	   double D_AB, double D_CD)
{
  double weight;
  weight = 0.25*(D_AC + D_BD + D_AD + D_BC) + 0.5*(D_AB + D_CD);
  return(weight);
}

int bNNIEdgeTest(meEdge *e, meTree *T, double **A, double *weight)
{
  meEdge *f;
  double D_LR, D_LU, D_LD, D_RD, D_RU, D_DU;
  double w1,w2,w0;
  /*if (verbose)
    printf("Branch swap: testing edge %s.\n",e->label);*/
  if ((leaf(e->tail)) || (leaf(e->head)))
    return(NONE);

  f = siblingEdge(e);

  D_LR = A[e->head->leftEdge->head->index][e->head->rightEdge->head->index];
  D_LU = A[e->head->leftEdge->head->index][e->tail->index];
  D_LD = A[e->head->leftEdge->head->index][f->head->index];
  D_RU = A[e->head->rightEdge->head->index][e->tail->index];
  D_RD = A[e->head->rightEdge->head->index][f->head->index];
  D_DU = A[e->tail->index][f->head->index];

  w0 = wf5(D_RU,D_LD,D_LU,D_RD,D_DU,D_LR); /*weight of current config*/
  w1 = wf5(D_RU,D_LD,D_DU,D_LR,D_LU,D_RD); /*weight with L<->D switch*/
  w2 = wf5(D_DU,D_LR,D_LU,D_RD,D_RU,D_LD); /*weight with R<->D switch*/
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
	      printf("Possible swap across %s. ",e->label);
	      printf("Weight dropping by %lf.\n",w0 - w2);
	      printf("New weight would be %lf.\n",T->weight + w2 - w0);
	    }
	  return(RIGHT);
	}
    }
  else if (w2 <= w1) /*w2 <= w1 < w0*/
    {
      *weight = w2 - w0;
      if (verbose)
	{
	  printf("Possible swap across %s. ",e->label);
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
	  printf("Possible swap across %s. ",e->label);
	  printf("Weight dropping by %lf.\n",w0 - w1);
	  printf("New weight should be %lf.\n",T->weight + w1 - w0);
	}
      return(LEFT);
    }
}



/*limitedFillTableUp fills all the entries in D associated with
  e->head,f->head and those edges g->head above e->head, working
  recursively and stopping when trigger is reached*/
void limitedFillTableUp(meEdge *e, meEdge *f, double **A, meEdge *trigger)
{
  meEdge *g,*h;
  g = f->tail->parentEdge;
  if (f != trigger)
    limitedFillTableUp(e,g,A,trigger);
  h = siblingEdge(f);
  A[e->head->index][f->head->index] =
    A[f->head->index][e->head->index] =
    0.5*(A[e->head->index][g->head->index] + A[e->head->index][h->head->index]);
}

void WFext(meEdge *e, double **A) /*works except when e is the one edge
				  inserted to new vertex v by firstInsert*/
{
  meEdge *f, *g;
  if ((leaf(e->head)) && (leaf(e->tail)))
    e->distance = A[e->head->index][e->head->index];
  else if (leaf(e->head))
    {
      f = e->tail->parentEdge;
      g = siblingEdge(e);
      e->distance = 0.5*(A[e->head->index][g->head->index]
			 + A[e->head->index][f->head->index]
			 - A[g->head->index][f->head->index]);
    }
  else
    {
      f = e->head->leftEdge;
      g = e->head->rightEdge;
      e->distance = 0.5*(A[g->head->index][e->head->index]
			 + A[f->head->index][e->head->index]
			 - A[f->head->index][g->head->index]);
    }
}

void WFint(meEdge *e, double **A)
{
  int up, down, left, right;
  up = e->tail->index;
  down = (siblingEdge(e))->head->index;
  left = e->head->leftEdge->head->index;
  right = e->head->rightEdge->head->index;
  e->distance = 0.25*(A[up][left] + A[up][right] + A[left][down] + A[right][down]) - 0.5*(A[down][up] + A[left][right]);
}



void assignBalWeights(meTree *T, double **A)
{
  meEdge *e;
  e = depthFirstTraverse(T,NULL);
  while (NULL != e) {
    if ((leaf(e->head)) || (leaf(e->tail)))
      WFext(e,A);
    else
      WFint(e,A);
    e = depthFirstTraverse(T,e);
  }
}


END_SCOPE(fastme)
END_NCBI_SCOPE
