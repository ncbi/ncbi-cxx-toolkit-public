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
* File Description:  inputs.cpp
*
*    A part of the Miminum Evolution algorithm
*
*/

#include <ncbi_pch.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "fastme.h"
#include "graph.h"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(fastme)

boolean leaf(meNode *v);

meEdge *depthFirstTraverse(meTree *T, meEdge *e);

void compareSets(meTree *T, meSet *S, FILE *ofile)
{
  meEdge *e;
  meNode *v,*w;
  meSet *X;
  e = depthFirstTraverse(T,NULL);
  while (NULL != e)
    {
      v = e->head;
      for(X = S; NULL != X; X = X->secondNode)
	{
	  w = X->firstNode;
	  if (0 == strcmp(v->label,w->label))
	    {
	      v->index2 = w->index2;
	    w->index2 = -1;
	    break;
	    }
	}
      e = depthFirstTraverse(T,e);
    }
  v = T->root;
  for(X = S; NULL != X; X = X->secondNode)
    {
      w = X->firstNode;
      if (0 == strcmp(v->label,w->label))
	{
	  v->index2 = w->index2;
	  w->index2 = -1;
	  break;
	}
    }
  if (-1 == v->index2)
    {
      fprintf(stderr,"Error leaf %s in meTree not in distance matrix.\n",v->label);
      exit(EXIT_FAILURE);
}
  e = depthFirstTraverse(T,NULL);
  while (NULL != e)
    {
      v = e->head;
      if ((leaf(v)) && (-1 == v->index2))
	{
	  fprintf(stderr,"Error leaf %s in meTree not in distance matrix.\n",v->label);
	  exit(EXIT_FAILURE);
	}
      e = depthFirstTraverse(T,e);
      }
  for(X = S; NULL != X; X = X->secondNode)
    if (X->firstNode->index2 > -1)
      {
	fprintf(ofile,"(v1:0.0)v2;");
	fclose(ofile);
	fprintf(stderr,"Error meNode %s in matrix but not a leaf in tree.\n",X->firstNode->label);
	exit(EXIT_FAILURE);
      }
}

void freeMatrix(double **D, int size)
{
  int i;
  for(i=0;i<size;i++)
    free(D[i]);
  free(D);
}

double **loadMatrix(double **table_in, char **labels, int *size_in, meSet *S)
{
/*   char nextString[MAX_EVENT_NAME]; */
  meNode *v;
  double **table;
  int *size;
  int i,j;

  size = size_in;
  if ((*size < 0) || (*size > MAXSIZE))
    {
      printf("Problem inputting size.\n");
      exit(EXIT_FAILURE);
    }
  table = (double **) malloc(*size*sizeof(double *));
  for(i=0;i<*size;i++)
    {
      j = 0;
      table[i] = (double *) malloc(*size*sizeof(double));
/*       if (!(fscanf(ifile,"%s",nextString))) */
/*       { */
/*           fprintf(stderr,"Error loading label %d.\n",i); */
/*           exit(EXIT_FAILURE); */
/*       } */
/*       v = makeNewNode(nextString,-1); */
      v = makeNewNode(labels[i],-1);
      v->index2 = i;
      S = addToSet(v,S);
      while (j < *size)
	  {
/*           if (!(fscanf(ifile,"%s",nextString))) */
/* 	      { */
/*               fprintf(stderr,"Error loading (%d,%d)-entry.\n",i,j); */
/*               exit(EXIT_FAILURE); */
/* 	      } */
/*           table[i][j++] = atof(nextString); */
          table[i][j] = table_in[i][j];
          j++;
	  }	  
    }
    return(table);
}

double **loadMatrixOLD(FILE *ifile, int *size, meSet *S)
{
  char nextString[MAX_EVENT_NAME];
  meNode *v;
  double **table;
  int i,j;
  if (!(fscanf(ifile,"%s",nextString)))
    {
      fprintf(stderr,"Error loading input matrix.\n");
      exit(EXIT_FAILURE);
    }
  *size = atoi(nextString);
  if ((*size < 0) || (*size > MAXSIZE))
    {
      printf("Problem inputting size.\n");
      exit(EXIT_FAILURE);
    }
  table = (double **) malloc(*size*sizeof(double *));
  for(i=0;i<*size;i++)
    {
      j = 0;
      table[i] = (double *) malloc(*size*sizeof(double));
      if (!(fscanf(ifile,"%s",nextString)))
	{
	  fprintf(stderr,"Error loading label %d.\n",i);
	  exit(EXIT_FAILURE);
	}
      v = makeNewNode(nextString,-1);
      v->index2 = i;
      S = addToSet(v,S);
      while (j < *size)
	{
	  if (!(fscanf(ifile,"%s",nextString)))
	    {
	      fprintf(stderr,"Error loading (%d,%d)-entry.\n",i,j);
	      exit(EXIT_FAILURE);
	    }
	  table[i][j++] = atof(nextString);
	}
    }
  return(table);
}

void partitionSizes(meTree *T)
{
  meEdge *e;
  e = depthFirstTraverse(T,NULL);
  while (NULL != e)
    {
      if (leaf(e->head))
	e->bottomsize = 1;
      else
	e->bottomsize = e->head->leftEdge->bottomsize 
	  + e->head->rightEdge->bottomsize;
      e->topsize = (T->size + 2)/2 - e->bottomsize;
      e = depthFirstTraverse(T,e);
    }
}


END_SCOPE(fastme)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2004/05/21 21:41:04  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.1  2004/02/10 15:16:03  jcherry
 * Initial version
 *
 * ===========================================================================
 */
