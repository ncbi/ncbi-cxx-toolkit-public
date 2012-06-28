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
*  Author: Christiam Camacho
*
*  File Description:
*   Data loader implementation that uses the blast databases
*
* ===========================================================================
*/
#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <objtools/data_loaders/blastdb/bdbloader.hpp>
#include <objmgr/impl/tse_loadlock.hpp>
#include <objects/seq/seq__.hpp>
#include <objmgr/impl/tse_split_info.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <objmgr/data_loader_factory.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include "cached_sequence.hpp"
#include "local_blastdb_adapter.hpp"

//=======================================================================
// BlastDbDataLoader Public interface 
//

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CBlastDbDataLoader::TRegisterLoaderInfo 
CBlastDbDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const string& dbname,
    const EDbType dbtype,
    bool use_fixed_size_slices,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    SBlastDbParam param(dbname, dbtype, use_fixed_size_slices);
    TMaker maker(param);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}

CBlastDbDataLoader::TRegisterLoaderInfo 
CBlastDbDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    CRef<CSeqDB> db_handle,
    bool use_fixed_size_slices,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    SBlastDbParam param(db_handle, use_fixed_size_slices);
    TMaker maker(param);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}

CBlastDbDataLoader::TRegisterLoaderInfo 
CBlastDbDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const string& dbname,
    const EDbType dbtype,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    return RegisterInObjectManager(om, dbname, dbtype, true, is_default,
                                   priority);
}

CBlastDbDataLoader::TRegisterLoaderInfo 
CBlastDbDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    CRef<CSeqDB> db_handle,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    return RegisterInObjectManager(om, db_handle, true, is_default, priority);
}

inline
string DbTypeToStr(CBlastDbDataLoader::EDbType dbtype)
{
    switch (dbtype) {
    case CBlastDbDataLoader::eNucleotide: return "Nucleotide";
    case CBlastDbDataLoader::eProtein:    return "Protein";
    default:                              return "Unknown";
    }
}


inline
CSeqDB::ESeqType DbTypeToSeqType(CBlastDbDataLoader::EDbType dbtype)
{
    switch (dbtype) {
    case CBlastDbDataLoader::eNucleotide: return CSeqDB::eNucleotide;
    case CBlastDbDataLoader::eProtein:    return CSeqDB::eProtein;
    default:                              return CSeqDB::eUnknown;
    }
}

inline
CBlastDbDataLoader::EDbType SeqTypeToDbType(CSeqDB::ESeqType seq_type)
{
    switch (seq_type) {
    case CSeqDB::eNucleotide:   return CBlastDbDataLoader::eNucleotide;
    case CSeqDB::eProtein:      return CBlastDbDataLoader::eProtein;
    default:                    return CBlastDbDataLoader::eUnknown;
    }
}

CBlastDbDataLoader::SBlastDbParam::SBlastDbParam(const string& db_name,
                                                 CBlastDbDataLoader::EDbType
                                                 db_type,
                                                 bool use_fixed_size_slices)
: m_DbName(db_name), m_DbType(db_type),
  m_UseFixedSizeSlices(use_fixed_size_slices), m_BlastDbHandle(0)
{}

CBlastDbDataLoader::SBlastDbParam::SBlastDbParam(CRef<CSeqDB> db_handle,
                                                 bool use_fixed_size_slices)
{
    m_BlastDbHandle = db_handle;
    m_UseFixedSizeSlices = use_fixed_size_slices;
    m_DbName.assign(db_handle->GetDBNameList());
    m_DbType = SeqTypeToDbType(db_handle->GetSequenceType());
}

static const string kPrefix = "BLASTDB_";
string CBlastDbDataLoader::GetLoaderNameFromArgs(const SBlastDbParam& param)
{
    return kPrefix + param.m_DbName + DbTypeToStr(param.m_DbType);
}

string CBlastDbDataLoader::GetLoaderNameFromArgs(CConstRef<CSeqDB> db_handle)
{
    _ASSERT(db_handle.NotEmpty());
    return kPrefix + db_handle->GetDBNameList() + 
        DbTypeToStr(SeqTypeToDbType(db_handle->GetSequenceType()));
}


CBlastDbDataLoader::CBlastDbDataLoader(const string        & loader_name,
                                       const SBlastDbParam & param)
    : CDataLoader           (loader_name),
      m_DBName              (param.m_DbName),
      m_DBType              (param.m_DbType),
      m_BlastDb             (0),
      m_UseFixedSizeSlices  (param.m_UseFixedSizeSlices)
{
    if (param.m_BlastDbHandle.NotEmpty()) {
        m_BlastDb.Reset(new CLocalBlastDbAdapter(param.m_BlastDbHandle));
    }
    if (m_BlastDb.Empty() && !m_DBName.empty()) {
        const CSeqDB::ESeqType dbtype = DbTypeToSeqType(m_DBType);
        m_BlastDb.Reset(new CLocalBlastDbAdapter(m_DBName, dbtype));
    }
    if (m_BlastDb.Empty() && m_DBName.empty()) {
        NCBI_THROW(CSeqDBException, eArgErr, "Empty BLAST database handle");
    }
    _ASSERT(m_BlastDb.NotEmpty());
    _TRACE("Using " << GetLoaderNameFromArgs(param) << " data loader");
}

CBlastDbDataLoader::~CBlastDbDataLoader(void)
{
}

/// A BLAST DB (blob) ID
/// The first field represents an OID in the BLAST database
typedef pair<int, CSeq_id_Handle> TBlastDbId;

/// Template specialization to convert BLAST DB (blob) IDs to human readable
/// strings.
template<>
struct PConvertToString<TBlastDbId>
    : public unary_function<TBlastDbId, string>
{
    /// Convert TBlastDbId (blob IDs) to human readable strings.
    /// @param v The value to convert. [in]
    /// @return A string version of the value passed in.
    string operator()(const TBlastDbId& v) const
    {
        return NStr::IntToString(v.first) + ':' + v.second.AsString();
    }
};

/// Type definition consistent with those defined in objmgr/blob_id.hpp
typedef CBlobIdFor<TBlastDbId> CBlobIdBlastDb;


CBlastDbDataLoader::TTSE_LockSet
CBlastDbDataLoader::GetRecords(const CSeq_id_Handle& idh, EChoice choice)
{
    TTSE_LockSet locks;

    switch ( choice ) {
    case eBlob:
    case eBioseq:
    case eCore:
    case eBioseqCore:
    case eSequence:
    case eAll:
        {
            TBlobId blob_id = GetBlobId(idh);
            if ( blob_id ) {
                locks.insert(GetBlobById(blob_id));
            }
            break;
        }
    default:
        break;
    }
    
    return locks;
}

DEFINE_STATIC_FAST_MUTEX(s_Oid_Mutex);

void CBlastDbDataLoader::x_LoadData(const CSeq_id_Handle& idh,
                                    int oid,
                                    CTSE_LoadLock& lock,
                                    int slice_size)
{
    _ASSERT(oid != -1);
    _ASSERT(lock);
    _ASSERT(!lock.IsLoaded());

    CRef<CCachedSequence> cached(new CCachedSequence(*m_BlastDb, idh, oid,
                                                     m_UseFixedSizeSlices,
                                                     slice_size));
    {
        CFastMutexGuard guard(s_Oid_Mutex);
        cached->RegisterIds(m_Ids);
    }
    
    CCachedSequence::TCTSE_Chunk_InfoVector chunks;
    
    // Split data
    
    cached->SplitSeqData(chunks);
    
    // Fill TSE info
    lock->SetSeq_entry(*cached->GetTSE());
    
    // Attach all chunks to the TSE info
    NON_CONST_ITERATE(CCachedSequence::TCTSE_Chunk_InfoVector, it, chunks) {
        lock->GetSplitInfo().AddChunk(**it);
    }
    
    // Mark TSE info as loaded
    lock.SetLoaded();
}

TSeqPos CBlastDbDataLoader::GetSequenceLength(const CSeq_id_Handle& idh)
{
    int oid = 0;
    if (m_BlastDb->SeqidToOid(*idh.GetSeqId(), oid)) {
        return m_BlastDb->GetSeqLength(oid);
    }
    return kInvalidSeqPos;
}

CSeq_inst::TMol CBlastDbDataLoader::GetSequenceType(const CSeq_id_Handle& /*idh*/)
{
    switch (m_DBType) {
    case CBlastDbDataLoader::eNucleotide: return CSeq_inst::eMol_na;
    case CBlastDbDataLoader::eProtein:    return CSeq_inst::eMol_aa;
    default:                              return CSeq_inst::eMol_not_set;
    }
}

void CBlastDbDataLoader::GetSequenceTypes(const CDataLoader::TIds& ids, TLoaded& loaded,
                                          TSequenceTypes& ret)
{
    CSeq_inst::TMol retval = CSeq_inst::eMol_not_set;
    switch (m_DBType) {
    case CBlastDbDataLoader::eNucleotide: retval = CSeq_inst::eMol_na;
    case CBlastDbDataLoader::eProtein:    retval = CSeq_inst::eMol_aa;
    default:                              retval = CSeq_inst::eMol_not_set;
    }
    ret.clear();
    ret.assign(ids.size(), retval);
}

void CBlastDbDataLoader::GetChunk(TChunk chunk)
{
    static const CTSE_Chunk_Info::TBioseq_setId kIgnored = 0;
    _ASSERT(!chunk->IsLoaded());
    int oid = x_GetOid(chunk->GetBlobId());

    ITERATE ( CTSE_Chunk_Info::TLocationSet, it, chunk->GetSeq_dataInfos() ) {
        const CSeq_id_Handle& sih = it->first;
        TSeqPos start = it->second.GetFrom();
        TSeqPos end = it->second.GetToOpen();
        CTSE_Chunk_Info::TSequence seq;
        seq.push_back(CreateSeqDataChunk(*m_BlastDb, oid, start, end));
        chunk->x_LoadSequence(TPlace(sih, kIgnored), start, seq);
    }
    
    // Mark chunk as loaded
    chunk->SetLoaded();
}

int CBlastDbDataLoader::x_GetOid(const CSeq_id_Handle& idh)
{
    {
        CFastMutexGuard guard(s_Oid_Mutex);
        TIds::iterator iter = m_Ids.find(idh);
        if ( iter != m_Ids.end() ) {
            return iter->second;
        }
    }
    
    CConstRef<CSeq_id> seqid = idh.GetSeqId();
    
    int oid = -1;
    
    if (! m_BlastDb->SeqidToOid(*seqid, oid)) {
        _TRACE("FAILED to find '" << seqid->AsFastaString() << "'");
        return -1;
    }
    _TRACE("Found '" << seqid->AsFastaString() << "' at OID " << oid);
    
    // Test for deflines.  If the filtering eliminates the Seq-id we
    // are interested in, we just pretend we don't know anything about
    // this Seq-id.  If there are other data loaders installed, they
    // will have an opportunity to resolve the Seq-id.
    
    bool found = false;
    
    IBlastDbAdapter::TSeqIdList filtered = m_BlastDb->GetSeqIDs(oid);
    
    ITERATE(IBlastDbAdapter::TSeqIdList, id, filtered) {
        if (seqid->Compare(**id) == CSeq_id::e_YES) {
            found = true;
            break;
        }
    }
    
    if (! found) {
        return -1;
    }
    
    {
        CFastMutexGuard guard(s_Oid_Mutex);
        m_Ids.insert(TIds::value_type(idh, oid));
    }
    return oid;
}


int CBlastDbDataLoader::x_GetOid(const TBlobId& blob_id) const
{
    const TBlastDbId& id =
        dynamic_cast<const CBlobIdBlastDb&>(*blob_id).GetValue();
    return id.first;
}

bool CBlastDbDataLoader::CanGetBlobById(void) const
{
    return true;
}


CBlastDbDataLoader::TBlobId
CBlastDbDataLoader::GetBlobId(const CSeq_id_Handle& idh)
{
    TBlobId blob_id;
    int oid = x_GetOid(idh);
    if ( oid != -1 ) {
        blob_id = new CBlobIdBlastDb(TBlastDbId(oid, idh));
    }
    return blob_id;
}


CBlastDbDataLoader::TTSE_Lock
CBlastDbDataLoader::GetBlobById(const TBlobId& blob_id)
{
    CTSE_LoadLock lock = GetDataSource()->GetTSE_LoadLock(blob_id);
    if ( !lock.IsLoaded() ) {
        const TBlastDbId& id =
            dynamic_cast<const CBlobIdBlastDb&>(*blob_id).GetValue();
        x_LoadData(id.second, id.first, lock, kSequenceSliceSize);
    }
    return lock;
}


void
CBlastDbDataLoader::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
    // dummy assignment to eliminate compiler and doxygen warnings
    depth = depth;  
    ddc.SetFrame("CBlastDbDataLoader");
    DebugDumpValue(ddc,"m_DBName", m_DBName);
    DebugDumpValue(ddc,"m_DBType", m_DBType);
    DebugDumpValue(ddc,"m_UseFixedSizeSlices", m_UseFixedSizeSlices);
}

END_SCOPE(objects)

// ===========================================================================

USING_SCOPE(objects);

void DataLoaders_Register_BlastDb(void)
{
    // Typedef to silence compiler warning.  A better solution to this
    // problem is probably possible.
    
    typedef void(*TArgFuncType)(list<CPluginManager<CDataLoader>
                                ::SDriverInfo> &,
                                CPluginManager<CDataLoader>
                                ::EEntryPointRequest);
    
    RegisterEntryPoint<CDataLoader>((TArgFuncType)
                                    NCBI_EntryPoint_DataLoader_BlastDb);
}

const string kDataLoader_BlastDb_DriverName("blastdb");

/// Data Loader Factory for BlastDbDataLoader
///
/// This class provides an interface which builds an instance of the
/// BlastDbDataLoader and registers it with the object manager.

class CBlastDb_DataLoaderCF : public CDataLoaderFactory
{
public:
    /// Constructor
    CBlastDb_DataLoaderCF(void)
        : CDataLoaderFactory(kDataLoader_BlastDb_DriverName) {}
    
    /// Destructor
    virtual ~CBlastDb_DataLoaderCF(void) {}
    
protected:
    /// Create and register a data loader
    /// @param om
    ///   A reference to the object manager
    /// @param params
    ///   Arguments for the data loader constructor
    virtual CDataLoader* CreateAndRegister(
        CObjectManager& om,
        const TPluginManagerParamTree* params) const;
};


CDataLoader* CBlastDb_DataLoaderCF::CreateAndRegister(
    CObjectManager& om,
    const TPluginManagerParamTree* params) const
{
    if ( !ValidParams(params) ) {
        // Use constructor without arguments
        return CBlastDbDataLoader::RegisterInObjectManager(om).GetLoader();
    }
    // Parse params, select constructor
    const string& dbname =
        GetParam(GetDriverName(), params,
        kCFParam_BlastDb_DbName, false);
    const string& dbtype_str =
        GetParam(GetDriverName(), params,
        kCFParam_BlastDb_DbType, false);
    if ( !dbname.empty() ) {
        // Use database name
        CBlastDbDataLoader::EDbType dbtype = CBlastDbDataLoader::eUnknown;
        if ( !dbtype_str.empty() ) {
            if (NStr::CompareNocase(dbtype_str, "Nucleotide") == 0) {
                dbtype = CBlastDbDataLoader::eNucleotide;
            }
            else if (NStr::CompareNocase(dbtype_str, "Protein") == 0) {
                dbtype = CBlastDbDataLoader::eProtein;
            }
        }
        return CBlastDbDataLoader::RegisterInObjectManager(
            om,
            dbname,
            dbtype,
            true,   // use_fixed_size_slices
            GetIsDefault(params),
            GetPriority(params)).GetLoader();
    }
    // IsDefault and Priority arguments may be specified
    return CBlastDbDataLoader::RegisterInObjectManager(om).GetLoader();
}


void NCBI_EntryPoint_DataLoader_BlastDb(
    CPluginManager<CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<CDataLoader>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CBlastDb_DataLoaderCF>::
        NCBI_EntryPointImpl(info_list, method);
}


void NCBI_EntryPoint_xloader_blastdb(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_DataLoader_BlastDb(info_list, method);
}


END_NCBI_SCOPE
