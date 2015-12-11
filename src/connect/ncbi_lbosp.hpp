#ifndef CONNECT___NCBI_LBOSP__HPP
#define CONNECT___NCBI_LBOSP__HPP
/*
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
* Authors:  Dmitriy Elisov
* @file
* File Description:
*   Unit test functions for C++ wrapper for service discovery API based on LBOS.
*/

#include <corelib/ncbiexpt.hpp>
#include <connect/ncbi_lbos.hpp>

BEGIN_NCBI_SCOPE


/// Composite key for "hostname<->IP" cache
class CLBOSIpCacheKey
{
public:
    CLBOSIpCacheKey(string service, string hostname, string version,
                    unsigned short port);

    bool operator==(const CLBOSIpCacheKey& rh) const;

    bool operator<(const CLBOSIpCacheKey& rh) const;

    bool operator>(const CLBOSIpCacheKey& rh) const;
private:
    string x_Service;
    string x_Hostname;
    string x_Version;
    unsigned short x_Port;
};


class NCBI_XCONNECT_EXPORT CLBOSIpCache
{
public:
    /** Search the cache for previously resolved hostname with a given service name,
    * version, port. If not found - return the same hostname.
    */
    static
    string HostnameTryFind(string service, string hostname, string version,
    unsigned short port);


    /** Search the cache for previously resolved hostname with a given service name,
    * version, port. If not found - resolve, cache and return result.
    */
    static
        string HostnameResolve(string service, string hostname,
        string version, unsigned short port);


    /** We do not need to store hostname<->IP record after the server is
    * de-announced. So we just make sure that we delete the record only one time,
    * in case if many threads are going to delete it at the same moment */
    static
        void HostnameDelete(string service, string hostname, string version,
        unsigned short port);

private:
    static CSafeStatic< map< CLBOSIpCacheKey, string > > x_IpCache;
};


END_NCBI_SCOPE

#endif /* CONNECT___NCBI_LBOSP__HPP */
