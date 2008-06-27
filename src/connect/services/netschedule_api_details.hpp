#ifndef __NETSCHEDULE_API_DETAILS_HPP_
#define __NETSCHEDULE_API_DETAILS_HPP_


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
 */

#include <connect/ncbi_socket.hpp>
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

    SNSSendCmd(const CNetScheduleAPI& api, const string& cmd, TFlags flags)
        : m_API(&api), m_Cmd(cmd), m_Flags(flags)
    {}
    virtual ~SNSSendCmd() {}

    void operator()(CNetServerConnection conn)
    {
        string resp;
        try {
           resp = m_API->SendCmdWaitResponse(conn, m_Cmd);
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
        CNetServerConnection /* conn */) {}

    const CNetScheduleAPI* m_API;
    string m_Cmd;
    TFlags m_Flags;
};


END_NCBI_SCOPE

#endif // __NETSCHEDULE_API_DETAILS_HPP_
