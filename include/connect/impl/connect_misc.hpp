#ifndef CONNECT__IMPL__SOCKET_ADDRESS__HPP
#define CONNECT__IMPL__SOCKET_ADDRESS__HPP

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
 * Authors: Rafael Sadyrov
 *
 */

#include <connect/connect_export.h>

#include <corelib/ncbistl.hpp>
#include <corelib/ncbistr.hpp>

#include <chrono>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

BEGIN_NCBI_SCOPE

struct NCBI_XCONNECT_EXPORT SSocketAddress
{
    unsigned host;
    unsigned short port;

    struct NCBI_XCONNECT_EXPORT SHost
    {
        enum class EName { eResolved, eOriginal };

        std::optional<string> name;
        unsigned host;

        SHost(unsigned h) : host(h) {}
        SHost(const string& h, EName n = EName::eResolved);
    };

    struct NCBI_XCONNECT_EXPORT SPort
    {
        unsigned short port;
        SPort(unsigned short p) : port(p) {}
        SPort(const string& p)  : port(x_GetPort(p)) {}
        SPort(CTempString p)    : port(x_GetPort(p)) {}

    private:
        static unsigned short x_GetPort(CTempString p);
    };

    SSocketAddress(SHost h, SPort p) : host(h.host), port(p.port), m_Name(h.name) {}

    explicit operator bool() const { return host && port; }
    string GetHostName() const;
    string AsString() const { return GetHostName() + ':' + NStr::UIntToString(port); }

    static SSocketAddress Parse(const string& address, SHost::EName name = SHost::EName::eResolved);

    friend ostream& operator<<(ostream& os, const SSocketAddress& address) { return os << address.AsString(); }

private:
    std::optional<string> m_Name;
};

NCBI_XCONNECT_EXPORT bool operator==(const SSocketAddress& lhs, const SSocketAddress& rhs);
NCBI_XCONNECT_EXPORT bool operator< (const SSocketAddress& lhs, const SSocketAddress& rhs);

class NCBI_XCONNECT_EXPORT CServiceDiscovery
{
public:
    CServiceDiscovery(const string& service_name, SSocketAddress::SHost::EName name = SSocketAddress::SHost::EName::eOriginal);

    using TServer = pair<SSocketAddress, double>;
    using TServers = vector<TServer>;
    TServers operator()();

    const string& GetServiceName() const { return m_ServiceName; }
    bool IsSingleServer() const { return m_IsSingleServer; }

    static TServers DiscoverImpl(const string&, unsigned, shared_ptr<void>&, pair<string, const char*>, int, unsigned long);

private:
    const string m_ServiceName;
    shared_ptr<void> m_Data;
    const bool m_IsSingleServer;
};

template <class TType>
struct SThreadSafe
{
    template <class T>
    struct SLock : private unique_lock<std::mutex>
    {
        T& operator*()  { _ASSERT(m_Object); return *m_Object; }
        T* operator->() { _ASSERT(m_Object); return  m_Object; }

        // More convenient RAII alternative to explicit scopes or 'unlock' method.
        // It allows locks to be declared inside 'if' condition.
        using unique_lock<std::mutex>::operator bool;

        void Unlock() { m_Object = nullptr; unlock(); }

    private:
        SLock(T* c, std::mutex& m) : unique_lock(m), m_Object(c) { _ASSERT(m_Object); }

        T* m_Object;

        friend struct SThreadSafe;
    };

    template <class... TArgs>
    SThreadSafe(TArgs&&... args) : m_Object(std::forward<TArgs>(args)...) {}

    SLock<      TType> GetLock()       { return { &m_Object, m_Mutex }; }
    SLock<const TType> GetLock() const { return { &m_Object, m_Mutex }; }

    // Direct access to the protected object (e.g. to access atomic members).
    // All thread-safe members must be explicitly marked volatile to be available.
          volatile TType* operator->()       { return &m_Object; }
    const volatile TType* operator->() const { return &m_Object; }

protected:
    mutable mutex m_Mutex;

private:
    TType m_Object;
};

class NCBI_XCONNECT_EXPORT CLogLatencies
{
public:
    enum EWhich { eOff, eFirst, eLast, eAll };

    struct SRegex : regex
    {
        template <size_t SIZE> SRegex(const char (&s)[SIZE]) : regex(s, SIZE - 1) {}
    };

    CLogLatencies(SRegex start, SRegex stop, std::optional<SRegex> server_side = nullopt) :
        m_Start(std::move(start)),
        m_Stop(std::move(stop)),
        m_ServerSide(std::move(server_side))
    {}

    void SetWhich(EWhich which) { m_Which = which; }
    void SetDebug(bool debug) { m_Debug = debug; }

    using TData = deque<SDiagMessage>;
    using TLatency = chrono::microseconds;
    using TServerSide = string;
    using TServerResult = tuple<TLatency, TServerSide>;
    using TResult = map<string, TServerResult>;
    TResult Parse(const TData& data);

private:
    regex m_Start;
    regex m_Stop;
    std::optional<regex> m_ServerSide;
    EWhich m_Which = eOff;
    bool m_Debug = false;
};

class NCBI_XCONNECT_EXPORT CLogLatencyReport : public CLogLatencies
{
public:
    template <class... TArgs>
    CLogLatencyReport(string filter, TArgs&&... args) : CLogLatencies(std::forward<TArgs>(args)...), m_Filter(std::move(filter)) {}

    ~CLogLatencyReport();

    void Start(EWhich which);
    explicit operator bool() const { return m_Handler.get(); }

private:
    struct SHandler : CDiagHandler, TData
    {
        void Post(const SDiagMessage& msg) override { emplace_back(msg); }
    };

    string m_Filter;
    unique_ptr<SHandler> m_Handler;
};

END_NCBI_SCOPE

#endif
