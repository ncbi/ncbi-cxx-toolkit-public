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
* File Description:  bme.cpp
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

meEdge *siblingEdge(meEdge *e);
boolean leaf(meNode *v);
meEdge *depthFirstTraverse(meTree *T, meEdge *e);
meEdge *topFirstTraverse(meTree *T, meEdge *e);

void BalWFext(meEdge *e, double **A) /*works except when e is the one edge
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

void BalWFint(meEdge *e, double **A)
{
  int up, down, left, right;
  up = e->tail->index;
  down = (siblingEdge(e))->head->index;
  left = e->head->leftEdge->head->index;
  right = e->head->rightEdge->head->index;
  e->distance = 0.25*(A[up][left] + A[up][right] + A[left][down] + A[right][down]) - 0.5*(A[down][up] + A[left][right]);
} 

  

void assignBMEWeights(meTree *T, double **A)
{
  meEdge *e;
  e = depthFirstTraverse(T,NULL);
  while (NULL != e) {
    if ((leaf(e->head)) || (leaf(e->tail)))
      BalWFext(e,A);
    else
      BalWFint(e,A);
    e = depthFirstTraverse(T,e);
  }
}      

void BMEcalcDownAverage(meTree *T, meNode *v, meEdge *e, double **D, double **A)
{
  meEdge  *left, *right;
  if (leaf(e->head))
    A[e->head->index][v->index] = D[v->index2][e->head->index2]; 
  else
    {
      left = e->head->leftEdge;
      right = e->head->rightEdge;
      A[e->head->index][v->index] = 0.5 * A[left->head->index][v->index] 
	+ 0.5 * A[right->head->index][v->index];
    }
}

void BMEcalcUpAverage(meTree *T, meNode *v, meEdge *e, double **D, double **A)
{
  meEdge *up,*down;
  if (T->root == e->tail)
    A[v->index][e->head->index] = D[v->index2][e->tail->index2];
  /*for now, use convention
    v->index first => looking up
    v->index second => looking down */
  else
    {
      up = e->tail->parentEdge;
      down = siblingEdge(e);
      A[v->index][e->head->index] = 0.5 * A[v->index][up->head->index]
	+0.5  * A[down->head->index][v->index];
    }
}


void BMEcalcNewvAverages(meTree *T, meNode *v, double **D, double **A)
{
  /*loop over edges*/
  /*depth-first search*/
  meEdge *e;
  e = NULL;
  e = depthFirstTraverse(T,e);  /*the downward averages need to be
				  calculated from bottom to top */
  while(NULL != e)
    {
      BMEcalcDownAverage(T,v,e,D,A);
      e = depthFirstTraverse(T,e);
    }
  
  e = topFirstTraverse(T,e);   /*the upward averages need to be calculated 
				 from top to bottom */
  while(NULL != e)
    {
      BMEcalcUpAverage(T,v,e,D,A);
      e = topFirstTraverse(T,e);
    }
}


/*update Pair updates A[nearEdge][farEdge] and makes recursive call to subtree
  beyond farEdge*/
/*root is head or tail of meEdge being split, depending on direction toward
  v*/
void updatePair(double **A, meEdge *nearEdge, meEdge *farEdge, meNode *v,
		meNode *root, double dcoeff, int direction)
{
  meEdge *sib;
  switch(direction) /*the various cases refer to where the new vertex has
		      been inserted, in relation to the meEdge nearEdge*/
    {
    case UP: /*this case is called when v has been inserted above 
	       or skew to farEdge*/
      /*do recursive calls first!*/
      if (NULL != farEdge->head->leftEdge)
	updatePair(A,nearEdge,farEdge->head->leftEdge,v,root,dcoeff,UP);
      if (NULL != farEdge->head->rightEdge)
	updatePair(A,nearEdge,farEdge->head->rightEdge,v,root,dcoeff,UP);
      A[farEdge->head->index][nearEdge->head->index] =
	A[nearEdge->head->index][farEdge->head->index]
	= A[farEdge->head->index][nearEdge->head->index]
	+ dcoeff*A[farEdge->head->index][v->index]
	- dcoeff*A[farEdge->head->index][root->index];
      break; 
    case DOWN: /*called when v has been inserted below farEdge*/
      if (NULL != farEdge->tail->parentEdge)
	updatePair(A,nearEdge,farEdge->tail->parentEdge,v,root,dcoeff,DOWN);
      sib = siblingEdge(farEdge);
      if (NULL != sib)
	updatePair(A,nearEdge,sib,v,root,dcoeff,UP);
      A[farEdge->head->index][nearEdge->head->index] =
	A[nearEdge->head->index][farEdge->head->index]
	= A[farEdge->head->index][nearEdge->head->index]
	+ dcoeff*A[v->index][farEdge->head->index]
	- dcoeff*A[farEdge->head->index][root->index];	  
    }
}

void updateSubTree(double **A, meEdge *nearEdge, meNode *v, meNode *root,
		   meNode *newNode, double dcoeff, int direction)
{
  meEdge *sib;
  switch(direction)
    {
    case UP: /*newNode is above the meEdge nearEdge*/
      A[v->index][nearEdge->head->index] = A[nearEdge->head->index][v->index];
      A[newNode->index][nearEdge->head->index] = 
	A[nearEdge->head->index][newNode->index] =
	A[nearEdge->head->index][root->index];	
      if (NULL != nearEdge->head->leftEdge)
	updateSubTree(A, nearEdge->head->leftEdge, v, root, newNode, 0.5*dcoeff, UP);
      if (NULL != nearEdge->head->rightEdge)
	updateSubTree(A, nearEdge->head->rightEdge, v, root, newNode, 0.5*dcoeff, UP);
      updatePair(A, nearEdge, nearEdge, v, root, dcoeff, UP);
      break;
    case DOWN: /*newNode is below the meEdge nearEdge*/
      A[nearEdge->head->index][v->index] = A[v->index][nearEdge->head->index];
      A[newNode->index][nearEdge->head->index] = 
	A[nearEdge->head->index][newNode->index] =
	0.5*(A[nearEdge->head->index][root->index] 
	     + A[v->index][nearEdge->head->index]);
      sib = siblingEdge(nearEdge);
      if (NULL != sib)
	updateSubTree(A, sib, v, root, newNode, 0.5*dcoeff, SKEW);
      if (NULL != nearEdge->tail->parentEdge)
	updateSubTree(A, nearEdge->tail->parentEdge, v, root, newNode, 0.5*dcoeff, DOWN);
      updatePair(A, nearEdge, nearEdge, v, root, dcoeff, DOWN);
      break;
    case SKEW: /*newNode is neither above nor below nearEdge*/
      A[v->index][nearEdge->head->index] = A[nearEdge->head->index][v->index];
      A[newNode->index][nearEdge->head->index] = 
	A[nearEdge->head->index][newNode->index] =
	0.5*(A[nearEdge->head->index][root->index] + 
	     A[nearEdge->head->index][v->index]);	
      if (NULL != nearEdge->head->leftEdge)
	updateSubTree(A, nearEdge->head->leftEdge, v, root, newNode, 0.5*dcoeff,SKEW);
      if (NULL != nearEdge->head->rightEdge)
	updateSubTree(A, nearEdge->head->rightEdge, v, root, newNode, 0.5*dcoeff,SKEW);
      updatePair(A, nearEdge, nearEdge, v, root, dcoeff, UP);
    }
}


/*we update all the averages for nodes (u1,u2), where the insertion point of 
  v is in "direction" from both u1 and u2 */
/*The general idea is to proceed in a direction from those edges already corrected
 */

/*r is the root of the meTree relative to the inserted node*/

void BMEupdateAveragesMatrix(double **A, meEdge *e, meNode *v,meNode *newNode)
{
  meEdge *sib, *par, *left, *right;
  /*first, update the v,newNode entries*/
  A[newNode->index][newNode->index] = 0.5*(A[e->head->index][e->head->index]
					   + A[v->index][e->head->index]);
  A[v->index][newNode->index] = A[newNode->index][v->index] = 
    A[v->index][e->head->index];
  A[v->index][v->index] = 
    0.5*(A[e->head->index][v->index] + A[v->index][e->head->index]);
  left = e->head->leftEdge;
  right = e->head->rightEdge;
  if (NULL != left)
    updateSubTree(A,left,v,e->head,newNode,0.25,UP); /*updates left and below*/
  if (NULL != right)
    updateSubTree(A,right,v,e->head,newNode,0.25,UP); /*updates right and below*/
  sib = siblingEdge(e);
  if (NULL != sib)
    updateSubTree(A,sib,v,e->head,newNode,0.25,SKEW); /*updates sib and below*/
  par = e->tail->parentEdge;
  if (NULL != par)
    updateSubTree(A,par,v,e->head,newNode,0.25,DOWN); /*updates par and above*/

  /*must change values A[e->head][*] last, as they are used to update
    the rest of the matrix*/
  A[newNode->index][e->head->index] = A[e->head->index][newNode->index]
    = A[e->head->index][e->head->index];
  A[v->index][e->head->index] = A[e->head->index][v->index];

  updatePair(A,e,e,v,e->head,0.5,UP); /*updates e->head fields only*/
}      

/*A is meTree below sibling, B is meTree below edge, C is meTree above edge*/
double wf3(double D_AB, double D_AC, double D_kB, double D_kC)
{
  return(D_AC + D_kB - D_AB - D_kC);
}

void BMEtestEdge(meEdge *e, meNode *v, double **A)
{
  meEdge *up, *down;
  down = siblingEdge(e);
  up = e->tail->parentEdge;
  e->totalweight = wf3(A[e->head->index][down->head->index],
		      A[down->head->index][e->tail->index],
		      A[e->head->index][v->index],
		      A[v->index][e->tail->index])
    + up->totalweight;
}

void BMEsplitEdge(meTree *T, meNode *v, meEdge *e, double **A)
{
  meEdge *newPendantEdge;
  meEdge *newInternalEdge;
  meNode *newNode;
  char nodeLabel[NODE_LABEL_LENGTH];
  char edgeLabel1[EDGE_LABEL_LENGTH];
  char edgeLabel2[EDGE_LABEL_LENGTH];
  sprintf(nodeLabel,"I%d",T->size+1);
  sprintf(edgeLabel1,"E%d",T->size);
  sprintf(edgeLabel2,"E%d",T->size+1);
  

  /*make the new meNode and edges*/
  newNode = makeNewNode(nodeLabel,T->size+1);
  newPendantEdge = makeEdge(edgeLabel1,newNode,v,0.0);
  newInternalEdge = makeEdge(edgeLabel2,newNode,e->head,0.0);

  /*update the matrix of average distances*/
  BMEupdateAveragesMatrix(A,e,v,newNode);


  /*put them in the correct topology*/


  newNode->parentEdge = e;
  e->head->parentEdge = newInternalEdge;
  v->parentEdge = newPendantEdge;
  e->head = newNode;

  T->size = T->size + 2;    

  if (e->tail->leftEdge == e) 
    /*actually this is totally arbitrary and probably unnecessary*/
    {
      newNode->leftEdge = newInternalEdge;
      newNode->rightEdge = newPendantEdge;
    }
  else
    {
      newNode->leftEdge = newInternalEdge;
      newNode->rightEdge = newPendantEdge;
    }
}

meTree *BMEaddSpecies(meTree *T,meNode *v, double **D, double **A) 
     /*the key function of the program addSpeices inserts
       the meNode v to the meTree T.  It uses testEdge to see what the relative
       weight would be if v split a particular edge.  Once insertion point
       is found, v is added to T, and A is updated.  Edge weights
       are not assigned until entire meTree is build*/

{
  meTree *T_e;
  meEdge *e; /*loop variable*/
  meEdge *e_min; /*points to best meEdge seen thus far*/
  double w_min = 0.0;   /*used to keep track of meTree weights*/
  
  /*initialize variables as necessary*/
  
  /*CASE 1: T is empty, v is the first node*/
  if (NULL == T)  /*create a meTree with v as only vertex, no edges*/
    {
      T_e = newTree();
      T_e->root = v;  
      /*note that we are rooting T arbitrarily at a leaf.  
	T->root is not the phylogenetic root*/
      v->index = 0;
      T_e->size = 1;
      return(T_e);      
    }
  /*CASE 2: T is a single-vertex tree*/
  if (1 == T->size)
	{
	  v->index = 1;
	  e = makeEdge("",T->root,v,0.0);
	  sprintf(e->label,"E1");
	  A[v->index][v->index] = D[v->index2][T->root->index2];
	  T->root->leftEdge = v->parentEdge = e;
	  T->size = 2;
	  return(T); 
	}
  /*CASE 3: T has at least two nodes and an edge.  Insert new node
    by breaking one of the edges*/
  
  v->index = T->size;
  BMEcalcNewvAverages(T,v,D,A);
  /*calcNewvAverages will update A for the row and column 
    include the meNode v.  Will do so using pre-existing averages in T and
    information from A,D*/
  e_min = T->root->leftEdge;
  e = e_min->head->leftEdge;
  while (NULL != e)
    {
      BMEtestEdge(e,v,A); 
      /*testEdge tests weight of meTree if loop variable 
	e is the meEdge split, places this value in the e->totalweight field */
      if (e->totalweight < w_min)
	{
	  e_min = e;
	  w_min = e->totalweight;
	}
      e = topFirstTraverse(T,e);
    }
  /*e_min now points at the meEdge we want to split*/
  if (verbose)
    
    printf("Inserting %s between %s and %s on %s\n",v->label,e_min->tail->label,
	   e_min->head->label,e_min->label);
  BMEsplitEdge(T,v,e_min,A);
  return(T);
}

/*calcUpAverages will ensure that A[e->head->index][f->head->index] is
  filled for any f >= g.  Works recursively*/
void calcUpAverages(double **D, double **A, meEdge *e, meEdge *g)
{
  meNode *u,*v;
  meEdge *s;
  if (!(leaf(g->tail)))
    {
      calcUpAverages(D,A,e,g->tail->parentEdge);
      s = siblingEdge(g);
      u = g->tail;
      v = s->head;
      A[e->head->index][g->head->index] = A[g->head->index][e->head->index]
	= 0.5*(A[e->head->index][u->index] + A[e->head->index][v->index]);
    }
}

void makeBMEAveragesTable(meTree *T, double **D, double **A)
{
  meEdge *e, *f, *exclude;
  meNode *u,*v;
  /*first, let's deal with the averages involving the root of T*/
  e = T->root->leftEdge;
  f = depthFirstTraverse(T,NULL);
  while (NULL != f) {
    if (leaf(f->head))
      A[e->head->index][f->head->index] = A[f->head->index][e->head->index]
	= D[e->tail->index2][f->head->index2];
    else
      {
	u = f->head->leftEdge->head;
	v = f->head->rightEdge->head;
	A[e->head->index][f->head->index] = A[f->head->index][e->head->index]
	  = 0.5*(A[e->head->index][u->index] + A[e->head->index][v->index]);
      }
    f = depthFirstTraverse(T,f);
  }
 e = depthFirstTraverse(T,NULL);
  while (T->root->leftEdge != e) {
    f = exclude = e;
    while (T->root->leftEdge != f) {
      if (f == exclude)
	exclude = exclude->tail->parentEdge;
      else if (leaf(e->head))
	{
	  if (leaf(f->head))
	    A[e->head->index][f->head->index] = 
	      A[f->head->index][e->head->index]
	      = D[e->head->index2][f->head->index2];
	  else
	    {
	      u = f->head->leftEdge->head; /*since f is chosen using a
					     depth-first search, other values
					     have been calculated*/
	      v = f->head->rightEdge->head;
	      A[e->head->index][f->head->index] 
		= A[f->head->index][e->head->index] 
		= 0.5*(A[e->head->index][u->index] + A[e->head->index][v->index]);
	    }
	}
      else
	{
	  u = e->head->leftEdge->head;
	  v = e->head->rightEdge->head;
	  A[e->head->index][f->head->index] = A[f->head->index][e->head->index] = 0.5*(A[f->head->index][u->index] + A[f->head->index][v->index]);
	}
      f = depthFirstTraverse(T,f);
    }    
    e = depthFirstTraverse(T,e);
  }
  e = depthFirstTraverse(T,NULL);
  while (T->root->leftEdge != e)
    {
      calcUpAverages(D,A,e,e); /*calculates averages for 
				 A[e->head->index][g->head->index] for
				 any meEdge g in path from e to root of tree*/ 
      e = depthFirstTraverse(T,e);
    }
} /*makeAveragesMatrix*/


END_SCOPE(fastme)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2004/05/21 21:41:03  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.1  2004/02/10 15:16:00  jcherry
 * Initial version
 *
 * ===========================================================================
 */
