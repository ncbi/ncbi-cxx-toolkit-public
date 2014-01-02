#ifndef DBAPI_CONN_FACTORY_HPP
#define DBAPI_CONN_FACTORY_HPP

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

#include <corelib/ncbimtx.hpp>
#include <corelib/impl/ncbi_dbsvcmapper.hpp>
#include <dbapi/driver/dbapi_driver_conn_mgr.hpp>
#include <dbapi/driver/impl/handle_stack.hpp>
#include <map>
#include <vector>
#include <memory>

BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////////////////////////////
/// CDBConnectionFactory
///

enum EDefaultMapping
{
    eUseDefaultMapper,
    eNoMapping
};

class NCBI_DBAPIDRIVER_EXPORT CDBConnectionFactory : public IDBConnectionFactory
{
public:
    /// CDBConnectionFactory won't take ownership of registry.
    CDBConnectionFactory(IDBServiceMapper::TFactory svc_mapper_factory,
                         const IRegistry* registry = NULL,
                         EDefaultMapping def_mapping = eUseDefaultMapper);
    virtual ~CDBConnectionFactory(void);

    //
    virtual void Configure(const IRegistry* registry = NULL);

    //
    unsigned int GetMaxNumOfConnAttempts(void) const;
    void SetMaxNumOfConnAttempts(unsigned int max_num);

    unsigned int GetMaxNumOfValidationAttempts(void) const;
    void SetMaxNumOfValidationAttempts(unsigned int max_num);

    unsigned int GetMaxNumOfServerAlternatives(void) const;
    void SetMaxNumOfServerAlternatives(unsigned int max_num);

    unsigned int GetMaxNumOfDispatches(void) const;
    void SetMaxNumOfDispatches(unsigned int max_num);

    unsigned int GetConnectionTimeout(void) const;
    void SetConnectionTimeout(unsigned int timeout);

    unsigned int GetLoginTimeout(void) const;
    void SetLoginTimeout(unsigned int timeout);

    void GetServersList(const string& validator_name,
                        const string& service_name,
                        list<string>* serv_list);
    void WorkWithSingleServer(const string& validator_name,
                              const string& service_name,
                              const string& server);

protected:
    // Data
    class CRuntimeData
    {
    public:
        CRuntimeData(const CDBConnectionFactory& parent,
                     const CRef<IDBServiceMapper>& mapper);

    public:
        //
        TSvrRef GetDispatchedServer(const string& service_name);
        void SetDispatchedServer(const string&  service_name,
                                 const TSvrRef& server);

        //
        void IncNumOfValidationFailures(const string& server_name,
                                        const TSvrRef& dsp_srv);

        //
        unsigned int GetNumOfDispatches(const string& service_name);
        unsigned int GetNumOfValidationFailures(const string& service_name);

        //
        IDBServiceMapper& GetDBServiceMapper(void)
        {
            return *m_DBServiceMapper;
        }
        const IDBServiceMapper& GetDBServiceMapper(void) const
        {
            return *m_DBServiceMapper;
        }

        //
        const CDBConnectionFactory& GetParent(void) const
        {
            return *m_Parent;
        }

    private:
        // Data types
        typedef map<string, TSvrRef>      TDispatchedSet;
        typedef map<string, unsigned int> TServer2NumMap;

        const CDBConnectionFactory* m_Parent;
        CRef<IDBServiceMapper>      m_DBServiceMapper;
        TDispatchedSet              m_DispatchedSet;
        TServer2NumMap              m_DispatchNumMap;
        TServer2NumMap              m_ValidationFailureMap;
    };

    friend class CRuntimeData;

    CRuntimeData& GetRuntimeData(const CRef<IConnValidator> validator);
    CRuntimeData& GetRuntimeData(const string& validator_name);

    void ConfigureFromRegistry(const IRegistry* registry = NULL);
    virtual CDB_Connection* MakeDBConnection(
        I_DriverContext& ctx,
        const CDBConnParams& params);

private:
    struct SOpeningContext {
        SOpeningContext(I_DriverContext& driver_ctx_in);

        I_DriverContext&            driver_ctx;
        IConnValidator::EConnStatus conn_status;
        impl::CDBHandlerStack       handlers;
        unsigned int                retries;
        // params deliberately left out (potentially call-specific)
    };

    // Methods
    CDB_Connection* DispatchServerName(SOpeningContext& ctx,
                                       const CDBConnParams& params);

    CDB_Connection* MakeValidConnection(SOpeningContext& ctx,
                                        const CDBConnParams& params);

    virtual CDB_UserHandler::TExceptions* GetExceptions(void);

    unsigned int CalculateConnectionTimeout(const I_DriverContext& ctx) const;
    unsigned int CalculateLoginTimeout(const I_DriverContext& ctx) const;

    void x_LogConnection(const SOpeningContext& ctx,
                         const CDB_Connection* connection,
                         const CDBConnParams& params);

private:
    typedef map<string, CRuntimeData> TValidatorSet;

    class CMapperFactory
    {
    public:
        CMapperFactory(IDBServiceMapper::TFactory svc_mapper_factory,
                       const IRegistry* registry,
                       EDefaultMapping def_mapping);

    public:
        IDBServiceMapper* Make(void) const;

    private:
        const IDBServiceMapper::TFactory    m_SvcMapperFactory;
        const IRegistry*                    m_Registry;
        EDefaultMapping                     m_DefMapping;
    };

    mutable CFastMutex m_Mtx;

    const CMapperFactory        m_MapperFactory;
    TValidatorSet               m_ValidatorSet;
    CDB_UserHandler::TExceptions m_Errors;
    // 0 means *none* (even do not try to connect)
    unsigned int                m_MaxNumOfConnAttempts;
    // 0 means *unlimited*
    unsigned int                m_MaxNumOfValidationAttempts;
    // 0 means *none* (even do not try to connect)
    // 1 means *try only one server* (give up strategy)
    unsigned int                m_MaxNumOfServerAlternatives;
    // 0 means *unlimited*
    unsigned int                m_MaxNumOfDispatches;
    unsigned int                m_ConnectionTimeout;
    unsigned int                m_LoginTimeout;
    bool                        m_TryServerToo;
};

///////////////////////////////////////////////////////////////////////////////
/// CDBGiveUpFactory
///

/// Helper class
/// This policy will give up without trying to connect to alternative db servers.
class NCBI_DBAPIDRIVER_EXPORT CDBGiveUpFactory : public CDBConnectionFactory
{
public:
    CDBGiveUpFactory(IDBServiceMapper::TFactory svc_mapper_factory,
                     const IRegistry* registry = NULL,
                     EDefaultMapping def_mapping = eUseDefaultMapper);
    virtual ~CDBGiveUpFactory(void);
};

///////////////////////////////////////////////////////////////////////////////
/// CDBRedispatchFactory
///

/// Helper class
/// This policy will redispatch every time after a successful dispatch.
/// Servers will be kept in a list of available servers till they are
/// reported as IConnValidator::eInvalidConn by a validator.
class NCBI_DBAPIDRIVER_EXPORT CDBRedispatchFactory : public CDBConnectionFactory
{
public:
    CDBRedispatchFactory(IDBServiceMapper::TFactory svc_mapper_factory,
                         const IRegistry* registry = NULL,
                         EDefaultMapping def_mapping = eUseDefaultMapper);
    virtual ~CDBRedispatchFactory(void);
};


///////////////////////////////////////////////////////////////////////////////
/// CConnValidatorCoR
///
/// IConnValidator adaptor which implements the chain of responsibility
/// pattern
///

class NCBI_DBAPIDRIVER_EXPORT CConnValidatorCoR : public IConnValidator
{
public:
    CConnValidatorCoR(void);
    virtual ~CConnValidatorCoR(void);

    virtual EConnStatus Validate(CDB_Connection& conn);
    virtual string GetName(void) const;

    void Push(const CRef<IConnValidator>& validator);
    void Pop(void);
    CRef<IConnValidator> Top(void) const;
    bool Empty(void) const;

protected:
    typedef vector<CRef<IConnValidator> > TValidators;

    mutable CFastMutex m_Mtx;
    TValidators        m_Validators;
};

///////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CTrivialConnValidator : public IConnValidator
{
public:
    enum EValidateAttr {
        eKeepModifiedConnection = 0,
        eRestoreDefaultDB = 1,
        eCheckSysobjects = 2
    };
    enum {eDefaultValidateAttr = eRestoreDefaultDB | eCheckSysobjects};

    CTrivialConnValidator(const string& db_name,
                          int attr = eDefaultValidateAttr);
    virtual ~CTrivialConnValidator(void);

    virtual EConnStatus Validate(CDB_Connection& conn);
    virtual string GetName(void) const;

    const string& GetDBName(void) const
    {
        return m_DBName;
    }
    int GetAttr(void) const
    {
        return m_Attr;
    }

private:
    const string m_DBName;
    const int    m_Attr;
};



/////////////////////////////////////////////////////////////////////////////
inline
unsigned int
CDBConnectionFactory::GetMaxNumOfConnAttempts(void) const
{
    return m_MaxNumOfConnAttempts;
}

inline
unsigned int
CDBConnectionFactory::GetMaxNumOfValidationAttempts(void) const
{
    return m_MaxNumOfValidationAttempts;
}

inline
unsigned int
CDBConnectionFactory::GetMaxNumOfServerAlternatives(void) const
{
    return m_MaxNumOfServerAlternatives;
}

inline
unsigned int
CDBConnectionFactory::GetMaxNumOfDispatches(void) const
{
    return m_MaxNumOfDispatches;
}

inline
unsigned int
CDBConnectionFactory::GetConnectionTimeout(void) const
{
    return m_ConnectionTimeout;
}

inline
unsigned int
CDBConnectionFactory::GetLoginTimeout(void) const
{
    return m_LoginTimeout;
}

END_NCBI_SCOPE

#endif // DBAPI_CONN_FACTORY_HPP
