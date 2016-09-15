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
 * Author: Tom Madden
 *
 */

/** @file kblastp_args.cpp
 * Implementation of the BLASTP command line arguments
 */
#include <ncbi_pch.hpp>
#include <algo/blast/blastinput/kblastp_args.hpp>
#include <algo/blast/api/blast_advprot_options.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <algo/blast/blastinput/blast_input_aux.hpp>
#include <algo/blast/api/version.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(objects);

CKBlastpAppArgs::CKBlastpAppArgs()
{
    CRef<IBlastCmdLineArgs> arg;
    static const string kProgram("kblastp");
    arg.Reset(new CProgramDescriptionArgs(kProgram, "Protein-Protein BLAST"));
    const bool kQueryIsProtein = true;
    bool const kFilterByDefault = false;
    m_Args.push_back(arg);
    m_ClientId = kProgram + " " + CBlastVersion().Print();

    // m_BlastDbArgs.Reset(new CBlastDatabaseArgs);
    // m_BlastDbArgs->SetDatabaseMaskingSupport(false);
    // arg.Reset(m_BlastDbArgs);
    // m_Args.push_back(arg);

    m_StdCmdLineArgs.Reset(new CStdCmdLineArgs);
    arg.Reset(m_StdCmdLineArgs);
    m_Args.push_back(arg);

    arg.Reset(new CGenericSearchArgs(kQueryIsProtein));
    m_Args.push_back(arg);

    // arg.Reset(new CFilteringArgs(kQueryIsProtein, kFilterByDefault));
    // m_Args.push_back(arg);

    m_QueryOptsArgs.Reset(new CQueryOptionsArgs(kQueryIsProtein));
    arg.Reset(m_QueryOptsArgs);
    m_Args.push_back(arg);

    m_FormattingArgs.Reset(new CFormattingArgs);
    arg.Reset(m_FormattingArgs);
    m_Args.push_back(arg);

    m_MTArgs.Reset(new CMTArgs);
    arg.Reset(m_MTArgs);
    m_Args.push_back(arg);

    m_KBlastpArgs.Reset(new CKBlastpArgs);
    arg.Reset(m_KBlastpArgs);
    m_Args.push_back(arg);

    m_DebugArgs.Reset(new CDebugArgs);
    arg.Reset(m_DebugArgs);
    m_Args.push_back(arg);
}

CRef<CBlastOptionsHandle> 
CKBlastpAppArgs::x_CreateOptionsHandle(CBlastOptions::EAPILocality locality, 
                                      const CArgs& args)
{
    return CRef<CBlastOptionsHandle>(new CBlastProteinOptionsHandle(locality));
}

int
CKBlastpAppArgs::GetQueryBatchSize() const
{
    return 10000;
}

END_SCOPE(blast)
END_NCBI_SCOPE

