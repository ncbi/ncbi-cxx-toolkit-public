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
 *   A comprot client proxies for remote:
 *   I_Connection objects.
 *
 */

#include <dbapi/driver/gateway/interfaces.hpp>
// #include <dbapi/driver/util/numeric_convert.hpp>

#include "../../rdblib/common/cdb_object_send_read.hpp"


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
//  CGWContext::
//

CDB_Connection* CGWContext::Connect(
  const string& srv_name, const string& user_name, const string& passwd,
  TConnectionMode mode, bool reusable, const string&   pool_name)
{
  IGate* pGate = conn->getProtocol();
  pGate->set_RPC_call  ( "GWLib:Context:Connect" );
  pGate->set_output_arg( "object"   , &remoteObj   );
  pGate->set_output_arg( "srv_name" , srv_name .c_str() );
  pGate->set_output_arg( "user_name", user_name.c_str() );
  pGate->set_output_arg( "passwd"   , passwd   .c_str() );
  pGate->set_output_arg( "mode"     , (int*)&mode       );
  pGate->set_output_arg( "reusable" , (int*)&reusable   );
  pGate->set_output_arg( "pool_name", pool_name.c_str() );
  pGate->send_data();

  // cerr << "CGWContext::Connect\n";

  int res;
  if( pGate->get_input_arg("result", &res) != IGate::eGood ) {
    comprot_errmsg();
    return NULL;
  }

  if(res) {
    CGW_Connection* p = new CGW_Connection(this,res);
    return Create_Connection( *p );
  }
  else{
    return NULL;
  }
}

CDB_RPCCmd* CGW_Connection::RPC( const string&  rpc_name, unsigned int nof_args )
{
  int cmdObj = comprot_int2(
    "GWLib:Connection:RPC", remoteObj,
    "rpc_name", rpc_name.c_str(),
    "nof_args", (int*)&nof_args);

  if(cmdObj) {
    CGW_RPCCmd* p = new CGW_RPCCmd(this, cmdObj);
    return Create_RPCCmd( *p );
  }
  else{
    return NULL;
  }
}

CDB_LangCmd* CGW_Connection::LangCmd( const string& lang_query, unsigned int nof_params)
{
  int cmdObj = comprot_int2(
    "GWLib:Connection:LangCmd", remoteObj,
    "lang_query", lang_query.c_str(),
    "nof_params", (int*)&nof_params);

  if(cmdObj) {
    CGW_LangCmd* p = new CGW_LangCmd(this, cmdObj);
    return Create_LangCmd( *p );
  }
  else{
    return NULL;
  }
}

CDB_BCPInCmd* CGW_Connection::BCPIn(const string&  table_name, unsigned int   nof_columns)
{
  int cmdObj = comprot_int2(
    "GWLib:Connection:BCPIn", remoteObj,
    "table_name", table_name.c_str(),
    "nof_columns", (int*)&nof_columns);

  if(cmdObj) {
    CGW_BCPInCmd* p = new CGW_BCPInCmd(this, cmdObj);
    return Create_BCPInCmd( *p );
  }
  else{
    return NULL;
  }
}

CDB_CursorCmd* CGW_Connection::Cursor( const string& cursor_name, const string& query,
  unsigned int nof_params, unsigned int batch_size)
{
  IGate* pGate = conn->getProtocol();
  pGate->set_RPC_call  ( "GWLib:Connection:Cursor" );
  pGate->set_output_arg( "object"     , &remoteObj          );
  pGate->set_output_arg( "cursor_name", cursor_name.c_str() );
  pGate->set_output_arg( "query"      , query.c_str()       );
  pGate->set_output_arg( "nof_params" , (int*)&nof_params   );
  pGate->set_output_arg( "batch_size" , (int*)&batch_size   );
  pGate->send_data();

  int res;
  if( pGate->get_input_arg("result", &res) != IGate::eGood ) {
    comprot_errmsg();
    return NULL;
  }

  if(res) {
    CGW_CursorCmd* p = new CGW_CursorCmd(this,res);
    return Create_CursorCmd( *p );
  }
  else{
    return NULL;
  }

}

CDB_SendDataCmd* CGW_Connection::SendDataCmd(
  I_ITDescriptor& desc, size_t data_size, bool log_it)
{
  IGate* pGate = conn->getProtocol();
  pGate->set_RPC_call  ( "GWLib:Connection:SendDataCmd" );
  pGate->set_output_arg( "object"    , &remoteObj        );
  CGW_ITDescriptor* gwDesc = dynamic_cast<CGW_ITDescriptor*>(&desc);
  if(gwDesc==NULL) {
    cerr << "Error: dynamic_cast<CGW_ITDescriptor*> returns NULL\n";
    return NULL;
  }
  int i = gwDesc->getRemoteObj();
  pGate->set_output_arg( "desc"      , &i               );
  pGate->set_output_arg( "data_size" , (int*)&data_size );
  pGate->set_output_arg( "log_it"    , (int*)&log_it    );
  pGate->send_data();

  int res;
  if( pGate->get_input_arg("result", &res) != IGate::eGood ) {
    comprot_errmsg();
    return NULL;
  }

  if(res) {
    CGW_SendDataCmd* p = new CGW_SendDataCmd(this,res);
    return Create_SendDataCmd( *p );
  }
  else{
    return NULL;
  }
}

const string& CGW_Connection::ServerName() const
{
  char buf[1024];
  char* p = comprot_chars(
      "GWLib:Connection:ServerName", remoteObj,
      buf, sizeof(buf)-1 );
  vars->m_Server = p?p:"";
  return vars->m_Server;
}
const string& CGW_Connection::UserName()   const
{
  char buf[1024];
  char* p = comprot_chars(
      "GWLib:Connection:UserName", remoteObj,
      buf, sizeof(buf)-1 );
  vars->m_User =  p?p:"";
  return vars->m_User;
}
const string& CGW_Connection::Password()   const
{
  char buf[1024];
  char* p = comprot_chars(
      "GWLib:Connection:Password", remoteObj,
      buf, sizeof(buf)-1 );
  vars->m_Password =  p?p:"";
  return vars->m_Password;
}
const string& CGW_Connection::PoolName()   const
{
  char buf[1024];
  char* p = comprot_chars(
      "GWLib:Connection:PoolName", remoteObj,
      buf, sizeof(buf)-1 );
  vars->m_PoolName =  p?p:"";
  return vars->m_PoolName;
}

/*
void CGW_Connection::DropCmd(CDB_BaseEnt& cmd)
{
  CGW_Base* p = dynamic_cast<CGW_Base*>(&cmd);
  if(p==NULL) {
    cerr << "Error: dynamic_cast<CGW_Base*> returns NULL\n";
    return;
  }

  comprot_void("GWLib:Connection:DropCmd",p->remoteObj);
  // << also delete local proxy object?? >>
}
*/

CDB_Result* CGW_BaseCmd::Result()
{
  int remoteResult = comprot_int( "GWLib:BaseCmd:Result", remoteObj );
  if(remoteResult) {
    CGW_Result* p = new CGW_Result(remoteResult);
    return Create_Result( *p );
  }
  else {
    return NULL;
  }
}


/* A more complex function than the rest:
 * needs to handle all possible EDB_Type-s and memory allocation.
 */
CDB_Object* CGW_Result::GetItem(CDB_Object* item_buf)
{
  // Send the type and size to server (if they are available),
  // so that an appropriate receiving object might be created.
  int type = -1;
  int size = -1;
  if( item_buf!=NULL ) {
    type = (int)( item_buf->GetType() );

    CDB_Binary* objBinary = dynamic_cast<CDB_Binary*>(item_buf);
    if(objBinary) {
      size = objBinary->Size();
    }
    else {
      CDB_Char* objChar = dynamic_cast<CDB_Char*>(item_buf);
      if(objChar) {
        size = objChar->Size();
      }
    }
  }

  IGate* pGate = conn->getProtocol();
  pGate->set_RPC_call("GWLib:Result:GetItem");
  pGate->set_output_arg( "object", &remoteObj );
  pGate->set_output_arg( "type"  , &type      );
  if(size>=0) {
    pGate->set_output_arg( "size"  , &size      );
  }
  pGate->send_data();

  return read_CDB_Object(pGate,item_buf);
}

// A client callback invoked by comprot server from GWLib:Result:ReadItem
// May be called multiple times ( once on each send_data() ), to transfer
// smaller chunks of blob data. This allows to save both client and server memory.
class C_RDBLib_Result_ReadItem : public CRegProcCli
{
public:
  // In/out args to be passed to exec() from CGW_Result::ReadItem() via
  // IGate::set/getUserData().
  struct SContext {
    // In
    char* buffer;
    bool* is_null;
    size_t buffer_size;
    // Out
    bool bError;
    size_t bytesReceived;

    SContext(char* buffer_arg, size_t buffer_size_arg, bool* is_null_arg=NULL)
    {
      buffer      = buffer_arg;
      buffer_size = buffer_size_arg;
      is_null     = is_null_arg;

      bError=false;
      bytesReceived=0;
    }
  };
  C_RDBLib_Result_ReadItem() : CRegProcCli("GWLib:Result:ReadItem") {}

  virtual bool begin(IGate*) { return false; }
  virtual void end(IGate*, bool) {}
  virtual void exec(IGate* pGate)
  {
    // cerr << "C_RDBLib_Result_ReadItem::exec()\n";

    SContext* pContext = (SContext*) pGate->get_user_data();

    if(pContext->is_null) {
      int i=0;
	  if(  pGate->get_input_arg( "is_null", &i ) != IGate::eGood  ) {
        pContext->bError = true;
        pGate->send_cancel();
        return;
      }
      *(pContext->is_null) = i;
      // cerr << "  isNull=" << i << "\n";
    }

    unsigned chunkSize;
    char* chunkData;
    if( pGate->take_bin_arg("value", (void**)&chunkData, &chunkSize ) != IGate::eGood  ) {
        pContext->bError = true;
        pGate->send_cancel();
        return;
    }
    // cerr << "  chunkSize=" << chunkSize << "\n";

    if( pContext->bytesReceived+chunkSize <= pContext->buffer_size ) {
      memcpy(pContext->buffer+pContext->bytesReceived,chunkData,chunkSize);
      pContext->bytesReceived+=chunkSize;
      delete chunkData;
      pGate->send_done();
      // cerr << "  send_done()\n";
    }
    else{
      pContext->bError = true;
      delete chunkData;
      pGate->send_cancel();
      // cerr << "  send_cancel()\n";
    }
  }
};


size_t CGW_Result::ReadItem(void* buffer, size_t buffer_size, bool* is_null)
{
  // cerr << "CGW_Result::ReadItem\n";
  IGate* pGate = conn->getProtocol();

  C_RDBLib_Result_ReadItem::SContext callback_args( (char*)buffer, buffer_size, is_null );
  void* pOldUserData = pGate->get_user_data();
  pGate->set_user_data(&callback_args);

  pGate->set_RPC_call("GWLib:Result:ReadItem");
  pGate->set_output_arg( "object",       &remoteObj   );
  pGate->set_output_arg( "size"  , (int*)&buffer_size );
  pGate->send_data();
  pGate->set_user_data(pOldUserData);

  if( callback_args.bError ) {
    comprot_errmsg();
    return 0;
  }
  return callback_args.bytesReceived;
}

CGWContext::CGWContext(CSSSConnection& sssConnection)
{
  conn = &sssConnection;

  CProcBinCli* pBin = conn->getProcBin();
  pBin->regProc( new C_RDBLib_Result_ReadItem() );

  //DBINT version = DBVERSION_UNKNOWN; <-- 'version' is a rudiment from dblib
  int version = 0;
  remoteObj = comprot_int1( "GWLib:Context:new", 0, (int*)&version );
}

END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2002/03/26 17:58:06  sapojnik
 * Functions for sending/receiving CDB_Object-s moved to new gateway_common.lib
 *
 * Revision 1.4  2002/03/19 21:45:10  sapojnik
 * some small bugs fixed after testing - the ones related to Connection:SendDataCmd,DropCmd; BaseCmd:Send
 *
 * Revision 1.3  2002/03/15 22:01:45  sapojnik
 * more methods and classes
 *
 * Revision 1.2  2002/03/14 22:53:22  sapojnik
 * Inheriting from I_ interfaces instead of CDB_ classes from driver/public.hpp
 *
 * Revision 1.1  2002/03/14 20:00:41  sapojnik
 * A driver that communicates with a dbapi driver on another machine via CompactProtocol(aka ssssrv)
 *
 * Revision 1.2  2002/02/15 22:01:01  sapojnik
 * uses CDB_Object::create() instead of new_CDB_Object()
 *
 * Revision 1.1  2002/02/13 20:41:18  sapojnik
 * Remote dblib: MS SQL plus sss comprot
 *
 *
 * ===========================================================================
 */
