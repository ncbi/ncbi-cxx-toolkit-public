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
 *  Author:  Eugene Vasilchenko
 *
 *  File Description: blob stream processor interface
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/rwstream.hpp>

#include <objtools/data_loaders/genbank/writer.hpp>
#include <objtools/data_loaders/genbank/impl/processor.hpp>
#include <objtools/data_loaders/genbank/impl/processors.hpp>
#include <objtools/data_loaders/genbank/impl/request_result.hpp>
#include <objtools/data_loaders/genbank/impl/dispatcher.hpp>
#include <objtools/data_loaders/genbank/reader.hpp>
#include <objtools/data_loaders/genbank/impl/statistics.hpp>

#include <objtools/data_loaders/genbank/reader_snp.hpp>
#include <objtools/data_loaders/genbank/impl/split_parser.hpp>
#include <objtools/error_codes.hpp>

#include <objmgr/impl/tse_split_info.hpp>
#include <objmgr/annot_selector.hpp>

#include <objects/id1/id1__.hpp>
#include <objects/id2/ID2_Reply_Data.hpp>
#include <objects/seqsplit/seqsplit__.hpp>
// for read hooks setup
#include <objects/general/general__.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Imp_feat.hpp>

#include <objmgr/objmgr_exception.hpp>

#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/delaybuf.hpp>
#include <serial/serial.hpp>
#include <serial/iterator.hpp>

#include <util/compress/reader_zlib.hpp>
#include <util/compress/zlib.hpp>

#include <serial/pack_string.hpp>


#define NCBI_USE_ERRCODE_X   Objtools_Rd_Process

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

NCBI_PARAM_DEF_EX(bool, GENBANK, SNP_PACK_STRINGS, true,
                  eParam_NoThread, GENBANK_SNP_PACK_STRINGS);
NCBI_PARAM_DEF_EX(bool, GENBANK, SNP_SPLIT, true,
                  eParam_NoThread, GENBANK_SNP_SPLIT);
NCBI_PARAM_DEF_EX(bool, GENBANK, SNP_TABLE, true,
                  eParam_NoThread, GENBANK_SNP_TABLE);
NCBI_PARAM_DEF_EX(bool, GENBANK, USE_MEMORY_POOL, true,
                  eParam_NoThread, GENBANK_USE_MEMORY_POOL);
NCBI_PARAM_DEF_EX(int, GENBANK, READER_STATS, 0,
                  eParam_NoThread, GENBANK_READER_STATS);
NCBI_PARAM_DEF_EX(bool, GENBANK, CACHE_RECOMPRESS, true,
                  eParam_NoThread, GENBANK_CACHE_RECOMPRESS);


/////////////////////////////////////////////////////////////////////////////
// helper functions
/////////////////////////////////////////////////////////////////////////////

namespace {
    CProcessor::TMagic s_GetMagic(const char* s)
    {
        CProcessor::TMagic m = 0;
        const char* p = s;
        for ( size_t i = 0; i < sizeof(m); ++p, ++i ) {
            if ( !*p ) {
                p = s;
            }
            m = (m << 8) | (*p & 0xff);
        }
        return m;
    }


    class COSSReader : public IReader
    {
    public:
        typedef vector<char> TOctetString;
        typedef list<TOctetString*> TOctetStringSequence;

        COSSReader(const TOctetStringSequence& in)
            : m_Input(in),
              m_CurVec(in.begin())
            {
                x_SetVec();
            }
        
        virtual ERW_Result Read(void* buffer,
                                size_t  count,
                                size_t* bytes_read = 0)
            {
                size_t pending = x_Pending();
                count = min(pending, count);
                if ( bytes_read ) {
                    *bytes_read = count;
                }
                if ( pending == 0 ) {
                    return eRW_Eof;
                }
                if ( count ) {
                    memcpy(buffer, &(**m_CurVec)[m_CurPos], count);
                    m_CurPos += count;
                }
                return eRW_Success;
            }

        virtual ERW_Result PendingCount(size_t* count)
            {
                size_t pending = x_Pending();
                *count = pending;
                return pending? eRW_Success: eRW_Eof;
            }

    protected:
        void x_SetVec(void)
            {
                m_CurPos = 0;
                m_CurSize = m_CurVec == m_Input.end()? 0: (**m_CurVec).size();
            }
        size_t x_Pending(void)
            {
                size_t size;
                while ( (size = m_CurSize - m_CurPos) == 0 &&
                        m_CurVec != m_Input.end() ) {
                    ++m_CurVec;
                    x_SetVec();
                }
                return size;
            }
    private:
        const TOctetStringSequence& m_Input;
        TOctetStringSequence::const_iterator m_CurVec;
        size_t m_CurPos;
        size_t m_CurSize;
    };
    class COSSWriter : public IWriter
    {
    public:
        typedef vector<char> TOctetString;
        typedef list<TOctetString*> TOctetStringSequence;

        COSSWriter(TOctetStringSequence& out)
            : m_Output(out)
            {
            }
        
        virtual ERW_Result Write(const void* buffer,
                                 size_t  count,
                                 size_t* written)
            {
                const char* data = static_cast<const char*>(buffer);
                m_Output.push_back(new TOctetString(data, data+count));
                if ( written ) {
                    *written = count;
                }
                return eRW_Success;
            }
        virtual ERW_Result Flush(void)
            {
                return eRW_Success;
            }

    private:
        TOctetStringSequence& m_Output;
    };
}


/////////////////////////////////////////////////////////////////////////////
// CProcessor
/////////////////////////////////////////////////////////////////////////////


#define GB_STATS_STOP(action, stat, size)               \
    LogStat(action, blob_id, stat, r, size, result)


inline
int CProcessor::CollectStatistics(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(GENBANK, READER_STATS)> s_Value;
    return s_Value->Get();
}


CProcessor::CProcessor(CReadDispatcher& dispatcher)
    : m_Dispatcher(&dispatcher)
{
}


CProcessor::~CProcessor(void)
{
}


void CProcessor::ProcessStream(CReaderRequestResult& result,
                               const TBlobId& blob_id,
                               TChunkId chunk_id,
                               CNcbiIstream& stream) const
{
    //CReaderRequestResult::CRecurse r(result);
    CObjectIStreamAsnBinary obj_stream(stream);
    ProcessObjStream(result, blob_id, chunk_id, obj_stream);
    //LogStat(result, r, blob_id,
    //        CGBRequestStatistics::eStat_LoadBlob,
    //        "CProcessor: process stream",
    //        obj_stream.GetStreamPos());
}


void CProcessor::ProcessObjStream(CReaderRequestResult& /*result*/,
                                  const TBlobId& /*blob_id*/,
                                  TChunkId /*chunk_id*/,
                                  CObjectIStream& /*obj_stream*/) const
{
    NCBI_THROW(CLoaderException, eLoaderFailed,
               "CProcessor::ProcessObjStream() is not implemented");
}


void CProcessor::ProcessBlobFromID2Data(CReaderRequestResult& result,
                                        const TBlobId& blob_id,
                                        TChunkId chunk_id,
                                        const CID2_Reply_Data& data) const
{
    if ( !data.IsSetData() ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "CProcessor::ProcessBlobFromID2Data() no data");
    }
    if ( data.GetData_format() != data.eData_format_asn_binary ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "CProcessor::ProcessBlobFromID2Data() is not implemented");
    }

    CRStream stream(new COSSReader(data.GetData()),
                    0, 0, CRWStreambuf::fOwnAll);
    switch ( data.GetData_compression() ) {
    case CID2_Reply_Data::eData_compression_none:
    {
        ProcessStream(result, blob_id, chunk_id, stream);
        break;
    }
    case CID2_Reply_Data::eData_compression_gzip:
    {
        CCompressionIStream zip_stream(stream,
                                       new CZipStreamDecompressor,
                                       CCompressionIStream::fOwnProcessor);
        ProcessStream(result, blob_id, chunk_id, zip_stream);
        break;
    }
    default:
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "CProcessor::ProcessBlobFromID2Data() is not implemented");
    }
}


void CProcessor::RegisterAllProcessors(CReadDispatcher& d)
{
    d.InsertProcessor(CRef<CProcessor>(new CProcessor_ID1(d)));
    d.InsertProcessor(CRef<CProcessor>(new CProcessor_ID1_SNP(d)));
    d.InsertProcessor(CRef<CProcessor>(new CProcessor_SE(d)));
    d.InsertProcessor(CRef<CProcessor>(new CProcessor_SE_SNP(d)));
    d.InsertProcessor(CRef<CProcessor>(new CProcessor_St_SE(d)));
    d.InsertProcessor(CRef<CProcessor>(new CProcessor_St_SE_SNPT(d)));
    d.InsertProcessor(CRef<CProcessor>(new CProcessor_ID2(d)));
    d.InsertProcessor(CRef<CProcessor>(new CProcessor_ID2_Split(d)));
    d.InsertProcessor(CRef<CProcessor>(new CProcessor_ID2AndSkel(d)));
    d.InsertProcessor(CRef<CProcessor>(new CProcessor_ExtAnnot(d)));
}


bool CProcessor::TryStringPack(void)
{
    typedef NCBI_PARAM_TYPE(GENBANK, SNP_PACK_STRINGS) TPackStrings;
    if ( !TPackStrings::GetDefault() ) {
        return false;
    }
    if ( !CPackString::TryStringPack() ) {
        TPackStrings::SetDefault(false);
        return false;
    }
    return true;
}


bool CProcessor::TrySNPSplit(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(GENBANK, SNP_SPLIT)> s_Value;
    return s_Value->Get();
}


bool CProcessor::TrySNPTable(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(GENBANK, SNP_TABLE)> s_Value;
    return s_Value->Get();
}


static bool s_UseMemoryPool(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(GENBANK, USE_MEMORY_POOL)> s_Value;
    return s_Value->Get();
}


static bool s_CacheRecompress(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(GENBANK, CACHE_RECOMPRESS)> s_Value;
    return s_Value->Get();
}


void CProcessor::SetSeqEntryReadHooks(CObjectIStream& in)
{
    if ( TryStringPack() ) {
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
    if ( s_UseMemoryPool() ) {
        in.UseMemoryPool();
    }
}


void CProcessor::SetSNPReadHooks(CObjectIStream& in)
{
    if ( TryStringPack() ) {
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
}


inline
CWriter* CProcessor::GetWriter(const CReaderRequestResult& result) const
{
    return m_Dispatcher->GetWriter(result, CWriter::eBlobWriter);
}


NCBI_PARAM_DEF_EX(Int8, GENBANK, GI_OFFSET, 0,
                  eParam_NoThread, GENBANK_GI_OFFSET);


TIntId CProcessor::GetGiOffset(void)
{
    static volatile TIntId gi_offset;
    static volatile bool initialized;
    if ( !initialized ) {
        gi_offset = NCBI_PARAM_TYPE(GENBANK, GI_OFFSET)::GetDefault();
        initialized = true;
    }
    return gi_offset;
}


bool CProcessor::OffsetId(CSeq_id& id, TIntId gi_offset)
{
    if ( !gi_offset ) {
        return false;
    }
    if ( id.IsGi() ) {
        if ( TGi gi = id.GetGi() ) {
            id.SetGi(gi + gi_offset);
            return true;
        }
    }
    else if ( id.IsGeneral() ) {
        CDbtag& dbtag = id.SetGeneral();
        CObject_id& tag = dbtag.SetTag();
        if ( tag.IsStr() &&
             NStr::EqualNocase(dbtag.GetDb(), "NAnnot") ) {
            // Named annotation virtual sequence id:
            // gnl|NAnnot|12345:NA000012345.1
            const string& s = tag.GetStr();
            SIZE_TYPE sep = s.find(':');
            Int8 id8;
            if ( sep != NPOS &&
                 NStr::StringToNumeric(s, &id8, NStr::fConvErr_NoThrow) &&
                 id8 != 0 ) {
                // offset only non-zero GIs
                tag.SetStr(NStr::NumericToString(id8 + gi_offset) +
                           s.substr(sep));
                return true;
            }
        }
        else if ( NStr::StartsWith(dbtag.GetDb(), "ANNOT:", NStr::eNocase) ) {
            // External annotation virtual sequence id:
            // gnl|Annot:SNP|12345 
            CObject_id::TId8 id8 = 0;
            if ( tag.GetId8(id8) &&
                 id8 != 0 ) {
                // offset only non-zero GIs
                tag.SetId8(id8 + gi_offset);
                return true;
            }
        }
    }
    return false;
}


bool CProcessor::OffsetId(CSeq_id_Handle& id, TIntId gi_offset)
{
    if ( !gi_offset ) {
        return false;
    }
    if ( id.IsGi() ) {
        if ( TGi gi = id.GetGi() ) {
            id = CSeq_id_Handle::GetGiHandle(gi + gi_offset);
            return true;
        }
    }
    else if ( id.Which() == CSeq_id::e_General ) {
        CRef<CSeq_id> seq_id(SerialClone(*id.GetSeqId()));
        if ( OffsetId(*seq_id, gi_offset) ) {
            id = CSeq_id_Handle::GetHandle(*seq_id);
            return true;
        }
    }
    return false;
}


void CProcessor::OffsetAllGis(CBeginInfo obj, TIntId gi_offset)
{
    if ( !gi_offset ) {
        return;
    }
    for ( CTypeIterator<CSeq_id> it(obj); it; ++it ) {
        OffsetId(*it, gi_offset);
    }

    // ID2
    for ( CTypeIterator<CID1server_request> it(obj); it; ++it ) {
        CID1server_request& req = *it;
        switch ( req.Which() ) {
        case CID1server_request::e_Getseqidsfromgi:
            OffsetGi(req.SetGetseqidsfromgi(), gi_offset);
            break;
        case CID1server_request::e_Getgihist:
            OffsetGi(req.SetGetgihist(), gi_offset);
            break;
        case CID1server_request::e_Getgirev:
            OffsetGi(req.SetGetgirev(), gi_offset);
            break;
        case CID1server_request::e_Getgistate:
            OffsetGi(req.SetGetgistate(), gi_offset);
            break;
        default:
            break;
        }
    }
    for ( CTypeIterator<CID1server_maxcomplex> it(obj); it; ++it ) {
        OffsetGi(it->SetGi(), gi_offset);
    }
    for ( CTypeIterator<CID1server_back> it(obj); it; ++it ) {
        CID1server_back& reply = *it;
        if ( reply.IsGotgi() ) {
            OffsetGi(reply.SetGotgi(), gi_offset);
        }
    }
    for ( CTypeIterator<CID1blob_info> it(obj); it; ++it ) {
        OffsetGi(it->SetGi(), gi_offset);
    }
    
    // ID2
    for ( CTypeIterator<CID2S_Seq_loc> it(obj); it; ++it ) {
        CID2S_Seq_loc& loc = *it;
        if ( loc.IsWhole_gi() ) {
            OffsetGi(loc.SetWhole_gi(), gi_offset);
        }
    }
    for ( CTypeIterator<CID2S_Gi_Range> it(obj); it; ++it ) {
        OffsetGi(it->SetStart(), gi_offset);
    }
    for ( CTypeIterator<CID2S_Gi_Interval> it(obj); it; ++it ) {
        OffsetGi(it->SetGi(), gi_offset);
    }
    for ( CTypeIterator<CID2S_Gi_Ints> it(obj); it; ++it ) {
        OffsetGi(it->SetGi(), gi_offset);
    }
    for ( CTypeIterator<CID2S_Bioseq_Ids> it(obj); it; ++it ) {
        NON_CONST_ITERATE ( CID2S_Bioseq_Ids::Tdata, it2, it->Set() ) {
            if ( (*it2)->IsGi() ) {
                OffsetGi((*it2)->SetGi(), gi_offset);
            }
        }
    }
    for ( CTypeIterator<CID2S_Chunk_Data> it(obj); it; ++it ) {
        if ( it->SetId().IsGi() ) {
            OffsetGi(it->SetId().SetGi(), gi_offset);
        }
    }
}


void CProcessor::OffsetAllGis(CTSE_SetObjectInfo& set_info, TIntId gi_offset)
{
    if ( !gi_offset ) {
        return;
    }
    NON_CONST_ITERATE ( CTSE_SetObjectInfo::TSeq_annot_InfoMap, it,
                        set_info.m_Seq_annot_InfoMap ) {
        it->second.m_SNP_annot_Info->OffsetGi(gi_offset);
    }
}


void CProcessor::OffsetAllGisFromOM(CBeginInfo obj)
{
    OffsetAllGis(obj, -GetGiOffset());
}


void CProcessor::OffsetAllGisToOM(CBeginInfo obj, CTSE_SetObjectInfo* set_info)
{
    if ( TIntId gi_offset = GetGiOffset() ) {
        OffsetAllGis(obj, gi_offset);
        if ( set_info ) {
            OffsetAllGis(*set_info, gi_offset);
        }
    }
}


BEGIN_LOCAL_NAMESPACE;


static inline
bool s_CanBeWGSBlob(const CBlob_id& blob_id)
{
    return !CProcessor_ExtAnnot::IsExtAnnot(blob_id);
}


static
bool s_GoodLetters(CTempString s) {
    ITERATE ( CTempString, it, s ) {
        if ( !isalpha(*it & 0xff) ) {
            return false;
        }
    }
    return true;
}


static
bool s_GoodDigits(CTempString s) {
    bool have_non_zero = false;
    ITERATE ( CTempString, it, s ) {
        if ( *it != '0' ) {
            have_non_zero = true;
            if ( !isdigit(*it & 0xff) ) {
                return false;
            }
        }
    }
    return have_non_zero;
}


static
CSeq_id_Handle s_GetWGSMasterSeq_id(const CSeq_id_Handle& idh)
{
    CSeq_id_Handle master_idh;

    switch ( idh.Which() ) { // shortcut to exclude all non Textseq-id types
    case CSeq_id::e_not_set:
    case CSeq_id::e_Local:
    case CSeq_id::e_Gi:
    case CSeq_id::e_Gibbsq:
    case CSeq_id::e_Gibbmt:
    case CSeq_id::e_Giim:
    case CSeq_id::e_Patent:
    case CSeq_id::e_General:
    case CSeq_id::e_Pdb:
        return master_idh;
    default:
        break;
    }

    CConstRef<CSeq_id> id = idh.GetSeqId();
    const CTextseq_id* text_id = id->GetTextseq_Id();
    if ( !text_id || !text_id->IsSetAccession() ) {
        return master_idh;
    }

    CTempString acc = text_id->GetAccession();

    CSeq_id::EAccessionInfo type = CSeq_id::IdentifyAccession(acc);
    switch ( type & CSeq_id::eAcc_division_mask ) {
        // accepted accession types
    case CSeq_id::eAcc_wgs:
    case CSeq_id::eAcc_wgs_intermed:
    case CSeq_id::eAcc_tsa:
        break;
    default:
        return master_idh;
    }

    bool have_nz = NStr::StartsWith(acc, "NZ_");
    SIZE_TYPE letters_pos = have_nz? 3: 0;
    SIZE_TYPE digits_pos = letters_pos+4;
    SIZE_TYPE digits_count = acc.size() - digits_pos;
    if ( digits_count < 8 || digits_count > 10 ) {
        return master_idh;
    }
    if ( !s_GoodLetters(acc.substr(letters_pos, 4)) ) {
        return master_idh;
    }
    if ( !s_GoodDigits(acc.substr(digits_pos)) ) {
        return master_idh;
    }
    int version = NStr::StringToNumeric<int>(acc.substr(digits_pos, 2));
    Uint8 row_id = NStr::StringToNumeric<Uint8>(acc.substr(digits_pos+2));
    if ( !version || !row_id ) {
        return master_idh;
    }
    CSeq_id master_id;
    master_id.Assign(*id);
    CTextseq_id* master_text_id =
        const_cast<CTextseq_id*>(master_id.GetTextseq_Id());
    string master_acc = acc.substr(0, digits_pos);
    master_acc.resize(acc.size(), '0');
    master_text_id->Reset();
    master_text_id->SetAccession(master_acc);
    master_text_id->SetVersion(version);
    master_idh = CSeq_id_Handle::GetHandle(master_id);
    return master_idh;
}


static const int kForceDescrMask = ((1<<CSeqdesc::e_Pub) |
                                    (1<<CSeqdesc::e_Comment) |
                                    (1<<CSeqdesc::e_User));

static const int kOptionalDescrMask = ((1<<CSeqdesc::e_Source) |
                                       (1<<CSeqdesc::e_Molinfo) |
                                       (1<<CSeqdesc::e_Create_date) |
                                       (1<<CSeqdesc::e_Update_date));

static const int kGoodDescrMask = kForceDescrMask | kOptionalDescrMask;


static
bool s_IsGoodDescr(const CSeqdesc& desc)
{
    if ( desc.Which() == CSeqdesc::e_User ) {
        const CObject_id& type = desc.GetUser().GetType();
        if ( type.Which() == CObject_id::e_Str ) {
            const string& name = type.GetStr();
            if ( name == "DBLink" ||
                 name == "GenomeProjectsDB" ||
                 name == "StructuredComment" ||
                 name == "FeatureFetchPolicy" ) {
                return true;
            }
        }
    }
    else if ( (1 << desc.Which()) & kGoodDescrMask ) {
        return true;
    }
    return false;
}


static
void s_AddMasterDescr(CBioseq_Info& seq, const CSeq_descr& src)
{
    int existing_mask = 0;
    CSeq_descr::Tdata& dst = seq.x_SetDescr().Set();
    ITERATE ( CSeq_descr::Tdata, it, dst ) {
        const CSeqdesc& desc = **it;
        existing_mask |= 1 << desc.Which();
    }
    ITERATE ( CSeq_descr::Tdata, it, src.Get() ) {
        int mask = 1 << (*it)->Which();
        if ( mask & kOptionalDescrMask ) {
            if ( mask & existing_mask ) {
                continue;
            }
        }
        else if ( !(mask & kForceDescrMask) ) {
            continue;
        }
        dst.push_back(*it);
    }
}


static
CRef<CSeq_descr> s_GetWGSMasterDescr(CDataLoader* loader,
                                     const CSeq_id_Handle& master_idh)
{
    CRef<CSeq_descr> ret;
    CDataLoader::TTSE_LockSet locks =
        loader->GetRecordsNoBlobState(master_idh, CDataLoader::eBioseqCore);
    ITERATE ( CDataLoader::TTSE_LockSet, it, locks ) {
        CConstRef<CBioseq_Info> bs_info =
            (*it)->FindMatchingBioseq(master_idh);
        if ( !bs_info ) {
            continue;
        }
        if ( bs_info->IsSetDescr() ) {
            const CSeq_descr::Tdata& descr = bs_info->GetDescr().Get();
            ITERATE ( CSeq_descr::Tdata, it, descr ) {
                if ( s_IsGoodDescr(**it) ) {
                    if ( !ret ) {
                        ret = new CSeq_descr;
                    }
                    ret->Set().push_back(*it);
                }
            }
        }
        break;
    }
    return ret;
}


class CWGSBioseqUpdater_Base : public CBioseqUpdater
{
public:
    CWGSBioseqUpdater_Base(const CSeq_id_Handle& master_idh)
        : m_MasterId(master_idh)
        {
        }

    bool HasMasterId(const CBioseq_Info& seq) const {
        if ( m_MasterId ) {
            const CBioseq_Info::TId& ids = seq.GetId();
            ITERATE ( CBioseq_Info::TId, it, ids ) {
                if ( s_GetWGSMasterSeq_id(*it) == m_MasterId ) {
                    return true;
                }
            }
        }
        return false;
    }
    
private:
    CSeq_id_Handle m_MasterId;
};


class CWGSBioseqUpdaterChunk : public CWGSBioseqUpdater_Base
{
public:
    CWGSBioseqUpdaterChunk(const CSeq_id_Handle& master_idh)
        : CWGSBioseqUpdater_Base(master_idh)
        {
        }

    virtual void Update(CBioseq_Info& seq) {
        if ( HasMasterId(seq) ) {
            // register master descr chunk
            seq.x_AddDescrChunkId(kGoodDescrMask, kMasterWGS_ChunkId);
        }
    }
};


class CWGSBioseqUpdaterDescr : public CWGSBioseqUpdater_Base
{
public:
    CWGSBioseqUpdaterDescr(const CSeq_id_Handle& master_idh,
                           CRef<CSeq_descr> descr)
        : CWGSBioseqUpdater_Base(master_idh),
          m_Descr(descr)
        {
        }

    virtual void Update(CBioseq_Info& seq) {
        if ( m_Descr &&
             seq.x_NeedUpdate(seq.fNeedUpdate_descr) &&
             HasMasterId(seq) ) {
            s_AddMasterDescr(seq, *m_Descr);
        }
    }

private:
    CRef<CSeq_descr> m_Descr;
};


class CWGSMasterChunkInfo : public CTSE_Chunk_Info
{
public:
    CWGSMasterChunkInfo(const CSeq_id_Handle& master_idh)
        : CTSE_Chunk_Info(kMasterWGS_ChunkId),
          m_MasterId(master_idh)
        {
        }

    CSeq_id_Handle m_MasterId;
};


END_LOCAL_NAMESPACE;


void CProcessor::LoadWGSMaster(CDataLoader* loader,
                               CRef<CTSE_Chunk_Info> chunk)
{
    CSeq_id_Handle id = dynamic_cast<CWGSMasterChunkInfo&>(*chunk).m_MasterId;
    CRef<CSeq_descr> descr = s_GetWGSMasterDescr(loader, id);
    CRef<CBioseqUpdater> upd(new CWGSBioseqUpdaterDescr(id, descr));
    const_cast<CTSE_Split_Info&>(chunk->GetSplitInfo()).x_SetBioseqUpdater(upd);
    chunk->SetLoaded();
}


void CProcessor::AddWGSMaster(CLoadLockSetter& blob)
{
    CTSE_Info::TSeqIds ids;
    CTSE_LoadLock& lock = blob.GetTSE_LoadLock();
    lock->GetBioseqsIds(ids);
    ITERATE ( CTSE_Info::TSeqIds, it, ids ) {
        if ( CSeq_id_Handle id = s_GetWGSMasterSeq_id(*it) ) {
            CRef<CTSE_Chunk_Info> chunk(new CWGSMasterChunkInfo(id));
            lock->GetSplitInfo().AddChunk(*chunk);
            CRef<CBioseqUpdater> upd(new CWGSBioseqUpdaterChunk(id));
            lock->SetBioseqUpdater(upd);
            break;
        }
    }
}


namespace {
    inline
    CRef<CWriter::CBlobStream> OpenStream(CWriter* writer,
                                          CReaderRequestResult& result,
                                          const CProcessor::TBlobId& blob_id,
                                          CProcessor::TChunkId chunk_id,
                                          const CProcessor* processor)
    {
        _ASSERT(writer);
        _ASSERT(processor);
        return writer->OpenBlobStream(result, blob_id, chunk_id, *processor);
        /*
        if ( chunk_id == kMain_ChunkId ) {
            return writer->OpenBlobStream(result, blob_id, *processor);
        }
        else {
            return writer->OpenChunkStream(result, blob_id, chunk_id,
                                           *processor);
        }
        */
    }
}


/////////////////////////////////////////////////////////////////////////////
// CProcessor_ID1
/////////////////////////////////////////////////////////////////////////////

CProcessor_ID1::CProcessor_ID1(CReadDispatcher& dispatcher)
    : CProcessor(dispatcher)
{
}


CProcessor_ID1::~CProcessor_ID1(void)
{
}


CProcessor::EType CProcessor_ID1::GetType(void) const
{
    return eType_ID1;
}


CProcessor::TMagic CProcessor_ID1::GetMagic(void) const
{
    static TMagic kMagic = s_GetMagic("ID1r");
    return kMagic;
}


void CProcessor_ID1::ProcessObjStream(CReaderRequestResult& result,
                                      const TBlobId& blob_id,
                                      TChunkId chunk_id,
                                      CObjectIStream& obj_stream) const
{
    CLoadLockBlob blob(result, blob_id, chunk_id);
    if ( blob.IsLoadedChunk() ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "CProcessor_ID1: "
                       "double load of "<<blob_id<<'/'<<chunk_id);
    }
    CID1server_back reply;
    CStreamDelayBufferGuard guard;
    CWriter* writer = GetWriter(result);
    if ( writer ) {
        guard.StartDelayBuffer(obj_stream);
    }
    SetSeqEntryReadHooks(obj_stream);

    {{
        CReaderRequestResultRecursion r(result);
                
        obj_stream >> reply;
            
        LogStat(r, blob_id,
                CGBRequestStatistics::eStat_LoadBlob,
                "CProcessor_ID1: read data",
                obj_stream.GetStreamPos());
    }}

    TBlobVersion version = GetVersion(reply);
    if ( version >= 0 ) {
        result.SetAndSaveBlobVersion(blob_id, version);
    }
        
    TSeqEntryInfo entry = GetSeq_entry(result, blob_id, reply);
    result.SetAndSaveBlobState(blob_id, entry.second);
    CLoadLockSetter setter(blob);
    if ( !setter.IsLoaded() ) {
        if ( entry.first ) {
            OffsetAllGisToOM(*entry.first);
            setter.SetSeq_entry(*entry.first);
        }
        setter.SetLoaded();
    }
        
    if ( writer && version >= 0 ) {
        SaveBlob(result, blob_id, chunk_id, writer,
                 guard.EndDelayBuffer());
    }
}


CProcessor_ID1::TSeqEntryInfo
CProcessor_ID1::GetSeq_entry(CReaderRequestResult& result,
                             const TBlobId& blob_id,
                             CID1server_back& reply) const
{
    TSeqEntryInfo entry;
    switch ( reply.Which() ) {
    case CID1server_back::e_Gotseqentry:
        entry.first.Reset(&reply.SetGotseqentry());
        break;
    case CID1server_back::e_Gotdeadseqentry:
        entry.second |= CBioseq_Handle::fState_dead;
        entry.first.Reset(&reply.SetGotdeadseqentry());
        break;
    case CID1server_back::e_Gotsewithinfo:
    {{
        const CID1blob_info& info = reply.GetGotsewithinfo().GetBlob_info();
        if ( info.GetBlob_state() < 0 ) {
            entry.second |= CBioseq_Handle::fState_dead;
        }
        if ( reply.GetGotsewithinfo().IsSetBlob() ) {
            entry.first.Reset(&reply.SetGotsewithinfo().SetBlob());
        }
        else {
            // no Seq-entry in reply, probably private data
            entry.second |= CBioseq_Handle::fState_no_data;
        }
        if ( info.GetSuppress() ) {
            entry.second |=
                (info.GetSuppress() & 4)
                ? CBioseq_Handle::fState_suppress_temp
                : CBioseq_Handle::fState_suppress_perm;
        }
        if ( info.GetWithdrawn() ) {
            entry.second |= 
                CBioseq_Handle::fState_withdrawn|
                CBioseq_Handle::fState_no_data;
        }
        if ( info.GetConfidential() ) {
            entry.second |=
                CBioseq_Handle::fState_confidential|
                CBioseq_Handle::fState_no_data;
        }
        break;
    }}
    case CID1server_back::e_Error:
    {{
        int error = reply.GetError();
        switch ( error ) {
        case 1:
            entry.second |=
                CBioseq_Handle::fState_withdrawn|
                CBioseq_Handle::fState_no_data;
            break;
        case 2:
            entry.second |=
                CBioseq_Handle::fState_confidential|
                CBioseq_Handle::fState_no_data;
            break;
        case 10:
            entry.second |= CBioseq_Handle::fState_no_data;
            break;
        case 100:
            NCBI_THROW_FMT(CLoaderException, eConnectionFailed,
                           "ID1server-back.error "<<error);
        default:
            ERR_POST_X(1, "CId1Reader::GetMainBlob: "
                       "ID1server-back.error "<<error);
            NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                           "CProcessor_ID1::GetSeq_entry: "
                           "ID1server-back.error "<<error);
        }
        break;
    }}
    default:
        // no data
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "CProcessor_ID1::GetSeq_entry: "
                       "bad ID1server-back type: "<<reply.Which());
    }
    return entry;
}


CProcessor::TBlobVersion
CProcessor_ID1::GetVersion(const CID1server_back& reply) const
{
    switch ( reply.Which() ) {
    case CID1server_back::e_Gotblobinfo:
        return abs(reply.GetGotblobinfo().GetBlob_state());
    case CID1server_back::e_Gotsewithinfo:
        return abs(reply.GetGotsewithinfo().GetBlob_info().GetBlob_state());
    default:
        return -1;
    }
}


void CProcessor_ID1::SaveBlob(CReaderRequestResult& result,
                              const TBlobId& blob_id,
                              TChunkId chunk_id,
                              CWriter* writer,
                              CRef<CByteSource> byte_source) const
{
    _ASSERT(writer && byte_source);
    CRef<CWriter::CBlobStream> stream
        (OpenStream(writer, result, blob_id, chunk_id, this));
    if ( !stream ) {
        return;
    }
    try {
        CWriter::WriteBytes(**stream, byte_source);
        stream->Close();
    }
    catch ( CException& ) { // ignored
    }
}


void CProcessor_ID1::SaveBlob(CReaderRequestResult& result,
                              const TBlobId& blob_id,
                              TChunkId chunk_id,
                              CWriter* writer,
                              const CID1server_back& reply) const
{
    _ASSERT(writer);
    CRef<CWriter::CBlobStream> stream
        (OpenStream(writer, result, blob_id, chunk_id, this));
    if ( !stream ) {
        return;
    }
    try {
        {{
            CObjectOStreamAsnBinary obj_stream(**stream);
            obj_stream << reply;
        }}
        stream->Close();
    }
    catch ( CException& ) { // ignored
    }
}


/////////////////////////////////////////////////////////////////////////////
// CProcessor_ID1_SNP
/////////////////////////////////////////////////////////////////////////////

CProcessor_ID1_SNP::CProcessor_ID1_SNP(CReadDispatcher& dispatcher)
    : CProcessor_ID1(dispatcher)
{
}


CProcessor_ID1_SNP::~CProcessor_ID1_SNP(void)
{
}


CProcessor::EType CProcessor_ID1_SNP::GetType(void) const
{
    return eType_ID1_SNP;
}


CProcessor::TMagic CProcessor_ID1_SNP::GetMagic(void) const
{
    static TMagic kMagic = s_GetMagic("ID1S");
    return kMagic;
}


void CProcessor_ID1_SNP::ProcessObjStream(CReaderRequestResult& result,
                                          const TBlobId& blob_id,
                                          TChunkId chunk_id,
                                          CObjectIStream& obj_stream) const
{
    CLoadLockBlob blob(result, blob_id, chunk_id);
    if ( blob.IsLoadedChunk() ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "CProcessor_ID1_SNP: "
                       "double load of "<<blob_id<<'/'<<chunk_id);
    }
    CTSE_SetObjectInfo set_info;
    CID1server_back reply;
    {{
        CReaderRequestResultRecursion r(result);
            
        CSeq_annot_SNP_Info_Reader::Parse(obj_stream,
                                          Begin(reply),
                                          set_info);
            
        LogStat(r, blob_id,
                CGBRequestStatistics::eStat_LoadSNPBlob,
                "CProcessor_ID1: read SNP data",
                obj_stream.GetStreamPos());
    }}

    TBlobVersion version = GetVersion(reply);
    if ( version >= 0 ) {
        result.SetAndSaveBlobVersion(blob_id, version);
    }
    TSeqEntryInfo entry = GetSeq_entry(result, blob_id, reply);
    result.SetAndSaveBlobState(blob_id, entry.second);
    
    CWriter* writer = GetWriter(result);
    if ( writer && version >= 0 ) {
        if ( set_info.m_Seq_annot_InfoMap.empty() || !entry.first ) {
            const CProcessor_ID1* prc =
                dynamic_cast<const CProcessor_ID1*>
                (&m_Dispatcher->GetProcessor(eType_ID1));
            if ( prc ) {
                prc->SaveBlob(result, blob_id, chunk_id, writer, reply);
            }
        }
        else {
            const CProcessor_St_SE_SNPT* prc =
                dynamic_cast<const CProcessor_St_SE_SNPT*>
                (&m_Dispatcher->GetProcessor(eType_St_Seq_entry_SNPT));
            if ( prc ) {
                prc->SaveSNPBlob(result, blob_id, chunk_id, writer,
                                 *entry.first, entry.second, set_info);
            }
        }
    }

    CLoadLockSetter setter(blob);
    if ( !setter.IsLoaded() ) {
        if ( entry.first ) {
            OffsetAllGisToOM(*entry.first, &set_info);
            setter.SetSeq_entry(*entry.first, &set_info);
        }
        setter.SetLoaded();
    }
}


/////////////////////////////////////////////////////////////////////////////
// CProcessor_SE
/////////////////////////////////////////////////////////////////////////////

CProcessor_SE::CProcessor_SE(CReadDispatcher& dispatcher)
    : CProcessor(dispatcher)
{
}


CProcessor_SE::~CProcessor_SE(void)
{
}


CProcessor::EType CProcessor_SE::GetType(void) const
{
    return eType_Seq_entry;
}


CProcessor::TMagic CProcessor_SE::GetMagic(void) const
{
    static TMagic kMagic = s_GetMagic("SeqE");
    return kMagic;
}


void CProcessor_SE::ProcessObjStream(CReaderRequestResult& result,
                                     const TBlobId& blob_id,
                                     TChunkId chunk_id,
                                     CObjectIStream& obj_stream) const
{
    CLoadLockBlob blob(result, blob_id, chunk_id);
    CLoadLockSetter setter(blob);
    if ( setter.IsLoaded() ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "CProcessor_SE: "
                       "double load of "<<blob_id<<'/'<<chunk_id);
    }
    CRef<CSeq_entry> seq_entry(new CSeq_entry);
    {{
        CStreamDelayBufferGuard guard;
        CWriter* writer = x_GetWriterToSaveBlob(result, blob_id, setter, "SE");
        if ( writer ) {
            guard.StartDelayBuffer(obj_stream);
        }

        SetSeqEntryReadHooks(obj_stream);

        {{
            CReaderRequestResultRecursion r(result);
            
            obj_stream >> *seq_entry;

            LogStat(r, blob_id,
                    CGBRequestStatistics::eStat_LoadBlob,
                    "CProcessor_SE: read seq-entry",
                    obj_stream.GetStreamPos());
        }}

        
        OffsetAllGisToOM(*seq_entry);
        setter.SetSeq_entry(*seq_entry);
        if ( chunk_id == kMain_ChunkId &&
             s_CanBeWGSBlob(blob_id) &&
             result.GetAddWGSMasterDescr() ) {
            AddWGSMaster(setter);
        }
        setter.SetLoaded();

        if ( writer ) {
            const CProcessor_St_SE* prc =
                dynamic_cast<const CProcessor_St_SE*>
                (&m_Dispatcher->GetProcessor(eType_St_Seq_entry));
            if ( prc ) {
                prc->SaveBlob(result, blob_id, chunk_id,
                              setter.GetBlobState(), writer,
                              guard.EndDelayBuffer());
            }
        }
    }}
}


CWriter* CProcessor_SE::x_GetWriterToSaveBlob(CReaderRequestResult& result,
                                              const CBlob_id& blob_id,
                                              CLoadLockSetter& setter,
                                              const char* processor_name) const
{
    if ( !result.IsLoadedBlobVersion(blob_id) ) {
        ERR_POST_X(4, "CProcessor_"<<processor_name<<"::ProcessObjStream: "
                   "blob version is not set");
        return 0;
    }
    if ( setter.GetBlobState() & CBioseq_Handle::fState_no_data ) {
        ERR_POST_X(5, "CProcessor_"<<processor_name<<"::ProcessObjStream: "
                   "state no_data is set");
        return 0;
    }
    return GetWriter(result);
}


/////////////////////////////////////////////////////////////////////////////
// CProcessor_SE_SNP
/////////////////////////////////////////////////////////////////////////////

CProcessor_SE_SNP::CProcessor_SE_SNP(CReadDispatcher& dispatcher)
    : CProcessor_SE(dispatcher)
{
}


CProcessor_SE_SNP::~CProcessor_SE_SNP(void)
{
}


CProcessor::EType CProcessor_SE_SNP::GetType(void) const
{
    return eType_Seq_entry_SNP;
}


CProcessor::TMagic CProcessor_SE_SNP::GetMagic(void) const
{
    static TMagic kMagic = s_GetMagic("SESN");
    return kMagic;
}


void CProcessor_SE_SNP::ProcessObjStream(CReaderRequestResult& result,
                                         const TBlobId& blob_id,
                                         TChunkId chunk_id,
                                         CObjectIStream& obj_stream) const
{
    CLoadLockBlob blob(result, blob_id, chunk_id);
    CLoadLockSetter setter(blob);
    if ( setter.IsLoaded() ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "CProcessor_SE_SNP: "
                       "double load of "<<blob_id<<'/'<<chunk_id);
    }
    CTSE_SetObjectInfo set_info;
    CRef<CSeq_entry> seq_entry(new CSeq_entry);
    {{
        CWriter* writer = x_GetWriterToSaveBlob(result, blob_id, setter, "SE_SNP");

        {{
            CReaderRequestResultRecursion r(result);
            
            CSeq_annot_SNP_Info_Reader::Parse(obj_stream,
                                              Begin(*seq_entry),
                                              set_info);
            
            LogStat(r, blob_id,
                    CGBRequestStatistics::eStat_ParseSNPBlob,
                    "CProcessor_SE_SNP: parse SNP data",
                    obj_stream.GetStreamPos());
        }}

        if ( writer ) {
            if ( set_info.m_Seq_annot_InfoMap.empty() || !seq_entry ) {
                const CProcessor_St_SE* prc =
                    dynamic_cast<const CProcessor_St_SE*>
                    (&m_Dispatcher->GetProcessor(eType_St_Seq_entry));
                if ( prc ) {
                    if ( seq_entry ) {
                        prc->SaveBlob(result, blob_id, chunk_id,
                                      setter.GetBlobState(), writer, *seq_entry);
                    }
                    else {
                        prc->SaveNoBlob(result, blob_id, chunk_id,
                                        setter.GetBlobState(), writer);
                    }
                }
            }
            else {
                const CProcessor_St_SE_SNPT* prc =
                    dynamic_cast<const CProcessor_St_SE_SNPT*>
                    (&m_Dispatcher->GetProcessor(eType_St_Seq_entry_SNPT));
                if ( prc ) {
                    prc->SaveSNPBlob(result, blob_id, chunk_id, writer,
                                     *seq_entry, setter.GetBlobState(), set_info);
                }
            }
        }
    }}
    OffsetAllGisToOM(*seq_entry, &set_info);
    setter.SetSeq_entry(*seq_entry, &set_info);
    setter.SetLoaded();
}


/////////////////////////////////////////////////////////////////////////////
// CProcessor_St_SE
/////////////////////////////////////////////////////////////////////////////

CProcessor_St_SE::CProcessor_St_SE(CReadDispatcher& dispatcher)
    : CProcessor_SE(dispatcher)
{
}


CProcessor_St_SE::~CProcessor_St_SE(void)
{
}


CProcessor::EType CProcessor_St_SE::GetType(void) const
{
    return eType_St_Seq_entry;
}


CProcessor::TMagic CProcessor_St_SE::GetMagic(void) const
{
    static TMagic kMagic = s_GetMagic("StSE");
    return kMagic;
}


void CProcessor_St_SE::ProcessObjStream(CReaderRequestResult& result,
                                        const TBlobId& blob_id,
                                        TChunkId chunk_id,
                                        CObjectIStream& obj_stream) const
{
    CLoadLockBlob blob(result, blob_id, chunk_id);
    TBlobState blob_state;

    {{
        CReaderRequestResultRecursion r(result);
        
        blob_state = ReadBlobState(obj_stream);
        
        LogStat(r, blob_id,
                CGBRequestStatistics::eStat_LoadBlob,
                "CProcessor_St_SE: read state",
                obj_stream.GetStreamPos());
    }}

    result.SetAndSaveBlobState(blob_id, blob_state);
    if ( blob_state & CBioseq_Handle::fState_no_data ) {
        CLoadLockSetter setter(blob);
        if ( !setter.IsLoaded() ) {
            setter.SetLoaded();
        }

        CWriter* writer = GetWriter(result);
        if ( writer ) {
            const CProcessor_St_SE* prc =
                dynamic_cast<const CProcessor_St_SE*>
                (&m_Dispatcher->GetProcessor(eType_St_Seq_entry));
            if ( prc ) {
                prc->SaveNoBlob(result, blob_id, chunk_id,
                                blob_state, writer);
            }
        }
    }
    else {
        CProcessor_SE::ProcessObjStream(result, blob_id, chunk_id, obj_stream);
    }
}


CProcessor::TBlobState
CProcessor_St_SE::ReadBlobState(CObjectIStream& obj_stream) const
{
    return obj_stream.ReadInt4();
}


void CProcessor_St_SE::WriteBlobState(CObjectOStream& obj_stream,
                                      TBlobState blob_state) const
{
    obj_stream.SetFlags(CObjectOStream::fFlagNoAutoFlush);
    obj_stream.WriteInt4(blob_state);
}


CProcessor::TBlobState
CProcessor_St_SE::ReadBlobState(CNcbiIstream& stream) const
{
    CObjectIStreamAsnBinary obj_stream(stream);
    return ReadBlobState(obj_stream);
}


void CProcessor_St_SE::WriteBlobState(CNcbiOstream& stream,
                                      TBlobState blob_state) const
{
    CObjectOStreamAsnBinary obj_stream(stream);
    obj_stream.SetFlags(CObjectOStream::fFlagNoAutoFlush);
    WriteBlobState(obj_stream, blob_state);
}


void CProcessor_St_SE::SaveBlob(CReaderRequestResult& result,
                                const TBlobId& blob_id,
                                TChunkId chunk_id,
                                TBlobState blob_state,
                                CWriter* writer,
                                CRef<CByteSource> byte_source) const
{
    SaveBlob(result, blob_id, chunk_id, blob_state, writer, byte_source->Open());
}


void CProcessor_St_SE::SaveBlob(CReaderRequestResult& result,
                                const TBlobId& blob_id,
                                TChunkId chunk_id,
                                TBlobState blob_state,
                                CWriter* writer,
                                CRef<CByteSourceReader> reader) const
{
    _ASSERT(writer && reader);
    CRef<CWriter::CBlobStream> stream
        (OpenStream(writer, result, blob_id, chunk_id, this));
    if ( !stream ) {
        return;
    }
    try {
        WriteBlobState(**stream, blob_state);
        CWriter::WriteBytes(**stream, reader);
        stream->Close();
    }
    catch ( CException& ) { // ignored
    }
}


void CProcessor_St_SE::SaveBlob(CReaderRequestResult& result,
                                const TBlobId& blob_id,
                                TChunkId chunk_id,
                                TBlobState blob_state,
                                CWriter* writer,
                                const TOctetStringSequence& data) const
{
    _ASSERT(writer);
    CRef<CWriter::CBlobStream> stream
        (OpenStream(writer, result, blob_id, chunk_id, this));
    if ( !stream ) {
        return;
    }
    try {
        WriteBlobState(**stream, blob_state);
        CWriter::WriteBytes(**stream, data);
        stream->Close();
    }
    catch ( CException& ) { // ignored
    }
}


void CProcessor_St_SE::SaveBlob(CReaderRequestResult& result,
                                const TBlobId& blob_id,
                                TChunkId chunk_id,
                                TBlobState blob_state,
                                CWriter* writer,
                                const CSeq_entry& seq_entry) const
{
    _ASSERT(writer);
    CRef<CWriter::CBlobStream> stream
        (OpenStream(writer, result, blob_id, chunk_id, this));
    if ( !stream ) {
        return;
    }
    try {
        {{
            CObjectOStreamAsnBinary obj_stream(**stream);
            obj_stream.SetFlags(CObjectOStream::fFlagNoAutoFlush);
            WriteBlobState(obj_stream, blob_state);
            obj_stream << seq_entry;
        }}
        stream->Close();
    }
    catch ( CException& ) { // ignored
    }
}


void CProcessor_St_SE::SaveNoBlob(CReaderRequestResult& result,
                                  const TBlobId& blob_id,
                                  TChunkId chunk_id,
                                  TBlobState blob_state,
                                  CWriter* writer) const
{
    _ASSERT(writer);
    CRef<CWriter::CBlobStream> stream
        (OpenStream(writer, result, blob_id, chunk_id, this));
    if ( !stream ) {
        return;
    }
    try {
        WriteBlobState(**stream, blob_state);
        stream->Close();
    }
    catch ( CException& ) { // ignored
    }
}


/////////////////////////////////////////////////////////////////////////////
// CProcessor_St_SE_SNPT
/////////////////////////////////////////////////////////////////////////////

CProcessor_St_SE_SNPT::CProcessor_St_SE_SNPT(CReadDispatcher& d)
    : CProcessor_St_SE(d)
{
}


CProcessor_St_SE_SNPT::~CProcessor_St_SE_SNPT(void)
{
}


CProcessor::EType CProcessor_St_SE_SNPT::GetType(void) const
{
    return eType_St_Seq_entry_SNPT;
}


CProcessor::TMagic CProcessor_St_SE_SNPT::GetMagic(void) const
{
    static TMagic kMagic = s_GetMagic("SEST");
    return kMagic;
}


void CProcessor_St_SE_SNPT::ProcessStream(CReaderRequestResult& result,
                                          const TBlobId& blob_id,
                                          TChunkId chunk_id,
                                          CNcbiIstream& stream) const
{
    CLoadLockBlob blob(result, blob_id, chunk_id);
    CLoadLockSetter setter(blob);
    if ( setter.IsLoaded() ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "CProcessor_St_SE_SNPT: "
                       "double load of "<<blob_id<<'/'<<chunk_id);
    }

    TBlobState blob_state = ReadBlobState(stream);
    result.SetAndSaveBlobState(blob_id, blob_state);

    CRef<CSeq_entry> seq_entry(new CSeq_entry);
    CTSE_SetObjectInfo set_info;

    {{
        CReaderRequestResultRecursion r(result);
        Int8 size = NcbiStreamposToInt8(stream.tellg());
        
        CSeq_annot_SNP_Info_Reader::Read(stream, Begin(*seq_entry), set_info);

        size = NcbiStreamposToInt8(stream.tellg()) - size;
        LogStat(r, blob_id,
                CGBRequestStatistics::eStat_LoadSNPBlob,
                "CProcessor_St_SE_SNPT: read SNP table",
                double(size));
    }}
    
    CWriter* writer = GetWriter(result);
    if ( writer ) {
        SaveSNPBlob(result, blob_id, chunk_id, writer, *seq_entry, blob_state, set_info);
    }
    OffsetAllGisToOM(*seq_entry, &set_info);
    setter.SetSeq_entry(*seq_entry, &set_info);
    setter.SetLoaded();
}


void CProcessor_St_SE_SNPT::SaveSNPBlob(CReaderRequestResult& result,
                                        const TBlobId& blob_id,
                                        TChunkId chunk_id,
                                        CWriter* writer,
                                        const CSeq_entry& seq_entry,
                                        TBlobState blob_state,
                                        const CTSE_SetObjectInfo& set_info) const
{
    _ASSERT(writer);
    CRef<CWriter::CBlobStream> stream
        (OpenStream(writer, result, blob_id, chunk_id, this));
    if ( !stream ) {
        return;
    }
    try {
        WriteBlobState(**stream, blob_state);
        CSeq_annot_SNP_Info_Reader::Write(**stream,
                                          ConstBegin(seq_entry), set_info);
        stream->Close();
    }
    catch ( CException& ) { // ignored
    }
}


/////////////////////////////////////////////////////////////////////////////
// CProcessor_ID2
/////////////////////////////////////////////////////////////////////////////

CProcessor_ID2::CProcessor_ID2(CReadDispatcher& d)
    : CProcessor(d)
{
}


CProcessor_ID2::~CProcessor_ID2(void)
{
}


CProcessor::EType CProcessor_ID2::GetType(void) const
{
    return eType_ID2;
}


CProcessor::TMagic CProcessor_ID2::GetMagic(void) const
{
    static TMagic kMagic = s_GetMagic("ID2s");
    return kMagic;
}


void CProcessor_ID2::ProcessObjStream(CReaderRequestResult& result,
                                      const TBlobId& blob_id,
                                      TChunkId chunk_id,
                                      CObjectIStream& obj_stream) const
{
    TBlobState blob_state;
    CID2_Reply_Data data;

    {{
        CReaderRequestResultRecursion r(result);
        
        blob_state = obj_stream.ReadInt4();
        obj_stream >> data;
        
        LogStat(r, blob_id,
                CGBRequestStatistics::eStat_LoadBlob,
                "CProcessor_ID2: read data",
                obj_stream.GetStreamPos());
    }}

    ProcessData(result, blob_id, blob_state, chunk_id, data);
}


void CProcessor_ID2::ProcessData(CReaderRequestResult& result,
                                 const TBlobId& blob_id,
                                 TBlobState blob_state,
                                 TChunkId chunk_id,
                                 const CID2_Reply_Data& data,
                                 int split_version,
                                 const CID2_Reply_Data* skel) const
{
    CLoadLockBlob blob(result, blob_id, chunk_id);
    if ( blob.IsLoadedChunk() ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "CProcessor_ID2: "
                       "double load of "<<blob_id<<'/'<<chunk_id);
    }

    size_t data_size = 0;
    switch ( data.GetData_type() ) {
    case CID2_Reply_Data::eData_type_seq_entry:
    {
        // plain seq-entry
        if ( split_version != 0 || skel ) {
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "CProcessor_ID2: "
                       "plain Seq-entry with extra ID2S-Split-Info");
        }
        if ( chunk_id != kMain_ChunkId && chunk_id != kDelayedMain_ChunkId ) {
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "CProcessor_ID2: "
                       "plain Seq-entry in chunk reply");
        }
        CRef<CSeq_entry> entry(new CSeq_entry);

        {{
            CReaderRequestResultRecursion r(result);
            
            x_ReadData(data, Begin(*entry), data_size);
            
            LogStat(r, blob_id,
                    CGBRequestStatistics::eStat_ParseBlob,
                    "CProcessor_ID2: parsed Seq-entry",
                    data_size);
        }}
        
        result.SetAndSaveBlobState(blob_id, blob_state);
        CLoadLockSetter setter(blob);
        if ( !setter.IsLoaded() ) {
            OffsetAllGisToOM(*entry);
            setter.SetSeq_entry(*entry);
            if ( s_CanBeWGSBlob(blob_id) &&
                 result.GetAddWGSMasterDescr() ) {
                AddWGSMaster(setter);
            }
            setter.SetLoaded();
        }
        
        CWriter* writer = GetWriter(result);
        if ( writer ) {
            if ( data.GetData_format() == data.eData_format_asn_binary &&
                 data.GetData_compression() == data.eData_compression_none &&
                 !s_CacheRecompress() ) {
                // can save as simple Seq-entry
                const CProcessor_St_SE* prc =
                    dynamic_cast<const CProcessor_St_SE*>
                    (&m_Dispatcher->GetProcessor(eType_St_Seq_entry));
                if ( prc ) {
                    prc->SaveBlob(result, blob_id, chunk_id,
                                  blob_state, writer, data.GetData());
                }
            }
            else {
                SaveData(result, blob_id, blob_state, chunk_id, writer, data);
            }
        }
        break;
    }
    case CID2_Reply_Data::eData_type_id2s_split_info:
    {
        if ( chunk_id != kMain_ChunkId && chunk_id != kDelayedMain_ChunkId ) {
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "CProcessor_ID2: "
                       "plain ID2S-Split-Info in non-main reply");
        }
        CRef<CID2S_Split_Info> split_info(new CID2S_Split_Info);

        {{
            CReaderRequestResultRecursion r(result);
            
            x_ReadData(data, Begin(*split_info), data_size);
            
            LogStat(r, blob_id,
                    CGBRequestStatistics::eStat_ParseSplit,
                    "CProcessor_ID2: parsed split info",
                    data_size);
        }}

        bool with_skeleton = split_info->IsSetSkeleton();
        if ( !with_skeleton ) {
            // update skeleton field
            if ( !skel ) {
                NCBI_THROW(CLoaderException, eLoaderFailed,
                           "CProcessor_ID2: "
                           "ID2S-Split-Info without skeleton Seq-entry");
            }

            {{
                CReaderRequestResultRecursion r(result);
                
                x_ReadData(*skel, Begin(split_info->SetSkeleton()), data_size);
                
                LogStat(r, blob_id,
                        CGBRequestStatistics::eStat_ParseChunk,
                        "CProcessor_ID2: parsed Seq-entry",
                        data_size);
            }}
        }

        result.SetAndSaveBlobState(blob_id, blob_state);
        CLoadLockSetter setter(blob);
        if ( !setter.IsLoaded() ) {
            CTSE_LoadLock& lock = setter.GetTSE_LoadLock();
            lock->GetSplitInfo().SetSplitVersion(split_version);
            OffsetAllGisToOM(*split_info);
            CSplitParser::Attach(*lock, *split_info);
            if ( s_CanBeWGSBlob(blob_id) &&
                 result.GetAddWGSMasterDescr() ) {
                AddWGSMaster(setter);
            }
            setter.SetLoaded();
        }

        CWriter* writer = GetWriter(result);
        if ( writer ) {
            if ( with_skeleton ) {
                const CProcessor_ID2_Split* prc =
                    dynamic_cast<const CProcessor_ID2_Split*>
                    (&m_Dispatcher->GetProcessor(eType_ID2_Split));
                if ( prc ) {
                    prc->SaveSplitData(result, blob_id, blob_state, chunk_id,
                                       writer, split_version, data);
                }
            }
            else if ( skel ) {
                const CProcessor_ID2AndSkel* prc =
                    dynamic_cast<const CProcessor_ID2AndSkel*>
                    (&m_Dispatcher->GetProcessor(eType_ID2AndSkel));
                if ( prc ) {
                    prc->SaveDataAndSkel(result, blob_id, blob_state, chunk_id,
                                         writer, split_version, data, *skel);
                }
            }
        }
        break;
    }
    case CID2_Reply_Data::eData_type_id2s_chunk:
    {
        if ( chunk_id == kMain_ChunkId || chunk_id == kDelayedMain_ChunkId ) {
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "CProcessor_ID2: "
                       "ID2S-Chunk in main reply");
        }
        CLoadLockSetter setter(blob);
        if ( setter.IsLoaded() ) {
            break;
        }
        CRef<CID2S_Chunk> chunk(new CID2S_Chunk);
        
        {{
            CReaderRequestResultRecursion r(result);
            
            x_ReadData(data, Begin(*chunk), data_size);
            OffsetAllGisToOM(*chunk);
            CSplitParser::Load(setter.GetTSE_Chunk_Info(), *chunk);
            
            LogStat(r, blob_id, chunk_id,
                    CGBRequestStatistics::eStat_ParseChunk,
                    "CProcessor_ID2: parsed split chunk",
                    data_size);
        }}
        setter.SetLoaded();
        
        CWriter* writer = GetWriter(result);
        if ( writer ) {
            SaveData(result, blob_id, blob_state, chunk_id, writer, data);
        }
        break;
    }
    default:
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "CProcessor_ID2: "
                       "invalid data type: "<<data.GetData_type());
    }
}


void CProcessor_ID2::SaveData(CReaderRequestResult& result,
                              const TBlobId& blob_id,
                              TBlobState blob_state,
                              TChunkId chunk_id,
                              CWriter* writer,
                              const CID2_Reply_Data& data) const
{
    _ASSERT(writer);
    CRef<CWriter::CBlobStream> stream
        (OpenStream(writer, result, blob_id, chunk_id, this));
    if ( !stream ) {
        return;
    }
    try {
        if ( s_CacheRecompress() ) {
            x_FixCompression(const_cast<CID2_Reply_Data&>(data));
        }
        {{
            CObjectOStreamAsnBinary obj_stream(**stream);
            SaveData(obj_stream, blob_state, data);
        }}
        stream->Close();
    }
    catch ( CException& ) { // ignored
    }
}


void CProcessor_ID2::SaveData(CObjectOStream& obj_stream,
                              TBlobState blob_state,
                              const CID2_Reply_Data& data) const
{
    obj_stream.SetFlags(CObjectOStream::fFlagNoAutoFlush);
    obj_stream.WriteInt4(blob_state);
    obj_stream << data;
}


void CProcessor_ID2::x_FixDataFormat(CID2_Reply_Data& data)
{
    // TEMP: TODO: remove this
    if ( data.GetData_format() == CID2_Reply_Data::eData_format_xml &&
         data.GetData_compression()==CID2_Reply_Data::eData_compression_gzip ){
        // FIX old/wrong split fields
        data.SetData_format(CID2_Reply_Data::eData_format_asn_binary);
        data.SetData_compression(CID2_Reply_Data::eData_compression_nlmzip);
        if ( data.GetData_type() > CID2_Reply_Data::eData_type_seq_entry ) {
            data.SetData_type(data.GetData_type()+1);
        }
    }
}


void CProcessor_ID2::x_FixCompression(CID2_Reply_Data& data)
{
    if (data.GetData_compression() != CID2_Reply_Data::eData_compression_none)
        return;
    
    CID2_Reply_Data new_data;
    {{
        COSSWriter writer(new_data.SetData());
        CWStream wstream(&writer);
        CCompressionOStream stream(wstream,
                                   new CZipStreamCompressor(ICompression::eLevel_Lowest),
                                   CCompressionIStream::fOwnProcessor);
        ITERATE ( CID2_Reply_Data::TData, it, data.GetData() ) {
            stream.write(&(**it)[0], (*it)->size());
        }
    }}
    data.SetData().swap(new_data.SetData());
    data.SetData_compression(CID2_Reply_Data::eData_compression_gzip);
}


CObjectIStream* CProcessor_ID2::x_OpenDataStream(const CID2_Reply_Data& data)
{
    x_FixDataFormat(const_cast<CID2_Reply_Data&>(data));
    ESerialDataFormat format;
    switch ( data.GetData_format() ) {
    case CID2_Reply_Data::eData_format_asn_binary:
        format = eSerial_AsnBinary;
        break;
    case CID2_Reply_Data::eData_format_asn_text:
        format = eSerial_AsnText;
        break;
    case CID2_Reply_Data::eData_format_xml:
        format = eSerial_Xml;
        break;
    default:
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "CId2Reader::x_ReadData(): unknown data format");
    }
    auto_ptr<IReader> reader(new COSSReader(data.GetData()));
    auto_ptr<CNcbiIstream> stream;
    switch ( data.GetData_compression() ) {
    case CID2_Reply_Data::eData_compression_none:
        break;
    case CID2_Reply_Data::eData_compression_nlmzip:
        reader.reset(new CNlmZipReader(reader.release(),
                                       CNlmZipReader::fOwnAll));
        break;
    case CID2_Reply_Data::eData_compression_gzip:
        stream.reset(new CRStream(reader.release(),
                                  0, 0, CRWStreambuf::fOwnAll));
        stream.reset(new CCompressionIStream(*stream.release(),
                                             new CZipStreamDecompressor,
                                             CCompressionIStream::fOwnAll));
        break;
    default:
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "CId2Reader::x_ReadData(): unknown data compression");
    }
    if ( !stream.get() ) {
        stream.reset(new CRStream(reader.release(),
                                  0, 0, CRWStreambuf::fOwnAll));
    }
    auto_ptr<CObjectIStream> in;
    in.reset(CObjectIStream::Open(format, *stream.release(), eTakeOwnership));
    return in.release();
}


void CProcessor_ID2::x_ReadData(const CID2_Reply_Data& data,
                                const CObjectInfo& object,
                                size_t& data_size)
{
    auto_ptr<CObjectIStream> in(x_OpenDataStream(data));
    switch ( data.GetData_type() ) {
    case CID2_Reply_Data::eData_type_seq_entry:
        if ( object.GetTypeInfo() != CSeq_entry::GetTypeInfo() ) {
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "CId2Reader::x_ReadData(): unexpected Seq-entry");
        }
        break;
    case CID2_Reply_Data::eData_type_id2s_split_info:
        if ( object.GetTypeInfo() != CID2S_Split_Info::GetTypeInfo() ) {
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "CId2Reader::x_ReadData(): unexpected ID2S-Split-Info");
        }
        break;
    case CID2_Reply_Data::eData_type_id2s_chunk:
        if ( object.GetTypeInfo() != CID2S_Chunk::GetTypeInfo() ) {
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "CId2Reader::x_ReadData(): unexpected ID2S-Chunk");
        }
        break;
    default:
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "CId2Reader::x_ReadData(): unknown data type");
    }
    SetSeqEntryReadHooks(*in);
    in->SetSkipUnknownMembers(eSerialSkipUnknown_Yes);
    in->SetSkipUnknownVariants(eSerialSkipUnknown_Yes);
    in->Read(object);
    data_size += size_t(in->GetStreamPos()); // in-memory size can't be bigger than size_t
}


void CProcessor_ID2::DumpDataAsText(const CID2_Reply_Data& data,
                                    CNcbiOstream& out_stream)
{
    auto_ptr<CObjectIStream> in(x_OpenDataStream(data));
    auto_ptr<CObjectOStream> out(CObjectOStream::Open(eSerial_AsnText,
                                                      out_stream));
    TTypeInfo type;
    switch ( data.GetData_type() ) {
    case CID2_Reply_Data::eData_type_seq_entry:
        type = CSeq_entry::GetTypeInfo();
        break;
    case CID2_Reply_Data::eData_type_id2s_split_info:
        type = CID2S_Split_Info::GetTypeInfo();
        break;
    case CID2_Reply_Data::eData_type_id2s_chunk:
        type = CID2S_Chunk::GetTypeInfo();
        break;
    default:
        return;
    }
    CObjectStreamCopier copier(*in, *out);
    copier.Copy(type);
}


/////////////////////////////////////////////////////////////////////////////
// CProcessor_ID2_Split
/////////////////////////////////////////////////////////////////////////////

CProcessor_ID2_Split::CProcessor_ID2_Split(CReadDispatcher& d)
    : CProcessor_ID2(d)
{
}


CProcessor_ID2_Split::~CProcessor_ID2_Split(void)
{
}


CProcessor::EType CProcessor_ID2_Split::GetType(void) const
{
    return eType_ID2_Split;
}


CProcessor::TMagic CProcessor_ID2_Split::GetMagic(void) const
{
    static TMagic kMagic = s_GetMagic("I2sp");
    return kMagic;
}


void CProcessor_ID2_Split::ProcessObjStream(CReaderRequestResult& result,
                                            const TBlobId& blob_id,
                                            TChunkId chunk_id,
                                            CObjectIStream& obj_stream) const
{
    TBlobState blob_state;
    TSplitVersion split_version;
    CID2_Reply_Data split_data;

    {{
        CReaderRequestResultRecursion r(result);
        
        blob_state = obj_stream.ReadInt4();
        split_version = obj_stream.ReadInt4();
        obj_stream >> split_data;
        
        LogStat(r, blob_id,
                CGBRequestStatistics::eStat_LoadSplit,
                "CProcessor_ID2_Split: read skel",
                obj_stream.GetStreamPos());
    }}

    ProcessData(result, blob_id, blob_state, chunk_id,
                split_data, split_version);
}


void CProcessor_ID2_Split::SaveSplitData(CReaderRequestResult& result,
                                         const TBlobId& blob_id,
                                         TBlobState blob_state,
                                         TChunkId chunk_id,
                                         CWriter* writer,
                                         TSplitVersion split_version,
                                         const CID2_Reply_Data& split) const
{
    _ASSERT(writer);
    CRef<CWriter::CBlobStream> stream
        (OpenStream(writer, result, blob_id, chunk_id, this));
    if ( !stream ) {
        return;
    }
    try {
        if ( s_CacheRecompress() ) {
            x_FixCompression(const_cast<CID2_Reply_Data&>(split));
        }
        {{
            CObjectOStreamAsnBinary obj_stream(**stream);
            SaveSplitData(obj_stream, blob_state, split_version, split);
        }}
        stream->Close();
    }
    catch ( CException& ) { // ignored
    }
}


void CProcessor_ID2_Split::SaveSplitData(CObjectOStream& obj_stream,
                                         TBlobState blob_state,
                                         TSplitVersion split_version,
                                         const CID2_Reply_Data& split) const
{
    obj_stream.SetFlags(CObjectOStream::fFlagNoAutoFlush);
    obj_stream.WriteInt4(blob_state);
    obj_stream.WriteInt4(split_version);
    obj_stream << split;
}


/////////////////////////////////////////////////////////////////////////////
// CProcessor_ID2AndSkel
/////////////////////////////////////////////////////////////////////////////

CProcessor_ID2AndSkel::CProcessor_ID2AndSkel(CReadDispatcher& d)
    : CProcessor_ID2(d)
{
}


CProcessor_ID2AndSkel::~CProcessor_ID2AndSkel(void)
{
}


CProcessor::EType CProcessor_ID2AndSkel::GetType(void) const
{
    return eType_ID2AndSkel;
}


CProcessor::TMagic CProcessor_ID2AndSkel::GetMagic(void) const
{
    static TMagic kMagic = s_GetMagic("I2ss");
    return kMagic;
}


void CProcessor_ID2AndSkel::ProcessObjStream(CReaderRequestResult& result,
                                             const TBlobId& blob_id,
                                             TChunkId chunk_id,
                                             CObjectIStream& obj_stream) const
{
    TBlobState blob_state;
    TSplitVersion split_version;
    CID2_Reply_Data split_data, skel_data;

    {{
        CReaderRequestResultRecursion r(result);
        
        blob_state = obj_stream.ReadInt4();
        split_version = obj_stream.ReadInt4();
        obj_stream >> split_data;
        obj_stream >> skel_data;
        
        LogStat(r, blob_id,
                CGBRequestStatistics::eStat_LoadSplit,
                "CProcessor_ID2AndSkel: read skel",
                obj_stream.GetStreamPos());
    }}

    ProcessData(result, blob_id, blob_state, chunk_id,
                split_data, split_version, ConstRef(&skel_data));
}


void CProcessor_ID2AndSkel::SaveDataAndSkel(CReaderRequestResult& result,
                                            const TBlobId& blob_id,
                                            TBlobState blob_state,
                                            TChunkId chunk_id,
                                            CWriter* writer,
                                            TSplitVersion split_version,
                                            const CID2_Reply_Data& split,
                                            const CID2_Reply_Data& skel) const
{
    _ASSERT(writer);
    CRef<CWriter::CBlobStream> stream
        (OpenStream(writer, result, blob_id, chunk_id, this));
    if ( !stream ) {
        return;
    }
    try {
        if ( s_CacheRecompress() ) {
            x_FixCompression(const_cast<CID2_Reply_Data&>(split));
            x_FixCompression(const_cast<CID2_Reply_Data&>(skel));
        }
        {{
            CObjectOStreamAsnBinary obj_stream(**stream);
            SaveDataAndSkel(obj_stream, blob_state, split_version,
                            split, skel);
        }}
        stream->Close();
    }
    catch ( CException& ) { // ignored
    }
}


void CProcessor_ID2AndSkel::SaveDataAndSkel(CObjectOStream& obj_stream,
                                            TBlobState blob_state,
                                            TSplitVersion split_version,
                                            const CID2_Reply_Data& split,
                                            const CID2_Reply_Data& skel) const
{
    obj_stream.SetFlags(CObjectOStream::fFlagNoAutoFlush);
    obj_stream.WriteInt4(blob_state);
    obj_stream.WriteInt4(split_version);
    obj_stream << split;
    obj_stream << skel;
}


/////////////////////////////////////////////////////////////////////////////
// CProcessor_ExtAnnot
/////////////////////////////////////////////////////////////////////////////


CProcessor_ExtAnnot::CProcessor_ExtAnnot(CReadDispatcher& d)
    : CProcessor(d)
{
}


CProcessor_ExtAnnot::~CProcessor_ExtAnnot(void)
{
}


CProcessor::EType CProcessor_ExtAnnot::GetType(void) const
{
    return eType_ExtAnnot;
}


CProcessor::TMagic CProcessor_ExtAnnot::GetMagic(void) const
{
    static TMagic kMagic = s_GetMagic("EA26");
    return kMagic;
}


void CProcessor_ExtAnnot::ProcessStream(CReaderRequestResult& result,
                                        const TBlobId& blob_id,
                                        TChunkId chunk_id,
                                        CNcbiIstream& /*stream*/) const
{
    Process(result, blob_id, chunk_id);
}


bool CProcessor_ExtAnnot::IsExtAnnot(const TBlobId& blob_id)
{
    switch ( blob_id.GetSubSat() ) {
    case eSubSat_SNP:
    case eSubSat_SNP_graph:
    case eSubSat_MGC:
    case eSubSat_HPRD:
    case eSubSat_tRNA:
    case eSubSat_STS:
    case eSubSat_microRNA:
    case eSubSat_Exon:
        return blob_id.GetSat() == eSat_ANNOT;
    case eSubSat_CDD:
        return blob_id.GetSat() == eSat_ANNOT_CDD;
    default:
        return false;
    }
}


bool CProcessor_ExtAnnot::IsExtAnnot(const TBlobId& blob_id,
                                     TChunkId chunk_id)
{
    return IsExtAnnot(blob_id) && chunk_id == kDelayedMain_ChunkId;
}


void CProcessor_ExtAnnot::Process(CReaderRequestResult& result,
                                  const TBlobId& blob_id,
                                  TChunkId chunk_id) const
{
    if ( !IsExtAnnot(blob_id) || chunk_id != kMain_ChunkId ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "CProcessor_ExtAnnot: "
                       "bad blob "<<blob_id<<'/'<<chunk_id);
    }
    CLoadLockBlob blob(result, blob_id, chunk_id);
    CLoadLockSetter setter(blob);
    if ( setter.IsLoaded() ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "CProcessor_ExtAnnot: "
                       "double load of "<<blob_id<<'/'<<chunk_id);
    }
    // create special external annotations blob
    CAnnotName name;
    SAnnotTypeSelector type;
    vector<SAnnotTypeSelector> more_types;
    string db_name;
    switch ( blob_id.GetSubSat() ) {
    case eSubSat_SNP:
        name.SetNamed("SNP");
        type.SetFeatSubtype(CSeqFeatData::eSubtype_variation);
        db_name = "Annot:SNP";
        break;
    case eSubSat_SNP_graph:
        name.SetNamed("SNP");
        type.SetAnnotType(CSeq_annot::C_Data::e_Graph);
        db_name = "Annot:SNP graph";
        break;
    case eSubSat_CDD:
        name.SetNamed("CDD");
        type.SetFeatSubtype(CSeqFeatData::eSubtype_region);
        more_types.push_back(SAnnotTypeSelector(CSeqFeatData::eSubtype_site));
        db_name = "Annot:CDD";
        break;
    case eSubSat_MGC:
        name.SetNamed("MGC");
        type.SetFeatSubtype(CSeqFeatData::eSubtype_misc_difference);
        db_name = "Annot:MGC";
        break;
    case eSubSat_HPRD:
        name.SetNamed("HPRD");
        type.SetFeatSubtype(CSeqFeatData::eSubtype_site);
        db_name = "Annot:HPRD";
        break;
    case eSubSat_STS:
        name.SetNamed("STS");
        type.SetFeatSubtype(CSeqFeatData::eSubtype_STS);
        db_name = "Annot:STS";
        break;
    case eSubSat_tRNA:
        name.SetNamed("tRNA");
        type.SetFeatSubtype(CSeqFeatData::eSubtype_tRNA);
        db_name = "Annot:tRNA";
        break;
    case eSubSat_microRNA:
        name.SetNamed("other");
        type.SetFeatSubtype(CSeqFeatData::eSubtype_ncRNA);
        more_types.push_back(SAnnotTypeSelector(CSeqFeatData::eSubtype_otherRNA));
        db_name = "Annot:microRNA";
        break;
    case eSubSat_Exon:
        name.SetNamed("Exon");
        type.SetFeatSubtype(CSeqFeatData::eSubtype_exon);
        db_name = "Annot:Exon";
        break;
    default:
        _ASSERT(0 && "unknown annot type");
        break;
    }
    _ASSERT(!db_name.empty());
    if ( name.IsNamed() ) {
        setter.GetTSE_LoadLock()->SetName(name);
    }

    TGi gi = ConvertGiToOM(blob_id.GetSatKey());
    CSeq_id_Handle gih = CSeq_id_Handle::GetGiHandle(gi);
    CSeq_id seq_id;
    seq_id.SetGeneral().SetDb(db_name);
    seq_id.SetGeneral().SetTag().SetId8(gi);
    CSeq_id_Handle seh = CSeq_id_Handle::GetHandle(seq_id);
    
    CRef<CTSE_Chunk_Info> chunk(new CTSE_Chunk_Info(kDelayedMain_ChunkId));
    chunk->x_AddAnnotType(name, type, gih);
    ITERATE ( vector<SAnnotTypeSelector>, it, more_types ) {
        chunk->x_AddAnnotType(name, *it, gih);
    }
    chunk->x_AddBioseqPlace(0);
    chunk->x_AddBioseqId(seh);
    setter.GetSplitInfo().AddChunk(*chunk);
    _ASSERT(setter.GetTSE_LoadLock()->x_NeedsDelayedMainChunk());

    setter.SetLoaded();

    CWriter* writer = GetWriter(result);
    if ( writer ) {
        m_Dispatcher->LoadBlobVersion(result, blob_id);
        CRef<CWriter::CBlobStream> stream =
            (OpenStream(writer, result, blob_id, chunk_id, this));
        if ( stream ) {
            try {
                stream->Close();
            }
            catch ( CException& ) { // ignored
            }
        }
    }
}


CProcessor_AnnotInfo::CProcessor_AnnotInfo(CReadDispatcher& dispatcher)
    : CProcessor(dispatcher)
{
}


CProcessor_AnnotInfo::~CProcessor_AnnotInfo(void)
{
}


CProcessor_AnnotInfo::EType CProcessor_AnnotInfo::GetType(void) const
{
    return eType_AnnotInfo;
}


CProcessor_AnnotInfo::TMagic CProcessor_AnnotInfo::GetMagic(void) const
{
    static TMagic kMagic = s_GetMagic("NANT");
    return kMagic;
}


void CProcessor_AnnotInfo::LoadBlob(CReaderRequestResult& result,
                                    const CBlob_Info& info)
{
    _ASSERT(info.IsSetAnnotInfo());
    const CBlob_id& blob_id = *info.GetBlob_id();
    CLoadLockBlob blob(result, blob_id);
    CLoadLockSetter setter(blob);
    if ( setter.IsLoaded() ) {
        NCBI_THROW_FMT(CLoaderException, eLoaderFailed,
                       "CProcessor_AnnotInfo: "
                       "double load of "<<blob_id);
    }
    
    CRef<CTSE_Chunk_Info> chunk(new CTSE_Chunk_Info(kDelayedMain_ChunkId));
    const CBlob_Annot_Info::TAnnotInfo& annot_infos =
        info.GetAnnotInfo()->GetAnnotInfo();
    ITERATE ( CBlob_Annot_Info::TAnnotInfo, it, annot_infos ) {
        const CID2S_Seq_annot_Info& annot_info = **it;
        // create special external annotations blob
        CAnnotName name(annot_info.GetName());
        if ( name.IsNamed() && !ExtractZoomLevel(name.GetName(), 0, 0) ) {
            setter.GetTSE_LoadLock()->SetName(name);
        }

        vector<SAnnotTypeSelector> types;
        if ( annot_info.IsSetAlign() ) {
            types.push_back(SAnnotTypeSelector(CSeq_annot::C_Data::e_Align));
        }
        if ( annot_info.IsSetGraph() ) {
            types.push_back(SAnnotTypeSelector(CSeq_annot::C_Data::e_Graph));
        }
        if ( annot_info.IsSetFeat() ) {
            ITERATE ( CID2S_Seq_annot_Info::TFeat, it, annot_info.GetFeat() ) {
                const CID2S_Feat_type_Info& finfo = **it;
                int feat_type = finfo.GetType();
                if ( feat_type == 0 ) {
                    types.push_back(SAnnotTypeSelector
                                    (CSeq_annot::C_Data::e_Seq_table));
                }
                else if ( !finfo.IsSetSubtypes() ) {
                    types.push_back(SAnnotTypeSelector
                                    (CSeqFeatData::E_Choice(feat_type)));
                }
                else {
                    ITERATE ( CID2S_Feat_type_Info::TSubtypes,
                              it2, finfo.GetSubtypes() ) {
                        types.push_back(SAnnotTypeSelector
                                        (CSeqFeatData::ESubtype(*it2)));
                    }
                }
            }
        }

        CTSE_Chunk_Info::TLocationSet loc;
        CSplitParser::x_ParseLocation(loc, annot_info.GetSeq_loc());

        ITERATE ( vector<SAnnotTypeSelector>, it, types ) {
            chunk->x_AddAnnotType(name, *it, loc);
        }
    }
    setter.GetSplitInfo().AddChunk(*chunk);
    _ASSERT(setter.GetTSE_LoadLock()->x_NeedsDelayedMainChunk());

    setter.SetLoaded();
}


namespace {
    class CCommandParseBlob : public CReadDispatcherCommand
    {
    public:
        CCommandParseBlob(CReaderRequestResult& result,
                          CGBRequestStatistics::EStatType stat_type,
                          const char* descr,
                          const CBlob_id& blob_id,
                          int chunk_id = -1)
            : CReadDispatcherCommand(result),
              m_StatType(stat_type), m_Descr(descr),
              m_Blob_id(blob_id), m_ChunkId(chunk_id)
            {
            }
        bool IsDone(void) {
            return true;
        }
        bool Execute(CReader& reader) {
            return true;
        }
        string GetErrMsg(void) const {
            return string();
        }
        CGBRequestStatistics::EStatType GetStatistics(void) const
            {
                return m_StatType;
            }
        string GetStatisticsDescription(void) const
            {
                CNcbiOstrstream str;
                str << m_Descr << ' ' << m_Blob_id;
                if ( m_ChunkId >= 0 && m_ChunkId < kMax_Int )
                    str << '.' << m_ChunkId;
                return CNcbiOstrstreamToString(str);
            }
    private:
        CGBRequestStatistics::EStatType m_StatType;
        const string m_Descr;
        const CBlob_id& m_Blob_id;
        int m_ChunkId;
    };
}


void CProcessor::LogStat(CReaderRequestResultRecursion& recursion,
                         const CBlob_id& blob_id,
                         CGBRequestStatistics::EStatType stat_type,
                         const char* descr,
                         double size)
{
    CCommandParseBlob cmd(recursion.GetResult(),
                          stat_type, descr, blob_id);
    CReadDispatcher::LogStat(cmd, recursion, size);
}


void CProcessor::LogStat(CReaderRequestResultRecursion& recursion,
                         const CBlob_id& blob_id,
                         int chunk_id,
                         CGBRequestStatistics::EStatType stat_type,
                         const char* descr,
                         double size)
{
    CCommandParseBlob cmd(recursion.GetResult(),
                          stat_type, descr, blob_id, chunk_id);
    CReadDispatcher::LogStat(cmd, recursion, size);
}


END_SCOPE(objects)
END_NCBI_SCOPE
