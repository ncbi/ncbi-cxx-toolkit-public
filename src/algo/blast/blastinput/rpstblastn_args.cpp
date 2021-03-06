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

/** @file rpstblastn_args.cpp
 * Implementation of the RPSTBLASTN command line arguments
 */

#include <ncbi_pch.hpp>
#include <algo/blast/blastinput/rpstblastn_args.hpp>
#include <algo/blast/api/rpstblastn_options.hpp>
#include <algo/blast/blastinput/blast_input_aux.hpp>
#include <algo/blast/blastinput/rpsblast_args.hpp>
#include <algo/blast/api/version.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(objects);

CRPSTBlastnAppArgs::CRPSTBlastnAppArgs()
{
    CRef<IBlastCmdLineArgs> arg;
    bool const kFilterByDefault = false;
    static const string kProgram("rpstblastn");
    arg.Reset(new CProgramDescriptionArgs(kProgram,
                               "Translated Reverse Position Specific BLAST"));
    const bool kQueryIsProtein = false;
    const bool kIsRpsBlast = true;
    const bool kIsCBS2and3Supported = false;
    m_Args.push_back(arg);
    m_ClientId = kProgram + " " + CBlastVersion().Print();

    static const char kDefaultTask[] = "rpstblastn";
    SetTask(kDefaultTask);

    m_BlastDbArgs.Reset(new CBlastDatabaseArgs(false, kIsRpsBlast));
    arg.Reset(m_BlastDbArgs);
    m_Args.push_back(arg);

    m_StdCmdLineArgs.Reset(new CStdCmdLineArgs);
    arg.Reset(m_StdCmdLineArgs);
    m_Args.push_back(arg);

    // N.B.: query is not protein because the options are applied on the 
    // translated query
    arg.Reset(new CGenericSearchArgs( !kQueryIsProtein, kIsRpsBlast ));
    m_Args.push_back(arg);

    arg.Reset(new CGeneticCodeArgs(CGeneticCodeArgs::eQuery));
    m_Args.push_back(arg);

    // N.B.: query is not protein because the filtering is applied on the 
    // translated query
    arg.Reset(new CFilteringArgs( !kQueryIsProtein, kFilterByDefault));
    m_Args.push_back(arg);

    arg.Reset(new CWindowSizeArg);
    m_Args.push_back(arg);

    arg.Reset(new CGappedArgs);
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

    arg.Reset(new CCompositionBasedStatsArgs(kIsCBS2and3Supported, kDfltArgCompBasedStatsRPS ));
    m_Args.push_back(arg);

}

CRef<CBlastOptionsHandle> 
CRPSTBlastnAppArgs::x_CreateOptionsHandle(CBlastOptions::EAPILocality locality,
                                      const CArgs& /*args*/)
{
    return CRef<CBlastOptionsHandle>(new CRPSTBlastnOptionsHandle(locality));
}

int
CRPSTBlastnAppArgs::GetQueryBatchSize() const
{
    bool is_remote = (m_RemoteArgs.NotEmpty() && m_RemoteArgs->ExecuteRemotely());
    return blast::GetQueryBatchSize(eRPSTblastn, m_IsUngapped, is_remote);
}

/// Get the input stream
CNcbiIstream&
CRPSTBlastnAppArgs::GetInputStream()
{
    return CBlastAppArgs::GetInputStream();
}
/// Get the output stream
CNcbiOstream&
CRPSTBlastnAppArgs::GetOutputStream()
{
    return CBlastAppArgs::GetOutputStream();
}

/// Get the input stream
CNcbiIstream&
CRPSTBlastnNodeArgs::GetInputStream()
{
	if ( !m_InputStream ) {
		abort();
	}
	return *m_InputStream;
}
/// Get the output stream
CNcbiOstream&
CRPSTBlastnNodeArgs::GetOutputStream()
{
	return m_OutputStream;
}

CRPSTBlastnNodeArgs::CRPSTBlastnNodeArgs(const string & input)
{
	m_InputStream = new CNcbiIstrstream(input);
}

CRPSTBlastnNodeArgs::~CRPSTBlastnNodeArgs()
{
	if (m_InputStream) {
		delete m_InputStream;
		m_InputStream = NULL;
	}
}

int
CRPSTBlastnNodeArgs::GetQueryBatchSize() const
{
    bool is_remote = (m_RemoteArgs.NotEmpty() && m_RemoteArgs->ExecuteRemotely());
    return blast::GetQueryBatchSize(eRPSTblastn, m_IsUngapped, is_remote);
}

CRef<CBlastOptionsHandle>
CRPSTBlastnNodeArgs::x_CreateOptionsHandle(CBlastOptions::EAPILocality locality,
                                      const CArgs& /*args*/)
{
    CRef<CBlastOptionsHandle> retval
        (new CRPSTBlastnOptionsHandle(locality));
    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE

