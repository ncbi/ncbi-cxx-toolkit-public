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
* File Description:  gme.cpp
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

/*from NNI.c*/
void fillTableUp(meEdge *e, meEdge *f, double **A, double **D, meTree *T);

/*from graph.c*/
void updateSizes(meEdge *e, int direction);

/*OLSint and OLSext use the average distance array to calculate weights
  instead of using the meEdge average weight fields*/

void OLSext(meEdge *e, double **A)
{
  meEdge *f, *g;
  if(leaf(e->head))
    {
      f = siblingEdge(e);
      e->distance = 0.5*(A[e->head->index][e->tail->index] 
			 + A[e->head->index][f->head->index]
			 - A[f->head->index][e->tail->index]);
    }
  else
    {
      f = e->head->leftEdge;
      g = e->head->rightEdge;
      e->distance = 0.5*(A[e->head->index][f->head->index]
			 + A[e->head->index][g->head->index]
			 - A[f->head->index][g->head->index]);
    }
}

double wf(double lambda, double D_LR, double D_LU, double D_LD, 
	   double D_RU, double D_RD, double D_DU)
{
  double weight;
  weight = 0.5*(lambda*(D_LU  + D_RD) + (1 -lambda)*(D_LD + D_RU)
		- (D_LR + D_DU));  
  return(weight);
}

void OLSint(meEdge *e, double **A)
{
  double lambda;
  meEdge *left, *right, *sib;
  left = e->head->leftEdge;
  right = e->head->rightEdge;
  sib = siblingEdge(e);
  lambda = ((double) sib->bottomsize*left->bottomsize + 
	    right->bottomsize*e->tail->parentEdge->topsize) /
    (e->bottomsize*e->topsize);
  e->distance = wf(lambda,A[left->head->index][right->head->index],
		   A[left->head->index][e->tail->index],
		   A[left->head->index][sib->head->index],
		   A[right->head->index][e->tail->index],
		   A[right->head->index][sib->head->index],
		   A[sib->head->index][e->tail->index]);
}


void assignOLSWeights(meTree *T, double **A)
{
  meEdge *e;
  e = depthFirstTraverse(T,NULL);
  while (NULL != e) {
    if ((leaf(e->head)) || (leaf(e->tail)))
      OLSext(e,A);
    else
      OLSint(e,A);
    e = depthFirstTraverse(T,e);
  }
}      

/*makes table of average distances from scratch*/
void makeOLSAveragesTable(meTree *T, double **D, double **A)
{
  meEdge *e, *f, *g, *h;
  meEdge *exclude;
  e = f = NULL;
  e = depthFirstTraverse(T,e);
  while (NULL != e)
    {
      f = e;
      exclude = e->tail->parentEdge;
      /*we want to calculate A[e->head][f->head] for all edges
	except those edges which are ancestral to e.  For those
	edges, we will calculate A[e->head][f->head] to have a
	different meaning, later*/
      if(leaf(e->head))
	while (NULL != f)
	  {
	    if (exclude != f)	   
	      {
		if (leaf(f->head))
		  A[e->head->index][f->head->index] = A[f->head->index][e->head->index] = D[e->head->index2][f->head->index2];
		else
		  {
		    g = f->head->leftEdge;
		    h = f->head->rightEdge;
		    A[e->head->index][f->head->index] = A[f->head->index][e->head->index] = (g->bottomsize*A[e->head->index][g->head->index] + h->bottomsize*A[e->head->index][h->head->index])/f->bottomsize; 
		  }
	      }
	    else /*exclude == f*/
	      exclude = exclude->tail->parentEdge; 
	    f = depthFirstTraverse(T,f);
	  }
      else 
	/*e->head is not a leaf, so we go recursively to values calculated for
	  the nodes below e*/
	while(NULL !=f )
	  {
	    if (exclude != f)	      
	      {
		g = e->head->leftEdge;
		h = e->head->rightEdge;
		A[e->head->index][f->head->index] = A[f->head->index][e->head->index] = (g->bottomsize*A[f->head->index][g->head->index] + h->bottomsize*A[f->head->index][h->head->index])/e->bottomsize;
	      }
	    else
	      exclude = exclude->tail->parentEdge;
	    f = depthFirstTraverse(T,f);
	  }

  /*now we move to fill up the rest of the table: we want
    A[e->head->index][f->head->index] for those cases where e is an
    ancestor of f, or vice versa.  We'll do this by choosing e via a
    depth first-search, and the recursing for f up the path to the
    root*/
      f = e->tail->parentEdge;
      if (NULL != f)
	fillTableUp(e,f,A,D,T);	   
      e = depthFirstTraverse(T,e);
    } 

  /*we are indexing this table by vertex indices, but really the
    underlying object is the meEdge set.  Thus, the array is one element
    too big in each direction, but we'll ignore the entries involving the root,
    and instead refer to each meEdge by the head of that edge.  The head of
    the root points to the meEdge ancestral to the rest of the tree, so
    we'll keep track of up distances by pointing to that head*/

  /*10/13/2001: collapsed three depth-first searces into 1*/
}


void GMEcalcDownAverage(meNode *v, meEdge *e, double **D, double **A)
{
  meEdge *left, *right;
  if (leaf(e->head))
    A[e->head->index][v->index] = D[v->index2][e->head->index2]; 
  else
    {
      left = e->head->leftEdge;
      right = e->head->rightEdge;
      A[e->head->index][v->index] = 
	( left->bottomsize * A[left->head->index][v->index] + 
	  right->bottomsize * A[right->head->index][v->index]) 
	/ e->bottomsize;
    }
}

void GMEcalcUpAverage(meNode *v, meEdge *e, double **D, double **A)
{
  meEdge *up, *down;
  if (NULL == e->tail->parentEdge)
    A[v->index][e->head->index] =  D[v->index2][e->tail->index2];
  else
    {
      up = e->tail->parentEdge;
      down = siblingEdge(e);
      A[v->index][e->head->index] = 
	(up->topsize * A[v->index][up->head->index] + 
	 down->bottomsize * A[down->head->index][v->index])
	/ e->topsize;
      }
}

/*this function calculates average distance D_Xv for each X which is
  a meSet of leaves of an induced submeTree of T*/
void GMEcalcNewvAverages(meTree *T, meNode *v, double **D, double **A)
{
  /*loop over edges*/
  /*depth-first search*/
  meEdge *e;
  e = NULL;
  e = depthFirstTraverse(T,e);  /*the downward averages need to be
				  calculated from bottom to top */
  while(NULL != e)
    {
      GMEcalcDownAverage(v,e,D,A);
      e = depthFirstTraverse(T,e);
    }
  
  e = topFirstTraverse(T,e);   /*the upward averages need to be calculated 
				 from top to bottom */
  while(NULL != e)
    {
      GMEcalcUpAverage(v,e,D,A);
      e = topFirstTraverse(T,e);
    }
}

double wf4(double lambda, double lambda2, double D_AB, double D_AC, 
	   double D_BC, double D_Av, double D_Bv, double D_Cv)
{
  return(((1 - lambda) * (D_AC + D_Bv) + (lambda2 - 1)*(D_AB + D_Cv)
	 + (lambda - lambda2)*(D_BC + D_Av)));
}


/*testEdge cacluates what the OLS weight would be if v were inserted into
  T along e.  Compare against known values for inserting along 
  f = e->parentEdge */
/*edges are tested by a top-first, left-first scheme. we presume
  all distances are fixed to the correct weight for 
  e->parentEdge, if e is a left-oriented edge*/
void testEdge(meEdge *e, meNode *v, double **A)
{
  double lambda, lambda2;
  meEdge *par, *sib;
  sib = siblingEdge(e);
  par = e->tail->parentEdge;
  /*C is meSet above e->tail, B is meSet below e, and A is meSet below sib*/
  /*following the nomenclature of Desper & Gascuel*/
  lambda =  (((double) (sib->bottomsize + e->bottomsize*par->topsize))
	     / ((1 + par->topsize)*(par->bottomsize)));
  lambda2 = (((double) (sib->bottomsize + e->bottomsize*par->topsize))
	     / ((1 + e->bottomsize)*(e->topsize)));
  e->totalweight = par->totalweight 
    + wf4(lambda,lambda2,A[e->head->index][sib->head->index],
	  A[sib->head->index][e->tail->index],
	  A[e->head->index][e->tail->index],
	  A[sib->head->index][v->index],A[e->head->index][v->index],
	  A[v->index][e->tail->index]);  
}

void printDoubleTable(double **A, int d)
{
  int i,j;
  for(i=0;i<d;i++)
    {
      for(j=0;j<d;j++)
	printf("%lf ", A[i][j]);
      printf("\n");
    }
}

void GMEsplitEdge(meTree *T, meNode *v, meEdge *e, double **A);

meTree *GMEaddSpecies(meTree *T,meNode *v, double **D, double **A) 
     /*the key function of the program addSpeices inserts
       the meNode v to the meTree T.  It uses testEdge to see what the
       weight would be if v split a particular edge.  Weights are assigned by
       OLS formula*/
{
  meTree *T_e;
  meEdge *e; /*loop variable*/
  meEdge *e_min; /*points to best meEdge seen thus far*/
  double w_min = 0.0;   /*used to keep track of meTree weights*/

  if (verbose)
    printf("Adding %s.\n",v->label);
 
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
	  e->topsize = 1;
	  e->bottomsize = 1;
	  A[v->index][v->index] = D[v->index2][T->root->index2];
	  T->root->leftEdge = v->parentEdge = e;
	  T->size = 2;
	  return(T); 
	}
  /*CASE 3: T has at least two nodes and an edge.  Insert new node
    by breaking one of the edges*/
  
  v->index = T->size;
  /*if (!(T->size % 100))
    printf("T->size is %d\n",T->size);*/
  GMEcalcNewvAverages(T,v,D,A);
  /*calcNewvAverges will assign values to all the meEdge averages of T which
    include the meNode v.  Will do so using pre-existing averages in T and
    information from A,D*/
  e_min = T->root->leftEdge;  
  e = e_min->head->leftEdge;  
  while (NULL != e)
    {
      testEdge(e,v,A); 
      /*testEdge tests weight of meTree if loop variable 
	e is the meEdge split, places this weight in e->totalweight field */
      if (e->totalweight < w_min)
	{
	  e_min = e;
	  w_min = e->totalweight;
	}
      e = topFirstTraverse(T,e);
    }
  /*e_min now points at the meEdge we want to split*/
  GMEsplitEdge(T,v,e_min,A);
  return(T);
}

void updateSubTreeAverages(double **A, meEdge *e, meNode *v, int direction);

/*the ME version of updateAveragesMatrix does not update the entire matrix
  A, but updates A[v->index][w->index] whenever this represents an average
  of 1-distant or 2-distant subtrees*/

void GMEupdateAveragesMatrix(double **A, meEdge *e, meNode *v, meNode *newNode)
{
  meEdge *sib, *par, *left, *right;
  sib = siblingEdge(e);
  left = e->head->leftEdge;
  right = e->head->rightEdge;
  par = e->tail->parentEdge;

  /*we need to update the matrix A so all 1-distant, 2-distant, and
    3-distant averages are correct*/
  
  /*first, initialize the newNode entries*/
  /*1-distant*/
  A[newNode->index][newNode->index] = 
    (e->bottomsize*A[e->head->index][e->head->index]
     + A[v->index][e->head->index])
    / (e->bottomsize + 1);
  /*1-distant for v*/
  A[v->index][v->index] = 
    (e->bottomsize*A[e->head->index][v->index] 
     + e->topsize*A[v->index][e->head->index])
    / (e->bottomsize + e->topsize);

  /*2-distant for v,newNode*/
  A[v->index][newNode->index] = A[newNode->index][v->index] = 
    A[v->index][e->head->index];
  
  /*second 2-distant for newNode*/
  A[newNode->index][e->tail->index] = A[e->tail->index][newNode->index]
    = (e->bottomsize*A[e->head->index][e->tail->index]
       + A[v->index][e->tail->index])/(e->bottomsize + 1);
  /*third 2-distant for newNode*/
  A[newNode->index][e->head->index] = A[e->head->index][newNode->index]
    = A[e->head->index][e->head->index];
   
  if (NULL != sib)
    {
      /*fourth and last 2-distant for newNode*/
      A[newNode->index][sib->head->index] =
	A[sib->head->index][newNode->index] = 
	(e->bottomsize*A[sib->head->index][e->head->index]
	 + A[sib->head->index][v->index]) / (e->bottomsize + 1);
      updateSubTreeAverages(A,sib,v,SKEW); /*updates sib and below*/
    }
  if (NULL != par)
    {
      if (e->tail->leftEdge == e)
	updateSubTreeAverages(A,par,v,LEFT); /*updates par and above*/
      else
	updateSubTreeAverages(A,par,v,RIGHT);
    }
  if (NULL != left)
    updateSubTreeAverages(A,left,v,UP); /*updates left and below*/
  if (NULL != right)
    updateSubTreeAverages(A,right,v,UP); /*updates right and below*/  

  /*1-dist for e->head*/
  A[e->head->index][e->head->index] = 
    (e->topsize*A[e->head->index][e->head->index]
     + A[e->head->index][v->index]) / (e->topsize+1);
  /*2-dist for e->head (v,newNode,left,right)
    taken care of elsewhere*/
  /*3-dist with e->head either taken care of elsewhere (below)
    or unchanged (sib,e->tail)*/
  
  /*symmetrize the matrix (at least for distant-2 subtrees) */
  A[v->index][e->head->index] = A[e->head->index][v->index];
  /*and distant-3 subtrees*/
  A[e->tail->index][v->index] = A[v->index][e->tail->index];
  if (NULL != left)
    A[v->index][left->head->index] = A[left->head->index][v->index];
  if (NULL != right)
    A[v->index][right->head->index] = A[right->head->index][v->index];
  if (NULL != sib)
    A[v->index][sib->head->index] = A[sib->head->index][v->index];

}      
  
void GMEsplitEdge(meTree *T, meNode *v, meEdge *e, double **A)
{
  char nodelabel[NODE_LABEL_LENGTH];
  char edgelabel[EDGE_LABEL_LENGTH];
  meEdge *newPendantEdge;
  meEdge *newInternalEdge;
  meNode *newNode;
    
  sprintf(nodelabel,"I%d",T->size+1); 
  newNode = makeNewNode(nodelabel,T->size + 1);  
  
  sprintf(edgelabel,"E%d",T->size); 
  newPendantEdge = makeEdge(edgelabel,newNode,v,0.0);   
  
  sprintf(edgelabel,"E%d",T->size+1); 
  newInternalEdge = makeEdge(edgelabel,newNode,e->head,0.0);   
  
  if (verbose)
    printf("Inserting meNode %s on meEdge %s between nodes %s and %s.\n",
	   v->label,e->label,e->tail->label,e->head->label); 
  /*update the matrix of average distances*/
  /*also updates the bottomsize, topsize fields*/
  
  GMEupdateAveragesMatrix(A,e,v,newNode);

  newNode->parentEdge = e;
  e->head->parentEdge = newInternalEdge;
  v->parentEdge = newPendantEdge;
  e->head = newNode;
  
  T->size = T->size + 2;

  if (e->tail->leftEdge == e) 
    {
      newNode->leftEdge = newInternalEdge;
      newNode->rightEdge = newPendantEdge;
    }
  else
    {
      newNode->leftEdge = newInternalEdge;
      newNode->rightEdge = newPendantEdge;
    }
  
  /*assign proper topsize, bottomsize values to the two new Edges*/
  
  newPendantEdge->bottomsize = 1; 
  newPendantEdge->topsize = e->bottomsize + e->topsize;
  
  newInternalEdge->bottomsize = e->bottomsize;
  newInternalEdge->topsize = e->topsize;  /*off by one, but we adjust
					    that below*/
  
  /*and increment these fields for all other edges*/
  updateSizes(newInternalEdge,UP);
  updateSizes(e,DOWN);
}

void updateSubTreeAverages(double **A, meEdge *e, meNode *v, int direction)
     /*the monster function of this program*/
{
  meEdge *sib, *left, *right, *par;
  left = e->head->leftEdge;
  right = e->head->rightEdge;
  sib = siblingEdge(e);
  par = e->tail->parentEdge;
  switch(direction)
    {
      /*want to preserve correctness of 
	all 1-distant, 2-distant, and 3-distant averages*/	
      /*1-distant updated at meEdge splitting the two trees*/
      /*2-distant updated:
	(left->head,right->head) and (head,tail) updated at
	a given edge.  Note, NOT updating (head,sib->head)!
	(That would lead to multiple updating).*/
      /*3-distant updated: at meEdge in center of quartet*/
    case UP: /*point of insertion is above e*/
      /*1-distant average of nodes below e to 
       nodes above e*/
      A[e->head->index][e->head->index] = 
	(e->topsize*A[e->head->index][e->head->index] + 
	 A[e->head->index][v->index])/(e->topsize + 1);      
      /*2-distant average of nodes below e to 
	nodes above parent of e*/
      A[e->head->index][par->head->index] = 
	A[par->head->index][e->head->index] = 
	(par->topsize*A[par->head->index][e->head->index]
	 + A[e->head->index][v->index]) / (par->topsize + 1);
      /*must do both 3-distant averages involving par*/
      if (NULL != left)
	{
	  updateSubTreeAverages(A, left, v, UP); /*and recursive call*/
	  /*3-distant average*/
	  A[par->head->index][left->head->index]
	    = A[left->head->index][par->head->index]
	    = (par->topsize*A[par->head->index][left->head->index]
	       + A[left->head->index][v->index]) / (par->topsize + 1);
	}
      if (NULL != right)
	{
	  updateSubTreeAverages(A, right, v, UP);
	  A[par->head->index][right->head->index]
	    = A[right->head->index][par->head->index]
	    = (par->topsize*A[par->head->index][right->head->index]
	       + A[right->head->index][v->index]) / (par->topsize + 1);
	}
      break;
    case SKEW: /*point of insertion is skew to e*/
      /*1-distant average of nodes below e to 
	nodes above e*/
      A[e->head->index][e->head->index] = 
	(e->topsize*A[e->head->index][e->head->index] + 
	 A[e->head->index][v->index])/(e->topsize + 1);      
      /*no 2-distant averages to update in this case*/
      /*updating 3-distant averages involving sib*/
      if (NULL != left)
	{
	  updateSubTreeAverages(A, left, v, UP);
	  A[sib->head->index][left->head->index]
	    = A[left->head->index][sib->head->index]
	    = (sib->bottomsize*A[sib->head->index][left->head->index]
	       + A[left->head->index][v->index]) / (sib->bottomsize + 1);
	}
      if (NULL != right)
	{
	  updateSubTreeAverages(A, right, v, UP);
	  A[sib->head->index][right->head->index]
	    = A[right->head->index][sib->head->index]
	    = (sib->bottomsize*A[par->head->index][right->head->index]
	       + A[right->head->index][v->index]) / (sib->bottomsize + 1);
	}
      break;


    case LEFT: /*point of insertion is below the meEdge left*/
      /*1-distant average*/
      A[e->head->index][e->head->index] = 
	(e->bottomsize*A[e->head->index][e->head->index] + 
	 A[v->index][e->head->index])/(e->bottomsize + 1);        
      /*2-distant averages*/
      A[e->head->index][e->tail->index] = 
	A[e->tail->index][e->head->index] = 
	(e->bottomsize*A[e->head->index][e->tail->index] + 
	 A[v->index][e->tail->index])/(e->bottomsize + 1);  
      A[left->head->index][right->head->index] = 
	A[right->head->index][left->head->index] = 
	(left->bottomsize*A[right->head->index][left->head->index]
	 + A[right->head->index][v->index]) / (left->bottomsize+1);
      /*3-distant avereages involving left*/
      if (NULL != sib)
	{
	  updateSubTreeAverages(A, sib, v, SKEW);
	  A[left->head->index][sib->head->index]
	    = A[sib->head->index][left->head->index]
	    = (left->bottomsize*A[left->head->index][sib->head->index]
	       + A[sib->head->index][v->index]) / (left->bottomsize + 1);
	}
      if (NULL != par)
	{
	  if (e->tail->leftEdge == e)
	    updateSubTreeAverages(A, par, v, LEFT);
	  else
	    updateSubTreeAverages(A, par, v, RIGHT);
	  A[left->head->index][par->head->index]
	    = A[par->head->index][left->head->index]
	    = (left->bottomsize*A[left->head->index][par->head->index]
	       + A[v->index][par->head->index]) / (left->bottomsize + 1);
	}
      break;    
    case RIGHT: /*point of insertion is below the meEdge right*/
      /*1-distant average*/
      A[e->head->index][e->head->index] = 
	(e->bottomsize*A[e->head->index][e->head->index] + 
	 A[v->index][e->head->index])/(e->bottomsize + 1);        
      /*2-distant averages*/
      A[e->head->index][e->tail->index] = 
	A[e->tail->index][e->head->index] = 
	(e->bottomsize*A[e->head->index][e->tail->index] + 
	 A[v->index][e->tail->index])/(e->bottomsize + 1);  
      A[left->head->index][right->head->index] = 
	A[right->head->index][left->head->index] = 
	(right->bottomsize*A[right->head->index][left->head->index]
	 + A[left->head->index][v->index]) / (right->bottomsize+1);
      /*3-distant avereages involving right*/
      if (NULL != sib)
	{
	  updateSubTreeAverages(A, sib, v, SKEW);
	  A[right->head->index][sib->head->index]
	    = A[sib->head->index][right->head->index]
	    = (right->bottomsize*A[right->head->index][sib->head->index]
	       + A[sib->head->index][v->index]) / (right->bottomsize + 1);
	}
      if (NULL != par)
	{
	  if (e->tail->leftEdge == e)
	    updateSubTreeAverages(A, par, v, LEFT);
	  else
	    updateSubTreeAverages(A, par, v, RIGHT);
	  A[right->head->index][par->head->index]
	    = A[par->head->index][right->head->index]
	    = (right->bottomsize*A[right->head->index][par->head->index]
	       + A[v->index][par->head->index]) / (right->bottomsize + 1);
	}

      break;
    }
}

void assignBottomsize(meEdge *e)
{
  if (leaf(e->head))
    e->bottomsize = 1;
  else
    {
      assignBottomsize(e->head->leftEdge);
      assignBottomsize(e->head->rightEdge);
      e->bottomsize = e->head->leftEdge->bottomsize
	+ e->head->rightEdge->bottomsize;
    }
}

void assignTopsize(meEdge *e, int numLeaves)
{
  if (NULL != e)
    {
      e->topsize = numLeaves - e->bottomsize;
      assignTopsize(e->head->leftEdge,numLeaves);
      assignTopsize(e->head->rightEdge,numLeaves);
    }
}

void assignAllSizeFields(meTree *T)
{
  assignBottomsize(T->root->leftEdge);
  assignTopsize(T->root->leftEdge,T->size/2 + 1);
}


END_SCOPE(fastme)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2004/05/21 21:41:03  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.1  2004/02/10 15:16:02  jcherry
 * Initial version
 *
 * ===========================================================================
 */
