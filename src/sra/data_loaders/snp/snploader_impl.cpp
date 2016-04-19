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

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/general/general__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqres/seqres__.hpp>

#include <objmgr/annot_selector.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/tse_loadlock.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/tse_split_info.hpp>
#include <objmgr/data_loader_factory.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>

#include <sra/error_codes.hpp>
#include <sra/readers/ncbi_traces_path.hpp>
#include <sra/data_loaders/snp/snploader.hpp>
#include <sra/data_loaders/snp/impl/snploader_impl.hpp>

#include <util/sequtil/sequtil_manip.hpp>

#include <algorithm>
#include <cmath>

BEGIN_NCBI_SCOPE

#define NCBI_USE_ERRCODE_X   SNPLoader
NCBI_DEFINE_ERR_SUBCODE_X(16);

BEGIN_SCOPE(objects)

class CDataLoader;

// magic chunk ids
static const int kTSEId = 1;
static const int kMainChunkId = -1;

// algirithm options

// splitter parameters for aligns and graphs
static const unsigned kSNPChunkSize = 10000;
static const unsigned kGraphChunkSize = 10000;
static const unsigned kAlignEmptyPages = 16;
static const unsigned kGraphEmptyPages = 4;
static const unsigned kChunkGraphMul = 8;
static const unsigned kChunkSeqDataMul = 8;

#define PILEUP_NAME_SUFFIX "pileup graphs"

NCBI_PARAM_DECL(int, SNP_LOADER, DEBUG);
NCBI_PARAM_DEF_EX(int, SNP_LOADER, DEBUG, 0,
                  eParam_NoThread, SNP_LOADER_DEBUG);

static int GetDebugLevel(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(SNP_LOADER, DEBUG)> s_Value;
    return s_Value->Get();
}


NCBI_PARAM_DECL(size_t, SNP_LOADER, GC_SIZE);
NCBI_PARAM_DEF_EX(size_t, SNP_LOADER, GC_SIZE, 10,
                  eParam_NoThread, SNP_LOADER_GC_SIZE);

static size_t GetGCSize(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(SNP_LOADER, GC_SIZE)> s_Value;
    return s_Value->Get();
}


NCBI_PARAM_DECL(size_t, SNP_LOADER, MISSING_GC_SIZE);
NCBI_PARAM_DEF_EX(size_t, SNP_LOADER, MISSING_GC_SIZE, 10000,
                  eParam_NoThread, SNP_LOADER_MISSING_GC_SIZE);

static size_t GetMissingGCSize(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(SNP_LOADER, MISSING_GC_SIZE)> s_Value;
    return s_Value->Get();
}


/////////////////////////////////////////////////////////////////////////////
// CSNPBlobId
/////////////////////////////////////////////////////////////////////////////

CSNPBlobId::CSNPBlobId(const CTempString& str)
{
    FromString(str);
}


CSNPBlobId::CSNPBlobId(const CSNPFileInfo& file,
                       const CSeq_id_Handle& seq_id)
    : m_File(file.GetFileName()),
      m_SeqId(seq_id)
{
}


CSNPBlobId::~CSNPBlobId(void)
{
}


static const char kFileEnd[] = "|||";

string CSNPBlobId::ToString(void) const
{
    CNcbiOstrstream out;
    out << m_File;
    out << kFileEnd;
    out << m_SeqId;
    return CNcbiOstrstreamToString(out);
}


void CSNPBlobId::FromString(CTempString str0)
{
    CTempString str = str0;
    SIZE_TYPE div = str.rfind(kFileEnd);
    if ( div == NPOS ) {
        NCBI_THROW_FMT(CSraException, eOtherError,
                       "Bad CSNPBlobId: "<<str0);
    }
    m_File = str.substr(0, div);
    str = str.substr(div+strlen(kFileEnd));
    m_SeqId = CSeq_id_Handle::GetHandle(str);
}


bool CSNPBlobId::operator<(const CBlobId& id) const
{
    const CSNPBlobId& id2 = dynamic_cast<const CSNPBlobId&>(id);
    if ( m_File != id2.m_File ) {
        return m_File < id2.m_File;
    }
    return m_SeqId < id2.m_SeqId;
}


bool CSNPBlobId::operator==(const CBlobId& id) const
{
    const CSNPBlobId& id2 = dynamic_cast<const CSNPBlobId&>(id);
    return m_SeqId == id2.m_SeqId &&
        m_File == id2.m_File;
}


/////////////////////////////////////////////////////////////////////////////
// CSNPDataLoader_Impl
/////////////////////////////////////////////////////////////////////////////


CSNPDataLoader_Impl::CSNPDataLoader_Impl(
    const CSNPDataLoader::SLoaderParams& params)
    : m_FoundFiles(GetGCSize()),
      m_MissingFiles(GetMissingGCSize())
{
    m_DirPath = params.m_DirPath;
    m_AnnotName = params.m_AnnotName;
    
    if ( params.m_VDBFiles.empty() ) {
        if ( !m_DirPath.empty() ) {
            m_DirPath.erase();
            AddFixedFile(params.m_DirPath);
        }
    }
    for ( auto& file : params.m_VDBFiles ) {
        AddFixedFile(file);
    }
}


CSNPDataLoader_Impl::~CSNPDataLoader_Impl(void)
{
}


void CSNPDataLoader_Impl::AddFixedFile(const string& file)
{
    m_FixedFiles[file] = new CSNPFileInfo(*this, file);
}


CRef<CSNPFileInfo> CSNPDataLoader_Impl::GetFixedFile(const string& acc)
{
    TFixedFiles::iterator it = m_FixedFiles.find(acc);
    if ( it != m_FixedFiles.end() ) {
        return it->second;
    }
    return null;
}


CRef<CSNPFileInfo> CSNPDataLoader_Impl::FindFile(const string& acc)
{
    if ( !m_FixedFiles.empty() ) {
        // no dynamic accessions
        return null;
    }
    CMutexGuard guard(m_Mutex);
    TMissingFiles::iterator it2 = m_MissingFiles.find(acc);
    if ( it2 != m_MissingFiles.end() ) {
        return null;
    }
    TFoundFiles::iterator it = m_FoundFiles.find(acc);
    if ( it != m_FoundFiles.end() ) {
        return it->second;
    }
    CRef<CSNPFileInfo> info;
    try {
        info = new CSNPFileInfo(*this, acc);
    }
    catch ( CSraException& exc ) {
        if ( exc.GetErrCode() == exc.eNotFoundDb ||
             exc.GetErrCode() == exc.eProtectedDb ) {
            // no such SRA table
            return null;
        }
        ERR_POST_X(4, "CSNPDataLoader::FindFile("<<acc<<"): accession not found: "<<exc);
        m_MissingFiles[acc] = true;
        return null;
    }
    // store file in cache
    m_FoundFiles[acc] = info;
    return info;
}


CRef<CSNPFileInfo>
CSNPDataLoader_Impl::GetFileInfo(const CSNPBlobId& blob_id)
{
    if ( !m_FixedFiles.empty() ) {
        return GetFixedFile(blob_id.m_File);
    }
    else {
        return FindFile(blob_id.m_File);
    }
}


CTSE_LoadLock CSNPDataLoader_Impl::GetBlobById(CDataSource* data_source,
                                                const CSNPBlobId& blob_id)
{
    CDataLoader::TBlobId loader_blob_id(&blob_id);
    CTSE_LoadLock load_lock = data_source->GetTSE_LoadLock(loader_blob_id);
    if ( !load_lock.IsLoaded() ) {
        LoadBlob(blob_id, load_lock);
        load_lock.SetLoaded();
    }
    return load_lock;
}


CDataLoader::TTSE_LockSet
CSNPDataLoader_Impl::GetRecords(CDataSource* data_source,
                                 const CSeq_id_Handle& idh,
                                 CDataLoader::EChoice choice)
{
    CDataLoader::TTSE_LockSet locks;
    // SNPs are available by NA accession only, see GetOrphanAnnotRecords()
    return locks;
}


CDataLoader::TTSE_LockSet
CSNPDataLoader_Impl::GetOrphanAnnotRecords(CDataSource* ds,
                                           const CSeq_id_Handle& id,
                                           const SAnnotSelector* sel)
{
    CDataLoader::TTSE_LockSet locks;
    // implicitly load NA accessions
    if ( sel && sel->IsIncludedAnyNamedAnnotAccession() ) {
        const SAnnotSelector::TNamedAnnotAccessions& accs =
            sel->GetNamedAnnotAccessions();
        if ( m_FoundFiles.get_size_limit() < accs.size() ) {
            // increase VDB cache size
            m_FoundFiles.set_size_limit(accs.size()+GetGCSize());
        }
        if ( m_MissingFiles.get_size_limit() < accs.size() ) {
            // increase VDB cache size
            m_MissingFiles.set_size_limit(accs.size()+GetMissingGCSize());
        }
        ITERATE ( SAnnotSelector::TNamedAnnotAccessions, it, accs ) {
            if ( CRef<CSNPFileInfo> info = FindFile(it->first) ) {
                if ( CRef<CSNPSeqInfo> seq = info->GetSeqInfo(id) ) {
                    locks.insert(GetBlobById(ds, *seq->GetBlobId()));
                }
            }
        }
    }
    return locks;
}


void CSNPDataLoader_Impl::LoadBlob(const CSNPBlobId& blob_id,
                                    CTSE_LoadLock& load_lock)
{
    CRef<CSNPFileInfo> file_info = GetFileInfo(blob_id);
    file_info->GetSeqInfo(blob_id)->LoadAnnotBlob(load_lock);
}


void CSNPDataLoader_Impl::LoadChunk(const CSNPBlobId& blob_id,
                                    CTSE_Chunk_Info& chunk_info)
{
    _TRACE("Loading chunk "<<blob_id.ToString()<<"."<<chunk_info.GetChunkId());
    CRef<CSNPFileInfo> file_info = GetFileInfo(blob_id);
    file_info->GetSeqInfo(blob_id)->LoadAnnotChunk(chunk_info);
}


CSNPDataLoader::TAnnotNames
CSNPDataLoader_Impl::GetPossibleAnnotNames(void) const
{
    TAnnotNames names;
    names.push_back("SNP");
    return names;
}


/////////////////////////////////////////////////////////////////////////////
// CSNPFileInfo
/////////////////////////////////////////////////////////////////////////////


CSNPFileInfo::CSNPFileInfo(CSNPDataLoader_Impl& impl,
                           const string& acc)
{
    //CMutexGuard guard(GetMutex());
    x_Initialize(impl, acc);
    for ( CSNPDbSeqIterator rit(m_SNPDb); rit; ++rit ) {
        AddSeq(rit.GetSeqIdHandle());
    }
}


void CSNPFileInfo::x_Initialize(CSNPDataLoader_Impl& impl,
                                 const string& csra)
{
    m_FileName = csra;
    m_AnnotName = impl.m_AnnotName;
    if ( m_AnnotName.empty() ) {
        m_AnnotName = "SNP";
    }
    m_SNPDb = CSNPDb(impl.m_Mgr, CDirEntry::MakePath(impl.m_DirPath, csra));
    if ( GetDebugLevel() >= 1 ) {
        LOG_POST_X(8, Info <<
                   "CSNPDataLoader("<<csra<<")="<<csra);
    }
}


string CSNPFileInfo::GetSNPAnnotName(void) const
{
    return "SNP";
}


string CSNPFileInfo::GetGraphAnnotName(void) const
{
    return "SNP";
}


void CSNPFileInfo::GetPossibleAnnotNames(TAnnotNames& names) const
{
    names.push_back(CAnnotName("SNP"));
}


void CSNPFileInfo::AddSeq(const CSeq_id_Handle& id)
{
    if ( GetDebugLevel() >= 1 ) {
        LOG_POST_X(9, Info << "CSNPDataLoader(" << m_FileName << "): "
                   "Found "<<id);
    }
    m_Seqs[id] = new CSNPSeqInfo(this, id);
}


CRef<CSNPSeqInfo>
CSNPFileInfo::GetSeqInfo(const CSeq_id_Handle& seq_id)
{
    //CMutexGuard guard(GetMutex());
    TSeqs::const_iterator it = m_Seqs.find(seq_id);
    if ( it != m_Seqs.end() ) {
        return it->second;
    }
    return null;
}


/////////////////////////////////////////////////////////////////////////////
// CSNPSeqInfo
/////////////////////////////////////////////////////////////////////////////


CSNPSeqInfo::CSNPSeqInfo(CSNPFileInfo* file,
                         const CSeq_id_Handle& seq_id)
    : m_File(file),
      m_SeqId(seq_id)
{
}


CRef<CSNPBlobId> CSNPSeqInfo::GetBlobId(void) const
{
    return Ref(new CSNPBlobId(*m_File, GetSeqId()));
}


void CSNPSeqInfo::SetBlobId(CRef<CSNPBlobId>& ret,
                            const CSeq_id_Handle& idh) const
{
    CRef<CSNPBlobId> id(new CSNPBlobId(*m_File, GetSeqId()));
    if ( ret ) {
        ERR_POST_X(3, "CSNPDataLoader::GetBlobId: "
                   "Seq-id "<<idh<<" appears in two files: "
                   <<ret->ToString()<<" & "<<id->ToString());
    }
    else {
        ret = id;
    }
}


CSNPDbSeqIterator
CSNPSeqInfo::GetSeqIterator(void) const
{
    return CSNPDbSeqIterator(*m_File, GetSeqId());
}


void CSNPSeqInfo::LoadAnnotBlob(CTSE_LoadLock& load_lock)
{
    CRef<CSeq_entry> entry(new CSeq_entry);
    CSNPDbSeqIterator::TAnnotSet annots =
        GetSeqIterator().GetTableFeatAnnots(CRange<TSeqPos>::GetWhole());
    for ( auto& annot : annots ) {
        entry->SetSet().SetAnnot().push_back(annot);
    }
    load_lock->SetSeq_entry(*entry);
}


void CSNPSeqInfo::LoadAnnotChunk(CTSE_Chunk_Info& chunk_info)
{
}


#if 0
namespace {
    struct SRefStat {
        SRefStat(void)
            : m_Count(0),
              m_RefPosFirst(0),
              m_RefPosLast(0)
            {
            }

        unsigned m_Count;
        TSeqPos m_RefPosFirst;
        TSeqPos m_RefPosLast;

        void Collect(CSNPDb& csra_db, const CSeq_id_Handle& ref_id,
                     TSeqPos ref_pos, unsigned count, int min_quality);

        unsigned GetStatCount(void) const {
            return m_Count;
        }
        double GetStatLen(void) const {
            return m_RefPosLast - m_RefPosFirst + .5;
        }
    };
    
    
    void SRefStat::Collect(CSNPDb& csra_db, const CSeq_id_Handle& ref_id,
                           TSeqPos ref_pos, unsigned count, int min_quality)
    {
        m_RefPosFirst = kInvalidSeqPos;
        m_RefPosLast = 0;
        size_t skipped = 0;
        CCSraAlignIterator ait(csra_db, ref_id, ref_pos);
        for ( ; ait; ++ait ) {
            if ( min_quality > 0 && ait.GetMapQuality() < min_quality ) {
                ++skipped;
                continue;
            }
            TSeqPos pos = ait.GetRefSeqPos();
            _ASSERT(pos >= ref_pos); // filtering is done by CCSraAlignIterator
            if ( pos < m_RefPosFirst ) {
                m_RefPosFirst = pos;
            }
            if ( pos > m_RefPosLast ) {
                m_RefPosLast = pos;
            }
            if ( ++m_Count == count ) {
                break;
            }
        }
        if ( GetDebugLevel() >= 2 ) {
            LOG_POST_X(4, Info << "CSNPDataLoader: "
                       "Stat @ "<<ref_pos<<": "<<m_Count<<" entries: "<<
                       m_RefPosFirst<<"-"<<m_RefPosLast<<
                       " skipped: "<<skipped);
        }
    }
};


void CSNPSeqInfo::LoadRanges(void)
{
    if ( !m_AlignChunks.empty() ) {
        return;
    }
    CMutexGuard guard(m_File->GetMutex());
    if ( !m_AlignChunks.empty() ) {
        return;
    }

    _TRACE("Loading "<<GetRefSeqId());

    m_CovAnnot = CCSraRefSeqIterator(*m_File, GetRefSeqId())
        .GetCoverageAnnot(m_File->GetAlignAnnotName());
    x_LoadRangesStat();

    _TRACE("Loaded ranges on "<<GetRefSeqId());
    _ASSERT(!m_AlignChunks.empty());
    if ( GetDebugLevel() >= 2 ) {
        CRange<TSeqPos> range;
        for ( size_t k = 0; k+1 < m_AlignChunks.size(); ++k ) {
            if ( !m_AlignChunks[k].align_count ) {
                continue;
            }
            range.SetFrom(m_AlignChunks[k].start_pos);
            range.SetToOpen(m_AlignChunks[k+1].start_pos);
            LOG_POST_X(6, Info << "CSNPDataLoader: "
                       "Align Chunk "<<k<<": "<<range<<
                       " with "<<m_AlignChunks[k].align_count<<" aligns");
        }
        for ( size_t k = 0; k+1 < m_GraphChunks.size(); ++k ) {
            if ( !m_GraphChunks[k].align_count ) {
                continue;
            }
            range.SetFrom(m_GraphChunks[k].start_pos);
            range.SetToOpen(m_GraphChunks[k+1].start_pos);
            LOG_POST_X(6, Info << "CSNPDataLoader: "
                       "Graph Chunk "<<k<<": "<<range<<
                       " with "<<m_GraphChunks[k].align_count<<" aligns");
        }
    }
}


void CSNPSeqInfo::x_LoadRangesStat(void)
{
    if ( kUseFullAlignCounts ) {
        TSeqPos segment_len = m_File->GetDb().GetRowSize();
        const size_t kAlignLimitCount = kAlignChunkSize;
        const size_t kGraphLimitCount = kGraphChunkSize;
        const TSeqPos kAlignEmptyLength = kAlignEmptyPages*segment_len;
        const TSeqPos kGraphEmptyLength = kGraphEmptyPages*segment_len;
        const TSeqPos kGraphLimitLength = segment_len*kChunkGraphMul;
        CCSraRefSeqIterator iter(*m_File, GetRefSeqId());
        TSeqPos ref_length = iter.GetSeqLength();
        uint64_t total = 0;

        SChunkInfo a = { 0, 0 }, g = { 0, 0 };
        TSeqPos a_empty = 0, g_empty = 0;
        for ( TSeqPos p = 0; p < ref_length; p += segment_len ) {
            TSeqPos end = min(ref_length, p + segment_len);
            unsigned c = unsigned(iter.GetAlignCountAtPos(p));
            total += c;

            // align chunk
            // If this page itself is too big
            // or previous empty range is too big
            // add previous range as a separate chunk
            if ( (p > a.start_pos && a.align_count+c >= 2*kAlignLimitCount) ||
                 (p >= a.start_pos+kAlignEmptyLength && c && !a.align_count) ) {
                m_AlignChunks.push_back(a);
                a.start_pos = p;
                a.align_count = 0;
            }
            if ( c ) {
                a.align_count += c;
                a_empty = end;
            }
            else if ( a.align_count && end >= a_empty+kAlignEmptyLength ) {
                // too large empty region started
                m_AlignChunks.push_back(a);
                a.start_pos = a_empty;
                a.align_count = 0;
            }
            if ( a.align_count >= kAlignLimitCount ) {
                // collected data is big enough -> add it as a chunk
                m_AlignChunks.push_back(a);
                a.start_pos = end;
                a.align_count = 0;
            }

            // graph chunk
            // If this page itself is too big
            // or previous empty range is too big
            // add previous range as a separate chunk
            if ( (p > g.start_pos && g.align_count+c >= 2*kGraphLimitCount) ||
                 (p >= g.start_pos+kGraphEmptyLength && c && !g.align_count) ) {
                m_GraphChunks.push_back(g);
                g.start_pos = p;
                g.align_count = 0;
            }
            if ( c ) {
                g.align_count += c;
                g_empty = end;
            }
            else if ( g.align_count && end >= g_empty+kGraphEmptyLength ) {
                // too large empty region started
                m_GraphChunks.push_back(g);
                g.start_pos = g_empty;
                g.align_count = 0;
            }
            if ( g.align_count >= kGraphLimitCount ||
                 (g.align_count &&
                  (p + segment_len - g.start_pos) >= kGraphLimitLength) ) {
                // collected data is big enough -> add it as a chunk
                m_GraphChunks.push_back(g);
                g.start_pos = end;
                g.align_count = 0;
            }
        }

        // finalize annot chunks
        if ( a.align_count ) {
            _ASSERT(a.start_pos < ref_length);
            m_AlignChunks.push_back(a);
            a.start_pos = ref_length;
            a.align_count = 0;
        }
        _ASSERT(a.start_pos <= ref_length);
        _ASSERT(!a.align_count);
        m_AlignChunks.push_back(a);

        // finalize graph chunks
        if ( g.align_count ) {
            _ASSERT(g.start_pos < ref_length);
            m_GraphChunks.push_back(g);
            g.start_pos = ref_length;
            g.align_count = 0;
        }
        _ASSERT(g.start_pos <= ref_length);
        _ASSERT(!g.align_count);
        m_GraphChunks.push_back(g);

        if ( GetDebugLevel() >= 1 ) {
            size_t align_chunks = 0;
            ITERATE ( TChunks, it, m_AlignChunks ) {
                if ( it->align_count ) {
                    ++align_chunks;
                }
            }
            size_t graph_chunks = 0;
            ITERATE ( TChunks, it, m_GraphChunks ) {
                if ( it->align_count ) {
                    ++graph_chunks;
                }
            }
            LOG_POST_X(5, Info << "CSNPDataLoader:"
                       " align count: "<<total<<
                       " align chunks: "<<align_chunks<<
                       " graph chunks: "<<graph_chunks);
        }
        return;
    }
    vector<TSeqPos> pp;
    if ( kEstimateAlignCounts ) {
        TSeqPos segment_len = m_File->GetDb().GetRowSize();
        CCSraRefSeqIterator iter(*m_File, GetRefSeqId());
        TSeqPos ref_length = iter.GetSeqLength();
        Uint8 est_count = iter.GetEstimatedNumberOfAlignments();
        if ( est_count <= 2*kAlignChunkSize ) {
            pp.push_back(0);
        }
        else {
            TSeqPos chunk_len = TSeqPos((double(ref_length)*kAlignChunkSize/est_count/segment_len+.5))*segment_len;
            chunk_len = max(chunk_len, segment_len);
            for ( TSeqPos pos = 0; pos < ref_length; pos += chunk_len ) {
                pp.push_back(pos);
            }
        }
        if ( GetDebugLevel() >= 1 ) {
            LOG_POST_X(5, Info << "CSNPDataLoader: "
                       " exp count: "<<est_count<<" chunks: "<<pp.size());
        }
        pp.push_back(ref_length);
    }
    else {
        const unsigned kNumStat = 10;
        const unsigned kStatCount = 1000;
        vector<SRefStat> stat(kNumStat);
        TSeqPos ref_length =
            CCSraRefSeqIterator(*m_File, GetRefSeqId()).GetSeqLength();
        TSeqPos ref_begin = 0, ref_end = ref_length;
        double stat_len = 0, stat_cnt = 0;
        const unsigned scan_first = 1;
        if ( scan_first ) {
            stat[0].Collect(*m_File, GetRefSeqId(), 0,
                            kStatCount, m_MinMapQuality);
            if ( stat[0].m_Count != kStatCount ) {
                // single chunk
                SChunkInfo c;
                if ( stat[0].m_Count > 0 ) {
                    c.start_pos = stat[0].m_RefPosFirst;
                    c.align_count = 1;
                    m_AlignChunks.push_back(c);
                    c.start_pos = stat[0].m_RefPosLast+1;
                }
                else {
                    c.start_pos = 0;
                }
                c.align_count = 0;
                m_AlignChunks.push_back(c);
                m_GraphChunks = m_AlignChunks;
                return;
            }
            ref_begin = stat[0].m_RefPosFirst;
            stat_len = stat[0].GetStatLen();
            stat_cnt = stat[0].GetStatCount();
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
        }
        double density = stat_cnt / stat_len;
        double exp_count = (ref_end-ref_begin)*density;
        unsigned chunks = unsigned(exp_count/kAlignChunkSize+1);
        chunks = min(chunks, unsigned(sqrt(exp_count)+1));
        if ( GetDebugLevel() >= 1 ) {
            LOG_POST_X(5, Info << "CSNPDataLoader: "
                       "Total range: "<<ref_begin<<"-"<<ref_end-1<<
                       " exp count: "<<exp_count<<" chunks: "<<chunks);
        }
        pp.resize(chunks+1);
        for ( unsigned k = 1; k < chunks; ++k ) {
            TSeqPos pos = ref_begin +
                TSeqPos(double(ref_end-ref_begin)*k/chunks);
            pp[k] = pos;
        }
        pp[chunks] = ref_end;
    }
    SChunkInfo c = { 0, 1 };
    ITERATE ( vector<TSeqPos>, it, pp ) {
        c.start_pos = *it;
        m_AlignChunks.push_back(c);
    }
    m_AlignChunks.back().align_count = 0;
    m_GraphChunks = m_AlignChunks;
}


int CSNPSeqInfo::GetAnnotChunkId(TSeqPos ref_pos) const
{
    TChunks::const_iterator it =
        upper_bound(m_AlignChunks.begin(), m_AlignChunks.end(),
                    ref_pos, SChunkInfo());
    if ( it == m_AlignChunks.begin() || it == m_AlignChunks.end() ) {
        return -1;
    }
    int k = int(it-m_AlignChunks.begin()-1);
    return k*eSNPAnnotChunk_mul + eSNPAnnotChunk_align;
}


void CSNPSeqInfo::LoadAnnotMainSplit(CTSE_LoadLock& load_lock)
{
    CMutexGuard guard(m_File->GetMutex());
    CRef<CSeq_entry> entry(new CSeq_entry);
    entry->SetSet().SetId().SetId(1);

    load_lock->SetSeq_entry(*entry);
    CTSE_Split_Info& split_info = load_lock->GetSplitInfo();

    bool has_pileup = m_File->GetPileupGraphs();
    bool separate_spot_groups = !m_File->GetSeparateSpotGroups().empty();
    string align_name, pileup_name;
    if ( !separate_spot_groups ) {
        align_name = m_File->GetAlignAnnotName();
        if ( has_pileup ) {
            pileup_name = m_File->GetPileupAnnotName();
        }
    }

    CRef<CTSE_Chunk_Info> chunk(new CTSE_Chunk_Info(kMainChunkId));
    CRange<TSeqPos> whole_range = CRange<TSeqPos>::GetWhole();

    if ( separate_spot_groups ) {
        ITERATE ( vector<string>, it, m_File->GetSeparateSpotGroups() ) {
            string align_name = m_File->GetAlignAnnotName(*it);
            chunk->x_AddAnnotType(align_name,
                                  SAnnotTypeSelector(CSeq_annot::C_Data::e_Graph),
                                  GetRefSeqId(),
                                  whole_range);
            chunk->x_AddAnnotType(align_name,
                                  SAnnotTypeSelector(CSeq_annot::C_Data::e_Align),
                                  GetRefSeqId(),
                                  whole_range);
            if ( has_pileup ) {
                string align_name = m_File->GetPileupAnnotName(*it);
                chunk->x_AddAnnotType(pileup_name,
                                      SAnnotTypeSelector(CSeq_annot::C_Data::e_Graph),
                                      GetRefSeqId(),
                                      whole_range);
            }
        }
    }
    else {
        chunk->x_AddAnnotType(align_name,
                              SAnnotTypeSelector(CSeq_annot::C_Data::e_Graph),
                              GetRefSeqId(),
                              whole_range);
        chunk->x_AddAnnotType(align_name,
                              SAnnotTypeSelector(CSeq_annot::C_Data::e_Align),
                              GetRefSeqId(),
                              whole_range);
        if ( has_pileup ) {
            chunk->x_AddAnnotType(pileup_name,
                                  SAnnotTypeSelector(CSeq_annot::C_Data::e_Graph),
                                  GetRefSeqId(),
                                  whole_range);
        }
    }
    split_info.AddChunk(*chunk);
}


void CSNPSeqInfo::LoadAnnotMainChunk(CTSE_Chunk_Info& chunk_info)
{
    CMutexGuard guard(m_File->GetMutex());
    if ( GetDebugLevel() >= 5 ) {
        LOG_POST_X(13, Info<<
                   "CSNPDataLoader:LoadAnnotMain("<<
                   chunk_info.GetBlobId().ToString()<<", "<<
                   chunk_info.GetChunkId());
    }
    LoadRanges(); // also loads m_CovAnnot
    CTSE_Split_Info& split_info =
        const_cast<CTSE_Split_Info&>(chunk_info.GetSplitInfo());
    string align_name;
    bool separate_spot_groups = !m_File->GetSeparateSpotGroups().empty();
    if ( !separate_spot_groups ) {
        align_name = m_File->GetAlignAnnotName();
    }
    CTSE_Chunk_Info::TPlace place(CSeq_id_Handle(), kTSEId);

    // whole coverage graph
    if ( separate_spot_groups ) {
        // duplucate coverage graph for all spot groups
        ITERATE ( vector<string>, it, m_File->GetSeparateSpotGroups() ) {
            string align_name = m_File->GetAlignAnnotName(*it);
            CRef<CSeq_annot> annot(new CSeq_annot);
            annot->SetData().SetGraph() = m_CovAnnot->GetData().GetGraph();
            CRef<CAnnotdesc> desc(new CAnnotdesc);
            desc->SetName(align_name);
            annot->SetDesc().Set().push_back(desc);
            chunk_info.x_LoadAnnot(place, *annot);
        }
    }
    else {
        chunk_info.x_LoadAnnot(place, *m_CovAnnot);
    }
    
    CCSraRefSeqIterator iter(*m_File, GetRefSeqId());
    CRange<TSeqPos> range;
    {{
        // aligns
        const TChunks& chunks = m_AlignChunks;
        // create chunk info for alignments
        for ( size_t k = 0; k+1 < chunks.size(); ++k ) {
            if ( !chunks[k].align_count ) {
                continue;
            }
            int id = int(k)*eSNPAnnotChunk_mul + eSNPAnnotChunk_align;
            CRef<CTSE_Chunk_Info> chunk(new CTSE_Chunk_Info(id));
            range.SetFrom(chunks[k].start_pos);
            range.SetToOpen(chunks[k+1].start_pos);
            range.SetToOpen(iter.GetAlnOverToOpen(range));
            if ( separate_spot_groups ) {
                ITERATE (vector<string>, it, m_File->GetSeparateSpotGroups()) {
                    chunk->x_AddAnnotType
                        (CAnnotName(m_File->GetAlignAnnotName(*it)),
                         SAnnotTypeSelector(CSeq_annot::C_Data::e_Align),
                         GetRefSeqId(),
                         range);
                }
            }
            else {
                chunk->x_AddAnnotType
                    (align_name,
                     SAnnotTypeSelector(CSeq_annot::C_Data::e_Align),
                     GetRefSeqId(),
                     range);
            }
            chunk->x_AddAnnotPlace(kTSEId);
            split_info.AddChunk(*chunk);
        }
    }}
    if ( m_File->GetPileupGraphs() ) {
        string pileup_name = m_File->GetPileupAnnotName();
        
        // pileup
        const TChunks& chunks = m_GraphChunks;
        // create chunk info for alignments
        for ( size_t k = 0; k+1 < chunks.size(); ++k ) {
            int id = int(k)*eSNPAnnotChunk_mul + eSNPAnnotChunk_pileup_graph;
            CRef<CTSE_Chunk_Info> chunk(new CTSE_Chunk_Info(id));
            range.SetFrom(chunks[k].start_pos);
            range.SetToOpen(chunks[k+1].start_pos);
            if ( separate_spot_groups ) {
                ITERATE (vector<string>, it, m_File->GetSeparateSpotGroups()) {
                    chunk->x_AddAnnotType
                        (CAnnotName(m_File->GetPileupAnnotName(*it)),
                         SAnnotTypeSelector(CSeq_annot::C_Data::e_Graph),
                         GetRefSeqId(),
                         range);
                }
            }
            else {
                chunk->x_AddAnnotType
                    (pileup_name,
                     SAnnotTypeSelector(CSeq_annot::C_Data::e_Graph),
                     GetRefSeqId(),
                     range);
            }
            chunk->x_AddAnnotPlace(kTSEId);
            split_info.AddChunk(*chunk);
        }
    }

    chunk_info.SetLoaded();
}


void CSNPSeqInfo::LoadAnnotChunk(CTSE_Chunk_Info& chunk_info)
{
    if ( chunk_info.GetChunkId() == kMainChunkId ) {
        LoadAnnotMainChunk(chunk_info);
    }
    else {
        switch ( chunk_info.GetChunkId() % eSNPAnnotChunk_mul ) {
        case eSNPAnnotChunk_align:
            LoadAnnotAlignChunk(chunk_info);
            break;
        case eSNPAnnotChunk_pileup_graph:
            LoadAnnotPileupChunk(chunk_info);
            break;
        }
    }
}


BEGIN_LOCAL_NAMESPACE;

enum EBaseStat {
    kStat_A = 0,
    kStat_C = 1,
    kStat_G = 2,
    kStat_T = 3,
    kStat_Insert = 4,
    kStat_Match = 5,
    kNumStat = 6
};

struct SBaseStat
{
    SBaseStat(void) {
        for ( int i = 0; i < kNumStat; ++i ) {
            cnts[i] = 0;
        }
    }

    unsigned total() const {
        unsigned ret = 0;
        for ( int i = 0; i < kNumStat; ++i ) {
            ret += cnts[i];
        }
        return ret;
    }

    void add_base(char b) {
        switch ( b ) {
        case 'A': cnts[kStat_A] += 1; break;
        case 'C': cnts[kStat_C] += 1; break;
        case 'G': cnts[kStat_G] += 1; break;
        case 'T': cnts[kStat_T] += 1; break;
        case '=': cnts[kStat_Match] += 1; break;
        }
    }
    void add_match() {
        cnts[kStat_Match] += 1;
    }
    void add_insert() {
        cnts[kStat_Insert] += 1;
    }

    unsigned cnts[kNumStat];
};

struct SBaseStats
{
    TSeqPos x_size;
    vector<SBaseStat> ss;

    TSeqPos size(void) const
        {
            return x_size;
        }
    void resize(TSeqPos len)
        {
            x_size = len;
        }
    bool x_empty(EBaseStat stat) const
        {
            return false;
        }
    unsigned x_get(EBaseStat stat, TSeqPos pos) const
        {
            return ss[pos].cnts[stat];
        }
    void x_add(EBaseStat stat, TSeqPos pos)
        {
            ss.resize(size());
            ss[pos].cnts[stat] += 1;
        }

    void add_stat(EBaseStat stat, TSeqPos pos)
        {
            x_add(stat, pos);
        }
    void add_stat(EBaseStat stat, TSeqPos pos, TSeqPos count)
        {
            TSeqPos end = pos + count;
            if ( pos > end ) { // alignment starts before current graph
                pos = 0;
            }
            end = min(size(), end);
            for ( TSeqPos i = pos; i < end; ++i ) {
                x_add(stat, i);
            }
        }
    void add_base(char b, TSeqPos pos)
        {
            if ( pos < size() ) {
                EBaseStat stat;
                switch ( b ) {
                case 'A': stat = kStat_A; break;
                case 'C': stat = kStat_C; break;
                case 'G': stat = kStat_G; break;
                case 'T': stat = kStat_T; break;
                case '=': stat = kStat_Match; break;
                default: return;
                }
                add_stat(stat, pos);
            }
        }
    void add_match(TSeqPos pos, TSeqPos count)
        {
            add_stat(kStat_Match, pos, count);
        }
    void add_insert(TSeqPos pos, TSeqPos count)
        {
            add_stat(kStat_Insert, pos, count);
        }

    pair<unsigned, unsigned> get_min_max(EBaseStat stat) const
        {
            pair<unsigned, unsigned> c_min_max;
            if ( !ss.empty() && !x_empty(stat) ) {
                c_min_max.first = c_min_max.second = x_get(stat, 0);
                for ( TSeqPos i = 1; i < size(); ++i ) {
                    unsigned c = x_get(stat, i);
                    c_min_max.first = min(c_min_max.first, c);
                    c_min_max.second = max(c_min_max.second, c);
                }
            }
            return c_min_max;
        }
    
    void get_bytes(EBaseStat stat, CByte_graph::TValues& vv)
        {
            vv.reserve(size());
            for ( TSeqPos i = 0; i < size(); ++i ) {
                vv.push_back(CByte_graph::TValues::value_type(x_get(stat, i)));
            }
        }
    void get_ints(EBaseStat stat, CInt_graph::TValues& vv)
        {
            vv.reserve(size());
            for ( TSeqPos i = 0; i < size(); ++i ) {
                vv.push_back(CInt_graph::TValues::value_type(x_get(stat, i)));
            }
        }
};


struct SChunkAnnots {
    SChunkAnnots(CSNPFileInfo* file_info,
                 ESNPAnnotChunkIdType type);

    CRef<CSNPFileInfo> m_File;
    bool m_SeparateSpotGroups;
    ESNPAnnotChunkIdType m_Type;
    typedef pair<CRef<CSeq_annot>, SBaseStats> TSlot;
    typedef map<string, TSlot> TAnnots;
    TAnnots m_Annots;
    TAnnots::iterator m_LastAnnot;

    TSlot& Select(const CCSraAlignIterator& ait);
    void Create(const string& name);
};


SChunkAnnots::SChunkAnnots(CSNPFileInfo* file,
                           ESNPAnnotChunkIdType type)
    : m_File(file),
      m_SeparateSpotGroups(!m_File->GetSeparateSpotGroups().empty()),
      m_Type(type),
      m_LastAnnot(m_Annots.end())
{
    if ( !m_SeparateSpotGroups ) {
        Create(kEmptyStr);
    }
    else if ( m_Type == eSNPAnnotChunk_pileup_graph ) {
        ITERATE ( vector<string>, it, m_File->GetSeparateSpotGroups() ) {
            Create(*it);
        }
    }
}


SChunkAnnots::TSlot& SChunkAnnots::Select(const CCSraAlignIterator& ait)
{
    if ( m_SeparateSpotGroups ) {
        CTempString spot_group_name = ait.GetSpotGroup();
        if ( m_LastAnnot == m_Annots.end() ||
             spot_group_name != m_LastAnnot->first ) {
            Create(spot_group_name);
        }
    }
    return m_LastAnnot->second;
}


void SChunkAnnots::Create(const string& name)
{
    m_LastAnnot = m_Annots.insert(TAnnots::value_type(name, TSlot(null, SBaseStats()))).first;
    if ( !m_LastAnnot->second.first ) {
        const string& annot_name = m_File->GetAnnotName(name, m_Type);
        if ( m_Type == eSNPAnnotChunk_align ) {
            m_LastAnnot->second.first =
                CCSraAlignIterator::MakeEmptyMatchAnnot(annot_name);
        }
        else {
            m_LastAnnot->second.first =
                CCSraAlignIterator::MakeSeq_annot(annot_name);
        }
    }
}

END_LOCAL_NAMESPACE;


void CSNPSeqInfo::LoadAnnotAlignChunk(CTSE_Chunk_Info& chunk_info)
{
    if ( GetDebugLevel() >= 5 ) {
        LOG_POST_X(15, Info<<
                   "CSNPDataLoader:LoadAlignChunk("<<
                   chunk_info.GetBlobId().ToString()<<", "<<
                   chunk_info.GetChunkId());
    }
    CMutexGuard guard(m_File->GetMutex());
    int range_id = chunk_info.GetChunkId() / eSNPAnnotChunk_mul;
    TSeqPos pos = m_AlignChunks[range_id].start_pos;
    TSeqPos end = m_AlignChunks[range_id+1].start_pos;
    CTSE_Chunk_Info::TPlace place(CSeq_id_Handle(), kTSEId);
    int min_quality = m_MinMapQuality;
    _TRACE("Loading aligns "<<GetRefSeqId()<<" @ "<<pos<<"-"<<(end-1));
    size_t skipped = 0, count = 0;
    SChunkAnnots annots(m_File, eSNPAnnotChunk_align);
   
    CCSraAlignIterator ait(*m_File, GetRefSeqId(), pos, end-pos,
                           CCSraAlignIterator::eSearchByStart);
    for( ; ait; ++ait ){
        _ASSERT(ait.GetRefSeqPos() >= pos && ait.GetRefSeqPos() < end);
        if ( min_quality > 0 && ait.GetMapQuality() < min_quality ) {
            ++skipped;
            continue;
        }
        ++count;

        annots.Select(ait).first->SetData().SetAlign().push_back(ait.GetMatchAlign());
    }
    if ( !annots.m_Annots.empty() ) {
        if ( GetDebugLevel() >= 9 ) {
            LOG_POST_X(8, Info <<
                       "CSNPDataLoader(" << m_File->GetFileName() << "): "
                       "Chunk "<<chunk_info.GetChunkId());
        }
        ITERATE ( SChunkAnnots::TAnnots, it, annots.m_Annots ) {
            chunk_info.x_LoadAnnot(place, *it->second.first);
        }
    }
    if ( GetDebugLevel() >= 2 ) {
        LOG_POST_X(7, Info<<"CSNPDataLoader: "
                   "Loaded "<<GetRefSeqId()<<" @ "<<
                   pos<<"-"<<(end-1)<<": "<<
                   count<<" skipped: "<<skipped);
    }
    chunk_info.SetLoaded();
}


void CSNPSeqInfo::LoadAnnotPileupChunk(CTSE_Chunk_Info& chunk_info)
{
    if ( GetDebugLevel() >= 5 ) {
        LOG_POST_X(16, Info<<
                   "CSNPDataLoader:LoadPileupChunk("<<
                   chunk_info.GetBlobId().ToString()<<", "<<
                   chunk_info.GetChunkId());
    }
    CMutexGuard guard(m_File->GetMutex());
    int chunk_id = chunk_info.GetChunkId();
    int range_id = chunk_id / eSNPAnnotChunk_mul;
    TSeqPos pos = m_GraphChunks[range_id].start_pos;
    TSeqPos end = m_GraphChunks[range_id+1].start_pos;
    TSeqPos len = end - pos;
    CTSE_Chunk_Info::TPlace place(CSeq_id_Handle(), kTSEId);
    int min_quality = m_MinMapQuality;
    _TRACE("Loading pileup "<<GetRefSeqId()<<" @ "<<pos<<"-"<<(end-1));
    size_t count = 0, skipped = 0;

    SChunkAnnots annots(m_File, eSNPAnnotChunk_pileup_graph);

    CCSraAlignIterator ait(*m_File, GetRefSeqId(), pos, len);
    for ( ; ait; ++ait ) {
        if ( min_quality > 0 && ait.GetMapQuality() < min_quality ) {
            ++skipped;
            continue;
        }
        ++count;
        SBaseStats& ss = annots.Select(ait).second;
        ss.resize(len);

        TSeqPos ref_pos = ait.GetRefSeqPos()-pos;
        TSeqPos read_pos = ait.GetShortPos();
        CTempString read = ait.GetMismatchRead();
        CTempString cigar = ait.GetCIGARLong();
        const char* ptr = cigar.data();
        const char* end = cigar.end();
        while ( ptr != end ) {
            char type = 0;
            int seglen = 0;
            for ( ; ptr != end; ) {
                char c = *ptr++;
                if ( c >= '0' && c <= '9' ) {
                    seglen = seglen*10+(c-'0');
                }
                else {
                    type = c;
                    break;
                }
            }
            if ( seglen == 0 ) {
                NCBI_THROW_FMT(CSraException, eOtherError,
                               "Bad CIGAR length: " << type <<
                               "0 in " << cigar);
            }
            if ( type == '=' ) {
                // match
                ss.add_match(ref_pos, seglen);
                ref_pos += seglen;
                read_pos += seglen;
            }
            else if ( type == 'M' || type == 'X' ) {
                // mismatch
                for ( int i = 0; i < seglen; ++i ) {
                    if ( ref_pos < len ) {
                        ss.add_base(read[read_pos], ref_pos);
                    }
                    ++ref_pos;
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
                ref_pos += seglen;
            }
            else if ( type == 'D' ) {
                // delete
                ss.add_insert(ref_pos, seglen);
                ref_pos += seglen;
            }
            else if ( type != 'P' ) {
                NCBI_THROW_FMT(CSraException, eOtherError,
                               "Bad CIGAR char: " <<type<< " in " <<cigar);
            }
        }
    }

    if ( GetDebugLevel() >= 2 ) {
        LOG_POST_X(10, Info<<"CSNPDataLoader: "
                   "Loaded pileup "<<GetRefSeqId()<<" @ "<<
                   pos<<"-"<<(end-1)<<": "<<
                   count<<" skipped: "<<skipped);
    }

    size_t total_bytes = 0;
    NON_CONST_ITERATE ( SChunkAnnots::TAnnots, it, annots.m_Annots ) {
        SBaseStats& ss = it->second.second;
        if ( kOmitEmptyPileup && !ss.size() ) {
            continue;
        }
        for ( int k = 0; k < kNumStat; ++k ) {
            CRef<CSeq_graph> graph(new CSeq_graph);
            static const char* const titles[6] = {
                "Number of A bases",
                "Number of C bases",
                "Number of G bases",
                "Number of T bases",
                "Number of inserts",
                "Number of matches"
            };
            graph->SetTitle(titles[k]);
            CSeq_interval& loc = graph->SetLoc().SetInt();
            loc.SetId(*ait.GetRefSeq_id());
            loc.SetFrom(pos);
            loc.SetTo(pos+len-1);
            graph->SetNumval(len);

            pair<unsigned, unsigned> c_min_max = ss.get_min_max(EBaseStat(k));
            if ( c_min_max.second == 0 ) {
                CByte_graph& data = graph->SetGraph().SetByte();
                data.SetValues().resize(len);
                data.SetMin(0);
                data.SetMax(0);
                data.SetAxis(0);
                total_bytes += data.SetValues().size();
            }
            else if ( c_min_max.second < 256 ) {
                CByte_graph& data = graph->SetGraph().SetByte();
                CByte_graph::TValues& vv = data.SetValues();
                ss.get_bytes(EBaseStat(k), vv);
                data.SetMin(c_min_max.first);
                data.SetMax(c_min_max.second);
                data.SetAxis(0);
                total_bytes += vv.size();
            }
            else {
                CInt_graph& data = graph->SetGraph().SetInt();
                CInt_graph::TValues& vv = data.SetValues();
                ss.get_ints(EBaseStat(k), vv);
                data.SetMin(c_min_max.first);
                data.SetMax(c_min_max.second);
                data.SetAxis(0);
                total_bytes += vv.size()*sizeof(vv[0]);
            }
            it->second.first->SetData().SetGraph().push_back(graph);
        }
        if ( GetDebugLevel() >= 9 ) {
            LOG_POST_X(11, Info<<"CSNPDataLoader: "
                       "Loaded pileup "<<GetRefSeqId()<<" @ "<<
                       pos<<"-"<<(end-1)<<": "<<
                       MSerial_AsnText<<*it->second.first);
        }
        chunk_info.x_LoadAnnot(place, *it->second.first);
    }
    if ( GetDebugLevel() >= 2 ) {
        LOG_POST_X(11, Info<<"CSNPDataLoader: "
                   "Loaded pileup "<<GetRefSeqId()<<" @ "<<
                   pos<<"-"<<(end-1)<<": "<<total_bytes<<" bytes");
    }
    chunk_info.SetLoaded();
}
#endif

END_SCOPE(objects)
END_NCBI_SCOPE
