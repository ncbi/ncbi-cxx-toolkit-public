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
 *   A comprot client proxies for remote I_Connection objects (CGW_Connection)
 *   and internal gateway functions/classes (sendITDescriptor, C_GWLib_TextImageCallback,
 *   C_GWLib_ObjectGetter).
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/gateway/internal_cli.hpp>

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
  int iReusable = (int)reusable;
  pGate->set_output_arg( "reusable" , &iReusable   );
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

bool sendITDescriptor(IGate* pGate, I_ITDescriptor& desc)
{
  int i = 0;

  CGW_ITDescriptor* gwDesc = dynamic_cast<CGW_ITDescriptor*>(&desc);
  if(gwDesc==NULL) {
    CDB_ITDescriptor* cdbDesc = dynamic_cast<CDB_ITDescriptor*>(&desc);
    if(cdbDesc==NULL) {
      cerr << "Error: dynamic_cast(s)<CGW/CDB_ITDescriptor*> both return NULL\n";
      return false;
    }
    else {
      pGate->set_output_arg( "desc_table" , cdbDesc->TableName       ().c_str() );
      pGate->set_output_arg( "desc_column", cdbDesc->ColumnName      ().c_str() );
      pGate->set_output_arg( "desc_where" , cdbDesc->SearchConditions().c_str() );
    }
  }
  else {
    i = gwDesc->getRemoteObj();
  }

  pGate->set_output_arg( "desc", &i );
  pGate->send_data();
  return true;
}

CDB_SendDataCmd* CGW_Connection::SendDataCmd(
  I_ITDescriptor& desc, size_t data_size, bool log_it)
{
  IGate* pGate = conn->getProtocol();
  pGate->set_RPC_call  ( "GWLib:Connection:SendDataCmd" );
  pGate->set_output_arg( "object"    , &remoteObj       );

  pGate->set_output_arg( "data_size" , (int*)&data_size );
  pGate->set_output_arg( "log_it"    , (int*)&log_it    );
  if( !sendITDescriptor(pGate, desc) ) return NULL; // invokes pGate->send_data();

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

void C_GWLib_TextImageCallback::exec(IGate* pGate)
{
  SContext* pContext = (SContext*) pGate->get_user_data();
  CDB_Stream* pStream; //= pContext->pStream;

  int i=0;
  if( pGate->get_input_arg( "cmd", &i ) != IGate::eGood  ) {
    cerr << "  cannot get_input_arg(\"cmd\")\n";
    pContext->bError = true;

    const char* pchAnswer;
    unsigned sz;
    if( pGate->get_input_arg("Error", &pchAnswer, &sz) == IGate::eGood ) {
      cerr << "  C_GWLib_TextImageCallback::exec comprot_errmsg=" << pchAnswer << "\n";
    }
    else {
      cerr << "  C_GWLib_TextImageCallback::exec comprot_errmsg - no Error message from server\n";
    }

    pGate->send_cancel();
    return;
  }

  void* mem = 0;
  switch(i){
    case CDB_Stream_Read: // void* buff, size_t nof_bytes
    {
      if( pGate->get_input_arg( "nof_bytes", &i          ) != IGate::eGood || i<0 ||
          pGate->get_input_arg( "object", (int*)&pStream ) != IGate::eGood || pStream==NULL )
      {
        pGate->send_cancel();
        pContext->bError = true;
        return;
      }

      mem = new char[i+1];
      size_t sz = pStream->Read(mem,i);

      if( sz>i ) {
        // Probably, an error flag
        string sTmp = NStr::IntToString(sz);
        pGate->send_message( 0, "CGW_Stream:Read_", sTmp.c_str(), sTmp.size()+1  );
      }
      else{
        pGate->send_message( 0, "CGW_Stream:Read" , (const char*)mem, sz );
      }
    }
    break;

    case 0: // server function finished
      if( pGate->get_input_arg( "result", &i ) != IGate::eGood  ) {
        pContext->bError = true;
        pGate->send_cancel();
        return;
      }
      pContext->result=i;
      break;

    default:
      pContext->bError = true;
      pGate->send_cancel();
      return;
  }
  pGate->send_done();

  if(mem) {
    delete mem;
  }
}

bool CGW_Connection::xSendData(I_ITDescriptor& desc, CDB_Stream* data, bool log_it)
{
  IGate* pGate = conn->getProtocol();
  void* pOldUserData = pGate->get_user_data();
  C_GWLib_TextImageCallback::SContext callback_args; //(data);
  pGate->set_user_data((void*)&callback_args);

  pGate->set_RPC_call  ( "GWLib:Connection:SendData" );
  pGate->set_output_arg( "object"    , &remoteObj    );

  int type = (int)data->GetType();
  int sz   = (int)data->Size();
  pGate->set_output_arg( "type", &type);
  pGate->set_output_arg( "size", &sz);
  pGate->set_output_arg( "stream", (int*)&data);

  pGate->set_output_arg( "log_it"    , (int*)&log_it    );
  if( !sendITDescriptor(pGate, desc) ) return NULL; // invokes pGate->send_data();

  int res;
  if( pGate->get_input_arg("result", &res) != IGate::eGood ) {
    comprot_errmsg();
    return false;
  }

  pGate->set_user_data(pOldUserData);
  if( callback_args.bError ) {
    comprot_errmsg();
    return false;
  }
  return callback_args.result;
}

bool CGW_Connection::SendData(I_ITDescriptor& desc, CDB_Image& data, bool log_it)
{
  return xSendData(desc, &data, log_it);
}

bool CGW_Connection::SendData(I_ITDescriptor& desc, CDB_Text&  data, bool log_it)
{
  return xSendData(desc, &data, log_it);
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

void C_GWLib_Result_ReadItem::exec(IGate* pGate)
{
  SContext* pContext = (SContext*) pGate->get_user_data();
  //cerr << "C_GWLib_Result_ReadItem::exec() pContext==" << (int)pContext << "\n";

  if(pContext->is_null) {
    int i=0;
    if(  pGate->get_input_arg( "is_null", &i ) != IGate::eGood  ) {
      pContext->bError = true;
      pGate->send_cancel();
      return;
    }
    *(pContext->is_null) = i;
  }

  unsigned chunkSize;
  char* chunkData;
  if( pGate->take_bin_arg("value", (void**)&chunkData, &chunkSize ) != IGate::eGood  ) {
      pContext->bError = true;
      pGate->send_cancel();
      return;
  }

  /*
  if(pContext->stream) {
    stream->Append( (const void*)chunkData, (size_t)chunkSize );
    delete chunkData;
    pGate->send_done();
  }
  else{
  */
    if( pContext->bytesReceived+chunkSize <= pContext->buffer_size ) {
      memcpy(pContext->buffer+pContext->bytesReceived,chunkData,chunkSize);
      pContext->bytesReceived+=chunkSize;
      delete chunkData;
      pGate->send_done();
    }
    else{
      pContext->bError = true;
      delete chunkData;
      pGate->send_cancel();
    }
  //}
}


void C_GWLib_ObjectGetter::exec(IGate* pGate)
{
  SContext* pContext = (SContext*) pGate->get_user_data();

  if(!pContext->bStreamAppend) {
    int type;
    if (pGate->get_input_arg("type", &type) != IGate::eGood) {
      cerr << "C_GWLib_ObjectGetter::exec() - cannot get_input_arg(type)\n";
      pContext->bError = true;
      pGate->send_cancel();
      return;
    }

    if( (EDB_Type)type!=eDB_Text && (EDB_Type)type!=eDB_Image ) {
      // Regular small object - we are finished in one call.
      pContext->obj = read_CDB_Object(pGate, pContext->obj, type);
      pGate->send_done();
      return;
    }

    // Prepare the stream for receiving data
    if(pContext->obj) {
      ((CDB_Stream*)pContext->obj)->Truncate(0);
    }
    else{
      pContext->obj = CDB_Object::Create((EDB_Type)type);
    }

    int isNull;
    if(pGate->get_input_arg("is_null", &isNull) == IGate::eGood && isNull) {
      // Empty stream - we are finished in one call.
      pGate->send_done();
      return;
    }

    pContext->bStreamAppend=true;
  }

  // Receive a chunk of stream data
  unsigned chunkSize;
  char* chunkData;
  if( pGate->take_bin_arg("value", (void**)&chunkData, &chunkSize ) != IGate::eGood  ) {
    cerr << "C_GWLib_ObjectGetter::exec() - cannot take_bin_arg(value)\n";
    pContext->bError = true;
    pGate->send_cancel();
    return;
  }

  ((CDB_Stream*)pContext->obj)->Append( (const void*)chunkData, (size_t)chunkSize );
  delete chunkData;
  pGate->send_done();
}

void C_GWLib_MsgCallback::exec(IGate* pGate)
{
  CGW_Connection* pConnection;
  if( pGate->get_input_arg("connection", (int*)&pConnection) != IGate::eGood ) {
    cerr << "C_GWLib_MsgCallback::exec() - cannot get_input_arg(connection)\n";
  }
  else {
    CDB_Exception* ex = read_CDB_Exception(pGate);
    pConnection->m_MsgHandlers.PostMsg(ex);
    delete ex;
  }
  pGate->send_done();
}

void CGW_Connection::PushMsgHandler(CDB_UserHandler* h)
{
  m_MsgHandlers.Push(h);

  // Make sure there is a server-side handler listening to messages
  int i=(int)this;
  comprot_void1("GWLib:Connection:MsgHandler", remoteObj, &i);
}


void CGW_Connection::PopMsgHandler(CDB_UserHandler* h)
{
  m_MsgHandlers.Pop(h);
}

bool CGW_Connection::Abort()
{
  return false;
}


END_NCBI_SCOPE

