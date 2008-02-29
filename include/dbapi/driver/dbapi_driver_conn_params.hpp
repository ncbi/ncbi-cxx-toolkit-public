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

#include <dbapi/driver/dbapi_driver_conn_mgr.hpp>

BEGIN_NCBI_SCOPE

namespace impl {

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
    virtual bool IsSecureLogin(void) const;

    // Connection pool related methods.

    /// Use connection pool with this connection.
    virtual bool IsPooled(void) const;
    /// Use connections from NotInUse pool
    virtual bool IsDoNotConnect(void) const;  
    /// Pool name to be used with this connection
    virtual string GetPoolName(void) const; 

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
    void SetSequreLogin(bool flag = true)
    {
	m_IsSecureLogin = flag;
    }

    void SetPooled(bool flag = true)
    {
	m_IsPooled = flag;
    }
    void SetDoNotConnect(bool flag = true)
    {
	m_IsDoNotConnect = flag;
    }
    void SetPoolName(const string& name)
    {
	m_PoolName = name;
    }

private:
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

    string  m_PoolName;
    bool    m_IsSecureLogin;
    bool    m_IsPooled;
    bool    m_IsDoNotConnect;
};

} // namespace impl


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
                         const CRef<IConnValidator>& validator,
                         Uint4 host = 0,
                         Uint2 port = 0,
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

    void x_MapPairToParam(const string& key, const string& value);
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
    void x_MapPairToParam(const string& key, const string& value);
};


END_NCBI_SCOPE



#endif  /* DBAPI_DRIVER___DBAPI_DRIVER_CONN_PARAMS__HPP */
