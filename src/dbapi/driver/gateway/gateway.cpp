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

CDB_Result* CGW_RPCCmd::Result()
{
  int remoteResult = comprot_int( "GWLib:RPCCmd:Result", remoteObj );
  if(remoteResult) {
    CGW_Result* p = new CGW_Result(remoteResult);
    return Create_Result( *p );
  }
  else {
    return NULL;
  }
}

CDB_Result* CGW_LangCmd::Result()
{
  int remoteResult = comprot_int( "GWLib:LangCmd:Result", remoteObj );
  if(remoteResult) {
    CGW_Result* p = new CGW_Result(remoteResult);
    return Create_Result( *p );
  }
  else {
    return NULL;
  }
}

CDB_Object* read_CDB_Object(CDB_Object* obj)
{
  //// EDB_Type, construction, IsNULL
  IGate* pGate = conn->getProtocol();
  int type;
  int size=-1;
  if (pGate->get_input_arg("type", &type) != IGate::eGood) {
    comprot_errmsg();
    return 0;
  }
  if (pGate->get_input_arg("size", &size) != IGate::eGood) {
    //comprot_errmsg();
    //return 0;
  }

  if( obj==NULL ) {
    obj = CDB_Object::Create((EDB_Type)type,size);
    if (obj==NULL ) {
      string msg = "Invalid CDB_Object type: ";
      msg+= NStr::IntToString(type);
      msg+= "\n";
      conn->setErrMsg( msg.c_str() );
      return 0;
    }
  }
  else if( type!=obj->GetType() ) {
    // We can check for exact match without worrying about compatible types
    // (e.g. Int and SmallInt) because the server side provides any valid
    // conversions to the type the user wants.
    string msg = "Returned and requested CDB_Object types do not match: ";
    msg+= NStr::IntToString( type );
    msg+= " != ";
    msg+= NStr::IntToString( obj->GetType() );
    msg+= "\n";
    conn->setErrMsg( msg.c_str() );
    return 0;
  }

  int isNull=0;
  if( pGate->get_input_arg("null", &isNull) != IGate::eGood ) {
    // comprot_errmsg();
    // return 0;
  }
  else if(isNull) {
    obj->AssignNULL();
    return obj;
  }

  //// Recieve the type-specific value of CDB_Object
  switch( (EDB_Type)type ) {
    case eDB_Int:
    {
      int val;
      if (pGate->get_input_arg("value", &val) != IGate::eGood) {
        comprot_errmsg();
        return 0;
      }
      *((CDB_Int*)obj) = val;
      break;
    }

    case eDB_SmallInt:
    {
      int val;
      if (pGate->get_input_arg("value", &val) != IGate::eGood) {
        comprot_errmsg();
        return 0;
      }
      *((CDB_SmallInt*)obj) = val;
      break;
    }

    case eDB_TinyInt:
    {
      int val;
      if (pGate->get_input_arg("value", &val) != IGate::eGood) {
        comprot_errmsg();
        return 0;
      }
      *((CDB_TinyInt*)obj) = val;
      break;
    }

    // case eDB_BigInt:

    case eDB_VarChar:
    {
      const char* str;
      int len;
      if( pGate->get_input_arg("value", &str, (unsigned*)&len) != IGate::eGood ) {
        comprot_errmsg();
        return 0;
      }
      ((CDB_VarChar*)obj)->SetValue(str,len);
      break;
    }
    case eDB_Char:
    {
      const char* str;
      int len;
      if (pGate->get_input_arg("value", &str, (unsigned*)&len) != IGate::eGood) {
        comprot_errmsg();
        return 0;
      }
      ((CDB_Char*)obj)->SetValue(str,len);
      break;
    }

    /*
    case eDB_VarBinary    :
    case eDB_Binary       :
    case eDB_Float        :
    case eDB_Double       :
    */
    case eDB_DateTime:
    {
      int val, sec;
      if (pGate->get_input_arg("value", &val) != IGate::eGood) {
        comprot_errmsg();
        return 0;
      }
      if (pGate->get_input_arg("secs", &sec) != IGate::eGood) {
        comprot_errmsg();
        return 0;
      }

      // cerr << "Days=" << val << " 300Secs=" << sec << "\n";

      ((CDB_DateTime*)obj)->Assign(val,sec);
      break;
    }
    /*
    case eDB_SmallDateTime:
    case eDB_Text         :
    case eDB_Image        :
    case eDB_Bit          :
    case eDB_Numeric      :
    */

  }
  return obj;

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

  return read_CDB_Object(item_buf);
}

// A client callback invoked by comprot server from "GWLib:Result:ReadItem"
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
  /*
  int i=-1;
  if(is_null) {
    if( pGate->get_input_arg("is_null", &i) != IGate::eGood) {
      comprot_errmsg();
      return 0;
    }
    *is_null = i;
  }
  cerr << "  is_null==" << i << "\n";

  int size=0;
  if( pGate->get_input_arg("size", &size) != IGate::eGood ) {
    comprot_errmsg();
    return 0;
  }
  cerr << "  size=" << size << "\n";

  if( pGate->get_input_arg("value", (char*)buffer, buffer_size ) != IGate::eGood ) {
    comprot_errmsg();
    return 0;
  }
  cerr << "  value==" << buffer << "\n";
  return size;
  */
}

CGWContext::CGWContext(CSSSConnection& sssConnection)
{
  conn = &sssConnection;

  CProcBinCli* pBin = conn->getProcBin();
  pBin->regProc( new C_RDBLib_Result_ReadItem() );

  DBINT version = DBVERSION_UNKNOWN;
  remoteObj = comprot_int1( "GWLib:Context:new", 0, (int*)&version );
}

END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
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
