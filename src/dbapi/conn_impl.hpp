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
*/

#include <dbapi/dbapi.hpp>
#include "active_obj.hpp"

BEGIN_NCBI_SCOPE

class CDataSource;

class CConnection : public CActiveObject,
                    public IConnection
{
public:
    CConnection(CDataSource* ds, EOwnership ownership);

public:
    virtual ~CConnection();

    virtual void SetMode(EConnMode mode);
    virtual void ResetMode(EConnMode mode);
    virtual unsigned int GetModeMask();

    virtual void ForceSingle(bool enable);

    virtual IDataSource* GetDataSource();

    virtual void Connect(const string& user,
                         const string& password,
                         const string& server,
                         const string& database = kEmptyStr);

    virtual IConnection* CloneConnection(EOwnership ownership);

    // New part begin

    virtual IStatement* GetStatement();
    virtual ICallableStatement* GetCallableStatement(const string& proc,
                                                     int nofArgs = 0);
    virtual ICursor* GetCursor(const string& name,
                               const string& sql,
                               int nofArgs,
                               int batchSize);

    virtual IBulkInsert* GetBulkInsert(const string& table_name,
                                       unsigned int nof_cols);

    // New part end

    virtual IStatement* CreateStatement();
    virtual ICallableStatement* PrepareCall(const string& proc,
                                            int nofArgs = 0);
    virtual ICursor* CreateCursor(const string& name,
                                  const string& sql,
                                  int nofArgs,
                                  int batchSize);

    virtual IBulkInsert* CreateBulkInsert(const string& table_name,
                                          unsigned int nof_cols);

    virtual void Close();
    virtual void Abort();

    virtual CDB_Connection* GetCDB_Connection();

    virtual void SetDatabase(const string& name);

    virtual string GetDatabase();

    virtual bool IsAlive();

    CConnection* Clone();

    void SetDbName(const string& name, CDB_Connection* conn = 0);

    CDB_Connection* CloneCDB_Conn();

    bool IsAux() {
        return m_connCounter < 0;
    }

    // Interface IEventListener implementation
    virtual void Action(const CDbapiEvent& e);

    // If enabled, redirects all error messages
    // to CDB_MultiEx object (see below)
    virtual void MsgToEx(bool v);

    // Returns all error messages as a CDB_MultiEx object
    virtual CDB_MultiEx* GetErrorAsEx();

    // Returns all error messages as a single string
    virtual string GetErrorInfo();

protected:
    CConnection(class CDB_Connection *conn,
                CDataSource* ds);
    // Clone connection, if the original cmd structure is taken
    CConnection* GetAuxConn();
    // void DeleteConn(CConnection* conn);

    class CToMultiExHandler* GetHandler();

    void FreeResources();

private:
    string m_database;
    class CDataSource* m_ds;
    CDB_Connection *m_connection;
    int m_connCounter;
    bool m_connUsed;
    unsigned int m_modeMask;
    bool m_forceSingle;
    class CToMultiExHandler *m_multiExH;
    bool m_msgToEx;

    EOwnership m_ownership;

};

//====================================================================
END_NCBI_SCOPE
/*
*
* $Log$
* Revision 1.20  2005/04/04 13:03:56  ssikorsk
* Revamp of DBAPI exception class CDB_Exception
*
* Revision 1.19  2005/02/24 19:51:03  kholodov
* Added: CConnection::Abort() method
*
* Revision 1.18  2004/11/16 19:59:46  kholodov
* Added: GetBlobOStream() with explicit connection
*
* Revision 1.17  2004/11/08 14:52:50  kholodov
* Added: additional TRACE messages
*
* Revision 1.16  2004/07/28 18:36:13  kholodov
* Added: setting ownership for connection objects
*
* Revision 1.15  2004/04/08 15:56:58  kholodov
* Multiple bug fixes and optimizations
*
* Revision 1.14  2004/03/08 22:15:19  kholodov
* Added: 3 new Get...() methods internally
*
* Revision 1.13  2003/11/18 16:59:45  kholodov
* Added: CloneConnection() method
*
* Revision 1.12  2003/05/16 20:17:28  kholodov
* Modified: default 0 arguments in PrepareCall()
*
* Revision 1.11  2003/03/07 21:21:15  kholodov
* Added: IsAlive() method
*
* Revision 1.10  2002/11/27 17:19:49  kholodov
* Added: Error output redirection to CToMultiExHandler object.
*
* Revision 1.9  2002/09/30 20:45:34  kholodov
* Added: ForceSingle() method to enforce single connection used
*
* Revision 1.8  2002/09/23 18:25:10  kholodov
* Added: GetDataSource() method.
*
* Revision 1.7  2002/09/18 18:49:26  kholodov
* Modified: class declaration and Action method to reflect
* direct inheritance of CActiveObject from IEventListener
*
* Revision 1.6  2002/09/16 19:34:41  kholodov
* Added: bulk insert support
*
* Revision 1.5  2002/05/16 22:11:11  kholodov
* Improved: using minimum connections possible
*
* Revision 1.4  2002/02/08 22:43:11  kholodov
* Set/GetDataBase() renamed to Set/GetDatabase() respectively
*
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
*/
#endif // _CONN_IMPL_HPP_
