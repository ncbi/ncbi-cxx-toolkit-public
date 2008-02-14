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

#include <dbapi/driver/interfaces.hpp>

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
///
///  CDBDefaultConnParams::
///

class CDBDefaultConnParams : public CDBConnParams 
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
        m_DriverName = name;
    }
    void SetProtocolVersion(Uint4 version)
    {
        m_ProtocolVersion = version;
    }

public:
    virtual string GetDriverName(void) const;
    virtual Uint4  GetProtocolVersion(void) const;

    virtual string GetServerName(void) const;
    virtual string GetUserName(void) const;
    virtual string GetPassword(void) const;

    virtual EServerType GetServerType(void) const;
    virtual Uint4 GetHost(void) const;
    virtual Uint2 GetPort(void) const;

    virtual CRef<IConnValidator> GetConnValidator(void) const;
    virtual bool IsSequreLogin(void) const;

    // Connection pool related methods.

    /// Use connection pool with this connection.
    virtual bool IsPooled(void) const;
    /// Use connections from NotInUse pool
    virtual bool IsDoNotConnect(void) const;  
    /// Pool name to be used with this connection
    virtual string GetPoolName(void) const; 

private:
    string m_DriverName;
    Uint4  m_ProtocolVersion;

    const string m_ServerName;
    const string m_UserName;
    const string m_Password;
    const Uint4  m_Host;
    const Uint2  m_PortNumber;
    const CRef<IConnValidator> m_Validator;

    const string m_PoolName;
    const bool m_IsSequreLogin;
    const bool m_IsPooled;
    const bool m_IsDoNotConnect;  
};


END_NCBI_SCOPE



#endif  /* DBAPI_DRIVER___DBAPI_DRIVER_CONN_PARAMS__HPP */
