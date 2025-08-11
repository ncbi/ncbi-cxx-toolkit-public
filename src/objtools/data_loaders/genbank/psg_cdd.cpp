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
 * Author: Eugene Vasilchenko, Aleksey Grichenko
 *
 * File Description: PSG support code for CDD entries
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <objtools/data_loaders/genbank/impl/psg_cdd.hpp>
#include <objtools/data_loaders/genbank/psg_loader.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/tse_split_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/tse_loadlock.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>
#include <util/compress/compress.hpp>
#include <util/compress/stream.hpp>
#include <util/compress/zlib.hpp>

#if defined(HAVE_PSG_LOADER)

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(objects);
BEGIN_NAMESPACE(psgl);


/////////////////////////////////////////////////////////////////////////////
// common PSG loader/processors stuff
/////////////////////////////////////////////////////////////////////////////


bool s_GetBlobByIdShouldFail = false;


NCBI_PARAM_DECL(unsigned, PSG_LOADER, DEBUG);
NCBI_PARAM_DEF_EX(unsigned, PSG_LOADER, DEBUG, 1,
                  eParam_NoThread, PSG_LOADER_DEBUG);
typedef NCBI_PARAM_TYPE(PSG_LOADER, DEBUG) TPSG_Debug;

unsigned s_GetDebugLevel()
{
    static auto value = TPSG_Debug::GetDefault();
    return value;
}


CSeq_id_Handle PsgIdToHandle(const CPSG_BioId& id)
{
    string sid = id.GetId();
    if (sid.empty()) return CSeq_id_Handle();
    try {
        return CSeq_id_Handle::GetHandle(sid);
    }
    catch (exception& exc) {
        ERR_POST("CPSGDataLoader: cannot parse Seq-id "<<sid<<": "<<exc.what());
    }
    return CSeq_id_Handle();
}


void UpdateOMBlobId(CTSE_LoadLock& load_lock, const CConstRef<CPsgBlobId>& dl_blob_id)
{
    // copy extra TSE parameters stored in TSE blob id object from the request blob id
    const CPsgBlobId* om_blob_id = dynamic_cast<const CPsgBlobId*>(&*load_lock->GetBlobId());
    _ASSERT(om_blob_id);
    if ( om_blob_id != dl_blob_id ) {
        if ( om_blob_id->GetTSEName() != dl_blob_id->GetTSEName() ) {
            const_cast<CPsgBlobId*>(om_blob_id)->SetTSEName(dl_blob_id->GetTSEName());
        }
        if ( dl_blob_id->HasBioseqIsDead() ) {
            if ( !om_blob_id->HasBioseqIsDead() ||
                 om_blob_id->BioseqIsDead() != dl_blob_id->BioseqIsDead() ) {
                const_cast<CPsgBlobId*>(om_blob_id)->SetBioseqIsDead(dl_blob_id->BioseqIsDead());
            }
        }
        else {
            if ( om_blob_id->HasBioseqIsDead() ) {
                const_cast<CPsgBlobId*>(om_blob_id)->ResetBioseqIsDead();
            }
        }
    }
    // assign extra TSE parameters to the TSE from its blob id
    if ( om_blob_id->HasTSEName() ) {
        load_lock->SetName(om_blob_id->GetTSEName());
    }
    if ( om_blob_id->BioseqIsDead() ) {
        load_lock->SetBlobState(CBioseq_Handle::fState_dead);
    }
}


/////////////////////////////////////////////////////////////////////////////
// serialization stuff
/////////////////////////////////////////////////////////////////////////////


CObjectIStream* GetBlobDataStream(const CPSG_BlobInfo& blob_info,
                                  const CPSG_BlobData& blob_data)
{
    istream& data_stream = blob_data.GetStream();
    CNcbiIstream* in = &data_stream;
    unique_ptr<CNcbiIstream> z_stream;
    CObjectIStream* ret = nullptr;

    if (blob_info.GetCompression() == "gzip") {
        z_stream.reset(new CCompressionIStream(data_stream,
                                               new CZipStreamDecompressor(CZipCompression::fGZip),
                                               CCompressionIStream::fOwnProcessor));
        in = z_stream.get();
    }
    else if (!blob_info.GetCompression().empty()) {
        _TRACE("Unsupported data compression: '" << blob_info.GetCompression() << "'");
        return nullptr;
    }

    EOwnership own = z_stream.get() ? eTakeOwnership : eNoOwnership;
    if (blob_info.GetFormat() == "asn.1") {
        ret = CObjectIStream::Open(eSerial_AsnBinary, *in, own);
    }
    else if (blob_info.GetFormat() == "asn1-text") {
        ret = CObjectIStream::Open(eSerial_AsnText, *in, own);
    }
    else if (blob_info.GetFormat() == "xml") {
        ret = CObjectIStream::Open(eSerial_Xml, *in, own);
    }
    else if (blob_info.GetFormat() == "json") {
        ret = CObjectIStream::Open(eSerial_Json, *in, own);
    }
    else {
        _TRACE("Unsupported data format: '" << blob_info.GetFormat() << "'");
        return nullptr;
    }
    _ASSERT(ret);
    z_stream.release();
    return ret;
}


/////////////////////////////////////////////////////////////////////////////
// CDD stuff
/////////////////////////////////////////////////////////////////////////////


SCDDIds x_GetCDDIds(const CDataLoader::TIds& ids)
{
    SCDDIds ret;
    bool is_protein = true;
    for ( auto id : ids ) {
        if ( id.IsGi() ) {
            ret.gi = id;
            continue;
        }
        if ( id.Which() == CSeq_id::e_Pdb ||
             id.Which() == CSeq_id::e_Pir ||
             id.Which() == CSeq_id::e_Prf ) {
            if ( !ret.acc_ver ) {
                ret.acc_ver = id;
            }
            continue;
        }
        auto seq_id = id.GetSeqId();
        if ( auto text_id = seq_id->GetTextseq_Id() ) {
            auto acc_type = seq_id->IdentifyAccession();
            if ( acc_type & CSeq_id::fAcc_nuc ) {
                is_protein = false;
                break;
            }
            else if ( text_id->IsSetAccession() && text_id->IsSetVersion() &&
                      (acc_type & CSeq_id::fAcc_prot) ) {
                is_protein = true;
                ret.acc_ver = CSeq_id_Handle::GetHandle(CSeq_id(seq_id->Which(),
                                                                text_id->GetAccession(),
                                                                kEmptyStr,
                                                                text_id->GetVersion()));
            }
        }
    }
    if (!is_protein) {
        ret.gi.Reset();
        ret.acc_ver.Reset();
    }
    return ret;
}


string x_MakeLocalCDDEntryId(const SCDDIds& cdd_ids)
{
    ostringstream str;
    _ASSERT(cdd_ids.gi && cdd_ids.gi.IsGi());
    str << kLocalCDDEntryIdPrefix << cdd_ids.gi.GetGi();
    if ( cdd_ids.acc_ver ) {
        str << kLocalCDDEntryIdSeparator << cdd_ids.acc_ver;
    }
    return str.str();
}


bool x_IsLocalCDDEntryId(const CPsgBlobId& blob_id)
{
    return NStr::StartsWith(blob_id.ToPsgId(), kLocalCDDEntryIdPrefix);
}


SCDDIds x_ParseLocalCDDEntryId(const CPsgBlobId& blob_id)
{
    if ( !x_IsLocalCDDEntryId(blob_id) ) {
        return SCDDIds();
    }
    istringstream str(blob_id.ToPsgId().substr(strlen(kLocalCDDEntryIdPrefix)));
    TIntId gi_id = 0;
    str >> gi_id;
    if ( !gi_id ) {
        return SCDDIds();
    }
    CSeq_id_Handle gi = CSeq_id_Handle::GetGiHandle(GI_FROM(TIntId, gi_id));
    CSeq_id_Handle acc_ver;
    if ( str.get() == kLocalCDDEntryIdSeparator ) {
        string extra;
        str >> extra;
        acc_ver = CSeq_id_Handle::GetHandle(extra);
    }
    return { gi, acc_ver };
}


bool x_ParseLocalCDDEntryId(const CPsgBlobId& blob_id, SCDDIds& cdd_ids)
{
    return (cdd_ids = x_ParseLocalCDDEntryId(blob_id));
}


CPSG_BioId x_LocalCDDEntryIdToBioId(const SCDDIds& cdd_ids)
{
    return { NStr::NumericToString(cdd_ids.gi.GetGi()), CSeq_id::e_Gi };
}


CPSG_BioId x_LocalCDDEntryIdToBioId(const CPsgBlobId& blob_id)
{
    const string& str = blob_id.ToPsgId();
    size_t start = strlen(kLocalCDDEntryIdPrefix);
    size_t end = str.find(kLocalCDDEntryIdSeparator, start);
    return { str.substr(start, end-start), CSeq_id::e_Gi };
}


CRef<CTSE_Chunk_Info> x_CreateLocalCDDEntryChunk(const SCDDIds& cdd_ids)
{
    if ( !cdd_ids.gi && !cdd_ids.acc_ver ) {
        return null;
    }
    CRange<TSeqPos> range = CRange<TSeqPos>::GetWhole();
    CRef<CTSE_Chunk_Info> chunk(new CTSE_Chunk_Info(kDelayedMain_ChunkId));
    // add main annot types
    CAnnotName name = kCDDAnnotName;
    CSeqFeatData::ESubtype subtypes[] = {
        CSeqFeatData::eSubtype_region,
        CSeqFeatData::eSubtype_site
    };
    set<CSeq_id_Handle> ids;
    for ( int i = 0; i < 2; ++i ) {
        const CSeq_id_Handle& id = i ? cdd_ids.acc_ver : cdd_ids.gi;
        if ( !id ) {
            continue;
        }
        ids.insert(id);
    }
    if ( s_GetDebugLevel() >= 6 ) {
        for ( auto& id : ids ) {
            LOG_POST(Info<<"CPSGDataLoader: CDD synthetic id "<<MSerial_AsnText<<*id.GetSeqId());
        }
    }
    for ( auto subtype : subtypes ) {
        SAnnotTypeSelector type(subtype);
        for ( auto& id : ids ) {
            chunk->x_AddAnnotType(name, type, id, range);
        }
    }
    return chunk;
}


CTSE_Lock x_CreateLocalCDDEntry(CDataSource* data_source, const SCDDIds& cdd_ids)
{
    if ( auto chunk = x_CreateLocalCDDEntryChunk(cdd_ids) ) {
        CRef<CPsgBlobId> dl_blob_id(new CPsgBlobId(x_MakeLocalCDDEntryId(cdd_ids)));
        dl_blob_id->SetTSEName(kCDDAnnotName);
        CTSE_LoadLock load_lock = data_source->GetTSE_LoadLock(CBlobIdKey(dl_blob_id));
        _ASSERT(load_lock);
        if ( load_lock ) {
            if ( !load_lock.IsLoaded() ) {
                if ( s_GetBlobByIdShouldFail ) {
                    return CTSE_Lock();
                }
                UpdateOMBlobId(load_lock, dl_blob_id);
                load_lock->GetSplitInfo().AddChunk(*chunk);
                _ASSERT(load_lock->x_NeedsDelayedMainChunk());
                load_lock.SetLoaded();
            }
            return load_lock;
        }
    }
    return CTSE_Lock();
}


void x_CreateEmptyLocalCDDEntry(CDataSource* data_source, CTSE_Chunk_Info* chunk)
{
    CTSE_LoadLock load_lock = data_source->GetTSE_LoadLock(chunk->GetBlobId());
    _ASSERT(load_lock);
    _ASSERT(load_lock.IsLoaded());
    _ASSERT(load_lock->HasNoSeq_entry());
    CRef<CSeq_entry> entry(new CSeq_entry);
    entry->SetSet().SetSeq_set();
    if ( s_GetDebugLevel() >= 8 ) {
        LOG_POST(Info<<"PSG loader: TSE "<<load_lock->GetBlobId().ToString()<<" "<<
                 " created empty CDD entry");
    }
    load_lock->SetSeq_entry(*entry);
    chunk->SetLoaded();
}


bool x_IsEmptyCDD(const CTSE_Info& tse)
{
    // check core Seq-entry content
    auto core = tse.GetTSECore();
    if ( !core->IsSet() ) {
        // wrong entry type
        return false;
    }
    auto& seqset = core->GetSet();
    return seqset.GetSeq_set().empty() && seqset.GetAnnot().empty();
}


END_NAMESPACE(psgl);
END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // HAVE_PSG_LOADER
