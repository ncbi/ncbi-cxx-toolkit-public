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

#include <dbapi/driver/ftds/interfaces.hpp>
#include <dbapi/driver/util/numeric_convert.hpp>


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
             msgtxt? msgtxt : "", srvname? srvname : "", procname? procname : "", 
	     line);
        return 0;
    }
}


CTDSContext* CTDSContext::m_pTDSContext = 0;


CTDSContext::CTDSContext(DBINT version) :
    m_AppName("TDSDriver"), m_HostName(""), m_PacketSize(0)
{
    if (m_pTDSContext != 0) {
        throw CDB_ClientEx(eDB_Error, 200000, "CTDSContext::CTDSContext",
                           "You cannot use more than one dblib contexts "
                           "concurrently");
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

    if (reusable  &&  m_NotInUse.NofItems() > 0) { // try to reuse connection
        if (!pool_name.empty()) { // try to use pool name
            int n = m_NotInUse.NofItems();
            while (n--) {
                t_con = static_cast<CTDS_Connection*> (m_NotInUse.Get(n));
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
            t_con = static_cast<CTDS_Connection*> (m_NotInUse.Get(n));
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


unsigned int CTDSContext::NofConnections(const string& srv_name) const
{
    if (srv_name.empty()) {
        return m_InUse.NofItems() + m_NotInUse.NofItems();
    }

    int n = 0;
    CTDS_Connection* t_con;

    for (int i = m_NotInUse.NofItems(); i--; ) {
        t_con = static_cast<CTDS_Connection*> (m_NotInUse.Get(i));
        if (srv_name == t_con->ServerName())
            ++n;
    }
    for (int i = m_InUse.NofItems(); i--; ) {
        t_con = static_cast<CTDS_Connection*> (m_InUse.Get(i));
        if (srv_name == t_con->ServerName())
            ++n;
    }
    return n;
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
                                       int           dberr,    int oserr,
                                       const string& dberrstr,
                                       const string& oserrstr)
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
                                        int           msgstate, int   severity,
                                        const string& msgtxt,
                                        const string& srvname,
                                        const string& procname,
                                        int           line)
{
    if ((severity == 0 && msgno == 0)  ||  msgno == 5701  ||  msgno == 5703)
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
		     (int)m_Timeout, (int)m_Timeout);
    return dbopen(m_Login, (char*) srv_name.c_str());
}



///////////////////////////////////////////////////////////////////////
// Driver manager related functions
//

I_DriverContext* FTDS_CreateContext(map<string,string>* attr = 0)
{
    DBINT version= DBVERSION_UNKNOWN;

    if(attr) {
	string vers= (*attr)["version"];
	if(vers.find("42") != string::npos)
	    version= DBVERSION_42;
	else if(vers.find("46") != string::npos)
	    version= DBVERSION_46;
	else if(vers.find("70") != string::npos)
	    version= DBVERSION_70;
	else if(vers.find("100") != string::npos)
	    version= DBVERSION_100;

    }
    return new CTDSContext(version);
}

void DBAPI_RegisterDriver_FTDS(I_DriverMgr& mgr)
{
    mgr.RegisterDriver("ftds", FTDS_CreateContext);
}

extern "C" {
    void* DBAPI_E_ftds()
    {
	return (void*)DBAPI_RegisterDriver_FTDS;
    }
} 


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
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
