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
#include <dbapi/error_codes.hpp>

#include <algorithm>

#ifdef NCBI_FTDS
#if defined(NCBI_OS_MSWIN)
#  include <winsock2.h>
#endif
#endif

#if defined(NCBI_OS_MSWIN)
#  include "../ncbi_win_hook.hpp"
#endif


#define NCBI_USE_ERRCODE_X   Dbapi_Ftds8_Context

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
inline
CDiagCompileInfo GetBlankCompileInfo(void)
{
    return CDiagCompileInfo();
}


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
CDblibContextRegistry::CDblibContextRegistry(void)
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
    NCBI_CATCH_ALL_X( 1, NCBI_CURRENT_FUNCTION )
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


DEFINE_STATIC_FAST_MUTEX(s_CtxMutex);
static CTDSContext* g_pTDSContext = NULL;


CTDSContext::CTDSContext(DBINT version)
: m_PacketSize(0)
, m_TDSVersion(version)
, m_Registry(NULL)
, m_BufferSize(1)
{
    CFastMutexGuard mg(s_CtxMutex);

    SetApplicationName("TDSDriver");

    CHECK_DRIVER_ERROR(
        g_pTDSContext != 0,
        "You cannot use more than one ftds contexts "
       "concurrently", 200000 );

    ResetEnvSybase();

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

    char s[64];
    sprintf(s, "%lu", (unsigned long) GetMaxTextImageSize());

    CFastMutexGuard ctx_mg(s_CtxMutex);
    return Check(dbsetopt(0, DBTEXTLIMIT, s, -1)) == SUCCEED
        /* && dbsetopt(0, DBTEXTSIZE, s, -1) == SUCCEED */;
}


impl::CConnection*
CTDSContext::MakeIConnection(const CDBConnParams& params)
{
    return new CTDS_Connection(*this, params);
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
    NCBI_CATCH_ALL_X( 2, NCBI_CURRENT_FUNCTION )
}

void
CTDSContext::x_Close(bool delete_conn)
{
    CMutexGuard mg(m_CtxMtx);

    if (g_pTDSContext) {
        CFastMutexGuard ctx_mg(s_CtxMutex);

        if (g_pTDSContext) {
            // Unregister first
            g_pTDSContext = NULL;
            x_RemoveFromRegistry();

            if (x_SafeToFinalize()) {
                // close all connections first
                try {
                    if (delete_conn) {
                        DeleteAllConn();
                    } else {
                        CloseAllConn();
                    }
                }
                NCBI_CATCH_ALL_X( 3, NCBI_CURRENT_FUNCTION )

                // Finalize client library even if we cannot close connections.
                dbexit();
                CheckFunctCall();
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
    if (m_Registry) {
#if defined(NCBI_OS_MSWIN)
        return m_Registry->ExitProcessIsPatched();
#endif
    }

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
    CFastMutexGuard mg(s_CtxMutex);
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
            CDB_TimeoutEx ex(GetBlankCompileInfo(),
                             0,
                             message,
                             dberr);

            ex.SetServerName(server_name);
            ex.SetUserName(user_name);
            ex.SetSybaseSeverity(severity);

            GetFTDS8ExceptionStorage().Accept(ex);
        }
        return INT_TIMEOUT;
    case 50000:
        {
            CDB_TruncateEx ex(GetBlankCompileInfo(),
                              0,
                              message,
                              dberr);

            ex.SetServerName(server_name);
            ex.SetUserName(user_name);
            ex.SetSybaseSeverity(severity);

            GetFTDS8ExceptionStorage().Accept(ex);

            return INT_CANCEL;
        }
    default:
        if(dberr == 1205) {
            CDB_DeadlockEx ex(GetBlankCompileInfo(),
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
            CDB_ClientEx ex(GetBlankCompileInfo(),
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
            CDB_ClientEx ex(GetBlankCompileInfo(),
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
            CDB_TimeoutEx ex(GetBlankCompileInfo(),
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
            CDB_ClientEx ex(GetBlankCompileInfo(),
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
        CDB_DeadlockEx ex(GetBlankCompileInfo(),
                          0,
                          message);

        ex.SetServerName(server_name);
        ex.SetUserName(user_name);
        ex.SetSybaseSeverity(severity);

        GetFTDS8ExceptionStorage().Accept(ex);
    }
    else if (msgno == 1708) {
        // "The table has been created but its maximum row size exceeds the maximum number of bytes
        //  per row (8060). INSERT or UPDATE of a row in this table will fail if the resulting row
        //  length exceeds 8060 bytes."
        ERR_POST_X(5, Warning << msgtxt);
    }
    else {
        EDiagSev sev =
            severity <  10 ? eDiag_Info :
            severity == 10 ? eDiag_Warning :
            severity <  16 ? eDiag_Error : eDiag_Critical;

        if (!procname.empty()) {
            CDB_RPCEx ex(GetBlankCompileInfo(),
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
            CDB_DSEx ex(GetBlankCompileInfo(),
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


void CTDSContext::CheckFunctCall(void)
{
    GetFTDS8ExceptionStorage().Handle(GetCtxHandlerStack());
}


///////////////////////////////////////////////////////////////////////
// Driver manager related functions
//

I_DriverContext* FTDS_CreateContext(const map<string,string>* attr)
{
    DBINT version = DBVERSION_UNKNOWN;
    map<string,string>::const_iterator citer;

    if ( attr ) {
        citer = attr->find("reuse_context");
        if ( citer != attr->end() ) {
            CFastMutexGuard mg(s_CtxMutex);

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
            version = DBVERSION_42;
        else if ( vers.find("46") != string::npos )
            version = DBVERSION_46;
        else if ( vers.find("70") != string::npos )
            version = DBVERSION_70;
        else if ( vers.find("100") != string::npos )
            version = DBVERSION_100;

    }

    CTDSContext* cntx =  new CTDSContext(version);
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
class CDbapiFtdsCFBase : public CSimpleClassFactoryImpl<I_DriverContext, CTDSContext>
{
public:
    typedef CSimpleClassFactoryImpl<I_DriverContext, CTDSContext> TParent;

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
: TParent(driver_name, 0)
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
    auto_ptr<TImplementation> drv;

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
        string buffer_size;

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
                    CFastMutexGuard mg(s_CtxMutex);

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
                } else if ( v.id == "buffer_size" ) {
                    buffer_size = v.value;
                }
            }
        }

        // Create a driver ...
        drv.reset(new CTDSContext(db_version));

        // Set parameters ...
        if ( page_size ) {
            drv->TDS_SetPacketSize( page_size );
        }

        // Set client's charset ...
        if ( !client_charset.empty() ) {
            drv->SetClientCharset( client_charset.c_str() );
        }

        // Set buffer size ...
        if ( !buffer_size.empty() ) {
            drv->SetBufferSize(NStr::StringToUInt(buffer_size));
        }
    }

    return drv.release();
}

///////////////////////////////////////////////////////////////////////////////
class CDbapiFtdsCF_ftds8 : public CDbapiFtdsCFBase
{
public:
    CDbapiFtdsCF_ftds8(void)
    : CDbapiFtdsCFBase("ftds8")
    {
    }
};


class CDbapiFtdsCF_ftds : public CDbapiFtdsCFBase
{
public:
    CDbapiFtdsCF_ftds(void)
    : CDbapiFtdsCFBase("ftds")
    {
    }
};


///////////////////////////////////////////////////////////////////////////////
void
NCBI_EntryPoint_xdbapi_ftds(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CDbapiFtdsCF_ftds>::NCBI_EntryPointImpl( info_list, method );
}

void
NCBI_EntryPoint_xdbapi_ftds8(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CDbapiFtdsCF_ftds8>::NCBI_EntryPointImpl( info_list, method );
}

NCBI_DBAPIDRIVER_DBLIB_EXPORT
void
DBAPI_RegisterDriver_FTDS(void)
{
    RegisterEntryPoint<I_DriverContext>( NCBI_EntryPoint_xdbapi_ftds );
    RegisterEntryPoint<I_DriverContext>( NCBI_EntryPoint_xdbapi_ftds8 );
}


END_NCBI_SCOPE


