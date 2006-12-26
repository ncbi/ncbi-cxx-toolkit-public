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
#include "../ncbi_win_hook.hpp"

#include <algorithm>

#ifdef HAVE_ODBCSS_H
#include <odbcss.h>
#endif

#include "odbc_utils.hpp"

BEGIN_NCBI_SCOPE

static const CDiagCompileInfo kBlankCompileInfo;

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

private:
    CODBCContextRegistry(void);
    ~CODBCContextRegistry(void) throw();

    mutable CMutex          m_Mutex;
    vector<CODBCContext*>   m_Registry;

    friend class CSafeStaticPtr<CODBCContextRegistry>;
};


CODBCContextRegistry::CODBCContextRegistry(void)
{
#if defined(NCBI_OS_MSWIN)
    // NWinHook::COnExitProcess::Instance().Add(CODBCContextRegistry::StaticClearAll);
#endif
}

CODBCContextRegistry::~CODBCContextRegistry(void) throw()
{
    try {
        ClearAll();
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
}

CODBCContextRegistry&
CODBCContextRegistry::Instance(void)
{
    static CSafeStaticPtr<CODBCContextRegistry> instance;

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
            // x_Close will unregister and remove handler from the registry.
            m_Registry.back()->x_Close(false);
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
CODBC_Reporter::CODBC_Reporter(CDBHandlerStack* hs,
                               SQLSMALLINT ht,
                               SQLHANDLE h,
                               const CODBC_Reporter* parent_reporter)
: m_HStack(hs)
, m_Handle(h)
, m_HType(ht)
, m_ParentReporter(parent_reporter)
{
}

CODBC_Reporter::~CODBC_Reporter(void)
{
}

string
CODBC_Reporter::GetExtraMsg(void) const
{
    if ( m_ParentReporter != NULL ) {
        return " " + m_ExtraMsg + " " + m_ParentReporter->GetExtraMsg();
    }

    return " " + m_ExtraMsg;
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
                if(util::strncmp(SqlState, _T("HYT"), 3) == 0) { // timeout

                    CDB_TimeoutEx to(kBlankCompileInfo,
                                    0,
                                    err_msg.c_str(),
                                    NativeError);

                    m_HStack->PostMsg(&to);
                }
                else if(util::strncmp(SqlState, _T("40001"), 5) == 0) { // deadlock
                    CDB_DeadlockEx dl(kBlankCompileInfo,
                                    0,
                                    err_msg.c_str());
                    m_HStack->PostMsg(&dl);
                }
                else if(NativeError != 5701
                    && NativeError != 5703 ){
                    CDB_SQLEx se(kBlankCompileInfo,
                                0,
                                err_msg.c_str(),
                                (NativeError == 0 ? eDiag_Info : eDiag_Warning),
                                NativeError,
                                CODBCString(SqlState).AsLatin1(),
                                0);
                    m_HStack->PostMsg(&se);
                }
                continue;

            case SQL_NO_DATA:
                break;

            case SQL_SUCCESS_WITH_INFO:
                err_msg = "Message is too long to be retrieved";
                err_msg += GetExtraMsg();

                {
                    CDB_DSEx dse(kBlankCompileInfo,
                                0,
                                err_msg.c_str(),
                                eDiag_Warning,
                                777);
                    m_HStack->PostMsg(&dse);
                }

                continue;

            default:
                err_msg = "SQLGetDiagRec failed (memory corruption suspected";
                err_msg += GetExtraMsg();

                {
                    CDB_ClientEx ce(kBlankCompileInfo,
                                    0,
                                    err_msg.c_str(),
                                    eDiag_Warning,
                                    420016);
                    m_HStack->PostMsg(&ce);
                }

                break;
            }
        }

        break;
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

#ifdef UNICODE
    SetClientCharset("UTF-8");
#endif

    if(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_Context) != SQL_SUCCESS) {
        string err_message = "Cannot allocate a context" + m_Reporter.GetExtraMsg();
        DATABASE_DRIVER_ERROR( err_message, 400001 );
    }

    m_Reporter.SetHandle(m_Context);
    m_Reporter.SetHandlerStack(GetCtxHandlerStack());

    SQLSetEnvAttr(m_Context, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)version, 0);
    // For FreeTDS sake.
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
CODBCContext::MakeIConnection(const SConnAttr& conn_attr)
{
    CMutexGuard mg(m_CtxMtx);

    return new CODBC_Connection(*this, conn_attr);
}

CODBCContext::~CODBCContext()
{
    try {
        x_Close();
    }
    NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
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
CODBCContext::SetupErrorReporter(const I_DriverContext::SConnAttr& conn_attr)
{
    string extra_msg = " SERVER: " + conn_attr.srv_name + "; USER: " + conn_attr.user_name;

    CMutexGuard mg(m_CtxMtx);
    m_Reporter.SetExtraMsg( extra_msg );
}


void CODBCContext::SetPacketSize(SQLUINTEGER packet_size)
{
    CMutexGuard mg(m_CtxMtx);

    m_PacketSize = (SQLULEN)packet_size;
}


bool CODBCContext::CheckSIE(int rc, SQLHDBC con)
{
    switch(rc) {
    case SQL_SUCCESS_WITH_INFO:
        xReportConError(con);
    case SQL_SUCCESS:
        return true;
    case SQL_ERROR:
        xReportConError(con);
        SQLFreeHandle(SQL_HANDLE_DBC, con);
        break;
    default:
        m_Reporter.ReportErrors();
        break;
    }

    return false;
}


void CODBCContext::xReportConError(SQLHDBC con)
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

I_DriverContext* ODBC_CreateContext(const map<string,string>* attr = 0)
{
    SQLINTEGER version= SQL_OV_ODBC3;
    bool use_dsn= false;
    map<string,string>::const_iterator citer;

    if(attr) {
        string vers;
        citer = attr->find("version");
        if (citer != attr->end()) {
            vers = citer->second;
        }
        if(vers.find("3") != string::npos)
            version= SQL_OV_ODBC3;
        else if(vers.find("2") != string::npos)
            version= SQL_OV_ODBC2;
        citer = attr->find("use_dsn");
        if (citer != attr->end()) {
            use_dsn = citer->second == "true";
        }
    }

    CODBCContext* cntx=  new CODBCContext(version, use_dsn);
    if(cntx && attr) {
        string page_size;
        citer = attr->find("packet");
        if (citer != attr->end()) {
            page_size = citer->second;
        }
        if(!page_size.empty()) {
            SQLUINTEGER s= atoi(page_size.c_str());
            cntx->SetPacketSize(s);
      }
    }
    return cntx;
}



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
    TImplementation* drv = 0;
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
        drv = new CODBCContext( SQL_OV_ODBC3, tds_version, use_dsn );

        // Set parameters ...
        if ( page_size ) {
            drv->SetPacketSize( page_size );
        }

        if ( !client_charset.empty() ) {
            drv->SetClientCharset( client_charset );
        }
    }
    return drv;
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

class CDbapiOdbcCF_ftds64 : public CDbapiOdbcCFBase
{
public:
    CDbapiOdbcCF_ftds64(void)
    : CDbapiOdbcCFBase("ftds64_odbc")
    {
    }
};

class CDbapiOdbcCF_odbcw : public CDbapiOdbcCFBase
{
public:
    CDbapiOdbcCF_odbcw(void)
    : CDbapiOdbcCFBase("odbcw")
    {
    }
};


///////////////////////////////////////////////////////////////////////////////
extern "C"
{

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
    NCBI_EntryPoint_xdbapi_ftds64_odbc(
        CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
        CPluginManager<I_DriverContext>::EEntryPointRequest method)
    {
        CHostEntryPointImpl<CDbapiOdbcCF_ftds64>::NCBI_EntryPointImpl( info_list, method );
    }

    NCBI_DBAPIDRIVER_ODBC_EXPORT
    void
    NCBI_EntryPoint_xdbapi_odbcw(
        CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
        CPluginManager<I_DriverContext>::EEntryPointRequest method)
    {
        CHostEntryPointImpl<CDbapiOdbcCF_odbcw>::NCBI_EntryPointImpl( info_list, method );
    }

    NCBI_DBAPIDRIVER_ODBC_EXPORT
    void
    DBAPI_RegisterDriver_ODBC(void)
    {
        RegisterEntryPoint<I_DriverContext>( NCBI_EntryPoint_xdbapi_odbc );
        RegisterEntryPoint<I_DriverContext>( NCBI_EntryPoint_xdbapi_ftds64_odbc );
        RegisterEntryPoint<I_DriverContext>( NCBI_EntryPoint_xdbapi_odbcw );
    }

}

///////////////////////////////////////////////////////////////////////////////
NCBI_DBAPIDRIVER_ODBC_EXPORT
void DBAPI_RegisterDriver_ODBC(I_DriverMgr& mgr)
{
    mgr.RegisterDriver("odbc", ODBC_CreateContext);
    DBAPI_RegisterDriver_ODBC();
}

void DBAPI_RegisterDriver_ODBC_old(I_DriverMgr& mgr)
{
    DBAPI_RegisterDriver_ODBC( mgr );
}

extern "C" {
    NCBI_DBAPIDRIVER_ODBC_EXPORT
    void* DBAPI_E_odbc()
    {
        return (void*)DBAPI_RegisterDriver_ODBC_old;
    }
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.72  2006/12/26 17:41:39  ssikorsk
 * Implemented CODBCContext::SetupErrorReporter().
 *
 * Revision 1.71  2006/12/15 16:43:36  ssikorsk
 * Replaced CFastMutex with CMutex. Improved thread-safety.
 *
 * Revision 1.70  2006/10/26 14:56:08  ssikorsk
 * Initialize CODBCContext with client charset set to UTF-8 in case of UNICODE API.
 *
 * Revision 1.69  2006/10/05 20:41:14  ssikorsk
 * - #include <corelib/ncbiapp.hpp>
 *
 * Revision 1.68  2006/10/05 19:53:57  ssikorsk
 * Moved connection logic from CODBCContext to CODBC_Connection.
 *
 * Revision 1.67  2006/09/13 20:06:56  ssikorsk
 * Revamp code to support  unicode version of ODBC API.
 *
 * Revision 1.66  2006/08/31 15:00:24  ssikorsk
 * Handle ClientCharset.
 *
 * Revision 1.65  2006/08/21 18:16:16  ssikorsk
 * Set the SQL_ATTR_OUTPUT_NTS attribut with an environment handle.
 *
 * Revision 1.64  2006/08/18 15:16:07  ssikorsk
 * Minor improvement to CODBC_Reporter::ReportErrors.
 *
 * Revision 1.63  2006/08/10 15:24:22  ssikorsk
 * Revamp code to use new CheckXXX methods.
 *
 * Revision 1.62  2006/08/04 21:03:34  ssikorsk
 * Handle winsock2 on Windows only.
 *
 * Revision 1.61  2006/08/04 20:41:21  ssikorsk
 * Manually initialize/finalize winsock2 in case of FreeTDS.
 *
 * Revision 1.60  2006/08/01 17:01:28  ssikorsk
 * + NCBI_DBAPIDRIVER_ODBC_EXPORT to NCBI_EntryPoint_xdbapi_odbc/NCBI_EntryPoint_xdbapi_ftds64_odbc
 *
 * Revision 1.59  2006/07/31 23:24:37  ucko
 * +<algorithm> for find
 *
 * Revision 1.58  2006/07/31 15:52:06  ssikorsk
 * Added support for the FreeTDS odbc driver.
 *
 * Revision 1.57  2006/07/20 14:38:39  ssikorsk
 * More exception safe CODBCContext::x_Close.
 *
 * Revision 1.56  2006/07/12 17:11:11  ssikorsk
 * Fixed compilation isssues.
 *
 * Revision 1.55  2006/07/12 16:29:31  ssikorsk
 * Separated interface and implementation of CDB classes.
 *
 * Revision 1.54  2006/06/08 14:19:59  ssikorsk
 * Fixed a compilation problem.
 *
 * Revision 1.53  2006/06/07 22:20:16  ssikorsk
 * Context finalization improvements.
 *
 * Revision 1.52  2006/06/05 14:51:47  ssikorsk
 * Set value of m_MsgHandlers in I_DriverContext::Create_Connection.
 *
 * Revision 1.51  2006/06/02 19:37:40  ssikorsk
 * + NCBI_CATCH_ALL( NCBI_CURRENT_FUNCTION )
 *
 * Revision 1.50  2006/05/18 17:11:11  ssikorsk
 * Assign values to m_LoginTimeout and m_Timeout
 *
 * Revision 1.49  2006/05/05 16:11:04  ssikorsk
 * Added workaround for IRegistry.GetString
 *
 * Revision 1.48  2006/04/20 16:29:45  ssikorsk
 * Added handling of a registry's section "ODBC" and key "DRIVER_NAME" to the driver.
 * Multiword driver names must be single-quoted. Multiple driver names are allowed.
 *
 * Revision 1.47  2006/04/13 15:12:54  ssikorsk
 * Fixed erasing of an element from a std::vector.
 *
 * Revision 1.46  2006/04/05 14:33:16  ssikorsk
 * Improved CODBCContext::x_Close
 *
 * Revision 1.45  2006/03/15 19:56:37  ssikorsk
 * Replaced "static auto_ptr" with "static CSafeStaticPtr".
 *
 * Revision 1.44  2006/03/09 20:37:25  ssikorsk
 * Utilized method I_DriverContext:: CloseAllConn.
 *
 * Revision 1.43  2006/03/06 22:14:59  ssikorsk
 * Added CODBCContext::Close
 *
 * Revision 1.42  2006/03/02 17:20:58  ssikorsk
 * Report errors with SQLFreeHandle(SQL_HANDLE_ENV, m_Context)
 *
 * Revision 1.41  2006/02/28 15:14:30  ssikorsk
 * Replaced argument type SQLINTEGER on SQLLEN where needed.
 *
 * Revision 1.40  2006/02/28 15:00:45  ssikorsk
 * Use larger type (SQLLEN) instead of SQLINTEGER where it needs to be converted to a pointer.
 *
 * Revision 1.39  2006/02/22 15:15:51  ssikorsk
 * *** empty log message ***
 *
 * Revision 1.38  2006/02/17 17:58:47  ssikorsk
 * Initialize Connection::m_BCPable value using connection mode.
 *
 * Revision 1.37  2006/02/09 12:03:19  ssikorsk
 * Set severity level of error messages with native error num == 0 to informational.
 *
 * Revision 1.36  2006/02/07 17:24:01  ssikorsk
 * Added an extra space prior server name in the regular exception string.
 *
 * Revision 1.35  2006/02/01 13:59:19  ssikorsk
 * Report server's and user's names in case of a failed connection attempt.
 *
 * Revision 1.34  2006/01/23 13:42:13  ssikorsk
 * Renamed CODBCContext::MakeConnection to MakeIConnection;
 * Removed connection attribut checking from this method;
 *
 * Revision 1.33  2006/01/04 12:28:35  ssikorsk
 * Fix compilation issues
 *
 * Revision 1.32  2006/01/03 19:02:44  ssikorsk
 * Implement method MakeConnection.
 *
 * Revision 1.31  2005/11/28 13:22:59  ssikorsk
 * Report SQL statement and database connection parameters in case
 * of an error in addition to a server error message.
 *
 * Revision 1.30  2005/11/02 16:46:21  ssikorsk
 * Pass context information with an error message of a database exception.
 *
 * Revision 1.29  2005/11/02 15:59:47  ucko
 * Revert previous change in favor of supplying empty compilation info
 * via a static constant.
 *
 * Revision 1.28  2005/11/02 15:38:07  ucko
 * Replace CDiagCompileInfo() with DIAG_COMPILE_INFO, as the latter
 * automatically fills in some useful information and is less likely to
 * confuse compilers into thinking they're looking at function prototypes.
 *
 * Revision 1.27  2005/11/02 13:37:08  ssikorsk
 * Fixed file merging problems.
 *
 * Revision 1.26  2005/11/02 13:30:34  ssikorsk
 * Do not report function name, file name and line number in case of SQL Server errors.
 *
 * Revision 1.25  2005/11/02 12:58:38  ssikorsk
 * Report extra information in exceptions and error messages.
 *
 * Revision 1.24  2005/10/27 16:48:49  grichenk
 * Redesigned CTreeNode (added search methods),
 * removed CPairTreeNode.
 *
 * Revision 1.23  2005/09/19 14:19:05  ssikorsk
 * Use NCBI_CATCH_ALL macro instead of catch(...)
 *
 * Revision 1.22  2005/09/15 11:00:02  ssikorsk
 * Destructors do not throw exceptions any more.
 *
 * Revision 1.21  2005/06/07 16:22:51  ssikorsk
 * Included <dbapi/driver/driver_mgr.hpp> to make CDllResolver_Getter<I_DriverContext> explicitly visible.
 *
 * Revision 1.20  2005/04/04 13:03:57  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.19  2005/03/21 14:08:30  ssikorsk
 * Fixed the 'version' of a databases protocol parameter handling
 *
 * Revision 1.18  2005/03/02 21:19:20  ssikorsk
 * Explicitly call a new RegisterDriver function from the old one
 *
 * Revision 1.17  2005/03/02 19:29:54  ssikorsk
 * Export new RegisterDriver function on Windows
 *
 * Revision 1.16  2005/03/01 15:22:55  ssikorsk
 * Database driver manager revamp to use "core" CPluginManager
 *
 * Revision 1.15  2004/12/20 19:17:33  ssikorsk
 * Refactoring of dbapi/driver/samples
 *
 * Revision 1.14  2004/05/17 21:16:05  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.13  2004/04/07 13:41:48  gorelenk
 * Added export prefix to implementations of DBAPI_E_* functions.
 *
 * Revision 1.12  2004/03/17 19:25:38  gorelenk
 * Added NCBI_DBAPIDRIVER_ODBC_EXPORT export prefix for definition of
 * DBAPI_RegisterDriver_ODBC .
 *
 * Revision 1.11  2003/11/14 20:46:51  soussov
 * implements DoNotConnect mode
 *
 * Revision 1.10  2003/10/27 17:00:53  soussov
 * adds code to prevent the return of broken connection from the pool
 *
 * Revision 1.9  2003/07/21 21:58:08  soussov
 * fixes bug whith pool name mismatch in Connect()
 *
 * Revision 1.8  2003/07/18 19:20:34  soussov
 * removes SetPacketSize function
 *
 * Revision 1.7  2003/07/17 20:47:10  soussov
 * connections pool improvements
 *
 * Revision 1.6  2003/05/08 20:48:33  soussov
 * adding datadirect-odbc type of connecting string. Rememner that you need ODBCINI environment variable to make it works
 *
 * Revision 1.5  2003/05/05 20:48:29  ucko
 * Check HAVE_ODBCSS_H; fix some typos in an error message.
 *
 * Revision 1.4  2003/04/01 21:51:17  soussov
 * new attribute 'packet=XXX' (where XXX is a packet size) added to ODBC_CreateContext
 *
 * Revision 1.3  2002/07/03 21:48:50  soussov
 * adds DSN support if needed
 *
 * Revision 1.2  2002/07/02 20:52:54  soussov
 * adds RegisterDriver for ODBC
 *
 * Revision 1.1  2002/06/18 22:06:24  soussov
 * initial commit
 *
 *
 * ===========================================================================
 */
