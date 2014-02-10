#ifndef _STMT_IMPL_HPP_
#define _STMT_IMPL_HPP_

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
* File Description:  Statement implementation
*
*
*/

#include <dbapi/dbapi.hpp>
#include "active_obj.hpp"
#include <corelib/ncbistre.hpp>
#include "dbexception.hpp"


BEGIN_NCBI_SCOPE

class CStatement : public CActiveObject, 
                   public virtual IStatement
{
public:
    CStatement(class CConnection* conn);

    virtual ~CStatement();
  
    virtual IResultSet* GetResultSet();

    virtual bool HasMoreResults();

    virtual bool HasRows();
    virtual bool Failed();
    virtual int GetRowCount();
  
    virtual void SendSql(const string& sql);
    virtual void Execute(const string& sql);
    virtual void ExecuteUpdate(const string& sql);
    virtual IResultSet* ExecuteQuery(const string& sql);

    virtual void ExecuteLast();

    virtual void PurgeResults();
    virtual void Cancel();
    virtual void Close();

    virtual void SetParam(const CVariant& v,
                          const CDBParamVariant& param);

    virtual void ClearParamList();
    virtual const IResultSetMetaData& GetParamsMetaData(void);

    virtual IConnection* GetParentConn();

    virtual IWriter* GetBlobWriter(I_ITDescriptor &d, size_t blob_size,
                                   TBlobOStreamFlags flags);
    virtual IWriter* GetBlobWriter(I_ITDescriptor &d, size_t blob_size,
                                   EAllowLog log_it)
    {
        return IStatement::GetBlobWriter(d, blob_size, log_it);
    }

    virtual CNcbiOstream& GetBlobOStream(I_ITDescriptor &d, 
                                         size_t blob_size, 
                                         TBlobOStreamFlags flags,
                                         size_t buf_size);
    virtual CNcbiOstream& GetBlobOStream(I_ITDescriptor &d, 
		                                 size_t blob_size, 
										 EAllowLog log_it,
                                         size_t buf_size)
    {
        return IStatement::GetBlobOStream(d, blob_size, log_it, buf_size);
    }

    CConnection* GetConnection() {
        return m_conn;
    }

    CDB_Result* GetCDB_Result();

    CDB_LangCmd* GetLangCmd();
    

    // Interface IEventListener implementation
    virtual void Action(const CDbapiEvent& e);

public:
    virtual void SetAutoClearInParams(bool flag = true) {
        m_AutoClearInParams = flag;
    }
    virtual bool IsAutoClearInParams(void) const {
        return m_AutoClearInParams;
    }

protected:    
    void x_Send(const string& sql);
    void SetBaseCmd(I_BaseCmd *cmd) { m_cmd = cmd; }
    I_BaseCmd* GetBaseCmd() { return m_cmd; }

    void CacheResultSet(CDB_Result *rs);

    void SetFailed(bool f) {
        m_failed = f;
    }

    void FreeResources();

private:

    class CStmtParamsMetaData : public IResultSetMetaData
    {
    public: 
        CStmtParamsMetaData(I_BaseCmd*& cmd);

        virtual ~CStmtParamsMetaData();

        virtual unsigned int GetTotalColumns() const;
        virtual EDB_Type GetType(const CDBParamVariant& param) const;
        virtual int GetMaxSize(const CDBParamVariant& param) const;
        virtual string GetName(const CDBParamVariant& param) const;
	virtual CDBParams::EDirection GetDirection(const CDBParamVariant& param) const;
        
    private:
        I_BaseCmd*& m_Cmd;
    };


private:
    typedef map<string, CVariant*> ParamList;
    typedef vector<CVariant*>      ParamByPosList;

    class CConnection*  m_conn;
    I_BaseCmd*          m_cmd;
    CStmtParamsMetaData m_InParams;
    int                 m_rowCount;
    bool                m_failed;
    ParamList           m_params;
    ParamByPosList      m_posParams;
    class CResultSet*   m_irs;
    class IWriter*      m_wr;
    class CWStream*	m_ostr;
    bool                m_AutoClearInParams;
};

END_NCBI_SCOPE
//====================================================================

#endif // _STMT_IMPL_HPP_
