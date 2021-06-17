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


CBMARefinerEngine::CBMARefinerEngine(unsigned int nCycles, unsigned int nTrials, bool looFirst, bool verbose, double scoreDeviationThreshold)
{

    Initialize(nCycles, nTrials, looFirst, verbose, scoreDeviationThreshold);
};

CBMARefinerEngine::CBMARefinerEngine(const LeaveOneOutParams& looParams, const BlockEditingParams& beParams, unsigned int nCycles, unsigned int nTrials, bool looFirst, bool verbose, double scoreDeviationThreshold) {

    Initialize(nCycles, nTrials, looFirst, verbose, scoreDeviationThreshold);
    SetLOOParams(looParams);
    SetBEParams(beParams);
}

void CBMARefinerEngine::Initialize(unsigned int nCycles, unsigned int nTrials, bool looFirst, bool verbose, double scoreDeviationThreshold) {
    m_nTrials = (nTrials > 0) ? nTrials : NTRIALS_DEFAULT;
    m_verbose = verbose;
    m_scoreDeviationThreshold = scoreDeviationThreshold;
    if (scoreDeviationThreshold < 0 || scoreDeviationThreshold > 1.0) {
        m_scoreDeviationThreshold = SCORE_DEV_THRESH_DEFAULT;
    }

    m_originalAlignment = NULL;
    m_trial = new CBMARefinerTrial(nCycles, looFirst, verbose);
}

CBMARefinerEngine::~CBMARefinerEngine() {
    CleanUp(true);
    delete m_trial;
}

unsigned int CBMARefinerEngine::NumCycles() const {
 return (m_trial) ? m_trial->NumCycles() : 0;
}

bool CBMARefinerEngine:: IsLNOFirst() const {
 return (m_trial) ? m_trial->IsLOOFirst() : false;
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

    RefinedAlignmentsRevCIt revIt, revEnd;
    AlignmentUtility* tmp = NULL, *bestRefinedAlignment = NULL;
    if (m_perTrialResults.size() > 0) {
        revEnd = m_perTrialResults.rend();
        revIt  = m_perTrialResults.rbegin();
        for (; revIt != revEnd; ++revIt) {
            if (revIt->first != REFINER_INVALID_SCORE && revIt->second.au != NULL) {
                tmp = revIt->second.au;
                break;
            }
        }
        bestRefinedAlignment = (tmp) ? tmp->Clone() : NULL;
    }
    return bestRefinedAlignment;
}

/////////////////////////////////////////////////////////////////////////////
//  Run a series of LOO-based refinement trials 

RefinerResultCode CBMARefinerEngine::Refine(ncbi::objects::CCdd& cdd, ostream* detailsStream, TFProgressCallback callback) {

    bool installedNewAlignment = false;
    RefinedAlignmentsRevCIt revIt, revEnd;
    AlignmentUtility* originalAlignment = new AlignmentUtility(cdd.GetSequences(), cdd.GetSeqannot());
    RefinerResultCode result = Refine(originalAlignment, detailsStream, callback);


    //  Install alignment w/ highest score in the original CD.
    if (m_perTrialResults.size() > 0) {
        if (result == eRefinerResultOK) {
            revEnd = GetAllResults().rend();
            revIt  = GetAllResults().rbegin();
            for (; revIt != revEnd; ++revIt) {
                if (revIt->first != REFINER_INVALID_SCORE && revIt->second.au != NULL) {
                    cdd.SetSeqannot() = revIt->second.au->GetSeqAnnots();
                    installedNewAlignment = true;
                    break;
                }
            }
        }
    }

    if (!installedNewAlignment) {
        result = eRefinerResultNoResults;
        ERROR_MESSAGE_CL("Could not refine alignment using with specified parameters.\nAlignment of input CD unchanged");
    }

    delete originalAlignment;
    return result;
}


RefinerResultCode CBMARefinerEngine::Refine(AlignmentUtility* originalAlignment, ostream* detailsStream, TFProgressCallback callback) {

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

    return RunTrials(detailsStream, callback);
}

   
RefinerResultCode CBMARefinerEngine::RunTrials(ostream* detailsStream, TFProgressCallback callback)
{
    string message;
    bool converged = false;
    RefinerResultCode resultCode;
    TrialStats stats;
    RowScorer rowScorer;

    //  This is the object that undergoes refinement.
    AlignmentUtility* au = NULL; 

    align_refine::refinerCallbackCounter = 0;

    //  Initialize the trial's cycles; 
    //  Required LOO/BE/... parameters need to have been set already.
    if (!m_originalAlignment || !m_trial) {
        return eRefinerResultTrialInitializationError;
    }


    //  Check if we can do the refinement based on CD content and refiner parameters.
    unsigned int nStructs = 0, nRows = (m_originalAlignment) ? m_originalAlignment->GetNRows() : 0;
    for (unsigned int i = 1; i < nRows; ++i) {      //  not checking the master, which can't be refined anyway
        if (m_originalAlignment->IsRowPDB(i)) {
            ++nStructs;
        }
    }

    //  If only have structures, nothing to do if 'fixStructures' is set to be true and block editing is not done.
    //  (Master is not refined, hence nRows - 1)
    if (nStructs == nRows - 1) {
        const LeaveOneOutParams* looParams = m_trial->GetLOOParams();
        const BlockEditingParams* beParams = m_trial->GetBEParams();
        if (looParams && looParams->doLOO && !(beParams && beParams->editBlocks)) {
            if (looParams->fixStructures) 
                return eRefinerResultNoRowsToRefine; 
        }
    }


    //  In GetScore, might reset diagnostic level
    EDiagSev origDiagSev = SetDiagPostLevel((m_verbose) ? eDiag_Info : eDiag_Error);

    LOG_MESSAGE_CL(string(50, '=') + "\nBeginning Alignment Refinement\n");

    //  Loop over trials.
    for (unsigned int i = 0; i < m_nTrials && !converged; ++i) {

        LOG_MESSAGE_CL("\nStart refinement trial " << i+1);

        // create same starting AlignmentUtility class object for trial
        delete au;
        //TERSE_INFO_MESSAGE_CL(  about to clone original in RunTrials\n");
        au = m_originalAlignment->Clone();
        //TERSE_INFO_MESSAGE_CL(  after cloned   original in RunTrials\n");
        if (!au || !au->Okay()) {
            delete au;
            CleanUp(false);
            return eRefinerResultAlignmentUtilityError;
        }

    	resultCode = m_trial->DoTrial(au, detailsStream, callback);

        LOG_MESSAGE_CL("\n    Trial " << i+1 << " results:\n");
        if (m_trial->NumCyclesRun() != m_trial->NumCycles()) {
            LOG_MESSAGE_CL("    (stopped after " << m_trial->NumCyclesRun() << " of " << m_trial->NumCycles() << " cycles)");
        }

        if (au && resultCode == eRefinerResultOK) {
            LOG_MESSAGE_CL("Original alignment score = " << GetInitialScore() << ":  final score = " << GetFinalScore() << "\n");
            m_perTrialResults.insert(RefinedAlignmentsVT(m_trial->GetFinalScore(), RefinerAU(i+1, au->Clone())));
        } else {
            LOG_MESSAGE_CL("Original alignment score = " << GetInitialScore() << ":  Invalid final score\n");
            ERROR_MESSAGE_CL("Problem in trial " << i+1 << ": refiner error code " << (int) resultCode << "\nSkipping to next scheduled trial");
            m_perTrialResults.insert(RefinedAlignmentsVT(REFINER_INVALID_SCORE, RefinerAU(i+1, NULL)));
            continue;
        }

        converged = IsConverged(message);
        LOG_MESSAGE_CL(message);
        LOG_MESSAGE_CL("\nEnd refinement trial " << i+1 << ":\n");

    }

    LOG_MESSAGE_CL("\nAlignment Refinement Completed\n" + string(50, '=') + "\n");

    delete au;

    SetDiagPostLevel(origDiagSev);

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
