#ifndef CONNECT_SERVICES___GRID_GLOBALS__HPP
#define CONNECT_SERVICES___GRID_GLOBALS__HPP

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
 * Authors:  Maxim Didenko
 *
 */

#include <corelib/ncbimisc.hpp>

#include <connect/services/netschedule_client.hpp>


BEGIN_NCBI_SCOPE


/// Grid worker global varialbles
///
/// @sa CNetScheduleClient
///

/// @internal
class NCBI_XCONNECT_EXPORT CGridGlobals
{
public:
    ~CGridGlobals();
    
    static CGridGlobals& GetInstance();

    unsigned int GetNewJobNumber();

    bool ReuseJobObject() const { return m_ReuseJobObject; }
    void  SetReuseJobObject(bool value) { m_ReuseJobObject = value; }

    /// Request node shutdown
    void RequestShutdown(CNetScheduleClient::EShutdownLevel level) 
                      { m_ShutdownLevel = level; }


    /// Check if shutdown was requested.
    ///
    CNetScheduleClient::EShutdownLevel GetShutdownLevel(void) 
                      { return m_ShutdownLevel; }

private:

    CGridGlobals();
    static auto_ptr<CGridGlobals> sm_Instance;       

    CAtomicCounter m_JobsStarted;
    bool m_ReuseJobObject;

    volatile CNetScheduleClient::EShutdownLevel m_ShutdownLevel;

    CGridGlobals(const CGridGlobals&);
    CGridGlobals& operator=(const CGridGlobals&);
    
};

END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2006/04/04 19:15:01  didenko
 * Added max_failed_jobs parameter to a worker node configuration.
 *
 * Revision 1.3  2006/02/16 15:39:09  didenko
 * If an instance of a job's class could not be create then the worker node
 * should shutdown itself.
 *
 * Revision 1.2  2006/02/15 20:27:45  didenko
 * Added new optional config parameter "reuse_job_object" which allows
 * reusing IWorkerNodeJob objects in the jobs' threads instead of
 * creating a new object for each job.
 *
 * Revision 1.1  2006/02/15 15:19:03  didenko
 * Implemented an optional possibility for a worker node to have a permanent connection
 * to a NetSchedule server.
 * To expedite the job exchange between a worker node and a NetSchedule server,
 * a call to CNetScheduleClient::PutResult method is replaced to a
 * call to CNetScheduleClient::PutResultGetJob method.
 *
 * ===========================================================================
 */


#endif // CONNECT_SERVICES___GRID_GLOBALS__HPP
