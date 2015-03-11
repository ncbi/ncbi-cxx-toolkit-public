#ifndef INTERNAL_MISC_SERIAL___GRID_RPC_CLIENT__HPP
#define INTERNAL_MISC_SERIAL___GRID_RPC_CLIENT__HPP

/* $Id$
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
 * Authors: Peter Meric, Dmitry Kazimirov
 *
 */

/// @file gridrpcclient.hpp
/// Classes pertaining to GRID-based ASN.1 RPC clients

#include <corelib/ncbiobj.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <connect/services/grid_worker_app.hpp>
#include <connect/services/grid_rw_impl.hpp>
#include <connect/services/grid_client.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <corelib/perf_log.hpp>
#include <corelib/rwstream.hpp>
#include <util/compress/zlib.hpp>


BEGIN_NCBI_SCOPE


///
/// Traits class for binary ASN.1
///
class CAsnBin
{
public:
    /// Return the serialization type
    ///
    /// @return
    ///   data format enumeration
    static ESerialDataFormat GetDataFormat(void)
    {
        return eSerial_AsnBinary;
    }
};

///
/// Traits class for uncompressed binary ASN.1 streams
///
class CAsnBinUncompressed : public CAsnBin
{
public:
    /// Return an object output stream (CObjectOStream)
    ///
    /// @param ostr
    ///   underlying output stream
    /// @return
    ///   object stream
    static CObjectOStream*
    GetOStream(CNcbiOstream& ostr)
    {
        return CObjectOStream::Open(GetDataFormat(), ostr);
    }

    /// Return an object input stream (CObjectIStream)
    ///
    /// @param istr
    ///   underlying input stream
    /// @return
    ///   object stream
    static CObjectIStream*
    GetIStream(CNcbiIstream& istr)
    {
        return CObjectIStream::Open(GetDataFormat(), istr);
    }
};

///
/// Traits class for compressed binary ASN.1 streams
///
class CAsnBinCompressed : public CAsnBin
{
public:
    /// Return an object output stream (CObjectOStream)
    ///
    /// @param ostr
    ///   underlying output stream
    /// @return
    ///   object stream
    static CObjectOStream*
    GetOStream(CNcbiOstream& ostr)
    {
        static const enum ENcbiOwnership ownership = eTakeOwnership;
        auto_ptr<CCompressionOStream> outstr_zip(
            new CCompressionOStream(
                ostr,
                new CZipStreamCompressor(),
                CCompressionStream::fOwnProcessor
            )
        );
        return CObjectOStream::Open(GetDataFormat(), *outstr_zip.release(), ownership);
    }

    /// Return an object input stream (CObjectIStream)
    ///
    /// @param istr
    ///   underlying input stream
    /// @return
    ///   object stream
    static CObjectIStream*
    GetIStream(CNcbiIstream& istr)
    {
        static const enum ENcbiOwnership ownership = eTakeOwnership;
        auto_ptr<CCompressionIStream> instr_zip(
            new CCompressionIStream(
                istr,
                new CZipStreamDecompressor(CZipCompression::fAllowTransparentRead),
                CCompressionStream::fOwnProcessor
            )
        );
        return CObjectIStream::Open(GetDataFormat(), *instr_zip.release(), ownership);
    }
};


class CGridRPCBaseClientException : public CException
{
public:
    enum EErrCode
    {
        eWaitTimeout,    ///< timeout while waiting for job completion
        eUnexpectedFailure
    };
    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CGridRPCBaseClientException, CException);
};


///
/// CGridRPCBaseClient<typename TConnectTraits>
///
/// Base class for GRID-based ASN.1 RPC clients
/// 
/// TConnectTraits template classes: CAsnBinCompressed and CAsnBinUncompressed
///
template <typename TConnectTraits, int DefaultTimeout = 20>
class CGridRPCBaseClient : private TConnectTraits
{
public:
    CGridRPCBaseClient(const string& NS_service,
                       const string& NS_queue,
                       const string& client_name,
                       const string& NC_registry_section
                      )
        : m_NS_api(NS_service, client_name, NS_queue),
          m_Timeout(DefaultTimeout)
    {
        x_Init(NC_registry_section);
    }

    CGridRPCBaseClient(const string& NS_registry_section = "netschedule_api",
                       const string& NC_registry_section = kEmptyStr
                      )
        : m_NS_api(CNetScheduleAPI::eAppRegistry, NS_registry_section),
          m_Timeout(DefaultTimeout)
    {
        LOG_POST(Trace << "NS: " << NS_registry_section);
        CMutexGuard guard(CNcbiApplication::GetInstanceMutex());
        static const CNcbiRegistry& cfg = CNcbiApplication::Instance()->GetConfig();
        const string nc_reg(
            NStr::IsBlank(NC_registry_section)
                ? cfg.GetString(NS_registry_section, "netcache_api", "netcache_api")
                : NC_registry_section
        );
        x_Init(nc_reg);
    }

    void SetTimeout(const size_t timeout)
    {
        m_Timeout = timeout;
    }

    void x_Init(const string& NC_registry_section)
    {
        LOG_POST(Trace << "NS queue NC: " << NC_registry_section);
        m_NC_api = CNetCacheAPI(CNetCacheAPI::eAppRegistry, NC_registry_section);
        m_Grid_cli.reset(new CGridClient(m_NS_api.GetSubmitter(),
                                         m_NC_api,
                                         CGridClient::eManualCleanup,
                                         CGridClient::eProgressMsgOn
                                        )
                        );
    }

    virtual ~CGridRPCBaseClient()
    {
    }

    template <class TRequest, class TReply>
    void Ask(const TRequest& request, TReply& reply) const
    {
        CNcbiOstream& job_in = m_Grid_cli->GetOStream(); // job input stream
        auto_ptr<CObjectOStream> outstr(TConnectTraits::GetOStream(job_in));
        *outstr << request;
        if (job_in.bad()) {
            NCBI_THROW(CIOException, eWrite, "Error while writing request");
        }
        outstr.reset();
        m_Grid_cli->CloseStream();

        CNetScheduleJob& job = m_Grid_cli->GetJob();
        x_PrepareJob(job);

        CNetScheduleNotificationHandler submit_job_handler;
        submit_job_handler.SubmitJob(m_NS_api.GetSubmitter(),
                                     job,
                                     m_Timeout
                                    );
        LOG_POST(Trace << "submitted job: " << job.job_id);
        x_GetJobReply(submit_job_handler, job, reply);
    }

protected:
    virtual void x_PrepareJob(CNetScheduleJob& job) const
    {
    }

    template <class TReply>
    void x_GetJobReply(CNetScheduleNotificationHandler& job_handler, CNetScheduleJob& job, TReply& reply) const
    {
        CDeadline deadline(m_Timeout, 0);
        CNetScheduleAPI::EJobStatus status(
            job_handler.WaitForJobCompletion(job, deadline, m_NS_api)
        );

        // TODO The wait is over; the current job status is in the status
        // variable. CNetScheduleAPI::StatusToString(status) can convert it
        // to a human-readable string.
        if (status == CNetScheduleAPI::eDone) {
            // TODO The job has completed successfully.
            CStringOrBlobStorageReader reader(job.output, m_NC_api);
            CRStream rstr(&reader);
            unique_ptr<CObjectIStream> instr(TConnectTraits::GetIStream(rstr));
            *instr >> reply;
        }
        else {
            // TODO Check 'status' to see if the job's still running or
            // it has failed.
            NCBI_THROW(CGridRPCBaseClientException, eWaitTimeout, kEmptyStr);
        }
    }

    template <class TReply>
    void x_GetJobRepliesById(const vector<string> job_ids, vector<CRef<TReply>>& replies) const
    {
        CDeadline deadline(m_Timeout, 0);
        static const CDeadline kTimeSlice = { 0, 100L * 1000000 }; // 100ms
        CDeadline timeslice = kTimeSlice;

        CNetScheduleNotificationHandler job_handler;
        list<string> _job_ids(begin(job_ids), end(job_ids));
CStopWatch sw(CStopWatch::eStart);
        CPerfLogGuard pl("x_GetJobRepliesById");
        do {
            ERASE_ITERATE(list<string>, it, _job_ids) {
                const auto& job_id = *it;
                //CNetScheduleAPI::EJobStatus job_status;
                int last_event_index;
            CNetScheduleJob job;
            job.job_id = job_id;
                const auto job_status = job_handler.WaitForJobCompletion(job, timeslice, m_NS_api);
                if (job_status == CNetScheduleAPI::eDone) {
                    VECTOR_ERASE(it, _job_ids);
                    CStringOrBlobStorageReader reader(job.output, m_NC_api);
                    CRStream rstr(&reader);
                    unique_ptr<CObjectIStream> instr(TConnectTraits::GetIStream(rstr));
                    auto reply = Ref(new TReply());
                    *instr >> *reply;
                    replies.push_back(reply);
                }
            }
        } while (!deadline.IsExpired() && !_job_ids.empty());
        pl.Post(CRequestStatus::e200_Ok, kEmptyStr);
cerr << "x_GetJobRepliesById finished waiting: " << sw.Elapsed() << "s\n";
    }

    template <class TReply>
    void x_GetJobById(const string job_id, TReply& reply) const
    {
        CDeadline deadline(m_Timeout, 0);
        CNetScheduleNotificationHandler job_handler;
        CNetScheduleAPI::EJobStatus job_status;
        int last_event_index;
        if (job_handler.RequestJobWatching(m_NS_api, job_id, deadline, &job_status, &last_event_index)) {
            CNetScheduleJob job;
            job.job_id = job_id;
            x_GetJobReply(job_handler, job, reply);
        }
    }

private:
    mutable CNetScheduleAPI m_NS_api;
    mutable CNetCacheAPI m_NC_api;
    mutable auto_ptr<CGridClient> m_Grid_cli;
    int m_Timeout;
};


END_NCBI_SCOPE

#endif // INTERNAL_MISC_SERIAL___GRID_RPC_CLIENT__HPP

