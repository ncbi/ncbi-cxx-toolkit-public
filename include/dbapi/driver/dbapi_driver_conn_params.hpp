#ifndef DBAPI_DRIVER___DBAPI_DRIVER_CONN_PARAMS__HPP
#define DBAPI_DRIVER___DBAPI_DRIVER_CONN_PARAMS__HPP

/* $Id$
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
 *
 */

#include <corelib/ncbienv.hpp>
#include <dbapi/driver/dbapi_driver_conn_mgr.hpp>
#include <dbapi/driver/impl/handle_stack.hpp>

BEGIN_NCBI_SCOPE


NCBI_PARAM_DECL_EXPORT(NCBI_DBAPIDRIVER_EXPORT, bool, dbapi, can_use_kerberos);
typedef NCBI_PARAM_TYPE(dbapi, can_use_kerberos) TDbapi_CanUseKerberos;


namespace impl 
{

/////////////////////////////////////////////////////////////////////////////
///
///  impl::CDBConnParamsBase::
///

class NCBI_DBAPIDRIVER_EXPORT CDBConnParamsBase : 
    public CDBConnParams 
{
public:
    CDBConnParamsBase(void);
    virtual ~CDBConnParamsBase(void);

public:
    virtual string GetDriverName(void) const;
    virtual Uint4  GetProtocolVersion(void) const;
    virtual EEncoding GetEncoding(void) const;

    virtual string GetServerName(void) const;
    virtual string GetDatabaseName(void) const;
    virtual string GetUserName(void) const;
    virtual string GetPassword(void) const;

    virtual EServerType GetServerType(void) const;
    virtual Uint4 GetHost(void) const;
    virtual Uint2 GetPort(void) const;

    virtual CRef<IConnValidator> GetConnValidator(void) const;
    virtual const CDBHandlerStack& GetOpeningMsgHandlers(void) const;

	virtual string GetParam(const string& key) const;

protected:
    void SetDriverName(const string& name)
    {
        m_DriverName = name;
    }
    void SetProtocolVersion(Uint4 version)
    {
        m_ProtocolVersion = version;
    }
    void SetEncoding(EEncoding encoding)
    {
        m_Encoding = encoding;
    }

    void SetServerName(const string& name)
    {
        m_ServerName = name;
    }
    void SetDatabaseName(const string& name)
    {
        m_DatabaseName = name;
    }
    void SetUserName(const string& name)
    {
        m_UserName = name;
    }
    void SetPassword(const string& passwd)
    {
        m_Password = passwd;
    }

    void SetServerType(EServerType type)
    {
        m_ServerType = type;
    }
    void SetHost(Uint4 host)
    {
        m_Host = host;
    }
    void SetPort(Uint2 port)
    {
        m_PortNumber = port;
    }

    void SetConnValidator(const CRef<IConnValidator>& validator)
    {
        m_Validator = validator;
    }

    CDBHandlerStack& SetOpeningMsgHandlers(void)
    {
        return m_OpeningMsgHandlers;
    }

	void SetParam(const string& key, const string& value)
    {
        m_UnclassifiedParamMap[key] = value;
    }

private:
    // Non-copyable.
    CDBConnParamsBase(const CDBConnParamsBase& other);
    CDBConnParamsBase& operator =(const CDBConnParamsBase& other);

private:
	typedef map<string, string, PNocase> TUnclassifiedParamMap;

    string    m_DriverName;
    Uint4     m_ProtocolVersion;
    EEncoding m_Encoding;

    string                m_ServerName;
    string                m_DatabaseName;
    string                m_UserName;
    string                m_Password;
    EServerType           m_ServerType;
    Uint4                 m_Host;
    Uint2                 m_PortNumber;
    CRef<IConnValidator>  m_Validator;
    CDBHandlerStack       m_OpeningMsgHandlers;
	TUnclassifiedParamMap m_UnclassifiedParamMap;
};

} // namespace impl

/////////////////////////////////////////////////////////////////////////////
///
///  CDBConnParamsBase::
///

class CDBConnParamsBase : 
    public impl::CDBConnParamsBase 
{
public:
    CDBConnParamsBase(void) {}
    virtual ~CDBConnParamsBase(void) {}

public:
    void SetDriverName(const string& name)
    {
        impl::CDBConnParamsBase::SetDriverName(name);
    }
    void SetProtocolVersion(Uint4 version)
    {
        impl::CDBConnParamsBase::SetProtocolVersion(version);
    }
    void SetEncoding(EEncoding encoding)
    {
        impl::CDBConnParamsBase::SetEncoding(encoding);
    }

    void SetServerName(const string& name)
    {
        impl::CDBConnParamsBase::SetServerName(name);
    }
    void SetDatabaseName(const string& name)
    {
        impl::CDBConnParamsBase::SetDatabaseName(name);
    }
    void SetUserName(const string& name)
    {
        impl::CDBConnParamsBase::SetUserName(name);
    }
    void SetPassword(const string& passwd)
    {
        impl::CDBConnParamsBase::SetPassword(passwd);
    }

    void SetServerType(EServerType type)
    {
        impl::CDBConnParamsBase::SetServerType(type);
    }
    void SetHost(Uint4 host)
    {
        impl::CDBConnParamsBase::SetHost(host);
    }
    void SetPort(Uint2 port)
    {
        impl::CDBConnParamsBase::SetPort(port);
    }

    void SetConnValidator(const CRef<IConnValidator>& validator)
    {
        impl::CDBConnParamsBase::SetConnValidator(validator);
    }

    impl::CDBHandlerStack& SetOpeningMsgHandlers(void)
    {
        return impl::CDBConnParamsBase::SetOpeningMsgHandlers();
    }

	void SetParam(const string& key, const string& value)
    {
        impl::CDBConnParamsBase::SetParam(key, value);
    }

private:
    // Non-copyable.
    CDBConnParamsBase(const CDBConnParamsBase& other);
    CDBConnParamsBase& operator =(const CDBConnParamsBase& other);
};


/////////////////////////////////////////////////////////////////////////////
///
///  CDBDefaultConnParams::
///

class NCBI_DBAPIDRIVER_EXPORT CDBDefaultConnParams : 
    public impl::CDBConnParamsBase 
{
public:
    CDBDefaultConnParams(const string&   srv_name,
                         const string&   user_name,
                         const string&   passwd,
                         I_DriverContext::TConnectionMode mode = 0,
                         bool            reusable = false,
                         const string&   pool_name = kEmptyStr);
    virtual ~CDBDefaultConnParams(void);

public:
    void SetDriverName(const string& name)
    {
        impl::CDBConnParamsBase::SetDriverName(name);
    }
    void SetProtocolVersion(Uint4 version)
    {
        impl::CDBConnParamsBase::SetProtocolVersion(version);
    }
    void SetEncoding(EEncoding encoding)
    {
        impl::CDBConnParamsBase::SetEncoding(encoding);
    }
    void SetDatabaseName(const string& name)
    {
        impl::CDBConnParamsBase::SetDatabaseName(name);
    }
    void SetServerType(EServerType type)
    {
        impl::CDBConnParamsBase::SetServerType(type);
    }

    void SetHost(Uint4 host)
    {
        impl::CDBConnParamsBase::SetHost(host);
    }
    void SetPort(Uint2 port)
    {
        impl::CDBConnParamsBase::SetPort(port);
    }

    void SetConnValidator(const CRef<IConnValidator>& validator)
    {
        impl::CDBConnParamsBase::SetConnValidator(validator);
    }

    impl::CDBHandlerStack& SetOpeningMsgHandlers(void)
    {
        return impl::CDBConnParamsBase::SetOpeningMsgHandlers();
    }

	void SetParam(const string& key, const string& value)
	{
        impl::CDBConnParamsBase::SetParam(key, value);
	}

private:
    // Non-copyable.
    CDBDefaultConnParams(const CDBDefaultConnParams& other);
    CDBDefaultConnParams& operator =(const CDBDefaultConnParams& other);
};


/////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CDBUriConnParams : 
    public impl::CDBConnParamsBase 
{
public:
    CDBUriConnParams(const string& params);
    virtual ~CDBUriConnParams(void);

public:
    void SetPassword(const string& passwd)
    {
        impl::CDBConnParamsBase::SetPassword(passwd);
    }

private:
    void ParseServer(const string& params, size_t cur_pos);
    void ParseSlash(const string& params, size_t cur_pos);
    void ParseParamPairs(const string& param_pairs, size_t cur_pos);

private:
    // Non-copyable.
    CDBUriConnParams(const CDBUriConnParams& other);
    CDBUriConnParams& operator =(const CDBUriConnParams& other);
};


/////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CDB_ODBC_ConnParams : 
    public impl::CDBConnParamsBase 
{
public:
    CDB_ODBC_ConnParams(const string& params);
    virtual ~CDB_ODBC_ConnParams(void);

public:
    void SetPassword(const string& passwd)
    {
        impl::CDBConnParamsBase::SetPassword(passwd);
    }

private:
    // Non-copyable.
    CDB_ODBC_ConnParams(const CDB_ODBC_ConnParams& other);
    CDB_ODBC_ConnParams& operator =(const CDB_ODBC_ConnParams& other);

private:
	void x_MapPairToParam(const string& key, const string& value);
};


/////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CDBEnvConnParams : public CDBConnParamsDelegate
{
public:
    CDBEnvConnParams(
        const CDBConnParams& other,
        const string& server_name_env = "DBAPI_SERVER",
        const string& database_name_env = "DBAPI_DATABASE",
        const string& user_name_env = "DBAPI_USER",
        const string& passwd_env = "DBAPI_PASSWORD"
        );
    virtual ~CDBEnvConnParams(void);

public:
    void SetServerNameEnv(const string& name)
    {
        m_ServerNameEnv = name;
    }
    void SetDatabaseNameEnv(const string& name)
    {
        m_DatabaseNameEnv = name;
    }
    void SetUserNameEnv(const string& name)
    {
        m_UserNameEnv = name;
    }
    void SetPasswordEnv(const string& pwd)
    {
        m_PasswordEnv = pwd;
    }

public:
    virtual string GetServerName(void) const;
    virtual string GetDatabaseName(void) const;
    virtual string GetUserName(void) const;
    virtual string GetPassword(void) const;

private:
    // Non-copyable.
    CDBEnvConnParams(const CDBEnvConnParams& other);
    CDBEnvConnParams& operator =(const CDBEnvConnParams& other);

private:
    const CNcbiEnvironment m_Env;
    string m_ServerNameEnv;
    string m_DatabaseNameEnv;
    string m_UserNameEnv;
    string m_PasswordEnv;
};


/////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CDBInterfacesFileConnParams : 
    public CDBConnParamsDelegate
{
public:
    CDBInterfacesFileConnParams(
        const CDBConnParams& other,
        const string& file = kEmptyStr
        );
    virtual ~CDBInterfacesFileConnParams(void);

public:
    virtual EServerType GetServerType(void) const;
    virtual Uint4 GetHost(void) const;
    virtual Uint2 GetPort(void) const;

private:
    // Non-copyable.
    CDBInterfacesFileConnParams(const CDBInterfacesFileConnParams& other);
    CDBInterfacesFileConnParams& operator =(const CDBInterfacesFileConnParams& other);

private:
    struct SIRecord
    {
        SIRecord(void)
        : m_Host(0)
        , m_Port(0)
        {
        }
        SIRecord(Uint4 host, Uint2 port)
        : m_Host(host)
        , m_Port(port)
        {
        }

        Uint4 m_Host;
        Uint2 m_Port;
    };

    typedef map<string, SIRecord> records_type;
    records_type m_Records;
};


/////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CCPPToolkitConnParams : 
    public CDBConnParamsDelegate
{
public:
    CCPPToolkitConnParams(const CDBConnParams& other);
    virtual ~CCPPToolkitConnParams(void);

public:
    static EServerType GetServerType(const CTempString& server_name);
    virtual EServerType GetServerType(void) const;

private:
    // Non-copyable.
    CCPPToolkitConnParams(const CCPPToolkitConnParams& other);
    CCPPToolkitConnParams& operator =(const CCPPToolkitConnParams& other);
};


END_NCBI_SCOPE



#endif  /* DBAPI_DRIVER___DBAPI_DRIVER_CONN_PARAMS__HPP */
