#ifndef CONN___NETSCHEDULE_CLIENT__HPP
#define CONN___NETSCHEDULE_CLIENT__HPP

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
 * File Description:
 *   NetSchedule client API.
 *
 */

/// @file netschedule_client.hpp
/// NetSchedule client specs. 
///

#include <connect/connect_export.h>
#include <connect/ncbi_types.h>
#include <connect/netservice_client.hpp>
#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE


class CSocket;


/** @addtogroup NetScheduleClient
 *
 * @{
 */

/// Client API for NCBI NetSchedule server
///
/// This API is logically divided into two sections:
/// Job Submitter API and Worker Node API.
///
///
/// @sa CNetServiceException, CNetScheduleException
///

class NCBI_XCONNECT_EXPORT CNetScheduleClient : protected CNetServiceClient
{
public:
    /// Construct the client without linking it to any particular
    /// server. Actual server (host and port) will be extracted from the
    /// job key 
    ///
    /// @param client_name
    ///    Name of the client program(project)
    /// @param queue_name
    ///    Name of the job queue
    ///
    CNetScheduleClient(const string& client_name,
                       const string& queue_name);

    CNetScheduleClient(const string&  host,
                       unsigned short port,
                       const string&  client_name,
                       const string&  queue_name);

    /// Contruction based on existing TCP/IP socket
    /// @param sock
    ///    Connected socket to the primary server. 
    ///    CNetScheduleClient does not take socket ownership 
    ///    and does not change communication parameters (like timeout)
    ///
    CNetScheduleClient(CSocket*      sock,
                       const string& client_name,
                       const string& queue_name);

    virtual ~CNetScheduleClient();

    /// Job status codes
    ///
    enum EJobStatus
    {
        eJobNotFound = -1,  ///< No such job
        ePending     = 0,   ///< Waiting for execution
        eRunning,           ///< Running on a worker node
        eReturned,          ///< Returned back to the queue(to be rescheduled)
        eCanceled,          ///< Explicitly canceled
        eFailed,            ///< Failed to run (execution timeout)
        eDone,              ///< Job is ready (computed successfully)

        eLastStatus         ///< Fake status (do not use)
    };



    /// Submit job to server
    ///
    /// @param input
    ///    Input data. Arbitrary string (cannot exceed 1K). This string
    ///    encodes input data for the job. It is suggested to use NetCache
    ///    to keep the actual data and pass NetCache key as job input.
    ///
    /// @return job key
    string SubmitJob(const string& input);

    /// Cancel job
    ///
    /// @param job_key
    ///    Job identification string
    void CancelJob(const string& job_key);

    /// Get a pending job. Job receives running status.
    ///
    /// @param job_key
    ///     Job key
    /// @param input
    ///     Job input data (NetCache key). 
    /// @return
    ///     TRUE if job has been returned from the queue.
    ///     FALSE means queue is empty or for some reason scheduler
    ///     decided not to grant the job (node is overloaded).
    ///     In this case worker node should pause and come again later
    ///     for a new job.
    ///
    bool GetJob(string* job_key, string* input);

    /// Put job result (job should be received by GetJob())
    /// 
    /// @param job_key
    ///     Job key
    /// @param ret_code
    ///     Job return code
    /// @param output
    ///     Job output data (NetCache key). 
    ///
    void PutResult(const string& job_key, 
                   int           ret_code, 
                   const string& output);

    /// Request of current job status
    /// eJobNotFound is returned if job status cannot be found 
    /// (job record timed out)
    ///
    EJobStatus GetStatus(const string& job_key, 
                         int*          ret_code,
                         string*       output);

    /// Return version string
    string ServerVersion();


protected:

    /// Shutdown the server daemon.
    ///
    void ShutdownServer();

    /// Kill all jobs in the queue.
    ///
    void DropQueue();


protected:
    virtual 
    void CheckConnect(const string& key);
    bool IsError(const char* str);

    /// Adds trailing: "client\r\n queue\r\n COMMAND"
    void MakeCommandPacket(string* out_str, 
                           const string& cmd_str);

    /// check string for "OK:" prefix, throws an exception if "ERR:"
    void TrimPrefix(string* str);

    /// Check if server returned "OK:"
    void CheckOK(string* str);

    void CommandInitiate(const string& command, 
                         const string& job_key,
                         string*       answer);

private:
    CNetScheduleClient(const CNetScheduleClient&);
    CNetScheduleClient& operator=(const CNetScheduleClient&);

protected:
    string         m_Queue;

private:
    string         m_Tmp; ///< Temporary string
};


/// NetSchedule internal exception
///
class CNetScheduleException : public CNetServiceException
{
public:
    enum EErrCode {
        eAuthenticationError,
        eKeyFormatError,
        eInvalidJobStatus,
        eUnknownQueue,
        eTooManyPendingJobs,
        eDataTooLong
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode())
        {
        case eAuthenticationError:return "eAuthenticationError";
        case eKeyFormatError:     return "eKeyFormatError";
        case eInvalidJobStatus:   return "eInvalidJobStatus";
        case eUnknownQueue:       return "eUnknownQueue";
        case eTooManyPendingJobs: return "eTooManyPendingJobs";
        case eDataTooLong:        return "eDataTooLong";
        default:                  return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CNetScheduleException, CNetServiceException);
};

/// Meaningful information encoded in the NetSchedule key
///
/// @sa CNetSchedule_ParseBlobKey
///
struct CNetSchedule_Key
{
    string       prefix;    ///< Key prefix
    unsigned     version;   ///< Key version
    unsigned     id;        ///< Job id
    string       hostname;  ///< server name
    unsigned     port;      ///< TCP/IP port number
};

/// Parse blob key string into a CNetSchedule_Key structure
extern NCBI_XCONNECT_EXPORT
void CNetSchedule_ParseJobKey(CNetSchedule_Key* key, const string& key_str);

/// Parse blob key, extract job id
extern NCBI_XCONNECT_EXPORT
unsigned CNetSchedule_GetJobId(const string&  key_str);


/// Generate job key string
///
/// Please note that "id" is an integer issued by the scheduling server.
/// Clients should not use this function with custom ids. 
/// Otherwise it may disrupt the communication protocol.
///
extern NCBI_XCONNECT_EXPORT
void CNetSchedule_GenerateJobKey(string*        key, 
                                  unsigned       id, 
                                  const string&  host, 
                                  unsigned short port);

/// @internal
#define NETSCHEDULE_JOBMASK "JSID_01_%u_%s_%u"


/// @internal
const unsigned int kNetScheduleMaxDataSize = 512;

/* @} */


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2005/02/28 12:17:58  kuznets
 * Added new job status (Returned), + DropQueue()
 *
 * Revision 1.4  2005/02/22 16:14:00  kuznets
 * Reduced size of input data
 *
 * Revision 1.3  2005/02/10 20:00:54  kuznets
 * +GetJob(), +PutResult()
 *
 * Revision 1.2  2005/02/09 18:58:04  kuznets
 * Implemented job submission part of the API
 *
 * Revision 1.1  2005/02/07 13:02:32  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */


#endif  /* CONN___NETSCHEDULE_CLIENT__HPP */
