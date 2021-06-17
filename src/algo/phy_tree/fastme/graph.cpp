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
* File Description:  graph.cpp
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

boolean leaf(meNode *v)
{
  int count = 0;
  if (NULL != v->parentEdge)
    count++;
  if (NULL != v->leftEdge)
    count++;
  if (NULL != v->rightEdge)
    count++;
  if (NULL != v->middleEdge)
    count++;
  if (count > 1)
    return(FALSE_FASTME);
  return(TRUE_FASTME);
}

meSet *addToSet(meNode *v, meSet *X)
{
  if (NULL == X)
    {
      X = (meSet *) malloc(sizeof(meSet));
      X->firstNode = v;
      X->secondNode = NULL;
    }
  else if (NULL == X->firstNode) 
    X->firstNode = v;
  else
    X->secondNode = addToSet(v,X->secondNode);
  return(X);
}

meNode *makeNode(const char *label, meEdge *parentEdge, int index)
{
  meNode *newNode;  /*points to new meNode added to the graph*/
  newNode = (meNode *) malloc(sizeof(meNode));
  strcpy(newNode->label,label);
  newNode->index = index;
  newNode->index2 = -1;
  newNode->parentEdge = parentEdge;
  newNode->leftEdge = NULL;
  newNode->middleEdge = NULL;
  newNode->rightEdge = NULL;
  /*all fields have been initialized*/
  return(newNode);
}

meEdge *makeEdge(const char *label, meNode *tail, meNode *head, double weight)
{
  meEdge *newEdge;
  newEdge = (meEdge *) malloc(sizeof(meEdge));
  strcpy(newEdge->label,label);
  newEdge->tail = tail;
  newEdge->head = head;
  newEdge->distance = weight;
  newEdge->totalweight = 0.0;
  return(newEdge);
}

meTree *newTree()
{
  meTree *T;
  T = (meTree *) malloc(sizeof(meTree));
  T->root = NULL;
  T->size = 0;
  T->weight = -1;
  return(T);
}

void freeSubTree(meEdge *e)
{
  meNode *v;
  meEdge *e1, *e2;
  v = e->head;
  e1 = v->leftEdge;
  if (NULL != e1)
    {
      freeSubTree(e1);
      v->leftEdge = NULL;
    }
  e2 = v->rightEdge;
  if (NULL != e2)
    {
      freeSubTree(e2);
      v->rightEdge = NULL;
    }
  v->parentEdge = NULL;
  free(v);
  e->tail = NULL;
  e->head = NULL;
  free(e);
}

void freeTree(meTree *T)
{
  meNode *v;
  v = T->root;
  if (NULL != v->leftEdge)
    freeSubTree(v->leftEdge);
  v->leftEdge = NULL;
  free(v);
  T->root = NULL;
  free(T);
}

void freeSet(meSet *S)
{
  if (NULL != S)
    freeSet(S->secondNode);
  free(S);
}

/*copyNode returns a copy of v which has all of the fields identical to those
of v, except the meNode pointer fields*/
meNode *copyNode(meNode *v)
{
  meNode *w;
  w = makeNode(v->label,NULL,v->index);
  return(w);
}

meNode *makeNewNode(const char *label, int i)
{
  return(makeNode(label,NULL,i));
}

/*copyEdge calls makeEdge to make a copy of a given meEdge */
/*does not copy all fields*/
meEdge *copyEdge (meEdge *e)
{
  meEdge *newEdge;
  newEdge = makeEdge(e->label,e->tail,e->head,e->distance);
  newEdge->topsize = e->topsize;
  newEdge->bottomsize = e->bottomsize;
  return(newEdge);
}

/*detrifurcate takes the (possibly trifurcated) input tree
  and reroots the meTree to a leaf*/
/*assumes meTree is only trifurcated at root*/
meTree *detrifurcate(meTree *T)
{
  meNode *v, *w = NULL;
  meEdge *e, *f;
  v = T->root;
  if(leaf(v))
    return(T);
  if (NULL != v->parentEdge)
    {
      fprintf(stderr,"Error: root %s is poorly rooted.\n",v->label);
      exit(EXIT_FAILURE);
    }
  for(e = v->middleEdge, v->middleEdge = NULL; NULL != e; e = f )
    {
      w = e->head;
      v = e->tail;
      e->tail = w;
      e->head = v;
      f = w->leftEdge;
      v->parentEdge = e;
      w->leftEdge = e;
      w->parentEdge = NULL;      
    }
  T->root = w;
  return(T);
}

meEdge *siblingEdge(meEdge *e)
{
  if(e == e->tail->leftEdge)
    return(e->tail->rightEdge);
  else
    return(e->tail->leftEdge);
}

void updateSizes(meEdge *e, int direction)
{
  meEdge *f;
  switch(direction)
    {
    case UP:
      f = e->head->leftEdge;
      if (NULL != f)
	updateSizes(f,UP);
      f = e->head->rightEdge;
      if (NULL != f)
	updateSizes(f,UP);
      e->topsize++;
      break;
    case DOWN:
      f = siblingEdge(e);
      if (NULL != f)
	updateSizes(f,UP);
      f = e->tail->parentEdge;
      if (NULL != f)
	updateSizes(f,DOWN);
      e->bottomsize++;
      break;
    }
}      


END_SCOPE(fastme)
END_NCBI_SCOPE
