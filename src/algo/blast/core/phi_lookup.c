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
#include <algo/blast/core/blast_encoding.h>
#include "pattern_priv.h"

/* Mask for all 1 bits up to an alphabet size. Declared in pattern_priv.h */
const int kMaskAaAlphabetBits = (1 << BLASTAA_SIZE) - 1;

/** Set up matches for words that encode 4 DNA characters; figure out
 * for each of 256 possible DNA 4-mers, where a prefix matches the pattern
 * and where a suffix matches the pattern. Masks are used to do the 
 * calculations with bit arithmetic.
 * @param S Array of words [in]
 * @param mask Has 1 bits for whatever lengths of string the pattern can 
 *             match [in]
 * @param mask2 Has 4 1 bits corresponding to the last 4 positions of a 
 *              match [in]
 * @param prefixPos Saved prefix position [out]
 * @param suffixPos Saved suffix position [out]
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
  for (i = 0; i < PHI_ASCII_SIZE; i++) {
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

/** Initialize mask and other arrays for DNA patterns.
 * @param pattern_blk The SPHIPatternSearchBlk structure to initialize [in][out]
*/
static void 
s_InitDNAPattern(SPHIPatternSearchBlk *pattern_blk)
{
  Int4 mask1; /*mask for one word in a set position*/
  Int4 compositeMask; /*superimposed mask1 in 4 adjacent positions*/
  Int4 wordIndex; /*index over words in pattern*/

  if (pattern_blk->flagPatternLength != eOneWord) {
      SLongPatternItems* multiword_items = pattern_blk->multi_word_items;
      SDNALongPatternItems* dna_items = multiword_items->dna_items;
     for (wordIndex = 0; wordIndex < multiword_items->numWords; wordIndex++) {
          mask1 = multiword_items->match_maskL[wordIndex];
          compositeMask = mask1 + (mask1>>1)+(mask1>>2)+(mask1>>3);
          s_FindPrefixAndSuffixPos(multiword_items->SLL[wordIndex], 
                                   multiword_items->match_maskL[wordIndex], 
                                   compositeMask, 
                                   dna_items->DNAprefixSLL[wordIndex], 
                                   dna_items->DNAsuffixSLL[wordIndex]);
     }
  } else {
      SShortPatternItems* word_items = pattern_blk->one_word_items;
      SDNAShortPatternItems* dna_items = word_items->dna_items;
      Int4 match_mask = word_items->match_mask;

      compositeMask = 
          match_mask + (match_mask>>1) + (match_mask>>2) + (match_mask>>3); 
      dna_items->DNAwhichPrefixPosPtr = dna_items->DNAwhichPrefixPositions; 
      dna_items->DNAwhichSuffixPosPtr = dna_items->DNAwhichSuffixPositions;
      s_FindPrefixAndSuffixPos(word_items->whichPositionPtr, 
                               word_items->match_mask, compositeMask, 
                               dna_items->DNAwhichPrefixPositions, 
                               dna_items->DNAwhichSuffixPositions);
  }
}

/** Expands pattern. @todo FIXME: what exactly does it do?
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
    Int4 tempPatternMask[PHI_MAX_PATTERN_LENGTH]; /*used as a local representation of
                               part of inputPatternMasked*/
    Uint1 tempPattern[PHI_MAX_PATTERN_LENGTH]; /*used as a local representation of part of
                               inputPattern*/

    for (i = 0; i < length; i++) {
      thisPlaceMasked = -inputPatternMasked[i];
      if (thisPlaceMasked > 0) {  /*represented variable wildcard*/
	inputPatternMasked[i] = kMaskAaAlphabetBits;
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
		inputPatternMasked[recReturnValue1++] = kMaskAaAlphabetBits;
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
 * the array pattern_blk->match_maskL. Also packs 
 * pattern_blk->bitPatternByLetter.
 * @param numPlaces Number of positions in inputPattern [in]
 * @param inputPattern Input pattern [in]
 * @param pattern_blk The structure containing pattern search 
 *                      information. [in] [out]
 */
static void 
s_PackLongPattern(Int4 numPlaces, Uint1 *inputPattern, 
                  SPHIPatternSearchBlk *pattern_blk)
{
    Int4 charIndex; /*index over characters in alphabet*/
    Int4 bitPattern; /*bit pattern for one word to pack*/
    Int4 i;  /*loop index over places*/
    Int4 wordIndex; /*loop counter over words to pack into*/
    SLongPatternItems* multiword_items = pattern_blk->multi_word_items;

    multiword_items->numWords = (numPlaces-1) / PHI_BITS_PACKED_PER_WORD +1;

    for (wordIndex = 0; wordIndex < multiword_items->numWords; wordIndex++) {
        bitPattern = 0;
        for (i = 0; i < PHI_BITS_PACKED_PER_WORD; i++) {
            if (inputPattern[wordIndex*PHI_BITS_PACKED_PER_WORD+i]) 
                bitPattern += (1 << i);
        }
        multiword_items->match_maskL[wordIndex] = bitPattern;
    }
    for (charIndex = 0; charIndex < BLASTAA_SIZE; charIndex++) {
        for (wordIndex = 0; wordIndex < multiword_items->numWords; wordIndex++) {
            bitPattern = 0;
            for (i = 0; i < PHI_BITS_PACKED_PER_WORD; i++) {
                if ((1<<charIndex) & 
                    multiword_items->inputPatternMasked[wordIndex*PHI_BITS_PACKED_PER_WORD + i]) 
                    bitPattern = bitPattern | (1 << i);
            }
            multiword_items->bitPatternByLetter[charIndex][wordIndex] = 
                bitPattern;
        }
    }
}

/** Return the number of 1 bits in the base 2 representation of a number a.
 * @param a Value to count bits in [in]
 */
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

/** Sets up fields in SPHIPatternSearchBlk structure when pattern is very long. 
 * @param inputPatternMasked Array of pattern bit masks [in]
 * @param numPlacesInPattern Number of bit masks for the pattern [in]
 * @param pattern_blk Structure to do the setup for [in] [out]
 */
static void 
s_PackVeryLongPattern(Int4 *inputPatternMasked, Int4 numPlacesInPattern, 
                      SPHIPatternSearchBlk *pattern_blk)
{
    Int4 placeIndex; /*index over places in pattern rep.*/
    Int4 wordIndex; /*index over words*/
    Int4 placeInWord, placeInWord2;  /*index for places in a single word*/
    Int4 charIndex; /*index over characters in alphabet*/
    Int4 oneWordMask; /*mask of matching characters for one word in
                        pattern representation*/
    double patternWordProbability;
    double  most_specific; /*lowest probability of a word in the pattern*/
    Int4 *oneWordSLL; /*holds pattern_blk->SLL for one word*/
    SLongPatternItems* multiword_items = pattern_blk->multi_word_items;
    SExtraLongPatternItems* extra_items;

    /* Allocate the extra long pattern items structure. */
    multiword_items->extra_long_items = extra_items = 
        (SExtraLongPatternItems*) calloc(1, sizeof(SExtraLongPatternItems));;

    most_specific = 1.0; 
    extra_items->whichMostSpecific = 0; 
    patternWordProbability = 1.0;
    for (placeIndex = 0, wordIndex = 0, placeInWord=0; 
         placeIndex <= numPlacesInPattern; 	 placeIndex++, placeInWord++) {
        if (placeIndex==numPlacesInPattern || inputPatternMasked[placeIndex] < 0 
            || placeInWord == PHI_BITS_PACKED_PER_WORD ) {
            multiword_items->match_maskL[wordIndex] = 1 << (placeInWord-1);
            oneWordSLL = multiword_items->SLL[wordIndex];
            for (charIndex = 0; charIndex < BLASTAA_SIZE; charIndex++) {
                oneWordMask = 0;
                for (placeInWord2 = 0; placeInWord2 < placeInWord; placeInWord2++) {
                    if ((1<< charIndex) & 
                        inputPatternMasked[placeIndex-placeInWord+placeInWord2]) 
                        oneWordMask |= (1 << placeInWord2);
                }
                oneWordSLL[charIndex] = oneWordMask;
            }
            extra_items->numPlacesInWord[wordIndex] = placeInWord;
            if (patternWordProbability < most_specific) {
                most_specific = patternWordProbability;
                extra_items->whichMostSpecific = wordIndex;
            }
            if (placeIndex == numPlacesInPattern) 
                extra_items->spacing[wordIndex++] = 0; 
            else if (inputPatternMasked[placeIndex] < 0) { 
                extra_items->spacing[wordIndex++] = -inputPatternMasked[placeIndex];
            } else { 
                placeIndex--; 
                extra_items->spacing[wordIndex++] = 0;
            }
            placeInWord = -1; 
            patternWordProbability = 1.0;
        } else {
            patternWordProbability *= (double) 
                s_NumOfOne(inputPatternMasked[placeIndex])/ (double) BLASTAA_SIZE;
        }
    }
    multiword_items->numWords = wordIndex;
}

/** Allocates the SPHIPatternSearchBlk structure. */
static 
SPHIPatternSearchBlk* s_PatternSearchItemsInit()
{
    SPHIPatternSearchBlk* retval =  
        (SPHIPatternSearchBlk*) calloc(1, sizeof(SPHIPatternSearchBlk));
    retval->one_word_items = 
        (SShortPatternItems*) calloc(1, sizeof(SShortPatternItems));
    retval->multi_word_items = 
        (SLongPatternItems*) calloc(1, sizeof(SLongPatternItems));

    retval->flagPatternLength = eOneWord; 
    retval->patternProbability = 1.0;
    retval->minPatternMatchLength = 0;

    return retval;
}

Int2
SPHIPatternSearchBlkNew(char* pattern, Boolean is_dna, BlastScoreBlk* sbp, 
                       SPHIPatternSearchBlk* *pattern_blk_out, 
                       Blast_Message* *error_msg)
{
    const int kWildcardThreshold = 30; /* Threshold for product of variable-length
                                        wildcards*/
    Int4 posIndex; /*index for position in pattern*/
    Int4 charIndex; /*index over string describing the pattern, or over 
                      characters in alphabet*/
    Int4 secondIndex; /*second index into pattern*/
    Int4 numIdentical; /*number of consec. positions with identical
                         specification */
    Uint4 charSetMask;  /*index over masks for specific characters*/
    Int4 currentSetMask, prevSetMask; /*mask for current and previous character
                                        positions*/    
    Int4 thisMask;    /*integer representing a bit pattern for a 
                        set of characters*/
    Int4 minWildcard, maxWildcard; /*used for variable number of wildcard
                                     positions*/
    Int4  tempPosIndex=0; /*temporary copy of posIndex*/
    Int4 tempInputPatternMasked[PHI_MAX_PATTERN_LENGTH]; /*local copy of parts of
                                         inputPatternMasked */
    char next_char;  /*character occurring in pattern*/
    Uint1 localPattern[PHI_MAX_PATTERN_LENGTH]; /*local variable to hold for each position whether
                                it is last in pattern (1) or not (0) */
    double positionProbability; /*probability of a set of characters allowed in
                                  one position*/
    Int4 currentWildcardProduct; /*product of wildcard lengths for consecutive 
                                   character positions that overlap*/
    Int4 wildcardProduct;       /* Maximal product of wildcard lengths. */
    /* Which positions can a character occur in for short patterns*/
    Int4 whichPositionsByCharacter[PHI_ASCII_SIZE]; 
    SPHIPatternSearchBlk* pattern_blk;
    SShortPatternItems* one_word_items;
    SLongPatternItems* multiword_items;
    const Uint1* kOrder = (is_dna ? IUPACNA_TO_NCBI4NA : AMINOACID_TO_NCBISTDAA);
    Blast_ResFreq* rfp = NULL;

    *pattern_blk_out = pattern_blk = s_PatternSearchItemsInit();        
    one_word_items = pattern_blk->one_word_items;
    multiword_items = pattern_blk->multi_word_items;
    
    rfp = Blast_ResFreqNew(sbp);
    Blast_ResFreqStdComp(sbp, rfp);

    wildcardProduct = 1;
    currentWildcardProduct = 1;
    prevSetMask = 0;
    currentSetMask = 0;

    memset(localPattern, 0, PHI_MAX_PATTERN_LENGTH*sizeof(Uint1));

    /* Parse the pattern */
    for (charIndex = 0, posIndex = 0; charIndex < (Int4)strlen(pattern); 
         charIndex++) {
        if ((next_char=pattern[charIndex]) == '-' || next_char == '\n' || 
            next_char == '.' || next_char =='>' || next_char ==' ' || 
            next_char == '<')  /*spacers that mean nothing*/
            continue;
        if ( next_char != '[' && next_char != '{') { /*not the start of a set of characters*/
            if (next_char == 'x' || next_char== 'X') {  /*wild-card character matches anything*/
                /* Next line checks to see if wild card is for multiple 
                   positions */
                if (pattern[charIndex+1] == '(') {
                    charIndex++;
                    secondIndex = charIndex;
                    /* Find end of description of how many positions are 
                       wildcarded will look like x(2) or x(2,5) */
                    while (pattern[secondIndex] != ',' && 
                           pattern[secondIndex] != ')')
                        secondIndex++;
                    if (pattern[secondIndex] == ')') {  
                        /* Fixed number of positions wildcarded*/
                        charIndex -= 1; 
                        /* Wildcard, so all characters are allowed*/
                        charSetMask = kMaskAaAlphabetBits; 
                        positionProbability = 1;
                    }
                    else { /*variable number of positions wildcarded*/	  
                        sscanf(&pattern[++charIndex], "%d,%d", 
                               &minWildcard, &maxWildcard);
                        maxWildcard = maxWildcard - minWildcard;
                        currentWildcardProduct *= (maxWildcard + 1);
                        if (currentWildcardProduct > wildcardProduct)
                            wildcardProduct = currentWildcardProduct;
                        pattern_blk->minPatternMatchLength += minWildcard;
                        while (minWildcard-- > 0) { 
                            /*use one position each for the minimum number of
                              wildcard spaces required */
                            multiword_items->inputPatternMasked[posIndex++] = 
                                kMaskAaAlphabetBits; 
                            if (posIndex >= PHI_MAX_PATTERN_LENGTH) {
                                Blast_MessageWrite(error_msg, BLAST_SEV_WARNING,
                                                   2, 1, "Pattern too long");
                                return(-1);
                            }
                        }
                        if (maxWildcard != 0) {
                            /* Negative masking used to indicate variability
                              in number of wildcard spaces; e.g., if pattern 
                              looks like x(3,5) then variability is 2 and there
                              will be three wildcard positions with mask 
                              kMaskAaAlphabetBits followed by a single position 
                              with mask -2. */
                            multiword_items->inputPatternMasked[posIndex++] = 
                                -maxWildcard;
                            pattern_blk->patternProbability *= maxWildcard;
                        }
                        /* Now skip over wildcard description with the i index */
                        while (pattern[++charIndex] != ')') ; 
                        continue;
                    }
                }
                else {  /*wild card is for one position only*/
                    charSetMask = kMaskAaAlphabetBits; 
                    positionProbability =1;
                }
            } 
            else {
                if (next_char == 'U') {   /*look for special U character*/
                    charSetMask = kMaskAaAlphabetBits*2+1;
                    positionProbability = 1; 
                }
                else { 
                    /*exactly one character matches*/
                    prevSetMask = currentSetMask;
                    currentSetMask =  
                        charSetMask = (1 << kOrder[(Uint1)next_char]);
                    if (!(prevSetMask & currentSetMask)) 
                        /* Character sets don't overlap */
                        currentWildcardProduct = 1;
                    positionProbability = 
                    rfp->prob[(Uint1)kOrder[(Uint1)next_char]];
                }
            }
        } else {
            if (next_char == '[') {  /*start of a set of characters allowed*/
                charSetMask = 0;
                positionProbability = 0;
                /*For each character in the set add it to the mask and
                  add its probability to positionProbability*/
                while ((next_char=pattern[++charIndex]) != ']') { /*end of set*/
                    if ((next_char < 'A') || (next_char > 'Z') || (next_char == '\0')) {
                        Blast_MessageWrite(error_msg, BLAST_SEV_WARNING, 2, 1, 
                            "pattern description has a non-alphabetic"
                            "character inside a bracket");
                        
                        return(-1);
                    }
                    charSetMask = 
                        charSetMask | (1 << kOrder[(Uint1)next_char]);
                    positionProbability += 
                    rfp->prob[(Uint1)kOrder[(Uint1)next_char]];
                }
                prevSetMask = currentSetMask;
                currentSetMask = charSetMask;
                if (!(prevSetMask & currentSetMask)) 
                    /* Character sets don't overlap */
                    currentWildcardProduct = 1;
            } else {   /*start of a set of characters forbidden*/
                /*For each character forbidden remove it to the mask and
                  subtract its probability from positionProbability*/
                charSetMask = kMaskAaAlphabetBits; 
                positionProbability = 1;
                while ((next_char=pattern[++charIndex]) != '}') { /*end of set*/
                    charSetMask = charSetMask - 
                        (charSetMask & (1 << kOrder[(Uint1)next_char]));
                    positionProbability -= 
                    rfp->prob[(Uint1)kOrder[(Uint1)next_char]];
                }
                prevSetMask = currentSetMask;
                currentSetMask = charSetMask;
                if (!(prevSetMask & currentSetMask)) 
                    /* Character sets don't overlap */
                    currentWildcardProduct = 1;
            }
        }
        /*handle a number of positions that are the same */
        if (pattern[charIndex+1] == '(') {  /*read opening paren*/
            charIndex++;
            numIdentical = atoi(&pattern[++charIndex]);  /*get number of positions*/
            pattern_blk->minPatternMatchLength += numIdentical;
            while (pattern[++charIndex] != ')') ;  /*skip over piece in pattern*/
            while ((numIdentical--) > 0) {
                /*set up mask for these positions*/
                multiword_items->inputPatternMasked[posIndex++] = charSetMask;
                pattern_blk->patternProbability *= positionProbability; 
            }
        } 
        else {   /*specification is for one posiion only*/
            multiword_items->inputPatternMasked[posIndex++] = charSetMask;
            pattern_blk->minPatternMatchLength++;
            pattern_blk->patternProbability *= positionProbability;
        }
        if (posIndex >= PHI_MAX_PATTERN_LENGTH) {
            Blast_MessageWrite(error_msg, BLAST_SEV_WARNING, 2, 1, 
                               "Pattern is too long");
        }
    }
    
    /* Free the residue frequencies structure - it's no longer needed */
    rfp = Blast_ResFreqFree(rfp);
    
    /* Pattern should not end in a variable region */
    while (multiword_items->inputPatternMasked[posIndex-1] < 0)
        posIndex--;

    /* The first pattern region should also be of fixed size */
    for (charIndex = 0; charIndex < posIndex; charIndex++) {
        if (multiword_items->inputPatternMasked[charIndex] != 
                                             kMaskAaAlphabetBits)
            break;
    }
    if (multiword_items->inputPatternMasked[charIndex] < 0) {
        for (secondIndex = charIndex + 1; secondIndex < posIndex; 
                                            secondIndex++) {
            if (multiword_items->inputPatternMasked[secondIndex] > 0)
                break;
        }
        for (; secondIndex < posIndex; secondIndex++, charIndex++) {
              multiword_items->inputPatternMasked[charIndex] =
                        multiword_items->inputPatternMasked[secondIndex];
        }
        posIndex = charIndex;
    }
    
    localPattern[posIndex-1] = 1;
    if (pattern_blk->patternProbability > 1.0)
        pattern_blk->patternProbability = 1.0;
    
    for (charIndex = 0; charIndex < posIndex; charIndex++) {
        tempInputPatternMasked[charIndex] = 
            multiword_items->inputPatternMasked[charIndex]; 
        tempPosIndex = posIndex;
    }
    posIndex = s_ExpandPattern(multiword_items->inputPatternMasked, localPattern, 
                        posIndex, PHI_MAX_PATTERN_LENGTH);
    if ((posIndex== -1) || ((posIndex > PHI_BITS_PACKED_PER_WORD) && is_dna)) {
        pattern_blk->flagPatternLength = eVeryLong;
        s_PackVeryLongPattern(tempInputPatternMasked, tempPosIndex, pattern_blk);
        for (charIndex = 0; charIndex < tempPosIndex; charIndex++) 
            multiword_items->inputPatternMasked[charIndex] =
                tempInputPatternMasked[charIndex];
        multiword_items->extra_long_items->highestPlace = tempPosIndex;
        if (is_dna) 
            s_InitDNAPattern(pattern_blk);
        return 0;
    }
    if (posIndex > PHI_BITS_PACKED_PER_WORD) {
        pattern_blk->flagPatternLength = eMultiWord;
        s_PackLongPattern(posIndex, localPattern, pattern_blk);
        return 0;
    } 
    /*make a bit mask out of local pattern of length posIndex*/
    one_word_items->match_mask = s_PackPattern(localPattern, posIndex);
    /*store for each character a bit mask of which positions
      that character can occur in*/
    for (charIndex = 0; charIndex < BLASTAA_SIZE; charIndex++) {
        thisMask = 0;
        for (charSetMask = 0; charSetMask < (Uint4)posIndex; charSetMask++) {
            if ((1<< charIndex) & multiword_items->inputPatternMasked[charSetMask]) 
                thisMask |= (1 << charSetMask);
        }
        whichPositionsByCharacter[charIndex] = thisMask;
    }
    one_word_items->whichPositionPtr = whichPositionsByCharacter;
    if (is_dna) 
        s_InitDNAPattern(pattern_blk);

    if (wildcardProduct > kWildcardThreshold) {
        Blast_MessageWrite(error_msg, BLAST_SEV_WARNING, 2, 1, 
                           "Due to variable wildcards pattern is likely to "
                           "occur too many times in a single sequence\n");
    }
    
    return 0; /*return number of places for pattern representation*/
}

SPHIPatternSearchBlk* SPHIPatternSearchBlkFree(SPHIPatternSearchBlk* lut)
{
    if (lut->multi_word_items) {
        sfree(lut->multi_word_items->extra_long_items);
        sfree(lut->multi_word_items->dna_items);
        sfree(lut->multi_word_items);
    }
    if (lut->one_word_items) {
        sfree(lut->one_word_items->dna_items);
        sfree(lut->one_word_items);
    }

    sfree(lut);
    return NULL;
}

/** Implementation of the ScanSubject function for PHI BLAST.
 * @param lookup_wrap PHI BLAST lookup table [in]
 * @param query_blk Query sequence [in]
 * @param subject_blk Subject sequence [in]
 * @param offset_ptr Next offset in subject - set to end of sequence [out]
 * @param offset_pairs Starts and stops for pattern occurrences in subject [out]
 * @param array_size Not used.
 * @return Number of pattern occurrences found.
 */
Int4 PHIBlastScanSubject(const LookupTableWrap* lookup_wrap,
        const BLAST_SequenceBlk *query_blk, 
        const BLAST_SequenceBlk *subject_blk, 
        Int4* offset_ptr, BlastOffsetPair* NCBI_RESTRICT offset_pairs,
        Int4 array_size)
{
   Uint1* subject;
   SPHIPatternSearchBlk* pattern_blk;
   Int4 index, count = 0, twiceNumHits;
   Int4 hitArray[PHI_MAX_HIT];
   const Boolean kIsDna = (lookup_wrap->lut_type == PHI_NA_LOOKUP);

   ASSERT(lookup_wrap->lut_type == PHI_NA_LOOKUP ||
          lookup_wrap->lut_type == PHI_AA_LOOKUP);

   pattern_blk = (SPHIPatternSearchBlk*) lookup_wrap->lut;

   subject = subject_blk->sequence;
   /* It must be guaranteed that all pattern matches for a given 
    * subject sequence are processed in one call to this function.
    */
   *offset_ptr = subject_blk->length;

   twiceNumHits = FindPatternHits(hitArray, subject, subject_blk->length, 
                                  kIsDna, pattern_blk);


   for (index = 0; index < twiceNumHits; index += 2) {
      offset_pairs[count].phi_offsets.s_start = hitArray[index+1];
      offset_pairs[count].phi_offsets.s_end = hitArray[index];
      ++count;
   }
   return count;
}
