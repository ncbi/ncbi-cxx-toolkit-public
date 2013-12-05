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
 * Author:  Anton Butanayev
 *
 * File Description:  Driver for MySQL server
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/mysql/interfaces.hpp>
#include <dbapi/error_codes.hpp>
#include <memory>


#define NCBI_USE_ERRCODE_X   Dbapi_Mysql_Conn


BEGIN_NCBI_SCOPE


CMySQL_Connection::CMySQL_Connection(CMySQLContext& cntx,
                                     const CDBConnParams& params) :
    impl::CConnection(cntx, params),
    m_ActiveCmd(NULL),
    m_IsOpen(false)
{
    SetServerType(CDBConnParams::eMySQL);

    if ( !mysql_init(&m_MySQL) ) {
        DATABASE_DRIVER_WARNING( "Failed: mysql_init", 800001 );
    }

    if ( !mysql_real_connect(&m_MySQL,
                             params.GetServerName().c_str(),
                             params.GetUserName().c_str(), 
                             params.GetPassword().c_str(),
                             NULL, 
                             0, 
                             NULL, 
                             0)) 
    {
        DATABASE_DRIVER_WARNING( "Failed: mysql_real_connect", 800002 );
    }

    m_IsOpen = true;
}


CMySQL_Connection::~CMySQL_Connection()
{
    try {
        // Close is a virtual function but it is safe to call it from a destructor
        // because it is defined in this class.
        Close();
    }
    NCBI_CATCH_ALL_X( 1, NCBI_CURRENT_FUNCTION )
    if (m_ActiveCmd) {
        m_ActiveCmd->m_IsActive = false;
    }
}


bool CMySQL_Connection::IsAlive()
{
    return false;
}


void 
CMySQL_Connection::SetTimeout(size_t nof_secs)
{
    // DATABASE_DRIVER_ERROR( "SetTimeout is not supported.", 100011 );
    _TRACE("SetTimeout is not supported.");
}


void 
CMySQL_Connection::SetCancelTimeout(size_t nof_secs)
{
    // DATABASE_DRIVER_ERROR( "SetCancelTimeout is not supported.", 100011 );
    _TRACE("SetCancelTimeout is not supported.");
}


CDB_SendDataCmd* CMySQL_Connection::SendDataCmd(I_ITDescriptor& /*descr_in*/,
                                                size_t          /*data_size*/,
                                                bool            /*log_it*/,
                                                bool            /*dump_results*/)
{
    return 0;
}


bool CMySQL_Connection::SendData(I_ITDescriptor& /*desc*/,
                                 CDB_Stream&      /*img*/,
                                 bool            /*log_it*/)
{
    return true;
}

bool CMySQL_Connection::Refresh()
{
    // close all commands first
    DeleteAllCommands();

    return true;
}


I_DriverContext::TConnectionMode CMySQL_Connection::ConnectMode() const
{
    return 0;
}


CDB_LangCmd* CMySQL_Connection::LangCmd(const string& lang_query)
{
    return Create_LangCmd(*new CMySQL_LangCmd(*this, lang_query));

}


CDB_RPCCmd *CMySQL_Connection::RPC(const string& /*rpc_name*/)
{
    return NULL;
}


CDB_BCPInCmd* CMySQL_Connection::BCPIn(const string& /*table_name*/)
{
    return NULL;
}


CDB_CursorCmd *CMySQL_Connection::Cursor(const string& /*cursor_name*/,
                                         const string& /*query*/,
                                         unsigned int  /*batch_size*/)
{
    return NULL;
}

bool CMySQL_Connection::Abort()
{
    return false;
}

bool CMySQL_Connection::Close(void)
{
    if (m_IsOpen) {
        Refresh();

        mysql_close(&m_MySQL);

        MarkClosed();

        m_IsOpen = false;

        return true;
    }

    return false;
}

END_NCBI_SCOPE


