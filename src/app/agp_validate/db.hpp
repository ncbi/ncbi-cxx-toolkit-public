#ifndef NCBI_GENSATDB__HPP
#define NCBI_GENSATDB__HPP

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
 * Author:
 *
 * ===========================================================================
 */ 

#include <corelib/ncbiobj.hpp> 
#include <corelib/ncbifile.hpp>

#include <dbapi/dbapi.hpp>
#include <dbapi/driver/drivers.hpp>
#include <list>

BEGIN_NCBI_SCOPE


class CDb: public CObject
{  
public:

    CDb(const string& server,
        const string& database, 
        const string& user,
        const string& password,
        bool bcp_enabled = false);

    // dtor
    ~CDb();

    bool Run(const string& sql);
    IResultSet* Next();
    int Execute(const string& sql);

    void OpenConnection(void);
    void CloseConnection(void);

    IBulkInsert* CreateBulkInsert(const string& table_name, 
                                  unsigned int num_of_cols);

private:
    auto_ptr<IConnection> m_Connection;  
    auto_ptr<IStatement> m_Stmt;
    auto_ptr<IResultSet> m_ResultSet;

    string m_Server;
    string m_Database;
    string m_User;
    string m_Passwd;  
    bool m_BcpEnabled;
};


END_NCBI_SCOPE

/*
 * =========================================================================
 * $Log$
 * Revision 1.2  2006/08/01 18:34:58  meric
 * Renamed variable num_of_rows to num_of_cols
 *
 * Revision 1.1  2006/03/29 19:51:12  friedman
 * Initial version
 * 
 * =========================================================================
 */

#endif /* NCBI_GENSATDB__HPP */
