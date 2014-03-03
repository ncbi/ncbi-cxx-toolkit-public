#ifndef READER_CACHE__HPP_INCLUDED
#define READER_CACHE__HPP_INCLUDED

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

#include <objtools/data_loaders/genbank/reader.hpp>
#include <corelib/ncbi_tree.hpp>

#include <vector>

BEGIN_NCBI_SCOPE

class IBLOB_Cache;
class IIntCache;
class ICache;
class IReader;
class IWriter;
class CByteSource;

BEGIN_SCOPE(objects)

class CSeq_id;
class CID2_Reply_Data;
class CLoadLockSeq_ids;

/// structure for common cache reader&writer implementation
struct NCBI_XREADER_CACHE_EXPORT SCacheInfo
{
    typedef vector<int> TIdCacheData;

    static const int    BLOB_IDS_MAGIC;
    static const char*  BLOB_IDS_SUBKEY;

    //////////////////////////////////////////////////////////////////
    // Keys manipulation methods:

    /// Return Id cache key string based on CSeq_id of gi
    static string GetIdKey(const CSeq_id& id);
    static string GetIdKey(const CSeq_id_Handle& id);
    static string GetIdKey(TGi gi);

    /// Id cache subkeys:
    // Seq-id/gi -> blob_id & contents info
    static void GetBlob_idsSubkey(const SAnnotSelector* sel,
                                  string& subkey,
                                  string& true_subkey);
    // Seq-id -> gi (1 int)
    static const char* GetGiSubkey(void);
    // Seq-id -> acc (string: fasta)
    static const char* GetAccVerSubkey(void);
    // Seq-id -> label (string)
    static const char* GetLabelSubkey(void);
    // Seq-id -> taxid (1 int)
    static const char* GetTaxIdSubkey(void);
    // Seq-id -> list of Seq-id, binary ASN.1
    static const char* GetSeq_idsSubkey(void);
    // blob_id -> blob state (1 int)
    static const char* GetBlobStateSubkey(void);
    // blob_id -> blob version (1 int)
    static const char* GetBlobVersionSubkey(void);

    /// Return BLOB cache key string based on Sat() and SatKey()
    static string GetBlobKey(const CBlob_id& blob_id);

    /// BLOB cache subkeys:
    enum {
        kMain_ChunkId       = -1, // not a chunk, but main Seq-entry
        kMasterWGS_ChunkId  = kMax_Int-1, // chunk with master WGS descr
        kDelayedMain_ChunkId= kMax_Int // main Seq-entry with delayed ext annot
    };

    static string GetBlobSubkey(CLoadLockBlob& blob,
                                int chunk_id = kMain_ChunkId);

    static int GetDebugLevel(void);

    typedef CTreePair<string, string> TParamPair;
    typedef TParamPair::TPairTreeNode TParams;

    enum EReaderOrWriter {
        eCacheReader,
        eCacheWriter
    };
    enum EIdOrBlob {
        eIdCache,
        eBlobCache
    };
    static ICache *CreateCache(const TParams* params,
                               EReaderOrWriter reader_or_writer,
                               EIdOrBlob id_or_blob);
};


class NCBI_XREADER_CACHE_EXPORT CCacheHolder
{
public:
    CCacheHolder(void);
    ~CCacheHolder(void);

    void SetBlobCache(ICache* blob_cache);
    void SetIdCache(ICache* id_cache);

    ICache* GetIdCache(void) const {
        return m_IdCache;
    }

    ICache* GetBlobCache(void) const {
        return m_BlobCache;
    }

protected:
    ICache* m_BlobCache;
    ICache* m_IdCache;

private:
    // to prevent copying
    CCacheHolder(const CCacheHolder&);
    void operator=(const CCacheHolder&);
};


class NCBI_XREADER_CACHE_EXPORT CCacheReader : public CReader,
                                               public CCacheHolder,
                                               public SCacheInfo
{
public:
    CCacheReader(void);
    CCacheReader(const TPluginManagerParamTree* params,
                 const string& driver_name);

    //////////////////////////////////////////////////////////////////
    // Overloaded loading methods:
    bool LoadStringSeq_ids(CReaderRequestResult& result,
                           const string& seq_id);
    bool LoadSeq_idSeq_ids(CReaderRequestResult& result,
                           const CSeq_id_Handle& seq_id);
    bool LoadSeq_idGi(CReaderRequestResult& result,
                      const CSeq_id_Handle& seq_id);
    bool LoadSeq_idAccVer(CReaderRequestResult& result,
                          const CSeq_id_Handle& seq_id);
    bool LoadSeq_idLabel(CReaderRequestResult& result,
                         const CSeq_id_Handle& seq_id);
    bool LoadSeq_idTaxId(CReaderRequestResult& result,
                         const CSeq_id_Handle& seq_id);
    bool LoadSeq_idBlob_ids(CReaderRequestResult& result,
                            const CSeq_id_Handle& seq_id,
                            const SAnnotSelector* sel);

    bool LoadAccVers(CReaderRequestResult& result,
                     const TIds& ids, TLoaded& loaded, TIds& ret);
    bool LoadGis(CReaderRequestResult& result,
                 const TIds& ids, TLoaded& loaded, TGis& ret);
    bool LoadLabels(CReaderRequestResult& result,
                    const TIds& ids, TLoaded& loaded, TLabels& ret);
    bool LoadTaxIds(CReaderRequestResult& result,
                    const TIds& ids, TLoaded& loaded, TTaxIds& ret);

    bool LoadBlobState(CReaderRequestResult& result,
                       const TBlobId& blob_id);
    bool LoadBlobVersion(CReaderRequestResult& result,
                         const TBlobId& blob_id);

    bool LoadBlob(CReaderRequestResult& result,
                  const TBlobId& blob_id);
    bool LoadChunk(CReaderRequestResult& result,
                   const TBlobId& blob_id, TChunkId chunk_id);

    bool ReadSeq_ids(CReaderRequestResult& result,
                     const string& key, CLoadLockSeq_ids& ids);

    int GetRetryCount(void) const;
    bool MayBeSkippedOnErrors(void) const;
    int GetMaximumConnectionsLimit(void) const;

    virtual void InitializeCache(CReaderCacheManager& cache_manager,
                                 const TPluginManagerParamTree* params);
    virtual void ResetCache(void);

protected:
    void x_AddConnectionSlot(TConn conn);
    void x_RemoveConnectionSlot(TConn conn);
    void x_DisconnectAtSlot(TConn conn, bool failed);
    void x_ConnectAtSlot(TConn conn);
    void x_ProcessBlob(CReaderRequestResult& result,
                       const TBlobId& blob_id,
                       TChunkId chunk_id,
                       CNcbiIstream& stream);
    void x_SetBlobVersionAsCurrent(CReaderRequestResult& result,
                                   const string& key,
                                   const string& subkey,
                                   TBlobVersion version);

    ESwitch m_JoinedBlobVersion;
};


SCacheInfo::TParams*
GetCacheParams(const SCacheInfo::TParams* src_params,
                SCacheInfo::EReaderOrWriter reader_or_writer,
                SCacheInfo::EIdOrBlob id_or_blob);


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // READER_CACHE__HPP_INCLUDED
