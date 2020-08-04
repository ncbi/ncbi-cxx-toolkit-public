#ifndef PSGS_OSGGETBLOBBASE__HPP
#define PSGS_OSGGETBLOBBASE__HPP

/*  $Id$
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
 * Authors: Eugene Vasilchenko
 *
 * File Description: processor to get blob/chunk from OSG
 *
 */

#include "osg_processor_base.hpp"


BEGIN_NCBI_NAMESPACE;

BEGIN_NAMESPACE(objects);

class CID2_Blob_Id;
class CID2_Reply_Data;
class CID2_Reply_Get_Blob;
class CID2S_Reply_Get_Split_Info;
class CID2S_Reply_Get_Chunk;

END_NAMESPACE(objects);

BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(osg);


class CPSGS_OSGGetBlobBase : virtual public CPSGS_OSGProcessorBase
{
public:
    CPSGS_OSGGetBlobBase();
    virtual ~CPSGS_OSGGetBlobBase();

    static bool CanLoad(const SPSGS_BlobId& blob_id)
        {
            return IsOSGBlob(blob_id);
        }
    
    static bool IsOSGBlob(const SPSGS_BlobId& blob_id);

    static CRef<CID2_Blob_Id> GetOSGBlobId(const SPSGS_BlobId& blob_id);
    static string GetPSGBlobId(const CID2_Blob_Id& blob_id);

protected:
    typedef int TID2BlobState;
    typedef int TID2BlobVersion;
    typedef int TID2SplitVersion;
    typedef int TID2ChunkId;

    void ProcessBlobReply(const CID2_Reply& reply);
    void SendBlob();
    
    static string x_GetSplitInfoPSGBlobId(const string& main_blob_id);
    static string x_GetChunkPSGBlobId(const string& main_blob_id,
                                      TID2ChunkId chunk_id);
    
    string x_GetPSGDataBlobId(const CID2_Blob_Id& blob_id,
                              const CID2_Reply_Data& data);
    void x_SetBlobState(CBlobRecord& blob_props,
                        TID2BlobState blob_state);
    void x_SetBlobVersion(CBlobRecord& blob_props,
                          const CID2_Blob_Id& blob_id);
    void x_SetId2SplitInfo(CBlobRecord& blob_props,
                           const string& main_blob_id,
                           TID2SplitVersion split_version);
    void x_SetBlobDataProps(CBlobRecord& blob_props,
                            const CID2_Reply_Data& data);
    
    void x_SendBlobProps(const string& psg_blob_id,
                         CBlobRecord& blob_props);
    void x_SendBlobData(const string& psg_blob_id,
                        const CID2_Reply_Data& data);

    void x_SendMainEntry(const CID2_Blob_Id& osg_blob_id,
                         TID2BlobState blob_state,
                         const CID2_Reply_Data& data);
    void x_SendSplitInfo(const CID2_Blob_Id& osg_blob_id,
                         TID2BlobState blob_state,
                         const CID2_Reply_Data& data,
                         TID2SplitVersion split_version);
    void x_SendChunk(const CID2_Blob_Id& osg_blob_id,
                     const CID2_Reply_Data& data,
                     TID2ChunkId chunk_id);
    
    CConstRef<CID2_Reply_Get_Blob> m_Blob;
    CConstRef<CID2S_Reply_Get_Split_Info> m_SplitInfo;
    CConstRef<CID2S_Reply_Get_Chunk> m_Chunk;
};


END_NAMESPACE(osg);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;

#endif  // PSGS_OSGGETBLOBBASE__HPP
