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
 *   Support for various Bind*() and Send*() methods in CGW_*Cmd classes.
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

// Assign value to already existing remote CDB_Object
bool assign_CDB_Object(int remoteObj, CDB_Object* localObj)
{
  IGate* pGate = conn->getProtocol();
  pGate->set_RPC_call( "GWLib:Object:AssignValue" );
  pGate->set_output_arg( "object", &remoteObj);

  // cerr << "assign_CDB_Object ( remote=" << remoteObj;
  // cerr << " <= local=" << localObj << " )\n";

  send_CDB_Object(pGate, localObj);
  pGate->send_done();

  int res;
  if( pGate->get_input_arg("result", &res) != IGate::eGood ) {
    comprot_errmsg();
    return false;
  }
  return res;
}

CRLObjPairs* CGW_Base::getBoundObjects(CRLObjPairs** ppVector)
{
  if(*ppVector==NULL) {
    *ppVector=new CRLObjPairs();
  }
  return *ppVector;
}

/* The common part of various Bind*() commands.
 * Initiates creation of empty CDB_Object on server;
 * gets back its id;
 * stores it paired with localObj in boundObjects vector.
 */

bool CGW_Base::xBind(IGate* pGate, CDB_Object* localObj, CRLObjPairs** boundObjects)
{
  pGate->set_output_arg( "object", &remoteObj);
  send_CDB_ObjTS(pGate, localObj);

  int remoteDataObj;
  if( pGate->get_input_arg("cdb_object", &remoteDataObj) != IGate::eGood ) {
    comprot_errmsg();
    return false;
  }
  getBoundObjects(boundObjects)->addObjPair(remoteDataObj, localObj);

  int res;
  if( pGate->get_input_arg("result", &res) != IGate::eGood ) {
    comprot_errmsg();
    return false;
  }

  return res;
}

bool CGW_BaseCmd::Send()
{
  return xSend("GWLib:BaseCmd:Send", boundObjects);
}

int CGW_Base::xSend(const char* rpc_name, CRLObjPairs* boundObjects)
{
  IGate* pGate = conn->getProtocol();

  // Send to server the values of all boundObjects (except CDB_Text/Image)
  if(boundObjects) {
    boundObjects->updateRemoteObjs();
  }

  // May need a callback to handle CDB_Text/Image
  void* pOldUserData = pGate->get_user_data();
  C_GWLib_TextImageCallback::SContext callback_args;
  pGate->set_user_data((void*)&callback_args);

  /* Change mode to ensure that nested callbacks work properly:
     cli: CGW_BCPInCmd::SendRow()
     srv:   CGW_Stream_Read()
     cli:     C_GWLib_TextImageCallback()
     srv:       CGW_Stream_DataReceiver::procEvent()
  */
  IGate::ETransmissionMode oldMode=pGate->get_transmission_mode();
  pGate->set_no_bulk();

  pGate->set_RPC_call  ( rpc_name );
  pGate->set_output_arg( "object", &remoteObj     );
  pGate->send_data();
  pGate->send_done();

  pGate->set_user_data(pOldUserData);
  pGate->set_transmission_mode(oldMode);

  if( callback_args.bError ) {
    comprot_errmsg();
    return false;
  }
  return callback_args.result;
}

bool CGW_BCPInCmd::Cancel()
{
  if(boundObjects) {
    boundObjects->deleteRemoteObjects();
  }
  return comprot_bool( "GWLib:BCPInCmd:Cancel", remoteObj );
}

bool CGW_BaseCmd::Cancel()
{
  if(boundObjects) {
    boundObjects->deleteRemoteObjects();
  }
  return comprot_bool( "GWLib:BaseCmd:Cancel", remoteObj );
}


bool CGW_LangCmd::BindParam(const string& param_name, CDB_Object* param_ptr)
{
  IGate* pGate = conn->getProtocol();
  pGate->set_RPC_call( "GWLib:LangCmd:BindParam" );
  pGate->set_output_arg( "param_name", param_name.c_str() );
  return xBind(pGate, param_ptr, &boundObjects);
}

bool CGW_RPCCmd::BindParam(const string& param_name, CDB_Object* param_ptr, bool out_param)
{
  IGate* pGate = conn->getProtocol();
  int i = out_param;
  pGate->set_RPC_call( "GWLib:RPCCmd:BindParam" );
  pGate->set_output_arg( "param_name", param_name.c_str() );
  pGate->set_output_arg( "out_param", &i );
  return xBind(pGate, param_ptr, &boundObjects);
}

bool CGW_CursorCmd::BindParam(const string& param_name, CDB_Object* param_ptr)
{
  IGate* pGate = conn->getProtocol();
  pGate->set_RPC_call( "GWLib:CursorCmd:BindParam" );
  pGate->set_output_arg( "param_name", param_name.c_str() );
  return xBind(pGate, param_ptr, &boundObjects);
}

CDB_Result* CGW_CursorCmd::Open()
{
  int remoteResult = xSend( "GWLib:CursorCmd:Open", boundObjects );
  if(remoteResult) {
    CGW_Result* p = new CGW_Result(remoteResult);
    return Create_Result( *p );
  }
  else {
    return NULL;
  }
}

bool CGW_BCPInCmd::Bind(unsigned int column_num, CDB_Object* localObj)
{
  IGate* pGate = conn->getProtocol();
  pGate->set_RPC_call( "GWLib:BCPInCmd:Bind" );
  pGate->set_output_arg( "column_num", (int*)&column_num );
  return xBind(pGate, localObj, &boundObjects);
}

bool CGW_BCPInCmd::SendRow()
{
  return xSend("GWLib:BCPInCmd:SendRow", boundObjects);
}

bool CGW_BCPInCmd::CompleteBCP()
{
  if(boundObjects) {
    boundObjects->deleteRemoteObjects();
  }
  return comprot_bool( "GWLib:BCPInCmd:CompleteBCP", remoteObj );
}

/////////////////////////////////////////////////////////////////////////
//// Bound CDB_Object support (except CDB_Stream-s ?)
/////////////////////////////////////////////////////////////////////////

int CRLObjPairs::updateRemoteObjs()
{
  int imageTextCount = 0;
  for( iterator it = begin(); it != end(); ++it) {
    CDB_Object* localObj = it->second;
    EDB_Type localType = localObj->GetType();
    if( localType == eDB_Text || localType == eDB_Image )
		imageTextCount++;
    if( !assign_CDB_Object(it->first, localObj) ) {
		return -1;
	}
  }

  return imageTextCount;
}

void CRLObjPairs::deleteRemoteObjects()
{
  IGate* pGate = conn->getProtocol();
  for( iterator it = begin(); it != end(); ++it) {
    int remoteObj = it->first;
    pGate->set_RPC_call( "GWLib:Object:delete" );
    pGate->set_output_arg( "object", &remoteObj);
    pGate->send_data();
  }
  clear();
}


CGW_BaseCmd::~CGW_BaseCmd()
{
  if(boundObjects) {
    delete boundObjects; // this also invokes CRLObjPairs::deleteRemoteObjects();
                          // which may need to communicate with server
    boundObjects=0;
  }
}

CGW_BCPInCmd::~CGW_BCPInCmd()
{
  if(boundObjects) {
    delete boundObjects; // this also invokes CRLObjPairs::deleteRemoteObjects();
                          // which may need to communicate with server
    boundObjects=0;
  }
}

CGW_CursorCmd::~CGW_CursorCmd()
{
  if(boundObjects) {
    delete boundObjects; // this also invokes CRLObjPairs::deleteRemoteObjects();
                          // which may need to communicate with server
    boundObjects=0;
  }
}

bool CGW_CursorCmd::Close()
{
  if(boundObjects) {
    boundObjects->deleteRemoteObjects();
  }
  return comprot_bool("GWLib:CursorCmd:Close", remoteObj);
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2004/05/17 21:14:35  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.1  2003/05/19 21:51:51  sapojnik
 * Client portion of gateway driver back in C++ tree - now assembles as dll, and runs on Sparc Solaris
 *
 * Revision 1.5  2003/05/05 21:51:56  sapojnik
 * CGW_CursorCmd::Open()
 *
 * Revision 1.4  2003/05/05 14:27:16  sapojnik
 * CGW_Base::xBind(),xSend() and boundObjects member in classes with Send/Bind()
 *
 * Revision 1.3  2003/04/22 17:02:17  sapojnik
 * bugfix: GWLib:Object:delete separate from GWLib:Base:delete
 *
 * Revision 1.2  2003/03/03 22:02:16  sapojnik
 * CGW_BCPInCmd finally debugged
 *
 * Revision 1.1  2003/02/21 20:10:25  sapojnik
 * many changes, mostly implementing and debugging BCP Bind/SendRow() (not completely debugged yet)
 *
 *
 * ===========================================================================
 */
