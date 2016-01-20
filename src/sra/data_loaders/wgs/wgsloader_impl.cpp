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
 * File Description: WGS file data loader
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/general/general__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqres/seqres__.hpp>

//#define USE_ID2_CLIENT
#ifdef USE_ID2_CLIENT
# include <objects/id2/id2__.hpp>
# include <objects/id2/id2_client.hpp>
#endif

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
#include <sra/readers/sra/wgsresolver.hpp>
#include <sra/data_loaders/wgs/wgsloader.hpp>
#include <sra/data_loaders/wgs/impl/wgsloader_impl.hpp>

#include <util/sequtil/sequtil_manip.hpp>

#include <algorithm>
#include <cmath>
#include <sra/error_codes.hpp>

BEGIN_NCBI_SCOPE

#define NCBI_USE_ERRCODE_X   WGSLoader
NCBI_DEFINE_ERR_SUBCODE_X(16);

BEGIN_SCOPE(objects)

class CDataLoader;

static const int kChunkId_QualityGraph = 1;

NCBI_PARAM_DECL(int, WGS_LOADER, DEBUG);
NCBI_PARAM_DEF_EX(int, WGS_LOADER, DEBUG, 0,
                  eParam_NoThread, WGS_LOADER_DEBUG);

static int GetDebugLevel(void)
{
    static NCBI_PARAM_TYPE(WGS_LOADER, DEBUG) s_Value;
    return s_Value.Get();
}


NCBI_PARAM_DECL(bool, WGS_LOADER, MASTER_DESCR);
NCBI_PARAM_DEF(bool, WGS_LOADER, MASTER_DESCR, true);

static bool GetMasterDescrParam(void)
{
    return NCBI_PARAM_TYPE(WGS_LOADER, MASTER_DESCR)::GetDefault();
}


NCBI_PARAM_DECL(size_t, WGS_LOADER, GC_SIZE);
NCBI_PARAM_DEF(size_t, WGS_LOADER, GC_SIZE, 100);

static size_t GetGCSize(void)
{
    return NCBI_PARAM_TYPE(WGS_LOADER, GC_SIZE)::GetDefault();
}


NCBI_PARAM_DECL(string, WGS_LOADER, VOL_PATH);
NCBI_PARAM_DEF(string, WGS_LOADER, VOL_PATH, "");

static string GetWGSVolPath(void)
{
    return NCBI_PARAM_TYPE(WGS_LOADER, VOL_PATH)::GetDefault();
}


NCBI_PARAM_DECL(bool, WGS_LOADER, RESOLVE_GIS);
NCBI_PARAM_DEF(bool, WGS_LOADER, RESOLVE_GIS, true);


static bool GetResolveGIsParam(void)
{
    return NCBI_PARAM_TYPE(WGS_LOADER, RESOLVE_GIS)::GetDefault();
}


NCBI_PARAM_DECL(bool, WGS_LOADER, RESOLVE_PROT_ACCS);
NCBI_PARAM_DEF(bool, WGS_LOADER, RESOLVE_PROT_ACCS, true);


static bool GetResolveProtAccsParam(void)
{
    static bool value =
        NCBI_PARAM_TYPE(WGS_LOADER, RESOLVE_PROT_ACCS)::GetDefault();
    return value;
}


NCBI_PARAM_DECL(bool, WGS_LOADER, SPLIT_QUALITY_GRAPH);
NCBI_PARAM_DEF(bool, WGS_LOADER, SPLIT_QUALITY_GRAPH, true);


static bool GetSplitQualityGraphParam(void)
{
    static bool value =
        NCBI_PARAM_TYPE(WGS_LOADER, SPLIT_QUALITY_GRAPH)::GetDefault();
    return value;
}


NCBI_PARAM_DECL(bool, WGS_LOADER, SPLIT_SEQUENCE);
NCBI_PARAM_DEF(bool, WGS_LOADER, SPLIT_SEQUENCE, false);


static bool GetSplitSequenceParam(void)
{
    static bool value =
        NCBI_PARAM_TYPE(WGS_LOADER, SPLIT_SEQUENCE)::GetDefault();
    return value;
}


/////////////////////////////////////////////////////////////////////////////
// CWGSBlobId
/////////////////////////////////////////////////////////////////////////////

CWGSBlobId::CWGSBlobId(CTempString str)
{
    FromString(str);
}


CWGSBlobId::CWGSBlobId(const CWGSFileInfo::SAccFileInfo& info)
    : m_WGSPrefix(info.file->GetWGSPrefix()),
      m_SeqType(info.seq_type),
      m_RowId(info.row_id)
{
}


CWGSBlobId::~CWGSBlobId(void)
{
}


string CWGSBlobId::ToString(void) const
{
    CNcbiOstrstream out;
    out << m_WGSPrefix << '.';
    if ( m_SeqType ) {
        out << m_SeqType;
    }
    out << m_RowId;
    return CNcbiOstrstreamToString(out);
}


void CWGSBlobId::FromString(CTempString str)
{
    SIZE_TYPE dot = str.rfind('.');
    if ( dot == NPOS ) {
        NCBI_THROW_FMT(CSraException, eOtherError,
                       "Bad CWGSBlobId: "<<str);
    }
    m_WGSPrefix = str.substr(0, dot);
    SIZE_TYPE pos = dot+1;
    if ( str[pos] == 'S' || str[pos] == 'P' ) {
        m_SeqType = str[pos++];
    }
    else {
        m_SeqType = '\0';
    }
    m_RowId = NStr::StringToNumeric<Uint8>(str.substr(dot+1));
}


bool CWGSBlobId::operator<(const CBlobId& id) const
{
    const CWGSBlobId& wgs2 = dynamic_cast<const CWGSBlobId&>(id);
    if ( int diff = NStr::CompareNocase(m_WGSPrefix, wgs2.m_WGSPrefix) ) {
        return diff < 0;
    }
    if ( m_SeqType != wgs2.m_SeqType ) {
        return m_SeqType < wgs2.m_SeqType;
    }
    return m_RowId < wgs2.m_RowId;
}


bool CWGSBlobId::operator==(const CBlobId& id) const
{
    const CWGSBlobId& wgs2 = dynamic_cast<const CWGSBlobId&>(id);
    return m_RowId == wgs2.m_RowId &&
        m_SeqType == wgs2.m_SeqType &&
        m_WGSPrefix == wgs2.m_WGSPrefix;
}


/////////////////////////////////////////////////////////////////////////////
// CWGSDataLoader_Impl
/////////////////////////////////////////////////////////////////////////////


CWGSDataLoader_Impl::CWGSDataLoader_Impl(
    const CWGSDataLoader::SLoaderParams& params)
    : m_WGSVolPath(params.m_WGSVolPath),
      m_FoundFiles(GetGCSize()),
      m_AddWGSMasterDescr(GetMasterDescrParam()),
      m_ResolveGIs(GetResolveGIsParam()),
      m_ResolveProtAccs(GetResolveProtAccsParam())
{
    if ( m_WGSVolPath.empty() && params.m_WGSFiles.empty() ) {
        m_WGSVolPath = GetWGSVolPath();
    }
    ITERATE (vector<string>, it, params.m_WGSFiles) {
        CRef<CWGSFileInfo> info(new CWGSFileInfo(*this, *it));
        if ( !m_FixedFiles.insert(TFixedFiles::value_type(info->GetWGSPrefix(),
                                                          info)).second ) {
            NCBI_THROW_FMT(CSraException, eOtherError,
                           "Duplicated fixed WGS prefix: "<<
                           info->GetWGSPrefix());
        }
    }
}


CWGSDataLoader_Impl::~CWGSDataLoader_Impl(void)
{
}


CWGSResolver& CWGSDataLoader_Impl::GetResolver(void)
{
    if ( !m_Resolver ) {
        CMutexGuard guard(m_Mutex);
        if ( !m_Resolver ) {
            m_Resolver = CWGSResolver::CreateResolver(m_Mgr);
        }
    }
    return *m_Resolver;
}


CConstRef<CWGSFileInfo> CWGSDataLoader_Impl::GetWGSFile(const string& prefix)
{
    if ( !m_FixedFiles.empty() ) {
        // no dynamic WGS accessions
        TFixedFiles::iterator it = m_FixedFiles.find(prefix);
        if ( it != m_FixedFiles.end() ) {
            return it->second;
        }
        return null;
    }
    CMutexGuard guard(m_Mutex);
    TFoundFiles::iterator it = m_FoundFiles.find(prefix);
    if ( it != m_FoundFiles.end() ) {
        return it->second;
    }
    try {
        CConstRef<CWGSFileInfo> info(new CWGSFileInfo(*this, prefix));
        m_FoundFiles[prefix] = info;
        return info;
    }
    catch ( CSraException& exc ) {
        if ( exc.GetErrCode() == exc.eNotFoundDb ||
             exc.GetErrCode() == exc.eProtectedDb ) {
            // no such WGS table
            return null;
        }
        throw;
    }
}


CWGSFileInfo::SAccFileInfo
CWGSDataLoader_Impl::GetFileInfoByGi(TGi gi)
{
    CWGSFileInfo::SAccFileInfo ret;
    if ( !m_FixedFiles.empty() ) {
        ITERATE ( TFixedFiles, it, m_FixedFiles ) {
            if ( it->second->FindGi(ret, gi) ) {
                if ( GetDebugLevel() >= 2 ) {
                    ERR_POST_X(3, "CWGSDataLoader: "
                               "Resolved gi "<<gi<<
                               " -> "<<ret.file->GetWGSPrefix());
                }
                return ret;
            }
        }
        if ( GetDebugLevel() >= 3 ) {
            ERR_POST_X(4, "CWGSDataLoader: "
                       "Failed to resolve gi "<<gi);
        }
        return ret;
    }
    {
        CMutexGuard guard(m_Mutex);
        ITERATE ( TFoundFiles, it, m_FoundFiles ) {
            if ( it->second->FindGi(ret, gi) ) {
                if ( GetDebugLevel() >= 2 ) {
                    ERR_POST_X(5, "CWGSDataLoader: "
                               "Resolved gi "<<gi<<
                               " -> "<<ret.file->GetWGSPrefix());
                }
                return ret;
            }
        }
    }
    CWGSResolver& resolver = GetResolver();
    CWGSResolver::TWGSPrefixes prefixes = resolver.GetPrefixes(gi);
    ITERATE ( CWGSResolver::TWGSPrefixes, it, prefixes ) {
        if ( CConstRef<CWGSFileInfo> file = GetWGSFile(*it) ) {
            if ( GetDebugLevel() >= 2 ) {
                ERR_POST_X(6, "CWGSDataLoader: "
                           "Resolved gi "<<gi<<
                           " -> "<<file->GetWGSPrefix());
            }
            if ( file->FindGi(ret, gi) ) {
                resolver.SetWGSPrefix(gi, prefixes, *it);
                return ret;
            }
        }
    }
    if ( !prefixes.empty() ) {
        resolver.SetNonWGS(gi, prefixes);
    }
    return ret;
}


CWGSFileInfo::SAccFileInfo
CWGSDataLoader_Impl::GetFileInfoByProtAcc(const string& acc)
{
    CWGSFileInfo::SAccFileInfo ret;
    if ( !m_FixedFiles.empty() ) {
        ITERATE ( TFixedFiles, it, m_FixedFiles ) {
            if ( it->second->FindProtAcc(ret, acc) ) {
                if ( GetDebugLevel() >= 2 ) {
                    ERR_POST_X(7, "CWGSDataLoader: "
                               "Resolved prot acc "<<acc<<
                               " -> "<<ret.file->GetWGSPrefix());
                }
                return ret;
            }
        }
        if ( GetDebugLevel() >= 3 ) {
            ERR_POST_X(8, "CWGSDataLoader: "
                       "Failed to resolve prot acc "<<acc);
        }
        return ret;
    }
    {
        CMutexGuard guard(m_Mutex);
        ITERATE ( TFoundFiles, it, m_FoundFiles ) {
            if ( it->second->FindProtAcc(ret, acc) ) {
                if ( GetDebugLevel() >= 2 ) {
                    ERR_POST_X(9, "CWGSDataLoader: "
                               "Resolved prot acc "<<acc<<
                               " -> "<<ret.file->GetWGSPrefix());
                }
                return ret;
            }
        }
    }
    CWGSResolver& resolver = GetResolver();
    CWGSResolver::TWGSPrefixes prefixes = resolver.GetPrefixes(acc);
    ITERATE ( CWGSResolver::TWGSPrefixes, it, prefixes ) {
        if ( CConstRef<CWGSFileInfo> file = GetWGSFile(*it) ) {
            if ( GetDebugLevel() >= 2 ) {
                ERR_POST_X(10, "CWGSDataLoader: "
                           "Resolved prot acc "<<acc<<
                           " -> "<<file->GetWGSPrefix());
            }
            if ( file->FindProtAcc(ret, acc) ) {
                resolver.SetWGSPrefix(acc, prefixes, *it);
                return ret;
            }
        }
    }
    if ( !prefixes.empty() ) {
        resolver.SetNonWGS(acc, prefixes);
    }
    return ret;
}


CWGSFileInfo::SAccFileInfo
CWGSDataLoader_Impl::GetFileInfoByAcc(const string& acc)
{
    CWGSFileInfo::SAccFileInfo ret;
    CSeq_id::EAccessionInfo type = CSeq_id::IdentifyAccession(acc);
    switch ( type & CSeq_id::eAcc_division_mask ) {
        // accepted accession types
    case CSeq_id::eAcc_wgs:
    case CSeq_id::eAcc_wgs_intermed:
    case CSeq_id::eAcc_tsa:
        break;
    default:
        return ret;
    }

    if ( (type & CSeq_id::fAcc_prot) && acc.size() <= 10 ) { // 3+5
        if ( m_ResolveProtAccs ) {
            ret = GetFileInfoByProtAcc(acc);
        }
        return ret;
    }

    const SIZE_TYPE prefix_len = 6;
    if ( acc.size() <= prefix_len ) {
        return ret;
    }
    string prefix = acc.substr(0, prefix_len);
    for ( SIZE_TYPE i = prefix_len-6; i < prefix_len-2; ++i ) {
        if ( !isalpha(acc[i]&0xff) ) {
            return ret;
        }
    }
    for ( SIZE_TYPE i = prefix_len-2; i < prefix_len; ++i ) {
        if ( !isdigit(acc[i]&0xff) ) {
            return ret;
        }
    }
    SIZE_TYPE row_pos = prefix_len;
    if ( acc[row_pos] == 'S' || acc[row_pos] == 'P' ) {
        ret.seq_type = acc[row_pos++];
    }
    if ( acc.size() <= row_pos ) {
        return ret;
    }
    ret.row_id = NStr::StringToNumeric<Uint8>(acc.substr(row_pos),
                                              NStr::fConvErr_NoThrow);
    if ( !ret.row_id ) {
        return ret;
    }
    if ( CConstRef<CWGSFileInfo> info = GetWGSFile(prefix) ) {
        SIZE_TYPE row_digits = acc.size() - row_pos;
        if ( info->m_WGSDb->GetIdRowDigits() == row_digits ) {
            ret.file = info;
            if ( !ret.IsValidRowId() ) {
                ret.file = 0;
            }
        }
    }
    return ret;
}


CWGSFileInfo::SAccFileInfo
CWGSDataLoader_Impl::GetFileInfo(const CSeq_id_Handle& idh)
{
    if ( m_ResolveGIs && idh.IsGi() ) {
        return GetFileInfoByGi(idh.GetGi());
    }
    switch ( idh.Which() ) { // shortcut
    case CSeq_id::e_not_set:
    case CSeq_id::e_Local:
    case CSeq_id::e_Gi:
    case CSeq_id::e_Gibbsq:
    case CSeq_id::e_Gibbmt:
    case CSeq_id::e_Giim:
    case CSeq_id::e_Patent:
    case CSeq_id::e_General:
    case CSeq_id::e_Pdb:
        return CWGSFileInfo::SAccFileInfo();
    default:
        break;
    }
    CConstRef<CSeq_id> id = idh.GetSeqId();
    const CTextseq_id* text_id = id->GetTextseq_Id();
    if ( !text_id ) {
        return CWGSFileInfo::SAccFileInfo();
    }
    CWGSFileInfo::SAccFileInfo ret;
    if ( text_id->IsSetAccession() ) {
        ret = GetFileInfoByAcc(text_id->GetAccession());
    }
    if ( !ret && text_id->IsSetName() ) {
        ret = GetFileInfoByAcc(text_id->GetName());
    }
    if ( !ret ) {
        return ret;
    }
    if ( text_id->IsSetVersion() ) {
        ret.has_version = true;
        ret.version = text_id->GetVersion();
        if ( !ret.file->IsCorrectVersion(ret) ) {
            ret.file = null;
        }
    }
    return ret;
}


CConstRef<CWGSFileInfo>
CWGSDataLoader_Impl::GetFileInfo(const CWGSBlobId& blob_id)
{
    return GetWGSFile(blob_id.m_WGSPrefix);
}


CRef<CWGSBlobId> CWGSDataLoader_Impl::GetBlobId(const CSeq_id_Handle& idh)
{
    // return blob-id of blob with sequence
    if ( CWGSFileInfo::SAccFileInfo info = GetFileInfo(idh) ) {
        return Ref(new CWGSBlobId(info));
    }
    return null;
}


CWGSSeqIterator
CWGSFileInfo::SAccFileInfo::GetContigIterator(void) const
{
    _ASSERT(IsContig() && row_id != 0);
    return CWGSSeqIterator(file->GetDb(), row_id,
                           CWGSSeqIterator::eIncludeWithdrawn);
}


CWGSScaffoldIterator
CWGSFileInfo::SAccFileInfo::GetScaffoldIterator(void) const
{
    _ASSERT(IsScaffold() && row_id != 0);
    return CWGSScaffoldIterator(file->GetDb(), row_id);
}


CWGSProteinIterator
CWGSFileInfo::SAccFileInfo::GetProteinIterator(void) const
{
    _ASSERT(IsProtein() && row_id != 0);
    return CWGSProteinIterator(file->GetDb(), row_id);
}


CWGSSeqIterator
CWGSFileInfo::GetContigIterator(const CWGSBlobId& blob_id) const
{
    _ASSERT(blob_id.m_SeqType == '\0');
    _ASSERT(blob_id.m_RowId);
    return CWGSSeqIterator(GetDb(), blob_id.m_RowId,
                           CWGSSeqIterator::eIncludeWithdrawn);
}


CWGSScaffoldIterator
CWGSFileInfo::GetScaffoldIterator(const CWGSBlobId& blob_id) const
{
    _ASSERT(blob_id.m_SeqType == 'S');
    _ASSERT(blob_id.m_RowId);
    return CWGSScaffoldIterator(GetDb(), blob_id.m_RowId);
}


CWGSProteinIterator
CWGSFileInfo::GetProteinIterator(const CWGSBlobId& blob_id) const
{
    _ASSERT(blob_id.m_SeqType == 'P');
    _ASSERT(blob_id.m_RowId);
    return CWGSProteinIterator(GetDb(), blob_id.m_RowId);
}


CTSE_LoadLock CWGSDataLoader_Impl::GetBlobById(CDataSource* data_source,
                                               const CWGSBlobId& blob_id)
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
CWGSDataLoader_Impl::GetRecords(CDataSource* data_source,
                                const CSeq_id_Handle& idh,
                                CDataLoader::EChoice choice)
{
    CDataLoader::TTSE_LockSet locks;
    if ( choice == CDataLoader::eExtAnnot ||
         choice == CDataLoader::eExtFeatures ||
         choice == CDataLoader::eExtAlign ||
         choice == CDataLoader::eExtGraph ||
         choice == CDataLoader::eOrphanAnnot ) {
        // WGS loader doesn't provide external annotations
        return locks;
    }
    // return blob-id of blob with annotations and possibly with sequence

    if ( CRef<CWGSBlobId> blob_id = GetBlobId(idh) ) {
        CDataLoader::TTSE_Lock lock = GetBlobById(data_source, *blob_id);
        if ( (lock->GetBlobState() & CBioseq_Handle::fState_no_data) &&
             (lock->GetBlobState() != CBioseq_Handle::fState_no_data) ) {
            NCBI_THROW2(CBlobStateException, eBlobStateError,
                        "blob state error for "+idh.AsString(),
                        lock->GetBlobState());
        }
        locks.insert(lock);
    }

    return locks;
}


void CWGSDataLoader_Impl::LoadBlob(const CWGSBlobId& blob_id,
                                   CTSE_LoadLock& load_lock)
{
    GetFileInfo(blob_id)->LoadBlob(blob_id, load_lock);
}


void CWGSDataLoader_Impl::LoadChunk(const CWGSBlobId& blob_id,
                                    CTSE_Chunk_Info& chunk_info)
{
    GetFileInfo(blob_id)->LoadChunk(blob_id, chunk_info);
}


void CWGSDataLoader_Impl::GetIds(const CSeq_id_Handle& idh, TIds& ids)
{
    CBioseq::TId ids2;
    if ( CWGSFileInfo::SAccFileInfo info = GetFileInfo(idh) ) {
        switch ( info.seq_type ) {
        case 'S':
            info.GetScaffoldIterator().GetIds(ids2);
            break;
        case 'P':
            info.GetProteinIterator().GetIds(ids2);
            break;
        default:
            info.GetContigIterator().GetIds(ids2);
            break;
        }
    }
    ITERATE ( CBioseq::TId, it2, ids2 ) {
        ids.push_back(CSeq_id_Handle::GetHandle(**it2));
    }
}


CSeq_id_Handle CWGSDataLoader_Impl::GetAccVer(const CSeq_id_Handle& idh)
{
    if ( CWGSFileInfo::SAccFileInfo info = GetFileInfo(idh) ) {
        CRef<CSeq_id> acc_id;
        switch ( info.seq_type ) {
        case 'S':
            if ( CWGSScaffoldIterator it = info.GetScaffoldIterator() ) {
                acc_id = it.GetAccSeq_id();
            }
            break;
        case 'P':
            if ( CWGSProteinIterator it = info.GetProteinIterator() ) {
                acc_id = it.GetAccSeq_id();
            }
            break;
        default:
            if ( CWGSSeqIterator it = info.GetContigIterator() ) {
                acc_id = it.GetAccSeq_id();
            }
            break;
        }
        if ( acc_id ) {
            return CSeq_id_Handle::GetHandle(*acc_id);
        }
    }
    return CSeq_id_Handle();
}


TGi CWGSDataLoader_Impl::GetGi(const CSeq_id_Handle& idh)
{
    if ( CWGSFileInfo::SAccFileInfo info = GetFileInfo(idh) ) {
        if ( info.IsContig() ) {
            if ( CWGSSeqIterator it = info.GetContigIterator() ) {
                if ( it.HasGi() ) {
                    return it.GetGi();
                }
            }
        }
    }
    return ZERO_GI;
}


int CWGSDataLoader_Impl::GetTaxId(const CSeq_id_Handle& idh)
{
    if ( CWGSFileInfo::SAccFileInfo info = GetFileInfo(idh) ) {
        if ( info.IsContig() ) {
            if ( CWGSSeqIterator it = info.GetContigIterator() ) {
                if ( it.HasTaxId() ) {
                    return it.GetTaxId();
                }
                return 0; // taxid is not defined
            }
        }
        else if ( info.IsValidRowId() ) {
            return 0; // taxid is not defined
        }
    }
    return -1; // sequence is unknown
}


TSeqPos CWGSDataLoader_Impl::GetSequenceLength(const CSeq_id_Handle& idh)
{
    if ( CWGSFileInfo::SAccFileInfo info = GetFileInfo(idh) ) {
        switch ( info.seq_type ) {
        case 'S':
            if ( CWGSScaffoldIterator it = info.GetScaffoldIterator() ) {
                return it.GetSeqLength();
            }
            break;
        case 'P':
            if ( CWGSProteinIterator it = info.GetProteinIterator() ) {
                return it.GetSeqLength();
            }
            break;
        default:
            if ( CWGSSeqIterator it = info.GetContigIterator() ) {
                return it.GetSeqLength();
            }
            break;
        }
    }
    return kInvalidSeqPos;
}


int CWGSDataLoader_Impl::GetSequenceHash(const CSeq_id_Handle& idh)
{
    if ( CWGSFileInfo::SAccFileInfo info = GetFileInfo(idh) ) {
        switch ( info.seq_type ) {
        case 'S':
            /*
            if ( CWGSScaffoldIterator it = info.GetScaffoldIterator() ) {
                return it.GetSeqHash();
            }
            */
            break;
        case 'P':
            /*
            if ( CWGSProteinIterator it = info.GetProteinIterator() ) {
                return it.GetSeqHash();
            }
            */
            break;
        default:
            if ( CWGSSeqIterator it = info.GetContigIterator() ) {
                return it.GetSeqHash();
            }
            break;
        }
    }
    return 0;
}


CSeq_inst::TMol CWGSDataLoader_Impl::GetSequenceType(const CSeq_id_Handle& idh)
{
    if ( GetBlobId(idh) ) {
        return CSeq_inst::eMol_na;
    }
    return CSeq_inst::eMol_not_set;
}


/////////////////////////////////////////////////////////////////////////////
// CWGSFileInfo
/////////////////////////////////////////////////////////////////////////////


CWGSFileInfo::CWGSFileInfo(CWGSDataLoader_Impl& impl,
                           CTempString prefix)
{
    try {
        x_Initialize(impl, prefix);
    }
    catch ( CSraException& exc ) {
        if ( GetDebugLevel() >= 1 ) {
            ERR_POST_X(1, "CWGSDataLoader: "
                       "Exception while opening WGS DB "<<prefix<<": "<<exc);
        }
        if ( exc.GetParam().find(prefix) == NPOS ) {
            exc.SetParam(exc.GetParam()+" acc="+string(prefix));
        }
        throw exc;
    }
    catch ( CException& exc ) {
        if ( GetDebugLevel() >= 1 ) {
            ERR_POST_X(1, "CWGSDataLoader: "
                       "Exception while opening WGS DB "<<prefix<<": "<<exc);
        }
        NCBI_RETHROW_FMT(exc, CSraException, eOtherError,
                         "CWGSDataLoader: exception while opening WGS DB "<<prefix);
    }
}


void CWGSFileInfo::x_Initialize(CWGSDataLoader_Impl& impl,
                                CTempString prefix)
{
    m_WGSDb = CWGSDb(impl.m_Mgr, prefix, impl.m_WGSVolPath);
    m_WGSPrefix = m_WGSDb->GetIdPrefixWithVersion();
    if ( GetDebugLevel() >= 1 ) {
        ERR_POST_X(2, "CWGSDataLoader: "
                   "Opened WGS DB "<<prefix<<" -> "<<
                   GetWGSPrefix()<<" "<<m_WGSDb.GetWGSPath());
    }
    if ( impl.GetAddWGSMasterDescr() ) {
        x_InitMasterDescr();
    }
}


void CWGSFileInfo::x_InitMasterDescr(void)
{
    if ( m_WGSDb.LoadMasterDescr() ) {
        // loaded descriptors from metadata
        return;
    }
    CRef<CSeq_id> id = m_WGSDb->GetMasterSeq_id();
    if ( !id ) {
        // no master sequence id
        return;
    }
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*id);
    CDataLoader* gb_loader =
        CObjectManager::GetInstance()->FindDataLoader("GBLOADER");
    if ( !gb_loader ) {
        // no GenBank loader found -> no way to load master record
        return;
    }
    CDataLoader::TTSE_LockSet locks =
        gb_loader->GetRecordsNoBlobState(idh, CDataLoader::eBioseqCore);
    ITERATE ( CDataLoader::TTSE_LockSet, it, locks ) {
        CConstRef<CBioseq_Info> bs_info = (*it)->FindMatchingBioseq(idh);
        if ( !bs_info ) {
            continue;
        }
        if ( bs_info->IsSetDescr() ) {
            m_WGSDb.SetMasterDescr(bs_info->GetDescr().Get());
        }
        break;
    }
}


bool CWGSFileInfo::SAccFileInfo::IsValidRowId(void) const
{
    if ( row_id == 0 ) {
        return false;
    }
    if ( IsScaffold() ) {
        return GetScaffoldIterator();
    }
    else if ( IsProtein() ) {
        return GetProteinIterator();
    }
    else {
        return GetContigIterator();
    }
}


bool CWGSFileInfo::IsCorrectVersion(const SAccFileInfo& info) const
{
    if ( info.row_id == 0 ) {
        return false;
    }
    if ( info.IsContig() ) {
        CWGSSeqIterator iter = info.GetContigIterator();
        return iter && info.version == iter.GetAccVersion();
    }
    else {
        // scaffolds and proteins can have only version 1
        return info.version == 1;
    }
}


bool CWGSFileInfo::FindGi(SAccFileInfo& info, TGi gi) const
{
    CWGSGiIterator it(m_WGSDb, gi);
    if ( it ) {
        info.file = this;
        info.row_id = it.GetRowId();
        info.seq_type = it.GetSeqType() == it.eProt? 'P': '\0';
        info.has_version = false;
        return true;
    }
    return false;
}


bool CWGSFileInfo::FindProtAcc(SAccFileInfo& info, const string& acc) const
{
    if ( TVDBRowId row_id = m_WGSDb.GetProtAccRowId(acc) ) {
        info.file = this;
        info.row_id = row_id;
        info.seq_type = 'P';
        info.has_version = false;
        return true;
    }
    return false;
}



static CSeq_id_Handle s_GetAccessId(const CWGSSeqIterator& it)
{
    if ( TGi gi = it.GetGi() ) {
        return CSeq_id_Handle::GetHandle(gi);
    }
    else if ( CRef<CSeq_id> id = it.GetAccSeq_id() ) {
        return CSeq_id_Handle::GetHandle(*id);
    }
    else if ( CRef<CSeq_id> id = it.GetGeneralSeq_id() ) {
        return CSeq_id_Handle::GetHandle(*id);
    }
    return CSeq_id_Handle();
}


void CWGSFileInfo::LoadBlob(const CWGSBlobId& blob_id,
                            CTSE_LoadLock& load_lock) const
{
    if ( !load_lock.IsLoaded() ) {
        NCBI_gb_state gb_state = 0;
        CRef<CSeq_entry> entry;
        vector<CRef<CTSE_Chunk_Info> > chunks;
        if ( blob_id.m_SeqType == 'S' ) {
            if ( CWGSScaffoldIterator it = GetScaffoldIterator(blob_id) ) {
                entry = it.GetSeq_entry();
            }
        }
        else if ( blob_id.m_SeqType == 'P' ) {
            if ( CWGSProteinIterator it = GetProteinIterator(blob_id) ) {
                gb_state = it.GetGBState();
                entry = it.GetSeq_entry();
            }
        }
        else {
            if ( CWGSSeqIterator it = GetContigIterator(blob_id) ) {
                gb_state = it.GetGBState();
                CWGSSeqIterator::TFlags flags = it.fDefaultFlags;
                if ( GetSplitQualityGraphParam() ) {
                    flags &= ~it.fQualityGraph;
                    if ( it.CanHaveQualityGraph() ) {
                        CRef<CTSE_Chunk_Info> chunk
                            (new CTSE_Chunk_Info(kChunkId_QualityGraph));
                        CSeq_id_Handle id = s_GetAccessId(it);
                        chunk->x_AddAnnotPlace(id);
                        chunk->x_AddAnnotType(CAnnotName(it.GetQualityAnnotName()),
                                              SAnnotTypeSelector(CSeq_annot::C_Data::e_Graph),
                                              id, CRange<TSeqPos>(0, it.GetSeqLength()));
                        chunks.push_back(chunk);
                    }
                }
                if ( GetSplitSequenceParam() ) {
                }
                entry = it.GetSeq_entry(flags);
            }
        }
        CBioseq_Handle::TBioseqStateFlags state = 0;
        if ( gb_state ) {
            switch ( gb_state ) {
            case 1:
                state |= CBioseq_Handle::fState_suppress_perm;
                break;
            case 2:
                state |= CBioseq_Handle::fState_dead;
                break;
            case 3:
                state |= (CBioseq_Handle::fState_withdrawn |
                          CBioseq_Handle::fState_no_data);
                break;
            default:
                break;
            }
        }
        if ( !entry ) {
            state |= CBioseq_Handle::fState_no_data;
        }
        if ( state ) {
            load_lock->SetBlobState(state);
        }
        if ( entry ) {
            load_lock->SetSeq_entry(*entry);
            NON_CONST_ITERATE ( vector<CRef<CTSE_Chunk_Info> >, it, chunks ) {
                load_lock->GetSplitInfo().AddChunk(**it);
            }
        }
    }
}


void CWGSFileInfo::LoadChunk(const CWGSBlobId& blob_id,
                             CTSE_Chunk_Info& chunk_info) const
{
    if ( chunk_info.GetChunkId() == kChunkId_QualityGraph ) {
        if ( blob_id.m_SeqType == '\0' ) {
            CWGSSeqIterator it = GetContigIterator(blob_id);
            CTSE_Chunk_Info::TPlace place(s_GetAccessId(it), 0);
            CBioseq::TAnnot annot_set;
            it.GetQualityAnnot(annot_set);
            ITERATE ( CBioseq::TAnnot, it, annot_set ) {
                chunk_info.x_LoadAnnot(place, **it);
            }
            chunk_info.SetLoaded();
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
