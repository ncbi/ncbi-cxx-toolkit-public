/* $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's offical duties as a United States Government employee and
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
 * Author: Ilya Dondoshansky
 *
 */

/** @file pattern.c
 * Functions for finding pattern matches in sequence.
 * @todo FIXME needs doxygen comments and lines shorter than 80 characters
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/pattern.h>


/** Looks for 1 bits in the same position of s and mask
 * Let rightOne be the rightmost position where s and mask both have a 1.
 * Let rightMaskOnly < rightOne be the rightmost position where mask has a 1, 
 * if any or -1 otherwise.
 * @param s Some number [in]
 * @param mask Mask [in]
 * @return (rightOne - rightMaskOnly) 
 */
static Int4 
s_LenOf(Int4 s, Int4 mask)
{
    Int4 checkingMatches = s & mask;  /*look for 1 bits in same position*/
    Int4 rightOne; /*loop index looking for 1 in checkingMatches*/
    Int4 rightMaskOnly; /*rightnost bit that is 1 in the mask only*/
    rightMaskOnly = -1;
    /*AAS Changed upper bound on loop here*/
    for (rightOne = 0; rightOne < BITS_PACKED_PER_WORD; rightOne++) {
       if ((checkingMatches >> rightOne) % 2  == 1) 
          return rightOne - rightMaskOnly;
       if ((mask >> rightOne) %2  == 1) 
          rightMaskOnly = rightOne;
    }
    /*ErrPostEx(SEV_FATAL, 1, 0, "wrong\n");*/
    return(-1);
}

/** Routine to find hits of pattern to sequence when sequence is proteins
 * @param hitArray An array of matches to pass back [out]
 * @param seq The input sequence [in]
 * @param len1 Length of the input sequence. [in]
 * @param patternSearch Carries variables that keep track of search 
 *                      parameters. [in]
 * @return the number of matches found.
 */
static Int4 
s_FindHitsShort(Int4 *hitArray, const Uint1* seq, Int4 len1, 
            patternSearchItems *patternSearch)
{
    Int4 i; /*loop index on sequence*/
    Int4 prefixMatchedBitPattern = 0; /*indicates where pattern aligns
                 with seq; e.g., if value is 9 = 0101 then 
                 last 3 chars of seq match first 3 positions in pattern
                 and last 1 char of seq matches 1 position of pattern*/
    Int4 numMatches = 0; /*number of matches found*/
    Int4 mask;  /*mask of input pattern positions after which
                  a match can be declared*/
    Int4 maskShiftPlus1; /*mask shifted left 1 plus 1 */

    mask = patternSearch->match_mask; 
    maskShiftPlus1 = (mask << 1) + 1;
    for (i = 0; i < len1; i++) {
      /*shift the positions matched by 1 and try to match up against
        the next character, also allow next character to match the
        first position*/
      prefixMatchedBitPattern =  
         ((prefixMatchedBitPattern << 1) | maskShiftPlus1) & 
         patternSearch->whichPositionPtr[seq[i]];
      if (prefixMatchedBitPattern & mask) { 
         /*first part of pair is index of place in seq where match
           ends; second part is where match starts*/
         hitArray[numMatches++] = i;
         hitArray[numMatches++] = i - s_LenOf(prefixMatchedBitPattern, mask)+1;
         if (numMatches == MAX_HIT)
         {
            /*ErrPostEx(SEV_WARNING, 0, 0, 
              "%ld matches saved, discarding others", numMatches);*/
            break;
         }
      }
    }
    return numMatches;
}

/** Find hits when sequence is DNA and pattern is short returns twice the number
 * of hits.
 * @param hitArray Array of hits to pass back [out]
 * @param seq The input sequence [in]
 * @param pos Starting position [in]
 * @param len Length of sequence seq [in]
 * @param patternSearch Carries variables that keep track of search 
 *                      parameters. [in]
 * @return Number of hits found.
 */
static Int4 
s_FindHitsShortDNA(Int4* hitArray, const Uint1* seq, Int4 pos, Int4 len,
	       patternSearchItems *patternSearch)
{
  /*Some variables and the algorithm are similar to what is
    used in find_hits() above; see more detailed comments there*/
  Uint4 prefixMatchedBitPattern; /*indicates where pattern aligns
                                  with sequence*/
  Uint4 mask2; /*mask to match agaist*/
  Int4 maskShiftPlus1; /*mask2 shifted plus 1*/
  Uint4 tmp; /*intermediate result of masked comparisons*/
  Int4 i; /*index on seq*/
  Int4 end; /*count of number of 4-mer iterations needed*/
  Int4 remain; /*0,1,2,3 DNA letters left over*/
  Int4 j; /*index on suffixRemnant*/
  Int4 twiceNumHits = 0; /*twice the number of hits*/

  mask2 = patternSearch->match_mask*BITS_PACKED_PER_WORD+15; 
  maskShiftPlus1 = (patternSearch->match_mask << 1)+1;

  if (pos != 0) {
    pos = 4 - pos;
    prefixMatchedBitPattern = ((patternSearch->match_mask * ((1 << (pos+1))-1)*2) +
	 (1 << (pos+1))-1)& patternSearch->DNAwhichSuffixPosPtr[seq[0]];
    seq++;
    end = (len-pos)/4; 
    remain = (len-pos) % 4;
  } 
  else {
    prefixMatchedBitPattern = maskShiftPlus1;
    end = len/4; 
    remain = len % 4;
  }
  for (i = 0; i < end; i++) {
    if ( (tmp = (prefixMatchedBitPattern &
                 patternSearch->DNAwhichPrefixPosPtr[seq[i]]))) {
      for (j = 0; j < 4; j++) {
	if (tmp & patternSearch->match_mask) {
	  hitArray[twiceNumHits++] = i*4+j + pos;
	  hitArray[twiceNumHits++] = i*4+j + pos -s_LenOf(tmp & patternSearch->match_mask, 
					  patternSearch->match_mask)+1;
	}
	tmp = (tmp << 1);
      }
    }
    prefixMatchedBitPattern = (((prefixMatchedBitPattern << 4) | mask2) & patternSearch->DNAwhichSuffixPosPtr[seq[i]]);
  }
  /* In the last byte check bits only up to 'remain' */
  if ( (tmp = (prefixMatchedBitPattern &
               patternSearch->DNAwhichPrefixPosPtr[seq[i]]))) {
     for (j = 0; j < remain; j++) {
        if (tmp & patternSearch->match_mask) {
           hitArray[twiceNumHits++] = i*4+j + pos;
           hitArray[twiceNumHits++] = i*4+j + pos - s_LenOf(tmp & patternSearch->match_mask, patternSearch->match_mask)+1;
        }
        tmp = (tmp << 1);
     }
  }
  return twiceNumHits;
}

/** Top level routine to find hits when pattern has a short description.
 * @param hitArray Array of hits to pass back [out]
 * @param seq Input sequence [in]
 * @param start Position to start at in seq [in]
 * @param len Length of seq [in]
 * @param is_dna 1 if and only if seq is a DNA sequence [in]
 * @param patternSearch Carries variables that keep track of search 
 *                      parameters. [in]
 * @return Number of matches found.
 */
static Int4  
s_FindHitsShortHead(Int4* hitArray, const Uint1* seq, Int4 start, Int4 len, 
                Uint1 is_dna, patternSearchItems *patternSearch)
{
  if (is_dna) 
    return s_FindHitsShortDNA(hitArray, &seq[start/4], start % 4, len, patternSearch);
  return s_FindHitsShort(hitArray, &seq[start], len, patternSearch);
}

/** Shift each word in the array left by 1 bit and add bit b,
 * if the new values is bigger that OVERFLOW1, then subtract OVERFLOW1.
 * @param a Array to work with [in] [out]
 * @param b Bit to add [in]
 * @param patternSearch Carries variables that keep track of search 
 *                      parameters. [in]
 */
static void 
s_MoveLeft(Int4 *a, Uint1 b, patternSearchItems *patternSearch)
{
    Int4 x;
    Int4 i; /*index on words*/
    for (i = 0; i < patternSearch->numWords; i++) {
      x = (a[i] << 1) + b;
      if (x >= OVERFLOW1) {
	a[i] = x - OVERFLOW1; 
	b = 1;
      }
      else { 
	a[i] = x; 
	b = 0;
      }
    }
}  

/** Do a word-by-word bit-wise or of a and b and put the result back in a.
 * @param a First array [in] [out]
 * @param b Second array [in]
 * @param patternSearch Carries variables that keep track of search 
 *                      parameters. [in]
 */
static void 
s_BitwiseOr(Int4 *a, Int4 *b, patternSearchItems *patternSearch)
{
    Int4 i; /*index over words*/
    for (i = 0; i < patternSearch->numWords; i++) 
	a[i] = (a[i] | b[i]);
}

/** Do a word-by-word bit-wise or of a and b and put the result in
 * result.
 * @param result Resulting value [out]
 * @param a First array [in]
 * @param b Second array [in]
 * @param patternSearch Carries variables that keep track of search 
 *                      parameters. [in]
 * @return 1 if there are any non-zero words, otherwise 0.
 */
static Int4 
s_BitwiseAnd(Int4 *result, Int4 *a, Int4 *b, patternSearchItems *patternSearch)
{
    Int4 i; /*index over words*/
    Int4 returnValue = 0;

    for (i = 0; i < patternSearch->numWords; i++) 
      if ( (result[i] = (a[i] & b[i]) ) ) 
	returnValue = 1;
    return returnValue;
}

/** Let F be the number of the first bit in s that is 1.
 * Let G be the first bit in mask that is one such that G < F;
 * Else let G = -1;
 * @param s Input sequence [in]
 * @param mask Array of word masks [in]
 * @param patternSearch Carries variables that keep track of search 
 *                      parameters. [in]
 * @return F - G
 */
static Int4 
s_LenOfL(Int4 *s, Int4 *mask, patternSearchItems *patternSearch)
{
    Int4 bitIndex; /*loop index over bits in a word*/
    Int4 wordIndex;  /*loop index over words*/
    Int4 firstOneInMask;

    firstOneInMask = -1;
    for (wordIndex = 0; wordIndex < patternSearch->numWords; wordIndex++) {
      for (bitIndex = 0; bitIndex < BITS_PACKED_PER_WORD; bitIndex++) { 
	if ((s[wordIndex] >> bitIndex) % 2  == 1) 
	  return wordIndex*BITS_PACKED_PER_WORD+bitIndex-firstOneInMask;
	if ((mask[wordIndex] >> bitIndex) %2  == 1) 
	  firstOneInMask = wordIndex*BITS_PACKED_PER_WORD+bitIndex;
      }
    }
    /*ErrPostEx(SEV_FATAL, 1, 0, "wrong\n");*/
    return(-1);
}

/** Finds places where pattern matches seq and returns them as
 * pairs of positions in consecutive entries of hitArray;
 * similar to s_FindHitsShort
 * @param hitArray Array of hits to return [out]
 * @param seq Input sequence [in]
 * @param len1 Length of seq [in]
 * @param patternSearch carries all the pattern variables
 * @return twice the number of hits.
 */
static Int4 
s_FindHitsLong(Int4 *hitArray, const Uint1* seq, Int4 len1, 
               patternSearchItems *patternSearch)
{
    Int4 wordIndex; /*index on words in mask*/
    Int4 i; /*loop index on seq */
    Int4  *prefixMatchedBitPattern; /*see similar variable in
                                      s_FindHitsShort*/
    Int4 twiceNumHits = 0; /*counter for hitArray*/
    Int4 *mask; /*local copy of match_maskL version of pattern
                  indicates after which positions a match can be declared*/
    Int4 *matchResult; /*Array of words to hold the result of the
                         final test for a match*/

    matchResult = (Int4 *) calloc(patternSearch->numWords, sizeof(Int4));
    mask = (Int4 *) calloc(patternSearch->numWords, sizeof(Int4));
    prefixMatchedBitPattern = (Int4 *) calloc(patternSearch->numWords, sizeof(Int4));
    for (wordIndex = 0; wordIndex < patternSearch->numWords; wordIndex++) {
      mask[wordIndex] = patternSearch->match_maskL[wordIndex];
      prefixMatchedBitPattern[wordIndex] = 0;
    }
    /*This is a multiword version of the algorithm in s_FindHitsShort*/
    s_MoveLeft(mask, 1, patternSearch);
    for (i = 0; i < len1; i++) {
      s_MoveLeft(prefixMatchedBitPattern, 0, patternSearch);
      s_BitwiseOr(prefixMatchedBitPattern, mask, patternSearch); 
      s_BitwiseAnd(prefixMatchedBitPattern, prefixMatchedBitPattern, patternSearch->bitPatternByLetter[seq[i]], patternSearch);
      if (s_BitwiseAnd(matchResult, prefixMatchedBitPattern, patternSearch->match_maskL, patternSearch)) { 
	hitArray[twiceNumHits++] = i; 
	hitArray[twiceNumHits++] = i-s_LenOfL(matchResult, patternSearch->match_maskL, patternSearch)+1;
      }
    }
    sfree(prefixMatchedBitPattern); 
    sfree(matchResult); 
    sfree(mask);
    return twiceNumHits;
}

/** Find matches when pattern is very long,
 * @param hitArray Array to pass back pairs of start and end positions for 
 *                 hits [out]
 * @param seq Input sequence [in]
 * @param len Length of seq [in]
 * @param is_dna Is sequence DNA or protein? [in]
 * @param patternSearch carries all the pattern variables [in]
 * @return Twice the number of hits found.
 */
static Int4 
s_FindHitsVeryLong(Int4 *hitArray, const Uint1* seq, Int4 len, Boolean is_dna,
             patternSearchItems *patternSearch)
{
    Int4 twiceNumHits; /*twice the number of matches*/
    Int4 twiceHitsOneCall; /*twice the number of hits in one call to 
                                 s_FindHitsShort */
    Int4 wordIndex;  /*index over words in pattern*/
    Int4 start; /*start position in sequence for calls to s_FindHitsShort */
    Int4 hitArray1[MAX_HIT]; /*used to get hits against different words*/
    Int4 nextPosInHitArray; /*next available position in hitArray1 */
    Int4 hitIndex1, hitIndex2;  /*indices over hitArray1*/

    patternSearch->whichPositionPtr = 
      patternSearch->SLL[patternSearch->whichMostSpecific]; 
    patternSearch->match_mask = 
      patternSearch->match_maskL[patternSearch->whichMostSpecific];
    if (is_dna) {
      patternSearch->DNAwhichPrefixPosPtr = patternSearch->DNAprefixSLL[patternSearch->whichMostSpecific];
      patternSearch->DNAwhichSuffixPosPtr = patternSearch->DNAsuffixSLL[patternSearch->whichMostSpecific];
    }
    /*find matches to most specific word of pattern*/
    twiceNumHits = s_FindHitsShortHead(hitArray, seq, 0, len, is_dna, patternSearch);
    if (twiceNumHits < 2) 
      return 0;
    /*extend matches word by word*/
    for (wordIndex = patternSearch->whichMostSpecific+1; 
	 wordIndex < patternSearch->numWords; wordIndex++) {
	patternSearch->whichPositionPtr = 
	  patternSearch->SLL[wordIndex]; 
	patternSearch->match_mask = patternSearch->match_maskL[wordIndex];
	if (is_dna) {
	  patternSearch->DNAwhichPrefixPosPtr = patternSearch->DNAprefixSLL[wordIndex]; 
	  patternSearch->DNAwhichSuffixPosPtr = patternSearch->DNAsuffixSLL[wordIndex];
	}
	nextPosInHitArray = 0;
	for (hitIndex2 = 0; hitIndex2 < twiceNumHits; hitIndex2 += 2) {
	  twiceHitsOneCall = s_FindHitsShortHead(&hitArray1[nextPosInHitArray], seq, 
       hitArray[hitIndex2]+1, MIN(len-hitArray[hitIndex2]-1, 
       patternSearch->spacing[wordIndex-1] + patternSearch->numPlacesInWord[wordIndex]), is_dna, patternSearch);
	  for (hitIndex1 = 0; hitIndex1 < twiceHitsOneCall; hitIndex1+= 2) {
	    hitArray1[nextPosInHitArray+hitIndex1] = 
	      hitArray[hitIndex2]+hitArray1[nextPosInHitArray+hitIndex1]+1;
	    hitArray1[nextPosInHitArray+hitIndex1+1] = hitArray[hitIndex2+1];
	  }
	  nextPosInHitArray += twiceHitsOneCall;
	}
	twiceNumHits = nextPosInHitArray;
	if (twiceNumHits < 2) 
	  return 0;
        /*copy back matches that extend */
	for (hitIndex2 = 0; hitIndex2 < nextPosInHitArray; hitIndex2++) 
	  hitArray[hitIndex2] = hitArray1[hitIndex2];
    }
    /*extend each match back one word at a time*/
    for (wordIndex = patternSearch->whichMostSpecific-1; wordIndex >=0; 
	 wordIndex--) {
      patternSearch->whichPositionPtr = 
	patternSearch->SLL[wordIndex]; 
      patternSearch->match_mask = patternSearch->match_maskL[wordIndex];
      if (is_dna) {
	patternSearch->DNAwhichPrefixPosPtr = patternSearch->DNAprefixSLL[wordIndex]; 
	patternSearch->DNAwhichSuffixPosPtr = patternSearch->DNAsuffixSLL[wordIndex];
      }
      nextPosInHitArray = 0;
      for (hitIndex2 = 0; hitIndex2 < twiceNumHits; hitIndex2 += 2) {
	start = hitArray[hitIndex2+1]-patternSearch->spacing[wordIndex]-patternSearch->numPlacesInWord[wordIndex];
	if (start < 0) 
	  start = 0;
	twiceHitsOneCall = s_FindHitsShortHead(&hitArray1[nextPosInHitArray], seq, start, 
			    hitArray[hitIndex2+1]-start, is_dna, patternSearch);
	for (hitIndex1 = 0; hitIndex1 < twiceHitsOneCall; hitIndex1+= 2) {
	  hitArray1[nextPosInHitArray+hitIndex1] = hitArray[hitIndex2];
	  hitArray1[nextPosInHitArray+hitIndex1+1] = start + 
	    hitArray1[nextPosInHitArray+hitIndex1+1];
	}
	nextPosInHitArray += twiceHitsOneCall;
      }
      twiceNumHits = nextPosInHitArray;
      if (twiceNumHits < 2) 
	return 0;
      /*copy back matches that extend*/
      for (hitIndex2 = 0; hitIndex2 < nextPosInHitArray; hitIndex2++) 
	hitArray[hitIndex2] = hitArray1[hitIndex2];
    }
    return twiceNumHits;
}

Int4 FindPatternHits(Int4 *hitArray, const Uint1* seq, Int4 len, 
               Boolean is_dna, patternSearchItems * patternSearch)
{
    if (patternSearch->flagPatternLength == ONE_WORD_PATTERN) 
      return s_FindHitsShortHead(hitArray, seq, 0, len, is_dna, patternSearch);
    if (patternSearch->flagPatternLength == MULTI_WORD_PATTERN) 
      return s_FindHitsLong(hitArray, seq, len, patternSearch);
    return s_FindHitsVeryLong(hitArray, seq, len, is_dna, patternSearch);
}
