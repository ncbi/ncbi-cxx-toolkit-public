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
*       Block multiple alignment refiner application.  
*       (formerly named AlignRefineApp2.cpp)
*      
*
* ===========================================================================
*/

#ifndef BMA_REFINER_APP__HPP
#define BMA_REFINER_APP__HPP

#include <corelib/ncbiapp.hpp>
#include <algo/structure/bma_refine/RefinerDefs.hpp>

USING_NCBI_SCOPE;
BEGIN_SCOPE(align_refine)

// class for standalone application
class CAlignmentRefiner : public ncbi::CNcbiApplication
{

    static const unsigned int N_MAX_TRIALS;
    static const unsigned int N_MAX_CYCLES;
    static const unsigned int N_MAX_ROWS;

public:

//    CAlignmentRefiner() : m_quietMode(false), m_quietDetails(false), m_nTrials(1), m_nCycles(1), m_forcedThreshold(-1), m_scoreDeviationThreshold(0.01)
    CAlignmentRefiner() : m_quietMode(false), m_quietDetails(false), m_nTrials(1), m_nCycles(1), m_scoreDeviationThreshold(0.01)
{
    SetVersion(CVersionInfo(1,2,0, "a Block-based Multiple Alignment Refinement program"));
    };

    ~CAlignmentRefiner() {
    }

    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

private:

    bool          m_quietMode;
    bool          m_quietDetails;
    unsigned int  m_nTrials;
    unsigned int  m_nCycles;

//    double        m_forcedThreshold;
    double        m_scoreDeviationThreshold;  //  if absolute deviation is < this fraction
                                              //  of the absolute score, stop doing more trials.


    LeaveOneOutParams  m_loo;
    BlockEditingParams m_blockEdit;    

    RefinerResultCode ExtractLOOArgs(unsigned int nAlignedBlocks, string& msg);
    RefinerResultCode ExtractBEArgs(unsigned int nAlignedBlocks, string& msg);

    unsigned int GetBlocksToAlign(unsigned int nBlocks, vector<unsigned int>& blocks, string& msg, bool useExtras);

    //  If echoLOO & echoBE are both true or both false, also print 
    //  global refinement parameters
    void EchoSettings(ostream& echoStream, bool echoLOO, bool echoBE);


};

END_SCOPE(align_refine)

#endif // BMA_REFINER_APP__HPP
