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
* ===========================================================================
*/

#ifndef AR_REFINERTRIAL__HPP
#define AR_REFINERTRIAL__HPP

#include <algo/structure/bma_refine/RefinerDefs.hpp>

BEGIN_SCOPE(align_refine)

class CBMARefinerCycle;


// Class for default refiner trial:  one LOO phase and one BE phase per cycle.
class NCBI_BMAREFINE_EXPORT CBMARefinerTrial
{

    static const unsigned int NCYCLES_DEFAULT;
    static bool m_cyclesCreated;

public:

    CBMARefinerTrial(unsigned int nCycles = NCYCLES_DEFAULT, bool looFirst = true, bool verbose = false);

    virtual ~CBMARefinerTrial();

    //  Modified alignment replaces old alignment in passed object.
    //  If final score is 'REFINER_INVALID_SCORE' there was a problem.
    RefinerResultCode DoTrial(AlignmentUtility* au, ostream* details = NULL, TFProgressCallback callback = NULL);

    bool SavedIntermediateAlignments() const {return m_saveIntermediateAlignments;}
    void SaveIntermediateAlignments(bool save = true) {m_saveIntermediateAlignments = save;}

    TScoreType GetInitialScore() const;

    //  Looks for the score corresponding to the highest cycle number 
    //  for which the final AlignmentUtility is non-NULL.
    TScoreType GetFinalScore() const;

    //  The pointers are only useful if m_saveIntermediateAlignments is true.
    const RefinedAlignments& GetResults() const {return m_trialResults;}


    void SetVerbose(bool verbose);
    void SetLOOParams(const LeaveOneOutParams& looParams);
    void SetBEParams(const BlockEditingParams& beParams);
    const LeaveOneOutParams* GetLOOParams() const {return m_loo;}
    const BlockEditingParams* GetBEParams() const {return m_blockEdit;}

    bool IsLOOFirst() const {return m_looFirst;}
    unsigned int NumCycles() const {return m_cycles.size();}
    unsigned int NumCyclesRun() const {return m_trialResults.size();}

protected:

    bool m_verbose;  //  more output written to detailsStream
    bool m_saveIntermediateAlignments;
    bool m_looFirst;

    LeaveOneOutParams*  m_loo;
    BlockEditingParams* m_blockEdit;    

    //  Each cycle's result is saved, indexed by score.
    RefinedAlignments    m_trialResults;

    vector<CBMARefinerCycle*> m_cycles;


    //  Return true only if all cycles and phases therein are created successfully.
    //  This method intended to configure subclasses to get cycles w/ the
    //  proper set of phases.  By default (m_looFirst true):  cycle == one LOO phase followed
    //  by one BE phase.
    virtual bool CreateCycles();

    void CleanUp();

};

END_SCOPE(align_refine)

#endif // AR_REFINERTRIAL__HPP
