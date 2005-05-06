#ifndef _GRID_THREAD_CONTEXT_HPP_
#define _GRID_THREAD_CONTEXT_HPP_


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
 *    NetSchedule Worker Node implementation
 */

#include <connect/services/grid_worker.hpp>
#include <util/request_control.hpp>


BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
// 
///@internal
class CGridThreadContext
{
public:
    CGridThreadContext(CWorkerNodeJobContext& job_context);

    CWorkerNodeJobContext& GetJobContext();

    void SetJobContext(CWorkerNodeJobContext& job_context);
    void Reset();

    CNcbiIstream& GetIStream();
    CNcbiOstream& GetOStream();
    void PutProgressMessage(const string& msg);

    void SetJobRunTimeout(unsigned time_to_run);

    bool IsJobCommitted() const;
    void PutResult(int ret_code);
    void ReturnJob();
    void PutFailure(const string& msg);
    bool IsJobCanceled();
   
    IWorkerNodeJob* CreateJob();
private:
    CWorkerNodeJobContext*        m_JobContext;
    auto_ptr<CNetScheduleClient>  m_Reporter;
    auto_ptr<INetScheduleStorage> m_Reader;
    auto_ptr<INetScheduleStorage> m_Writer;
    auto_ptr<INetScheduleStorage> m_ProgressWriter;
    CRequestRateControl           m_RateControl; 

    CGridThreadContext(const CGridThreadContext&);
    CGridThreadContext& operator=(const CGridThreadContext&);
};

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 6.2  2005/05/06 13:08:06  didenko
 * Added check for a job cancelation in the GetShoutdownLevel method
 *
 * Revision 6.1  2005/05/05 15:18:51  didenko
 * Added debugging facility to worker nodes
 *
 * ===========================================================================
 */
 
#endif // _GRID_THREAD_CONTEXT_HPP_
