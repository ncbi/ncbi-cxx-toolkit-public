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

/** @file tblastn_args.cpp
 * Implementation of the TBLASTN command line arguments
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] 
    = "$Id$";
#endif

#include <ncbi_pch.hpp>
#include <algo/blast/blastinput/tblastn_args.hpp>
#include <algo/blast/api/tblastn_options.hpp>
#include <algo/blast/api/psiblast_options.hpp>
#include <algo/blast/blastinput/blast_input_aux.hpp>
#include <algo/blast/api/version.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(objects);

CTblastnAppArgs::CTblastnAppArgs()
{
    CRef<IBlastCmdLineArgs> arg;
    static const string kProgram("tblastn");
    arg.Reset(new CProgramDescriptionArgs(kProgram,
                                  "Protein Query-Translated Subject BLAST"));
    const bool kQueryIsProtein = true;
    m_Args.push_back(arg);
    m_ClientId = kProgram + " " + CBlastVersion().Print();

    static const string kDefaultTask = "tblastn";
    SetTask(kDefaultTask);

    m_BlastDbArgs.Reset(new CBlastDatabaseArgs);
    m_BlastDbArgs->SetDatabaseMaskingSupport(true);
    arg.Reset(m_BlastDbArgs);
    m_Args.push_back(arg);

    m_StdCmdLineArgs.Reset(new CStdCmdLineArgs);
    arg.Reset(m_StdCmdLineArgs);
    m_Args.push_back(arg);

    arg.Reset(new CGenericSearchArgs(kQueryIsProtein));
    m_Args.push_back(arg);

    arg.Reset(new CGeneticCodeArgs(CGeneticCodeArgs::eDatabase));
    m_Args.push_back(arg);

    arg.Reset(new CFrameShiftArgs);
    m_Args.push_back(arg);

    arg.Reset(new CGappedArgs);
    m_Args.push_back(arg);

    arg.Reset(new CLargestIntronSizeArgs);
    m_Args.push_back(arg);

    // N.B.: this is because the filtering is applied on the translated query
    arg.Reset(new CFilteringArgs(kQueryIsProtein));
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

    m_PsiBlastArgs.Reset(new CPsiBlastArgs(CPsiBlastArgs::eNucleotideDb));
    arg.Reset(m_PsiBlastArgs);
    m_Args.push_back(arg);

    m_DebugArgs.Reset(new CDebugArgs);
    arg.Reset(m_DebugArgs);
    m_Args.push_back(arg);
}

CRef<CBlastOptionsHandle> 
CTblastnAppArgs::x_CreateOptionsHandle(CBlastOptions::EAPILocality locality,
                                      const CArgs& args)
{
    if (args.Exist(kArgPSIInputChkPntFile) && args[kArgPSIInputChkPntFile]) {
       CPSIBlastOptionsHandle * rv(new CPSIBlastOptionsHandle(locality));
       rv->SetPSITblastnDefaults();
       return CRef<CBlastOptionsHandle>(rv);
    }
    else
       return CRef<CBlastOptionsHandle>(new CTBlastnOptionsHandle(locality));
}

CRef<objects::CPssmWithParameters>
CTblastnAppArgs::GetInputPssm() const
{
    return m_PsiBlastArgs->GetInputPssm();
}

void
CTblastnAppArgs::SetInputPssm(CRef<objects::CPssmWithParameters> pssm)
{
    m_PsiBlastArgs->SetInputPssm(pssm);
}

int
CTblastnAppArgs::GetQueryBatchSize() const
{
    return blast::GetQueryBatchSize(eTblastn, m_IsUngapped);
}

END_SCOPE(blast)
END_NCBI_SCOPE

