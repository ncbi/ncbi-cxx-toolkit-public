/* ===========================================================================
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
 * Authors: 
 *
 * File Description:
 *
 * ---------------------------------------------------------------------------
 *
 * ===========================================================================
 */

//#include <dbapi/driver/drivers.hpp>
//#include <internal/gensat/database/gensat_data.hpp>

#include <ncbi_pch.hpp>
#include "db.hpp"

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  ctor

CDb::CDb(const string& server,
                     const string& database,
                     const string& user,
                     const string& password,
                     bool bcp_enabled)
    : m_Server(server),
      m_Database(database), 
      m_User(user),
      m_Passwd(password),
      m_BcpEnabled(bcp_enabled)
{
}

/////////////////////////////////////////////////////////////////////////////
//  dtor

CDb::~CDb()
{
}

/////////////////////////////////////////////////////////////////////////////
//
void CDb::OpenConnection(void)
{
    // If still no connection create the normal way
    if( !m_Connection.get()  ||  !m_Connection->IsAlive() ) {
        CDriverManager &dm = CDriverManager::GetInstance();
        
        DBAPI_RegisterDriver_FTDS(dm);
        IDataSource* ds   = dm.CreateDs("ftds");
        if (!ds){
            LOG_POST(Error << "failed to create data source");
            throw runtime_error("failed to create data source");
        }
 
        try {
            m_Connection.reset(ds->CreateConnection());
            if (m_BcpEnabled) {
                m_Connection->SetMode(IConnection::eBulkInsert);
            }
            m_Connection->Connect(m_User, m_Passwd, m_Server, m_Database);
        }
        catch (CDB_Exception& e) {
            // error do log
            LOG_POST(Error << "failed to connect to GENSAT database: "
                     << e.what());
        }
    }
  
}



/////////////////////////////////////////////////////////////////////////////
//
void CDb::CloseConnection(void)
{
    m_Connection.reset();
}

/////////////////////////////////////////////////////////////////////////////
//
/*
CDB_LangCmd* CDb::RunSqlCmd(const string& SqlCmd )
{
    try {
        if ( !m_Connection.get() ) {
            OpenConnection();
            if ( !m_Connection.get() ) {
                NCBI_THROW(CException, eUnknown,
                           "Can't connect to database");
            }
        }

        auto_ptr<CDB_LangCmd> cmd(m_Connection->LangCmd(SqlCmd, 0));
        cmd->Send();
        if ( cmd->HasMoreResults() ) {
            return cmd.release();
        }
    }
    catch (CDB_Exception& e) {
        // error do log
        LOG_POST(Error << "error sending command: " << SqlCmd
                 << ": " << e.what());
    }
    return NULL;
}
*/

bool CDb::Run(const string& sql)
{
    try {
        if ( !m_Connection.get() ) {
            OpenConnection();
            if ( !m_Connection.get() ) {
                NCBI_THROW(CException, eUnknown,
                           "Can't connect to database");
            }
        }

        m_Stmt.reset(m_Connection->GetStatement());
        m_Stmt->Execute(sql);

        if (m_Stmt.get() && m_Stmt->HasMoreResults()) {
            m_ResultSet.reset(m_Stmt->GetResultSet());
            return true;
        }
    }
    catch (CDB_Exception& e) {
        // error do log
        LOG_POST(Error << "error sending command: " << sql << ": "
                 << e.what());
    }
    return false;
}

IResultSet* CDb::Next()
{
    // check for anymore rows in this set
    if (m_ResultSet.get() && m_ResultSet->Next()) {
        return m_ResultSet.get();
    }

    // Get new set and return new row
    while (m_Stmt.get() && m_Stmt->HasMoreResults()) {
        m_ResultSet.reset(m_Stmt->GetResultSet());
        if (m_ResultSet.get() && m_ResultSet->Next()) {
            return m_ResultSet.get();
        }
    }

    // nothing more
    return NULL;
}

int CDb::Execute(const string& sql)
{
    try {
        if ( !m_Connection.get() ) {
            OpenConnection();
            if ( !m_Connection.get() ) {
                NCBI_THROW(CException, eUnknown,
                           "Can't connect to database");
            }
        }

        IStatement* stmt = m_Connection->GetStatement();
        stmt->ExecuteUpdate(sql);

        return stmt->GetRowCount();
    }
    catch (CDB_Exception& e) {
        LOG_POST(Error << "Error executing Update SQL: " << sql << ": " << e.what());
        return false;
    }

    // shouldn't get here
    NCBI_THROW(CException, eUnknown,
               "Unknown error executing SQL: " + sql);

}

IBulkInsert* CDb::CreateBulkInsert(const string& table_name, 
                                   unsigned int num_of_rows)
{
    try {
        IBulkInsert* bi;
        if ( !m_Connection.get() ) {
            OpenConnection();
            if ( !m_Connection.get() ) {
                NCBI_THROW(CException, eUnknown,
                           "Can't connect to database");
            }
        }

        bi = m_Connection->CreateBulkInsert(table_name, num_of_rows);
        return bi;
    }
    catch (CDB_Exception& e) {
        LOG_POST(Error << "Error executing CreateBulkInsert: " << e.what());
        return NULL;
    }

    // shouldn't get here
    NCBI_THROW(CException, eUnknown,
               "Unknown error executing CreateBulkInsert");
    return NULL;

}

/////////////////////////////////////////////////////////////////////////////
//
/*
bool CDb::SendData(I_ITDescriptor& desc, CDB_Image& img, bool log_it)
{
    return m_Connection->SendData(desc, img, log_it);
}
*/
END_NCBI_SCOPE

/*
 * ========================================================================
 * $Log$
 * Revision 1.1  2006/03/29 19:51:12  friedman
 * Initial version
 *
 * Revision 1.3  2004/02/17 11:48:23  dicuccio
 * Lots of clean-up.  Rearranged files in library; introduced new DB results
 * class.  Deprecated old loader, results classes
 *
 * ========================================================================
 */

