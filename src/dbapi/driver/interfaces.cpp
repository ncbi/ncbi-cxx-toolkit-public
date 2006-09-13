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
 *  ===========================================================================
 *
 * Author:  Vladimir Soussov
 *
 * File Description:  Data Server public interfaces
 *
 */

#include <ncbi_pch.hpp>

#include <dbapi/driver/interfaces.hpp>


BEGIN_NCBI_SCOPE


////////////////////////////////////////////////////////////////////////////
// C_ITDescriptorGuard
C_ITDescriptorGuard::C_ITDescriptorGuard(I_ITDescriptor* d)
{
    m_D = d;
}

C_ITDescriptorGuard::~C_ITDescriptorGuard(void)
{
    if ( m_D )
        delete m_D;
}

////////////////////////////////////////////////////////////////////////////
//  I_ITDescriptor::
//

I_ITDescriptor::~I_ITDescriptor(void)
{
    return;
}


////////////////////////////////////////////////////////////////////////////
//  I_BaseCmd::
//  I_LangCmd::
//  I_RPCCmd::
//  I_BCPInCmd::
//  I_CursorCmd::
//  I_SendDataCmd::
//

I_BaseCmd::I_BaseCmd(void)
{
}

I_BaseCmd::~I_BaseCmd(void)
{
    return;
}

I_LangCmd::I_LangCmd(void)
{
}

I_LangCmd::~I_LangCmd(void)
{
    return;
}

I_RPCCmd::I_RPCCmd(void)
{
}

I_RPCCmd::~I_RPCCmd(void)
{
    return;
}

I_BCPInCmd::I_BCPInCmd(void)
{
    return;
}

I_BCPInCmd::~I_BCPInCmd(void)
{
    return;
}

I_CursorCmd::I_CursorCmd(void)
{
}

I_CursorCmd::~I_CursorCmd(void)
{
    return;
}

I_SendDataCmd::I_SendDataCmd(void)
{
}

I_SendDataCmd::~I_SendDataCmd(void)
{
    return;
}


////////////////////////////////////////////////////////////////////////////
//  I_Result::
//

I_Result::I_Result(void)
{
}

I_Result::~I_Result(void)
{
    return;
}


////////////////////////////////////////////////////////////////////////////
//  I_DriverContext::SConnAttr::
//

I_DriverContext::SConnAttr::SConnAttr(void)
: mode(fDoNotConnect)
{
}

////////////////////////////////////////////////////////////////////////////
//  I_DriverContext::
//

I_DriverContext::I_DriverContext(void) :
    m_LoginTimeout(0),
    m_Timeout(0)
{
    return;
}

I_DriverContext::~I_DriverContext(void)
{
    return;
}

void
I_DriverContext::SetApplicationName(const string& app_name)
{
    m_AppName = app_name;
}

const string&
I_DriverContext::GetApplicationName(void) const
{
    return m_AppName;
}

void
I_DriverContext::SetHostName(const string& host_name)
{
    m_HostName = host_name;
}

const string&
I_DriverContext::GetHostName(void) const
{
    return m_HostName;
}

bool I_DriverContext::SetLoginTimeout (unsigned int nof_secs)
{
    m_LoginTimeout = nof_secs;

    return true;
}

bool I_DriverContext::SetTimeout      (unsigned int nof_secs)
{
    m_Timeout = nof_secs;

    return true;
}

////////////////////////////////////////////////////////////////////////////
I_Connection::I_Connection(void)
{
}

I_Connection::~I_Connection(void)
{
    return;
}


////////////////////////////////////////////////////////////////////////////
//  I_DriverMgr::
//

I_DriverMgr::I_DriverMgr(void)
{
}

I_DriverMgr::~I_DriverMgr(void)
{
    return;
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.31  2006/09/13 19:47:09  ssikorsk
 * Implemented SetLoginTimeout and SetTimeout for I_DriverContext.
 *
 * Revision 1.30  2006/07/12 16:29:30  ssikorsk
 * Separated interface and implementation of CDB classes.
 *
 * Revision 1.29  2006/06/22 18:42:11  ssikorsk
 * In I_DriverContext::I_DriverContext register default message handler
 * with eTakeOwnership.
 *
 * Revision 1.28  2006/06/19 19:10:11  ssikorsk
 * Fixed I_DriverContext::PopDefConnMsgHandler to pop msg handlers
 * from created connections.
 *
 * Revision 1.27  2006/06/06 19:12:27  ssikorsk
 * Fixed I_Connection::DeleteAllCommands.
 *
 * Revision 1.26  2006/06/05 21:02:38  ssikorsk
 * Implemented I_Connection::Release.
 *
 * Revision 1.25  2006/06/05 14:42:29  ssikorsk
 * Added methods PushMsgHandler, PopMsgHandler and DropCmd to I_Connection.
 *
 * Revision 1.24  2006/05/18 16:53:46  ssikorsk
 *        Set default login timeout and operation timeout to 0.
 *
 * Revision 1.23  2006/05/15 19:32:43  ssikorsk
 * Added EOwnership argument to methods PushCtxMsgHandler and PushDefConnMsgHandler
 * of class I_DriverContext.
 *
 * Revision 1.22  2006/04/20 22:17:02  ssikorsk
 * Added explicit type cast for x64 sake.
 *
 * Revision 1.21  2006/04/13 15:10:25  ssikorsk
 * Fixed erasing of an element from a std::list.in x_Recycle
 *
 * Revision 1.20  2006/04/11 18:39:33  ssikorsk
 * Fixed erasing of an element from a std::list.
 *
 * Revision 1.19  2006/04/05 15:51:32  ssikorsk
 * Widen protection scope of I_DriverContext::NofConnections
 *
 * Revision 1.18  2006/04/05 14:26:28  ssikorsk
 * Implemented I_DriverContext::DeleteAllConn
 *
 * Revision 1.17  2006/03/30 16:27:04  ssikorsk
 * Allow to use empty server name, user name and password
 * with a connection pool.
 *
 * Revision 1.16  2006/03/28 22:31:00  ssikorsk
 * Deleted needless dynamic_cast.
 *
 * Revision 1.15  2006/03/09 19:01:37  ssikorsk
 * Replaced types of I_DriverContext's m_NotInUse and
 * m_InUse from CPointerPot to list<I_Connection*>.
 * Added method I_DriverContext:: CloseAllConn.
 *
 * Revision 1.14  2006/02/01 13:55:07  ssikorsk
 * Report server and user names in case of a failed connection attempt.
 *
 * Revision 1.13  2006/01/23 13:35:36  ssikorsk
 * Implemented I_DriverContext::ConnectValidated;
 *
 * Revision 1.12  2006/01/09 19:19:21  ssikorsk
 * Validate server name, user name and password before connection for all drivers.
 *
 * Revision 1.11  2006/01/03 18:59:52  ssikorsk
 * Implement I_DriverContext::MakePooledConnection and
 * I_DriverContext::Connect.
 *
 * Revision 1.10  2005/10/20 13:03:20  ssikorsk
 * Implemented:
 * I_DriverContext::SetApplicationName
 * I_DriverContext::GetApplicationName
 * I_DriverContext::SetHostName
 * I_DriverContext::GetHostName
 *
 * Revision 1.9  2004/05/17 21:11:38  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.8  2003/07/17 20:44:57  soussov
 * connections pool improvements
 *
 * Revision 1.7  2002/01/20 07:21:02  vakatov
 * I_DriverMgr:: -- added virtual destructor
 *
 * Revision 1.6  2001/11/06 17:59:53  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.5  2001/10/01 20:09:29  vakatov
 * Introduced a generic default user error handler and the means to
 * alternate it. Added an auxiliary error handler class
 * "CDB_UserHandler_Stream".
 * Moved "{Push/Pop}{Cntx/Conn}MsgHandler()" to the generic code
 * (in I_DriverContext).
 *
 * Revision 1.4  2001/09/27 20:08:32  vakatov
 * Added "DB_" (or "I_") prefix where it was missing
 *
 * Revision 1.3  2001/09/26 23:23:29  vakatov
 * Moved the err.message handlers' stack functionality (generic storage
 * and methods) to the "abstract interface" level.
 *
 * Revision 1.2  2001/09/24 19:48:49  vakatov
 * + implementation for CDB_BaseEnt::Create_Result
 *
 * Revision 1.1  2001/09/21 23:39:59  vakatov
 * -----  Initial (draft) revision.  -----
 * This is a major revamp (by Denis Vakatov, with help from Vladimir Soussov)
 * of the DBAPI "driver" libs originally written by Vladimir Soussov.
 * The revamp involved massive code shuffling and grooming, numerous local
 * API redesigns, adding comments and incorporating DBAPI to the C++ Toolkit.
 *
 * ===========================================================================
 */
