#ifndef OBJTOOLS_DATA_LOADERS_SNP___SNPLOADER_IMPL__HPP
#define OBJTOOLS_DATA_LOADERS_SNP___SNPLOADER_IMPL__HPP

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
 * Author: Eugene Vasilchenko
 *
 * File Description: SNP file data loader
 *
 * ===========================================================================
 */


#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>
#include <sra/data_loaders/snp/snploader.hpp>
#include <sra/readers/sra/snpread.hpp>
#include <util/limited_size_map.hpp>
#include <objects/dbsnp/primary_track/snpptis.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataLoader;
class CSNPDataLoader_Impl;
class CSNPSeqChunkInfo;
class CSNPSeqInfo;
class CSNPFileInfo;
class CID2SNPContext;

class CSNPBlobId : public CBlobId
{
public:
    explicit
    CSNPBlobId(const CTempString& str);
    CSNPBlobId(const CSNPFileInfo& file,
               const CSeq_id_Handle& seq_id,
               size_t filter_index);
    CSNPBlobId(const CSNPDbSeqIterator& seq,
               size_t filter_index);
    CSNPBlobId(const CSNPFileInfo& file,
               size_t seq_index,
               size_t filter_index);
    ~CSNPBlobId(void);

    // string blob id representation:
    // eBlobType_annot_plain_id
    string ToString(void) const;
    void FromString(CTempString str);
    bool FromSatString(CTempString str);

    bool operator<(const CBlobId& id) const;
    bool operator==(const CBlobId& id) const;

    bool IsSatId(void) const;

    Int4 GetSat(void) const;
    Int4 GetSubSat(void) const;
    Int4 GetSatKey(void) const;

    bool IsValidSat(void) const;
    bool IsValidSubSat(void) const;
    bool IsValidSatKey(void) const;
    
    static bool IsValidNAIndex(size_t index);
    static bool IsValidNAVersion(size_t version);
    static bool IsValidNA(pair<size_t, size_t> na) {
        return IsValidNAIndex(na.first) && IsValidNAVersion(na.second);
    }
    static bool IsValidSeqIndex(size_t seq_index);
    static bool IsValidFilterIndex(size_t filter_index);

    static pair<size_t, size_t> ParseNA(CTempString acc);
    static bool IsValidNA(CTempString acc) {
        return ParseNA(acc).first != 0;
    }

    string GetSatNA(void) const;
    int GetSatBase(void) const;
    int GetSubSatBase(void) const;
    
    size_t GetNAIndex(void) const
        {
            return m_NAIndex;
        }
    size_t GetNAVersion(void) const
        {
            return m_NAVersion;
        }
    size_t GetSeqIndex(void) const
        {
            return m_SeqIndex;
        }
    size_t GetFilterIndex(void) const
        {
            return m_FilterIndex;
        }

    void SetSatNA(CTempString acc);
    void SetNAIndex(size_t na_index);
    void SetNAVersion(size_t na_version);
    void SetSeqAndFilterIndex(size_t seq_index,
                              size_t filter_index);

    CSeq_id_Handle GetSeqId(void) const;
    string GetAccession(void) const;

    bool IsPrimaryTrack() const
        {
            return m_IsPrimaryTrack;
        }
    bool IsPrimaryTrackGraph() const
        {
            return m_IsPrimaryTrackGraph;
        }
    bool IsPrimaryTrackFeat() const
        {
            return IsPrimaryTrack() && !IsPrimaryTrackGraph();
        }

    void SetPrimaryTrackFeat();
    void SetPrimaryTrackGraph();

protected:
    // ID2 blob id
    Uint4 m_NAIndex;
    Uint2 m_NAVersion;
    bool m_IsPrimaryTrack;
    bool m_IsPrimaryTrackGraph;
    Uint4 m_SeqIndex;
    Uint4 m_FilterIndex;
    
    // SNP file name or VDB accession
    string m_Accession;
    // Ref Seq-id for annot blobs
    CSeq_id_Handle m_SeqId;
};


enum ESNPAnnotChunkIdType {
    eSNPAnnotChunk_graph,
    eSNPAnnotChunk_snp
};

    
class CSNPSeqInfo : public CObject
{
public:
    CSNPSeqInfo(CSNPFileInfo* file,
                const CSNPDbSeqIterator& it);

    CSNPDbSeqIterator GetSeqIterator(void) const;

    CRef<CSNPBlobId> GetAnnotBlobId(void) const;
    int GetAnnotChunkId(TSeqPos ref_pos) const;

    string GetAnnotName(void) const;

    void LoadRanges(void);

    void LoadAnnotBlob(CTSE_LoadLock& load_lock);
    void LoadAnnotChunk(CTSE_Chunk_Info& chunk_info);

    void LoadAnnotMainSplit(CTSE_LoadLock& load_lock);
    void LoadAnnotMainChunk(CTSE_Chunk_Info& chunk_info);
    void LoadAnnotAlignChunk(CTSE_Chunk_Info& chunk_info);
    void LoadAnnotGraphChunk(CTSE_Chunk_Info& chunk_info);

    void LoadSeqBlob(CTSE_LoadLock& load_lock);
    void LoadSeqChunk(CTSE_Chunk_Info& chunk_info);

    void LoadSeqMainEntry(CTSE_LoadLock& load_lock);

    CRef<CSNPBlobId> GetBlobId(void) const;
    void SetFilterIndex(size_t filter_index);
    void SetFromBlobId(const CSNPBlobId& blob_id);

    bool IncludeFeat(void) const
        {
            return !m_IsPrimaryTrack || !m_IsPrimaryTrackGraph;
        }
    bool IncludeGraph(void) const
        {
            return !m_IsPrimaryTrack || m_IsPrimaryTrackGraph;
        }

protected:
    friend class CSNPDataLoader_Impl;

    CSNPFileInfo* m_File;
    size_t m_SeqIndex;
    size_t m_FilterIndex;
    CSeq_id_Handle m_SeqId;
    bool m_IsPrimaryTrack;
    bool m_IsPrimaryTrackGraph;
};


class CSNPFileInfo : public CObject
{
public:
    CSNPFileInfo(CSNPDataLoader_Impl& impl,
                  const string& file_name);
    
    bool IsValidNA(void) const {
        return m_IsValidNA;
    }

    const string& GetFileName(void) const
        {
            return m_FileName;
        }
    const string& GetAccession(void) const
        {
            return m_Accession;
        }
    const string& GetBaseAnnotName(void) const
        {
            return m_AnnotName;
        }
    string GetSNPAnnotName(size_t filter_index) const;

    CRef<CSNPBlobId> GetAnnotBlobId(const CSeq_id_Handle& id) const;

    CRef<CSNPSeqInfo> GetSeqInfo(const CSeq_id_Handle& seq_id);
    CRef<CSNPSeqInfo> GetSeqInfo(size_t seq_index);
    CRef<CSNPSeqInfo> GetSeqInfo(const CSNPBlobId& blob_id);

    CMutex& GetMutex(void) const
        {
            return m_SNPMutex;
        }

    CSNPDb& GetDb(void)
        {
            return m_SNPDb;
        }
    operator CSNPDb&(void)
        {
            return GetDb();
        }

    void AddSeq(const CSeq_id_Handle& id);

    typedef CSNPDataLoader::TAnnotNames TAnnotNames;
    void GetPossibleAnnotNames(TAnnotNames& names) const;
    
protected:
    friend class CSNPDataLoader_Impl;

    typedef map<CSeq_id_Handle, CRef<CSNPSeqInfo> > TSeqById;
    typedef map<size_t, CRef<CSNPSeqInfo> > TSeqByIdx;

    void x_Initialize(CSNPDataLoader_Impl& impl,
                      const string& file_name);

    bool m_IsValidNA;
    string m_FileName; // external VDB file access string
    string m_Accession; // OM named annot accession (without filter index)
    string m_AnnotName; // OM annot name (without filter index)
    mutable CMutex m_SNPMutex;
    CSNPDb m_SNPDb;
    TSeqById m_SeqById;
    TSeqByIdx m_SeqByIdx;
};


class CSNPDataLoader_Impl : public CObject
{
public:
    explicit CSNPDataLoader_Impl(const CSNPDataLoader::SLoaderParams& params);
    ~CSNPDataLoader_Impl(void);

    void AddFixedFile(const string& file_name);
    
    CRef<CSNPFileInfo> GetFixedFile(const string& acc);
    CRef<CSNPFileInfo> FindFile(const string& acc);
    CRef<CSNPFileInfo> GetFileInfo(const string& acc);
    CRef<CSNPFileInfo> GetFileInfo(const CSNPBlobId& blob_id);
    CRef<CSNPSeqInfo> GetSeqInfo(const CSNPBlobId& blob_id);

    CDataLoader::TTSE_LockSet GetRecords(CDataSource* data_source,
                                         const CSeq_id_Handle& idh,
                                         CDataLoader::EChoice choice);
    CDataLoader::TTSE_LockSet GetOrphanAnnotRecords(CDataSource* ds,
                                                    const CSeq_id_Handle& idh,
                                                    const SAnnotSelector* sel,
                                                    CDataLoader::TProcessedNAs* processed_nas);

    CTSE_LoadLock GetBlobById(CDataSource* data_source,
                              const CSNPBlobId& blob_id);
    void LoadBlob(const CSNPBlobId& blob_id,
                  CTSE_LoadLock& load_lock);
    void LoadChunk(const CSNPBlobId& blob_id,
                   CTSE_Chunk_Info& chunk);

    CObjectManager::TPriority GetDefaultPriority(void) const;
    
    typedef CSNPDataLoader::TAnnotNames TAnnotNames;
    TAnnotNames GetPossibleAnnotNames(void) const;

protected:
    friend class CSNPFileInfo;
    struct SDirSeqInfo {
        CSeq_id_Handle m_SeqId;
        string m_SNPFileName;
        string m_SNPSeqLabel;
        string m_Label;
    };

private:
    typedef map<string, CRef<CSNPFileInfo> > TFixedFiles;
    typedef limited_size_map<string, CRef<CSNPFileInfo> > TFoundFiles;
    typedef limited_size_map<string, bool> TMissingFiles;

    // mutex guarding input into the map
    mutable CMutex m_Mutex;
    CVDBMgr m_Mgr;
    string  m_DirPath;
    string  m_AnnotName;
    TFixedFiles m_FixedFiles;
    TFoundFiles m_FoundFiles;
    TMissingFiles m_MissingFiles;
    bool m_AddPTIS;
    CRef<CSnpPtisClient> m_PTISClient;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // OBJTOOLS_DATA_LOADERS_SNP___SNPLOADER_IMPL__HPP
