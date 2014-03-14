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
#include <sra/data_loaders/csra/csraloader.hpp>
#include <sra/data_loaders/csra/impl/csraloader_impl.hpp>

#include <util/sequtil/sequtil_manip.hpp>

#include <algorithm>
#include <cmath>

BEGIN_NCBI_SCOPE

#define NCBI_USE_ERRCODE_X   cSRALoader
NCBI_DEFINE_ERR_SUBCODE_X(16);

BEGIN_SCOPE(objects)

class CDataLoader;

static const int kTSEId = 1;
static const int kMainChunkId = -1;

// refseq data chunks
static const size_t kChunkSize = 500;
static const size_t kChunkSeqDataMul = 8;

// reads
static const size_t kReadsPerBlob = 1;

static const Uint4 kMaxReadId = 8;

#define SPOT_GROUP_SEPARATOR ": "
#define PILEUP_NAME_SUFFIX "pileup graphs"

NCBI_PARAM_DECL(int, CSRA_LOADER, DEBUG);
NCBI_PARAM_DEF_EX(int, CSRA_LOADER, DEBUG, 0,
                  eParam_NoThread, CSRA_LOADER_DEBUG);

static int GetDebugLevel(void)
{
    static NCBI_PARAM_TYPE(CSRA_LOADER, DEBUG) s_Value;
    return s_Value.Get();
}


NCBI_PARAM_DECL(bool, CSRA_LOADER, PILEUP_GRAPHS);
NCBI_PARAM_DEF_EX(bool, CSRA_LOADER, PILEUP_GRAPHS, 1,
                  eParam_NoThread, CSRA_LOADER_PILEUP_GRAPHS);

bool CCSRADataLoader::GetPileupGraphsParamDefault(void)
{
    return NCBI_PARAM_TYPE(CSRA_LOADER, PILEUP_GRAPHS)::GetDefault();
}


void CCSRADataLoader::SetPileupGraphsParamDefault(bool param)
{
    NCBI_PARAM_TYPE(CSRA_LOADER, PILEUP_GRAPHS)::SetDefault(param);
}


static bool GetPileupGraphsParam(void)
{
    NCBI_PARAM_TYPE(CSRA_LOADER, PILEUP_GRAPHS) s_Value;
    return s_Value.Get();
}


NCBI_PARAM_DECL(bool, CSRA_LOADER, QUALITY_GRAPHS);
NCBI_PARAM_DEF_EX(bool, CSRA_LOADER, QUALITY_GRAPHS, 0,
                  eParam_NoThread, CSRA_LOADER_QUALITY_GRAPHS);

static bool GetQualityGraphsParam(void)
{
    static NCBI_PARAM_TYPE(CSRA_LOADER, QUALITY_GRAPHS) s_Value;
    return s_Value.Get();
}


NCBI_PARAM_DECL(int, CSRA_LOADER, MIN_MAP_QUALITY);
NCBI_PARAM_DEF_EX(int, CSRA_LOADER, MIN_MAP_QUALITY, 0,
                  eParam_NoThread, CSRA_LOADER_MIN_MAP_QUALITY);

static int GetDefaultMinMapQuality(void)
{
    static NCBI_PARAM_TYPE(CSRA_LOADER, MIN_MAP_QUALITY) s_Value;
    return s_Value.Get();
}


NCBI_PARAM_DECL(int, CSRA_LOADER, MAX_SEPARATE_SPOT_GROUPS);
NCBI_PARAM_DEF_EX(int, CSRA_LOADER, MAX_SEPARATE_SPOT_GROUPS, 0,
                  eParam_NoThread, CSRA_LOADER_MAX_SEPARATE_SPOT_GROUPS);

static int GetMaxSeparateSpotGroups(void)
{
    static NCBI_PARAM_TYPE(CSRA_LOADER, MAX_SEPARATE_SPOT_GROUPS) s_Value;
    return s_Value.Get();
}


NCBI_PARAM_DECL(size_t, CSRA_LOADER, GC_SIZE);
NCBI_PARAM_DEF_EX(size_t, CSRA_LOADER, GC_SIZE, 10,
                  eParam_NoThread, CSRA_LOADER_GC_SIZE);

static size_t GetGCSize(void)
{
    static NCBI_PARAM_TYPE(CSRA_LOADER, GC_SIZE) s_Value;
    return s_Value.Get();
}


/////////////////////////////////////////////////////////////////////////////
// CCSRABlobId
/////////////////////////////////////////////////////////////////////////////

CCSRABlobId::CCSRABlobId(const CTempString& str)
{
    FromString(str);
}


CCSRABlobId::CCSRABlobId(EBlobType blob_type,
                         const CCSRAFileInfo& file,
                         const CSeq_id_Handle& seq_id)
    : m_BlobType(blob_type),
      m_RefIdType(file.GetRefIdType()),
      m_File(file.GetCSRAName()),
      m_SeqId(seq_id),
      m_FirstSpotId(0)
{
    _ASSERT(blob_type != eBlobType_reads);
}


CCSRABlobId::CCSRABlobId(const CCSRAFileInfo& file,
                         Uint8 first_spot_id)
    : m_BlobType(eBlobType_reads),
      m_RefIdType(file.GetRefIdType()),
      m_File(file.GetCSRAName()),
      m_FirstSpotId(first_spot_id)
{
}


CCSRABlobId::~CCSRABlobId(void)
{
}


SIZE_TYPE CCSRABlobId::ParseReadId(CTempString str,
                                   Uint8* spot_id_ptr,
                                   Uint4* read_id_ptr)
{
    const char* begin = str.data();
    const char* ptr = begin+str.size();
    const char* end = ptr;
    bool parsing_read_id = true;
    for ( ; ptr != begin; ) {
        char c = *--ptr;
        if ( isdigit(c&0xff) ) {
            if ( parsing_read_id && ptr < end-1 ) {
                // too long read_id
                return NPOS;
            }
        }
        else if ( c == '.' ) {
            // end of number
            if ( ptr == end ) {
                // no id part
                return NPOS;
            }
            if ( ptr[1] == '0' ) {
                // leading zero
                return NPOS;
            }
            if ( parsing_read_id ) {
                Uint4 read_id = ptr[1] - '0';
                if ( read_id > kMaxReadId ) {
                    return NPOS;
                }
                if ( read_id_ptr ) {
                    *read_id_ptr = read_id;
                }
                parsing_read_id = false;
            }
            else {
                // got both spot_id and read_id
                if ( spot_id_ptr ) {
                    *spot_id_ptr =
                        NStr::StringToUInt8(CTempString(ptr+1, end-ptr-1));
                }
                break;
            }
            // start of next part
            end = ptr;
        }
        else {
            // bad format of short read id
            return NPOS;
        }
    }
    if ( ptr == begin ) {
        // no accession part
        return NPOS;
    }
    return ptr - begin;
}


CCSRABlobId::EGeneralIdType
CCSRABlobId::GetGeneralIdType(const CSeq_id_Handle& idh,
                              EGeneralIdType allow_type,
                              const string* srr)
{
    // id is of type gnl|SRA|<srr>/<label>
    // or gnl|SRA|<srr>.<spot>.<read>
    if ( idh.Which() != CSeq_id::e_General ) {
        return eNotGeneralIdType;
    }
    CConstRef<CSeq_id> id = idh.GetSeqId();
    const CDbtag& dbtag = id->GetGeneral();
    if ( !NStr::EqualNocase(dbtag.GetDb(), "SRA") ) {
        return eNotGeneralIdType;
    }
    if ( !dbtag.GetTag().IsStr() ) {
        return eNotGeneralIdType;
    }
    const string& str = dbtag.GetTag().GetStr();
    SIZE_TYPE srr_len = str.find('/');
    EGeneralIdType type;
    if ( srr_len != NPOS ) {
        if ( !(allow_type & eGeneralIdType_refseq) ) {
            return eNotGeneralIdType;
        }
        type = eGeneralIdType_refseq;
    }
    else {
        // check short read id
        if ( !(allow_type & eGeneralIdType_read) ) {
            return eNotGeneralIdType;
        }
        type = eGeneralIdType_read;
        srr_len = ParseReadId(str);
        if ( srr_len == NPOS ) {
            return eNotGeneralIdType;
        }
    }
    // check accession
    if ( srr ) {
        if ( srr->size() != srr_len ) {
            return eNotGeneralIdType;
        }
        if ( !NStr::StartsWith(str, *srr) ) {
            return eNotGeneralIdType;
        }
    }
    return type;
}


bool CCSRABlobId::GetGeneralSRAAccLabel(const CSeq_id_Handle& idh,
                                        string* srr_acc_ptr,
                                        string* label_ptr)
{
    // id is of type gnl|SRA|srr/label
    if ( !GetGeneralIdType(idh, eGeneralIdType_refseq) ) {
        return false;
    }
    CConstRef<CSeq_id> id = idh.GetSeqId();
    const string& str = id->GetGeneral().GetTag().GetStr();
    SIZE_TYPE srr_end = str.find('/');
    if ( srr_end == NPOS ) {
        return false;
    }
    if ( srr_acc_ptr ) {
        *srr_acc_ptr = str.substr(0, srr_end);
    }
    if ( label_ptr ) {
        *label_ptr = str.substr(srr_end+1);
    }
    return true;
}


bool CCSRABlobId::GetGeneralSRAAccReadId(const CSeq_id_Handle& idh,
                                         string* srr_acc_ptr,
                                         Uint8* spot_id_ptr,
                                         Uint4* read_id_ptr)
{
    // id is of type gnl|SRA|srr/label
    if ( !GetGeneralIdType(idh, eGeneralIdType_read) ) {
        return false;
    }
    CConstRef<CSeq_id> id = idh.GetSeqId();
    CTempString str = id->GetGeneral().GetTag().GetStr();
    SIZE_TYPE srr_end = ParseReadId(str, spot_id_ptr, read_id_ptr);
    if ( srr_end == NPOS ) {
        return false;
    }
    if ( srr_acc_ptr ) {
        *srr_acc_ptr = str.substr(0, srr_end);
    }
    return true;
}


static const char kBlobPrefixAnnot[] = "annot|";
static const char kBlobPrefixRefSeq[] = "refseq|";
static const char kBlobPrefixReads[] = "reads|";
static const char kRefIdPrefixGeneral[] = "gnl|";
static const char kRefIdPrefixId[] = "id|";
static const char kFileEnd[] = "|||";

string CCSRABlobId::ToString(void) const
{
    CNcbiOstrstream out;
    out << (m_BlobType == eBlobType_annot? kBlobPrefixAnnot:
            (m_BlobType == eBlobType_refseq?
             kBlobPrefixRefSeq: kBlobPrefixReads));
    out << (m_RefIdType == CCSraDb::eRefId_gnl_NAME?
            kRefIdPrefixGeneral: kRefIdPrefixId);
    out << m_File;
    out << kFileEnd;
    if ( m_BlobType != eBlobType_reads ) {
        out << m_SeqId;
    }
    else {
        out << m_FirstSpotId;
    }
    return CNcbiOstrstreamToString(out);
}


void CCSRABlobId::FromString(CTempString str0)
{
    CTempString str = str0;
    if ( NStr::StartsWith(kBlobPrefixAnnot, str) ) {
        str = str.substr(strlen(kBlobPrefixAnnot));
        m_BlobType = eBlobType_annot;
    }
    else if ( NStr::StartsWith(kBlobPrefixRefSeq, str) ) {
        str = str.substr(strlen(kBlobPrefixRefSeq));
        m_BlobType = eBlobType_refseq;
    }
    else if ( NStr::StartsWith(kBlobPrefixReads, str) ) {
        str = str.substr(strlen(kBlobPrefixReads));
        m_BlobType = eBlobType_reads;
    }
    else {
        NCBI_THROW_FMT(CSraException, eOtherError,
                       "Bad CCSRABlobId: "<<str0);
    }
    if ( NStr::StartsWith(kRefIdPrefixGeneral, str) ) {
        str = str.substr(strlen(kRefIdPrefixGeneral));
        m_RefIdType = CCSraDb::eRefId_gnl_NAME;
    }
    else if ( NStr::StartsWith(kRefIdPrefixId, str) ) {
        str = str.substr(strlen(kRefIdPrefixId));
        m_RefIdType = CCSraDb::eRefId_SEQ_ID;
    }
    else {
        NCBI_THROW_FMT(CSraException, eOtherError,
                       "Bad CCSRABlobId: "<<str0);
    }
    SIZE_TYPE div = str.rfind(kFileEnd);
    if ( div == NPOS ) {
        NCBI_THROW_FMT(CSraException, eOtherError,
                       "Bad CCSRABlobId: "<<str0);
    }
    m_File = str.substr(0, div);
    str = str.substr(div+strlen(kFileEnd));
    if ( m_BlobType != eBlobType_reads ) {
        m_SeqId = CSeq_id_Handle::GetHandle(str);
        m_FirstSpotId = 0;
    }
    else {
        m_SeqId.Reset();
        m_FirstSpotId = NStr::StringToUInt8(str);
    }
}


bool CCSRABlobId::operator<(const CBlobId& id) const
{
    const CCSRABlobId& csra2 = dynamic_cast<const CCSRABlobId&>(id);
    if ( m_File != csra2.m_File ) {
        return m_File < csra2.m_File;
    }
    if ( m_RefIdType != csra2.m_RefIdType ) {
        return m_RefIdType < csra2.m_RefIdType;
    }
    if ( m_BlobType != csra2.m_BlobType ) {
        return m_BlobType < csra2.m_BlobType;
    }
    if ( m_FirstSpotId != csra2.m_FirstSpotId ) {
        return m_FirstSpotId < csra2.m_FirstSpotId;
    }
    return m_SeqId < csra2.m_SeqId;
}


bool CCSRABlobId::operator==(const CBlobId& id) const
{
    const CCSRABlobId& csra2 = dynamic_cast<const CCSRABlobId&>(id);
    return m_BlobType == csra2.m_BlobType &&
        m_RefIdType == csra2.m_RefIdType && 
        m_FirstSpotId == csra2.m_FirstSpotId &&
        m_SeqId == csra2.m_SeqId &&
        m_File == csra2.m_File;
}


/////////////////////////////////////////////////////////////////////////////
// CCSRADataLoader_Impl
/////////////////////////////////////////////////////////////////////////////


CCSRADataLoader_Impl::CCSRADataLoader_Impl(
    const CCSRADataLoader::SLoaderParams& params)
    : m_SRRFiles(GetGCSize()),
      m_IdMapper(params.m_IdMapper)
{
    m_DirPath = params.m_DirPath;
    m_AnnotName = params.m_AnnotName;
    m_MinMapQuality = params.m_MinMapQuality;
    if ( m_MinMapQuality == -1 ) {
        m_MinMapQuality = GetDefaultMinMapQuality();
    }
    
    if ( params.m_CSRAFiles.empty() ) {
        if ( !m_DirPath.empty() ) {
            m_DirPath.erase();
            AddCSRAFile(params.m_DirPath);
        }
    }
    if ( !m_DirPath.empty() && *m_DirPath.rbegin() != '/' ) {
        m_DirPath += '/';
    }
    ITERATE (vector<string>, it, params.m_CSRAFiles) {
        AddCSRAFile(*it);
    }
}


CCSRADataLoader_Impl::~CCSRADataLoader_Impl(void)
{
}


void CCSRADataLoader_Impl::AddCSRAFile(const string& csra)
{
    m_FixedFiles[csra] =
        new CCSRAFileInfo(*this, csra, CCSraDb::eRefId_SEQ_ID);
}


CRef<CCSRAFileInfo> CCSRADataLoader_Impl::GetSRRFile(const string& acc)
{
    if ( !m_FixedFiles.empty() ) {
        // no dynamic SRR accessions
        return null;
    }
    CMutexGuard guard(m_Mutex);
    TSRRFiles::iterator it = m_SRRFiles.find(acc);
    if ( it != m_SRRFiles.end() ) {
        return it->second;
    }
    CRef<CCSRAFileInfo> info;
    try {
        info = new CCSRAFileInfo(*this, acc, CCSraDb::eRefId_gnl_NAME);
    }
    catch ( CSraException& exc ) {
        if ( exc.GetErrCode() == CSraException::eNotFoundDb ) {
            // no such SRA table
            return null;
        }
        ERR_POST_X(4, "CCSRADataLoader::GetSRRFile("<<acc<<"): accession not found: "<<exc);
        return null;
    }
    // store file in cache
    m_SRRFiles[acc] = info;
    return info;
}


CRef<CCSRARefSeqInfo>
CCSRADataLoader_Impl::GetRefSeqInfo(const CSeq_id_Handle& idh)
{
    string acc;
    if ( CCSRABlobId::GetGeneralSRAAccLabel(idh, &acc) ) {
        CRef<CCSRAFileInfo> file = GetSRRFile(acc);
        if ( !file ) {
            return null;
        }
        return file->GetRefSeqInfo(idh);
    }
    CRef<CCSRARefSeqInfo> ret;
    NON_CONST_ITERATE ( TFixedFiles, it, m_FixedFiles ) {
        CRef<CCSRARefSeqInfo> info = it->second->GetRefSeqInfo(idh);
        if ( info ) {
            if ( ret ) {
                // conflict
                ERR_POST_X(1, "CCSRADataLoader::GetRefSeqInfo: "
                           "Seq-id "<<idh<<" appears in two files: "
                           <<ret->m_File->GetCSRAName()<<" & "
                           <<info->m_File->GetCSRAName());
                continue;
            }
            ret = info;
        }
    }
    return ret;
}


CRef<CCSRAFileInfo>
CCSRADataLoader_Impl::GetReadsFileInfo(const CSeq_id_Handle& idh,
                                       Uint8* spot_id_ptr,
                                       Uint4* read_id_ptr,
                                       CRef<CCSRARefSeqInfo>* ref_ptr,
                                       TSeqPos *ref_pos_ptr)
{
    string acc;
    Uint8 spot_id;
    Uint4 read_id;
    if ( ref_ptr ) {
        *ref_ptr = 0;
    }
    if ( !CCSRABlobId::GetGeneralSRAAccReadId(idh, &acc, &spot_id, &read_id) ) {
        return null;
    }
    CRef<CCSRAFileInfo> info;
    NON_CONST_ITERATE ( TFixedFiles, it, m_FixedFiles ) {
        if ( it->second->m_CSRADb->GetSraIdPart() == acc ) {
            if ( info ) {
                // duplicate id
                ERR_POST_X(2, "CCSRADataLoader::GetReadsFileInfo: "
                           "Seq-id "<<idh<<" appears in two files: "
                           <<it->second->GetCSRAName()<<" & "
                           <<info->GetCSRAName());
                return null;
            }
            info = it->second;
        }
    }
    if ( !info ) {
        // load by SRR accession
        info = GetSRRFile(acc);
    }
    if ( !info ) {
        return null;
    }
    // check if spot_id exists
    if ( !info->IsValidReadId(spot_id, read_id, ref_ptr, ref_pos_ptr) ) {
        return null;
    }
    if ( spot_id_ptr ) {
        *spot_id_ptr = spot_id;
    }
    if ( read_id_ptr ) {
        *read_id_ptr = read_id;
    }
    return info;
}


CRef<CCSRAFileInfo>
CCSRADataLoader_Impl::GetFileInfo(const CCSRABlobId& blob_id)
{
    if ( blob_id.m_RefIdType == CCSraDb::eRefId_SEQ_ID ) {
        TFixedFiles::iterator it = m_FixedFiles.find(blob_id.m_File);
        if ( it == m_FixedFiles.end() ) {
            return null;
        }
        return it->second;
    }
    return GetSRRFile(blob_id.m_File);
}


CRef<CCSRABlobId> CCSRADataLoader_Impl::GetBlobId(const CSeq_id_Handle& idh)
{
    // return blob-id of blob with sequence
    // annots may be different
    if ( CRef<CCSRARefSeqInfo> info = GetRefSeqInfo(idh) ) {
        return info->GetBlobId(CCSRABlobId::eBlobType_refseq);
    }
    Uint8 spot_id;
    if ( CRef<CCSRAFileInfo> info = GetReadsFileInfo(idh, &spot_id) ) {
        return info->GetReadsBlobId(spot_id);
    }
    return null;
}


CTSE_LoadLock CCSRADataLoader_Impl::GetBlobById(CDataSource* data_source,
                                                const CCSRABlobId& blob_id)
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
CCSRADataLoader_Impl::GetRecords(CDataSource* data_source,
                                 const CSeq_id_Handle& idh,
                                 CDataLoader::EChoice choice)
{
    CDataLoader::TTSE_LockSet locks;
    // return blob-id of blob with annotations and possibly with sequence

    bool need_seq = false, need_align = false, need_graph = false, need_orphan = false;
    switch ( choice ) {
    case CDataLoader::eAll:
        need_seq = need_align = need_graph = true;
        break;
    case CDataLoader::eBlob:
    case CDataLoader::eBioseq:
    case CDataLoader::eBioseqCore:
    case CDataLoader::eSequence:
        need_seq = true;
        break;
    case CDataLoader::eAnnot:
    case CDataLoader::eExtAnnot:
        need_align = need_graph = true;
        break;
    case CDataLoader::eGraph:
    case CDataLoader::eExtGraph:
        need_graph = true;
        break;
    case CDataLoader::eFeatures:
    case CDataLoader::eExtFeatures:
        break;
    case CDataLoader::eAlign:
    case CDataLoader::eExtAlign:
        need_align = true;
        break;
    case CDataLoader::eOrphanAnnot:
        need_orphan = true;
        break;
    default:
        break;
    }

    if ( CRef<CCSRARefSeqInfo> info = GetRefSeqInfo(idh) ) {
        // refseq: annots and possibly ref sequence
        if ( info->m_File->m_RefIdType == CCSraDb::eRefId_gnl_NAME ) {
            // we have refseq+annot
            if ( need_align || need_graph ) {
                CRef<CCSRABlobId> annot_blob_id =
                    info->GetBlobId(CCSRABlobId::eBlobType_annot);
                locks.insert(GetBlobById(data_source, *annot_blob_id));
            }
            if ( need_seq ) {
                // include refseq blob
                CRef<CCSRABlobId> refseq_blob_id =
                    info->GetBlobId(CCSRABlobId::eBlobType_refseq);
                locks.insert(GetBlobById(data_source, *refseq_blob_id));
            }
        }
        else {
            // we have orphan annot only
            if ( need_orphan ) {
                CRef<CCSRABlobId> annot_blob_id =
                    info->GetBlobId(CCSRABlobId::eBlobType_annot);
                locks.insert(GetBlobById(data_source, *annot_blob_id));
            }
        }
        return locks;
    }

    // otherwise it might be request by short read id
    if ( need_orphan ) {
        // no orphan annots in this case
        return locks;
    }

    Uint8 spot_id;
    CRef<CCSRARefSeqInfo> ref_info;
    TSeqPos ref_pos;
    CRef<CCSRAFileInfo> info;
    if ( need_align ) {
        info = GetReadsFileInfo(idh, &spot_id, 0, &ref_info, &ref_pos);
    }
    else {
        info = GetReadsFileInfo(idh, &spot_id);
    }
    if ( info ) {
        // short read: we have sequence blob and alignment on refseq
        if ( need_seq || need_graph ) {
            if ( CRef<CCSRABlobId> blob_id = info->GetReadsBlobId(spot_id) ) {
                locks.insert(GetBlobById(data_source, *blob_id));
            }
        }
        if ( need_align && ref_info ) {
            if ( CRef<CCSRABlobId> blob_id = ref_info->GetBlobId(CCSRABlobId::eBlobType_annot) ) {
                CDataLoader::TTSE_Lock tse_lock = GetBlobById(data_source, *blob_id);
                tse_lock->x_LoadChunk(kMainChunkId);
                int chunk_id = ref_info->GetAnnotChunkId(ref_pos);
                if ( chunk_id >= 0 ) {
                    tse_lock->x_LoadChunk(chunk_id);
                }
                locks.insert(tse_lock);
            }
        }
    }
    return locks;
}


void CCSRADataLoader_Impl::LoadBlob(const CCSRABlobId& blob_id,
                                    CTSE_LoadLock& load_lock)
{
    CRef<CCSRAFileInfo> file_info = GetFileInfo(blob_id);
    switch ( blob_id.m_BlobType ) {
    case CCSRABlobId::eBlobType_annot:
        file_info->GetRefSeqInfo(blob_id)->LoadAnnotBlob(load_lock);
        break;
    case CCSRABlobId::eBlobType_refseq:
        file_info->GetRefSeqInfo(blob_id)->LoadRefSeqBlob(load_lock);
        break;
    case CCSRABlobId::eBlobType_reads:
        file_info->LoadReadsBlob(blob_id, load_lock);
        break;
    }
}


void CCSRADataLoader_Impl::LoadChunk(const CCSRABlobId& blob_id,
                                    CTSE_Chunk_Info& chunk_info)
{
    _TRACE("Loading chunk "<<blob_id.ToString()<<"."<<chunk_info.GetChunkId());
    CRef<CCSRAFileInfo> file_info = GetFileInfo(blob_id);
    switch ( blob_id.m_BlobType ) {
    case CCSRABlobId::eBlobType_annot:
        file_info->GetRefSeqInfo(blob_id)->LoadAnnotChunk(chunk_info);
        break;
    case CCSRABlobId::eBlobType_refseq:
        file_info->GetRefSeqInfo(blob_id)->LoadRefSeqChunk(chunk_info);
        break;
    case CCSRABlobId::eBlobType_reads:
        file_info->LoadReadsChunk(blob_id, chunk_info);
        break;
    }
}


CCSRADataLoader::TAnnotNames
CCSRADataLoader_Impl::GetPossibleAnnotNames(void) const
{
    TAnnotNames names;
    ITERATE ( TFixedFiles, it, m_FixedFiles ) {
        it->second->GetPossibleAnnotNames(names);
    }
    sort(names.begin(), names.end());
    names.erase(unique(names.begin(), names.end()), names.end());
    return names;
}


CCSraRefSeqIterator
CCSRADataLoader_Impl::GetRefSeqIterator(const CSeq_id_Handle& idh)
{
    if ( CRef<CCSRARefSeqInfo> info = GetRefSeqInfo(idh) ) {
        return info->GetRefSeqIterator();
    }
    return CCSraRefSeqIterator();
}


CCSraShortReadIterator
CCSRADataLoader_Impl::GetShortReadIterator(const CSeq_id_Handle& idh)
{
    Uint8 spot_id;
    Uint4 read_id;
    if ( CRef<CCSRAFileInfo> info = GetReadsFileInfo(idh, &spot_id, &read_id) ) {
        return CCSraShortReadIterator(info->m_CSRADb, spot_id, read_id);
    }
    return CCSraShortReadIterator();
}


void CCSRADataLoader_Impl::GetIds(const CSeq_id_Handle& idh, TIds& ids)
{
    if ( CCSraRefSeqIterator iter = GetRefSeqIterator(idh) ) {
        ITERATE ( CBioseq::TId, it, iter.GetRefSeq_ids() ) {
            ids.push_back(CSeq_id_Handle::GetHandle(**it));
        }
    }
    else if ( GetReadsFileInfo(idh) ) {
        ids.push_back(idh);
    }
}


CSeq_id_Handle CCSRADataLoader_Impl::GetAccVer(const CSeq_id_Handle& idh)
{
    // the only possible acc.ver is for reference sequence
    if ( CCSraRefSeqIterator iter = GetRefSeqIterator(idh) ) {
        ITERATE ( CBioseq::TId, it, iter.GetRefSeq_ids() ) {
            if ( (*it)->GetTextseq_Id() ) {
                return CSeq_id_Handle::GetHandle(**it);
            }
        }
    }
    return CSeq_id_Handle();
}


int CCSRADataLoader_Impl::GetGi(const CSeq_id_Handle& idh)
{
    // the only possible gi is for reference sequence
    if ( CCSraRefSeqIterator iter = GetRefSeqIterator(idh) ) {
        ITERATE ( CBioseq::TId, it, iter.GetRefSeq_ids() ) {
            if ( (*it)->IsGi() ) {
                return (*it)->GetGi();
            }
        }
    }
    return 0;
}


string CCSRADataLoader_Impl::GetLabel(const CSeq_id_Handle& idh)
{
    if ( GetBlobId(idh) ) {
        return objects::GetLabel(idh); // default label from Seq-id
    }
    return string(); // sequence is unknown
}


int CCSRADataLoader_Impl::GetTaxId(const CSeq_id_Handle& idh)
{
    if ( GetBlobId(idh) ) {
        return 0; // taxid is not defined
    }
    return -1; // sequence is unknown
}


TSeqPos CCSRADataLoader_Impl::GetSequenceLength(const CSeq_id_Handle& idh)
{
    // the only possible acc.ver is for reference sequence
    if ( CCSraRefSeqIterator iter = GetRefSeqIterator(idh) ) {
        return iter.GetSeqLength();
    }
    if ( CCSraShortReadIterator iter = GetShortReadIterator(idh) ) {
        return iter.GetShortLen();
    }
    return kInvalidSeqPos;
}


CSeq_inst::TMol CCSRADataLoader_Impl::GetSequenceType(const CSeq_id_Handle& idh)
{
    if ( GetBlobId(idh) ) {
        return CSeq_inst::eMol_na;
    }
    return CSeq_inst::eMol_not_set;
}


/////////////////////////////////////////////////////////////////////////////
// CCSRAFileInfo
/////////////////////////////////////////////////////////////////////////////


CCSRAFileInfo::CCSRAFileInfo(CCSRADataLoader_Impl& impl,
                             const string& csra,
                             CCSraDb::ERefIdType ref_id_type)
{
    //CMutexGuard guard(GetMutex());
    x_Initialize(impl, csra, ref_id_type);
    for ( CCSraRefSeqIterator rit(m_CSRADb); rit; ++rit ) {
        CSeq_id_Handle seq_id = rit.GetRefSeq_id_Handle();
        string refseq_label = seq_id.AsString();
        AddRefSeq(refseq_label, seq_id);
    }
}


void CCSRAFileInfo::x_Initialize(CCSRADataLoader_Impl& impl,
                                 const string& csra,
                                 CCSraDb::ERefIdType ref_id_type)
{
    m_CSRAName = csra;
    m_RefIdType = ref_id_type;
    m_AnnotName = impl.m_AnnotName;
    if ( m_AnnotName.empty() ) {
        m_AnnotName = m_CSRAName;
    }
    m_MinMapQuality = impl.GetMinMapQuality();
    m_CSRADb = CCSraDb(impl.m_Mgr, impl.m_DirPath+csra,
                       impl.m_IdMapper.get(),
                       ref_id_type);
    int max_separate_spot_groups = GetMaxSeparateSpotGroups();
    if ( max_separate_spot_groups > 1 ) {
        m_CSRADb.GetSpotGroups(m_SeparateSpotGroups);
        if ( m_SeparateSpotGroups.size() > size_t(max_separate_spot_groups) ) {
            m_SeparateSpotGroups.clear();
        }
    }
}


string CCSRAFileInfo::GetAnnotName(const string& spot_group,
                                   ECSRAAnnotChunkIdType type) const
{
    string name = GetBaseAnnotName();
    if ( !m_SeparateSpotGroups.empty() ) {
        name += SPOT_GROUP_SEPARATOR;
        name += spot_group;
    }
    if ( type == eCSRAAnnotChunk_pileup_graph ) {
        if ( !name.empty() ) {
            name += ' ';
        }
        name += PILEUP_NAME_SUFFIX;
    }
    return name;
}


string CCSRAFileInfo::GetAlignAnnotName(void) const
{
    return GetAnnotName(kEmptyStr, eCSRAAnnotChunk_align);
}


string CCSRAFileInfo::GetAlignAnnotName(const string& spot_group) const
{
    return GetAnnotName(spot_group, eCSRAAnnotChunk_align);
}


string CCSRAFileInfo::GetPileupAnnotName(void) const
{
    return GetAnnotName(kEmptyStr, eCSRAAnnotChunk_pileup_graph);
}


string CCSRAFileInfo::GetPileupAnnotName(const string& spot_group) const
{
    return GetAnnotName(spot_group, eCSRAAnnotChunk_pileup_graph);
}


void CCSRAFileInfo::GetPossibleAnnotNames(TAnnotNames& names) const
{
    if ( GetSeparateSpotGroups().empty() ) {
        string align_annot_name = GetAlignAnnotName();
        if ( align_annot_name.empty() ) {
            names.push_back(CAnnotName());
        }
        else {
            names.push_back(CAnnotName(align_annot_name));
        }
        names.push_back(CAnnotName(GetPileupAnnotName()));
    }
    else {
        ITERATE ( vector<string>, it, GetSeparateSpotGroups() ) {
            names.push_back(CAnnotName(GetAlignAnnotName(*it)));
            names.push_back(CAnnotName(GetPileupAnnotName(*it)));
        }
    }
}


void CCSRAFileInfo::AddRefSeq(const string& refseq_label,
                              const CSeq_id_Handle& refseq_id)
{
    if ( GetDebugLevel() >= 1 ) {
        LOG_POST_X(9, Info << "CCSRADataLoader(" << m_CSRAName << "): "
                   "Found "<<refseq_label<<" -> "<<refseq_id);
    }
    m_RefSeqs[refseq_id] = new CCSRARefSeqInfo(this, refseq_id);
}


void CCSRAFileInfo::GetShortSeqBlobId(CRef<CCSRABlobId>& ret,
                                      const CSeq_id_Handle& idh) const
{

}


void CCSRAFileInfo::GetRefSeqBlobId(CRef<CCSRABlobId>& ret,
                                    const CSeq_id_Handle& idh)
{
    CRef<CCSRARefSeqInfo> info = GetRefSeqInfo(idh);
    if ( info ) {
        info->SetBlobId(ret, CCSRABlobId::eBlobType_refseq, idh);
    }
}


CRef<CCSRARefSeqInfo>
CCSRAFileInfo::GetRefSeqInfo(const CSeq_id_Handle& seq_id)
{
    //CMutexGuard guard(GetMutex());
    TRefSeqs::const_iterator it = m_RefSeqs.find(seq_id);
    if ( it != m_RefSeqs.end() ) {
        return it->second;
    }
    string srr, label;
    if ( CCSRABlobId::GetGeneralSRAAccLabel(seq_id, &srr, &label) &&
         srr == GetCSRAName() ) {
        AddRefSeq(label, seq_id);
        it = m_RefSeqs.find(seq_id);
        if ( it != m_RefSeqs.end() ) {
            return it->second;
        }
    }
    return null;
}


bool CCSRAFileInfo::IsValidReadId(Uint8 spot_id, Uint4 read_id,
                                  CRef<CCSRARefSeqInfo>* ref_ptr,
                                  TSeqPos* ref_pos_ptr)
{
    //CMutexGuard guard(GetMutex());
    CCSraShortReadIterator read_it(m_CSRADb, spot_id, read_id);
    if ( ref_ptr ) {
        *ref_ptr = 0;
    }
    if ( ref_pos_ptr ) {
        *ref_pos_ptr = kInvalidSeqPos;
    }
    if ( !read_it ) {
        return false;
    }
    if ( ref_ptr || ref_pos_ptr ) {
        CCSraRefSeqIterator ref_seq_it = read_it.GetRefSeqIter(ref_pos_ptr);
        if ( ref_seq_it ) {
            if ( ref_ptr ) {
                *ref_ptr = GetRefSeqInfo(ref_seq_it.GetRefSeq_id_Handle());
            }
        }
    }
    return true;
}


CRef<CCSRABlobId> CCSRAFileInfo::GetReadsBlobId(Uint8 spot_id) const
{
    Uint8 first_spot_id = (spot_id-1)/kReadsPerBlob*kReadsPerBlob+1;
    return Ref(new CCSRABlobId(*this, first_spot_id));
}


void CCSRAFileInfo::LoadReadsBlob(const CCSRABlobId& blob_id,
                                  CTSE_LoadLock& load_lock)
{
    LoadReadsMainEntry(blob_id, load_lock);
}


void CCSRAFileInfo::LoadReadsChunk(const CCSRABlobId& /*blob_id*/,
                                   CTSE_Chunk_Info& /*chunk_info*/)
{
    _ASSERT(!"invalid call");
}


void CCSRAFileInfo::LoadReadsMainEntry(const CCSRABlobId& blob_id,
                                       CTSE_LoadLock& load_lock)
{
    //CMutexGuard guard(GetMutex());
    CRef<CSeq_entry> entry(new CSeq_entry);
    Uint8 first_spot_id = blob_id.m_FirstSpotId;
    Uint8 stop_spot_id = first_spot_id + kReadsPerBlob;
    if ( GetDebugLevel() >= 5 ) {
        LOG_POST_X(12, Info<<
                   "CCSRADataLoader:LoadReads("<<blob_id.ToString()<<", "<<
                   first_spot_id<<"-"<<(stop_spot_id-1));
    }
    CCSraShortReadIterator::TBioseqFlags flags = CCSraShortReadIterator::fDefaultBioseqFlags;
    if ( GetQualityGraphsParam() ) {
        flags |= CCSraShortReadIterator::fQualityGraph;
    }
    for ( CCSraShortReadIterator it(*this, first_spot_id); it; ++it ) {
        if ( it.GetSpotId() >= stop_spot_id ) {
            break;
        }
        CRef<CSeq_entry> e(new CSeq_entry);
        e->SetSeq(*it.GetShortBioseq(flags));
        entry->SetSet().SetSeq_set().push_back(e);
    }

    load_lock->SetSeq_entry(*entry);
}


/////////////////////////////////////////////////////////////////////////////
// CCSRARefSeqInfo
/////////////////////////////////////////////////////////////////////////////


CCSRARefSeqInfo::CCSRARefSeqInfo(CCSRAFileInfo* csra_file,
                                 const CSeq_id_Handle& seq_id)
    : m_File(csra_file),
      m_RefSeqId(seq_id),
      m_MinMapQuality(csra_file->GetMinMapQuality()),
      m_LoadedRanges(false)
{
}


CRef<CCSRABlobId> CCSRARefSeqInfo::GetBlobId(CCSRABlobId::EBlobType type) const
{
    return Ref(new CCSRABlobId(type, *m_File, GetRefSeqId()));
}


void CCSRARefSeqInfo::SetBlobId(CRef<CCSRABlobId>& ret,
                                CCSRABlobId::EBlobType blob_type,
                                const CSeq_id_Handle& idh) const
{
    CRef<CCSRABlobId> id(new CCSRABlobId(blob_type, *m_File, GetRefSeqId()));
    if ( ret ) {
        ERR_POST_X(3, "CCSRADataLoader::GetBlobId: "
                   "Seq-id "<<idh<<" appears in two files: "
                   <<ret->ToString()<<" & "<<id->ToString());
    }
    else {
        ret = id;
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

        void Collect(CCSraDb& csra_db, const CSeq_id_Handle& ref_id,
                     TSeqPos ref_pos, unsigned count, int min_quality);

        unsigned GetStatCount(void) const {
            return m_Count;
        }
        double GetStatLen(void) const {
            return m_RefPosLast - m_RefPosFirst + .5;
        }
    };
    
    
    void SRefStat::Collect(CCSraDb& csra_db, const CSeq_id_Handle& ref_id,
                           TSeqPos ref_pos, unsigned count, int min_quality)
    {
        m_RefPosQuery = ref_pos;
        size_t skipped = 0;
        CCSraAlignIterator ait(csra_db, ref_id, ref_pos);
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
            m_RefPosLast = pos;
            TSeqPos len = ait.GetRefSeqLen();
            if ( len > m_RefLenMax ) {
                m_RefLenMax = len;
            }
            TSeqPos max = pos + len;
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
        if ( GetDebugLevel() >= 2 ) {
            LOG_POST_X(4, Info << "CCSRADataLoader: "
                       "Stat @ "<<m_RefPosQuery<<": "<<m_Count<<" entries: "<<
                       m_RefPosFirst<<"-"<<m_RefPosLast<<
                       "(+"<<m_RefPosMax-m_RefPosLast<<")"<<
                       " max len: "<<m_RefLenMax<<
                       " skipped: "<<skipped);
        }
    }
};


void CCSRARefSeqInfo::LoadRanges(void)
{
    if ( m_LoadedRanges ) {
        return;
    }
    CMutexGuard guard(m_File->GetMutex());
    if ( m_LoadedRanges ) {
        return;
    }

    _TRACE("Loading "<<GetRefSeqId());
    if ( !x_LoadRangesCov() ) {
        x_LoadRangesStat();
    }
    _TRACE("Loaded ranges on "<<GetRefSeqId());
    m_LoadedRanges = true;
}


CCSraRefSeqIterator
CCSRARefSeqInfo::GetRefSeqIterator(void) const
{
    return CCSraRefSeqIterator(*m_File, GetRefSeqId());
}


bool CCSRARefSeqInfo::x_LoadRangesCov(void)
{
    m_CovAnnot = CCSraRefSeqIterator(*m_File, GetRefSeqId())
        .GetCoverageAnnot(m_File->GetAlignAnnotName());
    return false;
}


void CCSRARefSeqInfo::x_LoadRangesStat(void)
{
    const unsigned kNumStat = 10;
    const unsigned kStatCount = 1000;
    vector<SRefStat> stat(kNumStat);
    TSeqPos ref_length =
        CCSraRefSeqIterator(*m_File, GetRefSeqId()).GetSeqLength();
    TSeqPos ref_begin = 0, ref_end = ref_length, max_len = 0;
    double stat_len = 0, stat_cnt = 0;
    const unsigned scan_first = 1;
    if ( scan_first ) {
        stat[0].Collect(*m_File, GetRefSeqId(), 0,
                        kStatCount, m_MinMapQuality);
        if ( stat[0].m_Count != kStatCount ) {
            // single chunk
            if ( stat[0].m_Count > 0 ) {
                CCSRARefSeqChunkInfo chunk;
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
        max_len = stat[0].m_RefLenMax;
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
        if ( stat[k].m_RefLenMax > max_len ) {
            max_len = stat[k].m_RefLenMax;
        }
    }
    double density = stat_cnt / stat_len;
    double exp_count = (ref_end-ref_begin)*density;
    unsigned chunks = unsigned(exp_count/kChunkSize+1);
    chunks = min(chunks, unsigned(sqrt(exp_count)+1));
    max_len *= 2;
    if ( GetDebugLevel() >= 1 ) {
        LOG_POST_X(5, Info << "CCSRADataLoader: "
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
        CCSRARefSeqChunkInfo chunk;
        chunk.m_AlignCount = 1;
        TSeqPos pos = pp[k];
        TSeqPos end = pp[k+1];
        chunk.m_RefSeqRange.SetFrom(pos);
        chunk.m_RefSeqRange.SetToOpen(end+max_len);
        chunk.m_MaxRefSeqFrom = end-1;
        m_Chunks.push_back(chunk);
        if ( GetDebugLevel() >= 1 ) {
            LOG_POST_X(6, Info << "CCSRADataLoader: "
                       "Chunk "<<k<<": "<<chunk.m_RefSeqRange<<
                       " max: "<<chunk.m_MaxRefSeqFrom);
        }
    }
}


int CCSRARefSeqInfo::GetAnnotChunkId(TSeqPos ref_pos) const
{
    int chunk_count = int(m_Chunks.size());
    for ( int range_id = 0; range_id < chunk_count; ++range_id ) {
        const CCSRARefSeqChunkInfo& info = m_Chunks[range_id];
        if ( info.GetRefSeqRange().GetFrom() <= ref_pos &&
             info.GetMaxRefSeqFrom() >= ref_pos ) {
            int chunk_id = range_id*eCSRAAnnotChunk_mul + eCSRAAnnotChunk_align;
            return chunk_id;
        }
    }
    return -1;
}


void CCSRARefSeqInfo::LoadAnnotMainSplit(CTSE_LoadLock& load_lock)
{
    CMutexGuard guard(m_File->GetMutex());
    CRef<CSeq_entry> entry(new CSeq_entry);
    entry->SetSet().SetId().SetId(1);

    load_lock->SetSeq_entry(*entry);
    CTSE_Split_Info& split_info = load_lock->GetSplitInfo();

    bool has_pileup = GetPileupGraphsParam();
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


void CCSRARefSeqInfo::LoadAnnotMainChunk(CTSE_Chunk_Info& chunk_info)
{
    CMutexGuard guard(m_File->GetMutex());
    if ( GetDebugLevel() >= 5 ) {
        LOG_POST_X(13, Info<<
                   "CCSRADataLoader:LoadAnnotMain("<<
                   chunk_info.GetBlobId().ToString()<<", "<<
                   chunk_info.GetChunkId());
    }
    LoadRanges(); // also loads m_CovAnnot
    CTSE_Split_Info& split_info =
        const_cast<CTSE_Split_Info&>(chunk_info.GetSplitInfo());
    int chunk_count = int(m_Chunks.size());
    bool has_pileup = GetPileupGraphsParam();
    string align_name, pileup_name;
    bool separate_spot_groups = !m_File->GetSeparateSpotGroups().empty();
    if ( !separate_spot_groups ) {
        align_name = m_File->GetAlignAnnotName();
        if ( has_pileup ) {
            pileup_name = m_File->GetPileupAnnotName();
        }
    }
    CTSE_Chunk_Info::TPlace place(CSeq_id_Handle(), kTSEId);

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

    // create chunk info for alignments
    for ( int range_id = 0; range_id < chunk_count; ++range_id ) {
        const CCSRARefSeqChunkInfo& info = m_Chunks[range_id];
        
        int chunk_id = range_id*eCSRAAnnotChunk_mul + eCSRAAnnotChunk_align;
        CRef<CTSE_Chunk_Info> chunk(new CTSE_Chunk_Info(chunk_id));
        if ( separate_spot_groups ) {
            ITERATE ( vector<string>, it, m_File->GetSeparateSpotGroups() ) {
                chunk->x_AddAnnotType
                    (CAnnotName(m_File->GetAlignAnnotName(*it)),
                     SAnnotTypeSelector(CSeq_annot::C_Data::e_Align),
                     GetRefSeqId(),
                     info.GetRefSeqRange());
            }
        }
        else {
            chunk->x_AddAnnotType
                (align_name,
                 SAnnotTypeSelector(CSeq_annot::C_Data::e_Align),
                 GetRefSeqId(),
                 info.GetRefSeqRange());
        }
        chunk->x_AddAnnotPlace(kTSEId);
        split_info.AddChunk(*chunk);

        if ( !has_pileup ) {
            continue;
        }

        chunk_id = range_id*eCSRAAnnotChunk_mul+eCSRAAnnotChunk_pileup_graph;
        chunk = new CTSE_Chunk_Info(chunk_id);
        if ( separate_spot_groups ) {
            ITERATE ( vector<string>, it, m_File->GetSeparateSpotGroups() ) {
                chunk->x_AddAnnotType
                    (CAnnotName(m_File->GetPileupAnnotName(*it)),
                     SAnnotTypeSelector(CSeq_annot::C_Data::e_Graph),
                     GetRefSeqId(),
                     CRange<TSeqPos>(info.GetRefSeqRange().GetFrom(),
                                     info.GetMaxRefSeqFrom()));
            }
        }
        else {
            chunk->x_AddAnnotType
                (pileup_name,
                 SAnnotTypeSelector(CSeq_annot::C_Data::e_Graph),
                 GetRefSeqId(),
                 CRange<TSeqPos>(info.GetRefSeqRange().GetFrom(),
                                 info.GetMaxRefSeqFrom()));
        }
        chunk->x_AddAnnotPlace(kTSEId);
        split_info.AddChunk(*chunk);
    }

    chunk_info.SetLoaded();
}


void CCSRARefSeqInfo::LoadAnnotBlob(CTSE_LoadLock& load_lock)
{
    LoadAnnotMainSplit(load_lock);
}


void CCSRARefSeqInfo::LoadAnnotChunk(CTSE_Chunk_Info& chunk_info)
{
    if ( chunk_info.GetChunkId() == kMainChunkId ) {
        LoadAnnotMainChunk(chunk_info);
    }
    else {
        switch ( chunk_info.GetChunkId() % eCSRAAnnotChunk_mul ) {
        case eCSRAAnnotChunk_align:
            LoadAnnotAlignChunk(chunk_info);
            break;
        case eCSRAAnnotChunk_pileup_graph:
            LoadAnnotPileupChunk(chunk_info);
            break;
        }
    }
}


void CCSRARefSeqInfo::LoadRefSeqBlob(CTSE_LoadLock& load_lock)
{
    LoadRefSeqMainEntry(load_lock);
}


void CCSRARefSeqInfo::LoadRefSeqMainEntry(CTSE_LoadLock& load_lock)
{
    CMutexGuard guard(m_File->GetMutex());
    CRef<CSeq_entry> entry(new CSeq_entry);

    CCSraRefSeqIterator it(*m_File, GetRefSeqId());
    CRef<CBioseq> seq = it.GetRefBioseq(it.eOmitData);
    entry->SetSeq(*seq);
    TSeqPos ref_data_size = it.GetSeqLength();

    load_lock->SetSeq_entry(*entry);
    CTSE_Split_Info& split_info = load_lock->GetSplitInfo();

    // register ref seq data chunks
    TSeqPos chunk_size = m_File->GetDb().GetRowSize()*kChunkSeqDataMul;
    vector<CTSE_Chunk_Info::TLocation> loc_set(1);
    loc_set[0].first = GetRefSeqId();
    for ( TSeqPos i = 0; i*chunk_size < ref_data_size; ++i ) {
        int chunk_id = i;
        CRef<CTSE_Chunk_Info> chunk(new CTSE_Chunk_Info(chunk_id));
        loc_set[0].second.SetFrom(i*chunk_size);
        loc_set[0].second.SetToOpen(min(i*chunk_size+chunk_size,
                                        ref_data_size));
        chunk->x_AddSeq_data(loc_set);
        split_info.AddChunk(*chunk);
    }
}


void CCSRARefSeqInfo::LoadRefSeqChunk(CTSE_Chunk_Info& chunk_info)
{
    CMutexGuard guard(m_File->GetMutex());
    int range_id = chunk_info.GetChunkId();
    CTSE_Chunk_Info::TPlace place(GetRefSeqId(), 0);
    TRange range;
    TSeqPos chunk_data_size = m_File->GetDb().GetRowSize()*kChunkSeqDataMul;
    range.SetFrom(range_id*chunk_data_size);
    range.SetLength(chunk_data_size);
    if ( GetDebugLevel() >= 5 ) {
        LOG_POST_X(14, Info<<
                   "CCSRADataLoader:LoadRefSeqData("<<
                   chunk_info.GetBlobId().ToString()<<", "<<
                   chunk_info.GetChunkId());
    }
    _TRACE("Loading ref "<<GetRefSeqId()<<" @ "<<range);
    list< CRef<CSeq_literal> > data;
    CCSraRefSeqIterator(*m_File, GetRefSeqId()).GetRefLiterals(data, range);
    _ASSERT(!data.empty());
    chunk_info.x_LoadSequence(place, range.GetFrom(), data);
    chunk_info.SetLoaded();
}


BEGIN_LOCAL_NAMESPACE;

struct SBaseStat
{
    enum {
        kStat_A = 0,
        kStat_C = 1,
        kStat_G = 2,
        kStat_T = 3,
        kStat_Insert = 4,
        kStat_Match = 5,
        kNumStat = 6
    };
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
    void add_gap() {
        cnts[kStat_Insert] += 1;
    }

    unsigned cnts[kNumStat];
};


struct SChunkAnnots {
    SChunkAnnots(CCSRAFileInfo* file_info,
                 ECSRAAnnotChunkIdType type);

    CRef<CCSRAFileInfo> m_File;
    bool m_SeparateSpotGroups;
    ECSRAAnnotChunkIdType m_Type;
    typedef pair<CRef<CSeq_annot>, vector<SBaseStat> > TSlot;
    typedef map<string, TSlot> TAnnots;
    TAnnots m_Annots;
    TAnnots::iterator m_LastAnnot;

    TSlot& Select(const CCSraAlignIterator& ait);
    void Create(const string& name);
};


SChunkAnnots::SChunkAnnots(CCSRAFileInfo* file,
                           ECSRAAnnotChunkIdType type)
    : m_File(file),
      m_SeparateSpotGroups(!m_File->GetSeparateSpotGroups().empty()),
      m_Type(type),
      m_LastAnnot(m_Annots.end())
{
    if ( !m_SeparateSpotGroups ) {
        Create(kEmptyStr);
    }
    else if ( m_Type == eCSRAAnnotChunk_pileup_graph ) {
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
    m_LastAnnot = m_Annots.insert(TAnnots::value_type(name, TSlot(null, vector<SBaseStat>()))).first;
    if ( !m_LastAnnot->second.first ) {
        const string& annot_name = m_File->GetAnnotName(name, m_Type);
        if ( m_Type == eCSRAAnnotChunk_align ) {
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


void CCSRARefSeqInfo::LoadAnnotAlignChunk(CTSE_Chunk_Info& chunk_info)
{
    if ( GetDebugLevel() >= 5 ) {
        LOG_POST_X(15, Info<<
                   "CCSRADataLoader:LoadAlignChunk("<<
                   chunk_info.GetBlobId().ToString()<<", "<<
                   chunk_info.GetChunkId());
    }
    CMutexGuard guard(m_File->GetMutex());
    int range_id = chunk_info.GetChunkId() / eCSRAAnnotChunk_mul;
    const CCSRARefSeqChunkInfo& chunk = m_Chunks[range_id];
    TSeqPos pos = chunk.GetRefSeqRange().GetFrom();
    TSeqPos len = chunk.GetMaxRefSeqFrom() - pos + 1;
    CTSE_Chunk_Info::TPlace place(CSeq_id_Handle(), kTSEId);
    int min_quality = m_MinMapQuality;
    _TRACE("Loading aligns "<<GetRefSeqId()<<" @ "<<chunk.GetRefSeqRange());
    size_t skipped = 0, count = 0;
    SChunkAnnots annots(m_File, eCSRAAnnotChunk_align);
   
    CCSraAlignIterator ait(*m_File, GetRefSeqId(), pos, len);
    for( ; ait; ++ait ){
        if ( ait.GetRefSeqPos() < pos ) {
            // the alignments starts before current chunk range
            ++skipped;
            continue;
        }
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
                       "CCSRADataLoader(" << m_File->GetCSRAName() << "): "
                       "Chunk "<<chunk_info.GetChunkId());
        }
        ITERATE ( SChunkAnnots::TAnnots, it, annots.m_Annots ) {
            chunk_info.x_LoadAnnot(place, *it->second.first);
        }
    }
    if ( GetDebugLevel() >= 2 ) {
        LOG_POST_X(7, Info<<"CCSRADataLoader: "
                   "Loaded "<<GetRefSeqId()<<" @ "<<
                   chunk.GetRefSeqRange()<<": "<<
                   count<<" skipped: "<<skipped);
    }
    chunk_info.SetLoaded();
}


void CCSRARefSeqInfo::LoadAnnotPileupChunk(CTSE_Chunk_Info& chunk_info)
{
    if ( GetDebugLevel() >= 5 ) {
        LOG_POST_X(16, Info<<
                   "CCSRADataLoader:LoadPileupChunk("<<
                   chunk_info.GetBlobId().ToString()<<", "<<
                   chunk_info.GetChunkId());
    }
    CMutexGuard guard(m_File->GetMutex());
    int chunk_id = chunk_info.GetChunkId();
    int range_id = chunk_id / eCSRAAnnotChunk_mul;
    const CCSRARefSeqChunkInfo& chunk = m_Chunks[range_id];
    TSeqPos pos = chunk.GetRefSeqRange().GetFrom();
    TSeqPos len = chunk.GetMaxRefSeqFrom() - pos + 1;
    CTSE_Chunk_Info::TPlace place(CSeq_id_Handle(), kTSEId);
    int min_quality = m_MinMapQuality;
    _TRACE("Loading pileup "<<GetRefSeqId()<<" @ "<<chunk.GetRefSeqRange());
    size_t count = 0, skipped = 0;

    SChunkAnnots annots(m_File, eCSRAAnnotChunk_pileup_graph);

    CCSraAlignIterator ait(*m_File, GetRefSeqId(), pos, len);
    for ( ; ait; ++ait ) {
        if ( min_quality > 0 && ait.GetMapQuality() < min_quality ) {
            ++skipped;
            continue;
        }
        ++count;
        vector<SBaseStat>& ss = annots.Select(ait).second;
        if ( ss.empty() ) {
            ss.resize(len);
        }

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
                for ( int i = 0; i < seglen; ++i ) {
                    if ( ref_pos < len ) {
                        ss[ref_pos].add_match();
                    }
                    ++ref_pos;
                    ++read_pos;
                }
            }
            else if ( type == 'M' || type == 'X' ) {
                // mismatch
                for ( int i = 0; i < seglen; ++i ) {
                    if ( ref_pos < len ) {
                        ss[ref_pos].add_base(read[read_pos]);
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
            else if ( type == 'D' || type == 'N' ) {
                // delete
                for ( int i = 0; i < seglen; ++i ) {
                    if ( ref_pos < len ) {
                        ss[ref_pos].add_gap();
                    }
                    ++ref_pos;
                }
            }
            else if ( type != 'P' ) {
                NCBI_THROW_FMT(CSraException, eOtherError,
                               "Bad CIGAR char: " <<type<< " in " <<cigar);
            }
        }
    }

    if ( GetDebugLevel() >= 2 ) {
        LOG_POST_X(10, Info<<"CCSRADataLoader: "
                   "Loaded pileup "<<GetRefSeqId()<<" @ "<<
                   chunk.GetRefSeqRange()<<": "<<
                   count<<" skipped: "<<skipped);
    }

    int c_min[SBaseStat::kNumStat], c_max[SBaseStat::kNumStat];
    for ( int i = 0; i < SBaseStat::kNumStat; ++i ) {
        c_min[i] = kMax_Int;
        c_max[i] = 0;
    }
    NON_CONST_ITERATE ( SChunkAnnots::TAnnots, it, annots.m_Annots ) {
        vector<SBaseStat>& ss = it->second.second;
        if ( ss.empty() ) {
            ss.resize(len);
        }
        for ( size_t j = 0; j < len; ++j ) {
            const SBaseStat& s = ss[j];
            for ( int i = 0; i < SBaseStat::kNumStat; ++i ) {
                int c = s.cnts[i];
                c_min[i] = min(c_min[i], c);
                c_max[i] = max(c_max[i], c);
            }
        }

        for ( int i = 0; i < SBaseStat::kNumStat; ++i ) {
            CRef<CSeq_graph> graph(new CSeq_graph);
            static const char* const titles[6] = {
                "Number of A bases",
                "Number of C bases",
                "Number of G bases",
                "Number of T bases",
                "Number of inserts",
                "Number of matches"
            };
            graph->SetTitle(titles[i]);
            CSeq_interval& loc = graph->SetLoc().SetInt();
            loc.SetId(*ait.GetRefSeq_id());
            loc.SetFrom(pos);
            loc.SetTo(pos+len-1);
            graph->SetNumval(len);

            if ( c_max[i] < 256 ) {
                CByte_graph& data = graph->SetGraph().SetByte();
                CByte_graph::TValues& vv = data.SetValues();
                vv.reserve(len);
                for ( size_t j = 0; j < len; ++j ) {
                    vv.push_back(ss[j].cnts[i]);
                }
                data.SetMin(c_min[i]);
                data.SetMax(c_max[i]);
                data.SetAxis(0);
            }
            else {
                CInt_graph& data = graph->SetGraph().SetInt();
                CInt_graph::TValues& vv = data.SetValues();
                vv.reserve(len);
                for ( size_t j = 0; j < len; ++j ) {
                    vv.push_back(ss[j].cnts[i]);
                }
                data.SetMin(c_min[i]);
                data.SetMax(c_max[i]);
                data.SetAxis(0);
            }
            it->second.first->SetData().SetGraph().push_back(graph);
        }
        chunk_info.x_LoadAnnot(place, *it->second.first);
    }
    chunk_info.SetLoaded();
}


/////////////////////////////////////////////////////////////////////////////
// CCSRARefSeqChunkInfo
/////////////////////////////////////////////////////////////////////////////


void CCSRARefSeqChunkInfo::AddRefSeqRange(const TRange& range)
{
    ++m_AlignCount;
    m_RefSeqRange += range;
    m_MaxRefSeqFrom = max(m_MaxRefSeqFrom, range.GetFrom());
}


END_SCOPE(objects)
END_NCBI_SCOPE
