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

#include <connect/services/netschedule_key.hpp>
#include <connect/services/netschedule_api_expt.hpp>

#include <corelib/ncbistr.hpp>


BEGIN_NCBI_SCOPE

#define NS_KEY_V1_PREFIX "JSID_01_"
#define NS_KEY_V1_PREFIX_LEN (sizeof(NS_KEY_V1_PREFIX) - 1)

#define NS_KEY_V2_PREFIX "nsid|"
#define NS_KEY_V2_PREFIX_LEN (sizeof(NS_KEY_V2_PREFIX) - 1)

CNetScheduleKey::CNetScheduleKey(const string& key_str)
{

    // Parses several notations for job id:
    // version 1:
    //   JSID_01_1_MYHOST_9000
    // version 2:
    //   nsid|host/port/[queue]/num[/run] e.g.,
    //     nsid|192.168.1.1/9101/splign/1
    //     nsid|192.168.1.1/9101/splign/1/5
    //     nsid|192.168.1.1/9101//1/5
    // "version 0", or just job number or job number with run number:
    //   num[/run]

    const char* ch = key_str.c_str();
    if (key_str.compare(0, NS_KEY_V1_PREFIX_LEN, NS_KEY_V1_PREFIX) == 0) {
        // Old (version 1) key
        version = 1;
        ch += 8;

        // id
        id = (unsigned) atoi(ch);
        while (*ch && *ch != '_') {
            ++ch;
        }
        if (*ch == 0 || id == 0) {
            NCBI_THROW_FMT(CNetScheduleException, eKeyFormatError,
                    key_str << ": key syntax error.");
        }
        ++ch;

        // hostname
        for (;*ch && *ch != '_'; ++ch) {
            host += *ch;
        }
        if (*ch == 0) {
            NCBI_THROW_FMT(CNetScheduleException, eKeyFormatError,
                    key_str << ": key syntax error.");
        }
        ++ch;

        // port
        port = atoi(ch);
        return;
    } else if (key_str.compare(0, 5, NS_KEY_V2_PREFIX) == 0) {
        version = 2;
        ch += 5;
        do {
            // hostname
            for (;*ch && *ch != '/'; ++ch)
                host += *ch;
            if (*ch == 0) break;
            ++ch;
            // port
            port = atoi(ch);
            while (*ch && *ch != '/') ++ch;
            if (*ch == 0) break;
            ++ch;
            // queue
            for (;*ch && *ch != '/'; ++ch)
                queue += *ch;
            if (*ch == 0) break;
            ++ch;
            // id
            id = (unsigned) atoi(ch);
            while (*ch && *ch != '/') ++ch;
            if (*ch) {
                // run
                run = atoi(ch+1);
            } else {
                run = -1;
            }
            return;
        } while (0);
    } else if (isdigit(*ch)) {
        version = 0;
        id = (unsigned) atoi(ch);
        while (*ch && *ch != '/') ++ch;
        if (*ch) {
            ch += 1;
            if (!isdigit(*ch))
                NCBI_THROW_FMT(CNetScheduleException, eKeyFormatError,
                        key_str << ": key syntax error.");
            // run
            run = atoi(ch);
        } else {
            run = -1;
        }
        return;
    }

    NCBI_THROW_FMT(CNetScheduleException, eKeyFormatError,
            key_str << ": key syntax error.");
}

#define MAX_INT_TO_STR_LEN(type) (sizeof(type) * 3 / 2)

CNetScheduleKeyGenerator::CNetScheduleKeyGenerator(
    const string& host, unsigned port)
{
    string port_str(NStr::IntToString(port));

    m_V1HostPort.reserve(1 + host.size() + 1 + port_str.size());
    m_V1HostPort.push_back('_');
    m_V1HostPort.append(host);
    m_V1HostPort.push_back('_');
    m_V1HostPort.append(port_str);

    m_V2Prefix.reserve(NS_KEY_V2_PREFIX_LEN + m_V1HostPort.size());
    m_V2Prefix.append(NS_KEY_V2_PREFIX, NS_KEY_V2_PREFIX_LEN);
    m_V2Prefix.append(host);
    m_V2Prefix.push_back('/');
    m_V2Prefix.append(port_str);
    m_V2Prefix.push_back('/');
}

void CNetScheduleKeyGenerator::GenerateV1(string* key, unsigned id) const
{
    key->reserve(NS_KEY_V1_PREFIX_LEN +
        MAX_INT_TO_STR_LEN(unsigned) +
        m_V1HostPort.size());
    key->append(NS_KEY_V1_PREFIX, NS_KEY_V1_PREFIX_LEN);
    key->append(NStr::IntToString(id));
    key->append(m_V1HostPort);
}

void CNetScheduleKeyGenerator::GenerateV2(string* key, unsigned id,
    const string& queue, int run /* = -1 */) const
{
    key->reserve(m_V2Prefix.size() +
        queue.size() +
        1 +
        MAX_INT_TO_STR_LEN(unsigned) +
        1 +
        MAX_INT_TO_STR_LEN(int));
    key->append(m_V2Prefix);
    key->append(queue);
    key->push_back('/');
    key->append(NStr::IntToString(id));
    if (run >= 0) {
        key->push_back('/');
        key->append(NStr::IntToString(run));
    }
}


END_NCBI_SCOPE
