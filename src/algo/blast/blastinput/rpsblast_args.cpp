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
 * Author: Jason Papadopoulos
 *
 */

/** @file rpsblast_args.cpp
 * Implementation of the RPSBLAST command line arguments
 */
#include <ncbi_pch.hpp>
#include <algo/blast/blastinput/rpsblast_args.hpp>
#include <algo/blast/api/blast_rps_options.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <algo/blast/blastinput/blast_input_aux.hpp>
#include <algo/blast/api/version.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(objects);

CRPSBlastAppArgs::CRPSBlastAppArgs()
{
    const bool kQueryIsProtein = true;
    const bool kIsRpsBlast = true;
    const bool kIsCBS2and3Supported = false;
    const bool kFilterByDefault = false;
    CRef<IBlastCmdLineArgs> arg;
    static const string kProgram("rpsblast");
    arg.Reset(new CProgramDescriptionArgs(kProgram,
                                          "Reverse Position Specific BLAST"));
    m_Args.push_back(arg);
    m_ClientId = kProgram + " " + CBlastVersion().Print();

    static const char kDefaultTask[] = "rpsblast";
    SetTask(kDefaultTask);

    m_BlastDbArgs.Reset(new CBlastDatabaseArgs(false, kIsRpsBlast));
    arg.Reset(m_BlastDbArgs);
    m_Args.push_back(arg);

    m_StdCmdLineArgs.Reset(new CStdCmdLineArgs);
    arg.Reset(m_StdCmdLineArgs);
    m_Args.push_back(arg);

    arg.Reset(new CGenericSearchArgs(kQueryIsProtein, kIsRpsBlast));
    m_Args.push_back(arg);

    arg.Reset(new CFilteringArgs(kQueryIsProtein, kFilterByDefault));
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

    m_MTArgs.Reset(new CMTArgs(true));
    arg.Reset(m_MTArgs);
    m_Args.push_back(arg);

    m_RemoteArgs.Reset(new CRemoteArgs);
    arg.Reset(m_RemoteArgs);
    m_Args.push_back(arg);

    m_DebugArgs.Reset(new CDebugArgs);
    arg.Reset(m_DebugArgs);
    m_Args.push_back(arg);

    string cbs_opt_zero_descr = "Simplified Composition-based statistics as in"
        " Bioinformatics 15:1000-1011, 1999";
    arg.Reset(new CCompositionBasedStatsArgs(kIsCBS2and3Supported,
                                             kDfltArgCompBasedStatsRPS,
                                             cbs_opt_zero_descr));
    m_Args.push_back(arg);
}

CRef<CBlastOptionsHandle> 
CRPSBlastAppArgs::x_CreateOptionsHandle(CBlastOptions::EAPILocality locality, 
                                      const CArgs& /*args*/)
{
    CRef<CBlastOptionsHandle> retval
        (new CBlastRPSOptionsHandle(locality));
    return retval;
}

int
CRPSBlastAppArgs::GetQueryBatchSize() const
{
    bool is_remote = (m_RemoteArgs.NotEmpty() && m_RemoteArgs->ExecuteRemotely());
    return blast::GetQueryBatchSize(eRPSBlast, m_IsUngapped, is_remote);
}

END_SCOPE(blast)
END_NCBI_SCOPE

