#ifndef _CONN_IMPL_HPP_
#define _CONN_IMPL_HPP_

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
* File Name:  $Id$
*
* Author:  Michael Kholodov
*   
* File Description:  Connection implementation
*
*
* $Log$
* Revision 1.3  2002/02/08 21:29:55  kholodov
* SetDataBase() restored, connection cloning algorithm changed
*
* Revision 1.2  2002/02/08 17:47:34  kholodov
* Removed SetDataBase() method
*
* Revision 1.1  2002/01/30 14:51:22  kholodov
* User DBAPI implementation, first commit
*
*
*
*/

#include <dbapi/dbapi.hpp>
#include "active_obj.hpp"

BEGIN_NCBI_SCOPE

class CDataSource;

class CConnection : public CActiveObject, 
                    public IEventListener,
                    public IConnection
{
public:
    CConnection(CDataSource* ds);
    CConnection(class CDB_Connection *conn, 
                CDataSource* ds);

public:
    virtual ~CConnection();

    virtual void Connect(const string& user,
                         const string& password,
                         const string& server,
                         const string& database = kEmptyStr);

    virtual IStatement* CreateStatement();
    virtual ICallableStatement* PrepareCall(const string& proc,
                                            int nofArgs);
    virtual ICursor* CreateCursor(const string& name,
                                  const string& sql,
                                  int nofArgs,
                                  int batchSize);

    virtual void Close();

    CConnection* Clone();

    CDB_Connection* GetConnection() {
        return m_connection;
    }

    CDB_Connection* GetConnAux();

    virtual void SetDataBase(const string& name);
    virtual string GetDataBase();

    void SetDbName(const string& name, CDB_Connection* conn = 0);

    // Interface IEventListener implementation
    virtual void Action(const CDbapiEvent& e);

protected:
    // Clone connection, if the original cmd structure is taken
    CConnection* GetFreeConn();

private:
    string m_database;
    class CDataSource* m_ds;
    CDB_Connection *m_connection;
    bool m_cmdUsed;
};

//====================================================================
END_NCBI_SCOPE

#endif // _CONN_IMPL_HPP_
