#ifndef NC_SMNG_THREAD__HPP
#define NC_SMNG_THREAD__HPP

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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description: Session management thread
 *                   
 *
 */

#include <corelib/ncbistd.hpp>
#include <util/thread_nonstop.hpp>

BEGIN_NCBI_SCOPE

class CNetCacheServer;

/// Thread class, takes care about application shutdown if
/// there are no more connected sessions 
///
/// @internal
class CSessionManagementThread : public CThreadNonStop
{
public:
    ///
    /// @param nc_server
    ///    Server reference
    /// @param inact_shut_delay
    ///    Time to stay alive after the last session logs out
    ///    (hopefully new sessions will arrive)
    ///
    CSessionManagementThread(CNetCacheServer& nc_server,
                             unsigned         inact_shut_delay,
                             unsigned         run_delay,
                             unsigned         stop_request_poll = 10);
    ~CSessionManagementThread();

    virtual void DoJob(void);

    /// @returns true if session has been registered,
    ///          false if session management cannot do it (shutdown)
    bool RegisterSession(const string& host, unsigned pid);
    void UnRegisterSession(const string& host, unsigned pid);

private:
    CSessionManagementThread(const CSessionManagementThread&);
    CSessionManagementThread& operator=(const CSessionManagementThread&);
private:
    struct SSessionInfo
    {
        string    host_name;
        unsigned  pid;
        SSessionInfo(const string& host, unsigned pid_)
            : host_name(host), pid(pid_) 
        {}
    };
    typedef vector<SSessionInfo> TSessionList;
private:
    TSessionList::iterator 
        x_FindSession(const string& host, unsigned pid);
private:
    CNetCacheServer& m_NC_Server;
    unsigned         m_InactivityShutdownDelay;    
    time_t           m_LastSessionExitTime;

    CFastMutex       m_Lock;
    bool             m_ShutdownFlag;
    TSessionList     m_SessionList;
};


END_NCBI_SCOPE

#endif
