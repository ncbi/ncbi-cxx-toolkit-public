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

/** @file vecscreen_args.cpp
 * Implementation of the VecScreen command line arguments
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] 
    = "$Id";
#endif

#include <ncbi_pch.hpp>
#include <algo/blast/blastinput/vecscreen_args.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(objects);

CVecScreenAppArgs::CVecScreenAppArgs()
{
    CRef<IBlastCmdLineArgs> arg;
    static const string kProgram("vecscreen");
    CRef<CProgramDescriptionArgs> prog_args;
    prog_args.Reset(new CProgramDescriptionArgs(kProgram,
            "Vector contamination screening tool"));
    prog_args->SetVersion(&m_Version);
    arg.Reset(prog_args);
             
    m_Args.push_back(arg);
    m_ClientId = kProgram + " " + m_Version.Print();

    SetTask(kProgram);

    m_BlastDbArgs.Reset(new CVecScreenDatabaseArgs);
    arg.Reset(m_BlastDbArgs);
    m_Args.push_back(arg);

    m_StdCmdLineArgs.Reset(new CStdCmdLineArgs);
    arg.Reset(m_StdCmdLineArgs);
    m_Args.push_back(arg);

    //const bool kQueryIsProtein = false;
    //arg.Reset(new CFilteringArgs(kQueryIsProtein));
    //m_Args.push_back(arg);

    //m_QueryOptsArgs.Reset(new CQueryOptionsArgs(kQueryIsProtein));
    //arg.Reset(m_QueryOptsArgs);
    //m_Args.push_back(arg);

    arg.Reset(new CVecScreenFormattingArgs);
    m_Args.push_back(arg);

    //m_MTArgs.Reset(new CMTArgs);
    //arg.Reset(m_MTArgs);
    //m_Args.push_back(arg);

    //m_RemoteArgs.Reset(new CRemoteArgs);
    //arg.Reset(m_RemoteArgs);
    //m_Args.push_back(arg);

    m_DebugArgs.Reset(new CDebugArgs);
    arg.Reset(m_DebugArgs);
    m_Args.push_back(arg);

    // Remove the search strategy options
    remove(m_Args.begin(), m_Args.end(), CRef<IBlastCmdLineArgs>(&*m_SearchStrategyArgs));
}

CRef<CBlastOptionsHandle> 
CVecScreenAppArgs::x_CreateOptionsHandle(CBlastOptions::EAPILocality locality,
                                      const CArgs& /*args*/)
{
    CRef<CBlastOptionsHandle> retval;
    SetTask("vecscreen");
    retval.Reset(CBlastOptionsFactory::CreateTask(GetTask(), locality));
    _ASSERT(retval.NotEmpty());
    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE

