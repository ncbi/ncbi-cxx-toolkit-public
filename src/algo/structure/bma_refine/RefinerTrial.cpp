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
 *    Represent a single blocked multiple alignment refinement trial.
 *    Trials can be specialized to execute a specific schedule of phases
 *    per cycle, or may specify cycles with different phase schedules.
 *
 */

#include <ncbi_pch.hpp>

#include <algo/structure/bma_refine/RefinerTrial.hpp>
#include <algo/structure/bma_refine/RefinerCycle.hpp>
#include <algo/structure/bma_refine/AlignRefineScorer.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(struct_util);

BEGIN_SCOPE(align_refine)

const unsigned int CBMARefinerTrial::NCYCLES_DEFAULT = 1;
bool CBMARefinerTrial::m_cyclesCreated = false;

CBMARefinerTrial::CBMARefinerTrial(unsigned int nCycles, bool looFirst, bool verbose) : m_saveIntermediateAlignments(false), m_looFirst(looFirst), m_loo(NULL), m_blockEdit(NULL)
{
    m_cycles.clear();
    if (nCycles > 0) {
        m_cycles.resize(nCycles, NULL);
    }

    m_verbose = verbose;
}

CBMARefinerTrial::~CBMARefinerTrial() {
    CleanUp();
    for (unsigned int i = 0; i < m_cycles.size(); ++i) {
        delete m_cycles[i];
    }
    m_cyclesCreated = false;

    delete m_loo;
    delete m_blockEdit;
}


void CBMARefinerTrial::SetVerbose(bool verbose) {
    m_verbose = verbose;
    for (unsigned int i = 0; i < m_cycles.size(); ++i) {
        if (m_cycles[i]) m_cycles[i]->SetVerbose(verbose);
    }
}



void CBMARefinerTrial::SetLOOParams(const LeaveOneOutParams& looParams) 
{
    if (!m_loo) {
        m_loo = new LeaveOneOutParams();
        if (!m_loo) return;
    }

    *m_loo = looParams;
}

void CBMARefinerTrial::SetBEParams(const BlockEditingParams& beParams) 
{
    if (!m_blockEdit) {
        m_blockEdit = new BlockEditingParams();
        if (!m_blockEdit) return;
    }

    *m_blockEdit = beParams;
}

TScoreType CBMARefinerTrial::GetInitialScore() const {
    return (m_cycles.size() > 0 && m_cycles[0]) ? m_cycles[0]->GetInitialScore() : REFINER_INVALID_SCORE;
}

TScoreType CBMARefinerTrial::GetFinalScore() const {
    unsigned int highestValidFinishedCycle = 0;
    TScoreType finalScore = REFINER_INVALID_SCORE;
    RefinedAlignmentsCIt rcit = m_trialResults.begin(), rcitEnd = m_trialResults.end();
    for (; rcit != rcitEnd && highestValidFinishedCycle < m_cycles.size(); ++rcit) {
        if (rcit->second.au && rcit->second.iteration >= highestValidFinishedCycle) {
            highestValidFinishedCycle = rcit->second.iteration;
            finalScore = rcit->first;
        }
    }
    return finalScore;
}

void CBMARefinerTrial::CleanUp() 
{
    //  Only have ownership of the AlignmentUtility objects if flag is true.
    if (m_saveIntermediateAlignments) {
        for (RefinedAlignmentsIt it = m_trialResults.begin(); it != m_trialResults.end(); ++it) {
            delete it->second.au;
        }
    }
    m_trialResults.clear();

}

RefinerResultCode CBMARefinerTrial::DoTrial(AlignmentUtility* au, ostream* detailsStream, TFProgressCallback callback) {
    bool writeDetails = (detailsStream || m_verbose);
    bool madeChange = true;
    unsigned int nCycles = m_cycles.size();
    TScoreType finalScore = REFINER_INVALID_SCORE;
    RefinerResultCode cycleResult = eRefinerResultOK;
    RowScorer rowScorer;

    BlockMultipleAlignment::UngappedAlignedBlockList alignedBlocks;
    BlockMultipleAlignment::UngappedAlignedBlockList::iterator blockIt;
    vector<unsigned int> blockWidths;
    vector<TScoreType> originalBlockScores, finalBlockScores;

    CleanUp();
    if (!CreateCycles()) {
        return eRefinerResultTrialInitializationError;
    }

    if (writeDetails) {
        //TERSE_INFO_MESSAGE_CL(  about to compute block scores in DoTrial\n");
        rowScorer.ComputeBlockScores(*au, originalBlockScores);
        //TERSE_INFO_MESSAGE_CL(  after computed  block scores in DoTrial\n");

        if (au->GetBlockMultipleAlignment()) {
            au->GetBlockMultipleAlignment()->GetUngappedAlignedBlocks(&alignedBlocks);
            for (blockIt = alignedBlocks.begin(); blockIt != alignedBlocks.end(); ++blockIt) {
                blockWidths.push_back((*blockIt)->m_width);
            }
        }

    }

    //TERSE_INFO_MESSAGE_CL(art cycles loop in DoTrial\n");
    CBMARefinerCycle::ResetCycleId();
    for (unsigned int i = 0; i < nCycles; ++i) {

        //  Stop the trial if no change in previous cycle or some error condition
        if (!m_cycles[i] || !au || cycleResult != eRefinerResultOK || !madeChange) {
            if (!m_cycles[i] || (!au && cycleResult == eRefinerResultOK)) {
                cycleResult = eRefinerResultTrialExecutionError;
            }
            break;
        }

        cycleResult = m_cycles[i]->DoCycle(au, detailsStream, callback);
        if (au && cycleResult == eRefinerResultOK) {
            finalScore = m_cycles[i]->GetFinalScore();
            madeChange = m_cycles[i]->MadeChange();

            //  If saving intermediate alignments, we own it; otherwise, just use
            //  the pointer to signal a successfully completed cycle (we don't own
            //  the object pointed to by 'au').
            if (m_saveIntermediateAlignments) {
                TRACE_MESSAGE_CL("Cycle " << i+1 << " saving intermediate alignments.  Score = " << finalScore << ";  made change = " << madeChange << "\n");
                m_trialResults.insert(RefinedAlignmentsVT(finalScore, RefinerAU(i, au->Clone())));
            } else {
                m_trialResults.insert(RefinedAlignmentsVT(finalScore, RefinerAU(i, au)));
                TRACE_MESSAGE_CL("Cycle " << i+1 << " not saving intermediate alignments.  Score = " << finalScore << "; made change = " << madeChange << "\n");
            }
        } else {
            WARNING_MESSAGE_CL("Cycle " << i+1 << " reports a problem (code " << (int) cycleResult << "); assigning cycle an invalid final score and terminating trial.\n");
            finalScore = REFINER_INVALID_SCORE;
            m_trialResults.insert(RefinedAlignmentsVT(finalScore, RefinerAU(i, NULL)));
        }
        
    }  // end cycles loop

    //  Block scores (summed over rows) for entire trial
    //  ** if block number changes this needs to be fixed!!!!  **
    if (writeDetails && finalScore != REFINER_INVALID_SCORE) {
        IOS_BASE::fmtflags initFlags = (detailsStream) ? detailsStream->flags() : cout.flags();

        rowScorer.ComputeBlockScores(*au, finalBlockScores);
        TERSE_INFO_MESSAGE_CL("Block scores summed over all rows (block number, initial block size, trial start, trial end): ");
        for (unsigned int bnum = 0; bnum < originalBlockScores.size(); ++bnum) {
            if (detailsStream) detailsStream->setf(IOS_BASE::left, IOS_BASE::adjustfield);
            TERSE_INFO_MESSAGE_CL("    BLOCK " << setw(4) << bnum+1 << " size " << setw(4) << blockWidths[bnum] << " "
             << " " << setw(7) << originalBlockScores[bnum] << " " << setw(7) << finalBlockScores[bnum]);

//            if (detailsStream) detailsStream->setf(IOS_BASE::right, IOS_BASE::adjustfield);
//            TERSE_INFO_MESSAGE_CL(" " << setw(7) << originalBlockScores[bnum] << " " << setw(7) << finalBlockScores[bnum]);
        }
        TERSE_INFO_MESSAGE_CL("");
        if (detailsStream) detailsStream->setf(initFlags, IOS_BASE::adjustfield);

    }

    return cycleResult;
}

//  Default implementation of virtual function:  LOO phase + BE phase if looFirst true,
//  BE phase + LOO phase if looFirst false
bool CBMARefinerTrial::CreateCycles() {

//    static bool cyclesCreated = false;

    //  If we've already successfully done this, return.  Otherwise, remove
    //  objects created in failed attempt.
    if (m_cyclesCreated) return true;

    bool result = (m_loo && m_blockEdit);
    CBMARefinerCycle* cycle = NULL;

    for (unsigned int i = 0; i < m_cycles.size() && result; ++i) {
        delete m_cycles[i];
        m_cycles[i] = NULL;
    }
            
    for (unsigned int i = 0; i < m_cycles.size() && result; ++i) {
        cycle = new CBMARefinerCycle();
        if (cycle) {
            cycle->SetVerbose(m_verbose);

            //  Default implementation:  one LOO phase and one BE phase
            if (m_looFirst) {
                if (cycle->AddPhase(*m_loo) && cycle->AddPhase(*m_blockEdit)) {
                    m_cycles[i] = cycle;
                } else {
                    result = false;
                    delete cycle;
                }
            } else {
                if (cycle->AddPhase(*m_blockEdit) && cycle->AddPhase(*m_loo)) {
                    m_cycles[i] = cycle;
                } else {
                    result = false;
                    delete cycle;
                }
            }
        } else {
            result = false;
        }

    }
    m_cyclesCreated = result;
    return result;
}

END_SCOPE(align_refine)
