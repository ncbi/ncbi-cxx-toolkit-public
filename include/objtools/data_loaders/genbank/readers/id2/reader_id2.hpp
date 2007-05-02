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

#include <objtools/data_loaders/genbank/reader_id2_base.hpp>
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
class CID2_Request_Get_Blob_Info;
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

class NCBI_XREADER_ID2_EXPORT CId2Reader : public CId2ReaderBase
{
public:
    CId2Reader(int max_connections = 0);
    CId2Reader(const TPluginManagerParamTree* params,
               const string& driver_name);
    ~CId2Reader();

    int GetMaximumConnectionsLimit(void) const;

protected:
    virtual void x_AddConnectionSlot(TConn conn);
    virtual void x_RemoveConnectionSlot(TConn conn);
    virtual void x_DisconnectAtSlot(TConn conn);
    virtual void x_ConnectAtSlot(TConn conn);
    virtual string x_ConnDescription(TConn conn) const;

    virtual void x_SendPacket(TConn conn, const CID2_Request_Packet& packet);
    virtual void x_ReceiveReply(TConn conn, CID2_Reply& reply);

    string x_ConnDescription(CConn_IOStream& stream) const;
    CConn_IOStream* x_GetCurrentConnection(TConn conn) const;
    CConn_IOStream* x_GetConnection(TConn conn);
    CConn_IOStream* x_NewConnection(TConn conn);
    void x_InitConnection(CConn_IOStream& stream, TConn conn);

private:
    string m_ServiceName;
    int    m_Timeout;

    typedef map< TConn, AutoPtr<CConn_IOStream> > TConnections;
    TConnections   m_Connections;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // READER_ID2__HPP_INCLUDED
