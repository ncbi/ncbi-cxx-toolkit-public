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
* Author:  Aaron Ucko
*
* File Description:
*   Help distribute connections within a pool across servers.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbi_pool_balancer.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <corelib/ncbierror.hpp>
#include <corelib/error_codes.hpp>

#include <numeric>
#include <random>

#define NCBI_USE_ERRCODE_X Corelib_Balancer

BEGIN_NCBI_SCOPE

DEFINE_STATIC_FAST_MUTEX(s_RandomMutex);
static CSafeStatic<default_random_engine> s_RandomEngine;
static bool s_RandomnessSeeded;

class CRandomGuard
{
public:
    CRandomGuard()
        : m_MutexGuard(s_RandomMutex)
        {
            if ( !s_RandomnessSeeded ) {
                random_device rdev;
                s_RandomEngine->seed(rdev());
                s_RandomnessSeeded = true;
            }
        }

private:
    CFastMutexGuard m_MutexGuard;
};


CPoolBalancer::CPoolBalancer(const string& service_name,
                             const IDBServiceMapper::TOptions& options,
                             TFlags flags)
    : m_ServiceName(service_name), m_TotalCount(0U), m_Flags(flags)
{
    for (auto it : options) {
        CTempString name = it->GetName();
        CEndpointKey key(it->GetHost(), it->GetPort());
        if (key == 0  &&  name != service_name) {
            key = x_NameToKey(name);
            if (key != 0) {
                if ((flags & fIgnoreRawIPs)  &&  name == it->GetName() ) {
                    continue;
                }
                it.Reset(new CDBServerOption(name, key.GetHost(),
                                             key.GetPort(),
                                             it->GetRanking(), it->GetState(),
                                             it->GetExpireTime()));
            }
        }
        _TRACE_X(1,
                 service_name << ": " << key << " -> " << name << " per DBLB");

        auto endpoint = x_FindEndpoint(key, name);
        if (endpoint == m_Endpoints.end()) {
            SEndpointInfo info;
            info.ref = it;
            endpoint = m_Endpoints.emplace(key, info);
        }

        double ranking  = it->GetRanking();

        if (it->IsPenalized()) {
            ranking *= numeric_limits<double>::epsilon();
            ++endpoint->second.penalty_level;
        }
        if (it->IsExcluded()) {
            ranking *= numeric_limits<double>::epsilon();
            ++endpoint->second.penalty_level;
        }
        
        endpoint->second.ref = it;
        endpoint->second.effective_ranking = ranking;
        m_Rankings.insert(ranking);
    }
}


void CPoolBalancer::LocallyPenalize(TSvrRef server)
{
    if (server.Empty()) {
        return;
    }
    CEndpointKey key(server->GetHost(), server->GetPort());
    auto it = x_FindEndpoint(key, server->GetName());
    if (it == m_Endpoints.end()) {
        return;
    }
    m_Rankings.erase(m_Rankings.find(it->second.effective_ranking));
    ++it->second.penalty_level;
    it->second.effective_ranking *= numeric_limits<double>::epsilon();
    m_Rankings.insert(it->second.effective_ranking);
}


void CPoolBalancer::x_InitFromCounts(const TCounts& counts)
{
    if (m_TotalCount != 0) {
        for (auto& it : m_Endpoints) {
            it.second.actual_count = 0;
        }
        m_TotalCount = 0;
    }
    double legacy_base_ranking = 0.0;
    for (const auto& cit : counts) {
        CTempString  name  = cit.first;
        auto         key   = x_NameToKey(name);
        auto         eit   = m_Endpoints.lower_bound(key);
        CTime        exp(CTime::eEmpty, CTime::eUTC);
        if ((eit == m_Endpoints.end()
             ||  key.GetHost() != eit->first.GetHost()
             ||  (key.GetPort() != 0
                  &&  key.GetPort() != eit->first.GetPort()))
            &&  ( !(m_Flags & fIgnoreRawIPs)  ||  key == 0
                 ||  name != cit.first)) {
            _TRACE_X(2,
                     m_ServiceName << ": " << key << " -> " << name
                     << " per existing connection(s)");
            eit = m_Endpoints.insert(eit, make_pair(key, SEndpointInfo()));
        }
        if ( eit != m_Endpoints.end() ) {
            auto& endpoint = eit->second;
            if (endpoint.ref.Empty()) {
                if (legacy_base_ranking == 0.0) {
                    if (m_Rankings.empty()) {
                        legacy_base_ranking = 1.0; // arbitrary >0
                    } else {
                        legacy_base_ranking = 1e-4 * *m_Rankings.begin();
                    }
                }
                // scaled Poisson distribution peaking around 4 and 5,
                // using a hardcoded approximation of log(5).
                double ranking
                    = (std::exp(1.61 * cit.second - lgamma(cit.second + 1))
                       * legacy_base_ranking);

                if (exp.IsEmpty()) {
                    exp.SetCurrent();
                    exp.AddSecond(10);
                }
                endpoint.ref.Reset(new CDBServerOption
                                   (name, key.GetHost(), key.GetPort(),
                                    ranking,
                                    CDBServerOption::fState_Normal,
                                    exp.GetTimeT()));
                m_Rankings.insert(ranking);
                endpoint.effective_ranking = ranking;
            }
            endpoint.actual_count += cit.second;
        }
        m_TotalCount += cit.second;
    }
}


TSvrRef CPoolBalancer::x_GetServer(const void* params, IBalanceable** conn)
{
    TSvrRef      result;
    CEndpointKey conn_key(0u);
    
    // trivial if <= 1 endpoint
    if (m_Endpoints.empty()) {
        return result;
    } else if (m_Endpoints.size() == 1) {
        return TSvrRef(&*m_Endpoints.begin()->second.ref);
    }

    double total_ranking = accumulate(m_Rankings.begin(), m_Rankings.end(),
                                      0.0);
    if (total_ranking <= 0.0) {
        ERR_POST_X(3, Info << "No positive rankings found");
        return result;
    }

    if (/* m_TotalCount > 1  && */  conn != nullptr  &&  params != nullptr) {
        *conn = x_TryPool(params);
        if (*conn != NULL) {
            const string&  server_name  = (*conn)->ServerName();
            Uint4          host         = (*conn)->Host();
            Uint2          port         = (*conn)->Port();
            double         excess;
            bool           keep         = true;
            string         penalty_note;
            conn_key = CEndpointKey(host, port);
            auto it = m_Endpoints.find(conn_key);
            if (it == m_Endpoints.end()) {
                ERR_POST_X(4,
                           "Unrecognized endpoint for existing connection to "
                           << conn_key << " (" << server_name << ')');
                excess = x_GetCount(params, server_name);
                time_t t = CurrentTime(CTime::eUTC).GetTimeT() + 10;
                result.Reset(new CDBServer(server_name, host, port, t));
            } else {
                double scale_factor = m_TotalCount / total_ranking;
                excess = (it->second.actual_count
                          - it->second.effective_ranking * scale_factor);
                result.Reset(&*it->second.ref);
                if (it->second.penalty_level > 0) {
                    auto min_penalty = it->second.penalty_level;
                    for (auto it2 : m_Endpoints) {
                        min_penalty = min(min_penalty,
                                          it2.second.penalty_level);
                    }
                    if (min_penalty < it->second.penalty_level) {
                        keep = false;
                        penalty_note = FORMAT(
                            "; penalty level " << it->second.penalty_level
                            << " exceeds minimum value " << min_penalty);
                    }
                }
            }
            _TRACE_X(5,
                     "Considering connection to " << conn_key << " ("
                     << server_name
                     << ") for turnover; projected excess count " << excess
                     << penalty_note);
            if (keep  &&  excess > 0.0) {
                unsigned int pool_max = x_GetPoolMax(params);
                if (pool_max == 0u) {
                    pool_max = m_TotalCount * 2;
                }
                CRandomGuard rg;
                uniform_real_distribution<double> urd(0.0, pool_max);
                if (urd(*s_RandomEngine) <= excess) {
                    keep = false;
                }
            }
            if (keep) {
                _TRACE_X(6, "Sparing connection immediately");
                return result;
            } else {
                // defer turnover (endpoint may be reselected!) but
                // speculatively update counts
                --m_TotalCount;
                if (it != m_Endpoints.end()) {
                    --it->second.actual_count;
                }
            }
        }
    }

    vector<TEndpoints::value_type*> options;
    vector<double> weights;
    double scale_factor = (m_TotalCount + 1.0) / total_ranking;
    _TRACE_X(7,
             "Scale factor for new connection: " << (m_TotalCount + 1) << " / "
             << total_ranking << " = " << scale_factor);

    for (auto& it : m_Endpoints) {
        it.second.ideal_count = it.second.effective_ranking * scale_factor;
        double d = it.second.ideal_count - it.second.actual_count;
        _TRACE_X(8,
                 it.first << " (" << it.second.ref->GetName()
                 << "): current count " << it.second.actual_count
                 << ", ideal count " << it.second.ideal_count << ", delta "
                 << d << (d > 0 ? " > 0" : " <= 0"));
        if (d > 0) {
            options.push_back(&it);
            weights.push_back(d);
        }
    }
    if (weights.empty()) {
        ERR_POST_X(9, "No positive deltas");
        return result;
    }

    CRandomGuard rg;
#if defined(NCBI_COMPILER_MSVC)  &&  _MSC_VER < 1900
    // Work around limitation in VS 2013's discrete_distribution<>
    // mitigated by a non-standard initializer_list<> constructor.
    discrete_distribution<> dd(
        initializer_list<double>(
            weights.data(), weights.data() + weights.size()));
#else
    discrete_distribution<> dd(weights.begin(), weights.end());
#endif
    auto i = dd(*s_RandomEngine);
    _TRACE_X(10,
             "Picked " << options[i]->first << " ("
             << options[i]->second.ref->GetName() << ')');
    if (conn != NULL  &&  *conn != NULL) {
        if (conn_key == options[i]->first) {
            _TRACE_X(11, "Sparing connection (endpoint reselected)");
            ++options[i]->second.actual_count;
            ++m_TotalCount;
        } else {
            _TRACE_X(12, "Proceeding to request turnover");
            auto to_discard = *conn;
            *conn = nullptr;
            x_Discard(params, to_discard);
        }
    }
    return TSvrRef(&*options[i]->second.ref);
}

CEndpointKey CPoolBalancer::x_NameToKey(CTempString& name) const
{
    _TRACE_X(13, name);
    CTempString  address  = name;
    SIZE_TYPE    pos      = name.find_last_not_of("0123456789.:");
    if (pos != NPOS) {
        if (name[pos] == '@') {
            address = name.substr(pos + 1);
            name    = name.substr(0, pos);
        } else {
            for (const auto& it : m_Endpoints) {
                if (it.first > 0  &&  it.second.ref->GetName() == name) {
                    _TRACE_X(14, "Found at " << it.first);
                    return it.first;
                }
            }
            return 0;
        }
    }
    CEndpointKey key(address, NStr::fConvErr_NoThrow);
    if (key == 0) {
        ERR_POST_X(15, "Error parsing " << address << ": "
                   << CNcbiError::GetLast().Extra());
    }
    _TRACE_X(16, key);
    return key;
}


CPoolBalancer::TEndpoints::iterator CPoolBalancer::x_FindEndpoint(
    CEndpointKey key, CTempString name)
{
    auto it = x_FindEndpointAsIs(key, name);
    if (it == m_Endpoints.end()) {
        auto host = key.GetHost();
        auto port = key.GetPort();
        if (port != 0U) {
            it = x_FindEndpointAsIs(CEndpointKey(host, 0U), name);
        }
        if (it == m_Endpoints.end()  &&  host != 0U) {
            it = x_FindEndpointAsIs(CEndpointKey(0U, port), name);
        }
        if (it == m_Endpoints.end()  &&  host != 0U  &&  port != 0U) {
            it = x_FindEndpointAsIs(CEndpointKey(0U, 0U), name);
        }
    }
    return it;
}


CPoolBalancer::TEndpoints::iterator CPoolBalancer::x_FindEndpointAsIs(
    CEndpointKey key, CTempString name)
{
    auto it = m_Endpoints.lower_bound(key);
    if (it == m_Endpoints.end()  ||  it->first != key) {
        return m_Endpoints.end();
    } else if (key.GetHost() == 0U /* ||  key.GetPort() == 0U */) {
        for (;  it != m_Endpoints.end()  &&  it->first == key;  ++it) {
            if (it->second.ref->GetName() == name) {
                return it;
            }
        }
        return m_Endpoints.end();
    }
    return it;
}

END_NCBI_SCOPE
