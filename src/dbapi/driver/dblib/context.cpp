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

#include <dbapi/driver/dblib/interfaces.hpp>
#include <dbapi/driver/util/numeric_convert.hpp>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CDBLibContext::
//


extern "C" {

#ifdef NCBI_OS_MSWIN
    static int s_DBLIB_err_callback(
        DBPROCESS* dblink,   int   severity,
        int        dberr,    int   oserr,
        const char*  dberrstr, const char* oserrstr)
    {
        return CDBLibContext::DBLIB_dberr_handler
            (dblink, severity, dberr, oserr, dberrstr? dberrstr : "",
	     oserrstr? oserrstr : "");
    }

    static int s_DBLIB_msg_callback(
        DBPROCESS* dblink,   DBINT msgno,
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
    static int s_DBLIB_err_callback(DBPROCESS* dblink,   int   severity,
                                    int        dberr,    int   oserr,
                                    char*      dberrstr, char* oserrstr)
    {
        return CDBLibContext::DBLIB_dberr_handler
            (dblink, severity, dberr, oserr, dberrstr? dberrstr : "",
	     oserrstr? oserrstr : "");
    }

    static int s_DBLIB_msg_callback(DBPROCESS* dblink,   DBINT msgno,
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


CDBLibContext::CDBLibContext(DBINT /*version*/) :
    m_AppName("DBLibDriver"), m_HostName(""), m_PacketSize(0)
{
    if (m_pDBLibContext != 0) {
        throw CDB_ClientEx(eDB_Error, 200000, "CDBLibContext::CDBLibContext",
                           "You cannot use more than one dblib contexts "
                           "concurrently");
    }

#ifdef NCBI_OS_MSWIN
    if (dbinit() == NULL)
#else
    if (dbinit() != SUCCEED)
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
#ifdef NCBI_OS_MSWIN
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

    if (reusable  &&  m_NotInUse.NofItems() > 0) { // try to reuse connection
        if (!pool_name.empty()) { // try to use pool name
            int n = m_NotInUse.NofItems();
            while (n--) {
                t_con = static_cast<CDBL_Connection*> (m_NotInUse.Get(n));
                if (pool_name == t_con->PoolName() == 0) {
                    m_NotInUse.Remove(n);
                    m_InUse.Add((TPotItem) t_con);
                    t_con->Refresh();
                    return Create_Connection(*t_con);
                }
            }
        }
        if (srv_name.empty())
            return 0;
        int n = m_NotInUse.NofItems();
        // try to use server name
        while (n--) {
            t_con = static_cast<CDBL_Connection*> (m_NotInUse.Get(n));
            if (srv_name == t_con->ServerName()) {
                m_NotInUse.Remove(n);
                m_InUse.Add((TPotItem) t_con);
                t_con->Refresh();
                return Create_Connection(*t_con);
            }
        }
    }

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


unsigned int CDBLibContext::NofConnections(const string& srv_name) const
{
    if (srv_name.empty()) {
        return m_InUse.NofItems() + m_NotInUse.NofItems();
    }

    int n = 0;
    CDBL_Connection* t_con;

    for (int i = m_NotInUse.NofItems(); i--; ) {
        t_con = static_cast<CDBL_Connection*> (m_NotInUse.Get(i));
        if (srv_name == t_con->ServerName())
            ++n;
    }
    for (int i = m_InUse.NofItems(); i--; ) {
        t_con = static_cast<CDBL_Connection*> (m_InUse.Get(i));
        if (srv_name == t_con->ServerName())
            ++n;
    }
    return n;
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

#ifdef NCBI_OS_MSWIN
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
    if ((severity == 0 && msgno == 0)  ||  msgno == 5701  ||  msgno == 5703)
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

#ifndef NCBI_OS_MSWIN
    if (mode & fPasswordEncrypted)
        DBSETLENCRYPT(m_Login, TRUE);
#endif

    return dbopen(m_Login, (char*) srv_name.c_str());
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
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
