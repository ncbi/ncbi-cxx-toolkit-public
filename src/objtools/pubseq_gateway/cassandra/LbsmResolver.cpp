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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 *  Wrapper class around lbsm "C"-API
 *
 */

#include <ncbi_pch.hpp>

#include <memory>
#include <vector>
#include <sstream>
#include <algorithm>

#include <connect/ncbi_connutil.h>
#include <objtools/pubseq_gateway/impl/cassandra/LbsmResolver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>

BEGIN_NCBI_SCOPE;

bool LbsmLookup::Resolve(const string& Service, vector<pair<string, int> >& Result, TSERV_Type ServType) {

    unique_ptr<SConnNetInfo, function<void(SConnNetInfo*)> > net_info(ConnNetInfo_Create(Service.c_str()),
        [](SConnNetInfo* net_info) {
            ConnNetInfo_Destroy(net_info);
        }
    );
    unique_ptr<SSERV_IterTag, function<void(SSERV_IterTag*)> > iter(
        SERV_Open(
            Service.c_str(),
            ServType,
            0,                  // prefered host
            net_info.get()
        ),
        [](SSERV_IterTag* iter) {
            SERV_Close(iter);
        }
    );
    const SSERV_Info *pInfo;
    while (pInfo = SERV_GetNextInfo(iter.get()), pInfo != NULL) {
        char sIpBuff[128];
        SOCK_HostPortToString(pInfo->host, pInfo->port, sIpBuff, sizeof(sIpBuff));
        Result.push_back(make_pair(sIpBuff, pInfo->rate));
    }
    sort(Result.begin(), Result.end(), 
        [](const pair<string, int> & a, const pair<string, int> & b) -> bool {
            return a.second > b.second;
        }
    );
    return true;
}

string LbsmLookup::Resolve(const string& Service, char Delimiter, TSERV_Type ServType) {
    stringstream rv;
    vector<pair<string,int> > Result;
    if (Resolve(Service, Result, ServType)) {
        bool is_first = true;
        for (auto const & el : Result) {
            if (!is_first)
                rv << Delimiter;
            is_first = false;
            rv << el.first;
        }
    }
    return rv.str();
}

END_NCBI_SCOPE;
