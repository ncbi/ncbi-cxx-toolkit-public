#ifndef GRAPH_H
#define GRAPH_H

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
* File Description:  graph.h
*
*    A part of the Miminum Evolution algorithm
*
*/

#include <corelib/ncbistl.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(fastme)

#define MAX_LABEL_LENGTH 50
#define NODE_LABEL_LENGTH 50
#define EDGE_LABEL_LENGTH 50

#ifndef true_fastme
#define true_fastme 1
#endif

#ifndef TRUE_FASTME
#define TRUE_FASTME 1
#endif

#ifndef false_fastme
#define false_fastme 0
#endif
#ifndef FALSE_FASTME
#define FALSE_FASTME 0
#endif
#ifndef EXIT_FAILURE
#define EXIT_FAILURE (-1)
#endif

#define ReadOpenParenthesis 0
#define ReadSubTree 1
#define ReadLabel 2
#define ReadWeight 3
#define ReadSize 4
#define ReadEntries 5
#define Done 6

#define MAXSIZE 70000

typedef struct meNode {
  char label[NODE_LABEL_LENGTH];
  struct meEdge *parentEdge;
  struct meEdge *leftEdge;
  struct meEdge *middleEdge;
  struct meEdge *rightEdge;
  int index;
  int index2;
} meNode;

typedef struct meEdge {
  char label[EDGE_LABEL_LENGTH];
  struct meNode *tail; /*for edge (u,v), u is the tail, v is the head*/
  struct meNode *head;
  int bottomsize; /*number of nodes below edge */
  int topsize;    /*number of nodes above edge */
  double distance;
  double totalweight;
} meEdge;

typedef struct meTree {
  char name[MAX_LABEL_LENGTH];
  struct meNode *root;
  int size;
  double weight;
} meTree;

typedef struct meSet 
{
  struct meNode *firstNode;
  struct meSet *secondNode;
} meSet;

meNode *makeNewNode(const char *label, int i);
meNode *makeNode(const char *label, meEdge *parentEdge, int index);
meEdge *makeEdge(const char *label, meNode *tail, meNode *head, double weight);
meSet *addToSet(meNode *v, meSet *X);
meTree *newTree();

END_SCOPE(fastme)
END_NCBI_SCOPE

#endif /*  GRAPH_H  */
