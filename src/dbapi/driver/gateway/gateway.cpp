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

#include <ncbi_pch.hpp>
#include <dbapi/driver/gateway/internal_cli.hpp>

#ifdef NCBI_OS_MSWIN
#define DllExport   __declspec( dllexport )
#else
#define DllExport
#endif

BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////////////////////
// Driver manager related functions
//

I_DriverContext* GW_CreateContext(map<string,string>* attr = 0)
{
  static CLogger *log;
  static CSSSConnection *sssConnection=NULL;
  if(sssConnection==NULL) {
    log = new CLogger(&cerr); // Do it better??
    sssConnection = new CSSSConnection(log);
  }

  // To do: read defaults from ssssrv.ini
  string sHost = "stmartin";
  unsigned short iPort = 8765;

  if(attr) {
    map<string,string>::iterator it;
    it=attr->find("host"  ); if( it!=attr->end() ) { sHost  =it->second; }
    it=attr->find("port"  ); if( it!=attr->end() ) {
      iPort  = NStr::StringToUInt(it->second);
	}
  }

  if( sssConnection->connect(sHost.c_str(), iPort) != CSSSConnection::eOk ) {
      cerr<< "FATAL(" << sssConnection->getRetCodeDesc()
          << "):Failed to connect to SSS server on host:\""
          << sHost << "\" port:" << iPort << endl
          << "\tReason:" << sssConnection->getErrMsg() << endl;
      return NULL;
  }

  return new CGWContext(*sssConnection);
}

void DBAPI_RegisterDriver_GATEWAY(I_DriverMgr& mgr)
{
    mgr.RegisterDriver("gateway", GW_CreateContext);
}

extern "C" {
    DllExport void* DBAPI_E_gateway()
    {
        return (void*)DBAPI_RegisterDriver_GATEWAY;
    }
}

/////////////////////////////////////////////////////////////////////////////
//
//  Commands
//

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

bool CGW_CursorCmd::UpdateTextImage(unsigned int item_num, CDB_Stream& data, bool log_it)
{
  IGate* pGate = conn->getProtocol();
  void* pOldUserData = pGate->get_user_data();
  C_GWLib_TextImageCallback::SContext callback_args; //(&data);
  pGate->set_user_data((void*)&callback_args);

  pGate->set_RPC_call  ( "GWLib:CursorCmd:UpdateTextImage" );
  pGate->set_output_arg( "object"  , &remoteObj      );
  pGate->set_output_arg( "item_num", (int*)&item_num );

  int type = (int)( data.GetType() );
  int sz   = (int)( data.Size   () );
  CDB_Stream* pdata = &data;
  pGate->set_output_arg( "type"    , &type);
  pGate->set_output_arg( "size"    , &sz  );
  pGate->set_output_arg( "stream"  , (int*)&pdata     );

  pGate->set_output_arg( "log_it"  , (int*)&log_it   );
  pGate->send_data();
  //pGate->send_done();
  pGate->set_user_data(pOldUserData);

  if( callback_args.bError ) {
    comprot_errmsg();
    return false;
  }
  return callback_args.result;
}

CDB_SendDataCmd* CGW_CursorCmd::SendDataCmd(unsigned int item_num, size_t size, bool log_it)
{
  IGate* pGate = conn->getProtocol();
  pGate->set_RPC_call  ( "GWLib:CursorCmd:SendDataCmd" );
  pGate->set_output_arg( "object"  , &remoteObj      );
  pGate->set_output_arg( "item_num", (int*)&item_num );
  pGate->set_output_arg( "size"    , (int*)&size     );
  pGate->set_output_arg( "log_it"  , (int*)&log_it   );

  pGate->send_data();

  int res;
  if( pGate->get_input_arg("result", &res) != IGate::eGood ) {
    comprot_errmsg();
    return NULL;
  }

  if(res) {
    CGW_SendDataCmd* p = new CGW_SendDataCmd(con, res);
    return con->Create_SendDataCmd( *p );
  }
  else{
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

  C_GWLib_ObjectGetter::SContext callback_args( item_buf );
  void* pOldUserData = pGate->get_user_data();
  pGate->set_user_data(&callback_args);

  pGate->set_RPC_call("GWLib:Result:GetItem");
  pGate->set_output_arg( "object", &remoteObj );
  pGate->set_output_arg( "type"  , &type      );
  if(size>=0) {
    pGate->set_output_arg( "size"  , &size      );
  }
  pGate->send_data();

  //CDB_Object* obj=read_CDB_Object(pGate,item_buf);
  //return obj;

  pGate->set_user_data(pOldUserData);
  if( callback_args.bError ) {
    comprot_errmsg();
    return 0;
  }
  return callback_args.obj;
}


size_t CGW_Result::ReadItem(void* buffer, size_t buffer_size, bool* is_null)
{
  //cerr << "CGW_Result::ReadItem\n";
  IGate* pGate = conn->getProtocol();

  C_GWLib_Result_ReadItem::SContext callback_args( (char*)buffer, buffer_size, is_null );
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

const char* CGW_Result::ItemName(unsigned int item_num) const
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

CGWContext::CGWContext(CSSSConnection& sssConnection)
{
  conn = &sssConnection;

  CProcBinCli* pBin = conn->getProcBin();
  pBin->regProc( new C_GWLib_Result_ReadItem  () );
  pBin->regProc( new C_GWLib_ObjectGetter     () );
  pBin->regProc( new C_GWLib_TextImageCallback() );
  pBin->regProc( new C_GWLib_MsgCallback      () );

  remoteObj = comprot_int( "GWLib:Context:new", 0 );
}


size_t CGW_SendDataCmd::SendChunk(const void* pChunk, size_t nofBytes)
{
  IGate* pGate = conn->getProtocol();
  pGate->set_RPC_call  ( "GWLib:SendDataCmd:SendChunk" );
  pGate->set_output_arg( "object"  , &remoteObj      );
  pGate->set_output_arg( "data", pChunk, (const unsigned*)&nofBytes );
  pGate->send_data();

  int res;
  if( pGate->get_input_arg("result", &res) != IGate::eGood ) {
    comprot_errmsg();
    return 0;
  }
  return res;
}


bool CGW_LangCmd::SetParam(const string& param_name, CDB_Object* param_ptr)
{
  IGate* pGate = conn->getProtocol();
  pGate->set_RPC_call( "GWLib:LangCmd:SetParam" );
  pGate->set_output_arg( "object"  , &remoteObj      );
  pGate->set_output_arg( "param_name", param_name.c_str() );
  send_CDB_Object(pGate, param_ptr);

  int res;
  if( pGate->get_input_arg("result", &res) != IGate::eGood ) {
    comprot_errmsg();
    return false;
  }
  return res;
}

bool CGW_RPCCmd::SetParam(const string& param_name, CDB_Object* param_ptr, bool out_param)
{
  IGate* pGate = conn->getProtocol();
  int i = out_param;
  pGate->set_RPC_call( "GWLib:RPCCmd:SetParam" );
  pGate->set_output_arg( "object"  , &remoteObj      );
  pGate->set_output_arg( "param_name", param_name.c_str() );
  pGate->set_output_arg( "out_param", &i );
  send_CDB_Object(pGate, param_ptr);

  int res;
  if( pGate->get_input_arg("result", &res) != IGate::eGood ) {
    comprot_errmsg();
    return false;
  }
  return res;
}


bool CGW_LangCmd::Cancel()
{
  return CGW_BaseCmd::Cancel();
}



END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.7  2004/05/17 21:14:35  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.6  2003/05/19 21:51:51  sapojnik
 * Client portion of gateway driver back in C++ tree - now assembles as dll, and runs on Sparc Solaris
 *
 * Revision 1.13  2003/05/12 20:51:34  sapojnik
 * register C_GWLib_MsgCallback
 *
 * Revision 1.12  2003/05/05 21:52:49  sapojnik
 * CGW_CursorCmd::Open() - moved to bind_send.cpp
 *
 * Revision 1.11  2003/05/05 14:33:23  sapojnik
 * CGW_LangCmd::Cancel()
 *
 * Revision 1.10  2003/04/30 14:22:44  sapojnik
 * CGW_CursorCmd::UpdateTextImage() fixed
 *
 * Revision 1.9  2003/04/25 22:36:32  sapojnik
 * CGW_Result::GetItem() can handle CDB_Stream-s; it uses the new C_GWLib_ObjectGetter
 *
 * Revision 1.8  2003/04/22 16:59:31  sapojnik
 * read host, port from the "attr" hash
 *
 * Revision 1.7  2003/03/03 21:57:18  sapojnik
 * debug diagnostic messages commented out
 *
 * Revision 1.6  2003/02/21 20:12:22  sapojnik
 * many changes, mostly implementing and debugging BCP Bind/SendRow() (not completely debugged yet)
 *
 * Revision 1.5  2003/01/08 22:52:26  sapojnik
 * *** empty log message ***
 *
 * Revision 1.4  2003/01/08 22:09:17  sapojnik
 * bugfix in GW_CreateContext - do not delete CSSSConnection on return
 *
 * Revision 1.3  2002/12/03 16:43:02  sapojnik
 * Driver manager related functions
 *
 * Revision 1.2  2002/11/27 21:11:22  sapojnik
 * Bind/SetParam(), SendData(), sending CDB_Float/DoubleO. etc. implemented for various classes, not tested yet; it seems that only CGW_Connection::Push/PopMsgHandler() still remain
 *
 * Revision 1.1  2002/11/26 21:16:09  sapojnik
 * first commit - not guaranteed to compile or to work
 *
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
