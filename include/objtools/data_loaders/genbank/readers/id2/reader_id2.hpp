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

class CSeq_id;
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
class CLoadLockBlob_ids;

class CReaderRequestResult;
struct SId2LoadedSet;

class NCBI_XREADER_ID2_EXPORT CId2Reader : public CReader
{
public:
    CId2Reader(int max_connections = 5);
    ~CId2Reader();

    int GetMaximumConnectionsLimit(void) const;

    // new interface
    bool LoadStringSeq_ids(CReaderRequestResult& result,
                           const string& seq_id);
    bool LoadSeq_idSeq_ids(CReaderRequestResult& result,
                           const CSeq_id_Handle& seq_id);
    bool LoadSeq_idBlob_ids(CReaderRequestResult& result,
                            const CSeq_id_Handle& seq_id);
    bool LoadBlobVersion(CReaderRequestResult& result,
                         const TBlobId& blob_id);

    bool LoadBlobs(CReaderRequestResult& result,
                   const string& seq_id,
                   TContentsMask mask);
    bool LoadBlobs(CReaderRequestResult& result,
                   const CSeq_id_Handle& seq_id,
                   TContentsMask mask);
    bool LoadBlobs(CReaderRequestResult& result,
                   CLoadLockBlob_ids blobs,
                   TContentsMask mask);
    bool LoadBlob(CReaderRequestResult& result,
                  const TBlobId& blob_id);
    bool LoadChunk(CReaderRequestResult& result,
                   const TBlobId& blob_id, TChunkId chunk_id);

    static TBlobId GetBlobId(const CID2_Blob_Id& blob_id);

protected:
    virtual void x_AddConnectionSlot(TConn conn);
    virtual void x_RemoveConnectionSlot(TConn conn);
    virtual void x_DisconnectAtSlot(TConn conn);
    virtual void x_ConnectAtSlot(TConn conn);

    CConn_IOStream* x_GetConnection(TConn conn);
    CConn_IOStream* x_NewConnection(void);
    void x_InitConnection(CNcbiIostream& stream);

    void x_SetResolve(CID2_Request_Get_Blob_Id& get_blob_id,
                      const CSeq_id& seq_id);
    void x_SetResolve(CID2_Request_Get_Blob_Id& get_blob_id,
                      const string& seq_id);
    void x_SetResolve(CID2_Blob_Id& blob_id, const CBlob_id& src);

    void x_SetDetails(CID2_Get_Blob_Details& details,
                      TContentsMask mask);

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

    void x_ProcessReply(CReaderRequestResult& result,
                        SId2LoadedSet& loaded_set,
                        const CID2_Reply& reply);
    void x_ProcessReply(CReaderRequestResult& result,
                        SId2LoadedSet& loaded_set,
                        TErrorFlags errors,
                        const CID2_Reply_Get_Seq_id& reply);
    void x_ProcessReply(CReaderRequestResult& result,
                        SId2LoadedSet& loaded_set,
                        TErrorFlags errors,
                        const string& seq_id,
                        const CID2_Reply_Get_Seq_id& reply);
    void x_ProcessReply(CReaderRequestResult& result,
                        SId2LoadedSet& loaded_set,
                        TErrorFlags errors,
                        const CSeq_id_Handle& seq_id,
                        const CID2_Reply_Get_Seq_id& reply);
    void x_ProcessReply(CReaderRequestResult& result,
                        SId2LoadedSet& loaded_set,
                        TErrorFlags errors,
                        const CID2_Reply_Get_Blob_Id& reply);
    void x_ProcessReply(CReaderRequestResult& result,
                        SId2LoadedSet& loaded_set,
                        TErrorFlags errors,
                        const CID2_Reply_Get_Blob_Seq_ids& reply);
    void x_ProcessReply(CReaderRequestResult& result,
                        SId2LoadedSet& loaded_set,
                        TErrorFlags errors,
                        const CID2_Reply_Get_Blob& reply);
    void x_ProcessReply(CReaderRequestResult& result,
                        SId2LoadedSet& loaded_set,
                        TErrorFlags errors,
                        const CID2S_Reply_Get_Split_Info& reply);
    void x_ProcessReply(CReaderRequestResult& result,
                        SId2LoadedSet& loaded_set,
                        TErrorFlags errors,
                        const CID2S_Reply_Get_Chunk& reply);

    void x_UpdateLoadedSet(CReaderRequestResult& result,
                           const SId2LoadedSet& loaded_set);

private:
    typedef map< TConn, AutoPtr<CConn_IOStream> > TConnections;
    TConnections   m_Connections;
    CTime          m_NextConnectTime;

    CAtomicCounter m_RequestSerialNumber;

    enum {
        fAvoidRequest_nested_get_blob_info = 1
    };
    typedef int TAvoidRequests;
    TAvoidRequests m_AvoidRequest;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // READER_ID2__HPP_INCLUDED
