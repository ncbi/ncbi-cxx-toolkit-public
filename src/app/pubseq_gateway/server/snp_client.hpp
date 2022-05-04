#ifndef SNP_CLIENT__HPP
#define SNP_CLIENT__HPP

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
 * Authors: Aleksey Grichenko, Eugene Vasilchenko
 *
 * File Description: client for reading SNP data
 *
 */

#include "psgs_request.hpp"
#include <sra/readers/sra/snpread.hpp>
#include <objects/dbsnp/primary_track/snpptis.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <util/limited_size_map.hpp>


BEGIN_NCBI_NAMESPACE;

BEGIN_NAMESPACE(objects);
class CID2_Blob_Id;
class CID2S_Seq_annot_Info;
class CID2S_Split_Info;
class CID2S_Chunk;
class CSeq_id_Handle;
class CSeq_entry;
END_NAMESPACE(objects);

BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(snp);


class CSNPFileInfo;
class CSNPClient;


struct SSNPProcessor_Config
{
    size_t m_GCSize = 10;
    size_t m_MissingGCSize = 10000;
    bool m_Split = true;
    string m_AnnotName;
    bool m_AddPTIS = true;
    vector<string> m_VDBFiles;
};


struct SSNPData
{
    typedef vector<CRef<objects::CID2S_Seq_annot_Info>> TAnnotInfo;

    string     m_BlobId;
    string     m_Name;
    TAnnotInfo m_AnnotInfo;

    CRef<objects::CSeq_entry> m_TSE;
    CRef<objects::CID2S_Split_Info> m_SplitInfo;
    CRef<objects::CID2S_Chunk> m_Chunk;
    int m_SplitVersion = 0;
};


class CSNPBlobId
{
public:
    explicit CSNPBlobId(const CTempString& str);
    CSNPBlobId(const CSNPFileInfo& file, const objects::CSeq_id_Handle& seq_id, size_t filter_index);
    CSNPBlobId(const objects::CSNPDbSeqIterator& seq, size_t filter_index);
    CSNPBlobId(const CSNPFileInfo& file, size_t seq_index, size_t filter_index);
    ~CSNPBlobId(void);

    string ToString(void) const;
    void FromString(CTempString str);
    bool FromSatString(CTempString str);

    bool IsSatId(void) const;

    Int4 GetSat(void) const;
    Int4 GetSubSat(void) const;
    Int4 GetSatKey(void) const;

    bool IsValidSat(void) const;
    bool IsValidSubSat(void) const;
    bool IsValidSatKey(void) const;

    static bool IsValidNAIndex(size_t index);
    static bool IsValidNAVersion(size_t version);
    static bool IsValidNA(pair<size_t, size_t> na)
    {
        return IsValidNAIndex(na.first) && IsValidNAVersion(na.second);
    }
    static bool IsValidSeqIndex(size_t seq_index);
    static bool IsValidFilterIndex(size_t filter_index);

    static pair<size_t, size_t> ParseNA(CTempString acc);
    static bool IsValidNA(CTempString acc)
    {
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

    objects::CSeq_id_Handle GetSeqId(void) const;
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
    objects::CSeq_id_Handle m_SeqId;
};


enum ESNPAnnotChunkIdType {
    eSNPAnnotChunk_graph,
    eSNPAnnotChunk_snp
};


class CSNPSeqInfo : public CObject
{
public:
    CSNPSeqInfo(CSNPFileInfo* file, const objects::CSNPDbSeqIterator& it);

    objects::CSNPDbSeqIterator GetSeqIterator(void) const;

    CSNPBlobId GetAnnotBlobId(void) const;
    int GetAnnotChunkId(TSeqPos ref_pos) const;

    string GetAnnotName(void) const;

    void LoadRanges(void);

    CSNPBlobId GetBlobId(void) const;
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

    void LoadBlob(SSNPData& data, bool split_enabled);
    void LoadChunk(SSNPData& data, int chunk_id);

protected:
    friend class CSNPClient;

    CSNPFileInfo* m_File;
    size_t m_SeqIndex;
    size_t m_FilterIndex;
    objects::CSeq_id_Handle m_SeqId;
    bool m_IsPrimaryTrack;
    bool m_IsPrimaryTrackGraph;
};


class CSNPFileInfo : public CObject
{
public:
    CSNPFileInfo(CSNPClient& client, const string& file_name);

    bool IsValidNA(void) const
    {
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

    CSNPBlobId GetAnnotBlobId(const objects::CSeq_id_Handle& id) const;

    CRef<CSNPSeqInfo> GetSeqInfo(const objects::CSeq_id_Handle& seq_id);
    CRef<CSNPSeqInfo> GetSeqInfo(size_t seq_index);
    CRef<CSNPSeqInfo> GetSeqInfo(const CSNPBlobId& blob_id);

    objects::CSNPDb& GetDb(void)
    {
        return m_SNPDb;
    }
    operator objects::CSNPDb& (void)
    {
        return GetDb();
    }

    void AddSeq(const objects::CSeq_id_Handle& id);

protected:
    friend class CSNPClient;

    typedef map<objects::CSeq_id_Handle, CRef<CSNPSeqInfo> > TSeqById;
    typedef map<size_t, CRef<CSNPSeqInfo> > TSeqByIdx;

    void x_Initialize(CSNPClient& client, const string& file_name);

    bool m_IsValidNA;
    string m_FileName; // external VDB file access string
    string m_Accession; // OM named annot accession (without filter index)
    string m_AnnotName; // OM annot name (without filter index)
    objects::CSNPDb m_SNPDb;
    TSeqById m_SeqById;
    TSeqByIdx m_SeqByIdx;
};


class CSNPClient
{
public:
    CSNPClient(const SSNPProcessor_Config& config);
    ~CSNPClient(void);

    bool CanProcessRequest(CPSGS_Request& request, TProcessorPriority priority) const;

    vector<SSNPData> GetAnnotInfo(const objects::CSeq_id_Handle& id, const vector<string>& names);
    SSNPData GetBlobByBlobId(const string& blob_id);
    SSNPData GetChunk(const string& id2info, int chunk_id);

    void AddFixedFile(const string& file);
    CRef<CSNPFileInfo> GetFixedFile(const string& acc);
    CRef<CSNPFileInfo> FindFile(const string& acc);
    CRef<CSNPFileInfo> GetFileInfo(const string& acc);
    CRef<CSNPSeqInfo> GetSeqInfo(const CSNPBlobId& blob_id);

    static objects::CSeq_id_Handle GetRequestSeq_id(const SPSGS_AnnotRequest& request);

private:
    friend class CSNPFileInfo;

    typedef map<string, CRef<CSNPFileInfo> > TFixedFiles;
    typedef limited_size_map<string, CRef<CSNPFileInfo> > TFoundFiles;
    typedef limited_size_map<string, bool> TMissingFiles;

    CRef<objects::CID2S_Seq_annot_Info> x_GetFeatInfo(const string& name, const objects::CSeq_id_Handle& id);
    CRef<objects::CID2S_Seq_annot_Info> x_GetGraphInfo(const string& name, const objects::CSeq_id_Handle& id);

    SSNPProcessor_Config m_Config;
    shared_ptr<objects::CVDBMgr> m_Mgr;
    CRef<objects::CSnpPtisClient> m_PTISClient;
    CMutex m_Mutex;
    TFixedFiles m_FixedFiles;
    TFoundFiles m_FoundFiles;
    TMissingFiles m_MissingFiles;
};


END_NAMESPACE(snp);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;

#endif  // CDD_PROCESSOR__HPP
