#ifndef CONNECT_SERVICES___NS_CLIENT_WRAPPERS__HPP
#define CONNECT_SERVICES___NS_CLIENT_WRAPPERS__HPP

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

#include <corelib/ncbimtx.hpp>
#include <connect/services/netschedule_api.hpp>


BEGIN_NCBI_SCOPE

class NCBI_XCONNECT_EXPORT INSCWrapper
{
public:
    virtual ~INSCWrapper();

    virtual const string& GetQueueName() const = 0;
    virtual const string& GetClientName() const  = 0;
    virtual const string& GetServiceName() const = 0;

    virtual  bool GetJob(CNetScheduleJob& job,
                         unsigned short udp_port) = 0;
    
    virtual bool WaitJob(CNetScheduleJob& job, 
                         unsigned       wait_time,
                         unsigned short udp_port,
                         CNetScheduleExecuter::EWaitMode wait_mode = 
                         CNetScheduleExecuter::eNoWaitNotification) = 0;

    virtual void PutResult(const CNetScheduleJob& job) = 0;


    virtual bool PutResultGetJob(const CNetScheduleJob& done_job, CNetScheduleJob& new_job) = 0;

    virtual void PutProgressMsg(const CNetScheduleJob& job) = 0;

    virtual void GetProgressMsg(CNetScheduleJob& job) = 0;

    virtual void PutFailure(const CNetScheduleJob& job) = 0;

    virtual CNetScheduleAPI::EJobStatus GetJobStatus(const string& job_key) = 0;

    virtual void ReturnJob(const string& job_key) = 0;

    virtual void SetRunTimeout(const string& job_key, 
                               unsigned time_to_run) = 0;

    virtual void JobDelayExpiration(const string& job_key,
                                    unsigned runtime_inc) = 0;

    virtual void RegisterClient(unsigned short udp_port) = 0;
    virtual  void UnRegisterClient(unsigned short udp_port) = 0;

};

/////////////////////////////////////////////////////////////////////
///
class NCBI_XCONNECT_EXPORT CNSCWrapperShared : public INSCWrapper
{
public:
    CNSCWrapperShared(const CNetScheduleExecuter& ns_client, CFastMutex& mutex) 
        : m_NSClient(ns_client), m_Mutex(mutex) {}

    virtual ~CNSCWrapperShared();


    virtual const string& GetQueueName() const ;

    virtual const string& GetClientName() const;
    virtual const string& GetServiceName() const;

    virtual  bool GetJob(CNetScheduleJob& job,
                         unsigned short udp_port);
    
    virtual bool WaitJob(CNetScheduleJob& job, 
                         unsigned       wait_time,
                         unsigned short udp_port,
                         CNetScheduleExecuter::EWaitMode wait_mode = 
                         CNetScheduleExecuter::eNoWaitNotification);

    virtual void PutResult(const CNetScheduleJob& job);


    virtual bool PutResultGetJob(const CNetScheduleJob& done_job, CNetScheduleJob& new_job);

    virtual void PutProgressMsg(const CNetScheduleJob& job);

    virtual void GetProgressMsg(CNetScheduleJob& job);

    virtual void PutFailure(const CNetScheduleJob& job);

    virtual CNetScheduleAPI::EJobStatus GetJobStatus(const string& job_key);


    virtual void ReturnJob(const string& job_key);

    virtual void SetRunTimeout(const string& job_key, 
                               unsigned time_to_run);

    virtual void JobDelayExpiration(const string& job_key,
                                    unsigned runtime_inc);


    virtual void RegisterClient(unsigned short udp_port);
    virtual  void UnRegisterClient(unsigned short udp_port);
        
private:

    CNetScheduleExecuter m_NSClient;
    CFastMutex&         m_Mutex;
};

/////////////////////////////////////////////////////////////////////
/// 
                                               
/*
class NCBI_XCONNECT_EXPORT CNSCWrapperExclusive : public INSCWrapper
{
public:
    CNSCWrapperExclusive(CNetScheduleExecuAPI* ns_client, CFastMutex& mutex) 
        : m_NSClient(ns_client), m_Mutex(mutex) {}

    virtual ~CNSCWrapperExclusive();


    virtual const string& GetQueueName() const ;

    virtual const string& GetClientName() const;
    virtual string GetConnectionInfo() const;

    virtual  bool GetJob(CNetScheduleJob& job,
                         unsigned short udp_port);
    
    virtual bool WaitJob(CNetScheduleJob& job, 
                         unsigned       wait_time,
                         unsigned short udp_port,
                         CNetScheduleExecuter::EWaitMode wait_mode = 
                         CNetScheduleExecuter::eNoWaitNotification);

    virtual void PutResult(const CNetScheduleJob& job);


    virtual bool PutResultGetJob(const CNetScheduleJob& done_job, CNetScheduleJob& new_job);

    virtual void PutProgressMsg(const CNetScheduleJob& job);

    //    virtual string GetProgressMsg(const string& job_key);

    virtual void PutFailure(const CNetScheduleJob& job);

    virtual CNetScheduleAPI::EJobStatus GetJobStatus(const string& job_key);

    virtual void ReturnJob(const string& job_key);

    virtual void SetRunTimeout(const string& job_key, 
                               unsigned time_to_run);
    virtual void JobDelayExpiration(const string& job_key,
                                    unsigned runtime_inc);


    virtual void RegisterClient(unsigned short udp_port);
    virtual  void UnRegisterClient(unsigned short udp_port);
        
private:

    auto_ptr<CNetScheduleAPI> m_NSClient;
    CFastMutex&                  m_Mutex;

};

*/

END_NCBI_SCOPE


 
/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2006/07/17 14:28:27  ucko
 * Don't declare references (to CFastMutex) to be mutable, as they can't
 * be repointed anyway.
 *
 * Revision 1.5  2006/06/28 16:01:42  didenko
 * Redone job's exlusivity processing
 *
 * Revision 1.4  2006/05/22 18:11:43  didenko
 * Added an option to fail a job if a remote app returns non zore code
 *
 * Revision 1.3  2006/05/10 19:54:21  didenko
 * Added JobDelayExpiration method to CWorkerNodeContext class
 * Added keep_alive_period and max_job_run_time parmerter to the config
 * file of remote_app
 *
 * Revision 1.2  2006/02/27 14:50:20  didenko
 * Redone an implementation of IBlobStorage interface based on NetCache as a plugin
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


#endif // CONNECT_SERVICES___NS_CLIENT_WRAPPERS__HPP
