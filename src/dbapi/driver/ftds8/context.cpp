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

#include <corelib/ncbimtx.hpp>

#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>

#include <dbapi/driver/ftds/interfaces.hpp>
#include <dbapi/driver/util/numeric_convert.hpp>

#ifdef NCBI_FTDS
#ifdef _WIN32
#include <winsock2.h>
#endif
#endif

BEGIN_NCBI_SCOPE


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


CTDSContext* CTDSContext::m_pTDSContext = 0;


CTDSContext::CTDSContext(DBINT version) :
    m_AppName("TDSDriver"), m_HostName(""), m_PacketSize(0), m_TDSVersion(version)
{
    DEFINE_STATIC_FAST_MUTEX(xMutex);
    CFastMutexGuard mg(xMutex);

    if (m_pTDSContext != 0) {
        throw CDB_ClientEx(eDB_Error, 200000, "CTDSContext::CTDSContext",
                           "You cannot use more than one ftds contexts "
                           "concurrently");
    }

    char hostname[256];
    if(gethostname(hostname, 256) == 0) {
        hostname[255]= '\0';
        m_HostName= hostname;
    }

    if (dbinit() != SUCCEED) {
        throw CDB_ClientEx(eDB_Fatal, 200001, "CTDSContext::CTDSContext",
                           "dbinit failed");
    }

    m_Timeout= m_LoginTimeout= 0;

    dberrhandle(s_TDS_err_callback);
    dbmsghandle(s_TDS_msg_callback);

    m_pTDSContext = this;
    m_Login = dblogin();
}


bool CTDSContext::SetLoginTimeout(unsigned int nof_secs)
{
    m_LoginTimeout= nof_secs;
    return true;
    //    return dbsetlogintime(nof_secs) == SUCCEED;
}


bool CTDSContext::SetTimeout(unsigned int nof_secs)
{
    m_Timeout= nof_secs;
    return true;
    //    return dbsettime(nof_secs) == SUCCEED;
}


bool CTDSContext::SetMaxTextImageSize(size_t nof_bytes)
{
    char s[64];
    sprintf(s, "%lu", (unsigned long) nof_bytes);
    return dbsetopt(0, DBTEXTLIMIT, s, -1) == SUCCEED
        /* && dbsetopt(0, DBTEXTSIZE, s, -1) == SUCCEED */;
}


CDB_Connection* CTDSContext::Connect(const string&   srv_name,
                                     const string&   user_name,
                                     const string&   passwd,
                                     TConnectionMode mode,
                                     bool            reusable,
                                     const string&   pool_name)
{
    CTDS_Connection* t_con;

    // static CFastMutex xMutex;
    CFastMutexGuard mg(m_Mtx);

    if (reusable  &&  m_NotInUse.NofItems() > 0) { // try to reuse connection
        if (!pool_name.empty()) { // try to use pool name
            int n = m_NotInUse.NofItems();
            while (n--) {
                t_con = static_cast<CTDS_Connection*> (m_NotInUse.Get(n));
                if (pool_name.compare(t_con->PoolName()) == 0) {
                    m_NotInUse.Remove(n);
                    if(t_con->Refresh()) {
                        m_InUse.Add((TPotItem) t_con);
                        return Create_Connection(*t_con);
                    }
                    delete t_con;
                }
            }
        }
        else {
            if (srv_name.empty())
                return 0;
            int n = m_NotInUse.NofItems();
            // try to use server name
            while (n--) {
                t_con = static_cast<CTDS_Connection*> (m_NotInUse.Get(n));
                if (srv_name.compare(t_con->ServerName()) == 0) {
                    m_NotInUse.Remove(n);
                    if(t_con->Refresh()) {
                        m_InUse.Add((TPotItem) t_con);
                        return Create_Connection(*t_con);
                    }
                    delete t_con;
                }
            }
        }
    }

    if((mode & fDoNotConnect) != 0) return 0;
    // new connection needed
    if (srv_name.empty()  ||  user_name.empty()  ||  passwd.empty()) {
        throw CDB_ClientEx(eDB_Error, 200010, "CTDSContext::Connect",
                           "Insufficient info/credentials to connect");
    }

    DBPROCESS* dbcon = x_ConnectToServer(srv_name, user_name, passwd, mode);

    if (!dbcon) {
        throw CDB_ClientEx(eDB_Error, 200011, "CTDSContext::Connect",
                           "Cannot connect to server");
    }

    t_con = new CTDS_Connection(this, dbcon, reusable, pool_name);
    t_con->m_MsgHandlers = m_ConnHandlers;
    t_con->m_Server      = srv_name;
    t_con->m_User        = user_name;
    t_con->m_Passwd      = passwd;
    t_con->m_BCPAble     = (mode & fBcpIn) != 0;
    t_con->m_SecureLogin = (mode & fPasswordEncrypted) != 0;
    m_InUse.Add((TPotItem) t_con);
    return Create_Connection(*t_con);
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
    CTDS_Connection* t_con;

    // close all connections first
    for (int i = m_NotInUse.NofItems(); i--; ) {
        t_con = static_cast<CTDS_Connection*> (m_NotInUse.Get(i));
        delete t_con;
    }

    for (int i = m_InUse.NofItems(); i--; ) {
        t_con = static_cast<CTDS_Connection*> (m_InUse.Get(i));
        delete t_con;
    }

    dbloginfree(m_Login);
    dbexit();
    m_pTDSContext = 0;
}


void CTDSContext::TDS_SetApplicationName(const string& app_name)
{
    m_AppName = app_name;
}


void CTDSContext::TDS_SetHostName(const string& host_name)
{
    m_HostName = host_name;
}


void CTDSContext::TDS_SetPacketSize(int p_size)
{
    m_PacketSize = (short) p_size;
}


bool CTDSContext::TDS_SetMaxNofConns(int n)
{
    return dbsetmaxprocs(n) == SUCCEED;
}


int CTDSContext::TDS_dberr_handler(DBPROCESS*    dblink,   int severity,
                                   int           dberr,    int /* oserr */,
                                   const string& dberrstr,
                                   const string& /* oserrstr */)
{
    CTDS_Connection* link = dblink ?
        reinterpret_cast<CTDS_Connection*> (dbgetuserdata(dblink)) : 0;
    CDBHandlerStack* hs   = link ?
        &link->m_MsgHandlers : &m_pTDSContext->m_CntxHandlers;

    switch (dberr) {
    case SYBETIME:
    case SYBEFCON:
    case SYBECONN:
        {
            CDB_TimeoutEx to(dberr, "dblib", dberrstr);
            hs->PostMsg(&to);
        }
        return INT_TIMEOUT;
    default:
        if(dberr == 1205) {
            CDB_DeadlockEx dl("dblib", dberrstr);
            hs->PostMsg(&dl);
            return INT_CANCEL;
        }

        break;
    }


    switch (severity) {
    case EXINFO:
    case EXUSER:
        {
            CDB_ClientEx info(eDB_Info, dberr, "dblib", dberrstr);
            hs->PostMsg(&info);
        }
        break;
    case EXNONFATAL:
    case EXCONVERSION:
    case EXSERVER:
    case EXPROGRAM:
        {
            CDB_ClientEx err(eDB_Error, dberr, "dblib", dberrstr);
            hs->PostMsg(&err);
        }
        break;
    case EXTIME:
        {
            CDB_TimeoutEx to(dberr, "dblib", dberrstr);
            hs->PostMsg(&to);
        }
        return INT_TIMEOUT;
    default:
        {
            CDB_ClientEx ftl(eDB_Fatal, dberr, "dblib", dberrstr);
            hs->PostMsg(&ftl);
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
    if (msgno == 5701  ||  msgno == 5703)
        return;

    CTDS_Connection* link = dblink ?
        reinterpret_cast<CTDS_Connection*>(dbgetuserdata(dblink)) : 0;
    CDBHandlerStack* hs   = link ?
        &link->m_MsgHandlers : &m_pTDSContext->m_CntxHandlers;

    if (msgno == 1205/*DEADLOCK*/) {
        CDB_DeadlockEx dl(srvname, msgtxt);
        hs->PostMsg(&dl);
    } else {
        EDB_Severity sev =
            severity <  10 ? eDB_Info :
            severity == 10 ? eDB_Warning :
            severity <  16 ? eDB_Error :
            severity >  16 ? eDB_Fatal :
            eDB_Unknown;
        if (!procname.empty()) {
            CDB_RPCEx rpc(sev, msgno, srvname, msgtxt, procname, line);
            hs->PostMsg(&rpc);
        } else {
            CDB_DSEx m(sev, msgno, srvname, msgtxt);
            hs->PostMsg(&m);
        }
    }
}


DBPROCESS* CTDSContext::x_ConnectToServer(const string&   srv_name,
                                          const string&   user_name,
                                          const string&   passwd,
                                          TConnectionMode mode)
{
    if (!m_HostName.empty())
        DBSETLHOST(m_Login, (char*) m_HostName.c_str());
    if (m_PacketSize > 0)
        DBSETLPACKET(m_Login, m_PacketSize);
    if (DBSETLAPP (m_Login, (char*) m_AppName.c_str())
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


    tds_set_timeouts((tds_login*)(m_Login->tds_login), (int)m_LoginTimeout,
                     (int)m_Timeout, 0 /*(int)m_Timeout*/);
    tds_setTDS_version((tds_login*)(m_Login->tds_login), m_TDSVersion);
    return dbopen(m_Login, (char*) srv_name.c_str());
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
                 CTDSContext::m_pTDSContext ) {
                return CTDSContext::m_pTDSContext;
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

        if ( params != NULL ) {
            typedef TPluginManagerParamTree::TNodeList_CI TCIter;
            typedef TPluginManagerParamTree::TTreeValueType TValue;

            // Get parameters ...
            TCIter cit = params->SubNodeBegin();
            TCIter cend = params->SubNodeEnd();

            for (; cit != cend; ++cit) {
                const TValue& v = (*cit)->GetValue();

                if ( v.id == "reuse_context" ) {
                    reuse_context = (v.value != "false");
                    if ( reuse_context && CTDSContext::m_pTDSContext ) {
                        return CTDSContext::m_pTDSContext;
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
                    case 100 :
                        db_version = DBVERSION_100;
                        break;
                    }
                } else if ( v.id == "packet" ) {
                    page_size = NStr::StringToInt( v.value );
                }
            }
        }

        // Create a driver ...
        drv = new CTDSContext( db_version );

        // Set parameters ...
        if ( page_size ) {
            drv->TDS_SetPacketSize( page_size );
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

NCBI_DBAPIDRIVER_FTDS_EXPORT
void
DBAPI_RegisterDriver_FTDS(void)
{
    RegisterEntryPoint<I_DriverContext>( NCBI_EntryPoint_xdbapi_ftds );
}

///////////////////////////////////////////////////////////////////////////////
// NOTE:  we define a generic ("ftds") driver name here -- in order to
//        provide a default, but also to prevent from accidental linking
//        of more than one version of FreeTDS driver to the same application

NCBI_DBAPIDRIVER_FTDS_EXPORT
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

    NCBI_DBAPIDRIVER_FTDS_EXPORT
    void* DBAPI_E_ftds()
    {
        return NCBI_FTDS_ENTRY_POINT();
    }
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
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
