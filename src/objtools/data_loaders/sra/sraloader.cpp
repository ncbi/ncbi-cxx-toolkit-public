/*  $Id $
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

#include <objtools/data_loaders/sra/sraloader.hpp>
#include <objtools/data_loaders/sra/impl/sraloader_impl.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataLoader;


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
        m_Accession == sra2.m_Accession && m_SpotId < sra2.m_SpotId;
}


bool CSRABlobId::operator==(const CBlobId& id) const
{
    const CSRABlobId& sra2 = dynamic_cast<const CSRABlobId&>(id);
    return m_Accession == sra2.m_Accession && m_SpotId == sra2.m_SpotId;
}


/////////////////////////////////////////////////////////////////////////////
// SRA access
/////////////////////////////////////////////////////////////////////////////


static const bool trace = false;


CSraMgr::CSraMgr(const string& rep_path, const string& vol_path)
{
    if ( rc_t rc = SRAMgrMakeRead(x_InitPtr()) ) {
        NCBI_THROW_FMT(CSraException, eOtherError,
                       "Cannot open SRAMgr: " << rc);
    }
    if ( !rep_path.empty() ) {
        if ( rc_t rc = SRAMgrAddRepPath(*this, rep_path.c_str()) ) {
            NCBI_THROW_FMT(CSraException, eOtherError,
                           "Cannot add SRAMgr rep path: " << rc
                           << ": " << rep_path);
        }
    }
    if ( !vol_path.empty() ) {
        if ( rc_t rc = SRAMgrAddVolPath(*this, vol_path.c_str()) ) {
            NCBI_THROW_FMT(CSraException, eOtherError,
                           "Cannot add SRAMgr vol path: " << rc
                           << ": " << vol_path);
        }
    }
}


CRef<CSeq_entry> CSraMgr::GetSpotEntry(const string& sra) const
{
    if ( trace ) {
        NcbiCout << "Generating Seq-entry for spot "<<sra<<NcbiEndl;
    }
    SIZE_TYPE dot = sra.find('.');
    string acc;
    spotid_t spot_id = 0;
    if ( dot == NPOS ) {
        ERR_POST("No spot id defined");
        return null;
    }
    else {
        acc = sra.substr(0, dot);
        spot_id = NStr::StringToUInt(sra.substr(dot+1));
    }
    CSraRun run(*this, acc);
    return run.GetSpotEntry(spot_id);
}


CRef<CSeq_entry> CSraMgr::GetSpotEntry(const string& sra,
                                       AutoPtr<CSraRun>& run) const
{
    if ( trace ) {
        NcbiCout << "Generating Seq-entry for spot "<<sra<<NcbiEndl;
    }
    SIZE_TYPE dot = sra.find('.');
    string acc;
    spotid_t spot_id = 0;
    if ( dot == NPOS ) {
        ERR_POST("No spot id defined");
        return null;
    }
    else {
        acc = sra.substr(0, dot);
        spot_id = NStr::StringToUInt(sra.substr(dot+1));
    }
    if ( !run || run->GetAccession() != acc ) {
        run.reset(new CSraRun(*this, acc));
    }
    return run->GetSpotEntry(spot_id);
}


void CSraRun::Init(const CSraMgr& mgr, const string& acc)
{
    m_Acc = acc;
    if ( rc_t rc = SRAMgrOpenRunRead(mgr, x_InitPtr(), acc.c_str()) ) {
        NCBI_THROW_FMT(CSraException, eOtherError,
                       "Cannot open run read: " << rc
                       << ": " << acc);
    }
    m_Name.Init(*this, "NAME", vdb_ascii_t);
    m_Read.Init(*this, "READ", insdc_fasta_t);
    m_Qual.Init(*this, "QUALITY", ncbi_qual1_t);
    m_SDesc.Init(*this, "SPOT_DESC", sra_spot_desc_t);
    m_RDesc.Init(*this, "READ_DESC", sra_read_desc_t);
}


void CSraColumn::Init(const CSraRun& run, const char* name, const char* type)
{
    if ( rc_t rc = SRATableOpenColumnRead(run, x_InitPtr(),
                                          name, type) ) {
        NCBI_THROW_FMT(CSraException, eOtherError,
                       "Cannot get SRA column: "<<rc
                       <<": "<<name);
    }
}


CRef<CSeq_entry> CSraRun::GetSpotEntry(spotid_t spot_id) const
{
    CRef<CSeq_entry> entry;
    
    if ( trace ) {
        NcbiCout << "Id: " << spot_id << NcbiEndl;
    }
    
    CSraStringValue name(m_Name, spot_id);
    if ( !name ) {
        ERR_POST("Cannot read spot name");
        return entry;
    }

    entry = new CSeq_entry();
    CBioseq_set& seqset = entry->SetSet();
    seqset.SetLevel(0);
    seqset.SetClass(seqset.eClass_other);

    if ( trace ) {
        NcbiCout << "Name: " << name.Value() << NcbiEndl;
    }
    
    CSraValueFor<SRASpotDesc> sdesc(m_SDesc, spot_id);
    if ( trace ) {
        NcbiCout << "Spot desc: \n"
                 << "    spot len: " << sdesc->spot_len << "\n"
                 << "   fixed len: " << sdesc->fixed_len << "\n"
                 << "  signal len: " << sdesc->signal_len << "\n"
                 << "  clip right: " << sdesc->clip_qual_right << "\n"
                 << "   num reads: " << int(sdesc->num_reads)
                 << NcbiEndl;
    }

    CSraValueFor<SRAReadDesc> rdesc(m_RDesc, spot_id);
    if ( trace ) {
        for ( int r = 0; r < sdesc->num_reads; ++r ) {
            const SRAReadDesc& rd = rdesc[r];
            NcbiCout << "Read desc["<<r<<"]: \n"
                     << "    start: " << rd.seg.start << "\n"
                     << "      len: " << rd.seg.len
                     << NcbiEndl;
        }
    }

    CSraStringValue read(m_Read, spot_id);
    if ( trace ) {
        NcbiCout << "Read: " << read.Value() << NcbiEndl;
    }

    CSraBytesValue qual(m_Qual, spot_id);
    if ( trace ) {
        NcbiCout << "Qual: ";
        for ( size_t i = 0; i < 10 && i < qual.GetLength(); ++i ) {
            NcbiCout << " " << int(qual[i]);
        }
        NcbiCout << "... of " << qual.GetLength() << NcbiEndl;
    }

    int seq_count = 0;
    string id_start = GetAccession()+'.'+NStr::UIntToString(spot_id)+'.';
    for ( int r = 0; r < sdesc->num_reads; ++r ) {
        if ( rdesc[r].type != SRA_READ_TYPE_BIOLOGICAL ) {
            continue;
        }
        TSeqPos len = rdesc[r].seg.len;
        if ( len == 0 ) {
            continue;
        }
        TSeqPos start = rdesc[r].seg.start;
        if ( start >= sdesc->clip_qual_right ) {
            //continue;
        }

        CRef<CSeq_entry> seq_entry(new CSeq_entry);
        CBioseq& seq = seq_entry->SetSeq();
        
        CRef<CSeq_id> id(new CSeq_id);
        id->SetGeneral().SetDb("SRA");
        id->SetGeneral().SetTag().SetStr(id_start+NStr::UIntToString(r+1));
        seq.SetId().push_back(id);

        {{
            CRef<CSeqdesc> desc(new CSeqdesc);
            desc->SetTitle(name.Value());
            seq.SetDescr().Set().push_back(desc);
        }}
        {{
            CSeq_inst& inst = seq.SetInst();
            inst.SetRepr(inst.eRepr_raw);
            inst.SetMol(inst.eMol_na);
            inst.SetLength(len);
            inst.SetSeq_data().SetIupacna().Set()
                .assign(read.data()+start, len);
        }}
        {{
            CRef<CSeq_annot> annot(new CSeq_annot);
            CRef<CSeq_graph> graph(new CSeq_graph);
            annot->SetData().SetGraph().push_back(graph);
            graph->SetTitle("Phred Quality");
            graph->SetLoc().SetWhole(*id);
            graph->SetNumval(len);
            CByte_graph& bytes = graph->SetGraph().SetByte();
            bytes.SetAxis(0);
            CByte_graph::TValues& values = bytes.SetValues();
            values.reserve(len);
            int min = kMax_Int;
            int max = kMin_Int;
            for ( size_t i = 0; i < len; ++i ) {
                int v = qual[start+i];
                values.push_back(v);
                if ( v < min ) {
                    min = v;
                }
                if ( v > max ) {
                    max = v;
                }
            }
            bytes.SetMin(min);
            bytes.SetMax(max);

            seq.SetAnnot().push_back(annot);
        }}

        seqset.SetSeq_set().push_back(seq_entry);
        ++seq_count;
    }
    switch ( seq_count ) {
    case 0:
        entry.Reset();
        break;
    case 1:
        entry = seqset.GetSeq_set().front();
        break;
    }
    return entry;
}


const char* CSraException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eNullPtr:    return "eNullPtr";
    case eAddRefFailed: return "eAddRefFailed";
    case eInvalidArg: return "eInvalidArg";
    case eOtherError: return "eOtherError";
    default:          return CException::GetErrCodeString();
    }
}


/////////////////////////////////////////////////////////////////////////////
// CSRADataLoader_Impl
/////////////////////////////////////////////////////////////////////////////


CSRADataLoader_Impl::CSRADataLoader_Impl(void)
    : m_Mgr("/panfs/traces01", "sra0:sra1")
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


/////////////////////////////////////////////////////////////////////////////
// CSRADataLoader
/////////////////////////////////////////////////////////////////////////////

CSRADataLoader::TRegisterLoaderInfo CSRADataLoader::RegisterInObjectManager(
    CObjectManager& om,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    TMaker maker;
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


string CSRADataLoader::GetLoaderNameFromArgs(void)
{
    return "SRADataLoader";
}


CSRADataLoader::CSRADataLoader(const string& loader_name)
    : CDataLoader(loader_name),
      m_Impl(new CSRADataLoader_Impl)
{
}


static CDataLoader::TBlobId sx_GetBlobId(const string& sra, bool with_chunk)
{
    SIZE_TYPE dot1 = sra.find('.');
    if ( dot1 == NPOS ) {
        return CDataLoader::TBlobId();
    }
    SIZE_TYPE dot2 = with_chunk? sra.find('.', dot1+1): sra.size();
    if ( dot2 == NPOS || dot1+1 >= dot2 || sra[dot1+1] == '0' ||
         with_chunk && ( dot2+2 != sra.size() ||
                         sra[dot2+1] != '2' && sra[dot2+1] != '4' ) ) {
        return CDataLoader::TBlobId();
    }
    unsigned spot_id =
        NStr::StringToUInt(CTempString(sra.data()+dot1+1, dot2-dot1-1));
    return CDataLoader::TBlobId(new CSRABlobId(sra.substr(0, dot1), spot_id));
}


CDataLoader::TBlobId CSRADataLoader::GetBlobId(const CSeq_id_Handle& idh)
{
    if ( idh.Which() != CSeq_id::e_General ) {
        return TBlobId();
    }
    CConstRef<CSeq_id> id = idh.GetSeqId();
    const CDbtag& general = id->GetGeneral();
    if ( general.GetDb() != "SRA") {
        return TBlobId();
    }
    return sx_GetBlobId(general.GetTag().GetStr(), true);
}


CDataLoader::TBlobId
CSRADataLoader::GetBlobIdFromString(const string& str) const
{
    return sx_GetBlobId(str, false);
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
