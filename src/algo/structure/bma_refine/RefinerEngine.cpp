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
 *    Engine for performing blocked multiple alignment refinement.
 */

#include <ncbi_pch.hpp>

#include <algo/structure/bma_refine/RefinerEngine.hpp>
#include <algo/structure/bma_refine/RefinerTrial.hpp>
#include <algo/structure/bma_refine/AlignRefineScorer.hpp>

BEGIN_SCOPE(align_refine)

const unsigned int CBMARefinerEngine::NTRIALS_DEFAULT = 1;
const unsigned int CBMARefinerEngine::MIN_TRIALS_FOR_CONVERGENCE = 2;
const double CBMARefinerEngine::SCORE_DEV_THRESH_DEFAULT = 0.01;


CBMARefinerEngine::CBMARefinerEngine(unsigned int nCycles, unsigned int nTrials, bool verbose, double scoreDeviationThreshold)
{

    Initialize(nCycles, nTrials, verbose, scoreDeviationThreshold);
};

CBMARefinerEngine::CBMARefinerEngine(const LeaveOneOutParams& looParams, const BlockEditingParams& beParams, unsigned int nCycles, unsigned int nTrials, bool verbose, double scoreDeviationThreshold) {

    Initialize(nCycles, nTrials, verbose, scoreDeviationThreshold);
    SetLOOParams(looParams);
    SetBEParams(beParams);
}

void CBMARefinerEngine::Initialize(unsigned int nCycles, unsigned int nTrials, bool verbose, double scoreDeviationThreshold) {
    m_nTrials = (nTrials > 0) ? nTrials : NTRIALS_DEFAULT;
    m_verbose = verbose;
    m_scoreDeviationThreshold = scoreDeviationThreshold;
    if (scoreDeviationThreshold < 0 || scoreDeviationThreshold > 1.0) {
        m_scoreDeviationThreshold = SCORE_DEV_THRESH_DEFAULT;
    }

    m_originalAlignment = NULL;
    m_trial = new CBMARefinerTrial(nCycles, verbose);
}

CBMARefinerEngine::~CBMARefinerEngine() {
    CleanUp(true);
    delete m_trial;
}

TScoreType CBMARefinerEngine::GetInitialScore() const {
    return (m_trial) ? m_trial->GetInitialScore() : REFINER_INVALID_SCORE;
}

TScoreType CBMARefinerEngine::GetFinalScore() const {
    return (m_trial) ? m_trial->GetFinalScore() : REFINER_INVALID_SCORE;
}

TScoreType CBMARefinerEngine::GetFinalScore(unsigned int trialId) const {
    TScoreType result = REFINER_INVALID_SCORE;
    if (trialId < m_nTrials) {
        RefinedAlignmentsCIt trialIt = m_perTrialResults.begin(), trialEnd = m_perTrialResults.end();
        while (trialIt != trialEnd) {
            if (trialIt->second.iteration == trialId) {
                result = trialIt->first;
                break;
            }
            ++trialIt;
        }
    }
    return result;
}

void CBMARefinerEngine::SetLOOParams(const LeaveOneOutParams& looParams) 
{
    if (m_trial) {
        m_trial->SetLOOParams(looParams);
    }
}

void CBMARefinerEngine::SetBEParams(const BlockEditingParams& beParams) 
{
    if (m_trial) {
        m_trial->SetBEParams(beParams);
    }
}

bool CBMARefinerEngine::GetLOOParams(LeaveOneOutParams& looParams) const
{
    bool result = true;
    if (m_trial && m_trial->GetLOOParams()) {
        looParams = *(m_trial->GetLOOParams());
    } else {
        result = false;
    }
    return result;
}

bool CBMARefinerEngine::GetBEParams(BlockEditingParams& beParams) const
{
    bool result = true;
    if (m_trial && m_trial->GetBEParams()) {
        beParams = *(m_trial->GetBEParams());
    } else {
        result = false;
    }
    return result;
}


AlignmentUtility* CBMARefinerEngine::GetBestRefinedAlignment() const {

    AlignmentUtility* tmp, *bestRefinedAlignment = NULL;
    if (m_perTrialResults.size() > 0) {
        tmp = m_perTrialResults.rbegin()->second.au;
        bestRefinedAlignment = (tmp) ? tmp->Clone() : NULL;
    }
    return bestRefinedAlignment;
}

/////////////////////////////////////////////////////////////////////////////
//  Run a series of LOO-based refinement trials 

RefinerResultCode CBMARefinerEngine::Refine(ncbi::objects::CCdd& cdd, ostream* detailsStream) {

    AlignmentUtility* originalAlignment = new AlignmentUtility(cdd.GetSequences(), cdd.GetSeqannot());
    RefinerResultCode result = Refine(originalAlignment, detailsStream);


    //  Install alignment w/ highest score in the original CD.
    if (m_perTrialResults.size() > 0) {
        if (result == eRefinerResultOK) {
            cdd.SetSeqannot() = m_perTrialResults.rbegin()->second.au->GetSeqAnnots();
        }
    } else {
        result = eRefinerResultNoResults;
    }

    delete originalAlignment;
    return result;
}


RefinerResultCode CBMARefinerEngine::Refine(AlignmentUtility* originalAlignment, ostream* detailsStream) {

    // create AlignmentUtility class object; delete any existing data from prior runs.
    CleanUp(true);
    if (!originalAlignment || !originalAlignment->Okay()) {
        return eRefinerResultBadInputAlignmentUtilityError;
    }

    m_originalAlignment = originalAlignment->Clone();
    if (!m_originalAlignment || !m_originalAlignment->Okay()) {
        CleanUp(true);
        return eRefinerResultAlignmentUtilityError;
    }

    return RunTrials(detailsStream);
}

   
RefinerResultCode CBMARefinerEngine::RunTrials(ostream* detailsStream)
{
    string message;
    bool converged = false;
    RefinerResultCode resultCode;
    TrialStats stats;
    RowScorer rowScorer;

    //  This is the object that undergoes refinement.
    AlignmentUtility* au = NULL;  

    //  Initialize the trial's cycles; 
    //  Required LOO/BE/... parameters need to have been set already.
    if (!m_originalAlignment || !m_trial) {
        return eRefinerResultTrialInitializationError;
    }

    //  In GetScore, might reset diagnostic level
    SetDiagPostLevel((m_verbose) ? eDiag_Info : eDiag_Error);

    //  Loop over trials.
    for (unsigned int i = 0; i < m_nTrials && !converged; ++i) {
        if (detailsStream) {
            (*detailsStream) << "\nStart Trial " << i+1 << endl;
        }

        // create same starting AlignmentUtility class object for trial
        delete au;
        au = m_originalAlignment->Clone();
        if (!au || !au->Okay()) {
            delete au;
            CleanUp(false);
            return eRefinerResultAlignmentUtilityError;
        }

    	resultCode = m_trial->DoTrial(au, detailsStream);


        if (detailsStream) {
            (*detailsStream) << "\nEnd trial " << i+1 << ":\n";

            if (m_trial->NumCyclesRun() != m_trial->NumCycles()) {
                (*detailsStream) << "    (stopped after " << m_trial->NumCyclesRun()+1 << " of " << m_trial->NumCycles() << " cycles)\n";
            }
        }


        if (au && resultCode == eRefinerResultOK) {
            if (detailsStream) {
                (*detailsStream) << "Original alignment score = " << GetInitialScore() << ":  final score = " << GetFinalScore() << "\n";
            }
            m_perTrialResults.insert(RefinedAlignmentsVT(m_trial->GetFinalScore(), RefinerAU(i+1, au->Clone())));
        } else {
            if (detailsStream) {
                (*detailsStream) << "Original alignment score = " << GetInitialScore() << ":  Invalid final score\n";
            }
            ERROR_MESSAGE_CL("Problem in trial " << i+1 << ": returned with error code " << (int) resultCode << "\nSkipping to next scheduled trial.\n");
            m_perTrialResults.insert(RefinedAlignmentsVT(REFINER_INVALID_SCORE, RefinerAU(i+1, NULL)));
            continue;
        }

        converged = IsConverged(message);
        if (detailsStream) {
            (*detailsStream) << message << endl;
        }
    }

    delete au;

    return eRefinerResultOK;
}

TScoreType myAbs(TScoreType x) { 
  return ((x >=0) ? x : -x);
}

bool CBMARefinerEngine::IsConverged(string& message) const {

    bool converged = false;
    unsigned int nTrialsRun = m_perTrialResults.size();

    message.erase();
    if (nTrialsRun < MIN_TRIALS_FOR_CONVERGENCE) return converged;

    RefinedAlignmentsCIt cit = m_perTrialResults.begin(); 
    TScoreType scoreSum = 0;
    TScoreType avgScore, absDeviation = 0;

    for (; cit != m_perTrialResults.end(); ++cit) scoreSum += cit->first;
    avgScore = scoreSum / (TScoreType) nTrialsRun;

    cit = m_perTrialResults.begin(); 
    for (; cit != m_perTrialResults.end(); ++cit) {
        absDeviation += myAbs(cit->first - avgScore);
    }
    absDeviation /= (TScoreType) nTrialsRun;

    message = "      Avg score = " + NStr::DoubleToString(avgScore) + "; avg. absolute deviation = " + NStr::DoubleToString(absDeviation);
    if (absDeviation/avgScore < m_scoreDeviationThreshold) {
        message +=  "\n####  Score converges after " + NStr::UIntToString(nTrialsRun) + " trials!!!.";
        converged = true;
    }

    return converged;
}

void CBMARefinerEngine::CleanUp(bool deleteOriginalAlignment) 
{
    for (RefinedAlignmentsIt it = m_perTrialResults.begin(); it != m_perTrialResults.end(); ++it) {
        delete it->second.au;
    }
    m_perTrialResults.clear();

    if (deleteOriginalAlignment) {
        delete m_originalAlignment;
        m_originalAlignment = NULL;
    }
}


END_SCOPE(align_refine)

/*
 * ===========================================================================
 * $Log: RefinerEngine.cpp,v 
 * Revision 1.1  2005/05/24 22:31:43  lanczyc
 * initial versions:  app builds but not yet tested
 *
 * ===========================================================================
 */
