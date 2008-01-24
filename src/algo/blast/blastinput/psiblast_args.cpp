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
 * Author: Christiam Camacho
 *
 */

/** @file psiblast_args.cpp
 * Implementation of the PSI-BLAST command line arguments
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif

#include <ncbi_pch.hpp>
#include <algo/blast/blastinput/psiblast_args.hpp>
#include <algo/blast/api/psiblast_options.hpp>
#include "blast_input_aux.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(objects);

#ifndef SKIP_DOXYGEN_PROCESSING
#define ARG_CHECKPOINT "C"
#define ARG_ASCII_MATRIX "Q"
#define ARG_MSA_RESTART "B"
#define ARG_GAP_TRIGGER "N"
// this is how this is represented in PSI-BLAST
#define ARG_COMP_BASED_STATS_PSI "t"    

// Program option for PHI-BLAST (default blastpgp)
#define ARG_PHI_PROGRAM "p"         // not supported
// Number of best hits from a region to keep (default 0)
#define ARG_NUM_BEST_HITS "K"   // not supported
#endif

CPsiBlastAppArgs::CPsiBlastAppArgs()
{
    bool const kQueryIsProtein = true;
    CRef<IBlastCmdLineArgs> arg;
    arg.Reset(new CProgramDescriptionArgs("psiblast", 
                                          "Position-Specific Initiated BLAST"));
    m_Args.push_back(arg);

    static const string kDefaultTask = "psiblast";
    SetTask(kDefaultTask);

    m_BlastDbArgs.Reset(new CBlastDatabaseArgs);
    arg.Reset(m_BlastDbArgs);
    m_Args.push_back(arg);

    m_StdCmdLineArgs.Reset(new CStdCmdLineArgs);
    arg.Reset(m_StdCmdLineArgs);
    m_Args.push_back(arg);

    arg.Reset(new CGenericSearchArgs(kQueryIsProtein));
    m_Args.push_back(arg);

    arg.Reset(new CFilteringArgs(kQueryIsProtein));
    m_Args.push_back(arg);

    arg.Reset(new CMatrixNameArg);
    m_Args.push_back(arg);

    arg.Reset(new CWordThresholdArg);
    m_Args.push_back(arg);

    arg.Reset(new CCullingArgs);
    m_Args.push_back(arg);

    arg.Reset(new CWindowSizeArg);
    m_Args.push_back(arg);

    m_QueryOptsArgs.Reset(new CQueryOptionsArgs(kQueryIsProtein));
    arg.Reset(m_QueryOptsArgs);
    m_Args.push_back(arg);

    m_FormattingArgs.Reset(new CFormattingArgs);
    arg.Reset(m_FormattingArgs);
    m_Args.push_back(arg);

    m_MTArgs.Reset(new CMTArgs);
    arg.Reset(m_MTArgs);
    m_Args.push_back(arg);

    m_RemoteArgs.Reset(new CRemoteArgs);
    arg.Reset(m_RemoteArgs);
    m_Args.push_back(arg);

    arg.Reset(new CCompositionBasedStatsArgs);
    m_Args.push_back(arg);

    arg.Reset(new CGapTriggerArgs(kQueryIsProtein));
    m_Args.push_back(arg);

    m_PsiBlastArgs.Reset(new CPsiBlastArgs(CPsiBlastArgs::eProteinDb));
    arg.Reset(m_PsiBlastArgs);
    m_Args.push_back(arg);

    arg.Reset(new CPhiBlastArgs);
    m_Args.push_back(arg);

    m_DebugArgs.Reset(new CDebugArgs);
    arg.Reset(m_DebugArgs);
    m_Args.push_back(arg);
}

CRef<CBlastOptionsHandle>
CPsiBlastAppArgs::x_CreateOptionsHandle(CBlastOptions::EAPILocality locality,
                                        const CArgs& /*args*/)
{
    return CRef<CBlastOptionsHandle>(new CPSIBlastOptionsHandle(locality));
}

size_t
CPsiBlastAppArgs::GetNumberOfIterations() const
{
    return m_PsiBlastArgs->GetNumberOfIterations();
}

CRef<objects::CPssmWithParameters>
CPsiBlastAppArgs::GetInputPssm() const
{
    return m_PsiBlastArgs->GetInputPssm();
}

void
CPsiBlastAppArgs::SetInputPssm(CRef<objects::CPssmWithParameters> pssm)
{
    m_PsiBlastArgs->SetInputPssm(pssm);
}

int 
CPsiBlastAppArgs::GetQueryBatchSize() const
{
    return blast::GetQueryBatchSize(ePSIBlast);
}

bool
CPsiBlastAppArgs::SaveCheckpoint() const
{
    return static_cast<bool>(m_PsiBlastArgs->GetCheckPointOutputStream() != 0);
}

CNcbiOstream*
CPsiBlastAppArgs::GetCheckpointStream() const
{
    return m_PsiBlastArgs->GetCheckPointOutputStream();
}

END_SCOPE(blast)
END_NCBI_SCOPE

