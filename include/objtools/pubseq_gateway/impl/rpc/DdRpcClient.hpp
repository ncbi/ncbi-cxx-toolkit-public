#ifndef DDRPCCLIENT__HPP
#define DDRPCCLIENT__HPP

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
 */

#include <memory>
#include <string>
#include <mutex>
#include <stdexcept>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <chrono>
#include <tuple>

#include "HttpClientTransport.hpp"

namespace HCT {

class io_future;
class http2_request;
class http2_end_point;

};

namespace DDRPC {

#define ERR_DDRPC_SERVICE_UNRESOLVED  0x20001


using time_point_t = std::chrono::time_point<std::chrono::system_clock>;
using end_point_weight_t = uint32_t;

class ServiceResolver {
private:
    struct ResolverElement
    {
        time_point_t m_end_of_live;
        std::vector<std::pair<std::shared_ptr<HCT::http2_end_point>, end_point_weight_t>> m_end_points;
        ResolverElement() : m_end_of_live(std::chrono::milliseconds::zero()) {}
    };
    std::mutex m_map_mux;
    std::unordered_map<std::string, std::shared_ptr<ResolverElement>> m_static_map;
    std::unordered_map<std::string, std::shared_ptr<ResolverElement>> m_map;
    uint32_t m_version;
    uint32_t m_alive_TTL_ms;
    uint32_t m_dead_TTL_ms;
    static thread_local std::unordered_map<std::string, std::shared_ptr<ResolverElement>> m_local_map;
    static thread_local uint32_t m_local_version;
    static thread_local end_point_weight_t m_weight_counter;
    static thread_local std::vector<end_point_weight_t> m_stagering;
    void PopulateFromStatic(const std::string& ServiceId);
    std::shared_ptr<ResolverElement> ResolveService(const std::string& ServiceId);
    friend class Request;
public:
    ServiceResolver(const std::vector<std::pair<std::string, HCT::http2_end_point>>* StaticMapping = nullptr);
    virtual ~ServiceResolver() {}
    virtual std::shared_ptr<HCT::http2_end_point> SericeIdToEndPoint(const std::string& ServiceId);
};

class RequestBase {
protected:
    HCT::TagHCT m_tag;
    std::shared_ptr<HCT::http2_request> m_Pimpl;
    RequestBase(HCT::TagHCT tag = 0);
public:
    ~RequestBase();
    bool IsDone() const;
    bool IsCancelled() const;
    std::string Get();
    bool HasError() const;
    std::string GetErrorDescription() const;
    HCT::TagHCT GetTag() {
        return m_tag;
    }
    void SetTag(HCT::TagHCT tag) {
        m_tag = tag;
    }
};

class Request: public RequestBase {
private:
    Request(HCT::TagHCT tag = 0);
    void RunRequest(const std::string& ServiceId, std::string SerializedArgs);
    friend class DdRpcClient;
public:
    void Wait();
    void WaitAny();
};


class CRequestQueue;

class CRequestQueueItem: public RequestBase {
private:
    CRequestQueueItem(std::shared_ptr<HCT::io_future> afuture, std::tuple<std::string, std::string, HCT::TagHCT> args);
    void WaitFor(long timeout_ms);
    void Wait();
    void Cancel();
    friend class CRequestQueue;
public:
};

class CRequestQueue {
private:
    mutable std::mutex m_items_mtx;
    std::vector<std::unique_ptr<CRequestQueueItem>> m_items;
    std::shared_ptr<HCT::io_future> m_future;
public:
    CRequestQueue();
    ~CRequestQueue();
    void Submit(std::vector<std::tuple<std::string, std::string, HCT::TagHCT>>& requests, const std::chrono::duration<long, std::chrono::milliseconds::period>& timeout);
    std::vector<std::unique_ptr<CRequestQueueItem>> WaitResults(const std::chrono::duration<long, std::chrono::milliseconds::period>& timeout);
    bool IsEmpty() const;
    void Clear();
};


class DdRpcClient final {
private:
    static std::unique_ptr<ServiceResolver> m_Resolver;
    static bool m_Initialized;
public:
    static void Init(std::unique_ptr<ServiceResolver> Resolver);
    static void Finalize();
    static std::unique_ptr<Request> AsyncRequest(const std::string& ServiceId, std::string SerializedArgs, HCT::TagHCT tag = 0);
    static std::string SyncRequest(const std::string& ServiceId, std::string SerializedArgs);
    static std::shared_ptr<HCT::http2_end_point> SericeIdToEndPoint(const std::string& ServiceId) {
        if (!m_Resolver)
            EDdRpcException::raise("Resolver is not assigned");
        return m_Resolver->SericeIdToEndPoint(ServiceId);
    }
};

};

#endif
