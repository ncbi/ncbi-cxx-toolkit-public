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

class CRemoteDBContext;
class CRDB_Connection;
class CRDB_RPCCmd;

/*
class CRDB_LangCmd;
class CRDB_CursorCmd;
class CRDB_BCPInCmd;
class CRDB_SendDataCmd;
class CRDB_RowResult;
class CRDB_ParamResult;
class CRDB_ComputeResult;
class CRDB_StatusResult;
class CRDB_BlobResult;
*/

// const unsigned int kDBLibMaxNameLen = 128 + 4;

class CRemoteDBContext : public I_DriverContext
{
    // friend class CDB_Connection;
    int remoteObj;

public:
    CRemoteDBContext(CSSSConnection& sssConnection);

    virtual bool SetLoginTimeout (unsigned int nof_secs = 0)
    {
      return comprot_bool1( "RDBLib:Context:SetLoginTimeout", remoteObj, &nof_secs );
    }
    virtual bool SetTimeout(unsigned int nof_secs = 0)
    {
      return comprot_bool1( "RDBLib:Context:SetTimeout", remoteObj, &nof_secs );
    }
    virtual bool SetMaxTextImageSize(size_t nof_bytes)
    {
      return comprot_bool1( "RDBLib:Context:SetMaxTextImageSize", remoteObj, (int*)&nof_bytes );
    }

    virtual unsigned int NofConnections(const string& srv_name = kEmptyStr) const
    {
      return (unsigned int)(comprot_int1( "RDBLib:Context:NofConnections", remoteObj, srv_name.c_str() ));
    }

    virtual ~CRemoteDBContext()
    {
      comprot_void( "RDBLib:Context:delete", remoteObj );
    }


    virtual CDB_Connection* Connect(
        const string&   srv_name,
        const string&   user_name,
        const string&   passwd,
        TConnectionMode mode,
        bool            reusable  = false,
        const string&   pool_name = kEmptyStr);

/*
    //
    // DBLIB specific functionality
    //

    // the following methods are optional (driver will use the default values if not called)
    // the values will affect the new connections only

    virtual void DBLIB_SetApplicationName(const string& a_name);
    virtual void DBLIB_SetHostName(const string& host_name);
    virtual void DBLIB_SetPacketSize(int p_size);
    virtual bool DBLIB_SetMaxNofConns(int n);
    static  int  DBLIB_dberr_handler(DBPROCESS*    dblink,   int     severity,
                                     int           dberr,    int     oserr,
                                     const string& dberrstr,
                                     const string& oserrstr);
    static  void DBLIB_dbmsg_handler(DBPROCESS*    dblink,   DBINT   msgno,
                                     int           msgstate, int     severity,
                                     const string& msgtxt,
                                     const string& srvname,
                                     const string& procname,
                                     int           line);

private:
    static CRemoteDBContext* m_pDBLibContext;
    string                m_AppName;
    string                m_HostName;
    short                 m_PacketSize;
    LOGINREC*             m_Login;

    DBPROCESS* x_ConnectToServer(const string&   srv_name,
                                 const string&   user_name,
                                 const string&   passwd,
                                 TConnectionMode mode);\
*/
};


/////////////////////////////////////////////////////////////////////////////
//
//  CTL_Connection::
//

class CRDB_Connection : public CDB_Connection
{
    /*
    friend class CRemoteDBContext;
    friend class CDB_Connection;
    friend class CRDB_LangCmd;
    friend class CRDB_RPCCmd;
    friend class CRDB_CursorCmd;
    friend class CRDB_BCPInCmd;
    friend class CRDB_SendDataCmd;
    */

protected:
    int remoteObj;
    CRemoteDBContext* localContext;

public:
    // CRDB_Connection(CRemoteDBContext* cntx, DBPROCESS* con,
    //                bool reusable, const string& pool_name);
    CRDB_Connection(CRemoteDBContext* localContext_arg, int remoteObj_arg)
    {
      localContext = localContext_arg;
      remoteObj    = remoteObj_arg;
    }

    ~CRDB_Connection()
    {
      comprot_void( "RDBLib:Connection:delete", remoteObj );
    }

    virtual bool IsAlive()
    {
      return comprot_bool( "RDBLib:Connection:IsAlive", remoteObj );
    }

    virtual CDB_RPCCmd* RPC( const string& rpc_name, unsigned int nof_args);
    virtual CDB_LangCmd* LangCmd(const string&  lang_query, unsigned int nof_params = 0);

    /*
    virtual CDB_BCPInCmd* BCPIn(
      const string&  table_name,
      unsigned int   nof_columns)
    {
      return NULL;
    }

    virtual CDB_CursorCmd* Cursor(
      const string&  cursor_name,
      const string&  query,
      unsigned int   nof_params,
      unsigned int   batch_size = 1)
    {
      return NULL;
    }

    virtual CDB_SendDataCmd* SendDataCmd(
      I_ITDescriptor& desc,
      size_t          data_size,
      bool            log_it = true)
    {
      return NULL;
    }


    virtual bool SendData(
      I_ITDescriptor& desc, CDB_Image& img, bool log_it = true)
    {
      return false;
    }

    virtual bool SendData(
      I_ITDescriptor& desc, CDB_Text&  txt, bool log_it = true)
    {
      return false;
    }

    virtual bool Refresh()
    {
      return comprot_bool("RDBLib:Connection:Refresh", remoteObj);
    }

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

    virtual ~CRDB_Connection();

    void DropCmd(CDB_BaseEnt& cmd);
    */

};



/////////////////////////////////////////////////////////////////////////////
//
//  CDBL_LangCmd::
//

class CRDB_LangCmd : public CDB_LangCmd
{
protected:
    CRDB_Connection* con;
    int remoteObj;
public:
    CRDB_LangCmd(CRDB_Connection* con_arg, int remoteObj_arg)
    {
      con       = con_arg;
      remoteObj = remoteObj_arg;
    }
    ~CRDB_LangCmd()
    {
      comprot_void( "RDBLib:LangCmd:delete", remoteObj );
    }

    virtual bool More(const string& query_text)
    {
      return comprot_bool1("RDBLib:LangCmd:More", remoteObj, query_text.c_str() );
    }
    /*
    virtual bool BindParam(const string& param_name, CDB_Object* param_ptr);
    virtual bool SetParam(const string& param_name, CDB_Object* param_ptr);
    */
    virtual bool Send()
    {
      return comprot_bool("RDBLib:LangCmd:Send", remoteObj );
    }
    /*
    virtual bool WasSent() const;
    virtual bool Cancel();
    virtual bool WasCanceled() const;
    */
    virtual CDB_Result* Result();
    virtual bool HasMoreResults() const
    {
      return comprot_bool("RDBLib:LangCmd:HasMoreResults", remoteObj );
    }
    virtual bool HasFailed() const
    {
      return comprot_bool("RDBLib:LangCmd:HasFailed", remoteObj );
    }
    virtual int  RowCount() const
    {
      return comprot_int("RDBLib:LangCmd:HasFailed", remoteObj );
    }
};



/////////////////////////////////////////////////////////////////////////////
//
//  CRDB_RPCCmd::
//

class CRDB_RPCCmd : public CDB_RPCCmd
{
    //friend class CRDB_Connection;
protected:
    CRDB_Connection* con;
    int remoteObj;
public:
    CRDB_RPCCmd(CRDB_Connection* con_arg, int remoteObj_arg)
    {
      con       = con_arg;
      remoteObj = remoteObj_arg;
    }
    ~CRDB_RPCCmd()
    {
      comprot_void( "RDBLib:RPCCmd:delete", remoteObj );
    }
    /*
    virtual bool BindParam(const string& param_name, CDB_Object* param_ptr,
                           bool out_param = false);
    virtual bool SetParam(const string& param_name, CDB_Object* param_ptr,
                          bool out_param = false);
    */
    virtual bool Send()
    {
      return comprot_bool( "RDBLib:RPCCmd:Send", remoteObj );
    }

    /*
    virtual bool WasSent() const;
    virtual bool Cancel();
    virtual bool WasCanceled() const;
    */
    virtual CDB_Result* Result();
    virtual bool HasMoreResults() const
    {
      return comprot_bool( "RDBLib:RPCCmd:HasMoreResults", remoteObj );
    }
    /*
    virtual bool HasFailed() const ;
    virtual int  RowCount() const;
    virtual void SetRecompile(bool recompile = true);
    */
};

class CRDB_ITDescriptor : public I_ITDescriptor
{
private:
  int remoteObj;
public:
  CRDB_ITDescriptor(int remoteObj_arg)
  {
    remoteObj = remoteObj_arg;
  }
  int getRemoteObj()
  {
    return remoteObj;
  }
};

class CRDB_Result : public CDB_Result
{
protected:
    // I_BaseCmd* cmd;
    int remoteObj;

    typedef map<unsigned,string> MapUnsignedToString;
    MapUnsignedToString mapItemNames;

public:

    CRDB_Result(int remoteObj_arg)
    {
      remoteObj = remoteObj_arg;
    }

    ~CRDB_Result()
    {
      //mapItemNames.clear();
      comprot_void( "RDBLib:Result:delete", remoteObj );
    }

    virtual EDB_ResType ResultType() const
    {
      return (EDB_ResType)comprot_int("RDBLib:Result:ResultType",remoteObj);
    }


    // Get # of columns in the result
    virtual unsigned int NofItems() const
    {
      return comprot_int("RDBLib:Result:NofItems",remoteObj);
    }

    // Get name of a result item.
    // Return NULL if "item_num" >= NofItems().
    virtual const char* ItemName(unsigned int item_num) const
    {
      char buf[1024];
      const char* res = comprot_chars1( "RDBLib:Result:ItemName", remoteObj, (int*)&item_num, buf, sizeof(buf) );
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
      return comprot_int1("RDBLib:Result:ItemMaxSize",remoteObj,(int*)&item_num);
    }

    // Get datatype of a result item.
    // Return 'eDB_UnsupportedType' if "item_num" >= NofItems().
    virtual EDB_Type ItemDataType(unsigned int item_num) const
    {
      return (EDB_Type) comprot_int1("RDBLib:Result:ItemDataType",remoteObj,(int*)&item_num);
    }

    // Fetch next row.
    // Return FALSE if no more rows to fetch. Throw exception on any error.
    virtual bool Fetch()
    {
      return comprot_bool( "RDBLib:Result:Fetch", remoteObj );
    }

    // Return current item number we can retrieve (0,1,...)
    // Return "-1" if no more items left (or available) to read.
    virtual int CurrentItemNo() const
    {
      return comprot_int( "RDBLib:Result:CurrentItemNo", remoteObj );
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
      int res = comprot_int( "RDBLib:Result:GetImageOrTextDescriptor", remoteObj );
      if(res) {
        return new CRDB_ITDescriptor(res);
      }
      else{
        return NULL;
      }
    }

    // Skip result item
    virtual bool SkipItem()
    {
      return comprot_bool( "RDBLib:Result:SkipItem", remoteObj );
    }

/*
    // Destructor
    virtual ~CDB_Result();
private:
    I_Result* m_Res;

    // The constructor should be called by "I_***Cmd" only!
    friend class CDB_BaseEnt;
    CDB_Result(I_Result* r);

    // Prohibit default- and copy- constructors, and assignment
    CDB_Result& operator= (const CDB_Result&);
    CDB_Result(const CDB_Result&);
    CDB_Result();
*/
};


END_NCBI_SCOPE


#endif  /* DBAPI_DRIVER_RDBLIB___INTERFACES__HPP */



/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2002/03/14 20:00:41  sapojnik
 * A driver that communicates with a dbapi driver on another machine via CompactProtocol(aka ssssrv)
 *
 * Revision 1.1  2002/02/13 20:41:18  sapojnik
 * Remote dblib: MS SQL plus sss comprot
 *
 * ===========================================================================
 */
