/*  $Id$
 * ===========================================================================
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
 * ===========================================================================
 *
 *  Author:  Anton Butanaev, Eugene Vasilchenko
 *
 *  File Description: Base data reader interface
 *
 */

#include <ncbi_pch.hpp>
#include <objtools/data_loaders/genbank/reader.hpp>
#include <objtools/data_loaders/genbank/split_parser.hpp>
#include <objtools/data_loaders/genbank/request_result.hpp>

#include <serial/pack_string.hpp>
#include <objmgr/annot_selector.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/impl/snp_annot_info.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/handle_range_map.hpp>

#include <objtools/data_loaders/genbank/seqref.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/id2/ID2_Blob_Id.hpp>

#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objectinfo.hpp>
#include <serial/objectiter.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

static const char* const STRING_PACK_ENV = "GENBANK_SNP_PACK_STRINGS";
static const char* const SNP_SPLIT_ENV = "GENBANK_SNP_SPLIT";
static const char* const SNP_TABLE_ENV = "GENBANK_SNP_TABLE";
static const char* const ENV_YES = "YES";

CReader::CReader(void)
{
}


CReader::~CReader(void)
{
}


int CReader::GetConst(const string& ) const
{
    return 0;
}


bool CReader::s_GetEnvFlag(const char* env, bool def_val)
{
    const char* val = ::getenv(env);
    if ( !val ) {
        return def_val;
    }
    string s(val);
    return s == "1" || NStr::CompareNocase(s, ENV_YES) == 0;
}


bool CReader::TryStringPack(void)
{
    static bool use_string_pack =
        CPackString::TryStringPack() && s_GetEnvFlag(STRING_PACK_ENV, true);
    return use_string_pack;
}


void CReader::SetSeqEntryReadHooks(CObjectIStream& in)
{
    if ( !TryStringPack() ) {
        return;
    }

    CObjectTypeInfo type;

    type = CObjectTypeInfo(CType<CObject_id>());
    type.FindVariant("str").SetLocalReadHook(in, new CPackStringChoiceHook);

    type = CObjectTypeInfo(CType<CImp_feat>());
    type.FindMember("key").SetLocalReadHook(in,
                                            new CPackStringClassHook(32, 128));

    type = CObjectTypeInfo(CType<CDbtag>());
    type.FindMember("db").SetLocalReadHook(in, new CPackStringClassHook);

    type = CType<CGb_qual>();
    type.FindMember("qual").SetLocalReadHook(in, new CPackStringClassHook);
}


void CReader::SetSNPReadHooks(CObjectIStream& in)
{
    if ( !TryStringPack() ) {
        return;
    }

    CObjectTypeInfo type;

    type = CType<CGb_qual>();
    type.FindMember("qual").SetLocalReadHook(in, new CPackStringClassHook);
    type.FindMember("val").SetLocalReadHook(in,
                                            new CPackStringClassHook(4, 128));

    type = CObjectTypeInfo(CType<CImp_feat>());
    type.FindMember("key").SetLocalReadHook(in,
                                            new CPackStringClassHook(32, 128));

    type = CObjectTypeInfo(CType<CObject_id>());
    type.FindVariant("str").SetLocalReadHook(in, new CPackStringChoiceHook);

    type = CObjectTypeInfo(CType<CDbtag>());
    type.FindMember("db").SetLocalReadHook(in, new CPackStringClassHook);

    type = CObjectTypeInfo(CType<CSeq_feat>());
    type.FindMember("comment").SetLocalReadHook(in, new CPackStringClassHook);
}


void CReader::LoadBlobs(CReaderRequestResult& result,
                        const string& seq_id,
                        TContentsMask mask)
{
    CLoadLockSeq_ids ids(result, seq_id);
    if ( !ids ) {
        ResolveString(result, seq_id);
        if ( !ids ) {
            return;
        }
    }
    if ( ids->size() == 1 ) {
        LoadBlobs(result, *ids->begin(), mask);
    }
}


void CReader::LoadBlobs(CReaderRequestResult& result,
                        const CSeq_id_Handle& seq_id,
                        TContentsMask mask)
{
    CLoadLockBlob_ids ids(result, seq_id);
    if ( !ids ) {
        ResolveSeq_id(result, seq_id);
        if ( !ids ) {
            return;
        }
    }
    LoadBlobs(result, ids, mask);
}


void CReader::LoadBlobs(CReaderRequestResult& result,
                        CLoadLockBlob_ids blobs,
                        TContentsMask mask)
{
    ITERATE ( CLoadInfoBlob_ids, it, *blobs ) {
        const CBlob_Info& info = it->second;
        if ( (info.GetContentsMask() & mask) != 0 ) {
            LoadBlob(result, it->first);
        }
    }
}


void CReader::LoadChunk(CReaderRequestResult& /*result*/,
                        const TBlob_id& /*blob_id*/, TChunk_id /*chunk_id*/)
{
    NCBI_THROW(CLoaderException, eOtherError,
               "unexpected chunk load request");
}


/////////////////////////////////////////////////////////////////////////////
// CId1ReaderBase
/////////////////////////////////////////////////////////////////////////////


CId1ReaderBase::CId1ReaderBase(void)
{
}


CId1ReaderBase::~CId1ReaderBase()
{
}


bool CId1ReaderBase::TrySNPSplit(void)
{
    static bool snp_split = s_GetEnvFlag(SNP_SPLIT_ENV, true);
    return snp_split;
}


bool CId1ReaderBase::TrySNPTable(void)
{
    static bool snp_table = s_GetEnvFlag(SNP_TABLE_ENV, true);
    return snp_table;
}


void CId1ReaderBase::ResolveString(CReaderRequestResult& result,
                                   const string& seq_id)
{
}


void CId1ReaderBase::ResolveSeq_id(CReaderRequestResult& result,
                                   const CSeq_id_Handle& seq_id)
{
    CLoadLockBlob_ids ids(result, seq_id);
    if ( !ids.IsLoaded() ) {
        CReaderRequestConn conn(result);
        ResolveSeq_id(ids, *seq_id.GetSeqId(), conn);
        ids.SetLoaded();
    }
}


void CId1ReaderBase::LoadBlob(CReaderRequestResult& result,
                              const TBlob_id& blob_id)
{
    CLoadLockBlob blob(result, blob_id);
    if ( !blob.IsLoaded() ) {
        try {
            {{
                CReaderRequestConn conn(result);
                GetBlob(*blob, blob_id, conn);
            }}
            blob.SetLoaded();
            result.AddTSE_Lock(blob);
            result.UpdateLoadedSet();
        }
        catch ( CLoaderException& exc ) {
            if ( exc.GetErrCode() == exc.ePrivateData ) {
                blob->SetSuppressionLevel(CTSE_Info::eSuppression_private);
                blob.SetLoaded();
            }
            else if ( exc.GetErrCode() == exc.eNoData ) {
                blob->SetSuppressionLevel(CTSE_Info::eSuppression_withdrawn);
                blob.SetLoaded();
            }
            else {
                throw;
            }
        }
    }
}


CReader::TBlobVersion
CId1ReaderBase::GetBlobVersion(CReaderRequestResult& result,
                               const TBlob_id& blob_id)
{
    CLoadLockBlob blob(result, blob_id);
    try {
        if ( blob->GetBlobVersion() < 0 ) {
            CReaderRequestConn conn(result);
            blob->SetBlobVersion(GetVersion(blob_id, conn));
        }
        return blob->GetBlobVersion();
    }
    catch ( CLoaderException& exc ) {
        if ( exc.GetErrCode() == exc.ePrivateData ) {
            return 0;
        }
        else if ( exc.GetErrCode() == exc.eNoData ) {
            return 0;
        }
        else {
            throw;
        }
    }
}


void CId1ReaderBase::LoadChunk(CReaderRequestResult& result,
                               const TBlob_id& blob_id, TChunk_id chunk_id)
{
    CLoadLockBlob blob(result, blob_id);
    _ASSERT(blob);
    _ASSERT(blob.IsLoaded());
    CTSE_Chunk_Info& chunk_info = blob->GetChunk(chunk_id);
    if ( chunk_info.NotLoaded() ) {
        CInitGuard init(chunk_info, result);
        if ( init ) {
            {{
                CReaderRequestConn conn(result);
                GetChunk(chunk_info, blob_id, conn);
            }}
            chunk_info.SetLoaded();
        }
    }
}


bool CId1ReaderBase::IsSNPBlob_id(const CBlob_id& blob_id)
{
    return blob_id.GetSat() == CSeqref::eSat_SNP &&
        blob_id.GetSubSat() == CID2_Blob_Id::eSub_sat_snp;
}


void CId1ReaderBase::AddSNPBlob_id(CLoadLockBlob_ids& ids, int gi)
{
    CBlob_id blob_id;
    blob_id.SetSat(CSeqref::eSat_SNP);
    blob_id.SetSubSat(CID2_Blob_Id::eSub_sat_snp);
    blob_id.SetSatKey(gi);
    //blob_id.SetVersion(0);
    ids.AddBlob_id(blob_id, fBlobHasExternal);
}


void CId1ReaderBase::GetBlob(CTSE_Info& tse_info,
                             const CBlob_id& blob_id,
                             TConn conn)
{
    if ( IsSNPBlob_id(blob_id) ) {
        GetSNPBlob(tse_info, blob_id, conn);
    }
    else {
        GetTSEBlob(tse_info, blob_id, conn);
    }
}


void CId1ReaderBase::GetChunk(CTSE_Chunk_Info& chunk_info,
                              const CBlob_id& blob_id,
                              TConn conn)
{
    if ( IsSNPBlob_id(blob_id) && chunk_info.GetChunkId()==kSNP_ChunkId ) {
        GetSNPChunk(chunk_info, blob_id, conn);
    }
    else {
        GetTSEChunk(chunk_info, blob_id, conn);
    }
}


void CId1ReaderBase::GetSNPBlob(CTSE_Info& tse_info,
                                const CBlob_id& blob_id,
                                TConn /*conn*/)
{
    CRef<CSeq_entry> seq_entry(new CSeq_entry);
    seq_entry->SetSet().SetSeq_set();
    seq_entry->SetSet().SetId().SetId(kSNP_EntryId);

    // create CTSE_Info
    tse_info.SetSeq_entry(*seq_entry);
    tse_info.SetName("SNP");

    CRef<CTSE_Chunk_Info> info(new CTSE_Chunk_Info(kSNP_ChunkId));

    info->x_AddAnnotPlace(CTSE_Chunk_Info::eBioseq_set, kSNP_EntryId);

    info->x_AddAnnotType(CAnnotName("SNP"),
                         SAnnotTypeSelector(CSeqFeatData::eSubtype_variation),
                         CSeq_id_Handle::GetGiHandle(blob_id.GetSatKey()),
                         CTSE_Chunk_Info::TLocationRange::GetWhole());

    info->x_TSEAttach(tse_info);
}


void CId1ReaderBase::GetTSEChunk(CTSE_Chunk_Info& /*chunk_info*/,
                                 const CBlob_id& /*blob_id*/,
                                 TConn /*conn*/)
{
    NCBI_THROW(CLoaderException, eNoData,
               "Chunks are not implemented");
}


void CId1ReaderBase::GetSNPChunk(CTSE_Chunk_Info& chunk,
                                 const CBlob_id& blob_id,
                                 TConn conn)
{
    _ASSERT(IsSNPBlob_id(blob_id));
    _ASSERT(chunk.GetChunkId() == kSNP_ChunkId);
    CRef<CSeq_annot_SNP_Info> snp_annot = GetSNPAnnot(blob_id, conn);
    CRef<CSeq_annot_Info> annot_info(new CSeq_annot_Info(*snp_annot));
    CTSE_Chunk_Info::TPlace place(CTSE_Chunk_Info::eBioseq_set, kSNP_EntryId);
    chunk.x_LoadAnnot(place, annot_info);
}


END_SCOPE(objects)
END_NCBI_SCOPE
