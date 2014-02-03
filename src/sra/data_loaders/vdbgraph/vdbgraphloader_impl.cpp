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
 * File Description: data loader for VDB graph data
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/general/general__.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqres/seqres__.hpp>

#include <objmgr/annot_selector.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/tse_loadlock.hpp>
#include <objmgr/impl/tse_split_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/data_loader_factory.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>

#include <sra/readers/sra/graphread.hpp>
#include <sra/data_loaders/vdbgraph/vdbgraphloader.hpp>
#include <sra/data_loaders/vdbgraph/impl/vdbgraphloader_impl.hpp>
#include <sra/error_codes.hpp>

BEGIN_NCBI_SCOPE

#define NCBI_USE_ERRCODE_X   VDBGraphLoader
NCBI_DEFINE_ERR_SUBCODE_X(6);

class CObject;

BEGIN_SCOPE(objects)

class CDataLoader;

#define OVERVIEW_NAME_SUFFIX "@@5000"
#define MID_ZOOM_NAME_SUFFIX "@@100"

static const TSeqPos kOverviewChunkSize = 20000*5000;
static const TSeqPos kMidZoomChunkSize = 20000*100;
static const TSeqPos kMainChunkSize = 100000;

static const size_t kOverviewChunkIdAdd = 0;
static const size_t kMidZoomChunkIdAdd = 1;
static const size_t kMainGraphChunkIdAdd = 2;
static const size_t kMainTableChunkIdAdd = 3;
static const size_t kChunkIdMul = 4;

static const int kTSEId = 1;

NCBI_PARAM_DECL(int, VDBGRAPH_LOADER, DEBUG);
NCBI_PARAM_DEF_EX(int, VDBGRAPH_LOADER, DEBUG, 0,
                  eParam_NoThread, VDBGRAPH_LOADER_DEBUG);

static int GetDebugLevel(void)
{
    static NCBI_PARAM_TYPE(VDBGRAPH_LOADER, DEBUG) s_Value;
    return s_Value.Get();
}


NCBI_PARAM_DECL(size_t, VDBGRAPH_LOADER, GC_SIZE);
NCBI_PARAM_DEF_EX(size_t, VDBGRAPH_LOADER, GC_SIZE, 10,
                  eParam_NoThread, VDBGRAPH_LOADER_GC_SIZE);

static size_t GetGCSize(void)
{
    static NCBI_PARAM_TYPE(VDBGRAPH_LOADER, GC_SIZE) s_Value;
    return s_Value.Get();
}


NCBI_PARAM_DECL(size_t, VDBGRAPH_LOADER, MISSING_GC_SIZE);
NCBI_PARAM_DEF_EX(size_t, VDBGRAPH_LOADER, MISSING_GC_SIZE, 10000,
                  eParam_NoThread, VDBGRAPH_LOADER_MISSING_GC_SIZE);

static size_t GetMissingGCSize(void)
{
    static NCBI_PARAM_TYPE(VDBGRAPH_LOADER, MISSING_GC_SIZE) s_Value;
    return s_Value.Get();
}


NCBI_PARAM_DECL(int, VDBGRAPH_LOADER, USE_TABLE);
NCBI_PARAM_DEF_EX(int, VDBGRAPH_LOADER, USE_TABLE, 2,
                  eParam_NoThread, VDBGRAPH_LOADER_USE_TABLE);

static int GetUseTable(void)
{
    static NCBI_PARAM_TYPE(VDBGRAPH_LOADER, USE_TABLE) s_Value;
    return s_Value.Get();
}


NCBI_PARAM_DECL(bool, VDBGRAPH_LOADER, DISABLE_ZOOM);
NCBI_PARAM_DEF_EX(bool, VDBGRAPH_LOADER, DISABLE_ZOOM, false,
                  eParam_NoThread, VDBGRAPH_LOADER_DISABLE_ZOOM);

static bool GetDisabledZoom(void)
{
    static NCBI_PARAM_TYPE(VDBGRAPH_LOADER, DISABLE_ZOOM) s_Value;
    return s_Value.Get();
}


static bool GetEnabledOverview(void)
{
    return !GetDisabledZoom();
}


static bool GetEnabledMidZoom(void)
{
    return !GetDisabledZoom();
}


/////////////////////////////////////////////////////////////////////////////
// CVDBGraphBlobId
/////////////////////////////////////////////////////////////////////////////

class CVDBGraphBlobId : public CBlobId
{
public:
    CVDBGraphBlobId(const string& file, const CSeq_id_Handle& id);
    ~CVDBGraphBlobId(void);

    string m_VDBFile;
    CSeq_id_Handle m_SeqId;
    CRef<CVDBGraphDataLoader_Impl::SVDBFileInfo> m_FileInfo;

    string ToString(void) const;
    CVDBGraphBlobId(const string& str);

    bool operator<(const CBlobId& id) const;
    bool operator==(const CBlobId& id) const;
};


CVDBGraphBlobId::CVDBGraphBlobId(const string& file, const CSeq_id_Handle& id)
    : m_VDBFile(file),
      m_SeqId(id)
{
}


CVDBGraphBlobId::~CVDBGraphBlobId(void)
{
}


string CVDBGraphBlobId::ToString(void) const
{
    CNcbiOstrstream out;
    out << m_VDBFile << '\0' << m_SeqId;
    return CNcbiOstrstreamToString(out);
}


CVDBGraphBlobId::CVDBGraphBlobId(const string& str)
{
    SIZE_TYPE pos1 = str.find('\0');
    m_VDBFile = str.substr(0, pos1);
    m_SeqId = CSeq_id_Handle::GetHandle(str.substr(pos1+1));
}


bool CVDBGraphBlobId::operator<(const CBlobId& id) const
{
    const CVDBGraphBlobId& sra2 = dynamic_cast<const CVDBGraphBlobId&>(id);
    return m_SeqId < sra2.m_SeqId ||
        (m_SeqId == sra2.m_SeqId && m_VDBFile < sra2.m_VDBFile);
}


bool CVDBGraphBlobId::operator==(const CBlobId& id) const
{
    const CVDBGraphBlobId& sra2 = dynamic_cast<const CVDBGraphBlobId&>(id);
    return m_SeqId == sra2.m_SeqId && m_VDBFile == sra2.m_VDBFile;
}


/////////////////////////////////////////////////////////////////////////////
// CVDBGraphDataLoader_Impl
/////////////////////////////////////////////////////////////////////////////


CVDBGraphDataLoader_Impl::CVDBGraphDataLoader_Impl(const TVDBFiles& vdb_files)
    : m_AutoFileMap(GetGCSize()),
      m_MissingFileSet(GetMissingGCSize())
{
    ITERATE ( TVDBFiles, it, vdb_files ) {
        if ( GetDebugLevel() >= 2 ) {
            LOG_POST_X(1, "CVDBGraphDataLoader: opening explict file "<<*it);
        }
        CRef<SVDBFileInfo> info(new SVDBFileInfo);
        info->m_VDBFile = *it;
        info->m_BaseAnnotName = CDirEntry(*it).GetName();
        info->m_VDB = CVDBGraphDb(m_Mgr, *it);
        m_FixedFileMap[*it] = info;
        for ( CVDBGraphSeqIterator seq_it(info->m_VDB); seq_it; ++seq_it ) {
            m_SeqIdIndex.insert
                (TSeqIdIndex::value_type(seq_it.GetSeq_id_Handle(), info));
        }
    }
}


CVDBGraphDataLoader_Impl::~CVDBGraphDataLoader_Impl(void)
{
}


bool CVDBGraphDataLoader_Impl::SVDBFileInfo::ContainsAnnotsFor(const CSeq_id_Handle& id) const
{
    return CVDBGraphSeqIterator(m_VDB, id);
}


string CVDBGraphDataLoader_Impl::SVDBFileInfo::GetMainAnnotName(void) const
{
    return m_BaseAnnotName;
}


string CVDBGraphDataLoader_Impl::SVDBFileInfo::GetOverviewAnnotName(void) const
{
    return m_BaseAnnotName+OVERVIEW_NAME_SUFFIX;
}


string CVDBGraphDataLoader_Impl::SVDBFileInfo::GetMidZoomAnnotName(void) const
{
    return m_BaseAnnotName+MID_ZOOM_NAME_SUFFIX;
}


CVDBGraphDataLoader_Impl::TAnnotNames
CVDBGraphDataLoader_Impl::GetPossibleAnnotNames(void) const
{
    TAnnotNames names;
    ITERATE ( TFixedFileMap, it, m_FixedFileMap ) {
        const SVDBFileInfo& info = *it->second;
        names.push_back(CAnnotName(info.GetMainAnnotName()));
        names.push_back(CAnnotName(info.GetMidZoomAnnotName()));
        names.push_back(CAnnotName(info.GetOverviewAnnotName()));
    }
    sort(names.begin(), names.end());
    names.erase(unique(names.begin(), names.end()), names.end());
    return names;
}


CDataLoader::TBlobId
CVDBGraphDataLoader_Impl::GetBlobId(const CSeq_id_Handle& /*idh*/)
{
    // no blobs with sequence
    return CDataLoader::TBlobId();
}


CDataLoader::TBlobId
CVDBGraphDataLoader_Impl::GetBlobIdFromString(const string& str) const
{
    return CDataLoader::TBlobId(new CVDBGraphBlobId(str));
}


CDataLoader::TTSE_Lock
CVDBGraphDataLoader_Impl::GetBlobById(CDataSource* ds,
                                      const CDataLoader::TBlobId& blob_id0)
{
    CTSE_LoadLock load_lock = ds->GetTSE_LoadLock(blob_id0);
    if ( !load_lock.IsLoaded() ) {
        const CVDBGraphBlobId& blob_id =
            dynamic_cast<const CVDBGraphBlobId&>(*blob_id0);
        if ( 1 ) {
            LoadSplitEntry(*load_lock, blob_id);
        }
        else {
            load_lock->SetSeq_entry(*LoadFullEntry(blob_id));
        }
        load_lock.SetLoaded();
    }
    return load_lock;
}


CDataLoader::TTSE_LockSet
CVDBGraphDataLoader_Impl::GetRecords(CDataSource* ds,
                                     const CSeq_id_Handle& id,
                                     CDataLoader::EChoice choice)
{
    TTSE_LockSet locks;
    if ( choice == CDataLoader::eOrphanAnnot ||
         choice == CDataLoader::eAll ) {
        for ( TSeqIdIndex::iterator it = m_SeqIdIndex.lower_bound(id);
              it != m_SeqIdIndex.end() && it->first == id; ++it ) {
            TBlobId blob_id(new CVDBGraphBlobId(it->second->m_VDBFile, id));
            locks.insert(GetBlobById(ds, blob_id));
        }
    }
    return locks;
}


CDataLoader::TTSE_LockSet
CVDBGraphDataLoader_Impl::GetOrphanAnnotRecords(CDataSource* ds,
                                                const CSeq_id_Handle& id,
                                                const SAnnotSelector* sel)
{
    TTSE_LockSet locks;
    // explicitly specified files
    for ( TSeqIdIndex::iterator it = m_SeqIdIndex.lower_bound(id);
          it != m_SeqIdIndex.end() && it->first == id; ++it ) {
        if ( !sel || sel->IsIncludedNamedAnnotAccession(it->second->m_BaseAnnotName) ) {
            TBlobId blob_id(new CVDBGraphBlobId(it->second->m_VDBFile, id));
            locks.insert(GetBlobById(ds, blob_id));
        }
    }
    // implicitly load NA accessions
    if ( sel && sel->IsIncludedAnyNamedAnnotAccession() ) {
        const SAnnotSelector::TNamedAnnotAccessions& accs =
            sel->GetNamedAnnotAccessions();
        if ( m_AutoFileMap.get_size_limit() < accs.size() ) {
            // increase VDB cache size
            m_AutoFileMap.set_size_limit(accs.size()+GetGCSize());
        }
        if ( m_MissingFileSet.get_size_limit() < accs.size() ) {
            // increase VDB cache size
            m_MissingFileSet.set_size_limit(accs.size()+GetMissingGCSize());
        }
        ITERATE ( SAnnotSelector::TNamedAnnotAccessions, it, accs ) {
            if ( 1 ) {
                TBlobId blob_id(new CVDBGraphBlobId(it->first, id));
                if ( CTSE_LoadLock lock = ds->GetTSE_LoadLockIfLoaded(blob_id) ) {
                    locks.insert(GetBlobById(ds, blob_id));
                    continue;
                }
            }
            SVDBFileInfo* file = x_GetNAFileInfo(it->first);
            if ( file && file->ContainsAnnotsFor(id) ) {
                TBlobId blob_id(new CVDBGraphBlobId(file->m_VDBFile, id));
                locks.insert(GetBlobById(ds, blob_id));
            }
        }
    }
    return locks;
}


CRef<CSeq_entry>
CVDBGraphDataLoader_Impl::LoadFullEntry(const CVDBGraphBlobId& blob_id)
{
    if ( GetDebugLevel() >= 5 ) {
        LOG_POST_X(4, "CVDBGraphDataLoader: "
                   "loading full entry for "<<blob_id.m_SeqId);
    }
    CRef<SVDBFileInfo> info_ref = x_GetFileInfo(blob_id.m_VDBFile);
    SVDBFileInfo& info = *info_ref;
    CVDBGraphSeqIterator it(info.m_VDB, blob_id.m_SeqId);
    if ( !it ) {
        return null;
    }
    CRef<CSeq_entry> entry(new CSeq_entry);
    entry->SetSet().SetSeq_set();
    CRange<TSeqPos> range = CRange<TSeqPos>::GetWhole();
    CBioseq_set::TAnnot& dst = entry->SetSet().SetAnnot();
    CVDBGraphSeqIterator::TContentFlags overview_flags = it.fGraphQAll;
    CVDBGraphSeqIterator::TContentFlags mid_zoom_flags = it.fGraphZoomQAll;
    CVDBGraphSeqIterator::TContentFlags main_flags = it.fGraphMain;
    if ( GetUseTable() == 2 ||
         (GetUseTable() == 1 && it.SeqTableIsSmaller(range)) ) {
        main_flags |= it.fGraphMainAsTable;
    }
    if ( GetEnabledOverview() ) {
        dst.push_back(it.GetAnnot(range,
                                  info.GetMainAnnotName(),
                                  overview_flags));
    }
    if ( GetEnabledMidZoom() && info.m_VDB->HasMidZoomGraphs() ) {
        dst.push_back(it.GetAnnot(range,
                                  info.GetMainAnnotName(),
                                  mid_zoom_flags));
    }
    dst.push_back(it.GetAnnot(range,
                              info.GetMainAnnotName(),
                              main_flags));
    return entry;
}


void CVDBGraphDataLoader_Impl::LoadSplitEntry(CTSE_Info& tse,
                                              const CVDBGraphBlobId& blob_id)
{
    if ( GetDebugLevel() >= 5 ) {
        LOG_POST_X(5, "CVDBGraphDataLoader: "
                   "loading split entry for "<<blob_id.m_SeqId);
    }
    CRef<SVDBFileInfo> info_ref = x_GetFileInfo(blob_id.m_VDBFile);
    const_cast<CVDBGraphBlobId&>(blob_id).m_FileInfo = info_ref;
    SVDBFileInfo& info = *info_ref;
    CVDBGraphSeqIterator it(info.m_VDB, blob_id.m_SeqId);
    if ( !it ) {
        return;
    }
    CRef<CSeq_entry> entry(new CSeq_entry);
    entry->SetSet().SetSeq_set();
    entry->SetSet().SetId().SetId(kTSEId);
    tse.SetSeq_entry(*entry);
    TSeqPos length = it.GetSeqLength();
    size_t kIdAdd[3] = {
        kOverviewChunkIdAdd,
        kMidZoomChunkIdAdd,
        kMainGraphChunkIdAdd
    };
    if ( GetUseTable() == 2 ||
         (GetUseTable() == 1 &&
          it.SeqTableIsSmaller(CRange<TSeqPos>::GetWhole())) ) {
        kIdAdd[2] = kMainTableChunkIdAdd;
    }
    static const TSeqPos kSize[3] = {
        kOverviewChunkSize,
        kMidZoomChunkSize,
        kMainChunkSize
    };
    CAnnotName kName[3] = {
        info.GetOverviewAnnotName(),
        info.GetMidZoomAnnotName(),
        info.GetMainAnnotName()
    };
    CTSE_Split_Info& split_info = tse.GetSplitInfo();
    CTSE_Chunk_Info::TPlace place(CSeq_id_Handle(), kTSEId);
    for ( int k = 0; k < 3; ++k ) {
        if ( kIdAdd[k] == kOverviewChunkIdAdd &&
             !GetEnabledOverview() ) {
            continue;
        }
        if ( kIdAdd[k] == kMidZoomChunkIdAdd &&
             !(GetEnabledMidZoom() && info.m_VDB->HasMidZoomGraphs()) ) {
            continue;
        }
        for ( int i = 0; i*kSize[k] < length; ++i ) {
            TSeqPos from = i*kSize[k], to_open = min(length, from+kSize[k]);
            COpenRange<TSeqPos> range(from, to_open);
            size_t id_add = kIdAdd[k];
            CSeq_annot::TData::E_Choice type = CSeq_annot::C_Data::e_Graph;
            if ( id_add == kMainTableChunkIdAdd ) {
                type = CSeq_annot::C_Data::e_Seq_table;
            }
            int chunk_id = int(i*kChunkIdMul+id_add);
            CRef<CTSE_Chunk_Info> chunk(new CTSE_Chunk_Info(chunk_id));
            chunk->x_AddAnnotType(kName[k], type, it.GetSeq_id_Handle(), range);
            chunk->x_AddAnnotPlace(place);
            split_info.AddChunk(*chunk);
        }
    }
}


void CVDBGraphDataLoader_Impl::GetChunk(CTSE_Chunk_Info& chunk)
{
    const CVDBGraphBlobId& blob_id =
        dynamic_cast<const CVDBGraphBlobId&>(*chunk.GetBlobId());
    CRef<SVDBFileInfo> info_ref = blob_id.m_FileInfo;
    if ( !info_ref ) {
        info_ref = x_GetFileInfo(blob_id.m_VDBFile);
    }
    SVDBFileInfo& info = *info_ref;
    CVDBGraphSeqIterator it(info.m_VDB, blob_id.m_SeqId);
    if ( !it ) {
        return;
    }
    TSeqPos length = it.GetSeqLength();

    static const TSeqPos kSize[kChunkIdMul] = {
        kOverviewChunkSize,
        kMidZoomChunkSize,
        kMainChunkSize,
        kMainChunkSize
    };
    static const CVDBGraphSeqIterator::TContentFlags kFlags[kChunkIdMul] = {
        CVDBGraphSeqIterator::fGraphQAll,
        CVDBGraphSeqIterator::fGraphZoomQAll,
        CVDBGraphSeqIterator::fGraphMain,
        CVDBGraphSeqIterator::fGraphMain|CVDBGraphSeqIterator::fGraphMainAsTable
    };
    static const char* const kTypeName[kChunkIdMul] = {
        "overview",
        "mid-zoom",
        "main graph",
        "main table"
    };
    string name = info.GetMainAnnotName();
    int k = chunk.GetChunkId()%kChunkIdMul;
    int i = chunk.GetChunkId()/kChunkIdMul;
    TSeqPos from = i*kSize[k], to_open = min(length, from+kSize[k]);
    if ( GetDebugLevel() >= 6 ) {
        LOG_POST_X(6, "CVDBGraphDataLoader: "
                   "loading "<<kTypeName[k]<<" chunk "<<blob_id.m_SeqId<<
                   " @ "<<from<<"-"<<(to_open-1));
    }
    CVDBGraphSeqIterator::TContentFlags flags = kFlags[k];
    CRef<CSeq_annot> annot =
        it.GetAnnot(COpenRange<TSeqPos>(from, to_open), name, flags);
    if ( GetDebugLevel() >= 6 ) {
        LOG_POST_X(6, "CVDBGraphDataLoader: "
                   "loaded "<<kTypeName[k]<<" chunk "<<blob_id.m_SeqId<<
                   " @ "<<from<<"-"<<(to_open-1)<<": "<<MSerial_AsnText<<*annot);
    }
    CTSE_Chunk_Info::TPlace place(CSeq_id_Handle(), kTSEId);
    chunk.x_LoadAnnot(place, *annot);
    chunk.SetLoaded();
}


CRef<CVDBGraphDataLoader_Impl::SVDBFileInfo>
CVDBGraphDataLoader_Impl::x_GetNAFileInfo(const string& na_acc)
{
    if ( !m_FixedFileMap.empty() ) {
        return null;
    }
    CMutexGuard guard(m_Mutex);
    TMissingFileSet::iterator it2 = m_MissingFileSet.find(na_acc);
    if ( it2 != m_MissingFileSet.end() ) {
        return null;
    }
    TAutoFileMap::iterator it = m_AutoFileMap.find(na_acc);
    if ( it != m_AutoFileMap.end() ) {
        return it->second;
    }
    CRef<SVDBFileInfo> info(new SVDBFileInfo);
    info->m_VDBFile = na_acc;
    info->m_BaseAnnotName = na_acc;
    try {
        if ( GetDebugLevel() >= 2 ) {
            LOG_POST_X(2, "CVDBGraphDataLoader: auto-opening file "<<na_acc);
        }
        info->m_VDB = CVDBGraphDb(m_Mgr, na_acc);
    }
    catch ( CSraException& exc ) {
        if ( exc.GetErrCode() != exc.eNotFoundTable ) {
            throw;
        }
        if ( GetDebugLevel() >= 2 ) {
            LOG_POST_X(3, "CVDBGraphDataLoader: accession not found: "<<na_acc);
        }
        m_MissingFileSet[na_acc] = true;
        return null;
    }
    m_AutoFileMap[na_acc] = info;
    return info;
}


CRef<CVDBGraphDataLoader_Impl::SVDBFileInfo>
CVDBGraphDataLoader_Impl::x_GetFileInfo(const string& name)
{
    TFixedFileMap::iterator it = m_FixedFileMap.find(name);
    if ( it != m_FixedFileMap.end() ) {
        return it->second;
    }
    return x_GetNAFileInfo(name);
}


END_SCOPE(objects)
END_NCBI_SCOPE
