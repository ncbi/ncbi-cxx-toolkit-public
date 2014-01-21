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
#include <sra/data_loaders/wgs/wgsloader.hpp>
#include <sra/data_loaders/wgs/impl/wgsloader_impl.hpp>

#include <util/sequtil/sequtil_manip.hpp>

#include <algorithm>
#include <cmath>
#include <sra/error_codes.hpp>

BEGIN_NCBI_SCOPE

#define NCBI_USE_ERRCODE_X   WGSLoader
NCBI_DEFINE_ERR_SUBCODE_X(10);

BEGIN_SCOPE(objects)

class CDataLoader;

static const int kTSEId = 1;
static const int kMainChunkId = -1;

NCBI_PARAM_DECL(int, WGS_LOADER, DEBUG);
NCBI_PARAM_DEF_EX(int, WGS_LOADER, DEBUG, 0,
                  eParam_NoThread, WGS_LOADER_DEBUG);

static int GetDebugLevel(void)
{
    static NCBI_PARAM_TYPE(WGS_LOADER, DEBUG) s_Value;
    return s_Value.Get();
}


NCBI_PARAM_DECL(bool, WGS_LOADER, MASTER_DESCR);
NCBI_PARAM_DEF_EX(bool, WGS_LOADER, MASTER_DESCR, true,
                  eParam_NoThread, WGS_LOADER_MASTER_DESCR);

static bool GetMasterDescrParam(void)
{
    static NCBI_PARAM_TYPE(WGS_LOADER, MASTER_DESCR) s_Value;
    return s_Value.Get();
}


NCBI_PARAM_DECL(size_t, WGS_LOADER, GC_SIZE);
NCBI_PARAM_DEF_EX(size_t, WGS_LOADER, GC_SIZE, 10,
                  eParam_NoThread, WGS_LOADER_GC_SIZE);

static size_t GetGCSize(void)
{
    static NCBI_PARAM_TYPE(WGS_LOADER, GC_SIZE) s_Value;
    return s_Value.Get();
}


/////////////////////////////////////////////////////////////////////////////
// CWGSBlobId
/////////////////////////////////////////////////////////////////////////////

CWGSBlobId::CWGSBlobId(CTempString str)
{
    FromString(str);
}


CWGSBlobId::CWGSBlobId(CTempString prefix, bool scaffold, Uint8 row_id)
    : m_WGSPrefix(prefix),
      m_Scaffold(scaffold),
      m_RowId(row_id)
{
}


CWGSBlobId::~CWGSBlobId(void)
{
}


string CWGSBlobId::ToString(void) const
{
    CNcbiOstrstream out;
    out << m_WGSPrefix << '.' << (m_Scaffold? "S": "") << m_RowId;
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
    if ( str[pos] == 'S' ) {
        m_Scaffold = true;
        ++pos;
    }
    else {
        m_Scaffold = false;
    }
    m_RowId = NStr::StringToNumeric<Uint8>(str.substr(dot+1));
}


bool CWGSBlobId::operator<(const CBlobId& id) const
{
    const CWGSBlobId& wgs2 = dynamic_cast<const CWGSBlobId&>(id);
    if ( int diff = NStr::CompareNocase(m_WGSPrefix, wgs2.m_WGSPrefix) ) {
        return diff < 0;
    }
    if ( m_Scaffold != wgs2.m_Scaffold ) {
        return m_Scaffold < wgs2.m_Scaffold;
    }
    return m_RowId < wgs2.m_RowId;
}


bool CWGSBlobId::operator==(const CBlobId& id) const
{
    const CWGSBlobId& wgs2 = dynamic_cast<const CWGSBlobId&>(id);
    return m_RowId == wgs2.m_RowId &&
        m_Scaffold == wgs2.m_Scaffold &&
        m_WGSPrefix == wgs2.m_WGSPrefix;
}


/////////////////////////////////////////////////////////////////////////////
// CWGSDataLoader_Impl
/////////////////////////////////////////////////////////////////////////////


CWGSDataLoader_Impl::CWGSDataLoader_Impl(
    const CWGSDataLoader::SLoaderParams& params)
    : m_WGSVolPath(params.m_WGSVolPath),
      m_FoundFiles(GetGCSize())
{
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


CRef<CWGSFileInfo> CWGSDataLoader_Impl::GetWGSFile(const string& prefix)
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
        CRef<CWGSFileInfo> info(new CWGSFileInfo(*this, prefix));
        m_FoundFiles[prefix] = info;
        return info;
    }
    catch ( CSraException& exc ) {
        if ( exc.GetErrCode() == CSraException::eNotFoundDb ) {
            // no such WGS table
            return null;
        }
        throw;
    }
}


CRef<CWGSFileInfo>
CWGSDataLoader_Impl::GetFileInfo(const string& acc,
                                 bool* scaffold_ptr,
                                 Uint8* row_id_ptr)
{
    CSeq_id::EAccessionInfo type = CSeq_id::IdentifyAccession(acc);
    switch ( type & CSeq_id::eAcc_division_mask ) {
        // accepted accession types
    case CSeq_id::eAcc_wgs:
    case CSeq_id::eAcc_wgs_intermed:
    case CSeq_id::eAcc_tsa:
        break;
    default:
        return null;
    }
    SIZE_TYPE prefix_len = NStr::StartsWith(acc, "NZ_")? 9: 6;
    if ( acc.size() <= prefix_len ) {
        return null;
    }
    string prefix = acc.substr(0, prefix_len);
    SIZE_TYPE row_pos = prefix_len;
    bool scaffold = false;
    if ( acc[row_pos] == 'S' ) {
        scaffold = true;
        ++row_pos;
    }
    if ( scaffold_ptr ) {
        *scaffold_ptr = scaffold;
    }
    if ( acc.size() <= row_pos ) {
        return null;
    }
    Uint8 row_id = NStr::StringToNumeric<Uint8>(acc.substr(row_pos),
                                                NStr::fConvErr_NoThrow);
    if ( !row_id && errno ) {
        return null;
    }
    if ( row_id_ptr ) {
        *row_id_ptr = row_id;
    }
    if ( CRef<CWGSFileInfo> info = GetWGSFile(prefix) ) {
        SIZE_TYPE row_digits = acc.size() - row_pos;
        if ( info->IsValidRowId(scaffold, row_id, row_digits) ) {
            return info;
        }
    }
    return null;
}


CRef<CWGSFileInfo>
CWGSDataLoader_Impl::GetFileInfo(const CSeq_id_Handle& idh,
                                 bool* scaffold_ptr,
                                 Uint8* read_id_ptr)
{
    switch ( idh.Which() ) { // shortcut
    case CSeq_id::e_not_set:
    case CSeq_id::e_Local:
    case CSeq_id::e_Gibbsq:
    case CSeq_id::e_Gibbmt:
    case CSeq_id::e_Giim:
    case CSeq_id::e_Patent:
    case CSeq_id::e_General:
    case CSeq_id::e_Gi:
    case CSeq_id::e_Pdb:
        return null;
    default:
        break;
    }
    CConstRef<CSeq_id> id = idh.GetSeqId();
    const CTextseq_id* text_id = id->GetTextseq_Id();
    if ( !text_id ) {
        return null;
    }
    if ( false && text_id->IsSetVersion() && text_id->GetVersion() != 1 ) {
        return null;
    }
    return GetFileInfo(text_id->GetAccession(), scaffold_ptr, read_id_ptr);
}


CRef<CWGSFileInfo> CWGSDataLoader_Impl::GetFileInfo(const CWGSBlobId& blob_id)
{
    return GetWGSFile(blob_id.m_WGSPrefix);
}


CRef<CWGSBlobId> CWGSDataLoader_Impl::GetBlobId(const CSeq_id_Handle& idh)
{
    // return blob-id of blob with sequence
    bool scaffold;
    Uint8 row_id;
    if ( CRef<CWGSFileInfo> info = GetFileInfo(idh, &scaffold, &row_id) ) {
        return Ref(new CWGSBlobId(info->GetWGSPrefix(), scaffold, row_id));
    }
    return null;
}


CWGSSeqIterator
CWGSDataLoader_Impl::GetSeqIterator(const CSeq_id_Handle& idh,
                                    CWGSScaffoldIterator* iter2_ptr)
{
    // return blob-id of blob with sequence
    bool scaffold;
    Uint8 row_id;
    if ( CRef<CWGSFileInfo> info = GetFileInfo(idh, &scaffold, &row_id) ) {
        if ( scaffold ) {
            if ( iter2_ptr ) {
                *iter2_ptr = CWGSScaffoldIterator(info->GetDb(), row_id);
            }
            return CWGSSeqIterator();
        }
        else {
            return CWGSSeqIterator(info->GetDb(), row_id);
        }
    }
    return CWGSSeqIterator();
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
    CWGSScaffoldIterator it2;
    if ( CWGSSeqIterator it = GetSeqIterator(idh, &it2) ) {
        it.GetIds(ids2);
    }
    else if ( it2 ) {
        it2.GetIds(ids2);
    }
    ITERATE ( CBioseq::TId, it2, ids2 ) {
        ids.push_back(CSeq_id_Handle::GetHandle(**it2));
    }
}


CSeq_id_Handle CWGSDataLoader_Impl::GetAccVer(const CSeq_id_Handle& idh)
{
    CWGSScaffoldIterator it2;
    if ( CWGSSeqIterator it = GetSeqIterator(idh, &it2) ) {
        if ( CRef<CSeq_id> acc_id = it.GetAccSeq_id() ) {
            return CSeq_id_Handle::GetHandle(*acc_id);
        }
    }
    else if ( it2 ) {
        if ( CRef<CSeq_id> acc_id = it2.GetAccSeq_id() ) {
            return CSeq_id_Handle::GetHandle(*acc_id);
        }
    }
    return CSeq_id_Handle();
}


int CWGSDataLoader_Impl::GetGi(const CSeq_id_Handle& idh)
{
    if ( CWGSSeqIterator it = GetSeqIterator(idh) ) {
        if ( it.HasGi() ) {
            return it.GetGi();
        }
    }
    return 0;
}


int CWGSDataLoader_Impl::GetTaxId(const CSeq_id_Handle& idh)
{
    if ( CWGSSeqIterator it = GetSeqIterator(idh) ) {
        if ( it.HasTaxId() ) {
            return it.GetTaxId();
        }
        return 0; // taxid is not defined
    }
    return -1; // sequence is unknown
}


TSeqPos CWGSDataLoader_Impl::GetSequenceLength(const CSeq_id_Handle& idh)
{
    CWGSScaffoldIterator it2;
    if ( CWGSSeqIterator it = GetSeqIterator(idh, &it2) ) {
        return it.GetSeqLength();
    }
    else if ( it2 ) {
        return it2.GetSeqLength();
    }
    return kInvalidSeqPos;
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
    catch ( CException& exc ) {
        if ( GetDebugLevel() >= 1 ) {
            ERR_POST_X(1, "Exception while opeining WGS DB "<<prefix<<": "<<exc);
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
    for ( int i = 0; i < 2; ++i ) {
        m_FirstBadRowId[i] = 0;
    }
    if ( GetDebugLevel() >= 1 ) {
        ERR_POST_X(1, "Opened WGS DB "<<prefix<<" -> "<<
                   GetWGSPrefix()<<" "<<m_WGSDb.GetWGSPath());
    }
    x_InitMasterDescr();
}


void CWGSFileInfo::x_InitMasterDescr(void)
{
    if ( !GetMasterDescrParam() ) {
        return;
    }
    CRef<CSeq_id> id = m_WGSDb->GetMasterSeq_id();
    if ( !id ) {
        return;
    }
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*id);
    CDataLoader* gb_loader =
        CObjectManager::GetInstance()->FindDataLoader("GBLOADER");
    if ( !gb_loader ) {
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
            CWGSDb_Impl::TMasterDescr master_descr;
            const CSeq_descr::Tdata& descr = bs_info->GetDescr().Get();
            ITERATE ( CSeq_descr::Tdata, it, descr ) {
                const CSeqdesc& desc = **it;
                if ( desc.Which() == CSeqdesc::e_Pub ||
                     desc.Which() == CSeqdesc::e_Comment ) {
                    master_descr.push_back(*it);
                }
                else if ( desc.Which() == CSeqdesc::e_User ) {
                    const CObject_id& type = desc.GetUser().GetType();
                    if ( type.Which() == CObject_id::e_Str ) {
                        const string& name = type.GetStr();
                        if ( name == "DBLink" ||
                             name == "GenomeProjectsDB" ||
                             name == "StructuredComment" ||
                             name == "FeatureFetchPolicy" ) {
                            master_descr.push_back(*it);
                        }
                    }
                }
            }
            m_WGSDb->SetMasterDescr(master_descr);
        }
        break;
    }
}


bool CWGSFileInfo::IsValidRowId(bool scaffold,
                                Uint8 row_id,
                                SIZE_TYPE row_digits)
{
    if ( row_id == 0 || m_WGSDb->GetIdRowDigits() != row_digits ) {
        return false;
    }
    Uint8 first_bad_row_id;
    {{
        CMutexGuard guard(GetMutex());
        first_bad_row_id = m_FirstBadRowId[scaffold];
        if ( first_bad_row_id == 0 ) {
            if ( scaffold ) {
                first_bad_row_id = CWGSScaffoldIterator(m_WGSDb).GetLastRowId()+1;
            }
            else {
                first_bad_row_id = CWGSSeqIterator(m_WGSDb).GetLastRowId()+1;
            }
            m_FirstBadRowId[scaffold] = first_bad_row_id;
        }
    }}
    return row_id < first_bad_row_id;
}


void CWGSFileInfo::LoadBlob(const CWGSBlobId& blob_id,
                            CTSE_LoadLock& load_lock)
{
    if ( !load_lock.IsLoaded() ) {
        CRef<CBioseq> seq;
        if ( blob_id.m_Scaffold ) {
            CWGSScaffoldIterator it(GetDb(), blob_id.m_RowId);
            seq = it.GetBioseq();
        }
        else {
            CWGSSeqIterator it(GetDb(), blob_id.m_RowId,
                               CWGSSeqIterator::eIncludeWithdrawn);
            if ( it ) {
                CBioseq_Handle::TBioseqStateFlags state = 0;
                if ( NCBI_gb_state gb_state = it.GetGBState() ) {
                    if ( gb_state == 1 ) {
                        state |= CBioseq_Handle::fState_suppress_perm;
                    }
                    if ( gb_state == 2 ) {
                        state |= CBioseq_Handle::fState_dead;
                    }
                    if ( gb_state == 3 ) {
                        state |= CBioseq_Handle::fState_withdrawn;
                        state |= CBioseq_Handle::fState_no_data;
                    }
                }
                load_lock->SetBlobState(state);
                if ( !(state & CBioseq_Handle::fState_no_data) ) {
                    seq = it.GetBioseq();
                }
            }
            else {
                load_lock->SetBlobState(CBioseq_Handle::fState_no_data);
            }
        }
        if ( seq ) {
            CRef<CSeq_entry> entry(new CSeq_entry);
            entry->SetSeq(*seq);
            load_lock->SetSeq_entry(*entry);
        }
    }
}


void CWGSFileInfo::LoadChunk(const CWGSBlobId& blob_id,
                             CTSE_Chunk_Info& chunk_info)
{
}


END_SCOPE(objects)
END_NCBI_SCOPE
