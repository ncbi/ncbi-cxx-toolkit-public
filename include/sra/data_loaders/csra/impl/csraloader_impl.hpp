#ifndef OBJTOOLS_DATA_LOADERS_CSRA___CSRALOADER_IMPL__HPP
#define OBJTOOLS_DATA_LOADERS_CSRA___CSRALOADER_IMPL__HPP

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
 * File Description: CSRA file data loader
 *
 * ===========================================================================
 */


#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>
#include <sra/data_loaders/csra/csraloader.hpp>
#include <sra/readers/sra/csraread.hpp>
#include <objtools/readers/iidmapper.hpp>
#include <util/limited_size_map.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataLoader;
class CCSRADataLoader_Impl;
class CCSRARefSeqChunkInfo;
class CCSRARefSeqInfo;
class CCSRAFileInfo;

class CCSRABlobId : public CBlobId
{
public:
    enum EBlobType {
        eBlobType_annot,
        eBlobType_refseq,
        eBlobType_reads
    };

    CCSRABlobId(const CTempString& str);
    CCSRABlobId(EBlobType blob_type,
                const CCSRAFileInfo& file,
                const CSeq_id_Handle& seq_id);
    CCSRABlobId(const CCSRAFileInfo& file,
                Uint8 first_spot_id);
    ~CCSRABlobId(void);

    EBlobType m_BlobType;
    CCSraDb::ERefIdType m_RefIdType;
    // cSRA file name or SRR accession
    string m_File;
    // Ref Seq-id for annot blobs
    // First short read Seq-id for reads' blobs
    CSeq_id_Handle m_SeqId;
    Uint8 m_FirstSpotId;

    // returns length of accession part or NPOS
    static SIZE_TYPE ParseReadId(CTempString str,
                                 Uint8* spot_id_ptr = 0,
                                 Uint4* read_id_ptr = 0);
    static bool GetGeneralSRAAccLabel(const CSeq_id_Handle& idh,
                                      string* srr_acc_ptr = 0,
                                      string* label_ptr = 0);
    static bool GetGeneralSRAAccReadId(const CSeq_id_Handle& idh,
                                       string* srr_acc_ptr = 0,
                                       Uint8* spot_id_ptr = 0,
                                       Uint4* read_id_ptr = 0);

    enum EGeneralIdType {
        eNotGeneralIdType      = 0,
        eGeneralIdType_refseq  = 1<<0,
        eGeneralIdType_read    = 1<<1,
        eGeneralIdType_both    = eGeneralIdType_refseq|eGeneralIdType_read
    };
    static EGeneralIdType GetGeneralIdType(const CSeq_id_Handle& idh,
                                           EGeneralIdType allow_type,
                                           const string* srr = 0);
    static EGeneralIdType GetGeneralIdType(const CSeq_id_Handle& idh,
                                           EGeneralIdType allow_type,
                                           const string& srr)
    {
        return GetGeneralIdType(idh, allow_type, &srr);
    }

    // string blob id representation:
    // eBlobType_annot_plain_id
    string ToString(void) const;
    void FromString(CTempString str);

    bool operator<(const CBlobId& id) const;
    bool operator==(const CBlobId& id) const;
};


class CCSRARefSeqChunkInfo
{
public:
    CCSRARefSeqChunkInfo(void)
        : m_AlignCount(0), m_MaxRefSeqFrom(0)
        {
        }

    size_t GetAlignCount(void) const
        {
            return m_AlignCount;
        }

    typedef CRange<TSeqPos> TRange;

    void AddRefSeqRange(const TRange& range);
    const TRange& GetRefSeqRange(void) const
        {
            return m_RefSeqRange;
        }
    TSeqPos GetMaxRefSeqFrom(void) const
        {
            return m_MaxRefSeqFrom;
        }

protected:
    friend class CCSRARefSeqInfo;

    size_t m_AlignCount; // 0 - graph only
    TRange m_RefSeqRange; // total range of alignments in the chunk
    TSeqPos m_MaxRefSeqFrom; // max 'from' coordinate of alignments
};


enum ECSRAAnnotChunkIdType {
    eCSRAAnnotChunk_align,
    eCSRAAnnotChunk_pileup_graph,
    eCSRAAnnotChunk_mul
};

    
class CCSRARefSeqInfo : public CObject
{
public:
    CCSRARefSeqInfo(CCSRAFileInfo* csra_file,
                    const CSeq_id_Handle& seq_id);

    const CSeq_id_Handle& GetRefSeqId(void) const
        {
            return m_RefSeqId;
        }

    CCSraRefSeqIterator GetRefSeqIterator(void) const;

    CRef<CCSRABlobId> GetBlobId(CCSRABlobId::EBlobType type) const;
    int GetAnnotChunkId(TSeqPos ref_pos) const;

    void LoadRanges(void);

    void LoadAnnotBlob(CTSE_LoadLock& load_lock);
    void LoadAnnotChunk(CTSE_Chunk_Info& chunk_info);

    void LoadAnnotMainSplit(CTSE_LoadLock& load_lock);
    void LoadAnnotMainChunk(CTSE_Chunk_Info& chunk_info);
    void LoadAnnotAlignChunk(CTSE_Chunk_Info& chunk_info);
    void LoadAnnotPileupChunk(CTSE_Chunk_Info& chunk_info);

    void LoadRefSeqBlob(CTSE_LoadLock& load_lock);
    void LoadRefSeqChunk(CTSE_Chunk_Info& chunk_info);

    void LoadRefSeqMainEntry(CTSE_LoadLock& load_lock);

    void SetBlobId(CRef<CCSRABlobId>& ret,
                   CCSRABlobId::EBlobType blob_type,
                   const CSeq_id_Handle& idh) const;

protected:
    friend class CCSRADataLoader_Impl;

    typedef CRange<TSeqPos> TRange;
    typedef vector<CCSRARefSeqChunkInfo> TChunks;
    typedef map<CSeq_id_Handle, int> TSeq2Chunk;

    void x_LoadRangesScan(void);
    void x_LoadRangesStat(void);
    bool x_LoadRangesCov(void);

    CCSRAFileInfo* m_File;
    CSeq_id_Handle m_RefSeqId;
    CRef<CSeq_annot> m_CovAnnot;
    int m_MinMapQuality;
    TChunks m_Chunks;
    bool m_LoadedRanges;
    TSeq2Chunk m_Seq2Chunk;
};


class CCSRAFileInfo : public CObject
{
public:
    CCSRAFileInfo(CCSRADataLoader_Impl& impl,
                  const string& csra,
                  CCSraDb::ERefIdType ref_id_type);
    
    const string& GetCSRAName(void) const
        {
            return m_CSRAName;
        }
    const string& GetBaseAnnotName(void) const
        {
            return m_AnnotName;
        }
    string GetAnnotName(const string& spot_group,
                        ECSRAAnnotChunkIdType type) const;
    string GetAlignAnnotName(void) const;
    string GetAlignAnnotName(const string& spot_group) const;
    string GetPileupAnnotName(void) const;
    string GetPileupAnnotName(const string& spot_group) const;

    CCSraDb::ERefIdType GetRefIdType(void) const
        {
            return m_RefIdType;
        }
    int GetMinMapQuality(void) const
        {
            return m_MinMapQuality;
        }

    bool IsValidReadId(Uint8 spot_id, Uint4 read_id,
                       CRef<CCSRARefSeqInfo>* ref_ptr = 0,
                       TSeqPos* ref_pos_ptr = 0);
    CRef<CCSRABlobId> GetReadsBlobId(Uint8 spot_id) const;

    void GetAnnotBlobId(CRef<CCSRABlobId>& ret,
                        const CSeq_id_Handle& idh);
    void GetRefSeqBlobId(CRef<CCSRABlobId>& ret,
                         const CSeq_id_Handle& idh);
    void GetShortSeqBlobId(CRef<CCSRABlobId>& ret,
                           const CSeq_id_Handle& idh) const;

    CRef<CCSRARefSeqInfo> GetRefSeqInfo(const CSeq_id_Handle& seq_id);
    CRef<CCSRARefSeqInfo> GetRefSeqInfo(const CCSRABlobId& blob_id)
        {
            return GetRefSeqInfo(blob_id.m_SeqId);
        }

    CMutex& GetMutex(void) const
        {
            return m_CSRAMutex;
        }

    CCSraDb& GetDb(void)
        {
            return m_CSRADb;
        }
    operator CCSraDb&(void)
        {
            return GetDb();
        }

    void AddRefSeq(const string& refseq_label,
                   const CSeq_id_Handle& refseq_id);

    const vector<string>& GetSeparateSpotGroups(void) const
        {
            return m_SeparateSpotGroups;
        }

    typedef CCSRADataLoader::TAnnotNames TAnnotNames;
    void GetPossibleAnnotNames(TAnnotNames& names) const;
    
    void LoadReadsBlob(const CCSRABlobId& blob_id,
                       CTSE_LoadLock& load_lock);
    void LoadReadsChunk(const CCSRABlobId& blob_id,
                        CTSE_Chunk_Info& chunk_info);

    void LoadReadsMainEntry(const CCSRABlobId& blob_id,
                            CTSE_LoadLock& load_lock);

protected:
    friend class CCSRADataLoader_Impl;

    typedef map<CSeq_id_Handle, CRef<CCSRARefSeqInfo> > TRefSeqs;

    void x_Initialize(CCSRADataLoader_Impl& impl,
                      const string& csra,
                      CCSraDb::ERefIdType ref_id_type);

    string m_CSRAName;
    CCSraDb::ERefIdType m_RefIdType;
    string m_AnnotName;
    int m_MinMapQuality;
    mutable CMutex m_CSRAMutex;
    CCSraDb m_CSRADb;
    vector<string> m_SeparateSpotGroups;
    TRefSeqs m_RefSeqs;
};


class CCSRADataLoader_Impl : public CObject
{
public:
    CCSRADataLoader_Impl(const CCSRADataLoader::SLoaderParams& params);
    ~CCSRADataLoader_Impl(void);

    void AddSrzDef(void);
    void AddCSRAFile(const string& csra);
    CRef<CCSRAFileInfo> GetSRRFile(const string& acc);

    int GetMinMapQuality(void) const
        {
            return m_MinMapQuality;
        }

    CRef<CCSRARefSeqInfo> GetRefSeqInfo(const CSeq_id_Handle& idh);
    CRef<CCSRAFileInfo> GetReadsFileInfo(const CSeq_id_Handle& idh,
                                         Uint8* spot_id_ptr = 0,
                                         Uint4* read_id_ptr = 0,
                                         CRef<CCSRARefSeqInfo>* ref_ptr = 0,
                                         TSeqPos* ref_pos_ptr = 0);
    CRef<CCSRAFileInfo> GetFileInfo(const CCSRABlobId& blob_id);
    CCSraRefSeqIterator GetRefSeqIterator(const CSeq_id_Handle& idh);
    CCSraShortReadIterator GetShortReadIterator(const CSeq_id_Handle& idh);

    CRef<CCSRABlobId> GetShortSeqBlobId(const CSeq_id_Handle& idh);

    CRef<CCSRABlobId> GetRefSeqBlobId(const CSeq_id_Handle& idh);

    CDataLoader::TTSE_LockSet GetRecords(CDataSource* data_source,
                                         const CSeq_id_Handle& idh,
                                         CDataLoader::EChoice choice);
    CRef<CCSRABlobId> GetBlobId(const CSeq_id_Handle& idh);
    CTSE_LoadLock GetBlobById(CDataSource* data_source,
                              const CCSRABlobId& blob_id);
    void LoadBlob(const CCSRABlobId& blob_id,
                  CTSE_LoadLock& load_lock);
    void LoadChunk(const CCSRABlobId& blob_id,
                   CTSE_Chunk_Info& chunk);

    typedef CCSRADataLoader::TAnnotNames TAnnotNames;
    TAnnotNames GetPossibleAnnotNames(void) const;

    typedef vector<CSeq_id_Handle> TIds;
    void GetIds(const CSeq_id_Handle& idh, TIds& ids);
    CSeq_id_Handle GetAccVer(const CSeq_id_Handle& idh);
    int GetGi(const CSeq_id_Handle& idh);
    string GetLabel(const CSeq_id_Handle& idh);
    int GetTaxId(const CSeq_id_Handle& idh);
    TSeqPos GetSequenceLength(const CSeq_id_Handle& idh);
    CSeq_inst::TMol GetSequenceType(const CSeq_id_Handle& idh);

protected:
    friend class CCSRAFileInfo;
    struct SDirSeqInfo {
        CSeq_id_Handle m_SeqId;
        string m_CSRAFileName;
        string m_CSRASeqLabel;
        string m_Label;
    };

private:
    // first:
    //   false if explicitly listed file in the loader params
    //   true if dynamically loaded SRA
    // second: SRA accession or csra file path
        
    typedef map<string, CRef<CCSRAFileInfo> > TFixedFiles;
    typedef limited_size_map<string, CRef<CCSRAFileInfo> > TSRRFiles;

    // mutex guarding input into the map
    mutable CMutex  m_Mutex;
    CVDBMgr m_Mgr;
    string  m_DirPath;
    int m_MinMapQuality;
    TFixedFiles m_FixedFiles;
    TSRRFiles m_SRRFiles;
    AutoPtr<IIdMapper> m_IdMapper;
    string m_AnnotName;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // OBJTOOLS_DATA_LOADERS_CSRA___CSRALOADER_IMPL__HPP
