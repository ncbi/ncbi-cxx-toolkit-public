#ifndef NETSCHEDULE_QUEUE_CLEAN_THREAD__HPP
#define NETSCHEDULE_QUEUE_CLEAN_THREAD__HPP

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
 * Authors:  Anatoliy Kuznetsov, Victor Joukov
 *
 * File Description: Queue cleaning threads.
 *
 *
 */

#include "background_host.hpp"
#include <util/thread_nonstop.hpp>


BEGIN_NCBI_SCOPE

class CQueueDataBase;


// Purge support
// Used as argument and as return value of the method which checks jobs
// expiration. See the comments to individual members.
struct SPurgeAttributes
{
    unsigned int    scans;      // in: max # of scans to be done
                                // out: # of scans done
    unsigned int    deleted;    // in: max # of jobs to mark for deletion
                                // out: # of jobs marks for deletion
    unsigned int    job_id;     // in: job id to start scan from
                                // out: last scanned job id

    SPurgeAttributes() :
        scans(0), deleted(0), job_id(0)
    {}
};



// Thread class, removes obsolete job records
class CJobQueueCleanerThread : public CThread
{
public:
    CJobQueueCleanerThread(CBackgroundHost &    host,
                           CQueueDataBase &     qdb,
                           unsigned int         sec_delay,
                           unsigned int         nanosec_delay,
                           const bool &         logging);
    ~CJobQueueCleanerThread();

    void RequestStop(void);

protected:
    virtual void *  Main(void);

private:
    void x_DoJob(void);

private:
    CBackgroundHost &   m_Host;
    CQueueDataBase &    m_QueueDB;
    const bool &        m_CleaningLogging;
    unsigned int        m_SecDelay;
    unsigned int        m_NanosecDelay;

private:
    mutable CSemaphore                      m_StopSignal;
    mutable CAtomicCounter_WithAutoInit     m_StopFlag;

private:
    CJobQueueCleanerThread(const CJobQueueCleanerThread&);
    CJobQueueCleanerThread& operator=(const CJobQueueCleanerThread&);
};


// Thread class, watches job execution, reschedules forgotten jobs
class CJobQueueExecutionWatcherThread : public CThread
{
public:
    CJobQueueExecutionWatcherThread(CBackgroundHost &   host,
                                    CQueueDataBase &    qdb,
                                    unsigned int        sec_delay,
                                    unsigned int        nanosec_delay,
                                    const bool &        logging);
    ~CJobQueueExecutionWatcherThread();

    void RequestStop(void);

protected:
    virtual void *  Main(void);

private:
    void x_DoJob(void);

private:
    CBackgroundHost &   m_Host;
    CQueueDataBase &    m_QueueDB;
    const bool &        m_ExecutionLogging;
    unsigned int        m_SecDelay;
    unsigned int        m_NanosecDelay;

private:
    mutable CSemaphore                      m_StopSignal;
    mutable CAtomicCounter_WithAutoInit     m_StopFlag;

private:
    CJobQueueExecutionWatcherThread(const CJobQueueExecutionWatcherThread&);
    CJobQueueExecutionWatcherThread&
                operator=(const CJobQueueExecutionWatcherThread&);
};



END_NCBI_SCOPE

#endif /* NETSCHEDULE_QUEUE_CLEAN_THREAD__HPP */

