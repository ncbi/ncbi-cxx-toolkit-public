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
 * Author: Dmitri Dmitrienko
 *
 */

#include <ncbi_pch.hpp>
#include <string>
#include <memory>
#include <mutex>

#include <objtools/pubseq_gateway/impl/rpc/DdRpcClient.hpp>
#include <objtools/pubseq_gateway/impl/rpc/HttpClientTransportP.hpp>

using namespace std;

namespace DDRPC {


#define DEFAULT_ALIVE_TTL_MS 60000
#define DEFAULT_DEAD_TTL_MS  5000

#define WEIGHT_NORMALIZE_VALUE 256


thread_local unordered_map<string, shared_ptr<ServiceResolver::ResolverElement>> ServiceResolver::m_local_map;
thread_local uint32_t ServiceResolver::m_local_version;
thread_local end_point_weight_t ServiceResolver::m_weight_counter;
thread_local vector<end_point_weight_t> ServiceResolver::m_stagering;

ServiceResolver::ServiceResolver(const vector<pair<string, HCT::http2_end_point>>* StaticMapping) :
    m_version(1),
    m_alive_TTL_ms(DEFAULT_ALIVE_TTL_MS),
    m_dead_TTL_ms(DEFAULT_DEAD_TTL_MS)
{
    if (StaticMapping) {
        unique_lock<mutex> _(m_map_mux);
        for (const auto& it : *StaticMapping) {
            auto existing_it = m_static_map.find(it.first);
            if (existing_it == m_static_map.end()) {
                auto vec = make_shared<ResolverElement>();
                existing_it = m_static_map.emplace(it.first, vec).first;
            }
            existing_it->second->m_end_of_live = chrono::system_clock::now() + chrono::milliseconds(m_alive_TTL_ms);
            existing_it->second->m_end_points.emplace_back(
                make_pair(
                    make_shared<HCT::http2_end_point>(HCT::http2_end_point(it.second)),
                    1
                )
            );
        }
    }
}

void ServiceResolver::PopulateFromStatic(const string& ServiceId)
{
    auto vec = make_shared<ResolverElement>();
    time_point_t t = chrono::system_clock::now();

// TODO begin:
// ADD REAL RESOLVER

    auto it = m_static_map.find(ServiceId);
    if (vec->m_end_points.size() == 0) {
        if (it == m_static_map.end() || it->second->m_end_points.size() == 0) {
            shared_ptr<HCT::http2_end_point> ep = make_shared<HCT::http2_end_point>();
            stringstream ss;
            ss << "Failed to resolve " << ServiceId << " service";
            ep->error.error(ERR_DDRPC_SERVICE_UNRESOLVED, ss.str().c_str());
            end_point_weight_t ep_weight = 1;
            vec->m_end_points.emplace_back(ep, ep_weight);
            vec->m_end_of_live = t + chrono::milliseconds(m_dead_TTL_ms);
        }
        else {
            vec->m_end_points.assign(it->second->m_end_points.begin(), it->second->m_end_points.end());
            vec->m_end_of_live = t + chrono::milliseconds(m_alive_TTL_ms);
        }
    }
    else {
        if (it != m_static_map.end()) {
            std::copy(it->second->m_end_points.begin(), it->second->m_end_points.end(), std::back_inserter(vec->m_end_points));
            vec->m_end_of_live = t + chrono::milliseconds(m_alive_TTL_ms);
        }
    }
    assert(vec->m_end_points.size() > 0);
    m_map.emplace(ServiceId, std::move(vec));
}

shared_ptr<ServiceResolver::ResolverElement> ServiceResolver::ResolveService(const string& ServiceId)
{
    unique_lock<mutex> _(m_map_mux);
    auto it = m_map.end();
    if ((it = m_map.find(ServiceId)) == m_map.end() || it->second->m_end_of_live < chrono::system_clock::now()) {
        PopulateFromStatic(ServiceId);
        it = m_map.find(ServiceId);
        assert(it != m_map.end());
        assert(it->second->m_end_points.size() > 0);
    }
    return it->second;
}

shared_ptr<HCT::http2_end_point> ServiceResolver::SericeIdToEndPoint(const string& ServiceId)
{
    auto it = m_local_map.end();
    if (m_local_version != m_version || (it = m_local_map.find(ServiceId)) == m_local_map.end() || it->second->m_end_of_live < chrono::system_clock::now()) {
        shared_ptr<ServiceResolver::ResolverElement> vec = ResolveService(ServiceId);
        it = m_local_map.emplace(ServiceId, vec).first;
        assert(it != m_local_map.end());
        assert(it->second->m_end_points.size() > 0);
    }
    size_t index = 0;
    size_t size = it->second->m_end_points.size();
    assert(size > 0);
    if (size > 1) {
        end_point_weight_t total_w = 0;
        m_stagering.resize(it->second->m_end_points.size());
        size_t i = 0;
        for (auto & pep : it->second->m_end_points) {
            end_point_weight_t w = pep.second;
            m_stagering[i++] = w;
            total_w += w;
        }
        if (total_w <= 0) total_w = 1;

        int32_t counter = (++m_weight_counter) % WEIGHT_NORMALIZE_VALUE;
        for (auto & el : m_stagering) {
            counter -= (el * WEIGHT_NORMALIZE_VALUE) / total_w;
            if (counter < 0) break;
            ++index;
        }
    }
    assert(it->second->m_end_points[index].first.get());
    return it->second->m_end_points[index].first;
}


/** DdRpcClient */

bool DdRpcClient::m_Initialized(false);
unique_ptr<ServiceResolver> DdRpcClient::m_Resolver;

void DdRpcClient::Init(unique_ptr<ServiceResolver> Resolver)
{
    if (m_Initialized)
        EDdRpcException::raise("DDRPC has already been initialized");
    m_Initialized = true;
    m_Resolver.swap(Resolver);
    HCT::HttpClientTransport::Init();
}

void DdRpcClient::Finalize()
{
    if (!m_Initialized)
        EDdRpcException::raise("DDRPC has not been initialized");
    m_Initialized = false;
    HCT::HttpClientTransport::Finalize();
}


};
