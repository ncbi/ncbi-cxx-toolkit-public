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
 * File Description:  Driver for MySQL server
 *
 */

#include <dbapi/driver/public.hpp>
#include <dbapi/driver/util/parameters.hpp>
#include <mysql.h>

BEGIN_NCBI_SCOPE

class CMySQLContext;
class CMySQL_Connection;
class CMySQL_LangCmd;

class CMySQLContext : public I_DriverContext
{

public:
  CMySQLContext();
  virtual ~CMySQLContext();

  virtual bool SetLoginTimeout (unsigned int nof_secs = 0);
  virtual bool SetTimeout      (unsigned int nof_secs = 0);
  virtual bool SetMaxTextImageSize(size_t nof_bytes);

  virtual bool IsAbleTo(ECapability cpb) const;
  virtual CDB_Connection* Connect(const string &srv_name,
                                  const string &user_name,
                                  const string &passwd,
                                  TConnectionMode mode,
                                  bool reusable  = false,
                                  const string &pool_name = kEmptyStr);

  virtual unsigned int NofConnections(const string &srv_name = kEmptyStr) const;

private:
  friend class CMySQL_Connection;
};

class CMySQL_Connection : public I_Connection
{
protected:
  friend class CMySQLContext;

  CMySQL_Connection(CMySQLContext* cntx,
                    const string &srv_name,
                    const string &user_name,
                    const string &passwd);

  virtual ~CMySQL_Connection();

  virtual bool IsAlive();

  virtual CDB_LangCmd *LangCmd(const string &lang_query,
                               unsigned int nof_params = 0);

  virtual CDB_SendDataCmd* SendDataCmd(I_ITDescriptor &desc,
                                       size_t data_size,
                                       bool log_it = true);

  virtual CDB_RPCCmd *RPC(const string &rpc_name,
                          unsigned int nof_args);

  virtual CDB_BCPInCmd *BCPIn(const string &table_name,
                              unsigned int nof_columns);

  virtual CDB_CursorCmd *Cursor(const string &cursor_name,
                                const string &query,
                                unsigned int nof_params,
                                unsigned int batch_size = 1);


  virtual bool SendData(I_ITDescriptor& desc, CDB_Image& img,
                        bool log_it = true);
  virtual bool SendData(I_ITDescriptor& desc, CDB_Text&  txt,
                        bool log_it = true);
  virtual bool Refresh();
  virtual const string& ServerName() const;
  virtual const string& UserName()   const;
  virtual const string& Password()   const;
  virtual I_DriverContext::TConnectionMode ConnectMode() const;
  virtual bool IsReusable() const;
  virtual const string& PoolName() const;
  virtual I_DriverContext* Context() const;
  virtual void PushMsgHandler(CDB_UserHandler* h);
  virtual void PopMsgHandler (CDB_UserHandler* h);
  virtual void Release();


  void DropCmd(CDB_BaseEnt& cmd);

private:
  friend class CMySQL_LangCmd;
  friend class CMySQL_RowResult;

  CMySQLContext *m_Context;
  MYSQL m_MySQL;

};

class CMySQL_LangCmd : public I_LangCmd
{
  friend class CMySQL_Connection;
protected:
  CMySQL_LangCmd(CMySQL_Connection* conn,
                 const string& lang_query, unsigned int nof_params);
  virtual ~CMySQL_LangCmd();

  virtual bool More(const string& query_text);
  virtual bool BindParam(const string& param_name, CDB_Object* param_ptr);
  virtual bool SetParam(const string& param_name, CDB_Object* param_ptr);
  virtual bool Send();
  virtual bool WasSent() const;
  virtual bool Cancel();
  virtual bool WasCanceled() const;
  virtual CDB_Result* Result();
  virtual bool HasMoreResults() const;
  virtual bool HasFailed() const;
  virtual int  RowCount() const;
  virtual void Release();
private:
  CMySQL_Connection *m_Connect;
  string m_Query;
  bool m_HasResults;
};

struct SMySQL_ColDescr
{
  unsigned long max_length;
  EDB_Type data_type;
  string  col_name;
};

class CMySQL_RowResult : public I_Result
{
protected:
  friend class CMySQL_LangCmd;
  CMySQL_RowResult(CMySQL_Connection* conn);
  virtual ~CMySQL_RowResult();

  virtual EDB_ResType ResultType() const;
  virtual unsigned int NofItems() const;
  virtual const char *ItemName(unsigned int item_num) const;
  virtual size_t ItemMaxSize(unsigned int item_num) const;
  virtual EDB_Type ItemDataType(unsigned int item_num) const;
  virtual bool Fetch();
  virtual int CurrentItemNo() const;
  virtual CDB_Object *GetItem(CDB_Object* item_buf = 0);
  virtual size_t ReadItem(void* buffer, size_t buffer_size,
                          bool* is_null = 0);
  virtual I_ITDescriptor *GetImageOrTextDescriptor();
  virtual bool SkipItem();

private:
  MYSQL_RES *m_Result;
  MYSQL_ROW m_Row;
  unsigned long *m_Lengths;
  CMySQL_Connection *m_Connect;
  int m_CurrItem;
  unsigned int m_NofCols;
  SMySQL_ColDescr *m_ColFmt;

};

END_NCBI_SCOPE

#endif

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2002/08/13 20:21:59  butanaev
 * The beginning.
 *
 * ===========================================================================
 */
