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
 * Author: Greg Boratyn
 *
 */

/** @file deltablast_args.cpp
 * Implementation of the DELTA-BLAST command line arguments
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif

#include <ncbi_pch.hpp>
#include <algo/blast/blastinput/deltablast_args.hpp>
#include <algo/blast/api/deltablast_options.hpp>
#include <algo/blast/blastinput/blast_input_aux.hpp>
#include <algo/blast/api/version.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(objects);

CDeltaBlastAppArgs::CDeltaBlastAppArgs()
{
     static const string kProgram("deltablast");
     CRef<IBlastCmdLineArgs> arg;
     arg.Reset(new CProgramDescriptionArgs(kProgram, "Domain enhanced lookup "
                                           "time accelarated BLAST"));

     m_Args.push_back(arg);
     
     bool const kQueryIsProtein = true;
     bool const kFilterByDefault = false;
     bool const kIsDeltaBlast = true;
     m_ClientId = kProgram + " " + CBlastVersion().Print();

     static const string kDefaultTask = "deltablast";
     SetTask(kDefaultTask);

     m_BlastDbArgs.Reset(new CBlastDatabaseArgs(false, false, false, true));
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

     arg.Reset(new CCompositionBasedStatsArgs(kIsDeltaBlast));
     m_Args.push_back(arg);

     arg.Reset(new CGapTriggerArgs(kQueryIsProtein));
     m_Args.push_back(arg);

     m_PsiBlastArgs.Reset(new CPsiBlastArgs(CPsiBlastArgs::eProteinDb,
                                            kIsDeltaBlast));
     arg.Reset(m_PsiBlastArgs);
     m_Args.push_back(arg);

     arg.Reset(new CPssmEngineArgs(kIsDeltaBlast));
     m_Args.push_back(arg);

     m_DeltaBlastArgs.Reset(new CDeltaBlastArgs());
     arg.Reset(m_DeltaBlastArgs);
     m_Args.push_back(arg);

     m_DebugArgs.Reset(new CDebugArgs);
     arg.Reset(m_DebugArgs);
     m_Args.push_back(arg);
}

int
CDeltaBlastAppArgs::GetQueryBatchSize(void) const
{
    return blast::GetQueryBatchSize(eDeltaBlast, m_IsUngapped);
}

CRef<CBlastOptionsHandle>
CDeltaBlastAppArgs::x_CreateOptionsHandle(CBlastOptions::EAPILocality locality,
                                        const CArgs& args)
{
    return CRef<CBlastOptionsHandle>(new CDeltaBlastOptionsHandle(locality));
}


END_SCOPE(blast)
END_NCBI_SCOPE

