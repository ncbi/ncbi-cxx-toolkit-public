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
 *    Single cycle run during blocked multiple alignment refinement.  
 *    Consists of one or more phases (represented by concrete classes
 *    deriving from RefinerPhase) run in a user-defined sequence.
 */

#ifndef AR_REFINERPHASE__HPP
#define AR_REFINERPHASE__HPP

#include <algo/structure/bma_refine/RefinerDefs.hpp>
#include <algo/structure/bma_refine/RowSelector.hpp>

BEGIN_SCOPE(align_refine)

class NCBI_BMAREFINE_EXPORT CBMARefinerPhase
{
protected:

    CBMARefinerPhase(bool saveBlockScores = true) : m_verbose(true), m_initialScore(REFINER_INVALID_SCORE), m_finalScore(REFINER_INVALID_SCORE), m_saveBlockScores(saveBlockScores) {};

public:
    enum {
        eRefinerPhaseLOO = 0,
        eRefinerPhaseBE
    };

    virtual ~CBMARefinerPhase() {;}

    virtual int  PhaseType() const = 0;
    virtual bool PhaseSkipped() const {return false;}

    virtual RefinerResultCode DoPhase(AlignmentUtility* au, ostream* detailsStream, TFProgressCallback callback) = 0;

    //  Check if the last invokation of 'DoPhase' modified the alignment detectably.
    //  Used by other objects to check convergence.  Most simply, this will return
    //  true if any change was made *and* recorded in m_scalarChangeData.
    //  Override in subclasses if want apply some significance criteria to changes
    //  made to the alignment.
    virtual bool MadeChange() const {
        return (m_scalarChangeData.size() > 0);
    }


    string PhaseName() const;

    void SetVerbose(bool verbose = true) {m_verbose = verbose;}

    bool IsSavingBlockScores() const { return m_saveBlockScores;}
    void SetSaveBlockScores(bool saveBlockScores) { m_saveBlockScores = saveBlockScores;}

    //  From the given alignment, get the score for a full alignment, or a specific row.

    TScoreType GetScore(bool initialScore) const { 
        return ((initialScore) ? m_initialScore : m_finalScore);
    }

    void GetBlockScores(unsigned int index, bool initialScores, vector<TScoreType>& scores) const;

    int GetScalarChange(unsigned int index) const;


protected:

    bool m_verbose;

    TScoreType m_initialScore;
    TScoreType m_finalScore;

    //  Block scores are only guaranteed to be saved if 'm_saveBlockScores' is true.
    //  Each class instance will have the option to save each block's individual score,
    //  organized as appropriate.  For LOO and BE, the index to the map will be a row 
    //  number, and the vector to contain the block score for each block.  For LOO, the
    //  score is that of the block immediately before and after that particular row 
    //  undergoes LOO.  For BE (block editing), scores are for the initial alignment
    //  and the modified alignment.  If there was no change, the final block scores
    //  map is left empty.
    bool               m_saveBlockScores;
    map<unsigned int, vector<TScoreType> > m_initialBlockScores;
    map<unsigned int, vector<TScoreType> > m_finalBlockScores;

    //  The index and value in this map are completely subclass-specific.
    //  Provide a common way of retrieving some kind of integral metric of the
    //  change observed for each value of an index.
    //  LOO:  index == row number; value = sum of distances blocks are shifted on that row
    //  BE:   index == block number; value = change in length of block
    map<unsigned int, int> m_scalarChangeData;
    
    //  This is a subclass-defined value that indicates no change in the scalar.
    int m_noScalarChangeValue;


    void ResetBase();

};

class NCBI_BMAREFINE_EXPORT CBMARefinerLOOPhase : public CBMARefinerPhase {
public:

    CBMARefinerLOOPhase(LeaveOneOutParams looParams) : CBMARefinerPhase(true), m_looParams(looParams) {
        m_noScalarChangeValue = 0;
    }

    virtual ~CBMARefinerLOOPhase() { 
        delete m_rowSelector;
        m_rowSelector = NULL;
    }

    virtual int  PhaseType() const {return eRefinerPhaseLOO;}
    virtual bool PhaseSkipped() const {return !m_looParams.doLOO;}

    virtual RefinerResultCode DoPhase(AlignmentUtility* au, ostream* detailsStream, TFProgressCallback callback);

    //  Currently, true if any detectable change is recorded.  Override to change.
    //  virtual bool MadeChange() const;

    //  Return the total block-weighted shift for the specified row
    unsigned int AnalyzeRowShifts(const Ranges& before, const Ranges& after, unsigned int row, ostream* details);

    LeaveOneOutParams GetLooParams() const { return m_looParams; }

private:

    //  To support 

    LeaveOneOutParams m_looParams;
    static CRowSelector* m_rowSelector;

    static CRowSelector* MakeRowSelector(AlignmentUtility* au, const LeaveOneOutParams& looParams, string& message, unsigned int nRowsRequested, bool makeUnique);

    bool UpdateRanges(AlignmentUtility* au);

    //  Compute the range over which the refinement algorithm will operate for each row
    //  in the block multiple alignment.  Accesses the command-line arguments -fs, -ex, -nex
    //  and -cex to determine which, if any, extensions should be applied.  Return false
    //  if any row has queryTo < queryFrom, or if the range cannot contain the full alignment,
    //  in which case the range for a full sequence (ignoring extensions) is used as a placeholder.
    bool GetRangeForRefinement(const struct_util::BlockMultipleAlignment* bma, vector<unsigned int>& froms, vector<unsigned int>& tos);


};



class NCBI_BMAREFINE_EXPORT CBMARefinerBlockEditPhase : public CBMARefinerPhase
{
public:

    CBMARefinerBlockEditPhase(BlockEditingParams blockEditParams) : CBMARefinerPhase(false), m_blockEditParams(blockEditParams) {
        m_noScalarChangeValue = 0;
    }

    virtual ~CBMARefinerBlockEditPhase() {};

    virtual int PhaseType() const {return eRefinerPhaseBE;}
    virtual bool PhaseSkipped() const {return !m_blockEditParams.editBlocks;}

    virtual RefinerResultCode DoPhase(AlignmentUtility* au, ostream* detailsStream, TFProgressCallback callback);

    //  Currently, true if any detectable change is recorded.  Override to change.
    //  virtual bool MadeChange() const;

    BlockEditingParams GetBlockEditParams() const { return m_blockEditParams; }

private:

    //  unsigned int m_numBlocksChanged;  // don't need: can deduce from scalardata map in base class
    BlockEditingParams m_blockEditParams;

    ColumnScorer* MakeColumnScorer();
    BlockBoundaryAlgorithm* MakeBlockBoundaryAlgorithm(ColumnScorer* colScorer, TScoreType ext, TScoreType shrink);

};

END_SCOPE(align_refine)

#endif  //  AR_REFINERPHASE__HPP
