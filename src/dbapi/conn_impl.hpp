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

    virtual void Connect(const CDBConnParams& params);

    virtual void ConnectValidated(IConnValidator& validator,
                                  const string& user,
                                  const string& password,
                                  const string& server,
                                  const string& database = kEmptyStr);

    virtual IConnection* CloneConnection(EOwnership ownership);

    // New part begin

    virtual IStatement* GetStatement();
    virtual ICallableStatement* GetCallableStatement(const string& proc);
    virtual ICursor* GetCursor(const string& name,
                               const string& sql,
                               int batchSize);

    virtual IBulkInsert* GetBulkInsert(const string& table_name);

    // New part end

    virtual IStatement* CreateStatement();
    virtual ICallableStatement* PrepareCall(const string& proc);
    virtual ICursor* CreateCursor(const string& name,
                                  const string& sql,
                                  int batchSize);

    virtual IBulkInsert* CreateBulkInsert(const string& table_name);

    virtual void Close();
    virtual void Abort();
    virtual void SetTimeout(size_t nof_secs);
    virtual void SetCancelTimeout(size_t nof_secs);

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

#endif // _CONN_IMPL_HPP_
