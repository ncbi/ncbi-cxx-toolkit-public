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
 * Authors:  Denis Vakatov, Vladimir Ivanov
 *
 * File Description:
 *   NCBI C++ API for timing-and-logging
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/perf_log.hpp>
#include <corelib/ncbi_param.hpp>


BEGIN_NCBI_SCOPE


/// Turn on/off performance logging (globally)
// Registry file:
//     [Log]
//     PerfLogging = true/false
// Environment variable:
//     LOG_PerfLogging
//
NCBI_PARAM_DECL(bool, Log, PerfLogging);
NCBI_PARAM_DEF_EX(bool, Log, PerfLogging, false, eParam_NoThread, LOG_PerfLogging);
typedef NCBI_PARAM_TYPE(Log, PerfLogging) TPerfLogging;


//////////////////////////////////////////////////////////////////////////////
//
// CPerfLogger
//

bool CPerfLogger::IsON(void)
{
    return TPerfLogging::GetDefault();
}


void CPerfLogger::SetON(bool enable) {
    TPerfLogging::SetDefault(enable);
}


CDiagContext_Extra CPerfLogger::Post(int         status,
                                     CTempString resource,
                                     CTempString status_msg)
{
    Suspend();
    if ( !x_CheckValidity("Post")  ||  !CPerfLogger::IsON() ) {
        Discard();
        return GetDiagContext().Extra();
    }
    SDiagMessage::TExtraArgs args;
    if ( resource.empty() ) {
        NCBI_THROW(CCoreException, eInvalidArg,
            "CPerfLogger::Log: resource name is not specified");
    }
    args.push_back(SDiagMessage::TExtraArg("resource", resource));
    if ( !status_msg.empty() ) {
        args.push_back(SDiagMessage::TExtraArg("status_msg", status_msg));
    }
    double elapsed = m_StopWatch->Elapsed() + m_Adjustment;
    CDiagContext_Extra extra = g_PostPerf((int)status,
        elapsed < 0.0 ? 0.0 : elapsed, args);
    Discard();
    return extra;
}



//////////////////////////////////////////////////////////////////////////////
//
// CPerfLogGuard
//

void CPerfLogGuard::Post(int status, CTempString status_msg)
{
    // Check a validity
    if ( m_Logger.m_IsDiscarded ) {
        ERR_POST_ONCE(Error << "Post() cannot be done, " \
                      "CPerfLogGuard is already discarded");
        return;
    }
    // Check that logging is enabled to avoid extra 'extra' to be printed
    if ( CPerfLogger::IsON() ) {
        CDiagContext_Extra extra = m_Logger.Post(status, m_Resource, status_msg);
        extra.Print(m_Parameters);
    }
    Discard();
}


END_NCBI_SCOPE
