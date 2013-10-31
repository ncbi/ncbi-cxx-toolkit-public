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
 * Author:  Anatoliy Kuznetsov, Maxim Didenko, Victor Joukov, Dmitry Kazimirov
 *
 * File Description:
 *   Implementation of NetSchedule API.
 *
 */

#include <ncbi_pch.hpp>

#include "util.hpp"
#include "netschedule_api_impl.hpp"

#include <connect/services/netschedule_key.hpp>
#include <connect/services/netschedule_api_expt.hpp>

#include <corelib/ncbistr.hpp>


BEGIN_NCBI_SCOPE

#define NS_KEY_V1_PREFIX "JSID_01_"
#define NS_KEY_V1_PREFIX_LEN (sizeof(NS_KEY_V1_PREFIX) - 1)

CNetScheduleKey::CNetScheduleKey(const string& key_str,
        CCompoundIDPool::TInstance id_pool)
{
    if (!ParseJobKey(key_str, id_pool)) {
        NCBI_THROW_FMT(CNetScheduleException, eKeyFormatError,
                "Invalid job key format: '" <<
                        NStr::PrintableString(key_str) << '\'');
    }
}

bool CNetScheduleKey::ParseJobKey(const string& key_str,
        CCompoundIDPool::TInstance id_pool)
{
    // Parses several notations for job id:
    // version 1:
    //   JSID_01_JOBNUMBER_IP_PORT_QUEUE
    // "version 0", or just a job number.

    const char* ch = key_str.c_str();
    if (key_str.compare(0, NS_KEY_V1_PREFIX_LEN, NS_KEY_V1_PREFIX) == 0) {
        // Version 1 key
        version = 1;
        ch += NS_KEY_V1_PREFIX_LEN;

        // Extract the job number field.
        if ((id = (unsigned) atoi(ch)) == 0)
            return false;
        do
            if (*++ch == '\0')
                return false;
        while (*ch != '_');

        // Find the host name field boundaries.
        const char* token_begin = ++ch;
        while (*ch != '\0' && *ch++ != '_')
            /* noop */;

        // Extract the server port number.
        if ((port = (unsigned short) atoi(ch)) == 0)
            return false;

        // Everything's OK so far, save the host name/IP.
        host.assign(token_begin, ch - token_begin - 1);
        if (host.empty())
            return false;

        // Skip to the queue name.
        while (*++ch != '_')
            if (*ch < '0' || *ch > '9')
                return *ch == '\0';

        // Queue name is specified - extract it.
        int underscores_to_skip = 0;
        while (*++ch == '_')
            ++underscores_to_skip;
        if (*ch == '\0')
            return false;
        // At this point, *ch is neither '_' nor '\0'.
        token_begin = ch;
        while (*++ch != '\0')
            if ((*ch < '0' || *ch > '9') && (*ch < 'a' || *ch > 'z') &&
                    (*ch < 'A' || *ch > 'Z') && *ch != '-') {
                if (*ch != '_')
                    return false;
                else if (--underscores_to_skip < 0)
                    break;
            }
        if (underscores_to_skip > 0)
            return false;
        queue.assign(token_begin, ch - token_begin);

        return true;
    } else if (id_pool != NULL) {
        try {
            CCompoundIDPool pool_obj(id_pool);
            CCompoundID cid(pool_obj.FromString(key_str));
            version = 2;
            CCompoundIDField field(cid.GetFirst(eCIT_ID));
            if (field) {
                id = (unsigned) field.GetID();
                field = cid.GetFirst(eCIT_DatabaseName);
                if (field)
                    queue = field.GetDatabaseName();
                field = cid.GetFirst(eCIT_IPv4SockAddr);
                if (field) {
                    host = CSocketAPI::ntoa(field.GetIPv4Address());
                    port = field.GetPort();
                    return true;
                } else {
                    field = cid.GetFirst(eCIT_Host);
                    if (field) {
                        host = field.GetHost();
                        field = cid.GetFirst(eCIT_Port);
                        if (field) {
                            port = field.GetPort();
                            return true;
                        }
                    }
                }
            }
        }
        catch (CCompoundIDException&) {
            // Do not exit the function yet -- check if
            // it's a verbatim job ID.
        }
    }

    if (isdigit(*ch)) {
        version = 0;
        id = (unsigned) atoi(ch);
        return true;
    }

    return false;
}

#define MAX_INT_TO_STR_LEN(type) (sizeof(type) * 3 / 2)

CNetScheduleKeyGenerator::CNetScheduleKeyGenerator(
        const string& host, unsigned port, const string& queue_name)
{
    SNetScheduleAPIImpl::VerifyQueueNameAlphabet(queue_name);

    m_UseIPv4Addr = CSocketAPI::isip(host, true);
    if (m_UseIPv4Addr)
        m_HostIPv4Addr = CSocketAPI::gethostbyname(host);
    else
        m_HostName = host;
    m_Port = (unsigned short) port;
    m_QueueName = queue_name;

    string port_str(NStr::IntToString(port));

    unsigned queue_prefix_len = g_NumberOfUnderscoresPlusOne(queue_name);

    m_V1HostPortQueue.reserve(1 + host.size() + 1 + port_str.size() +
            queue_prefix_len + queue_name.size());

    m_V1HostPortQueue.push_back('_');
    m_V1HostPortQueue.append(host);
    m_V1HostPortQueue.push_back('_');
    m_V1HostPortQueue.append(port_str);
    m_V1HostPortQueue.append(queue_prefix_len, '_');
    m_V1HostPortQueue.append(queue_name);
}

void CNetScheduleKeyGenerator::Generate(string* key, unsigned id) const
{
    key->reserve(NS_KEY_V1_PREFIX_LEN +
        MAX_INT_TO_STR_LEN(unsigned) +
        m_V1HostPortQueue.size());
    key->assign(NS_KEY_V1_PREFIX, NS_KEY_V1_PREFIX_LEN);
    key->append(NStr::IntToString(id));
    key->append(m_V1HostPortQueue);
}

string CNetScheduleKeyGenerator::GenerateCompoundID(unsigned id,
        CCompoundIDPool id_pool) const
{
    CCompoundID ns_key_cid = id_pool.NewID(eCIC_NetScheduleKey);

    ns_key_cid.AppendID(id);

    if (m_UseIPv4Addr)
        ns_key_cid.AppendIPv4SockAddr(m_HostIPv4Addr, m_Port);
    else {
        ns_key_cid.AppendHost(m_HostName);
        ns_key_cid.AppendPort(m_Port);
    }

    if (!m_QueueName.empty())
        ns_key_cid.AppendDatabaseName(m_QueueName);

    return ns_key_cid.ToString();
}

END_NCBI_SCOPE
