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

/** @file pattern.h
 * Functions for finding pattern matches in sequence (PHI-BLAST).
 */

#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_options.h>

#ifndef PATTERN__H
#define PATTERN__H

#ifdef __cplusplus
extern "C" {
#endif

/** @todo: FIXME comment #defines */

#define BUF_SIZE 100
#define ASCII_SIZE 256
#define BITS_PACKED_PER_WORD 30
#define MaxW   11
#define MaxP   (BITS_PACKED_PER_WORD * MaxW) /*threshold pattern length*/
#define MAX_WORDS_IN_PATTERN 100
#define MAX_HIT 20000
#define OVERFLOW1  (1 << BITS_PACKED_PER_WORD)
#define ONE_WORD_PATTERN  0
#define MULTI_WORD_PATTERN 1
#define ALPHABET_SIZE 25
#define PATTERN_SPACE_SIZE 1000

typedef struct patternSearchItems {
   Int4 numWords;  /**< Number of words need to hold bit representation
                        of pattern*/
   Int4 match_mask;/**< Bit mask representation of input pattern
                        for patterns that fit in a word*/
   Int4 match_maskL[BUF_SIZE]; /**< Bit mask representation of input pattern
                                    for long patterns*/
   Int4 bitPatternByLetter[ASCII_SIZE][MaxW]; /**< Which positions can a 
                                       character occur in for long patterns*/
   Int4 *whichPositionPtr; /**< Used to pass a piece a row of the arrays*/
   Uint4 *DNAwhichPrefixPosPtr; /**< Prefix position array for DNA patterns */
   Uint4 *DNAwhichSuffixPosPtr; /* Suffix position array for DNA patterns*/
   Int4 whichPositionsByCharacter[ASCII_SIZE]; /**< Which positions can a 
                                      character occur in for short patterns*/
   Uint4 DNAwhichPrefixPositions[ASCII_SIZE]; /**< For DNA sequence: where
                                      prefix of DNA 4-mer matches pattern*/
   Uint4 DNAwhichSuffixPositions[ASCII_SIZE]; /**< Similar to above for 
                                                 suffixes*/
    /*for each letter in the alphabet and each word in the masked
      pattern representation, holds a bit pattern saying for which
      positions the letter will match*/
   Int4   SLL[MAX_WORDS_IN_PATTERN][ASCII_SIZE]; /**< Similar to
                  whichPositionsByCharacter for many-word patterns*/
   Uint4   DNAprefixSLL[MAX_WORDS_IN_PATTERN][ASCII_SIZE];
  /*similar to DNAwhichPrefixPositions for many word patterns*/
   Uint4   DNAsuffixSLL[MAX_WORDS_IN_PATTERN][ASCII_SIZE];
  /*similar to DNAwhichSuffixPositions for many word patterns*/
   Char   flagPatternLength; /**< Indicates if pattern fits in 1 word,
                                some words, or is too long*/
   double  patternProbability;  /**< Probability of this letter
                                        combination*/
   Int4   whichMostSpecific; /**< Which word in an extra long pattern
                                has the lowest probability of a match*/
   Int4   numPlacesInWord[MAX_WORDS_IN_PATTERN]; /**< When pattern has more 
             than 7 words, keep track of how many places of pattern in each 
             word of the  representation; was called lening */
   Int4   spacing[MAX_WORDS_IN_PATTERN]; /**< Spaces until next word due to
                                            wildcard*/
   Int4   inputPatternMasked[MaxP];
   Int4   highestPlace; /**< Number of places in pattern representation
                           as computed in input_pattern; was called num*/
  Int4   minPatternMatchLength; /**< Minimum length of string to match this 
                                   pattern*/
  Int4   wildcardProduct; /**< Product of wildcard lengths*/
} patternSearchItems;

/** Find the places where the pattern matches seq;
 * 3 different methods are used depending on the length of the pattern.
 * @param hitArray Stores the results as pairs of positions in consecutive
 *                 entries [out]
 * @param seq Sequence [in]
 * @param len Length of the sequence [in]
 * @param is_dna Indicates whether seq is made of DNA or protein letters [in]
 * @param patternSearch Pattern information [in]
 * @return Twice the number of hits (length of hitArray filled in)
*/
Int4 FindPatternHits(Int4 *hitArray, const Uint1* seq, Int4 len, 
               Boolean is_dna, patternSearchItems * patternSearch);

#ifdef __cplusplus
}
#endif

#endif /* PATTERN__H */
