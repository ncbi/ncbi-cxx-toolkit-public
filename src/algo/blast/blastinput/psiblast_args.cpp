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
#include <ncbi_pch.hpp>
#include <algo/blast/blastinput/psiblast_args.hpp>
#include <algo/blast/api/psiblast_options.hpp>
#include <algo/blast/blastinput/blast_input_aux.hpp>
#include <algo/blast/api/phiblast_prot_options.hpp>
#include <algo/blast/api/version.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(objects);

CPsiBlastAppArgs::CPsiBlastAppArgs()
{
    bool const kQueryIsProtein = true;
    bool const kFilterByDefault = false;
    static const string kProgram("psiblast");
    CRef<IBlastCmdLineArgs> arg;
    arg.Reset(new CProgramDescriptionArgs(kProgram,
                                          "Position-Specific Iterated BLAST"));
    m_Args.push_back(arg);
    m_ClientId = kProgram + " " + CBlastVersion().Print();

    static const char kDefaultTask[] = "psiblast";
    SetTask(kDefaultTask);

    m_BlastDbArgs.Reset(new CBlastDatabaseArgs);
    m_BlastDbArgs->SetIPGFilteringSupport(true);
    arg.Reset(m_BlastDbArgs);
    m_Args.push_back(arg);

    m_StdCmdLineArgs.Reset(new CStdCmdLineArgs);
    arg.Reset(m_StdCmdLineArgs);
    m_Args.push_back(arg);

    arg.Reset(new CGenericSearchArgs(kQueryIsProtein));
    m_Args.push_back(arg);

    arg.Reset(new CFilteringArgs(kQueryIsProtein, kFilterByDefault));
    m_Args.push_back(arg);

    arg.Reset(new CMatrixNameArg);
    m_Args.push_back(arg);

    arg.Reset(new CWordThresholdArg);
    m_Args.push_back(arg);

    m_HspFilteringArgs.Reset(new CHspFilteringArgs);
    arg.Reset(m_HspFilteringArgs);
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

    arg.Reset(new CPssmEngineArgs);
    m_Args.push_back(arg);

    arg.Reset(new CPhiBlastArgs);
    m_Args.push_back(arg);

    m_DebugArgs.Reset(new CDebugArgs);
    arg.Reset(m_DebugArgs);
    m_Args.push_back(arg);
}

CRef<CBlastOptionsHandle>
CPsiBlastAppArgs::x_CreateOptionsHandle(CBlastOptions::EAPILocality locality,
                                        const CArgs& args)
{
    if (args.Exist(kArgPHIPatternFile) && args[kArgPHIPatternFile])
       return CRef<CBlastOptionsHandle>(new CPHIBlastProtOptionsHandle(locality));
    else
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

void
CPsiBlastAppArgs::SetNumberOfIterations(unsigned int num_iters)
{
    m_PsiBlastArgs->SetNumberOfIterations(num_iters);
}

int 
CPsiBlastAppArgs::GetQueryBatchSize() const
{
    bool is_remote = (m_RemoteArgs.NotEmpty() && m_RemoteArgs->ExecuteRemotely());
    return blast::GetQueryBatchSize(ePSIBlast, m_IsUngapped, is_remote);
}

bool
CPsiBlastAppArgs::SaveCheckpoint() const
{
    return m_PsiBlastArgs->RequiresCheckPointOutput();
}

CNcbiOstream*
CPsiBlastAppArgs::GetCheckpointStream()
{
    return m_PsiBlastArgs->GetCheckPointOutputStream();
}

bool
CPsiBlastAppArgs::SaveAsciiPssm() const
{
    return m_PsiBlastArgs->RequiresAsciiPssmOutput();
}

CNcbiOstream*
CPsiBlastAppArgs::GetAsciiPssmStream()
{
    return m_PsiBlastArgs->GetAsciiMatrixOutputStream();
}

bool
CPsiBlastAppArgs::GetSaveLastPssm() const
{
    return m_PsiBlastArgs->GetSaveLastPssm();
}

END_SCOPE(blast)
END_NCBI_SCOPE

