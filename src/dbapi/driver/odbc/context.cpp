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

#include <dbapi/driver/odbc/interfaces.hpp>
#include <dbapi/driver/util/numeric_convert.hpp>

#ifdef HAVE_ODBCSS_H
#include <odbcss.h>
#endif

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//
//  CODBC_Reporter::
//
void CODBC_Reporter::ReportErrors()
{
    SQLINTEGER NativeError;
    SQLSMALLINT MsgLen;
    SQLCHAR SqlState[6];
    SQLCHAR Msg[1024];

    if(!m_HStack) return;

    for(SQLSMALLINT i= 1; i < 128; i++) {
        switch(SQLGetDiagRec(m_HType, m_Handle, i, SqlState, &NativeError,
                             Msg, sizeof(Msg), &MsgLen)) {
        case SQL_SUCCESS:
            if(strncmp((const char*)SqlState, "HYT", 3) == 0) { // timeout
                CDB_TimeoutEx to(DIAG_COMPILE_INFO,
                                 0,
                                 (const char*)Msg,
                                 NativeError);

                m_HStack->PostMsg(&to);
            }
            else if(strncmp((const char*)SqlState, "40001", 5) == 0) { // deadlock
                CDB_DeadlockEx dl(DIAG_COMPILE_INFO,
                                  0,
                                  (const char*)Msg);
                m_HStack->PostMsg(&dl);
            }
            else if(NativeError != 5701 && NativeError != 5703){
                CDB_SQLEx se(DIAG_COMPILE_INFO,
                             0,
                             (const char*)Msg,
                             eDiag_Warning,
                             NativeError,
                             (const char*)SqlState,
                             0);
                m_HStack->PostMsg(&se);
            }
            continue;

        case SQL_NO_DATA: break;

        case SQL_SUCCESS_WITH_INFO:
            {
                CDB_DSEx dse(DIAG_COMPILE_INFO,
                             0,
                             "Message is too long to be retrieved",
                             eDiag_Warning,
                             777);
                m_HStack->PostMsg(&dse);
            }
            continue;

        default:
            {
                CDB_ClientEx ce(DIAG_COMPILE_INFO,
                                0,
                                "SQLGetDiagRec failed (memory corruption suspected",
                                eDiag_Warning,
                                420016);
                m_HStack->PostMsg(&ce);
            }
            break;
        }
        break;
    }
}

/////////////////////////////////////////////////////////////////////////////
//
//  CODBCContext::
//




CODBCContext::CODBCContext(SQLINTEGER version, bool use_dsn) : m_Reporter(0, SQL_HANDLE_ENV, 0)
{

    if(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_Context) != SQL_SUCCESS) {
        DATABASE_DRIVER_FATAL( "Cannot allocate a context", 400001 );
    }

    m_Reporter.SetHandle(m_Context);
    m_Reporter.SetHandlerStack(&m_CntxHandlers);

    SQLSetEnvAttr(m_Context, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)version, 0);


    m_PacketSize= 0;
    m_LoginTimeout= 0;
    m_Timeout= 0;
    m_TextImageSize= 0;
    m_UseDSN= use_dsn;
}


bool CODBCContext::SetLoginTimeout(unsigned int nof_secs)
{
    m_LoginTimeout= (SQLUINTEGER)nof_secs;
    return true;
}


bool CODBCContext::SetTimeout(unsigned int nof_secs)
{
    m_Timeout= (SQLUINTEGER)nof_secs;

    for (int i = m_NotInUse.NofItems(); i--;) {
        CODBC_Connection* t_con
            = static_cast<CODBC_Connection*> (m_NotInUse.Get(i));
        t_con->ODBC_SetTimeout(m_Timeout);
    }

    for (int i = m_InUse.NofItems(); i--;) {
        CODBC_Connection* t_con
            = static_cast<CODBC_Connection*> (m_NotInUse.Get(i));
        t_con->ODBC_SetTimeout(m_Timeout);
    }
    return true;
}


bool CODBCContext::SetMaxTextImageSize(size_t nof_bytes)
{
    m_TextImageSize = (SQLUINTEGER) nof_bytes;
    for (int i = m_NotInUse.NofItems(); i--;) {
        CODBC_Connection* t_con
            = static_cast<CODBC_Connection*> (m_NotInUse.Get(i));
        t_con->ODBC_SetTextImageSize(m_TextImageSize);
    }

    for (int i = m_InUse.NofItems(); i--;) {
        CODBC_Connection* t_con
            = static_cast<CODBC_Connection*> (m_NotInUse.Get(i));
        t_con->ODBC_SetTextImageSize(m_TextImageSize);
    }
    return true;
}


CDB_Connection* CODBCContext::Connect(const string&   srv_name,
                                      const string&   user_name,
                                      const string&   passwd,
                                      TConnectionMode mode,
                                      bool            reusable,
                                      const string&   pool_name)
{
    // static CFastMutex xMutex;
    CFastMutexGuard mg(m_Mtx);

    if (reusable  &&  m_NotInUse.NofItems() > 0) {
        // try to get a connection from the pot
        if ( !pool_name.empty() ) {
            // use a pool name
            for (int i = m_NotInUse.NofItems();  i--; ) {
                CODBC_Connection* t_con
                    = static_cast<CODBC_Connection*> (m_NotInUse.Get(i));

                if (pool_name.compare(t_con->PoolName()) == 0) {
                    m_NotInUse.Remove(i);
                    if(t_con->Refresh()) {
                        m_InUse.Add((TPotItem) t_con);
                        return Create_Connection(*t_con);
                    }
                    delete t_con;
                }
            }
        }
        else {

            if ( srv_name.empty() )
                return 0;

            // try to use a server name
            for (int i = m_NotInUse.NofItems();  i--; ) {
                CODBC_Connection* t_con
                    = static_cast<CODBC_Connection*> (m_NotInUse.Get(i));

                if (srv_name.compare(t_con->ServerName()) == 0) {
                    m_NotInUse.Remove(i);
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
        DATABASE_DRIVER_ERROR( "You have to provide server name, user name and "
                           "password to connect to the server", 100010 );
    }

    SQLHDBC con = x_ConnectToServer(srv_name, user_name, passwd, mode);
    if (con == 0) {
        DATABASE_DRIVER_ERROR( "Cannot connect to the server", 100011 );
    }

    CODBC_Connection* t_con = new CODBC_Connection(this, con, reusable, pool_name);
    t_con->m_MsgHandlers = m_ConnHandlers;
    t_con->m_Server      = srv_name;
    t_con->m_User        = user_name;
    t_con->m_Passwd      = passwd;
//    t_con->m_BCPAble     = (mode & fBcpIn) != 0;
    t_con->m_SecureLogin = (mode & fPasswordEncrypted) != 0;

    m_InUse.Add((TPotItem) t_con);

    return Create_Connection(*t_con);
}


CODBCContext::~CODBCContext()
{
    if ( !m_Context ) {
        return;
    }

    // close all connections first
    for (int i = m_NotInUse.NofItems();  i--; ) {
        CODBC_Connection* t_con = static_cast<CODBC_Connection*>(m_NotInUse.Get(i));
        delete t_con;
    }

    for (int i = m_InUse.NofItems();  i--; ) {
        CODBC_Connection* t_con = static_cast<CODBC_Connection*> (m_InUse.Get(i));
        delete t_con;
    }

    SQLFreeHandle(SQL_HANDLE_ENV, m_Context);
}


void CODBCContext::ODBC_SetPacketSize(SQLUINTEGER packet_size)
{
    m_PacketSize = packet_size;
}


SQLHENV CODBCContext::ODBC_GetContext() const
{
    return m_Context;
}


SQLHDBC CODBCContext::x_ConnectToServer(const string&   srv_name,
                                               const string&   user_name,
                                               const string&   passwd,
                                               TConnectionMode mode)
{
    SQLHDBC con;
    SQLRETURN r;

    r= SQLAllocHandle(SQL_HANDLE_DBC, m_Context, &con);
    if((r != SQL_SUCCESS) && (r != SQL_SUCCESS_WITH_INFO))
        return 0;

    if(m_Timeout) {
        SQLSetConnectAttr(con, SQL_ATTR_CONNECTION_TIMEOUT, (void*)m_Timeout, 0);
    }

    if(m_LoginTimeout) {
        SQLSetConnectAttr(con, SQL_ATTR_LOGIN_TIMEOUT, (void*)m_LoginTimeout, 0);
    }

    if(m_PacketSize) {
        SQLSetConnectAttr(con, SQL_ATTR_PACKET_SIZE, (void*)m_PacketSize, 0);
    }

#ifdef SQL_COPT_SS_BCP
    if((mode & fBcpIn) != 0) {
        SQLSetConnectAttr(con, SQL_COPT_SS_BCP, (void*) SQL_BCP_ON, SQL_IS_INTEGER);
    }
#endif


    if(!m_UseDSN) {
#ifdef NCBI_OS_MSWIN
      string connect_str("DRIVER={SQL Server};SERVER=");
#else
      string connect_str("DSN=");
#endif
        connect_str+= srv_name;
        connect_str+= ";UID=";
        connect_str+= user_name;
        connect_str+= ";PWD=";
        connect_str+= passwd;
        r= SQLDriverConnect(con, 0, (SQLCHAR*) connect_str.c_str(), SQL_NTS,
                            0, 0, 0, SQL_DRIVER_NOPROMPT);
    }
    else {
        r= SQLConnect(con, (SQLCHAR*) srv_name.c_str(), SQL_NTS,
                   (SQLCHAR*) user_name.c_str(), SQL_NTS,
                   (SQLCHAR*) passwd.c_str(), SQL_NTS);
    }
    switch(r) {
    case SQL_SUCCESS_WITH_INFO:
        xReportConError(con);
    case SQL_SUCCESS: return con;

    case SQL_ERROR:
        xReportConError(con);
        SQLFreeHandle(SQL_HANDLE_DBC, con);
        break;
    default:
        m_Reporter.ReportErrors();
        break;
    }

    return 0;
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
    cntx->ODBC_SetPacketSize(s);
      }
    }
    return cntx;
}



///////////////////////////////////////////////////////////////////////////////
const string kDBAPI_ODBC_DriverName("odbc");

class CDbapiOdbcCF2 : public CSimpleClassFactoryImpl<I_DriverContext, CODBCContext>
{
public:
    typedef CSimpleClassFactoryImpl<I_DriverContext, CODBCContext> TParent;

public:
    CDbapiOdbcCF2(void);
    ~CDbapiOdbcCF2(void);

public:
    virtual TInterface*
    CreateInstance(
        const string& driver  = kEmptyStr,
        CVersionInfo version =
        NCBI_INTERFACE_VERSION(I_DriverContext),
        const TPluginManagerParamTree* params = 0) const;

};

CDbapiOdbcCF2::CDbapiOdbcCF2(void)
    : TParent( kDBAPI_ODBC_DriverName, 0 )
{
    return ;
}

CDbapiOdbcCF2::~CDbapiOdbcCF2(void)
{
    return ;
}

CDbapiOdbcCF2::TInterface*
CDbapiOdbcCF2::CreateInstance(
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
        SQLINTEGER odbc_version = SQL_OV_ODBC3;
        bool use_dsn = false;

        // Optional parameters ...
        int page_size = 0;

        if ( params != NULL ) {
            typedef TPluginManagerParamTree::TNodeList_CI TCIter;
            typedef TPluginManagerParamTree::TTreeValueType TValue;

            // Get parameters ...
            TCIter cit = params->SubNodeBegin();
            TCIter cend = params->SubNodeEnd();

            for (; cit != cend; ++cit) {
                const TValue& v = (*cit)->GetValue();

                if ( v.id == "use_dsn" ) {
                    use_dsn = (v.value != "false");
                } else if ( v.id == "version" ) {
                    int value = NStr::StringToInt( v.value );

                    switch ( value ) {
                    case 3 :
                        odbc_version = SQL_OV_ODBC3;
                        break;
                    case 2 :
                        odbc_version = SQL_OV_ODBC3;
                        break;
                    }
                } else if ( v.id == "packet" ) {
                    page_size = NStr::StringToInt( v.value );
                }
            }
        }

        // Create a driver ...
        drv = new CODBCContext( odbc_version, use_dsn );

        // Set parameters ...
        if ( page_size ) {
            drv->ODBC_SetPacketSize( page_size );
        }
    }
    return drv;
}

///////////////////////////////////////////////////////////////////////////////
void
NCBI_EntryPoint_xdbapi_odbc(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CDbapiOdbcCF2>::NCBI_EntryPointImpl( info_list, method );
}

NCBI_DBAPIDRIVER_ODBC_EXPORT
void
DBAPI_RegisterDriver_ODBC(void)
{
    RegisterEntryPoint<I_DriverContext>( NCBI_EntryPoint_xdbapi_odbc );
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
