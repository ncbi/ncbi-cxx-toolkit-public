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
 * File Description:  Driver for DBLib server
 *
 */

#include <ncbi_pch.hpp>

#include "dblib_utils.hpp"

#include <corelib/ncbimtx.hpp>

#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <corelib/ncbi_safe_static.hpp>

// DO NOT DELETE this include !!!
#include <dbapi/driver/driver_mgr.hpp>

#include <dbapi/driver/dblib/interfaces.hpp>
#include <dbapi/driver/dblib/interfaces_p.hpp>

#include <dbapi/driver/util/numeric_convert.hpp>

#if defined(NCBI_OS_MSWIN)
#  include <winsock2.h>
#  include "../ncbi_win_hook.hpp"
#endif

#include <algorithm>
#include <stdio.h>

BEGIN_NCBI_SCOPE


static const CDiagCompileInfo kBlankCompileInfo;

/////////////////////////////////////////////////////////////////////////////
//
//  CDblibContextRegistry (Singleton)
//

class NCBI_DBAPIDRIVER_DBLIB_EXPORT CDblibContextRegistry
{
public:
    static CDblibContextRegistry& Instance(void);

    void Add(CDBLibContext* ctx);
    void Remove(CDBLibContext* ctx);
    void ClearAll(void);
    static void StaticClearAll(void);

    bool ExitProcessIsPatched(void) const
    {
        return m_ExitProcessPatched;
    }

private:
    CDblibContextRegistry(void);
    ~CDblibContextRegistry(void) throw();

    mutable CMutex          m_Mutex;
    vector<CDBLibContext*>  m_Registry;
    bool                    m_ExitProcessPatched;

    friend class CSafeStaticPtr<CDblibContextRegistry>;
};


/////////////////////////////////////////////////////////////////////////////
CDblibContextRegistry::CDblibContextRegistry(void) :
m_ExitProcessPatched(false)
{
#if defined(NCBI_OS_MSWIN)

    try {
        NWinHook::COnExitProcess::Instance().Add(CDblibContextRegistry::StaticClearAll);
        m_ExitProcessPatched = true;
    } catch (const NWinHook::CWinHookException&) {
        // Just in case ...
        m_ExitProcessPatched = false;
    }

#endif
}

CDblibContextRegistry::~CDblibContextRegistry(void) throw()
{
    try {
        ClearAll();
    }
    NCBI_CATCH_ALL( kEmptyStr )
}

CDblibContextRegistry&
CDblibContextRegistry::Instance(void)
{
    static CSafeStaticPtr<CDblibContextRegistry> instance;

    return instance.Get();
}

void
CDblibContextRegistry::Add(CDBLibContext* ctx)
{
    CMutexGuard mg(m_Mutex);

    vector<CDBLibContext*>::iterator it = find(m_Registry.begin(),
                                               m_Registry.end(),
                                               ctx);
    if (it == m_Registry.end()) {
        m_Registry.push_back(ctx);
    }
}

void
CDblibContextRegistry::Remove(CDBLibContext* ctx)
{
    CMutexGuard mg(m_Mutex);

    vector<CDBLibContext*>::iterator it = find(m_Registry.begin(),
                                              m_Registry.end(),
                                              ctx);

    if (it != m_Registry.end()) {
        m_Registry.erase(it);
        ctx->x_SetRegistry(NULL);
    }
}


void
CDblibContextRegistry::ClearAll(void)
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
CDblibContextRegistry::StaticClearAll(void)
{
    CDblibContextRegistry::Instance().ClearAll();
}

/////////////////////////////////////////////////////////////////////////////
//
//  CDBLibContext::
//


extern "C" {

#ifdef MS_DBLIB_IN_USE
    static int s_DBLIB_err_callback
    (DBPROCESS* dblink,   int   severity,
     int        dberr,    int   oserr,
     const char*  dberrstr, const char* oserrstr)
    {
        return CDBLibContext::DBLIB_dberr_handler
            (dblink, severity, dberr, oserr, dberrstr? dberrstr : "",
             oserrstr? oserrstr : "");
    }

    static int s_DBLIB_msg_callback
    (DBPROCESS* dblink,   DBINT msgno,
     int        msgstate, int   severity,
     const char*      msgtxt,   const char* srvname,
     const char*      procname, unsigned short   line)
    {
        CDBLibContext::DBLIB_dbmsg_handler
            (dblink, msgno,   msgstate, severity,
             msgtxt? msgtxt : "", srvname? srvname : "", procname? procname : "",
             line);
        return 0;
    }
#else
    static int CS_PUBLIC s_DBLIB_err_callback
    (DBPROCESS* dblink,   int   severity,
     int        dberr,    int   oserr,
     char*      dberrstr, char* oserrstr)
    {
        return CDBLibContext::DBLIB_dberr_handler
            (dblink, severity, dberr, oserr, dberrstr? dberrstr : "",
             oserrstr? oserrstr : "");
    }

    static int CS_PUBLIC s_DBLIB_msg_callback
    (DBPROCESS* dblink,   DBINT msgno,
     int        msgstate, int   severity,
     char*      msgtxt,   char* srvname,
     char*      procname, int   line)
    {
        CDBLibContext::DBLIB_dbmsg_handler
            (dblink, msgno,   msgstate, severity,
             msgtxt? msgtxt : "", srvname? srvname : "", procname? procname : "",
             line);
        return 0;
    }
#endif
}


static CDBLibContext* g_pContext = NULL;


CDBLibContext::CDBLibContext(DBINT version)
: m_PacketSize( 0 )
, m_TDSVersion( version )
{
    DEFINE_STATIC_FAST_MUTEX(xMutex);

    SetApplicationName("DBLibDriver");

    CFastMutexGuard mg(xMutex);

    CHECK_DRIVER_ERROR(
        g_pContext != NULL,
        "You cannot use more than one dblib context "
        "concurrently",
        200000 );

    char hostname[256];
    if(gethostname(hostname, 256) == 0) {
        hostname[255] = '\0';
        SetHostName( hostname );
    }

#ifdef MS_DBLIB_IN_USE
    if (Check(dbinit()) == NULL || version == 31415)
#else
    if (Check(dbinit()) != SUCCEED || Check(dbsetversion(version)) != SUCCEED)
#endif
    {
        DATABASE_DRIVER_ERROR( "dbinit failed", 200001 );
    }

    Check(dberrhandle(s_DBLIB_err_callback));
    Check(dbmsghandle(s_DBLIB_msg_callback));

    g_pContext = this;
    m_Login = NULL;
    m_Login = Check(dblogin());
    _ASSERT(m_Login);

    m_Registry = &CDblibContextRegistry::Instance();
    x_AddToRegistry();
}

void
CDBLibContext::x_AddToRegistry(void)
{
    if (m_Registry) {
        m_Registry->Add(this);
    }
}

void
CDBLibContext::x_RemoveFromRegistry(void)
{
    if (m_Registry) {
        m_Registry->Remove(this);
    }
}

void
CDBLibContext::x_SetRegistry(CDblibContextRegistry* registry)
{
    m_Registry = registry;
}

bool CDBLibContext::ConnectedToMSSQLServer(void) const
{
#if defined(MS_DBLIB_IN_USE)
    return true;
#elif defined(FTDS_IN_USE)
    return (m_TDSVersion == DBVERSION_70 ||
            m_TDSVersion == DBVERSION_80 ||
            m_TDSVersion == DBVERSION_UNKNOWN);
#else
    return false;
#endif
}

int CDBLibContext::GetTDSVersion(void) const
{
    return m_TDSVersion;
}

bool CDBLibContext::SetLoginTimeout(unsigned int nof_secs)
{
    if (I_DriverContext::SetLoginTimeout(nof_secs)) {
        return Check(dbsetlogintime(GetLoginTimeout())) == SUCCEED;
    }

    return false;
}


bool CDBLibContext::SetTimeout(unsigned int nof_secs)
{
    if (impl::CDriverContext::SetTimeout(nof_secs)) {
        return Check(dbsettime(GetTimeout())) == SUCCEED;
    }

    return false;
}


bool CDBLibContext::SetMaxTextImageSize(size_t nof_bytes)
{
    impl::CDriverContext::SetMaxTextImageSize(nof_bytes);

    CFastMutexGuard mg(m_Mtx);

    char s[64];
    sprintf(s, "%lu", (unsigned long) GetMaxTextImageSize());

#ifdef MS_DBLIB_IN_USE
    return Check(dbsetopt(0, DBTEXTLIMIT, s)) == SUCCEED;
#else
    return Check(dbsetopt(0, DBTEXTLIMIT, s, -1)) == SUCCEED;
#endif
}

void CDBLibContext::SetClientCharset(const string& charset)
{
    _ASSERT( m_Login );
    _ASSERT( !charset.empty() );

    CDriverContext::SetClientCharset(charset);

#ifndef MS_DBLIB_IN_USE
    DBSETLCHARSET( m_Login, const_cast<char*>(charset.c_str()) );
#endif
}

impl::CConnection*
CDBLibContext::MakeIConnection(const SConnAttr& conn_attr)
{
    CFastMutexGuard mg(m_Mtx);

    DBPROCESS* dbcon = x_ConnectToServer(conn_attr.srv_name,
                                         conn_attr.user_name,
                                         conn_attr.passwd,
                                         conn_attr.mode);

    if (!dbcon) {
        string err;

        err += "Cannot connect to the server '" + conn_attr.srv_name;
        err += "' as user '" + conn_attr.user_name + "'";
        DATABASE_DRIVER_ERROR( err, 200011 );
    }


#ifdef MS_DBLIB_IN_USE
    Check(dbsetopt(dbcon, DBTEXTLIMIT, "0" )); // No limit
    Check(dbsetopt(dbcon, DBTEXTSIZE , "2147483647" )); // 0x7FFFFFFF
#endif

    CDBL_Connection* t_con = NULL;
    t_con = new CDBL_Connection(*this,
                                dbcon,
                                conn_attr.reusable,
                                conn_attr.pool_name);

    t_con->SetServerName(conn_attr.srv_name);
    t_con->SetUserName(conn_attr.user_name);
    t_con->SetPassword(conn_attr.passwd);
    t_con->SetBCPable((conn_attr.mode & fBcpIn) != 0);
    t_con->SetSecureLogin((conn_attr.mode & fPasswordEncrypted) != 0);

    return t_con;
}


bool CDBLibContext::IsAbleTo(ECapability cpb) const
{
    switch(cpb) {
    case eBcp:
    case eReturnITDescriptors:
    case eReturnComputeResults:
        return true;
    default:
        break;
    }
    return false;
}


CDBLibContext::~CDBLibContext()
{
    try {
        x_Close();
    }
    NCBI_CATCH_ALL( kEmptyStr )
}

void
CDBLibContext::x_Close(bool delete_conn)
{
    if (g_pContext) {
        CFastMutexGuard mg(m_Mtx);
        if (g_pContext) {

            if (x_SafeToFinalize()) {
                // Unregister itself before any other poeration
                // for sake of exception safety.
                g_pContext = NULL;
                x_RemoveFromRegistry();

                // close all connections first
                try {
                    if (delete_conn) {
                        DeleteAllConn();
                    } else {
                        CloseAllConn();
                    }
                }
                NCBI_CATCH_ALL( kEmptyStr );

#ifdef MS_DBLIB_IN_USE
                dbfreelogin(m_Login);
                CheckFunctCall();
                dbwinexit();
                CheckFunctCall();
#else
                dbloginfree(m_Login);
                CheckFunctCall();
                try {
                    // This function can fail if we try to connect to MS SQL server
                    // using Sybase client.
                    dbexit();
                    CheckFunctCall();
                }
                NCBI_CATCH_ALL( "dbexit() call failed. This usually happens when "
                    "Sybase client has been used to connect to a MS SQL Server." )
#endif
            } else {
                g_pContext = NULL;
                x_RemoveFromRegistry();
            }
        }
    } else {
        if (delete_conn && x_SafeToFinalize()) {
            DeleteAllConn();
        }
    }
}

bool CDBLibContext::x_SafeToFinalize(void) const
{
    if (m_Registry) {
#if defined(NCBI_OS_MSWIN)
        return m_Registry->ExitProcessIsPatched();
#endif
    }

    return true;
}

void CDBLibContext::DBLIB_SetApplicationName(const string& app_name)
{
    SetApplicationName( app_name );
}


void CDBLibContext::DBLIB_SetHostName(const string& host_name)
{
    SetHostName( host_name );
}


void CDBLibContext::DBLIB_SetPacketSize(int p_size)
{
    m_PacketSize = (short) p_size;
}


bool CDBLibContext::DBLIB_SetMaxNofConns(int n)
{
    return Check(dbsetmaxprocs(n)) == SUCCEED;
}


int CDBLibContext::DBLIB_dberr_handler(DBPROCESS*    dblink,
                                       int           severity,
                                       int           dberr,
                                       int           /*oserr*/,
                                       const string& dberrstr,
                                       const string& /*oserrstr*/)
{
    string server_name;
    string user_name;
    string message = dberrstr;

    CDBL_Connection* link = dblink ?
        reinterpret_cast<CDBL_Connection*> (dbgetuserdata(dblink)) : 0;

    if ( link ) {
        server_name = link->ServerName();
        user_name = link->UserName();
    }

    switch (dberr) {
    case SYBETIME:
    case SYBEFCON:
    case SYBECONN:
        {
            CDB_TimeoutEx ex(kBlankCompileInfo,
                             0,
                             message,
                             dberr);

            ex.SetServerName(server_name);
            ex.SetUserName(user_name);
            ex.SetSybaseSeverity(severity);

            GetDBLExceptionStorage().Accept(ex);
        }
        return INT_TIMEOUT;
    default:
        if(dberr == 1205) {
            CDB_DeadlockEx ex(kBlankCompileInfo,
                              0,
                              message);

            ex.SetServerName(server_name);
            ex.SetUserName(user_name);
            ex.SetSybaseSeverity(severity);

            GetDBLExceptionStorage().Accept(ex);

            return INT_CANCEL;
        }

        break;
    }


    switch (severity) {
    case EXINFO:
    case EXUSER:
        {
            CDB_ClientEx ex(kBlankCompileInfo,
                            0,
                            message,
                            eDiag_Info,
                            dberr);

            ex.SetServerName(server_name);
            ex.SetUserName(user_name);
            ex.SetSybaseSeverity(severity);

            GetDBLExceptionStorage().Accept(ex);
        }
        break;
    case EXNONFATAL:
    case EXCONVERSION:
    case EXSERVER:
    case EXPROGRAM:
        if ( dberr != 20018 ) {
            CDB_ClientEx ex(kBlankCompileInfo,
                            0,
                            message,
                            eDiag_Error,
                            dberr);

            ex.SetServerName(server_name);
            ex.SetUserName(user_name);
            ex.SetSybaseSeverity(severity);

            GetDBLExceptionStorage().Accept(ex);
        }
        break;
    case EXTIME:
        {
            CDB_TimeoutEx ex(kBlankCompileInfo,
                             0,
                             message,
                             dberr);

            ex.SetServerName(server_name);
            ex.SetUserName(user_name);
            ex.SetSybaseSeverity(severity);

            GetDBLExceptionStorage().Accept(ex);
        }
        return INT_TIMEOUT;
    default:
        {
            CDB_ClientEx ex(kBlankCompileInfo,
                            0,
                            message,
                            eDiag_Critical,
                            dberr);

            ex.SetServerName(server_name);
            ex.SetUserName(user_name);
            ex.SetSybaseSeverity(severity);

            GetDBLExceptionStorage().Accept(ex);
        }
        break;
    }
    return INT_CANCEL;
}


void CDBLibContext::DBLIB_dbmsg_handler(DBPROCESS*    dblink,
                                        DBINT         msgno,
                                        int           /*msgstate*/,
                                        int           severity,
                                        const string& msgtxt,
                                        const string& srvname,
                                        const string& procname,
                                        int           line)
{
    string server_name;
    string user_name;
    string message = msgtxt;

    if (msgno == 5701 || msgno == 5703 || msgno == 5704)
        return;

    CDBL_Connection* link = dblink ?
        reinterpret_cast<CDBL_Connection*>(dbgetuserdata(dblink)) : 0;

    if ( link ) {
        server_name = link->ServerName();
        user_name = link->UserName();
    } else {
        server_name = srvname;
    }

    if (msgno == 1205/*DEADLOCK*/) {
        CDB_DeadlockEx ex(kBlankCompileInfo,
                          0,
                          message);

        ex.SetServerName(server_name);
        ex.SetUserName(user_name);
        ex.SetSybaseSeverity(severity);

        GetDBLExceptionStorage().Accept(ex);
    } else {
        EDiagSev sev =
            severity <  10 ? eDiag_Info :
            severity == 10 ? eDiag_Warning :
            severity <  16 ? eDiag_Error : eDiag_Critical;

        if (!procname.empty()) {
            CDB_RPCEx ex(kBlankCompileInfo,
                         0,
                         message,
                         sev,
                         msgno,
                         procname,
                         line);

            ex.SetServerName(server_name);
            ex.SetUserName(user_name);
            ex.SetSybaseSeverity(severity);

            GetDBLExceptionStorage().Accept(ex);
        } else {
            CDB_DSEx ex(kBlankCompileInfo,
                        0,
                        message,
                        sev,
                        msgno);

            ex.SetServerName(server_name);
            ex.SetUserName(user_name);
            ex.SetSybaseSeverity(severity);

            GetDBLExceptionStorage().Accept(ex);
        }
    }
}


DBPROCESS* CDBLibContext::x_ConnectToServer(const string&   srv_name,
                                            const string&   user_name,
                                            const string&   passwd,
                                            TConnectionMode mode)
{
    if (!GetHostName().empty())
        DBSETLHOST(m_Login, (char*) GetHostName().c_str());
    if (m_PacketSize > 0)
        DBSETLPACKET(m_Login, m_PacketSize);
    if (DBSETLAPP (m_Login, (char*) GetApplicationName().c_str())
        != SUCCEED ||
        DBSETLUSER(m_Login, (char*) user_name.c_str())
        != SUCCEED ||
        DBSETLPWD (m_Login, (char*) passwd.c_str())
        != SUCCEED)
        return 0;

    if (mode & fBcpIn)
        BCP_SETL(m_Login, TRUE);

#ifndef MS_DBLIB_IN_USE
    if (mode & fPasswordEncrypted)
        DBSETLENCRYPT(m_Login, TRUE);
#endif

    return Check(dbopen(m_Login, (char*) srv_name.c_str()));
}


void CDBLibContext::CheckFunctCall(void)
{
    GetDBLExceptionStorage().Handle(GetCtxHandlerStack());
}

///////////////////////////////////////////////////////////////////////
// Driver manager related functions
//
#if defined(MS_DBLIB_IN_USE)

I_DriverContext* MSDBLIB_CreateContext(const map<string,string>* attr = 0)
{
    DBINT version = DBVERSION_46;

    return new CDBLibContext(version);
}

#elif defined(FTDS_IN_USE)

I_DriverContext* FTDS_CreateContext(const map<string,string>* attr)
{
    DBINT version= DBVERSION_UNKNOWN;
    map<string,string>::const_iterator citer;
    string client_charset;

    if ( attr ) {
        //
        citer = attr->find("reuse_context");
        if ( citer != attr->end() ) {
            if ( citer->second == "true" && g_pContext ) {
                return g_pContext;
            }
        }

        //
        string vers;
        citer = attr->find("version");
        if ( citer != attr->end() ) {
            vers = citer->second;
        }
        if ( vers.find("42") != string::npos )
            version= DBVERSION_42;
        else if ( vers.find("46") != string::npos )
            version= DBVERSION_46;
        else if ( vers.find("70") != string::npos )
            version= DBVERSION_70;
        else if ( vers.find("80") != string::npos )
            version= DBVERSION_80;
        else if ( vers.find("100") != string::npos )
            version= DBVERSION_100;

        //
        citer = attr->find("client_charset");
        if ( citer != attr->end() ) {
            client_charset = citer->second;
        }

    }
    CDBLibContext* cntx=  new CDBLibContext(version);
    if ( cntx && attr ) {

        string page_size;
        citer = attr->find("packet");
        if ( citer != attr->end() ) {
            page_size = citer->second;
        }

        if ( !page_size.empty() ) {
            int s= atoi(page_size.c_str());
            cntx->DBLIB_SetPacketSize(s);
        }

        // Set client's charset ...
        if ( !client_charset.empty() ) {
            cntx->SetClientCharset( client_charset.c_str() );
        }
    }
    return cntx;
}


#else

I_DriverContext* DBLIB_CreateContext(const map<string,string>* attr = 0)
{
    DBINT version= DBVERSION_46;
    string client_charset;

    if ( attr ) {
        map<string,string>::const_iterator citer;

        //
        citer = attr->find("reuse_context");
        if ( citer != attr->end() ) {
            if ( citer->second == "true" && g_pContext ) {
                return g_pContext;
            }
        }

        //
        string vers;
        citer = attr->find("version");
        if ( citer != attr->end() ) {
            vers = citer->second;
        }

        if ( vers.find("46") != string::npos )
            version= DBVERSION_46;
        else if ( vers.find("100") != string::npos )
            version= DBVERSION_100;


        //
        citer = attr->find("client_charset");
        if ( citer != attr->end() ) {
            client_charset = citer->second;
        }
    }

    CDBLibContext* cntx= new CDBLibContext(version);
    if ( cntx && attr ) {

        string page_size;
        map<string,string>::const_iterator citer = attr->find("packet");
        if ( citer != attr->end() ) {
            page_size = citer->second;
        }

        if ( !page_size.empty() ) {
            int s= atoi(page_size.c_str());
            cntx->DBLIB_SetPacketSize(s);
        }

        // Set client's charset ...
        if ( !client_charset.empty() ) {
            cntx->SetClientCharset( client_charset.c_str() );
        }
    }
    return cntx;
}

#endif

///////////////////////////////////////////////////////////////////////////////
#if defined(MS_DBLIB_IN_USE)

const string kDBAPI_MSDBLIB_DriverName("msdblib");

class CDbapiMSDblibCF2 : public CSimpleClassFactoryImpl<I_DriverContext, CDBLibContext>
{
public:
    typedef CSimpleClassFactoryImpl<I_DriverContext, CDBLibContext> TParent;

public:
    CDbapiMSDblibCF2(void);
    ~CDbapiMSDblibCF2(void);

public:
    virtual TInterface*
    CreateInstance(
        const string& driver  = kEmptyStr,
        CVersionInfo version =
        NCBI_INTERFACE_VERSION(I_DriverContext),
        const TPluginManagerParamTree* params = 0) const;

};

CDbapiMSDblibCF2::CDbapiMSDblibCF2(void)
    : TParent( kDBAPI_MSDBLIB_DriverName, 0 )
{
    return ;
}

CDbapiMSDblibCF2::~CDbapiMSDblibCF2(void)
{
    return ;
}

CDbapiMSDblibCF2::TInterface*
CDbapiMSDblibCF2::CreateInstance(
    const string& driver,
    CVersionInfo version,
    const TPluginManagerParamTree* params) const
{
    TImplementation* drv = NULL;

    if ( !driver.empty()  &&  driver != m_DriverName ) {
        return 0;
    }
    if (version.Match(NCBI_INTERFACE_VERSION(I_DriverContext))
                        != CVersionInfo::eNonCompatible) {
        // Mandatory parameters ....
        DBINT version = DBVERSION_46;

        drv = new CDBLibContext(version);
    }
    return drv;
}

void
NCBI_EntryPoint_xdbapi_msdblib(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CDbapiMSDblibCF2>::NCBI_EntryPointImpl( info_list, method );
}

NCBI_DBAPIDRIVER_DBLIB_EXPORT
void
DBAPI_RegisterDriver_MSDBLIB(void)
{
    RegisterEntryPoint<I_DriverContext>( NCBI_EntryPoint_xdbapi_msdblib );
}

NCBI_DBAPIDRIVER_DBLIB_EXPORT
void DBAPI_RegisterDriver_MSDBLIB(I_DriverMgr& mgr)
{
    mgr.RegisterDriver("msdblib", MSDBLIB_CreateContext);
    DBAPI_RegisterDriver_MSDBLIB();
}

void DBAPI_RegisterDriver_MSDBLIB_old(I_DriverMgr& mgr)
{
    DBAPI_RegisterDriver_MSDBLIB( mgr );
}

extern "C" {
    NCBI_DBAPIDRIVER_DBLIB_EXPORT
    void* DBAPI_E_msdblib()
    {
        return (void*)DBAPI_RegisterDriver_MSDBLIB_old;
    }
}


#elif defined(FTDS_IN_USE)

#define NCBI_FTDS 8

///////////////////////////////////////////////////////////////////////////////
// Version-specific driver name and DLL entry point
#if defined(NCBI_FTDS)
#  if   NCBI_FTDS == 7
#    define NCBI_FTDS_DRV_NAME    "ftds7"
#    define NCBI_FTDS_ENTRY_POINT DBAPI_E_ftds7
#  elif NCBI_FTDS == 8
#    define NCBI_FTDS_DRV_NAME    "ftds8"
#    define NCBI_FTDS_ENTRY_POINT DBAPI_E_ftds8
#  elif
#    error "This version of FreeTDS is not supported"
#  endif
#endif

const string kDBAPI_FTDS_DriverName("ftds");

class CDbapiFtdsCFBase : public CSimpleClassFactoryImpl<I_DriverContext, CDBLibContext>
{
public:
    typedef CSimpleClassFactoryImpl<I_DriverContext, CDBLibContext> TParent;

public:
    CDbapiFtdsCFBase(const string& driver_name);
    ~CDbapiFtdsCFBase(void);

public:
    virtual TInterface*
    CreateInstance(
        const string& driver  = kEmptyStr,
        CVersionInfo version =
        NCBI_INTERFACE_VERSION(I_DriverContext),
        const TPluginManagerParamTree* params = 0) const;

};

CDbapiFtdsCFBase::CDbapiFtdsCFBase(const string& driver_name)
    : TParent(driver_name, 1)
{
    return ;
}

CDbapiFtdsCFBase::~CDbapiFtdsCFBase(void)
{
    return ;
}

CDbapiFtdsCFBase::TInterface*
CDbapiFtdsCFBase::CreateInstance(
    const string& driver,
    CVersionInfo version,
    const TPluginManagerParamTree* params) const
{
    TImplementation* drv = 0;
    if ( !driver.empty() && driver != m_DriverName ) {
        return 0;
    }
    if (version.Match(NCBI_INTERFACE_VERSION(I_DriverContext))
                        != CVersionInfo::eNonCompatible) {
        // Mandatory parameters ....
        DBINT db_version = DBVERSION_UNKNOWN;

        // Optional parameters ...
        CS_INT page_size = 0;

        string client_charset;

        if ( params != NULL ) {
            typedef TPluginManagerParamTree::TNodeList_CI TCIter;
            typedef TPluginManagerParamTree::TValueType   TValue;

            // Get parameters ...
            TCIter cit = params->SubNodeBegin();
            TCIter cend = params->SubNodeEnd();

            for (; cit != cend; ++cit) {
                const TValue& v = (*cit)->GetValue();

                if ( v.id == "reuse_context" ) {
                    bool reuse_context = (v.value != "false");
                    if ( reuse_context && g_pContext ) {
                        return g_pContext;
                    }
                } else if ( v.id == "version" ) {
                    int value = NStr::StringToInt( v.value );

                    switch ( value ) {
                    case 42 :
                        db_version = DBVERSION_42;
                        break;
                    case 46 :
                        db_version = DBVERSION_46;
                        break;
                    case 70 :
                        db_version = DBVERSION_70;
                        break;
                    case 80 :
                        db_version = DBVERSION_80;
                        break;
                    case 100 :
                        db_version = DBVERSION_100;
                        break;
                    }
                } else if ( v.id == "packet" ) {
                    page_size = NStr::StringToInt( v.value );
                } else if ( v.id == "client_charset" ) {
                    client_charset = v.value;
                }
            }
        }

        // Create a driver ...
        drv = new CDBLibContext( db_version );

        // Set parameters ...
        if ( page_size ) {
            drv->DBLIB_SetPacketSize( page_size );
        }

        // Set client's charset ...
        if ( !client_charset.empty() ) {
            drv->SetClientCharset( client_charset.c_str() );
        }
    }
    return drv;
}

class CDbapiFtdsCF : public CDbapiFtdsCFBase
{
public:
    CDbapiFtdsCF(void)
    : CDbapiFtdsCFBase(kDBAPI_FTDS_DriverName)
    {
    }
};

class CDbapiFtdsCF63 : public CDbapiFtdsCFBase
{
public:
    CDbapiFtdsCF63(void)
    : CDbapiFtdsCFBase("ftds63")
    {
    }
};

class CDbapiFtdsCF64 : public CDbapiFtdsCFBase
{
public:
    CDbapiFtdsCF64(void)
    : CDbapiFtdsCFBase("ftds64_dblib")
    {
    }
};

///////////////////////////////////////////////////////////////////////////////
void
NCBI_EntryPoint_xdbapi_ftds(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CDbapiFtdsCF>::NCBI_EntryPointImpl( info_list, method );
}

void
NCBI_EntryPoint_xdbapi_ftds63(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CDbapiFtdsCF63>::NCBI_EntryPointImpl( info_list, method );
}

void
NCBI_EntryPoint_xdbapi_ftds64_dblib(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CDbapiFtdsCF64>::NCBI_EntryPointImpl( info_list, method );
}

NCBI_DBAPIDRIVER_DBLIB_EXPORT
void
DBAPI_RegisterDriver_FTDS(void)
{
    RegisterEntryPoint<I_DriverContext>( NCBI_EntryPoint_xdbapi_ftds );
    RegisterEntryPoint<I_DriverContext>( NCBI_EntryPoint_xdbapi_ftds63 );
    RegisterEntryPoint<I_DriverContext>( NCBI_EntryPoint_xdbapi_ftds64_dblib );
}

///////////////////////////////////////////////////////////////////////////////
// NOTE:  we define a generic ("ftds") driver name here -- in order to
//        provide a default, but also to prevent from accidental linking
//        of more than one version of FreeTDS driver to the same application

NCBI_DBAPIDRIVER_DBLIB_EXPORT
void DBAPI_RegisterDriver_FTDS(I_DriverMgr& mgr)
{
    mgr.RegisterDriver(NCBI_FTDS_DRV_NAME, FTDS_CreateContext);
    mgr.RegisterDriver("ftds",             FTDS_CreateContext);
    DBAPI_RegisterDriver_FTDS();
}

void DBAPI_RegisterDriver_FTDS_old(I_DriverMgr& mgr)
{
    DBAPI_RegisterDriver_FTDS(mgr);
}

extern "C" {
    void* NCBI_FTDS_ENTRY_POINT()
    {
        if (dbversion())  return 0;  /* to prevent linking to Sybase dblib */
        return (void*) DBAPI_RegisterDriver_FTDS_old;
    }

    NCBI_DBAPIDRIVER_DBLIB_EXPORT
    void* DBAPI_E_ftds()
    {
        return NCBI_FTDS_ENTRY_POINT();
    }
}

#else

///////////////////////////////////////////////////////////////////////////////
const string kDBAPI_DBLIB_DriverName("dblib");

class CDbapiDblibCF2 : public CSimpleClassFactoryImpl<I_DriverContext, CDBLibContext>
{
public:
    typedef CSimpleClassFactoryImpl<I_DriverContext, CDBLibContext> TParent;

public:
    CDbapiDblibCF2(void);
    ~CDbapiDblibCF2(void);

public:
    virtual TInterface*
    CreateInstance(
        const string& driver  = kEmptyStr,
        CVersionInfo version =
        NCBI_INTERFACE_VERSION(I_DriverContext),
        const TPluginManagerParamTree* params = 0) const;

};

CDbapiDblibCF2::CDbapiDblibCF2(void)
    : TParent( kDBAPI_DBLIB_DriverName, 0 )
{
    return ;
}

CDbapiDblibCF2::~CDbapiDblibCF2(void)
{
    return ;
}

CDbapiDblibCF2::TInterface*
CDbapiDblibCF2::CreateInstance(
    const string& driver,
    CVersionInfo version,
    const TPluginManagerParamTree* params) const
{
    TImplementation* drv = 0;
    if ( !driver.empty() && driver != m_DriverName ) {
        return 0;
    }
    if (version.Match(NCBI_INTERFACE_VERSION(I_DriverContext))
                        != CVersionInfo::eNonCompatible) {
        // Mandatory parameters ....
        DBINT db_version = DBVERSION_46;

        // Optional parameters ...
        CS_INT page_size = 0;
        string client_charset;

        if ( params != NULL ) {
            typedef TPluginManagerParamTree::TNodeList_CI TCIter;
            typedef TPluginManagerParamTree::TValueType   TValue;

            // Get parameters ...
            TCIter cit = params->SubNodeBegin();
            TCIter cend = params->SubNodeEnd();

            for (; cit != cend; ++cit) {
                const TValue& v = (*cit)->GetValue();

                if ( v.id == "reuse_context" ) {
                    bool reuse_context = (v.value != "false");
                    if ( reuse_context && g_pContext ) {
                        return g_pContext;
                    }
                } else if ( v.id == "version" ) {
                    int value = NStr::StringToInt( v.value );
                    switch ( value ) {
                    case 46 :
                        db_version = DBVERSION_46;
                        break;
                    case 100 :
                        db_version = DBVERSION_100;
                    }
                } else if ( v.id == "packet" ) {
                    page_size = NStr::StringToInt( v.value );
                } else if ( v.id == "client_charset" ) {
                    client_charset = v.value;
                }
            }
        }

        // Create a driver ...
        drv = new CDBLibContext( db_version );

        // Set parameters ...
        if ( page_size ) {
            drv->DBLIB_SetPacketSize( page_size );
        }

        // Set client's charset ...
        if ( !client_charset.empty() ) {
            drv->SetClientCharset( client_charset.c_str() );
        }
    }
    return drv;
}

///////////////////////////////////////////////////////////////////////////////
void
NCBI_EntryPoint_xdbapi_dblib(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CDbapiDblibCF2>::NCBI_EntryPointImpl( info_list, method );
}

NCBI_DBAPIDRIVER_DBLIB_EXPORT
void
DBAPI_RegisterDriver_DBLIB(void)
{
    RegisterEntryPoint<I_DriverContext>( NCBI_EntryPoint_xdbapi_dblib );
}

///////////////////////////////////////////////////////////////////////////////
NCBI_DBAPIDRIVER_DBLIB_EXPORT
void DBAPI_RegisterDriver_DBLIB(I_DriverMgr& mgr)
{
    mgr.RegisterDriver("dblib", DBLIB_CreateContext);
    DBAPI_RegisterDriver_DBLIB();
}

void DBAPI_RegisterDriver_DBLIB_old(I_DriverMgr& mgr)
{
    DBAPI_RegisterDriver_DBLIB(mgr);
}

extern "C" {
    NCBI_DBAPIDRIVER_DBLIB_EXPORT
    void* DBAPI_E_dblib()
    {
        return (void*)DBAPI_RegisterDriver_DBLIB_old;
    }
}

#endif

END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.90  2006/09/13 19:53:56  ssikorsk
 * Revamp code to use CWinSock.
 *
 * Revision 1.89  2006/08/02 18:49:03  ssikorsk
 * Added implementation of NCBI_EntryPoint_xdbapi_ftds64_dblib.
 *
 * Revision 1.88  2006/07/28 15:00:49  ssikorsk
 * Revamp code to use CDB_Exception::SetSybaseSeverity.
 *
 * Revision 1.87  2006/07/20 14:41:09  ssikorsk
 * Put x_RemoveFromRegistry() after x_SafeToFinalize().
 *
 * Revision 1.86  2006/07/12 16:29:30  ssikorsk
 * Separated interface and implementation of CDB classes.
 *
 * Revision 1.85  2006/07/11 14:24:41  ssikorsk
 * Made method x_Close more exception safe.
 *
 * Revision 1.84  2006/06/07 22:19:46  ssikorsk
 * Context finalization improvements.
 *
 * Revision 1.83  2006/06/05 14:49:38  ssikorsk
 * Set value of m_MsgHandlers in I_DriverContext::Create_Connection.
 *
 * Revision 1.82  2006/05/30 18:53:52  ssikorsk
 * Revamp code to use server and user names with database exceptions;
 *
 * Revision 1.81  2006/05/18 16:58:03  ssikorsk
 * Assign values to m_LoginTimeout and m_Timeout.
 *
 * Revision 1.80  2006/05/15 19:41:28  ssikorsk
 * Fixed CTLibContext::x_SafeToFinalize in case of Unix.
 *
 * Revision 1.79  2006/05/11 19:10:27  ssikorsk
 * Ignore messages having msgno == 5704
 *
 * Revision 1.78  2006/05/11 18:14:01  ssikorsk
 * Fixed compilation issues
 *
 * Revision 1.77  2006/05/11 18:07:34  ssikorsk
 * Utilized new exception storage
 *
 * Revision 1.76  2006/05/10 14:47:00  ssikorsk
 * Implemented CDBLExceptions::CGuard;
 * Improved CDBLExceptions::Handle;
 *
 * Revision 1.75  2006/05/04 20:24:32  ssikorsk
 * Fixed compilation issues in case of MSDBLIB.
 *
 * Revision 1.74  2006/05/04 20:12:17  ssikorsk
 * Implemented classs CDBL_Cmd, CDBL_Result and CDBLExceptions;
 * Surrounded each native dblib call with Check;
 *
 * Revision 1.73  2006/04/13 15:12:16  ssikorsk
 * Fixed erasing of an element from a std::vector.
 *
 * Revision 1.72  2006/04/05 14:30:17  ssikorsk
 * Improved CDBLibContext::x_Close
 *
 * Revision 1.71  2006/03/29 21:26:37  ucko
 * +<algorithm> for find()
 *
 * Revision 1.70  2006/03/28 20:07:17  ssikorsk
 * Killed all CRs
 *
 * Revision 1.69  2006/03/28 19:16:56  ssikorsk
 * Added finalization logic to CDBLibContext
 *
 * Revision 1.68  2006/03/15 20:03:57  ssikorsk
 * Made more MT-safe.
 *
 * Revision 1.67  2006/03/09 19:03:58  ssikorsk
 * Utilized method I_DriverContext:: CloseAllConn.
 *
 * Revision 1.66  2006/03/09 17:21:16  ssikorsk
 * Replaced database error severity eDiag_Fatal with eDiag_Critical.
 *
 * Revision 1.65  2006/02/22 15:15:50  ssikorsk
 * *** empty log message ***
 *
 * Revision 1.64  2006/02/01 13:58:29  ssikorsk
 * Report server and user names in case of a failed connection attempt.
 *
 * Revision 1.63  2006/01/23 13:38:52  ssikorsk
 * Renamed CDBLibContext::MakeConnection to MakeIConnection;
 *
 * Revision 1.62  2006/01/11 18:06:10  ssikorsk
 * Do not call dbexit with Sybase on Windows.
 *
 * Revision 1.61  2006/01/09 19:19:21  ssikorsk
 * Validate server name, user name and password before connection for all drivers.
 *
 * Revision 1.60  2006/01/03 19:01:46  ssikorsk
 * Implement method MakeConnection.
 *
 * Revision 1.59  2005/12/02 14:15:01  ssikorsk
 *  Log server and user names with error message.
 *
 * Revision 1.58  2005/11/02 15:59:36  ucko
 * Revert previous change in favor of supplying empty compilation info
 * via a static constant.
 *
 * Revision 1.57  2005/11/02 15:37:57  ucko
 * Replace CDiagCompileInfo() with DIAG_COMPILE_INFO, as the latter
 * automatically fills in some useful information and is less likely to
 * confuse compilers into thinking they're looking at function prototypes.
 *
 * Revision 1.56  2005/11/02 13:30:33  ssikorsk
 * Do not report function name, file name and line number in case of SQL Server errors.
 *
 * Revision 1.55  2005/10/31 12:19:59  ssikorsk
 * Do not use separate include files for msdblib.
 *
 * Revision 1.54  2005/10/27 16:48:49  grichenk
 * Redesigned CTreeNode (added search methods),
 * removed CPairTreeNode.
 *
 * Revision 1.53  2005/10/26 11:33:02  ssikorsk
 * Handle TDS v8.0 in case of the ftds driiver
 *
 * Revision 1.52  2005/10/24 11:07:22  ssikorsk
 * Fixed CDbapiFtdsCFBase::CDbapiFtdsCFBase typo.
 *
 * Revision 1.51  2005/10/20 13:07:20  ssikorsk
 * Fixed:
 * CDBLibContext::DBLIB_SetApplicationName
 * CDBLibContext::DBLIB_SetHostName
 * An attempt to register two entry points within one driver
 * for the ftds63 driver
 *
 * Revision 1.50  2005/09/19 14:19:05  ssikorsk
 * Use NCBI_CATCH_ALL macro instead of catch(...)
 *
 * Revision 1.49  2005/09/16 14:45:53  ssikorsk
 * Improved the CDBLibContext::ConnectedToMSSQLServer method
 *
 * Revision 1.48  2005/09/15 11:00:01  ssikorsk
 * Destructors do not throw exceptions any more.
 *
 * Revision 1.47  2005/09/14 14:11:42  ssikorsk
 * Implement ConnectedToMSSQLServer and GetTDSVersion methods for the CDBLibContext class
 *
 * Revision 1.46  2005/07/18 18:11:31  ssikorsk
 * Export DBAPI_RegisterDriver_MSDBLIB(I_DriverMgr& mgr)
 *
 * Revision 1.45  2005/07/18 17:50:21  ssikorsk
 * Export DBAPI_RegisterDriver_DBLIB(I_DriverMgr& mgr)
 *
 * Revision 1.44  2005/07/18 12:50:28  ssikorsk
 * Changed patch level from 0 to 1;
 * Winsock32 cleanup;
 * WIN32 -> NCBI_OS_MSWIN;
 *
 * Revision 1.43  2005/07/12 13:22:59  ssikorsk
 * Added initialization of winsock
 *
 * Revision 1.42  2005/07/07 20:34:16  vasilche
 * Added #include <stdio.h> for sprintf().
 *
 * Revision 1.41  2005/07/07 19:12:55  ssikorsk
 * Improved to support a ftds driver
 *
 * Revision 1.40  2005/06/07 16:22:51  ssikorsk
 * Included <dbapi/driver/driver_mgr.hpp> to make CDllResolver_Getter<I_DriverContext> explicitly visible.
 *
 * Revision 1.39  2005/04/14 18:35:32  ssikorsk
 * Prepare code for correct driver deinitialization
 *
 * Revision 1.38  2005/04/04 13:03:57  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.37  2005/03/21 16:30:31  dicuccio
 * Permit driver initialization even if parameter lists are not supplied
 *
 * Revision 1.36  2005/03/21 14:08:30  ssikorsk
 * Fixed the 'version' of a databases protocol parameter handling
 *
 * Revision 1.35  2005/03/02 21:19:20  ssikorsk
 * Explicitly call a new RegisterDriver function from the old one
 *
 * Revision 1.34  2005/03/02 19:29:54  ssikorsk
 * Export new RegisterDriver function on Windows
 *
 * Revision 1.33  2005/03/01 15:22:55  ssikorsk
 * Database driver manager revamp to use "core" CPluginManager
 *
 * Revision 1.32  2004/12/20 16:20:29  ssikorsk
 * Refactoring of dbapi/driver/samples
 *
 * Revision 1.31  2004/06/16 16:08:51  ivanov
 * Added export specifier for DBAPI_E_dblib()
 *
 * Revision 1.30  2004/05/17 21:12:41  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.29  2004/04/08 12:18:35  ivanov
 * Fixed name of export specifier for DBAPI_E_msdblib()
 *
 * Revision 1.28  2004/04/07 13:41:47  gorelenk
 * Added export prefix to implementations of DBAPI_E_* functions.
 *
 * Revision 1.27  2003/11/14 20:46:21  soussov
 * implements DoNotConnect mode
 *
 * Revision 1.26  2003/10/27 17:00:29  soussov
 * adds code to prevent the return of broken connection from the pool
 *
 * Revision 1.25  2003/08/28 19:54:36  soussov
 * allowes print messages go to handler
 *
 * Revision 1.24  2003/07/21 22:00:47  soussov
 * fixes bug whith pool name mismatch in Connect()
 *
 * Revision 1.23  2003/07/17 20:46:02  soussov
 * connections pool improvements
 *
 * Revision 1.22  2003/04/18 20:26:39  soussov
 * fixes typo in Connect for reusable connection with specified connection pool
 *
 * Revision 1.21  2003/04/01 21:50:26  soussov
 * new attribute 'packet=XXX' (where XXX is a packet size) added to DBLIB_CreateContext
 *
 * Revision 1.20  2002/12/20 17:56:33  soussov
 * renames the members of ECapability enum
 *
 * Revision 1.19  2002/09/19 20:05:43  vasilche
 * Safe initialization of static mutexes
 *
 * Revision 1.18  2002/07/10 15:47:46  soussov
 * fixed typo in DBAPI_RegisterDriver_MSDBLIB
 *
 * Revision 1.17  2002/07/09 17:01:53  soussov
 * the register functions for msdblib was renamed
 *
 * Revision 1.16  2002/07/02 16:05:49  soussov
 * splitting Sybase dblib and MS dblib
 *
 * Revision 1.15  2002/06/19 15:02:03  soussov
 * changes default version from unknown to 46
 *
 * Revision 1.14  2002/06/17 21:26:32  soussov
 * dbsetversion added
 *
 * Revision 1.13  2002/04/19 15:05:50  soussov
 * adds static mutex to Connect
 *
 * Revision 1.12  2002/04/12 22:13:09  soussov
 * mutex protection for contex constructor added
 *
 * Revision 1.11  2002/03/26 15:37:52  soussov
 * new image/text operations added
 *
 * Revision 1.10  2002/02/26 17:53:25  sapojnik
 * Removed blob size limits for MS SQL
 *
 * Revision 1.9  2002/01/17 22:12:47  soussov
 * changes driver registration
 *
 * Revision 1.8  2002/01/15 15:45:50  sapojnik
 * Use DBVERSION_46/100 constants only ifndef NCBI_OS_MSWIN
 *
 * Revision 1.7  2002/01/11 20:25:08  soussov
 * driver manager support added
 *
 * Revision 1.6  2002/01/08 18:10:18  sapojnik
 * Syabse to MSSQL name translations moved to interface_p.hpp
 *
 * Revision 1.5  2002/01/03 17:01:56  sapojnik
 * fixing CR/LF mixup
 *
 * Revision 1.4  2002/01/03 15:46:23  sapojnik
 * ported to MS SQL (about 12 'ifdef NCBI_OS_MSWIN' in 6 files)
 *
 * Revision 1.3  2001/10/24 16:40:00  lavr
 * Comment out unused function arguments
 *
 * Revision 1.2  2001/10/22 18:38:49  soussov
 * sending NULL instead of emty string fixed
 *
 * Revision 1.1  2001/10/22 15:19:55  lavr
 * This is a major revamp (by Anton Lavrentiev, with help from Vladimir
 * Soussov and Denis Vakatov) of the DBAPI "driver" libs originally
 * written by Vladimir Soussov. The revamp follows the one of CTLib
 * driver, and it involved massive code shuffling and grooming, numerous
 * local API redesigns, adding comments and incorporating DBAPI to
 * the C++ Toolkit.
 *
 * ===========================================================================
 */
