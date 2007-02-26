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
#include "blast_input_aux.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(objects);

#define ARG_REQUIRED_START_IN_QUERY "S"
#define ARG_REQUIRED_END_IN_QUERY "H"
#define ARG_CHECKPOINT "C"
#define ARG_ASCII_MATRIX "Q"
#define ARG_MSA_RESTART "B"
#define ARG_GAP_TRIGGER "N"
// this is how this is represented in PSI-BLAST
#define ARG_COMP_BASED_STATS_PSI "t"    

// Deprecated args (i.e.: no-ops), present only for backwards compatibility
#define ARG_CHECKPOINT_INPUT_TYPE "q"
#define ARG_CHECKPOINT_OUTPUT_TYPE "u"

// Program option for PHI-BLAST (default blastpgp)
#define ARG_PHI_PROGRAM "p"         // not supported
// Number of best hits from a region to keep (default 0)
#define ARG_NUM_BEST_HITS "K"   // not supported
// Hit file for PHI-BLAST (default "hit_file")
#define ARG_PHI_HIT_FILE "k"    // not supported yet
// Cost to decline alignment (disabled when 0) (default 0)
#define ARG_DECLINE2ALIGN "L"   // not supported yet

CPsiBlastAppArgs::CPsiBlastAppArgs()
{
    CRef<IBlastCmdLineArgs> arg;
    arg.Reset(new CProgramDescriptionArgs("psiblast", 
                                          "Position-Specific Iterated BLAST"));
    m_Args.push_back(arg);

    m_StdCmdLineArgs.Reset(new CStdCmdLineArgs);
    arg.Reset(m_StdCmdLineArgs);
    m_Args.push_back(arg);

    m_BlastDbArgs.Reset(new CBlastDatabaseArgs(true));
    arg.Reset(m_BlastDbArgs);
    m_Args.push_back(arg);

    arg.Reset(new CGenericSearchArgs);
    m_Args.push_back(arg);

    arg.Reset(new CMatrixNameArg);
    m_Args.push_back(arg);

    arg.Reset(new CWordThresholdArg);
    m_Args.push_back(arg);

    arg.Reset(new CWindowSizeArg);
    m_Args.push_back(arg);

    m_QueryOptsArgs.Reset(new CQueryOptionsArgs(true));
    arg.Reset(m_QueryOptsArgs);
    m_Args.push_back(arg);

    m_FormattingArgs.Reset(new CFormattingArgs);
    arg.Reset(m_FormattingArgs);
    m_Args.push_back(arg);

    m_MTArgs.Reset(new CMTArgs);
    arg.Reset(m_MTArgs);
    m_Args.push_back(arg);

    arg.Reset(new CCompositionBasedStatsArgs);
    m_Args.push_back(arg);

    arg.Reset(new CGapTriggerArgs);
    m_Args.push_back(arg);

    m_PsiBlastArgs.Reset(new CPsiBlastArgs);
    arg.Reset(m_PsiBlastArgs);
    m_Args.push_back(arg);
}

CArgDescriptions*
CPsiBlastAppArgs::SetCommandLine()
{
    return SetUpCommandLineArguments(m_Args);
}

CRef<CPSIBlastOptionsHandle>
CPsiBlastAppArgs::SetOptions(const CArgs& args)
{
    const CBlastOptions::EAPILocality locality = 
        args[kArgRemote] ? CBlastOptions::eRemote : CBlastOptions::eLocal;
    CRef<CPSIBlastOptionsHandle> retval(new CPSIBlastOptionsHandle(locality));

    CBlastOptions& opt = retval->SetOptions();
    NON_CONST_ITERATE(TBlastCmdLineArgs, arg, m_Args) {
        (*arg)->ExtractAlgorithmOptions(args, opt);
    }

    /*
    if (args[ARG_MATRIX])
        opt.SetMatrixName(args[ARG_MATRIX].AsString().c_str());
        */

    /*
    if (args[ARG_DBSIZE])
        opt.SetDbLength((Int8) args[ARG_DBSIZE].AsDouble());

    if (args[ARG_COMP_BASED_STATS]) {
        s_SetCompositionBasedStats(program, opt, 
                                   args[ARG_COMP_BASED_STATS].AsString(),
                                   args[ARG_SMITH_WATERMAN],
                                   args[ARG_SMITH_WATERMAN].AsBoolean());
    }
        */

    //s_SetWordThreshold(opt, program, args, opt.GetMatrixName());

    return retval;
}

CRef<CBlastDatabaseArgs>
CPsiBlastAppArgs::GetBlastDatabaseArgs() const
{
    return m_BlastDbArgs;
}

CRef<CQueryOptionsArgs>
CPsiBlastAppArgs::GetQueryOptionsArgs() const
{
    return m_QueryOptsArgs;
}

CRef<CFormattingArgs>
CPsiBlastAppArgs::GetFormattingArgs() const
{
    return m_FormattingArgs;
}

size_t
CPsiBlastAppArgs::GetNumThreads() const
{
    return m_MTArgs->GetNumThreads();
}

size_t
CPsiBlastAppArgs::GetNumberOfIterations() const
{
    return m_PsiBlastArgs->GetNumberOfIterations();
}

CNcbiIstream&
CPsiBlastAppArgs::GetInputStream() const
{
    return m_StdCmdLineArgs->GetInputStream();
}

CNcbiOstream&
CPsiBlastAppArgs::GetOutputStream() const
{
    return m_StdCmdLineArgs->GetOutputStream();
}

CRef<CPssmWithParameters>
CPsiBlastAppArgs::GetPssm() const
{
    return m_PsiBlastArgs->GetPssm();
}

int 
CPsiBlastAppArgs::GetQueryBatchSize() const
{
    return blast::GetQueryBatchSize("psiblast");
}

END_SCOPE(blast)
END_NCBI_SCOPE

