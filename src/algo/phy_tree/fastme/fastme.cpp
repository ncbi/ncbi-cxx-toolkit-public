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
* File Description:  fastme.cpp
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
#include "newick.h"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(fastme)

/*functions from newickstring.c*/
meTree *loadNewickTree(FILE *ifile, int numLeaves);
void NewickPrintTree(meTree *T, FILE *ofile);
char *NewickPrintTreeString(meTree *T);
meTree *detrifurcate(meTree *T);
void partitionSizes(meTree *T);

/*functions from bme.c*/
meTree *BMEaddSpecies(meTree *T,meNode *v, double **D, double **A); 
void makeBMEAveragesTable(meTree *T, double **D, double **A);
void assignBMEWeights(meTree *T, double **A);

/*from gme.c*/
meTree *GMEaddSpecies(meTree *T,meNode *v, double **D, double **A);
void makeOLSAveragesTable(meTree *T, double **D, double **A);
void assignOLSWeights(meTree *T, double **A);
void assignAllSizeFields(meTree *T);

/*from inputs.c*/
double **loadMatrixOLD(FILE *ifile, int *size, meSet *S);
double **loadMatrix(double **table_in, char **labels, int *size_in, meSet *S);
void compareSets(meTree *T, meSet *S, FILE *ofile);
void freeMatrix(double **D, int size);

/*from NNI.c*/
void NNI(meTree *T, double **avgDistArray, int *count);

/*from bNNI.c*/
void bNNI(meTree *T, double **avgDistArray, int *count);

/*from graph.c*/
void freeSet(meSet *S);
void freeTree(meTree *T);

void chooseSettings(int argc, char **argv, int *btype, int *ntype, int *wtype, int *numDataSets, char **filenames)
{
  int counter = 1;
  int i;
  sprintf(filenames[0],"input.d");
  sprintf(filenames[1],"output.t");
  sprintf(filenames[2],"input.t");
  while (counter < argc)
    {
      switch(argv[counter][1])
	{	
	case 'i':
	  counter++;
	  if (NULL != argv[counter])
	    strcpy(filenames[0],argv[counter]);
	  else
	    {
	      fprintf(stderr,"Error: -d flag requires argument.\n");
	      exit(EXIT_FAILURE);
	    }
	  counter++;
	  break;
	case 'o':
	  counter++;
	  if (NULL != argv[counter])	    
	    strcpy(filenames[1],argv[counter]);
	  else
	    {
	      fprintf(stderr,"Error: -o flag requires argument.\n");
	      exit(EXIT_FAILURE);
	    }
	  counter++;
	  break;
	case 't':
	  counter++;
	  if (NULL != argv[counter])
	    strcpy(filenames[2],argv[counter]);
	  else
	    {
	      fprintf(stderr,"Error: -i flag requires argument.\n");
	      exit(EXIT_FAILURE);
	    }
	  counter++;
	  *btype = USER;
	  break;
	case 'w':
	  counter++;
	  switch(argv[counter][0])
	    {
	    case 'b':
	    case 'B':
	      *wtype = BAL;
	      break;
	    case 'g':
	    case 'G':
	    case 'o':
	    case 'O':
	      *wtype = OLS;
	      break;
	    default:
	      fprintf(stderr,"Unknown argument to -w option: please");
	      fprintf(stderr," use (b)alanced or (O)LS\n");
	      exit(EXIT_FAILURE);
	    }
	  counter++;
	  break;
	case 'b':
	  counter++;
	  switch(argv[counter][0])
	    {
	    case 'b':
	    case 'B':
	      *btype = BAL;
	      break;
	    case 'g':
	    case 'G':
	    case 'o':
	    case 'O':
	      *btype = OLS;
	      break;
	    default:
	      fprintf(stderr,"Unknown argument to -b option: please");
	      fprintf(stderr," use BME or GME\n");
	      exit(EXIT_FAILURE);
	    }
	  counter++;
	  break;
	case 'n':
	  counter++;
	  *numDataSets = i = 0;
	  while (argv[counter][i])
	    *numDataSets = 10* (*numDataSets) + (argv[counter][i++] - '0');
	  counter++;
	  break;
	case 's':
	  counter++;
	  switch(argv[counter][0])
	    {
	    case 'b':
	    case 'B':
	      *ntype = BAL;
	      break;
	    case 'g':
	    case 'G':
	    case 'o':
	    case 'O':
	      *ntype = OLS;
	      break;
	    case 'n':
	    case 'N':
	      *ntype = NONE;
	      break;
	    default:
	      fprintf(stderr,"Unknown argument to -s option: please");
	      fprintf(stderr," use BME, GME, or none\n");
	      exit(EXIT_FAILURE);
	    }
	  counter++;
	  break;
	case 'v':
	  verbose = TRUE_FASTME;
	  counter++;
	  break;
	case 'h':
	default:
	  fprintf(stderr,"Usage: fastme -binostv\n");
	  fprintf(stderr,"-b specify method for building initial tree: ");
	  fprintf(stderr,"BME or GME(default).\n");
	  fprintf(stderr,"-i filename of distance matrix\n");
	  fprintf(stderr,"-n number of trees/matrices input (default = 1)\n");
	  fprintf(stderr,"-o filename for meTree output\n");
	  fprintf(stderr,"-s specify type of meTree swapping (NNIs): ");
	  fprintf(stderr,"(b)alanced, (O)LS, or (n)one. (Default is balanced.)\n");
	  fprintf(stderr,"-t (optional) filename of starting meTree topology\n");

	  fprintf(stderr,"-v for verbose output\n");
	  fprintf(stderr,"-w (b)alanced or (O)LS weights (if not doing NNIs on input topology) \n");
	  fprintf(stderr,"-help to get this message\n");
	  exit(0);
	}
    }
}

double **initDoubleMatrix(int d)
{
  int i,j;
  double **A;
  A = (double **) malloc(d*sizeof(double *));
  for(i=0;i<d;i++)
    {
      A[i] = (double *) malloc(d*sizeof(double));
      for(j=0;j<d;j++)
	A[i][j] = 0.0;
    }
  return(A);
}


/*void fastme_run(int argc, char **argv)*/
meTree* fastme_run(double** D_in, int numSpecies_in, char **labels, int btype_in, int wtype_in, int ntype_in)
{

//  FILE *ifile2, *ofile;

  double **D, **A;

  meSet *species, *slooper;

//  char **filenames;
  char *treeString = NULL;   /* space for this allocated in newickPrintTreeString */
  meTree *T;


  int setCounter = 0;
  int numSets = 1;
  int count = 0;
  int nniCount = 0;
  int ntype = ntype_in, btype = btype_in, wtype = wtype_in;

  numSpecies = numSpecies_in;
  T = NULL;

//  int i;
//  FILE *ifile1;
//  char **filenames;
//  ntype = BAL;
//  wtype = btype = OLS;

//  verbose = FALSE_FASTME;
//  filenames = (char **) malloc (3*sizeof(char *));
//  for(i=0;i<3;i++)
//    filenames[i] = (char *) malloc(MAX_FILE_NAME_LENGTH*sizeof(char));

//  chooseSettings(argc,argv,&btype,&ntype,&wtype,&numSets,filenames);
/*   ifile1 = fopen(filenames[0],"r"); */
/*   if (!ifile1) */
/*     { */
/*       fprintf(stderr,"Error opening input file %s\n",filenames[0]); */
/*       exit(EXIT_FAILURE); */
/*     } */
/*   ofile = fopen(filenames[1],"w"); */
/*   if (!ofile) */
/*     { */
/*       fprintf(stderr,"Error opening output file %s\n",filenames[1]); */
/*       exit(EXIT_FAILURE); */
/*     } */
/*   if (USER == btype) */
/*     ifile2 = fopen(filenames[2],"r"); */

  while ( setCounter < numSets ) {
    species = (meSet *) malloc(sizeof(meSet));
    species->firstNode = NULL;
    species->secondNode = NULL;
    D = loadMatrix(D_in,labels,&numSpecies,species);
    A = initDoubleMatrix(2*numSpecies-2);  
    switch(btype)
    {
    case USER:
/*
 *  Old option for user-specified tree from file.
        if (!ifile2)
            {
                fprintf(stderr,"Error opening input file %s\n",filenames[2]);
                exit(EXIT_FAILURE);
            }
        T = loadNewickTree(ifile2,numSpecies);
        T = detrifurcate(T);      
        compareSets(T,species,ofile);
        partitionSizes(T);
*/
		break;
    case OLS:
        for(slooper = species; NULL != slooper; slooper = slooper->secondNode)
            T = GMEaddSpecies(T,slooper->firstNode,D,A);
        break;
    case BAL:
        for(slooper = species; NULL != slooper; slooper = slooper->secondNode)
            T = BMEaddSpecies(T,slooper->firstNode,D,A);
        break;
    }
    switch(wtype) /*assign weights*/
        {
        case OLS: 
            break;
        case BAL:
            
            break;
        }

    switch(ntype)
      {      
      case OLS:
	if (OLS != btype)
	  assignAllSizeFields(T);
	makeOLSAveragesTable(T,D,A);
	NNI(T,A,&nniCount);
	assignOLSWeights(T,A);
	break;
      case BAL:	
	if (BAL != btype)
	  makeBMEAveragesTable(T,D,A);
	bNNI(T,A,&nniCount);
	assignBMEWeights(T,A);
	break;
      case NONE:
	switch(wtype)
	  {
	  case OLS:
	    if (OLS != btype)
	      assignAllSizeFields(T);
	    makeOLSAveragesTable(T,D,A);
	    assignOLSWeights(T,A);
	    break;
	  case BAL:
	    if (BAL != btype)
	      makeBMEAveragesTable(T,D,A);
	    assignBMEWeights(T,A);
	    break;
	  default:
	    fprintf(stderr,"Error in program: variable 'btype' has illegal ");
	      fprintf(stderr,"value %d.\n",btype);
	      exit(EXIT_FAILURE);
	  }
	break;
      default:
	fprintf(stderr,"Error in program: variable 'ntype' has illegal ");
	fprintf(stderr,"value %d.\n",ntype);
	exit(EXIT_FAILURE);
      }
/*     NewickPrintTree(T,ofile); */
	if (T == NULL) {
		return NULL;
	}
   /* treeString = NewickPrintTreeString(T); */
    freeMatrix(D,numSpecies);
    freeMatrix(A,2*numSpecies - 2);
    freeSet(species);
    //freeTree(T);
    //T = NULL;
    setCounter++;
    if ((verbose) && (ntype))
      printf("Performed %d NNIs on data meSet %d\n",nniCount,setCounter);
    nniCount = 0;
  }
  return T;
}

/* main(int argc, char **argv) */
/* { */
/*     fprintf(stdout, "Hello from fastme!\n"); */
/* } */


END_SCOPE(fastme)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2005/04/18 18:38:31  jcherry
 * Changed strange code to eliminate compiler warning
 *
 * Revision 1.2  2004/05/21 21:41:03  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.1  2004/02/10 15:16:01  jcherry
 * Initial version
 *
 * ===========================================================================
 */
