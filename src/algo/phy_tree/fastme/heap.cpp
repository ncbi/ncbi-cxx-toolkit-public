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
* File Description:  heap.cpp
*
*    A part of the Miminum Evolution algorithm
*
*/

#include <ncbi_pch.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "graph.h"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(fastme)

typedef char boolean;

int *initPerm(int size)
{
  int *p;
  int i;
  p = (int *) malloc(size*sizeof(int));
  for(i = 0;i<size;i++)
    p[i] = i;
  return(p);
}

void permInverse(int *p, int *q, int length)
{
  int i;
  for(i=0;i<length;i++)
    q[p[i]] = i;
}

/*swaps two values of a permutation*/
void meSwap(int *p, int *q, int i, int j)
{
  int temp;
  temp = p[i];
  p[i] = p[j];
  p[j] = temp;
  q[p[i]] = i;
  q[p[j]] = j;
}

/*The usual Heapify function, tailored for our use with a heap of scores*/
/*will use array p to keep track of indexes*/
/*after scoreHeapify is called, the submeTree rooted at i 
  will be a heap*/

/*p goes from heap to array, q goes from array to heap*/

void heapify(int *p, int *q, double *HeapArray, int i, int n)
{
  boolean moreswap = TRUE_FASTME;

  do {
    int left = 2 * i;
    int right = 2* i + 1;
    int smallest;
    if ((left <= n) &&	(HeapArray[p[left]] < HeapArray[p[i]]))
      smallest = left;
    else
      smallest = i;
    if ((right <= n) && (HeapArray[p[right]] < HeapArray[p[smallest]]))
      smallest = right;
    if (smallest != i){
      meSwap(p,q,i, smallest);     
      /*push smallest up the heap*/    
      i = smallest;            /*check next level down*/
    }
    else
      moreswap = FALSE_FASTME;
  } while(moreswap);
}

/*heap is of indices of elements of v, 
  popHeap takes the index at position i and pushes it out of the heap
  (by pushing it to the bottom of the heap, where it is not noticed)*/

void reHeapElement(int *p, int *q, double *v, int length, int i)
{
  int up, here;
  here = i;
  up = i / 2;
  if ((up > 0) && (v[p[here]] < v[p[up]]))
    while ((up > 0) && (v[p[here]] < v[p[up]])) /*we push the new
						  value up the heap*/
      {
	meSwap(p,q,up,here);
	here = up;
	up = here / 2;
      }
  else
    heapify(p,q,v,i,length);
}

void popHeap(int *p, int *q, double *v, int length, int i)
{
  meSwap(p,q,i,length); /*puts new value at the last position in the heap*/
  reHeapElement(p,q, v,length-1,i); /*put the swapped guy in the right place*/
}

void pushHeap(int *p, int *q, double *v, int length, int i)
{
  meSwap(p,q,i,length+1); /*puts new value at the last position in the heap*/
  reHeapElement(p,q, v,length+1,length+1); /*put that guy in the right place*/
}



int makeThreshHeap(int *p, int *q, double *v, int arraySize, double thresh)
{
  int i, heapsize;
  heapsize = 0;
  for(i = 1; i < arraySize;i++)
    if(v[q[i]] < thresh)
      pushHeap(p,q,v,heapsize++,i);
  return(heapsize);
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
