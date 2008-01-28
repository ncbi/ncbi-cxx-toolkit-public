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


#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>


BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////////////////////////////
/// Forward declaration
///

class IRegistry;

///////////////////////////////////////////////////////////////////////////////
/// IDBServiceMapper
///

class CDBServer : public CObject
{
public:
    CDBServer(void);
    CDBServer(const string& name,
              Uint4         host = 0,
              Uint2         port = 0);

    const string& GetName(void) const { return m_Name; }
    Uint4         GetHost(void) const { return m_Host; }
    Uint2         GetPort(void) const { return m_Port; }

    bool IsValid(void) const
    {
        return !GetName().empty() || GetHost() != 0;
    }

private:
    const string m_Name;
    const Uint4  m_Host;
    const Uint2  m_Port;
};
typedef CRef<CDBServer> TSvrRef;

///////////////////////////////////////////////////////////////////////////////
/// IDBServiceMapper
///

class IDBServiceMapper : public CObject
{
public:
    typedef IDBServiceMapper* (*TFactory)(const IRegistry* registry);

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

    virtual void    Configure    (const IRegistry* registry = NULL) = 0;
    /// Map a service to a server
    virtual TSvrRef GetServer    (const string&    service) = 0;

    /// Exclude a server from the mapping for a service
    virtual void    Exclude      (const string&    service,
                                  const TSvrRef&   server)  = 0;


    /// Set up mapping preferences for a service
    /// preference - value between 0 and 100
    ///      (0 means *no particular preferances*, 100 means *do not choose,
    ///      just use a given server*)
    /// preferred_server - preferred server
    virtual void    SetPreference(const string&    service,
                                  const TSvrRef&   preferred_server,
                                  double           preference = 100) = 0;
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
    return (l.GetName().compare(r.GetName()) < 0 ||
            l.GetHost() < r.GetHost() ||
            l.GetPort() < r.GetPort());
}

///////////////////////////////////////////////////////////////////////////////

inline
CDBServer::CDBServer(void) :
    m_Host(0),
    m_Port(0)
{
}

inline
CDBServer::CDBServer(const string& name,
                     Uint4         host,
                     Uint2         port) :
m_Name(name),
m_Host(host),
m_Port(port)
{
}


END_NCBI_SCOPE

#endif  // CORELIB___DB_SERVICE_MAPPER__HPP
