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
* File Description:  newickstring.cpp
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
#include "newick.h"

/*
Hi Chris,

I've written a function which will write the meTree as a string.  It's in
the newickstring.c file, called NewickPrintTreeString.

I needed to add one constant to the newick.h file to do this, MAX_DIGITS.
I'm assuming 20 digits would suffice - if you need more you'll need to change 
this value, or the program may not allot a sufficiently long string.  

Rick
*/


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(fastme)

boolean leaf(meNode *v);

boolean whiteSpace(char c)
{
  if ((' ' == c) || ('\t' == c) || ('\n' == c))
    return(TRUE_FASTME);
  else return(FALSE_FASTME);
}

/*decodeNewickSubmeTree is used to turn a string of the form
  "(v1:d1,v2:d2,(subtree) v3:d3....vk:dk) subroot:d," into a subtree
  rooted at subrooted, with corresponding subtrees and leaves at v1
  through vk.  It is called recursively on subtrees*/

meNode *decodeNewickSubtree(char *treeString, meTree *T, int *uCount)
{
  meNode *thisNode = NULL;
  meNode *centerNode;
  double thisWeight;
  meEdge *thisEdge;
  char label[MAX_LABEL_LENGTH];
  char stringWeight[MAX_LABEL_LENGTH];
  int state;
  int i = 0;
  int j;
  int left,right;
  int parcount;
  sprintf(label,"Default Label");
  left = right = 0;
  parcount = 0;
  state = ReadOpenParenthesis;
  if('(' == treeString[0])
    parcount++;
  centerNode = makeNode(label,NULL,nodeCount++);
  T->size++;
  while(parcount > 0)
    {
      while(whiteSpace(treeString[i]))
	i++;
      switch(state) 
	{
	case(ReadOpenParenthesis):
	  if('(' != treeString[0])
	    {
	      fprintf(stderr,"Error reading subtree.\n");
	      exit(EXIT_FAILURE);
	    }
	  i++;
	  state = ReadSubTree;
	  break;
	case(ReadSubTree):
	  if('(' == treeString[i])  /*if treeString[i] is a left parenthesis,
				      we scan down the string until we find its partner.
				      the relative number of '('s vs. ')'s is counted
				      by the variable parcount*/
	    {
	      left = i++;
	      parcount++;
	      while(parcount > 1)
		{
		  while (('(' != treeString[i]) && (')' != treeString[i]))
		    i++;  /*skip over characters which are not parentheses*/
		  if('(' == treeString[i])
		    parcount++;
		  else if (')' == treeString[i])
		    parcount--;
		  i++;
		}  /*end while */
	      right = i;  /*at this point, the submeTree string goes from 
			    treeString[left] to treeString[right - 1]*/
	      thisNode = decodeNewickSubtree(treeString + left,T,uCount);  /*note that this
								      step will put 
								      thisNode in T*/
	      i = right;  /*having created the meNode for the subtree, we move
			    to assigning the label for the new node.
			    treeString[right] will be the start of this label */
	    } /* end if ('(' == treeString[i]) condition */
	  else
	    {	  
	      thisNode = makeNode(label,NULL,nodeCount++);
	      T->size++;
	    }
	  state = ReadLabel;
	  break;
	case(ReadLabel):
	  left = i;  /*recall "left" is the left marker for the substring, "right" the right*/
	  if (':' == treeString[i])   /*this means an internal node?*/
	    {
	      sprintf(thisNode->label,"I%d",(*uCount)++);
	      right = i;
	    }
	  else
	    {
	      while((':' != treeString[i]) && (',' != treeString[i]))
		i++;
	      right = i;
	      j = 0;
	      for(i = left; i < right;i++)
		if(!(whiteSpace(treeString[i])))
		  thisNode->label[j++] = treeString[i];
	      thisNode->label[j] = '\0';
	    }	      
	  if(':' == treeString[right])
	    state = ReadWeight;
	  else
	    state = ReadSubTree;
	  i = right + 1;
	  break;
	case(ReadWeight):
	  left = i;
	  while
	    ((',' != treeString[i]) && (')' != treeString[i]))
	    i++;
	  right = i;
	  if (',' == treeString[right])
	    state = ReadSubTree;
	  else
	    parcount--;
	  j = 0;
	  for(i = left; i < right; i++)
	    stringWeight[j++] = treeString[i];
	  stringWeight[j] = '\0';
	  thisWeight = atof(stringWeight);
	  thisEdge = makeEdge(label,centerNode,thisNode,thisWeight);
	  thisNode->parentEdge = thisEdge;
	  if (NULL == centerNode->leftEdge)
	    centerNode->leftEdge = thisEdge;
	  else if (NULL == centerNode->rightEdge)
	    centerNode->rightEdge = thisEdge;
	  else if (NULL == centerNode->middleEdge)
	    centerNode->middleEdge = thisEdge;
	  else
	    {
	      fprintf(stderr,"Error: meNode %s has too many (>3) children.\n",centerNode->label);
	      exit(EXIT_FAILURE);
	    }
	  sprintf(thisEdge->label,"E%d",edgeCount++);
	  i = right + 1;
	  break;
	}
    }
  return(centerNode);
}

meTree *loadNewickTree(FILE *ifile, int numLeaves)
{
  char label[] = "EmptyEdge";
  meTree *T;
  meNode *centerNode;
  int i = 0;
  int j = 0;
  int inputLength;
  int uCount = 0;
  int parCount = 0;
  char c;
  boolean Comment;
  char *nextString;
  char rootLabel[MAX_LABEL_LENGTH];
  nodeCount = edgeCount = 0;
  T = newTree();
  nextString = (char *) malloc(numLeaves*INPUT_SIZE*sizeof(char));
  if (NULL == nextString)
    nextString = (char *) malloc(MAX_INPUT_SIZE*sizeof(char));
  Comment = FALSE_FASTME;
  while(1 == fscanf(ifile,"%c",&c))
    {
      if('[' == c)
	Comment = TRUE_FASTME;
      else if (']' == c)
	Comment = FALSE_FASTME;
      else if (!(Comment))
	{
	  if(whiteSpace(c)) 
	    {
	      if (i > 0)
		nextString[i++] = ' ';
	    }
	  else  /*note that this else goes with if(whiteSpace(c))*/
	    nextString[i++] = c;
	  if (';' == c)
	    break;
	}
    }
  if ('(' != nextString[0])
    {
      fprintf(stderr,"Error reading input file - does not start with '('.\n");
      exit(EXIT_FAILURE);
    }
  inputLength = i;
  for(i = 0; i < inputLength;i++)
    {
      if ('(' == nextString[i])
	parCount++;
      else if (')' == nextString[i])
	parCount--;
      if (parCount > 0)
	;
      else if (0 == parCount)
	{
	  i++;
	  if(';' == nextString[i])
	    sprintf(rootLabel,"URoot");  
	  else
	    {
	      while(';' != nextString[i]) 
		if(!(whiteSpace(nextString[i++])))
		  rootLabel[j++] = nextString[i-1];  /*be careful here */
	      rootLabel[j] = '\0';
	    }
	  i = inputLength;
	}
      else if (parCount < 0)
	{
	  fprintf(stderr,"Error reading meTree input file.  Too many right parentheses.\n");
	  exit(EXIT_FAILURE);
	}
    }
  centerNode = decodeNewickSubtree(nextString,T,&uCount);
  sprintf(centerNode->label,rootLabel);
  T->root = centerNode;
  free(nextString);
  return(T);
}

void NewickPrintSubtree(meTree *T, meEdge *e, FILE *ofile)
{
  if (NULL == e)
    {
      fprintf(stderr,"Error with Newick Printing routine.\n");
      exit(EXIT_FAILURE);
    }
  if(!(leaf(e->head)))
    {
      fprintf(ofile,"(");
      NewickPrintSubtree(T,e->head->leftEdge,ofile);
      fprintf(ofile,", ");
      NewickPrintSubtree(T,e->head->rightEdge,ofile);
      fprintf(ofile,")");
    }
  fprintf(ofile," ");
  fprintf(ofile,"%s ",e->head->label);
  fprintf(ofile,":%lf",e->distance);
}
	      
int NewickPrintSubtreeString(meTree *T, meEdge *e, char *s)
{
  meEdge *left, *right;
  int slength, llength, rlength;
  slength = 0;
  left = e->head->leftEdge;
  right = e->head->rightEdge;
  if (!(NULL == left))
    {
      s[0] = '(';
      left = e->head->leftEdge;
      llength = NewickPrintSubtreeString(T,left,s+1);
      s[1 + llength] = ',';
      right = e->head->rightEdge;
      rlength = NewickPrintSubtreeString(T,right,s+2+llength);
      s[2 + rlength + llength]=')';
      slength = 3 + rlength + llength;
    }
  sprintf(s+slength,"%s:%lf",e->head->label,e->distance);
  while ('\0' != s[slength])
    slength++;
  return(slength);
}


void NewickPrintTree(meTree *T, FILE *ofile)
{
  meEdge *e, *f;
  meNode *rootchild;
  e = T->root->leftEdge;
  rootchild = e->head;
  fprintf(ofile,"(%s: %lf",T->root->label,e->distance);
  f = rootchild->leftEdge;
  if (NULL != f)
    {
      fprintf(ofile,",");
      NewickPrintSubtree(T,f,ofile);
    }
  f = rootchild->rightEdge;
  if (NULL != f)
    {
      fprintf(ofile,",");
      NewickPrintSubtree(T,f,ofile);
    }
  fprintf(ofile,")");
  if (NULL != rootchild->label)
    fprintf(ofile,"%s",rootchild->label);
  fprintf(ofile,";\n");
}

char *NewickPrintTreeString(meTree *T)
{
  char *s;
  int i,slength, llength, rlength;
  meEdge *e, *left, *right;
  e = T->root->leftEdge;

  slength = T->size * NODE_LABEL_LENGTH + (T->size-1)*MAX_DIGITS
    + 2*T->size + T->size + (T->size-1) + 1;
  /*sum represents characters needed for T->size meNode labels, T->size-1 
    meEdge lengths, at most 2*T->size parentheses, at most T->size commas, 
    T->size - 1 ':' characters for meEdge lengths, and 1 ';' character at end*/
  
  s = (char *) malloc(slength*sizeof(char));
  left = e->head->leftEdge;
  right = e->head->rightEdge;
  i=0;
  s[0] = '(';
  if (!(NULL == left))
    {
      left = e->head->leftEdge;
      llength = NewickPrintSubtreeString(T,left,s+1);
      s[1 + llength] = ',';
      right = e->head->rightEdge;
      rlength = NewickPrintSubtreeString(T,right,s+2+llength);
      s[2 + rlength + llength]=',';
      i = 3 + rlength + llength;
    }
  sprintf(s+i,"%s:%lf)%s;",e->tail->label,e->distance,e->head->label);
  return(s);
}

meEdge *depthFirstTraverse(meTree *T, meEdge *e);


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
