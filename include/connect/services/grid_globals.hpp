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
class NCBI_XCONNECT_EXPORT CGridGlobals
{
public:
    ~CGridGlobals();
    
    static CGridGlobals& GetInstance();

    int GetNewJobNumber();
    void SetMaxJobsAllowed(int value) { m_MaxJobsAllowed = value; }

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

    int m_JobsStarted;
    int m_MaxJobsAllowed;

    volatile CNetScheduleClient::EShutdownLevel m_ShutdownLevel;

    CGridGlobals(const CGridGlobals&);
    CGridGlobals& operator=(const CGridGlobals&);
    
};

END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
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
