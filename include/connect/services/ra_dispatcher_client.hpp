#ifndef CONNECT_SERVICES__RA_DISPATCHER_CLIENT_HPP
#define CONNECT_SERVICES__RA_DISPATCHER_CLIENT_HPP

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
 * Authors:  Maxim Didneko,
 *
 * File Description:
 *
 */

#include <connect/ncbi_types.h>
#include <connect/services/remote_app_sb.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbiexpt.hpp>



BEGIN_NCBI_SCOPE

class IBlobStorageFactory;

class NCBI_XCONNECT_EXPORT CRADispatcherClient
{
public:
    enum ECmdId {
        eNewJobCmd    = 0,
        eGetStatusCmd,
        eCancelJobCmd
    };

    enum EResId {
        eOkRes     = 0,
        eErrorRes,
        eJobStatusRes
    };

    enum EJobState {
        eSendingRequest,
        ePending,           ///< Waiting for execution
        eRunning,           ///< Running on a worker node
        eRescheduled,       ///< Returned back to the queue(to be rescheduled)
        eReceivingResult,
        eReady              ///< Job is ready (computed successfully)
    };


    CRADispatcherClient(IBlobStorageFactory&, CNcbiIostream&);
    ~CRADispatcherClient();

    /// Start new job.
    /// Throw exception if at least one of the following is true:
    ///   - there is already a job being processed, with no failures so far
    ///   - there is a job in "eReady" status for which GetResult() has not been
    ///     called at least once
    ///   - an unrecoverable error has occured in strating the new job
    void StartJob(CRemoteAppRequestSB&);

    /// Check the job's status
    /// Throw an exception if any error acquires
    EJobState GetJobState();

    /// Get the job's result
    /// Throw an exception if the result is not ready yet.
    CRemoteAppResultSB& GetResult();

    /// Release request and result objects, if any.
    /// Cancel current job, if any.
    void Reset();

    //    void SetTimeout(const STimeout* timeout = NULL);

private:
    IBlobStorageFactory&      m_Factory;
    CNcbiIostream&            m_Stream;
    CRef<CRemoteAppRequestSB> m_Request;
    CRef<CRemoteAppResultSB>  m_Result;
    bool   m_WasGetResultCalled;
    string m_Jid;

    EJobState x_ProcessResponse();

    EResId     x_ReadResponseId();
private:
    CRADispatcherClient(const CRADispatcherClient&);
    CRADispatcherClient& operator=(const CRADispatcherClient&);
};


////////////////////////////////////////////////////////////////////////////////////
/// Remote Application Dispatcher Client  internal exception
///
class CRADispatcherClientException : public CException
{
public:
    enum EErrCode {
        eJobIsBeingProcessed,
        eJobExecutionError,
        eConfError,
        eRequestIsNotSet,
        eJobIsNotReady,
        eIOError
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode())
        {
        case eJobIsBeingProcessed:    return "eJobIsBeingProcessed";
        case eJobExecutionError:      return "eJobExecutionError";
        case eConfError:              return "eConfError";
        case eRequestIsNotSet:        return "eRequestIsNotSet";
        case eJobIsNotReady:          return "eJobIsNotReady";
        case eIOError:                return "eIOError";
        default:                      return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CRADispatcherClientException, CException);
};

END_NCBI_SCOPE

#endif // CONNECT_SERVICES__RA_DISPATCHER_CLIENT_HPP
