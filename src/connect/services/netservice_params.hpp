#ifndef CONNECT_SERVICES__NETSERVICE_PARAMS_HPP
#define CONNECT_SERVICES__NETSERVICE_PARAMS_HPP

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
 * File Description:
 *   Declarations of the configuration parameters for connect/services.
 *
 * Authors:
 *   Dmitry Kazimirov
 *
 */

#include <connect/ncbi_types.h>

#include <corelib/ncbi_param.hpp>
#include <corelib/ncbi_config.hpp>

#include <memory>

// The number of connection attemps
#define CONNECTION_MAX_RETRIES 4

// Delay between two successive connection attempts in seconds.
#define RETRY_DELAY_DEFAULT 1.0

// Connection timeout, which is used as the eIO_Open timeout
// in CSocket::Connect.
#define CONNECTION_TIMEOUT_DEFAULT 2.0

// Communication timeout, upon reaching which the connection
// is closed on the client side.
#define COMMUNICATION_TIMEOUT_DEFAULT 12.0

// The size of an internal array, which is used for calculation
// of the connection failure rate.
#define CONNECTION_ERROR_HISTORY_MAX 128

// Maximum cumulative query time in seconds.
// The parameter is ignored if it's zero.
#define MAX_CONNECTION_TIME_DEFAULT 0.0

#define FIRST_SERVER_TIMEOUT_DEFAULT 0.3

#define COMMIT_JOB_INTERVAL_DEFAULT 2

BEGIN_NCBI_SCOPE

NCBI_PARAM_DECL(string, netservice_api, communication_timeout);
typedef NCBI_PARAM_TYPE(netservice_api, communication_timeout)
    TServConn_CommTimeout;

NCBI_PARAM_DECL(unsigned int, netservice_api, connection_max_retries);
typedef NCBI_PARAM_TYPE(netservice_api, connection_max_retries)
    TServConn_ConnMaxRetries;

NCBI_PARAM_DECL(string, netservice_api, retry_delay);
typedef NCBI_PARAM_TYPE(netservice_api, retry_delay)
    TServConn_RetryDelay;

NCBI_PARAM_DECL(bool, netservice_api, use_linger2);
typedef NCBI_PARAM_TYPE(netservice_api, use_linger2)
    TServConn_UserLinger2;

NCBI_PARAM_DECL(int, netservice_api, max_find_lbname_retries);
typedef NCBI_PARAM_TYPE(netservice_api, max_find_lbname_retries)
    TServConn_MaxFineLBNameRetries;

NCBI_PARAM_DECL(string, netcache_api, fallback_server);
typedef NCBI_PARAM_TYPE(netcache_api, fallback_server)
    TCGI_NetCacheFallbackServer;

NCBI_PARAM_DECL(int, netservice_api, max_connection_pool_size);
typedef NCBI_PARAM_TYPE(netservice_api, max_connection_pool_size)
    TServConn_MaxConnPoolSize;

NCBI_PARAM_DECL(bool, netservice_api, connection_data_logging);
typedef NCBI_PARAM_TYPE(netservice_api, connection_data_logging)
    TServConn_ConnDataLogging;

// Worker node-specific parameters

// Determine how long the worker node should wait for the
// configured NetSchedule servers to appear before quitting.
// When none of the servers are accessible, the worker node
// will keep trying to connect until it succeeds, or the specified
// timeout elapses.
NCBI_PARAM_DECL(unsigned, server, max_wait_for_servers);
typedef NCBI_PARAM_TYPE(server, max_wait_for_servers)
    TWorkerNode_MaxWaitForServers;

NCBI_PARAM_DECL(bool, server, stop_on_job_errors);
typedef NCBI_PARAM_TYPE(server, stop_on_job_errors)
    TWorkerNode_StopOnJobErrors;

NCBI_PARAM_DECL(bool, server, allow_implicit_job_return);
typedef NCBI_PARAM_TYPE(server, allow_implicit_job_return)
    TWorkerNode_AllowImplicitJobReturn;

NCBI_XCONNECT_EXPORT unsigned long s_SecondsToMilliseconds(
    const string& seconds, unsigned long default_value);
NCBI_XCONNECT_EXPORT STimeout s_GetDefaultCommTimeout();
NCBI_XCONNECT_EXPORT unsigned long s_GetRetryDelay();


class NCBI_XNCBI_EXPORT CConfigRegistry : public IRegistry
{
public:
    CConfigRegistry(CConfig* config = nullptr, EOwnership ownership = eNoOwnership);
    void Reset(CConfig* config = nullptr, EOwnership ownership = eNoOwnership);

private:
    bool x_Empty(TFlags flags) const override;
    const string& x_Get(const string& section, const string& name, TFlags flags) const override;
    bool x_HasEntry(const string& section, const string& name, TFlags flags) const override;
    const string& x_GetComment(const string& section, const string& name, TFlags flags) const override;
    void x_Enumerate(const string& section, list<string>& entries, TFlags flags) const override;

    shared_ptr<CConfig> m_Config;
};

class NCBI_XNCBI_EXPORT ISynRegistry
{
    template <typename TType> struct TR;

public:
    virtual ~ISynRegistry() {}

    // XXX:
    // Microsoft VS 2013 has a bug (violates 13.3.3.2/3.1.1 of the standard).
    // So, to avoid overload 'issues' it would report (string vs initializer_list<string>),
    // parameter 'name' has type of 'const char*' (instead of 'const string&').

    // XXX:
    // Since there is no such thing as a virtual template method, a workaround is used:
    // Template methods (ISynRegistry::Get) call virtual methods (ISynRegistry::VGet).
    // Their overrides (TSynRegistry<TImpl>::VGet) call actual implementations (TImpl::TGet).

    template <typename TType>
    typename TR<TType>::T Get(const string& section, const char* name, TType default_value)
    {
        return VGet(section, name, static_cast<typename TR<TType>::T>(default_value));
    }

    template <typename TType>
    typename TR<TType>::T Get(const string& section, initializer_list<string> names, TType default_value)
    {
        return VGet(section, names, static_cast<typename TR<TType>::T>(default_value));
    }

    template <typename TType>
    typename TR<TType>::T Get(initializer_list<string> sections, const char* name, TType default_value)
    {
        return VGet(sections, name, static_cast<typename TR<TType>::T>(default_value));
    }

    template <typename TType>
    typename TR<TType>::T Get(initializer_list<string> sections, initializer_list<string> names, TType default_value)
    {
        return VGet(sections, names, static_cast<typename TR<TType>::T>(default_value));
    }

    bool Has(const string& section, const char* name)
    {
        return HasImpl(section, name);
    }

    bool Has(const string& section, initializer_list<string> names);
    bool Has(initializer_list<string> sections, const char* name);
    bool Has(initializer_list<string> sections, initializer_list<string> names);

    virtual void Reset(IRegistry* registry = nullptr, EOwnership ownership = eNoOwnership) = 0;

protected:
    virtual string VGet(const string& section, const char* name, string default_value) = 0;
    virtual bool   VGet(const string& section, const char* name, bool default_value) = 0;
    virtual int    VGet(const string& section, const char* name, int default_value) = 0;
    virtual double VGet(const string& section, const char* name, double default_value) = 0;
    virtual string VGet(const string& section, initializer_list<string> names, string default_value) = 0;
    virtual bool   VGet(const string& section, initializer_list<string> names, bool default_value) = 0;
    virtual int    VGet(const string& section, initializer_list<string> names, int default_value) = 0;
    virtual double VGet(const string& section, initializer_list<string> names, double default_value) = 0;
    virtual string VGet(initializer_list<string> sections, const char* name, string default_value) = 0;
    virtual bool   VGet(initializer_list<string> sections, const char* name, bool default_value) = 0;
    virtual int    VGet(initializer_list<string> sections, const char* name, int default_value) = 0;
    virtual double VGet(initializer_list<string> sections, const char* name, double default_value) = 0;
    virtual string VGet(initializer_list<string> sections, initializer_list<string> names, string default_value) = 0;
    virtual bool   VGet(initializer_list<string> sections, initializer_list<string> names, bool default_value) = 0;
    virtual int    VGet(initializer_list<string> sections, initializer_list<string> names, int default_value) = 0;
    virtual double VGet(initializer_list<string> sections, initializer_list<string> names, double default_value) = 0;

    virtual bool   HasImpl(const string& section, const char* name) = 0;
};

template <typename TType> struct ISynRegistry::TR              { using T = TType;  };
template <>               struct ISynRegistry::TR<const char*> { using T = string; };
template <>               struct ISynRegistry::TR<unsigned>    { using T = int;    };

template <class TImpl>
class TSynRegistry : public TImpl
{
public:
    template <class... TArgs>
    TSynRegistry(TArgs&&... args) : TImpl(std::forward<TArgs>(args)...) {}

protected:
    string VGet(const string& section, const char* name, string default_value) final;
    bool   VGet(const string& section, const char* name, bool default_value) final;
    int    VGet(const string& section, const char* name, int default_value) final;
    double VGet(const string& section, const char* name, double default_value) final;
    string VGet(const string& section, initializer_list<string> names, string default_value) final;
    bool   VGet(const string& section, initializer_list<string> names, bool default_value) final;
    int    VGet(const string& section, initializer_list<string> names, int default_value) final;
    double VGet(const string& section, initializer_list<string> names, double default_value) final;
    string VGet(initializer_list<string> sections, const char* name, string default_value) final;
    bool   VGet(initializer_list<string> sections, const char* name, bool default_value) final;
    int    VGet(initializer_list<string> sections, const char* name, int default_value) final;
    double VGet(initializer_list<string> sections, const char* name, double default_value) final;
    string VGet(initializer_list<string> sections, initializer_list<string> names, string default_value) final;
    bool   VGet(initializer_list<string> sections, initializer_list<string> names, bool default_value) final;
    int    VGet(initializer_list<string> sections, initializer_list<string> names, int default_value) final;
    double VGet(initializer_list<string> sections, initializer_list<string> names, double default_value) final;
};

class NCBI_XNCBI_EXPORT CSynRegistryImpl : public ISynRegistry
{
public:
    CSynRegistryImpl(IRegistry* registry = nullptr, EOwnership ownership = eNoOwnership);

    void Reset(IRegistry* registry = nullptr, EOwnership ownership = eNoOwnership) override;

protected:
    template <typename TType>
    TType TGet(const string& section, const char* name, TType default_value);

    template <typename TType>
    TType TGet(const string& section, initializer_list<string> names, TType default_value);

    template <typename TType>
    TType TGet(initializer_list<string> sections, const char* name, TType default_value);

    template <typename TType>
    TType TGet(initializer_list<string> sections, initializer_list<string> names, TType default_value);


    bool HasImpl(const string& section, const char* name) final
    {
        _ASSERT(m_Registry);
        return m_Registry->HasEntry(section, name);
    }

private:
    shared_ptr<IRegistry> m_Registry;
};

using CSynRegistry = TSynRegistry<CSynRegistryImpl>;

class NCBI_XNCBI_EXPORT CCachedSynRegistryImpl : public ISynRegistry
{
    class CCache;

public:
    CCachedSynRegistryImpl(ISynRegistry* registry = nullptr, EOwnership ownership = eNoOwnership);
    ~CCachedSynRegistryImpl();

    void Reset(IRegistry* registry = nullptr, EOwnership ownership = eNoOwnership) override;

protected:
    template <typename TSections, typename TNames, typename TType>
    TType TGet(TSections sections, TNames names, TType default_value);

    bool HasImpl(const string& section, const char* name) final;

private:
    shared_ptr<ISynRegistry> m_Registry;
    unique_ptr<CCache> m_Cache;
};

using CCachedSynRegistry = TSynRegistry<CCachedSynRegistryImpl>;

extern template NCBI_XNCBI_EXPORT string CSynRegistryImpl::TGet(const string& section, const char* name, string default_value);
extern template NCBI_XNCBI_EXPORT bool   CSynRegistryImpl::TGet(const string& section, const char* name, bool default_value);
extern template NCBI_XNCBI_EXPORT int    CSynRegistryImpl::TGet(const string& section, const char* name, int default_value);
extern template NCBI_XNCBI_EXPORT double CSynRegistryImpl::TGet(const string& section, const char* name, double default_value);
extern template NCBI_XNCBI_EXPORT string CSynRegistryImpl::TGet(const string& section, initializer_list<string> names, string default_value);
extern template NCBI_XNCBI_EXPORT bool   CSynRegistryImpl::TGet(const string& section, initializer_list<string> names, bool default_value);
extern template NCBI_XNCBI_EXPORT int    CSynRegistryImpl::TGet(const string& section, initializer_list<string> names, int default_value);
extern template NCBI_XNCBI_EXPORT double CSynRegistryImpl::TGet(const string& section, initializer_list<string> names, double default_value);
extern template NCBI_XNCBI_EXPORT string CSynRegistryImpl::TGet(initializer_list<string> sections, const char* name, string default_value);
extern template NCBI_XNCBI_EXPORT bool   CSynRegistryImpl::TGet(initializer_list<string> sections, const char* name, bool default_value);
extern template NCBI_XNCBI_EXPORT int    CSynRegistryImpl::TGet(initializer_list<string> sections, const char* name, int default_value);
extern template NCBI_XNCBI_EXPORT double CSynRegistryImpl::TGet(initializer_list<string> sections, const char* name, double default_value);
extern template NCBI_XNCBI_EXPORT string CSynRegistryImpl::TGet(initializer_list<string> sections, initializer_list<string> names, string default_value);
extern template NCBI_XNCBI_EXPORT bool   CSynRegistryImpl::TGet(initializer_list<string> sections, initializer_list<string> names, bool default_value);
extern template NCBI_XNCBI_EXPORT int    CSynRegistryImpl::TGet(initializer_list<string> sections, initializer_list<string> names, int default_value);
extern template NCBI_XNCBI_EXPORT double CSynRegistryImpl::TGet(initializer_list<string> sections, initializer_list<string> names, double default_value);

extern template NCBI_XNCBI_EXPORT string CCachedSynRegistryImpl::TGet(string section, const char* name, string default_value);
extern template NCBI_XNCBI_EXPORT bool   CCachedSynRegistryImpl::TGet(string section, const char* name, bool default_value);
extern template NCBI_XNCBI_EXPORT int    CCachedSynRegistryImpl::TGet(string section, const char* name, int default_value);
extern template NCBI_XNCBI_EXPORT double CCachedSynRegistryImpl::TGet(string section, const char* name, double default_value);
extern template NCBI_XNCBI_EXPORT string CCachedSynRegistryImpl::TGet(string section, initializer_list<string> names, string default_value);
extern template NCBI_XNCBI_EXPORT bool   CCachedSynRegistryImpl::TGet(string section, initializer_list<string> names, bool default_value);
extern template NCBI_XNCBI_EXPORT int    CCachedSynRegistryImpl::TGet(string section, initializer_list<string> names, int default_value);
extern template NCBI_XNCBI_EXPORT double CCachedSynRegistryImpl::TGet(string section, initializer_list<string> names, double default_value);
extern template NCBI_XNCBI_EXPORT string CCachedSynRegistryImpl::TGet(initializer_list<string> sections, const char* name, string default_value);
extern template NCBI_XNCBI_EXPORT bool   CCachedSynRegistryImpl::TGet(initializer_list<string> sections, const char* name, bool default_value);
extern template NCBI_XNCBI_EXPORT int    CCachedSynRegistryImpl::TGet(initializer_list<string> sections, const char* name, int default_value);
extern template NCBI_XNCBI_EXPORT double CCachedSynRegistryImpl::TGet(initializer_list<string> sections, const char* name, double default_value);
extern template NCBI_XNCBI_EXPORT string CCachedSynRegistryImpl::TGet(initializer_list<string> sections, initializer_list<string> names, string default_value);
extern template NCBI_XNCBI_EXPORT bool   CCachedSynRegistryImpl::TGet(initializer_list<string> sections, initializer_list<string> names, bool default_value);
extern template NCBI_XNCBI_EXPORT int    CCachedSynRegistryImpl::TGet(initializer_list<string> sections, initializer_list<string> names, int default_value);
extern template NCBI_XNCBI_EXPORT double CCachedSynRegistryImpl::TGet(initializer_list<string> sections, initializer_list<string> names, double default_value);

template <typename TImpl>
string TSynRegistry<TImpl>::VGet(const string& section, const char* name, string default_value)
{
    return TImpl::TGet(section, name, default_value);
}

template <typename TImpl>
bool TSynRegistry<TImpl>::VGet(const string& section, const char* name, bool default_value)
{
    return TImpl::TGet(section, name, default_value);
}

template <typename TImpl>
int TSynRegistry<TImpl>::VGet(const string& section, const char* name, int default_value)
{
    return TImpl::TGet(section, name, default_value);
}

template <typename TImpl>
double TSynRegistry<TImpl>::VGet(const string& section, const char* name, double default_value)
{
    return TImpl::TGet(section, name, default_value);
}

template <typename TImpl>
string TSynRegistry<TImpl>::VGet(const string& section, initializer_list<string> names, string default_value)
{
    return TImpl::TGet(section, names, default_value);
}

template <typename TImpl>
bool TSynRegistry<TImpl>::VGet(const string& section, initializer_list<string> names, bool default_value)
{
    return TImpl::TGet(section, names, default_value);
}

template <typename TImpl>
int TSynRegistry<TImpl>::VGet(const string& section, initializer_list<string> names, int default_value)
{
    return TImpl::TGet(section, names, default_value);
}

template <typename TImpl>
double TSynRegistry<TImpl>::VGet(const string& section, initializer_list<string> names, double default_value)
{
    return TImpl::TGet(section, names, default_value);
}

template <typename TImpl>
string TSynRegistry<TImpl>::VGet(initializer_list<string> sections, const char* name, string default_value)
{
    return TImpl::TGet(sections, name, default_value);
}

template <typename TImpl>
bool TSynRegistry<TImpl>::VGet(initializer_list<string> sections, const char* name, bool default_value)
{
    return TImpl::TGet(sections, name, default_value);
}

template <typename TImpl>
int TSynRegistry<TImpl>::VGet(initializer_list<string> sections, const char* name, int default_value)
{
    return TImpl::TGet(sections, name, default_value);
}

template <typename TImpl>
double TSynRegistry<TImpl>::VGet(initializer_list<string> sections, const char* name, double default_value)
{
    return TImpl::TGet(sections, name, default_value);
}

template <typename TImpl>
string TSynRegistry<TImpl>::VGet(initializer_list<string> sections, initializer_list<string> names, string default_value)
{
    return TImpl::TGet(sections, names, default_value);
}

template <typename TImpl>
bool TSynRegistry<TImpl>::VGet(initializer_list<string> sections, initializer_list<string> names, bool default_value)
{
    return TImpl::TGet(sections, names, default_value);
}

template <typename TImpl>
int TSynRegistry<TImpl>::VGet(initializer_list<string> sections, initializer_list<string> names, int default_value)
{
    return TImpl::TGet(sections, names, default_value);
}

template <typename TImpl>
double TSynRegistry<TImpl>::VGet(initializer_list<string> sections, initializer_list<string> names, double default_value)
{
    return TImpl::TGet(sections, names, default_value);
}

END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES__NETSERVICE_PARAMS_HPP */
