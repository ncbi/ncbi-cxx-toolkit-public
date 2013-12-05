#ifndef DBAPI_DRIVER_MYSQL___INTERFACES__HPP
#define DBAPI_DRIVER_MYSQL___INTERFACES__HPP

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
 * File Description:
 *    Driver for MySQL server
 *
 */

#include <dbapi/driver/public.hpp> // Kept for compatibility reasons ...
#include <dbapi/driver/impl/dbapi_impl_context.hpp>
#include <dbapi/driver/impl/dbapi_impl_connection.hpp>
#include <dbapi/driver/impl/dbapi_impl_cmd.hpp>
#include <dbapi/driver/impl/dbapi_impl_result.hpp>
#include <dbapi/driver/util/parameters.hpp>

#if defined(NCBI_OS_MSWIN)
#  include <windows.h>
#ifdef WIN32_LEAN_AND_MEAN
#  include <winsock2.h>
#endif
#endif

#include <mysql.h>

BEGIN_NCBI_SCOPE


class CMySQLContext;
class CMySQL_Connection;
class CMySQL_LangCmd;


/////////////////////////////////////////////////////////////////////////////
//
//  CMySQLContext::
//

class NCBI_DBAPIDRIVER_MYSQL_EXPORT CMySQLContext : public impl::CDriverContext
{
    friend class CMySQL_Connection;

public:
    CMySQLContext();
    virtual ~CMySQLContext();

public:
    virtual bool IsAbleTo(ECapability cpb) const;

    virtual string GetDriverName(void) const;

protected:
    virtual impl::CConnection* MakeIConnection(const CDBConnParams& params);
};



/////////////////////////////////////////////////////////////////////////////
//
//  CMySQL_Connection::
//

class NCBI_DBAPIDRIVER_MYSQL_EXPORT CMySQL_Connection : public impl::CConnection
{
    friend class CMySQLContext;

protected:
    CMySQL_Connection(CMySQLContext& cntx, const CDBConnParams& params);
    virtual ~CMySQL_Connection();

protected:
    virtual bool IsAlive();

    virtual CDB_LangCmd*     LangCmd(const string& lang_query);
    virtual CDB_SendDataCmd* SendDataCmd(I_ITDescriptor& desc,
                                         size_t          data_size,
                                         bool            log_it = true,
                                         bool            dump_results = true);
    virtual CDB_RPCCmd*      RPC(const string& rpc_name);
    virtual CDB_BCPInCmd*    BCPIn(const string& table_name);
    virtual CDB_CursorCmd* Cursor(const string& cursor_name,
                                  const string& query,
                                  unsigned int batch_size = 1);


    virtual bool SendData(I_ITDescriptor& desc, CDB_Stream& lob,
                          bool log_it = true);

    virtual bool Refresh();
    virtual I_DriverContext::TConnectionMode ConnectMode() const;

    // abort the connection
    // Attention: it is not recommended to use this method unless you absolutely have to.
    // The expected implementation is - close underlying file descriptor[s] without
    // destroing any objects associated with a connection.
    // Returns: true - if succeed
    //          false - if not
    virtual bool Abort();

    /// Close an open connection.
    /// Returns: true - if successfully closed an open connection.
    ///          false - if not
    virtual bool Close(void);

    virtual void SetTimeout(size_t nof_secs);
    virtual void SetCancelTimeout(size_t nof_secs);

    string GetDbgInfo(void) const;

    string GetBaseDbgInfo(void) const
    {
        return " " + GetExecCntxInfo();
    }

private:
    friend class CMySQL_LangCmd;
    friend class CMySQL_RowResult;

    // for NCBI_DATABASE_THROW_ANNOTATED
    const CDBParams* GetBindParams(void) const;

    MYSQL          m_MySQL;
    CMySQL_LangCmd* m_ActiveCmd;
    bool m_IsOpen;
};



/////////////////////////////////////////////////////////////////////////////
//
//  CMySQL_LangCmd::
//

class NCBI_DBAPIDRIVER_MYSQL_EXPORT CMySQL_LangCmd : public impl::CBaseCmd
{
protected:
    CMySQL_LangCmd(CMySQL_Connection& conn,
                   const string&      lang_query);
    virtual ~CMySQL_LangCmd();

protected:
    virtual bool        Send();
    virtual bool        Cancel();
    virtual CDB_Result* Result();
    virtual bool        HasMoreResults() const;
    virtual bool        HasFailed() const;
    virtual int         RowCount() const;
    int                 LastInsertId() const;

    void SetExecCntxInfo(const string& info)
    {
        m_ExecCntxInfo = info;
    }
    const string& GetExecCntxInfo(void) const
    {
        return m_ExecCntxInfo;
    }
    string GetDbgInfo(void) const
    {
        return " " + GetExecCntxInfo() + " "
            + GetConnection().GetExecCntxInfo();
    }

public:
    string EscapeString(const char* str, unsigned long len);

private:
    friend class CMySQL_Connection;

    CMySQL_Connection& GetConnection(void)
    {
        _ASSERT(m_Connect);
        return *m_Connect;
    }

    const CMySQL_Connection& GetConnection(void) const
    {
        _ASSERT(m_Connect);
        return *m_Connect;
    }

    CMySQL_Connection* m_Connect;
    string             m_ExecCntxInfo;
    bool               m_HasMoreResults;
    bool               m_IsActive;
};


/////////////////////////////////////////////////////////////////////////////
//
//  CMySQL_RowResult::
//

class NCBI_DBAPIDRIVER_MYSQL_EXPORT CMySQL_RowResult : public impl::CResult
{
    friend class CMySQL_LangCmd;

protected:
    CMySQL_RowResult(CMySQL_Connection& conn);
    virtual ~CMySQL_RowResult();

protected:
    virtual EDB_ResType     ResultType() const;
    virtual bool            Fetch();
    virtual int             CurrentItemNo() const;
    virtual int             GetColumnNum(void) const;
    virtual CDB_Object*     GetItem(CDB_Object* item_buf = 0,
							I_Result::EGetItem policy = I_Result::eAppendLOB);
    virtual size_t          ReadItem(void* buffer, size_t buffer_size,
                                     bool* is_null = 0);
    virtual I_ITDescriptor* GetImageOrTextDescriptor();
    virtual bool            SkipItem();

    const CMySQL_Connection& GetConnection() const
    {
        _ASSERT(m_Connect);
        return *m_Connect;
    }

    string GetDbgInfo(void) const
    {
        return GetConnection().GetDbgInfo();
    }

    const CDBParams* GetBindParams(void) const 
    {
        return GetConnection().GetBindParams();
    }


private:
    MYSQL_RES*         m_Result;
    MYSQL_ROW          m_Row;
    unsigned long*     m_Lengths;
    CMySQL_Connection* m_Connect;
    int                m_CurrItem;
    // SMySQL_ColDescr*   m_ColFmt;
};

/////////////////////////////////////////////////////////////////////////////
extern NCBI_DBAPIDRIVER_MYSQL_EXPORT const string kDBAPI_MYSQL_DriverName;

extern "C"
{

NCBI_DBAPIDRIVER_MYSQL_EXPORT
void
NCBI_EntryPoint_xdbapi_mysql(
    CPluginManager<I_DriverContext>::TDriverInfoList&   info_list,
    CPluginManager<I_DriverContext>::EEntryPointRequest method);

} // extern C

/////////////////////////////////////////////////////////////////////////////
inline
string CMySQL_Connection::GetDbgInfo(void) const {
    return m_ActiveCmd ? m_ActiveCmd->GetDbgInfo() : GetBaseDbgInfo();
}

inline
const CDBParams* CMySQL_Connection::GetBindParams(void) const {
    return m_ActiveCmd ? &m_ActiveCmd->GetBindParams() : NULL;
}

END_NCBI_SCOPE


#endif


