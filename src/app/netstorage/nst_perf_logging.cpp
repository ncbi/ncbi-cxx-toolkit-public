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
 * Authors:  Sergey Satskiy
 *
 * File Description: NetStorage performance logging
 *
 */


#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/perf_log.hpp>

#include "nst_perf_logging.hpp"



BEGIN_NCBI_SCOPE


void g_DoPerfLogging(const string &  resource,
                     const CNSTPreciseTime &  timing,
                     CRequestStatus::ECode  code)
{
    if (!CPerfLogger::IsON())
        return;

    CPerfLogger         perf_logger(CPerfLogger::eSuspend);
    perf_logger.Adjust(CTimeSpan(timing.tv_sec, timing.tv_nsec));

    // Log the event
    perf_logger.Post(code, resource);
}


END_NCBI_SCOPE

