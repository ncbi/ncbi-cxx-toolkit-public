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

void
CRPSBlastMTArgs::SetArgumentDescriptions(CArgDescriptions& arg_desc)
{
    arg_desc.SetCurrentGroup("Miscellaneous options");
#ifdef NCBI_THREADS
    const int kDfltRpsThreadingMode = 1;
    arg_desc.AddDefaultKey(kArgNumThreads, "int_value",
                           "Number of threads to use in RPS BLAST search:\n "
                           "0 (auto = num of databases)\n "
                           "1 (disable)\n max number of threads = num of databases",
                           CArgDescriptions::eInteger,
                           NStr::IntToString(kDfltRpsThreadingMode));
    arg_desc.SetConstraint(kArgNumThreads, 
                           new CArgAllowValuesGreaterThanOrEqual(0));
    arg_desc.AddDefaultKey(kArgMTMode, "int_value",
                           "Multi-thread mode to use in RPS BLAST search:\n "
                           "0 (auto) split by database vols\n "
                           "1 split by queries",
                            CArgDescriptions::eInteger,
                            NStr::IntToString(0));
       arg_desc.SetConstraint(kArgMTMode,
                              new CArgAllowValuesBetween(0, 1, true));
#endif
    arg_desc.SetCurrentGroup("");
}

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

    m_MTArgs.Reset(new CRPSBlastMTArgs());
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

/// Get the input stream
CNcbiIstream&
CRPSBlastAppArgs::GetInputStream()
{
    return CBlastAppArgs::GetInputStream();
}
/// Get the output stream
CNcbiOstream&
CRPSBlastAppArgs::GetOutputStream()
{
    return CBlastAppArgs::GetOutputStream();
}

/// Get the input stream
CNcbiIstream&
CRPSBlastNodeArgs::GetInputStream()
{
	if ( !m_InputStream ) {
		abort();
	}
	return *m_InputStream;
}
/// Get the output stream
CNcbiOstream&
CRPSBlastNodeArgs::GetOutputStream()
{
	return m_OutputStream;
}

CRPSBlastNodeArgs::CRPSBlastNodeArgs(const string & input)
{
	m_InputStream = new CNcbiIstrstream(input.c_str(), input.length());
}

CRPSBlastNodeArgs::~CRPSBlastNodeArgs()
{
	if (m_InputStream) {
		free(m_InputStream);
		m_InputStream = NULL;
	}
}

int
CRPSBlastNodeArgs::GetQueryBatchSize() const
{
    bool is_remote = (m_RemoteArgs.NotEmpty() && m_RemoteArgs->ExecuteRemotely());
    return blast::GetQueryBatchSize(eRPSBlast, m_IsUngapped, is_remote);
}

CRef<CBlastOptionsHandle>
CRPSBlastNodeArgs::x_CreateOptionsHandle(CBlastOptions::EAPILocality locality,
                                      const CArgs& /*args*/)
{
    CRef<CBlastOptionsHandle> retval
        (new CBlastRPSOptionsHandle(locality));
    return retval;
}
END_SCOPE(blast)
END_NCBI_SCOPE

