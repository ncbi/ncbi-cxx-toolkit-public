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
*   Data loader implementation that uses the blast databases at NCBI
*
* ===========================================================================
*/
#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <objtools/data_loaders/blastdb/bdbloader_rmt.hpp>
#include <objmgr/impl/tse_loadlock.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <objmgr/data_loader_factory.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include "remote_blastdb_adapter.hpp"

//=======================================================================
// BlastDbDataLoader Public interface 
//

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CRemoteBlastDbDataLoader::TRegisterLoaderInfo 
CRemoteBlastDbDataLoader::RegisterInObjectManager(
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

inline
string DbTypeToStr(CRemoteBlastDbDataLoader::EDbType dbtype)
{
    switch (dbtype) {
    case CRemoteBlastDbDataLoader::eNucleotide: return "Nucleotide";
    case CRemoteBlastDbDataLoader::eProtein:    return "Protein";
    default:                              return "Unknown";
    }
}


inline
CSeqDB::ESeqType DbTypeToSeqType(CRemoteBlastDbDataLoader::EDbType dbtype)
{
    switch (dbtype) {
    case CRemoteBlastDbDataLoader::eNucleotide: return CSeqDB::eNucleotide;
    case CRemoteBlastDbDataLoader::eProtein:    return CSeqDB::eProtein;
    default:                              return CSeqDB::eUnknown;
    }
}

inline
CRemoteBlastDbDataLoader::EDbType SeqTypeToDbType(CSeqDB::ESeqType seq_type)
{
    switch (seq_type) {
    case CSeqDB::eNucleotide:   return CRemoteBlastDbDataLoader::eNucleotide;
    case CSeqDB::eProtein:      return CRemoteBlastDbDataLoader::eProtein;
    default:                    return CRemoteBlastDbDataLoader::eUnknown;
    }
}

static const string kPrefix = "REMOTE_BLASTDB_";
string CRemoteBlastDbDataLoader::GetLoaderNameFromArgs(const SBlastDbParam& param)
{
    return kPrefix + param.m_DbName + DbTypeToStr(param.m_DbType);
}

CRemoteBlastDbDataLoader::CRemoteBlastDbDataLoader(const string& loader_name, 
                                                   const SBlastDbParam & param)
{
    m_DBName = param.m_DbName;
    m_DBType = param.m_DbType;
    m_UseFixedSizeSlices = param.m_UseFixedSizeSlices;
    SetName(loader_name);
    _ASSERT(param.m_BlastDbHandle.Empty());
    m_BlastDb.Reset();
    if (m_DBName.empty()) {
        NCBI_THROW(CSeqDBException, eArgErr, "Empty BLAST database name");
    }
    const CSeqDB::ESeqType dbtype = DbTypeToSeqType(m_DBType);
    m_BlastDb.Reset(new CRemoteBlastDbAdapter(m_DBName, dbtype,
                                              m_UseFixedSizeSlices));
    _ASSERT(m_BlastDb.NotEmpty());
    _TRACE("Using " << GetLoaderNameFromArgs(param) << " data loader");
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

CRemoteBlastDbDataLoader::TTSE_Lock
CRemoteBlastDbDataLoader::GetBlobById(const TBlobId& blob_id)
{
    CTSE_LoadLock lock = GetDataSource()->GetTSE_LoadLock(blob_id);
    if ( !lock.IsLoaded() ) {
        const TBlastDbId& id =
            dynamic_cast<const CBlobIdBlastDb&>(*blob_id).GetValue();
        x_LoadData(id.second, id.first, lock, kRmtSequenceSliceSize);
    }
    return lock;
}

void
CRemoteBlastDbDataLoader::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
    // dummy assignment to eliminate compiler and doxygen warnings
    depth = depth;  
    ddc.SetFrame("CRemoteBlastDbDataLoader");
    DebugDumpValue(ddc,"m_DBName", m_DBName);
    DebugDumpValue(ddc,"m_DBType", m_DBType);
    DebugDumpValue(ddc,"m_UseFixedSizeSlices", m_UseFixedSizeSlices);
   
}

END_SCOPE(objects)

// ===========================================================================

USING_SCOPE(objects);

void DataLoaders_Register_RmtBlastDb(void)
{
    // Typedef to silence compiler warning.  A better solution to this
    // problem is probably possible.
    
    typedef void(*TArgFuncType)(list<CPluginManager<CDataLoader>
                                ::SDriverInfo> &,
                                CPluginManager<CDataLoader>
                                ::EEntryPointRequest);
    
    RegisterEntryPoint<CDataLoader>((TArgFuncType)
                                    NCBI_EntryPoint_DataLoader_RmtBlastDb);
}

const string kDataLoader_RmtBlastDb_DriverName("rmt_blastdb");

/// Data Loader Factory for BlastDbDataLoader
///
/// This class provides an interface which builds an instance of the
/// BlastDbDataLoader and registers it with the object manager.

class CRmtBlastDb_DataLoaderCF : public CDataLoaderFactory
{
public:
    /// Constructor
    CRmtBlastDb_DataLoaderCF(void)
        : CDataLoaderFactory(kDataLoader_BlastDb_DriverName) {}
    
    /// Destructor
    virtual ~CRmtBlastDb_DataLoaderCF(void) {}
    
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


CDataLoader* CRmtBlastDb_DataLoaderCF::CreateAndRegister(
    CObjectManager& om,
    const TPluginManagerParamTree* params) const
{
    if ( !ValidParams(params) ) {
        // Use constructor without arguments
        return CRemoteBlastDbDataLoader::RegisterInObjectManager(om).GetLoader();
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
        CRemoteBlastDbDataLoader::EDbType dbtype = CRemoteBlastDbDataLoader::eUnknown;
        if ( !dbtype_str.empty() ) {
            if (NStr::CompareNocase(dbtype_str, "Nucleotide") == 0) {
                dbtype = CRemoteBlastDbDataLoader::eNucleotide;
            }
            else if (NStr::CompareNocase(dbtype_str, "Protein") == 0) {
                dbtype = CRemoteBlastDbDataLoader::eProtein;
            }
        }
        return CRemoteBlastDbDataLoader::RegisterInObjectManager(
            om,
            dbname,
            dbtype,
            true,   // use_fixed_size_slices
            GetIsDefault(params),
            GetPriority(params)).GetLoader();
    }
    // IsDefault and Priority arguments may be specified
    return CRemoteBlastDbDataLoader::RegisterInObjectManager(om).GetLoader();
}


void NCBI_EntryPoint_DataLoader_RmtBlastDb(
    CPluginManager<CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<CDataLoader>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CRmtBlastDb_DataLoaderCF>::
        NCBI_EntryPointImpl(info_list, method);
}


void NCBI_EntryPoint_xloader_blastdb_rmt(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_DataLoader_RmtBlastDb(info_list, method);
}


END_NCBI_SCOPE
