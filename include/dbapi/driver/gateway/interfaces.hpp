#ifndef DBAPI_DRIVER_GATEWAY___INTERFACES__HPP
#define DBAPI_DRIVER_GATEWAY___INTERFACES__HPP

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
 * Author:  Victor Sapojnikov
 *
 * File Description:
 *   A gateway to a remote database driver that is running on the sss server.
 *
 */

#include <dbapi/driver/interfaces.hpp>
#include <dbapi/driver/public.hpp>
#include <dbapi/driver/util/handle_stack.hpp>

#include <dbapi/driver/gateway/comprot_cli.hpp>
#include <srv/igate.hpp>
#include <map>

BEGIN_NCBI_SCOPE

// DllExport ...
class CRLObjPairs;

void DBAPI_RegisterDriver_GW(I_DriverMgr& mgr);

class CGWContext;
class CGW_Connection;
class CGW_RPCCmd;
class CGW_CursorCmd;
class C_GWLib_MsgCallback;

class CGW_Base
{
  friend class CGW_Connection;
protected:
  int remoteObj;
  virtual void Release()
  {
    comprot_void("GWLib:Base:Release",remoteObj);
  }
  ~CGW_Base()
  {
    comprot_void("GWLib:Base:delete",remoteObj);
  }

  int xSend(const char* rpc_name, CRLObjPairs* boundObjects);
  bool xBind(IGate* pGate, CDB_Object* localObj, CRLObjPairs** boundObjects);
  CRLObjPairs* getBoundObjects(CRLObjPairs** ppVector); // Allocate vector on first request
};

class CGWContext : public I_DriverContext
{
  int remoteObj;
public:
  CGWContext(CSSSConnection& sssConnection);

  virtual bool IsAbleTo(ECapability cpb) const
  {
    return comprot_bool1( "GWLib:Context:IsAbleTo", remoteObj, (int*)&cpb );
  }

  virtual bool SetLoginTimeout (unsigned int nof_secs = 0)
  {
    return comprot_bool1( "GWLib:Context:SetLoginTimeout", remoteObj, &nof_secs );
  }
  virtual bool SetTimeout(unsigned int nof_secs = 0)
  {
    return comprot_bool1( "GWLib:Context:SetTimeout", remoteObj, &nof_secs );
  }
  virtual bool SetMaxTextImageSize(size_t nof_bytes)
  {
    return comprot_bool1( "GWLib:Context:SetMaxTextImageSize", remoteObj, (int*)&nof_bytes );
  }

  virtual unsigned int NofConnections(const string& srv_name = kEmptyStr) const
  {
    return (unsigned int)(comprot_int1( "GWLib:Context:NofConnections", remoteObj, srv_name.c_str() ));
  }

  virtual ~CGWContext()
  {
    // cerr << "~CGWContext()\n";
    comprot_void( "GWLib:Context:delete", remoteObj );
  }


  virtual CDB_Connection* Connect(
      const string&   srv_name,
      const string&   user_name,
      const string&   passwd,
      TConnectionMode mode,
      bool            reusable  = false,
      const string&   pool_name = kEmptyStr);
};



struct SConnectionVars
{
  // These variables are not stored directly in CGW_Connection
  // in order to make it possible to change them in const methods:
  // ServerName(), UserName(), etc.
  string m_Server, m_User, m_Password, m_PoolName;
};

class CGW_Connection : public I_Connection, CGW_Base
{
  friend class CGWContext;
  friend class CGW_CursorCmd;
  friend class C_GWLib_MsgCallback;

protected:
  CGWContext* localContext;
  SConnectionVars* vars;

  CGW_Connection(CGWContext* localContext_arg, int remoteObj_arg)
  {
    localContext = localContext_arg;
    remoteObj    = remoteObj_arg;
    vars = new SConnectionVars;
  }

  // virtual
  ~CGW_Connection()
  {
    delete vars;

    // Free server-side MsgHandler
    comprot_void("GWLib:Connection:delete",remoteObj);
  }

  virtual bool IsAlive()
  {
    return comprot_bool( "GWLib:Connection:IsAlive", remoteObj );
  }

  virtual CDB_RPCCmd* RPC( const string& rpc_name, unsigned int nof_args);
  virtual CDB_LangCmd* LangCmd(const string&  lang_query, unsigned int nof_params = 0);

  virtual CDB_BCPInCmd* BCPIn(const string&  table_name, unsigned int   nof_columns);
  virtual CDB_CursorCmd* Cursor(
    const string&  cursor_name,
    const string&  query,
    unsigned int   nof_params,
    unsigned int   batch_size = 1);

  virtual CDB_SendDataCmd* SendDataCmd(
    I_ITDescriptor& desc,
    size_t          data_size,
    bool            log_it = true);

  virtual bool Refresh()
  {
    return comprot_bool("GWLib:Connection:Refresh", remoteObj);
  }

  virtual const string& ServerName() const;
  virtual const string& UserName()   const;
  virtual const string& Password()   const;
  virtual const string& PoolName()   const;

  virtual I_DriverContext::TConnectionMode ConnectMode() const
  {
    return (I_DriverContext::TConnectionMode)
      comprot_int("GWLib:Connection:ConnectMode", remoteObj);
  }
  virtual bool IsReusable() const
  {
    return comprot_bool("GWLib:Connection:IsReusable", remoteObj);
  }

  virtual I_DriverContext* Context() const
  {
    return localContext;
  }

  // void DropCmd(CDB_BaseEnt& cmd);

  virtual bool xSendData(I_ITDescriptor& desc, CDB_Stream* img, bool log_it = true);
  virtual bool SendData(I_ITDescriptor& desc, CDB_Image& img, bool log_it = true);
  virtual bool SendData(I_ITDescriptor& desc, CDB_Text&  txt, bool log_it = true);

  virtual CDB_ResultProcessor* SetResultProcessor(CDB_ResultProcessor* rp) { return NULL; }

  virtual void PushMsgHandler(CDB_UserHandler* /*h*/);
  virtual void PopMsgHandler (CDB_UserHandler* /*h*/);
  // virtual void Release() {}
  CDBHandlerStack m_MsgHandlers;

    // abort the connection
    // Attention: it is not recommended to use this method unless you absolutely have to.
    // The expected implementation is - close underlying file descriptor[s] without
    // destroing any objects associated with a connection.
    // Returns: true - if succeed
    //          false - if not
    virtual bool Abort();

};


class CGW_BaseCmd : public I_BaseCmd, public CGW_Base
{
protected:
  CRLObjPairs* boundObjects;

public:
  CGW_BaseCmd()
  {
    boundObjects=NULL;
  }

  virtual bool Send();
  virtual bool WasSent() const
  {
    return comprot_bool("GWLib:BaseCmd:WasSent", remoteObj);
  }

  // Cancel the command execution
  virtual bool Cancel();
  virtual bool WasCanceled() const
  {
    return comprot_bool("GWLib:BaseCmd:WasCanceled", remoteObj);
  }

  // Get result set
  virtual CDB_Result* Result();
  virtual bool HasMoreResults() const
  {
    return comprot_bool( "GWLib:BaseCmd:HasMoreResults", remoteObj );
  }

  // Check if command has failed
  virtual bool HasFailed() const
  {
    return comprot_bool("GWLib:BaseCmd:HasFailed", remoteObj );
  }

  // Get the number of rows affected by the command
  // Special case:  negative on error or if there is no way that this
  //                command could ever affect any rows (like PRINT).
  virtual int RowCount() const
  {
    return comprot_int("GWLib:BaseCmd:RowCount", remoteObj );
  }

  // Destructor
  virtual ~CGW_BaseCmd();
};

class CGW_LangCmd : public I_LangCmd, CGW_BaseCmd
{
  friend class CGW_Connection;
protected:
  CGW_Connection* con;

  CGW_LangCmd(CGW_Connection* con_arg, int remoteObj_arg)
  {
    con       = con_arg;
    remoteObj = remoteObj_arg;
  }

  virtual bool More(const string& query_text)
  {
    return comprot_bool1("GWLib:LangCmd:More", remoteObj, query_text.c_str() );
  }

  virtual bool BindParam(const string& param_name, CDB_Object* param_ptr);
  virtual bool SetParam(const string& param_name, CDB_Object* param_ptr);

  virtual bool Send()
  {
    // ?? send bound params ??
    return CGW_BaseCmd::Send();
  }

  virtual bool WasSent() const
  {
    return CGW_BaseCmd::WasSent();
  }
  virtual bool Cancel();
  /*
  {
    return CGW_BaseCmd::Cancel();
  }
  */
  virtual bool WasCanceled() const
  {
    return CGW_BaseCmd::WasCanceled();
  }

  virtual CDB_Result* Result()
  {
    return CGW_BaseCmd::Result();
  }
  virtual bool HasMoreResults() const
  {
    return CGW_BaseCmd::HasMoreResults();
  }

  virtual bool HasFailed() const
  {
    return CGW_BaseCmd::HasFailed();
  }
  virtual int  RowCount() const
  {
    return CGW_BaseCmd::RowCount();
  }
  virtual void DumpResults()
  {
    comprot_void("GWLib:LangCmd:DumpResults",remoteObj);
  }
};


class CGW_RPCCmd : public I_RPCCmd, CGW_BaseCmd
{
  friend class CGW_Connection;
protected:
  CGW_Connection* con;

  CGW_RPCCmd(CGW_Connection* con_arg, int remoteObj_arg)
  {
    con       = con_arg;
    remoteObj = remoteObj_arg;
  }


  virtual bool BindParam(const string& param_name, CDB_Object* param_ptr, bool out_param = false);
  virtual bool SetParam(const string& param_name, CDB_Object* param_ptr, bool out_param = false);

  virtual bool Send()
  {
    // ?? send bound params ??
    return CGW_BaseCmd::Send();
  }


  virtual bool WasSent() const
  {
    return CGW_BaseCmd::WasSent();
  }
  virtual bool Cancel()
  {
    return CGW_BaseCmd::Cancel();
  }
  virtual bool WasCanceled() const
  {
    return CGW_BaseCmd::WasCanceled();
  }

  virtual CDB_Result* Result()
  {
    return CGW_BaseCmd::Result();
  }
  virtual bool HasMoreResults() const
  {
    return CGW_BaseCmd::HasMoreResults();
  }

  virtual bool HasFailed() const
  {
    return CGW_BaseCmd::HasFailed();
  }
  virtual int  RowCount() const
  {
    return CGW_BaseCmd::RowCount();
  }


  virtual void SetRecompile(bool recompile = true)
  {
    int i = (int)recompile;
    comprot_void1("GWLib:RPCCmd:SetRecompile", remoteObj, &i );
  }
  virtual void DumpResults()
  {
    comprot_void("GWLib:RPCCmd:DumpResults",remoteObj);
  }
};

class CGW_BCPInCmd : public I_BCPInCmd, CGW_Base
{
  friend class CGW_Connection;
protected:
  CGW_Connection* con;
  CRLObjPairs* boundObjects;

  CGW_BCPInCmd(CGW_Connection* con_arg, int remoteObj_arg)
  {
    con       = con_arg;
    remoteObj = remoteObj_arg;
    boundObjects=NULL;
  }


  // Binding
  virtual bool Bind(unsigned int column_num, CDB_Object* param_ptr);

  // Send row to the server
  virtual bool SendRow();

  // Complete batch -- to store all rows transferred so far in this batch
  // into the table
  virtual bool CompleteBatch()
  {
    return comprot_bool( "GWLib:BCPInCmd:CompleteBatch", remoteObj );
  }

  // Cancel the BCP command
  virtual bool Cancel();

  // Complete the BCP and store all rows transferred in last batch
  // into the table
  virtual bool CompleteBCP();

  ~CGW_BCPInCmd();

};

class CGW_CursorCmd : public I_CursorCmd, CGW_Base
{
  friend class CGW_Connection;
protected:
  CGW_Connection* con;
  CRLObjPairs* boundObjects;

  CGW_CursorCmd(CGW_Connection* con_arg, int remoteObj_arg)
  {
    con       = con_arg;
    remoteObj = remoteObj_arg;
    boundObjects = NULL;
  }

  // Not implemented
  virtual bool BindParam(const string& name, CDB_Object* param_ptr);

  // Open the cursor.
  // Return NULL if cursor resulted in no data.
  // Throw exception on error.
  virtual CDB_Result* Open();

  // Update the last fetched row.
  // NOTE: the cursor must be declared for update in CDB_Connection::Cursor()
  virtual bool Update(const string& table_name, const string& upd_query)
  {
    return comprot_int2(
      "GWLib:CursorCmd:Update", remoteObj,
      "table_name", table_name.c_str(),
      "upd_query", upd_query.c_str() );
  }
  virtual bool UpdateTextImage(unsigned int item_num, CDB_Stream& data,
                                bool log_it = true);

  virtual CDB_SendDataCmd* SendDataCmd(
      unsigned int item_num, size_t size, bool log_it = true);

  // Delete the last fetched row.
  // NOTE: the cursor must be declared for delete in CDB_Connection::Cursor()
  virtual bool Delete(const string& table_name)
  {
    return comprot_bool1("GWLib:CursorCmd:Delete", remoteObj, table_name.c_str() );
  }

  // Get the number of fetched rows
  // Special case:  negative on error or if there is no way that this
  //                command could ever affect any rows (like PRINT).
  virtual int RowCount() const
  {
    return comprot_int("GWLib:CursorCmd:RowCount", remoteObj);
  }

  // Close the cursor.
  // Return FALSE if the cursor is closed already (or not opened yet)
  virtual bool Close();
  ~CGW_CursorCmd();
};

class CGW_SendDataCmd : public I_SendDataCmd, CGW_Base
{
  friend class CGW_Connection;
  friend class CGW_CursorCmd;
protected:
  CGW_Connection* con;

  CGW_SendDataCmd(CGW_Connection* con_arg, int remoteObj_arg)
  {
    con       = con_arg;
    remoteObj = remoteObj_arg;
  }

  // Send chunk of data to the server.
  // Return number of bytes actually transferred to server.
  virtual size_t SendChunk(const void* pChunk, size_t nofBytes);
};



class CGW_ITDescriptor : public I_ITDescriptor
{
private:
  int remoteObj;
public:
  CGW_ITDescriptor(int remoteObj_arg)
  {
    remoteObj = remoteObj_arg;
  }
  int getRemoteObj()
  {
    return remoteObj;
  }
  int DescriptorType() const
  {
    return comprot_int("GWLib:ITDescriptor:DescriptorType", remoteObj);
  }
};

class CGW_Result : public I_Result, CGW_Base
{
protected:
  typedef map<unsigned,string> MapUnsignedToString;
  MapUnsignedToString mapItemNames;

public:

  CGW_Result(int remoteObj_arg)
  {
    remoteObj = remoteObj_arg;
  }

  // ~CGW_Result() { mapItemNames.clear(); }

  virtual EDB_ResType ResultType() const
  {
    return (EDB_ResType)comprot_int("GWLib:Result:ResultType",remoteObj);
  }


  // Get # of columns in the result
  virtual unsigned int NofItems() const
  {
    return comprot_int("GWLib:Result:NofItems",remoteObj);
  }

  // Get name of a result item; returnr NULL if "item_num" >= NofItems().
  virtual const char* ItemName(unsigned int item_num) const;

  // Get size (in bytes) of a result item.
  // Return zero if "item_num" >= NofItems().
  virtual size_t ItemMaxSize(unsigned int item_num) const
  {
    return comprot_int1("GWLib:Result:ItemMaxSize",remoteObj,(int*)&item_num);
  }

  // Get datatype of a result item.
  // Return 'eDB_UnsupportedType' if "item_num" >= NofItems().
  virtual EDB_Type ItemDataType(unsigned int item_num) const
  {
    return (EDB_Type) comprot_int1("GWLib:Result:ItemDataType",remoteObj,(int*)&item_num);
  }

  // Fetch next row.
  // Return FALSE if no more rows to fetch. Throw exception on any error.
  virtual bool Fetch()
  {
    return comprot_bool( "GWLib:Result:Fetch", remoteObj );
  }

  // Return current item number we can retrieve (0,1,...)
  // Return "-1" if no more items left (or available) to read.
  virtual int CurrentItemNo() const
  {
    return comprot_int( "GWLib:Result:CurrentItemNo", remoteObj );
  }

  // Get a result item (you can use either GetItem or ReadItem).
  // If "item_buf" is not NULL, then use "*item_buf" (its type should be
  // compatible with the type of retrieved item!) to retrieve the item to;
  // otherwise allocate new "CDB_Object".
  virtual CDB_Object* GetItem(CDB_Object* item_buf = 0);

  // Read a result item body (for text/image mostly).
  // Return number of successfully read bytes.
  // Set "*is_null" to TRUE if the item is <NULL>.
  // Throw an exception on any error.
  virtual size_t ReadItem(void* buffer, size_t buffer_size,
                          bool* is_null = 0);

  // Get a descriptor for text/image column (for SendData).
  // Return NULL if this result does not (or cannot) have img/text descriptor.
  // NOTE: you need to call ReadItem (maybe even with buffer_size == 0)
  //       before calling this method!
  virtual I_ITDescriptor* GetImageOrTextDescriptor()
  {
    int res = comprot_int( "GWLib:Result:GetImageOrTextDescriptor", remoteObj );
    if(res) {
      return new CGW_ITDescriptor(res);
    }
    else{
      return NULL;
    }
  }

  // Skip result item
  virtual bool SkipItem()
  {
    return comprot_bool( "GWLib:Result:SkipItem", remoteObj );
  }

};


END_NCBI_SCOPE


#endif  /* DBAPI_DRIVER_GATEWAY___INTERFACES__HPP */



/*
 * ===========================================================================
 * $Log$
 * Revision 1.8  2005/02/23 21:32:35  soussov
 * Adds Abort() method to connection
 *
 * Revision 1.7  2003/07/16 17:33:11  sapojnik
 * SetResultProcessor(), DumpResults() - almost empty implementations
 *
 * Revision 1.6  2003/05/19 21:51:51  sapojnik
 * Client portion of gateway driver back in C++ tree - now assembles as dll, and runs on Sparc Solaris
 *
 * Revision 1.9  2003/05/12 20:53:31  sapojnik
 * class C_GWLib_MsgCallback,  CGW_Connection::Push/PopMsgHandler()
 *
 * Revision 1.8  2003/05/05 21:53:33  sapojnik
 * CGWBase::xSend() returns int
 *
 * Revision 1.7  2003/05/05 14:36:06  sapojnik
 * CGW_Base::xBind(),xSend() and boundObjects member in classes with Send/Bind()
 *
 * Revision 1.6  2003/02/21 20:12:22  sapojnik
 * many changes, mostly implementing and debugging BCP Bind/SendRow()
 *
 * Revision 1.5  2003/01/08 22:52:27  sapojnik
 * *** empty log message ***
 *
 * Revision 1.4  2003/01/08 22:11:07  sapojnik
 * RPCCmd:SetRecompile - do send flag vlue
 *
 * Revision 1.3  2002/12/03 16:43:25  sapojnik
 * *** empty log message ***
 *
 * Revision 1.2  2002/11/27 21:11:22  sapojnik
 * Bind/SetParam(), SendData(), sending CDB_Float/DoubleO. etc.
 * implemented for various classes, not tested yet;
 * it seems that only CGW_Connection::Push/PopMsgHandler() still remain.
 *
 * ===========================================================================
 */
