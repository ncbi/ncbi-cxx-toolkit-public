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

#include <ncbi_pch.hpp>

#include <algo/structure/bma_refine/RefinerCycle.hpp>
#include <algo/structure/bma_refine/RefinerPhase.hpp>

BEGIN_SCOPE(align_refine)

    unsigned int CBMARefinerCycle::m_cycleId = 0;

CBMARefinerCycle::~CBMARefinerCycle() {
    for (unsigned int i = 0; i < m_phases.size(); ++i) {
        delete m_phases[i];
    }
    m_phases.clear();
}

void CBMARefinerCycle::SetVerbose(bool verbose) {
    m_verbose = verbose;
    for (unsigned int i = 0; i < m_phases.size(); ++i) {
        if (m_phases[i]) m_phases[i]->SetVerbose(verbose);
    }
}

bool CBMARefinerCycle::AddPhase(LeaveOneOutParams looParams) {

    CBMARefinerLOOPhase* looPhase = new CBMARefinerLOOPhase(looParams);
    if (looPhase) {
        looPhase->SetVerbose(m_verbose);
        m_phases.push_back(looPhase);
    }
    return (looPhase != NULL);
}

bool CBMARefinerCycle::AddPhase(BlockEditingParams blockEditParams) {

    CBMARefinerBlockEditPhase* bePhase = new CBMARefinerBlockEditPhase(blockEditParams);
    if (bePhase) {
        bePhase->SetVerbose(m_verbose);
        m_phases.push_back(bePhase);
    }
    return (bePhase != NULL);
}

TScoreType CBMARefinerCycle::GetInitialScore() const {
    TScoreType score = REFINER_INVALID_SCORE;
    if (m_phases.size() > 0 && m_phases[0]) {
        score = m_phases[0]->GetScore(true);
    }
    return score;
}

TScoreType CBMARefinerCycle::GetFinalScore() const {
    TScoreType score = REFINER_INVALID_SCORE;
    int phaseNum = (int) m_phases.size() - 1;
    while (phaseNum >= 0 && m_phases[phaseNum] && m_phases[phaseNum]->PhaseSkipped()) {
        --phaseNum;
    }
    if (phaseNum >= 0 && m_phases[phaseNum]) {
        score = m_phases[phaseNum]->GetScore(false);
    }
    return score;
}

RefinerResultCode CBMARefinerCycle::DoCycle(AlignmentUtility* au, ostream* detailsStream, TFProgressCallback callback) {

    m_nextPhase = 0;
    ++m_cycleId;
    if (!au || m_phases.size() == 0) {
        return eRefinerResultAlignmentUtilityError;
    }

    RefinerResultCode phaseResult = eRefinerResultOK;
    unsigned int nPhases = m_phases.size();
    string phaseName;

    TERSE_INFO_MESSAGE_CL("\n    Start Cycle = " << m_cycleId << "\n");

    for (unsigned int p = 0; p < nPhases; ++p) {
        phaseResult = m_phases[p]->DoPhase(au, detailsStream, callback);
        if (phaseResult != eRefinerResultOK && phaseResult != eRefinerResultPhaseSkipped) {
            return phaseResult;
        }

        if (m_verbose) {
            phaseName = m_phases[p]->PhaseName();
            if (phaseResult == eRefinerResultOK) {
                TERSE_INFO_MESSAGE_CL("    End " << phaseName << " phase for cycle " << m_cycleId);
                TERSE_INFO_MESSAGE_CL("    Phase's initial alignment score = " << m_phases[p]->GetScore(true) 
                                 << "; alignment score after " << phaseName << " phase = " << m_phases[p]->GetScore(false) << "\n");
            } else {
                TERSE_INFO_MESSAGE_CL("    Skipped " << phaseName << " phase for cycle " << m_cycleId << "\n");
            }
        }


        // increment here so if there's an error in some phase, m_nextPhase 
        // left recording the last attempted phase
        ++m_nextPhase;
        if (phaseResult == eRefinerResultOK && IsConverged()) {
            break;
        }
    }

    TERSE_INFO_MESSAGE_CL("    End cycle = " << m_cycleId << ":  Cycle's initial alignment score = " << GetInitialScore() << "; Cycle's ending alignment score = " << GetFinalScore() << "\n");

    //  If made it here, either all phases completed OK or were skipped.
    return eRefinerResultOK;
}


bool CBMARefinerCycle::MadeChange() const {

    bool result = false;
    unsigned int lastPhaseCompleted = m_nextPhase - 1;

    for (unsigned int i = 0; i < lastPhaseCompleted && !result; ++i) {
        if (m_phases[i]->MadeChange()) {
            result = true;
        }
    }
    return result;
}


bool CBMARefinerCycle::IsConverged() const {

    bool result = false;
    bool hasRelevantPhasePending;
    unsigned int nPhases = m_phases.size(), lastPhaseCompleted;
    unsigned int phaseIndexOfLastChange = kMax_UInt;  // no changes

    //  Must have run two or more phases to be able to test for convergence.
    if (m_nextPhase > 1) {

        //  Only could have converged if last completed phase made no change.
        lastPhaseCompleted = m_nextPhase - 1;
        if (!m_phases[lastPhaseCompleted]->MadeChange()) {

            for (unsigned int i = lastPhaseCompleted; phaseIndexOfLastChange == kMax_UInt && i > 0; --i) {
                if (m_phases[i-1]->MadeChange()) {
                    phaseIndexOfLastChange = i-1;
                }
            }

            //  If there's never been a change, we're converged.
            if (phaseIndexOfLastChange == kMax_UInt) {

                result = true;

            } else {

                hasRelevantPhasePending = false;

                //  If no change in a LOO phase, no future LOO phase will either.
                //  However, a future BE phase could make a difference ... unless the
                //  most recently completed BE phase also had no changes and no
                //  intervening phase made a change...
                if (m_phases[lastPhaseCompleted]->PhaseType() == CBMARefinerPhase::eRefinerPhaseLOO) {
                    for (unsigned int i = m_nextPhase; i < nPhases; ++i) {
                        //  there's an upcoming BE phase...
                        if (m_phases[i]->PhaseType() == CBMARefinerPhase::eRefinerPhaseBE) {

                            //  Find most recently completed BE phase, if any;
                            //  did it occur after the phase of last change??
                            hasRelevantPhasePending = true;                        
                            for (unsigned int j = lastPhaseCompleted - 1; j > phaseIndexOfLastChange; --j) {
                                if (m_phases[j]->PhaseType() == CBMARefinerPhase::eRefinerPhaseBE) {
                                    hasRelevantPhasePending = false;
                                }
                            }
                            break;
                        }
                    }
                }
                //  If no change in a BE phase, no future BE phase will either.
                //  However, a future LOO phase could make a difference unless the
                //  most recently completed LOO phase also had no changes and no
                //  intervening phase made a change...
                else if (m_phases[lastPhaseCompleted]->PhaseType() == CBMARefinerPhase::eRefinerPhaseBE) {
                    for (unsigned int i = m_nextPhase; i < nPhases; ++i) {
                        //  there's an upcoming LOO phase...
                        if (m_phases[i]->PhaseType() == CBMARefinerPhase::eRefinerPhaseLOO) {

                            //  Find most recently completed LOO phase, if any;
                            //  did it occur after the phase of last change??
                            hasRelevantPhasePending = true;                        
                            for (unsigned int j = lastPhaseCompleted - 1; j > phaseIndexOfLastChange; --j) {
                                if (m_phases[j]->PhaseType() == CBMARefinerPhase::eRefinerPhaseLOO) {
                                    hasRelevantPhasePending = false;
                                }
                            }
                            break;
                        }
                    }
                }

                result = !hasRelevantPhasePending;
            }
        }
    }
    return result;
}

END_SCOPE(align_refine)
