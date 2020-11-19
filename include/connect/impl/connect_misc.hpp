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
#include <memory>
#include <mutex>
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
        unsigned host;
        SHost(unsigned h) : host(h) {}
        SHost(const string& h);
    };

    struct SPort
    {
        unsigned short port;
        SPort(unsigned short p) : port(p) {}
        SPort(const string& p)  : port(NStr::StringToNumeric<unsigned short>(p)) {}
        SPort(CTempString p)    : port(NStr::StringToNumeric<unsigned short>(p)) {}
    };

    SSocketAddress(SHost h, SPort p) : host(h.host), port(p.port) {}

    explicit operator bool() const { return host && port; }
    string GetHostName() const;
    string AsString() const { return GetHostName() + ':' + NStr::UIntToString(port); }

    static SSocketAddress Parse(const string& address);
};

NCBI_XCONNECT_EXPORT bool operator==(const SSocketAddress& lhs, const SSocketAddress& rhs);
NCBI_XCONNECT_EXPORT bool operator< (const SSocketAddress& lhs, const SSocketAddress& rhs);

class NCBI_XCONNECT_EXPORT CServiceDiscovery
{
public:
    CServiceDiscovery(const string& service_name);

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
    SThreadSafe(TArgs&&... args) : m_Object(forward<TArgs>(args)...) {}

    SLock<      TType> GetLock()       { return { &m_Object, m_Mutex }; }
    SLock<const TType> GetLock() const { return { &m_Object, m_Mutex }; }

    // Direct access to the protected object (e.g. to access atomic members).
    // All thread-safe members must be explicitly marked volatile to be available.
          volatile TType& GetMTSafe()       { return m_Object; }
    const volatile TType& GetMTSafe() const { return m_Object; }

protected:
    mutex m_Mutex;

private:
    TType m_Object;
};

class NCBI_XCONNECT_EXPORT CLogLatencies : public unordered_map<string, chrono::microseconds>
{
public:
    template <size_t SIZE1, size_t SIZE2>
    CLogLatencies(const char (&s1)[SIZE1], const char (&s2)[SIZE2]) :
        m_Start(s1, SIZE1 - 1),
        m_Stop(s2, SIZE2 - 1)
    {}

    void SetDebug(bool debug) { m_Debug = debug; }

    friend istream& operator>>(istream& is, CLogLatencies& latencies);

private:
    regex m_Start;
    regex m_Stop;
    bool m_Debug = false;
};

class NCBI_XCONNECT_EXPORT CLogLatencyReport : public CLogLatencies
{
public:
    template <class... TArgs>
    CLogLatencyReport(TArgs&&... args) : CLogLatencies(forward<TArgs>(args)...) {}

    ~CLogLatencyReport();

    void Start();
    explicit operator bool() const { return m_CerrBuf; }

private:
    struct SNullBuf : streambuf
    {
        int_type overflow(int_type c) override { return traits_type::not_eof(c); }
    };

    SNullBuf m_NullBuf;
    stringstream m_CerrOutput;
    streambuf* m_CerrBuf = nullptr;
};

END_NCBI_SCOPE

#endif
