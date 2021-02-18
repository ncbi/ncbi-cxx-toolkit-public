#ifndef CORELIB___DB_SERVICE_MAPPER__HPP
#define CORELIB___DB_SERVICE_MAPPER__HPP

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
 * Author:  Sergey Sikorskiy
 *
 * File Description:
 *   Database service name to server mapping policy.
 *
 */


#include <corelib/ncbimtx.hpp>

#ifdef NCBI_OS_MSWIN
#  include <winsock2.h>
#endif

BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////////////////////////////
/// Forward declarations
///

class CDBServer;
class IDBServiceMapper;
class IRegistry;

///////////////////////////////////////////////////////////////////////////////
/// I_ConnectionExtra
///

class I_ConnectionExtra
{
public:
    virtual ~I_ConnectionExtra(void) { }

#ifdef NCBI_OS_MSWIN
    typedef SOCKET  TSockHandle;
#else
    typedef int     TSockHandle;
#endif

    /// Get OS handle of the socket represented by the connection
    virtual TSockHandle GetLowLevelHandle(void) const = 0;

    virtual void SetUserData(CObject* data) = 0;
    template<typename T> T* GetUserData(void)
    { return dynamic_cast<T*>(x_GetUserData()); }
    template<typename T> const T* GetUserData(void) const
    { return dynamic_cast<T*>(x_GetUserData()); }

protected:
    virtual CObject* x_GetUserData(void) const = 0;
    virtual void     x_RecordServer(const CDBServer&) { }

private:
    friend class IDBServiceMapper;
};


/// Lightweight representation of just a host and a port.
class NCBI_XNCBI_EXPORT CEndpointKey
{
private:
    typedef Uint8 TValue;

public:
    CEndpointKey(const CEndpointKey& that)
        : m_Value(that.m_Value)
        { }
    
    CEndpointKey(Uint4 host, Uint2 port = 0)
        : m_Value((static_cast<TValue>(host) << 16) | port)
        { }

    CEndpointKey(const CTempString& name, NStr::TConvErrFlags flags = 0);

    Uint4 GetHost(void) const {
        _ASSERT(m_Value >> 48 == 0);
        return static_cast<Uint4>(m_Value >> 16);
    }

    Uint2 GetPort(void) const {
        return m_Value & 0xFFFF;
    }

    bool operator==(const CEndpointKey& that) const
        { return m_Value == that.m_Value; }

    bool operator!=(const CEndpointKey& that) const
        { return m_Value != that.m_Value; }

    bool operator<(const CEndpointKey& that) const
        { return m_Value < that.m_Value; }

    bool operator<=(const CEndpointKey& that) const
        { return m_Value <= that.m_Value; }

    bool operator>(const CEndpointKey& that) const
        { return m_Value > that.m_Value; }

    bool operator>=(const CEndpointKey& that) const
        { return m_Value >= that.m_Value; }

private:
    TValue m_Value;
};


ostream& operator<<(ostream& os, const CEndpointKey& key);


///////////////////////////////////////////////////////////////////////////////
/// IDBServiceMapper
///

class CDBServer : public CObject
{
public:
    CDBServer(void);
    CDBServer(const string& name,
              Uint4         host = 0,
              Uint2         port = 0,
              unsigned int  expire_time = 0);

    const string& GetName      (void) const { return m_Name; }
    Uint4         GetHost      (void) const { return m_Host; }
    Uint2         GetPort      (void) const { return m_Port; }
    time_t        GetExpireTime(void) const { return time_t(m_ExpireTime); }

    bool IsValid(void) const
    {
        return !GetName().empty() || GetHost() != 0;
    }

private:
    const string       m_Name;
    const Uint4        m_Host;
    const Uint2        m_Port;
    const unsigned int m_ExpireTime;
};
typedef CRef<CDBServer> TSvrRef;

bool operator== (const CDBServer& l, const CDBServer& r);
bool operator< (const CDBServer& l, const CDBServer& r);

/// CDBServerOption -- CDBServer extended with additional information
/// that helps maintain a balanced pool of connections.  Rankings are
/// relative; what matters there is the ratios between rankings
/// obtained together.  Some rankings may be zero as long as the total
/// isn't.
class CDBServerOption : public CDBServer
{
public:
    enum EState {
        fState_Penalized = 1 << 0, ///< Penalized by the load balancer
        fState_Excluded  = 1 << 1, ///< Excluded by DBAPI
        fState_Normal    = 0       ///< Fully available
    };
    DECLARE_SAFE_FLAGS_TYPE(EState, TState);
    
    CDBServerOption(const string& name,
                    Uint4         host,
                    Uint2         port,
                    double        ranking,
                    TState        state = fState_Normal,
                    unsigned int  expire_time = 0)
        : CDBServer(name, host, port, expire_time),
          m_Ranking(ranking), m_State(state)
        { }

    double GetRanking (void) const { return m_Ranking;                  }
    TState GetState   (void) const { return m_State;                    }
    bool   IsPenalized(void) const { return (m_State & fState_Penalized) != 0; }
    bool   IsExcluded (void) const { return (m_State & fState_Excluded) != 0; }
    bool   IsNormal   (void) const { return m_State == fState_Normal;   }

private:
    friend class IDBServiceMapper;
    friend class CDBUDRandomMapper;

    double m_Ranking;
    TState m_State;
};

DECLARE_SAFE_FLAGS(CDBServerOption::EState);

///////////////////////////////////////////////////////////////////////////////
/// IDBServiceMapper
///

class NCBI_XNCBI_EXPORT IDBServiceMapper : public CObject
{
public:
    typedef IDBServiceMapper* (*TFactory)(const IRegistry* registry);
    typedef list<CRef<CDBServerOption> > TOptions;

    struct SDereferenceLess
    {
        template <typename T>
        bool operator()(T l, T r) const
        {
            _ASSERT(l.NotEmpty());
            _ASSERT(r.NotEmpty());

            return *l < *r;
        }
    };

    virtual ~IDBServiceMapper    (void) {}

    virtual string  GetName      (void) const;
    virtual void    Configure    (const IRegistry* registry = NULL) = 0;
    /// Map a service to a server
    virtual TSvrRef GetServer    (const string&    service) = 0;

    /// Exclude a server from the mapping for a service.
    /// @note Classes that override this method should either override
    /// GetServerOptions too or chain to the base implementation (and
    /// have CleanExcluded do the same!)
    virtual void    Exclude      (const string&    service,
                                  const TSvrRef&   server)
    {
        _TRACE("For " << service << ": excluding server '" << server->GetName()
               << "' on "
               << CEndpointKey(server->GetHost(), server->GetPort()));
        CFastMutexGuard mg(m_Mtx);
        m_ExcludeMap[service].insert(server);
    }

    /// Clean the list of excluded servers for the given service
    virtual void    CleanExcluded(const string&    service)
    {
        CFastMutexGuard mg(m_Mtx);
        _TRACE("For " << service << ": cleaning excluded list");
        m_ExcludeMap.erase(service);
    }

    virtual bool    HasExclusions(const string& service) const
    {
        CFastMutexGuard mg(m_Mtx);
        auto it = m_ExcludeMap.find(service);
        return it != m_ExcludeMap.end()  &&  !it->second.empty();
    }

    /// Get list of all servers for the given service disregarding any exclusions
    virtual void GetServersList(const string& /* service */,
                                list<string>* serv_list) const
    {
        serv_list->clear();
    }

    /// Get an annotated list of all servers for the given service.
    /// The default implementation just pads out the results of
    /// calling GetServersList, which predates this method and as such
    /// is likelier to be defined by derived classes.
    virtual void GetServerOptions(const string& service, TOptions* options);

    /// Set up mapping preferences for a service
    /// preference - value between 0 and 100
    ///      (0 means *no particular preferances*, 100 means *do not choose,
    ///      just use a given server*)
    /// preferred_server - preferred server
    virtual void    SetPreference(const string&    service,
                                  const TSvrRef&   preferred_server,
                                  double           preference = 100) = 0;

    /// Given a connection that succeeded even though this service
    /// mapper was unable to identify a good server beforehand, try to
    /// determine which server it actually reached on the basis of its
    /// low-level handle (if available); on success, pass the result
    /// to its x_RecordServer method to inform subsequent logging.
    /// @return true if successful, false otherwise.
    virtual bool RecordServer(I_ConnectionExtra&) const { return false; }

protected:
    static void x_RecordServer(I_ConnectionExtra& extra, CDBServer& server)
        { extra.x_RecordServer(server); }

    typedef set<TSvrRef, SDereferenceLess>  TSrvSet;
    typedef map<string, TSrvSet>            TExcludeMap;
    mutable CFastMutex  m_Mtx;
    TExcludeMap         m_ExcludeMap;
};


///////////////////////////////////////////////////////////////////////////////
/// DBServiceMapperTraits
/// IDBServiceMapper traits
///

template <class T>
class CDBServiceMapperTraits
{
public:
    static string GetName(void)
    {
        _ASSERT(false);
        return "none";
    }
};

inline
string IDBServiceMapper::GetName(void) const
{
    return CDBServiceMapperTraits<IDBServiceMapper>::GetName();
}

///////////////////////////////////////////////////////////////////////////////

inline
bool operator== (const CDBServer& l, const CDBServer& r)
{
    return (l.GetName() == r.GetName() &&
            l.GetHost() == r.GetHost() &&
            l.GetPort() == r.GetPort());
}


inline
bool operator< (const CDBServer& l, const CDBServer& r)
{
    int res = l.GetName().compare(r.GetName());
    if (res != 0)
        return res < 0;
    if (l.GetHost() != r.GetHost())
        return l.GetHost() < r.GetHost();
    return l.GetPort() < r.GetPort();
}

///////////////////////////////////////////////////////////////////////////////

inline
CDBServer::CDBServer(void) :
    m_Host(0),
    m_Port(0),
    m_ExpireTime(0)
{
}

inline
CDBServer::CDBServer(const string& name,
                     Uint4         host,
                     Uint2         port,
                     unsigned int  expire_time) :
m_Name(name),
m_Host(host),
m_Port(port),
m_ExpireTime(expire_time)
{
}


END_NCBI_SCOPE

#endif  // CORELIB___DB_SERVICE_MAPPER__HPP
