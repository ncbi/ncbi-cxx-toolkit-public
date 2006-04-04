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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Maxim Didenko
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbidiag.hpp>
#include <connect/services/grid_globals.hpp>

BEGIN_NCBI_SCOPE

auto_ptr<CGridGlobals> CGridGlobals::sm_Instance;

CGridGlobals::CGridGlobals()
    : m_ReuseJobObject(false),
      m_ShutdownLevel(CNetScheduleClient::eNoShutdown)
{
}

CGridGlobals::~CGridGlobals() 
{
}
    
/* static */
CGridGlobals& CGridGlobals::GetInstance()
{
    if ( !sm_Instance.get() )
        sm_Instance.reset(new CGridGlobals);
    return *sm_Instance;
}


unsigned int CGridGlobals::GetNewJobNumber()
{
    return (unsigned int)m_JobsStarted.Add(1);
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 6.3  2006/04/04 19:15:02  didenko
 * Added max_failed_jobs parameter to a worker node configuration.
 *
 * Revision 6.2  2006/02/15 19:48:34  didenko
 * Added new optional config parameter "reuse_job_object" which allows reusing
 * IWorkerNodeJob objects in the jobs' threads instead of creating
 * a new object for each job.
 *
 * Revision 6.1  2006/02/15 15:19:03  didenko
 * Implemented an optional possibility for a worker node to have a permanent connection
 * to a NetSchedule server.
 * To expedite the job exchange between a worker node and a NetSchedule server,
 * a call to CNetScheduleClient::PutResult method is replaced to a
 * call to CNetScheduleClient::PutResultGetJob method.
 *
 * ===========================================================================
 */
 
