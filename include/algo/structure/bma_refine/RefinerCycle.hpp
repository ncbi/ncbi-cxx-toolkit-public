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

#ifndef AR_REFINERCYCLE__HPP
#define AR_REFINERCYCLE__HPP

#include <algo/structure/bma_refine/RefinerDefs.hpp>


BEGIN_SCOPE(align_refine)

    class CBMARefinerPhase;

class NCBI_BMAREFINE_EXPORT CBMARefinerCycle
{

public:

    CBMARefinerCycle() : m_verbose(true), m_nextPhase(0) {};

	virtual ~CBMARefinerCycle();

    virtual RefinerResultCode DoCycle(AlignmentUtility* au, ostream* detailsStream, TFProgressCallback callback);

    void SetVerbose(bool verbose = true);

    //  Did any of the phases make a change??
    bool MadeChange() const;

    //  Is the PSSM changing after the (m_nextPhase - 1)th phase??  Can be
    //  unconverged after all phases run.  Each phase individually reports
    //  as to whether it made a change, and can define that however it likes.
    virtual bool IsConverged() const;  
    unsigned int GetNumPhases() const {return m_phases.size();}
    unsigned int GetCurrentPhaseNumber() const {return (m_nextPhase > 0) ? m_nextPhase - 1 : 0;}

    //  Creates phase objects; does not add NULL pointers to internal list.
    bool AddPhase(LeaveOneOutParams looParams);
    bool AddPhase(BlockEditingParams blockEditParams);

    TScoreType GetInitialScore() const;
    TScoreType GetFinalScore() const;

    static unsigned int GetCycleId() { return m_cycleId;}
    static void ResetCycleId() { m_cycleId = 0;}

private:

    static unsigned int m_cycleId;
    bool m_verbose;
    unsigned int m_nextPhase;
    vector< CBMARefinerPhase* > m_phases;

};

END_SCOPE(align_refine)

#endif  //  AR_REFINERCYCLE__HPP
