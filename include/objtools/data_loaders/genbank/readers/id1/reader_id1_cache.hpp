#ifndef READER_ID1_CACHE__HPP_INCLUDED
#define READER_ID1_CACHE__HPP_INCLUDED

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
*  Author:  Eugene Vasilchenko, Anatoliy Kuznetsov
*
*  File Description: Cached extension of data reader from ID1
*
*/

#include <objtools/data_loaders/genbank/readers/id1/reader_id1.hpp>

#include <vector>

BEGIN_NCBI_SCOPE

class IBLOB_Cache;
class IIntCache;
class ICache;
class IReader;
class IWriter;
class CByteSource;

BEGIN_SCOPE(objects)

class CID2_Reply_Data;

/// ID1 reader with caching capabilities
///
class NCBI_XREADER_ID1_EXPORT CCachedId1Reader : public CId1Reader
{
    typedef CId1Reader TParent;
public:
    CCachedId1Reader(TConn noConn = 1,
                     ICache* blob_cache = 0,
                     ICache* id_cache = 0);
    ~CCachedId1Reader();


    //////////////////////////////////////////////////////////////////
    // Setup methods:

    void SetBlobCache(ICache* blob_cache);
    void SetIdCache(ICache* id_cache);


    //////////////////////////////////////////////////////////////////
    // Keys manipulation methods:

    /// Return BLOB cache key string based on Sat() and SatKey()
    string GetBlobKey(const CBlob_id& blob_id) const;

    /// BLOB cache subkeys:
    const char* GetSeqEntrySubkey(void) const;
    const char* GetSNPTableSubkey(void) const;
    const char* GetSkeletonSubkey(void) const;
    const char* GetSplitInfoSubkey(void) const;
    string GetChunkSubkey(int chunk_id) const;

    /// Return Id cache key string based on CSeq_id of gi
    string GetIdKey(const CSeq_id& id) const;
    string GetIdKey(int gi) const;

    /// Id cache subkeys:
    const char* GetBlob_idsSubkey(void) const;//Seq-id/gi -> blob_id (4*N ints)
    const char* GetGiSubkey(void) const;      //Seq-id -> gi (1 int)

    // blob_id -> blob version (1 int)
    const char* GetBlobVersionSubkey(void) const;


    //////////////////////////////////////////////////////////////////
    // Overloaded loading methods:

    int ResolveSeq_id_to_gi(const CSeq_id& id, TConn conn);
    void ResolveGi(CLoadLockBlob_ids& ids, int gi, TConn conn);
    TBlobVersion GetVersion(const CBlob_id& blob_id, TConn = 0);

    void GetTSEBlob(CTSE_Info& tse_info,
                    const CBlob_id& blob_id,
                    TConn conn);
    void GetTSEChunk(CTSE_Chunk_Info& chunk_info,
                     const CBlob_id& blob_id,
                     TConn conn = 0);


    //////////////////////////////////////////////////////////////////
    // Id cache low level access methods:

    bool LoadIds(int gi, CLoadLockBlob_ids& ids);
    bool LoadIds(const CSeq_id& id, CLoadLockBlob_ids& ids);
    bool LoadIds(const string& key, CLoadLockBlob_ids& ids);
    void StoreIds(int gi, const CLoadLockBlob_ids& ids);
    void StoreIds(const CSeq_id& id, const CLoadLockBlob_ids& ids);
    void StoreIds(const string& key, const CLoadLockBlob_ids& ids);

    bool LoadVersion(const string& key, TBlobVersion& version);
    void StoreVersion(const string& key, TBlobVersion version);


    //////////////////////////////////////////////////////////////////
    // Blob cache low level access methods:

    bool LoadBlob(CID1server_back& id1_reply,
                  CRef<CID2S_Split_Info>& split_info,
                  const CBlob_id& blob_id);
    bool LoadWholeBlob(CTSE_Info& tse_info,
                       const string& key, TBlobVersion version);
    bool LoadSplitBlob(CTSE_Info& tse_info,
                       const string& key, TBlobVersion version);

    bool LoadSNPTable(CSeq_annot_SNP_Info& snp_info,
                      const string& key, TBlobVersion version);
    void StoreSNPTable(const CSeq_annot_SNP_Info& snp_info,
                       const string& key, TBlobVersion version);

    size_t LoadData(const string& key, TBlobVersion version,
                    const string& subkey,
                    CID2_Reply_Data& data, int data_type);

protected:
    
    void PrintStatistics(void) const;

    void LogIdLoadStat(const char* type,
                       const string& key,
                       const string& subkey,
                       CStopWatch& sw);
    void LogIdStoreStat(const char* type,
                        const string& key,
                        const string& subkey,
                        CStopWatch& sw);

    typedef vector<int> TBlob_idsData;

    bool x_LoadIdCache(const string& key,
                       const string& subkey,
                       TBlob_idsData& ints);
    bool x_LoadIdCache(const string& key,
                       const string& subkey,
                       int& value);
    void x_StoreIdCache(const string& key,
                        const string& subkey,
                        const TBlob_idsData& ints);
    void x_StoreIdCache(const string& key,
                        const string& subkey,
                        const int& value);
    
    void x_GetSNPAnnot(CSeq_annot_SNP_Info& snp_info,
                       const CBlob_id& blob_id,
                       TConn conn);

    void x_ReadTSEBlob(CID1server_back& id1_reply,
                       const CBlob_id&  blob_id,
                       CNcbiIstream&    stream);

    void x_SetBlobRequest(CID1server_request& request,
                          const CBlob_id& blob_id);
    void x_ReadBlobReply(CID1server_back& reply,
                         CObjectIStream& stream,
                         const CBlob_id& blob_id);

    void StoreBlob(const string& key, TBlobVersion version,
                   CRef<CByteSource> bytes);

    CObjectIStream* OpenData(CID2_Reply_Data& data);

private:
    ICache*   m_BlobCache;
    ICache*   m_IdCache;

private:
    // to prevent copying
    CCachedId1Reader(const CCachedId1Reader& );
    CCachedId1Reader& operator=(const CCachedId1Reader&);
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // READER_ID1_CACHE__HPP_INCLUDED
