#ifndef OBJTOOLS_DATA_LOADERS_BAM___BAMLOADER_IMPL__HPP
#define OBJTOOLS_DATA_LOADERS_BAM___BAMLOADER_IMPL__HPP

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
 * File Description: BAM file data loader
 *
 * ===========================================================================
 */


#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>
#include <sra/data_loaders/bam/bamloader.hpp>
#include <sra/readers/bam/bamread.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataLoader;
class CBAMDataLoader_Impl;
class CBamRefSeqChunkInfo;
class CBamRefSeqInfo;
class CBamFileInfo;

class CBAMBlobId : public CBlobId
{
public:
    explicit CBAMBlobId(const CTempString& str);
    CBAMBlobId(const string& bam_name, const CSeq_id_Handle& seq_id);
    ~CBAMBlobId(void);

    string  m_BamName;
    CSeq_id_Handle m_SeqId;

    string ToString(void) const;
    bool operator<(const CBlobId& id) const;
    bool operator==(const CBlobId& id) const;
};


class CBamRefSeqChunkInfo
{
public:
    CBamRefSeqChunkInfo(void)
        : m_AlignCount(0), m_MaxRefSeqFrom(0)
        {
        }

    // estimated number of alignments
    Uint8 GetAlignCount(void) const
        {
            return m_AlignCount;
        }

    typedef CRange<TSeqPos> TRange;

    void AddRefSeqRange(const TRange& range);

    // full reference sequence range covered by alignments in this chunk
    const TRange& GetRefSeqRange(void) const
        {
            return m_RefSeqRange;
        }
    // max start position of an alignmnent in this chunk
    TSeqPos GetMaxRefSeqFrom(void) const
        {
            return m_MaxRefSeqFrom;
        }
    // All alignments have their start position in range
    // GetRefSeqRange().GetFrom()...GetMaxRefSeqFrom().
    // The chunk is registered to have alignments in range GetRefSeqRange(),
    // and having pileup graphs over range GetRefSeqRange() excluding start of the next chunk.
    const TRange& GetAlignRange() const
        {
            return GetRefSeqRange();
        }
    TRange GetAlignStartRange() const
        {
            TRange range = GetRefSeqRange();
            range.SetTo(GetMaxRefSeqFrom());
            return range;
        }
    TRange GetGraphRangeUnfiltered() const
        {
            return GetRefSeqRange();
        }

protected:
    friend class CBamRefSeqInfo;

    Uint8 m_AlignCount; // 0 - graph only
    TRange m_RefSeqRange;
    TSeqPos m_MaxRefSeqFrom;
};


class CBamRefSeqInfo : public CObject
{
public:
    CBamRefSeqInfo(CBamFileInfo* bam_file,
                   const string& refseqid,
                   const CSeq_id_Handle& seq_id);

    const string& GetRefSeqId(void) const
        {
            return m_RefSeqId;
        }
    const CSeq_id_Handle& GetRefSeq_id(void) const
        {
            return m_RefSeq_id;
        }

    void SetCovFileName(const string& name)
        {
            m_CovFileName = name;
        }

    void LoadRanges(void);
    CRange<TSeqPos> GetChunkGraphRange(size_t range_id);
    
    void LoadMainSplit(CTSE_LoadLock& load_lock);
    void LoadMainEntry(CTSE_LoadLock& load_lock);
    void CreateChunks(CTSE_Split_Info& split_info);
    double EstimateLoadSeconds(const CTSE_Chunk_Info& chunk,
                               Uint4 bytes) const;
    double EstimatePileupLoadSeconds(const CTSE_Chunk_Info& chunk,
                                     Uint4 bytes) const;
    double EstimateAlignLoadSeconds(const CTSE_Chunk_Info& chunk,
                                    Uint4 bytes) const;
    double EstimateSeqLoadSeconds(const CTSE_Chunk_Info& chunk,
                                  Uint4 bytes) const;
    void LoadChunk(CTSE_Chunk_Info& chunk_info);
    void LoadMainChunk(CTSE_Chunk_Info& chunk_info);
    void LoadAlignChunk(CTSE_Chunk_Info& chunk_info);
    void LoadSeqChunk(CTSE_Chunk_Info& chunk_info);
    void LoadPileupChunk(CTSE_Chunk_Info& chunk_info);

    void GetShortSeqBlobId(CRef<CBAMBlobId>& ret,
                           const CSeq_id_Handle& idh) const;
    void SetBlobId(CRef<CBAMBlobId>& ret,
                   const CSeq_id_Handle& idh) const;

protected:
    typedef CRange<TSeqPos> TRange;
    typedef vector<CBamRefSeqChunkInfo> TChunks;
    typedef map<CSeq_id_Handle, int> TSeq2Chunk;

    void x_LoadRangesScan(void);
    void x_LoadRangesStat(void);
    bool x_LoadRangesCov(void);
    bool x_LoadRangesEstimated(void);

    CBamFileInfo* m_File;
    string m_RefSeqId;
    CSeq_id_Handle m_RefSeq_id;
    string m_CovFileName;
    CRef<CSeq_entry> m_CovEntry;
    int m_MinMapQuality;
    TChunks m_Chunks;
    bool m_LoadedRanges;
    CIRef<CBamAlignIterator::ISpotIdDetector> m_SpotIdDetector;
    TSeq2Chunk m_Seq2Chunk;
};


class CBamFileInfo : public CObject
{
public:
    CBamFileInfo(const CBAMDataLoader_Impl& impl,
                 const CBAMDataLoader::SBamFileName& bam,
                 const string& refseq_label = kEmptyStr,
                 const CSeq_id_Handle& seq_id = CSeq_id_Handle());
    
    const string& GetBamName(void) const
        {
            return m_BamName;
        }
    const string& GetAnnotName(void) const
        {
            return m_AnnotName;
        }

    void GetShortSeqBlobId(CRef<CBAMBlobId>& ret,
                           const CSeq_id_Handle& idh) const;
    void GetRefSeqBlobId(CRef<CBAMBlobId>& ret,
                         const CSeq_id_Handle& idh) const;

    CBamRefSeqInfo* GetRefSeqInfo(const CSeq_id_Handle& seq_id) const;

    TSeqPos GetRefSeqLength(const string& id) const
        {
            return m_BamDb.GetRefSeqLength(id);
        }

    CMutex& GetMutex(void) const
        {
            return m_BamMutex;
        }

    CBamDb& GetBamDb(void)
        {
            return m_BamDb;
        }
    operator CBamDb&(void)
        {
            return GetBamDb();
        }

    void AddRefSeq(const string& refseq_label,
                   const CSeq_id_Handle& refseq_id);
    
protected:
    typedef map<CSeq_id_Handle, CRef<CBamRefSeqInfo> > TRefSeqs;

    void x_Initialize(const CBAMDataLoader_Impl& impl,
                      const CBAMDataLoader::SBamFileName& bam);

    string m_BamName;
    string m_AnnotName;
    mutable CMutex m_BamMutex;
    CBamDb m_BamDb;
    TRefSeqs m_RefSeqs;
};


class CBAMDataLoader_Impl : public CObject
{
public:
    explicit CBAMDataLoader_Impl(const CBAMDataLoader::SLoaderParams& params);
    ~CBAMDataLoader_Impl(void);

    void AddSrzDef(void);
    void AddBamFile(const CBAMDataLoader::SBamFileName& bam);
    void OpenBAMFiles();
    bool BAMFilesOpened() const;

    CRef<CBAMBlobId> GetShortSeqBlobId(const CSeq_id_Handle& idh);
    CRef<CBAMBlobId> GetRefSeqBlobId(const CSeq_id_Handle& idh);

    typedef pair<CBamFileInfo*, const CBamRefSeqInfo*> TRefSeqInfo;
    CBamRefSeqInfo* GetRefSeqInfo(const CBAMBlobId& blob_id);
    void LoadBAMEntry(const CBAMBlobId& blob_id,
                      CTSE_LoadLock& load_lock);
    void LoadChunk(const CBAMBlobId& blob_id,
                   CTSE_Chunk_Info& chunk);
    double EstimateLoadSeconds(const CBAMBlobId& blob_id,
                               const CTSE_Chunk_Info& chunk,
                               Uint4 bytes);

    CBAMDataLoader::TAnnotNames GetPossibleAnnotNames(void) const;

    bool IsShortSeq(const CSeq_id_Handle& idh);
    typedef vector<CSeq_id_Handle> TIds;
    void GetIds(const CSeq_id_Handle& idh, TIds& ids);
    CDataSource::SAccVerFound GetAccVer(const CSeq_id_Handle& idh);
    CDataSource::SGiFound GetGi(const CSeq_id_Handle& idh);
    string GetLabel(const CSeq_id_Handle& idh);
    TTaxId GetTaxId(const CSeq_id_Handle& idh);

protected:
    friend class CBamFileInfo;
    struct SDirSeqInfo {
        CSeq_id_Handle m_SeqId;
        CBAMDataLoader::SBamFileName m_BamFileName;
        string m_BamSeqLabel;
        string m_Label;
        string m_CovFileName;
        string m_AnnotName;
    };

private:
    typedef map<string, CRef<CBamFileInfo> > TBamFiles;
    typedef vector<SDirSeqInfo> TSeqInfos;

    // mutex guarding input into the map
    mutable CMutex m_Mutex;
    CBamMgr m_Mgr;
    string  m_DirPath;
    TSeqInfos m_SeqInfos;
    TBamFiles m_BamFiles;
    AutoPtr<IIdMapper> m_IdMapper;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // OBJTOOLS_DATA_LOADERS_BAM___BAMLOADER_IMPL__HPP
