#ifndef CONNECT_SERVICES_GRID__GRID_CLIENT__HPP
#define CONNECT_SERVICES_GRID__GRID_CLIENT__HPP

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

/// @file grid_client.hpp
/// NetSchedule Framework specs. 
///


#include <corelib/ncbistre.hpp>
#include <connect/connect_export.h>
#include <connect/services/netschedule_client.hpp>
#include <connect/services/netschedule_storage.hpp>


BEGIN_NCBI_SCOPE

/** @addtogroup NetScheduleClient
 *
 * @{
 */


class CGridClient;

/// Grid Job Submiter
///
class NCBI_XCONNECT_EXPORT CGridJobSubmiter
{
public:

    ~CGridJobSubmiter();

    /// Set a job's input This string will be sent to 
    /// then the job is submitted.
    ///
    /// This method can be used to send a short data to the worker node.
    /// To send a large data use GetOStream method. Don't call this 
    /// method after GetOStream method is called.
    ///
    void SetJobInput(const string& input);

    /// Get a stream where a client can write an input 
    /// data for the remote job
    ///
    CNcbiOstream& GetOStream();

    /// Submit a job to the queue
    ///
    /// @return a job key
    string Submit();

private:
    /// Only CGridClient can create an instnce of this class
    friend class CGridClient;
    CGridJobSubmiter(CGridClient&);

    CGridClient& m_GridClient;
    string       m_Input;

    /// The copy constructor and the assignment operator
    /// are prohibited
    CGridJobSubmiter(const CGridJobSubmiter&);
    CGridJobSubmiter& operator=(CGridJobSubmiter&);
};

/// Grid Job Status checker
///
class NCBI_XCONNECT_EXPORT CGridJobStatus
{
public:

    ~CGridJobStatus();

    /// Get a job's output string.
    ///
    /// This string can be used in two ways.
    /// 1. It can be an output data from a remote job (if that data is short)
    ///    If it is so don't use GetIStream method.
    /// 2. It holds a key to a data stored in an external data storage.
    ///    (NetCache)  In this case use GetIStream method to get a stream with 
    ///    a job's result. 
    ///
    const string& GetJobOutput() const    { return m_Output; }
    
    /// Get a job's input sting
    const string& GetJobInput() const    { return m_Input; }

    /// Get a job's return code
    //
    int           GetReturnCode() const   { return m_RetCode; }

    /// If something bad has happend this method will return an
    /// explanation
    ///
    const string& GetErrorMessage() const { return m_ErrMsg; }

    /// Get a job status
    ///
    CNetScheduleClient::EJobStatus GetStatus();

    /// Get a stream with a job's result. Stream is based on network
    /// data storage (NetCache). Size of the input data can be determined 
    /// using GetInputBlobSize
    ///
    CNcbiIstream& GetIStream();

    /// Get the size of an input stream
    ///
    size_t        GetBlobSize() const     { return m_BlobSize; }

private:
    /// Only CGridClient can create an instnce of this class
    friend class CGridClient;
    CGridJobStatus(CGridClient&, bool auto_cleanup);
    void x_SetJobKey(const string& job_key);

    CGridClient& m_GridClient;
    string       m_JobKey;
    string       m_Output;
    string       m_ErrMsg;
    string       m_Input;
    int          m_RetCode;
    size_t       m_BlobSize;
    bool         m_AutoCleanUp;

    /// The copy constructor and the assignment operator
    /// are prohibited
    CGridJobStatus(const CGridJobStatus&);
    CGridJobStatus& operator=(const CGridJobStatus&);
};

/// Grid Client
///
class NCBI_XCONNECT_EXPORT CGridClient
{
public:
    /// Constructor
    ///
    /// @param ns_client
    ///     NetSchedule client
    /// @param storage
    ///     NetSchedule storage
    /// @param auto_cleanup
    ///     if true the grid client will automatically remove
    ///     a job's input data from a storage when the job is
    ///     done or canceled
    ///
    CGridClient(CNetScheduleClient& ns_client, 
                INetScheduleStorage& storage,
                bool auto_cleanup = true);

    /// Destructor
    ///
    ~CGridClient();

    /// Get a job submiter
    ///
    CGridJobSubmiter& GetJobSubmiter();

    /// Get a job status checker
    ///
    /// @param job_key
    ///     Job key
    ///
    CGridJobStatus&   GetJobStatus(const string& job_key);

    /// Cancel Job
    ///
    /// @param job_key
    ///     Job key
    ///
    void CancelJob(const string& job_key);

    /// Remove a data blob from the storage
    ///
    /// @param data_key
    ///     Blob key
    ///
    void RemoveDataBlob(const string& data_key);

    CNetScheduleClient&  GetNSClient() { return m_NSClient; }
    INetScheduleStorage& GetStorage()  { return m_NSStorage; }

private:
    CNetScheduleClient&  m_NSClient;
    INetScheduleStorage& m_NSStorage;

    string                     m_Input;
    auto_ptr<CGridJobSubmiter> m_JobSubmiter;
    auto_ptr<CGridJobStatus>   m_JobStatus;

    /// The copy constructor and the assignment operator
    /// are prohibited
    CGridClient(const CGridClient&);
    CGridClient& operator=(const CGridClient&);
};


/////////////////////////////////////////////////////////////////////////////

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2005/04/18 18:53:57  didenko
 * Added optional automatic NetScheduler storage cleanup
 *
 * Revision 1.3  2005/03/29 14:10:16  didenko
 * + removing a date from the storage
 *
 * Revision 1.2  2005/03/28 19:29:37  didenko
 * Got rid of a warnning on GCC
 *
 * Revision 1.1  2005/03/25 16:23:43  didenko
 * Initail version
 *
 * ===========================================================================
 */


#endif // CONNECT_SERVICES_GRID__GRID_CLIENT__HPP
