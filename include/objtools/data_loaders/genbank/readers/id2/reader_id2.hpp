#ifndef READER_ID2__HPP_INCLUDED
#define READER_ID2__HPP_INCLUDED

/*  $Id$
* ===========================================================================
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
* ===========================================================================
*
*  Author:  Eugene Vasilchenko
*
*  File Description: Data reader from ID2
*
*/

#include <objtools/data_loaders/genbank/reader.hpp>
#include <corelib/ncbitime.hpp>

BEGIN_NCBI_SCOPE

class CConn_IOStream;
class CByteSourceReader;
class CObjectIStream;
class CObjectInfo;

BEGIN_SCOPE(objects)

class CID2_Blob_Id;

class CID2_Request;
class CID2_Request_Packet;
class CID2_Request_Get_Seq_id;
class CID2_Request_Get_Blob_Id;
class CID2_Request_Get_Blob;
class CID2_Get_Blob_Details;

class CID2_Error;
class CID2_Reply;
class CID2_Reply_Get_Seq_id;
class CID2_Reply_Get_Blob_Id;
class CID2_Reply_Get_Blob_Seq_ids;
class CID2_Reply_Get_Blob;
class CID2_Reply_Data;
class CID2S_Reply_Get_Split_Info;
class CID2S_Split_Info;
class CID2S_Reply_Get_Chunk;
class CID2S_Chunk_Id;
class CID2S_Chunk;

class CLoadLockSeq_ids;

class CReaderRequestResult;

class NCBI_XREADER_ID2_EXPORT CId2Reader : public CReader
{
public:
    CId2Reader(TConn noConn = 5);
    ~CId2Reader();

    typedef CBlob_id TBlob_id;
    typedef int TChunk_id;

    // new interface
    void ResolveString(CReaderRequestResult& result,
                       const string& seq_id);
    void ResolveSeq_id(CReaderRequestResult& result,
                       const CSeq_id_Handle& seq_id);
    void ResolveSeq_ids(CReaderRequestResult& result,
                        const CSeq_id_Handle& seq_id);
    TBlobVersion GetBlobVersion(CReaderRequestResult& result,
                                const TBlob_id& blob_id);

    void LoadBlobs(CReaderRequestResult& result,
                   const string& seq_id, int sr_mask);
    void LoadBlobs(CReaderRequestResult& result,
                   const CSeq_id_Handle& seq_id, int sr_mask);
    void LoadBlobs(CReaderRequestResult& result,
                   CLoadLockBlob_ids blobs,
                   int sr_mask);
    void LoadBlob(CReaderRequestResult& result,
                  const TBlob_id& blob_id);
    void LoadChunk(CReaderRequestResult& result,
                   const TBlob_id& blob_id, TChunk_id chunk_id);

    virtual TConn GetParallelLevel(void) const;
    virtual void SetParallelLevel(TConn noConn);
    virtual void Reconnect(TConn conn);

    static TBlob_id GetBlob_id(const CID2_Blob_Id& blob_id);
    CTSE_Info& GetTSE_Info(CReaderRequestResult& result,
                           const CID2_Blob_Id& blob_id);
    CTSE_Chunk_Info& GetChunk_Info(CReaderRequestResult& result,
                                   const CID2_Blob_Id& blob_id,
                                   const CID2S_Chunk_Id& chunk_id);

protected:
    CConn_IOStream* x_GetConnection(TConn conn);
    CConn_IOStream* x_NewConnection(void);
    void x_InitConnection(CNcbiIostream& stream);

    void x_SetResolve(CID2_Request_Get_Blob_Id& get_blob_id,
                      const CSeq_id& seq_id);
    void x_SetResolve(CID2_Request_Get_Blob_Id& get_blob_id,
                      const string& seq_id);
    void x_SetResolve(CID2_Blob_Id& blob_id, const CBlob_id& src);

    void x_SetDetails(CID2_Get_Blob_Details& details,
                      TBlobContentsMask mask);

    void x_ProcessRequest(CReaderRequestResult& result,
                          CID2_Request& req);
    void x_ProcessPacket(CReaderRequestResult& result,
                         CID2_Request_Packet& packet);

    enum EErrorFlags {
        fError_warning          = 1 << 0,
        fError_no_data          = 1 << 1,
        fError_bad_command      = 1 << 2,
        fError_bad_connection   = 1 << 3
    };
    typedef int TErrorFlags;
    TErrorFlags x_ProcessError(CReaderRequestResult& result,
                               const CID2_Error& error);
    void x_Reconnect(CReaderRequestResult& result,
                     const CID2_Error& error, int retry_delay);
    void x_Reconnect(CReaderRequestResult& result, int retry_delay);

    void x_ProcessReply(CReaderRequestResult& result,
                        const CID2_Reply& reply);
    void x_ProcessReply(CReaderRequestResult& result,
                        TErrorFlags errors,
                        const CID2_Reply_Get_Seq_id& reply);
    void x_ProcessReply(CReaderRequestResult& result,
                        TErrorFlags errors,
                        CLoadLockSeq_ids& ids,
                        const CID2_Reply_Get_Seq_id& reply);
    void x_ProcessReply(CReaderRequestResult& result,
                        TErrorFlags errors,
                        const CID2_Reply_Get_Blob_Id& reply);
    void x_ProcessReply(CReaderRequestResult& result,
                        TErrorFlags errors,
                        const CID2_Reply_Get_Blob_Seq_ids& reply);
    void x_ProcessReply(CReaderRequestResult& result,
                        TErrorFlags errors,
                        const CID2_Reply_Get_Blob& reply);
    void x_ProcessReply(CReaderRequestResult& result,
                        TErrorFlags errors,
                        const CID2S_Reply_Get_Split_Info& reply);
    void x_ProcessReply(CReaderRequestResult& result,
                        TErrorFlags errors,
                        const CID2S_Reply_Get_Chunk& reply);

    CObjectIStream* x_OpenDataStream(const CID2_Reply_Data& data);
    void x_ReadData(const CID2_Reply_Data& data, const CObjectInfo& object);
    void x_ReadSNPData(CTSE_Info& tse, const CID2_Reply_Data& data);

private:
    vector<CConn_IOStream*>     m_Pool;
    auto_ptr<CConn_IOStream>    m_FirstConnection;
    bool                        m_NoMoreConnections;
    CAtomicCounter              m_RequestSerialNumber;

    enum {
        fAvoidRequest_nested_get_blob_info = 1
    };
    typedef int TAvoidRequests;
    TAvoidRequests              m_AvoidRequest;
    CTime                       m_NextConnectTime;
};


END_SCOPE(objects)

extern NCBI_XREADER_ID2_EXPORT const string kId2ReaderDriverName;

extern "C" 
{

NCBI_XREADER_ID2_EXPORT
void NCBI_EntryPoint_Id2Reader(
     CPluginManager<objects::CReader>::TDriverInfoList&   info_list,
     CPluginManager<objects::CReader>::EEntryPointRequest method);

NCBI_XREADER_ID2_EXPORT
void NCBI_EntryPoint_xreader_id2(
     CPluginManager<objects::CReader>::TDriverInfoList&   info_list,
     CPluginManager<objects::CReader>::EEntryPointRequest method);

} // extern C


END_NCBI_SCOPE

#endif // READER_ID2__HPP_INCLUDED
