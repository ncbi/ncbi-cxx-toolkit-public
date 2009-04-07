#ifndef CONNECT_SERVICES___NETSERVICE_API__HPP
#define CONNECT_SERVICES___NETSERVICE_API__HPP

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
 * Authors:  Dmitry Kazimirov, Maxim Didenko
 *
 * File Description:
 *   Network client for ICache (NetCache).
 *
 */

#include "srv_discovery.hpp"

BEGIN_NCBI_SCOPE

struct SNetServerGroupIteratorImpl;

class NCBI_XCONNECT_EXPORT CNetServerGroupIterator
{
    NET_COMPONENT(NetServerGroupIterator);

    CNetServer GetServer();
    CNetServer operator *() {return GetServer();}

    bool Next();
    CNetServerGroupIterator& operator ++() {Next(); return *this;}
};

struct SNetServerGroupImpl;

class NCBI_XCONNECT_EXPORT CNetServerGroup
{
    NET_COMPONENT(NetServerGroup);

    int GetCount() const;
    CNetServer GetServer(int index);

    CNetServerGroupIterator Iterate();
};

class INetServerFinder : public CNetObject
{
public:
    virtual bool Consider(CNetServer server) = 0;
};

struct SNetServiceImpl;

class NCBI_XCONNECT_EXPORT CNetService
{
    NET_COMPONENT(NetService);

    const string& GetClientName() const;
    const string& GetServiceName() const;

    enum EServerSortMode {
        eSortByLoad,
        eRandomize,
        eSortByAddress
    };

    CNetServerGroup DiscoverServers(EServerSortMode sort_mode = eSortByLoad);

    bool FindServer(const CNetObjectRef<INetServerFinder>& finder,
        EServerSortMode sort_mode = eRandomize);

    CNetServerConnection GetBestConnection();

    bool IsLoadBalanced() const;

    void SetPermanentConnection(ESwitch type);

    void DiscoverLowPriorityServers(ESwitch on_off);

    void SetRebalanceStrategy(IRebalanceStrategy* strategy);

    void SetCommunicationTimeout(const STimeout& to);
    const STimeout& GetCommunicationTimeout() const;

    void SetCreateSocketMaxRetries(unsigned int retries);
    unsigned int GetCreateSocketMaxRetries() const;

    enum ECmdOutputStyle {
        eSingleLineOutput,
        eDumpNoHeaders,
        eMultilineOutput,
        eMultilineOutput_NetCacheStyle
    };

    void PrintCmdOutput(const string& cmd,
        CNcbiOstream& output_stream,
        ECmdOutputStyle output_style);
};

END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES___NETSERVICE_API__HPP */
