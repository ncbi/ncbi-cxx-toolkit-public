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
static const int kChunkIdFeat = 0;
static const int kChunkIdGraph = 1;
static const int kChunkIdMul = 2;

// algirithm options

// splitter parameters for SNPs and graphs
static const TSeqPos kFeatChunkSize = 1000000;
static const TSeqPos kGraphChunkSize = 1000000;

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


NCBI_PARAM_DECL(string, SNP_LOADER, ANNOT_NAME);
NCBI_PARAM_DEF_EX(string, SNP_LOADER, ANNOT_NAME, "SNP",
                  eParam_NoThread, SNP_LOADER_ANNOT_NAME);

static string GetDefaultAnnotName(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(SNP_LOADER, ANNOT_NAME)> s_Value;
    return s_Value->Get();
}


NCBI_PARAM_DECL(bool, SNP_LOADER, SPLIT);
NCBI_PARAM_DEF_EX(bool, SNP_LOADER, SPLIT, true,
                  eParam_NoThread, SNP_LOADER_SPLIT);

static bool IsSplitEnabled(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(SNP_LOADER, SPLIT)> s_Value;
    return s_Value->Get();
}


/////////////////////////////////////////////////////////////////////////////
// CSNPBlobId
/////////////////////////////////////////////////////////////////////////////

// Blob id
// sat = 2001-2099 : SNP NA version 1 - 99
// subsat : NA accession number
// satkey : SequenceIndex + 1000000*FilterIndex;
// satkey bits 24-30: 

const int kSNPSatBase = 2000;
const int kNAVersionMin = 1;
const int kNAVersionMax = 99;
const int kSeqIndexCount = 1000000;
const int kFilterIndexCount = 2000;

CSNPBlobId::CSNPBlobId(const CTempString& str)
{
    FromString(str);
}


CSNPBlobId::CSNPBlobId(const CSNPFileInfo& file,
                       const CSeq_id_Handle& seq_id)
    : m_Sat(0),
      m_SubSat(0),
      m_SatKey(0),
      m_File(file.GetFileName()),
      m_SeqId(seq_id)
{
}


CSNPBlobId::CSNPBlobId(const CSNPFileInfo& file,
                       size_t seq_index,
                       size_t filter_index)
{
    SetSatNA(file.GetFileName());
    SetSeqAndFilterIndex(seq_index, filter_index);
}


CSNPBlobId::CSNPBlobId(const CSNPDbSeqIterator& seq,
                       size_t filter_index)
{
    SetSatNA(seq.GetDb().GetDbPath());
    SetSeqAndFilterIndex(seq.GetVDBSeqIndex(), filter_index);
}


CSNPBlobId::~CSNPBlobId(void)
{
}


bool CSNPBlobId::IsValidNAIndex(size_t na_index)
{
    return na_index > 0 && na_index < 1000000000;
}


bool CSNPBlobId::IsValidNAVersion(size_t na_version)
{
    return na_version >= kNAVersionMin && na_version <= kNAVersionMax;
}


bool CSNPBlobId::IsValidSeqIndex(size_t seq_index)
{
    return seq_index < kSeqIndexCount;
}


bool CSNPBlobId::IsValidFilterIndex(size_t filter_index)
{
    return filter_index < kFilterIndexCount;
}


size_t CSNPBlobId::GetNAIndex(void) const
{
    _ASSERT(IsSatId());
    return GetSubSat();
}


void CSNPBlobId::SetNAIndex(size_t na_index)
{
    _ASSERT(IsValidNAIndex(na_index));
    m_SubSat = na_index;
}


bool CSNPBlobId::IsValidSubSat(void) const
{
    return IsValidNAIndex(GetNAIndex());
}


size_t CSNPBlobId::GetNAVersion(void) const
{
    _ASSERT(IsSatId());
    return GetSat() - kSNPSatBase;
}


void CSNPBlobId::SetNAVersion(size_t na_version)
{
    _ASSERT(IsValidNAVersion(na_version));
    m_Sat = kSNPSatBase + na_version;
}


bool CSNPBlobId::IsValidSat(void) const
{
    return IsValidNAVersion(GetNAVersion());
}


pair<size_t, size_t> CSNPBlobId::ParseNA(CTempString acc)
{
    pair<size_t, size_t> ret(0, 0);
    // NA123456789.1
    if ( acc.size() < 13 || acc.size() > 15 ||
         acc[0] != 'N' || acc[1] != 'A' || acc[11] != '.' ) {
        return ret;
    }
    size_t na_index = NStr::StringToNumeric<size_t>(acc.substr(2, 9),
                                                    NStr::fConvErr_NoThrow);
    if ( !IsValidNAIndex(na_index) ) {
        return ret;
    }
    size_t na_version = NStr::StringToNumeric<size_t>(acc.substr(12),
                                                      NStr::fConvErr_NoThrow);
    if ( !IsValidNAVersion(na_version) ) {
        return ret;
    }
    ret.first = na_index;
    ret.second = na_version;
    return ret;
}


string CSNPBlobId::GetSatNA(void) const
{
    CNcbiOstrstream str;
    str << "NA" << setw(9) << setfill('0') << GetNAIndex()
        << '.' << GetNAVersion();
    return CNcbiOstrstreamToString(str);
}


void CSNPBlobId::SetSatNA(CTempString acc)
{
    pair<size_t, size_t> na = ParseNA(acc);
    SetNAIndex(na.first);
    SetNAVersion(na.second);
}


size_t CSNPBlobId::GetSeqIndex(void) const
{
    _ASSERT(IsSatId());
    return GetSatKey() % kSeqIndexCount;
}


size_t CSNPBlobId::GetFilterIndex(void) const
{
    _ASSERT(IsSatId());
    return GetSatKey() / kSeqIndexCount;
}


void CSNPBlobId::SetSeqAndFilterIndex(size_t seq_index,
                                      size_t filter_index)
{
    _ASSERT(IsValidSeqIndex(seq_index));
    _ASSERT(IsValidFilterIndex(filter_index));
    m_SatKey = seq_index + filter_index * kSeqIndexCount;
}


bool CSNPBlobId::IsValidSatKey(void) const
{
    return IsValidSeqIndex(GetSeqIndex()) &&
        IsValidFilterIndex(GetFilterIndex());
}


CSeq_id_Handle CSNPBlobId::GetSeqId(void) const
{
    _ASSERT(!IsSatId());
    return m_SeqId;
}


string CSNPBlobId::GetFileName(void) const
{
    if ( IsSatId() ) {
        return GetSatNA();
    }
    else {
        return m_File;
    }
}


static const char kFileEnd[] = "|||";

string CSNPBlobId::ToString(void) const
{
    CNcbiOstrstream out;
    if ( IsSatId() ) {
        out << m_Sat << '.' << m_SubSat << '.' << m_SatKey;
    }
    else {
        out << m_File;
        out << kFileEnd;
        out << m_SeqId;
    }
    return CNcbiOstrstreamToString(out);
}


bool CSNPBlobId::FromSatString(CTempString str)
{
    if ( str.empty() || !isdigit(Uchar(str[0])) ) {
        return false;
    }

    size_t dot1 = str.find('.');
    if ( dot1 == NPOS ) {
        return false;
    }
    size_t dot2 = str.find('.', dot1+1);
    if ( dot2 == NPOS ) {
        return false;
    }
    m_Sat = NStr::StringToNumeric<size_t>(str.substr(0, dot1),
                                          NStr::fConvErr_NoThrow);
    if ( !IsValidSat() ) {
        return false;
    }
    m_SubSat = NStr::StringToNumeric<size_t>(str.substr(dot1+1, dot2-dot1-1),
                                             NStr::fConvErr_NoThrow);

    if ( !IsValidSubSat() ) {
        return false;
    }
    
    m_SatKey = NStr::StringToNumeric<size_t>(str.substr(dot2+1),
                                             NStr::fConvErr_NoThrow);
    if ( !IsValidSatKey() ) {
        return false;
    }
    
    _ASSERT(IsSatId());
    return true;
}


void CSNPBlobId::FromString(CTempString str)
{
    if ( FromSatString(str) ) {
        return;
    }
    m_Sat = 0;
    m_SubSat = 0;
    m_SatKey = 0;
    _ASSERT(!IsSatId());

    SIZE_TYPE div = str.rfind(kFileEnd);
    if ( div == NPOS ) {
        NCBI_THROW_FMT(CSraException, eOtherError,
                       "Bad CSNPBlobId: "<<str);
    }
    m_File = str.substr(0, div);
    m_SeqId = CSeq_id_Handle::GetHandle(str.substr(div+strlen(kFileEnd)));
}


bool CSNPBlobId::operator<(const CBlobId& id) const
{
    const CSNPBlobId& id2 = dynamic_cast<const CSNPBlobId&>(id);
    if ( m_Sat != id2.m_Sat ) {
        return m_Sat < id2.m_Sat;
    }
    if ( m_SubSat != id2.m_SubSat ) {
        return m_SubSat < id2.m_SubSat;
    }
    if ( m_SatKey != id2.m_SatKey ) {
        return m_SatKey < id2.m_SatKey;
    }
    if ( m_File != id2.m_File ) {
        return m_File < id2.m_File;
    }
    return m_SeqId < id2.m_SeqId;
}


bool CSNPBlobId::operator==(const CBlobId& id) const
{
    const CSNPBlobId& id2 = dynamic_cast<const CSNPBlobId&>(id);
    return m_Sat == id2.m_Sat &&
        m_SubSat == id2.m_SubSat &&
        m_SatKey == id2.m_SatKey &&
        m_SeqId == id2.m_SeqId &&
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
        return GetFixedFile(blob_id.GetFileName());
    }
    else {
        return FindFile(blob_id.GetFileName());
    }
}


CTSE_LoadLock CSNPDataLoader_Impl::GetBlobById(CDataSource* data_source,
                                                const CSNPBlobId& blob_id)
{
    CDataLoader::TBlobId loader_blob_id(&blob_id);
    CTSE_LoadLock load_lock = data_source->GetTSE_LoadLock(loader_blob_id);
    if ( !load_lock.IsLoaded() ) {
        CStopWatch sw(CStopWatch::eStart);
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
    LOG_POST("CSNPDataLoader::LoadBlob("<<blob_id.ToString()<<")");
    CRef<CSNPFileInfo> file_info = GetFileInfo(blob_id);
    file_info->GetSeqInfo(blob_id)->LoadAnnotBlob(load_lock);
}


void CSNPDataLoader_Impl::LoadChunk(const CSNPBlobId& blob_id,
                                    CTSE_Chunk_Info& chunk_info)
{
    LOG_POST("CSNPDataLoader::LoadChunk("<<blob_id.ToString()<<"."<<chunk_info.GetChunkId()<<")");
    CRef<CSNPFileInfo> file_info = GetFileInfo(blob_id);
    file_info->GetSeqInfo(blob_id)->LoadAnnotChunk(chunk_info);
}


CSNPDataLoader::TAnnotNames
CSNPDataLoader_Impl::GetPossibleAnnotNames(void) const
{
    TAnnotNames names;
    names.push_back(m_AnnotName);
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
    /*
    for ( CSNPDbSeqIterator rit(m_SNPDb); rit; ++rit ) {
        AddSeq(rit.GetSeqIdHandle());
    }
    */
}


void CSNPFileInfo::x_Initialize(CSNPDataLoader_Impl& impl,
                                const string& csra)
{
    m_IsValidNA = CSNPBlobId::IsValidNA(csra);
    m_FileName = csra;
    m_AnnotName = impl.m_AnnotName;
    if ( m_AnnotName.empty() ) {
        m_AnnotName = GetDefaultAnnotName();
    }
    m_SNPDb = CSNPDb(impl.m_Mgr, CDirEntry::MakePath(impl.m_DirPath, csra));
    if ( GetDebugLevel() >= 1 ) {
        LOG_POST_X(8, Info <<
                   "CSNPDataLoader("<<csra<<")="<<csra);
    }
}


string CSNPFileInfo::GetSNPAnnotName(void) const
{
    return m_AnnotName;
}


void CSNPFileInfo::GetPossibleAnnotNames(TAnnotNames& names) const
{
    names.push_back(CAnnotName(GetSNPAnnotName()));
}


/*
void CSNPFileInfo::AddSeq(const CSeq_id_Handle& id)
{
    if ( GetDebugLevel() >= 1 ) {
        LOG_POST_X(9, Info << "CSNPDataLoader(" << m_FileName << "): "
                   "Found "<<id);
    }
    m_Seqs[id] = new CSNPSeqInfo(this, id);
}
*/


CRef<CSNPSeqInfo>
CSNPFileInfo::GetSeqInfo(const CSeq_id_Handle& seq_id)
{
    CMutexGuard guard(GetMutex());
    TSeqById::const_iterator it = m_SeqById.find(seq_id);
    if ( it != m_SeqById.end() ) {
        return it->second;
    }
    CRef<CSNPSeqInfo> ret;
    CSNPDbSeqIterator seq_it(m_SNPDb, seq_id);
    if ( seq_it ) {
        size_t seq_index = seq_it.GetVDBSeqIndex();
        auto& idx_slot = m_SeqByIdx[seq_index];
        if ( idx_slot ) {
            ret = idx_slot;
        }
        else {
            // new sequence
            ret = new CSNPSeqInfo(this, seq_it);
            idx_slot = ret;
        }
    }
    return ret;
}


CRef<CSNPSeqInfo>
CSNPFileInfo::GetSeqInfo(size_t seq_index)
{
    CMutexGuard guard(GetMutex());
    TSeqByIdx::const_iterator it = m_SeqByIdx.find(seq_index);
    if ( it != m_SeqByIdx.end() ) {
        return it->second;
    }
    CSNPDbSeqIterator seq_it(m_SNPDb, seq_index);
    _ASSERT(seq_it);
    CRef<CSNPSeqInfo> ret(new CSNPSeqInfo(this, seq_it));
    m_SeqByIdx[seq_it.GetVDBSeqIndex()] = ret;
    return ret;
}


CRef<CSNPSeqInfo>
CSNPFileInfo::GetSeqInfo(const CSNPBlobId& blob_id)
{
    if ( blob_id.IsSatId() ) {
        return GetSeqInfo(blob_id.GetSeqIndex());
    }
    else {
        return GetSeqInfo(blob_id.GetSeqId());
    }
}


/////////////////////////////////////////////////////////////////////////////
// CSNPSeqInfo
/////////////////////////////////////////////////////////////////////////////


CSNPSeqInfo::CSNPSeqInfo(CSNPFileInfo* file,
                         const CSNPDbSeqIterator& it)
    : m_File(file),
      m_SeqIndex(it.GetVDBSeqIndex()),
      m_FilterIndex(0)
{
    if ( !m_File->IsValidNA() ||
         !CSNPBlobId::IsValidSeqIndex(m_SeqIndex) ||
         !CSNPBlobId::IsValidFilterIndex(m_FilterIndex) ) {
        m_SeqIndex = 0;
        m_FilterIndex = 0;
        m_SeqId = it.GetSeqIdHandle();
        _ASSERT(m_SeqId);
    }
}


CRef<CSNPBlobId> CSNPSeqInfo::GetBlobId(void) const
{
    _ASSERT(m_File);
    if ( !m_SeqId ) {
        return Ref(new CSNPBlobId(*m_File, m_SeqIndex, m_FilterIndex));
    }
    return Ref(new CSNPBlobId(*m_File, m_SeqId));
}


void CSNPSeqInfo::SetBlobId(CRef<CSNPBlobId>& ret,
                            const CSeq_id_Handle& idh) const
{
    CRef<CSNPBlobId> id = GetBlobId();
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
    if ( !m_SeqId ) {
        return CSNPDbSeqIterator(*m_File, m_SeqIndex);
    }
    else {
        return CSNPDbSeqIterator(*m_File, m_SeqId);
    }
}


void CSNPSeqInfo::LoadAnnotBlob(CTSE_LoadLock& load_lock)
{
    CSNPDbSeqIterator it = GetSeqIterator();
    CRange<TSeqPos> total_range = it.GetSNPRange();
    if ( IsSplitEnabled() ) {
        // split
        CRef<CSeq_entry> entry(new CSeq_entry);
        entry->SetSet().SetId().SetId(kTSEId);
        entry->SetSet().SetSeq_set();
        load_lock->SetSeq_entry(*entry);
        CTSE_Split_Info& split_info = load_lock->GetSplitInfo();
        CTSE_Chunk_Info::TPlace place(CSeq_id_Handle(), kTSEId);
        string name = m_File->GetSNPAnnotName();
        SAnnotTypeSelector type(CSeq_annot::C_Data::e_Graph);
        for ( int i = 0; i*kGraphChunkSize < total_range.GetToOpen(); ++i ) {
            CRange<TSeqPos> range;
            range.SetFrom(i*kGraphChunkSize);
            range.SetToOpen((i+1)*kGraphChunkSize);
            int chunk_id = i*kChunkIdMul+kChunkIdGraph;
            CRef<CTSE_Chunk_Info> chunk(new CTSE_Chunk_Info(chunk_id));
            chunk->x_AddAnnotType(name, type, it.GetSeqIdHandle(), range);
            chunk->x_AddAnnotPlace(place);
            split_info.AddChunk(*chunk);
        }
        type = CSeqFeatData::eSubtype_variation;
        for ( int i = 0; i*kFeatChunkSize < total_range.GetToOpen(); ++i ) {
            CRange<TSeqPos> range;
            range.SetFrom(i*kFeatChunkSize);
            range.SetTo((i+1)*kFeatChunkSize+it.GetMaxSNPLength());
            int chunk_id = i*kChunkIdMul+kChunkIdFeat;
            CRef<CTSE_Chunk_Info> chunk(new CTSE_Chunk_Info(chunk_id));
            chunk->x_AddAnnotType(name, type, it.GetSeqIdHandle(), range);
            chunk->x_AddAnnotPlace(place);
            split_info.AddChunk(*chunk);
        }
    }
    else {
        string name = m_File->GetSNPAnnotName();
        CRef<CSeq_entry> entry(new CSeq_entry);
        for ( auto& annot : it.GetTableFeatAnnots(total_range, name) ) {
            entry->SetSet().SetAnnot().push_back(annot);
        }
        load_lock->SetSeq_entry(*entry);
    }
}


void CSNPSeqInfo::LoadAnnotChunk(CTSE_Chunk_Info& chunk_info)
{
    int chunk_id = chunk_info.GetChunkId();
    int chunk_type = chunk_id%kChunkIdMul;
    int i = chunk_id/kChunkIdMul;
    CTSE_Chunk_Info::TPlace place(CSeq_id_Handle(), kTSEId);
    if ( chunk_type == kChunkIdFeat ) {
        CRange<TSeqPos> range;
        range.SetFrom(i*kFeatChunkSize);
        range.SetToOpen((i+1)*kFeatChunkSize);
        string name = m_File->GetSNPAnnotName();
        CSNPDbSeqIterator it = GetSeqIterator();
        for ( auto& annot : it.GetTableFeatAnnots(range, name) ) {
            chunk_info.x_LoadAnnot(place, *annot);
        }
    }
    else if ( chunk_type == kChunkIdGraph ) {
        CRange<TSeqPos> range;
        range.SetFrom(i*kGraphChunkSize);
        range.SetToOpen((i+1)*kGraphChunkSize);
        string name = m_File->GetSNPAnnotName();
        CSNPDbSeqIterator it = GetSeqIterator();
        if ( auto annot = it.GetCoverageAnnot(range, name) ) {
            chunk_info.x_LoadAnnot(place, *annot);
        }
    }
    chunk_info.SetLoaded();
}


END_SCOPE(objects)
END_NCBI_SCOPE
