/* ===========================================================================
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
*/

/* File name: blockalign1.c

Author: Alejandro A. Schaffer

Contents: Code to test block-based alignments for proteins */

/*
 * $Id$
 *
 * This file is slightly modified from Alejandro's blockalign.c as of 9/16/02
 */

#include <ncbi.h>
#include <objseq.h>
#include <objsset.h>
#include <sequtil.h>
#include <seqport.h>
#include <tofasta.h>
#include <blast.h>
#include <blastpri.h>
#include <txalign.h>
#include <gapxdrop.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sqnutils.h>
#include <cdd.h>
#include <objcdd.h>
#include <cddutil.h>
#include <posit.h>
#include <cddposutil.h>
#include <profiles.h>
#include <accid1.h>
#include <objmmdb3.h>
#include <ddvcreate.h>
#include "cn3d_blocka.h"

// hack so I can catch memory leaks specific to this module, at the line where allocation occurs
#ifdef _DEBUG
#ifdef MemNew
#undef MemNew
#endif
#define MemNew(sz) memset(malloc(sz), 0, (sz))
#endif

#define PROTEIN_ALPHABET 26
#define EFFECTIVE_ALPHABET 20
#define scaleFactor 10000

typedef struct {
  Int4Ptr sapp;			/* Current script append ptr */
  Int4  last;
} alignscript_t;


#define DEL_(k) \
data.last = (data.last < 0) ? (data.sapp[-1] -= (k)) : (*data.sapp++ = -(k));

/* Append "Insert k" op */
#define INS_(k) \
data.last = (data.last > 0) ? (data.sapp[-1] += (k)) : (*data.sapp++ = (k));

/* Append "Replace" op */
#define REP_ \
{ data.last = *data.sapp++ = 0; }

/* Divide by two to prevent underflows. */
#define MININT INT4_MIN/2
#define REPP_ \
{ *data.sapp++ = MININT; data.last = 0; }


alignPiece **alignPieceLists;
Int4 *numPiecesInList;

alignBlocks ***bestPairs;
alignBlocks *finalPairsToPrint;
Int4 ***bestScores;
Int4 **numInBestPairs;

static Uint1Ptr convertQuery(Uint1Ptr inQuery, Int4 length)
{
  Uint1Ptr returnQuery;  /*modified version to return*/
  Int4 i; /*loop index*/

  returnQuery = (Uint1Ptr) MemNew(length * sizeof(Uint1));
  for(i = 0; i < length; i++)
    returnQuery[i] = ResToInt(inQuery[i]);
  return(returnQuery);
}

/*Partition gapLengths[start..end] around the first element*/
Int4 quickPartition(Int4 *gapLengths, Int4 start, Int4 end)
{
  Int4 partitionElement; /*item to partition around*/
  Int4 temp; /*item for swapping*/
  Int4 i,j; /*indices to move arround the array*/

  partitionElement = gapLengths[start];
  i = start - 1;
  j = end + 1;
  do {
    do {
      j = j -1;
    } while ((j > start) && (gapLengths[j] > partitionElement));
    do {
      i = i+1;
    } while ((i < end) && (gapLengths[i] < partitionElement));
    if (i < j) {
      temp = gapLengths[i];
      gapLengths[i] = gapLengths[j];
      gapLengths[j] = temp;
    }
    else
      return(j);
  } while(1);
}

/*Code from page 187 of Cormen-Leiserson-Rivest to select
  item of rank selectIndex from gapLengths[start,end]*/
Int4 quickSelect(Int4 *gapLengths,Int4 start,Int4 end,Int4 selectIndex)
{
  Int4 middle, midRank;

  if (start == end)
    return(gapLengths[start]);
  middle = quickPartition(gapLengths,start,end);
  midRank = middle -start +1;
  if (selectIndex <= midRank)
    return(quickSelect(gapLengths,start,middle,selectIndex));
  else
    return(quickSelect(gapLengths,middle+1,end,selectIndex-midRank));
}

/*find the maximum gap length in the array gapLength of size numAlignCounter*/
static Int4 findMaxGap(Int4 *gapLengths, Int4 numAlignCounter)
{
  Int4 lengthToReturn = 0;
  Int4 i; /*loop index*/

  for(i = 0; i < numAlignCounter; i++) {
    if (gapLengths[i] > lengthToReturn)
      lengthToReturn = gapLengths[i];
  }
  return(lengthToReturn);
}

/*Find the distribution of gap lengths in the alignments and then
  use it to determine the maximum allowed gap lengths in the
  block alignments*/
void findAllowedGaps(SeqAlign *listOfSeqAligns, Int4 numBlocks,
		     Int4 *allowedGaps,
		     Nlm_FloatHi percentile, Int4 gapAddition)
{
  Int4 numAligns; /*number of alignments in listOfSeqAligns*/
  Int4 alignCounter; /*counter of alignments*/
  SeqAlignPtr thisAlign; /*one alignment in listOfSeqAligns*/
  DenseDiagPtr ddp; /*segments of 1 alignment*/
  Int4 mFirst, mLast; /*first and last positions of alignment segment*/
  Int4 blockCounter; /*counter of blocks*/
  Int4 **gapLengths; /*two-dimensional array of gap lengths seen*/
  Int4 index;  /*index of item to select*/


  numAligns = 0;
  thisAlign = listOfSeqAligns;
  while (NULL != thisAlign) {
    thisAlign = thisAlign->next;
    numAligns++;
  }

  gapLengths = (Int4 **) MemNew((numBlocks-1) * sizeof(Int4*));
  for(blockCounter = 0; blockCounter < (numBlocks -1); blockCounter++) {
    gapLengths[blockCounter] = (Int4 *) MemNew(numAligns * sizeof(Int4));
  }

  alignCounter = 0;
  thisAlign = listOfSeqAligns;
  while (NULL != thisAlign) {
    ddp = thisAlign->segs;
    blockCounter = 0;
    while (ddp) {
      mFirst = ddp->starts[1];
      if (blockCounter > 0) {
	gapLengths[blockCounter -1][alignCounter] = mFirst - mLast - 1;
      }
      mLast =  ddp->starts[1] + ddp->len -1;
      blockCounter++;
      ddp = ddp->next;
    }
    thisAlign = thisAlign->next;
    alignCounter++;
  } 
  if (percentile < 1.0) {
    index = MIN(Nlm_Nint((alignCounter-1) * percentile), alignCounter -1);
    for(blockCounter = 0; blockCounter < (numBlocks -1); blockCounter++) {
      allowedGaps[blockCounter] = quickSelect(gapLengths[blockCounter],0,alignCounter-1,index);
      allowedGaps[blockCounter]+= gapAddition;
    }
  }
  else {
    for(blockCounter = 0; blockCounter < (numBlocks -1); blockCounter++) {
      allowedGaps[blockCounter] = Nlm_Nint(findMaxGap(gapLengths[blockCounter],alignCounter-1) * percentile);
      allowedGaps[blockCounter]+= gapAddition;
    }
  }

  for(blockCounter = 0; blockCounter < (numBlocks -1); blockCounter++) 
    MemFree(gapLengths[blockCounter]);
  MemFree(gapLengths);
    
}

static Int4 readBlockData(CharPtr asnFileName, FILE *diagfp,
                       Uint1Ptr query, Int4 length, 
                       CharPtr matrixName, Int4 **blockStarts, 
                       Int4 **blockEnds, Uint1Ptr *masterSequence, 
                       Int4 *masterLength, Nlm_FloatHi ***thisPosFreqs,
			  BLAST_Score ***thisScoreMat, Int4 **allowedGaps,
                       SeqIdPtr *sequence_id, Nlm_FloatHi percentile, 
			  Int4 gapAddition, Nlm_FloatHi scaleMult)
{
  CddPtr blockAlignPtr; /*blocked alignment structure*/
  SeqAlignPtr  listOfSeqAligns;
  DenseDiagPtr ddp;
  struct struct_Matrix PNTR structPosFreqs; /*frequencies stored for this CDD*/
  struct struct_Matrix PNTR structScoreMat; /*matrix stored for this CDD*/
  ValNodePtr vnp1, vnp2; /*pointers to track through list of score values*/
  Int4         iFirst, iLast;
  Int4         blockCounter;
  Int4 pseudoCounts = 9;
  Int4 a, c, i; /*loop indices*/
  Int4 charOrder[EFFECTIVE_ALPHABET]; /*standard order of letters according to S. Altschul*/
  Boolean *firstPositions, *lastPositions; /*which positions are first, last
                                           in a block*/
  Boolean *inBlocks;

  /*Look at CddAsnRead in objcdd.c*/
  blockAlignPtr = CddReadFromFile(asnFileName, TRUE);
  if (NULL == blockAlignPtr) {
    ErrPostEx(SEV_FATAL, 0, 0, "Failed to retrieve domain data from %s", asnFileName);
    exit(1);
  }
  listOfSeqAligns = blockAlignPtr->seqannot->data;
  ddp = listOfSeqAligns->segs;
  (*sequence_id) = ddp->id;
  blockCounter = 0;
  
  /* get masterSequence which is a Uint1Ptr out of
     blockAlignPtr->trunc_master which is a BioseqPtr; */

  (*masterSequence) = BlastGetSequenceFromBioseq(
                      blockAlignPtr->trunc_master, masterLength);
  for (c= 0; c < (*masterLength); c++)
     (*masterSequence)[c] = ResToInt((Char) (*masterSequence)[c]); 
  structPosFreqs = blockAlignPtr->posfreq;
  structScoreMat = blockAlignPtr->scoremat;

  /*check if masterLength == number of columns*/

  if ((structPosFreqs->nrows != PROTEIN_ALPHABET) || 
      (structPosFreqs->ncolumns != (*masterLength))) {
    CddSevError("posfreq matrix size mismatch in test blocks");  
  }

  firstPositions = (Boolean *) MemNew((*masterLength) * sizeof(Boolean));
  lastPositions = (Boolean *) MemNew((*masterLength) * sizeof(Boolean));
  inBlocks = (Boolean *) MemNew((*masterLength) * sizeof(Boolean));
  for (c = 0; c < (*masterLength); c++) {
    firstPositions[c] = FALSE;
    lastPositions[c] = FALSE;
    inBlocks[c] = FALSE;
  }

  while (ddp) {
    iFirst = ddp->starts[0];
    iLast  = ddp->starts[0] + ddp->len - 1;
    blockCounter++;
    fprintf(diagfp, "Block %d goes from %d to %d\n",blockCounter,iFirst,iLast);
    firstPositions[iFirst] = TRUE;
    lastPositions[iLast] = TRUE;
    for (c = iFirst; c <= iLast; c++)
      inBlocks[c] = TRUE;
    ddp = ddp->next;
  }

  (*blockStarts) = (Int4 *) MemNew(blockCounter * sizeof(Int4));
  (*blockEnds) = (Int4 *) MemNew(blockCounter * sizeof(Int4));
  (*allowedGaps) = (Int4 *) MemNew(blockCounter * sizeof(Int4));
  i = 0;
  for(c = 0; c < (*masterLength); c++)
    if (firstPositions[c]) {
      (*blockStarts)[i] = c;
      i++;
    }
  i = 0;
  for(c = 0; c < (*masterLength); c++)
    if (lastPositions[c]) {
      (*blockEnds)[i] = c;
      i++;
    }
  findAllowedGaps(listOfSeqAligns,blockCounter,*allowedGaps, percentile, gapAddition);

  for(i = 0; i < (blockCounter-1); i++) 
    fprintf(diagfp, "Alllowed gap length after block %d is %d\n",i+1,(*allowedGaps)[i]);

  (*thisScoreMat) = (BLAST_Score **) MemNew(((*masterLength) +1) * sizeof(BLAST_Score *));
  (*thisPosFreqs) = (Nlm_FloatHi **) MemNew(((*masterLength) +1) * sizeof(Nlm_FloatHi *));
  for(c = 0; c < (*masterLength); c++) {
    (*thisScoreMat)[c] = (BLAST_Score *) MemNew(PROTEIN_ALPHABET * sizeof(BLAST_Score));
    (*thisPosFreqs)[c] = (Nlm_FloatHi *) MemNew(PROTEIN_ALPHABET * sizeof(Nlm_FloatHi));
  }

  vnp1 = structPosFreqs->columns;
  vnp2 = structScoreMat->columns;
  for(c = 0; c < (*masterLength); c++) {
    for(a = 0; a < PROTEIN_ALPHABET; a++) {
      (*thisPosFreqs)[c][a] = (Nlm_FloatHi) vnp1->data.intvalue
	/ ((Nlm_FloatHi) structPosFreqs->scale_factor);
      (*thisScoreMat)[c][a] = Nlm_Nint(vnp2->data.intvalue *scaleMult);
      vnp1 = vnp1->next;
      vnp2 = vnp2->next;
    }
  }

     charOrder[0] =  1;  /*A*/
   charOrder[1] =  16; /*R*/
   charOrder[2] =  13; /*N*/  
   charOrder[3] =  4;  /*D*/ 
   charOrder[4] =  3;  /*C*/
   charOrder[5] =  15; /*Q*/
   charOrder[6] =  5; /*E*/ 
   charOrder[7] =  7;  /*G*/
   charOrder[8] =  8;  /*H*/
   charOrder[9] =  9;  /*I*/
   charOrder[10] = 11; /*L*/
   charOrder[11] = 10; /*K*/
   charOrder[12] = 12; /*M*/  
   charOrder[13] =  6; /*F*/
   charOrder[14] = 14; /*P*/
   charOrder[15] = 17; /*S*/
   charOrder[16] = 18; /*T*/
   charOrder[17] = 20; /*W*/
   charOrder[18] = 22; /*Y*/
   charOrder[19] = 19; /*V*/

   return(blockCounter);
}

void  allocateAlignPieceMemory(Int4 numBlocks)
{
  Int4 blockIndex;

   alignPieceLists = (alignPiece **) MemNew(numBlocks * sizeof(alignPiece *));
   numPiecesInList = (Int4 *) MemNew(numBlocks * sizeof(Int4));
   for(blockIndex = 0; blockIndex < numBlocks; blockIndex++) {
     alignPieceLists[blockIndex] = NULL;
     numPiecesInList[blockIndex] = 0;
   }
}

void allocateBestPairs(Int4 numBlocks)
{
  Int4 blockIndex1, blockIndex2;

  bestPairs = (alignBlocks***) MemNew(numBlocks * sizeof(alignBlocks**));
  numInBestPairs = (Int4**) MemNew(numBlocks * sizeof(Int4));
  for(blockIndex1 = 0; blockIndex1 < numBlocks; blockIndex1++) {
    bestPairs[blockIndex1] = (alignBlocks**) MemNew(numBlocks * sizeof(alignBlocks *));
    numInBestPairs[blockIndex1] = (Int4 *) MemNew(numBlocks * sizeof(Int4));
    for(blockIndex2 = 0; blockIndex2 < numBlocks; blockIndex2++) {
      bestPairs[blockIndex1][blockIndex2] = NULL;
      numInBestPairs[blockIndex1][blockIndex2] = 0;
    }
  }
}

void freeAlignPieceLists(Int4 numBlocks)
{
  Int4 blockIndex; /*loop index over blocks*/
  alignPiece *thisPiece, *nextPiece; /*used to step through a list*/

  for(blockIndex = 0; blockIndex < numBlocks; blockIndex++) {
    thisPiece = alignPieceLists[blockIndex];
    while (NULL != thisPiece) {
      nextPiece = thisPiece->next;
      MemFree(thisPiece);
      thisPiece = nextPiece;
    }
  }
  MemFree(numPiecesInList);
  MemFree(alignPieceLists);
}

alignBlocks *alignBlocksFree(alignBlocks *itemToFree)
{
  MemFree(itemToFree->queryStarts);
  MemFree(itemToFree->queryEnds);
  MemFree(itemToFree);
  return(NULL);
}

alignBlocks * alignBlocksListFree(alignBlocks *listToFree)
{
  alignBlocks *thisItem, *nextItem;

  thisItem = listToFree;
  while(NULL != thisItem) {
    nextItem = thisItem->next;
    alignBlocksFree(thisItem);
    thisItem = nextItem;
  }
  return(NULL);
}

void freeBestPairs(Int4 numBlocks)
{
   Int4 blockIndex1, blockIndex2;
/*   alignBlocks *thisAlign, *nextAlign;*/

   for(blockIndex1 = 0; blockIndex1 < numBlocks; blockIndex1++) {
     for(blockIndex2 = 0; blockIndex2 < numBlocks; blockIndex2++) {
       alignBlocksListFree(bestPairs[blockIndex1][blockIndex2]);
     }
     MemFree(bestPairs[blockIndex1]);
     MemFree(numInBestPairs[blockIndex1]);
   }
   MemFree(bestPairs);
   MemFree(numInBestPairs);
}

void insertPiece(alignPiece *newPiece, Int4 blockIndex)
{
  alignPiece *tempPiece;

  tempPiece = alignPieceLists[blockIndex];
  newPiece->next = tempPiece;
  alignPieceLists[blockIndex] = newPiece;
  numPiecesInList[blockIndex]++;
}

/*find all high scoring single-block gapless alignments*/
void findAlignPieces(Uint1Ptr convertedQuery, Int4 queryLength, 
		     Int4 startQueryPosition, Int4 endQueryPosition,
		     Int4 numBlocks,
                     Int4 *blockStarts, Int4 *blockEnds, Int4 masterLength,
		     BLAST_Score **posMatrix, BLAST_Score scoreThreshold,
		     Int4 *frozenBlocks, Boolean localAlign)
{

   Int4 blockIndex;  /*loop index*/
   Int4 recordIndex = 0;
   Int4 queryPos, dbPos; /*positions in database and query sequences*/
   Int4 actualStartQuery, actualEndQuery; /*actual starting and
					    ending position+1 we can use*/
   Int4 incrementedQueryPos; /*index for moving down quey diagonal*/
   Int4 score; /*total score*/
   Int4 *requiredStartMAX, *requiredStartMIN; /*where do block alignments have to 
                                        start w. r. t. query to fit 
					in*/
   Int4 currentLength; /*current length of blocks*/
   alignPiece *newPiece;
   Int4 candidateQueryEnd; /*candidate for end of match in query*/

   if (!localAlign) {
     requiredStartMAX = (Int4 *) MemNew(numBlocks * sizeof(Int4));
     requiredStartMIN = (Int4 *) MemNew(numBlocks * sizeof(Int4));
     for(blockIndex = 0; blockIndex < numBlocks; blockIndex++) {
       requiredStartMIN[blockIndex] = 0;
       requiredStartMAX[blockIndex] = queryLength -1;
     }
     currentLength = 0;
     for(blockIndex = numBlocks -1; blockIndex >= 0; blockIndex--){
       currentLength += blockEnds[blockIndex] - blockStarts[blockIndex] + 1;
       requiredStartMAX[blockIndex] = queryLength - currentLength;
     }
     currentLength = 0;     
     for(blockIndex = 0; blockIndex < numBlocks; blockIndex++) {
       requiredStartMIN[blockIndex] = currentLength;
       currentLength += blockEnds[blockIndex] - blockStarts[blockIndex] + 1;
     }
   }
   for(blockIndex = 0; blockIndex < numBlocks; blockIndex++) {
     if (-1 == frozenBlocks[blockIndex]) {
       if (localAlign) {
	 actualStartQuery = startQueryPosition;
	 actualEndQuery = endQueryPosition;
       }
       else {
	 actualStartQuery = MAX(startQueryPosition,requiredStartMIN[blockIndex]);
	 actualEndQuery = MIN(endQueryPosition,requiredStartMAX[blockIndex]);
       }
     }
     else {
       actualStartQuery = frozenBlocks[blockIndex];
       actualEndQuery = actualStartQuery+1;
     }
       for(queryPos = actualStartQuery; queryPos < actualEndQuery; queryPos++) {
	 score = 0;
	 for (dbPos = blockStarts[blockIndex], incrementedQueryPos = queryPos; 
	      ((dbPos <= blockEnds[blockIndex]) && (incrementedQueryPos < queryLength));
	      dbPos++, incrementedQueryPos++) {
	   score += posMatrix[dbPos][convertedQuery[incrementedQueryPos]];
	 }
	 candidateQueryEnd = queryPos + blockEnds[blockIndex] -   blockStarts[blockIndex];
       if (((score >= scoreThreshold) || (-1 != frozenBlocks[blockIndex])) 
	   && (candidateQueryEnd < endQueryPosition)) {
	 if ((score < 0) && (-1 != frozenBlocks[blockIndex]))
	   ErrPostEx(SEV_WARNING, 0, 0, "in frozen block %d score is %d",blockIndex+1,score);
	 newPiece = (alignPiece *) MemNew(1 * sizeof(alignPiece));
	 newPiece->queryStart = queryPos;
	 newPiece->queryEnd = candidateQueryEnd;
	 newPiece->blockNumber = blockIndex;
	 newPiece->score = score;
	 newPiece->index = recordIndex;
	 newPiece->next = NULL;
	 insertPiece(newPiece,blockIndex);
	 recordIndex++;
       }
     }
   }
   if (!localAlign) {
     MemFree(requiredStartMAX);
     MemFree(requiredStartMIN);
   }
}

/*Bubble sort the entries in qs from index i through j*/
static void alignPiece_bbsort(alignPiece **qs, Int4 i, Int4 j)
{
    Int4 x, y; /*loop bounds for the two ends of the array to be sorted*/
    alignPiece *sp; /*temporary pointer for swapping*/

    for (x = j; x > i; x--) {
      for (y = i; y < x; y++) {
	if ((qs[y]->score > qs[y+1]->score) ||
	    ((qs[y]->score == qs[y+1]->score) &&
             (qs[y]->index > qs[y+1]->index))){
	  /*swap pointers for inverted adjacent elements*/
	  sp = qs[y];
	  qs[y] = qs[y+1]; 
	  qs[y+1] = sp;
	}
      }
    }
}

/*Bubble sort the entries in qs from index i through j*/
static void alignBlocks_bbsort(alignBlocks **qs, Int4 i, Int4 j)
{
    Int4 x, y; /*loop bounds for the two ends of the array to be sorted*/
    alignBlocks *sp; /*temporary pointer for swapping*/

    for (x = j; x > i; x--) {
      for (y = i; y < x; y++) {
	if ((qs[y]->score > qs[y+1]->score) ||
            ((qs[y]->score == qs[y+1]->score) &&
	     (qs[y]->numBlocksMatched > qs[y+1]->numBlocksMatched))) {
	  /*swap pointers for inverted adjacent elements*/
	  sp = qs[y];
	  qs[y] = qs[y+1]; 
	  qs[y+1] = sp;
	}
      }
    }
}

/*choose the median of  elemnts indexed i, i+j/2, j for
partitioning in quicksort, if median is not in
position i to start with, swap it to position i*/
static void  alignPiecemedianOfThree(alignPiece **qs, Int4 i, Int4 j)
{
  Int4 middle;
  Int4 swapIndex;
  alignPiece *swapTemp;

  middle = (i+j)/2;
  if (qs[i]->score < qs[middle]->score) {
    if (qs[middle]->score < qs[j]->score)
      swapIndex = middle;
    else
      if (qs[i]->score < qs[j]->score)
        swapIndex = j;
      else
        swapIndex = i;
  }
  else {
    if (qs[j]->score < qs[middle]->score)
      swapIndex = middle;
    else
      if (qs[j]->score < qs[i]->score)
        swapIndex = j;
      else
        swapIndex = i;
  }
  if (i != swapIndex) {
    swapTemp = qs[i];
    qs[i] = qs[swapIndex];
    qs[swapIndex] = swapTemp;
  }
}

/*choose the median of  elemnts indexed i, i+j/2, j for
partitioning in quicksort, if median is not in
position i to start with, swap it to position i*/
static void  alignBlocksmedianOfThree(alignBlocks **qs, Int4 i, Int4 j)
{
  Int4 middle;
  Int4 swapIndex;
  alignBlocks *swapTemp;

  middle = (i+j)/2;
  if ((qs[i]->score < qs[middle]->score) ||
      ((qs[i]->score == qs[middle]->score) &&
       (qs[i]->numBlocksMatched < qs[middle]->numBlocksMatched))) {
    if ((qs[middle]->score < qs[j]->score) ||
	((qs[middle]->score == qs[j] ->score) &&
	 (qs[middle]->numBlocksMatched < qs[j]->numBlocksMatched)))
      swapIndex = middle;
    else
      if ((qs[i]->score < qs[j]->score) ||
	  ((qs[i]->score == qs[j]->score) &&
	   (qs[i]->numBlocksMatched < qs[j]->numBlocksMatched)))
        swapIndex = j;
      else
        swapIndex = i;
  }
  else {
    if ((qs[j]->score < qs[middle]->score) ||
	((qs[j]->score == qs[middle]->score) &&
	 (qs[j]->numBlocksMatched < qs[middle]->numBlocksMatched)))
      swapIndex = middle;
    else
      if ((qs[j]->score < qs[i]->score) ||
	  (qs[j]->score == qs[i]->score) &&
	  (qs[j]->numBlocksMatched < qs[i]->numBlocksMatched))
        swapIndex = j;
      else
        swapIndex = i;
  }
  if (i != swapIndex) {
    swapTemp = qs[i];
    qs[i] = qs[swapIndex];
    qs[swapIndex] = swapTemp;
  }
}

/*quicksort the entries in qs from qs[i] through qs[j] */
static void alignPiece_quicksort(alignPiece **qs, Int4 i, Int4 j)
{
    Int4 lf, rt;  /*left and right fingers into the array*/
    Nlm_FloatHi partitionScore; /*score to partition around*/
    Int4  secondaryPartitionValue; /*for breaking ties*/
    alignPiece * tp; /*temporary pointer for swapping*/
    if (j-i <= SORT_THRESHOLD) {
      alignPiece_bbsort(qs, i,j);
      return;
    }

    lf = i+1; 
    rt = j; 
    /*use median of three since array may be nearly sorted*/
    alignPiecemedianOfThree(qs, i, j);
    /*implicitly choose qs[i] as the partition element*/    
    partitionScore = qs[i]->score;
    secondaryPartitionValue = qs[i]->index;
    /*partititon around partitionScore = qs[i]->score*/
    while (lf <= rt) {
      while ((qs[lf]->score <  partitionScore)  ||
              ((qs[lf]->score == partitionScore) && (qs[lf]->index > secondaryPartitionValue)))
	lf++;
      while ((qs[rt]->score >  partitionScore)  ||
              ((qs[rt]->score == partitionScore) && (qs[rt]->index < secondaryPartitionValue)))
	rt--;


      if (lf < rt) {
	/*swap elements on wrong side of partition*/
	tp = qs[lf];
	qs[lf] = qs[rt];
	qs[rt] = tp;
	rt--;
	lf++;
      } 
      else 
	break;
    }
    /*swap partition element into middle position*/
    tp = qs[i];
    qs[i] = qs[rt]; 
    qs[rt] = tp;
    /*call recursively on two parts*/
    alignPiece_quicksort(qs, i,rt-1); 
    alignPiece_quicksort(qs, rt+1, j);
}

/*quicksort the entries in qs from qs[i] through qs[j] */
static void alignBlocks_quicksort(alignBlocks **qs, Int4 i, Int4 j)
{
    Int4 lf, rt;  /*left and right fingers into the array*/
    Nlm_FloatHi partitionScore; /*score to partition around*/
    Int4 partitionBlocksMatched; /*secondary key for breaking ties*/
    alignBlocks * tp; /*temporary pointer for swapping*/
    if (j-i <= SORT_THRESHOLD) {
      alignBlocks_bbsort(qs, i,j);
      return;
    }

    lf = i+1; 
    rt = j; 
    /*use median of three since array may be nearly sorted*/
    alignBlocksmedianOfThree(qs, i, j);
    /*implicitly choose qs[i] as the partition element*/    
    partitionScore = qs[i]->score;
    partitionBlocksMatched = qs[i]->numBlocksMatched;
    /*partititon around partitionScore = qs[i]->score*/
    while (lf <= rt) {
      while ((qs[lf]->score <  partitionScore) ||
	     ((qs[lf]->score == partitionScore) &&
	      (qs[lf]->numBlocksMatched <= partitionBlocksMatched)))
	lf++;
      while ((qs[rt]->score >  partitionScore) ||
	     ((qs[rt]->score == partitionScore) &&
	      qs[rt]->numBlocksMatched > partitionBlocksMatched))
	rt--;
      if (lf < rt) {
	/*swap elements on wrong side of partition*/
	tp = qs[lf];
	qs[lf] = qs[rt];
	qs[rt] = tp;
	rt--;
	lf++;
      } 
      else 
	break;
    }
    /*swap partition element into middle position*/
    tp = qs[i];
    qs[i] = qs[rt]; 
    qs[rt] = tp;
    /*call recursively on two parts*/
    alignBlocks_quicksort(qs, i,rt-1); 
    alignBlocks_quicksort(qs, rt+1, j);
}



/*Sort alignPieces for each block by decreasing score;
  adapted from quicksort_hits of pseed3.c*/
void LIBCALL sortAlignPieces(Int4 numBlocks)
{
  Int4 i; /*loop index for the results list*/
  alignPiece *sp; /*pointer to one entry in the array*/
  alignPiece sentinel; /*sentinel to add at the end of the array*/
  alignPiece **qs; /*local array for sorting*/
  Int4 blockIndex; /* loop index */

  for(blockIndex = 0; blockIndex < numBlocks; blockIndex++) {
    /*Copy the list  into the array qs*/
    if (NULL != alignPieceLists[blockIndex]) {
      qs = (alignPiece **) MemNew(sizeof(alignPiece *)*(numPiecesInList[blockIndex]+1));
      for (i = 0, sp = (alignPieceLists[blockIndex]); 
	   i < numPiecesInList[blockIndex]; i++, sp = sp->next) 
	qs[i] = sp;
      /*Put sentinel at the end of the array*/
      qs[i] = &sentinel; 
      sentinel.score = -(BLAST_SCORE_MIN);
      alignPiece_quicksort(qs, 0, numPiecesInList[blockIndex]-1);
      /*Copy back to the list */
      for (i = numPiecesInList[blockIndex]-1; i > 0; i--)
	qs[i]->next = qs[i-1];
      qs[0]->next = NULL;
      alignPieceLists[blockIndex] = qs[numPiecesInList[blockIndex]-1];
      MemFree(qs);
    }
  }
}

/*Sort alignBlocksList  by decreasing score;
  adapted from quicksort_hits of pseed3.c*/
alignBlocks * sortAlignBlocks(alignBlocks *alignBlocksList, Int4 num)
{
  Int4 i; /*loop index for the results list*/
  alignBlocks *sp; /*pointer to one entry in the array*/
  alignBlocks sentinel; /*sentinel to add at the end of the array*/
  alignBlocks **qs; /*local array for sorting*/
  alignBlocks *returnValue;

  /*Copy the list  into the array qs*/
  qs = (alignBlocks **) MemNew(sizeof(alignBlocks *)*(num+1));
  for (i = 0, sp = alignBlocksList; i < num; i++, sp = sp->next) 
      qs[i] = sp;
  /*Put sentinel at the end of the array*/
  qs[i] = &sentinel; 
  sentinel.score = -(BLAST_SCORE_MIN);
  alignBlocks_quicksort(qs, 0, num-1);
  /*Copy back to the list */
  for (i = num; i > 0; i--)
    qs[i]->next = qs[i-1];
  qs[0]->next = NULL;
  returnValue = qs[num-1];
  MemFree(qs);
  return(returnValue);
}

/*allocate memory for a new item of type align blocks and initialize its
  fields*/
alignBlocks * alignBlocksNew(numBlocks)
{
  alignBlocks *returnValue;
  int i; /*loop index*/

  returnValue = (alignBlocks *) MemNew(1 * sizeof(alignBlocks));
  returnValue->queryStarts = (Int4 *) MemNew(numBlocks * sizeof(Int4));
  returnValue->queryEnds = (Int4 *) MemNew(numBlocks * sizeof(Int4));
  for (i = 0; i < numBlocks; i++) {
    returnValue->queryStarts[i] = -1;
    returnValue->queryEnds[i] = -1;
  }
  returnValue->extendedBackScore = 0;
  returnValue->extendedForwardScore = 0;
  returnValue->score = 0;
  returnValue->numBlocksMatched = 0;
  returnValue->next = NULL;
  return(returnValue);

}


void freeBestScores(Int4 numBlocks)
{
  Int4 blockIndex, blockIndex2; /*loop indices over blocks*/
  for(blockIndex = 0; blockIndex < numBlocks; blockIndex++) {
    for(blockIndex2 = blockIndex; blockIndex2 < numBlocks; blockIndex2++) {
      if (NULL != bestScores[blockIndex][blockIndex2])
	MemFree(bestScores[blockIndex][blockIndex2]);
    }
    if (NULL != bestScores[blockIndex])
      MemFree(bestScores[blockIndex]);
  }
  if (NULL != bestScores)
    MemFree(bestScores);
}

void initializeBestPairs(Int4 numBlocks, Int4 queryLength, Int4 initialBestScore)
{
  Int4 blockIndex, blockIndex2; /*loop indices over blocks*/
  alignPiece *thisAlignPiece; /*used to walked down a list*/
  alignBlocks *thisAlignBlock, *nextAlignBlock; /*used to keep two pointers
						  into a list*/
  Int4 c;

  allocateBestPairs(numBlocks);
  bestScores = (Int4 ***) MemNew(numBlocks * sizeof(Int4**));
  for(blockIndex = 0; blockIndex < numBlocks; blockIndex++) {
    bestScores[blockIndex] = (Int4**) MemNew(numBlocks * sizeof(Int4*));
    for(blockIndex2 = blockIndex; blockIndex2 < numBlocks; blockIndex2++) {
      bestScores[blockIndex][blockIndex2] = (Int4 *) MemNew(queryLength * sizeof(Int4));
      for(c = 0; c < queryLength; c++)
	bestScores[blockIndex][blockIndex2][c] = initialBestScore;
    }
  }
  for(blockIndex = 0; blockIndex < numBlocks; blockIndex++) {
    thisAlignBlock = NULL;
    thisAlignPiece = alignPieceLists[blockIndex];
    /*convert alignPieces into alignBlocks with 1 block, and keep in
      decreasing sorted order of score*/
    while (NULL != thisAlignPiece) {
      nextAlignBlock = alignBlocksNew(numBlocks);
      nextAlignBlock->score = thisAlignPiece->score;
      nextAlignBlock->numBlocksMatched = 1;
      nextAlignBlock->queryStarts[blockIndex] = thisAlignPiece->queryStart;
      nextAlignBlock->queryEnds[blockIndex] = thisAlignPiece->queryEnd;
      if (NULL == thisAlignBlock) {
	bestPairs[blockIndex][blockIndex] = nextAlignBlock;
	numInBestPairs[blockIndex][blockIndex]++;
      }
      else {
	thisAlignBlock->next = nextAlignBlock;
	numInBestPairs[blockIndex][blockIndex]++;
      }
      thisAlignBlock = nextAlignBlock;
      thisAlignPiece = thisAlignPiece->next;
    }
  }
}

/*eliminate items from list whose scores are not optimal for the
  last query position list; assumes alignments
  go from blockIndex1 through blockIndex2*/
void pruneBestPairs(Int4 blockIndex1, Int4 blockIndex2, Int4 queryLength,
		    Int4 scoreThreshold, Boolean localAlignment)
{
  alignBlocks *prevAlignBlock,*thisAlignBlock/*, *nextAlignBlock*/;
  Int4 lastPosition; /*what is the last position matched in the query*/

  thisAlignBlock = bestPairs[blockIndex1][blockIndex2];
  prevAlignBlock = NULL;
  while(thisAlignBlock != NULL) {
    lastPosition = thisAlignBlock->queryEnds[blockIndex2];
    if ((thisAlignBlock->score < bestScores[blockIndex1][blockIndex2][lastPosition]) || 
      ((localAlignment) && (thisAlignBlock->score < scoreThreshold))) {
      numInBestPairs[blockIndex1][blockIndex2]--;
      if (NULL == prevAlignBlock) {
	bestPairs[blockIndex1][blockIndex2] = thisAlignBlock->next;
	alignBlocksFree(thisAlignBlock);
	thisAlignBlock = bestPairs[blockIndex1][blockIndex2];
      }
      else {
	prevAlignBlock->next = thisAlignBlock->next;
	alignBlocksFree(thisAlignBlock);
	thisAlignBlock = prevAlignBlock->next;
      }
    }
    else {
      prevAlignBlock = thisAlignBlock;
      thisAlignBlock = thisAlignBlock->next;
    }
  }
  if (0 < numInBestPairs[blockIndex1][blockIndex2])
    bestPairs[blockIndex1][blockIndex2] = sortAlignBlocks(bestPairs[blockIndex1][blockIndex2], numInBestPairs[blockIndex1][blockIndex2]);
}

  /*can firstBlockCandidate and secondBlockcandidate be consolidated
    to make a longer candidate*/
Boolean alignBlocksCompatible (alignBlocks *firstBlockCandidate, 
       alignBlocks *secondBlockCandidate, Int4 blockIndex1, 
       Int4 blockIndex2, Int4 *allowedGaps)
  {
    if ((firstBlockCandidate->score + secondBlockCandidate->score) <
	bestScores[blockIndex1][blockIndex2][secondBlockCandidate->
        queryEnds[blockIndex2]])
	return(FALSE);
    if ((secondBlockCandidate->queryStarts[blockIndex2] -
	  firstBlockCandidate -> queryEnds[blockIndex2-1] - 1)
	> allowedGaps[blockIndex2-1])
      return(FALSE);
    if (secondBlockCandidate->queryStarts[blockIndex2] <=
	  firstBlockCandidate -> queryEnds[blockIndex2-1])
      return(FALSE);
    return(TRUE);
  }

void updateBestPairs(Int4 numBlocks, Int4 queryLength, Int4 *allowedGaps, 
		     Int4 scoreThreshold, Boolean localAlignment)
{
  Int4 blockIndex1, blockIndex2; /*indices over blocks*/
  alignBlocks *firstBlockCandidate, *secondBlockCandidate, *newAlignBlocks;
  alignBlocks *lastInserted;
  Int4 firstLoopUpperBound;
  Int4 c,i; /*loop index*/

  if (localAlignment)
    firstLoopUpperBound = numBlocks-1;
  else
    firstLoopUpperBound = 1;
  for(blockIndex1 = 0; blockIndex1 < firstLoopUpperBound; blockIndex1++) {
    for(blockIndex2 = blockIndex1+1; blockIndex2 < numBlocks; blockIndex2++) {
      lastInserted = NULL;
      for(firstBlockCandidate = bestPairs[blockIndex1][blockIndex2 -1];
	  firstBlockCandidate != NULL; 
	  firstBlockCandidate= firstBlockCandidate->next) {
	for(secondBlockCandidate = bestPairs[blockIndex2][blockIndex2];
	    secondBlockCandidate != NULL;
	    secondBlockCandidate = secondBlockCandidate->next) {
	  /*next test assumes list of firstCandidates is sorted in
          decreasing order of score; if this secondCandidate was already
          extended back, then that must have yielded a better score*/
	  if (0 == secondBlockCandidate->extendedBackScore) {
	    if (alignBlocksCompatible(firstBlockCandidate, 
                secondBlockCandidate,blockIndex1, blockIndex2, allowedGaps)) {
	      newAlignBlocks = alignBlocksNew(numBlocks);
              for(i = blockIndex1; i <= blockIndex2; i++) {
		newAlignBlocks->queryStarts[i] = firstBlockCandidate->queryStarts[i];
		newAlignBlocks->queryEnds[i] = firstBlockCandidate->queryEnds[i];
	      }
	      newAlignBlocks->queryStarts[blockIndex2] = 
		secondBlockCandidate->queryStarts[blockIndex2];
	      newAlignBlocks->queryEnds[blockIndex2] =
		secondBlockCandidate->queryEnds[blockIndex2];
	      newAlignBlocks->score = firstBlockCandidate->score +
		                      secondBlockCandidate->score;
	      newAlignBlocks->numBlocksMatched = firstBlockCandidate->numBlocksMatched +
		                      secondBlockCandidate->numBlocksMatched;
	      if (newAlignBlocks->score > firstBlockCandidate->extendedForwardScore)
		firstBlockCandidate->extendedForwardScore = newAlignBlocks->score;
	      if (newAlignBlocks->score > secondBlockCandidate->extendedBackScore)
		secondBlockCandidate->extendedBackScore = newAlignBlocks->score;
	      if (NULL == lastInserted)
		bestPairs[blockIndex1][blockIndex2] = newAlignBlocks;
	      else {
		lastInserted->next = newAlignBlocks;
	      }
	      lastInserted = newAlignBlocks;
	      numInBestPairs[blockIndex1][blockIndex2]++;
	      c = newAlignBlocks->queryEnds[blockIndex2];
	      while ((c >= 0) && 
		     (newAlignBlocks->score > bestScores[blockIndex1][blockIndex2][c]))
		bestScores[blockIndex1][blockIndex2][c] = newAlignBlocks->score;
	    }		     
	  }
	}
      }
      pruneBestPairs(blockIndex1, blockIndex2, queryLength, scoreThreshold,
                     localAlignment);
    }
  }
}

alignBlocks *copyAlignBlock(alignBlocks *oldAlignBlock, Int4 numBlocks)
{
  alignBlocks *returnAlignBlock; /*value to return*/
  Int4 i;  /*loop index over blocks*/


  returnAlignBlock = alignBlocksNew(numBlocks);
  for(i = 0; i < numBlocks; i++) {
    returnAlignBlock->queryStarts[i] = oldAlignBlock->queryStarts[i];
    returnAlignBlock->queryEnds[i] = oldAlignBlock->queryEnds[i];
    returnAlignBlock->score = oldAlignBlock->score;
    returnAlignBlock->numBlocksMatched = oldAlignBlock->numBlocksMatched;
    returnAlignBlock->extendedForwardScore = oldAlignBlock->extendedForwardScore;
    returnAlignBlock->extendedBackScore = oldAlignBlock->extendedBackScore;
  }
  returnAlignBlock->next = NULL;
  return(returnAlignBlock);
}

SeqAlign *convertAlignBlockToSeqAlign(Uint1Ptr query, Int4 queryLength, 
    SeqIdPtr subject_id, SeqIdPtr query_id, Uint1Ptr seq, 
    alignBlocks *lastAlignBlock, Int4 numBlocks, Int4 *blockStarts,
    Int4 *blockEnds, Nlm_FloatHi Lambda, Nlm_FloatHi K, Int4 searchSpaceSize)
{
   GapXEditBlockPtr nextEditBlock;  /*intermediate structure towards seqlign*/
   SeqAlignPtr nextSeqAlign; /*seqAlign to return*/
   Nlm_FloatHi eValue = 0.0,  logK = 0.0;
   Boolean done; /*used to move across blocks*/
   Int4 blockIndex;
   Int4 overallQueryStart, overallSeqStart;
   Int4 overallQueryEnd, overallSeqEnd;
   Int4 commonSpacing, querySpacing, seqSpacing;
   alignscript_t data; /*holds alignment script*/
   Int4 j; /*loop index*/
   Int4 scriptIndex;

   done = FALSE;
   blockIndex = 0;
   if (0 != K)
     logK = log(K);
   data.sapp = (Int4 *) MemNew((2 * queryLength) * sizeof(Int4));
   while (-1 == lastAlignBlock->queryStarts[blockIndex])
     blockIndex++;
   overallQueryStart = lastAlignBlock->queryStarts[blockIndex];
   overallSeqStart = blockStarts[blockIndex];
   
   done = FALSE;
   scriptIndex = 0;
   do {
     /*add replaces for this block*/
     for(j = 0; j < (lastAlignBlock->queryEnds[blockIndex] - 
		     lastAlignBlock->queryStarts[blockIndex] +1); j++) {
       data.sapp[scriptIndex++] = 0; 
       /*data.last = *data.sapp++ = 0;*/
     }
     blockIndex++;
     if ((numBlocks > blockIndex) && (-1 != lastAlignBlock->queryStarts[blockIndex])) {
       seqSpacing = blockStarts[blockIndex] - blockEnds[blockIndex -1] -1;
       querySpacing = lastAlignBlock->queryStarts[blockIndex] -
	              lastAlignBlock->queryEnds[blockIndex -1] -1;
       commonSpacing = MIN(seqSpacing,querySpacing);
       for(j = 0; j < commonSpacing; j++) {
	 data.sapp[scriptIndex++] = MININT; 
	 data.last = 0;
       }
       if ((seqSpacing - querySpacing) != 0)
	 data.sapp[scriptIndex++] = (seqSpacing - querySpacing);
	 /* data.last = (data.last > 0) ? (data.sapp[-1] += 
	    (seqSpacing - querySpacing)) : (*data.sapp++ = (seqSpacing - querySpacing));*/
     }
     else
       done = TRUE;
   } while (!done);

   overallQueryEnd = lastAlignBlock->queryEnds[blockIndex-1];
   overallSeqEnd = blockEnds[blockIndex -1];
   scriptIndex--;
   nextEditBlock = TracebackToGapXEditBlock(
                     query - 1 + overallQueryStart,
		     seq - 1 + overallSeqStart,
		     overallQueryEnd - overallQueryStart +1, 
                     overallSeqEnd - overallSeqStart +1,
                     &(data.sapp[0]), overallQueryStart, 
		     overallSeqStart);
   nextEditBlock->discontinuous = TRUE;
   nextSeqAlign = GapXEditBlockToSeqAlign(nextEditBlock, subject_id, query_id);
   if ((0.0 == Lambda) || (0.0 == K))
     eValue = 0;
   else
     eValue = K * searchSpaceSize * exp(-(lastAlignBlock->score) * Lambda +logK);
   nextSeqAlign->score = addScoresToSeqAlign(lastAlignBlock->score, 
                     eValue, Lambda, logK);
   nextEditBlock = GapXEditBlockDelete(nextEditBlock);
   if (NULL != data.sapp)
     MemFree(data.sapp);
   return(nextSeqAlign);
}

SeqAlign *getAlignmentsFromBestPairs(Uint1Ptr query, SeqIdPtr subject_id,
				     SeqIdPtr query_id, 
				     Uint1Ptr seq, Int4 seqLength,
    Int4 numBlocks, Int4 queryLength, Int4 *blockStarts, Int4 *blockEnds,
    Int4 *bestFirstBlock, Int4 *bestLastBlock, Nlm_FloatHi Lambda, 
    Nlm_FloatHi K, Int4 searchSpaceSize, Boolean localAlignment)
{
  Int4 blockIndex1, blockIndex2;
  alignBlocks *oldAlignBlock, *newAlignBlock, *lastAlignBlock;
  SeqAlign *listToReturn, *lastSeqAlign, *newSeqAlign;
  Int4 numToPrint = 0;
  Int4 i; /*loop index*/
  Int4 actualSearchSpace;
  Int4 startLoop1, endLoop1, startLoop2, endLoop2; /*loop bounds that
      depend on whether we are doing local or global alignment*/
            


  *bestFirstBlock = -1;
  *bestLastBlock = -1;
  lastAlignBlock = finalPairsToPrint = NULL;
  if (0 == searchSpaceSize) 
    actualSearchSpace = queryLength * seqLength;
  else
    actualSearchSpace = searchSpaceSize;
  if (localAlignment) {
    startLoop1 = 0;
    endLoop1 = numBlocks;
  }
  else {
    startLoop1 = 0;
    endLoop1 = 1;
  }
  for(blockIndex1 = startLoop1; blockIndex1 < endLoop1; blockIndex1++) {  
    if (localAlignment) {
      startLoop2 = blockIndex1;
      endLoop2 = numBlocks;
    }
    else {
      startLoop2 = numBlocks-1;
      endLoop2 = numBlocks;
    }
    for(blockIndex2 = startLoop2; blockIndex2 < endLoop2; blockIndex2++) {
      oldAlignBlock = bestPairs[blockIndex1][blockIndex2];
      while(NULL != oldAlignBlock) {
	if ((0 == oldAlignBlock->extendedForwardScore) &&
	    (0 == oldAlignBlock->extendedBackScore)) {
	  newAlignBlock = copyAlignBlock(oldAlignBlock,numBlocks);
	  numToPrint++;
	  if (NULL == lastAlignBlock) {
	    finalPairsToPrint = newAlignBlock;
	  }
	  else
	    lastAlignBlock->next = newAlignBlock;
	  lastAlignBlock = newAlignBlock;
	}
	oldAlignBlock = oldAlignBlock->next;
      }
    }
  }
  if (NULL != finalPairsToPrint) {
    finalPairsToPrint = sortAlignBlocks(finalPairsToPrint,numToPrint);
  }
  lastAlignBlock = finalPairsToPrint;
  listToReturn = lastSeqAlign = NULL;
  while (NULL != lastAlignBlock) {
    newSeqAlign = convertAlignBlockToSeqAlign(query, queryLength, 
           subject_id, query_id, 
           seq, lastAlignBlock,numBlocks, blockStarts, blockEnds,
           Lambda, K, actualSearchSpace);
    if (NULL == lastSeqAlign) {
      for (i = 0; i < numBlocks; i++)
	if (-1 != lastAlignBlock->queryStarts[i]) {
	  (*bestFirstBlock) = i;
	  break;
	}
      for (i = numBlocks-1; i >= 0; i--)
	if (-1 != lastAlignBlock->queryStarts[i]) {
	  (*bestLastBlock) = i;
	  break;
	}
      listToReturn = lastSeqAlign = newSeqAlign;
    }
    else {
      lastSeqAlign->next = newSeqAlign;
      lastSeqAlign = newSeqAlign;
    }
    lastAlignBlock = lastAlignBlock->next;
  }
  alignBlocksListFree(finalPairsToPrint);
  return(listToReturn);
}

SeqAlign *makeMultiPieceAlignments(Uint1Ptr query, Int4 numBlocks, 
   Int4 queryLength, Uint1Ptr seq, Int4 seqLength, Int4 *blockStarts, Int4 *blockEnds,
   Int4 *allowedGaps, Int4 scoreThreshold, SeqIdPtr subject_id, 
				   SeqIdPtr query_id, Int4* bestFirstBlock,
                                   Int4 *bestLastBlock, Nlm_FloatHi Lambda,
                                   Nlm_FloatHi K, Int4 searchSpaceSize,
				   Boolean localAlignment)
{
   SeqAlign *returnValue;

   if (localAlignment)
     initializeBestPairs(numBlocks, queryLength, 0);
   else
     initializeBestPairs(numBlocks, queryLength, NEG_INFINITY);
   updateBestPairs(numBlocks, queryLength, allowedGaps, scoreThreshold,
		   localAlignment);
   returnValue = getAlignmentsFromBestPairs(query, subject_id, 
         query_id, seq, seqLength, numBlocks, queryLength, 
         blockStarts, blockEnds, bestFirstBlock, bestLastBlock,
         Lambda,K,searchSpaceSize, localAlignment);
   return(returnValue);
}

 
Int4 getSearchSpaceSize(Int4 masterLength, Int4 databaseLength, Int4 initSearchSpaceSize)
{

   if ((0 != databaseLength) && (0 != initSearchSpaceSize))
       ErrPostEx(SEV_WARNING, 0, 0, "Non-zero values given for both database size (-z) and search space size (-Y), the -Y value takes precedence");
   if (0 != initSearchSpaceSize)
     return(initSearchSpaceSize);
   else {
     if (0 != databaseLength)
       return(masterLength * databaseLength);
     else
       return(0);
   }
}

/* Parse inputBlockString, which describes the blocks to be frozen.
   This procedure is called only if there is at least 1 block to be
   frozen. The value of frozenBlocks[N] indicates that block N in the
   CDD sequence should be forcibly aligned to with its first residue 
   matching position frozenBlocks[N] (numbering from zero .. sequence length
   - 1) in the query. A value of -1 indicates that block N is unrestricted.
   When this proceudre is called, the memory for frozenBlocks has already
   been allocated and all entries have been initialized to -1. 
*/              

static void parseFrozenBlocksString(Char *inputBlockString, Int4 *frozenBlocks,
     Int4 numBlocks)
{
     Char *blockStr, *posStr; /*strings to temporarily hold one block number and

                           one position*/  
     Int4 block, pos; /*Integer versions of blockStr and posStr*/

     blockStr = StrTok(inputBlockString, ",");
     posStr = StrTok(NULL, ",");
     while (posStr != NULL) {
       block = atoi(blockStr);
       pos = atoi(posStr);
       if (block <= 0) {
         ErrPostEx(SEV_WARNING, 0, 0, "block numbers must be >=1");
         return;
       }
       if (block > numBlocks) {
         ErrPostEx(SEV_WARNING, 0, 0, "block number %d greater than number of blocks %d",block,numBlocks);
         return;
       }
       /* the '-1' here is to convert one-numbered user strings to zero-numbered */
       frozenBlocks[block - 1] = pos - 1;
       blockStr = StrTok(NULL, ",");
       posStr = StrTok(NULL, ",");
     }
   }

/* checks to make sure frozen block positions are legal */
Boolean ValidateFrozenBlockPositions(Int4 *frozenBlocks,
   Int4 numBlocks, Int4 startQueryRegion, Int4 endQueryRegion,
   Int4 *blockStarts, Int4 *blockEnds, Int4 *allowedGaps)
{
    Int4
        i,
        blockLength,
        unfrozenBlocksLength = 0,
        prevFrozenBlockEnd = startQueryRegion - 1,
        maxGapsLength = 0;

    for (i=0; i<numBlocks; i++) {
        blockLength = blockEnds[i] - blockStarts[i] + 1;

        /* keep track of max gap space between frozen blocks */
        if (i > 0) maxGapsLength += allowedGaps[i - 1];

        /* to allow room for unfrozen blocks between frozen ones */
        if (frozenBlocks[i] < 0) {
            unfrozenBlocksLength += blockLength;
            continue;
        }

        /* check absolute block end positions */
        if (frozenBlocks[i] < startQueryRegion) {
            ErrPostEx(SEV_WARNING, 0, 0, "Frozen block %i can't start < %i", i+1, startQueryRegion+1);
            return FALSE;
        }
        if (frozenBlocks[i] + blockLength > endQueryRegion) {
            ErrPostEx(SEV_WARNING, 0, 0, "Frozen block %i can't end >= %i", i+1, endQueryRegion+1);
            return FALSE;
        }

        /* checks for legal distances between frozen blocks */
        if (prevFrozenBlockEnd >= startQueryRegion) {

            /* check for adequate room for unfrozen blocks between frozen blocks */
            if (frozenBlocks[i] <= prevFrozenBlockEnd + unfrozenBlocksLength) {
                ErrPostEx(SEV_WARNING, 0, 0,
                    "Frozen block %i starts before end of prior frozen block, "
		    "or doesn't leave room for intervening unfrozen block(s)", i+1);
                return FALSE;
            }

            /* check for too much gap space since last frozen block */
            if (frozenBlocks[i] > prevFrozenBlockEnd + 1 + unfrozenBlocksLength + maxGapsLength) {
                ErrPostEx(SEV_WARNING, 0, 0,
                    "Frozen block %i is too far away from prior frozen block"
		    " given allowed gap length(s) (%i)", i+1, maxGapsLength);
                return FALSE;
            }
        }

        /* reset counters after each frozen block */
        prevFrozenBlockEnd = frozenBlocks[i] + blockLength - 1;
        unfrozenBlocksLength = maxGapsLength = 0;
    }

    return TRUE;
}

#define NUMARG (sizeof(myargs)/sizeof(myargs[0]))

static Args myargs [] = {
    {"Input query sequence (this parameter must be set)",  /* 0 */
     "stdin", NULL,NULL,FALSE,'i',ARG_FILE_IN, 0.0,0,NULL},
    {"Alignment file in ASN.1 (this parameter must be set)",            /* 1 */
     NULL, NULL,NULL,FALSE,'a',ARG_FILE_IN, 0.0,0,NULL},
    { "Expectation value (E)",        /* 2 */
      "10.0", NULL, NULL, FALSE, 'e', ARG_FLOAT, 0.0, 0, NULL},
    { "alignment view options:\n0 = pairwise,\n1 = query-anchored showing identities,\n2 = query-anchored no identities,\n3 = flat query-anchored, show identities,\n4 = flat query-anchored, no identities,\n5 = query-anchored no identities and blunt ends,\n6 = flat query-anchored, no identities and blunt ends,\n7 = XML Blast output,\n8 = tabular output, \n9 = tabular output with comments", /* 3 */
      "0", NULL, NULL, FALSE, 'm', ARG_INT, 0.0, 0, NULL},
    { "Output File for Alignment", /* 4 */
      "stdout", NULL, NULL, TRUE, 'o', ARG_FILE_OUT, 0.0, 0, NULL},
    { "Filter query sequence with SEG", /* 5 */
      "F", NULL, NULL, FALSE, 'F', ARG_STRING, 0.0, 0, NULL},
    { "Cost to open a gap outside blocks",     /* 6 */
      "11", NULL, NULL, FALSE, 'G', ARG_INT, 0.0, 0, NULL},
    { "Cost to extend a gap outside blocks",   /* 7 */
      "1", NULL, NULL, FALSE, 'E', ARG_INT, 0.0, 0, NULL},
    { "Cost to open a gap inside blocks",     /* 8 */
      "5", NULL, NULL, FALSE, 'A', ARG_INT, 0.0, 0, NULL},
    { "Cost to extend a gap outside blocks",   /* 9 */
      "1", NULL, NULL, FALSE, 'B', ARG_INT, 0.0, 0, NULL},
    { "Scoring Matrix",                                       /*10*/
      "BLOSUM62",NULL,NULL,FALSE,'M',ARG_STRING, 0.0,0,NULL},
    { "Show GI's in deflines",  /* 11 */
      "F", NULL, NULL, FALSE, 'I', ARG_BOOLEAN, 0.0, 0, NULL},
    { "Believe the query defline", /* 12 */
      "F", NULL, NULL, FALSE, 'J', ARG_BOOLEAN, 0.0, 0, NULL},
    { "SeqAlign file ('Believe the query defline' must be TRUE)", /* 13 */
      NULL, NULL, NULL, TRUE, 'O', ARG_FILE_OUT, 0.0, 0, NULL},
    { "Effective length of the database", /* 14 */
      "0", NULL, NULL, FALSE, 'z', ARG_INT, 0.0, 0, NULL},
    { "Effective length of the search space", /* 15 */
      "0", NULL, NULL, FALSE, 'Y', ARG_INT, 0.0, 0, NULL},
    { "Produce HTML output",  /* 16 */
      "F", NULL, NULL, FALSE, 'T', ARG_BOOLEAN, 0.0, 0, NULL},
    { "Score threshold for 1 block",  /* 17 */
      "11", NULL, NULL, FALSE, 'f', ARG_INT, 0.0, 0, NULL},
    { "Score thresholds for multiple blocks",  /* 18 */
      "11", NULL, NULL, FALSE, 't', ARG_INT, 0.0, 0, NULL},
    { "Number of one-line descriptions to print", /* 19 */
      "250", NULL, NULL, FALSE, 'v', ARG_INT, 0.0, 0, NULL},
    { "Number of alignments to show ",  /* 20 */
      "250", NULL, NULL, FALSE, 'b', ARG_INT, 0.0, 0, NULL},
    { "Percentile as fraction of gap lengths to use for maximum allowed", /*21*/
      ".9", NULL, NULL, FALSE, 'p', ARG_FLOAT, 0.0, 0, NULL},
    { "Additional allowed gap length", /*22*/
      "10", NULL, NULL, FALSE, 's', ARG_INT, 0.0, 0, NULL},
    { "Output File for Diagnostics", /* 23 */
      "stderr", NULL, NULL, FALSE, 'D', ARG_FILE_OUT, 0.0, 0, NULL},
    { "Lambda for scoring system", /*24*/
      "0.0", NULL, NULL, FALSE, 'L', ARG_FLOAT, 0.0, 0, NULL},
    { "K for scoring system", /*25*/
      "0.0", NULL, NULL, FALSE, 'K', ARG_FLOAT, 0.0, 0, NULL},
    { "Multiplicative scaling factor for scores", /*26*/
      "1.0", NULL, NULL, FALSE, 'S', ARG_FLOAT, 0.0, 0, NULL},
    { "Start of desired region in query", /* 27 */
      "1", NULL, NULL, FALSE, 'Q', ARG_INT, 0.0, 0, NULL},
    { "End of required region in query (-1 indicates end of query)", /* 28 */
      "-1", NULL, NULL, FALSE, 'R', ARG_INT, 0.0, 0, NULL},
    { "Local alignment if T (default), global alignment if F",  /* 29 */
      "T", NULL, NULL, FALSE, 'l', ARG_BOOLEAN, 0.0, 0, NULL},
    { "Frozen blocks", /* 30 */
      "none", NULL, NULL, FALSE, 'N', ARG_STRING, 0.0, 0, NULL}
};

Int2  Main(void)
{
   FILE *infp, *outfp; /*file descriptors for  query file and output file*/
   FILE *diagfp; /*File descriptor for diagnostic messages*/
   CharPtr blockalign_outputfile; /*name of file for output*/
   CharPtr diag_outputfile; /*name of file for diagnostics*/

   Boolean show_gi; /*should the gi number be shown in the output?*/ 
   Boolean believe_query=FALSE; /*do we believe the def line in the query file?*/
   Int4 number_of_descriptions, number_of_alignments; /* maximum number 
        		 of header lines and alignments to be displayed*/

   SeqEntryPtr sep; /*sequence entry pointer representation of query*/
   BioseqPtr query_bsp; /*bioseq pointer representation of query*/
   BioseqPtr fake_bsp; /*extra pointer used when query def line is not
                         believed*/
   ObjectIdPtr obidp; /*object id associated with fake bsp*/
   Uint1Ptr query;
   Uint1Ptr convertedQuery;
   Int4 queryLength;
   Int4 numBlocks;
   Int4 *blockStarts;
   Int4 *blockEnds;
   Uint1Ptr masterSequence;
   Int4 masterLength;
   Nlm_FloatHi **thisPosFreqs;
   BLAST_Score **thisScoreMat;
   Int4 *allowedGaps;
   SeqAlignPtr results; /*holds the list of matches as alignments*/
   SeqAnnotPtr  seqannot = NULL; /*annotated version of results*/
   BlastPruneSapStructPtr prune; /*stores possibly-reduced  of sequence
                                   alignments for output*/
/*   Uint1 featureOrder[FEATDEF_ANY]; /*dummy argument for alignment display*/
/*   Uint1 groupOrder[FEATDEF_ANY]; /*dummy argument for alignment display*/
   Uint4 align_options, print_options; /*store as masks options for displaying
					 alignments and headers */
   TxDfDbInfoPtr dbinfo=NULL; /*placeholder for information about the database*/
   AsnIoPtr aip=NULL; /*descriptor for writing sequence alignments
                        in ASN to a separate file*/
   SeqLocPtr   private_slp=NULL; /*used to get a sequence id*/
   SeqIdPtr local_sequence_id; /*represents sequence id used to display alignments*/
   SeqIdPtr query_id; /*id of query for printing*/
   BLAST_MatrixPtr matrix; /*name of underlying 20x20 score matrix*/
   Int4Ptr PNTR txmatrix; /*score matrix for printing alignments*/
   SeqIdPtr  subject_id;
   Int4 bestFirstBlock, bestLastBlock; /*to store extent of highest scoring
                                         alignment*/
   Int4 searchSpaceSize; /*size of search space for getting E-values*/
   Int4 i; /*loop index*/
   Int4 startQueryRegion, endQueryRegion; /*start and end positions of query 
					    desired in alignments*/
   Boolean localAlignment; /*should the alignment be local or global*/
   Int4 *frozenBlocks = NULL; /*locations in query of start of frozen blocks
                                -1, if not frozen*/

   if (! GetArgs ("blockalign", NUMARG, myargs))
     {
        return (1);
     }

   if (! SeqEntryLoad())
     return 1;

   if (! objmmdb3AsnLoad())
     return 1;

   UseLocalAsnloadDataAndErrMsg();

   if ((infp = FileOpen(myargs [0].strvalue, "r")) == NULL) {
        ErrPostEx(SEV_FATAL, 0, 0, "blockalign: Unable to open input file %s\n", 
                  myargs [0].strvalue);
        return 0;
   }

   outfp = NULL;
   blockalign_outputfile = myargs[4].strvalue;
   show_gi = (Boolean) myargs[11].intvalue;
   believe_query = (Boolean) myargs[11].intvalue;
   if (blockalign_outputfile != NULL)
     {
       if ((outfp = FileOpen(blockalign_outputfile, "w")) == NULL)
	 {
	    ErrPostEx(SEV_FATAL, 0, 0, "blockalign: Unable to open output file %s\n", blockalign_outputfile);
	    return (1);
	 }
     }

   diag_outputfile = myargs[23].strvalue;
   if (diag_outputfile != NULL)
     {
       if ((diagfp = FileOpen(diag_outputfile, "w")) == NULL)
	 {
	    ErrPostEx(SEV_FATAL, 0, 0, "blockalign: Unable to open diagnostic file %s\n", diag_outputfile);
	    return (1);
	 }
     }

   
   sep = FastaToSeqEntryEx(infp, FALSE, NULL, FALSE);
   if (sep != NULL) {
     query_bsp = NULL;
     SeqEntryExplore(sep, &query_bsp, FindProt);
     if (query_bsp == NULL) {
       ErrPostEx(SEV_FATAL, 0, 0, "Unable to obtain bioseq\n");
       return 2;
     }
     query = BlastGetSequenceFromBioseq(query_bsp, &queryLength);
     convertedQuery = convertQuery(query,queryLength);
   }
   if ((myargs[21].floatvalue <= 0.0)) {
       ErrPostEx(SEV_FATAL, 0, 0, "Percentile given as -p %.2lf for gap lengths must be strictly greater than 0", myargs[21].floatvalue);
       return(1);
   }

   if (myargs[21].floatvalue >= 1.0) {
       ErrPostEx(SEV_WARNING, 0, 0, "Percentile given as -p %.2lf for gap lengths is >= 1.0, reinterpreted as largest gap times this value", myargs[21].floatvalue);
   }

   numBlocks = readBlockData(myargs[1].strvalue, diagfp, convertedQuery, queryLength, 
	      myargs[10].strvalue, &blockStarts, &blockEnds,
	      &masterSequence, &masterLength, &thisPosFreqs, &thisScoreMat,
	      &allowedGaps, &subject_id, myargs[21].floatvalue, myargs[22].intvalue, 
              myargs[26].floatvalue);

   startQueryRegion = MAX(0,myargs[27].intvalue -1);
   endQueryRegion = queryLength;
   if (1 <= myargs[28].intvalue)
     endQueryRegion = MIN(queryLength, myargs[28].intvalue);

   /* 
    * blocks to be frozen; if any, then the frozenBlocks array
    * will contain numBlocks integers, where the value of frozenBlocks[N]
    * indicates that block[N] should have its first residue frozen at this
    * position (numbering from zero .. sequence length - 1); a value of
    * -1 indicates that block[N] is unrestricted.
    */              
   frozenBlocks = (Int4 *) MemNew(numBlocks * sizeof(Int4));
   for(i = 0; i < numBlocks; i++)
     frozenBlocks[i] = -1;
   if (myargs[30].strvalue && StrCmp(myargs[30].strvalue, "none") != 0) {
     parseFrozenBlocksString(myargs[30].strvalue, frozenBlocks, numBlocks);
     for(i = 0; i < numBlocks; i++)
       if (-1 != frozenBlocks[i])
     fprintf(diagfp, "block #%i frozen at residue %i\n", i+1 , frozenBlocks[i]+1);
     ValidateFrozenBlockPositions(frozenBlocks, numBlocks, startQueryRegion, endQueryRegion,
        blockStarts, blockEnds, allowedGaps);
   }

   init_buff_ex(90);
   fprintf(outfp, "\n");
   IMPALAPrintReference(FALSE, 90, outfp);
   fprintf(outfp, "\n");
   AcknowledgeBlastQuery(query_bsp, 70, outfp, believe_query, FALSE);
   free_buff();

   if (believe_query)
     {
       fake_bsp = query_bsp;
     }
   else
     {
       fake_bsp = BioseqNew();
       fake_bsp->descr = query_bsp->descr;
       fake_bsp->repr = query_bsp->repr;
       fake_bsp->mol = query_bsp->mol;
       fake_bsp->length = query_bsp->length;
       fake_bsp->seq_data_type = query_bsp->seq_data_type;
       fake_bsp->seq_data = query_bsp->seq_data;
       
       obidp = ObjectIdNew();
       obidp->str = StringSave("QUERY");
       ValNodeAddPointer(&(fake_bsp->id), SEQID_LOCAL, obidp);
       
       /* FASTA defline not parsed, ignore the "lcl|tempseq" ID. */
       query_bsp->id = SeqIdSetFree(query_bsp->id);
     }
 
   searchSpaceSize = getSearchSpaceSize(masterLength,myargs[14].intvalue,myargs[15].intvalue);

   private_slp = SeqLocIntNew(0, fake_bsp->length-1 , Seq_strand_plus, SeqIdFindBest(fake_bsp->id, SEQID_GI));
   local_sequence_id = SeqIdFindBest(SeqLocId(private_slp), SEQID_GI);
   query_id = SeqIdDup(local_sequence_id);

   allocateAlignPieceMemory(numBlocks);
   localAlignment = (Boolean) myargs[29].intvalue;
   if (localAlignment)
     findAlignPieces(convertedQuery,queryLength, startQueryRegion, endQueryRegion, numBlocks, blockStarts,blockEnds,masterLength, thisScoreMat, myargs[17].intvalue, frozenBlocks, localAlignment);
   else
     findAlignPieces(convertedQuery,queryLength, startQueryRegion, endQueryRegion, numBlocks, blockStarts,blockEnds,masterLength, thisScoreMat, 
NEG_INFINITY, frozenBlocks, localAlignment);
   sortAlignPieces(numBlocks);
   results = makeMultiPieceAlignments(convertedQuery, numBlocks, 
      queryLength, masterSequence, masterLength,
   blockStarts, blockEnds, allowedGaps, myargs[18].intvalue,
   subject_id,query_id, &bestFirstBlock,&bestLastBlock,myargs[24].floatvalue,
   myargs[25].floatvalue,searchSpaceSize, localAlignment);
   
   
#ifdef OS_UNIX
   fprintf(outfp, "%s", "done");
#endif

   ReadDBBioseqFetchEnable ("blockalign", "nr", FALSE, TRUE);
   print_options = 0;
   align_options = 0;
   align_options += TXALIGN_COMPRESS;
   align_options += TXALIGN_END_NUM;
   if (show_gi) {
     align_options += TXALIGN_SHOW_GI;
     print_options += TXALIGN_SHOW_GI;
   } 

   if (myargs[3].intvalue != 0)
     {
       align_options += TXALIGN_MASTER;
       if (myargs[3].intvalue == 1 || 
	   myargs[3].intvalue == 3)
	 align_options += TXALIGN_MISMATCH;
       if (myargs[3].intvalue == 3 || 
	   myargs[3].intvalue == 4 || 
	   myargs[3].intvalue == 6)
	 align_options += TXALIGN_FLAT_INS;
       if (myargs[3].intvalue == 5 || 
	   myargs[3].intvalue == 6)
	 align_options += TXALIGN_BLUNT_END;
     }
   else
     {
       align_options += TXALIGN_MATRIX_VAL;
       align_options += TXALIGN_SHOW_QS;
     }
   
   number_of_descriptions = myargs[19].intvalue;
   number_of_alignments = myargs[20].intvalue;

   /*matrix?*/
   matrix = NULL;
   txmatrix = NULL;
   matrix = BLAST_MatrixFetch("BLOSUM62");
   txmatrix = BlastMatrixToTxMatrix(matrix);

   if (results) {
     if (seqannot)
       seqannot = SeqAnnotFree(seqannot);
     seqannot = SeqAnnotNew();
     seqannot->type = 2;
     AddAlignInfoToSeqAnnot(seqannot, 2);
     seqannot->data = results;
     if (outfp)
       {
	 fprintf(outfp, "\nResults from blockalign search\n");
	 prune = BlastPruneHitsFromSeqAlign(results, number_of_descriptions, NULL);
	 ObjMgrSetHold();
	 init_buff_ex(85);
	 PrintDefLinesFromSeqAlign(prune->sap, 80, outfp, print_options, FIRST_PASS, NULL);
	 free_buff();
	 prune = BlastPruneHitsFromSeqAlign(results, number_of_alignments, prune);
	 if(!DDV_DisplayBlastPairList(prune->sap, NULL, 
                                         outfp, FALSE, 
                                         align_options,
                                         (Uint1)(align_options & TXALIGN_HTML ? 6 : 1))) {
                fprintf(stdout, 
                        "\n\n!!!\n   "
                        "    --------  Failure to print alignment...  --------"
                        "\n!!!\n\n");
                fflush(stdout);
            }
       }
     seqannot->data = results;
     prune = BlastPruneSapStructDestruct(prune);
     ObjMgrClearHold();
     ObjMgrFreeCache(0);
   }
   else  {
       fprintf(outfp, "\n\n ***** No hits found ******\n\n");
   }
   if (aip && seqannot) {
       SeqAnnotAsnWrite((SeqAnnotPtr) seqannot, aip, NULL);
       AsnIoReset(aip);
       aip = AsnIoClose(aip);
   }
   if (localAlignment)
     fprintf(diagfp, "Highest-scoring alignment extends from block %d through block %d\n",
	   bestFirstBlock, bestLastBlock);

   free_buff();

   ReadDBBioseqFetchDisable();

   if (NULL !=frozenBlocks) 
     MemFree(frozenBlocks);

   for (i = 0; i < masterLength; i++) {
     if (NULL != thisPosFreqs[i])
       MemFree(thisPosFreqs[i]);
     if (NULL != thisScoreMat[i])
       MemFree(thisScoreMat[i]);
   }
   freeBestScores(numBlocks);
   freeBestPairs(numBlocks);
   freeAlignPieceLists(numBlocks);
   FileClose(infp);   
   FileClose(outfp);
   FileClose(diagfp);
   return 0;
}


