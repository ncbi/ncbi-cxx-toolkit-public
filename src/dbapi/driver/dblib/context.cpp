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

#include <corelib/ncbimtx.hpp>

#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>

#ifndef USE_MS_DBLIB
#  include <dbapi/driver/dblib/interfaces.hpp>
#  include <dbapi/driver/dblib/interfaces_p.hpp>
#else
#  include <dbapi/driver/msdblib/interfaces.hpp>
#  include <dbapi/driver/msdblib/interfaces_p.hpp>
#endif
#include <dbapi/driver/util/numeric_convert.hpp>


BEGIN_NCBI_SCOPE


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


CDBLibContext* CDBLibContext::m_pDBLibContext = 0;


CDBLibContext::CDBLibContext(DBINT version) :
    m_AppName("DBLibDriver"), m_HostName(""), m_PacketSize(0)
{
    DEFINE_STATIC_FAST_MUTEX(xMutex);
    CFastMutexGuard mg(xMutex);

    if (m_pDBLibContext != 0) {
        throw CDB_ClientEx(eDB_Error, 200000, "CDBLibContext::CDBLibContext",
                           "You cannot use more than one dblib context "
                           "concurrently");
    }

#ifdef MS_DBLIB_IN_USE
    if (dbinit() == NULL || version == 31415)
#else
        if (dbinit() != SUCCEED || dbsetversion(version) != SUCCEED)
#endif
            {
                throw CDB_ClientEx(eDB_Fatal, 200001, "CDBLibContext::CDBLibContext",
                                   "dbinit failed");
            }

    dberrhandle(s_DBLIB_err_callback);
    dbmsghandle(s_DBLIB_msg_callback);

    m_pDBLibContext = this;
    m_Login = dblogin();
}


bool CDBLibContext::SetLoginTimeout(unsigned int nof_secs)
{
    return dbsetlogintime(nof_secs) == SUCCEED;
}


bool CDBLibContext::SetTimeout(unsigned int nof_secs)
{
    return dbsettime(nof_secs) == SUCCEED;
}


bool CDBLibContext::SetMaxTextImageSize(size_t nof_bytes)
{
    char s[64];
    sprintf(s, "%lu", (unsigned long) nof_bytes);
#ifdef MS_DBLIB_IN_USE
    return dbsetopt(0, DBTEXTLIMIT, s) == SUCCEED;
#else
    return dbsetopt(0, DBTEXTLIMIT, s, -1) == SUCCEED;
#endif
}


CDB_Connection* CDBLibContext::Connect(const string&   srv_name,
                                       const string&   user_name,
                                       const string&   passwd,
                                       TConnectionMode mode,
                                       bool            reusable,
                                       const string&   pool_name)
{
    CDBL_Connection* t_con;

    // DEFINE_STATIC_FAST_MUTEX(xMutex);
    CFastMutexGuard mg(m_Mtx);

    if (reusable  &&  m_NotInUse.NofItems() > 0) { // try to reuse connection
        if (!pool_name.empty()) { // try to use pool name
            int n = m_NotInUse.NofItems();
            while (n--) {
                t_con = static_cast<CDBL_Connection*> (m_NotInUse.Get(n));
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
                t_con = static_cast<CDBL_Connection*> (m_NotInUse.Get(n));
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
        throw CDB_ClientEx(eDB_Error, 200010, "CDBLibContext::Connect",
                           "Insufficient info/credentials to connect");
    }

    DBPROCESS* dbcon = x_ConnectToServer(srv_name, user_name, passwd, mode);

    if (!dbcon) {
        throw CDB_ClientEx(eDB_Error, 200011, "CDBLibContext::Connect",
                           "Cannot connect to server");
    }

#ifdef MS_DBLIB_IN_USE
    dbsetopt(dbcon, DBTEXTLIMIT, "0" ); // No limit
    dbsetopt(dbcon, DBTEXTSIZE , "2147483647" ); // 0x7FFFFFFF
#endif

    t_con = new CDBL_Connection(this, dbcon, reusable, pool_name);
    t_con->m_MsgHandlers = m_ConnHandlers;
    t_con->m_Server      = srv_name;
    t_con->m_User        = user_name;
    t_con->m_Passwd      = passwd;
    t_con->m_BCPAble     = (mode & fBcpIn) != 0;
    t_con->m_SecureLogin = (mode & fPasswordEncrypted) != 0;
    m_InUse.Add((TPotItem) t_con);
    return Create_Connection(*t_con);
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
    CDBL_Connection* t_con;

    // close all connections first
    for (int i = m_NotInUse.NofItems(); i--; ) {
        t_con = static_cast<CDBL_Connection*> (m_NotInUse.Get(i));
        delete t_con;
    }

    for (int i = m_InUse.NofItems(); i--; ) {
        t_con = static_cast<CDBL_Connection*> (m_InUse.Get(i));
        delete t_con;
    }

#ifdef MS_DBLIB_IN_USE
    dbfreelogin(m_Login);
#else
    dbloginfree(m_Login);
#endif

    dbexit();
    m_pDBLibContext = 0;
}


void CDBLibContext::DBLIB_SetApplicationName(const string& app_name)
{
    m_AppName = app_name;
}


void CDBLibContext::DBLIB_SetHostName(const string& host_name)
{
    m_HostName = host_name;
}


void CDBLibContext::DBLIB_SetPacketSize(int p_size)
{
    m_PacketSize = (short) p_size;
}


bool CDBLibContext::DBLIB_SetMaxNofConns(int n)
{
    return dbsetmaxprocs(n) == SUCCEED;
}


int CDBLibContext::DBLIB_dberr_handler(DBPROCESS*    dblink,
                                       int           severity,
                                       int           dberr,
                                       int           /*oserr*/,
                                       const string& dberrstr,
                                       const string& /*oserrstr*/)
{
    CDBL_Connection* link = dblink ?
        reinterpret_cast<CDBL_Connection*> (dbgetuserdata(dblink)) : 0;
    CDBHandlerStack* hs   = link ?
        &link->m_MsgHandlers : &m_pDBLibContext->m_CntxHandlers;

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


void CDBLibContext::DBLIB_dbmsg_handler(DBPROCESS*    dblink,
                                        DBINT         msgno,
                                        int           /*msgstate*/,
                                        int           severity,
                                        const string& msgtxt,
                                        const string& srvname,
                                        const string& procname,
                                        int           line)
{
    if (msgno == 5701  ||  msgno == 5703)
        return;

    CDBL_Connection* link = dblink ?
        reinterpret_cast<CDBL_Connection*>(dbgetuserdata(dblink)) : 0;
    CDBHandlerStack* hs   = link ?
        &link->m_MsgHandlers : &m_pDBLibContext->m_CntxHandlers;

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


DBPROCESS* CDBLibContext::x_ConnectToServer(const string&   srv_name,
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

#ifndef MS_DBLIB_IN_USE
    if (mode & fPasswordEncrypted)
        DBSETLENCRYPT(m_Login, TRUE);
#endif

    return dbopen(m_Login, (char*) srv_name.c_str());
}



///////////////////////////////////////////////////////////////////////
// Driver manager related functions
//
#ifndef MS_DBLIB_IN_USE

I_DriverContext* DBLIB_CreateContext(const map<string,string>* attr = 0)
{
    DBINT version= DBVERSION_46;

    if ( attr ) {
        string vers;
        map<string,string>::const_iterator citer = attr->find("version");
        if ( citer != attr->end() ) {
            vers = citer->second;
        }
        if ( vers.find("46") != string::npos )
            version= DBVERSION_46;
        else if ( vers.find("100") != string::npos )
            version= DBVERSION_100;

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
    }
    return cntx;
}

void DBAPI_RegisterDriver_DBLIB(I_DriverMgr& mgr)
{
    mgr.RegisterDriver("dblib", DBLIB_CreateContext);
}

extern "C" {
    NCBI_DBAPIDRIVER_DBLIB_EXPORT
    void* DBAPI_E_dblib()
    {
        return (void*)DBAPI_RegisterDriver_DBLIB;
    }
}

#else

I_DriverContext* MSDBLIB_CreateContext(const map<string,string>* attr = 0)
{
    DBINT version= DBVERSION_46;

    return new CDBLibContext(version);
}

void DBAPI_RegisterDriver_MSDBLIB(I_DriverMgr& mgr)
{
    mgr.RegisterDriver("msdblib", MSDBLIB_CreateContext);
}

extern "C" {
    NCBI_DBAPIDRIVER_MSDBLIB_EXPORT
    void* DBAPI_E_msdblib()
    {
        return (void*)DBAPI_RegisterDriver_MSDBLIB;
    }
}

#endif


///////////////////////////////////////////////////////////////////////////////
#ifndef MS_DBLIB_IN_USE

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
        DBINT version = DBVERSION_46;

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

                if ( v.id == "version" ) {
                    version = NStr::StringToInt( v.value );
                    switch ( version ) {
                    case 46 :
                        version = DBVERSION_46;
                        break;
                    case 100 :
                        version = DBVERSION_100;
                    }
                } else if ( v.id == "packet" ) {
                    page_size = NStr::StringToInt( v.value );
                }
            }

            // Create a driver ...
            drv = new CDBLibContext(version);

            // Set parameters ...
            if ( page_size ) {
                drv->DBLIB_SetPacketSize( page_size );
            }
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

void
DBAPI_RegisterDriver_DBLIB(void)
{
    RegisterEntryPoint<I_DriverContext>( NCBI_EntryPoint_xdbapi_dblib );
}

#else

///////////////////////////////////////////////////////////////////////////////
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

///////////////////////////////////////////////////////////////////////////////
void
NCBI_EntryPoint_xdbapi_msdblib(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CDbapiMSDblibCF2>::NCBI_EntryPointImpl( info_list, method );
}

void
DBAPI_RegisterDriver_MSDBLIB(void)
{
    RegisterEntryPoint<I_DriverContext>( NCBI_EntryPoint_xdbapi_msdblib );
}

#endif

END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
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
