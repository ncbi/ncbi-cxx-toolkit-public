#ifndef DBAPI_DRIVER_GATEWAY___INTERNAL_CLI__HPP
#define DBAPI_DRIVER_GATEWAY___INTERNAL_CLI__HPP

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
 *   Functions/classes used inside the implementation of the dbapi gateway client.
 *
 */

#include <dbapi/driver/gateway/interfaces.hpp>
#include <dbgateway/cdb_object_send_read.hpp>
#include <dbgateway/cdb_stream_methods.hpp>
#include <dbapi/driver/util/parameters.hpp>

#ifdef NCBI_OS_MSWIN
#define DllExport   __declspec( dllexport )
#else
#define DllExport
#endif

BEGIN_NCBI_SCOPE

/* Sends either an existing server I_ITDescriptor id ,
 * or 3 strings needed to re-create desriptor from a client CDB_ITDescriptor.
 * See also readITDescriptor().
 */
bool sendITDescriptor(IGate* pGate, I_ITDescriptor& desc);

// A client callback invoked by comprot server from GWLib:Result:UpdateTextImage
// and other RPCs.
// Called multiple times ( once on each send_data() ) in responce to send_data() in:
//   CGW_Text/Image::Read(),
DllExport class C_GWLib_TextImageCallback : public CRegProcCli
{
public:
  C_GWLib_TextImageCallback() : CRegProcCli("GWLib:_TextImageCallback") {}

  struct SContext {
    // In
    // CDB_Stream* pStream;
    // CDB_Params* pParams;
    // Out
    bool bError;
    int result;

    SContext() //(CDB_Stream* pStream_arg, CDB_Params* pParams_arg=NULL)
    {
      // pStream=pStream_arg;
      // pParams=pParams_arg;

      bError=false;
      result=false;
    }
  };

  virtual bool begin(IGate*) { return false; }
  virtual void end(IGate*, bool) {}
  virtual void exec(IGate* pGate);
};

// A client callback invoked by comprot server from GWLib:Result:ReadItem
// May be called multiple times ( once on each send_data() ), to transfer
// smaller chunks of blob data. This allows to save both client and server memory.
DllExport class C_GWLib_Result_ReadItem : public CRegProcCli
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
    // Alternative in/out
    //CDB_Stream* stream;

    SContext(char* buffer_arg, size_t buffer_size_arg, bool* is_null_arg=NULL)
    {
      buffer      = buffer_arg;
      buffer_size = buffer_size_arg;
      is_null     = is_null_arg;
      //stream      = NULL;

      bError=false;
      bytesReceived=0;
    }

    /*
    SContext(CDB_Stream* stream_arg) // Assert it is not null
    {
      bError=false;
      is_null=NULL;
      stream=stream_arg;
      stream->Truncate(0);
    }
    */
  };
  C_GWLib_Result_ReadItem() : CRegProcCli("GWLib:Result:ReadItem") {}

  virtual bool begin(IGate*) { return false; }
  virtual void end(IGate*, bool) {}
  virtual void exec(IGate* pGate);
};


// A client callback invoked by comprot server from GWLib:ObjectGetter.
// We cannot use C_GWLib_ObjectGetter on server, and this is why
// stream content reading is not incorporated into read_CDB_Object().
DllExport class C_GWLib_ObjectGetter : public CRegProcCli
{
public:
  // In/out args to be passed to exec() from CGW_Result::ReadItem() via
  // IGate::set/getUserData().
  struct SContext {
    CDB_Object* obj;
    bool bError; // Out
    bool bStreamAppend;

    SContext(CDB_Object* obj_arg)
    {
      bError=false;
      bStreamAppend=false;
      obj=obj_arg;
    }
  };
  C_GWLib_ObjectGetter() : CRegProcCli("GWLib:ObjectGetter") {}

  virtual bool begin(IGate*) { return false; }
  virtual void end(IGate*, bool) {}
  virtual void exec(IGate* pGate);
};

DllExport class C_GWLib_MsgCallback : public CRegProcCli
{
public:
  C_GWLib_MsgCallback() : CRegProcCli("GWLib:HandleMsg") {}
  virtual bool begin(IGate*) { return false; }
  virtual void end(IGate*, bool) {}
  virtual void exec(IGate* pGate);
};

/////////////////////////////////////////////////////////////////////////
//// Bound CDB_Object support (CDB_Stream-s, and all other compact types)
/////////////////////////////////////////////////////////////////////////
typedef pair<int,CDB_Object*> CRemoteLocalObjPair;

// Filled in Bind/BindParam(), used in Send/SendRow().
class CRLObjPairs : public vector<CRemoteLocalObjPair>
{
public:
  void deleteRemoteObjects(); // Also cleans up the vector

  ~CRLObjPairs()
  {
    deleteRemoteObjects();
  }


  /* Send "obj_ref", "type", "size" to pGate,
   * retrieve remotely constructed object,
   * add another CRemoteLocalObjPair to the vector.
   */
  void addObjPair(int remoteObj, CDB_Object* localObj)
  {
    //cerr << "addObjPair( remote=" << remoteObj;
    //cerr << ", local=" << (int)localObj << " )\n";
    push_back( CRemoteLocalObjPair( remoteObj, localObj ) );
  }

  /* Transfer local object values to remote objects
   * CDB_Text and CDB_Image objects are skipped here.
   *
   * Returns: number of CDB_Text or CDB_Image objects,
   * -1 on error.
   */
  int updateRemoteObjs();

};

END_NCBI_SCOPE

#endif  /* DBAPI_DRIVER_GATEWAY___INTERNAL_CLI__HPP */

