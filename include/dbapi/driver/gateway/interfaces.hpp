#ifndef DBAPI_DRIVER_RDBLIB___INTERFACES__HPP
#define DBAPI_DRIVER_RDBLIB___INTERFACES__HPP

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
 * File Description:  Driver for DBLib server
 *
 */

// prevent WINSOCK.H from being included - we use WINSOCK2.H
#define _WINSOCKAPI_

#include <dbapi/driver/dblib/interfaces.hpp>
#include <dbapi/driver/public.hpp>

#include <dbapi/driver/gateway/comprot_cli.hpp>
#include <common/igate.hpp>
#include <map>

BEGIN_NCBI_SCOPE

// DllExport ...

class CGWContext;
class CGW_Connection;
class CGW_RPCCmd;

/*
class CGW_LangCmd;
class CRDB_CursorCmd;
class CRDB_BCPInCmd;
class CRDB_SendDataCmd;
class CRDB_RowResult;
class CRDB_ParamResult;
class CRDB_ComputeResult;
class CRDB_StatusResult;
class CRDB_BlobResult;
*/

class CGWContext : public I_DriverContext
{
    int remoteObj;

public:
    CGWContext(CSSSConnection& sssConnection);

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


class CGW_Connection : public I_Connection
{
    friend class CGWContext;
    friend class CDB_Connection;
    /*
    friend class CGW_LangCmd;
    friend class CGW_RPCCmd;
    friend class CRDB_CursorCmd;
    friend class CRDB_BCPInCmd;
    friend class CRDB_SendDataCmd;
    */

protected:
    int remoteObj;
    CGWContext* localContext;

    CGW_Connection(CGWContext* localContext_arg, int remoteObj_arg)
    {
      localContext = localContext_arg;
      remoteObj    = remoteObj_arg;
    }

    // virtual
    ~CGW_Connection()
    {
      comprot_void( "GWLib:Connection:delete", remoteObj );
    }

    virtual bool IsAlive()
    {
      return comprot_bool( "GWLib:Connection:IsAlive", remoteObj );
    }

    virtual CDB_RPCCmd* RPC( const string& rpc_name, unsigned int nof_args);
    virtual CDB_LangCmd* LangCmd(const string&  lang_query, unsigned int nof_params = 0);

    //// Most methods below are not implemented yet:
    virtual CDB_BCPInCmd* BCPIn(
      const string&  table_name,
      unsigned int   nof_columns)
    { return NULL; }

    virtual CDB_CursorCmd* Cursor(
      const string&  cursor_name,
      const string&  query,
      unsigned int   nof_params,
      unsigned int   batch_size = 1)
    { return NULL; }

    virtual CDB_SendDataCmd* SendDataCmd(
      I_ITDescriptor& desc,
      size_t          data_size,
      bool            log_it = true)
    { return NULL; }


    virtual bool SendData(
      I_ITDescriptor& desc, CDB_Image& img, bool log_it = true)
    { return false; }

    virtual bool SendData(
      I_ITDescriptor& desc, CDB_Text&  txt, bool log_it = true)
    { return false; }

    virtual bool Refresh()
    {
      return comprot_bool("GWLib:Connection:Refresh", remoteObj);
    }

    virtual const string& ServerName() const
    { return NcbiEmptyString; }
    virtual const string& UserName()   const
    { return NcbiEmptyString; }
    virtual const string& Password()   const
    { return NcbiEmptyString; }
    virtual I_DriverContext::TConnectionMode ConnectMode() const
    { return 0; }
    virtual bool IsReusable() const
    { return false; }
    virtual const string& PoolName() const
    { return NcbiEmptyString; }
    virtual I_DriverContext* Context() const
    { return localContext; }
    virtual void PushMsgHandler(CDB_UserHandler* h)
    {}
    virtual void PopMsgHandler (CDB_UserHandler* h)
    {}
    virtual void Release()
    {}
    void DropCmd(CDB_BaseEnt& cmd)
    {}
};


class CGW_LangCmd : public I_LangCmd
{
    friend class CGW_Connection;
protected:
    CGW_Connection* con;
    int remoteObj;

    CGW_LangCmd(CGW_Connection* con_arg, int remoteObj_arg)
    {
      con       = con_arg;
      remoteObj = remoteObj_arg;
    }
    ~CGW_LangCmd()
    {
      comprot_void( "GWLib:LangCmd:delete", remoteObj );
    }

    virtual bool More(const string& query_text)
    {
      return comprot_bool1("GWLib:LangCmd:More", remoteObj, query_text.c_str() );
    }

    // Not implemented
    virtual bool BindParam(const string& param_name, CDB_Object* param_ptr)
    { return false; }
    virtual bool SetParam(const string& param_name, CDB_Object* param_ptr)
    { return false; }

    virtual bool Send()
    {
      return comprot_bool("GWLib:LangCmd:Send", remoteObj );
    }

    // Not implemented
    virtual bool WasSent() const
    { return false; }
    virtual bool Cancel()
    { return false; }
    virtual bool WasCanceled() const
    { return false; }

    virtual CDB_Result* Result();
    virtual bool HasMoreResults() const
    {
      return comprot_bool("GWLib:LangCmd:HasMoreResults", remoteObj );
    }
    virtual bool HasFailed() const
    {
      return comprot_bool("GWLib:LangCmd:HasFailed", remoteObj );
    }
    virtual int  RowCount() const
    {
      return comprot_int("GWLib:LangCmd:HasFailed", remoteObj );
    }
};


class CGW_RPCCmd : public I_RPCCmd
{
    friend class CGW_Connection;
protected:
    CGW_Connection* con;
    int remoteObj;

    CGW_RPCCmd(CGW_Connection* con_arg, int remoteObj_arg)
    {
      con       = con_arg;
      remoteObj = remoteObj_arg;
    }
    ~CGW_RPCCmd()
    {
      comprot_void( "GWLib:RPCCmd:delete", remoteObj );
    }


    // Not implemented
    virtual bool BindParam(const string& param_name, CDB_Object* param_ptr,
                           bool out_param = false)
    {
      return false;
    }
    virtual bool SetParam(const string& param_name, CDB_Object* param_ptr,
                          bool out_param = false)
    { return false; }

    virtual bool Send()
    {
      return comprot_bool( "GWLib:RPCCmd:Send", remoteObj );
    }

    // Not implemented
    virtual bool WasSent() const {return false;}
    virtual bool Cancel() {return false;}
    virtual bool WasCanceled() const {return false;}

    virtual CDB_Result* Result();
    virtual bool HasMoreResults() const
    {
      return comprot_bool( "GWLib:RPCCmd:HasMoreResults", remoteObj );
    }

    // Not implemented
    virtual bool HasFailed() const {return false;}
    virtual int  RowCount() const {return 0;}
    virtual void SetRecompile(bool recompile = true) {}

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
};

class CGW_Result : public I_Result
{
protected:
    // I_BaseCmd* cmd;
    int remoteObj;

    typedef map<unsigned,string> MapUnsignedToString;
    MapUnsignedToString mapItemNames;

public:

    CGW_Result(int remoteObj_arg)
    {
      remoteObj = remoteObj_arg;
    }

    ~CGW_Result()
    {
      //mapItemNames.clear();
      comprot_void( "GWLib:Result:delete", remoteObj );
    }

    virtual EDB_ResType ResultType() const
    {
      return (EDB_ResType)comprot_int("GWLib:Result:ResultType",remoteObj);
    }


    // Get # of columns in the result
    virtual unsigned int NofItems() const
    {
      return comprot_int("GWLib:Result:NofItems",remoteObj);
    }

    // Get name of a result item.
    // Return NULL if "item_num" >= NofItems().
    virtual const char* ItemName(unsigned int item_num) const
    {
      char buf[1024];
      const char* res = comprot_chars1( "GWLib:Result:ItemName", remoteObj, (int*)&item_num, buf, sizeof(buf) );
      if(!res) return NULL;

      //mapItemNames[item_num] = buf;
      //return mapItemNames[item_num].c_str();
      MapUnsignedToString* m = const_cast<MapUnsignedToString*>(&mapItemNames);
      MapUnsignedToString::iterator it = (MapUnsignedToString::iterator) m->find(item_num);
      if( it==m->end() ) {
        m->insert( MapUnsignedToString::value_type(item_num,res) );
        it = m->find(item_num);
      }
      else {
        it->second = res;
      }
      return it->second.c_str();
    }

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
    // {
    //   return item_buf; // To do: implement later for all 16 types...
    // }

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


#endif  /* DBAPI_DRIVER_RDBLIB___INTERFACES__HPP */



/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2002/03/14 22:53:23  sapojnik
 * Inheriting from I_ interfaces instead of CDB_ classes from driver/public.hpp
 *
 * Revision 1.1  2002/03/14 20:00:41  sapojnik
 * A driver that communicates with a dbapi driver on another machine via CompactProtocol(aka ssssrv)
 *
 * Revision 1.1  2002/02/13 20:41:18  sapojnik
 * Remote dblib: MS SQL plus sss comprot
 *
 * ===========================================================================
 */
