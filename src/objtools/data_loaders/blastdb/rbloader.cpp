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
*  Author: Kevin Bealer
*
*  File Description:
*   Data loader implementation that uses the CRemoteBlast API.
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <objtools/readers/seqdb/seqdb.hpp>
#include <objtools/data_loaders/blastdb/rbloader.hpp>
#include <objmgr/impl/tse_loadlock.hpp>
#include <objects/seq/seq__.hpp>
#include <objmgr/impl/tse_split_info.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <objmgr/data_loader_factory.hpp>
#include <corelib/plugin_manager_impl.hpp>

//=======================================================================
// RemoteBlastDataLoader Public interface 
//

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CRemoteBlastDataLoader::TRegisterLoaderInfo
CRemoteBlastDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const string& dbname,
    const EDbType dbtype,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    SBlastDbParam param(dbname, dbtype);
    TMaker maker(param);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}

inline
string DbTypeToStr(CRemoteBlastDataLoader::EDbType dbtype)
{
    switch (dbtype) {
    case CRemoteBlastDataLoader::eNucleotide: return "Nucleotide";
    case CRemoteBlastDataLoader::eProtein:    return "Protein";
    default:                                  return "Unknown";
    }
}

inline
CSeqDB::ESeqType DbTypeToSeqType(CRemoteBlastDataLoader::EDbType dbtype)
{
    switch (dbtype) {
    case CRemoteBlastDataLoader::eNucleotide: return CSeqDB::eNucleotide;
    case CRemoteBlastDataLoader::eProtein:    return CSeqDB::eProtein;
    default:                              return CSeqDB::eUnknown;
    }
}

string CRemoteBlastDataLoader::GetLoaderNameFromArgs(const SBlastDbParam& param)
{
    return "REMOTEBLAST_" + param.m_DbName + DbTypeToStr(param.m_DbType);
}


CRemoteBlastDataLoader::CRemoteBlastDataLoader(const string        & loader_name,
                                               const SBlastDbParam & param)
    : CDataLoader    (loader_name),
      m_DBName       (param.m_DbName),
      m_DBType       (param.m_DbType)
{
    m_Remote.Reset(new CRemoteDb(m_DBName, DbTypeToSeqType(m_DBType)));
}

CRemoteBlastDataLoader::~CRemoteBlastDataLoader()
{
}


typedef pair<int, CSeq_id_Handle> TBlob_id;
template<>
struct PConvertToString<TBlob_id>
    : public unary_function<TBlob_id, string>
{
    string operator()(const TBlob_id& v) const
    {
        return NStr::IntToString(v.first) + ':' + v.second.AsString();
    }
};


CRemoteBlastDataLoader::TTSE_LockSet
CRemoteBlastDataLoader::GetRecords(const CSeq_id_Handle& idh, EChoice choice)
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

CRemoteBlastDataLoader::CCachedSeqData::
CCachedSeqData(CRemoteDb            & db,
               const CSeq_id_Handle & idh,
               int                    oid)
    : m_SIH(idh), m_Remote(& db), m_OID(oid)
{
    m_TSE.Reset();
    m_Length = m_Remote->GetSeqLength(m_OID);
    
    CRef<CBioseq> bioseq(m_Remote->GetBioseqNoData(m_OID, *idh.GetSeqId()));
    CConstRef<CSeq_id> first_id( bioseq->GetFirstId() );
    
    _ASSERT(first_id);
    if ( first_id ) {
        m_SIH = CSeq_id_Handle::GetHandle(*first_id);
    }
    
    bioseq->SetInst().SetLength(m_Length);
    bioseq->SetInst().SetMol((m_Remote->GetSequenceType() == CSeqDB::eProtein)
                             ? CSeq_inst::eMol_aa
                             : CSeq_inst::eMol_na);
    
    // Seperate Seq_data.
    
    m_SeqData.Reset(& bioseq->SetInst().SetSeq_data());
    bioseq->SetInst().ResetSeq_data();
    bioseq->SetInst().ResetRepr();
    
    m_TSE.Reset(new CSeq_entry);
    m_TSE->SetSeq(*bioseq);
}

void CRemoteBlastDataLoader::CCachedSeqData::
RegisterIds(CRemoteBlastDataLoader::TIds & idmap)
{
    _ASSERT(m_TSE->IsSeq());
    
    CBioseq::TId & ids = m_TSE->SetSeq().SetId();
    
    ITERATE(CBioseq::TId, seqid, ids) {
        idmap[CSeq_id_Handle::GetHandle(**seqid)] = m_OID;
    }
}

void CRemoteBlastDataLoader::CSeqChunkData::BuildLiteral()
{
    CRef<CSeq_data> seq_data;
    
    if (m_Remote->GetSequenceType() == CSeqDB::eProtein) {
        seq_data = m_Remote->GetSequence(m_OID, m_Begin, m_End);
    } else {
        
        // This code works around the fact that SeqDB currently only
        // produces 8 bit output -- it builds an array and packs the
        // output into it in 4 bit format.  SeqDB should probably
        // provide more formats and combinations so that this code can
        // disappear.
        
        seq_data = m_Remote->GetSequence(m_OID, m_Begin, m_End);
    }
    
    CRef<CSeq_literal> literal(new CSeq_literal);
    literal->SetLength(m_End - m_Begin);
    literal->SetSeq_data(*seq_data);
    
    m_Literal.Reset(literal);
}


void CRemoteBlastDataLoader::x_SplitSeqData(TChunks        & chunks,
                                            CCachedSeqData & seqdata)
{
    CSeq_inst& inst = seqdata.GetTSE()->SetSeq().SetInst();
    TSeqPos length = seqdata.GetLength();
    if ( length <= kSequenceSliceSize ) {
        // single Seq-data, no need to use Delta
        inst.SetRepr(CSeq_inst::eRepr_raw);
        x_AddSplitSeqChunk(chunks, seqdata.GetSeqIdHandle(), 0, length);
    }
    else {
        // multiple Seq-data, we'll have to use Delta
        inst.SetRepr(CSeq_inst::eRepr_delta);
        CDelta_ext::Tdata& delta = inst.SetExt().SetDelta().Set();
        for( TSeqPos pos = 0; pos < length; pos += kSequenceSliceSize) {
            TSeqPos end = length;
            if ((end - pos) > kSequenceSliceSize) {
                end = pos + kSequenceSliceSize;
            }
            x_AddSplitSeqChunk(chunks, seqdata.GetSeqIdHandle(), pos, end);
            CRef<CDelta_seq> dseq(new CDelta_seq);
            dseq->SetLiteral().SetLength(end - pos);
            delta.push_back(dseq);
        }
    }
}

void CRemoteBlastDataLoader::x_AddSplitSeqChunk(TChunks&              chunks,
                                            const CSeq_id_Handle& id,
                                            TSeqPos               begin,
                                            TSeqPos               end)
{
    // Create location for the chunk
    CTSE_Chunk_Info::TLocationSet loc_set;
    CTSE_Chunk_Info::TLocationRange rg =
        CTSE_Chunk_Info::TLocationRange(begin, end-1);
    
    CTSE_Chunk_Info::TLocation loc(id, rg);
    loc_set.push_back(loc);
    
    // Create new chunk for the data
    CRef<CTSE_Chunk_Info> chunk(new CTSE_Chunk_Info(begin));
    
    // Add seq-data
    chunk->x_AddSeq_data(loc_set);

    chunks.push_back(chunk);
}

void CRemoteBlastDataLoader::x_LoadData(const CSeq_id_Handle& idh,
                                    int oid,
                                    CTSE_LoadLock& lock)
{
    _ASSERT(oid != -1);
    _ASSERT(lock);
    _ASSERT(!lock.IsLoaded());

    CRef<CCachedSeqData> cached(new CCachedSeqData(*m_Remote, idh, oid));
    
    cached->RegisterIds(m_Ids);
    
    TChunks chunks;
    
    // Split data
    
    x_SplitSeqData(chunks, *cached);
    
    // Fill TSE info
    lock->SetSeq_entry(*cached->GetTSE());
    
    // Attach all chunks to the TSE info
    NON_CONST_ITERATE(TChunks, it, chunks) {
        lock->GetSplitInfo().AddChunk(**it);
    }
    
    // Mark TSE info as loaded
    lock.SetLoaded();
}


void CRemoteBlastDataLoader::GetChunk(TChunk chunk)
{
    _ASSERT(!chunk->IsLoaded());
    int oid = GetOid(chunk->GetBlobId());

    ITERATE ( CTSE_Chunk_Info::TLocationSet, it, chunk->GetSeq_dataInfos() ) {
        const CSeq_id_Handle& sih = it->first;
        TSeqPos start = it->second.GetFrom();
        TSeqPos end = it->second.GetToOpen();
        CSeqChunkData scd(m_Remote, sih, oid, start, end);
        scd.BuildLiteral();
        CTSE_Chunk_Info::TSequence seq;
        seq.push_back(scd.GetLiteral());
        chunk->x_LoadSequence(TPlace(sih, 0), start, seq);
    }
    
    // Mark chunk as loaded
    chunk->SetLoaded();
}

int CRemoteBlastDataLoader::GetOid(const CSeq_id_Handle& idh)
{
    TIds::iterator iter = m_Ids.find(idh);
    if ( iter != m_Ids.end() ) {
        return iter->second;
    }
    
    CConstRef<CSeq_id> seqid = idh.GetSeqId();
    
    int oid = -1;
    
    if (! m_Remote->SeqidToOid(*seqid, oid, kSequenceSliceSize)) {
        return -1;
    }
    
    // Test for deflines.  If the filtering eliminates the Seq-id we
    // are interested in, we just pretend we don't know anything about
    // this Seq-id.  If there are other data loaders installed, they
    // will have an opportunity to resolve the Seq-id.
    
    bool found = false;
    
    list< CRef<CSeq_id> > filtered = m_Remote->GetSeqIDs(oid);
    
    ITERATE(list< CRef<CSeq_id> >, iter, filtered) {
        if (seqid->Compare(**iter) == CSeq_id::e_YES) {
            found = true;
            break;
        }
    }
    
    if (! found) {
        return -1;
    }
    
    m_Ids.insert(TIds::value_type(idh, oid));
    
    return oid;
}


int CRemoteBlastDataLoader::GetOid(const TBlobId& blob_id) const
{
    const TBlob_id& id =
        dynamic_cast<const CBlobIdFor<TBlob_id>&>(*blob_id).GetValue();
    return id.first;
}


bool CRemoteBlastDataLoader::CanGetBlobById() const
{
    return true;
}


CRemoteBlastDataLoader::TBlobId
CRemoteBlastDataLoader::GetBlobId(const CSeq_id_Handle& idh)
{
    TBlobId blob_id;
    int oid = GetOid(idh);
    if ( oid != -1 ) {
        blob_id = new CBlobIdFor<TBlob_id>(TBlob_id(oid, idh));
    }
    return blob_id;
}


CRemoteBlastDataLoader::TTSE_Lock
CRemoteBlastDataLoader::GetBlobById(const TBlobId& blob_id)
{
    CTSE_LoadLock lock = GetDataSource()->GetTSE_LoadLock(blob_id);
    if ( !lock.IsLoaded() ) {
        const TBlob_id& id =
            dynamic_cast<const CBlobIdFor<TBlob_id>&>(*blob_id).GetValue();
        x_LoadData(id.second, id.first, lock);
    }
    return lock;
}


void
CRemoteBlastDataLoader::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
    // dummy assignment to eliminate compiler and doxygen warnings
    depth = depth;  
    ddc.SetFrame("CRemoteBlastDataLoader");
    DebugDumpValue(ddc,"m_dbname", m_DBName);
    DebugDumpValue(ddc,"m_dbtype", m_DBType);
   
}

END_SCOPE(objects)

// ===========================================================================

USING_SCOPE(objects);

void DataLoaders_Register_RemoteBlast()
{
    // Typedef to silence compiler warning.  A better solution to this
    // problem is probably possible.
    
    typedef void(*TArgFuncType)(list<CPluginManager<CDataLoader>
                                ::SDriverInfo> &,
                                CPluginManager<CDataLoader>
                                ::EEntryPointRequest);
    
    RegisterEntryPoint<CDataLoader>((TArgFuncType)
                                    NCBI_EntryPoint_DataLoader_RemoteBlast);
}

const string kDataLoader_RemoteBlast_DriverName("remoteblast");

/// Data Loader Factory for RemoteBlastDataLoader
///
/// This class provides an interface which builds an instance of the
/// RemoteBlastDataLoader and registers it with the object manager.

class CRemoteBlast_DataLoaderCF : public CDataLoaderFactory
{
public:
    /// Constructor
    CRemoteBlast_DataLoaderCF()
        : CDataLoaderFactory(kDataLoader_RemoteBlast_DriverName) {}
    
    /// Destructor
    virtual ~CRemoteBlast_DataLoaderCF() {}
    
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


CDataLoader* CRemoteBlast_DataLoaderCF::CreateAndRegister(
    CObjectManager& om,
    const TPluginManagerParamTree* params) const
{
    if ( !ValidParams(params) ) {
        // Use constructor without arguments
        return CRemoteBlastDataLoader::RegisterInObjectManager(om).GetLoader();
    }
    // Parse params, select constructor
    const string& dbname =
        GetParam(GetDriverName(), params,
                 kCFParam_RemoteBlast_DbName, false, "");
    const string& dbtype_str =
        GetParam(GetDriverName(), params,
                 kCFParam_RemoteBlast_DbType, false, "");
    if ( !dbname.empty() ) {
        // Use database name
        CRemoteBlastDataLoader::EDbType dbtype = CRemoteBlastDataLoader::eUnknown;
        if ( !dbtype_str.empty() ) {
            if (NStr::CompareNocase(dbtype_str, "Nucleotide") == 0) {
                dbtype = CRemoteBlastDataLoader::eNucleotide;
            }
            else if (NStr::CompareNocase(dbtype_str, "Protein") == 0) {
                dbtype = CRemoteBlastDataLoader::eProtein;
            }
        }
        return CRemoteBlastDataLoader::RegisterInObjectManager(
            om,
            dbname,
            dbtype,
            GetIsDefault(params),
            GetPriority(params)).GetLoader();
    }
    // IsDefault and Priority arguments may be specified
    return CRemoteBlastDataLoader::RegisterInObjectManager(om).GetLoader();
}


void NCBI_EntryPoint_DataLoader_RemoteBlast(
    CPluginManager<CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<CDataLoader>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CRemoteBlast_DataLoaderCF>::
        NCBI_EntryPointImpl(info_list, method);
}


void NCBI_EntryPoint_xloader_remoteblast(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_DataLoader_RemoteBlast(info_list, method);
}


END_NCBI_SCOPE
