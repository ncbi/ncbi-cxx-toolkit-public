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
 *    Individual phase in a single cycle of blocked multiple alignment refinement.
 *    RefinerPhase is intended as a pure virtual base class.
 */



#include <ncbi_pch.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <algo/structure/bma_refine/RefinerPhase.hpp>
#include <algo/structure/bma_refine/AlignRefineScorer.hpp>
#include <algo/structure/bma_refine/BlockEditor.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(struct_util);

BEGIN_SCOPE(align_refine)

CRowSelector* CBMARefinerLOOPhase::m_rowSelector = NULL;


//
//  Misc functions for operating on an AlignmentUtility object 
//  (move these elsewhere eventually!).
//

unsigned int GetNRowsFromAU(const AlignmentUtility* au) {
    unsigned int result = 0;
    if (au && const_cast<AlignmentUtility*>(au)->GetBlockMultipleAlignment()) {
        result = const_cast<AlignmentUtility*>(au)->GetBlockMultipleAlignment()->NRows();
    }
    return result;
}

bool   IsRowPDBInAU(const AlignmentUtility* au, unsigned int row) {
    bool result = false;
    AlignmentUtility* nonConstAU = const_cast<AlignmentUtility*>(au);
    const BlockMultipleAlignment* bma = (nonConstAU) ? nonConstAU->GetBlockMultipleAlignment() : NULL;
    const Sequence* sequence = (bma) ? bma->GetSequenceOfRow(row) : NULL;

    if (sequence) {
        result = sequence->GetPreferredIdentifier().IsPdb();
    }

    return result;
}

string GetSeqIdStringForRowFromAU(const AlignmentUtility* au, unsigned int row) {
    AlignmentUtility* nonConstAU = const_cast<AlignmentUtility*>(au);
    const BlockMultipleAlignment* bma = (nonConstAU) ? nonConstAU->GetBlockMultipleAlignment() : NULL;
    const Sequence* sequence = (bma) ? bma->GetSequenceOfRow(row) : NULL;

    if (sequence) {
        const CSeq_id& id = sequence->GetPreferredIdentifier();
        if (id.IsPdb()) {
            char chain = id.GetPdb().GetChain();
            return "PDB " + id.GetPdb().GetMol().Get() + '_' + chain;
        } else if (id.IsGi()) {
            return "GI " + NStr::IntToString(id.GetGi());
        }
    }

    return "<Non-GI/PDB Sequence Type at row " + NStr::IntToString(row + 1) + '>';
}



//
//  Static base class methods
//

string CBMARefinerPhase::PhaseName() const {
    if (PhaseType() == eRefinerPhaseLOO) {
        return "LOO";
    } else if (PhaseType() == eRefinerPhaseBE) {
        return "BE";
    }
    return "";
}

void CBMARefinerPhase::GetBlockScores(unsigned int index, bool initialScores, vector<TScoreType>& scores) const {

    bool foundIndex = false;
    map<unsigned int, vector<TScoreType> >::const_iterator mapCit;

    scores.clear();
    if (initialScores) {
        mapCit = m_initialBlockScores.find(index);
        foundIndex = (mapCit != m_initialBlockScores.end());
    } else {
        mapCit = m_finalBlockScores.find(index);
        foundIndex = (mapCit != m_finalBlockScores.end());
    }

    if (foundIndex) {
        scores.insert(scores.begin(), mapCit->second.begin(), mapCit->second.end());
    }
}


int CBMARefinerPhase::GetScalarChange(unsigned int index) const {
    int result = m_noScalarChangeValue;
    map<unsigned int, int>::const_iterator cit = m_scalarChangeData.find(index);
    if (cit != m_scalarChangeData.end()) {
        result = cit->second;
    }
    return result;
}

void CBMARefinerPhase::ResetBase() {
    m_initialScore = REFINER_INVALID_SCORE;
    m_finalScore   = REFINER_INVALID_SCORE;
    m_initialBlockScores.clear();
    m_finalBlockScores.clear();
    m_scalarChangeData.clear();
}

//
//  LOO Phase class methods
//

RefinerResultCode CBMARefinerLOOPhase::DoPhase(AlignmentUtility* au, ostream* detailsStream) {

    //  Initialize everything from the base class.
    ResetBase();

    bool recomputeScores = false;
    bool writeDetails = (detailsStream != NULL && m_verbose);
    IOS_BASE::fmtflags initFlags = (detailsStream) ? detailsStream->flags() : cout.flags();


    string message, seqIdStr;
    unsigned int nRowsTried = 0;  //  not counting the master, in case the row selector generates it
    unsigned int tries = 0, scoreSame = 0, scoreDrop = 0, scoreDropAccepted = 0;
    unsigned int sumShifts, shiftOnRow, row;
    TScoreType score = 0, oldScore = 0;
    TScoreType rowScore, beforeLOORowScore;
    vector<unsigned int> blockWidths;
    vector<TScoreType> originalBlockScores;
    map<unsigned int, TScoreType> beforeLOORowScores;
    RowScorer rowScorer;

    vector<unsigned int> rows, froms, tos;

    string noChange = " no score change ", accepted = " accepted ";
    string lnoString = (m_looParams.lno > 1) ? "DoLeaveNOut" : "DoLeaveOneOut";

    const BlockMultipleAlignment* bma;
    BlockMultipleAlignment::UngappedAlignedBlockList alignedBlocks;
    BlockMultipleAlignment::UngappedAlignedBlockList::iterator blockIt;
    Ranges rangesBeforeLOO, rangesAfterLOO;
    map<unsigned int, Ranges> rangesBeforeLOOMap, rangesAfterLOOMap;

    //  In past we tried to accept only score-improving moves; preserve that capability.
    bool acceptAll = true;
    AlignmentUtility* auRollbackCopy = NULL;

    if (!m_looParams.doLOO) {
        return eRefinerResultPhaseSkipped;
    }


    //  Make the row selector if needed.
    if (!m_rowSelector) {
        MakeRowSelector(au, m_looParams, message, 0, true);
        if (writeDetails) {
            (*detailsStream) << message;
        }
    }
    if (!m_rowSelector) return eRefinerResultRowSelectorError;

    //  Set LOO range parameters for the passed alignment.
    //  If can't get new ranges, au is probably corrupted; exit.
    if (!UpdateRanges(au)) {
        return eRefinerResultRangeForRefinementError;
    }

    if (m_shuffleRowsAtStart) {
        if (writeDetails) {
            (*detailsStream) << "    (Reshuffle the row selection order for this LOO phase...)\n";
        }
        m_rowSelector->Shuffle();
    }

    //(*detailsStream) << "starting while loop over rows in DoPhase\n";
    tries = 0;
    sumShifts = 0;
    while (m_rowSelector->HasNext() && au) {

        row = m_rowSelector->GetNext();

        if (row == 0) {
            WARNING_MESSAGE_CL("Cannot run '" << lnoString << "' for the master (row = 0).  Continuing.");
            continue;
        }

        rows.push_back(row);
        froms.push_back(m_looParams.froms[row]);
        tos.push_back(m_looParams.tos[row]);


        seqIdStr = GetSeqIdStringForRowFromAU(au, row);

        score = (TScoreType) rowScorer.ComputeScore(*au);  //GetScore(*au);
        if (nRowsTried == 0) {
            m_initialScore = score;
        }

        rangesBeforeLOO.clear();
        rangesAfterLOO.clear();
        bma = au->GetBlockMultipleAlignment();
        if (bma) {
            bma->GetUngappedAlignedBlocks(&alignedBlocks);
            for (blockIt = alignedBlocks.begin(); blockIt != alignedBlocks.end(); ++blockIt) {
                rangesBeforeLOOMap[row].push_back(*(*blockIt)->GetRangeOfRow(row));
                if (nRowsTried == 0) {
                    blockWidths.push_back((*blockIt)->m_width);
                }
            }
        } else {
            ERROR_MESSAGE_CL("Null BlockMultipleAlignment found when starting " << nRowsTried + 1 << "-th LOO for row " << row + 1 << ".  Exiting LOO phase.");
            return eRefinerResultAlignmentUtilityError;
        }

        ++nRowsTried;

        //  This is inefficient (it regenerates ASN from the internal alignment structure), 
        //  but no other way in the API to roll back when don't accept the new alignment.
        //  Have to use a copy of AlignmentUtility as the Seq_annot reference is wiped out by
        //  the DoLeaveOneOut call.

        beforeLOORowScore = rowScorer.ComputeBlockScores(*au, m_initialBlockScores[row], row);
        beforeLOORowScores[row] = beforeLOORowScore;

        if (!acceptAll) {
            //                beforeLOORowScore = GetScore(*au, row);
            auRollbackCopy = au->Clone();
            if (!auRollbackCopy || !auRollbackCopy->Okay()) {
                WARNING_MESSAGE_CL("Bad rollback copy of alignment was created for " << seqIdStr << " at row " << row+1 << ".\nContinuing.\n\n");
                delete auRollbackCopy;
                continue;
            }
        }
            
        if (!m_verbose) SetDiagPostLevel(eDiag_Error);
        //                cout << "##### row: " << row+1 << " perc " << percentileLOO << "; ext " << extensionLOO << " cut " << cutoffLOO << " from/to " << queryFrom[row] << " " << queryTo[row] << endl;

        //  If LOO failed continue to the next row (rollbacks first, if created a copy earlier)
        //(*detailsStream) << "    about to do LOO for row " << row << endl;
//        if (!au->DoLeaveOneOut(row, m_looParams.blocks, m_looParams.percentile, m_looParams.extension,
//                               m_looParams.cutoff, m_looParams.froms[row], m_looParams.tos[row])) {

        //  Accummulate enough rows...
        if (rows.size() < m_looParams.lno) continue;

        if (!au->DoLeaveNOut(rows, m_looParams.blocks, m_looParams.percentile, m_looParams.extension,
                               m_looParams.cutoff, froms, tos)) {
            if (!acceptAll) {
                delete au;  //  DoLeaveOneOut likely has modified 'au' 
                au = auRollbackCopy;
                ERROR_MESSAGE_CL(lnoString << " failed for " << seqIdStr << " at row " << row+1 << ".\nRollback & continue.");
                continue;
            } else {
                ERROR_MESSAGE_CL(lnoString << " failed during call involving " << seqIdStr << " at row " << row+1 << ".\nTerminating phase at processing of " << nRowsTried << "-th row.");
                return (m_looParams.lno > 1) ? eRefinerResultLeaveNOutExecutionError : eRefinerResultLeaveOneOutExecutionError;
            }

        } else {

            oldScore = score;
            //(*detailsStream) << "    about to do rowScorer.ComputeScore after LOO for row " << row << endl;
            score    = (TScoreType) rowScorer.ComputeScore(*au);  //GetScore(*au);

            for (unsigned int i = 0; i < m_looParams.lno; ++i) {
                row = rows[i];
                (*detailsStream) << "    about to do rowScorer.ComputeBlockScores after LOO for row " << row << endl;
                rowScore = rowScorer.ComputeBlockScores(*au, m_finalBlockScores[row], row);
                seqIdStr = GetSeqIdStringForRowFromAU(au, row);

                if (writeDetails) {
                    (*detailsStream) << lnoString << " for " << seqIdStr << " at row " << row+1 << " (before LOO rowScore = " << beforeLOORowScores[row] << "; ";
                    (*detailsStream) << "after LOO rowScore = " << rowScore << ")" << endl; 
                } else {
                    TRACE_MESSAGE_CL(lnoString << " for " << seqIdStr << " at row " << row+1 << " (before LOO rowScore = " << beforeLOORowScores[row] << ")"); 
                    TRACE_MESSAGE_CL("              (after LOO rowScore = " << rowScore << ")"); 
                }


                if (au && (bma = au->GetBlockMultipleAlignment())) {
                    alignedBlocks.clear();
                    bma->GetUngappedAlignedBlocks(&alignedBlocks);
                    for (blockIt = alignedBlocks.begin(); blockIt != alignedBlocks.end(); ++blockIt) {
                        rangesAfterLOOMap[row].push_back(*(*blockIt)->GetRangeOfRow(row));
                    }
                }

                //  Only add shift to the map if its non-zero.
                shiftOnRow = AnalyzeRowShifts(rangesBeforeLOOMap[row], rangesAfterLOOMap[row], row, detailsStream);
                if (shiftOnRow > 0) {
                    m_scalarChangeData[row] = shiftOnRow;
                    sumShifts += shiftOnRow;
                }

                //  Output individual block scores
                if (writeDetails) {
                    (*detailsStream) << "    Block scores on row " << row+1 << " (row, block number, block size, before LOO, after LOO): " << endl;
                    for (unsigned int bnum = 0; bnum < m_initialBlockScores[row].size(); ++bnum) {
                        detailsStream->setf(IOS_BASE::left, IOS_BASE::adjustfield);
                        (*detailsStream) << "    row " << setw(5) << row+1 << " BLOCK " << setw(4) << bnum+1 << " size " << setw(4) << blockWidths[bnum];

                        detailsStream->setf(IOS_BASE::right, IOS_BASE::adjustfield);
                        (*detailsStream) << " " << setw(7) << m_initialBlockScores[row][bnum] << " " << setw(7) << m_finalBlockScores[row][bnum] << endl;
                    }
                    detailsStream->setf(initFlags, IOS_BASE::adjustfield);
                    (*detailsStream) << endl;
                }

            }

            //  DoLeaveOneOut modifies the underlying alignment.  If the change is not 
            //  accepted, roll back to the pre-LeaveOneOut alignment.
            //  Note:  energy = oldScore - score < 0 if alignment score has improved
            if (acceptAll) {
                if (writeDetails) {
                    string& msg = (score == oldScore) ? noChange : accepted;
//                    (*detailsStream) << "LOO move" << msg << "for " << seqIdStr << " at row " << row+1 << ".  oldScore = " << oldScore << "; new score = " << score;
                    (*detailsStream) << "LOO move" << msg << ".  oldScore = " << oldScore << "; new score = " << score;
                }
                if (score == oldScore) {
                    ++scoreSame;
                } else if (score < oldScore) {
                    ++scoreDrop;
                    ++scoreDropAccepted;
                }

            } else {
                TRACE_MESSAGE_CL(lnoString << " NOT accepted.  oldScore = " << oldScore << "; rejected new score = " << score);
                if (score < oldScore) ++scoreDrop; // use if to not depend on Accept implementation

                delete au;
                au = auRollbackCopy;
                score = oldScore;
            }
		
            tries += m_looParams.lno;
            rows.clear();
            froms.clear();
            tos.clear();
            beforeLOORowScores.clear();
            recomputeScores = false;

        }
    }
    m_finalScore = score;
    return eRefinerResultOK;
}


bool CBMARefinerLOOPhase::UpdateRanges(AlignmentUtility* au) {
    if (!au) return false;

    m_looParams.froms.clear();
    m_looParams.tos.clear();
    TRACE_MESSAGE_CL("Updating ranges for Loo Phase....");
    return GetRangeForRefinement(au->GetBlockMultipleAlignment(), m_looParams.froms, m_looParams.tos);
}

bool CBMARefinerLOOPhase::GetRangeForRefinement(const BlockMultipleAlignment* bma, vector<unsigned int>& froms, vector<unsigned int>& tos) {

    BlockMultipleAlignment::UngappedAlignedBlockList alignedBlocks;
    BlockMultipleAlignment::UngappedAlignedBlockList::iterator blockIt;
    bool result = true;
    bool hasBlocks;
    unsigned int i, queryLen, nAligned, alignmentLen;
    unsigned int nRows   = (bma) ? bma->NRows() : 0;
    int queryFrom, queryTo;

    TRACE_MESSAGE_CL("Range parameters:  -fs = " << m_looParams.fullSequence << "; (N-ext, C-ext) = (" << 
                    m_looParams.nExt << ", " << m_looParams.cExt << ")"); 

    froms.resize(nRows);
    tos.resize(nRows);
    bma->GetUngappedAlignedBlocks(&alignedBlocks);
    hasBlocks = (alignedBlocks.size() > 0);
    alignmentLen = bma->AlignmentWidth();  //  included the unaligned residues

    nAligned = 0;
    for (blockIt = alignedBlocks.begin(); blockIt != alignedBlocks.end(); ++blockIt) {
        nAligned += (*blockIt)->m_width;
    }
    TRACE_MESSAGE_CL("Alignment width = " << alignmentLen << "; N_aligned = " << nAligned); 


    //  Find the footprint to realign within on each sequence 
    //  A positive extension decreases queryFrom and increases queryTo;
    //  a negative extension increases queryFrom and decreases queryTo.

    for (i = 0; i < nRows; ++i) {
        queryLen  = bma->GetSequenceOfRow(i)->Length();
        
        if (hasBlocks && !m_looParams.fullSequence) {  //  footprint-based
            queryFrom = alignedBlocks.front()->GetIndexAt(0, i) - m_looParams.nExt;
            queryTo   = alignedBlocks.back()->GetIndexAt(alignedBlocks.back()->m_width-1, i) + m_looParams.cExt;
            
        } else {                           //  full-sequence-based
            queryFrom = -m_looParams.nExt;
            queryTo   =  queryLen - 1 + m_looParams.cExt;  
        }

        //  Truncate range to actual sequence size.
        if (queryFrom < 0) queryFrom = 0;
        if (queryTo >= (int) queryLen) queryTo = queryLen - 1;

        //  Check for badly defined ranges; set range to full sequence range if fails.
        //  Let the caller decide whether or not to continue.
        if (queryTo < queryFrom || queryTo - queryFrom + 1 < (int) nAligned) {
            ERROR_MESSAGE_CL("Range error in row " << i+1 << ":\n    with N-extension = " << m_looParams.nExt <<
                             " and C-extension = " << m_looParams.cExt << " computed invalid range (from, to) = (" <<
                             queryFrom << ", " << queryTo << ")\n    for a sequence of length " <<
                             queryLen << " and " << nAligned << " aligned residues");
            queryFrom = 0;
            queryTo   = -1;
            result = false;

        }

        froms[i] = (unsigned int) queryFrom;
        tos[i]   = (queryTo >= 0) ? (unsigned int) queryTo : kMax_UInt;
        TRACE_MESSAGE_CL("Range of refinement on row " << i+1 << ":  (queryFrom, queryTo) = (" << 
                        queryFrom+1 << ", " << queryTo+1 << ")"); 
    }

    return result;
}


CRowSelector* CBMARefinerLOOPhase::MakeRowSelector(AlignmentUtility* au, const LeaveOneOutParams& looParams, string& msg, unsigned int nRowsRequested, bool makeUnique) {

    CRowSelector* rowSelector = NULL;
    if (!au || !looParams.doLOO) {
        return rowSelector;
    }

    int extra;
    int nRows = 0;
    string pdbId;

    unsigned int nExtra = looParams.rowsToExclude.size();
    set<unsigned int> rowSet;

    msg.erase();
    nRows = GetNRowsFromAU(au);
    if (nRowsRequested > 0) {
        m_rowSelector = new CRowSelector(nRows, nRowsRequested, makeUnique, looParams.seed);
    } else {
        m_rowSelector = new CRowSelector(au, makeUnique, looParams.seed);
    }

    //  Take care of excluded rows.  
    //  Always exclude the master; conditionally exclude structures.
    if (m_rowSelector) {
        m_rowSelector->ExcludeRow(0);
        if (looParams.fixStructures) {
            for (int i = 1; i < nRows; ++i) {
                if (IsRowPDBInAU(au, i)) {
                    m_rowSelector->ExcludeRow(i);
                    pdbId = GetSeqIdStringForRowFromAU(au, i);
                    TRACE_MESSAGE_CL("    Fixed alignment for structure " << pdbId << "; skip LOO for row " << i+1 << "\n");
                }
            }
        }
        for (unsigned int i = 0; i < nExtra; ++i) {
            extra = looParams.rowsToExclude[i];
            if (extra < nRows && extra > 0) {
                TRACE_MESSAGE_CL("Excluded Row Specification:  Row " << extra+1 << " is excluded from the LOO procedure -- it will affect the PSSM, however.");
                m_rowSelector->ExcludeRow(extra);
            } else if (extra == 0) {
                WARNING_MESSAGE_CL("Redundant Row Specification:  Row 1 (the master) is already excluded from LOO; skipping.");
            } else {
                WARNING_MESSAGE_CL("Row Specification Error:  Row = " << extra+1 << "; must be > 1, <= # rows (" << nRows << "); skipping.");
            }
        }
    } else {
        ERROR_MESSAGE_CL("Error initializing row selector.");
    }

    if (m_rowSelector) {
        msg  = string(72, '=') + "\nRows that can be left out:\n";
        msg += m_rowSelector->PrintSequence(0, 0, true);
        msg += string(72, '=') + "\n\n";
    }

    return m_rowSelector;
}


unsigned int CBMARefinerLOOPhase::AnalyzeRowShifts(const Ranges& before, const Ranges& after, unsigned int row, ostream* details) {

    bool writeDetails = (details != NULL && m_verbose);
    int diffFrom, diffTo;
    double totalNormShift = 0.0;
    unsigned int totalShift = 0;
    unsigned int i, blockLen, nBlocks = before.size();
    if (nBlocks == 0 || nBlocks != after.size()) {
        WARNING_MESSAGE_CL("Cannot analyze row shifts:  unexpected or mismatched ranges (#blocks before " << nBlocks << "; after " << after.size() << ") for row " << row+1);
        nBlocks = 0;
    }

    for (i = 0; i < nBlocks; ++i) {

        if (i == 0 && writeDetails) {
            (*details) << "\n    Shift analysis for row " << row+1 << ":" << endl;
        }

        diffFrom = after[i].from - before[i].from;
        diffTo   = after[i].to - before[i].to;
        blockLen = after[i].to - after[i].from + 1;
        if (diffFrom != diffTo) {
            WARNING_MESSAGE_CL("AnalyzeRowShifts:  ignoring block " << i+1 << " on row " << row+1 << " as N- and C-terminii have different shifts, " << diffFrom << " and " << diffTo);
        } else if (diffFrom != 0) {
            totalShift += abs(diffFrom)*blockLen;
            totalNormShift += abs(diffFrom)/(double) blockLen;
            if (writeDetails) {
                (*details) << "        Row " << row+1 << " block " << i+1 << " of size " << blockLen << " shifts by " << diffFrom << endl;
            }
        }
    }

    if (writeDetails) {
        if (totalShift > 0) {
            (*details) << "    Shift detected on row " << row+1 << ":  block-width-weighted shift = " << totalShift;
            (*details) << "; block-width-normalized shift = " << totalNormShift << endl;
        } else if (nBlocks > 0) {
            (*details) << "    No Shift detected on row " << row+1 << endl;
        } 
        (*details) << endl;
    }

    return totalShift;
}

//
//  Block Edit Phase class methods
//

RefinerResultCode CBMARefinerBlockEditPhase::DoPhase(AlignmentUtility* au, ostream* detailsStream) {

    //  Initialize everything from the base class...
    ResetBase();

    if (!au) {
        return eRefinerResultAlignmentUtilityError;
    } else if (!m_blockEditParams.editBlocks) {
        return eRefinerResultPhaseSkipped;
    }

    bool writeDetails = (detailsStream != NULL && m_verbose);
    IOS_BASE::fmtflags initFlags = (detailsStream) ? detailsStream->flags() : cout.flags();
    if (writeDetails) {
        (*detailsStream) << "    Start block editing phase." << endl << endl;
    }

    TScoreType ext, shrink;
    CBlockedAlignmentEditor blockEd(au);
    ColumnScorer *colScorer, *colScorer2 = NULL, *colScorer3 = NULL;
    BlockBoundaryAlgorithm* algorithm;
    RowScorer rowScorer;

    vector<ExtendableBlock> eb, changedBlocks;
    unsigned nExtendable, nEditable, origNTerm, origCTerm, origWidth, newWidth;
    unsigned int nExtensions = 0, nShrinks = 0;
    unsigned int nChanged = 0;


    m_initialScore = (TScoreType) rowScorer.ComputeScore(*au);  //GetScore(*au);

    ext = m_blockEditParams.extensionThreshold;
    shrink = m_blockEditParams.shrinkageThreshold;
    //  ad hoc hack to get this working...
    if (m_blockEditParams.columnMethod == eCompoundScorer) {
        ext = m_blockEditParams.median;
        shrink = ext;
    }
    colScorer = MakeColumnScorer();
    algorithm = MakeBlockBoundaryAlgorithm(colScorer, ext, shrink);

    if (m_blockEditParams.columnMethod == eInvalidColumnScorerMethod || !colScorer ||
        m_blockEditParams.algMethod == eInvalidBBAMethod || !algorithm) {
        m_finalScore = m_initialScore;
        delete colScorer;
        delete algorithm;
        return eRefinerResultCantDoBlockEditing;
    }

//    detailsStream << "        Initial blocks + maximal extent (PSSM coordinates):" << endl;
//    detailsStream << blockEd.BoundsToString(8) << endl;

    nExtendable = blockEd.GetExtendableBlocks(eb, CBlockedAlignmentEditor::eEither);
    if (m_blockEditParams.algMethod != eSimpleShrink) {

        if (writeDetails) {
            (*detailsStream) << "        Possible block extensions (PSSM coordinates):" << endl;
            for (unsigned j = 0; j < nExtendable; ++j) {
                detailsStream->setf(IOS_BASE::left, IOS_BASE::adjustfield);
                (*detailsStream) << "        BLOCK " << setw(4) << eb[j].blockNum+1 << " [";
                detailsStream->setf(IOS_BASE::right, IOS_BASE::adjustfield);
                (*detailsStream) << setw(4) << eb[j].from << ", " << setw(4) << eb[j].to << "]:  max N-extension = ";

                detailsStream->setf(IOS_BASE::left, IOS_BASE::adjustfield);
                (*detailsStream) << setw(4) << eb[j].nExt << ";  max C-extension = " << setw(4) << eb[j].cExt << endl;
            }
            (*detailsStream) << "        **************************************************\n";
            detailsStream->setf(initFlags, IOS_BASE::adjustfield);

        }
    }

    //  Need to specify eAny if want to allow blocks that have no extensions 
    //  a chance to shrink.
    CBlockedAlignmentEditor::ExtensionLoc eloc = (m_blockEditParams.algMethod == eSimpleExtend) 
                                                            ? CBlockedAlignmentEditor::eEither 
                                                            : CBlockedAlignmentEditor::eAny;
    nEditable = blockEd.GetExtendableBlocks(eb, eloc);
    nChanged  = blockEd.MoveBlockBoundaries(*algorithm, eloc, &changedBlocks);

    //  NOTE:  if have deleted a block, some of the block indices may have been invalidated;
    //         either check after or in MoveBlockBoundaries have to reflect the new
    //         block number they have now or some other scheme.
    if (nChanged > 0) {
        nExtensions = 0;
        nShrinks = 0;
        if (writeDetails) {
            (*detailsStream) << "        Changed blocks (PSSM coordinates):" << endl;
            detailsStream->setf(IOS_BASE::left, IOS_BASE::adjustfield);
        }
        for (unsigned j = 0; j < nChanged; ++j) {
            origNTerm = origCTerm = origWidth = 0;
            newWidth = changedBlocks[j].to - changedBlocks[j].from + 1;
            for (unsigned k = 0; k < nEditable; ++k) {
                if (changedBlocks[j].blockNum == eb[k].blockNum) {
                    origNTerm = eb[k].from;
                    origCTerm = eb[k].to;
                    origWidth = origCTerm - origNTerm + 1;
                    break;
                }
            }
            if (origNTerm > changedBlocks[j].from) {
                nExtensions += origNTerm - changedBlocks[j].from;
            } else {
                nShrinks += changedBlocks[j].from - origNTerm;
            }
            if (origCTerm < changedBlocks[j].to) {
                nExtensions += changedBlocks[j].to - origCTerm;
            } else {
                nShrinks += origCTerm - changedBlocks[j].to;
            }

            if (newWidth != origWidth) {
                m_scalarChangeData[changedBlocks[j].blockNum] = newWidth - origWidth;
            }

            if (writeDetails) {
                (*detailsStream) << "        BLOCK " << setw(4) << changedBlocks[j].blockNum+1 << ":  [";

                detailsStream->setf(IOS_BASE::right, IOS_BASE::adjustfield);
                (*detailsStream) << setw(4) << origNTerm << ", " << setw(4) << origCTerm << "] size ";

                detailsStream->setf(IOS_BASE::left, IOS_BASE::adjustfield);
                (*detailsStream) << setw(4) << origWidth << " ==> [";

                detailsStream->setf(IOS_BASE::right, IOS_BASE::adjustfield);
                (*detailsStream) << setw(4) << changedBlocks[j].from << ", " << setw(4) << changedBlocks[j].to << "] size ";

                detailsStream->setf(IOS_BASE::left, IOS_BASE::adjustfield);
                (*detailsStream) << setw(4) << newWidth << endl;
            }

        }

        if (writeDetails) {
            detailsStream->setf(initFlags, IOS_BASE::adjustfield);
            (*detailsStream) << endl << "        Overall " << nExtensions << " extensions; " << nShrinks << " shrinkages." << endl;
            (*detailsStream) << "        Net change:  " << int(nExtensions - nShrinks) << " columns in " << nChanged << " different blocks." << endl;
            (*detailsStream) << "        **************************************************\n\n";
        }
    } else if (writeDetails) {
        (*detailsStream) << "\n        No Changed blocks." << endl;
        (*detailsStream) << "        **************************************************\n\n";
    }

    //  The original AlignmentUtility object was not modified in the BlockEditor;
    //  it's BMA had been cloned.
    if (m_saveBlockScores) {
        for (unsigned int j = 0; j < au->GetBlockMultipleAlignment()->NRows(); ++j) {
            rowScorer.ComputeBlockScores(*au, m_initialBlockScores[j], j);
        }
    }

    //  Only need to update the AlignmentUtility object if there's been a change.
    if (nChanged > 0) {
        const BlockMultipleAlignment* bma2 = blockEd.GetAlignment();

        au->SetBlockMultipleAlignment(bma2);
        m_finalScore = (TScoreType) rowScorer.ComputeScore(*au);  //GetScore(*au);
        if (m_saveBlockScores && bma2) {
            for (unsigned int j = 0; j < bma2->NRows(); ++j) {
                rowScorer.ComputeBlockScores(*au, m_finalBlockScores[j], j);
            }        
        }

    } else {
        m_finalScore = m_initialScore;
        m_finalBlockScores.clear();
    }

    delete colScorer;
    delete colScorer2;
    delete colScorer3;
    delete algorithm;
    return eRefinerResultOK;
}

ColumnScorer* CBMARefinerBlockEditPhase::MakeColumnScorer() {

    ColumnScorer* colScorer = NULL;
    PssmScoreUsage scoreUsage;

    switch (m_blockEditParams.columnMethod) {
        case eSumOfScores:
            colScorer = new SumOfScoresColumnScorer;
            break;
        case eMedianScore:
            colScorer = new MedianColumnScorer;
            break;
        case ePercentAtOrOverThreshold:
            colScorer = new PercentAtOrOverThresholdColumnScorer(m_blockEditParams.columnScorerThreshold);
            break;
        case ePercentOfWeightOverThreshold:
            scoreUsage = m_blockEditParams.negScoreFraction > 0 ? eUseRawScoreUnderBased : eUseRawScore;
            colScorer = new PercentOfWeightOverThresholdColumnScorer(m_blockEditParams.columnScorerThreshold, scoreUsage);
            break;
        case eCompoundScorer:
            //  ad hoc hack to get this working...
            colScorer = new MedianColumnScorer;
            break;
        case eInfoContent:
        default:
            colScorer = NULL;
            m_blockEditParams.columnMethod = eInvalidColumnScorerMethod;
    }
    return colScorer;
}

BlockBoundaryAlgorithm* CBMARefinerBlockEditPhase::MakeBlockBoundaryAlgorithm(ColumnScorer* colScorer, TScoreType ext, TScoreType shrink) {

    BlockBoundaryAlgorithm* algorithm = NULL;
    ColumnScorer *colScorer2 = NULL, *colScorer3 = NULL;

    if (colScorer) {
        switch (m_blockEditParams.algMethod) {
        case eSimpleExtend:
            algorithm = new SimpleBoundaryExtender(colScorer, ext);
            break;
        case eSimpleShrink:
            algorithm = new SimpleBoundaryShrinker(colScorer, shrink);
            break;
        case eSimpleExtendAndShrink:
            //  need to add a 2nd column scorer -> wait until factory-ize these...
            algorithm = new SimpleBoundaryExtenderAndShrinker(m_blockEditParams.extendFirst, colScorer,
                                                              ext, shrink);
            break;
        case eGreedyExtend:
            algorithm = new GreedyBoundaryExtender(colScorer, ext);
            break;
        default:
            m_blockEditParams.algMethod = eInvalidBBAMethod;
        }

        if (algorithm) {
            algorithm->SetMinBlockSize(m_blockEditParams.minBlockSize);

            //  If using compound scoring, add additional scorers...
            //  ...for now, assume want a vote and a %Weight.  Use common PSSM score threshold in these.
            //  Also use common extension/shrinkage thresholds...
            if (m_blockEditParams.columnMethod == eCompoundScorer) {
                colScorer2 = new PercentAtOrOverThresholdColumnScorer(m_blockEditParams.columnScorerThreshold);
                colScorer3 = new PercentOfWeightOverThresholdColumnScorer(m_blockEditParams.columnScorerThreshold, eUseRawScoreUnderBased);
                if (!colScorer2 || !colScorer3) {
                    delete colScorer2;
                    delete colScorer3;
                    delete algorithm;
                    algorithm = NULL;
                } else {
                    algorithm->AddScorer(colScorer2, 1.0 - m_blockEditParams.negRowsFraction);
                    algorithm->AddScorer(colScorer3, 1.0 - m_blockEditParams.negScoreFraction);
                }
            }
        }

    }
    return algorithm;
}

END_SCOPE(align_refine)

/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2005/09/06 19:08:24  lanczyck
 * add Leave-N-out support to DoPhase
 *
 * Revision 1.4  2005/07/07 22:22:06  lanczyck
 * return a default return value when sequence is neither gi nor pdb
 *
 * Revision 1.3  2005/06/29 00:35:07  ucko
 * Fix GCC 2.95 build errors.
 *
 * Revision 1.2  2005/06/28 14:25:23  lanczyck
 * don't treat cases of skipped phases as errors
 *
 * Revision 1.1  2005/06/28 13:44:23  lanczyck
 * block multiple alignment refiner code from internal/structure/align_refine
 *
 * Revision 1.3  2005/05/26 19:20:36  lanczyck
 * remove INFO level messages so can independently keep/suppress info level messages in struct_util/dp libraries
 *
 * Revision 1.1  2005/05/24 22:31:43  lanczyck
 * initial versions:  app builds but not yet tested
 *
 * ===========================================================================
 */
