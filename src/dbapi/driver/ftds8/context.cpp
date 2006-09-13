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
 * File Description:  Driver for TDS server
 *
 */

#include <ncbi_pch.hpp>

#include "ftds8_utils.hpp"

#include <corelib/ncbimtx.hpp>

#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <corelib/ncbi_safe_static.hpp>

// DO NOT DELETE this include !!!
#include <dbapi/driver/driver_mgr.hpp>

#include <dbapi/driver/ftds/interfaces.hpp>
#include <dbapi/driver/dblib/interfaces_p.hpp>
#include <dbapi/driver/util/numeric_convert.hpp>

#include <algorithm>

#ifdef NCBI_FTDS
#if defined(NCBI_OS_MSWIN)
#  include <winsock2.h>
#endif
#endif

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

private:
    CDblibContextRegistry(void);
    ~CDblibContextRegistry(void) throw();

    mutable CMutex          m_Mutex;
    vector<CDBLibContext*>  m_Registry;

    friend class CSafeStaticPtr<CDblibContextRegistry>;
};


/////////////////////////////////////////////////////////////////////////////
CDblibContextRegistry::CDblibContextRegistry(void)
{
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
//  CTDSContext::
//


extern "C" {
    static int s_TDS_err_callback(DBPROCESS* dblink,   int   severity,
                                  int        dberr,    int   oserr,
                                  char*      dberrstr, char* oserrstr)
    {
        return CTDSContext::TDS_dberr_handler
            (dblink, severity, dberr, oserr, dberrstr? dberrstr : "",
             oserrstr? oserrstr : "");
    }

    static int s_TDS_msg_callback(DBPROCESS* dblink,   DBINT msgno,
                                  int        msgstate, int   severity,
                                  char*      msgtxt,   char* srvname,
                                  char*      procname, int   line)
    {
        CTDSContext::TDS_dbmsg_handler
            (dblink, msgno,   msgstate, severity,
             msgtxt? msgtxt : "",
             srvname? srvname : "", procname? procname : "",
             line);
        return 0;
    }
}


static CTDSContext* g_pTDSContext = NULL;


CTDSContext::CTDSContext(DBINT version) :
    m_PacketSize(0),
    m_TDSVersion(version),
    m_Registry(NULL)
{
    DEFINE_STATIC_FAST_MUTEX(xMutex);
    CFastMutexGuard mg(xMutex);

    SetApplicationName("TDSDriver");

    CHECK_DRIVER_ERROR(
        g_pTDSContext != 0,
        "You cannot use more than one ftds contexts "
       "concurrently", 200000 );

    char hostname[256];
    if(gethostname(hostname, 256) == 0) {
        hostname[255]= '\0';
        SetHostName( hostname );
    }

    CHECK_DRIVER_ERROR(
        Check(dbinit()) != SUCCEED,
        "dbinit failed",
        200001 );

    Check(dberrhandle(s_TDS_err_callback));
    Check(dbmsghandle(s_TDS_msg_callback));

    g_pTDSContext = this;
    m_Login = Check(dblogin());

    m_Registry = &CDblibContextRegistry::Instance();
    x_AddToRegistry();
}

void
CTDSContext::x_AddToRegistry(void)
{
    if (m_Registry) {
        m_Registry->Add(this);
    }
}

void
CTDSContext::x_RemoveFromRegistry(void)
{
    if (m_Registry) {
        m_Registry->Remove(this);
    }
}

void
CTDSContext::x_SetRegistry(CDblibContextRegistry* registry)
{
    m_Registry = registry;
}

bool CTDSContext::ConnectedToMSSQLServer(void) const
{
    return (m_TDSVersion == DBVERSION_70 ||
            m_TDSVersion == DBVERSION_80 ||
            m_TDSVersion == DBVERSION_UNKNOWN);
}

int CTDSContext::GetTDSVersion(void) const
{
    return m_TDSVersion;
}

bool CTDSContext::SetLoginTimeout(unsigned int nof_secs)
{
    return I_DriverContext::SetLoginTimeout(nof_secs);
}


bool CTDSContext::SetTimeout(unsigned int nof_secs)
{
    return impl::CDriverContext::SetTimeout(nof_secs);
}


bool CTDSContext::SetMaxTextImageSize(size_t nof_bytes)
{
    impl::CDriverContext::SetMaxTextImageSize(nof_bytes);

    CFastMutexGuard mg(m_Mtx);

    char s[64];
    sprintf(s, "%lu", (unsigned long) GetMaxTextImageSize());

    return Check(dbsetopt(0, DBTEXTLIMIT, s, -1)) == SUCCEED
        /* && dbsetopt(0, DBTEXTSIZE, s, -1) == SUCCEED */;
}


void CTDSContext::SetClientCharset(const string& charset)
{
    _ASSERT( m_Login );
    _ASSERT( !charset.empty() );

    CDriverContext::SetClientCharset(charset);

    DBSETLCHARSET( m_Login, const_cast<char*>(charset.c_str()) );
}

impl::CConnection*
CTDSContext::MakeIConnection(const SConnAttr& conn_attr)
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

    CTDS_Connection* t_con = NULL;
    t_con = new CTDS_Connection(*this,
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

bool CTDSContext::IsAbleTo(ECapability cpb) const
{
    switch(cpb) {
    case eReturnITDescriptors:
        return true;
    case eReturnComputeResults:
    case eBcp:
    default:
        break;
    }
    return false;
}


CTDSContext::~CTDSContext()
{
    try {
        x_Close();
    }
    NCBI_CATCH_ALL( kEmptyStr )
}

void
CTDSContext::x_Close(bool delete_conn)
{
    if (g_pTDSContext) {
        CFastMutexGuard mg(m_Mtx);
        if (g_pTDSContext) {

            if (x_SafeToFinalize()) {
                // Unregister first
                g_pTDSContext = NULL;
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

                // Finalize client library even if we cannot close connections.
                dbloginfree(m_Login);
                CheckFunctCall();
                dbexit();
                CheckFunctCall();
            } else {
                g_pTDSContext = NULL;
                x_RemoveFromRegistry();
            }
        }
    } else {
        if (delete_conn && x_SafeToFinalize()) {
            DeleteAllConn();
        }
    }
}

bool CTDSContext::x_SafeToFinalize(void) const
{
    return true;
}


void CTDSContext::TDS_SetApplicationName(const string& app_name)
{
    SetApplicationName( app_name );
}


void CTDSContext::TDS_SetHostName(const string& host_name)
{
    SetHostName( host_name );
}


void CTDSContext::TDS_SetPacketSize(int p_size)
{
    m_PacketSize = (short) p_size;
}


bool CTDSContext::TDS_SetMaxNofConns(int n)
{
    return Check(dbsetmaxprocs(n)) == SUCCEED;
}


int CTDSContext::TDS_dberr_handler(DBPROCESS*    dblink,   int severity,
                                   int           dberr,    int /* oserr */,
                                   const string& dberrstr,
                                   const string& /* oserrstr */)
{
    string server_name;
    string user_name;
    string message = dberrstr;

    CTDS_Connection* link = dblink ?
        reinterpret_cast<CTDS_Connection*> (dbgetuserdata(dblink)) : 0;

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

            GetFTDS8ExceptionStorage().Accept(ex);
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

            GetFTDS8ExceptionStorage().Accept(ex);

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

            GetFTDS8ExceptionStorage().Accept(ex);
        }
        break;
    case EXNONFATAL:
    case EXCONVERSION:
    case EXSERVER:
    case EXPROGRAM:
        {
            CDB_ClientEx ex(kBlankCompileInfo,
                            0,
                            message,
                            eDiag_Error,
                            dberr);

            ex.SetServerName(server_name);
            ex.SetUserName(user_name);
            ex.SetSybaseSeverity(severity);

            GetFTDS8ExceptionStorage().Accept(ex);
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

            GetFTDS8ExceptionStorage().Accept(ex);
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

            GetFTDS8ExceptionStorage().Accept(ex);
        }
        break;
    }
    return INT_CANCEL;
}


void CTDSContext::TDS_dbmsg_handler(DBPROCESS*    dblink,   DBINT msgno,
                                    int           /* msgstate */,
                                    int           severity,
                                    const string& msgtxt,
                                    const string& srvname,
                                    const string& procname,
                                    int           line)
{
    string server_name;
    string user_name;
    string message = msgtxt;

    if (msgno == 5701  ||  msgno == 5703)
        return;

    CTDS_Connection* link = dblink ?
        reinterpret_cast<CTDS_Connection*>(dbgetuserdata(dblink)) : 0;

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

        GetFTDS8ExceptionStorage().Accept(ex);
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

            GetFTDS8ExceptionStorage().Accept(ex);
        } else {
            CDB_DSEx ex(kBlankCompileInfo,
                        0,
                        message,
                        sev,
                        msgno);

            ex.SetServerName(server_name);
            ex.SetUserName(user_name);
            ex.SetSybaseSeverity(severity);

            GetFTDS8ExceptionStorage().Accept(ex);
        }
    }
}


DBPROCESS* CTDSContext::x_ConnectToServer(const string&   srv_name,
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
#if 0
    if (mode & fPasswordEncrypted)
        DBSETLENCRYPT(m_Login, TRUE);
#endif


    tds_set_timeouts((tds_login*)(m_Login->tds_login), (int)GetLoginTimeout(),
                     (int)GetTimeout(), 0 /*(int)m_Timeout*/);
    tds_setTDS_version((tds_login*)(m_Login->tds_login), m_TDSVersion);
    return Check(dbopen(m_Login, (char*) srv_name.c_str()));
}


void CTDSContext::CheckFunctCall(void)
{
    GetFTDS8ExceptionStorage().Handle(GetCtxHandlerStack());
}


///////////////////////////////////////////////////////////////////////
// Driver manager related functions
//

I_DriverContext* FTDS_CreateContext(const map<string,string>* attr)
{
    DBINT version= DBVERSION_UNKNOWN;
    map<string,string>::const_iterator citer;

    if ( attr ) {
        citer = attr->find("reuse_context");
        if ( citer != attr->end() ) {
            if ( citer->second == "true" &&
                 g_pTDSContext ) {
                return g_pTDSContext;
            }
        }

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
        else if ( vers.find("100") != string::npos )
            version= DBVERSION_100;

    }
    CTDSContext* cntx=  new CTDSContext(version);
    if ( cntx && attr ) {
        string page_size;
        citer = attr->find("packet");
        if ( citer != attr->end() ) {
            page_size = citer->second;
        }
        if ( !page_size.empty() ) {
            int s= atoi(page_size.c_str());
            cntx->TDS_SetPacketSize(s);
        }
    }
    return cntx;
}


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


///////////////////////////////////////////////////////////////////////////////
const string kDBAPI_FTDS_DriverName("ftds");

class CDbapiFtdsCF2 : public CSimpleClassFactoryImpl<I_DriverContext, CTDSContext>
{
public:
    typedef CSimpleClassFactoryImpl<I_DriverContext, CTDSContext> TParent;

public:
    CDbapiFtdsCF2(void);
    ~CDbapiFtdsCF2(void);

public:
    virtual TInterface*
    CreateInstance(
        const string& driver  = kEmptyStr,
        CVersionInfo version =
        NCBI_INTERFACE_VERSION(I_DriverContext),
        const TPluginManagerParamTree* params = 0) const;

};

CDbapiFtdsCF2::CDbapiFtdsCF2(void)
    : TParent( kDBAPI_FTDS_DriverName, 0 )
{
    return ;
}

CDbapiFtdsCF2::~CDbapiFtdsCF2(void)
{
    return ;
}

CDbapiFtdsCF2::TInterface*
CDbapiFtdsCF2::CreateInstance(
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
        bool reuse_context = true;
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
                    reuse_context = (v.value != "false");
                    if ( reuse_context && g_pTDSContext ) {
                        return g_pTDSContext;
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
        drv = new CTDSContext( db_version );

        // Set parameters ...
        if ( page_size ) {
            drv->TDS_SetPacketSize( page_size );
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
NCBI_EntryPoint_xdbapi_ftds(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CDbapiFtdsCF2>::NCBI_EntryPointImpl( info_list, method );
}

NCBI_DBAPIDRIVER_DBLIB_EXPORT
void
DBAPI_RegisterDriver_FTDS(void)
{
    RegisterEntryPoint<I_DriverContext>( NCBI_EntryPoint_xdbapi_ftds );
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


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.82  2006/09/13 19:57:56  ssikorsk
 * Revamp code to use CWinSock.
 *
 * Revision 1.81  2006/08/21 18:03:26  ssikorsk
 * Adjusted winsock2 initialization/finalization.
 *
 * Revision 1.80  2006/07/28 15:00:56  ssikorsk
 * Revamp code to use CDB_Exception::SetSybaseSeverity.
 *
 * Revision 1.79  2006/07/20 14:41:30  ssikorsk
 * Put x_RemoveFromRegistry() after x_SafeToFinalize().
 *
 * Revision 1.78  2006/07/13 19:57:22  ssikorsk
 * Register CTDSContext with CTDSContextRegistry.
 *
 * Revision 1.77  2006/07/12 16:29:31  ssikorsk
 * Separated interface and implementation of CDB classes.
 *
 * Revision 1.76  2006/07/11 14:24:42  ssikorsk
 * Made method x_Close more exception safe.
 *
 * Revision 1.75  2006/06/07 22:19:54  ssikorsk
 * Context finalization improvements.
 *
 * Revision 1.74  2006/06/05 14:49:45  ssikorsk
 * Set value of m_MsgHandlers in I_DriverContext::Create_Connection.
 *
 * Revision 1.73  2006/05/30 18:54:01  ssikorsk
 * Revamp code to use server and user names with database exceptions;
 *
 * Revision 1.72  2006/05/18 16:59:49  ssikorsk
 * Assign values to m_LoginTimeout and m_Timeout.
 *
 * Revision 1.71  2006/05/11 18:14:12  ssikorsk
 * Fixed compilation issues
 *
 * Revision 1.70  2006/05/11 18:07:19  ssikorsk
 * Utilized new exception storage
 *
 * Revision 1.69  2006/05/10 14:47:46  ssikorsk
 * Implemented CTDSExceptions::CGuard;
 * Improved CTDSExceptions::Handle;
 *
 * Revision 1.68  2006/05/04 20:12:47  ssikorsk
 * Implemented classs CDBL_Cmd, CDBL_Result and CDBLExceptions;
 * Surrounded each native dblib call with Check;
 *
 * Revision 1.67  2006/04/18 19:07:13  ssikorsk
 * Added "client_charset" to DriverContext's attributes.
 *
 * Revision 1.66  2006/04/13 15:12:36  ssikorsk
 * Fixed erasing of an element from a std::vector.
 *
 * Revision 1.65  2006/04/05 14:31:28  ssikorsk
 * Improved CTDSContext::x_Close
 *
 * Revision 1.64  2006/03/29 21:31:39  ucko
 * +<algorithm> for find()
 *
 * Revision 1.63  2006/03/28 20:08:17  ssikorsk
 * Killed all CRs
 *
 * Revision 1.62  2006/03/28 19:16:56  ssikorsk
 * Added finalization logic to CDBLibContext
 *
 * Revision 1.61  2006/03/15 20:10:53  ssikorsk
 * Made more MT-safe.
 *
 * Revision 1.60  2006/03/09 19:04:17  ssikorsk
 * Utilized method I_DriverContext:: CloseAllConn.
 *
 * Revision 1.59  2006/03/09 17:20:43  ssikorsk
 * Replaced database error severity eDiag_Fatal with eDiag_Critical.
 *
 * Revision 1.58  2006/02/22 15:56:40  ssikorsk
 * CHECK_DRIVER_FATAL --> CHECK_DRIVER_ERROR
 *
 * Revision 1.57  2006/02/22 15:15:51  ssikorsk
 * *** empty log message ***
 *
 * Revision 1.56  2006/02/01 13:58:45  ssikorsk
 * Report server and user names in case of a failed connection attempt.
 *
 * Revision 1.55  2006/01/23 13:39:32  ssikorsk
 * Renamed CTDSContext::MakeConnection to MakeIConnection;
 *
 * Revision 1.54  2006/01/09 19:19:21  ssikorsk
 * Validate server name, user name and password before connection for all drivers.
 *
 * Revision 1.53  2006/01/03 19:02:22  ssikorsk
 * Implement method MakeConnection.
 *
 * Revision 1.52  2005/12/02 14:26:46  ssikorsk
 * Log server and user names with error message.
 *
 * Revision 1.51  2005/11/02 15:59:41  ucko
 * Revert previous change in favor of supplying empty compilation info
 * via a static constant.
 *
 * Revision 1.50  2005/11/02 15:38:02  ucko
 * Replace CDiagCompileInfo() with DIAG_COMPILE_INFO, as the latter
 * automatically fills in some useful information and is less likely to
 * confuse compilers into thinking they're looking at function prototypes.
 *
 * Revision 1.49  2005/11/02 13:30:34  ssikorsk
 * Do not report function name, file name and line number in case of SQL Server errors.
 *
 * Revision 1.48  2005/10/31 12:30:11  ssikorsk
 * Handle TDS v8.0
 *
 * Revision 1.47  2005/10/27 16:48:49  grichenk
 * Redesigned CTreeNode (added search methods),
 * removed CPairTreeNode.
 *
 * Revision 1.46  2005/10/20 13:08:16  ssikorsk
 * Fixed:
 * CTDSContext::TDS_SetApplicationName
 * CTDSContext::TDS_SetHostName
 *
 * Revision 1.45  2005/09/19 14:19:05  ssikorsk
 * Use NCBI_CATCH_ALL macro instead of catch(...)
 *
 * Revision 1.44  2005/09/16 14:45:53  ssikorsk
 * Improved the CDBLibContext::ConnectedToMSSQLServer method
 *
 * Revision 1.43  2005/09/15 11:00:02  ssikorsk
 * Destructors do not throw exceptions any more.
 *
 * Revision 1.42  2005/09/14 14:12:26  ssikorsk
 * Implement ConnectedToMSSQLServer and GetTDSVersion methods for the CFTDSContext class
 *
 * Revision 1.41  2005/07/20 13:17:39  ssikorsk
 * NCBI_DBAPIDRIVER_FTDS_EXPORT -> NCBI_DBAPIDRIVER_DBLIB_EXPORT
 *
 * Revision 1.40  2005/07/20 12:33:05  ssikorsk
 * Merged ftds/interfaces.hpp into dblib/interfaces.hpp
 *
 * Revision 1.39  2005/07/18 12:47:35  ssikorsk
 * Winsock32 cleanup;
 * WIN32 -> NCBI_OS_MSWIN;
 *
 * Revision 1.38  2005/07/12 13:36:19  ssikorsk
 * Added winsock initialization
 *
 * Revision 1.37  2005/06/07 16:22:51  ssikorsk
 * Included <dbapi/driver/driver_mgr.hpp> to make CDllResolver_Getter<I_DriverContext> explicitly visible.
 *
 * Revision 1.36  2005/04/04 13:03:57  ssikorsk
 * Revamp of DBAPI exception class CDB_Exception
 *
 * Revision 1.35  2005/03/21 14:08:30  ssikorsk
 * Fixed the 'version' of a databases protocol parameter handling
 *
 * Revision 1.34  2005/03/02 21:19:20  ssikorsk
 * Explicitly call a new RegisterDriver function from the old one
 *
 * Revision 1.33  2005/03/02 19:29:54  ssikorsk
 * Export new RegisterDriver function on Windows
 *
 * Revision 1.32  2005/03/01 15:22:55  ssikorsk
 * Database driver manager revamp to use "core" CPluginManager
 *
 * Revision 1.31  2005/02/22 22:30:56  soussov
 * Adds check for deadlock for err handler
 *
 * Revision 1.30  2005/02/02 19:49:54  grichenk
 * Fixed more warnings
 *
 * Revision 1.29  2004/12/20 16:20:29  ssikorsk
 * Refactoring of dbapi/driver/samples
 *
 * Revision 1.28  2004/12/14 20:46:24  ssikorsk
 * Changed WIN32 to _WIN32 in ftds8
 *
 * Revision 1.27  2004/12/10 15:26:11  ssikorsk
 * FreeTDS is ported on windows
 *
 * Revision 1.26  2004/05/17 21:13:37  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.25  2004/03/24 13:54:05  friedman
 * Fixed mutex comment
 *
 * Revision 1.24  2004/03/23 19:33:14  friedman
 * Replaced 'static CFastMutex' with DEFINE_STATIC_FAST_MUTEX
 *
 * Revision 1.23  2003/12/18 19:01:35  soussov
 * makes FTDS_CreateContext return an existing context if reuse_context option is set
 *
 * Revision 1.22  2003/11/14 20:46:29  soussov
 * implements DoNotConnect mode
 *
 * Revision 1.21  2003/10/27 17:00:44  soussov
 * adds code to prevent the return of broken connection from the pool
 *
 * Revision 1.20  2003/08/28 19:54:54  soussov
 * allowes print messages go to handler
 *
 * Revision 1.19  2003/07/21 22:00:56  soussov
 * fixes bug whith pool name mismatch in Connect()
 *
 * Revision 1.18  2003/07/17 20:46:30  soussov
 * connections pool improvements
 *
 * Revision 1.17  2003/04/18 20:27:00  soussov
 * fixes typo in Connect for reusable connection with specified connection pool
 *
 * Revision 1.16  2003/04/01 21:50:56  soussov
 * new attribute 'packet=XXX' (where XXX is a packet size) added to FTDS_CreateContext
 *
 * Revision 1.15  2003/03/17 15:29:38  soussov
 * sets the default host name using gethostname()
 *
 * Revision 1.14  2002/12/20 18:01:38  soussov
 * renames the members of ECapability enum
 *
 * Revision 1.13  2002/12/13 21:59:10  vakatov
 * Provide FreeTDS-version specific driver name and DLL entry point
 * ("ftds7" or "ftds8" in addition to the generic "ftds").
 * On the way, put a safeguard against accidental linking of more than
 * one version of FreeTDS driver to the same application.
 *
 * Revision 1.12  2002/09/19 18:53:13  soussov
 * check if driver was linked to Sybase dblib added
 *
 * Revision 1.10  2002/04/19 15:08:06  soussov
 * adds static mutex to Connect
 *
 * Revision 1.9  2002/04/12 22:13:01  soussov
 * mutex protection for contex constructor added
 *
 * Revision 1.8  2002/03/26 15:35:10  soussov
 * new image/text operations added
 *
 * Revision 1.7  2002/01/28 19:59:00  soussov
 * removing the long query timout
 *
 * Revision 1.6  2002/01/17 22:14:40  soussov
 * changes driver registration
 *
 * Revision 1.5  2002/01/15 17:13:30  soussov
 * renaming 'tds' driver to 'ftds' driver
 *
 * Revision 1.4  2002/01/14 20:38:48  soussov
 * timeout support for tds added
 *
 * Revision 1.3  2002/01/11 20:25:46  soussov
 * driver manager support added
 *
 * Revision 1.2  2001/11/06 18:00:02  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.1  2001/10/25 00:39:22  vakatov
 * Initial revision
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
