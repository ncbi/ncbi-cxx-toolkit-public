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

CBlastDbDataLoader::TRegisterLoaderInfo 
CBlastDbDataLoader::RegisterInObjectManager(
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

CBlastDbDataLoader::TRegisterLoaderInfo 
CBlastDbDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    CRef<CSeqDB> db_handle,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    TRecycler recycler(db_handle);
    CDataLoader::RegisterInObjectManager(om, recycler, is_default, priority);
    return recycler.GetRegisterInfo();
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

string CBlastDbDataLoader::GetLoaderNameFromArgs(const SBlastDbParam& param)
{
    return "BLASTDB_" + param.m_DbName + DbTypeToStr(param.m_DbType);
}

string CBlastDbDataLoader::GetLoaderNameFromArgs(CConstRef<CSeqDB> db_handle)
{
    _ASSERT(db_handle.NotEmpty());
    return "BLASTDB_" + db_handle->GetDBNameList() + 
        DbTypeToStr(SeqTypeToDbType(db_handle->GetSequenceType()));
}


CBlastDbDataLoader::CBlastDbDataLoader(const string        & loader_name,
                                       const SBlastDbParam & param)
    : CDataLoader    (loader_name),
      m_DBName       (param.m_DbName),
      m_DBType       (param.m_DbType)
{
    m_SeqDB.Reset(new CSeqDB(m_DBName, DbTypeToSeqType(m_DBType)));
}

CBlastDbDataLoader::CBlastDbDataLoader(const string        & loader_name,
                                       CRef<CSeqDB>        db_handle)
    : CDataLoader    (loader_name)
{
    if (db_handle.Empty()) {
        NCBI_THROW(CSeqDBException, eArgErr, "Empty BLAST database handle");
    }
    m_SeqDB = db_handle;
}

CBlastDbDataLoader::~CBlastDbDataLoader(void)
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

CBlastDbDataLoader::CCachedSeqData::CCachedSeqData(CSeqDB               & db,
                                                   const CSeq_id_Handle&  idh,
                                                   int                    oid)
    : m_SIH(idh), m_SeqDB(& db), m_OID(oid)
{
    m_TSE.Reset();
    m_Length = m_SeqDB->GetSeqLength(m_OID);
    
    CRef<CBioseq> bioseq(m_SeqDB->GetBioseqNoData(m_OID,
                                                  idh.IsGi()? idh.GetGi(): 0));
    
    CConstRef<CSeq_id> first_id( bioseq->GetFirstId() );
    _ASSERT(first_id);
    if ( first_id ) {
        m_SIH = CSeq_id_Handle::GetHandle(*first_id);
    }
    
    bioseq->SetInst().SetLength(m_Length);
    bioseq->SetInst().SetMol((m_SeqDB->GetSequenceType() == CSeqDB::eProtein)
                             ? CSeq_inst::eMol_aa
                             : CSeq_inst::eMol_na);
    
    m_TSE.Reset(new CSeq_entry);
    m_TSE->SetSeq(*bioseq);
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
        TSeqPos      length(0);
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
        TSeqPos      length(0);
        length = m_SeqDB->GetAmbigSeq(m_OID, & buffer, nucl_code, m_Begin, m_End);
        
        _ASSERT((m_End-m_Begin) == length);
        
        vector<char> v4;
        v4.reserve((length+1)/2);
        
        TSeqPos length_whole = length & -2;
        
        for(TSeqPos i = 0; i < length_whole; i += 2) {
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

void CBlastDbDataLoader::x_SplitSeqData(TChunks        & chunks,
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

void CBlastDbDataLoader::x_AddSplitSeqChunk(TChunks&              chunks,
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

void CBlastDbDataLoader::x_LoadData(const CSeq_id_Handle& idh,
                                    int oid,
                                    CTSE_LoadLock& lock)
{
    _ASSERT(oid != -1);
    _ASSERT(lock);
    _ASSERT(!lock.IsLoaded());

    CRef<CCachedSeqData> cached(new CCachedSeqData(*m_SeqDB, idh, oid));
    
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


void CBlastDbDataLoader::GetChunk(TChunk chunk)
{
    _ASSERT(!chunk->IsLoaded());
    int oid = GetOid(chunk->GetBlobId());

    ITERATE ( CTSE_Chunk_Info::TLocationSet, it, chunk->GetSeq_dataInfos() ) {
        const CSeq_id_Handle& sih = it->first;
        TSeqPos start = it->second.GetFrom();
        TSeqPos end = it->second.GetToOpen();
        CSeqChunkData scd(m_SeqDB, sih, oid, start, end);
        scd.BuildLiteral();
        CTSE_Chunk_Info::TSequence seq;
        seq.push_back(scd.GetLiteral());
        chunk->x_LoadSequence(TPlace(sih, 0), start, seq);
    }
    
    // Mark chunk as loaded
    chunk->SetLoaded();
}

int CBlastDbDataLoader::GetOid(const CSeq_id_Handle& idh)
{
    TIds::iterator iter = m_Ids.find(idh);
    if ( iter != m_Ids.end() ) {
        return iter->second;
    }
    
    CConstRef<CSeq_id> seqid = idh.GetSeqId();
    
    int oid = -1;
    
    if (! m_SeqDB->SeqidToOid(*seqid, oid)) {
        return -1;
    }
    
    // Test for deflines.  If the filtering eliminates the Seq-id we
    // are interested in, we just pretend we don't know anything about
    // this Seq-id.  If there are other data loaders installed, they
    // will have an opportunity to resolve the Seq-id.
    
    bool found = false;
    
    list< CRef<CSeq_id> > filtered = m_SeqDB->GetSeqIDs(oid);
    
    ITERATE(list< CRef<CSeq_id> >, id, filtered) {
        if (seqid->Compare(**id) == CSeq_id::e_YES) {
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


int CBlastDbDataLoader::GetOid(const TBlobId& blob_id) const
{
    const TBlob_id& id =
        dynamic_cast<const CBlobIdFor<TBlob_id>&>(*blob_id).GetValue();
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
    int oid = GetOid(idh);
    if ( oid != -1 ) {
        blob_id = new CBlobIdFor<TBlob_id>(TBlob_id(oid, idh));
    }
    return blob_id;
}


CBlastDbDataLoader::TTSE_Lock
CBlastDbDataLoader::GetBlobById(const TBlobId& blob_id)
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
 * Revision 1.40  2006/05/08 15:54:37  ucko
 * Tweak settings-retrieval APIs to account for the fact that the
 * supplied default string value may be a reference to a temporary, and
 * therefore unsafe to return by reference.
 *
 * Revision 1.39  2006/03/08 17:45:26  camacho
 * Remove compiler warning
 *
 * Revision 1.38  2006/02/13 18:21:00  bealer
 * - break out of loop once result is known.
 *
 * Revision 1.37  2006/01/04 16:23:27  bealer
 * - Check filtering of Seq-ids.
 *
 * Revision 1.36  2005/12/08 14:28:16  vasilche
 * Use TSeqPos for sequence length.
 * Pepresent sequences as raw Seq-data when they are short enough.
 *
 * Revision 1.35  2005/10/28 18:33:10  vasilche
 * Fixed selection of correct defline for gis.
 *
 * Revision 1.34  2005/10/26 14:36:43  vasilche
 * Updated for new CBlobId interface.
 * Removed extra maps for split info - used information from chunk info.
 * Fixed double loading of sequences.
 * Do not split descriptors as they are loaded anyway.
 *
 * Revision 1.33  2005/09/01 14:33:38  bealer
 * - Use Seq-id from Bioseq instead of user's Seq-id to register Seq-data.
 *
 * Revision 1.32  2005/08/31 15:12:35  bealer
 * - Allow eUnknown.
 *
 * Revision 1.31  2005/08/04 17:39:47  bealer
 * - Use single-oid interface for blastdb data loader.
 *
 * Revision 1.30  2005/07/28 11:29:57  ssikorsk
 * Removed CInterfaceVersion from CPluginManager template parameters.
 *
 * Revision 1.29  2005/07/20 16:38:26  didenko
 * Eugene: load blobs only when sequence is requested, excluding any annotations requests.
 *
 * Revision 1.28  2005/07/18 15:26:02  vasilche
 * Set ALL bits when adding split description info.
 *
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
