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

#include <objtools/data_loaders/genbank/processor.hpp>
#include <objtools/data_loaders/genbank/processors.hpp>
#include <objtools/data_loaders/genbank/request_result.hpp>
#include <objtools/data_loaders/genbank/dispatcher.hpp>
#include <objtools/data_loaders/genbank/reader.hpp>

#include <objtools/data_loaders/genbank/reader_snp.hpp>
#include <objtools/data_loaders/genbank/split_parser.hpp>
#include <objmgr/impl/tse_split_info.hpp>

#include <objects/id1/id1__.hpp>
#include <objects/id2/ID2_Reply_Data.hpp>
#include <objects/seqsplit/ID2S_Split_Info.hpp>
#include <objects/seqsplit/ID2S_Chunk.hpp>
// for read hooks setup
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Imp_feat.hpp>

#include <objmgr/objmgr_exception.hpp>

#include <serial/objistrasnb.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/delaybuf.hpp>
#include <serial/serial.hpp>
#include <serial/iterator.hpp>

#include <util/compress/reader_zlib.hpp>
#include <util/compress/zlib.hpp>
#include <util/rwstream.hpp>

#include <corelib/ncbi_config_value.hpp>
#include <serial/pack_string.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

static const char* const GENBANK_SECTION = "GENBANK";
static const char* const STRING_PACK_ENV = "SNP_PACK_STRINGS";
static const char* const SNP_SPLIT_ENV = "SNP_SPLIT";
static const char* const SNP_TABLE_ENV = "SNP_TABLE";


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
}


/////////////////////////////////////////////////////////////////////////////
// CProcessor
/////////////////////////////////////////////////////////////////////////////


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
    CObjectIStreamAsnBinary obj_stream(stream);
    ProcessObjStream(result, blob_id, chunk_id, obj_stream);
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
    d.InsertProcessor(CRef<CProcessor>(new CProcessor_ID2AndSkel(d)));
    d.InsertProcessor(CRef<CProcessor>(new CProcessor_ExtAnnot(d)));
}


bool CProcessor::TryStringPack(void)
{
    static bool var = CPackString::TryStringPack() &&
        GetConfigFlag(GENBANK_SECTION, STRING_PACK_ENV, true);
    return var;
}


bool CProcessor::TrySNPSplit(void)
{
    static bool var = GetConfigFlag(GENBANK_SECTION, SNP_SPLIT_ENV, true);
    return var;
}


bool CProcessor::TrySNPTable(void)
{
    static bool var = GetConfigFlag(GENBANK_SECTION, SNP_TABLE_ENV, true);
    return var;
}


void CProcessor::SetSeqEntryReadHooks(CObjectIStream& in)
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


void CProcessor::SetSNPReadHooks(CObjectIStream& in)
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


CWriter* CProcessor::GetWriter(CReaderRequestResult& result) const
{
    return m_Dispatcher->GetWriter(result, CWriter::eBlobWriter);
}


bool CProcessor::IsLoaded(const TBlobId& blob_id,
                          TChunkId chunk_id,
                          CLoadLockBlob& blob)
{
    if ( chunk_id == kMain_ChunkId ) {
        return blob.IsLoaded();
    }
    else {
        return blob->GetSplitInfo().GetChunk(chunk_id).IsLoaded();
    }
    //return blob.IsLoaded() && !CProcessor_ExtAnnot::IsExtAnnot(blob_id, blob);
}


void CProcessor::SetLoaded(CReaderRequestResult& result,
                           const TBlobId& blob_id,
                           TChunkId chunk_id,
                           CLoadLockBlob& blob)
{
    if ( chunk_id == kMain_ChunkId ) {
        blob.SetLoaded();
        if ( !blob->IsUnavailable() ) {
            result.AddTSE_Lock(blob);
        }
    }
    else {
        blob->GetSplitInfo().GetChunk(chunk_id).SetLoaded();
    }
}


namespace {
    CRef<CWriter::CBlobStream> OpenStream(CWriter* writer,
                                          CReaderRequestResult& result,
                                          const CProcessor::TBlobId& blob_id,
                                          CProcessor::TChunkId chunk_id,
                                          const CProcessor* processor)
    {
        _ASSERT(writer);
        _ASSERT(processor);
        if ( chunk_id == CProcessor::kMain_ChunkId ) {
            return writer->OpenBlobStream(result, blob_id, *processor);
        }
        else {
            return writer->OpenChunkStream(result, blob_id, chunk_id,
                                           *processor);
        }
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
    CLoadLockBlob blob(result, blob_id);
    if ( IsLoaded(blob_id, chunk_id, blob) ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "CProcessor_ID1: double load of "+
                   blob_id.ToString()+"/"+NStr::IntToString(chunk_id));
    }
    CID1server_back reply;
    {{
        CStreamDelayBufferGuard guard;
        CWriter* writer = GetWriter(result);
        if ( writer ) {
            guard.StartDelayBuffer(obj_stream);
        }

        SetSeqEntryReadHooks(obj_stream);
        obj_stream >> reply;

        TBlobVersion version = GetVersion(reply);
        if ( version >= 0 ) {
            m_Dispatcher->SetAndSaveBlobVersion(result, blob_id, blob,
                                                version);
        }

        if ( writer && blob.IsSetBlobVersion() ) {
            SaveBlob(result, blob_id, chunk_id, writer,
                     guard.EndDelayBuffer());
        }
    }}
    CRef<CSeq_entry> seq_entry = GetSeq_entry(result, blob_id, blob, reply);
    if ( seq_entry ) {
        CSeq_annot_SNP_Info_Reader::TSNP_InfoMap snps;
        blob->SetSeq_entry(*seq_entry, snps);
    }
    SetLoaded(result, blob_id, chunk_id, blob);
}


CRef<CSeq_entry> CProcessor_ID1::GetSeq_entry(CReaderRequestResult& result,
                                              const TBlobId& blob_id,
                                              CLoadLockBlob& blob,
                                              CID1server_back& reply) const
{
    CRef<CSeq_entry> seq_entry;
    TBlobState blob_state = 0;
    switch ( reply.Which() ) {
    case CID1server_back::e_Gotseqentry:
        seq_entry.Reset(&reply.SetGotseqentry());
        break;
    case CID1server_back::e_Gotdeadseqentry:
        blob_state |= CBioseq_Handle::fState_dead;
        seq_entry.Reset(&reply.SetGotdeadseqentry());
        break;
    case CID1server_back::e_Gotsewithinfo:
    {{
        const CID1blob_info& info = reply.GetGotsewithinfo().GetBlob_info();
        if ( info.GetBlob_state() < 0 ) {
            blob_state |= CBioseq_Handle::fState_dead;
        }
        if ( reply.GetGotsewithinfo().IsSetBlob() ) {
            seq_entry.Reset(&reply.SetGotsewithinfo().SetBlob());
        }
        else {
            // no Seq-entry in reply, probably private data
            blob_state |= CBioseq_Handle::fState_no_data;
        }
        if ( info.GetSuppress() ) {
            blob_state |=
                (info.GetSuppress() & 4)
                ? CBioseq_Handle::fState_suppress_temp
                : CBioseq_Handle::fState_suppress_perm;
        }
        if ( info.GetWithdrawn() ) {
            blob_state |= 
                CBioseq_Handle::fState_withdrawn|
                CBioseq_Handle::fState_no_data;
        }
        if ( info.GetConfidential() ) {
            blob_state |=
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
            blob_state |=
                CBioseq_Handle::fState_withdrawn|
                CBioseq_Handle::fState_no_data;
            break;
        case 2:
            blob_state |=
                CBioseq_Handle::fState_confidential|
                CBioseq_Handle::fState_no_data;
            break;
        case 10:
            blob_state |= CBioseq_Handle::fState_no_data;
            break;
        default:
            ERR_POST("CId1Reader::GetMainBlob: ID1server-back.error "<<error);
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "ID1server-back.error "+NStr::IntToString(error));
        }
        break;
    }}
    default:
        // no data
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "CProcessor_ID1::SetSeq_entry:: "
                   "bad ID1server-back type: "+
                   NStr::IntToString(reply.Which()));
    }

    m_Dispatcher->SetAndSaveBlobState(result, blob_id, blob, blob_state);
    return seq_entry;
}


CProcessor::TBlobVersion
CProcessor_ID1::GetVersion(const CID1server_back& reply) const
{
    switch ( reply.Which() ) {
    case CID1server_back::e_Gotblobinfo:
        return abs(reply.GetGotblobinfo().GetBlob_state());
        break;
    case CID1server_back::e_Gotsewithinfo:
        return abs(reply.GetGotsewithinfo().GetBlob_info().GetBlob_state());
        break;
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
    CWriter::WriteBytes(**stream, byte_source);
    stream->Close();
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
    {{
        CObjectOStreamAsnBinary obj_stream(**stream);
        obj_stream << reply;
    }}
    stream->Close();
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
    CLoadLockBlob blob(result, blob_id);
    if ( IsLoaded(blob_id, chunk_id, blob) ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "CProcessor_ID1_SNP: double load of "+
                   blob_id.ToString()+"/"+NStr::IntToString(chunk_id));
    }
    CSeq_annot_SNP_Info_Reader::TSNP_InfoMap snps;
    CID1server_back reply;
    CRef<CSeq_entry> seq_entry;
    {{
        CSeq_annot_SNP_Info_Reader::Parse(obj_stream, Begin(reply), snps);

        TBlobVersion version = GetVersion(reply);
        if ( version >= 0 ) {
            m_Dispatcher->SetAndSaveBlobVersion(result, blob_id, blob,
                                                version);
        }

        seq_entry = GetSeq_entry(result, blob_id, blob, reply);

        CWriter* writer = GetWriter(result);
        if ( writer && blob.IsSetBlobVersion() ) {
            if ( snps.empty() || !seq_entry ) {
                const CProcessor_ID1* prc =
                    dynamic_cast<const CProcessor_ID1*>
                    (&m_Dispatcher->GetProcessor(eType_St_Seq_entry));
                if ( prc ) {
                    prc->SaveBlob(result, blob_id, chunk_id, writer, reply);
                }
            }
            else {
                const CProcessor_St_SE_SNPT* prc =
                    dynamic_cast<const CProcessor_St_SE_SNPT*>
                    (&m_Dispatcher->GetProcessor(eType_St_Seq_entry_SNPT));
                if ( prc ) {
                    prc->SaveBlob(result, blob_id, chunk_id, blob, writer,
                                  *seq_entry, snps);
                }
            }
        }
    }}
    if ( seq_entry ) {
        blob->SetSeq_entry(*seq_entry, snps);
    }
    SetLoaded(result, blob_id, chunk_id, blob);
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
    CLoadLockBlob blob(result, blob_id);
    if ( IsLoaded(blob_id, chunk_id, blob) ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "CProcessor_SE: double load of "+
                   blob_id.ToString()+"/"+NStr::IntToString(chunk_id));
    }
    CRef<CSeq_entry> seq_entry(new CSeq_entry);
    {{
        CStreamDelayBufferGuard guard;
        CWriter* writer = 0;
        if ( !blob.IsSetBlobVersion() ) {
            ERR_POST("CProcessor_SE::ProcessObjStream: "
                     "blob version is not set");
        }
        else if ( blob.GetBlobState() & CBioseq_Handle::fState_no_data ) {
            ERR_POST("CProcessor_SE::ProcessObjStream: "
                     "state no_data is set");
        }
        else {
            writer = GetWriter(result);
            if ( writer ) {
                guard.StartDelayBuffer(obj_stream);
            }
        }

        SetSeqEntryReadHooks(obj_stream);
        obj_stream >> *seq_entry;

        if ( writer ) {
            const CProcessor_St_SE* prc =
                dynamic_cast<const CProcessor_St_SE*>
                (&m_Dispatcher->GetProcessor(eType_St_Seq_entry));
            if ( prc ) {
                prc->SaveBlob(result, blob_id, chunk_id, blob, writer,
                              guard.EndDelayBuffer());
            }
        }
    }}

    CSeq_annot_SNP_Info_Reader::TSNP_InfoMap snps;
    blob->SetSeq_entry(*seq_entry, snps);
    SetLoaded(result, blob_id, chunk_id, blob);
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
    CLoadLockBlob blob(result, blob_id);
    if ( IsLoaded(blob_id, chunk_id, blob) ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "CProcessor_SE_SNP: double load of "+
                   blob_id.ToString()+"/"+NStr::IntToString(chunk_id));
    }
    CSeq_annot_SNP_Info_Reader::TSNP_InfoMap snps;
    CRef<CSeq_entry> seq_entry(new CSeq_entry);
    {{
        CWriter* writer = 0;
        if ( !blob.IsSetBlobVersion() ) {
            ERR_POST("CProcessor_SE_SNP::ProcessObjStream: "
                     "blob version is not set");
        }
        else if ( blob.GetBlobState() & CBioseq_Handle::fState_no_data ) {
            ERR_POST("CProcessor_SE_SNP::ProcessObjStream: "
                     "state no_data is set");
        }
        else {
            writer = GetWriter(result);
        }

        CSeq_annot_SNP_Info_Reader::Parse(obj_stream, Begin(*seq_entry), snps);
        if ( writer ) {
            if ( snps.empty() || !seq_entry ) {
                const CProcessor_St_SE* prc =
                    dynamic_cast<const CProcessor_St_SE*>
                    (&m_Dispatcher->GetProcessor(eType_St_Seq_entry));
                if ( prc ) {
                    if ( seq_entry ) {
                        prc->SaveBlob(result, blob_id, chunk_id,
                                      blob, writer, *seq_entry);
                    }
                    else {
                        prc->SaveNoBlob(result, blob_id, chunk_id,
                                        blob, writer);
                    }
                }
            }
            else {
                const CProcessor_St_SE_SNPT* prc =
                    dynamic_cast<const CProcessor_St_SE_SNPT*>
                    (&m_Dispatcher->GetProcessor(eType_St_Seq_entry_SNPT));
                if ( prc ) {
                    prc->SaveBlob(result, blob_id, chunk_id, blob, writer,
                                  *seq_entry, snps);
                }
            }
        }
    }}
    blob->SetSeq_entry(*seq_entry, snps);
    SetLoaded(result, blob_id, chunk_id, blob);
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
    CLoadLockBlob blob(result, blob_id);
    if ( IsLoaded(blob_id, chunk_id, blob) ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "CProcessor_St_SE: double load of "+
                   blob_id.ToString()+"/"+NStr::IntToString(chunk_id));
    }
    TBlobState blob_state = ReadBlobState(obj_stream);
    m_Dispatcher->SetAndSaveBlobState(result, blob_id, blob, blob_state);
    if ( blob_state & CBioseq_Handle::fState_no_data ) {
        CWriter* writer = GetWriter(result);
        if ( writer ) {
            const CProcessor_St_SE* prc =
                dynamic_cast<const CProcessor_St_SE*>
                (&m_Dispatcher->GetProcessor(eType_St_Seq_entry));
            if ( prc ) {
                prc->SaveNoBlob(result, blob_id, chunk_id, blob, writer);
            }
        }
        SetLoaded(result, blob_id, chunk_id, blob);
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
                                const CLoadLockBlob& blob,
                                CWriter* writer,
                                CRef<CByteSource> byte_source) const
{
    SaveBlob(result, blob_id, chunk_id, blob, writer, byte_source->Open());
}


void CProcessor_St_SE::SaveBlob(CReaderRequestResult& result,
                                const TBlobId& blob_id,
                                TChunkId chunk_id,
                                const CLoadLockBlob& blob,
                                CWriter* writer,
                                CRef<CByteSourceReader> reader) const
{
    _ASSERT(writer && reader);
    CRef<CWriter::CBlobStream> stream
        (OpenStream(writer, result, blob_id, chunk_id, this));
    if ( !stream ) {
        return;
    }
    WriteBlobState(**stream, blob.GetBlobState());
    CWriter::WriteBytes(**stream, reader);
    stream->Close();
}


void CProcessor_St_SE::SaveBlob(CReaderRequestResult& result,
                                const TBlobId& blob_id,
                                TChunkId chunk_id,
                                const CLoadLockBlob& blob,
                                CWriter* writer,
                                const TOctetStringSequence& data) const
{
    _ASSERT(writer);
    CRef<CWriter::CBlobStream> stream
        (OpenStream(writer, result, blob_id, chunk_id, this));
    if ( !stream ) {
        return;
    }
    WriteBlobState(**stream, blob.GetBlobState());
    CWriter::WriteBytes(**stream, data);
    stream->Close();
}


void CProcessor_St_SE::SaveBlob(CReaderRequestResult& result,
                                const TBlobId& blob_id,
                                TChunkId chunk_id,
                                const CLoadLockBlob& blob,
                                CWriter* writer,
                                const CSeq_entry& seq_entry) const
{
    _ASSERT(writer);
    CRef<CWriter::CBlobStream> stream
        (OpenStream(writer, result, blob_id, chunk_id, this));
    if ( !stream ) {
        return;
    }
    {{
        CObjectOStreamAsnBinary obj_stream(**stream);
        obj_stream.SetFlags(CObjectOStream::fFlagNoAutoFlush);
        WriteBlobState(obj_stream, blob.GetBlobState());
        obj_stream << seq_entry;
    }}
    stream->Close();
}


void CProcessor_St_SE::SaveNoBlob(CReaderRequestResult& result,
                                  const TBlobId& blob_id,
                                  TChunkId chunk_id,
                                  const CLoadLockBlob& blob,
                                  CWriter* writer) const
{
    _ASSERT(writer);
    CRef<CWriter::CBlobStream> stream
        (OpenStream(writer, result, blob_id, chunk_id, this));
    if ( !stream ) {
        return;
    }
    WriteBlobState(**stream, blob.GetBlobState());
    stream->Close();
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
    CLoadLockBlob blob(result, blob_id);
    if ( IsLoaded(blob_id, chunk_id, blob) ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "CProcessor_St_SE_SNPT: double load of "+
                   blob_id.ToString()+"/"+NStr::IntToString(chunk_id));
    }
    TBlobState blob_state = ReadBlobState(stream);
    m_Dispatcher->SetAndSaveBlobState(result, blob_id, blob, blob_state);
    CRef<CSeq_entry> seq_entry(new CSeq_entry);
    TSNP_InfoMap snps;
    CSeq_annot_SNP_Info_Reader::Read(stream, Begin(*seq_entry), snps);
    CWriter* writer = GetWriter(result);
    if ( writer ) {
        SaveBlob(result, blob_id, chunk_id, blob, writer, *seq_entry, snps);
    }
    blob->SetSeq_entry(*seq_entry, snps);
    SetLoaded(result, blob_id, chunk_id, blob);
}


void CProcessor_St_SE_SNPT::SaveBlob(CReaderRequestResult& result,
                                     const TBlobId& blob_id,
                                     TChunkId chunk_id,
                                     const CLoadLockBlob& blob,
                                     CWriter* writer,
                                     const CSeq_entry& seq_entry,
                                     const TSNP_InfoMap& snps) const
{
    _ASSERT(writer);
    CRef<CWriter::CBlobStream> stream
        (OpenStream(writer, result, blob_id, chunk_id, this));
    if ( !stream ) {
        return;
    }
    WriteBlobState(**stream, blob.GetBlobState());
    CSeq_annot_SNP_Info_Reader::Write(**stream, ConstBegin(seq_entry), snps);
    stream->Close();
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
    static TMagic kMagic = s_GetMagic("ID2x");
    return kMagic;
}


void CProcessor_ID2::ProcessObjStream(CReaderRequestResult& result,
                                      const TBlobId& blob_id,
                                      TChunkId chunk_id,
                                      CObjectIStream& obj_stream) const
{
    CID2_Reply_Data data;
    obj_stream >> data;
    ProcessData(result, blob_id, chunk_id, data);
}


void CProcessor_ID2::ProcessData(CReaderRequestResult& result,
                                 const TBlobId& blob_id,
                                 TChunkId chunk_id,
                                 const CID2_Reply_Data& data,
                                 int split_version,
                                 CConstRef<CID2_Reply_Data> skel) const
{
    CLoadLockBlob blob(result, blob_id);
    if ( IsLoaded(blob_id, chunk_id, blob) ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "CProcessor_ID2: double load of "+
                   blob_id.ToString()+"/"+NStr::IntToString(chunk_id));
    }
    switch ( data.GetData_type() ) {
    case CID2_Reply_Data::eData_type_seq_entry:
    {
        // plain seq-entry
        if ( split_version != 0 || skel ) {
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "plain Seq-entry with extra ID2S-Split-Info");
        }
        if ( chunk_id != kMain_ChunkId && chunk_id != kDelayedMain_ChunkId ) {
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "plain Seq-entry in chunk reply");
        }
        CRef<CSeq_entry> entry(new CSeq_entry);
        x_ReadData(data, Begin(*entry));
        blob->SetSeq_entry(*entry);
        CWriter* writer = GetWriter(result);
        if ( writer ) {
            if ( data.GetData_format() == data.eData_format_asn_binary &&
                 data.GetData_compression() == data.eData_compression_none ) {
                // can save as simple Seq-entry
                const CProcessor_St_SE* prc =
                    dynamic_cast<const CProcessor_St_SE*>
                    (&m_Dispatcher->GetProcessor(eType_St_Seq_entry));
                if ( prc ) {
                    prc->SaveBlob(result, blob_id, chunk_id,
                                  blob, writer, data.GetData());
                }
            }
            else {
                SaveData(result, blob_id, chunk_id, writer, data);
            }
        }
        break;
    }
    case CID2_Reply_Data::eData_type_id2s_split_info:
    {
        if ( chunk_id != kMain_ChunkId ) {
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "plain ID2S-Split-Info in non-main reply");
        }
        CRef<CID2S_Split_Info> split_info(new CID2S_Split_Info);
        x_ReadData(data, Begin(*split_info));
        bool with_skeleton = split_info->IsSetSkeleton();
        if ( !with_skeleton ) {
            // update skeleton field
            if ( !skel ) {
                NCBI_THROW(CLoaderException, eLoaderFailed,
                           "ID2S-Split-Info without skeleton Seq-entry");
            }
            x_ReadData(*skel, Begin(split_info->SetSkeleton()));
        }
        CSplitParser::Attach(*blob, *split_info);
        blob->GetSplitInfo().SetSplitVersion(split_version);
        CWriter* writer = GetWriter(result);
        if ( writer ) {
            if ( with_skeleton ) {
                SaveData(result, blob_id, chunk_id, writer, data);
            }
            else if ( skel ) {
                const CProcessor_ID2AndSkel* prc =
                    dynamic_cast<const CProcessor_ID2AndSkel*>
                    (&m_Dispatcher->GetProcessor(eType_ID2AndSkel));
                if ( prc ) {
                    prc->SaveDataAndSkel(result, blob_id, chunk_id,
                                         blob, writer, split_version,
                                         data, *skel);
                }
            }
        }
        break;
    }
    case CID2_Reply_Data::eData_type_id2s_chunk:
    {
        if ( chunk_id == kMain_ChunkId || chunk_id == kDelayedMain_ChunkId ) {
            NCBI_THROW(CLoaderException, eLoaderFailed,
                       "ID2S-Chunk in main reply");
        }
        CTSE_Chunk_Info& chunk_info = blob->GetSplitInfo().GetChunk(chunk_id);
        if ( chunk_info.IsLoaded() ) {
            return;
        }
        
        CRef<CID2S_Chunk> chunk(new CID2S_Chunk);
        x_ReadData(data, Begin(*chunk));
        CSplitParser::Load(chunk_info, *chunk);
        
        CWriter* writer = GetWriter(result);
        if ( writer ) {
            SaveData(result, blob_id, chunk_id, writer, data);
        }
        break;
    }
    default:
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "invalid data type: "+
                   NStr::IntToString(data.GetData_type()));
    }
    SetLoaded(result, blob_id, chunk_id, blob);
}


void CProcessor_ID2::SaveData(CReaderRequestResult& result,
                              const TBlobId& blob_id,
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
    {{
        CObjectOStreamAsnBinary obj_stream(**stream);
        obj_stream << data;
    }}
    stream->Close();
}


void CProcessor_ID2::x_FixDataFormat(const CID2_Reply_Data& data)
{
    // TEMP: TODO: remove this
    if ( data.GetData_format() == CID2_Reply_Data::eData_format_xml &&
         data.GetData_compression()==CID2_Reply_Data::eData_compression_gzip ){
        // FIX old/wrong split fields
        const_cast<CID2_Reply_Data&>(data)
            .SetData_format(CID2_Reply_Data::eData_format_asn_binary);
        const_cast<CID2_Reply_Data&>(data)
            .SetData_compression(CID2_Reply_Data::eData_compression_nlmzip);
        if ( data.GetData_type() > CID2_Reply_Data::eData_type_seq_entry ) {
            const_cast<CID2_Reply_Data&>(data)
                .SetData_type(data.GetData_type()+1);
        }
    }
}


CObjectIStream* CProcessor_ID2::x_OpenDataStream(const CID2_Reply_Data& data)
{
    x_FixDataFormat(data);
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
    in.reset(CObjectIStream::Open(format, *stream.release(), true));
    return in.release();
}


void CProcessor_ID2::x_ReadData(const CID2_Reply_Data& data,
                                const CObjectInfo& object)
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
    in->Read(object);
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
    static TMagic kMagic = s_GetMagic("I2sk");
    return kMagic;
}


void CProcessor_ID2AndSkel::ProcessObjStream(CReaderRequestResult& result,
                                             const TBlobId& blob_id,
                                             TChunkId chunk_id,
                                             CObjectIStream& obj_stream) const
{
    TSplitVersion split_version;
    CID2_Reply_Data split_data, skel_data;
    split_version = obj_stream.ReadInt4();
    obj_stream >> split_data;
    obj_stream >> skel_data;
    ProcessData(result, blob_id, chunk_id,
                split_data, split_version, ConstRef(&skel_data));
}


void CProcessor_ID2AndSkel::SaveDataAndSkel(CReaderRequestResult& result,
                                            const TBlobId& blob_id,
                                            TChunkId chunk_id,
                                            const CLoadLockBlob& blob,
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
    {{
        CObjectOStreamAsnBinary obj_stream(**stream);
        SaveDataAndSkel(obj_stream, split_version, split, skel);
    }}
    stream->Close();
}


void CProcessor_ID2AndSkel::SaveDataAndSkel(CObjectOStream& obj_stream,
                                            TSplitVersion split_version,
                                            const CID2_Reply_Data& split,
                                            const CID2_Reply_Data& skel) const
{
    obj_stream.SetFlags(CObjectOStream::fFlagNoAutoFlush);
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
                                        CNcbiIstream& stream) const
{
    Process(result, blob_id, chunk_id);
}


bool CProcessor_ExtAnnot::IsExtAnnot(const TBlobId& blob_id)
{
    return blob_id.GetSat() == eSat_ANNOT &&
        (blob_id.GetSubSat() == eSubSat_SNP ||
         blob_id.GetSubSat() == eSubSat_CDD ||
         blob_id.GetSubSat() == eSubSat_SNP_graph ||
         blob_id.GetSubSat() == eSubSat_MGC);
}


bool CProcessor_ExtAnnot::IsExtAnnot(const TBlobId& blob_id,
                                     TChunkId chunk_id)
{
    return IsExtAnnot(blob_id) && chunk_id == kDelayedMain_ChunkId;
}


bool CProcessor_ExtAnnot::IsExtAnnot(const TBlobId& blob_id,
                                     CLoadLockBlob& blob)
{
    if ( !IsExtAnnot(blob_id) || blob->HasSeq_entry() ) {
        return false;
    }
    try {
        // ok it's special processing of external annotation
        return !blob->GetSplitInfo().GetChunk(kDelayedMain_ChunkId).IsLoaded();
    }
    catch ( ... ) {
    }
    return false;
}


void CProcessor_ExtAnnot::Process(CReaderRequestResult& result,
                                  const TBlobId& blob_id,
                                  TChunkId chunk_id) const
{
    if ( !IsExtAnnot(blob_id) || chunk_id != kMain_ChunkId ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "CProcessor_ExtAnnot: bad blob "+
                   blob_id.ToString()+"/"+NStr::IntToString(chunk_id));
    }
    CLoadLockBlob blob(result, blob_id);
    if ( IsLoaded(blob_id, chunk_id, blob) ) {
        NCBI_THROW(CLoaderException, eLoaderFailed,
                   "CProcessor_ExtAnnot: double load of "+
                   blob_id.ToString()+"/"+NStr::IntToString(chunk_id));
    }
    // create special external annotations blob
    CAnnotName name;
    SAnnotTypeSelector type;
    string db_name;
    if ( blob_id.GetSubSat() == eSubSat_SNP ) {
        blob->SetName("SNP");
        name.SetNamed("SNP");
        type.SetFeatSubtype(CSeqFeatData::eSubtype_variation);
        db_name = "Annot:SNP";
    }
    else if ( blob_id.GetSubSat() == eSubSat_CDD ) {
        name.SetNamed("CDDSearch");
        type.SetFeatSubtype(CSeqFeatData::eSubtype_region);
        db_name = "Annot:CDD";
    }
    else if ( blob_id.GetSubSat() == eSubSat_SNP_graph ) {
        blob->SetName("SNP");
        name.SetNamed("SNP");
        type.SetAnnotType(CSeq_annot::C_Data::e_Graph);
        db_name = "Annot:SNP graph";
    }
    else if ( blob_id.GetSubSat() == eSubSat_MGC ) {
        type.SetFeatSubtype(CSeqFeatData::eSubtype_misc_difference);
        db_name = "Annot:MGC";
    }
    _ASSERT(!db_name.empty());

    int gi = blob_id.GetSatKey();
    CSeq_id seq_id;
    seq_id.SetGeneral().SetDb(db_name);
    seq_id.SetGeneral().SetTag().SetId(gi);
    
    CRef<CTSE_Chunk_Info> chunk(new CTSE_Chunk_Info(kDelayedMain_ChunkId));
    chunk->x_AddAnnotType(name, type, CSeq_id_Handle::GetGiHandle(gi));
    chunk->x_AddBioseqPlace(0);
    chunk->x_AddBioseqId(CSeq_id_Handle::GetHandle(seq_id));
    blob->GetSplitInfo().AddChunk(*chunk);

    SetLoaded(result, blob_id, chunk_id, blob);

    CWriter* writer = GetWriter(result);
    if ( writer ) {
        if ( !blob.IsSetBlobVersion() ) {
            m_Dispatcher->LoadBlobVersion(result, blob_id);
            if ( !blob.IsSetBlobVersion() ) {
                return;
            }
        }
        CRef<CWriter::CBlobStream> stream =
            (OpenStream(writer, result, blob_id, chunk_id, this));
        if ( stream ) {
            stream->Close();
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
