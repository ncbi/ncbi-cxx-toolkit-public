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
 * Author: Ning Ma
 *
 */

/** @file igblastn_args.cpp
 * Implementation of the IGBLASTN command line arguments
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] 
    = "$Id$";
#endif

#include <ncbi_pch.hpp>
#include <algo/blast/blastinput/igblastn_args.hpp>
#include <algo/blast/api/disc_nucl_options.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <algo/blast/blastinput/blast_input_aux.hpp>
#include <algo/blast/api/version.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(objects);

CIgBlastnAppArgs::CIgBlastnAppArgs()
{
    CRef<IBlastCmdLineArgs> arg;
    static const string kProgram("igblastn");
    arg.Reset(new CProgramDescriptionArgs(kProgram,
                                          "Nucleotide-Nucleotide BLAST for immunoglobulin sequences"));
    const bool kQueryIsProtein = false;
    m_Args.push_back(arg);
    m_ClientId = kProgram + " " + CBlastVersion().Print();

    /*
    static const string kDefaultTask = "blastn";
    SetTask(kDefaultTask);
    set<string> tasks
        (CBlastOptionsFactory::GetTasks(CBlastOptionsFactory::eNuclNucl));
    arg.Reset(new CTaskCmdLineArgs(tasks, kDefaultTask));
    m_Args.push_back(arg); */

    m_IgBlastArgs.Reset(new CIgBlastArgs);
    arg.Reset(m_IgBlastArgs);
    m_Args.push_back(arg);    

    m_BlastDbArgs.Reset(new CBlastDatabaseArgs);
    m_BlastDbArgs->SetDatabaseMaskingSupport(true);
    arg.Reset(m_BlastDbArgs);
    m_Args.push_back(arg);

    m_StdCmdLineArgs.Reset(new CStdCmdLineArgs);
    arg.Reset(m_StdCmdLineArgs);
    m_Args.push_back(arg);

    arg.Reset(new CGenericSearchArgs(kQueryIsProtein, false, true));
    m_Args.push_back(arg);

    arg.Reset(new CNuclArgs);
    m_Args.push_back(arg);

    /*
    arg.Reset(new CDiscontiguousMegablastArgs);
    m_Args.push_back(arg);

    arg.Reset(new CFilteringArgs(kQueryIsProtein));
    m_Args.push_back(arg); */

    arg.Reset(new CGappedArgs);
    m_Args.push_back(arg);

    m_HspFilteringArgs.Reset(new CHspFilteringArgs);
    arg.Reset(m_HspFilteringArgs);
    m_Args.push_back(arg);

    arg.Reset(new CWindowSizeArg);
    m_Args.push_back(arg);

    arg.Reset(new COffDiagonalRangeArg);
    m_Args.push_back(arg);

    /*
    arg.Reset(new CMbIndexArgs);
    m_Args.push_back(arg); */

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

    m_DebugArgs.Reset(new CDebugArgs);
    arg.Reset(m_DebugArgs);
    m_Args.push_back(arg);
}

CRef<CBlastOptionsHandle> 
CIgBlastnAppArgs::x_CreateOptionsHandle(CBlastOptions::EAPILocality locality,
                                      const CArgs& args)
{
    CRef<CBlastOptionsHandle> retval;
    SetTask("blastn");
    retval.Reset(CBlastOptionsFactory::CreateTask(GetTask(), locality));
    _ASSERT(retval.NotEmpty());
    retval->SetEvalueThreshold(1e-15);
    retval->SetFilterString("F");
    // Default options for igblastn
    CBlastOptions & opts = retval->SetOptions();
    opts.SetMatchReward(1);
    opts.SetWordSize(7);
    return retval;
    // Default options for igblastp:   NoCompositionBased...
}

int
CIgBlastnAppArgs::GetQueryBatchSize() const
{
    return blast::GetQueryBatchSize(ProgramNameToEnum(GetTask()), m_IsUngapped);
}

END_SCOPE(blast)
END_NCBI_SCOPE

