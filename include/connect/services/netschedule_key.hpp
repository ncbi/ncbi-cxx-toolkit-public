#ifndef CONN___NETSCHEDULE_KEY__HPP
#define CONN___NETSCHEDULE_KEY__HPP

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
 * Authors: Maxim Didenko, Victor Joukov, Dmitry Kazimirov
 *
 * File Description:
 *   NetSchedule client API.
 *
 */


#include <connect/connect_export.h>
#include <connect/ncbi_connutil.h>

#include <corelib/ncbistl.hpp>

#include <string>
#include <map>
#include <vector>

/// @file netschedule_client.hpp
/// NetSchedule client specs.
///



BEGIN_NCBI_SCOPE


/** @addtogroup NetScheduleClient
 *
 * @{
 */

/// Meaningful information encoded in the NetSchedule key
///
struct NCBI_XCONNECT_EXPORT CNetScheduleKey
{
    explicit CNetScheduleKey(const string& str_key);

    unsigned     version;   ///< Key version
    string       host;      ///< Server name
    unsigned     port;      ///< TCP/IP port number
    string       queue;     ///< Queue name, optional
    unsigned     id;        ///< Job id
    int          run;       ///< Job run number, -1 - no run
};

class NCBI_XCONNECT_EXPORT CNetScheduleKeyGenerator
{
public:
    CNetScheduleKeyGenerator(const string& host, unsigned port);

    string GenerateV1(unsigned id) const;
    void GenerateV1(string* key, unsigned id) const;

    string GenerateV2(unsigned id, const string& queue, int run = -1) const;
    void GenerateV2(string* key, unsigned id,
        const string& queue, int run = -1) const;

private:
    string m_V1HostPort;
    string m_V2Prefix;
};

inline string CNetScheduleKeyGenerator::GenerateV1(unsigned id) const
{
    string key;
    GenerateV1(&key, id);
    return key;
}

inline string CNetScheduleKeyGenerator::GenerateV2(unsigned id,
    const string& queue, int run /* = -1 */) const
{
    string key;
    GenerateV2(&key, id, queue, run);
    return key;
}

/* @} */


END_NCBI_SCOPE


#endif  /* CONN___NETSCHEDULE_KEY__HPP */
