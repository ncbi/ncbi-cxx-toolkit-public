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
 * File Description: SRA file data loader
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/general/general__.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqres/seqres__.hpp>

#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/tse_loadlock.hpp>
#include <objmgr/data_loader_factory.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>

#include <sra/data_loaders/sra/sraloader.hpp>
#include <sra/data_loaders/sra/impl/sraloader_impl.hpp>
#include <sra/readers/ncbi_traces_path.hpp>

BEGIN_NCBI_SCOPE

class CObject;

NCBI_PARAM_DECL(bool, SRA_LOADER, TRIM);
NCBI_PARAM_DEF_EX(bool, SRA_LOADER, TRIM, false,
                  eParam_NoThread, SRA_LOADER_TRIM);

BEGIN_SCOPE(objects)

class CDataLoader;

BEGIN_LOCAL_NAMESPACE;

class CLoaderFilter : public CObjectManager::IDataLoaderFilter {
public:
    bool IsDataLoaderMatches(CDataLoader& loader) const {
        return dynamic_cast<CSRADataLoader*>(&loader) != 0;
    }
};


class CRevoker {
public:
    ~CRevoker() {
        CLoaderFilter filter;
        CObjectManager::GetInstance()->RevokeDataLoaders(filter);
    }
};
static CRevoker s_Revoker;

END_LOCAL_NAMESPACE;


/////////////////////////////////////////////////////////////////////////////
// CSRABlobId
/////////////////////////////////////////////////////////////////////////////

class CSRABlobId : public CBlobId
{
public:
    CSRABlobId(const string& acc, unsigned spot_id);
    ~CSRABlobId(void);

    string m_Accession;
    unsigned m_SpotId;

    string ToString(void) const;
    bool operator<(const CBlobId& id) const;
    bool operator==(const CBlobId& id) const;
};


CSRABlobId::CSRABlobId(const string& acc, unsigned spot_id)
    : m_Accession(acc), m_SpotId(spot_id)
{
}


CSRABlobId::~CSRABlobId(void)
{
}


string CSRABlobId::ToString(void) const
{
    CNcbiOstrstream out;
    out << m_Accession << '.' << m_SpotId;
    return CNcbiOstrstreamToString(out);
}


bool CSRABlobId::operator<(const CBlobId& id) const
{
    const CSRABlobId& sra2 = dynamic_cast<const CSRABlobId&>(id);
    return m_Accession < sra2.m_Accession ||
        (m_Accession == sra2.m_Accession && m_SpotId < sra2.m_SpotId);
}


bool CSRABlobId::operator==(const CBlobId& id) const
{
    const CSRABlobId& sra2 = dynamic_cast<const CSRABlobId&>(id);
    return m_Accession == sra2.m_Accession && m_SpotId == sra2.m_SpotId;
}


/////////////////////////////////////////////////////////////////////////////
// CSRADataLoader_Impl
/////////////////////////////////////////////////////////////////////////////


static bool GetTrimParam(void)
{
    static NCBI_PARAM_TYPE(SRA_LOADER, TRIM) s_Value;
    return s_Value.Get();
}


CSRADataLoader_Impl::CSRADataLoader_Impl(CSraMgr::ETrim trim)
    : m_Mgr(trim)
{
}


CSRADataLoader_Impl::~CSRADataLoader_Impl(void)
{
}


CRef<CSeq_entry> CSRADataLoader_Impl::LoadSRAEntry(const string& accession,
                                                   unsigned spot_id)
{
    CMutexGuard LOCK(m_Mutex);
    if ( m_Run.GetAccession() != accession ) {
        m_Run.Init(m_Mgr, accession);
    }
    return m_Run.GetSpotEntry(spot_id);
}


CSeq_inst::TMol CSRADataLoader_Impl::GetSequenceType(const string& accession,
                                                     unsigned spot_id,
                                                     unsigned read_id)
{
    CMutexGuard LOCK(m_Mutex);
    if ( m_Run.GetAccession() != accession ) {
        m_Run.Init(m_Mgr, accession);
    }
    return m_Run.GetSequenceType(spot_id, read_id);
}


TSeqPos CSRADataLoader_Impl::GetSequenceLength(const string& accession,
                                               unsigned spot_id,
                                               unsigned read_id)
{
    CMutexGuard LOCK(m_Mutex);
    if ( m_Run.GetAccession() != accession ) {
        m_Run.Init(m_Mgr, accession);
    }
    return m_Run.GetSequenceLength(spot_id, read_id);
}


/////////////////////////////////////////////////////////////////////////////
// CSRADataLoader
/////////////////////////////////////////////////////////////////////////////

CSRADataLoader::SLoaderParams::SLoaderParams(void)
    : m_Trim(GetTrimParam())
{
}


CSRADataLoader::SLoaderParams::SLoaderParams(bool trim)
    : m_Trim(trim)
{
}


CSRADataLoader::SLoaderParams::~SLoaderParams(void)
{
}


CSRADataLoader::TRegisterLoaderInfo CSRADataLoader::RegisterInObjectManager(
    CObjectManager& om,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    SLoaderParams params;
    TMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


CSRADataLoader::TRegisterLoaderInfo CSRADataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const string& rep_path,
    const string& vol_path,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    SLoaderParams params;
    params.m_RepPath = rep_path;
    params.m_VolPath = vol_path;
    TMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


CSRADataLoader::TRegisterLoaderInfo CSRADataLoader::RegisterInObjectManager(
    CObjectManager& om,
    ETrim trim,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    SLoaderParams params(trim == eTrim);
    TMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


CSRADataLoader::TRegisterLoaderInfo CSRADataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const string& rep_path,
    const string& vol_path,
    ETrim trim,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    SLoaderParams params(trim == eTrim);
    params.m_RepPath = rep_path;
    params.m_VolPath = vol_path;
    TMaker maker(params);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


string CSRADataLoader::GetLoaderNameFromArgs(const SLoaderParams& params)
{
    string ret = "SRADataLoader";
    if ( params.m_Trim ) {
        ret += "Trim";
    }
    if ( !params.m_RepPath.empty() || !params.m_VolPath.empty() ) {
        ret += ":";
        ret += params.m_RepPath;
        ret += ":";
        ret += params.m_VolPath;
    }
    return ret;
}


string CSRADataLoader::GetLoaderNameFromArgs(void)
{
    SLoaderParams params;
    return GetLoaderNameFromArgs(params);
}


string CSRADataLoader::GetLoaderNameFromArgs(const string& rep_path,
                                             const string& vol_path)
{
    SLoaderParams params;
    params.m_RepPath = rep_path;
    params.m_VolPath = vol_path;
    return GetLoaderNameFromArgs(params);
}


string CSRADataLoader::GetLoaderNameFromArgs(ETrim trim)
{
    SLoaderParams params(trim == eTrim);
    return GetLoaderNameFromArgs(params);
}


string CSRADataLoader::GetLoaderNameFromArgs(const string& rep_path,
                                             const string& vol_path,
                                             ETrim trim)
{
    SLoaderParams params(trim == eTrim);
    params.m_RepPath = rep_path;
    params.m_VolPath = vol_path;
    return GetLoaderNameFromArgs(params);
}


CSRADataLoader::CSRADataLoader(const string& loader_name,
                               const SLoaderParams& params)
    : CDataLoader(loader_name)
{
    CSraMgr::ETrim trim = params.m_Trim? CSraMgr::eTrim: CSraMgr::eNoTrim;
    m_Impl = new CSRADataLoader_Impl(trim);
}


CSRADataLoader::~CSRADataLoader(void)
{
}


typedef pair<CRef<CSRABlobId>, unsigned> TReadId;

static TReadId sx_GetReadId(const string& sra, bool with_chunk)
{
    SIZE_TYPE dot1 = sra.find('.');
    if ( dot1 == NPOS ) {
        return TReadId();
    }
    SIZE_TYPE dot2 = with_chunk? sra.find('.', dot1+1): sra.size();
    if ( dot2 == NPOS || dot1+1 >= dot2 || sra[dot1+1] == '0' ||
         (with_chunk && (dot2+2 != sra.size() ||
                         (sra[dot2+1] != '2' && sra[dot2+1] != '4') )) ) {
        return TReadId();
    }
    unsigned spot_id =
        NStr::StringToUInt(CTempString(sra.data()+dot1+1, dot2-dot1-1));
    TReadId ret;
    ret.first = new CSRABlobId(sra.substr(0, dot1), spot_id);
    ret.second = sra[dot2+1] - '0';
    return ret;
}


static TReadId sx_GetReadId(const CSeq_id_Handle& idh)
{
    if ( idh.Which() != CSeq_id::e_General ) {
        return TReadId();
    }
    CConstRef<CSeq_id> id = idh.GetSeqId();
    const CDbtag& general = id->GetGeneral();
    if ( general.GetDb() != "SRA") {
        return TReadId();
    }
    return sx_GetReadId(general.GetTag().GetStr(), true);
}


CDataLoader::TBlobId CSRADataLoader::GetBlobId(const CSeq_id_Handle& idh)
{
    return TBlobId(sx_GetReadId(idh).first);
}


CDataLoader::TBlobId
CSRADataLoader::GetBlobIdFromString(const string& str) const
{
    return TBlobId(sx_GetReadId(str, false).first);
}


bool CSRADataLoader::CanGetBlobById(void) const
{
    return true;
}


CDataLoader::TTSE_LockSet
CSRADataLoader::GetRecords(const CSeq_id_Handle& idh,
                           EChoice /* choice */)
{
    TTSE_LockSet locks;
    TBlobId blob_id = GetBlobId(idh);
    if ( blob_id ) {
        locks.insert(GetBlobById(blob_id));
    }
    return locks;
}


CDataLoader::TTSE_Lock
CSRADataLoader::GetBlobById(const TBlobId& blob_id)
{
    CTSE_LoadLock load_lock = GetDataSource()->GetTSE_LoadLock(blob_id);
    if ( !load_lock.IsLoaded() ) {
        const CSRABlobId& sra_id = dynamic_cast<const CSRABlobId&>(*blob_id);
        CRef<CSeq_entry> entry =
            m_Impl->LoadSRAEntry(sra_id.m_Accession, sra_id.m_SpotId);
        if ( entry ) {
            load_lock->SetSeq_entry(*entry);
        }
        load_lock.SetLoaded();
    }
    return load_lock;
}


TSeqPos CSRADataLoader::GetSequenceLength(const CSeq_id_Handle& idh)
{
    TReadId read_id = sx_GetReadId(idh);
    if ( read_id.first ) {
        const CSRABlobId& sra_id = *read_id.first;
        return m_Impl->GetSequenceLength(sra_id.m_Accession,
                                         sra_id.m_SpotId,
                                         read_id.second);
    }
    return kInvalidSeqPos;
}


CSeq_inst::TMol CSRADataLoader::GetSequenceType(const CSeq_id_Handle& idh)
{
    TReadId read_id = sx_GetReadId(idh);
    if ( read_id.first ) {
        const CSRABlobId& sra_id = *read_id.first;
        return m_Impl->GetSequenceType(sra_id.m_Accession,
                                       sra_id.m_SpotId,
                                       read_id.second);
    }
    return CSeq_inst::eMol_not_set;
}


END_SCOPE(objects)

// ===========================================================================

USING_SCOPE(objects);

void DataLoaders_Register_SRA(void)
{
    RegisterEntryPoint<CDataLoader>(NCBI_EntryPoint_DataLoader_Sra);
}


const string kDataLoader_Sra_DriverName("sra");

class CSRA_DataLoaderCF : public CDataLoaderFactory
{
public:
    CSRA_DataLoaderCF(void)
        : CDataLoaderFactory(kDataLoader_Sra_DriverName) {}
    virtual ~CSRA_DataLoaderCF(void) {}

protected:
    virtual CDataLoader* CreateAndRegister(
        CObjectManager& om,
        const TPluginManagerParamTree* params) const;
};


CDataLoader* CSRA_DataLoaderCF::CreateAndRegister(
    CObjectManager& om,
    const TPluginManagerParamTree* params) const
{
    if ( !ValidParams(params) ) {
        // Use constructor without arguments
        return CSRADataLoader::RegisterInObjectManager(om).GetLoader();
    }
    // IsDefault and Priority arguments may be specified
    return CSRADataLoader::RegisterInObjectManager(
        om,
        GetIsDefault(params),
        GetPriority(params)).GetLoader();
}


void NCBI_EntryPoint_DataLoader_Sra(
    CPluginManager<CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<CDataLoader>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CSRA_DataLoaderCF>::NCBI_EntryPointImpl(info_list, method);
}


void NCBI_EntryPoint_xloader_sra(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_DataLoader_Sra(info_list, method);
}


END_NCBI_SCOPE
