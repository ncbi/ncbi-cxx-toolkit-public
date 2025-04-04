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

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/general/general__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqres/seqres__.hpp>

#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/tse_loadlock.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/tse_split_info.hpp>
#include <objmgr/data_loader_factory.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>
#include <objtools/readers/idmapper.hpp>

#include <sra/error_codes.hpp>
#include <sra/readers/ncbi_traces_path.hpp>
#include <sra/readers/bam/bamgraph.hpp>
#include <sra/data_loaders/bam/bamloader.hpp>
#include <sra/data_loaders/bam/impl/bamloader_impl.hpp>

#include <util/sequtil/sequtil_manip.hpp>

#include <algorithm>
#include <numeric>
#include <cmath>

BEGIN_NCBI_SCOPE

#define NCBI_USE_ERRCODE_X   BAMLoader
NCBI_DEFINE_ERR_SUBCODE_X(30);

BEGIN_SCOPE(objects)

class CDataLoader;

static const int kTSEId = 1;
static const Uint8 kSingleAlignBytes = 60; // estimated size of a single alignment in bytes
//static const size_t kGraphScale = 1000;
//static const size_t kGraphPoints = 20;
static const size_t kChunkDataSize = 250000; // target chunk size in bytes
static const size_t kChunkSize = kChunkDataSize/kSingleAlignBytes; // target chunk align count
static const int kMainChunkId = CTSE_Chunk_Info::kDelayedMain_ChunkId;
static const size_t kSplitLevelsChunkDataSize = 2*kChunkDataSize; // split >0 index levels if size in bytes is bigger

// With raw index BAM we can position within index bin, so we can split
// huge bins into smaller pieces.
// We limit the splitting by both minimal annot chunk range (to avoid
// too many alignments to overlap with the next chunk), and by expected
// alignment count in a single chunk.
// parameters for sub-page splitting, available only with raw BAM index access
static const Uint8 kDefaultSplitBinDataSize = 4*kChunkDataSize; // split if size in bytes is bigger
static const TSeqPos kDefaultSplitBinMinLength = 2048; // stop splitting if ref seq range is this small

//#define SKIP_TOO_LONG_ALIGNMENTS
#define SEPARATE_PILEUP_READS

// Regular chunks have size of one or more whole index bins.
// Extremely populated bins may be split into smaller regions.
// The pileup chunk for such smaller regions still covers whole bin.
// Since the pileup chunk id is smaller than any other chunks for
// the same region it will be loaded first by OM.

// During the pileup graph generation we remember where each
// sub-range alignments start in the BAM file, and load all alignments
// that cross sub-range borders.
// So, the sub-range chunks have only alignments that are fully within
// their range for OM to index.
// All border crossing alignments will be loaded by the pileup chunk so
// it's reported to OM as having alignments for the whole range.

// When chunks for highly populated bin are generated we first split
// alignments by their BAM index level, then if necessary split the bin
// into smaller chunks.

// Regular chunks (eChunk_align) and chunks with alignments from higher
// index levels (eChunk_align2) are gathered by their starting positions.
// Their total alignment ranges (for OM indexing) may go beyond their
// range end.

// The chunks with alignments from BAM index level =0 (eChunk_align1)
// are gathered only if they end also within the chunk range, so their
// OM indexing ranges do not need to be extended

enum EChunkIdType {
    eChunk_pileup_graph, // pileup graphs over the range
    eChunk_align, // all alignments starting in range
    eChunk_align1, // alignments from BAM index level =0 within the range
    eChunk_align2, // alignments from BAM index levels >0 starting in range
    eChunk_short_seq_pileup, // reads for alignments loaded with pileup graph
    eChunk_short_seq, // reads corresponding to eChunk_align
    eChunk_short_seq1, // reads corresponding to eChunk_align1
    eChunk_short_seq2, // reads corresponding to eChunk_align2
    kChunkIdMul
};

#define PILEUP_NAME_SUFFIX "pileup graphs"

NCBI_PARAM_DECL(int, BAM_LOADER, DEBUG);
NCBI_PARAM_DEF_EX(int, BAM_LOADER, DEBUG, 0,
                  eParam_NoThread, BAM_LOADER_DEBUG);

static int GetDebugLevel(void)
{
    static int value = NCBI_PARAM_TYPE(BAM_LOADER, DEBUG)::GetDefault();
    return value;
}


NCBI_PARAM_DECL(string, BAM_LOADER, MAPPER_FILE);
NCBI_PARAM_DEF_EX(string, BAM_LOADER, MAPPER_FILE, "",
                  eParam_NoThread, BAM_LOADER_MAPPER_FILE);

static string GetMapperFileName(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(BAM_LOADER, MAPPER_FILE)> s_Value;
    return s_Value->Get();
}


NCBI_PARAM_DECL(string, BAM_LOADER, MAPPER_CONTEXT);
NCBI_PARAM_DEF_EX(string, BAM_LOADER, MAPPER_CONTEXT, "",
                  eParam_NoThread, BAM_LOADER_MAPPER_CONTEXT);

static string GetMapperContext(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(BAM_LOADER, MAPPER_CONTEXT)> s_Value;
    return s_Value->Get();
}


NCBI_PARAM_DECL(bool, BAM_LOADER, PILEUP_GRAPHS);
NCBI_PARAM_DEF(bool, BAM_LOADER, PILEUP_GRAPHS, true);

bool CBAMDataLoader::GetPileupGraphsParamDefault(void)
{
    return NCBI_PARAM_TYPE(BAM_LOADER, PILEUP_GRAPHS)::GetDefault();
}


void CBAMDataLoader::SetPileupGraphsParamDefault(bool param)
{
    NCBI_PARAM_TYPE(BAM_LOADER, PILEUP_GRAPHS)::SetDefault(param);
}


static inline bool GetPileupGraphsParam(void)
{
    return CBAMDataLoader::GetPileupGraphsParamDefault();
}


NCBI_PARAM_DECL(bool, BAM_LOADER, SKIP_EMPTY_PILEUP_GRAPHS);
NCBI_PARAM_DEF(bool, BAM_LOADER, SKIP_EMPTY_PILEUP_GRAPHS, true);

bool CBAMDataLoader::GetSkipEmptyPileupGraphsParamDefault(void)
{
    return NCBI_PARAM_TYPE(BAM_LOADER, SKIP_EMPTY_PILEUP_GRAPHS)::GetDefault();
}


void CBAMDataLoader::SetSkipEmptyPileupGraphsParamDefault(bool param)
{
    NCBI_PARAM_TYPE(BAM_LOADER, SKIP_EMPTY_PILEUP_GRAPHS)::SetDefault(param);
}


static inline bool GetSkipEmptyPileupGraphsParam(void)
{
    return CBAMDataLoader::GetSkipEmptyPileupGraphsParamDefault();
}


NCBI_PARAM_DECL(int, BAM_LOADER, GAP_TO_INTRON_THRESHOLD);
NCBI_PARAM_DEF(int, BAM_LOADER, GAP_TO_INTRON_THRESHOLD, -1);

static TSeqPos s_GetGapToIntronThreshold(void)
{
    static TSeqPos value = NCBI_PARAM_TYPE(BAM_LOADER, GAP_TO_INTRON_THRESHOLD)::GetDefault();
    return value;
}


NCBI_PARAM_DECL(bool, BAM_LOADER, INTRON_GRAPH);
NCBI_PARAM_DEF(bool, BAM_LOADER, INTRON_GRAPH, false);

static bool s_GetMakeIntronGraph(void)
{
    static TSeqPos value = NCBI_PARAM_TYPE(BAM_LOADER, INTRON_GRAPH)::GetDefault();
    return value;
}


NCBI_PARAM_DECL(bool, BAM_LOADER, ESTIMATED_COVERAGE_GRAPH);
NCBI_PARAM_DEF(bool, BAM_LOADER, ESTIMATED_COVERAGE_GRAPH, true);

bool CBAMDataLoader::GetEstimatedCoverageGraphParamDefault(void)
{
    return NCBI_PARAM_TYPE(BAM_LOADER, ESTIMATED_COVERAGE_GRAPH)::GetDefault();
}


void CBAMDataLoader::SetEstimatedCoverageGraphParamDefault(bool param)
{
    NCBI_PARAM_TYPE(BAM_LOADER, ESTIMATED_COVERAGE_GRAPH)::SetDefault(param);
}


static inline bool GetEstimatedCoverageGraphParam(void)
{
    return CBAMDataLoader::GetEstimatedCoverageGraphParamDefault();
}


NCBI_PARAM_DECL(bool, BAM_LOADER, PREFER_RAW_INDEX_OVER_COVERAGE_GRAPH);
NCBI_PARAM_DEF_EX(bool, BAM_LOADER, PREFER_RAW_INDEX_OVER_COVERAGE_GRAPH, false,
                  eParam_NoThread, BAM_LOADER_PREFER_RAW_INDEX_OVER_COVERAGE_GRAPH);

static bool GetPreferRawIndexOverCoverageGraphParam(void)
{
    return NCBI_PARAM_TYPE(BAM_LOADER, PREFER_RAW_INDEX_OVER_COVERAGE_GRAPH)::GetDefault();
}


NCBI_PARAM_DECL(bool, BAM_LOADER, PREOPEN);
NCBI_PARAM_DEF(bool, BAM_LOADER, PREOPEN, false);

bool CBAMDataLoader::GetPreOpenParam(void)
{
    return NCBI_PARAM_TYPE(BAM_LOADER, PREOPEN)::GetDefault();
}


void CBAMDataLoader::SetPreOpenParam(bool param)
{
    NCBI_PARAM_TYPE(BAM_LOADER, PREOPEN)::SetDefault(param);
}


NCBI_PARAM_DECL(string, BAM_LOADER, INCLUDE_ALIGN_TAGS);
NCBI_PARAM_DEF_EX(string, BAM_LOADER, INCLUDE_ALIGN_TAGS, "",
                  eParam_NoThread, "");

string CBAMDataLoader::GetIncludeAlignTagsParamDefault(void)
{
    return NCBI_PARAM_TYPE(BAM_LOADER, INCLUDE_ALIGN_TAGS)::GetDefault();
}


void CBAMDataLoader::SetIncludeAlignTagsParamDefault(const string& param)
{
    NCBI_PARAM_TYPE(BAM_LOADER, INCLUDE_ALIGN_TAGS)::SetDefault(param);
}


static inline string GetIncludeAlignTagsParam(void)
{
    return CBAMDataLoader::GetIncludeAlignTagsParamDefault();
}


NCBI_PARAM_DECL(int, BAM_LOADER, MIN_MAP_QUALITY);
NCBI_PARAM_DEF(int, BAM_LOADER, MIN_MAP_QUALITY, 1);

int CBAMDataLoader::GetMinMapQualityParamDefault(void)
{
    return NCBI_PARAM_TYPE(BAM_LOADER, MIN_MAP_QUALITY)::GetDefault();
}


void CBAMDataLoader::SetMinMapQualityParamDefault(int param)
{
    NCBI_PARAM_TYPE(BAM_LOADER, MIN_MAP_QUALITY)::SetDefault(param);
}


static inline bool GetMinMapQualityParam(void)
{
    return CBAMDataLoader::GetMinMapQualityParamDefault();
}


NCBI_PARAM_DECL(Uint8, BAM_LOADER, SPLIT_BIN_DATA_SIZE);
NCBI_PARAM_DEF_EX(Uint8, BAM_LOADER, SPLIT_BIN_DATA_SIZE,
                  kDefaultSplitBinDataSize,
                  eParam_NoThread, BAM_LOADER_SPLIT_BIN_DATA_SIZE);

static inline Uint8 GetSplitBinDataSize(void)
{
    return NCBI_PARAM_TYPE(BAM_LOADER, SPLIT_BIN_DATA_SIZE)::GetDefault();
}


NCBI_PARAM_DECL(TSeqPos, BAM_LOADER, SPLIT_BIN_MIN_LENGTH);
NCBI_PARAM_DEF_EX(TSeqPos, BAM_LOADER, SPLIT_BIN_MIN_LENGTH,
                  kDefaultSplitBinMinLength,
                  eParam_NoThread, BAM_LOADER_SPLIT_BIN_MIN_LENGTH);

static inline TSeqPos GetSplitBinMinLength(void)
{
    return NCBI_PARAM_TYPE(BAM_LOADER, SPLIT_BIN_MIN_LENGTH)::GetDefault();
}


template<class Call>
typename std::invoke_result<Call>::type
CallWithRetry(Call&& call,
              const char* name,
              int retry_count = 0)
{
    const int kDefaultRetryCount = 4;
    if ( retry_count == 0 ) {
        retry_count = kDefaultRetryCount;
    }
    for ( int t = 1; t < retry_count; ++ t ) {
        try {
            return call();
        }
        catch ( CBlobStateException& ) {
            // no retry
            throw;
        }
        catch ( CException& exc ) {
            LOG_POST(Warning<<name<<"() try "<<t<<" exception: "<<exc);
        }
        catch ( exception& exc ) {
            LOG_POST(Warning<<name<<"() try "<<t<<" exception: "<<exc.what());
        }
        catch ( ... ) {
            LOG_POST(Warning<<name<<"() try "<<t<<" exception");
        }
        if ( t >= 2 ) {
            //double wait_sec = m_WaitTime.GetTime(t-2);
            double wait_sec = 1;
            LOG_POST(Warning<<name<<"(): waiting "<<wait_sec<<"s before retry");
            SleepMilliSec(Uint4(wait_sec*1000));
        }
    }
    return call();
}
#define RETRY(expr) CallWithRetry([&]()->auto{return (expr);}, #expr)


/////////////////////////////////////////////////////////////////////////////
// CBAMBlobId
/////////////////////////////////////////////////////////////////////////////

CBAMBlobId::CBAMBlobId(const CTempString& str)
{
    SIZE_TYPE div = str.rfind('/');
    m_BamName = str.substr(0, div);
    CSeq_id id(str.substr(div+1));
    m_SeqId = CSeq_id_Handle::GetHandle(id);
}


CBAMBlobId::CBAMBlobId(const string& bam_name, const CSeq_id_Handle& seq_id)
    : m_BamName(bam_name), m_SeqId(seq_id)
{
}


CBAMBlobId::~CBAMBlobId(void)
{
}


string CBAMBlobId::ToString(void) const
{
    return m_BamName+'/'+m_SeqId.AsString();
}


bool CBAMBlobId::operator<(const CBlobId& id) const
{
    const CBAMBlobId& bam2 = dynamic_cast<const CBAMBlobId&>(id);
    return m_SeqId < bam2.m_SeqId ||
        (m_SeqId == bam2.m_SeqId && m_BamName < bam2.m_BamName);
}


bool CBAMBlobId::operator==(const CBlobId& id) const
{
    const CBAMBlobId& bam2 = dynamic_cast<const CBAMBlobId&>(id);
    return m_SeqId == bam2.m_SeqId && m_BamName == bam2.m_BamName;
}


/////////////////////////////////////////////////////////////////////////////
// CBAMDataLoader_Impl
/////////////////////////////////////////////////////////////////////////////


CBAMDataLoader_Impl::CBAMDataLoader_Impl(
    const CBAMDataLoader::SLoaderParams& params)
    : m_IdMapper(params.m_IdMapper)
{
    if ( !m_IdMapper ) {
        string mapper_file_name = GetMapperFileName();
        if ( !mapper_file_name.empty() ) {
            CNcbiIfstream in(mapper_file_name.c_str());
            m_IdMapper.reset(new CIdMapperConfig(in, GetMapperContext(), false));
        }
    }
    CSrzPath srz_path;
    m_DirPath = srz_path.FindAccPathNoThrow(params.m_DirPath);
    if ( m_DirPath.empty() ) {
        m_DirPath = params.m_DirPath;
    }
    
    if ( !m_DirPath.empty() && *m_DirPath.rbegin() != '/' ) {
        m_DirPath += '/';
    }
    if ( params.m_BamFiles.empty() ||
         (params.m_BamFiles.size() == 1 &&
          params.m_BamFiles[0].m_BamName == SRZ_CONFIG_NAME) ) {
        AddSrzDef();
    }
    else {
        ITERATE (vector<CBAMDataLoader::SBamFileName>, it, params.m_BamFiles) {
            AddBamFile(*it);
        }
    }
    if ( CBAMDataLoader::GetPreOpenParam() ) {
        OpenBAMFiles();
    }
}


static double s_CreateTime = 0;
static double s_AttachTime = 0;


CBAMDataLoader_Impl::~CBAMDataLoader_Impl(void)
{
    if ( GetDebugLevel() >= 4 ) {
        LOG_POST_X(21, Info<<"CBAMDataLoader: "
                   "Total create time: "<<s_CreateTime);
        LOG_POST_X(22, Info<<"CBAMDataLoader: "
                   "Total attach time: "<<s_AttachTime);
    }
}


void CBAMDataLoader_Impl::AddSrzDef(void)
{
    AutoPtr<CIdMapper> mapper(new CIdMapper);

    string def_name = m_DirPath+SRZ_CONFIG_NAME;
    CNcbiIfstream in(def_name.c_str());
    if ( !in ) {
        NCBI_THROW(CLoaderException, eNoData,
                   "CBAMDataLoader: no def file: "+def_name);
    }
    string line;
    vector<string> tokens;
    while ( getline(in, line) ) {
        tokens.clear();
        NStr::Split(line, "\t", tokens);
        if ( tokens.size() < 4 ) {
            NCBI_THROW(CLoaderException, eNoData,
                       "CBAMDataLoader: bad def line: \""+line+"\"");
        }
        SDirSeqInfo info;
        info.m_BamSeqLabel = tokens[0];
        info.m_Label = tokens[1];
        if ( tokens[2].empty() ) {
            info.m_SeqId = CSeq_id_Handle::GetHandle(info.m_Label);
        }
        else {
            info.m_SeqId = CSeq_id_Handle::GetHandle(tokens[2]);
        }
        info.m_BamFileName = tokens[3];
        if ( tokens.size() >= 4 ) {
            info.m_CovFileName = tokens[4];
        }
        info.m_AnnotName = CDirEntry(info.m_BamFileName.m_BamName).GetBase();
        m_SeqInfos.push_back(info);
        CRef<CSeq_id> src_id;
        try {
            src_id = new CSeq_id(info.m_BamSeqLabel);
        }
        catch ( CException& /*ignored*/ ) {
            src_id = new CSeq_id(CSeq_id::e_Local, info.m_BamSeqLabel);
        }
        mapper->AddMapping(CSeq_id_Handle::GetHandle(*src_id), info.m_SeqId);
    }
    m_IdMapper.reset(mapper.release());
}


bool CBAMDataLoader_Impl::BAMFilesOpened() const
{
    CMutexGuard guard(m_Mutex);
    return !m_BamFiles.empty();
}


void CBAMDataLoader_Impl::OpenBAMFilesOnce()
{
    CMutexGuard guard(m_Mutex);
    if ( !m_BamFiles.empty() ) {
        return;
    }
    ITERATE ( TSeqInfos, it, m_SeqInfos ) {
        const SDirSeqInfo& info = *it;

        CRef<CBamFileInfo> bam_info;
        auto iter = m_BamFiles.find(info.m_BamFileName.m_BamName);
        if ( iter == m_BamFiles.end() ) {
            bam_info = new CBamFileInfo(*this, info.m_BamFileName,
                                        info.m_BamSeqLabel, info.m_SeqId);
            m_BamFiles[info.m_BamFileName.m_BamName] = bam_info;
        }
        else {
            bam_info = iter->second;
            bam_info->AddRefSeq(info.m_BamSeqLabel, info.m_SeqId);
        }
        if ( !info.m_CovFileName.empty() ) {
            string file_name = m_DirPath + info.m_CovFileName;
            if ( !CFile(file_name).Exists() ) {
                ERR_POST_X(2, "CBAMDataLoader: "
                           "no cov file: \""+file_name+"\"");
            }
            else {
                if ( CBamRefSeqInfo* seq_info = bam_info->GetRefSeqInfo(info.m_SeqId) ) {
                    seq_info->SetCovFileName(file_name);
                }
            }
        }
    }
}


void CBAMDataLoader_Impl::OpenBAMFiles()
{
    RETRY(OpenBAMFilesOnce());
}


void CBAMDataLoader_Impl::AddBamFile(const CBAMDataLoader::SBamFileName& bam)
{
    SDirSeqInfo info;
    info.m_BamFileName = bam;
    info.m_AnnotName = CDirEntry(info.m_BamFileName.m_BamName).GetBase();
    m_SeqInfos.push_back(info);
}


CBamRefSeqInfo* CBAMDataLoader_Impl::GetRefSeqInfo(const CBAMBlobId& blob_id)
{
    OpenBAMFiles();
    TBamFiles::iterator bit = m_BamFiles.find(blob_id.m_BamName);
    if ( bit == m_BamFiles.end() ) {
        return 0;
    }
    CBamRefSeqInfo* info = bit->second->GetRefSeqInfo(blob_id.m_SeqId);
    return info;
}


void CBAMDataLoader_Impl::LoadBAMEntry(const CBAMBlobId& blob_id,
                                       CTSE_LoadLock& load_lock)
{
    RETRY(GetRefSeqInfo(blob_id)->LoadMainSplit(load_lock));
}


void CBAMDataLoader_Impl::LoadChunk(const CBAMBlobId& blob_id,
                                    CTSE_Chunk_Info& chunk_info)
{
    RETRY(GetRefSeqInfo(blob_id)->LoadChunk(chunk_info));
}


double CBAMDataLoader_Impl::EstimateLoadSeconds(const CBAMBlobId& blob_id,
                                                const CTSE_Chunk_Info& chunk,
                                                Uint4 bytes)
{
    return GetRefSeqInfo(blob_id)->EstimateLoadSeconds(chunk, bytes);
}


CRef<CBAMBlobId>
CBAMDataLoader_Impl::GetShortSeqBlobId(const CSeq_id_Handle& idh)
{
    CRef<CBAMBlobId> ret;
    if ( BAMFilesOpened() ) {
        ITERATE ( TBamFiles, it, m_BamFiles ) {
            it->second->GetShortSeqBlobId(ret, idh);
        }
    }
    return ret;
}


CRef<CBAMBlobId>
CBAMDataLoader_Impl::GetRefSeqBlobId(const CSeq_id_Handle& idh)
{
    CRef<CBAMBlobId> ret;
    OpenBAMFiles();
    ITERATE ( TBamFiles, it, m_BamFiles ) {
        it->second->GetRefSeqBlobId(ret, idh);
    }
    return ret;
}


CBAMDataLoader::TAnnotNames CBAMDataLoader_Impl::GetPossibleAnnotNames(void) const
{
    CBAMDataLoader::TAnnotNames names;
    ITERATE ( TSeqInfos, it, m_SeqInfos ) {
        const string& name = it->m_AnnotName;
        if ( name.empty() ) {
            names.push_back(CAnnotName());
        }
        else {
            names.push_back(CAnnotName(name));
        }
    }
    sort(names.begin(), names.end());
    names.erase(unique(names.begin(), names.end()), names.end());
    return names;
}


void CBAMDataLoader_Impl::GetIds(const CSeq_id_Handle& idh, TIds& ids)
{
    if ( IsShortSeq(idh) ) {
        ids.push_back(idh);
    }
}


CDataSource::SAccVerFound
CBAMDataLoader_Impl::GetAccVer(const CSeq_id_Handle& idh)
{
    CDataSource::SAccVerFound ret;
    if ( !idh.IsGi() && idh.GetSeqId()->GetTextseq_Id() && IsShortSeq(idh) ) {
        ret.sequence_found = true;
        ret.acc_ver = idh;
    }
    return ret;
}


CDataSource::SGiFound
CBAMDataLoader_Impl::GetGi(const CSeq_id_Handle& idh)
{
    CDataSource::SGiFound ret;
    if ( idh.IsGi() && IsShortSeq(idh) ) {
        ret.sequence_found = true;
        ret.gi = idh.GetGi();
    }
    return ret;
}


string CBAMDataLoader_Impl::GetLabel(const CSeq_id_Handle& idh)
{
    if ( IsShortSeq(idh) ) {
        return objects::GetLabel(idh);
    }
    return string();
}


TTaxId CBAMDataLoader_Impl::GetTaxId(const CSeq_id_Handle& idh)
{
    if ( IsShortSeq(idh) ) {
        return ZERO_TAX_ID;
    }
    return INVALID_TAX_ID;
}


bool CBAMDataLoader_Impl::IsShortSeq(const CSeq_id_Handle& idh)
{
    return GetShortSeqBlobId(idh) != null;
}


/////////////////////////////////////////////////////////////////////////////
// CBamFileInfo
/////////////////////////////////////////////////////////////////////////////


CBamFileInfo::CBamFileInfo(const CBAMDataLoader_Impl& impl,
                           const CBAMDataLoader::SBamFileName& bam,
                           const string& refseq_label,
                           const CSeq_id_Handle& seq_id)
{
    CStopWatch sw;
    if ( GetDebugLevel() >= 1 ) {
        sw.Start();
    }
    x_Initialize(impl, bam);
    if ( seq_id ) {
        AddRefSeq(refseq_label, seq_id);
    }
    else {
        for ( CBamRefSeqIterator rit(m_BamDb); rit; ++rit ) {
            string refseq_label = rit.GetRefSeqId();
            CSeq_id_Handle seq_id = CSeq_id_Handle::GetHandle(*rit.GetRefSeq_id());
            AddRefSeq(refseq_label, seq_id);
        }
    }
    if ( GetDebugLevel() >= 1 ) {
        LOG_POST_X(16, Info<<"CBAMDataLoader: "
                   "Opened BAM file "<<bam.m_BamName<<" in "<<sw.Elapsed());
    }
}


void CBamFileInfo::x_Initialize(const CBAMDataLoader_Impl& impl,
                                const CBAMDataLoader::SBamFileName& bam)
{
    m_BamName = bam.m_BamName;
    m_AnnotName = CDirEntry(m_BamName).GetBase();
    m_BamDb = CBamDb(impl.m_Mgr,
                     impl.m_DirPath+bam.m_BamName,
                     impl.m_DirPath+(bam.m_IndexName.empty()?
                                     bam.m_BamName:
                                     bam.m_IndexName));
    if ( impl.m_IdMapper.get() ) {
        m_BamDb.SetIdMapper(impl.m_IdMapper.get(), eNoOwnership);
    }
    string include_tags = GetIncludeAlignTagsParam();
    if ( !include_tags.empty() ) {
        vector<string> tags;
        NStr::Split(include_tags, ",", tags);
        for ( auto& tag : tags ) {
            m_BamDb.IncludeAlignTag(tag);
        }
    }
}


void CBamFileInfo::AddRefSeq(const string& refseq_label,
                               const CSeq_id_Handle& refseq_id)
{
    if ( GetDebugLevel() >= 2 ) {
        LOG_POST_X(9, Info << "CBAMDataLoader(" << m_BamName << "): "
                   "Found "<<refseq_label<<" -> "<<refseq_id);
    }
    auto& slot = m_RefSeqs[refseq_id];
    if ( slot ) {
        ERR_POST_X(15, "CBAMDataLoader::AddSeqRef: "
                   "duplicate Seq-id "<<refseq_id<<" for ref "<<refseq_label<<" in "<<GetBamName());
    }
    else {
        slot = new CBamRefSeqInfo(this, refseq_label, refseq_id);
    }
}


void CBamFileInfo::GetShortSeqBlobId(CRef<CBAMBlobId>& ret,
                                     const CSeq_id_Handle& idh) const
{
    ITERATE ( TRefSeqs, it, m_RefSeqs ) {
        it->second->GetShortSeqBlobId(ret, idh);
    }
}


void CBamFileInfo::GetRefSeqBlobId(CRef<CBAMBlobId>& ret,
                                   const CSeq_id_Handle& idh) const
{
    const CBamRefSeqInfo* info = GetRefSeqInfo(idh);
    if ( info ) {
        info->SetBlobId(ret, idh);
    }
}


CBamRefSeqInfo* CBamFileInfo::GetRefSeqInfo(const CSeq_id_Handle& seq_id) const
{
    TRefSeqs::const_iterator it = m_RefSeqs.find(seq_id);
    if ( it == m_RefSeqs.end() ) {
        return 0;
    }
    return it->second.GetNCPointer();
}


/////////////////////////////////////////////////////////////////////////////
// CBamRefSeqInfo
/////////////////////////////////////////////////////////////////////////////


class CDefaultSpotIdDetector : public CObject,
                               public CBamAlignIterator::ISpotIdDetector
{
public:
    void AddSpotId(string& short_id, const CBamAlignIterator* iter) {
        string seq = iter->GetShortSequence();
        if ( iter->IsSetStrand() && IsReverse(iter->GetStrand()) ) {
            CSeqManip::ReverseComplement(seq, CSeqUtil::e_Iupacna, 0, TSeqPos(seq.size()));
        }
        CFastMutexGuard guard(m_Mutex);
        SShortSeqInfo& info = m_ShortSeqs[short_id];
        if ( info.spot1.empty() ) {
            info.spot1 = seq;
            short_id += ".1";
            return;
        }
        if ( info.spot1 == seq ) {
            short_id += ".1";
            return;
        }
        if ( info.spot2.empty() ) {
            info.spot2 = seq;
            short_id += ".2";
            return;
        }
        if ( info.spot2 == seq ) {
            short_id += ".2";
            return;
        }
        short_id += ".?";
    }

private:
    CFastMutex m_Mutex;
    
    struct SShortSeqInfo {
        string spot1, spot2;
    };
    map<string, SShortSeqInfo> m_ShortSeqs;
};

CBamRefSeqInfo::CBamRefSeqInfo(CBamFileInfo* bam_file,
                               const string& refseqid,
                               const CSeq_id_Handle& seq_id)
    : m_File(bam_file),
      m_RefSeqId(refseqid),
      m_RefSeq_id(seq_id),
      m_MinMapQuality(GetMinMapQualityParam()),
      m_LoadedRanges(false)
{
    m_SpotIdDetector = new CDefaultSpotIdDetector();
}


void CBamRefSeqInfo::SetBlobId(CRef<CBAMBlobId>& ret,
                               const CSeq_id_Handle& idh) const
{
    CRef<CBAMBlobId> id(new CBAMBlobId(m_File->GetBamName(), GetRefSeq_id()));
    if ( ret ) {
        ERR_POST_X(1, "CBAMDataLoader::GetBlobId: "
                   "Seq-id "<<idh<<" appears in two files: "
                   <<ret->ToString()<<" & "<<id->ToString());
    }
    else {
        ret = id;
    }
}


void CBamRefSeqInfo::GetShortSeqBlobId(CRef<CBAMBlobId>& ret,
                                       const CSeq_id_Handle& idh) const
{
    bool exists;
    {{
        CMutexGuard guard(m_Seq2ChunkMutex);
        exists = m_Seq2Chunk.find(idh) != m_Seq2Chunk.end();
    }}
    if ( exists ) {
        SetBlobId(ret, idh);
    }
}


namespace {
    struct SRefStat {
        SRefStat(void)
            : m_RefPosQuery(0),
              m_Count(0),
              m_RefPosFirst(0),
              m_RefPosLast(0),
              m_RefPosMax(0),
              m_RefLenMax(0)
            {
            }

        TSeqPos m_RefPosQuery;
        unsigned m_Count;
        TSeqPos m_RefPosFirst;
        TSeqPos m_RefPosLast;
        TSeqPos m_RefPosMax;
        TSeqPos m_RefLenMax;

        void Collect(const CBamDb& bam_db, const string& ref_id,
                     TSeqPos ref_pos, unsigned count, int min_quality);

        unsigned GetStatCount(void) const {
            return m_Count;
        }
        double GetStatLen(void) const {
            return m_RefPosLast - m_RefPosFirst + .5;
        }
    };
    
    
    void SRefStat::Collect(const CBamDb& bam_db, const string& ref_id,
                           TSeqPos ref_pos, unsigned count, int min_quality)
    {
        m_RefPosQuery = ref_pos;
        size_t skipped = 0;
        TSeqPos ref_len = bam_db.GetRefSeqLength(ref_id);
        CBamAlignIterator ait(bam_db, ref_id, ref_pos);
        for ( ; ait; ++ait ) {
            TSeqPos pos = ait.GetRefSeqPos();
            if ( pos < ref_pos ) {
                // the alignment starts before current range
                continue;
            }
            if ( min_quality > 0 && ait.GetMapQuality() < min_quality ) {
                ++skipped;
                continue;
            }
            TSeqPos len = ait.GetCIGARRefSize();
            TSeqPos max = pos + len;
            if ( max > ref_len ) {
                ++skipped;
                continue;
            }
            m_RefPosLast = pos;
            if ( len > m_RefLenMax ) {
                m_RefLenMax = len;
            }
            if ( max > m_RefPosMax ) {
                m_RefPosMax = max;
            }
            if ( m_Count == 0 ) {
                m_RefPosFirst = pos;
            }
            if ( ++m_Count == count ) {
                break;
            }
        }
        if ( GetDebugLevel() >= 3 ) {
            LOG_POST_X(4, Info << "CBAMDataLoader: "
                       "Stat @ "<<m_RefPosQuery<<": "<<m_Count<<" entries: "<<
                       m_RefPosFirst<<"-"<<m_RefPosLast<<
                       "(+"<<m_RefPosMax-m_RefPosLast<<")"<<
                       " max len: "<<m_RefLenMax<<
                       " skipped: "<<skipped);
        }
    }
};


void CBamRefSeqInfo::LoadRanges(void)
{
    if ( m_LoadedRanges ) {
        return;
    }
    _TRACE("Loading "<<GetRefSeqId()<<" -> "<<GetRefSeq_id());
    if ( !x_LoadRangesCov() && !x_LoadRangesEstimated() ) {
        x_LoadRangesStat();
    }
    _TRACE("Loaded ranges on "<<GetRefSeqId());
    m_LoadedRanges = true;
}


static const CUser_field& GetIdField(const CUser_field& field, int id)
{
    ITERATE ( CUser_field::TData::TFields, it, field.GetData().GetFields() ) {
        const CUser_field& field = **it;
        if ( field.IsSetLabel() &&
             field.GetLabel().IsId() &&
             field.GetLabel().GetId() == id ) {
            return field;
        }
    }
    NCBI_THROW_FMT(CLoaderException, eOtherError, "CBAMDataLoader: "
                   "outlier value not found: "<<id);
}


bool CBamRefSeqInfo::x_LoadRangesCov(void)
{
    if ( m_CovFileName.empty() ) {
        return false;
    }
    if ( m_File->GetBamDb().UsesRawIndex() && GetPreferRawIndexOverCoverageGraphParam() ) {
        // use more precise index information
        return false;
    }
    try {
        CRef<CSeq_entry> entry(new CSeq_entry);
        CRef<CSeq_annot> annot;
        CRef<CSeq_graph> graph;
        AutoPtr<CObjectIStream> in(CObjectIStream::Open(eSerial_AsnBinary,
                                                        m_CovFileName));
        *in >> *entry;
        const CBioseq::TAnnot& alist = entry->GetSeq().GetAnnot();
        if ( alist.size() != 1 ) {
            NCBI_THROW_FMT(CLoaderException, eOtherError, "CBAMDataLoader: "
                           "wrong number of annots in cov entry: "<<
                           alist.size());
        }
        annot = alist.front();
        const CSeq_annot::TData::TGraph& glist = annot->GetData().GetGraph();
        if ( glist.size() != 1 ) {
            NCBI_THROW_FMT(CLoaderException, eOtherError, "CBAMDataLoader: "
                           "wrong number of graphs in cov entry: "<<
                           glist.size());
        }            
        graph = glist.front();
        
        CConstRef<CUser_object> params;
        ITERATE ( CAnnot_descr::Tdata, it, annot->GetDesc().Get() ) {
            if ( (*it)->IsUser() &&
                 (*it)->GetUser().GetType().GetStr() == "BAM coverage" ) {
                params = &(*it)->GetUser();
                break;
            }
        }

        TSeqPos slot = graph->GetComp();
        TSeqPos cnt = graph->GetNumval();
        CConstRef<CUser_field> outliers = params->GetFieldRef("Outliers");
        double vmul = graph->GetA();
        double vadd = graph->GetB();
        double outliers_mul = 1./slot;

        size_t non_zero_count = 0;
        vector<double> cov(cnt);
        if ( graph->GetGraph().IsByte() ) {
            const CByte_graph& g = graph->GetGraph().GetByte();
            const CByte_graph::TValues& vv = g.GetValues();
            int vmin = g.GetMin();
            int vmax = g.GetMax();
            for ( TSeqPos i = 0; i < cnt; ++i ) {
                double v;
                int vg = Uint1(vv[i]);
                if ( vg < vmin ) {
                    continue;
                }
                if ( vg > vmax ) {
                    v = GetIdField(*outliers, i).GetData().GetReal()*outliers_mul;
                }
                else {
                    v = vmul*vg+vadd;
                }
                cov[i] = v;
                ++non_zero_count;
            }
        }
        else {
            const CInt_graph& g = graph->GetGraph().GetInt();
            const CInt_graph::TValues& vv = g.GetValues();
            int vmin = g.GetMin();
            int vmax = g.GetMax();
            for ( TSeqPos i = 0; i < cnt; ++i ) {
                double v;
                int vg = vv[i];
                if ( vg < vmin ) {
                    continue;
                }
                if ( vg > vmax ) {
                    v = GetIdField(*outliers, i).GetData().GetReal()*outliers_mul;
                }
                else {
                    v = vmul*vg+vadd;
                }
                cov[i] = v;
                ++non_zero_count;
            }
        }

        m_MinMapQuality = params->GetField("MinMapQuality").GetData().GetInt();
        int align_cnt = params->GetField("AlignCount").GetData().GetInt();
        double avg_cov = params->GetField("AvgCoverage").GetData().GetReal();
        double total_cov = avg_cov*non_zero_count*slot;
        double avg_align_len = total_cov/align_cnt;
        int max_align_len;
        CConstRef<CUser_field> len_field = params->GetFieldRef("MaxAlignSpan");
        if ( len_field ) {
            max_align_len = len_field->GetData().GetInt();
        }
        else {
            max_align_len = int(avg_align_len*2+50);
        }
        double cov_to_align_cnt = (align_cnt*slot)/total_cov;

        TSeqPos cur_first = kInvalidSeqPos, cur_last = kInvalidSeqPos;
        double cur_cnt = 0;
        
        for ( TSeqPos i = 0; i <= cnt; ++i ) {
            bool empty = i==cnt || !cov[i];
            double next_cnt = i==cnt? 0: cov[i] * cov_to_align_cnt;
            if ( cur_first != kInvalidSeqPos &&
                 (i == cnt ||
                  cur_cnt >= kChunkSize*1.5 ||
                  (next_cnt >= kChunkSize*2 && cur_cnt >= kChunkSize*.7) ||
                  (empty && cur_cnt >= kChunkSize)) ) {
                // flush collected slots
                if ( GetDebugLevel() >= 3 ) {
                    LOG_POST_X(8, Info << "CBAMDataLoader:"
                               " Chunk "<<m_Chunks.size()<<
                               " Slots "<<cur_first<<"-"<<cur_last<<
                               " exp: "<<cur_cnt);
                }
                CBamRefSeqChunkInfo chunk;
                chunk.m_AlignCount = Uint8(cur_cnt+1);
                chunk.m_RefSeqRange.SetFrom(cur_first*slot);
                TSeqPos end = cur_last*slot+slot;
                chunk.m_RefSeqRange.SetToOpen(end+max_align_len);
                chunk.m_MaxRefSeqFrom = end-1;
                m_Chunks.push_back(chunk);
                cur_first = kInvalidSeqPos;
                cur_last = kInvalidSeqPos;
                cur_cnt = 0;
            }
            if ( empty ) continue;
            if ( cur_first == kInvalidSeqPos ) {
                cur_first = i;
            }
            cur_last = i;
            cur_cnt += max(next_cnt, 1e-9);
        }
        
        m_CovEntry = entry;
        return true;
    }
    catch ( CException& exc ) {
        ERR_POST_X(3, "CBAMDataLoader: "
                   "failed to load cov file: "<<m_CovFileName<<": "<<exc);
        return false;
    }
}


static inline
TSeqPos s_GetEnd(const vector<TSeqPos>& over_ends, TSeqPos i, TSeqPos bin_size)
{
    return i < over_ends.size()? over_ends[i]: i*bin_size+bin_size-1;
}


bool CBamRefSeqInfo::x_LoadRangesEstimated(void)
{
    if ( !m_File->GetBamDb().UsesRawIndex() ) {
        return false;
    }

    CBamRawDb& raw_db = m_File->GetBamDb().GetRawDb();
    auto refseq_index = raw_db.GetRefIndex(GetRefSeqId());
    auto& refseq = raw_db.GetIndex().GetRef(refseq_index);
    vector<Uint8> data_sizes = refseq.EstimateDataSizeByAlnStartPos(raw_db.GetRefSeqLength(refseq_index));
    vector<Uint4> over_ends = refseq.GetAlnOverEnds();
    TSeqPos bin_count = TSeqPos(data_sizes.size());
    TSeqPos bin_size = raw_db.GetIndex().GetMinBinSize();
    if ( GetDebugLevel() >= 2 ) {
        LOG_POST_X(26, Info<<"CBAMDataLoader:"
                   " Bin size: "<<bin_size<<
                   " count: "<<data_sizes.size()<<
                   " length: "<<(bin_size*data_sizes.size()));
        LOG_POST_X(26, Info<<"CBAMDataLoader:"
                   " Total cov: "<<accumulate(data_sizes.begin(), data_sizes.end(), Uint8(0)));
    }
    static const TSeqPos kZeroBlocks = 8;
    static const TSeqPos kMaxChunkLength = 300*1024*1024;

    m_Chunks.clear();
    TSeqPos last_pos = 0;
    TSeqPos zero_count = 0;
    Uint8 cur_data_size = 0;
    bool has_pileup = GetPileupGraphsParam();
    Uint8 split_bin_data_size = has_pileup? GetSplitBinDataSize(): 0;
    TSeqPos split_bin_min_length = has_pileup? GetSplitBinMinLength(): 0;
    if ( split_bin_data_size == 0 || split_bin_min_length == 0 ) {
        split_bin_data_size = kMax_UI8;
        split_bin_min_length = kInvalidSeqPos;
    }
    for ( TSeqPos i = 0; i <= bin_count; ++i ) {
        if ( i < bin_count && !data_sizes[i] ) {
            ++zero_count;
            continue;
        }
        TSeqPos pos = i*bin_size;
        if ( i == bin_count || zero_count >= kZeroBlocks ) {
            // add non-empty chunk at last_pos
            if ( cur_data_size ) {
                _ASSERT(i > zero_count);
                _ASSERT(last_pos < pos-zero_count*bin_size);
                _ASSERT(cur_data_size > 0);
                TSeqPos non_zero_end = pos - zero_count*bin_size;
                CBamRefSeqChunkInfo info;
                info.m_DataSize = cur_data_size;
                info.m_RefSeqRange.SetFrom(last_pos);
                info.m_RefSeqRange.SetTo(s_GetEnd(over_ends, i-zero_count-1, bin_size));
                info.m_MaxRefSeqFrom = non_zero_end-1;
                if ( GetDebugLevel() >= 3 ) {
                    LOG_POST_X(23, Info << "CBAMDataLoader:"
                               " Chunk "<<m_Chunks.size()<<
                               " Range "<<info.m_RefSeqRange.GetFrom()<<"-"<<info.m_MaxRefSeqFrom<<
                               " (.."<<info.m_RefSeqRange.GetTo()<<")"
                               " size: "<<info.m_DataSize);
                }
                _ASSERT(info.m_RefSeqRange.GetLength() > 0);
                m_Chunks.push_back(info);
                
                last_pos = non_zero_end;
                cur_data_size = 0;
            }
            // add remaining empty chunk
            if ( zero_count > 0 ) {
                _ASSERT(i > 0);
                _ASSERT(zero_count > 0);
                _ASSERT(last_pos == pos-zero_count*bin_size);
                _ASSERT(cur_data_size == 0);
                CBamRefSeqChunkInfo info;
                info.m_DataSize = 0;
                info.m_RefSeqRange.SetFrom(last_pos);
                info.m_RefSeqRange.SetTo(s_GetEnd(over_ends, i-zero_count, bin_size));
                info.m_MaxRefSeqFrom = last_pos;
                if ( GetDebugLevel() >= 3 ) {
                    LOG_POST_X(24, Info << "CBAMDataLoader:"
                               " Chunk "<<m_Chunks.size()<<
                               " Range "<<info.m_RefSeqRange.GetFrom()<<"-"<<info.m_MaxRefSeqFrom<<
                               " (.."<<info.m_RefSeqRange.GetTo()<<")"
                               " size: "<<info.m_DataSize);
                }
                _ASSERT(info.m_RefSeqRange.GetLength() > 0);
                m_Chunks.push_back(info);
                
                last_pos = pos;
                zero_count = 0;
            }
        }
        if ( i == bin_count ) {
            break;
        }
        zero_count = 0;
        cur_data_size += data_sizes[i];
        if ( cur_data_size >= kChunkDataSize ||
             pos+bin_size-last_pos >= kMaxChunkLength ||
             (i+1 < bin_count && data_sizes[i+1] > split_bin_data_size) ) {
            if ( has_pileup &&
                 data_sizes[i] > split_bin_data_size &&
                 bin_size > split_bin_min_length ) {
                // special split to sub-page size
                _ASSERT(last_pos == pos);
                _ASSERT(cur_data_size == data_sizes[i]);
                int split_shift = 0;
                while ( (cur_data_size >> split_shift) > split_bin_data_size &&
                        (bin_size >> split_shift) > split_bin_min_length ) {
                    ++split_shift;
                }
                int sub_chunk_count = 1 << split_shift;
                auto sub_chunk_data_size = cur_data_size >> split_shift;
                TSeqPos sub_chunk_len = bin_size >> split_shift;
                TSeqPos ref_end = s_GetEnd(over_ends, i, bin_size);
                if ( GetDebugLevel() >= 3 ) {
                    LOG_POST_X(27, Info << "CBAMDataLoader:"
                               " Huge Chunk "<<m_Chunks.size()<<
                               " Range "<<last_pos<<"-"<<(pos+bin_size)<<
                               " (.."<<ref_end<<")"<<
                               " size: "<<cur_data_size);
                }
                for ( int i = 0; i < sub_chunk_count; ++i ) {
                    CBamRefSeqChunkInfo info;
                    info.m_DataSize = sub_chunk_data_size;
                    info.m_RefSeqRange.SetFrom(last_pos+i*sub_chunk_len);
                    info.m_MaxRefSeqFrom = pos+(i+1)*sub_chunk_len-1;
                    info.m_RefSeqRange.SetTo(i==0? ref_end: info.m_MaxRefSeqFrom);
                    info.m_PileupChunkCount = i==0? sub_chunk_count: 0;
                    if ( GetDebugLevel() >= 3 ) {
                        LOG_POST_X(28, Info << "CBAMDataLoader:"
                                   " Huge Chunk "<<m_Chunks.size()<<
                                   " Range "<<info.m_RefSeqRange.GetFrom()<<"-"<<info.m_MaxRefSeqFrom<<
                                   " (.."<<info.m_RefSeqRange.GetTo()<<")"
                                   " size: "<<info.m_DataSize);
                    }
                    _ASSERT(info.m_RefSeqRange.GetLength() > 0);
                    m_Chunks.push_back(info);
                }
                last_pos = pos+bin_size;
                cur_data_size = 0;
            }
            else {
                // add chunk from last_pos to pos
                _ASSERT(last_pos <= pos);
                _ASSERT(cur_data_size > 0);
                CBamRefSeqChunkInfo info;
                info.m_DataSize = cur_data_size;
                info.m_RefSeqRange.SetFrom(last_pos);
                info.m_RefSeqRange.SetTo(s_GetEnd(over_ends, i, bin_size));
                info.m_MaxRefSeqFrom = pos+bin_size-1;
                if ( GetDebugLevel() >= 3 ) {
                    LOG_POST_X(25, Info << "CBAMDataLoader:"
                               " Chunk "<<m_Chunks.size()<<
                               " Range "<<info.m_RefSeqRange.GetFrom()<<"-"<<info.m_MaxRefSeqFrom<<
                               " (.."<<info.m_RefSeqRange.GetTo()<<")"
                               " size: "<<info.m_DataSize);
                }
                _ASSERT(info.m_RefSeqRange.GetLength() > 0);
                m_Chunks.push_back(info);
                last_pos = pos+bin_size;
                cur_data_size = 0;
            }
        }
    }

    if ( GetEstimatedCoverageGraphParam() ) {
        CRef<CSeq_entry> entry(new CSeq_entry);
        entry->SetSet().SetSeq_set();
        CBam2Seq_graph cvt;
        cvt.SetRefLabel(GetRefSeqId());
        cvt.SetRefId(*GetRefSeq_id().GetSeqId());
        cvt.SetAnnotName(m_File->GetAnnotName());
        cvt.SetEstimated();
        cvt.SetMinMapQuality(m_MinMapQuality);
        entry->SetAnnot().push_back(cvt.MakeSeq_annot(raw_db, m_File->GetBamName()));
        m_CovEntry = entry;
    }
    return true;
}


void CBamRefSeqInfo::x_LoadRangesStat(void)
{
    const unsigned kNumStat = 10;
    const unsigned kStatCount = 1000;
    vector<SRefStat> stat(kNumStat);
    TSeqPos ref_begin = 0, ref_end_min = 0, ref_end = 0, max_len = 0;
    double stat_len = 0, stat_cnt = 0;
    const unsigned scan_first = 1;
    if ( scan_first ) {
        stat[0].Collect(*m_File, GetRefSeqId(), 0,
                        kStatCount, m_MinMapQuality);
        if ( stat[0].m_Count != kStatCount ) {
            // single chunk
            if ( stat[0].m_Count > 0 ) {
                CBamRefSeqChunkInfo chunk;
                chunk.m_AlignCount = stat[0].m_Count;
                chunk.m_RefSeqRange.SetFrom(stat[0].m_RefPosFirst);
                chunk.m_RefSeqRange.SetToOpen(stat[0].m_RefPosMax);
                chunk.m_MaxRefSeqFrom = stat[0].m_RefPosLast;
                m_Chunks.push_back(chunk);
            }
            m_LoadedRanges = true;
            return;
        }
        ref_begin = stat[0].m_RefPosFirst;
        ref_end_min = stat[0].m_RefPosLast;
        max_len = stat[0].m_RefLenMax;
        stat_len = stat[0].GetStatLen();
        stat_cnt = stat[0].GetStatCount();
    }
    ref_end = m_File->GetRefSeqLength(GetRefSeqId());
    if ( ref_end == kInvalidSeqPos ) {
        TSeqPos min = ref_end_min;
        TSeqPos max = kInvalidSeqPos;
        while ( max > min+max_len+1 ) {
            TSeqPos mid = min + (max - min)/2;
            _TRACE("binary: "<<min<<"-"<<max<<" -> "<<mid);
            if ( CBamAlignIterator(*m_File, GetRefSeqId(), mid) ) {
                min = mid;
            }
            else {
                max = mid;
            }
        }
        ref_end = max;
        _TRACE("binary: end: "<<max);
    }
    for ( unsigned k = scan_first; k < kNumStat; ++k ) {
        TSeqPos ref_pos = ref_begin +
            TSeqPos(double(ref_end - ref_begin)*k/kNumStat);
        if ( k && ref_pos < stat[k-1].m_RefPosLast ) {
            ref_pos = stat[k-1].m_RefPosLast;
        }
        _TRACE("stat["<<k<<"] @ "<<ref_pos);
        stat[k].Collect(*m_File, GetRefSeqId(), ref_pos,
                        kStatCount, m_MinMapQuality);
        stat_len += stat[k].GetStatLen();
        stat_cnt += stat[k].GetStatCount();
        if ( stat[k].m_RefLenMax > max_len ) {
            max_len = stat[k].m_RefLenMax;
        }
    }
    double density = stat_cnt / stat_len;
    double exp_count = (ref_end-ref_begin)*density;
    unsigned chunks = unsigned(exp_count/kChunkSize+1);
    chunks = min(chunks, unsigned(sqrt(exp_count)+1));
    max_len *= 2;
    if ( GetDebugLevel() >= 2 ) {
        LOG_POST_X(5, Info << "CBAMDataLoader: "
                   "Total range: "<<ref_begin<<"-"<<ref_end-1<<
                   " exp count: "<<exp_count<<" chunks: "<<chunks);
    }
    vector<TSeqPos> pp(chunks+1);
    for ( unsigned k = 1; k < chunks; ++k ) {
        TSeqPos pos = ref_begin +
            TSeqPos(double(ref_end-ref_begin)*k/chunks);
        pp[k] = pos;
    }
    pp[chunks] = ref_end;
    for ( unsigned k = 0; k < chunks; ++k ) {
        CBamRefSeqChunkInfo chunk;
        chunk.m_AlignCount = 1;
        TSeqPos pos = pp[k];
        TSeqPos end = pp[k+1];
        chunk.m_RefSeqRange.SetFrom(pos);
        TSeqPos end2 = min(end+max_len, ref_end);
        if ( k+1 < chunks ) {
            end2 = min(end2, pp[k+2]);
        }
        chunk.m_RefSeqRange.SetToOpen(end2);
        chunk.m_MaxRefSeqFrom = end-1;
        m_Chunks.push_back(chunk);
    }
}


void CBamRefSeqInfo::x_LoadRangesScan(void)
{
    typedef CBamRefSeqInfo::TRange TRange;
    vector<TRange> rr;
    int min_quality = m_MinMapQuality;
    TSeqPos ref_len = m_File->GetRefSeqLength(GetRefSeqId());
    for ( CBamAlignIterator ait(*m_File, GetRefSeqId(), 0); ait; ++ait ) {
        if ( min_quality > 0 && ait.GetMapQuality() < min_quality ) {
            continue;
        }
        TSeqPos ref_pos = ait.GetRefSeqPos();
        TSeqPos ref_end = ref_pos + ait.GetCIGARRefSize() - 1;
        if ( ref_end > ref_len ) {
            continue;
        }
        rr.push_back(TRange(ref_pos, ref_end));
    }
    if ( !rr.empty() ) {
        sort(rr.begin(), rr.end());
        for ( size_t p = 0; p < rr.size(); ) {
            size_t e = min(p+kChunkSize, rr.size())-1;
            TSeqPos min_from = rr[p].GetFrom();
            TSeqPos max_from = rr[e++].GetFrom();
            while ( e < rr.size() && rr[e].GetFrom() == max_from ) {
                ++e;
            }
            TSeqPos max_to_open = max_from;
            for ( size_t i = p; i < e; ++i ) {
                max_to_open = max(max_to_open, rr[i].GetToOpen());
            }
            CBamRefSeqChunkInfo chunk;
            chunk.AddRefSeqRange(TRange(min_from, max_to_open-1));
            chunk.AddRefSeqRange(TRange(max_from, max_to_open-1));
            _TRACE("Chunk "<<m_Chunks.size()<<" count: "<<e-p<<
                   " "<<TRange(min_from, max_from)<<" "<<max_to_open-1);
            m_Chunks.push_back(chunk);
            p = e;
        }
        if ( GetDebugLevel() >= 2 ) {
            LOG_POST_X(6, Info<<"CBAMDataLoader: "
                       "Total range: "<<
                       rr[0].GetFrom()<<"-"<<rr.back().GetTo()<<
                       " count: "<<rr.size()<<" chunks: "<<m_Chunks.size());
        }
    }
}


CRange<TSeqPos> CBamRefSeqInfo::GetChunkGraphRange(size_t range_id)
{
    auto& chunk = m_Chunks[range_id];
    CRange<TSeqPos> range = chunk.GetRefSeqRange();
    auto chunk_count = chunk.m_PileupChunkCount;
    auto last_range_id = range_id;
    if ( chunk_count ) {
        last_range_id += chunk_count-1;
        range.SetToOpen(m_Chunks[last_range_id].GetRefSeqRange().GetToOpen());
    }
    if ( last_range_id+1 < m_Chunks.size() ) {
        range.SetToOpen(m_Chunks[last_range_id+1].GetRefSeqRange().GetFrom());
    }
    return range;
}


void CBamRefSeqInfo::LoadMainEntry(CTSE_LoadLock& load_lock)
{
    LoadRanges();
    CRef<CSeq_entry> entry(new CSeq_entry);
    entry->SetSet().SetId().SetId(kTSEId);
    if ( m_CovEntry ) {
        entry->SetSet().SetAnnot() = m_CovEntry->GetAnnot();
    }
    load_lock->SetSeq_entry(*entry);
    CreateChunks(load_lock->GetSplitInfo());
}


void CBamRefSeqInfo::LoadMainSplit(CTSE_LoadLock& load_lock)
{
    CStopWatch sw;
    if ( GetDebugLevel() >= 1 ) {
        sw.Start();
    }
    CRef<CSeq_entry> entry(new CSeq_entry);
    entry->SetSet().SetId().SetId(kTSEId);
    load_lock->SetSeq_entry(*entry);
    CTSE_Split_Info& split_info = load_lock->GetSplitInfo();
    bool has_pileup = GetPileupGraphsParam();
    CAnnotName name, pileup_name;
    if ( !m_File->GetAnnotName().empty() ) {
        string base = m_File->GetAnnotName();
        name = CAnnotName(base);
        if ( has_pileup ) {
            pileup_name = CAnnotName(base + ' ' + PILEUP_NAME_SUFFIX);
        }
    }

    CRef<CTSE_Chunk_Info> chunk(new CTSE_Chunk_Info(kMainChunkId));
    CRange<TSeqPos> whole_range = CRange<TSeqPos>::GetWhole();
    if ( m_CovEntry || !m_CovFileName.empty() ||
         (m_File->GetBamDb().UsesRawIndex() && GetEstimatedCoverageGraphParam()) ) {
        chunk->x_AddAnnotType(name,
                              CSeq_annot::C_Data::e_Graph,
                              GetRefSeq_id(),
                              whole_range);
    }
    if ( has_pileup ) {
        chunk->x_AddAnnotType(pileup_name,
                              CSeq_annot::C_Data::e_Graph,
                              GetRefSeq_id(),
                              whole_range);
    }
    chunk->x_AddAnnotType(name,
                          CSeq_annot::C_Data::e_Align,
                          GetRefSeq_id(),
                          whole_range);
    split_info.AddChunk(*chunk);
    if ( GetDebugLevel() >= 1 ) {
        LOG_POST_X(17, Info<<"CBAMDataLoader: "
                   "Initialized BAM refseq "<<GetRefSeq_id()<<" in "<<sw.Elapsed());
    }
}


void CBamRefSeqInfo::LoadMainChunk(CTSE_Chunk_Info& chunk_info)
{
    LoadRanges();
    if ( m_CovEntry ) {
        CTSE_Chunk_Info::TPlace place(CSeq_id_Handle(), kTSEId);
        ITERATE ( CBioseq::TAnnot, it, m_CovEntry->GetAnnot() ) {
            chunk_info.x_LoadAnnot(place, **it);
        }
    }
    CreateChunks(const_cast<CTSE_Split_Info&>(chunk_info.GetSplitInfo()));
    chunk_info.SetLoaded();
}


static const double k_make_graph_seconds = 7.5e-9; // 133 MB/s
static const double k_make_align_seconds = 250e-9; // 4 MB/s
static const double k_make_read_seconds = 80e-9; // 12 MB/s


void CBamRefSeqInfo::CreateChunks(CTSE_Split_Info& split_info)
{
    bool has_pileup = GetPileupGraphsParam();
    CAnnotName name, pileup_name;
    if ( !m_File->GetAnnotName().empty() ) {
        string base = m_File->GetAnnotName();
        name = CAnnotName(base);
        if ( has_pileup ) {
            pileup_name = CAnnotName(base + ' ' + PILEUP_NAME_SUFFIX);
        }
    }

    CBamRawDb* raw_db = 0;
    size_t refseq_index = size_t(-1);
    if ( m_File->GetBamDb().UsesRawIndex() ) {
        raw_db = &m_File->GetBamDb().GetRawDb();
        refseq_index = raw_db->GetRefIndex(GetRefSeqId());
    }

    // create chunk info for alignments
    for ( size_t range_id = 0; range_id < m_Chunks.size(); ++range_id ) {
        int base_id = int(range_id*kChunkIdMul);
        auto align_count = m_Chunks[range_id].GetAlignCount();
        auto data_size = m_Chunks[range_id].m_DataSize;
        if ( align_count == 0 && data_size != 0 ) {
            align_count = data_size / kSingleAlignBytes + 1;
        }
        else if ( data_size == 0 && align_count != 0 ) {
            data_size = align_count * kSingleAlignBytes;
        }
        CRange<TSeqPos> wide_range = m_Chunks[range_id].GetAlignRange(); // includes overhang after this range
        if ( has_pileup && m_Chunks[range_id].m_PileupChunkCount ) {
            _ASSERT(raw_db);
            CRef<CTSE_Chunk_Info> chunk(new CTSE_Chunk_Info(base_id+eChunk_pileup_graph));
            CRange<TSeqPos> pileup_range = GetChunkGraphRange(range_id);
            {{
                Uint8 bytes = data_size*m_Chunks[range_id].m_PileupChunkCount;
                chunk->x_SetLoadBytes(Uint4(min<size_t>(bytes, kMax_UI4)));
                chunk->x_AddAnnotType(pileup_name,
                                      CSeq_annot::C_Data::e_Graph,
                                      GetRefSeq_id(),
                                      pileup_range);
                if ( GetDebugLevel() >= 2 ) {
                    LOG_POST_X(13, Info << "CBAMDataLoader: "<<GetRefSeq_id()<<": "
                               "Pileup Chunk id="<<chunk->GetChunkId()<<": "<<pileup_range<<
                               " with "<<bytes<<" bytes");
                }
                if ( m_Chunks[range_id].m_PileupChunkCount > 1 ) {
                    // include annots crossing sub-range borders
                    chunk->x_AddAnnotType(name,
                                          CSeq_annot::C_Data::e_Align,
                                          GetRefSeq_id(),
                                          pileup_range);
                    if ( GetDebugLevel() >= 2 ) {
                        LOG_POST_X(13, Info << "CBAMDataLoader: "<<GetRefSeq_id()<<": "
                                   "Pileup Chunk id="<<chunk->GetChunkId()<<": aligns: "<<pileup_range);
                    }
                }
            }}
            split_info.AddChunk(*chunk);
            if ( m_Chunks[range_id].m_PileupChunkCount > 1 ) {
                // add separate chunk for high index level overlapping aligns
                if ( Uint8 bytes = CBamFileRangeSet(raw_db->GetIndex(), refseq_index, pileup_range,
                                                    CBamIndex::kLevel1, CBamIndex::kMaxLevel,
                                                    CBamIndex::eSearchByStart).GetFileSize() ) {
                    CRef<CTSE_Chunk_Info> chunk(new CTSE_Chunk_Info(base_id+eChunk_align2));
                    chunk->x_SetLoadBytes(Uint4(min<size_t>(bytes, kMax_UI4)));
                    //chunk->x_SetLoadSeconds(bytes*align_seconds);
                    chunk->x_AddAnnotType(name,
                                          CSeq_annot::C_Data::e_Align,
                                          GetRefSeq_id(),
                                          wide_range);
                    if ( GetDebugLevel() >= 2 ) {
                        LOG_POST_X(12, Info << "CBAMDataLoader: "<<GetRefSeq_id()<<": "
                                   "Align Chunk id="<<chunk->GetChunkId()<<": "<<wide_range<<
                                   " with "<<bytes<<" bytes");
                    }
                    split_info.AddChunk(*chunk);
                }
            }
        }
        if ( align_count ) {
            CRange<TSeqPos> start_range = m_Chunks[range_id].GetAlignStartRange();
            if ( m_Chunks[range_id].m_PileupChunkCount != 1 ) {
                // add single sub-page chunk for in-range aligns
                _ASSERT(raw_db);
                CRef<CTSE_Chunk_Info> chunk(new CTSE_Chunk_Info(base_id+eChunk_align1));
                chunk->x_SetLoadBytes(Uint4(min<size_t>(data_size, kMax_UI4)));
                //chunk->x_SetLoadSeconds(bytes*align_seconds);
                chunk->x_AddAnnotType(name,
                                      CSeq_annot::C_Data::e_Align,
                                      GetRefSeq_id(),
                                      start_range);
                if ( GetDebugLevel() >= 2 ) {
                    LOG_POST_X(12, Info << "CBAMDataLoader: "<<GetRefSeq_id()<<": "
                               "Align sub-page Chunk id="<<chunk->GetChunkId()<<": "<<start_range<<
                               " with "<<data_size<<" bytes");
                }
                split_info.AddChunk(*chunk);
            }
            else if ( raw_db && data_size >= kSplitLevelsChunkDataSize ) {
                // add two separate chunks for in-range and overlapping aligns
                if ( Uint8 bytes = CBamFileRangeSet(raw_db->GetIndex(), refseq_index, start_range,
                                                    CBamIndex::kLevel0, CBamIndex::kLevel0,
                                                    CBamIndex::eSearchByStart).GetFileSize() ) {
                    CRef<CTSE_Chunk_Info> chunk(new CTSE_Chunk_Info(base_id+eChunk_align1));
                    chunk->x_SetLoadBytes(Uint4(min<size_t>(bytes, kMax_UI4)));
                    //chunk->x_SetLoadSeconds(bytes*align_seconds);
                    chunk->x_AddAnnotType(name,
                                          CSeq_annot::C_Data::e_Align,
                                          GetRefSeq_id(),
                                          start_range);
                    split_info.AddChunk(*chunk);
                    if ( GetDebugLevel() >= 2 ) {
                        LOG_POST_X(12, Info << "CBAMDataLoader: "<<GetRefSeq_id()<<": "
                                   "Align Chunk id="<<chunk->GetChunkId()<<": "<<start_range<<
                                   " with "<<bytes<<" bytes");
                    }
                }
                if ( Uint8 bytes = CBamFileRangeSet(raw_db->GetIndex(), refseq_index, start_range,
                                                    CBamIndex::kLevel1, CBamIndex::kMaxLevel,
                                                    CBamIndex::eSearchByStart).GetFileSize() ) {
                    CRef<CTSE_Chunk_Info> chunk(new CTSE_Chunk_Info(base_id+eChunk_align2));
                    chunk->x_SetLoadBytes(Uint4(min<size_t>(bytes, kMax_UI4)));
                    //chunk->x_SetLoadSeconds(bytes*align_seconds);
                    chunk->x_AddAnnotType(name,
                                          CSeq_annot::C_Data::e_Align,
                                          GetRefSeq_id(),
                                          wide_range);
                    split_info.AddChunk(*chunk);
                    if ( GetDebugLevel() >= 2 ) {
                        LOG_POST_X(12, Info << "CBAMDataLoader: "<<GetRefSeq_id()<<": "
                                   "Align Chunk id="<<chunk->GetChunkId()<<": "<<wide_range<<
                                   " with "<<bytes<<" bytes");
                    }
                }
            }
            else {
                // add single chunk for in-range and overlapping aligns
                CRef<CTSE_Chunk_Info> chunk(new CTSE_Chunk_Info(base_id+eChunk_align));
                if ( raw_db ) {
                    data_size = CBamFileRangeSet(raw_db->GetIndex(), refseq_index, start_range,
                                                 CBamIndex::kLevel0, CBamIndex::kMaxLevel,
                                                 CBamIndex::eSearchByStart).GetFileSize();
                    if ( GetDebugLevel() >= 2 ) {
                        LOG_POST_X(12, Info << "CBAMDataLoader: "<<GetRefSeq_id()<<": "
                                   "Align Chunk id="<<chunk->GetChunkId()<<": "<<wide_range<<
                                   " with "<<data_size<<" bytes");
                    }
                }
                else {
                    if ( GetDebugLevel() >= 2 ) {
                        LOG_POST_X(12, Info << "CBAMDataLoader: "<<GetRefSeq_id()<<": "
                                   "Align Chunk id="<<chunk->GetChunkId()<<": "<<wide_range<<
                                   " with "<<align_count<<" aligns");
                    }
                }
                chunk->x_SetLoadBytes(Uint4(min<size_t>(data_size, kMax_UI4)));
                //chunk->x_SetLoadSeconds(bytes*align_seconds);
                chunk->x_AddAnnotType(name,
                                      CSeq_annot::C_Data::e_Align,
                                      GetRefSeq_id(),
                                      wide_range);
                split_info.AddChunk(*chunk);
            }
        }
    }
    {{
        CMutexGuard guard(m_Seq2ChunkMutex);
        m_Seq2Chunk.clear();
    }}
}


double CBamRefSeqInfo::EstimateAlignLoadSeconds(const CTSE_Chunk_Info& chunk,
                                                Uint4 bytes) const
{
    CBamRawDb* raw_db = 0;
    if ( m_File->GetBamDb().UsesRawIndex() ) {
        raw_db = &m_File->GetBamDb().GetRawDb();
    }
    double get_data_seconds = raw_db? raw_db->GetEstimatedSecondsPerByte(): 0;
    return bytes*(get_data_seconds + k_make_align_seconds);
}


double CBamRefSeqInfo::EstimatePileupLoadSeconds(const CTSE_Chunk_Info& chunk,
                                                 Uint4 bytes) const
{
    CBamRawDb* raw_db = 0;
    if ( m_File->GetBamDb().UsesRawIndex() ) {
        raw_db = &m_File->GetBamDb().GetRawDb();
    }
    double get_data_seconds = raw_db? raw_db->GetEstimatedSecondsPerByte(): 0;
    return bytes*(get_data_seconds + k_make_graph_seconds);
}


double CBamRefSeqInfo::EstimateSeqLoadSeconds(const CTSE_Chunk_Info& chunk,
                                              Uint4 bytes) const
{
    CBamRawDb* raw_db = 0;
    if ( m_File->GetBamDb().UsesRawIndex() ) {
        raw_db = &m_File->GetBamDb().GetRawDb();
    }
    double get_data_seconds = raw_db? raw_db->GetEstimatedSecondsPerByte(): 0;
    return bytes*(get_data_seconds + k_make_read_seconds);
}


double CBamRefSeqInfo::EstimateLoadSeconds(const CTSE_Chunk_Info& chunk_info,
                                           Uint4 bytes) const
{
    switch ( chunk_info.GetChunkId() % kChunkIdMul ) {
    case eChunk_pileup_graph:
        return EstimatePileupLoadSeconds(chunk_info, bytes);
    case eChunk_align:
    case eChunk_align1:
    case eChunk_align2:
        return EstimateAlignLoadSeconds(chunk_info, bytes);
    case eChunk_short_seq_pileup:
    case eChunk_short_seq:
    case eChunk_short_seq1:
    case eChunk_short_seq2:
        return EstimateSeqLoadSeconds(chunk_info, bytes);
    }
    return 0;
}


void CBamRefSeqInfo::LoadChunk(CTSE_Chunk_Info& chunk_info)
{
    if ( chunk_info.GetChunkId() == kMainChunkId ) {
        LoadMainChunk(chunk_info);
        return;
    }
    switch ( chunk_info.GetChunkId() % kChunkIdMul ) {
    case eChunk_pileup_graph:
        LoadPileupChunk(chunk_info);
        break;
    case eChunk_align:
    case eChunk_align1:
    case eChunk_align2:
        LoadAlignChunk(chunk_info);
        break;
    case eChunk_short_seq_pileup:
    case eChunk_short_seq:
    case eChunk_short_seq1:
    case eChunk_short_seq2:
        LoadSeqChunk(chunk_info);
        break;
    }
}


void CBamRefSeqInfo::x_InitAlignIterator(CBamAlignIterator& ait,
                                         TSeqPos& max_end_pos,
                                         CTSE_Chunk_Info& chunk_info,
                                         int base_id)
{
    int range_id = chunk_info.GetChunkId() / kChunkIdMul;
    int sub_chunk = (chunk_info.GetChunkId() % kChunkIdMul) - base_id + eChunk_align;
    _ASSERT(sub_chunk >= eChunk_pileup_graph && sub_chunk <= eChunk_align2);
#ifdef SKIP_TOO_LONG_ALIGNMENTS
    max_end_pos = m_File->GetRefSeqLength(GetRefSeqId());
#else
    max_end_pos = kInvalidSeqPos;
#endif
    if ( sub_chunk == eChunk_align1 ) {
        auto start_range = m_Chunks[range_id].GetAlignStartRange();
        ait = CBamAlignIterator(*m_File, GetRefSeqId(),
                                start_range.GetFrom(), start_range.GetLength(),
                                CBamIndex::kLevel0, CBamIndex::kLevel0,
                                CBamAlignIterator::eSearchByStart,
                                &m_Chunks[range_id].m_FilePosFirstStarting);
        // we also skip alignments that end beyond current region
        max_end_pos = min(max_end_pos, start_range.GetToOpen());
    }
    else if ( sub_chunk == eChunk_align2 ) {
        auto start_range = GetChunkGraphRange(range_id);
        ait = CBamAlignIterator(*m_File, GetRefSeqId(),
                                start_range.GetFrom(), start_range.GetLength(),
                                CBamIndex::kLevel1, CBamIndex::kMaxLevel,
                                CBamAlignIterator::eSearchByStart);
    }
    else {
        _ASSERT(sub_chunk == eChunk_align);
        auto start_range = m_Chunks[range_id].GetAlignStartRange();
        ait = CBamAlignIterator(*m_File, GetRefSeqId(),
                                start_range.GetFrom(), start_range.GetLength(),
                                CBamAlignIterator::eSearchByStart);
    }
    if ( m_SpotIdDetector ) {
        ait.SetSpotIdDetector(m_SpotIdDetector.GetNCPointer());
    }
}


void CBamRefSeqInfo::x_AddSeqChunk(CTSE_Chunk_Info& chunk_info,
                                   const vector<CSeq_id_Handle>& short_ids)
{
    int range_id = chunk_info.GetChunkId() / kChunkIdMul;
    int sub_chunk = chunk_info.GetChunkId() % kChunkIdMul;
    _ASSERT(sub_chunk >= eChunk_pileup_graph && sub_chunk <= eChunk_align2);
    int seq_chunk_id = range_id*kChunkIdMul+eChunk_short_seq+(sub_chunk-eChunk_align);
    vector<CSeq_id_Handle> new_short_ids;
    {{
        CMutexGuard guard(m_Seq2ChunkMutex);
        for ( auto& id : short_ids ) {
            if ( m_Seq2Chunk.insert(make_pair(id, seq_chunk_id)).first->second == seq_chunk_id ) {
                new_short_ids.push_back(id);
            }
        }
    }}
    if ( new_short_ids.empty() ) {
        return;
    }
    CRef<CTSE_Chunk_Info> chunk(new CTSE_Chunk_Info(seq_chunk_id));
    for ( auto& id : new_short_ids ) {
        chunk->x_AddBioseqId(id);
    }
    CStopWatch sw_attach;
    if ( GetDebugLevel() >= 4 ) {
        sw_attach.Start();
    }
    if ( m_File->GetBamDb().UsesRawIndex() ) {
        chunk->x_SetLoadBytes(Uint4(m_Chunks[range_id].GetAlignCount()));
    }
    chunk->x_AddBioseqPlace(kTSEId);
    CTSE_Split_Info& split_info =
        const_cast<CTSE_Split_Info&>(chunk_info.GetSplitInfo());
    split_info.AddChunk(*chunk);
    if ( GetDebugLevel() >= 4 ) {
        double time = sw_attach.Elapsed();
        s_AttachTime += time;
        LOG_POST_X(20, Info<<"CBAMDataLoader: "
                   "Created short reads chunk "<<
                   range_id<<"/"<<(seq_chunk_id-range_id*kChunkIdMul)<<" "<<
                   GetRefSeqId()<<" @ "<<
                   m_Chunks[range_id].GetRefSeqRange()<<" in "<<time);
    }
}


void CBamRefSeqInfo::LoadAlignChunk(CTSE_Chunk_Info& chunk_info)
{
    CStopWatch sw;
    CStopWatch sw_create;
    if ( GetDebugLevel() >= 3 ) {
        sw.Start();
    }
    int range_id = chunk_info.GetChunkId() / kChunkIdMul;
    CTSE_Chunk_Info::TPlace place(CSeq_id_Handle(), kTSEId);
    int min_quality = m_MinMapQuality;
    _TRACE("Loading aligns "<<GetRefSeqId()<<" @ "<<m_Chunks[range_id].GetRefSeqRange());
    size_t skipped = 0, count = 0, repl_count = 0, fail_count = 0;
    vector<CSeq_id_Handle> short_ids;
    
    CRef<CSeq_annot> annot;
    CSeq_annot::TData::TAlign* align_list = 0;

    CBamAlignIterator ait;
    TSeqPos max_end_pos;
    x_InitAlignIterator(ait, max_end_pos, chunk_info, eChunk_align);
    for( ; ait; ++ait ) {
        TSeqPos align_pos = ait.GetRefSeqPos();
        // should be filtered by CBamAlignIterator
        _ASSERT(align_pos >= m_Chunks[range_id].GetRefSeqRange().GetFrom());
        if ( min_quality > 0 && ait.GetMapQuality() < min_quality ) {
            ++skipped;
            continue;
        }
        if ( max_end_pos != kInvalidSeqPos ) {
            TSeqPos align_end = align_pos + ait.GetCIGARRefSize();
            if ( align_end > max_end_pos ) {
                ++skipped;
                continue;
            }
        }
        ++count;

        if ( !align_list ) {
            annot = ait.GetSeq_annot(m_File->GetAnnotName());
            align_list = &annot->SetData().SetAlign();
        }
        if ( GetDebugLevel() >= 4 ) {
            sw_create.Start();
        }
        align_list->push_back(ait.GetMatchAlign());
        if ( ait.GetShortSequenceLength() != 0 ) {
            short_ids.push_back(CSeq_id_Handle::GetHandle(*ait.GetShortSeq_id()));
        }
        if ( GetDebugLevel() >= 4 ) {
            sw_create.Stop();
        }
    }
    if ( GetDebugLevel() >= 4 ) {
        double time = sw_create.Elapsed();
        LOG_POST_X(19, Info<<"CBAMDataLoader: "
                   "Created alignments "<<GetRefSeqId()<<
                   " id="<<chunk_info.GetChunkId()<<
                   " @ "<<m_Chunks[range_id].GetRefSeqRange()<<": "<<
                   count<<" repl: "<<repl_count<<" fail: "<<fail_count<<
                   " skipped: "<<skipped<<" in "<<time);
        s_CreateTime += time;
    }
    if ( annot ) {
        CStopWatch sw_attach(CStopWatch::eStart);
        chunk_info.x_LoadAnnot(place, *annot);
        chunk_info.x_AddUsedMemory(count*2500+10000);
        if ( GetDebugLevel() >= 4 ) {
            double time = sw_attach.Elapsed();
            LOG_POST_X(19, Info<<"CBAMDataLoader: "
                       "Attached alignments "<<GetRefSeqId()<<
                       " id="<<chunk_info.GetChunkId()<<
                       " @ "<<m_Chunks[range_id].GetRefSeqRange()<<" in "<<time);
            s_AttachTime += time;
        }
    }
    x_AddSeqChunk(chunk_info, short_ids);
    if ( GetDebugLevel() >= 3 ) {
        LOG_POST_X(7, Info<<"CBAMDataLoader: "
                   "Loaded "<<GetRefSeqId()<<
                   " id="<<chunk_info.GetChunkId()<<
                   " @ "<<m_Chunks[range_id].GetRefSeqRange()<<": "<<
                   count<<" repl: "<<repl_count<<" fail: "<<fail_count<<
                   " skipped: "<<skipped<<" in "<<sw.Elapsed());
    }
    chunk_info.SetLoaded();
}


void CBamRefSeqInfo::LoadSeqChunk(CTSE_Chunk_Info& chunk_info)
{
    CStopWatch sw;
    if ( GetDebugLevel() >= 3 ) {
        sw.Start();
    }
    int chunk_id = chunk_info.GetChunkId();
    int range_id = chunk_id/kChunkIdMul;
    int sub_chunk = chunk_id%kChunkIdMul;
    const CBamRefSeqChunkInfo& chunk = m_Chunks[range_id];
    auto start_range = chunk.GetAlignStartRange();
    CTSE_Chunk_Info::TPlace place(CSeq_id_Handle(), kTSEId);
    int min_quality = m_MinMapQuality;
    _TRACE("Loading seqs "<<GetRefSeqId()<<" @ "<<chunk.GetRefSeqRange());
    size_t count = 0, skipped = 0, dups = 0, far_refs = 0;
    set<CSeq_id_Handle> loaded;
    
    list< CRef<CBioseq> > bioseqs;
    
    if ( sub_chunk == eChunk_short_seq_pileup ) {
#ifdef SEPARATE_PILEUP_READS
        int sub_chunk_count = chunk.m_PileupChunkCount;
        for ( int i = 1; i < sub_chunk_count; ++i ) {
            TSeqPos split_pos = m_Chunks[range_id+i].GetRefSeqRange().GetFrom();
            CBamAlignIterator ait(*m_File, GetRefSeqId(),
                                  start_range.GetFrom(), split_pos-start_range.GetFrom(),
                                  CBamIndex::kLevel0, CBamIndex::kLevel0,
                                  CBamAlignIterator::eSearchByStart,
                                  &m_Chunks[range_id].m_FilePosFirstCrossing);
            if ( m_SpotIdDetector ) {
                ait.SetSpotIdDetector(m_SpotIdDetector.GetNCPointer());
            }
            CMutexGuard guard(m_Seq2ChunkMutex);
            for ( ; ait; ++ait ) {
                TSeqPos align_pos = ait.GetRefSeqPos();
                _ASSERT(align_pos<split_pos);
                if ( min_quality > 0 && ait.GetMapQuality() < min_quality ) {
                    ++skipped;
                    continue;
                }
                TSeqPos align_end = align_pos + ait.GetCIGARRefSize();
                if ( align_end <= split_pos ) {
                    ++skipped;
                    continue;
                }

                if ( ait.GetShortSequenceLength() == 0 ) {
                    // far reference
                    ++far_refs;
                    continue;
                }
        
                CSeq_id_Handle seq_id =
                    CSeq_id_Handle::GetHandle(*ait.GetShortSeq_id());
                auto iter = m_Seq2Chunk.find(seq_id);
                if ( iter == m_Seq2Chunk.end() || iter->second != chunk_id ) {
                    ++skipped;
                    continue;
                }

                if ( !loaded.insert(seq_id).second ) {
                    ++dups;
                    continue;
                }
                bioseqs.push_back(ait.GetShortBioseq());
                ++count;
            }
        }
#endif
    }
    else {
        TSeqPos max_end_pos = kInvalidSeqPos;
        CBamAlignIterator ait;
        x_InitAlignIterator(ait, max_end_pos, chunk_info, eChunk_short_seq);
        CMutexGuard guard(m_Seq2ChunkMutex);
        for( ; ait; ++ait ){
            TSeqPos align_pos = ait.GetRefSeqPos();
            if ( align_pos < start_range.GetFrom() ) {
                // the alignments starts before current chunk range
                ++skipped;
                continue;
            }
            if ( min_quality > 0 && ait.GetMapQuality() < min_quality ) {
                ++skipped;
                continue;
            }
            if ( max_end_pos != kInvalidSeqPos ) {
                TSeqPos align_end = align_pos + ait.GetCIGARRefSize();
                if ( align_end > max_end_pos ) {
                    ++skipped;
                    continue;
                }
            }

            if ( ait.GetShortSequenceLength() == 0 ) {
                // far reference
                ++far_refs;
                continue;
            }
        
            CSeq_id_Handle seq_id =
                CSeq_id_Handle::GetHandle(*ait.GetShortSeq_id());
            auto iter = m_Seq2Chunk.find(seq_id);
            if ( iter == m_Seq2Chunk.end() || iter->second != chunk_id ) {
                ++skipped;
                continue;
            }

            if ( !loaded.insert(seq_id).second ) {
                ++dups;
                continue;
            }
            bioseqs.push_back(ait.GetShortBioseq());
            ++count;
        }
    }
    chunk_info.x_LoadBioseqs(place, bioseqs);

    if ( GetDebugLevel() >= 3 ) {
        LOG_POST_X(10, Info<<"CBAMDataLoader: "
                   "Loaded seqs "<<GetRefSeqId()<<
                   " id="<<chunk_info.GetChunkId()<<
                   " @ "<<chunk.GetRefSeqRange()<<": "<<
                   count<<" skipped: "<<skipped<<" dups: "<<dups<<" far: "<<far_refs<<" in "<<sw.Elapsed());
    }

    chunk_info.SetLoaded();
}

#define USE_NEW_PILEUP_COLLECTOR

#if defined USE_NEW_PILEUP_COLLECTOR && !defined HAVE_NEW_PILEUP_COLLECTOR
# undef USE_NEW_PILEUP_COLLECTOR
#endif

#ifdef USE_NEW_PILEUP_COLLECTOR

static Uint8 total_pileup_range;
static Uint8 total_pileup_aligns;
static double total_pileup_time_collect;
static double total_pileup_time_max;
static double total_pileup_time_make;

static struct STimePrinter {
    ~STimePrinter() {
        if ( total_pileup_range ) {
            LOG_POST_X(18, Info<<"CBAMDataLoader: "
                       "Total pileup bases: "<<total_pileup_range<<
                       " aligns: "<<total_pileup_aligns<<
                       " collect time: "<<total_pileup_time_collect<<
                       " max: "<<total_pileup_time_max<<
                       " make: "<<total_pileup_time_make);
        }
    }
} s_TimePrinter;

struct SPileupGraphCreator : public CBamDb::ICollectPileupCallback
{
    static const int kStat_Match = CBamDb::SPileupValues::kStat_Match;
    static const int kStat_Gap = CBamDb::SPileupValues::kStat_Gap;
    static const int kStat_Intron = CBamDb::SPileupValues::kStat_Intron;
    static const int kNumStat_ACGT = CBamDb::SPileupValues::kNumStat_ACGT;
    static const int kNumStat = CBamDb::SPileupValues::kNumStat;
    typedef CBamDb::SPileupValues::TCount TCount;

    string annot_name;
    CRef<CSeq_id> ref_id;
    CRange<TSeqPos> ref_range;
    int min_quality;
    bool make_intron;

    struct SGraph {
        SGraph()
            : bytes(0),
              ints(0),
              max_value(0)
            {
            }
        
        CRef<CSeq_graph> graph;
        CByte_graph::TValues* bytes;
        CInt_graph::TValues* ints;
        TCount max_value;
    };
    SGraph graphs[kNumStat];
    struct SSplit {
        SSplit(TSeqPos seq_pos)
            : seq_pos(seq_pos),
              file_pos_first_crossing(),
              file_pos_first_starting()
            {
            }
        TSeqPos seq_pos;
        CBGZFPos file_pos_first_crossing;
        CBGZFPos file_pos_first_starting;
    };

    typedef vector<SSplit> TSplits;
    TSplits splits;
    TSplits::iterator cur_split;
    CRef<CSeq_annot> annot;
    CSeq_annot::TData::TAlign* align_list = 0;
    typedef map<CSeq_id_Handle, int> TSeq2Chunk;
#ifdef SEPARATE_PILEUP_READS
    vector<CSeq_id_Handle> short_ids;
#else
    TSeq2Chunk* seq2chunk = 0;
    int seq_chunk_id = 0;
    size_t seq_skipped = 0;
    size_t seq_dups = 0;
    size_t seq_count = 0;
    set<CSeq_id_Handle> loaded;
    list<CRef<CBioseq>> bioseqs;
#endif
    
    SPileupGraphCreator(const string& annot_name,
                        const CSeq_id_Handle& ref_id,
                        CRange<TSeqPos> ref_range,
                        int min_quality)
        : annot_name(annot_name),
          ref_id(SerialClone(*ref_id.GetSeqId())),
          ref_range(ref_range),
          min_quality(min_quality),
          make_intron(s_GetMakeIntronGraph()),
          cur_split(splits.begin())
        {
        }

    bool AcceptAlign(const CBamAlignIterator& ait) override
        {
            if ( min_quality > 0 && ait.GetMapQuality() < min_quality ) {
                return false;
            }
            while ( cur_split != splits.end() ) {
                // skip split points that are before this alignment
                if ( !cur_split->file_pos_first_starting ) {
                    cur_split->file_pos_first_starting = ait.GetRawIndexIteratorPtr()->GetFilePos();
                }
                TSeqPos pos = ait.GetRefSeqPos();
                if ( pos >= cur_split->seq_pos ) {
                    ++cur_split;
                    continue;
                }
                if ( !ait.GetRawIndexIteratorPtr()->IsOnMinBinIndexLevel() ) {
                    // ignore non level 0 aligns - they will be collected by eChunk_align2
                    break;
                }
                TSeqPos len = ait.GetCIGARRefSize();
                TSeqPos end = pos + len;
                if ( end > cur_split->seq_pos ) {
                    // alignment crosses the split point
                    if ( !cur_split->file_pos_first_crossing ) {
                        cur_split->file_pos_first_crossing = ait.GetRawIndexIteratorPtr()->GetFilePos();
                    }
                    if ( !align_list ) {
                        annot = ait.GetSeq_annot(annot_name);
                        align_list = &annot->SetData().SetAlign();
                    }
                    align_list->push_back(ait.GetMatchAlign());
                    if ( ait.GetShortSequenceLength() != 0 ) {
                        CSeq_id_Handle seq_id = CSeq_id_Handle::GetHandle(*ait.GetShortSeq_id());
#ifdef SEPARATE_PILEUP_READS
                        short_ids.push_back(seq_id);
#else
                        auto iter = seq2chunk->insert(make_pair(seq_id, seq_chunk_id)).first;
                        if ( iter->second != seq_chunk_id ) {
                            ++seq_skipped;
                        }
                        else if ( !loaded.insert(seq_id).second ) {
                            ++seq_dups;
                        }
                        else {
                            bioseqs.push_back(ait.GetShortBioseq());
                            ++seq_count;
                        }
#endif
                    }
                }
                break;
            }
            return true;
        }

    void x_CreateGraph(SGraph& g)
        {
            _ASSERT(!g.graph);
            CRef<CSeq_graph> graph(new CSeq_graph);
            static const char* const titles[kNumStat] = {
                "Number of A bases",
                "Number of C bases",
                "Number of G bases",
                "Number of T bases",
                "Number of inserts",
                "Number of matches",
                "Number of introns"
            };
            graph->SetTitle(titles[&g-graphs]);
            CSeq_interval& loc = graph->SetLoc().SetInt();
            loc.SetId(*ref_id);
            loc.SetFrom(ref_range.GetFrom());
            loc.SetTo(ref_range.GetTo());
            TSeqPos length = ref_range.GetLength();
            graph->SetNumval(length);
            g.graph = graph;
        }
    void x_FinalizeGraph(SGraph& g)
        {
            if ( !g.graph ) {
                return;
            }
            if ( g.max_value < 256 ) {
                _ASSERT(g.bytes);
                _ASSERT(g.graph->GetGraph().IsByte());
                _ASSERT(g.graph->GetGraph().GetByte().GetValues().size() == ref_range.GetLength());
                CByte_graph& data = g.graph->SetGraph().SetByte();
                data.SetMin(0);
                data.SetMax(g.max_value);
                data.SetAxis(0);
            }
            else {
                _ASSERT(g.ints);
                _ASSERT(g.graph->GetGraph().IsInt());
                _ASSERT(g.graph->GetGraph().GetInt().GetValues().size() == ref_range.GetLength());
                CInt_graph& data = g.graph->SetGraph().SetInt();
                data.SetMin(0);
                data.SetMax(g.max_value);
                data.SetAxis(0);
            }
        }

    void x_AdjustACGT(TSeqPos ref_offset)
        {
            if ( GetSkipEmptyPileupGraphsParam() ) {
                // empty graphs can be skipped -> no adjustment
                return;
            }
            bool have_acgt = false;
            for ( int k = 0; k < kNumStat_ACGT; ++k ) {
                if ( graphs[k].graph ) {
                    have_acgt = true;
                    break;
                }
            }
            if ( have_acgt ) {
                for ( int k = 0; k < kNumStat_ACGT; ++k ) {
                    SGraph& g = graphs[k];
                    if ( g.graph ) {
                        // graph already created
                        continue;
                    }
                    x_CreateGraph(g);
                    g.bytes = &g.graph->SetGraph().SetByte().SetValues();
                    g.bytes->reserve(ref_range.GetLength());
                    NFast::AppendZerosAligned16(*g.bytes, ref_offset);
                }
            }
        }
    void Finalize()
        {
            if ( !GetSkipEmptyPileupGraphsParam() ) {
                // make missing empty graphs
                TSeqPos len = ref_range.GetLength();
                for ( int k = 0; k < kNumStat; ++k ) {
                    SGraph& g = graphs[k];
                    if ( g.graph ) {
                        // graph already created
                        continue;
                    }
                    if ( k == CBamDb::SPileupValues::kStat_Match ||
                         k == CBamDb::SPileupValues::kStat_Intron ) {
                        // do not generate empty 'matches' graph
                        continue;
                    }
                    x_CreateGraph(g);
                    g.bytes = &g.graph->SetGraph().SetByte().SetValues();
                    g.bytes->reserve(len);
                    NFast::AppendZeros(*g.bytes, len);
                }
            }
            for ( int k = 0; k < kNumStat; ++k ) {
                x_FinalizeGraph(graphs[k]);
            }
        }

    virtual void AddZerosBy16(TSeqPos len) override
        {
            for ( int k = 0; k < kNumStat; ++k ) {
                SGraph& g = graphs[k];
                if ( g.graph ) {
                    if ( g.ints ) {
                        NFast::AppendZerosAligned16(*g.ints, len);
                    }
                    else {
                        NFast::AppendZerosAligned16(*g.bytes, len);
                    }
                }
            }
        }

    bool x_UpdateMaxIsInt(SGraph& g, TCount max_added, TSeqPos ref_offset)
        {
            if ( !g.graph ) {
                _ASSERT(!g.max_value);
                g.max_value = max_added;
                x_CreateGraph(g);
                if ( max_added >= 256 ) {
                    g.ints = &g.graph->SetGraph().SetInt().SetValues();
                    g.ints->reserve(ref_range.GetLength());
                    NFast::AppendZerosAligned16(*g.ints, ref_offset);
                    return true;
                }
                else {
                    g.bytes = &g.graph->SetGraph().SetByte().SetValues();
                    g.bytes->reserve(ref_range.GetLength());
                    NFast::AppendZerosAligned16(*g.bytes, ref_offset);
                    return false;
                }
            }
            else if ( max_added >= 256 ) {
                g.max_value = max(g.max_value, max_added);
                if ( g.bytes ) {
                    CRef<CInt_graph> int_graph(new CInt_graph);
                    g.ints = &int_graph->SetValues();
                    g.ints->reserve(ref_range.GetLength());
                    size_t size = g.bytes->size();
                    NFast::ConvertBuffer(g.bytes->data(), size,
                                          NFast::AppendUninitialized(*g.ints, size));
                    g.bytes = 0;
                    g.graph->SetGraph().SetInt(*int_graph);
                }
                return true;
            }
            else if ( g.ints ) {
                return true;
            }
            else {
                g.max_value = max(g.max_value, max_added);
                return false;
            }
        }

    void x_AddValuesBy16(SGraph& g, TSeqPos len, const TCount* src)
        {
            if ( g.bytes ) {
                NFast::ConvertBuffer(src, len, NFast::AppendUninitialized(*g.bytes, len));
            }
            else if ( g.ints ) {
                NFast::CopyBuffer(src, len, NFast::AppendUninitialized(*g.ints, len));
            }
        }
    void x_AddValues(SGraph& g, TSeqPos len, const TCount* src)
        {
            if ( g.bytes ) {
                copy_n(src, len, NFast::AppendUninitialized(*g.bytes, len));
            }
            else if ( g.ints ) {
                copy_n(src, len, NFast::AppendUninitialized(*g.ints, len));
            }
        }
    void x_AddValuesBy16(TSeqPos len, const CBamDb::SPileupValues& values)
        {
            x_AddValuesBy16(graphs[kStat_Match], len, values.cc_match.data());
            x_AddValuesBy16(graphs[kStat_Gap], len, values.get_gap_counts());
            if ( make_intron ) {
                x_AddValuesBy16(graphs[kStat_Intron], len, values.get_intron_counts());
            }
            int dst_byte = 0, dst_int = 0;
            for ( int k = 0; k < kNumStat_ACGT; ++k ) {
                SGraph& g = graphs[k];
                if ( g.bytes ) {
                    ++dst_byte;
                }
                else if ( g.ints ) {
                    ++dst_int;
                }
            }
            if ( dst_byte == kNumStat_ACGT ) {
                NFast::SplitBufferInto4(values.get_acgt_counts(), len,
                                   NFast::AppendUninitialized(*graphs[0].bytes, len),
                                   NFast::AppendUninitialized(*graphs[1].bytes, len),
                                   NFast::AppendUninitialized(*graphs[2].bytes, len),
                                   NFast::AppendUninitialized(*graphs[3].bytes, len));
            }
            else if ( dst_int == kNumStat_ACGT ) {
                NFast::SplitBufferInto4(values.cc_acgt[0].cc, len,
                                   NFast::AppendUninitialized(*graphs[0].ints, len),
                                   NFast::AppendUninitialized(*graphs[1].ints, len),
                                   NFast::AppendUninitialized(*graphs[2].ints, len),
                                   NFast::AppendUninitialized(*graphs[3].ints, len));
            }
            else {
                // use split ACGT arrays
                for ( int k = 0; k < kNumStat_ACGT; ++k ) {
                    SGraph& g = graphs[k];
                    x_AddValuesBy16(g, len, values.get_split_acgt_counts(k, len));
                }
            }
        }
    void x_AddValues(TSeqPos len, const CBamDb::SPileupValues& values)
        {
            x_AddValues(graphs[kStat_Match], len, values.cc_match.data());
            x_AddValues(graphs[kStat_Gap], len, values.cc_gap.data());
            if ( make_intron ) {
                x_AddValues(graphs[kStat_Intron], len, values.cc_intron.data());
            }
            // use split ACGT into separate arrays
            for ( int k = 0; k < kNumStat_ACGT; ++k ) {
                SGraph& g = graphs[k];
                x_AddValues(g, len, values.get_split_acgt_counts(k, len));
            }
        }
    virtual void AddValuesBy16(TSeqPos len, const CBamDb::SPileupValues& values) override
        {
            _ASSERT(len > 0);
            _ASSERT(values.m_RefFrom >= ref_range.GetFrom());
            _ASSERT(values.m_RefFrom+len <= ref_range.GetToOpen());
            _ASSERT((values.m_RefFrom - ref_range.GetFrom())%16 == 0);
            _ASSERT(len%16 == 0);
            TSeqPos ref_offset = values.m_RefFrom-ref_range.GetFrom();
            for ( int k = 0; k < kNumStat; ++k ) {
                SGraph& g = graphs[k];
                TCount max_added = values.get_max_count(k);
                if ( max_added != 0 || g.graph ) {
                    x_UpdateMaxIsInt(g, max_added, ref_offset);
                }
            }
            x_AdjustACGT(ref_offset);
            x_AddValuesBy16(len, values);
        }
    virtual void AddValuesTail(TSeqPos len, const CBamDb::SPileupValues& values) override
        {
            _ASSERT(len > 0);
            _ASSERT(values.m_RefFrom >= ref_range.GetFrom());
            _ASSERT(values.m_RefFrom+len <= ref_range.GetToOpen());
            _ASSERT((values.m_RefFrom - ref_range.GetFrom())%16 == 0);
            _ASSERT(len%16 == 0 || values.m_RefFrom+len == ref_range.GetToOpen());
            TSeqPos ref_offset = values.m_RefFrom-ref_range.GetFrom();
            for ( int k = 0; k < kNumStat; ++k ) {
                SGraph& g = graphs[k];
                TCount max_added = values.get_max_count(k);
                if ( max_added != 0 || g.graph ) {
                    x_UpdateMaxIsInt(g, max_added, ref_offset);
                }
            }
            x_AdjustACGT(ref_offset);
            x_AddValues(len, values);
        }
};

void CBamRefSeqInfo::LoadPileupChunk(CTSE_Chunk_Info& chunk_info)
{
    CStopWatch sw;
    if ( GetDebugLevel() >= 2 ) {
        sw.Start();
    }
    size_t range_id = chunk_info.GetChunkId()/kChunkIdMul;
    const CBamRefSeqChunkInfo& chunk = m_Chunks[range_id];
    _ASSERT(chunk.m_PileupChunkCount);
    auto graph_range = GetChunkGraphRange(range_id);
    CTSE_Chunk_Info::TPlace place(CSeq_id_Handle(), kTSEId);
    int min_quality = m_MinMapQuality;
    _TRACE("Loading pileup "<<GetRefSeqId()<<" @ "<<chunk.GetRefSeqRange());

    SPileupGraphCreator gg(m_File->GetAnnotName(), GetRefSeq_id(), graph_range, min_quality);
    
    CMutexGuard seq2chunk_guard(eEmptyGuard);
    if ( chunk.m_PileupChunkCount ) {
        for ( int i = 1; i < chunk.m_PileupChunkCount; ++i ) {
            gg.splits.push_back(m_Chunks[range_id+i].GetRefSeqRange().GetFrom());
        }
        gg.cur_split = gg.splits.begin();
#ifndef SEPARATE_PILEUP_READS
        seq2chunk_guard.Guard(m_Seq2ChunkMutex);
        gg.seq_chunk_id = chunk_info.GetChunkId();
        gg.seq2chunk = &m_Seq2Chunk;
#endif
    }
    
    CBamDb::SPileupValues ss;
    CBamDb::SPileupValues::EIntronMode intron_mode =
        s_GetMakeIntronGraph()?
        CBamDb::SPileupValues::eCountIntron:
        CBamDb::SPileupValues::eNoCountIntron;
    TSeqPos gap_to_intron_threshold = s_GetGapToIntronThreshold();
    size_t count = m_File->GetBamDb().CollectPileup(ss, GetRefSeqId(), graph_range, &gg,
                                                    intron_mode, gap_to_intron_threshold);

#ifndef SEPARATE_PILEUP_READS
    gg.seq2chunk = 0;
    seq2chunk_guard.Release();
#endif
    
    for ( int i = 1; i < chunk.m_PileupChunkCount; ++i ) {
        m_Chunks[range_id+i].m_FilePosFirstCrossing = gg.splits[i-1].file_pos_first_crossing;
        m_Chunks[range_id+i].m_FilePosFirstStarting = gg.splits[i-1].file_pos_first_starting;
    }
    
    if ( GetDebugLevel() >= 3 ) {
        LOG_POST_X(11, Info<<"CBAMDataLoader: "
                   "Collected pileup counts "<<GetRefSeqId()<<
                   " id="<<chunk_info.GetChunkId()<<
                   " @ "<<chunk.GetRefSeqRange()<<": "<<
                   count<<" in "<<sw.Elapsed());
    }
    if ( count == 0 ) {
        // zero pileup graphs
        chunk_info.SetLoaded();
        return;
    }

    gg.Finalize();
    CRef<CSeq_annot> annot(new CSeq_annot);
    {
        string name = m_File->GetAnnotName();
        name += ' ';
        name += PILEUP_NAME_SUFFIX;
        CRef<CAnnotdesc> desc(new CAnnotdesc);
        desc->SetName(name);
        annot->SetDesc().Set().push_back(desc);
    }
    size_t total_bytes = 0;
    for ( int k = 0; k < ss.kNumStat; ++k ) {
        SPileupGraphCreator::SGraph& g = gg.graphs[k];
        if ( g.graph ) {
            annot->SetData().SetGraph().push_back(g.graph);
            if ( g.bytes ) {
                total_bytes += g.bytes->size()*sizeof(g.bytes->front())+10000;
            }
            else {
                total_bytes += g.ints->size()*sizeof(g.ints->front())+10000;
            }
        }
    }
    chunk_info.x_LoadAnnot(place, *annot);
    size_t align_count = 0;
    if ( gg.annot ) {
        // add alignments that cross sub-ranges borders
        align_count = gg.annot->GetData().GetAlign().size();
        //LOG_POST("Pileup align annot: "<<MSerial_AsnText<<*gg.annot);
        chunk_info.x_LoadAnnot(place, *gg.annot);
#ifdef SEPARATE_PILEUP_READS
        x_AddSeqChunk(chunk_info, gg.short_ids);
#else
        chunk_info.x_LoadBioseqs(place, gg.bioseqs);
#endif
    }
    chunk_info.x_AddUsedMemory(total_bytes);

    if ( GetDebugLevel() >= 3 ) {
        LOG_POST_X(11, Info<<"CBAMDataLoader: "
                   "Loaded pileup "<<GetRefSeqId()<<
                   " id="<<chunk_info.GetChunkId()<<
                   " @ "<<chunk.GetRefSeqRange()<<": "<<
                   count<<" ("<<align_count<<" aligns) in "<<sw.Elapsed());
    }
    
    chunk_info.SetLoaded();
}

#else // !USE_NEW_PILEUP

BEGIN_LOCAL_NAMESPACE;

struct SBaseStats
{
    typedef unsigned TCount;

    enum EStat {
        kStat_A = 0,
        kStat_C = 1,
        kStat_G = 2,
        kStat_T = 3,
        kStat_Gap = 4,
        kStat_Match = 5,
        kStat_Intron = 6,
        kNumStat = 7
    };
    
    TSeqPos len;
    bool make_intron_graph;
    TSeqPos gap_to_intron_threshold;
    vector<TCount> cc[kNumStat];
    
    explicit
    SBaseStats(TSeqPos len)
        : len(len),
          make_intron_graph(s_GetMakeIntronGraph()),
          gap_to_intron_threshold(s_GetGapToIntronThreshold())
        {
            for ( int k = 0; k < kNumStat; ++k ) {
                if ( k == kStat_Intron && !make_intron_graph ) {
                    continue;
                }
                cc[k].resize(len);
            }
        }

    void add_match(TSeqPos pos)
        {
            if ( pos < len ) {
                cc[kStat_Match][pos] += 1;
            }
        }
    void add_base(TSeqPos pos, char b)
        {
            if ( pos < len ) {
                switch ( b ) {
                case 'A': cc[kStat_A][pos] += 1; break;
                case 'C': cc[kStat_C][pos] += 1; break;
                case 'G': cc[kStat_G][pos] += 1; break;
                case 'T': cc[kStat_T][pos] += 1; break;
                case '=': cc[kStat_Match][pos] += 1; break;
                    // others including N are unknown mismatch, no pileup information
                }
            }
        }
    void add_base_raw(TSeqPos pos, Uint1 b)
        {
            if ( pos < len ) {
                switch ( b ) {
                case 1: /* A */ cc[kStat_A][pos] += 1; break;
                case 2: /* C */ cc[kStat_C][pos] += 1; break;
                case 4: /* G */ cc[kStat_G][pos] += 1; break;
                case 8: /* T */ cc[kStat_T][pos] += 1; break;
                case 0: /* = */ cc[kStat_Match][pos] += 1; break;
                    // others including N (=15) are unknown mismatch, no pileup information
                }
            }
        }
    void x_add_gap_or_intron(TSignedSeqPos gap_pos, TSeqPos gap_len, EStat stat)
        {
            _ASSERT(stat == kStat_Gap || stat == kStat_Intron);
            if ( gap_pos < 0 ) {
                if ( TSignedSeqPos(gap_len + gap_pos) <= 0 ) {
                    // gap is fully before graph segment
                    return;
                }
                gap_len += gap_pos;
                gap_pos = 0;
            }
            else if ( TSeqPos(gap_pos) >= len ) {
                // gap is fully after graph segment
                return;
            }
            TSeqPos gap_end = gap_pos + gap_len;
            if ( gap_end > len ) {
                // gap goes beyond end of graph segment
                gap_end = len;
            }
            cc[stat][gap_pos] += 1;
            if ( gap_end < len ) {
                cc[stat][gap_end] -= 1;
            }
        }
    void add_intron(TSignedSeqPos gap_pos, TSeqPos gap_len)
        {
            if ( make_intron_graph ) {
                x_add_gap_or_intron(gap_pos, gap_len, kStat_Intron);
            }
        }
    void add_gap(TSignedSeqPos gap_pos, TSeqPos gap_len)
        {
            if ( gap_len > gap_to_intron_threshold ) {
                add_intron(gap_pos, gap_len);
            }
            else {
                x_add_gap_or_intron(gap_pos, gap_len, kStat_Gap);
            }
        }
    void x_finish_add(EStat stat)
        {
            _ASSERT(stat == kStat_Gap || stat == kStat_Intron);
            TCount g = 0;
            for ( TSeqPos i = 0; i < len; ++i ) {
                g += cc[stat][i];
                cc[stat][i] = g;
            }
        }
    void finish_add()
        {
            x_finish_add(kStat_Gap);
            if ( make_intron_graph ) {
                x_finish_add(kStat_Intron);
            }
        }

    TCount get_max_count(int type) const
        {
            return cc[type].empty()? 0: *max_element(cc[type].begin(), cc[type].end());
        }
    void get_maxs(TCount (&c_max)[kNumStat]) const
        {
            for ( int k = 0; k < kNumStat; ++k ) {
                c_max[k] = get_max_count(k);
            }
        }
};


static inline
Uint1 sx_GetBaseRaw(CTempString read_raw, TSeqPos pos)
{
    Uint1 b2 = read_raw[pos/2];
    return pos%2? b2&0xf: b2>>4;
}


END_LOCAL_NAMESPACE;


void CBamRefSeqInfo::LoadPileupChunk(CTSE_Chunk_Info& chunk_info)
{
    CStopWatch sw;
    if ( GetDebugLevel() >= 3 ) {
        sw.Start();
    }
    size_t range_id = chunk_info.GetChunkId()/kChunkIdMul;
    const CBamRefSeqChunkInfo& chunk = m_Chunks[range_id];
    auto graph_range = GetChunkGraphRange(range_id);
    CTSE_Chunk_Info::TPlace place(CSeq_id_Handle(), kTSEId);
    int min_quality = m_MinMapQuality;
    _TRACE("Loading pileup "<<GetRefSeqId()<<" @ "<<chunk.GetRefSeqRange());
    size_t count = 0, skipped = 0;

    SBaseStats ss(graph_range.GetLength());
    
#ifdef SKIP_TOO_LONG_ALIGNMENTS
    TSeqPos ref_len = m_File->GetRefSeqLength(GetRefSeqId());
#endif
    CBamAlignIterator ait(*m_File, GetRefSeqId(), graph_range.GetFrom(), graph_range.GetLength());
    if ( m_SpotIdDetector ) {
        ait.SetSpotIdDetector(m_SpotIdDetector.GetNCPointer());
    }
    if ( CBamRawAlignIterator* rit = ait.GetRawIndexIteratorPtr() ) {
        for( ; ait; ++ait ) {
            if ( !ss.AcceptAlign(ait) ) {
                ++skipped;
                continue;
            }
            TSeqPos align_pos = rit->GetRefSeqPos();
#ifdef SKIP_TOO_LONG_ALIGNMENTS
            TSeqPos align_end = align_pos + ait.GetCIGARRefSize();
            if ( align_end > ref_len ) {
                ++skipped;
                continue;
            }
#endif            
            ++count;

            TSeqPos ss_pos = align_pos - graph_range.GetFrom();
            TSeqPos read_pos = 0;
            CTempString read_raw = rit->GetShortSequenceRaw();
            for ( Uint2 i = 0, count = rit->GetCIGAROpsCount(); i < count; ++i ) {
                Uint4 op = rit->GetCIGAROp(i);
                Uint4 seglen = op >> 4;
                switch ( op & 0xf ) {
                case SBamAlignInfo::kCIGAR_eq: // =
                    // match
                    for ( TSeqPos i = 0; i < seglen; ++i ) {
                        ss.add_match(ss_pos);
                        ++ss_pos;
                    }
                    read_pos += seglen;
                    break;
                case SBamAlignInfo::kCIGAR_M: // M
                case SBamAlignInfo::kCIGAR_X: // X
                    // mismatch ('X') or
                    // unspecified 'alignment match' ('M') that can be a mismatch too
                    for ( TSeqPos i = 0; i < seglen; ++i ) {
                        ss.add_base_raw(ss_pos, sx_GetBaseRaw(read_raw, read_pos));
                        ++ss_pos;
                        ++read_pos;
                    }
                    break;
                case SBamAlignInfo::kCIGAR_I: // I
                case SBamAlignInfo::kCIGAR_S: // S
                    read_pos += seglen;
                    break;
                case SBamAlignInfo::kCIGAR_N: // N
                    // intron
                    ss.add_intron(ss_pos, seglen);
                    ss_pos += seglen;
                    break;
                case SBamAlignInfo::kCIGAR_D: // D
                    // gap or intron
                    ss.add_gap(ss_pos, seglen);
                    ss_pos += seglen;
                    break;
                default: // P
                    break;
                }
            }
        }
    }
    else {
        for( ; ait; ++ait ) {
            if ( !ss.AcceptAlign(ait) ) {
                ++skipped;
                continue;
            }
            TSeqPos align_pos = ait.GetRefSeqPos();
#ifdef SKIP_TOO_LONG_ALIGNMENTS
            TSeqPos align_end = align_pos + ait.GetCIGARRefSize();
            if ( align_end > ref_len ) {
                ++skipped;
                continue;
            }
#endif            
            ++count;

            TSignedSeqPos ss_pos = align_pos - graph_range.GetFrom();
            TSeqPos read_pos = ait.GetCIGARPos();
            CTempString read = ait.GetShortSequence();
            CTempString cigar = ait.GetCIGAR();
            const char* ptr = cigar.data();
            const char* end = ptr + cigar.size();
            while ( ptr != end ) {
                char type = *ptr;
                TSeqPos seglen = 0;
                for ( ; ++ptr != end; ) {
                    char c = *ptr;
                    if ( c >= '0' && c <= '9' ) {
                        seglen = seglen*10+(c-'0');
                    }
                    else {
                        break;
                    }
                }
                if ( seglen == 0 ) {
                    ERR_POST_X(4, "CBAMDataLoader: Bad CIGAR length: "<<type<<"0 in "<<cigar);
                    break;
                }
                if ( type == '=' ) {
                    // match
                    for ( TSeqPos i = 0; i < seglen; ++i ) {
                        ss.add_match(ss_pos);
                        ++ss_pos;
                    }
                    read_pos += seglen;
                }
                else if ( type == 'M' || type == 'X' ) {
                    // mismatch ('X') or
                    // unspecified 'alignment match' ('M') that can be a mismatch too
                    for ( TSeqPos i = 0; i < seglen; ++i ) {
                        ss.add_base(ss_pos, read[read_pos]);
                        ++ss_pos;
                        ++read_pos;
                    }
                }
                else if ( type == 'I' || type == 'S' ) {
                    if ( type == 'S' ) {
                        // soft clipping already accounted in seqpos
                        continue;
                    }
                    read_pos += seglen;
                }
                else if ( type == 'N' ) {
                    // intron
                    ss.add_intron(ss_pos, seglen);
                    ss_pos += seglen;
                }
                else if ( type == 'D' ) {
                    // gap or intron
                    ss.add_gap(ss_pos, seglen);
                    ss_pos += seglen;
                }
                else if ( type != 'P' ) {
                    ERR_POST_X(14, "CBAMDataLoader: Bad CIGAR char: "<<type<<" in "<<cigar);
                    break;
                }
            }
        }
    }
    if ( GetDebugLevel() >= 3 ) {
        LOG_POST_X(11, Info<<"CBAMDataLoader: "
                   "Collected pileup counts "<<GetRefSeqId()<<
                   " id="<<chunk_info.GetChunkId()<<
                   " @ "<<chunk.GetRefSeqRange()<<": "<<
                   count<<" skipped: "<<skipped<<" in "<<sw.Elapsed());
    }
    if ( count == 0 ) {
        // zero pileup graphs
        chunk_info.SetLoaded();
        return;
    }

    ss.finish_add();
    
    CRef<CSeq_annot> annot(new CSeq_annot);
    {
        string name = m_File->GetAnnotName();
        name += ' ';
        name += PILEUP_NAME_SUFFIX;
        CRef<CAnnotdesc> desc(new CAnnotdesc);
        desc->SetName(name);
        annot->SetDesc().Set().push_back(desc);
    }
    size_t total_bytes = 0;
    CRef<CSeq_id> ref_id(SerialClone(*GetRefSeq_id().GetSeqId()));
    for ( int k = 0; k < SBaseStats::kNumStat; ++k ) {
        SBaseStats::TCount max = ss.get_max_count(k);
        if ( max == 0 ) {
            if ( k == SBaseStats::kStat_Match || k == SBaseStats::kStat_Intron ) {
                // do not generate empty 'matches' or 'introns' graph
                continue;
            }
            if ( GetSkipEmptyPileupGraphsParam() ) {
                // do not generate empty graph (configurable)
                continue;
            }
        }
        CRef<CSeq_graph> graph(new CSeq_graph);
        static const char* const titles[SBaseStats::kNumStat] = {
            "Number of A bases",
            "Number of C bases",
            "Number of G bases",
            "Number of T bases",
            "Number of inserts",
            "Number of matches",
            "Number of introns"
        };
        graph->SetTitle(titles[k]);
        CSeq_interval& loc = graph->SetLoc().SetInt();
        loc.SetId(*ref_id);
        loc.SetFrom(graph_range.GetFrom());
        loc.SetTo(graph_range.GetTo());
        graph->SetNumval(graph_range.GetLength());

        if ( max < 256 ) {
            CByte_graph& data = graph->SetGraph().SetByte();
            data.SetValues().assign(ss.cc[k].begin(), ss.cc[k].end());
            data.SetMin(0);
            data.SetMax(max);
            data.SetAxis(0);
            total_bytes += graph_range.GetLength()*sizeof(data.GetValues()[0])+10000;
        }
        else {
            CInt_graph& data = graph->SetGraph().SetInt();
            data.SetValues().assign(ss.cc[k].begin(), ss.cc[k].end());
            data.SetMin(0);
            data.SetMax(max);
            data.SetAxis(0);
            total_bytes += graph_range.GetLength()*sizeof(data.GetValues()[0])+10000;
        }
        annot->SetData().SetGraph().push_back(graph);
    }
    chunk_info.x_LoadAnnot(place, *annot);
    chunk_info.x_AddUsedMemory(total_bytes);

    if ( GetDebugLevel() >= 3 ) {
        LOG_POST_X(11, Info<<"CBAMDataLoader: "
                   "Loaded pileup "<<GetRefSeqId()<<" @ "<<
                   " id="<<chunk_info.GetChunkId()<<
                   chunk.GetRefSeqRange()<<": "<<
                   count<<" skipped: "<<skipped<<" in "<<sw.Elapsed());
    }

    chunk_info.SetLoaded();
}
#endif // USE_NEW_PILEUP


/////////////////////////////////////////////////////////////////////////////
// CBamRefSeqChunkInfo
/////////////////////////////////////////////////////////////////////////////


void CBamRefSeqChunkInfo::AddRefSeqRange(const TRange& range)
{
    ++m_AlignCount;
    m_RefSeqRange += range;
    m_MaxRefSeqFrom = max(m_MaxRefSeqFrom, range.GetFrom());
}


END_SCOPE(objects)
END_NCBI_SCOPE
