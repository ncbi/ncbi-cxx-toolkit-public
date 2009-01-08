#ifndef CONN_SERVICES___NETSCHEDULE_API_IMPL__HPP
#define CONN_SERVICES___NETSCHEDULE_API_IMPL__HPP

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
 * Authors:  Anatoliy Kuznetsov, Maxim Didenko, Victor Joukov, Dmitry Kazimirov
 *
 * File Description:
 *   NetSchedule client API implementation details.
 *
 */

#include "netservice_api_impl.hpp"

#include <connect/services/netschedule_api.hpp>
#include <connect/services/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   ConnServ_NetSchedule

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
///@internal


template<typename Pred>
bool s_WaitNotification(unsigned wait_time,
                        unsigned short udp_port, Pred pred)
{
    _ASSERT(wait_time);

    EIO_Status status;

    STimeout to;
    to.sec = wait_time;
    to.usec = 0;

    CDatagramSocket  udp_socket;
    udp_socket.SetReuseAddress(eOn);
    STimeout rto;
    rto.sec = rto.usec = 0;
    udp_socket.SetTimeout(eIO_Read, &rto);

    status = udp_socket.Bind(udp_port);
    if (eIO_Success != status) {
        return false;
    }
    time_t curr_time, start_time, end_time;

    start_time = time(0);
    end_time = start_time + wait_time;

    for (;;) {
        curr_time = time(0);
        if (curr_time >= end_time)
            break;
        to.sec = (unsigned int) (end_time - curr_time);

        status = udp_socket.Wait(&to);
        if (eIO_Success != status) {
            continue;
        }
        size_t msg_len;
        string   buf(1024/sizeof(int),0);
        status = udp_socket.Recv(&buf[0], buf.size(), &msg_len, NULL);
        _ASSERT(status != eIO_Timeout); // because we Wait()-ed
        if (eIO_Success == status) {
            buf.resize(msg_len);
            if( pred(buf) )
                return true;
        }
    } // for
    return false;
}

////////////////////////////////////////////////////////////////////////////////////////
//

struct SNSSendCmd
{
    enum EFlags {
        eLogExceptions = 1 >> 1,
        eIgnoreCommunicationError = 1 >> 2,
        eIgnoreDuplicateNameError = 1 >> 3
    };
    typedef int TFlags;

    SNSSendCmd(CNetScheduleAPI::TPtr api, const string& cmd, TFlags flags)
        : m_API(api), m_Cmd(cmd), m_Flags(flags)
    {}
    virtual ~SNSSendCmd() {}

    void operator()(CNetServerConnection conn)
    {
        string resp;
        try {
            resp = conn.Exec(m_Cmd);
        } catch (CNetScheduleException& ex) {
            if (m_Flags & eLogExceptions) {
                ERR_POST_X(11, conn.GetHost() << ":" << conn.GetPort()
                    << " returned error: \"" << ex.what() << "\"");
            }
            if (!(m_Flags & eIgnoreDuplicateNameError &&
                ex.GetErrCode() == CNetScheduleException::eDuplicateName))
                throw;
        } catch (CNetServiceException& ex) {
            if (m_Flags & eLogExceptions) {
                ERR_POST_X(12, conn.GetHost() << ":" << conn.GetPort()
                    << " returned error: \"" << ex.what() << "\"");
            }
            if (!(m_Flags & eIgnoreCommunicationError &&
                ex.GetErrCode() == CNetServiceException::eCommunicationError))
                throw;
        }
        ProcessResponse(resp, conn);
    }
    virtual void ProcessResponse(const string& /* resp */,
        CNetServerConnection::TPtr /* conn_impl */) {}

    CNetScheduleAPI m_API;
    string m_Cmd;
    TFlags m_Flags;
};


#define SERVER_PARAMS_ASK_MAX_COUNT 100

template<typename T> struct ToStr { static string Convert(T t); };

template<> struct ToStr<string> {
    static string Convert(const string& val) {
        return '\"' + NStr::PrintableString(val) + '\"';
    }
};
template<> struct ToStr<unsigned int> {
    static string Convert(unsigned int val) {
        return NStr::UIntToString(val);
    }
};

template<> struct ToStr<int> {
    static string Convert(int val) {
        return NStr::IntToString(val);
    }
};

struct SNetScheduleAPIImpl : public CNetObject
{
    SNetScheduleAPIImpl(const string& service_name,
        const string& client_name,
        const string& queue_name);

    class CNetScheduleServerListener : public INetServerConnectionListener
    {
    public:
        CNetScheduleServerListener(
            const std::string& client_name,
            const std::string& queue_name);

        void SetAuthString(
            const std::string& client_name,
            const std::string& program_version,
            const std::string& queue_name);

    private:
        virtual void OnConnected(CNetServerConnection::TPtr);
        virtual void OnError(const string& err_msg,
            SNetServerConnectionPoolImpl* pool);

    private:
        std::string m_Auth;
    };

    string x_SendJobCmdWaitResponse(const string& cmd, const string& job_key)
    {
        return x_GetConnection(job_key).Exec(cmd + ' ' + job_key);
    }
    template<typename Arg1>
    string x_SendJobCmdWaitResponse(const string& cmd, const string& job_key, Arg1 arg1)
    {
        string tmp = cmd;
        if (!job_key.empty())
            tmp += ' ' + job_key + ' ';
        tmp += ToStr<Arg1>::Convert(arg1);
        return x_GetConnection(job_key).Exec(tmp);
    }
    template<typename Arg1, typename Arg2>
    string x_SendJobCmdWaitResponse(const string& cmd, const string& job_key,
                                    Arg1 arg1, Arg2 arg2)
    {
        string tmp = cmd;
        if (!job_key.empty())
            tmp += ' ' + job_key + ' ';
        tmp += ToStr<Arg1>::Convert(arg1) + ' ' + ToStr<Arg2>::Convert(arg2);
        return x_GetConnection(job_key).Exec(tmp);
    }
    template<typename Arg1, typename Arg2, typename Arg3>
    string x_SendJobCmdWaitResponse(const string& cmd, const string& job_key,
                                    Arg1 arg1, Arg2 arg2, Arg3 arg3)
    {
        string tmp = cmd;
        if (!job_key.empty())
            tmp += ' ' + job_key + ' ';
        tmp += ToStr<Arg1>::Convert(arg1) + ' '
            + ToStr<Arg2>::Convert(arg2) + ' ' + ToStr<Arg3>::Convert(arg3);
        return x_GetConnection(job_key).Exec(tmp);
    }

    const CNetScheduleAPI::SServerParams& GetServerParams();

    CNetServerConnection x_GetConnection(const string& job_key = kEmptyStr);

    CNetScheduleAPI::EJobStatus x_GetJobStatus(
        const string& job_key,
        bool submitter);

    CNetService m_Service;

    static CNetScheduleExceptionMap sm_ExceptionMap;
    string            m_Queue;
    string            m_ProgramVersion;

    CNetObjectRef<CNetScheduleServerListener> m_Listener;

    auto_ptr<CNetScheduleAPI::SServerParams> m_ServerParams;
    long m_ServerParamsAskCount;
    CFastMutex m_ServerParamsMutex;
};

inline SNetScheduleAPIImpl::SNetScheduleAPIImpl(
    const string& service_name,
    const string& client_name,
    const string& queue_name) :
        m_Service(new SNetServiceImpl(service_name, client_name)),
        m_Queue(queue_name),
        m_ServerParamsAskCount(SERVER_PARAMS_ASK_MAX_COUNT)
{
    m_Listener = new CNetScheduleServerListener(
        m_Service.GetClientName(), queue_name);

    m_Service->SetListener(m_Listener);
}

inline CNetServerConnection
    SNetScheduleAPIImpl::x_GetConnection(const string& job_key)
{
    if (job_key.empty())
        return m_Service.GetBestConnection();

    CNetScheduleKey nskey(job_key);
    return m_Service.GetSpecificConnection(nskey.host, nskey.port);
}

struct SNetScheduleSubmitterImpl : public CNetObject
{
    SNetScheduleSubmitterImpl(CNetScheduleAPI::TPtr ns_api_impl);

    string SubmitJobImpl(CNetScheduleJob& job,
        unsigned short udp_port, unsigned wait_time) const;

    CNetScheduleAPI m_API;
};

inline SNetScheduleSubmitterImpl::SNetScheduleSubmitterImpl(
    CNetScheduleAPI::TPtr ns_api_impl) :
    m_API(ns_api_impl)
{
}

struct SNetScheduleExecuterImpl : public CNetObject
{
    SNetScheduleExecuterImpl(CNetScheduleAPI::TPtr ns_api_impl);

    bool GetJobImpl(const string& cmd, CNetScheduleJob& job) const;

    void x_RegUnregClient(const string& cmd, unsigned short udp_port) const;

    CNetScheduleAPI m_API;
};

inline SNetScheduleExecuterImpl::SNetScheduleExecuterImpl(
    CNetScheduleAPI::TPtr ns_api_impl) :
    m_API(ns_api_impl)
{
}

struct SNetScheduleAdminImpl : public CNetObject
{
    SNetScheduleAdminImpl(CNetScheduleAPI::TPtr ns_api_impl);

    typedef map<pair<string,unsigned int>, string> TIDsMap;

    TIDsMap x_QueueIDs(const string& queury) const;

    CNetScheduleAPI m_API;
};

inline SNetScheduleAdminImpl::SNetScheduleAdminImpl(
    CNetScheduleAPI::TPtr ns_api_impl) :
    m_API(ns_api_impl)
{
}


END_NCBI_SCOPE


#endif  /* CONN_SERVICES___NETSCHEDULE_API_IMPL__HPP */
