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
*
* ===========================================================================
*/

#ifndef AR_REFINERENGINE__HPP
#define AR_REFINERENGINE__HPP

#include <algo/structure/bma_refine/RefinerDefs.hpp>

BEGIN_SCOPE(align_refine)

class CBMARefinerTrial;

class NCBI_BMAREFINE_EXPORT CBMARefinerEngine
{

    static const unsigned int NTRIALS_DEFAULT;
    static const unsigned int MIN_TRIALS_FOR_CONVERGENCE;
    static const double SCORE_DEV_THRESH_DEFAULT;

public:

    CBMARefinerEngine(unsigned int nCycles, unsigned int nTrials = 1, bool looFirst = true, bool verbose = false, double scoreDeviationThreshold = 0.01);
    CBMARefinerEngine(const LeaveOneOutParams& looParams, const BlockEditingParams& beParams, unsigned int nCycles, unsigned int nTrials = 1, bool looFirst = true, bool verbose = false, double scoreDeviationThreshold = 0.01);
    ~CBMARefinerEngine();

    void SetLOOParams(const LeaveOneOutParams& looParams);
    bool GetLOOParams(LeaveOneOutParams& looParams) const;
    void SetBEParams(const BlockEditingParams& beParams);
    bool GetBEParams(BlockEditingParams& beParams) const;

    //  Returns a copy of the best alignment from the RefinedAlignments data structure.
    AlignmentUtility* GetBestRefinedAlignment() const;

    const RefinedAlignments& GetAllResults() const {return m_perTrialResults;}

    bool IsLNOFirst() const;
    unsigned int NumTrials() const {return m_nTrials;}
    unsigned int NumCycles() const;

    //  The first two wrap calls for the most recent trial.
    //  The third allows to specify a specific trial (initial alignment is same
    //  for all trials) and uses the stored results.  Use fourth to extract more 
    //  trial details.
    TScoreType GetInitialScore() const;
    TScoreType GetFinalScore() const;
    TScoreType GetFinalScore(unsigned int trialId) const;  // zero-based ID
    const CBMARefinerTrial* GetTrial() const {return m_trial;}

    //  For the input alignment, perform a series of refinement trials.
    //  The 'originalAlignment' has NOT been modified!!
    //  **  Any existing data is immediately deleted:  m_originalAlignment 
    //      was cloned from 'originalAlignment', and is owned by the RefinerEngine.
    RefinerResultCode Refine(AlignmentUtility* originalAlignment, ostream* detailsStream = NULL, TFProgressCallback callback = NULL);

    //  For the input alignment, perform a series of refinement trials.
    //  Best final alignment from all trials replaces original alignment in 'cdd'.
    //  **  Any existing data is immediately deleted:  m_originalAlignment 
    //      was cloned from data in 'cdd', and is owned by the RefinerEngine.
    RefinerResultCode Refine(ncbi::objects::CCdd& cdd, ostream* detailsStream = NULL, TFProgressCallback callback = NULL);


private:

    bool m_verbose;
    unsigned int m_nTrials;
    double m_scoreDeviationThreshold;

    //  Intend to use a single trial object; extract any relevant data from
    //  it before moving onto next trial.
    CBMARefinerTrial* m_trial;

    AlignmentUtility*  m_originalAlignment;
    RefinedAlignments  m_perTrialResults;

    //  perform all refinement trials
    RefinerResultCode RunTrials(ostream* detailsStream, TFProgressCallback callback);   

    //  Test if score deviation is smaller than threshold.
    //  Require a minimum # of trials to claim convergence.
    bool IsConverged(string& message) const;

    void Initialize(unsigned int nCycles, unsigned int nTrials, bool looFirst, bool verbose, double scoreDeviationThreshold);
    void CleanUp(bool deleteOriginalAlignment);

};

END_SCOPE(align_refine)

#endif // AR_REFINERENGINE__HPP
