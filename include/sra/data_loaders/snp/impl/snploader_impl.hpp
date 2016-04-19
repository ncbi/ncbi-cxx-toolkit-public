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

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataLoader;
class CSNPDataLoader_Impl;
class CSNPSeqChunkInfo;
class CSNPSeqInfo;
class CSNPFileInfo;

class CSNPBlobId : public CBlobId
{
public:
    explicit
    CSNPBlobId(const CTempString& str);
    CSNPBlobId(const CSNPFileInfo& file,
               const CSeq_id_Handle& seq_id);
    ~CSNPBlobId(void);

    // SNP file name or VDB accession
    string m_File;
    // Ref Seq-id for annot blobs
    CSeq_id_Handle m_SeqId;

    // string blob id representation:
    // eBlobType_annot_plain_id
    string ToString(void) const;
    void FromString(CTempString str);

    bool operator<(const CBlobId& id) const;
    bool operator==(const CBlobId& id) const;
};


enum ESNPAnnotChunkIdType {
    eSNPAnnotChunk_graph,
    eSNPAnnotChunk_snp
};

    
class CSNPSeqInfo : public CObject
{
public:
    CSNPSeqInfo(CSNPFileInfo* file,
                const CSeq_id_Handle& seq_id);

    const CSeq_id_Handle& GetSeqId(void) const
        {
            return m_SeqId;
        }

    CSNPDbSeqIterator GetSeqIterator(void) const;

    CRef<CSNPBlobId> GetAnnotBlobId(void) const;
    int GetAnnotChunkId(TSeqPos ref_pos) const;

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
    void SetBlobId(CRef<CSNPBlobId>& ret,
                   const CSeq_id_Handle& idh) const;

protected:
    friend class CSNPDataLoader_Impl;

    // start of chunk and number of alignments in the chunk
    struct SChunkInfo {
        TSeqPos start_pos;
        unsigned align_count;

        bool operator()(TSeqPos pos, const SChunkInfo& chunk) const
            { return pos < chunk.start_pos; }
    };
    typedef vector<SChunkInfo> TChunks;

    void x_LoadRangesStat(void);

    CSNPFileInfo* m_File;
    CSeq_id_Handle m_SeqId;
    CRef<CSeq_annot> m_CovAnnot;
    TChunks m_AlignChunks;
    TChunks m_GraphChunks;
};


class CSNPFileInfo : public CObject
{
public:
    CSNPFileInfo(CSNPDataLoader_Impl& impl,
                  const string& file_name);
    
    const string& GetFileName(void) const
        {
            return m_FileName;
        }
    const string& GetBaseAnnotName(void) const
        {
            return m_AnnotName;
        }
    string GetSNPAnnotName(void) const;
    string GetGraphAnnotName(void) const;

    CRef<CSNPBlobId> GetAnnotBlobId(const CSeq_id_Handle& id) const;

    CRef<CSNPSeqInfo> GetSeqInfo(const CSeq_id_Handle& seq_id);
    CRef<CSNPSeqInfo> GetSeqInfo(const CSNPBlobId& blob_id)
        {
            return GetSeqInfo(blob_id.m_SeqId);
        }

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

    typedef map<CSeq_id_Handle, CRef<CSNPSeqInfo> > TSeqs;

    void x_Initialize(CSNPDataLoader_Impl& impl,
                      const string& file_name);

    string m_FileName;
    string m_AnnotName;
    mutable CMutex m_SNPMutex;
    CSNPDb m_SNPDb;
    TSeqs m_Seqs;
};


class CSNPDataLoader_Impl : public CObject
{
public:
    explicit CSNPDataLoader_Impl(const CSNPDataLoader::SLoaderParams& params);
    ~CSNPDataLoader_Impl(void);

    void AddFixedFile(const string& file_name);
    
    CRef<CSNPFileInfo> GetFixedFile(const string& acc);
    CRef<CSNPFileInfo> FindFile(const string& acc);

    CRef<CSNPFileInfo> GetFileInfo(const CSNPBlobId& blob_id);

    CDataLoader::TTSE_LockSet GetRecords(CDataSource* data_source,
                                         const CSeq_id_Handle& idh,
                                         CDataLoader::EChoice choice);
    CDataLoader::TTSE_LockSet GetOrphanAnnotRecords(CDataSource* ds,
                                                    const CSeq_id_Handle& idh,
                                                    const SAnnotSelector* sel);

    CTSE_LoadLock GetBlobById(CDataSource* data_source,
                              const CSNPBlobId& blob_id);
    void LoadBlob(const CSNPBlobId& blob_id,
                  CTSE_LoadLock& load_lock);
    void LoadChunk(const CSNPBlobId& blob_id,
                   CTSE_Chunk_Info& chunk);

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
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // OBJTOOLS_DATA_LOADERS_SNP___SNPLOADER_IMPL__HPP
