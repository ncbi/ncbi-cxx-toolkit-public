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
* File Description:  traverse.cpp
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

meEdge *findBottomLeft(meEdge *e)
     /*findBottomLeft searches by gottom down in the meTree and to the left.*/
{
  meEdge *f;
  f = e;
  while (NULL != f->head->leftEdge)
    f = f->head->leftEdge;
  return(f);  
}
  
meEdge *moveRight(meEdge *e)
{
  meEdge *f;
  f = e->tail->rightEdge; /*this step moves from a left-oriented edge
			    to a right-oriented edge*/
  if (NULL != f)
    f = findBottomLeft(f);
  return(f);
}


meEdge *depthFirstTraverse(meTree *T, meEdge *e)
     /*depthFirstTraverse returns the meEdge f which is least in T according
       to the depth-first order, but which is later than e in the search
       pattern.  If e is null, f is the least meEdge of T*/
{
  meEdge *f;
  if (NULL == e)
    {
      f = T->root->leftEdge;   
      if (NULL != f)
	f = findBottomLeft(f); 
      return(f);  /*this is the first meEdge of this search pattern*/
    }
  else /*e is non-null*/
    {
      if (e->tail->leftEdge == e) 
	/*if e is a left-oriented edge, we skip the entire
	  meTree cut below e, and find least edge*/
	f = moveRight(e);
      else  /*if e is a right-oriented edge, we have already looked at its
	      sibling and everything below e, so we move up*/
	f = e->tail->parentEdge;
    }
  return(f);
}
        
meEdge *moveUpRight(meEdge *e)
{
  meEdge *f;
  f = e;
  while ((NULL != f) && ( f->tail->leftEdge != f))
    f = f->tail->parentEdge;
  /*go up the meTree until f is a leftEdge*/
  if (NULL == f)
    return(f); /*triggered at end of search*/
  else
    return(f->tail->rightEdge);      
  /*and then go right*/
}
  
boolean leaf(meNode *v);

meEdge *topFirstTraverse(meTree *T, meEdge *e)
     /*topFirstTraverse starts from the top of T, and from there moves stepwise
       down, left before right*/
     /*assumes meTree has been detrifurcated*/
{
  meEdge *f;
  if (NULL == e)
    return(T->root->leftEdge); /*first Edge searched*/
  else if (!(leaf(e->head)))
    return(e->head->leftEdge); /*down and to the left is preferred*/
  else /*e->head is a leaf*/
    {
      f = moveUpRight(e);
      return(f);
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
 * Revision 1.1  2004/02/10 15:16:04  jcherry
 * Initial version
 *
 * ===========================================================================
 */
