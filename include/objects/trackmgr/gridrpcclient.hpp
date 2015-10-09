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
#include <corelib/stream_utils.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <connect/services/grid_worker_app.hpp>
#include <connect/services/grid_rw_impl.hpp>
#include <connect/services/grid_client.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <corelib/perf_log.hpp>
#include <corelib/rwstream.hpp>
#include <util/compress/lzo.hpp>
#include <util/compress/stream_util.hpp>
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
/// Traits class for compressed binary ASN.1 streams
///
class CAsnBinCompressed : public CAsnBin
{
public:
    typedef int TOwnership;

    struct SStreamProp
    {
        SStreamProp(const CCompressStream::EMethod comp_mthd = CCompressStream::eZip)
            : compress_method(comp_mthd)
        {
        }

        CCompressStream::EMethod compress_method;
    };


    /// Return an object output stream (CObjectOStream)
    ///
    /// @param ostr
    ///   underlying output stream
    /// @return
    ///   object stream
    static CObjectOStream*
    GetOStream(CNcbiOstream& ostr, SStreamProp stream_prop = SStreamProp(CCompressStream::eZip))
    {
        auto_ptr<CCompressionOStream> outstr_zip(
            new CCompressionOStream(
                ostr,
                CreateStreamCompressor(stream_prop),
                CCompressionStream::fOwnProcessor
            )
        );
        return CObjectOStream::Open(GetDataFormat(), *outstr_zip.release(), eTakeOwnership);
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
        return GetIStream(istr, GetIStreamProperties(istr));
    }

    static CObjectIStream*
    GetIStream(const string& job_content, CNetCacheAPI& nc_api)
    {
        SStreamProp sp;
        return GetIStream(job_content, nc_api, sp);
    }

    static CObjectIStream*
    GetIStream(const string& job_content, CNetCacheAPI& nc_api, SStreamProp& streamprop)
    {
        streamprop = GetJobStreamProperties(job_content, nc_api);
        auto_ptr<CStringOrBlobStorageReader> reader(new CStringOrBlobStorageReader(job_content, nc_api));
        auto_ptr<CRStream> rstr(new CRStream(reader.release(), CRWStreambuf::fOwnReader));
        return GetIStream(*rstr.release(), streamprop, CCompressionStream::fOwnAll);
    }

protected:
    static CObjectIStream*
    GetIStream(CNcbiIstream& istr,
               const SStreamProp& stream_prop,
               TOwnership ownership = CCompressionStream::fOwnProcessor
              )
    {
        auto_ptr<CCompressionIStream> instr_zip(
            new CCompressionIStream(
                istr,
                CreateStreamDecompressor(stream_prop),
                ownership
            )
        );
        return CObjectIStream::Open(GetDataFormat(), *instr_zip.release(), eTakeOwnership);
    }

    static SStreamProp GetJobStreamProperties(const string& job_content, CNetCacheAPI& nc_api)
    {
        if (job_content.empty()) {
            NCBI_THROW(CException, eUnknown, "job content is empty");
        }
        CStringOrBlobStorageReader reader(job_content, nc_api);
        char buf[5];
        size_t count = ArraySize(buf);
        size_t bytes_read = 0UL;
        reader.Read(buf, count, &bytes_read);
        const bool is_lzo = IsLZOStream(CTempString(buf, bytes_read));
        return SStreamProp(is_lzo ? CCompressStream::eLZO : CCompressStream::eZip);
    };

    static SStreamProp
    GetIStreamProperties(CNcbiIstream& istr)
    {
        return SStreamProp(IsLZOStream(istr) ? CCompressStream::eLZO : CCompressStream::eZip);
    }

    static bool IsLZOStream(CNcbiIstream& istr)
    {
	char buf[5];
        const size_t buflen = ArraySize(buf);
        const streamsize readlen = CStreamUtils::Readsome(istr, buf, buflen);
        CStreamUtils::Stepback(istr, buf, readlen);
        return IsLZOStream(CTempString(buf, readlen));
    }

    static bool IsLZOStream(const CTempString& str)
    {
        /// LZO magic header (see fStreamFormat flag).
        static const char kMagic[] = { 'L', 'Z', 'O', '\0' };
        static const size_t kMagicSize = 4UL;
        return (str.size() < kMagicSize)
            ? false
            : NStr::Equal(CTempString(kMagic, kMagicSize), CTempString(str, 0, kMagicSize));
    }

    static CCompressionStreamProcessor* CreateStreamCompressor(const SStreamProp& stream_prop)
    {
        auto_ptr<CCompressionStreamProcessor> sp;
        if (stream_prop.compress_method == CCompressStream::eLZO) {
            sp.reset(new CLZOStreamCompressor());
        }
        else {
            sp.reset(new CZipStreamCompressor());
        }
        return sp.release();
    }

    static CCompressionStreamProcessor* CreateStreamDecompressor(const SStreamProp& stream_prop)
    {
        auto_ptr<CCompressionStreamProcessor> sp;
        if (stream_prop.compress_method == CCompressStream::eLZO) {
            sp.reset(new CLZOStreamDecompressor());
        }
        else {
            sp.reset(new CZipStreamDecompressor());
        }
        return sp.release();
    }

    static string CompMethodToString(const CCompressStream::EMethod method)
    {
        switch (method) {
        case CCompressStream::eNone:
            return "none";
        case CCompressStream::eBZip2:
            return "BZip2";
        case CCompressStream::eLZO:
            return "LZO";
        case CCompressStream::eZip:
            return "Zip";
        case CCompressStream::eGZipFile:
            return "GZipFile";
        case CCompressStream::eConcatenatedGZipFile:
            return "GZipFile";
        };
        NCBI_THROW(CException, eUnknown, "unexpected compression method");
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
/// TConnectTraits template classes: CAsnBinCompressed
///
template <typename TConnectTraits = CAsnBinCompressed, int DefaultTimeout = 20>
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
    pair<CNetScheduleJob, bool> Ask(const TRequest& request, TReply& reply) const
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
        bool timed_out = false;
        x_PrepareJob(job);

        try {
            const CNetScheduleAPI::EJobStatus evt = m_Grid_cli->SubmitAndWait(m_Timeout);
            switch (evt) {
            case CNetScheduleAPI::eDone:
            {
                m_NS_api.GetJobDetails(job);
                auto_ptr<CObjectIStream> instr(TConnectTraits::GetIStream(job.output, m_NC_api));
                *instr >> reply;
                break;
            }
            case CNetScheduleAPI::eFailed:
                NCBI_THROW(CGridRPCBaseClientException, eUnexpectedFailure, "Job failed");

            case CNetScheduleAPI::eCanceled:
                NCBI_THROW(CGridRPCBaseClientException, eUnexpectedFailure, "Job canceled");

            case CNetScheduleAPI::eRunning:
                NCBI_THROW(CGridRPCBaseClientException, eUnexpectedFailure, "Job running");

            default:
                NCBI_THROW(CGridRPCBaseClientException,
                           eWaitTimeout,
                           "Unexpected status: " + CNetScheduleAPI::StatusToString(evt)
                          );
            }
        }
        catch (const CGridRPCBaseClientException& e) {
            switch (e.GetErrCode()) {
            case CGridRPCBaseClientException::eUnexpectedFailure:
                timed_out = true;
                break;

            case CGridRPCBaseClientException::eWaitTimeout:
            default:
                break;
            }
            throw e;
        }
        return make_pair(job, timed_out);
    }

protected:
    virtual void x_PrepareJob(CNetScheduleJob& job) const
    {
    }

    template <class TReply>
    CNetScheduleJob x_GetJobById(const string job_id, TReply& reply) const
    {
        CNetScheduleJob job;
        job.job_id = job_id;

        CNetScheduleSubmitter job_submitter = m_NS_api.GetSubmitter();
        const CNetScheduleAPI::EJobStatus evt = job_submitter.WaitForJob(job.job_id, m_Timeout);
        switch (evt) {
        case CNetScheduleAPI::eDone:
        {
            m_NS_api.GetJobDetails(job);
            auto_ptr<CObjectIStream> instr(TConnectTraits::GetIStream(job.output, m_NC_api));
            *instr >> reply;
            break;
        }
        case CNetScheduleAPI::eFailed:
            NCBI_THROW(CGridRPCBaseClientException, eUnexpectedFailure, "Job failed");

        case CNetScheduleAPI::eCanceled:
            NCBI_THROW(CGridRPCBaseClientException, eUnexpectedFailure, "Job canceled");

        case CNetScheduleAPI::eRunning:
            NCBI_THROW(CGridRPCBaseClientException, eUnexpectedFailure, "Job running");

        default:
            NCBI_THROW(CGridRPCBaseClientException,
                       eWaitTimeout,
                       "Unexpected status: " + CNetScheduleAPI::StatusToString(evt)
                      );
        }
        return job;
    }

private:
    mutable CNetScheduleAPI m_NS_api;
    mutable CNetCacheAPI m_NC_api;
    mutable auto_ptr<CGridClient> m_Grid_cli;
    Uint4 m_Timeout;
};


END_NCBI_SCOPE

#endif // INTERNAL_MISC_SERIAL___GRID_RPC_CLIENT__HPP

