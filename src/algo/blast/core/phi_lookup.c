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

/** @file phi_lookup.c
 * Functions for accessing the lookup table for PHI-BLAST
 * @todo FIXME needs doxygen comments and lines shorter than 80 characters
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/pattern.h>
#include <algo/blast/core/phi_lookup.h>
#include <algo/blast/core/blast_message.h>


#define seedepsilon 0.00001
#define allone  ((1 << ALPHABET_SIZE) - 1)

/** Parameters for a seed search in PHI BLAST. */
typedef struct seedSearchItems {
    double  charMultiple[ALPHABET_SIZE];
    double  paramC; /**< used in e-value computation*/
    double  paramLambda; /**< used in e-value computation*/
    double  paramK; /**< used in the bit score computation*/
    Int4         cutoffScore; /**< lower bound for what is a hit*/
    double  standardProb[ALPHABET_SIZE]; /**< probability of each letter*/
    char         order[ASCII_SIZE];
    char         pchars[ALPHABET_SIZE+1];
    char         name_space[BUF_SIZE];  /**< name of a pattern*/
    char         pat_space[PATTERN_SPACE_SIZE];  /**< string description
                                                   of pattern*/
} seedSearchItems;

/** Initialize the order of letters in the alphabet, the score matrix,
 * and the row sums of the score matrix. matrixToFill is the
 * score matrix, program_flag says which variant of the program is
 * used; is_dna tells whether the strings are DNA or protein.
 */
static void 
s_InitOrder(Int4 **matrix, Int4 program_flag, Boolean is_dna, 
            seedSearchItems *seedSearch)
{
    Uint1 i, j; /*loop indices for matrix*/ 
    Int4 *matrixRow; /*row of matrixToFill*/ 
    double rowSum; /*sum of scaled substitution probabilities on matrixRow*/
    
    if (is_dna) {
      seedSearch->order['A'] = 0; 
      seedSearch->order['C'] = 1;
      seedSearch->order['G'] = 2; 
      seedSearch->order['T'] = 3;
    } 
    else {
      for (i = 0; i < ALPHABET_SIZE; i++) 
	seedSearch->order[(Uint1)seedSearch->pchars[i]] = i;
    }
    if (program_flag == SEED_FLAG) {
      for (i = 0; i < ALPHABET_SIZE; i++) 
        seedSearch->charMultiple[i] = 0;
      for (i = 0; i < ALPHABET_SIZE; i++) {
	if (seedSearch->standardProb[i] > seedepsilon) {
	  matrixRow = matrix[i];
	  rowSum= 0;
	  for (j = 0; j < ALPHABET_SIZE; j++) {
	    if (seedSearch->standardProb[j] > seedepsilon) 
	      rowSum += seedSearch->standardProb[j]*exp(-(seedSearch->paramLambda)*matrixRow[j]);
	  }
	  seedSearch->charMultiple[i] = rowSum;
	}
      }
    }
}

/** Initialize occurrence probabilities for each amino acid */
static void 
s_InitProbs(seedSearchItems * seedSearch)
{
   double totalCount;  /*for Robinson frequencies*/
   seedSearch->pchars[0] = '-';
   seedSearch->pchars[1] = 'A';
   seedSearch->pchars[2] = 'B';
   seedSearch->pchars[3] = 'C';
   seedSearch->pchars[4] = 'D';
   seedSearch->pchars[5] = 'E';
   seedSearch->pchars[6] = 'F';
   seedSearch->pchars[7] = 'G';
   seedSearch->pchars[8] = 'H';
   seedSearch->pchars[9] = 'I';
   seedSearch->pchars[10] = 'K';
   seedSearch->pchars[11] = 'L';
   seedSearch->pchars[12] = 'M';
   seedSearch->pchars[13] = 'N';
   seedSearch->pchars[14] = 'P';
   seedSearch->pchars[15] = 'Q';
   seedSearch->pchars[16] = 'R';
   seedSearch->pchars[17] = 'S';
   seedSearch->pchars[18] = 'T';
   seedSearch->pchars[19] = 'V';
   seedSearch->pchars[20] = 'W';
   seedSearch->pchars[21] = 'X';
   seedSearch->pchars[22] = 'Y';
   seedSearch->pchars[23] = 'Z';
   seedSearch->pchars[24] = 'U';
   seedSearch->pchars[25] = '*';
   totalCount = 78.0 + 19.0 + 54.0 + 63.0 + 39.0 +
    74.0 + 22.0 + 52.0 + 57.0 + 90.0 + 22.0 + 45.0 + 52.0 +
     43.0 + 51.0 + 71.0 + 59.0 + 64.0 + 13.0 + 32.0;
   seedSearch->standardProb[0] = 0.0;
   seedSearch->standardProb[1] = 78.0/totalCount; /*A*/
   seedSearch->standardProb[2] = 0.0;
   seedSearch->standardProb[3] = 19.0/totalCount; /*C*/
   seedSearch->standardProb[4] = 54.0/totalCount; /*D*/
   seedSearch->standardProb[5] = 63.0/totalCount; /*E*/
   seedSearch->standardProb[6] = 39.0/totalCount; /*F*/
   seedSearch->standardProb[7] = 74.0/totalCount; /*G*/
   seedSearch->standardProb[8] = 22.0/totalCount; /*H*/
   seedSearch->standardProb[9] = 52.0/totalCount; /*I*/
   seedSearch->standardProb[10] = 57.0/totalCount; /*K*/
   seedSearch->standardProb[11] = 90.0/totalCount; /*L*/
   seedSearch->standardProb[12] = 22.0/totalCount; /*M*/
   seedSearch->standardProb[13] = 45.0/totalCount; /*N*/
   seedSearch->standardProb[14] = 52.0/totalCount; /*P*/
   seedSearch->standardProb[15] = 43.0/totalCount; /*Q*/
   seedSearch->standardProb[16] = 51.0/totalCount; /*R*/
   seedSearch->standardProb[17] = 71.0/totalCount; /*S*/
   seedSearch->standardProb[18] = 59.0/totalCount; /*T*/
   seedSearch->standardProb[19] = 64.0/totalCount; /*V*/
   seedSearch->standardProb[20] = 13.0/totalCount; /*W*/
   seedSearch->standardProb[21] = 0.0;   /*X*/
   seedSearch->standardProb[22] = 32.0/totalCount;   /*Y*/
   seedSearch->standardProb[23] = 0.0;   /*Z*/
   seedSearch->standardProb[24] = 0.0;   /*U*/
}

/** Set uo matches for words that encode 4 DNA characters; figure out
 * for each of 256 possible DNA 4-mers, where a prefix matches the pattern
 * and where a suffix matches the pattern; store in prefixPos and
 * suffixPos; mask has 1 bits for whatever lengths of string
 * the pattern can match, mask2 has 4 1 bits corresponding to
 * the last 4 positions of a match; they are used to
 * do the prefixPos and suffixPos calculations with bit arithmetic.
 */
static void 
s_FindPrefixAndSuffixPos(Int4* S, Int4 mask, Int4 mask2, Uint4* prefixPos, 
		       Uint4* suffixPos)
{
  Int4 i; /*index over possible DNA encoded words, 4 bases per word*/
  Int4 tmp; /*holds different mask combinations*/
  Int4 maskLeftPlusOne; /*mask shifted left 1 plus 1; guarantees 1
                           1 character match effectively */
  Uint1 a1, a2, a3, a4;  /*four bases packed into an integer*/

  maskLeftPlusOne = (mask << 1)+1;
  for (i = 0; i < ASCII_SIZE; i++) {
    /*find out the 4 bases packed in integer i*/
    a1 = NCBI2NA_UNPACK_BASE(i, 3);
    a2 = NCBI2NA_UNPACK_BASE(i, 2);
    a3 = NCBI2NA_UNPACK_BASE(i, 1);
    a4 = NCBI2NA_UNPACK_BASE(i, 0);
    /*what positions match a prefix of a4 followed by a3*/
    tmp = ((S[a4]>>1) | mask) & S[a3];
    /*what positions match a prefix of a4 followed by a3 followed by a2*/
    tmp = ((tmp >>1) | mask) & S[a2];
    /*what positions match a prefix of a4, a3, a2,a1*/
    prefixPos[i] = mask2 & ((tmp >>1) | mask) & S[a1];
    
    /*what positions match a suffix of a2, a1*/
    tmp = ((S[a1]<<1) | maskLeftPlusOne) & S[a2];
    /* what positions match a suffix of a3, a2, a1*/
    tmp = ((tmp <<1) | maskLeftPlusOne) & S[a3];
    /*what positions match a suffix of a4, a3, a2, a1*/
    suffixPos[i] = ((((tmp <<1) | maskLeftPlusOne) & S[a4]) << 1) | maskLeftPlusOne;
  }
}

/** Initialize mask and other arrays for DNA patterns*/
static void 
s_InitDNAPattern(patternSearchItems *patternSearch)
{
  Int4 mask1; /*mask for one word in a set position*/
  Int4 compositeMask; /*superimposed mask1 in 4 adjacent positions*/
  Int4 wordIndex; /*index over words in pattern*/

  if (patternSearch->flagPatternLength != ONE_WORD_PATTERN) {
    for (wordIndex = 0; wordIndex < patternSearch->numWords; wordIndex++) {
      mask1 = patternSearch->match_maskL[wordIndex];
      compositeMask = mask1 + (mask1>>1)+(mask1>>2)+(mask1>>3);
      s_FindPrefixAndSuffixPos(patternSearch->SLL[wordIndex], 
      patternSearch->match_maskL[wordIndex], 
	 compositeMask, patternSearch->DNAprefixSLL[wordIndex], patternSearch->DNAsuffixSLL[wordIndex]);
    }
  } 
  else {
    compositeMask = patternSearch->match_mask + 
      (patternSearch->match_mask>>1) + 
      (patternSearch->match_mask>>2) + (patternSearch->match_mask>>3); 
    patternSearch->DNAwhichPrefixPosPtr = patternSearch->DNAwhichPrefixPositions; 
    patternSearch->DNAwhichSuffixPosPtr = patternSearch->DNAwhichSuffixPositions;
    s_FindPrefixAndSuffixPos(patternSearch->whichPositionsByCharacter, 
    patternSearch->match_mask, compositeMask, 
    patternSearch->DNAwhichPrefixPositions, patternSearch->DNAwhichSuffixPositions);
  }
}

/** Expands pattern.
 * @param inputPatternMasked Masked input pattern [in]
 * @param inputPattern Input pattern [in]
 * @param length Length of inputPattern [in]
 * @param maxLength Limit on how long inputPattern can get [in]
 * @return the final length of the pattern or -1 if too long.
 */
static Int4 
s_ExpandPattern(Int4 *inputPatternMasked, Uint1 *inputPattern, 
		      Int4 length, Int4 maxLength)
{
    Int4 i, j; /*pattern indices*/
    Int4 numPos; /*number of positions index*/
    Int4  k, t; /*loop indices*/
    Int4 recReturnValue1, recReturnValue2; /*values returned from
                                             recursive calls*/
    Int4 thisPlaceMasked; /*value of one place in inputPatternMasked*/
    Int4 tempPatternMask[MaxP]; /*used as a local representation of
                               part of inputPatternMasked*/
    Uint1 tempPattern[MaxP]; /*used as a local representation of part of
                               inputPattern*/

    for (i = 0; i < length; i++) {
      thisPlaceMasked = -inputPatternMasked[i];
      if (thisPlaceMasked > 0) {  /*represented variable wildcard*/
	inputPatternMasked[i] = allone;
	for (j = 0; j < length; j++) {
	  /*use this to keep track of pattern*/
	  tempPatternMask[j] = inputPatternMasked[j]; 
	  tempPattern[j] = inputPattern[j];
	}
	recReturnValue2 = recReturnValue1 = 
	  s_ExpandPattern(inputPatternMasked, inputPattern, length, maxLength);
	if (recReturnValue1 == -1)
	  return -1;
	for (numPos = 0; numPos <= thisPlaceMasked; numPos++) {
	  if (numPos == 1)
	    continue;
	  for (k = 0; k < length; k++) {
	    if (k == i) {
	      for (t = 0; t < numPos; t++) {
		inputPatternMasked[recReturnValue1++] = allone;
                if (recReturnValue1 >= maxLength)
                  return(-1);
	      }
	    }
	    else {
	      inputPatternMasked[recReturnValue1] = tempPatternMask[k];
	      inputPattern[recReturnValue1++] = tempPattern[k];
              if (recReturnValue1 >= maxLength)
                  return(-1);
	    }
	    if (recReturnValue1 >= maxLength) 
	      return (-1);
	  }
	  recReturnValue1 = 
	    s_ExpandPattern(&inputPatternMasked[recReturnValue2], 
		      &inputPattern[recReturnValue2], 
		      length + numPos - 1, 
		      maxLength - recReturnValue2);
	  if (recReturnValue1 == -1) 
	    return -1;
	  recReturnValue2 += recReturnValue1; 
	  recReturnValue1 = recReturnValue2;
	}
	return recReturnValue1;
      }
    }
    return length;
}

/** Pack the next length bytes of inputPattern into a bit vector
 * where the bit is 1 if and only if the byte is non-0.
 * @param inputPattern Input pattern [in]
 * @param length How many bytes to pack? [in]
 * @return packed bit vector.
 */
static Int4 
s_PackPattern(Uint1 *inputPattern, Int4 length)
{
    Int4 i; /*loop index*/
    Int4 returnValue = 0; /*value to return*/
    for (i = 0; i < length; i++) {
      if (inputPattern[i])
	returnValue += (1 << i);
    }
    return returnValue;
}

/** Pack the bit representation of the inputPattern into
 * the array patternSearch->match_maskL. Also packs 
 * patternSearch->bitPatternByLetter.
 * @param numPlaces Number of positions in inputPattern [in]
 * @param inputPattern Input pattern [in]
 * @param patternSearch The structure containing pattern search 
 *                      information. [in] [out]
 */
static void 
s_PackLongPattern(Int4 numPlaces, Uint1 *inputPattern, patternSearchItems *patternSearch)
{
    Int4 charIndex; /*index over characters in alphabet*/
    Int4 bitPattern; /*bit pattern for one word to pack*/
    Int4 i;  /*loop index over places*/
    Int4 wordIndex; /*loop counter over words to pack into*/
    
    patternSearch->numWords = (numPlaces-1) / BITS_PACKED_PER_WORD +1;

    for (wordIndex = 0; wordIndex < patternSearch->numWords; wordIndex++) {
      bitPattern = 0;
      for (i = 0; i < BITS_PACKED_PER_WORD; i++) {
	if (inputPattern[wordIndex*BITS_PACKED_PER_WORD+i]) 
	  bitPattern += (1 << i);
      }
      patternSearch->match_maskL[wordIndex] = bitPattern;
    }
    for (charIndex = 0; charIndex < ALPHABET_SIZE; charIndex++) {
      for (wordIndex = 0; wordIndex < patternSearch->numWords; wordIndex++) {
	bitPattern = 0;
	for (i = 0; i < BITS_PACKED_PER_WORD; i++) {
	  if ((1<< charIndex) & patternSearch->inputPatternMasked[wordIndex*BITS_PACKED_PER_WORD + i]) 
	    bitPattern = bitPattern | (1 << i);
	}
	patternSearch->bitPatternByLetter[charIndex][wordIndex] = 
	  bitPattern;
	}
    }
}

/** Return the number of 1 bits in the base 2 representation of a*/
static 
Int4 s_NumOfOne(Int4 a)
{
  Int4 returnValue;
  returnValue = 0;
  while (a > 0) {
    if (a % 2 == 1) 
      returnValue++;
    a = (a >> 1);
  }
  return returnValue;
}

/** Sets up field in patternSearch when pattern is very long*/
static void 
s_PackVeryLongPattern(Int4 *inputPatternMasked, Int4 numPlacesInPattern, patternSearchItems *patternSearch)
{
    Int4 placeIndex; /*index over places in pattern rep.*/
    Int4 wordIndex; /*index over words*/
    Int4 placeInWord, placeInWord2;  /*index for places in a single word*/
    Int4 charIndex; /*index over characters in alphabet*/
    Int4 oneWordMask; /*mask of matching characters for one word in
                        pattern representation*/
    double patternWordProbability;
    double  most_specific; /*lowest probability of a word in the pattern*/
    Int4 *oneWordSLL; /*holds patternSearch->SLL for one word*/

    most_specific = 1.0; 
    patternSearch->whichMostSpecific = 0; 
    patternWordProbability = 1.0;
    for (placeIndex = 0, wordIndex = 0, placeInWord=0; 
	 placeIndex <= numPlacesInPattern; 	 placeIndex++, placeInWord++) {
      if (placeIndex==numPlacesInPattern || inputPatternMasked[placeIndex] < 0 
	  || placeInWord == BITS_PACKED_PER_WORD ) {
	patternSearch->match_maskL[wordIndex] = 1 << (placeInWord-1);
	oneWordSLL = patternSearch->SLL[wordIndex];
	for (charIndex = 0; charIndex < ALPHABET_SIZE; charIndex++) {
	  oneWordMask = 0;
	  for (placeInWord2 = 0; placeInWord2 < placeInWord; placeInWord2++) {
	    if ((1<< charIndex) & 
		inputPatternMasked[placeIndex-placeInWord+placeInWord2]) 
	      oneWordMask |= (1 << placeInWord2);
	  }
	  oneWordSLL[charIndex] = oneWordMask;
	}
	patternSearch->numPlacesInWord[wordIndex] = placeInWord;
	if (patternWordProbability < most_specific) {
	  most_specific = patternWordProbability;
	  patternSearch->whichMostSpecific = wordIndex;
	}
	if (placeIndex == numPlacesInPattern) 
	  patternSearch->spacing[wordIndex++] = 0; 
	else 
	  if (inputPatternMasked[placeIndex] < 0) { 
	    patternSearch->spacing[wordIndex++] = -inputPatternMasked[placeIndex];
	  }
	  else { 
	    placeIndex--; 
	    patternSearch->spacing[wordIndex++] = 0;
	  }
	placeInWord = -1; 
	patternWordProbability = 1.0;
      }
      else {
	patternWordProbability *= (double) 
	  s_NumOfOne(inputPatternMasked[placeIndex])/ (double) ALPHABET_SIZE;
	}
    }
    patternSearch->numWords = wordIndex;
}

/** Initialize the pattern information structure.
 * @param pattern String describing the pattern to search for [in]
 * @param is_dna boolean describing whether the strings are DNA or protein [in]
 * @param sbp Scoring block with statistical parameters [in]
 * @param pattern_info The initialized structure [out]
 * @param error_msg Error message, if any.
 */
static Int4 
s_InitPattern(Uint1 *pattern, Boolean is_dna, BlastScoreBlk* sbp, 
             patternSearchItems* *pattern_info,
             Blast_Message* *error_msg)
{
    Int4 i; /*index over string describing the pattern*/
    Int4 j; /*index for position in pattern*/
    Int4 charIndex; /*index over characters in alphabet*/
    Int4 secondIndex; /*second index into pattern*/
    Int4 numIdentical; /*number of consec. positions with identical specification*/
    Uint4 charSetMask;  /*index over masks for specific characters*/
    Int4 currentSetMask, prevSetMask ; /*mask for current and previous character positions*/    
    Int4 thisMask;    /*integer representing a bit pattern for a 
                        set of characters*/
    Int4 minWildcard, maxWildcard; /*used for variable number of wildcard
                                     positions*/
    Int4  tj=0; /*temporary copy of j*/
    Int4 tempInputPatternMasked[MaxP]; /*local copy of parts
            of inputPatternMasked*/
    Uint1 c;  /*character occurring in pattern*/
    Uint1 localPattern[MaxP]; /*local variable to hold
                               for each position whether it is
                               last in pattern (1) or not (0) */
    double positionProbability; /*probability of a set of characters
                                    allowed in one position*/
    Int4 currentWildcardProduct; /*product of wildcard lengths for
                                   consecutive character positions that
                                   overlap*/
    seedSearchItems *seedSearch;
    patternSearchItems* patternSearch;

    seedSearch = (seedSearchItems*) calloc(1, sizeof(seedSearchItems));
    patternSearch = *pattern_info = 
       (patternSearchItems*) calloc(1, sizeof(patternSearchItems));

    s_InitProbs(seedSearch);
    s_InitOrder(sbp->matrix->data, PAT_SEED_FLAG, FALSE, seedSearch);

    patternSearch->flagPatternLength = ONE_WORD_PATTERN; 
    patternSearch->patternProbability = 1.0;
    patternSearch->minPatternMatchLength = 0;
    patternSearch->wildcardProduct = 1;
    currentWildcardProduct = 1;
    prevSetMask = 0;
    currentSetMask = 0;

    for (i = 0 ; i < MaxP; i++) {
      patternSearch->inputPatternMasked[i] = 0; 
      localPattern[i] = 0;
    }
    for (i = 0, j = 0; i < (Int4)strlen((Char *) pattern); i++) {
      if ((c=pattern[i]) == '-' || c == '\n' || c == '.' || c =='>' || c ==' ' 
|| c == '<')  /*spacers that mean nothing*/
	continue;
      if ( c != '[' && c != '{') { /*not the start of a set of characters*/
	if (c == 'x' || c== 'X') {  /*wild-card character matches anything*/
          /*next line checks to see if wild card is for multiple positions*/
	  if (pattern[i+1] == '(') {
	    i++;
	    secondIndex = i;
            /*find end of description of how many positions are wildcarded
               will look like x(2) or x(2,5) */
	    while (pattern[secondIndex] != ',' && pattern[secondIndex] != ')')
	      secondIndex++;
	    if (pattern[secondIndex] == ')') {  /*fixed number of positions wildcarded*/
	      i -= 1; 
              /*wildcard, so all characters are allowed*/
	      charSetMask=allone; 
	      positionProbability = 1;
	    }
	    else { /*variable number of positions wildcarded*/	  
	      sscanf((Char*) &pattern[++i], "%d,%d", &minWildcard, &maxWildcard);
	      maxWildcard = maxWildcard - minWildcard;
         currentWildcardProduct *= (maxWildcard + 1);
         if (currentWildcardProduct > patternSearch->wildcardProduct)
            patternSearch->wildcardProduct = currentWildcardProduct;
         patternSearch->minPatternMatchLength += minWildcard;
	      while (minWildcard-- > 0) { 
            /*use one position each for the minimum number of
              wildcard spaces required */
            patternSearch->inputPatternMasked[j++] = allone; 
            if (j >= MaxP) {
               Blast_MessageWrite(error_msg, BLAST_SEV_WARNING, 2, 1, 
                                  "pattern too long");
               return(-1);
            }
	      }
	      if (maxWildcard != 0) {
            /*negative masking used to indicate variability
              in number of wildcard spaces; e.g., if pattern looks
              like x(3,5) then variability is 2 and there will
              be three wildcard positions with mask allone followed
              by a single position with mask -2*/
            patternSearch->inputPatternMasked[j++] = -maxWildcard;
            patternSearch->patternProbability *= maxWildcard;
	      }
         /*now skip over wildcard description with the i index*/
	      while (pattern[++i] != ')') ; 
	      continue;
	    }
	  }
	  else {  /*wild card is for one position only*/
	    charSetMask=allone; 
	    positionProbability =1;
	  }
	} 
	else {
	  if (c == 'U') {   /*look for special U character*/
	    charSetMask = allone*2+1;
	    positionProbability = 1; 
	  }
	  else { 
        /*exactly one character matches*/
        prevSetMask = currentSetMask;
        currentSetMask =  charSetMask = (1 << seedSearch->order[c]);
        if (!(prevSetMask & currentSetMask)) /*character sets don't overlap*/
           currentWildcardProduct = 1;
        positionProbability = 
           seedSearch->standardProb[(Uint1)seedSearch->order[c]];
	  }
	}
      }
      else {
	if (c == '[') {  /*start of a set of characters allowed*/
	  charSetMask = 0;
	  positionProbability = 0;
	  /*For each character in the set add it to the mask and
            add its probability to positionProbability*/
	  while ((c=pattern[++i]) != ']') { /*end of set*/
        if ((c < 'A') || (c > 'Z') || (c == '\0')) {
           Blast_MessageWrite(error_msg, BLAST_SEV_WARNING, 2, 1, 
                              "pattern description has a non-alphabetic"
                              "character inside a bracket");
           
           return(-1);
        }
        charSetMask = charSetMask | (1 << seedSearch->order[c]);
        positionProbability += seedSearch->standardProb[(Uint1)seedSearch->order[c]];
	  }
     prevSetMask = currentSetMask;
     currentSetMask = charSetMask;
	  if (!(prevSetMask & currentSetMask)) /*character sets don't overlap*/
	      currentWildcardProduct = 1;
 	} 
	else {   /*start of a set of characters forbidden*/
	  /*For each character forbidden remove it to the mask and
            subtract its probability from positionProbability*/
	  charSetMask = allone; 
	  positionProbability = 1;
	  while ((c=pattern[++i]) != '}') { /*end of set*/
	    charSetMask = charSetMask -  (charSetMask & (1 << seedSearch->order[c]));
	    positionProbability -= seedSearch->standardProb[(Uint1)seedSearch->order[c]];
	  }
          prevSetMask = currentSetMask;
          currentSetMask = charSetMask;
	  if (!(prevSetMask & currentSetMask)) /*character sets don't overlap*/
	      currentWildcardProduct = 1;
	}
      }
      /*handle a number of positions that are the same */
      if (pattern[i+1] == '(') {  /*read opening paren*/
	i++;
	numIdentical = atoi((Char *) &pattern[++i]);  /*get number of positions*/
        patternSearch->minPatternMatchLength += numIdentical;
	while (pattern[++i] != ')') ;  /*skip over piece in pattern*/
	while ((numIdentical--) > 0) {
	  /*set up mask for these positions*/
	  patternSearch->inputPatternMasked[j++] = charSetMask;
	  patternSearch->patternProbability *= positionProbability; 
	}
      } 
      else {   /*specification is for one posiion only*/
         patternSearch->inputPatternMasked[j++] = charSetMask;
         patternSearch->minPatternMatchLength++;
         patternSearch->patternProbability *= positionProbability;
      }
      if (j >= MaxP) {
         Blast_MessageWrite(error_msg, BLAST_SEV_WARNING, 2, 1, 
                            "pattern too long");
      }
    }
    localPattern[j-1] = 1;
    if (patternSearch->patternProbability > 1.0)
      patternSearch->patternProbability = 1.0;

    for (i = 0; i < j; i++) {
      tempInputPatternMasked[i] = patternSearch->inputPatternMasked[i]; 
      tj = j;
    }
    j = s_ExpandPattern(patternSearch->inputPatternMasked, localPattern, j, MaxP);
    if ((j== -1) || ((j > BITS_PACKED_PER_WORD) && is_dna)) {
      patternSearch->flagPatternLength = PATTERN_TOO_LONG;
      s_PackVeryLongPattern(tempInputPatternMasked, tj, patternSearch);
      for (i = 0; i < tj; i++) 
         patternSearch->inputPatternMasked[i] = tempInputPatternMasked[i];
      patternSearch->highestPlace = tj;
      if (is_dna) 
         s_InitDNAPattern(patternSearch);
      return 1;
    }
    if (j > BITS_PACKED_PER_WORD) {
      patternSearch->flagPatternLength = MULTI_WORD_PATTERN;
      s_PackLongPattern(j, localPattern, patternSearch);
      return j;
    } 
    /*make a bit mask out of local pattern of length j*/
    patternSearch->match_mask = s_PackPattern(localPattern, j);
    /*store for each character a bit mask of which positions
      that character can occur in*/
    for (charIndex = 0; charIndex < ALPHABET_SIZE; charIndex++) {
      thisMask = 0;
      for (charSetMask = 0; charSetMask < (Uint4)j; charSetMask++) {
	if ((1<< charIndex) & patternSearch->inputPatternMasked[charSetMask]) 
	  thisMask |= (1 << charSetMask);
      }
      patternSearch->whichPositionsByCharacter[charIndex] = thisMask;
    }
    patternSearch->whichPositionPtr = patternSearch->whichPositionsByCharacter;
    if (is_dna) 
      s_InitDNAPattern(patternSearch);
    return j; /*return number of places for pattern representation*/
}

Int2 PHILookupTableNew(const LookupTableOptions* opt, 
                       BlastPHILookupTable* * lut,
                       Boolean is_dna, BlastScoreBlk* sbp)
{
   BlastPHILookupTable* lookup = *lut = 
      (BlastPHILookupTable*) malloc(sizeof(BlastPHILookupTable));
   Blast_Message* error_msg = NULL;

   if (!lookup)
      return -1;

   lookup->is_dna = is_dna;
   s_InitPattern((Uint1*)opt->phi_pattern, is_dna, sbp, &lookup->pattern_info,
                &error_msg); 
   lookup->num_matches = 0;
   lookup->allocated_size = MIN_PHI_LOOKUP_SIZE;
   if ((lookup->start_offsets = 
       (Int4*) malloc(MIN_PHI_LOOKUP_SIZE*sizeof(Int4))) == NULL)
      return -1;
   if ((lookup->lengths = (Int4*) malloc(MIN_PHI_LOOKUP_SIZE*sizeof(Int4)))
        == NULL)
      return -1;

   return 0;
}

/** Frees the PHI BLAST pseudo lookup table. */
BlastPHILookupTable* PHILookupTableDestruct(BlastPHILookupTable* lut)
{
   sfree(lut->pattern_info);
   sfree(lut->start_offsets);
   sfree(lut->lengths);
   sfree(lut);
   return NULL;
}

/** Adds a new pattern hit to the PHI BLAST pseudo lookup table.
 * @param lookup The lookup table structure [in] [out]
 * @param offset Offset in query at which pattern was found. [in]
 * @param length Length of the pattern at this offset. [in] 
 */
static Int2 
s_PHIBlastAddPatternHit(BlastPHILookupTable* lookup, Int4 offset, 
                                  Int4 length)
{
   if (lookup->num_matches >= lookup->allocated_size) {
      lookup->start_offsets = (Int4*) realloc(lookup->start_offsets, 
                                              2*lookup->allocated_size);
      lookup->lengths = (Int4*) realloc(lookup->lengths, 
                                              2*lookup->allocated_size);
      if (!lookup->start_offsets || !lookup->lengths)
         return -1;
      lookup->allocated_size *= 2;
   }      
      
   lookup->start_offsets[lookup->num_matches] = offset;
   lookup->lengths[lookup->num_matches] = length;
   ++lookup->num_matches;
   return 0;
}

/** Finds all pattern hits in a given query and saves them in the PHI BLAST 
 * lookup table structure. 
 * @param lookup The lookup table structure [in] [out]
 * @param query Query sequence(s) [in]
 * @param location Segments in the query sequence where to look for 
 *                 pattern [in]
 * @param is_dna Is this a nucleotide sequence? [in]
 */
Int4 PHIBlastIndexQuery(BlastPHILookupTable* lookup, 
        BLAST_SequenceBlk* query, BlastSeqLoc* location, Boolean is_dna)
{
   BlastSeqLoc* loc;
   Int4 from, to;
   Int4 loc_length;
   Uint1* sequence;
   patternSearchItems* pattern_info = lookup->pattern_info;
   Int4* hitArray;
   Int4 i, twiceNumHits;
   
   hitArray = (Int4 *) calloc(2*query->length, sizeof(Int4));

   for(loc=location; loc; loc=loc->next) {
      from = loc->ssr->left;
      to = loc->ssr->right;
      loc_length = to - from + 1;
      sequence = query->sequence + from;
      
      twiceNumHits = FindPatternHits(hitArray, sequence, loc_length, is_dna,
                                     pattern_info);
      
      for (i = 0; i < twiceNumHits; i += 2) {
         s_PHIBlastAddPatternHit(lookup, hitArray[i+1]+from, 
                               hitArray[i]-hitArray[i+1]+1);
      }
   }

   return lookup->num_matches;
}

/** Check if two sequences segments are identical.
 * @param subject Subject sequence [in]
 * @param query Query sequence [in]
 * @param length Length to match [in]
 * @return Do the two sequences match? 
 */
static Boolean 
s_PHIBlastMatchPatterns(Uint1* subject, Uint1* query, Int4 length)
{
   Int4 index;

   for (index = 0; index < length; ++index) {
      if (subject[index] != query[index])
         break;
   }
   return (index == length);
}

/** Implementation of the ScanSubject function for PHI BLAST.
 */
Int4 PHIBlastScanSubject(const LookupTableWrap* lookup_wrap,
        const BLAST_SequenceBlk *query_blk, 
        const BLAST_SequenceBlk *subject_blk, 
        Int4* offset_ptr, Uint4 * query_offsets, Uint4 * subject_offsets, 
        Int4 array_size)
{
   Uint1* subject, *query;
   BlastPHILookupTable* lookup = (BlastPHILookupTable*) lookup_wrap->lut;
   Int4 index, count = 0, twiceNumHits, i;
   Int4 *start_offsets = lookup->start_offsets;
   Int4 *pat_lengths = lookup->lengths;
   Int4 offset, length;
   Int4 hitArray[MAX_HIT];

   query = query_blk->sequence;
   subject = subject_blk->sequence;
   /* It must be guaranteed that all pattern matches for a given 
      subject sequence are processed in one call to this function.
   */
   *offset_ptr = subject_blk->length;

   twiceNumHits = FindPatternHits(hitArray, subject, subject_blk->length, 
                                  lookup->is_dna, lookup->pattern_info);


   for (i = 0; i < twiceNumHits; i += 2) {
#if 0
      if (count > array_size - lookup->num_matches)
            break;
#endif
      length = hitArray[i] - hitArray[i+1] + 1;
      offset = hitArray[i+1];

      for (index = 0; index < lookup->num_matches; ++index) {
         /* Match pattern lengths in subject in query first; then 
            check for identical match of pattern */
         if (length == pat_lengths[index] &&
             s_PHIBlastMatchPatterns(subject+offset, query+start_offsets[index], 
                                   length))
         {
            /* Pattern has matched completely. Save index into the array
               of pattern start offsets in query (so pattern length will
               be accessible in the word finder later), and the subject
               offset. */
            query_offsets[count] = index;
            subject_offsets[count] = offset;
            ++count;
         }
      }
   }
   return count;
}
