/* $Id$
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
 * Author: Ilya Dondoshansky
 *
 */

/** @file phi_gapalign.c
 * Functions to perform gapped alignment in PHI BLAST.
 * Pattern alignment does not contribute to the score.
 * The main function calls are described below in pseudo code.
 * 
 * <pre>
 * Preliminary gapped alignment (does not align patterns). The following
 * function is called once per query/subject sequence pair.
 *
 * PHIGetGappedScore
 *    for each pattern occurrence in query and in subject {
 *       s_PHIGappedAlignment
 *          Left score = Blast_SemiGappedAlign(...)
 *          Right score = Blast_SemiGappedAlign(...) 
 *    }
 *
 * Alignment with traceback, including alignment of patterns, called for each
 * HSP saved in the preliminary stage.
 * 
 * PHIGappedAlignmentWithTraceback
 *    Left score = Blast_SemiGappedAlign(...)
 *    s_PHIBlastAlignPatterns(...)    
 *    Right score = Blast_SemiGappedAlign(...) 
 * </pre>
 * @todo FIXME The alignment code uses complicated representation of the
 *             pattern, and may produce wrong alignments of patterns in case
 *             when pattern has variable length pieces longer than 32 residues.
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/phi_gapalign.h>
#include <algo/blast/core/blast_encoding.h>
#include "blast_gapalign_priv.h"
#include "pattern_priv.h"

/** @todo FIXME Figure out what these mean and document. */
typedef enum {
    eDiagonalInsert = 1,
    eDiagonalDelete = 2,
    eInsertCode = 10,
    eDeleteCode = 20
} EDiagonalState;

/** Returns the cost of an optimum conversion within highDiag and lowDiag 
 * between two sequence segments and appends such a conversion to the current
 * script.
 * @param seq1 Points 1 byte before the start of the first sequence segment [in]
 * @param seq2 Points 1 byte before the start of the second sequence segment [in]
 * @param end1 Length of the first sequence segment [in]
 * @param end2 Length of the second sequence segment [in]
 * @param lowDiag Low diagonal for the alignment [in]
 * @param highDiag High diagonal for the alignment [in]
 * @param matrix Scoring matrix [in]
 * @param gapOpen Gap opening cost [in]
 * @param gapExtend Gap extension cost [in]
 * @param alignScript Stores traceback information. [out]
 */
static Int4 
s_Align(Uint1 * seq1, Uint1 * seq2, Int4 end1, Int4 end2, Int4 lowDiag, 
        Int4 highDiag, Int4 **matrix, Int4 gapOpen, Int4 gapExtend, 
        GapPrelimEditBlock* alignScript)
{
    Int4 nextState; /*stores code for next entry in state*/
    Int4 score; /*score to return*/
	Int4 band; /*number of diagonals between highDiag and lowDiag 
                 inclusive*/
    Int4 diagIndex; /*loop index over diagonals*/
	Int4 leftd, rightd;	/* diagonal indices for CC, DD, CP and DP */
    BlastGapSmallDP* score_array; /*array for dynamic program information*/
	Int4 curd;	/* current index for CC, DD CP and DP */
    Int4 i;  /*loop index*/
    Int4 index1; /*index on seq1*/
	Int4 temp_sub_score = 0; /*placeholder for a substitution score */
	Int4 temp_indel_score = 0; /*placeholder for an indel score */
	Int4 tempHorScore; /*dual of temp_indel_score for the case where a
                              horizontal edge (insertion) is the last step*/
	BlastGapSmallDP* score_row = NULL; /*points to a row of CD*/
	Int4 stateDecoder; /*used to decode the edge information in a state*/
    Int4 initialScore; /*score to initialize dynamic program entries*/
    Int4 *matrixRow; /*row of score matrix*/
    Int1 **state; /*stores dynamic program information*/
    Int1 *stateRow; /*holds one row of state*/
    Int1 *editInstructions; /*holds instruction for string-to-string edit*/
	Int4 index2; /*index on seq2*/
    Int4 charCounter; /*counts number of characters involved in 
                        optimal edit sequence*/
    Int4 charIndex; /*index over character positions in optimal
                      edit sequence*/
    Int4 editStep, nextEditStep; /*steps in string conversion sequence*/
    const Int4 kMinScore = INT4_MIN/2;
	Int4 gapCost = gapOpen + gapExtend;

	/* Boundary cases: end1 <= 0 , end2 <= 0, or highDiag-lowDiag <= 0 */
	band = highDiag-lowDiag+1;

    /* Allocate array of scores. */
	score_array = (BlastGapSmallDP*) calloc(band+2, sizeof(BlastGapSmallDP));

	state = (Int1 **) malloc(sizeof(Int1 *)*(end1+1));
	state[0] = (Int1 *) malloc((end1+1)*(band+2));
	for (index1 = 1; index1 <= end1; index1++) 
	  state[index1] = state[index1-1]+band+2;

	/* Initialization */
	leftd = 1-lowDiag;
	rightd = highDiag-lowDiag+1;

	score_array[leftd].best = 0; 
	state[0][leftd] = -1;
	initialScore = -gapOpen;
	for (diagIndex = leftd+1; diagIndex <= rightd; diagIndex++) {
	  score_array[diagIndex].best = initialScore = initialScore-gapExtend;
	  score_array[diagIndex-1].best_gap = initialScore-gapCost;
	  state[0][diagIndex] = eDiagonalInsert;
	}
	score_array[rightd+1].best = kMinScore;
	score_array[rightd].best_gap = kMinScore;
	score_array[leftd-1].best_gap = -gapCost;
	score_array[leftd-1].best = kMinScore;
	for (i = 1; i <= end1; i++) {
        if (i > end2-highDiag) 
            rightd--;
        if (leftd > 1) 
            leftd--;
        matrixRow = matrix[seq1[i]];
        temp_indel_score = score_array[leftd].best_gap;
        nextState = 0;
        if ((index2 = leftd+lowDiag-1+i) > 0) 
            temp_sub_score = score_array[leftd].best+matrixRow[seq2[index2]];
        if (temp_indel_score > temp_sub_score || index2 <= 0) {
            temp_sub_score = temp_indel_score;
            nextState = eDiagonalDelete;
        }
        tempHorScore = temp_sub_score-gapCost;
        if (leftd >= 1) {
            if ((temp_indel_score-= gapExtend) >= tempHorScore) {
                score_array[leftd-1].best_gap = temp_indel_score;
                nextState += eDeleteCode;
            } else {
                score_array[leftd-1].best_gap = tempHorScore;
            }
        }
        stateRow = &state[i][leftd];
        *stateRow++ = nextState;
        score_array[leftd].best = temp_sub_score;
        for (curd=leftd+1, score_row = &score_array[curd]; curd <= rightd; curd++) {
            temp_sub_score = score_row->best + matrixRow[seq2[curd+lowDiag-1+i]];
            if ((temp_indel_score=score_row->best_gap) > temp_sub_score) { 
                if (temp_indel_score > tempHorScore) {
                    score_row->best = temp_indel_score; 
                    tempHorScore -= gapExtend; 
                    (score_row++-1)->best_gap = temp_indel_score-gapExtend; 
                    *stateRow++=eDeleteCode + eInsertCode + eDiagonalDelete;
                } else {
                    score_row->best = tempHorScore; 
                    tempHorScore -= gapExtend;
                    (score_row++-1)->best_gap = temp_indel_score-gapExtend; 
                    *stateRow++=eDeleteCode + eInsertCode + eDiagonalInsert;
                }
            } else if (tempHorScore > temp_sub_score) { 	       
                score_row->best = tempHorScore; 
                tempHorScore -= gapExtend;
                (score_row++-1)->best_gap = temp_indel_score-gapExtend; 
                *stateRow++=eDeleteCode + eInsertCode + eDiagonalInsert;;
            } else {
                score_row->best = temp_sub_score;
                if ((temp_sub_score -= gapCost) > 
                    (tempHorScore-=gapExtend)) {
                    tempHorScore = temp_sub_score;
                    nextState = 0;
                } else {
                    nextState = eInsertCode;
                }
                if (temp_sub_score > (temp_indel_score -= gapExtend)) { 
                    *stateRow++= nextState; 
                    (score_row++-1)->best_gap = temp_sub_score;
                } else { 
                    *stateRow++ = nextState+eDeleteCode; 
                    (score_row++-1)->best_gap = temp_indel_score;
                }
            }
        }
	}

	/* decide which path to be traced back */
	score = (score_row-1)->best;

	editInstructions = (Int1*) malloc(end1+end2);
	for (index1 = end1, diagIndex = rightd, editStep=0, charCounter = 0; 
	     index1>=0; index1--, charCounter++) {
        nextEditStep  = (stateDecoder=state[index1][diagIndex]) % eInsertCode;
        if (stateDecoder == -1) 
            break;
        if (editStep == eDiagonalInsert && ((stateDecoder/eInsertCode)%2) == 1) 
            nextEditStep = eDiagonalInsert;
        if (editStep == eDiagonalDelete && (stateDecoder/eDeleteCode)== 1) 
            nextEditStep = eDiagonalDelete;
        if (nextEditStep == eDiagonalInsert) { 
            diagIndex--;
            index1++;
        } else if (nextEditStep == eDiagonalDelete) {
	      diagIndex++;
        }
        editInstructions[charCounter] = editStep = nextEditStep;
	}
	for (charIndex = charCounter-1; charIndex >= 0; charIndex--) {
        switch(editInstructions[charIndex]) {
        case 0: 
            GapPrelimEditBlockAdd(alignScript, eGapAlignSub, 1);
            break;
        case eDiagonalInsert:
            GapPrelimEditBlockAdd(alignScript, eGapAlignDel, 1);
            break;
        case eDiagonalDelete:
            GapPrelimEditBlockAdd(alignScript, eGapAlignIns, 1);
            break;
        }
    }
	sfree(editInstructions); 
	sfree(state[0]); 
	sfree(state);
	sfree(score_array);

	return(score);
}


/** k-symbol indel cost.
 * @param gap_open Gap opening cost [in]
 * @param gap_extend Gap extension cost [in]
 * @param length Length of a gap [in]
 * @return Total cost of this gap = gap open + (gap extend)*length.
 */
static Int4
s_GapCost(Int4 gap_open, Int4 gap_extend, Int4 length)
{
    return (length <= 0 ? 0 : (gap_open + gap_extend*length));
}

/** Do a banded gapped alignment of two sequences.
 * @param seq1 First sequence [in]
 * @param seq2 Second sequence [in]
 * @param start1 Starting position in seq1 [in]
 * @param start2 Starting position in seq2 [in]
 * @param lowDiag Low diagonal in the banded alignment [in]
 * @param highDiag High diagonal in the banded alignment [in]
 * @param matrix Scoring matrix [in]
 * @param gapOpen Gap opening penalty [in]
 * @param gapExtend Gap extension penalty [in]
 * @param alignScript Stores traceback information [in] [out]
 * @return Alignment score.
 */
static Int4 
s_BandedAlign(Uint1 *seq1, Uint1 *seq2,Int4 start1, Int4 start2, 
              Int4 lowDiag, Int4 highDiag, Int4 **matrix, Int4 gapOpen,
              Int4 gapExtend, GapPrelimEditBlock* alignScript)
{ 
	Int4 score; /*score to return*/
        Int4 i; /*index over sequences*/
	Int4 band; /*width of band*/
    
	lowDiag = MIN(MAX(-start1, lowDiag),MIN(start2-start1,0));
	highDiag = MAX(MIN(start2, highDiag),MAX(start2-start1,0));

	if (start2 <= 0) { 
        if (start1 > 0) 
            GapPrelimEditBlockAdd(alignScript, eGapAlignIns, start1);
        return -s_GapCost(gapOpen, gapExtend, start1);
	}
	if (start1 <= 0) {
        GapPrelimEditBlockAdd(alignScript, eGapAlignDel, start2);
        return -s_GapCost(gapOpen, gapExtend, start2);
	}

	if ((band = highDiag-lowDiag+1) <= 1) {
        score = 0;
        for (i = 1; i <= start1; i++) {
            GapPrelimEditBlockAdd(alignScript, eGapAlignSub, 1);
            score += matrix[seq1[i]][seq2[i]];
        }
        return score;
	}

    score = s_Align(seq1, seq2, start1, start2, lowDiag, highDiag, matrix,
                    gapOpen, gapExtend, alignScript);

	return score;
}

/** Finds the position of the first pattern match in an input sequence, if 
 * pattern consists of a single word.
 * The existence of the pattern in sequence must be already established.
 * @param seq Sequence to find pattern in [in]
 * @param len Length of seq [in]
 * @param start Start of pattern [out]
 * @param end End of pattern [out]
 * @param pattern_blk Pattern information [in]
 */
static Int2 
s_PHIGetShortPattern(Uint1 *seq, Int4 len, Int4 *start, Int4 *end, 
                     SPHIPatternSearchBlk *pattern_blk)
{
    Int4 mask;   /*mask of input pattern positions after which
                  a match can be declared*/
    Int4  maskShiftPlus1; /*mask shifted left plus 1*/
    Int4 prefixMatchedBitPattern = 0; /*indicates where pattern aligns
                 with seq; e.g., if value is 9 = 0101 then 
                 last 3 chars of seq match first 3 positions in pattern
                 and last 1 char of seq matches 1 position of pattern*/
    Int4 i; /*index over seq */
    Int4 rightOne;  /*loop index looking for 1 in both mask and
                     prefixMatchedBitPattern*/
    Int4 rightMaskOnly; /*rightmost bit that is 1 in the mask only*/
    SShortPatternItems* word_items = pattern_blk->one_word_items;

    mask = word_items->match_mask; 
    maskShiftPlus1 = (mask << 1) +1;
    for (i = 0, prefixMatchedBitPattern= 0; i < len; i++) {
        prefixMatchedBitPattern =  
            ((prefixMatchedBitPattern << 1) | maskShiftPlus1) & 
            word_items->whichPositionPtr[seq[i]];
    }

    _PHIGetRightOneBits(prefixMatchedBitPattern, mask, 
                        &rightOne, &rightMaskOnly);
    
    *start = rightMaskOnly + 1;
    *end = rightOne;
    return 0;
}

/** Finds the position of the first pattern match in an input sequence, if 
 * pattern consists of a more than one word. The existence of the pattern in
 * sequence must be already established.
 * @param seq Sequence to find pattern in [in]
 * @param len Length of seq [in]
 * @param start Start of pattern [out]
 * @param end End of pattern [out]
 * @param pattern_blk Pattern information [in]
 */
static void 
s_PHIGetLongPattern(Uint1 *seq, Int4 len, Int4 *start, Int4 *end,
                    SPHIPatternSearchBlk *pattern_blk)
{
    Int4 *mask;  /*mask of input pattern positions after which
                  a match can be declared*/
    Int4 *prefixMatchedBitPattern; /*indicates where pattern aligns with seq*/
    Int4 wordIndex; /*index over words in pattern*/
    Int4  i;  /*index over seq*/
    Int4 rightMaskOnly; /*rightmost bit that is 1 in the mask only*/
    Int4 j = 0; /*index over bits in a word*/
    Boolean found = FALSE;  /*found match position yet*/
    SLongPatternItems* multiword_items = pattern_blk->multi_word_items;
    Int4 num_words = multiword_items->numWords;

    mask = (Int4 *) calloc(num_words, sizeof(Int4));
    prefixMatchedBitPattern = (Int4 *) 
        calloc(num_words, sizeof(Int4));
    for (wordIndex = 0; wordIndex < num_words; wordIndex++) {
        mask[wordIndex] = multiword_items->match_maskL[wordIndex];
        prefixMatchedBitPattern[wordIndex] = 0;
    }
    _PHIPatternWordsLeftShift(mask, 1, num_words);
    for (i = 0; i < len; i++) {
        _PHIPatternWordsLeftShift(prefixMatchedBitPattern, 0, num_words);
        _PHIPatternWordsBitwiseOr(prefixMatchedBitPattern, mask, num_words); 
        _PHIPatternWordsBitwiseAnd(prefixMatchedBitPattern, 
                                 prefixMatchedBitPattern, 
                                 multiword_items->bitPatternByLetter[seq[i]], 
                                 num_words);
    }
    _PHIPatternWordsBitwiseAnd(prefixMatchedBitPattern, prefixMatchedBitPattern,
                              multiword_items->match_maskL, num_words);
    rightMaskOnly = -1;
    for (wordIndex = 0; (wordIndex < num_words) && (!found); 
         wordIndex++) {
        for (j = 0; j < PHI_BITS_PACKED_PER_WORD && (!found); j++) {
            if ((prefixMatchedBitPattern[wordIndex]>>j) % 2 == 1)
                found = TRUE;
            else if ((multiword_items->match_maskL[wordIndex] >> j)%2 == 1) 
                rightMaskOnly = wordIndex*PHI_BITS_PACKED_PER_WORD+j;
        }
    }
    if (found) {
        wordIndex--;
        j --;
    }
    sfree(prefixMatchedBitPattern);
    sfree(mask);
    *start = rightMaskOnly+1; 
    *end = wordIndex*PHI_BITS_PACKED_PER_WORD+j;
}

/** Maximal possible size of the hits array for one word of pattern. */
#define MAX_HITS_IN_WORD 64

/** Find pattern occurrences in seq when the pattern description is
 * extra long, report the results back in hitArray
 * @param seq Sequence to find pattern in [in]
 * @param len Length of seq [in]
 * @param hitArray Stores pairs of length/position for matches [out]
 * @param pattern_blk All necessary information about pattern [in] 
 */
static Int2 
s_PHIGetExtraLongPattern(Uint1 *seq, Int4 len, Int4 *hitArray, 
                         SPHIPatternSearchBlk *pattern_blk)
{
    Int4 i, j; /*indices on one word hits*/
    Int4  wordIndex, wordIndex2; /*indices on words in pattern representation*/
    Int4  twiceHitsOneWord; /*Twice the number of hits against one
                              word of the pattern*/
    Int4  hitIndex; /*index over hits against one word*/
    Int4 pos = 0; /*keeps track of how many intermediate hits*/
    Int4 hitArray1[PHI_MAX_HIT];
    Int4 oneWordHitArray[MAX_HITS_IN_WORD]; /*hits for one word of 
                                            pattern representation*/
    SShortPatternItems* one_word_items = pattern_blk->one_word_items; 
    SLongPatternItems* multiword_items = pattern_blk->multi_word_items; 
    SExtraLongPatternItems* extra_items = multiword_items->extra_long_items;
    Int4 num_words = multiword_items->numWords;

    i = 1; 

    hitArray[0] = extra_items->numPlacesInWord[0];
    for (wordIndex = 1; wordIndex < num_words; wordIndex++) {
        one_word_items->whichPositionPtr = multiword_items->SLL[wordIndex]; 
        one_word_items->match_mask = multiword_items->match_maskL[wordIndex];
        pos = 0;
        for (j = 0; j < i; j += wordIndex) {
            Int4 lastOffset = hitArray[j+wordIndex-1];
            twiceHitsOneWord = 
                _PHIBlastFindHitsShort(oneWordHitArray, 
                    seq + lastOffset,
                    MIN(len-lastOffset, extra_items->spacing[wordIndex-1] +
                        extra_items->numPlacesInWord[wordIndex]),
                    pattern_blk);
            for (hitIndex = 0; hitIndex < twiceHitsOneWord; 
                 hitIndex+= 2, pos+= wordIndex+1) {
                for (wordIndex2 = 0; wordIndex2 < wordIndex; wordIndex2++)
                    hitArray1[pos+wordIndex2] = hitArray[j+wordIndex2];
                hitArray1[pos+wordIndex2] = hitArray1[pos+wordIndex2-1] +
                                             oneWordHitArray[hitIndex] + 1;
            }
        }
        i = pos;
        for (j = 0; j < pos; j++) 
            hitArray[j] = hitArray1[j];
    }
    for (j = 0; j < pos; j+= num_words) {
        if (hitArray[j+num_words-1] == len) {
            for (i = 0; i < num_words; i++) 
                hitArray[i] = hitArray[i+j];
            return 0;
        }
    }
    /* This point should never be reached. */
    return -1;
}

/** Align pattern occurrences of the query and subject sequences.
 * @param querySeq Pointer to start of pattern in query [in]
 * @param dbSeq Pointer to start of pattern in subject [in]
 * @param lenQuerySeq Length of pattern occurrence in query [in]
 * @param lenDbSeq Length of pattern occurrence in subject [in]
 * @param alignScript Traceback script [out]
 * @param score_options Scoring options [in]
 * @param score_matrix Scoring matrix [in]
 * @param pattern_blk Structure with information about pattern [in]
 * @return Score for this alignment.
 */
static Int4 
s_PHIBlastAlignPatterns(Uint1 *querySeq, Uint1 *dbSeq, Int4 lenQuerySeq, 
                      Int4 lenDbSeq, GapPrelimEditBlock *alignScript,  
                      const BlastScoringOptions* score_options, 
                      SBlastScoreMatrix* score_matrix, 
                      SPHIPatternSearchBlk *pattern_blk)
{
    const int kBandLow = -5;
    const int kBandHigh = 5;

    Int4  startQueryMatch, endQueryMatch; /*positions delimiting
                             where query matches pattern first */
    Int4 startDbMatch, endDbMatch; /*positions delimiting where
                                     database sequence matches pattern first*/
    Int4  local_score; /*score for return*/
    Int4 queryMatchOffset, dbMatchOffset; /*offset from sequence start where
                                            pattern character matches,
                                            used as indices*/
    Int4 patternPosQuery, patternPosDb; /*positions in pattern
                            for matches to query and database sequences*/

    Int4 placeIndex; /*index over places in pattern*/
    Int4  *hitArray1=NULL, *hitArray2=NULL;
    Int4 gap_open; /*gap opening penalty*/
    Int4 gap_extend; /*gap extension penalty*/
    Int4** matrix = score_matrix->data;
    SLongPatternItems* multiword_items = pattern_blk->multi_word_items;

    gap_open = score_options->gap_open;
    gap_extend = score_options->gap_extend;

    startQueryMatch = 0;
    endQueryMatch = lenQuerySeq - 1;
    startDbMatch = 0;
    endDbMatch = lenDbSeq - 1;
    local_score = 0;

    if (pattern_blk->flagPatternLength == eOneWord) {
        s_PHIGetShortPattern(querySeq, lenQuerySeq, &startQueryMatch, 
                             &endQueryMatch, pattern_blk);
        s_PHIGetShortPattern(dbSeq, lenDbSeq, &startDbMatch, 
                             &endDbMatch, pattern_blk);
    } else if (pattern_blk->flagPatternLength == eMultiWord) {
        s_PHIGetLongPattern(querySeq, lenQuerySeq, &startQueryMatch, 
                            &endQueryMatch, pattern_blk);
        s_PHIGetLongPattern(dbSeq, lenDbSeq, &startDbMatch, 
                            &endDbMatch, pattern_blk);
    } else {
        Int4 QueryWord, DbWord;
        Int4 QueryVarSize, DbVarSize;
        SExtraLongPatternItems* extra_items = multiword_items->extra_long_items;

        hitArray1 = calloc(PHI_MAX_HIT, sizeof(Int4));
        hitArray2 = calloc(PHI_MAX_HIT, sizeof(Int4));
        /* Populate the hit arrays by matching the pattern to the query and 
           subject sequence segments. */
        s_PHIGetExtraLongPattern(querySeq, lenQuerySeq, hitArray1, 
                                 pattern_blk);
        s_PHIGetExtraLongPattern(dbSeq, lenDbSeq, hitArray2, pattern_blk);

        queryMatchOffset = dbMatchOffset = 0;
        QueryWord = DbWord = 0;
        QueryVarSize = DbVarSize = 0;

        for (placeIndex = 0; 
             placeIndex < extra_items->highestPlace; placeIndex++) {

            Int4 patternWord = 
                    multiword_items->inputPatternMasked[placeIndex];

            if (patternWord < 0) {
                QueryVarSize += hitArray1[QueryWord] - 
                                hitArray1[QueryWord-1] -
                                extra_items->numPlacesInWord[QueryWord];
                DbVarSize += hitArray2[DbWord] - 
                             hitArray2[DbWord-1] -
                             extra_items->numPlacesInWord[DbWord];
            }
            else if (patternWord == kMaskAaAlphabetBits) {
                QueryVarSize++;
                DbVarSize++;
            }
            else {
                if (QueryVarSize || DbVarSize) {
                    if (QueryVarSize == DbVarSize) {
                        GapPrelimEditBlockAdd(alignScript, eGapAlignSub, 
                                              QueryVarSize);
                    }
                    else {
                        local_score += s_BandedAlign(querySeq-1, dbSeq-1, 
                                         QueryVarSize, DbVarSize, 
                                         kBandLow, kBandHigh, matrix, 
                                         gap_open, gap_extend, alignScript);
                    }
                    queryMatchOffset += QueryVarSize;
                    querySeq += QueryVarSize;
                    dbMatchOffset += DbVarSize;
                    dbSeq += DbVarSize;
                }

                GapPrelimEditBlockAdd(alignScript, eGapAlignSub, 1);
                queryMatchOffset++;
                querySeq++;
                dbMatchOffset++;
                dbSeq++;
                QueryVarSize = DbVarSize = 0;
            }

            if (queryMatchOffset + QueryVarSize >= hitArray1[QueryWord])
                QueryWord++;
            if (dbMatchOffset + DbVarSize >= hitArray2[DbWord])
                DbWord++;
        }

        sfree(hitArray1);
        sfree(hitArray2);
        
        return local_score;     
    }
    
    for (patternPosQuery = startQueryMatch, patternPosDb = startDbMatch; 
         patternPosQuery <= endQueryMatch || patternPosDb <= endDbMatch; ) {
      if (multiword_items->inputPatternMasked[patternPosQuery] 
          != kMaskAaAlphabetBits && 
          multiword_items->inputPatternMasked[patternPosDb] != 
          kMaskAaAlphabetBits) {
          GapPrelimEditBlockAdd(alignScript, eGapAlignSub, 1);
          patternPosQuery++; 
          patternPosDb++; 
          querySeq++; 
          dbSeq++;
      } else {
          for (queryMatchOffset =0; 
               multiword_items->inputPatternMasked[patternPosQuery] == 
                   kMaskAaAlphabetBits && 
                   patternPosQuery <= endQueryMatch; 
               patternPosQuery++, queryMatchOffset++) ;
          for (dbMatchOffset = 0; 
               multiword_items->inputPatternMasked[patternPosDb] == 
                   kMaskAaAlphabetBits && 
                   patternPosDb <= endDbMatch; 
               patternPosDb++, dbMatchOffset++) ;
          if (queryMatchOffset == dbMatchOffset) {
              do {
                  GapPrelimEditBlockAdd(alignScript, eGapAlignSub, 1);
                  querySeq++;
                  dbSeq++; 
                  queryMatchOffset--;
              } while (queryMatchOffset > 0);
          }	else {
              local_score += 
                  s_BandedAlign(querySeq-1, dbSeq-1, queryMatchOffset, dbMatchOffset, 
                                kBandLow, kBandHigh, matrix, gap_open, gap_extend, 
                                alignScript); 
              querySeq+=queryMatchOffset; 
              dbSeq+=dbMatchOffset;
          }
      }
    }
    return local_score;
}

/** Performs gapped extension for PHI BLAST, given two
 * sequence blocks, scoring and extension options, and an initial HSP 
 * with information from the previously performed ungapped extension
 * @param query_blk The query sequence block [in]
 * @param subject_blk The subject sequence block [in]
 * @param gap_align The auxiliary structure for gapped alignment [in]
 * @param score_params Parameters related to scoring [in]
 * @param query_offset Start of pattern in query [in]
 * @param subject_offset Start of pattern in subject [in]
 * @param query_length Length of pattern in query [in]
 * @param subject_length Length of pattern in subject [in]
 */
static Int2 
s_PHIGappedAlignment(BLAST_SequenceBlk* query_blk, 
                     BLAST_SequenceBlk* subject_blk, 
                     BlastGapAlignStruct* gap_align,
                     const BlastScoringParameters* score_params, 
                     Int4 query_offset, Int4 subject_offset,
                     Int4 query_length, Int4 subject_length)
{
   Boolean found_start, found_end;
   Int4 q_length=0, s_length=0, score_right, score_left;
   Int4 private_q_start, private_s_start;
   Uint1* query,* subject;
    
   if (gap_align == NULL)
      return -1;
   
   q_length = query_offset;
   s_length = subject_offset;
   query = query_blk->sequence;
   subject = subject_blk->sequence;

   found_start = FALSE;
   found_end = FALSE;
    
   /* Looking for "left" score */
   score_left = 0;
   if (q_length != 0 && s_length != 0) {
      found_start = TRUE;
      score_left = 
         Blast_SemiGappedAlign(query, subject, q_length, s_length,
            &private_q_start, &private_s_start, TRUE, NULL, gap_align, 
            score_params, query_offset, FALSE, TRUE);
        
      gap_align->query_start = q_length - private_q_start + 1;
      gap_align->subject_start = s_length - private_s_start + 1;
      
   }

   /* Pattern itself is not included in the gapped alignment */
   q_length += query_length - 1;
   s_length += subject_length - 1;

   score_right = 0;
   if (q_length < query_blk->length && s_length < subject_blk->length) {
      found_end = TRUE;
      score_right = Blast_SemiGappedAlign(query+q_length,
         subject+s_length, query_blk->length-q_length-1, 
         subject_blk->length-s_length-1, &(gap_align->query_stop), 
         &(gap_align->subject_stop), TRUE, NULL, gap_align, 
         score_params, q_length, FALSE, FALSE);
      gap_align->query_stop += q_length;
      gap_align->subject_stop += s_length;
   }
   
   if (found_start == FALSE) {	/* Start never found */
      gap_align->query_start = query_offset;
      gap_align->subject_start = subject_offset;
   }
   
   if (found_end == FALSE) {
      gap_align->query_stop = query_offset + query_length;
      gap_align->subject_stop = subject_offset + subject_length;
   }
   
   gap_align->score = score_right+score_left;

   return 0;
}

Int2 PHIGetGappedScore (EBlastProgramType program_number, 
        BLAST_SequenceBlk* query, BlastQueryInfo* query_info, 
        BLAST_SequenceBlk* subject, 
        BlastGapAlignStruct* gap_align,
        const BlastScoringParameters* score_params,
        const BlastExtensionParameters* ext_params,
        const BlastHitSavingParameters* hit_params,
        BlastInitHitList* init_hitlist,
        BlastHSPList** hsp_list_ptr, BlastGappedStats* gapped_stats)

{
   BlastHSPList* hsp_list;
   BlastInitHSP* init_hsp;
   Int4 index;
   Int2 status = 0;
   BlastHitSavingOptions* hit_options;
   Int4 pattern_index;
   Int4 num_patterns;

   if (!query || !subject || !gap_align || !score_params ||
       !hit_params || !init_hitlist || !hsp_list_ptr)
      return -1;

   if (init_hitlist->total == 0)
      return 0;

   hit_options = hit_params->options;
   if (*hsp_list_ptr == NULL)
      *hsp_list_ptr = hsp_list = Blast_HSPListNew(hit_options->hsp_num_max);
   else 
      hsp_list = *hsp_list_ptr;

   num_patterns = query_info->pattern_info->num_patterns;

   for (pattern_index = 0; pattern_index < num_patterns; ++pattern_index) {
       SPHIPatternInfo* query_pattern = 
           &query_info->pattern_info->occurrences[pattern_index];
       Uint4 q_pat_offset = query_pattern->offset;
       Uint4 q_pat_length = query_pattern->length;

       for (index=0; index<init_hitlist->total; index++) {
           BlastHSP* new_hsp;
           Int4 s_pat_offset, s_pat_length;
           
           init_hsp = &init_hitlist->init_hsp_array[index];
           s_pat_offset = init_hsp->offsets.phi_offsets.s_start;
           s_pat_length = init_hsp->offsets.phi_offsets.s_end - 
               init_hsp->offsets.phi_offsets.s_start + 1;

           if (gapped_stats)
               ++gapped_stats->extensions;
           
           status =  
               s_PHIGappedAlignment(query, subject, gap_align, score_params,
                                    q_pat_offset, s_pat_offset, q_pat_length,
                                    s_pat_length);
           
           if (status) {
               return status;
           }

           /* PHI BLAST does not support query concatenation, so context is 
              always 0. */
           if (gap_align->score >= hit_params->cutoff_score) {
               Blast_HSPInit(gap_align->query_start, gap_align->query_stop, 
                             gap_align->subject_start, gap_align->subject_stop, 
                             q_pat_offset, s_pat_offset, 
                             0, query_info->contexts[0].frame, 
                             subject->frame, gap_align->score,
                             &(gap_align->edit_script), &new_hsp);
           
               /* Save pattern index and subject length in the HSP structure. */
               new_hsp->pat_info = 
                   (SPHIHspInfo*) malloc(sizeof(SPHIHspInfo));
               
               new_hsp->pat_info->index = pattern_index;
               new_hsp->pat_info->length = s_pat_length;
               
               Blast_HSPListSaveHSP(hsp_list, new_hsp);
           }
       }
   }   

   /* Sort the HSP array by score */
   Blast_HSPListSortByScore(hsp_list);

   *hsp_list_ptr = hsp_list;
   return status;
}

Int2 PHIGappedAlignmentWithTraceback(Uint1* query, Uint1* subject, 
        BlastGapAlignStruct* gap_align, 
        const BlastScoringParameters* score_params,
        Int4 q_start, Int4 s_start, Int4 query_length, Int4 subject_length,
        Int4 q_pat_length, Int4 s_pat_length, 
        SPHIPatternSearchBlk *pattern_blk)
{
    Boolean found_end;
    Int4 score_right, score_left, private_q_length, private_s_length;
    Int2 status = 0;
    GapPrelimEditBlock *fwd_prelim_tback;
    GapPrelimEditBlock *rev_prelim_tback;
    GapPrelimEditBlock *pat_prelim_tback = GapPrelimEditBlockNew();

    if (!gap_align || !score_params || !pattern_blk)
        return -1;
    
    fwd_prelim_tback = gap_align->fwd_prelim_tback;
    rev_prelim_tback = gap_align->rev_prelim_tback;
    GapPrelimEditBlockReset(fwd_prelim_tback);
    GapPrelimEditBlockReset(rev_prelim_tback);

    found_end = FALSE;
    score_left = 0;
        
    score_left = 
       Blast_SemiGappedAlign(query, subject, q_start, s_start, 
           &private_q_length, &private_s_length, FALSE, rev_prelim_tback,
          gap_align, score_params, q_start, FALSE, TRUE);
    gap_align->query_start = q_start - private_q_length;
    gap_align->subject_start = s_start - private_s_length;

    s_PHIBlastAlignPatterns(query+q_start, subject+s_start, q_pat_length,
                            s_pat_length, pat_prelim_tback, score_params->options, 
                            gap_align->sbp->matrix, pattern_blk);

    /* Pattern traceback and left alignment traceback are both going in forward
       direction, so the former can be simply appended to the end of the
       latter. */
    GapPrelimEditBlockAppend(rev_prelim_tback, pat_prelim_tback);
    GapPrelimEditBlockFree(pat_prelim_tback);

    score_right = 0;

    q_start += q_pat_length - 1;
    s_start += s_pat_length - 1;

    if ((q_start < query_length) && (s_start < subject_length)) {
       found_end = TRUE;
       score_right = 
          Blast_SemiGappedAlign(query+q_start, subject+s_start, 
             query_length-q_start-1, subject_length-s_start-1, 
             &private_q_length, &private_s_length, FALSE, fwd_prelim_tback,
             gap_align, score_params, q_start, FALSE, FALSE);

       gap_align->query_stop = q_start + private_q_length + 1;
       gap_align->subject_stop = s_start + private_s_length + 1; 
    }
    
    if (found_end == FALSE) {
        gap_align->query_stop = q_start;
        gap_align->subject_stop = s_start;
    }

    gap_align->edit_script = 
        Blast_PrelimEditBlockToGapEditScript(rev_prelim_tback,
                                             fwd_prelim_tback);

    gap_align->score = score_right + score_left;
    return status;
}
