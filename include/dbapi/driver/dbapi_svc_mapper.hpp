#ifndef DBAPI_SVC_MAPPER_HPP
#define DBAPI_SVC_MAPPER_HPP

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
* Author: Sergey Sikorskiy
*
* File Description:
*
*============================================================================
*/

#include <dbapi/driver/dbapi_conn_factory.hpp>
#include <corelib/ncbimtx.hpp>

#ifdef HAVE_LIBCONNEXT
#  include <connect/ext/ncbi_dblb_svcmapper.hpp>
#endif

#include <vector>
#include <set>
#include <map>

BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////////////////////////////
/// CDBDefaultServiceMapper
///

class NCBI_DBAPIDRIVER_EXPORT CDBDefaultServiceMapper : public IDBServiceMapper
{
public:
    CDBDefaultServiceMapper(void);
    virtual ~CDBDefaultServiceMapper(void);

    virtual string  GetName      (void) const;
    virtual void    Configure    (const IRegistry* registry = NULL);
    virtual TSvrRef GetServer    (const string&    service);
    virtual void    Exclude      (const string&    service,
                                  const TSvrRef&   server);
    virtual void    CleanExcluded(const string&    service);
    virtual void    SetPreference(const string&    service,
                                  const TSvrRef&   preferred_server,
                                  double           preference = 100.0);

private:
    typedef set<string> TSrvSet;

    CFastMutex  m_Mtx;
    TSrvSet     m_SrvSet;
};

///////////////////////////////////////////////////////////////////////////////
/// CDBServiceMapperCoR
///
/// IDBServiceMapper adaptor which implements the chain of responsibility
/// pattern
///

class NCBI_DBAPIDRIVER_EXPORT CDBServiceMapperCoR : public IDBServiceMapper
{
public:
    CDBServiceMapperCoR(void);
    virtual ~CDBServiceMapperCoR(void);

    virtual string  GetName      (void) const;
    virtual void    Configure    (const IRegistry* registry = NULL);
    virtual TSvrRef GetServer    (const string&    service);
    virtual void    Exclude      (const string&    service,
                                  const TSvrRef&   server);
    virtual void    CleanExcluded(const string&    service);
    virtual void    SetPreference(const string&    service,
                                  const TSvrRef&   preferred_server,
                                  double           preference = 100.0);
    virtual void    GetServersList(const string& service,
                                   list<string>* serv_list) const;
    virtual bool    RecordServer(I_ConnectionExtra& extra) const;

    void Push(const CRef<IDBServiceMapper>& mapper);
    void Pop(void);
    CRef<IDBServiceMapper> Top(void) const;
    bool Empty(void) const;

protected:
    void ConfigureFromRegistry(const IRegistry* registry = NULL);

    typedef vector<CRef<IDBServiceMapper> > TDelegates;

    mutable CFastMutex m_Mtx;
    TDelegates         m_Delegates;
};



///////////////////////////////////////////////////////////////////////////////
/// CDBUDRandomMapper
///

class NCBI_DBAPIDRIVER_EXPORT CDBUDRandomMapper : public IDBServiceMapper
{
public:
    CDBUDRandomMapper(const IRegistry* registry = NULL);
    virtual ~CDBUDRandomMapper(void);

public:
    virtual string  GetName      (void) const;
    virtual void    Configure    (const IRegistry* registry = NULL);
    virtual TSvrRef GetServer    (const string&    service);
    virtual void    Exclude      (const string&    service,
                                  const TSvrRef&   server);
    virtual void    CleanExcluded(const string&    service);
    virtual void    SetPreference(const string&    service,
                                  const TSvrRef&   preferred_server,
                                  double           preference = 100.0);
    // Not implemented yet ...
            void    Add          (const string&    service,
                                  const TSvrRef&   server,
                                  double           preference = 0.0);

    static IDBServiceMapper* Factory(const IRegistry* registry);

protected:
    void ConfigureFromRegistry(const IRegistry* registry = NULL);
    void ScalePreference(const string& service, double coeff);
    void SetPreference(const string& service, double coeff);
    void SetServerPreference(const string& service,
                             double preference,
                             const TSvrRef& server);

private:
    typedef map<string, bool>                       TLBNameMap;
    typedef map<TSvrRef, double, SDereferenceLess>  TSvrMap;
    typedef map<string, TSvrMap>                    TServiceMap;

    CFastMutex  m_Mtx;
    TLBNameMap  m_LBNameMap;
    TServiceMap m_ServerMap;
    TServiceMap m_PreferenceMap;
};



///////////////////////////////////////////////////////////////////////////////
/// CDBUDPriorityMapper
///

class NCBI_DBAPIDRIVER_EXPORT CDBUDPriorityMapper : public IDBServiceMapper
{
public:
    CDBUDPriorityMapper(const IRegistry* registry = NULL);
    virtual ~CDBUDPriorityMapper(void);

public:
    virtual string  GetName      (void) const;
    virtual void    Configure    (const IRegistry* registry = NULL);
    virtual TSvrRef GetServer    (const string&    service);
    virtual void    Exclude      (const string&    service,
                                  const TSvrRef&   server);
    virtual void    CleanExcluded(const string&    service);
    virtual void    SetPreference(const string&    service,
                                  const TSvrRef&   preferred_server,
                                  double           preference = 100.0);
            void    Add          (const string&    service,
                                  const TSvrRef&   server,
                                  double           preference = 0.0);

    static IDBServiceMapper* Factory(const IRegistry* registry);

protected:
    void ConfigureFromRegistry(const IRegistry* registry = NULL);

private:
    typedef map<string, bool>                       TLBNameMap;
    typedef map<TSvrRef, double, SDereferenceLess>  TSvrMap;
    typedef map<string, TSvrMap>                    TServiceMap;
    typedef multimap<double, TSvrRef>               TServerUsageMap;
    typedef map<string, TServerUsageMap>            TServiceUsageMap;

    CFastMutex          m_Mtx;
    TLBNameMap          m_LBNameMap;
    TServiceMap         m_ServerMap;
    TServiceUsageMap    m_ServiceUsageMap;
    TServiceUsageMap    m_OrigServiceUsageMap;
};

class NCBI_DBAPIDRIVER_EXPORT CDBUniversalMapper : public CDBServiceMapperCoR
{
public:
    typedef pair<string, TFactory> TMapperConf;

    CDBUniversalMapper(const IRegistry* registry = nullptr,
                       const TMapperConf& ext_mapper
                       = TMapperConf(kEmptyStr, (TFactory)nullptr));
    virtual ~CDBUniversalMapper(void);

    virtual string GetName(void) const;
    virtual void Configure(const IRegistry* registry = NULL);

protected:
    void ConfigureFromRegistry(const IRegistry* registry = NULL);

private:
    TMapperConf m_ExtMapperConf;
};

///////////////////////////////////////////////////////////////////////////////
// Type traits
//

template <>
class NCBI_DBAPIDRIVER_EXPORT CDBServiceMapperTraits<CDBDefaultServiceMapper>
{
public:
    static string GetName(void);
};

template <>
class NCBI_DBAPIDRIVER_EXPORT CDBServiceMapperTraits<CDBServiceMapperCoR>
{
public:
    static string GetName(void);
};

template <>
class NCBI_DBAPIDRIVER_EXPORT CDBServiceMapperTraits<CDBUDRandomMapper>
{
public:
    static string GetName(void);
};

template <>
class NCBI_DBAPIDRIVER_EXPORT CDBServiceMapperTraits<CDBUDPriorityMapper>
{
public:
    static string GetName(void);
};

template <>
class NCBI_DBAPIDRIVER_EXPORT CDBServiceMapperTraits<CDBUniversalMapper>
{
public:
    static string GetName(void);
};


////////////////////////////////////////////////////////////////////////////////
inline
IDBServiceMapper*
MakeCDBUniversalMapper(const IRegistry* registry)
{
#ifdef HAVE_LIBCONNEXT
    return new CDBUniversalMapper(
        registry,
        CDBUniversalMapper::TMapperConf(
            CDBServiceMapperTraits<CDBLB_ServiceMapper>::GetName(),
            &CDBLB_ServiceMapper::Factory
            )
        );
#else
    return new CDBUniversalMapper(registry);
#endif
}


/// Easy-to-use macro to install the default DBAPI service mapper and
/// a user-defined connection factory, with control over what to do
/// if a connection factory has already been explicitly registered.
/// @sa DBLB_INSTALL_DEFAULT_EX, DBLB_INSTALL_FACTORY
#define DBLB_INSTALL_FACTORY_EX(factory_name, if_set)     \
    ncbi::CDbapiConnMgr::Instance().SetConnectionFactory( \
        new factory_name(ncbi::MakeCDBUniversalMapper),   \
        ncbi::CDbapiConnMgr::if_set)

/// Easy-to-use macro to install the default DBAPI service mapper
/// and a user-defined connection factory.
/// @sa DBLB_INSTALL_DEFAULT, DBLB_INSTALL_FACTORY_EX
#define DBLB_INSTALL_FACTORY(factory_name) \
    DBLB_INSTALL_FACTORY_EX(factory_name, eIfSet_Replace)

/// Easy-to-use macro to install the default DBAPI service mapper,
/// with control over what to do if a connection factory has already
/// been explicitly registered.
/// @sa DBLB_INSTALL_DEFAULT, DBLB_INSTALL_FACTORY_EX
#define DBLB_INSTALL_DEFAULT_EX(if_set) \
    DBLB_INSTALL_FACTORY_EX(ncbi::CDBConnectionFactory, if_set)

/// Easy-to-use macro to install the default DBAPI service mapper.
/// @sa DBLB_INSTALL_DEFAULT_EX, DBLB_INSTALL_FACTORY
#define DBLB_INSTALL_DEFAULT() DBLB_INSTALL_DEFAULT_EX(eIfSet_Replace)



END_NCBI_SCOPE

#endif // DBAPI_SVC_MAPPER_HPP
