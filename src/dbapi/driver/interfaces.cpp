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
* File Description:  Data Server public interfaces
*
*
*/

#include <dbapi/driver/interfaces.hpp>
#include <dbapi/driver/public.hpp>


BEGIN_NCBI_SCOPE


////////////////////////////////////////////////////////////////////////////
//  ITDescriptor::
//

ITDescriptor::~ITDescriptor()
{
    return;
}


////////////////////////////////////////////////////////////////////////////
//  CDB_BaseEnt::
//

void CDB_BaseEnt::Release()
{
    m_BR = 0;
}

CDB_BaseEnt::~CDB_BaseEnt()
{
    if (m_BR)
        *m_BR = 0;
}


////////////////////////////////////////////////////////////////////////////
//  I_BaseCmd::
//  I_LangCmd::
//  I_RPCCmd::
//  I_BCPInCmd::
//  I_CursorCmd::
//  I_SendDataCmd::
//

I_BaseCmd::~I_BaseCmd()
{
    return;
}

I_LangCmd::~I_LangCmd()
{
    return;
}

I_RPCCmd::~I_RPCCmd()
{
    return;
}

I_BCPInCmd::~I_BCPInCmd()
{
    return;
}

I_CursorCmd::~I_CursorCmd()
{
    return;
}

I_SendDataCmd::~I_SendDataCmd()
{
    return;
}


////////////////////////////////////////////////////////////////////////////
//  I_Result::
//

I_Result::~I_Result()
{
    return;
}


////////////////////////////////////////////////////////////////////////////
//  I_DriverContext::
//

void I_DriverContext::x_Recycle(I_Connection* conn, bool conn_reusable)
{
    m_InUse.Remove((TPotItem) conn);

    if ( conn_reusable ) {
        m_NotInUse.Add((TPotItem) conn);
    } else {
        delete conn;
    }
}

CDB_Connection* I_DriverContext::Create_Connection(I_Connection& connection)
{
    return new CDB_Connection(&connection);
}

I_DriverContext::~I_DriverContext()
{
    return;
}


////////////////////////////////////////////////////////////////////////////
//  I_Connection::
//


CDB_LangCmd* I_Connection::Create_LangCmd(I_LangCmd&lang_cmd)
{
    return new CDB_LangCmd(&lang_cmd);
}

CDB_RPCCmd* I_Connection::Create_RPCCmd(I_RPCCmd&rpc_cmd)
{
    return new CDB_RPCCmd(&rpc_cmd);
}

CDB_BCPInCmd* I_Connection::Create_BCPInCmd(I_BCPInCmd& bcpin_cmd)
{
    return new CDB_BCPInCmd(&bcpin_cmd);
}

CDB_CursorCmd* I_Connection::Create_CursorCmd(I_CursorCmd& cursor_cmd)
{
    return new CDB_CursorCmd(&cursor_cmd);
}

CDB_SendDataCmd* I_Connection::Create_SendDataCmd(I_SendDataCmd& senddata_cmd)
{
    return new CDB_SendDataCmd(&senddata_cmd);
}


I_Connection::~I_Connection()
{
    return;
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2001/09/21 23:39:59  vakatov
 * -----  Initial (draft) revision.  -----
 * This is a major revamp (by Denis Vakatov, with help from Vladimir Soussov)
 * of the DBAPI "driver" libs originally written by Vladimir Soussov.
 * The revamp involved massive code shuffling and grooming, numerous local
 * API redesigns, adding comments and incorporating DBAPI to the C++ Toolkit.
 *
 * ===========================================================================
 */
