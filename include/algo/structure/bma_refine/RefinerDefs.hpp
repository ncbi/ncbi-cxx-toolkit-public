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
* Authors:  Chris Lanczycki
*
* File Description:
*      
*    A bunch of typedefs and structure definitions.
*
* ===========================================================================
*/

#ifndef AR_REFINERDEFS__HPP
#define AR_REFINERDEFS__HPP

#include <vector>
#include <map>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <objects/cdd/Cdd.hpp>

#include <algo/structure/struct_util/su_block_multiple_alignment.hpp>
#include <algo/structure/struct_util/su_sequence_set.hpp>
#include <algo/structure/struct_util/struct_util.hpp>

#include <algo/structure/bma_refine/ColumnScorer.hpp>
#include <algo/structure/bma_refine/BlockBoundaryAlgorithm.hpp>
#include <algo/structure/bma_refine/diagnosticDefs.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(struct_util);

BEGIN_SCOPE(align_refine)

typedef double TScoreType;
const TScoreType REFINER_INVALID_SCORE = kMin_Int;  // so can cast to int if need be...


enum RefinerResultCode {
    eRefinerResultOK = 0,
    eRefinerResultNoResults = 9,
    eRefinerResultCantReadCD = 10,
    eRefinerResultAlignmentUtilityError = 11,
    eRefinerResultBadInputAlignmentUtilityError = 12,
    eRefinerResultRangeForRefinementError = 13,
    eRefinerResultRowSelectorError = 14,
    eRefinerResultInconsistentShrinkageSettings = 15,
    eRefinerResultInvalidThresholdValue = 16,
    eRefinerResultInvalidExtraArg = 17,
    eRefinerResultInconsistentArgumentCombination = 18,
    eRefinerResultCantDoBlockEditing = 19,
    eRefinerResultTrialInitializationError = 20,
    eRefinerResultTrialExecutionError = 21,
    eRefinerResultLeaveOneOutExecutionError = 22,
    eRefinerResultLeaveNOutExecutionError = 23,
    eRefinerResultNoRowsToRefine = 24,
    eRefinerResultPhaseSkipped = 100
};

enum RefinerRowSelectorCode {
    eRandomSelectionOrder = 0,
    eWorstScoreFirst,
    eBestScoreFirst
};

struct LeaveOneOutParams {
    bool doLOO;
    bool fixStructures;    //  true if rows that are structures are NOT to undergo LOO
    bool extrasAreRows;    //  any extra parameters are treated as rows (blocks) if true (false)

    bool fullSequence;     //  use full sequence vs. aligned footprint
    int  nExt, cExt;       //  allow extensions to footprints from alignment

    //  If this fraction or higher of LOO operations in a phase have no 
    //  score change, stop future LOO phases.
    double  sameScoreThreshold;

    unsigned int seed;   //  for determining order of row selection
    unsigned int lno;    //  number of rows to leave out between PSSM recalculation
    vector<unsigned int> rowsToExclude;  //  specify extra row numbers to exclude
    RefinerRowSelectorCode selectorCode; //  specify the method for setting order rows undergo LOO

    //  Parameters passed to block-aligner
    double percentile;
    unsigned int extension;
    unsigned int cutoff;
    vector<unsigned int> froms;
    vector<unsigned int> tos;
    vector<unsigned int> blocks;

    LeaveOneOutParams() {
        doLOO = true;
        fixStructures = true;
        extrasAreRows = true;
        fullSequence = false;
        nExt = 0;
        cExt = 0;

        sameScoreThreshold = 0;
        seed = 0;
        lno = 1;
        selectorCode = eRandomSelectionOrder;

        percentile = 1.0;
        extension = 0;
        cutoff = 0;
    }

    ~LeaveOneOutParams() {};

    LeaveOneOutParams(const LeaveOneOutParams& rhs) {
        Copy(rhs);
    }

    LeaveOneOutParams& operator=(const LeaveOneOutParams& rhs) {
        if (this != &rhs) {
            Copy(rhs);
        }
        return *this;
    }

    void Copy(const LeaveOneOutParams& rhs) {
        doLOO = rhs.doLOO;
        fixStructures = rhs.fixStructures;
        extrasAreRows = rhs.extrasAreRows;
        fullSequence = rhs.fullSequence;
        nExt = rhs.nExt;
        cExt = rhs.cExt;

        sameScoreThreshold = rhs.sameScoreThreshold;
        seed = rhs.seed;
        lno = rhs.lno;
        rowsToExclude.assign(rhs.rowsToExclude.begin(), rhs.rowsToExclude.end());
        selectorCode = rhs.selectorCode;

        percentile = rhs.percentile;
        extension = rhs.extension;
        cutoff = rhs.cutoff;

        froms.assign(rhs.froms.begin(), rhs.froms.end());
        tos.assign(rhs.tos.begin(), rhs.tos.end());
        blocks.assign(rhs.blocks.begin(), rhs.blocks.end());
    }
};

//  Not all parameters needed in all block editing scenarios.
struct BlockEditingParams {
    bool editBlocks;              // true if run block editor; false if skip
    bool canShrink;               // true if blocks are permitted to shrink
    BlockBoundaryAlgorithmMethod algMethod;
    ColumnScoringMethod columnMethod;
    TScoreType columnScorerThreshold; // for voting-type scoring, pssm score to compare against
    TScoreType extensionThreshold;    // column score required to extend
    TScoreType shrinkageThreshold;    // column score below which can shrink
    unsigned int minBlockSize;    // don't modify boundaries so block size < this value
    set<unsigned int> editableBlocks;  // only edit these blocks (empty -> edit all blocks)

    //  for compound scoring, may have extra thresholds (e.g. from negScore, negRows...)
    double negScoreFraction;
    double negRowsFraction;
    int median;

    // only for algorithms that allow shrink & extend: may have different scoring methods
    bool extendFirst;             
    ColumnScoringMethod columnMethod2;  

    BlockEditingParams() {
        editBlocks = true;
        canShrink = false;
        algMethod = eInvalidBBAMethod;
        columnMethod = eInvalidColumnScorerMethod;
        columnScorerThreshold = 0;
        extensionThreshold = 0;
        shrinkageThreshold = 0;
        minBlockSize = 1;
        negScoreFraction = 0.3;
        negRowsFraction = 0.3;
        median = 3;
        extendFirst = true;
        columnMethod2 = eInvalidColumnScorerMethod;
    }

    ~BlockEditingParams() {};

    BlockEditingParams(const BlockEditingParams& rhs) {
        Copy(rhs);
    }

    BlockEditingParams& operator=(const BlockEditingParams& rhs) {
        if (this != &rhs) {
            Copy(rhs);
        }
        return *this;
    }

    void Copy(const BlockEditingParams& rhs) {
        editBlocks = rhs.editBlocks;
        canShrink = rhs.canShrink;
        algMethod = rhs.algMethod;
        columnMethod = rhs.columnMethod;
        columnScorerThreshold = rhs.columnScorerThreshold;
        extensionThreshold = rhs.extensionThreshold;
        shrinkageThreshold = rhs.shrinkageThreshold;
        minBlockSize = rhs.minBlockSize;
        negScoreFraction = rhs.negScoreFraction;
        negRowsFraction = rhs.negRowsFraction;
        median = rhs.median;
        extendFirst = rhs.extendFirst;
        columnMethod2 = rhs.columnMethod2;

        editableBlocks = rhs.editableBlocks;
//        editableBlocks.clear();
//        editableBlocks.insert(rhs.editableBlocks.begin(), rhs.editableBlocks.end());
    }
};


struct TrialStats {
    vector<unsigned int> nTries;
    vector<unsigned int> nScoreSame;
    vector<unsigned int> nScoreDrop;
    vector<unsigned int> nScoreDropAccepted;
    unsigned int lastCycle;

    void Clear() {
        nTries.clear();
        nScoreSame.clear();
        nScoreDrop.clear();
        nScoreDropAccepted.clear();
        lastCycle = 0;
    }
};

//  associate an iteration number w/ the final alignment utility object
struct RefinerAU {
    unsigned int iteration;
    struct_util::AlignmentUtility* au;

    RefinerAU(unsigned int i, struct_util::AlignmentUtility* a) {
        iteration = i;
        au = a;
    }
};

typedef void (* TFProgressCallback ) (int num);
extern int refinerCallbackCounter;

typedef vector< Block::Range > Ranges;
typedef map< unsigned int, Ranges > RangeMap;

// use a multi-map in case have repeated scores
typedef multimap< TScoreType, RefinerAU > RefinedAlignments;
typedef RefinedAlignments::iterator RefinedAlignmentsIt;
typedef RefinedAlignments::const_iterator RefinedAlignmentsCIt;
typedef RefinedAlignments::reverse_iterator RefinedAlignmentsRevIt;
typedef RefinedAlignments::const_reverse_iterator RefinedAlignmentsRevCIt;
typedef RefinedAlignments::value_type RefinedAlignmentsVT;


END_SCOPE(align_refine)

#endif // AR_REFINERDEFS__HPP
