#ifndef DBAPI_DRIVER_CONN_MGR_HPP
#define DBAPI_DRIVER_CONN_MGR_HPP


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
 * File Description:  Connection manager
 *
 */

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimtx.hpp>
#include <dbapi/driver/interfaces.hpp>

BEGIN_NCBI_SCOPE


namespace impl
{
    class CDriverContext;
    class CConnection;
}


///////////////////////////////////////////////////////////////////////////////
// Forward declaration

class IRegistry;
template <typename T> class CSafeStaticPtr;


///////////////////////////////////////////////////////////////////////////////
/// IConnValidator
///

class NCBI_DBAPIDRIVER_EXPORT IConnValidator : public CObject
{
public:
    enum EConnStatus {
        eValidConn,         //< means "everything is fine"
        eInvalidConn,       //< means "bad connection, do not use it any more"
        eTempInvalidConn    //< means "temporarily unavailable connection"
        };

    virtual ~IConnValidator(void);

    // All CException-derived exceptions might be caught by a connection factory.
    // Please use other tools to report validation information to a user.
    virtual EConnStatus Validate(CDB_Connection& conn) = 0;
    // This method shouldn't rethrow the exception.
    virtual EConnStatus ValidateException(const CDB_Exception& ex);
    // Return unique name of validator. This name is used to identify a pair of
    // server and validator in order to validate resource against a particular
    // validator. Empty name is reserved.
    virtual string GetName(void) const;
};

///////////////////////////////////////////////////////////////////////////////
/// IDBConnectionFactory
///

class NCBI_DBAPIDRIVER_EXPORT IDBConnectionFactory : public CObject
{
public:
    /// IDBConnectionFactory will take ownership of validator if there is any.
    IDBConnectionFactory(void);
    virtual ~IDBConnectionFactory(void);

    /// Configure connection policy using registry.
    virtual void Configure(const IRegistry* registry = NULL) = 0;

protected:
    /// Create new connection object for the given context
    /// and connection attributes.
    virtual CDB_Connection* MakeDBConnection(
            I_DriverContext& ctx,
            const CDBConnParams& params) = 0;

    /// Helper method to provide access to a protected method in I_DriverContext
    /// for child classses.
    static CDB_Connection* CtxMakeConnection(
            I_DriverContext& ctx,
            const CDBConnParams& params);

    virtual CDB_UserHandler::TExceptions* GetExceptions(void)
    {
        return NULL;
    }

private:
    // Friends
    friend class impl::CDriverContext;
};


///////////////////////////////////////////////////////////////////////////////
/// CDbapiConnMgr
///

class NCBI_DBAPIDRIVER_EXPORT CDbapiConnMgr
{
public:
    /// Get access to the class instance.
    static CDbapiConnMgr& Instance(void);

    /// Set up a connection factory.
    void SetConnectionFactory(IDBConnectionFactory* factory)
    {
        m_ConnectFactory.Reset(factory);
    }

    /// Retrieve a connection factory.
    CRef<IDBConnectionFactory> GetConnectionFactory(void) const
    {
        return m_ConnectFactory;
    }

    static void SetMaxConnect(unsigned int max_connect);

    static unsigned int GetMaxConnect(void);

private:
    CDbapiConnMgr(void);
    ~CDbapiConnMgr(void);

    bool AddConnect(void);
    void DelConnect(void);

    CRef<IDBConnectionFactory> m_ConnectFactory;

    CMutex m_Mutex;
    unsigned int m_NumConnect;

    // Friends
    friend class CSafeStaticPtr<CDbapiConnMgr>;
    friend class impl::CConnection;
};


///////////////////////////////////////////////////////////////////////////////
inline
CDB_Connection*
IDBConnectionFactory::CtxMakeConnection
(I_DriverContext&                  ctx,
 const CDBConnParams& params)
{
    return ctx.MakePooledConnection(params);
}

END_NCBI_SCOPE


#endif  /* DBAPI_DRIVER_CONN_MGR_HPP */

