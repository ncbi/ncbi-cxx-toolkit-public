#ifndef UTIL_THREAD_NONSTOP__HPP
#define UTIL_THREAD_NONSTOP__HPP

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
 * File Description: CThread adaptation for running recurring tasks
 *
 */

#include <corelib/ncbithr.hpp>
#include <corelib/ncbimtx.hpp>


BEGIN_NCBI_SCOPE

/// Adaptation of CThread class repeatedly running some job.
///
/// DoJob() method is getting called until RequestStop
///
class NCBI_XUTIL_EXPORT CThreadNonStop : public CThread
{
public:
    /// Thread runs indefinetely with run_delay intervals
    /// the delay between job executions
    ///
    /// @param run_delay
    ///    Interval in seconds between consecutive job (DoJob) runs.
    /// @param stop_request_poll
    ///    Interval in seconds object polls stop thread flag
    ////   (stop can be scheduled by RequestStop())
    CThreadNonStop(unsigned run_delay,
                   unsigned stop_request_poll = 10);

    /// Return TRUE if thread stop has been requested
    bool IsStopRequested() const;

    /// Schedule thread Stop. Thread does not stop immediately, but rather
    /// waits for the current job to complete and for the next stop flag poll.
    /// Use Join to wait for the actual stop.
    void RequestStop();

protected:
    ~CThreadNonStop() {}

    /// Payload function. Must be overloaded to set the action
    /// This function is called over and over again until stop is requested.
    ///
    /// @sa RequestStop
    virtual void DoJob(void) = 0;

    /// Overload from CThread
    virtual void* Main(void);

private:
    unsigned int           m_RunInterval;
    mutable CSemaphore     m_StopSignal;
};


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2005/03/30 13:41:42  kuznets
 * Use semaphore to stop thread
 *
 * Revision 1.1  2004/10/07 18:00:48  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */

#endif
