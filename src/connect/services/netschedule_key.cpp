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

#include <connect/services/netschedule_key.hpp>
#include <connect/services/netschedule_api_expt.hpp>

#include <corelib/ncbistr.hpp>


BEGIN_NCBI_SCOPE

#define NS_KEY_V1_PREFIX "JSID_01_"
#define NS_KEY_V1_PREFIX_LEN (sizeof(NS_KEY_V1_PREFIX) - 1)

CNetScheduleKey::CNetScheduleKey(const string& key_str)
{
    if (!ParseJobKey(key_str)) {
        NCBI_THROW_FMT(CNetScheduleException, eKeyFormatError,
                "Invalid job key format: '" <<
                        NStr::PrintableString(key_str) << '\'');
    }
}

bool CNetScheduleKey::ParseJobKey(const string& key_str)
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

        // id
        if ((id = (unsigned) atoi(ch)) == 0)
            return false;
        do
            if (*++ch == '\0')
                return false;
        while (*ch != '_');

        // hostname
        {
            const char* host_begin = ++ch;
            unsigned host_len = 0;
            for (;;) {
                if (*ch == '\0')
                    return false;
                if (*ch++ != '_')
                    ++host_len;
                else
                    break;
            }
            host.assign(host_begin, host_len);
        }

        // port
        if ((port = (unsigned short) atoi(ch)) == 0)
            return false;

        while (*++ch != '_')
            switch (*ch) {
            case '\0':
                return true;
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                continue;
            default:
                return false;
            }

        // queue
        int underscores_to_skip = 0;
        while (*++ch == '_')
            ++underscores_to_skip;
        if (*ch == '\0')
            return false;
        const char* queue_begin = ch;
        while (*ch != '_' || --underscores_to_skip >= 0)
            if (*++ch == '\0')
                break;
        queue.assign(queue_begin, ch - queue_begin);

        return true;
    } else if (isdigit(*ch)) {
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
    if (queue_name.empty()) {
        NCBI_THROW_FMT(CNetScheduleException, eKeyFormatError,
                "Queue name cannot be empty.");
    }

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


END_NCBI_SCOPE
