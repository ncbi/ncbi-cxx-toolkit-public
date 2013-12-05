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
 * Author:  Vladimir Soussov
 *
 * File Description:  Driver for ODBC server
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbimtx.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <corelib/ncbi_safe_static.hpp>

// DO NOT DELETE this include !!!
#include <dbapi/driver/driver_mgr.hpp>

#include <dbapi/driver/odbc/interfaces.hpp>
#include <dbapi/driver/util/numeric_convert.hpp>
#include <dbapi/error_codes.hpp>
#include "../ncbi_win_hook.hpp"

#include <algorithm>

#ifdef HAVE_ODBCSS_H
#include <odbcss.h>
#endif

#include "odbc_utils.hpp"


#define NCBI_USE_ERRCODE_X   Dbapi_Odbc_Context

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//
//  CODBCContextRegistry (Singleton)
//

class CODBCContextRegistry
{
public:
    static CODBCContextRegistry& Instance(void);

    void Add(CODBCContext* ctx);
    void Remove(CODBCContext* ctx);
    void ClearAll(void);
    static void StaticClearAll(void);

    bool ExitProcessIsPatched(void) const
    {
        return m_ExitProcessPatched;
    }

private:
    CODBCContextRegistry(void);
    ~CODBCContextRegistry(void) throw();

    mutable CMutex          m_Mutex;
    vector<CODBCContext*>   m_Registry;
    bool                    m_ExitProcessPatched;

    friend class CSafeStatic_Allocator<CODBCContextRegistry>;
};


CODBCContextRegistry::CODBCContextRegistry(void)
{
#if defined(NCBI_OS_MSWIN) && defined(NCBI_DLL_BUILD)
    try {
        m_ExitProcessPatched =
            NWinHook::COnExitProcess::Instance().Add(CODBCContextRegistry::StaticClearAll);
    } catch (const NWinHook::CWinHookException&) {
        // Just in case ...
        m_ExitProcessPatched = false;
    }
#endif
}

CODBCContextRegistry::~CODBCContextRegistry(void) throw()
{
    try {
        ClearAll();
    }
    NCBI_CATCH_ALL_X( 1, NCBI_CURRENT_FUNCTION )
}

CODBCContextRegistry&
CODBCContextRegistry::Instance(void)
{
    static CSafeStatic<CODBCContextRegistry> instance;

    return *instance;
}

void
CODBCContextRegistry::Add(CODBCContext* ctx)
{
    CMutexGuard mg(m_Mutex);

    vector<CODBCContext*>::iterator it = find(m_Registry.begin(),
                                              m_Registry.end(),
                                              ctx);
    if (it == m_Registry.end()) {
        m_Registry.push_back(ctx);
    }
}

void
CODBCContextRegistry::Remove(CODBCContext* ctx)
{
    CMutexGuard mg(m_Mutex);

    vector<CODBCContext*>::iterator it = find(m_Registry.begin(),
                                              m_Registry.end(),
                                              ctx);

    if (it != m_Registry.end()) {
        m_Registry.erase(it);
        ctx->x_SetRegistry(NULL);
    }
}

void
CODBCContextRegistry::ClearAll(void)
{
    if (!m_Registry.empty())
    {
        CMutexGuard mg(m_Mutex);

        while ( !m_Registry.empty() ) {
            try {
                // x_Close will unregister and remove handler from the registry.
                m_Registry.back()->x_Close(false);
            }
            NCBI_CATCH_ALL_X(4, "Error closing context");
        }
    }
}

void
CODBCContextRegistry::StaticClearAll(void)
{
    CODBCContextRegistry::Instance().ClearAll();
}

/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_Reporter::
//
CODBC_Reporter::CODBC_Reporter(impl::CDBHandlerStack* hs,
                               SQLSMALLINT ht,
                               SQLHANDLE h,
                               const CODBC_Connection* connection,
                               const CODBC_Reporter* parent_reporter)
: m_HStack(hs)
, m_Handle(h)
, m_HType(ht)
, m_Connection(connection)
, m_ParentReporter(parent_reporter)
{
}

CODBC_Reporter::~CODBC_Reporter(void)
{
}

string
CODBC_Reporter::GetExtraMsg(void) const
{
    const CODBC_Reporter* next = this;
    // Start one level up when establishing a new connection.
    if (m_ParentReporter == NULL  &&  m_Connection != NULL) {
        next = &m_Connection->m_Reporter;
    }

    string result;
    do {
        result += " ";
        result += next->m_ExtraMsg;
        next    = next->m_ParentReporter;
    } while (next != NULL);

    return result;
}

void CODBC_Reporter::ReportErrors(void) const
{
    SQLINTEGER NativeError;
    SQLSMALLINT MsgLen;

    enum {eMsgStrLen = 1024};

    odbc::TSqlChar SqlState[6];
    odbc::TSqlChar Msg[eMsgStrLen];

    if( !m_HStack ) {
        return;
    }

    memset(Msg, 0, sizeof(Msg));

    for(SQLSMALLINT i= 1; i < 128; i++) {
        int rc = SQLGetDiagRec(m_HType, m_Handle, i, SqlState, &NativeError,
                               Msg, eMsgStrLen, &MsgLen);

        if (rc != SQL_NO_DATA) {
            string err_msg(CODBCString(Msg).AsUTF8());
            err_msg += GetExtraMsg();

            switch( rc ) {
            case SQL_SUCCESS:
                if(util::strncmp(SqlState, _T_NCBI_ODBC("HYT"), 3) == 0) { // timeout

                    CDB_TimeoutEx to(DIAG_COMPILE_INFO,
                                    0,
                                    err_msg,
                                    NativeError);

                    x_PostMsg(to);
                }
				else if(util::strncmp(SqlState, _T_NCBI_ODBC("40001"), 5) == 0) {
					// deadlock
                    CDB_DeadlockEx dl(DIAG_COMPILE_INFO,
                                    0,
                                    err_msg);
                    x_PostMsg(dl);
                }
                else if (NativeError == 1708  ||  NativeError == 1771) {
                    ERR_POST_X(3, Warning << err_msg);
                }
                else if(NativeError != 5701
                    && NativeError != 5703 ){
                    CDB_SQLEx se(DIAG_COMPILE_INFO,
                                0,
                                err_msg,
                                (NativeError == 0 ? eDiag_Info : eDiag_Warning),
                                NativeError,
                                CODBCString(SqlState).AsLatin1(),
                                0);
                    x_PostMsg(se);
                }
                continue;

            case SQL_NO_DATA:
                break;

            case SQL_SUCCESS_WITH_INFO:
                err_msg = "Message is too long to be retrieved";
                err_msg += GetExtraMsg();

                {
                    CDB_DSEx dse(DIAG_COMPILE_INFO,
                                0,
                                err_msg,
                                eDiag_Warning,
                                777);
                    x_PostMsg(dse);
                }

                continue;

            default:
                err_msg = "SQLGetDiagRec failed (memory corruption suspected";
                err_msg += GetExtraMsg();

                {
                    CDB_ClientEx ce(DIAG_COMPILE_INFO,
                                    0,
                                    err_msg,
                                    eDiag_Warning,
                                    420016);
                    x_PostMsg(ce);
                }

                break;
            }
        }

        break;
    }
}

void CODBC_Reporter::ReportError(CDB_Exception& ex) const
{
    string extra_msg = GetExtraMsg();
    if ( !NStr::EndsWith(ex.GetMsg(), extra_msg) ) {
        ex.AddToMessage(extra_msg);
    }
    x_PostMsg(ex, true);
}

void CODBC_Reporter::x_PostMsg(CDB_Exception& ex, bool always_throw) const
{
    const CDBParams* params = NULL;
    if (m_Connection != NULL) {
        const string& conn_msg = m_Connection->m_Reporter.m_ExtraMsg;
        // Explicitly setting the exception's server name and user name fields
        // keeps SetFromConnection from duplicating SERVER: and USER: info.
        if (ex.GetServerName().empty() && conn_msg.find(" SERVER: ") != NPOS) {
            ex.SetServerName(m_Connection->ServerName());
        }
        if (ex.GetUserName().empty()  &&  conn_msg.find(" USER: ") != NPOS) {
            ex.SetUserName(m_Connection->UserName());
        }
        params = m_Connection->GetBindParams();
    }

    if (always_throw) {
        if (m_Connection) {
            ex.SetFromConnection(*m_Connection);
        }
        ex.SetParams(params);
        throw ex;
    } else {
        m_HStack->PostMsg(&ex, kEmptyStr, m_Connection, params);
    }
}

/////////////////////////////////////////////////////////////////////////////
//
//  CODBCContext::
//

CODBCContext::CODBCContext(SQLLEN version,
                           int tds_version,
                           bool use_dsn)
: m_PacketSize(0)
, m_Reporter(0, SQL_HANDLE_ENV, 0)
, m_UseDSN(use_dsn)
, m_Registry(&CODBCContextRegistry::Instance())
, m_TDSVersion(tds_version)
{
    DEFINE_STATIC_FAST_MUTEX(xMutex);
    CFastMutexGuard mg(xMutex);
/**/
#ifdef UNICODE
    SetClientCharset("UTF-8");
#endif
/**/

    if(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_Context) != SQL_SUCCESS) {
        string err_message = "Cannot allocate a context" + m_Reporter.GetExtraMsg();
        DATABASE_DRIVER_ERROR( err_message, 400001 );
    }

    m_Reporter.SetHandle(m_Context);
    m_Reporter.SetHandlerStack(GetCtxHandlerStack());

    SQLSetEnvAttr(m_Context, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)version, 0);
    // For FreeTDS's sake.
    SQLSetEnvAttr(m_Context, SQL_ATTR_OUTPUT_NTS, (SQLPOINTER)SQL_FALSE, 0);

    x_AddToRegistry();
}

void
CODBCContext::x_AddToRegistry(void)
{
    if (m_Registry) {
        m_Registry->Add(this);
    }
}

void
CODBCContext::x_RemoveFromRegistry(void)
{
    if (m_Registry) {
        m_Registry->Remove(this);
    }
}

void
CODBCContext::x_SetRegistry(CODBCContextRegistry* registry)
{
    m_Registry = registry;
}


impl::CConnection*
CODBCContext::MakeIConnection(const CDBConnParams& params)
{
    return new CODBC_Connection(*this, params);
}

CODBCContext::~CODBCContext()
{
    try {
        x_Close();
    }
    NCBI_CATCH_ALL_X( 2, NCBI_CURRENT_FUNCTION )
}

void
CODBCContext::x_Close(bool delete_conn)
{
    CMutexGuard mg(m_CtxMtx);

    if (m_Context) {
        // Unregister first for sake of exception safety.
        x_RemoveFromRegistry();

        // close all connections first
        if (delete_conn) {
            DeleteAllConn();
        } else {
            CloseAllConn();
        }

        int rc = SQLFreeHandle(SQL_HANDLE_ENV, m_Context);
        switch( rc ) {
        case SQL_INVALID_HANDLE:
        case SQL_ERROR:
            m_Reporter.ReportErrors();
            break;
        case SQL_SUCCESS_WITH_INFO:
            m_Reporter.ReportErrors();
        case SQL_SUCCESS:
            break;
        default:
            m_Reporter.ReportErrors();
            break;
        };

        m_Context = NULL;
    } else {
        x_RemoveFromRegistry();
        if (delete_conn) {
            DeleteAllConn();
        }
    }
}


void
CODBCContext::SetupErrorReporter(const CDBConnParams& params)
{
    string extra_msg = " SERVER: " + params.GetServerName() + "; USER: " + params.GetUserName();

    CMutexGuard mg(m_CtxMtx);
    m_Reporter.SetExtraMsg( extra_msg );
}


string CODBCContext::GetDriverName(void) const
{
    return "odbc";
}


void CODBCContext::SetPacketSize(SQLUINTEGER packet_size)
{
    CMutexGuard mg(m_CtxMtx);

    m_PacketSize = packet_size;
}


bool CODBCContext::CheckSIE(int rc, SQLHDBC con)
{
    CMutexGuard mg(m_CtxMtx);

    switch(rc) {
    case SQL_SUCCESS_WITH_INFO:
        x_ReportConError(con);
    case SQL_SUCCESS:
        return true;
    case SQL_ERROR:
        x_ReportConError(con);
        SQLFreeHandle(SQL_HANDLE_DBC, con);
        break;
    default:
        m_Reporter.ReportErrors();
        break;
    }

    return false;
}


void CODBCContext::x_ReportConError(SQLHDBC con)
{
    m_Reporter.SetHandleType(SQL_HANDLE_DBC);
    m_Reporter.SetHandle(con);
    m_Reporter.ReportErrors();
    m_Reporter.SetHandleType(SQL_HANDLE_ENV);
    m_Reporter.SetHandle(m_Context);
}


/////////////////////////////////////////////////////////////////////////////
//
//  Miscellaneous
//


///////////////////////////////////////////////////////////////////////
// Driver manager related functions
//

///////////////////////////////////////////////////////////////////////////////
class CDbapiOdbcCFBase : public CSimpleClassFactoryImpl<I_DriverContext, CODBCContext>
{
public:
    typedef CSimpleClassFactoryImpl<I_DriverContext, CODBCContext> TParent;

public:
    CDbapiOdbcCFBase(const string& driver_name);
    ~CDbapiOdbcCFBase(void);

public:
    virtual TInterface*
    CreateInstance(
        const string& driver  = kEmptyStr,
        CVersionInfo version =
        NCBI_INTERFACE_VERSION(I_DriverContext),
        const TPluginManagerParamTree* params = 0) const;

};

CDbapiOdbcCFBase::CDbapiOdbcCFBase(const string& driver_name)
    : TParent( driver_name, 0 )
{
    return ;
}

CDbapiOdbcCFBase::~CDbapiOdbcCFBase(void)
{
    return ;
}

CDbapiOdbcCFBase::TInterface*
CDbapiOdbcCFBase::CreateInstance(
    const string& driver,
    CVersionInfo version,
    const TPluginManagerParamTree* params) const
{
    auto_ptr<TImplementation> drv;

    if ( !driver.empty()  &&  driver != m_DriverName ) {
        return 0;
    }

    if (version.Match(NCBI_INTERFACE_VERSION(I_DriverContext))
                        != CVersionInfo::eNonCompatible) {
        // Mandatory parameters ....
        int tds_version = 80;

        bool use_dsn = false;

        // Optional parameters ...
        int page_size = 0;
        string client_charset;

        if ( params != NULL ) {
            typedef TPluginManagerParamTree::TNodeList_CI TCIter;
            typedef TPluginManagerParamTree::TValueType   TValue;

            // Get parameters ...
            TCIter cit = params->SubNodeBegin();
            TCIter cend = params->SubNodeEnd();

            for (; cit != cend; ++cit) {
                const TValue& v = (*cit)->GetValue();

                if ( v.id == "use_dsn" ) {
                    use_dsn = (v.value != "false");
                } else if ( v.id == "version" ) {
                    tds_version = NStr::StringToInt( v.value );
                } else if ( v.id == "packet" ) {
                    page_size = NStr::StringToInt( v.value );
                } else if ( v.id == "client_charset" ) {
                    client_charset = v.value;
                }
            }
        }

        // Create a driver ...
        drv.reset(new CODBCContext( SQL_OV_ODBC3, tds_version, use_dsn));

        // Set parameters ...
        if ( page_size ) {
            drv->SetPacketSize( page_size );
        }

        if ( !client_charset.empty() ) {
            drv->SetClientCharset( client_charset );
        }
    }

    return drv.release();
}

///////////////////////////////////////////////////////////////////////////////
class CDbapiOdbcCF : public CDbapiOdbcCFBase
{
public:
    CDbapiOdbcCF(void)
    : CDbapiOdbcCFBase("odbc")
    {
    }
};

///////////////////////////////////////////////////////////////////////////////
NCBI_DBAPIDRIVER_ODBC_EXPORT
void
NCBI_EntryPoint_xdbapi_odbc(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CDbapiOdbcCF>::NCBI_EntryPointImpl( info_list, method );
}

NCBI_DBAPIDRIVER_ODBC_EXPORT
void
DBAPI_RegisterDriver_ODBC(void)
{
    RegisterEntryPoint<I_DriverContext>( NCBI_EntryPoint_xdbapi_odbc );
}


END_NCBI_SCOPE


