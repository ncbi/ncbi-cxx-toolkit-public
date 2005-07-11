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
#include <ncbi_pch.hpp>
#include <objtools/data_loaders/blastdb/bdbloader.hpp>
#include <objmgr/impl/tse_loadlock.hpp>
#include <objects/seq/seq__.hpp>
#include <objmgr/impl/tse_split_info.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <objmgr/data_loader_factory.hpp>
#include <corelib/plugin_manager_impl.hpp>

//=======================================================================
// BlastDbDataLoader Public interface 
//

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CBlastDbDataLoader::TRegisterLoaderInfo CBlastDbDataLoader::RegisterInObjectManager(
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
string DbTypeToStr(CBlastDbDataLoader::EDbType dbtype)
{
    switch (dbtype) {
    case CBlastDbDataLoader::eNucleotide: return "Nucleotide";
    case CBlastDbDataLoader::eProtein:    return "Protein";
    default:                              return "Unknown";
    }
}


string CBlastDbDataLoader::GetLoaderNameFromArgs(const SBlastDbParam& param)
{
    return "BLASTDB_" + param.m_DbName + DbTypeToStr(param.m_DbType);
}


CBlastDbDataLoader::CBlastDbDataLoader(const string        & loader_name,
                                       const SBlastDbParam & param)
    : CDataLoader    (loader_name),
      m_DBName       (param.m_DbName),
      m_DBType       (param.m_DbType),
      m_NextChunkId  (0)
{
    m_SeqDB.Reset(new CSeqDB(m_DBName, (m_DBType == eProtein
                                        ? CSeqDB::eProtein
                                        : CSeqDB::eNucleotide)));
}

CBlastDbDataLoader::~CBlastDbDataLoader(void)
{
}

CBlastDbDataLoader::TTSE_LockSet
CBlastDbDataLoader::GetRecords(const CSeq_id_Handle& idh, EChoice /*choice*/)
{
    CTSE_LoadLock lock;
    TTSE_LockSet locks;
    
    if (x_LoadData(idh, lock)) {
        locks.insert(TTSE_Lock(lock));
    }
    
    return locks;
}

CBlastDbDataLoader::CCachedSeqData::CCachedSeqData(const CSeq_id_Handle & idh,
                                                   CSeqDB               & db,
                                                   int                    oid)
    : m_SIH(idh), m_SeqDB(& db), m_OID(oid)
{
    m_TSE.Reset();
    m_Length = m_SeqDB->GetSeqLength(m_OID);
    
    CConstRef<CSeq_id> seq_id = idh.GetSeqId();
    
    int gi = ((seq_id->Which() == CSeq_id::e_Gi)
              ? seq_id->GetGi()
              : 0);
    
    CRef<CBioseq> bioseq(m_SeqDB->GetBioseqNoData(m_OID, gi));
    
    bioseq->SetInst().SetMol((m_SeqDB->GetSequenceType() == CSeqDB::eProtein)
                             ? CSeq_inst::eMol_aa
                             : CSeq_inst::eMol_na);
    
    m_TSE.Reset(new CSeq_entry);
    m_TSE->SetSeq(*bioseq);
    
    // Remove descriptions (annotations and features would be removed
    // here if SeqDB produced them.)
    
    if (bioseq->IsSetDescr()) {
        m_Descr.Reset(& bioseq->SetDescr());
        bioseq->ResetDescr();
    }
    
    bioseq->SetInst().ResetSeq_data();
}

void CBlastDbDataLoader::CCachedSeqData::
RegisterIds(CBlastDbDataLoader::TIds & idmap)
{
    _ASSERT(m_TSE->IsSeq());
    
    CBioseq::TId & ids = m_TSE->SetSeq().SetId();
    
    ITERATE(CBioseq::TId, seqid, ids) {
        idmap[CSeq_id_Handle::GetHandle(**seqid)] = m_OID;
    }
}

void CBlastDbDataLoader::CSeqChunkData::BuildLiteral()
{
    CRef<CSeq_data> seq_data(new CSeq_data);
    
    if (m_SeqDB->GetSequenceType() == CSeqDB::eProtein) {
        const char * buffer(0);
        int          length(0);
        length = m_SeqDB->GetSequence(m_OID, & buffer);
        
        _ASSERT((m_End-m_Begin) <= length);
        
        seq_data->SetNcbistdaa().Set().assign(buffer + m_Begin, buffer + m_End);
        
        m_SeqDB->RetSequence(& buffer);
    } else {
        
        // This code works around the fact that SeqDB currently only
        // produces 8 bit output -- it builds an array and packs the
        // output into it in 4 bit format.  SeqDB should probably
        // provide more formats and combinations so that this code can
        // disappear.
        
        int nucl_code(kSeqDBNuclNcbiNA8);
        
        const char * buffer(0);
        int          length(0);
        length = m_SeqDB->GetAmbigSeq(m_OID, & buffer, nucl_code, m_Begin, m_End);
        
        _ASSERT((m_End-m_Begin) == length);
        
        vector<char> v4;
        v4.reserve((length+1)/2);
        
        int length_whole = length & -2;
        
        for(int i = 0; i < length_whole; i += 2) {
            v4.push_back((buffer[i] << 4) | buffer[i+1]);
        }
        if (length_whole != length) {
            _ASSERT((length_whole) == (length-1));
            v4.push_back(buffer[length_whole] << 4);
        }
        
        seq_data->SetNcbi4na().Set().swap(v4);
        
        m_SeqDB->RetSequence(& buffer);
    }
    
    CRef<CSeq_literal> literal(new CSeq_literal);
    literal->SetLength(m_End - m_Begin);
    literal->SetSeq_data(*seq_data);
    
    m_Literal.Reset(literal);
}

CBlastDbDataLoader::CSeqChunkData
CBlastDbDataLoader::CCachedSeqData::BuildDataChunk(int id, int begin, int end)
{
    x_AddDelta(begin, end);
    m_SeqDataIds.push_back(id);
    return CSeqChunkData(m_SeqDB, m_SIH, m_OID, begin, end);
}

void CBlastDbDataLoader::CCachedSeqData::x_AddDelta(int begin, int end)
{
    CRef<CSeq_literal> segment(new CSeq_literal);
    segment->SetLength(end - begin);
    
    CRef<CDelta_seq> dseq(new CDelta_seq);
    dseq->SetLiteral(*segment);
    
    CDelta_ext & delta = m_TSE->SetSeq().SetInst().SetExt().SetDelta();
    delta.Set().push_back(dseq);
}

void CBlastDbDataLoader::x_SplitSeq(TChunks& chunks, CCachedSeqData & seqdata /*, bioseq, int length*/)
{
    // Split and remove annots
    CSeq_id_Handle idh = seqdata.GetSeqIdHandle();
    
    // Split and remove descrs
    x_SplitDescr(chunks, seqdata);
    
    // Split and remove data
    x_SplitSeqData(chunks, seqdata);
}

void CBlastDbDataLoader::x_SplitSeqData(TChunks        & chunks,
                                        CCachedSeqData & seqdata)
{
    int length = seqdata.GetLength();
    
    for(int i = 0; i<length; i += kSequenceSliceSize) {
        int j = length;
        
        if ((j - i) > kSequenceSliceSize) {
            j = i + kSequenceSliceSize;
        }
        
        x_AddSplitSeqChunk(chunks, seqdata, i, j);
    }
}

void CBlastDbDataLoader::x_AddSplitSeqChunk(TChunks        & chunks,
                                            CCachedSeqData & seqdata,
                                            int              begin,
                                            int              end)
{
    // Create location for the chunk
    CTSE_Chunk_Info::TLocationSet loc_set;
    CTSE_Chunk_Info::TLocationRange rg =
        CTSE_Chunk_Info::TLocationRange(begin, end-1);
    
    CTSE_Chunk_Info::TLocation loc(seqdata.GetSeqIdHandle(), rg);
    loc_set.push_back(loc);
    
    // Create new chunk for the data
    CRef<CTSE_Chunk_Info> chunk(new CTSE_Chunk_Info(++m_NextChunkId));
    
    // Add seq-data
    chunk->x_AddSeq_data(loc_set);
    
    // Prepare and store data locally, remember place and chunk id
    m_SeqChunks[m_NextChunkId] =
        seqdata.BuildDataChunk(m_NextChunkId, begin, end);
    
    chunks.push_back(chunk);
}

bool CBlastDbDataLoader::x_LoadData(const CSeq_id_Handle& idh, CTSE_LoadLock & lock)
{
    CRef<CCachedSeqData> cached;
    int oid = -1;
    
    {
        CConstRef<CSeq_id> seq_id(idh.GetSeqId());
        
        if (m_Ids.find(idh) != m_Ids.end()) {
            oid = m_Ids[idh];
        } else {
            vector<int> oids;
            m_SeqDB->SeqidToOids(*seq_id, oids);
            
            if (oids.empty()){
                return false;
            }
            
            oid = oids[0];
        }
        
        cached = m_OidCache[oid];
    }
    
    if (cached.NotEmpty()) {
        // Already loaded, get and return the lock
        lock = GetDataSource()->GetTSE_LoadLock(TBlobId(cached->GetTSE()));
        return true;
    }
    
    cached.Reset(new CCachedSeqData(idh, *m_SeqDB, oid));
    
    cached->RegisterIds(m_Ids);
    
    TChunks chunks;
    
    // Split data
    
    x_SplitSeq(chunks, *cached);
    
    // Fill TSE info
    CRef<CSeq_entry> tse(cached->GetTSE());
    
    lock = GetDataSource()->GetTSE_LoadLock(TBlobId(tse));
    
    CTSE_Info& info = *lock;
    info.SetSeq_entry(*tse);
    
    // Attach all chunks to the TSE info
    NON_CONST_ITERATE(TChunks, it, chunks) {
        info.GetSplitInfo().AddChunk(**it);
    }
    
    // Mark TSE info as loaded
    lock.SetLoaded();
    return true;
}

void CBlastDbDataLoader::x_SplitDescr(TChunks        & chunks,
                                      CCachedSeqData & seqdata)
{
    // Create new chunk for each descr
    CRef<CTSE_Chunk_Info> chunk(new CTSE_Chunk_Info(++m_NextChunkId));
    
    // Add descr info using bioseq id or bioseq-set id
    // Descr type mask includes everything for simplicity
    TPlace place(seqdata.GetSeqIdHandle(), 0);
    
    if ( place.first ) {
        chunk->x_AddDescInfo(0xffff, place.first);
    } else {
        chunk->x_AddDescInfo(0xffff, place.second);
    }
    
    // Store data locally, remember place and chunk id
    m_DescrChunks[m_NextChunkId] = seqdata.AddDescr(m_NextChunkId);
    
    chunks.push_back(chunk);
}

void CBlastDbDataLoader::GetChunk(TChunk chunk)
{
    // Check if already loaded
    if ( chunk->IsLoaded() ) {
        return;
    }
    
    // Find descriptors related to the chunk
    TDescrChunks::iterator descr = m_DescrChunks.find(chunk->GetChunkId());
    
    if (descr != m_DescrChunks.end()) {
        // Attach related descriptor
        chunk->x_LoadDescr(TPlace(descr->second.GetSeqIdHandle(), 0),
                           *descr->second.GetDescr());
    }
    
    // Load sequence data
    TSeqChunkCache::iterator seq = m_SeqChunks.find(chunk->GetChunkId());
    
    if (seq != m_SeqChunks.end()) {
        // Attach seq-data
        CTSE_Chunk_Info::TSequence sequence;
        
        CSeqChunkData & scd = seq->second;
        
        if (! scd.HasLiteral()) {
            scd.BuildLiteral();
        }
        
        sequence.push_back(scd.GetLiteral());
        chunk->x_LoadSequence(TPlace(scd.GetSeqIdHandle(), 0),
                              scd.GetPosition(),
                              sequence);
    }
    
    // Mark chunk as loaded
    chunk->SetLoaded();
}

bool CBlastDbDataLoader::CanGetBlobById(void) const
{
    return true;
}

CBlastDbDataLoader::TBlobId
CBlastDbDataLoader::GetBlobId(const CSeq_id_Handle& idh)
{
    TIds::iterator iditer = m_Ids.find(idh);
    
    // Check if the id is known
    if (iditer != m_Ids.end()) {
        TOidCache::iterator oiditer = m_OidCache.find(iditer->second);
        
        if (oiditer != m_OidCache.end()) {
            return TBlobId(oiditer->second.GetPointer());
        }
    }
    return TBlobId(0);
}

CBlastDbDataLoader::TTSE_Lock
CBlastDbDataLoader::GetBlobById(const TBlobId& blob_id)
{
    const CSeq_entry * entry =
        dynamic_cast<const CSeq_entry*>(blob_id.GetPointer());
    
    if (entry) {
        if (entry->IsSeq() && entry->GetSeq().GetFirstId()) {
            CSeq_id_Handle sih =
                CSeq_id_Handle::GetHandle(*entry->GetSeq().GetFirstId());
            
            CTSE_LoadLock lock;
            
            if (x_LoadData(sih, lock)) {
                return TTSE_Lock(lock);
            }
        }
    }
    
    return TTSE_Lock();
}

void CBlastDbDataLoader::DropTSE(CRef<CTSE_Info> tse_info)
{
    const CSeq_id_Handle & sih = tse_info->GetRequestedId();
    
    if (m_Ids.find(sih) != m_Ids.end()) {
        int oid = m_Ids[sih];
        
        if (m_OidCache.find(oid) != m_OidCache.end()) {
            m_OidCache[oid]->FreeChunks(m_DescrChunks, m_SeqChunks);
            m_OidCache.erase(oid);
        }
    }
}


void
CBlastDbDataLoader::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
    // dummy assignment to eliminate compiler and doxygen warnings
    depth = depth;  
    ddc.SetFrame("CBlastDbDataLoader");
    DebugDumpValue(ddc,"m_dbname", m_DBName);
    DebugDumpValue(ddc,"m_dbtype", m_DBType);
   
}

END_SCOPE(objects)

// ===========================================================================

USING_SCOPE(objects);

void DataLoaders_Register_BlastDb(void)
{
    // Typedef to silence compiler warning.  A better solution to this
    // problem is probably possible.
    
    typedef void(*TArgFuncType)(list<CPluginManager< CDataLoader,
                                CInterfaceVersion<CDataLoader> >
                                ::SDriverInfo> &,
                                CPluginManager<CDataLoader,
                                CInterfaceVersion<CDataLoader> >
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
        kCFParam_BlastDb_DbName, false, kEmptyStr);
    const string& dbtype_str =
        GetParam(GetDriverName(), params,
        kCFParam_BlastDb_DbType, false, kEmptyStr);
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


/* ========================================================================== 
 *
 * $Log$
 * Revision 1.27  2005/07/11 15:23:07  bealer
 * - Doxygen
 * - Remove unnecessary parameter.
 *
 * Revision 1.26  2005/07/07 17:33:52  camacho
 * fix compiler warning
 *
 * Revision 1.25  2005/07/06 19:01:00  bealer
 * - Doxygen.
 *
 * Revision 1.24  2005/07/06 17:21:44  bealer
 * - Sequence splitting capability for BlastDbDataLoader.
 *
 * Revision 1.23  2005/06/23 16:18:46  camacho
 * Doxygen fixes
 *
 * Revision 1.22  2005/05/10 15:36:06  bealer
 * - Silence warning.
 *
 * Revision 1.21  2005/04/18 16:11:29  bealer
 * - Remove use of deprecated SeqDB methods.
 *
 * Revision 1.20  2004/12/22 20:42:53  grichenk
 * Added entry points registration funcitons
 *
 * Revision 1.19  2004/11/29 20:57:09  grichenk
 * Added GetLoaderNameFromArgs with full set of arguments.
 * Fixed BlastDbDataLoader name.
 *
 * Revision 1.18  2004/11/29 20:44:28  camacho
 * Correct class name in DebugDump
 *
 * Revision 1.17  2004/11/16 15:51:37  jianye
 * delete asnconvertor.hpp include
 *
 * Revision 1.16  2004/11/10 20:13:10  jianye
 * specify gi when finding oid
 *
 * Revision 1.15  2004/10/28 17:54:52  bealer
 * - Remove unsupported soon-to-disappear default arguments to GetBioseq().
 *
 * Revision 1.14  2004/10/21 22:47:21  bealer
 * - Fix compile warning (unused var).
 *
 * Revision 1.13  2004/10/05 16:37:58  jianye
 * Use CSeqDB
 *
 * Revision 1.12  2004/08/10 16:56:11  grichenk
 * Fixed dll export declarations, moved entry points to cpp.
 *
 * Revision 1.11  2004/08/04 14:56:35  vasilche
 * Updated to changes in TSE locking scheme.
 *
 * Revision 1.10  2004/08/02 21:33:07  ucko
 * Preemptively include <util/rangemap.hpp> before anything can define
 * stat() as a macro.
 *
 * Revision 1.9  2004/08/02 17:34:43  grichenk
 * Added data_loader_factory.cpp.
 * Renamed xloader_cdd to ncbi_xloader_cdd.
 * Implemented data loader factories for all loaders.
 *
 * Revision 1.8  2004/07/28 14:02:57  grichenk
 * Improved MT-safety of RegisterInObjectManager(), simplified the code.
 *
 * Revision 1.7  2004/07/26 14:13:32  grichenk
 * RegisterInObjectManager() return structure instead of pointer.
 * Added CObjectManager methods to manipuilate loaders.
 *
 * Revision 1.6  2004/07/21 15:51:26  grichenk
 * CObjectManager made singleton, GetInstance() added.
 * CXXXXDataLoader constructors made private, added
 * static RegisterInObjectManager() and GetLoaderNameFromArgs()
 * methods.
 *
 * Revision 1.5  2004/07/12 15:05:32  grichenk
 * Moved seq-id mapper from xobjmgr to seq library
 *
 * Revision 1.4  2004/05/21 21:42:51  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.3  2003/09/30 17:22:06  vasilche
 * Fixed for new CDataLoader interface.
 *
 * Revision 1.2  2003/09/30 16:36:36  vasilche
 * Updated CDataLoader interface.
 *
 * Revision 1.1  2003/08/06 16:15:18  jianye
 * Add BLAST DB loader.
 *
 * Revision 1.7  2003/05/19 21:11:46  camacho
 * Added caching
 *
 * Revision 1.6  2003/05/16 14:27:48  camacho
 * Proper use of namespaces
 *
 * Revision 1.5  2003/05/15 15:58:28  camacho
 * Minor changes
 *
 * Revision 1.4  2003/05/08 15:11:43  camacho
 * Changed prototype for GetRecords in base class
 *
 * Revision 1.3  2003/03/21 17:42:54  camacho
 * Added loading of taxonomy info
 *
 * Revision 1.2  2003/03/18 21:19:26  camacho
 * Retrieve all seqids if available
 *
 * Revision 1.1  2003/03/14 22:37:26  camacho
 * Initial revision
 *
 *
 * ========================================================================== */
