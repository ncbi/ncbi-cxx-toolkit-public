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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *   Processor of ID2 requests for SNP data
 *
 */

#include <ncbi_pch.hpp>
#include <sra/data_loaders/snp/impl/id2snp_impl.hpp>
#include <objects/id2/id2processor_interface.hpp>
#include <sra/data_loaders/snp/id2snp_params.h>
#include <sra/readers/sra/snpread.hpp>
#include <sra/error_codes.hpp>
#include <corelib/reader_writer.hpp>
#include <corelib/rwstream.hpp>
#include <util/compress/zlib.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/serial.hpp>
#include <objects/id2/id2__.hpp>
#include <objects/seqsplit/seqsplit__.hpp>
#include <objects/general/general__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seqres/seqres__.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqsplit/ID2S_Split_Info.hpp>
#include <objects/seqsplit/ID2S_Chunk.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/annot_selector.hpp>

BEGIN_NCBI_NAMESPACE;

#define NCBI_USE_ERRCODE_X   ID2SNPProcessor
NCBI_DEFINE_ERR_SUBCODE_X(24);

BEGIN_NAMESPACE(objects);

// behavior options
#define TRACE_PROCESSING

enum EResolveMaster {
    eResolveMaster_never,
    eResolveMaster_without_gi,
    eResolveMaster_always
};
static const EResolveMaster kResolveMaster = eResolveMaster_never;

// default configuration parameters
#define DEFAULT_VDB_CACHE_SIZE 10
#define DEFAULT_INDEX_UPDATE_TIME 600
#define DEFAULT_COMPRESS_DATA CID2SNPContext::eCompressData_some

// debug levels
enum EDebugLevel {
    eDebug_none     = 0,
    eDebug_error    = 1,
    eDebug_open     = 2,
    eDebug_request  = 5,
    eDebug_replies  = 6,
    eDebug_resolve  = 7,
    eDebug_data     = 8,
    eDebug_all      = 9
};

// SNP accession parameters


// parameters reading
NCBI_PARAM_DECL(bool, ID2SNP, ENABLE);
NCBI_PARAM_DEF_EX(bool, ID2SNP, ENABLE, true,
                  eParam_NoThread, ID2SNP_ENABLE);


NCBI_PARAM_DECL(int, ID2SNP, DEBUG);
NCBI_PARAM_DEF_EX(int, ID2SNP, DEBUG, eDebug_error,
                  eParam_NoThread, ID2SNP_DEBUG);


NCBI_PARAM_DECL(bool, ID2SNP, FILTER_ALL);
NCBI_PARAM_DEF_EX(bool, ID2SNP, FILTER_ALL, true,
                  eParam_NoThread, ID2SNP_FILTER_ALL);


static inline bool s_Enabled(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(ID2SNP, ENABLE)> s_Value;
    return s_Value->Get();
}


static inline int s_DebugLevel(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(ID2SNP, DEBUG)> s_Value;
    return s_Value->Get();
}


static inline bool s_DebugEnabled(EDebugLevel level)
{
    return s_DebugLevel() >= level;
}


static inline bool s_FilterAll(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(ID2SNP, FILTER_ALL)> s_Value;
    return s_Value->Get();
}


/////////////////////////////////////////////////////////////////////////////
// CID2SNPProcessor_Impl
/////////////////////////////////////////////////////////////////////////////

// Blob id
// sat = 2001-2099 : SNP NA version 1 - 99
// subsat : NA accession number
// satkey : SequenceIndex + 1000000*FilterIndex;
// satkey bits 24-30: 

const int kSNPSatBase = 2000;
const int kNAIndexDigits = 9;
const int kNAIndexMin = 1;
const int kNAIndexMax = 999999999;
const int kNAVersionDigitsMin = 1;
const int kNAVersionDigitsMax = 2;
const int kNALengthMin = 2 + kNAIndexDigits + 1 + kNAVersionDigitsMin; // NA000000000.0
const int kNALengthMax = 2 + kNAIndexDigits + 1 + kNAVersionDigitsMax; // NA000000000.00
const int kNAVersionMin = 1;
const int kNAVersionMax = 99;
const int kSeqIndexCount = 1000000;
const int kFilterIndexCount = 2000;
const int kFilterIndexMaxLength = 4;


// splitter parameters for SNPs and graphs
static const int kTSEId = 1;
static const int kChunkIdFeat = 0;
static const int kChunkIdGraph = 1;
static const int kChunkIdMul = 2;
static const TSeqPos kFeatChunkSize = 1000000;
static const TSeqPos kGraphChunkSize = 10000000;


BEGIN_LOCAL_NAMESPACE;


template<class Cont>
typename Cont::value_type::TObjectType& sx_AddNew(Cont& cont)
{
    typename Cont::value_type obj(new typename Cont::value_type::TObjectType);
    cont.push_back(obj);
    return *obj;
}


void sx_SetZoomLevel(CSeq_annot& annot, int zoom_level)
{
    CUser_object& obj = sx_AddNew(annot.SetDesc().Set()).SetUser();
    obj.SetType().SetStr("AnnotationTrack");
    obj.AddField("ZoomLevel", zoom_level);
}


bool IsValidNAIndex(size_t na_index)
{
    return na_index >= kNAIndexMin && na_index <= kNAIndexMax;
}


bool IsValidNAVersion(size_t na_version)
{
    return na_version >= kNAVersionMin && na_version <= kNAVersionMax;
}


bool IsValidSeqIndex(size_t seq_index)
{
    return seq_index < kSeqIndexCount;
}


bool IsValidFilterIndex(size_t filter_index)
{
    return filter_index < kFilterIndexCount;
}


string GetNAAccession(const SSNPDbTrackInfo& track)
{
    CNcbiOstrstream str;
    str << "NA" << setw(kNAIndexDigits) << setfill('0') << track.m_NAIndex
        << '.' << track.m_NAVersion;
    return CNcbiOstrstreamToString(str);
}


string FormatTrack(const SSNPDbTrackInfo& track)
{
    CNcbiOstrstream str;
    str << "NA" << setw(kNAIndexDigits) << setfill('0') << track.m_NAIndex
        << '.' << track.m_NAVersion
        << '#' << (track.m_FilterIndex+1);
    return CNcbiOstrstreamToString(str);
}


SSNPDbTrackInfo ParseTrack(CTempString acc_filter)
{
    SSNPDbTrackInfo ret;
    // NA123456789.1#1234
    size_t hash_pos = acc_filter.find('#');
    if ( hash_pos == NPOS ) {
        return ret;
    }
    CTempString acc = acc_filter.substr(0, hash_pos);
    CTempString filter = acc_filter.substr(hash_pos+1);
    if ( acc.size() < kNALengthMin || acc.size() > kNALengthMax ||
         acc[0] != 'N' || acc[1] != 'A' || acc[2+kNAIndexDigits] != '.' ) {
        return ret;
    }
    if ( filter.empty() || filter[0] == '0' || filter.size() > kFilterIndexMaxLength ) {
        return ret;
    }
    size_t na_index = NStr::StringToNumeric<size_t>(acc.substr(2, kNAIndexDigits),
                                                    NStr::fConvErr_NoThrow);
    if ( !IsValidNAIndex(na_index) ) {
        return ret;
    }
    size_t na_version = NStr::StringToNumeric<size_t>(acc.substr(2+kNAIndexDigits+1),
                                                      NStr::fConvErr_NoThrow);
    if ( !IsValidNAVersion(na_version) ) {
        return ret;
    }
    size_t filter_index = NStr::StringToNumeric<size_t>(filter,
                                                        NStr::fConvErr_NoThrow)-1;
    if ( !IsValidFilterIndex(filter_index) ) {
        return ret;
    }
    ret.m_NAIndex = na_index;
    ret.m_NAVersion = na_version;
    ret.m_FilterIndex = filter_index;
    return ret;
}


#ifdef TRACE_PROCESSING

static CStopWatch sw;

# define START_TRACE() do { if(s_DebugLevel()>0)sw.Restart(); } while(0)

CNcbiOstream& operator<<(CNcbiOstream& out,
                         const CID2SNPProcessor_Impl::SSNPEntryInfo& seq)
{
    return out << FormatTrack(seq.m_Track) << '/' << seq.m_SeqIndex;
}
# define TRACE_X(t,l,m)                                                 \
    do {                                                                \
        if ( s_DebugEnabled(l) ) {                                      \
            LOG_POST_X(t, Info<<sw.Elapsed()<<": ID2SNP: "<<m);         \
        }                                                               \
    } while(0)
#else
# define START_TRACE() do{}while(0)
# define TRACE_X(t,l,m) do{}while(0)
#endif


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


size_t sx_GetSize(const CID2_Reply_Data& data)
{
    size_t size = 0;
    ITERATE ( CID2_Reply_Data::TData, it, data.GetData() ) {
        size += (*it)->size();
    }
    return size;
}


END_LOCAL_NAMESPACE;


CID2SNPContext::CID2SNPContext(void)
    : m_CompressData(eCompressData_never),
      m_ExplicitBlobState(false),
      m_AllowVDB(false)
{
}


CID2SNPProcessor_Impl::CID2SNPProcessor_Impl(const CConfig::TParamTree* params,
                                             const string& driver_name)
{
    unique_ptr<CConfig::TParamTree> app_params;
    if ( !params ) {
        if ( CNcbiApplicationGuard app = CNcbiApplication::InstanceGuard() ) {
            app_params.reset(CConfig::ConvertRegToTree(app->GetConfig()));
            params = app_params.get();
        }
    }
    if ( params ) {
        params = params->FindSubNode(CInterfaceVersion<CID2Processor>::GetName());
    }
    if ( params ) {
        params = params->FindSubNode(driver_name);
    }
    CConfig conf(params);

    size_t cache_size =
        conf.GetInt(driver_name,
                    NCBI_ID2PROC_SNP_PARAM_VDB_CACHE_SIZE,
                    CConfig::eErr_NoThrow,
                    DEFAULT_VDB_CACHE_SIZE);
    TRACE_X(23, eDebug_open, "ID2SNP: cache_size = "<<cache_size);
    m_SNPDbCache.set_size_limit(cache_size);

    int compress_data =
        conf.GetInt(driver_name,
                    NCBI_ID2PROC_SNP_PARAM_COMPRESS_DATA,
                    CConfig::eErr_NoThrow,
                    DEFAULT_COMPRESS_DATA);
    if ( compress_data >= CID2SNPContext::eCompressData_never &&
         compress_data <= CID2SNPContext::eCompressData_always ) {
        m_InitialContext.m_CompressData =
            CID2SNPContext::ECompressData(compress_data);
    }
    TRACE_X(23, eDebug_open, "ID2SNP: compress_data = "<<m_InitialContext.m_CompressData);
}


CID2SNPProcessor_Impl::~CID2SNPProcessor_Impl(void)
{
}


void CID2SNPProcessor_Impl::InitContext(CID2SNPContext& context,
                                        const CID2_Request& request)
{
    context = GetInitialContext();
    if ( request.IsSetParams() ) {
        // check if blob-state field is allowed
        ITERATE ( CID2_Request::TParams::Tdata, it, request.GetParams().Get() ) {
            const CID2_Param& param = **it;
            if ( param.GetName() == "id2:allow" && param.IsSetValue() ) {
                ITERATE ( CID2_Param::TValue, it2, param.GetValue() ) {
                    if ( *it2 == "*.blob-state" ) {
                        context.m_ExplicitBlobState = true;
                    }
                    if ( *it2 == "vdb-snp" ) {
                        context.m_AllowVDB = true;
                    }
                }
            }
        }
    }
}


CSNPDb CID2SNPProcessor_Impl::GetSNPDb(const string& na)
{
    CMutexGuard guard(m_Mutex);
    TSNPDbCache::iterator it = m_SNPDbCache.find(na);
    if ( it != m_SNPDbCache.end() ) {
        return it->second;
    }
    try {
        CSNPDb snp_db(m_Mgr, na);
        m_SNPDbCache[na] = snp_db;
        TRACE_X(1, eDebug_open, "GetSNPDb: "<<na);
        return snp_db;
    }
    catch ( CSraException& exc ) {
        if ( exc.GetErrCode() == exc.eNotFoundDb ||
             exc.GetErrCode() == exc.eProtectedDb ) {
            // no such SNP table
        }
        else {
            TRACE_X(22, eDebug_error, "ID2SNP: "
                    "Exception while opening SNP DB "<<na<<": "<<exc);
        }
    }
    catch ( CException& exc ) {
        TRACE_X(22, eDebug_error, "ID2SNP: "
                "Exception while opening SNP DB "<<na<<": "<<exc);
    }
    catch ( exception& exc ) {
        TRACE_X(22, eDebug_error, "ID2SNP: "
                "Exception while opening SNP DB "<<na<<": "<<exc.what());
    }
    return CSNPDb();
}


CSNPDb& CID2SNPProcessor_Impl::GetSNPDb(SSNPEntryInfo& seq)
{
    if ( !seq.m_SNPDb ) {
        seq.m_SNPDb = GetSNPDb(GetNAAccession(seq.m_Track));
        if ( seq.m_SNPDb ) {
            seq.m_Valid = true;
        }
    }
    return seq.m_SNPDb;
}


void CID2SNPProcessor_Impl::ResetIteratorCache(SSNPEntryInfo& seq)
{
    seq.m_SeqIter.Reset();
    seq.m_BlobId.Reset();
}


CSNPDbSeqIterator& CID2SNPProcessor_Impl::GetSeqIterator(SSNPEntryInfo& seq)
{
    if ( !seq.m_SeqIter ) {
        CSNPDb& db = GetSNPDb(seq);
        seq.m_SeqIter = CSNPDbSeqIterator(db, seq.m_SeqIndex);
        if ( seq.m_Track.m_FilterIndex ) {
            seq.m_SeqIter.SetTrack(CSNPDbTrackIterator(db, seq.m_Track.m_FilterIndex));
        }
    }
    return seq.m_SeqIter;
}


CID2_Blob_Id& CID2SNPProcessor_Impl::x_GetBlobId(SSNPEntryInfo& seq)
{
    if ( seq.m_BlobId ) {
        return *seq.m_BlobId;
    }
    CRef<CID2_Blob_Id> id(new CID2_Blob_Id);
    id->SetSat(kSNPSatBase + seq.m_Track.m_NAVersion);
    id->SetSub_sat(seq.m_Track.m_NAIndex);
    id->SetSat_key(seq.m_SeqIndex + seq.m_Track.m_FilterIndex * kSeqIndexCount);
    seq.m_BlobId = id;
    return *id;
}


CID2SNPProcessor_Impl::SSNPEntryInfo
CID2SNPProcessor_Impl::x_ResolveBlobId(const CID2_Blob_Id& id)
{
    SSNPEntryInfo seq;
    if ( id.GetSat() < kSNPSatBase + kNAVersionMin ||
         id.GetSat() > kSNPSatBase + kNAVersionMax ) {
        return SSNPEntryInfo();
    }
    seq.m_Track.m_NAVersion = id.GetSat() - kSNPSatBase;
    seq.m_Track.m_NAIndex = id.GetSub_sat();
    if ( !IsValidNAIndex(seq.m_Track.m_NAIndex) ) {
        return SSNPEntryInfo();
    }
    seq.m_SeqIndex = id.GetSat_key() % kSeqIndexCount;
    if ( !IsValidSeqIndex(seq.m_SeqIndex) ) {
        return SSNPEntryInfo();
    }
    seq.m_Track.m_FilterIndex = id.GetSat_key() / kSeqIndexCount;
    if ( !IsValidFilterIndex(seq.m_Track.m_FilterIndex) ) {
        return SSNPEntryInfo();
    }
    if ( CSNPDb snp_db = GetSNPDb(seq) ) {
        seq.m_Valid = true;
    }
    return seq;
}


CID2SNPProcessor_Impl::SSNPEntryInfo
CID2SNPProcessor_Impl::x_ResolveBlobId(const SSNPDbTrackInfo& track,
                                       const string& acc_ver)
{
    SSNPEntryInfo seq;
    if ( CSNPDb snp_db = GetSNPDb(GetNAAccession(track)) ) {
        CSNPDbSeqIterator seq_iter(snp_db, CSeq_id_Handle::GetHandle(acc_ver));
        if ( seq_iter ) {
            seq.m_Track = track;
            seq.m_SeqIndex = seq_iter.GetVDBSeqIndex();
            seq.m_SNPDb = snp_db;
            seq.m_Valid = true;
            seq.m_SeqIter = seq_iter;
        }
    }
    return seq;
}


bool CID2SNPProcessor_Impl::WorthCompressing(const SSNPEntryInfo& /*seq*/)
{
    return false;
}


void CID2SNPProcessor_Impl::WriteData(CID2SNPContext& context,
                                      const SSNPEntryInfo& seq,
                                      CID2_Reply_Data& data,
                                      const CSerialObject& obj)
{
    data.SetData_format(CID2_Reply_Data::eData_format_asn_binary);
    COSSWriter writer(data.SetData());
    CWStream writer_stream(&writer);
    AutoPtr<CNcbiOstream> str;
    if ( (context.m_CompressData == CID2SNPContext::eCompressData_always) ||
         (context.m_CompressData == CID2SNPContext::eCompressData_some && WorthCompressing(seq)) ) {
        data.SetData_compression(CID2_Reply_Data::eData_compression_gzip);
        str.reset(new CCompressionOStream(writer_stream,
                                          new CZipStreamCompressor,
                                          CCompressionIStream::fOwnProcessor));
    }
    else {
        data.SetData_compression(CID2_Reply_Data::eData_compression_none);
        str.reset(&writer_stream, eNoOwnership);
    }
    CObjectOStreamAsnBinary objstr(*str);
    objstr << obj;
}


bool CID2SNPProcessor_Impl::x_GetAccVer(string& acc_ver, const CSeq_id& id)
{
    if ( !acc_ver.empty() ) {
        return true;
    }
    if ( const CTextseq_id* text_id = id.GetTextseq_Id() ) {
        if ( text_id->IsSetAccession() && !text_id->GetAccession().empty() &&
             text_id->IsSetVersion() && text_id->GetVersion() > 0 ) {
            // fully qualified text id, no more information is necessary
            acc_ver = text_id->GetAccession()+'.'+NStr::NumericToString(text_id->GetVersion());
            return true;
        }
    }
    return false;
}


void CID2SNPProcessor_Impl::x_AddSeqIdRequest(CID2_Request_Get_Seq_id& request,
                                              CID2SNPProcessorPacketContext::SRequestInfo& info)
{
    CID2_Request_Get_Seq_id::TSeq_id_type request_type = request.GetSeq_id_type();
    info.m_OriginalSeqIdType = request_type;
    if ( request.GetSeq_id().IsSeq_id() &&
         x_GetAccVer(info.m_SeqAcc, request.GetSeq_id().GetSeq_id()) ) {
        return;
    }
    if ( request_type == CID2_Request_Get_Seq_id::eSeq_id_type_any ) {
        // ask for all Seq-ids instead of any
        request.SetSeq_id_type(CID2_Request_Get_Seq_id::eSeq_id_type_all);
    }
    else if ( request_type & CID2_Request_Get_Seq_id::eSeq_id_type_text ) {
        // text seq-id already asked
    }
    else {
        // add text seq-id to the requested type set
        request.SetSeq_id_type(request_type | CID2_Request_Get_Seq_id::eSeq_id_type_text);
    }
}


CID2SNPProcessor_Impl::EProcessStatus
CID2SNPProcessor_Impl::x_ProcessGetBlobId(CID2SNPContext& context,
                                          CID2SNPProcessorPacketContext& packet_context,
                                          TReplies& replies,
                                          CID2_Request& main_request,
                                          CID2_Request_Get_Blob_Id& request)
{
    START_TRACE();
    TRACE_X(7, eDebug_request, "GetBlobId: "<<MSerial_AsnText<<main_request);
    if ( request.IsSetSources() ) {
        CID2SNPProcessorPacketContext::SRequestInfo* info = 0;
        // move SNP NAs from ID2 sources to m_SNPAccs
        ERASE_ITERATE ( CID2_Request_Get_Blob_Id::TSources, it, request.SetSources() ) {
            SSNPDbTrackInfo track = ParseTrack(*it);
            if ( !track.m_NAIndex ) {
                continue;
            }
            CSNPDb db = GetSNPDb(GetNAAccession(track));
            if ( !db ) {
                continue;
            }
            if ( track.m_FilterIndex >= db.GetTrackCount() ) {
                // bad track index
                request.SetSources().erase(it);
                continue;
            }
            if ( !info ) {
                info = &packet_context.m_SNPRequests[main_request.GetSerial_number()];
            }
            info->m_SNPTracks.push_back(track);
            request.SetSources().erase(it);
        }
        if ( request.GetSources().empty() ) {
            // no other ID2 sources left
            request.ResetSources();
        }
        if ( info ) {
            // add accession request if it's not known
            x_AddSeqIdRequest(request.SetSeq_id(), *info);
            return eNeedReplies;
        }
    }
    return eNotProcessed;
}


BEGIN_LOCAL_NAMESPACE;

template<class Values>
bool sx_HasNonZero(const Values& values, TSeqPos index, TSeqPos count)
{
    TSeqPos end = min(index+count, TSeqPos(values.size()));
    for ( TSeqPos i = index; i < end; ++i ) {
        if ( values[i] ) {
            return true;
        }
    }
    return false;
}


template<class TValues>
void sx_AddBits2(vector<char>& bits,
                 TSeqPos bit_values,
                 TSeqPos pos_index,
                 const TValues& values)
{
    TSeqPos dst_ind = pos_index / bit_values;
    TSeqPos src_ind = 0;
    if ( TSeqPos first_offset = pos_index % bit_values ) {
        TSeqPos first_count = bit_values - first_offset;
        if ( !bits[dst_ind] ) {
            bits[dst_ind] = sx_HasNonZero(values, 0, first_count);
        }
        dst_ind += 1;
        src_ind += first_count;
    }
    while ( src_ind < values.size() ) {
        if ( !bits[dst_ind] ) {
            bits[dst_ind] = sx_HasNonZero(values, src_ind, bit_values);
        }
        ++dst_ind;
        src_ind += bit_values;
    }
}


static
void sx_AddBits(vector<char>& bits,
                TSeqPos kChunkSize,
                const CSeq_graph& graph)
{
    TSeqPos comp = graph.GetComp();
    _ASSERT(kChunkSize % comp == 0);
    TSeqPos bit_values = kChunkSize / comp;
    const CSeq_interval& loc = graph.GetLoc().GetInt();
    TSeqPos pos = loc.GetFrom();
    _ASSERT(pos % comp == 0);
    _ASSERT(graph.GetNumval()*comp == loc.GetLength());
    TSeqPos pos_index = pos/comp;
    if ( graph.GetGraph().IsByte() ) {
        auto& values = graph.GetGraph().GetByte().GetValues();
        _ASSERT(values.size() == graph.GetNumval());
        sx_AddBits2(bits, bit_values, pos_index, values);
    }
    else {
        auto& values = graph.GetGraph().GetInt().GetValues();
        _ASSERT(values.size() == graph.GetNumval());
        sx_AddBits2(bits, bit_values, pos_index, values);
    }
}


END_LOCAL_NAMESPACE;


CRef<CSerialObject> CID2SNPProcessor_Impl::x_LoadBlob(CID2SNPContext& context,
                                                      SSNPEntryInfo& info)
{
    CRef<CID2S_Split_Info> split_info(new CID2S_Split_Info);
    split_info->SetChunks();
    CBioseq_set& skeleton = split_info->SetSkeleton().SetSet();
    skeleton.SetId().SetId(kTSEId);
    skeleton.SetSeq_set();

                
    CSNPDbSeqIterator it = GetSeqIterator(info);
    CRange<TSeqPos> total_range = it.GetSNPRange();
    vector<char> feat_chunks(total_range.GetTo()/kFeatChunkSize+1);
    string na_acc = FormatTrack(info.m_Track);
    {{
        // overview graph
        CRef<CSeq_annot> annot = it.GetOverviewAnnot(total_range, na_acc);
        sx_SetZoomLevel(*annot, it.GetOverviewZoom());
        if ( annot ) {
            for ( auto& g : annot->GetData().GetGraph() ) {
                sx_AddBits(feat_chunks, kFeatChunkSize, *g);
            }
            skeleton.SetAnnot().push_back(annot);
        }
    }}
    {{
        // coverage graphs
        string graph_name = CombineWithZoomLevel(na_acc, it.GetCoverageZoom());
        _ASSERT(kGraphChunkSize % kFeatChunkSize == 0);
        const TSeqPos feat_per_graph = kGraphChunkSize/kFeatChunkSize;
        for ( int i = 0; i*kGraphChunkSize < total_range.GetToOpen(); ++i ) {
            if ( !sx_HasNonZero(feat_chunks, i*feat_per_graph, feat_per_graph) ) {
                continue;
            }
            int chunk_id = i*kChunkIdMul+kChunkIdGraph;
            CID2S_Chunk_Info& chunk = sx_AddNew(split_info->SetChunks());
            chunk.SetId().Set(chunk_id);
            CID2S_Seq_annot_Info& annot_info = sx_AddNew(chunk.SetContent()).SetSeq_annot();
            annot_info.SetName(graph_name);
            annot_info.SetGraph();
            CID2S_Seq_id_Interval& interval = annot_info.SetSeq_loc().SetSeq_id_interval();
            interval.SetSeq_id(*it.GetSeqId());
            interval.SetStart(i*kGraphChunkSize);
            interval.SetLength(kGraphChunkSize);
        }
    }}
    {{
        // features
        TSeqPos overflow = it.GetMaxSNPLength()-1;
        for ( int i = 0; i*kFeatChunkSize < total_range.GetToOpen(); ++i ) {
            if ( !feat_chunks[i] ) {
                continue;
            }
            int chunk_id = i*kChunkIdMul+kChunkIdFeat;
            CID2S_Chunk_Info& chunk = sx_AddNew(split_info->SetChunks());
            chunk.SetId().Set(chunk_id);
            CID2S_Seq_annot_Info& annot_info = sx_AddNew(chunk.SetContent()).SetSeq_annot();
            annot_info.SetName(na_acc);
            CID2S_Feat_type_Info& feat_type = sx_AddNew(annot_info.SetFeat());
            feat_type.SetType(CSeqFeatData::e_Imp);
            feat_type.SetSubtypes().push_back(CSeqFeatData::eSubtype_variation);
            CID2S_Seq_id_Interval& interval = annot_info.SetSeq_loc().SetSeq_id_interval();
            interval.SetSeq_id(*it.GetSeqId());
            interval.SetStart(i*kFeatChunkSize);
            interval.SetLength(kFeatChunkSize+overflow);
        }
    }}
    return Ref<CSerialObject>(split_info);
}


CRef<CSerialObject> CID2SNPProcessor_Impl::x_LoadChunk(CID2SNPContext& context,
                                                       SSNPEntryInfo& info,
                                                       int chunk_id)
{
    CRef<CID2S_Chunk> chunk(new CID2S_Chunk);
    CID2S_Chunk_Data& data = sx_AddNew(chunk->SetData());
    int chunk_type = chunk_id%kChunkIdMul;
    int i = chunk_id/kChunkIdMul;
    data.SetId().SetBioseq_set(kTSEId);
    
    string na_acc = FormatTrack(info.m_Track);
    CSNPDbSeqIterator& it = GetSeqIterator(info);
    if ( chunk_type == kChunkIdFeat ) {
        CRange<TSeqPos> range;
        range.SetFrom(i*kFeatChunkSize);
        range.SetToOpen((i+1)*kFeatChunkSize);
        for ( auto annot : it.GetTableFeatAnnots(range, na_acc) ) {
            data.SetAnnots().push_back(annot);
        }
    }
    else if ( chunk_type == kChunkIdGraph ) {
        CRange<TSeqPos> range;
        range.SetFrom(i*kGraphChunkSize);
        range.SetToOpen((i+1)*kGraphChunkSize);
        if ( auto annot = it.GetCoverageAnnot(range, na_acc) ) {
            sx_SetZoomLevel(*annot, it.GetCoverageZoom());
            data.SetAnnots().push_back(annot);
        }
    }
    return Ref<CSerialObject>(chunk);
}


CID2SNPProcessor_Impl::EProcessStatus
CID2SNPProcessor_Impl::x_ProcessGetBlobInfo(CID2SNPContext& context,
                                            CID2SNPProcessorPacketContext& packet_context,
                                            TReplies& replies,
                                            CID2_Request& main_request,
                                            CID2_Request_Get_Blob_Info& request)
{
    if ( !request.GetBlob_id().IsBlob_id() ) {
        return eNotProcessed;
    }
    if ( SSNPEntryInfo info = x_ResolveBlobId(request.GetBlob_id().GetBlob_id()) ) {
        CID2_Reply& main_reply = sx_AddNew(replies);
        if ( main_request.IsSetSerial_number() ) {
            main_reply.SetSerial_number(main_request.GetSerial_number());
        }
        CID2_Reply_Get_Blob& reply = main_reply.SetReply().SetGet_blob();
        reply.SetBlob_id(x_GetBlobId(info));
        CID2_Reply_Data& data = reply.SetData();

        CRef<CSerialObject> obj = x_LoadBlob(context, info);
        if ( obj->GetThisTypeInfo() == CID2S_Split_Info::GetTypeInfo() ) {
            // split info
            TRACE_X(11, eDebug_resolve, "GetSplitInfo: "<<info);
            data.SetData_type(CID2_Reply_Data::eData_type_id2s_split_info);
        }
        else {
            TRACE_X(11, eDebug_resolve, "GetSeq_entry: "<<info);
            data.SetData_type(CID2_Reply_Data::eData_type_seq_entry);
        }
        WriteData(context, info, data, *obj);
        TRACE_X(12, eDebug_resolve, "Seq("<<info<<"): "<<
                " data size: "<<sx_GetSize(data));
        main_reply.SetEnd_of_reply();
        return eProcessed;
    }
    return eNotProcessed;
}


CID2SNPProcessor_Impl::EProcessStatus
CID2SNPProcessor_Impl::x_ProcessGetChunks(CID2SNPContext& context,
                                          CID2SNPProcessorPacketContext& packet_context,
                                          TReplies& replies,
                                          CID2_Request& main_request,
                                          CID2S_Request_Get_Chunks& request)
{
    if ( SSNPEntryInfo info = x_ResolveBlobId(request.GetBlob_id()) ) {
        ITERATE ( CID2S_Request_Get_Chunks::TChunks, it, request.GetChunks() ) {
            CID2_Reply& main_reply = sx_AddNew(replies);
            if ( main_request.IsSetSerial_number() ) {
                main_reply.SetSerial_number(main_request.GetSerial_number());
            }
            CID2S_Reply_Get_Chunk& reply = main_reply.SetReply().SetGet_chunk();
            reply.SetBlob_id(request.SetBlob_id());
            reply.SetChunk_id(*it);
            CRef<CSerialObject> obj = x_LoadChunk(context, info, *it);
            if ( obj && obj->GetThisTypeInfo() == CID2S_Chunk::GetTypeInfo() ) {
                // chunk
                TRACE_X(11, eDebug_resolve, "GetChunk: "<<info<<"."<<*it);
                CID2_Reply_Data& data = reply.SetData();
                data.SetData_type(CID2_Reply_Data::eData_type_id2s_chunk);
                WriteData(context, info, data, *obj);
                TRACE_X(12, eDebug_resolve, "Seq("<<info<<"): "<<
                        " data size: "<<sx_GetSize(data));
            }
            else {
                TRACE_X(11, eDebug_resolve, "GetChunk: "<<info<<'.'<<*it<<": bad chunk");
                CID2_Error& error = sx_AddNew(main_reply.SetError());
                error.SetSeverity(CID2_Error::eSeverity_no_data);
                error.SetMessage("Invalid chunk id");
            }
        }
        replies.back()->SetEnd_of_reply();
        return eProcessed;
    }
    return eNotProcessed;
}


void CID2SNPProcessor_Impl::x_ProcessReplyGetSeqId(CID2SNPContext& context,
                                                   CID2SNPProcessorPacketContext& packet_context,
                                                   CID2_Reply& main_reply,
                                                   TReplies& replies,
                                                   CID2SNPProcessorPacketContext::SRequestInfo& info,
                                                   CID2_Reply_Get_Seq_id& reply)
{
    replies.push_back(Ref(&main_reply));
    if ( reply.IsSetSeq_id() ) {
        for ( auto& r : reply.GetSeq_id() ) {
            x_GetAccVer(info.m_SeqAcc, *r);
        }
    }
}


void CID2SNPProcessor_Impl::x_ProcessReplyGetBlobId(CID2SNPContext& context,
                                                    CID2SNPProcessorPacketContext& packet_context,
                                                    CID2_Reply& main_reply,
                                                    TReplies& replies,
                                                    CID2SNPProcessorPacketContext::SRequestInfo& req_info,
                                                    CID2_Reply_Get_Blob_Id& reply)
{
    replies.push_back(Ref(&main_reply));
    if ( !req_info.m_SentBlobIds && reply.IsSetBlob_id() && !reply.IsSetAnnot_info() ) {
        CRef<CSeq_id> seq_id(new CSeq_id(req_info.m_SeqAcc));
        for ( auto& track : req_info.m_SNPTracks ) {
            if ( SSNPEntryInfo snp_info = x_ResolveBlobId(track, req_info.m_SeqAcc) ) {
                string na_acc = FormatTrack(track);
                CID2_Reply& snp_main_reply = sx_AddNew(replies);
                snp_main_reply.SetSerial_number(main_reply.GetSerial_number());
                CID2_Reply_Get_Blob_Id& snp_reply = snp_main_reply.SetReply().SetGet_blob_id();
                snp_reply.SetSeq_id(reply.SetSeq_id());
                snp_reply.SetBlob_id(x_GetBlobId(snp_info));
                {{
                    // add SNP feat type info
                    CID2S_Seq_annot_Info& annot_info = sx_AddNew(snp_reply.SetAnnot_info());
                    annot_info.SetSeq_loc().SetWhole_seq_id(*seq_id);
                    annot_info.SetName(na_acc);
                    CID2S_Feat_type_Info& type_info = sx_AddNew(annot_info.SetFeat());
                    type_info.SetType(CSeqFeatData::e_Imp);
                    type_info.SetSubtypes().push_back(CSeqFeatData::eSubtype_variation);
                }}
                {{
                    // add SNP graph type info
                    CID2S_Seq_annot_Info& annot_info = sx_AddNew(snp_reply.SetAnnot_info());
                    annot_info.SetSeq_loc().SetWhole_seq_id(*seq_id);
                    annot_info.SetName(CombineWithZoomLevel(na_acc, GetSNPDb(snp_info).GetCoverageZoom()));
                    annot_info.SetGraph();
                }}
                {{
                    // add SNP overvew graph type info
                    CID2S_Seq_annot_Info& annot_info = sx_AddNew(snp_reply.SetAnnot_info());
                    annot_info.SetSeq_loc().SetWhole_seq_id(*seq_id);
                    annot_info.SetName(CombineWithZoomLevel(na_acc, GetSNPDb(snp_info).GetOverviewZoom()));
                    annot_info.SetGraph();
                }}
            }
        }
        if ( reply.IsSetEnd_of_reply() ) {
            reply.ResetEnd_of_reply();
            replies.back()->SetReply().SetGet_blob_id().SetEnd_of_reply();
        }
        if ( main_reply.IsSetEnd_of_reply() ) {
            main_reply.ResetEnd_of_reply();
            replies.back()->SetEnd_of_reply();
        }
        req_info.m_SentBlobIds = true;
    }
}


/////////////////////////////////////////////////////////////////////////////
// new interface


CRef<CID2SNPProcessorContext>
CID2SNPProcessor_Impl::CreateContext(void)
{
    CRef<CID2SNPProcessorContext> context(new CID2SNPProcessorContext);
    context->m_Context = m_InitialContext;
    return context;
}


CRef<CID2SNPProcessorPacketContext>
CID2SNPProcessor_Impl::ProcessPacket(CID2SNPProcessorContext* context,
                                     CID2_Request_Packet& packet,
                                     TReplies& replies)
{
    CRef<CID2SNPProcessorPacketContext> ret(new CID2SNPProcessorPacketContext);
    ERASE_ITERATE ( CID2_Request_Packet::Tdata, it, packet.Set() ) {
        // init request can come without serial number
        if ( (*it)->GetRequest().IsInit() ) {
            InitContext(context->m_Context, **it);
            continue;
        }
        if ( !context->m_Context.m_AllowVDB ) {
            continue;
        }
        if ( !(*it)->IsSetSerial_number() ) {
            // cannot process requests with no serial number
            continue;
        }
        EProcessStatus status = eNotProcessed;
        switch ( (*it)->GetRequest().Which() ) {
        case CID2_Request::TRequest::e_Get_blob_id:
            status = x_ProcessGetBlobId(context->m_Context, *ret, replies, **it,
                                        (*it)->SetRequest().SetGet_blob_id());
            break;
        case CID2_Request::TRequest::e_Get_blob_info:
            status = x_ProcessGetBlobInfo(context->m_Context, *ret, replies, **it,
                                          (*it)->SetRequest().SetGet_blob_info());
            break;
        case CID2_Request::TRequest::e_Get_chunks:
            status = x_ProcessGetChunks(context->m_Context, *ret, replies, **it,
                                        (*it)->SetRequest().SetGet_chunks());
            break;
        default:
            break;
        }
        if ( status == eProcessed ) {
            packet.Set().erase(it);
        }
    }
    if ( ret->m_SNPRequests.empty() ) {
        ret = null;
    }
    return ret;
}


void CID2SNPProcessor_Impl::ProcessReply(CID2SNPProcessorContext* context,
                                         CID2SNPProcessorPacketContext* packet_context,
                                         CID2_Reply& reply,
                                         TReplies& replies)
{
    if ( packet_context && reply.IsSetSerial_number() ) {
        auto it = packet_context->m_SNPRequests.find(reply.GetSerial_number());
        if ( it != packet_context->m_SNPRequests.end() ) {
            if ( reply.GetReply().IsGet_seq_id() ) {
                x_ProcessReplyGetSeqId(context->m_Context, *packet_context, reply, replies,
                                       it->second, reply.SetReply().SetGet_seq_id());
            }
            else if ( reply.GetReply().IsGet_blob_id() ) {
                x_ProcessReplyGetBlobId(context->m_Context, *packet_context, reply, replies,
                                        it->second, reply.SetReply().SetGet_blob_id());
            }
            else {
                replies.push_back(Ref(&reply));
            }
            return;
        }
    }
    // cannot process requests with no serial number
    replies.push_back(Ref(&reply));
}


// end of new interface
/////////////////////////////////////////////////////////////////////////////


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;
