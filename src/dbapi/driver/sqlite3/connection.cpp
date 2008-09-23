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
 * Author:  Sergey Sikorskiy
 *
 * File Description:
 *    Driver for SQLite v3 embedded database server
 *
 */

#include <ncbi_pch.hpp>

#include <dbapi/driver/sqlite3/interfaces.hpp>
#include <dbapi/error_codes.hpp>
#include <memory>

#include "sqlite3_utils.hpp"


#define NCBI_USE_ERRCODE_X   Dbapi_SQLt3_Conn


BEGIN_NCBI_SCOPE

CSL3_Connection::CSL3_Connection(CSL3Context&  cntx,
                                 const CDBConnParams& params) :
    impl::CConnection(cntx, params, true),
    m_SQLite3(NULL),
    m_IsOpen(false)
{
    SetServerType(CDBConnParams::eSqlite);

    Check(sqlite3_open(params.GetServerName().c_str(), &m_SQLite3));

    m_IsOpen = true;
}


CSL3_Connection::~CSL3_Connection()
{
    try {
        // Close is a virtual function but it is safe to call it from a destructor
        // because it is defined in this class.
        Close();
    }
    NCBI_CATCH_ALL_X( 1, NCBI_CURRENT_FUNCTION )
}


bool CSL3_Connection::IsAlive()
{
    return m_IsOpen;
}


void 
CSL3_Connection::SetTimeout(size_t nof_secs)
{
    // DATABASE_DRIVER_ERROR( "SetTimeout is not supported.", 100011 );
    ERR_POST_X_ONCE(2, "SetTimeout is not supported.");
}


CDB_SendDataCmd* CSL3_Connection::SendDataCmd(I_ITDescriptor& /*descr_in*/,
                                                size_t          /*data_size*/,
                                                bool            /*log_it*/)
{
    return NULL;
}


bool CSL3_Connection::SendData(I_ITDescriptor& /*desc*/,
                                 CDB_Stream&   /*img*/,
                                 bool          /*log_it*/)
{
    return false;
}

void CSL3_Connection::SetDatabaseName(const string& /*name*/)
{
}

bool CSL3_Connection::Refresh()
{
    // close all commands first
    DeleteAllCommands();

    return true;
}


I_DriverContext::TConnectionMode CSL3_Connection::ConnectMode() const
{
    return 0;
}


CDB_LangCmd* CSL3_Connection::LangCmd(const string& lang_query)
{
    CSL3_LangCmd* cmd = new CSL3_LangCmd(*this, lang_query);
    return Create_LangCmd(*cmd);
}


CDB_RPCCmd *CSL3_Connection::RPC(const string& rpc_name)
{
    CSL3_LangCmd* cmd = new CSL3_LangCmd(*this, rpc_name);
    return Create_RPCCmd(*cmd);
}


CDB_BCPInCmd* CSL3_Connection::BCPIn(const string& table_name)
{
    CSL3_BCPInCmd* cmd = new CSL3_BCPInCmd(*this, table_name);
    return Create_BCPInCmd(*cmd);
}


CDB_CursorCmd *CSL3_Connection::Cursor(const string& /*cursor_name*/,
                                         const string& /*query*/,
                                         unsigned int  /*batch_size*/)
{
    return NULL;
}

bool CSL3_Connection::Abort()
{
    return false;
}

bool CSL3_Connection::Close(void)
{
    if (m_IsOpen) {
        Refresh();
        Check(sqlite3_close(m_SQLite3));
        MarkClosed();
        m_IsOpen = false;
        return true;
    }

    return false;
}


int CSL3_Connection::Check(int rc)
{
    return CheckSQLite3(m_SQLite3, GetMsgHandlers(), rc);
}

END_NCBI_SCOPE


