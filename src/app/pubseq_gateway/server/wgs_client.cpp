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
 * Authors: Aleksey Grichenko, Eugene Vasilchenko
 *
 * File Description: client for loading data from WGS
 *
 */

#include <ncbi_pch.hpp>

#include "wgs_client.hpp"
#include "pubseq_gateway.hpp"
#include "pubseq_gateway_logging.hpp"
#include <objects/seqset/seqset__.hpp>
#include <objects/seqsplit/ID2S_Split_Info.hpp>
#include <sra/readers/sra/wgsresolver.hpp>
//#include "osg_getblob_base.hpp"
//#include "osg_resolve_base.hpp"
#include <objects/id2/ID2_Blob_Id.hpp>
#include <objects/id2/ID2_Blob_State.hpp>


BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(psg);
BEGIN_NAMESPACE(wgs);

USING_SCOPE(objects);

/////////////////////////////////////////////////////////////////////////////
// Processor settings
/////////////////////////////////////////////////////////////////////////////

NCBI_PARAM_DECL(bool, WGS, FILTER_ALL);
NCBI_PARAM_DEF_EX(bool, WGS, FILTER_ALL, false, eParam_NoThread, WGS_FILTER_ALL);

NCBI_PARAM_DECL(bool, WGS, SPLIT_FEATURES);
NCBI_PARAM_DEF(bool, WGS, SPLIT_FEATURES, true);

NCBI_PARAM_DECL(bool, WGS, KEEP_REPLACED);
NCBI_PARAM_DEF(bool, WGS, KEEP_REPLACED, true);

NCBI_PARAM_DECL(bool, WGS, KEEP_MIGRATED);
NCBI_PARAM_DEF(bool, WGS, KEEP_MIGRATED, false);

NCBI_PARAM_DECL(bool, WGS, KEEP_EXTERNAL);
NCBI_PARAM_DEF(bool, WGS, KEEP_EXTERNAL, true);

NCBI_PARAM_DECL(string, WGS, ADD_MASTER_DESCR);
NCBI_PARAM_DEF(string, WGS, ADD_MASTER_DESCR, "detached");

NCBI_PARAM_DECL(bool, WGS, MARK_MASTER_DESCR);
NCBI_PARAM_DEF(bool, WGS, MARK_MASTER_DESCR, false);


static inline bool s_FilterAll(void)
{
    static bool value = NCBI_PARAM_TYPE(WGS, FILTER_ALL)::GetDefault();
    return value;
}

static bool s_SplitFeatures(void)
{
    static bool value = NCBI_PARAM_TYPE(WGS, SPLIT_FEATURES)::GetDefault();
    return value;
}

static bool s_KeepReplaced(void)
{
    static bool value = NCBI_PARAM_TYPE(WGS, KEEP_REPLACED)::GetDefault();
    return value;
}

static bool s_KeepMigrated(void)
{
    static bool value = NCBI_PARAM_TYPE(WGS, KEEP_MIGRATED)::GetDefault();
    return value;
}

enum EAddMasterDescr
{
    eAddMasterDescr_none,
    eAddMasterDescr_detached,
    eAddMasterDescr_all,
};

static EAddMasterDescr s_ProcessAddMasterDescr(void)
{
    auto value = NCBI_PARAM_TYPE(WGS, ADD_MASTER_DESCR)::GetDefault();
    return (NStr::EqualNocase(value, "detached")? eAddMasterDescr_detached:
            (NStr::EqualNocase(value, "all")? eAddMasterDescr_all:
             eAddMasterDescr_none));
}

static EAddMasterDescr s_AddMasterDescrLevel(void)
{
    static EAddMasterDescr value = s_ProcessAddMasterDescr();
    return value;
}

static bool s_AddMasterDescrContig()
{
    // master descr on contig should be added only with "all" setting
    return s_AddMasterDescrLevel() == eAddMasterDescr_all;
}

static bool s_AddMasterDescrScaffold()
{
    // master descr on scaffold should be added only with any setting except "none"
    return s_AddMasterDescrLevel() != eAddMasterDescr_none;
}

static bool s_AddMasterDescrProtein()
{
    // master descr on protein should be added only with any setting except "none"
    return s_AddMasterDescrLevel() != eAddMasterDescr_none;
}

static bool s_MarkMasterDescr(void)
{
    static bool value = NCBI_PARAM_TYPE(WGS, MARK_MASTER_DESCR)::GetDefault();
    return value;
}


enum EResolveMaster {
    eResolveMaster_never,
    eResolveMaster_without_gi,
    eResolveMaster_always
};
static const EResolveMaster kResolveMaster = eResolveMaster_never;

static const char kSubSatSeparator = '/';
static const int kOSG_Sat_WGS_min = 1000;
static const int kOSG_Sat_WGS_max = 1130;
static const int kOSG_Sat_SNP_min = 2001;
static const int kOSG_Sat_SNP_max = 3999;
static const int kOSG_Sat_CDD_min = 8087;
static const int kOSG_Sat_CDD_max = 8088;

static inline bool s_IsEnabledOSGSat(CWGSClient::TEnabledFlags enabled_flags, Int4 sat)
{
    if ( sat >= kOSG_Sat_WGS_min &&
         sat <= kOSG_Sat_WGS_max &&
         (enabled_flags & CWGSClient::fEnabledWGS) ) {
        return true;
    }
    if ( sat >= kOSG_Sat_SNP_min &&
         sat <= kOSG_Sat_SNP_max &&
         (enabled_flags & CWGSClient::fEnabledSNP) ) {
        return true;
    }
    if ( sat >= kOSG_Sat_CDD_min &&
         sat <= kOSG_Sat_CDD_max &&
         (enabled_flags & CWGSClient::fEnabledCDD) ) {
        return true;
    }
    /*
    if ( sat >= kOSG_Sat_NAGraph_min &&
         sat <= kOSG_Sat_NAGraph_max &&
         (enabled_flags & CWGSClient::fEnabledNAGraph) ) {
        return true;
    }
    */
    return false;
}


static bool s_IsOSGSat(Int4 sat)
{
    return s_IsEnabledOSGSat(CWGSClient::fEnabledAll, sat);
}


static bool s_Skip(CTempString& str, char c)
{
    if ( str.empty() || str[0] != c ) {
        return false;
    }
    str = str.substr(1);
    return true;
}


static inline bool s_IsValidIntChar(char c)
{
    return c == '-' || (c >= '0' && c <= '9');
}


template<class Int>
static bool s_ParseInt(CTempString& str, Int& v)
{
    size_t int_size = 0;
    while ( int_size < str.size() && s_IsValidIntChar(str[int_size]) ) {
        ++int_size;
    }
    if ( !NStr::StringToNumeric(str.substr(0, int_size), &v,
                                NStr::fConvErr_NoThrow|NStr::fConvErr_NoErrMessage) ) {
        return false;
    }
    str = str.substr(int_size);
    return true;
}


static bool s_IsOSGBlob(Int4 sat, Int4 /*subsat*/, Int4 /*satkey*/)
{
    return s_IsOSGSat(sat);
}


static bool s_ParseOSGBlob(CTempString& s,
                           Int4& sat, Int4& subsat, Int4& satkey)
{
    if ( s.find(kSubSatSeparator) == NPOS ) {
        return false;
    }
    if ( !s_ParseInt(s, sat) ) {
        return false;
    }
    if ( !s_IsOSGSat(sat) ) {
        return false;
    }
    if ( !s_Skip(s, kSubSatSeparator) ) {
        return false;
    }
    if ( !s_ParseInt(s, subsat) ) {
        return false;
    }
    if ( !s_Skip(s, '.') ) {
        return false;
    }
    if ( !s_ParseInt(s, satkey) ) {
        return false;
    }
    return s_IsOSGBlob(sat, subsat, satkey);
}

static void s_FormatBlobId(ostream& s, const CID2_Blob_Id& blob_id)
{
    s << blob_id.GetSat()
      << kSubSatSeparator << blob_id.GetSub_sat()
      << '.' << blob_id.GetSat_key();
}


/////////////////////////////////////////////////////////////////////////////
// WGS seq-ids
/////////////////////////////////////////////////////////////////////////////


// WGS accession parameters
static const size_t kTypePrefixLen = 4; // "WGS:" or "TSA:"
static const size_t kNumLettersV1 = 4;
static const size_t kNumLettersV2 = 6;
static const size_t kVersionDigits = 2;
static const size_t kPrefixLenV1 = kNumLettersV1 + kVersionDigits;
static const size_t kPrefixLenV2 = kNumLettersV2 + kVersionDigits;
static const size_t kMinRowDigitsV1 = 6;
static const size_t kMaxRowDigitsV1 = 8;
static const size_t kMinRowDigitsV2 = 7;
static const size_t kMaxRowDigitsV2 = 9;

static const size_t kMinProtAccLen = 8; // 3+5
static const size_t kMaxProtAccLen = 10; // 3+7

static bool IsWGSGeneral(const CDbtag& dbtag)
{
    const string& db = dbtag.GetDb();
    if ( db.size() != kTypePrefixLen+kNumLettersV1 /* WGS:AAAA */ &&
         db.size() != kTypePrefixLen+kPrefixLenV1  /* WGS:AAAA01 */ &&
         db.size() != kTypePrefixLen+kNumLettersV2 /* WGS:AAAAAA */ &&
         db.size() != kTypePrefixLen+kPrefixLenV2  /* WGS:AAAAAA01 */ ) {
        return false;
    }
    if ( !NStr::StartsWith(db, "WGS:", NStr::eNocase) &&
         !NStr::StartsWith(db, "TSA:", NStr::eNocase) ) {
        return false;
    }
    return true;
}


enum EAlligSeqType {
    fAllow_contig   = 1,
    fAllow_scaffold = 2,
    fAllow_protein  = 4
};
typedef int TAllowSeqType;

static bool IsWGSAccession(const string& acc,
                           const CTextseq_id& id,
                           TAllowSeqType allow_seq_type)
{
    if ( acc.size() < kPrefixLenV1 + kMinRowDigitsV1 ||
         acc.size() > kPrefixLenV2 + kMaxRowDigitsV2 + 1 ) { // one for type letter
        return false;
    }
    size_t num_letters;
    for ( num_letters = 0; num_letters < kNumLettersV2; ++num_letters ) {
        if ( !isalpha(acc[num_letters]&0xff) ) {
            break;
        }
    }
    if ( num_letters != kNumLettersV1 && num_letters != kNumLettersV2 ) {
        return false;
    }
    size_t prefix_len = num_letters + kVersionDigits;
    for ( size_t i = num_letters; i < prefix_len; ++i ) {
        if ( !isdigit(acc[i]&0xff) ) {
            return false;
        }
    }
    SIZE_TYPE row_pos = prefix_len;
    switch ( acc[row_pos] ) { // optional type letter
    case 's':
    case 'S':
        // scaffold
        if ( !(allow_seq_type & fAllow_scaffold) ) {
            return false;
        }
        ++row_pos;
        break;
    case 'p':
    case 'P':
        // protein
        if ( !(allow_seq_type & fAllow_protein) ) {
            return false;
        }
        ++row_pos;
        break;
    default:
        // contig
        if ( !(allow_seq_type & fAllow_contig) ) {
            return false;
        }
        break;
    }
    size_t row_digits = acc.size() - row_pos;
    if ( num_letters == kNumLettersV1 ) {
        if ( row_digits < kMinRowDigitsV1 || row_digits > kMaxRowDigitsV1 ) {
            return false;
        }
    }
    else {
        if ( row_digits < kMinRowDigitsV2 || row_digits > kMaxRowDigitsV2 ) {
            return false;
        }
    }
    Uint8 row = 0;
    for ( size_t i = row_pos; i < acc.size(); ++i ) {
        char c = acc[i];
        if ( c < '0' || c > '9' ) {
            return false;
        }
        row = row*10+(c-'0');
    }
    if ( !row ) {
        return false;
    }
    return true;
}


static bool IsWGSProtAccession(const CTextseq_id& id)
{
    const string& acc = id.GetAccession();
    if ( acc.size() < kMinProtAccLen || acc.size() > kMaxProtAccLen ) {
        return false;
    }
    return true;
}


static bool IsWGSAccession(const CTextseq_id& id)
{
    if ( id.IsSetName() ) {
        // first try name reference if it has WGS format like AAAA01P000001
        // as it directly contains WGS accession
        return IsWGSAccession(id.GetName(), id, fAllow_protein);
    }
    if ( !id.IsSetAccession() ) {
        return false;
    }
    const string& acc = id.GetAccession();
    CSeq_id::EAccessionInfo type = CSeq_id::IdentifyAccession(acc);
    switch ( type & CSeq_id::eAcc_division_mask ) {
        // accepted accession types
    case CSeq_id::eAcc_wgs:
    case CSeq_id::eAcc_wgs_intermed:
    case CSeq_id::eAcc_tsa:
    case CSeq_id::eAcc_targeted:
        if ( type & CSeq_id::fAcc_prot ) {
            return IsWGSProtAccession(id);
        }
        else {
            return IsWGSAccession(acc, id, fAllow_contig|fAllow_scaffold);
        }
    case CSeq_id::eAcc_other:
        // Some EMBL WGS accession aren't identified as WGS, so we'll try lookup anyway
        if ( type == CSeq_id::eAcc_embl_prot ||
             (type == CSeq_id::eAcc_gb_prot && acc.size() == 10) ) { // TODO: remove
            return IsWGSProtAccession(id);
        }
        return false;
    default:
        // non-WGS accessions
        return false;
    }
}



/////////////////////////////////////////////////////////////////////////////
// WGS blob-ids
/////////////////////////////////////////////////////////////////////////////

// satkey: row-id
static const int kBlobIdV1Sat = 1000;
static const int kBlobIdV2SatMin = 1001;
static const int kBlobIdV2SatMax = 1129;
static const int kBlobIdV2VersionScaffold = 0;
static const int kBlobIdV2VersionProtein  = 1;
static const int kBlobIdV2VersionContig   = 2;
enum EBlobType {
    eBlobType_contig = 1,
    eBlobType_scaffold = 2,
    eBlobType_protein = 3
};
enum EBlobIdBits {
    eBlobIdBits_type = 2,
    eBlobIdBits_letter = 5,
    eBlobIdBits_digit = 4
};


/////////////////////////////////////////////////////////////////////////////
// Helper classes
/////////////////////////////////////////////////////////////////////////////

BEGIN_LOCAL_NAMESPACE;

class CIndexUpdateThread : public CThreadNonStop
{
public:
    CIndexUpdateThread(unsigned update_delay, CRef<CWGSResolver> resolver)
        : CThreadNonStop(update_delay),
          m_FirstRun(true),
          m_Resolver(resolver)
        {
        }

protected:
    virtual void DoJob(void) {
        if ( m_FirstRun ) {
            // CThreadNonStop runs first iteration immediately, ignore it
            m_FirstRun = false;
            return;
        }
        try {
            if ( m_Resolver->Update() ) {
                PSG_INFO("PSGS_WGS: updated WGS index");
            }
        }
        catch ( CException& exc ) {
            PSG_ERROR("PSGS_WGS: Exception while updating WGS index: " << exc);
        }
        catch ( exception& exc ) {
            PSG_ERROR("PSGS_WGS: Exception while updating WGS index: " << exc.what());
        }
    }

private:
    bool m_FirstRun;
    CRef<CWGSResolver> m_Resolver;
};

END_LOCAL_NAMESPACE;


/////////////////////////////////////////////////////////////////////////////
// CWGSClient
/////////////////////////////////////////////////////////////////////////////


CWGSClient::CWGSClient(const SWGSProcessor_Config& config)
    : m_Config(config),
      m_WGSDbCache(config.m_CacheSize, config.m_FileReopenTime, config.m_FileRecheckTime)
{
}


CWGSClient::~CWGSClient(void)
{
    if ( m_IndexUpdateThread ) {
        m_IndexUpdateThread->RequestStop();
        m_IndexUpdateThread->Join();
    }
}


CRef<CWGSResolver> CWGSClient::GetWGSResolver(void)
{
    if ( !m_Resolver ) {
        CFastMutexGuard guard(m_ResolverMutex);
        if ( !m_Resolver ) {
            m_Resolver = CWGSResolver::CreateResolver(m_Mgr);
        }
        if ( m_Resolver && !m_IndexUpdateThread ) {
            m_IndexUpdateThread = new CIndexUpdateThread(m_Config.m_IndexUpdateDelay, m_Resolver);
            m_IndexUpdateThread->Run();
        }
    }
    return m_Resolver;
}


bool CWGSClient::CanProcessRequest(CPSGS_Request& request)
{
    auto req_type = request.GetRequestType();
    string seq_id;
    int seq_id_type = -1;
    CRef<CID2_Blob_Id> blob_id;

    switch ( req_type ) {
    case CPSGS_Request::ePSGS_ResolveRequest:
    {
        auto& resolve_request = request.GetRequest<SPSGS_ResolveRequest>();
        seq_id = resolve_request.m_SeqId;
        seq_id_type = resolve_request.m_SeqIdType;
        break;
    }
    case CPSGS_Request::ePSGS_BlobBySeqIdRequest:
    {
        auto& blob_sid_request = request.GetRequest<SPSGS_BlobBySeqIdRequest>();
        seq_id = blob_sid_request.m_SeqId;
        seq_id_type = blob_sid_request.m_SeqIdType;
        break;
    }
    case CPSGS_Request::ePSGS_BlobBySatSatKeyRequest:
        blob_id = ParsePSGBlobId(request.GetRequest<SPSGS_BlobBySatSatKeyRequest>().m_BlobId);
        break;
    case CPSGS_Request::ePSGS_TSEChunkRequest:
    {
        auto& chunk_request = request.GetRequest<SPSGS_TSEChunkRequest>();
        blob_id = ParsePSGId2Info(chunk_request.m_Id2Info).tse_id;
        break;
    }
    default:
        return false;
    }

    if ( !seq_id.empty() ) {
        return CanBeWGS(seq_id_type, seq_id);
    }
    if ( blob_id  ) {
        return ResolveBlobId(*blob_id, true).m_ValidWGS;
    }
    return false;
}


shared_ptr<SWGSData> CWGSClient::ResolveSeqId(const CSeq_id& seq_id)
{
    shared_ptr<SWGSData> ret;
    SWGSSeqInfo seq = Resolve(seq_id);
    if (seq  &&  HasMigrated(seq)  &&  !s_KeepMigrated() ) {
        seq = SWGSSeqInfo();
    }
    if ( !seq ) return ret;

    GetBioseqInfo(ret, seq);
    _ASSERT(ret);
    return ret;
}


shared_ptr<SWGSData> CWGSClient::GetSeqInfoBySeqId(const CSeq_id& seq_id,
    SWGSSeqInfo& seq,
    const TBlobIds& excluded)
{
    shared_ptr<SWGSData> ret;
    seq = Resolve(seq_id);
    if (seq  &&  HasMigrated(seq)  &&  !s_KeepMigrated() ) {
        seq = SWGSSeqInfo();
    }
    if ( !seq ) return ret;

    GetBioseqInfo(ret, seq);
    _ASSERT(ret);
    if ( find(excluded.begin(), excluded.end(), ret->m_BlobId) != excluded.end() ) {
        ret->m_GetResult = SWGSData::eResult_Excluded;
    }

    return ret;
}


shared_ptr<SWGSData> CWGSClient::GetBlobByBlobId(const string& blob_id)
{
    shared_ptr<SWGSData> ret;
    CRef<CID2_Blob_Id> id2_blob_id(ParsePSGBlobId(blob_id));
    if ( !id2_blob_id ) return ret;

    SWGSSeqInfo seq = ResolveBlobId(*id2_blob_id);
    if ( !seq ) return ret;

    GetWGSData(ret, seq);
    return ret;
}


shared_ptr<SWGSData> CWGSClient::GetChunk(const string& id2info, int64_t chunk_id)
{
    shared_ptr<SWGSData> ret;
    SParsedId2Info parsed_id2info = ParsePSGId2Info(id2info);
    if ( !parsed_id2info.tse_id ) return ret;

    SWGSSeqInfo seq0 = ResolveBlobId(*parsed_id2info.tse_id);
    if ( !seq0 ) return ret;

    auto id2_blob_state = GetID2BlobState(seq0);
    if ( SWGSData::IsForbidden(id2_blob_state) ) {
        ret = make_shared<SWGSData>();
        ret->m_GetResult = SWGSData::eResult_Found;
        ret->m_Id2BlobId.Reset(&GetBlobId(seq0));
        ret->m_BlobId = GetPSGBlobId(*ret->m_Id2BlobId);
        ret->m_Id2BlobState = id2_blob_state;
        return ret;
    }

    SWGSSeqInfo& seq = GetRootSeq(seq0);
    if ( seq.IsContig() ) {
        CWGSSeqIterator& it = GetContigIterator(seq);
        // master descr shouldn't be added to proteins in chunks
        //CWGSSeqIterator::TFlags flags = it.fDefaultFlags & ~it.fMasterDescr;
        ret = make_shared<SWGSData>();
        ret->m_GetResult = SWGSData::eResult_Found;
        ret->m_Id2BlobId.Reset(&GetBlobId(seq0));
        ret->m_BlobId = GetPSGBlobId(*ret->m_Id2BlobId);
        ret->m_SplitVersion = parsed_id2info.split_version;
        ret->m_Id2BlobState = id2_blob_state;
        ret->m_Data = it.GetChunkDataForVersion(chunk_id, parsed_id2info.split_version);
        if ( !ret->m_Data ) {
            ret->m_Data = new CAsnBinData(*it.GetChunkForVersion(chunk_id, parsed_id2info.split_version));
        }
        ret->m_Compress = GetCompress(m_Config.m_CompressData, seq, *ret->m_Data);
    }
    return ret;
}


void CWGSClient::x_RegisterTiming(psg_time_point_t start,
                                  EPSGOperation operation,
                                  EPSGOperationStatus status)
{
    CPubseqGatewayApp::GetInstance()->
        GetTiming().Register(nullptr, operation, status, start, 0);
}


CWGSDb CWGSClient::GetWGSDb(const string& prefix)
{
    CWGSDb wgs_db;
    {{
        CRef<CWGSDbInfo> delete_info; // delete stale file info after releasing mutex
        auto slot = m_WGSDbCache.GetSlot(prefix);
        TWGSDbCache::CSlot::TSlotMutex::TWriteLockGuard guard(slot->GetSlotMutex());
        CRef<CWGSDbInfo> info = slot->GetObject<CWGSDbInfo>();
        if ( info && slot->IsExpired(m_WGSDbCache, prefix) ) {
            PSG_INFO("PSGS_WGS: GetWGSDb: opened " << prefix << " has expired");
            slot->ResetObject();
            delete_info.Swap(info);
        }
        if ( !info ) {
            slot->UpdateExpiration(m_WGSDbCache, prefix);
            try {
                psg_time_point_t start = psg_clock_t::now();
                wgs_db = CWGSDb(m_Mgr, prefix);
                if ( s_AddMasterDescrLevel() != eAddMasterDescr_none ) {
                    wgs_db.LoadMasterDescr();
                }
                x_RegisterTiming(start, eVDBOpen, eOpStatusFound);
            }
            catch ( CSraException& exc ) {
                if ( exc.GetErrCode() == exc.eNotFoundDb ||
                     exc.GetErrCode() == exc.eProtectedDb ) {
                    // no such WGS table
                }
                else {
                    // problem in VDB or WGS reader
                    PSG_ERROR("PSGS_WGS: Exception while opening WGS DB " << prefix << ": " << exc);
                    throw;
                }
                return CWGSDb();
            }
            catch ( CException& exc ) {
                // problem in VDB or WGS reader
                PSG_ERROR("PSGS_WGS: Exception while opening WGS DB " << prefix << ": " << exc);
                throw;
            }
            catch ( exception& exc ) {
                // problem in VDB or WGS reader
                PSG_ERROR("PSGS_WGS: Exception while opening WGS DB " << prefix << ": " << exc.what());
                throw;
            }
            info = new CWGSDbInfo;
            info->m_WGSDb = wgs_db;
            slot->SetObject(info);
        }
        wgs_db = info->m_WGSDb;
    }}
    if ( wgs_db->IsReplaced() && !s_KeepReplaced() ) {
        // replaced
        PSG_INFO("PSGS_WGS: GetWGSDb: " << prefix << " is replaced");
        return CWGSDb();
    }
    else {
        // found
        PSG_INFO("PSGS_WGS: GetWGSDb: " << prefix);
        return wgs_db;
    }
}


CWGSDb& CWGSClient::GetWGSDb(SWGSSeqInfo& seq)
{
    if ( !seq.m_WGSDb ) {
        seq.m_WGSDb = GetWGSDb(seq.m_WGSAcc);
        if ( seq.m_WGSDb ) {
            seq.m_IsWGS = true;
            seq.m_RowDigits = Uint1(seq.m_WGSDb->GetIdRowDigits());
        }
    }
    return seq.m_WGSDb;
}


void CWGSClient::ResetIteratorCache(SWGSSeqInfo& seq)
{
    seq.m_ContigIter.Reset();
    seq.m_ScaffoldIter.Reset();
    seq.m_ProteinIter.Reset();
    seq.m_BlobId.Reset();
}


CWGSSeqIterator& CWGSClient::GetContigIterator(SWGSSeqInfo& seq)
{
    if ( !seq.m_ContigIter ) {
        seq.m_ContigIter = CWGSSeqIterator(GetWGSDb(seq), seq.m_RowId,
                                           CWGSSeqIterator::fIncludeAll);
        seq.m_ContigIter.SelectAccVersion(seq.m_Version);
    }
    return seq.m_ContigIter;
}


CWGSScaffoldIterator& CWGSClient::GetScaffoldIterator(SWGSSeqInfo& seq)
{
    if ( !seq.m_ScaffoldIter ) {
        seq.m_ScaffoldIter = CWGSScaffoldIterator(GetWGSDb(seq), seq.m_RowId);
    }
    return seq.m_ScaffoldIter;
}


CWGSProteinIterator& CWGSClient::GetProteinIterator(SWGSSeqInfo& seq)
{
    if ( !seq.m_ProteinIter ) {
        seq.m_ProteinIter = CWGSProteinIterator(GetWGSDb(seq), seq.m_RowId);
    }
    return seq.m_ProteinIter;
}


CWGSClient::SWGSSeqInfo&
CWGSClient::GetRootSeq(SWGSSeqInfo& seq0)
{
    if ( seq0.m_RootSeq.get() ) {
        return *seq0.m_RootSeq;
    }
    if ( seq0.m_NoRootSeq ) {
        return seq0;
    }
    if ( !seq0.IsProtein() ) {
        seq0.m_NoRootSeq = true;
        return seq0;
    }
    // proteins can be located in nuc-prot set
    TVDBRowId cds_row_id = GetProteinIterator(seq0).GetBestProductFeatRowId();
    if ( !cds_row_id ) {
        seq0.m_NoRootSeq = true;
        return seq0;
    }
    CWGSFeatureIterator cds_it(GetWGSDb(seq0), cds_row_id);
    if ( !cds_it ) {
        seq0.m_NoRootSeq = true;
        return seq0;
    }
    switch ( cds_it.GetLocSeqType() ) {
    case NCBI_WGS_seqtype_contig:
    {
        // switch to contig
        seq0.m_RootSeq.reset(new SWGSSeqInfo(seq0));
        SWGSSeqInfo& seq = *seq0.m_RootSeq;
        seq.SetContig();
        seq.m_RowId = cds_it.GetLocRowId();
        ResetIteratorCache(seq);
        return seq;
    }
    case NCBI_WGS_seqtype_scaffold:
    {
        // switch to scaffold
        seq0.m_RootSeq.reset(new SWGSSeqInfo(seq0));
        SWGSSeqInfo& seq = *seq0.m_RootSeq;
        seq.SetScaffold();
        seq.m_RowId = cds_it.GetLocRowId();
        ResetIteratorCache(seq);
        return seq;
    }
    default:
        seq0.m_NoRootSeq = true;
        return seq0;
    }
}


bool CWGSClient::IsValidRowId(SWGSSeqInfo& seq)
{
    if ( seq.IsContig() ) {
        return GetContigIterator(seq);
    }
    if ( seq.IsScaffold() ) {
        return GetScaffoldIterator(seq);
    }
    if ( seq.IsProtein() ) {
        return GetProteinIterator(seq);
    }
    // master
    return true;
}


bool CWGSClient::IsCorrectVersion(SWGSSeqInfo& seq, int version)
{
    if ( !seq ) {
        return false;
    }
    if ( seq.IsContig() ) {
        CWGSSeqIterator& it = GetContigIterator(seq);
        return it && it.HasAccVersion(version);
    }
    else if ( seq.IsProtein() ) {
        CWGSProteinIterator& it = GetProteinIterator(seq);
        return it && it.GetAccVersion() == version;
    }
    else if ( seq.IsMaster() ) {
        // master version is already checked
        return true;
    }
    else {
        // scaffolds can have only version 1
        return version == 1;
    }
}


bool CWGSClient::HasSpecialState(SWGSSeqInfo& seq, NCBI_gb_state special_state)
{
    if ( !seq.IsProtein() ) {
        return false;
    }
    CWGSProteinIterator it = GetProteinIterator(seq);
    if ( !it.HasGi() ) {
        return false;
    }
    const auto project_state = seq.m_WGSDb->GetProjectGBState();
    switch (project_state) {
    case NCBI_gb_state_eWGSGenBankLive:
    case NCBI_gb_state_eWGSGenBankUnverified:
        return it.GetGBState() == special_state;
    default:
        return project_state == special_state;
    }
}


bool CWGSClient::HasMigrated(SWGSSeqInfo& seq)
{
    if ( !seq.IsProtein() ) {
        return false;
    }
    CWGSProteinIterator it = GetProteinIterator(seq);
    if ( !it.HasGi() ) {
        return false;
    }
    const auto project_state = seq.m_WGSDb->GetProjectGBState();
    switch (project_state) {
    case NCBI_gb_state_eWGSGenBankReplaced:
    case NCBI_gb_state_eWGSGenBankSuppressed:
        return it.GetGBState() == NCBI_gb_state_eWGSGenBankMigrated;
    default:
        return false;
    }
}


CWGSClient::SWGSSeqInfo
CWGSClient::Resolve(const CSeq_id& id, bool skip_lookup)
{
    switch ( id.Which() ) {
    case CSeq_id::e_Gi:
        return ResolveGi(id.GetGi(), skip_lookup);
    case CSeq_id::e_General:
        return ResolveGeneral(id.GetGeneral(), skip_lookup);
    case CSeq_id::e_not_set:
    case CSeq_id::e_Local:
    case CSeq_id::e_Gibbsq:
    case CSeq_id::e_Gibbmt:
    case CSeq_id::e_Giim:
    case CSeq_id::e_Patent:
    case CSeq_id::e_Pdb:
        return SWGSSeqInfo();
    default:
        break;
    }
    const CTextseq_id* text_id = id.GetTextseq_Id();
    if ( !text_id ) {
        return SWGSSeqInfo();
    }
    SWGSSeqInfo seq = ResolveAcc(*text_id, skip_lookup);
    if ( !seq ) {
        return seq;
    }
    if ( text_id->IsSetVersion() ) {
        int version = text_id->GetVersion();
        if ( !IsCorrectVersion(seq, version) ) {
            seq.m_ValidWGS = false;
            return seq;
        }
        if ( seq.IsContig() ) {
            GetContigIterator(seq).SelectAccVersion(version);
            seq.m_Version = version;
        }
    }
    seq.m_ValidWGS = true;
    return seq;
}


CWGSClient::SWGSSeqInfo
CWGSClient::ResolveGeneral(const CDbtag& dbtag, bool skip_lookup)
{
    const CObject_id& object_id = dbtag.GetTag();
    const string& db = dbtag.GetDb();
    if ( db.size() != kTypePrefixLen+kNumLettersV1 /* WGS:AAAA */ &&
         db.size() != kTypePrefixLen+kPrefixLenV1  /* WGS:AAAA01 */ &&
         db.size() != kTypePrefixLen+kNumLettersV2 /* WGS:AAAAAA */ &&
         db.size() != kTypePrefixLen+kPrefixLenV2  /* WGS:AAAAAA01 */ ) {
        return SWGSSeqInfo();
    }
    bool is_tsa = false;
    if ( NStr::StartsWith(db, "WGS:", NStr::eNocase) ) {
    }
    else if ( NStr::StartsWith(db, "TSA:", NStr::eNocase) ) {
        is_tsa = true;
    }
    else {
        return SWGSSeqInfo();
    }
    string wgs_acc = db.substr(kTypePrefixLen); // remove "WGS:" or "TSA:"

    NStr::ToUpper(wgs_acc);
    if ( isalpha(wgs_acc.back()&0xff) ) {
        wgs_acc += "01"; // add default version digits
    }
    SWGSSeqInfo seq;
    seq.m_WGSAcc = wgs_acc;
    seq.m_IsWGS = true;
    if (skip_lookup) {
        seq.m_ValidWGS = true;
        return seq;
    }
    CWGSDb wgs_db = GetWGSDb(seq);
    if ( !wgs_db || wgs_db->IsTSA() != is_tsa ) {
        // TSA or WGS type must match
        return seq;
    }
    string tag;
    if ( object_id.IsStr() ) {
        tag = object_id.GetStr();
        NStr::ToUpper(tag);
    }
    else {
        tag = NStr::NumericToString(object_id.GetId());
    }
    if ( TVDBRowId row = wgs_db.GetContigNameRowId(tag) ) {
        seq.m_ValidWGS = true;
        seq.SetContig();
        seq.m_RowId = row;
    }
    if ( TVDBRowId row = wgs_db.GetScaffoldNameRowId(tag) ) {
        seq.m_ValidWGS = true;
        seq.SetScaffold();
        seq.m_RowId = row;
    }
    if ( TVDBRowId row = wgs_db.GetProteinNameRowId(tag) ) {
        seq.m_ValidWGS = true;
        seq.SetProtein();
        seq.m_RowId = row;
    }
    return seq;
}


CWGSClient::SWGSSeqInfo
CWGSClient::ResolveGi(TGi gi, bool skip_lookup)
{
    CRef<CWGSResolver> wgs_resolver = GetWGSResolver();
    psg_time_point_t start = psg_clock_t::now();
    CWGSResolver::TWGSPrefixes prefixes = wgs_resolver->GetPrefixes(gi);
    x_RegisterTiming(start, eWGS_VDBLookup,
                     prefixes.empty()? eOpStatusNotFound: eOpStatusFound);
    ITERATE ( CWGSResolver::TWGSPrefixes, it, prefixes ) {
        if (skip_lookup) {
            SWGSSeqInfo fake_info;
            fake_info.m_IsWGS = fake_info.m_ValidWGS = true;
            return fake_info;
        } else if ( CWGSDb wgs_db = GetWGSDb(*it) ) {
            if ( kResolveMaster == eResolveMaster_always &&
                 gi == wgs_db->GetMasterGi() ) {
                // resolve master sequence with GI from VDB
                wgs_resolver->SetWGSPrefix(gi, prefixes, *it);
                SWGSSeqInfo seq;
                seq.m_WGSAcc = *it;
                seq.m_IsWGS = true;
                seq.m_ValidWGS = true;
                seq.m_WGSDb = wgs_db;
                seq.m_RowDigits = Uint1(wgs_db->GetIdRowDigits());
                seq.SetMaster();
                return seq;
            }
            CWGSGiIterator gi_it(wgs_db, gi);
            if ( gi_it ) {
                wgs_resolver->SetWGSPrefix(gi, prefixes, *it);
                SWGSSeqInfo seq;
                seq.m_WGSAcc = *it;
                seq.m_IsWGS = true;
                seq.m_ValidWGS = true;
                seq.m_WGSDb = wgs_db;
                seq.m_RowDigits = Uint1(wgs_db->GetIdRowDigits());
                seq.m_RowId = gi_it.GetRowId();
                if ( gi_it.GetSeqType() == gi_it.eProt ) {
                    seq.SetProtein();
                    if ( !GetProteinIterator(seq) ) {
                        return SWGSSeqInfo();
                    }
                }
                else {
                    seq.SetContig();
                    if ( !GetContigIterator(seq) ) {
                        return SWGSSeqInfo();
                    }
                }
                return seq;
            }
        }
    }
    if ( !prefixes.empty() ) {
        wgs_resolver->SetNonWGS(gi, prefixes);
    }
    return SWGSSeqInfo();
}


CWGSClient::SWGSSeqInfo
CWGSClient::ResolveAcc(const CTextseq_id& id, bool skip_lookup)
{
    if ( id.IsSetName() ) {
        // first try name reference if it has WGS format like AAAA01P000001
        // as it directly contains WGS accession
        if ( SWGSSeqInfo seq = ResolveWGSAcc(id.GetName(), id, fAllow_aa,
                                             skip_lookup) ) {
            _ASSERT(seq.IsProtein());
            if ( !id.IsSetAccession() ||
                 NStr::EqualNocase(GetProteinIterator(seq).GetAccession(),
                                   id.GetAccession()) ) {
                return seq;
            }
        }
    }
    if ( !id.IsSetAccession() ) {
        return SWGSSeqInfo();
    }
    const string& acc = id.GetAccession();
    CSeq_id::EAccessionInfo type = CSeq_id::IdentifyAccession(acc);
    switch ( type & CSeq_id::eAcc_division_mask ) {
        // accepted accession types
    case CSeq_id::eAcc_wgs:
    case CSeq_id::eAcc_wgs_intermed:
    case CSeq_id::eAcc_tsa:
    case CSeq_id::eAcc_targeted:
        if ( type & CSeq_id::fAcc_prot ) {
            return ResolveProtAcc(id, skip_lookup);
        }
        else {
            return ResolveWGSAcc(acc, id, fAllow_master|fAllow_na,
                                 skip_lookup);
        }
    case CSeq_id::eAcc_other:
        // Some EMBL WGS accession aren't identified as WGS, so we'll try lookup anyway
        if ( type == CSeq_id::eAcc_embl_prot ||
             (type == CSeq_id::eAcc_gb_prot && acc.size() == 10) ) { // TODO: remove
            return ResolveProtAcc(id, skip_lookup);
        }
        return SWGSSeqInfo();
    default:
        // non-WGS accessions
        return SWGSSeqInfo();
    }
}


CWGSClient::SWGSSeqInfo
CWGSClient::ResolveProtAcc(const CTextseq_id& id, bool skip_lookup)
{
    const string& acc = id.GetAccession();
    if ( acc.size() < kMinProtAccLen || acc.size() > kMaxProtAccLen ) {
        return SWGSSeqInfo();
    }
    int ask_version = id.IsSetVersion()? id.GetVersion(): -1;
    
    CRef<CWGSResolver> wgs_resolver = GetWGSResolver();
    psg_time_point_t start = psg_clock_t::now();
    CWGSResolver::TWGSPrefixes prefixes = wgs_resolver->GetPrefixes(acc);
    x_RegisterTiming(start, eWGS_VDBLookup,
                     prefixes.empty()? eOpStatusNotFound: eOpStatusFound);
    ITERATE ( CWGSResolver::TWGSPrefixes, it, prefixes ) {
        if (skip_lookup) {
            SWGSSeqInfo fake_info;
            fake_info.m_IsWGS = fake_info.m_ValidWGS = true;
            return fake_info;
        } else if ( CWGSDb wgs_db = GetWGSDb(*it) ) {
            if ( TVDBRowId row = wgs_db.GetProtAccRowId(acc, ask_version) ) {
                wgs_resolver->SetWGSPrefix(acc, prefixes, *it);
                SWGSSeqInfo seq;
                seq.m_WGSAcc = *it;
                seq.m_IsWGS = true;
                seq.m_ValidWGS = true;
                seq.m_WGSDb = wgs_db;
                seq.SetProtein();
                seq.m_RowDigits = Uint1(wgs_db->GetIdRowDigits());
                seq.m_RowId = row;
                return seq;
            }
        }
    }
    if ( !prefixes.empty() ) {
        wgs_resolver->SetNonWGS(acc, prefixes);
    }
    return SWGSSeqInfo();
}


CWGSClient::SWGSSeqInfo
CWGSClient::ResolveWGSAcc(const string& acc,
                          const CTextseq_id& id,
                          TAllowSeqType allow_seq_type,
                          bool skip_lookup)
{
    if ( acc.size() < kPrefixLenV1 + kMinRowDigitsV1 ||
         acc.size() > kPrefixLenV2 + kMaxRowDigitsV2 + 1 ) { // one for type letter
        return SWGSSeqInfo();
    }
    size_t num_letters;
    for ( num_letters = 0; num_letters < kNumLettersV2; ++num_letters ) {
        if ( !isalpha(acc[num_letters]&0xff) ) {
            break;
        }
    }
    if ( num_letters != kNumLettersV1 && num_letters != kNumLettersV2 ) {
        return SWGSSeqInfo();
    }
    size_t prefix_len = num_letters + kVersionDigits;
    for ( size_t i = num_letters; i < prefix_len; ++i ) {
        if ( !isdigit(acc[i]&0xff) ) {
            return SWGSSeqInfo();
        }
    }
    SWGSSeqInfo seq;
    seq.m_WGSAcc = acc.substr(0, prefix_len);
    NStr::ToUpper(seq.m_WGSAcc);
    seq.m_IsWGS = true;
    SIZE_TYPE row_pos = prefix_len;
    switch ( acc[row_pos] ) { // optional type letter
    case 'S':
        seq.SetScaffold();
        if ( !(allow_seq_type & fAllow_scaffold) ) {
            return seq;
        }
        ++row_pos;
        break;
    case 'P':
        seq.SetProtein();
        if ( !(allow_seq_type & fAllow_protein) ) {
            return seq;
        }
        ++row_pos;
        break;
    default:
        // it can be either contig or master sequence
        if ( !(allow_seq_type & (fAllow_master|fAllow_contig)) ) {
            return seq;
        }
        break;
    }
    size_t row_digits = acc.size() - row_pos;
    if ( num_letters == kNumLettersV1 ) {
        if ( row_digits < kMinRowDigitsV1 || row_digits > kMaxRowDigitsV1 ) {
            return SWGSSeqInfo();
        }
    }
    else {
        if ( row_digits < kMinRowDigitsV2 || row_digits > kMaxRowDigitsV2 ) {
            return SWGSSeqInfo();
        }
    }
    Uint8 row = 0;
    for ( size_t i = row_pos; i < acc.size(); ++i ) {
        char c = acc[i];
        if ( c < '0' || c > '9' ) {
            return SWGSSeqInfo();
        }
        row = row*10+(c-'0');
    }
    seq.m_RowId = row;
    if ( !row ) {
        // zero row might be master WGS sequence
        // it mustn't have type letter, version digits and row must be zero
        // version must be positive
        if ( !seq.IsMaster() ) {
            return SWGSSeqInfo();
        }
        if ( !(allow_seq_type & fAllow_master) ) {
            return seq;
        }
        // now, move version into version digits of the accession
        int version = id.IsSetVersion()? id.GetVersion(): 1;
        if ( version <= 0 ) {
            return SWGSSeqInfo();
        }
        for ( size_t i = kVersionDigits; i--; version /= 10) {
            if ( acc[num_letters+i] != '0' ) {
                return SWGSSeqInfo();
            }
            seq.m_WGSAcc[num_letters+i] = char('0'+version%10);
        }
        if ( version ) {
            // doesn't fit
            return SWGSSeqInfo();
        }
    }
    else if ( seq.IsContig() ) {
        if ( !(allow_seq_type & fAllow_contig) ) {
            return seq;
        }
    }
    if (skip_lookup) {
        seq.m_ValidWGS = true;
        return seq;
    }
    if ( !GetWGSDb(seq) ) {
        return seq;
    }
    if ( seq.m_WGSDb->GetIdRowDigits() != row_digits ) {
        return seq;
    }
    if ( !row ) {
        if ( kResolveMaster == eResolveMaster_never ) {
            // no master resolution
            seq.m_IsWGS = false;
            return seq;
        }
        else if ( kResolveMaster == eResolveMaster_without_gi ) {
            // only master sequences w/o GI are resolved
            if ( GetWGSDb(seq)->GetMasterGi() != ZERO_GI ) {
                // Let master sequences with GI to be processed by ID
                seq.m_IsWGS = false;
                return seq;
            }
        }
    }
    else if ( !IsValidRowId(seq) ) {
        return seq;
    }
    seq.m_ValidWGS = true;
    return seq;
}


CWGSClient::SWGSSeqInfo
CWGSClient::ResolveBlobId(const CID2_Blob_Id& id, bool skip_lookup)
{
    SWGSSeqInfo seq;
    CID2_Blob_Id::TSat sat = id.GetSat();
    if ( sat == kBlobIdV1Sat ) {
        // old 4-letter WGS accession format
        seq.m_IsWGS = true;
        unsigned subsat = unsigned(id.GetSub_sat());
        if ( unsigned seq_type = (subsat & ((1 << eBlobIdBits_type)-1)) ) {
            bool bad = false;
            // old blob-id subsat format
            switch ( seq_type ) {
            case eBlobType_contig: seq.SetContig(); break;
            case eBlobType_scaffold: seq.SetScaffold(); break;
            case eBlobType_protein: seq.SetProtein(); break;
            }
            int bit = eBlobIdBits_type;
            for ( size_t i = 0; i < kPrefixLenV1; ++i ) {
                if ( i < kNumLettersV1 ) {
                    int v = (subsat >> bit)&((1 << eBlobIdBits_letter)-1);
                    if ( v < 26 ) {
                        seq.m_WGSAcc += char('A'+v);
                    }
                    else {
                        bad = true;
                        break;
                    }
                    bit += eBlobIdBits_letter;
                }
                else {
                    int v = (subsat >> bit)&((1 << eBlobIdBits_digit)-1);
                    if ( v < 10 ) {
                        seq.m_WGSAcc += char('0'+v);
                    }
                    else {
                        bad = true;
                        break;
                    }
                    bit += eBlobIdBits_digit;
                }
            }
            if ( seq.IsContig() ) {
                // old format means version is 1
                seq.m_Version = 1;
            }
            if ( bad ) { // bad format - illegal letters or digits
                return seq;
            }
        }
        else {
            seq.SetContig();
            subsat /= 4;
            for ( size_t i = 0; i < kPrefixLenV1; ++i ) {
                if ( i < kNumLettersV1 ) {
                    seq.m_WGSAcc += char('A'+subsat%26);
                    subsat /= 26;
                }
                else {
                    seq.m_WGSAcc += char('0'+subsat%10);
                    subsat /= 10;
                }
            }
            seq.m_Version = subsat + 2; // remaining value is version
        }
        // verify if the WGS accession actually exists in VDB
        if (skip_lookup  ||  GetWGSDb(seq)) {
            seq.m_ValidWGS = true;
            seq.m_RowId = id.GetSat_key();
        }
    }
    else if ( sat >= kBlobIdV2SatMin && sat <= kBlobIdV2SatMax ) {
        seq.m_IsWGS = true;
        Uint8 v = (Uint8(sat-kBlobIdV2SatMin) << 32)|Uint4(id.GetSub_sat());
        for ( size_t i = 0; i < 6; ++i ) {
            seq.m_WGSAcc += char('A'+v%26);
            v /= 26;
        }
        for ( size_t i = 0; i < 2; ++i ) {
            seq.m_WGSAcc += char('0'+v%10);
            v /= 10;
        }
        if ( v == kBlobIdV2VersionScaffold ) {
            seq.SetScaffold();
        }
        else if ( v == kBlobIdV2VersionProtein ) {
            seq.SetProtein();
        }
        else {
            seq.SetContig();
            seq.m_Version = int(v - kBlobIdV2VersionContig + 1);
        }
        // verify if the WGS accession actually exists in VDB
        if (skip_lookup  ||  GetWGSDb(seq)) {
            seq.m_ValidWGS = true;
            seq.m_RowId = id.GetSat_key();
        }
    }
    return seq;
}


CID2_Blob_Id& CWGSClient::GetBlobId(SWGSSeqInfo& seq0)
{
    if ( seq0.m_BlobId ) {
        return *seq0.m_BlobId;
    }
    CRef<CID2_Blob_Id> id(new CID2_Blob_Id);
    SWGSSeqInfo& seq = GetRootSeq(seq0);
    if ( seq.m_WGSAcc.size() == kPrefixLenV2 ) {
        Uint8 mul = 1;
        Uint8 value = 0;
        for ( size_t i = 0; i < seq.m_WGSAcc.size(); ++i ) {
            if ( i < kNumLettersV2 ) {
                value += (seq.m_WGSAcc[i]-'A')*mul;
                mul *= 26;
            }
            else {
                value += (seq.m_WGSAcc[i]-'0')*mul;
                mul *= 10;
            }
        }
        unsigned version;
        if ( seq.IsScaffold() ) {
            version = kBlobIdV2VersionScaffold;
        }
        else if ( seq.IsProtein() ) {
            version = kBlobIdV2VersionProtein;
        }
        else {
            _ASSERT(seq.IsContig());
            if ( seq.m_Version == -1 ) {
                // need contig version to choose appropriate blob-id format
                seq.m_Version = GetContigIterator(seq).GetLatestAccVersion();
            }
            _ASSERT(seq.m_Version >= 1);
            _ASSERT(seq.m_Version <= 16);
            version = seq.m_Version - 1 + kBlobIdV2VersionContig;
        }
        value += mul*version;
        CID2_Blob_Id::TSat sat = kBlobIdV2SatMin + int(value >> 32); // high 32 bits
        CID2_Blob_Id::TSub_sat subsat = int(value & 0xFFFFFFFF); // low 32 bits
        _ASSERT(sat >= kBlobIdV2SatMin && sat <= kBlobIdV2SatMax);
        id->SetSat(sat);
        id->SetSub_sat(subsat);
        id->SetSat_key(int(seq.m_RowId));
    }
    else {
        _ASSERT(seq.m_WGSAcc.size() == kPrefixLenV1);
        unsigned subsat;
        if ( seq.IsContig() && seq.m_Version == -1 ) {
            // need contig version to choose appropriate blob-id format
            seq.m_Version = GetContigIterator(seq).GetLatestAccVersion();
        }
        if ( !seq.IsContig() || seq.m_Version <= 1 ) {
            // old blob-id subsat format, version is 1
            if ( seq.IsScaffold() ) {
                subsat = eBlobType_scaffold;
            }
            else if ( seq.IsProtein() ) {
                subsat = eBlobType_protein;
            }
            else { // contig or master
                subsat = eBlobType_contig;
            }
            int bit = eBlobIdBits_type;
            for ( size_t i = 0; i < seq.m_WGSAcc.size(); ++i ) {
                if ( i < kNumLettersV1 ) {
                    subsat |= (seq.m_WGSAcc[i]-'A') << bit;
                    bit += eBlobIdBits_letter;
                }
                else {
                    subsat |= (seq.m_WGSAcc[i]-'0') << bit;
                    bit += eBlobIdBits_digit;
                }
            }
        }
        else {
            // new blob-id subsat format that includes contig version > 1
            _ASSERT(seq.IsContig());
            _ASSERT(seq.m_Version >= 2);
            _ASSERT(seq.m_Version <= 24);
            subsat = 0;
            unsigned mul = 4;
            for ( size_t i = 0; i < seq.m_WGSAcc.size(); ++i ) {
                if ( i < kNumLettersV1 ) {
                    subsat += (seq.m_WGSAcc[i]-'A')*mul;
                    mul *= 26;
                }
                else {
                    subsat += (seq.m_WGSAcc[i]-'0')*mul;
                    mul *= 10;
                }
            }
            subsat += (seq.m_Version - 2)*mul;
        }
        id->SetSat(kBlobIdV1Sat);
        id->SetSub_sat(int(subsat));
        id->SetSat_key(int(seq.m_RowId));
    }
    seq0.m_BlobId = id;
    return *id;
}


static int s_GBStateToID2(NCBI_gb_state gb_state)
{
    int state = 0;
    switch ( gb_state ) {
    case NCBI_gb_state_eWGSGenBankSuppressed:
        state |= 1 << eID2_Blob_State_suppressed;
        break;
    case NCBI_gb_state_eWGSGenBankReplaced:
    case NCBI_gb_state_eWGSGenBankMigrated:
        state |= 1 << eID2_Blob_State_dead;
        break;
    case NCBI_gb_state_eWGSGenBankWithdrawn:
        state |= 1 << eID2_Blob_State_withdrawn;
        break;
    default:
        break;
    }
    return state;
}


int CWGSClient::GetID2BlobState(SWGSSeqInfo& seq)
{
    return s_GBStateToID2(GetGBState(seq)) | s_GBStateToID2(seq.m_WGSDb->GetProjectGBState());
}


NCBI_gb_state CWGSClient::GetGBState(SWGSSeqInfo& seq0)
{
    SWGSSeqInfo& seq = GetRootSeq(seq0);
    if ( seq.IsContig() ) {
        return GetContigIterator(seq).GetGBState();
    }
    if ( seq.IsScaffold() ) {
        return 0;
    }
    if ( seq.IsProtein() ) {
        return GetProteinIterator(seq).GetGBState();
    }
    // master
    return 0;
}


CRef<CSeq_id> CWGSClient::GetAccVer(SWGSSeqInfo& seq)
{
    if ( seq.IsContig() ) {
        return GetContigIterator(seq).GetAccSeq_id();
    }
    if ( seq.IsScaffold() ) {
        return GetScaffoldIterator(seq).GetAccSeq_id();
    }
    if ( seq.IsProtein() ) {
        return GetProteinIterator(seq).GetAccSeq_id();
    }
    // master
    return GetWGSDb(seq)->GetMasterSeq_id();
}


CRef<CSeq_id> CWGSClient::GetGeneral(SWGSSeqInfo& seq)
{
    if ( seq.IsContig() ) {
        return GetContigIterator(seq).GetGeneralOrPatentSeq_id();
    }
    if ( seq.IsScaffold() ) {
        return GetScaffoldIterator(seq).GetGeneralOrPatentSeq_id();
    }
    if ( seq.IsProtein() ) {
        return GetProteinIterator(seq).GetGeneralOrPatentSeq_id();
    }
    // master
    return null;
}


TGi CWGSClient::GetGi(SWGSSeqInfo& seq)
{
    if ( seq.IsContig() ) {
        CWGSSeqIterator it = GetContigIterator(seq);
        return it.HasGi()? it.GetGi(): ZERO_GI;
    }
    if ( seq.IsScaffold() ) {
        // scaffolds have no GIs
        return ZERO_GI;
    }
    if ( seq.IsProtein() ) {
        CWGSProteinIterator it = GetProteinIterator(seq);
        return it.HasGi()? it.GetGi(): ZERO_GI;
    }
    // master
    return ZERO_GI;
}


void CWGSClient::GetSeqIds(SWGSSeqInfo& seq, list<CRef<CSeq_id> >& ids)
{
    ids.push_back(GetAccVer(seq));
    if ( CRef<CSeq_id> id = GetGeneral(seq) ) {
        ids.push_back(id);
    }
    TGi gi = GetGi(seq);
    if ( gi != ZERO_GI ) {
        CRef<CSeq_id> gi_id(new CSeq_id);
        gi_id->SetGi(gi);
        ids.push_back(gi_id);
    }
}


bool CWGSClient::GetCompress(
    SWGSProcessor_Config::ECompressData comp,
    const SWGSSeqInfo& seq,
    const CAsnBinData& data) const
{
    switch ( comp ) {
    case SWGSProcessor_Config::eCompressData_always: return true;
    case SWGSProcessor_Config::eCompressData_never: return false;
    default: return dynamic_cast<const CSeq_entry*>(&data.GetMainObject()) &&  seq.IsMaster();
    }
}


void CWGSClient::SetSeqId(CSeq_id& id, int seq_id_type, const string& seq_id)
{
    if (seq_id_type <= 0) {
        // no type check
        id.Set(seq_id);
    }
    else {
        id.Set(CSeq_id::eFasta_AsTypeAndContent, CSeq_id::E_Choice(seq_id_type), seq_id);
    }
}


void CWGSClient::GetBioseqInfo(shared_ptr<SWGSData>& data, SWGSSeqInfo& seq)
{
    if ( !seq ) return;

    data = make_shared<SWGSData>();
    data->m_GetResult = SWGSData::eResult_Found;
    data->m_BioseqInfo = make_shared<CBioseqInfoRecord>();
    CBioseqInfoRecord& info = *data->m_BioseqInfo;

    list< CRef<CSeq_id> > wgs_ids;
    GetSeqIds(seq, wgs_ids);
    CBioseqInfoRecord::TSeqIds psg_ids;
    TGi gi = ZERO_GI;
    for ( auto& id : wgs_ids ) {
        if ( id->IsGi() ) {
            gi = id->GetGi();
            info.SetGI(GI_TO(CBioseqInfoRecord::TGI, gi));
            data->m_BioseqInfoFlags |= SPSGS_ResolveRequest::fPSGS_Gi;
            continue;
        }
        else if ( auto text_id = id->GetTextseq_Id() ) {
            // only versioned accession goes to canonical id
            if ( !(data->m_BioseqInfoFlags & SPSGS_ResolveRequest::fPSGS_CanonicalId) &&
                 text_id->IsSetAccession() && text_id->IsSetVersion() ) {
                info.SetSeqIdType(id->Which());
                info.SetAccession(text_id->GetAccession());
                info.SetVersion(text_id->GetVersion());
                if ( text_id->IsSetName() ) {
                    info.SetName(text_id->GetName());
                }
                data->m_BioseqInfoFlags |=
                    SPSGS_ResolveRequest::fPSGS_CanonicalId |
                    SPSGS_ResolveRequest::fPSGS_Name;
                continue;
            }
        }
        string content;
        id->GetLabel(&content, CSeq_id::eFastaContent);
        psg_ids.insert(make_tuple(id->Which(), move(content)));
    }
    if ( gi != ZERO_GI ) {
        // gi goes either to canonical id or to other ids
        CSeq_id gi_id(CSeq_id::e_Gi, gi);
        string content;
        gi_id.GetLabel(&content, CSeq_id::eFastaContent);
        if ( !(data->m_BioseqInfoFlags & SPSGS_ResolveRequest::fPSGS_CanonicalId) ) {
            // set canonical id from gi
            info.SetAccession(content);
            info.SetVersion(0);
            info.SetSeqIdType(gi_id.Which());
            data->m_BioseqInfoFlags |=
                SPSGS_ResolveRequest::fPSGS_CanonicalId |
                SPSGS_ResolveRequest::fPSGS_Name;
        }
        else {
            // to other ids
            psg_ids.insert(make_tuple(gi_id.Which(), move(content)));
        }
    }
    if ( (data->m_BioseqInfoFlags & SPSGS_ResolveRequest::fPSGS_CanonicalId)  ||  !psg_ids.empty() ) {
        info.SetSeqIds(move(psg_ids));
        // all ids are requested, so we should get GI and acc.ver too if they exist
        info.SetGI(GI_TO(CBioseqInfoRecord::TGI, gi)); // even if it's zero
        data->m_BioseqInfoFlags |=
            SPSGS_ResolveRequest::fPSGS_SeqIds |
            SPSGS_ResolveRequest::fPSGS_CanonicalId |
            SPSGS_ResolveRequest::fPSGS_Gi;
    }

    if ( seq.IsContig() ) {
        CWGSSeqIterator it = GetContigIterator(seq);
        info.SetHash(it.GetSeqHash());
        info.SetLength(it.GetSeqLength());
        info.SetMol(GetWGSDb(seq)->GetContigMolType());
        data->m_BioseqInfoFlags |=
            SPSGS_ResolveRequest::fPSGS_Hash |
            SPSGS_ResolveRequest::fPSGS_Length |
            SPSGS_ResolveRequest::fPSGS_MoleculeType;
        if ( it.HasTaxId() ) {
            info.SetTaxId(it.GetTaxId());
            data->m_BioseqInfoFlags |= SPSGS_ResolveRequest::fPSGS_TaxId;
        }
    }
    if ( seq.IsScaffold() ) {
        CWGSScaffoldIterator it = GetScaffoldIterator(seq);
        info.SetLength(it.GetSeqLength());
        info.SetMol(GetWGSDb(seq)->GetScaffoldMolType());
        data->m_BioseqInfoFlags |=
            SPSGS_ResolveRequest::fPSGS_Length |
            SPSGS_ResolveRequest::fPSGS_MoleculeType;
    }
    if ( seq.IsProtein() ) {
        CWGSProteinIterator it = GetProteinIterator(seq);
        info.SetLength(it.GetSeqLength());
        info.SetMol(GetWGSDb(seq)->GetProteinMolType());
        data->m_BioseqInfoFlags |=
            SPSGS_ResolveRequest::fPSGS_Length |
            SPSGS_ResolveRequest::fPSGS_MoleculeType;
        if ( it.HasSeqHash() ) {
            info.SetHash(it.GetSeqHash());
            data->m_BioseqInfoFlags |=
                SPSGS_ResolveRequest::fPSGS_Hash;
        }
        {{
            // set taxid
            auto wgs = GetWGSDb(seq);
            // faster common taxid retrieval if possible
            if ( wgs->HasCommonTaxId() ) {
                info.SetTaxId(wgs->GetCommonTaxId());
                data->m_BioseqInfoFlags |= SPSGS_ResolveRequest::fPSGS_TaxId;
            }
            else {
                // otherwise get taxid from root sequence (contig or protein itself)
                auto& root_seq = GetRootSeq(seq);
                if ( root_seq.IsContig() ) {
                    CWGSSeqIterator root_it = GetContigIterator(root_seq);
                    if ( root_it.HasTaxId() ) {
                        info.SetTaxId(root_it.GetTaxId());
                        data->m_BioseqInfoFlags |= SPSGS_ResolveRequest::fPSGS_TaxId;
                    }
                }
                if ( root_seq.IsProtein() ) {
                    if ( it.HasTaxId() ) {
                        info.SetTaxId(it.GetTaxId());
                        data->m_BioseqInfoFlags |= SPSGS_ResolveRequest::fPSGS_TaxId;
                    }
                }
            }
        }}
    }

    data->m_Id2BlobId.Reset(&GetBlobId(seq));
    data->m_BlobId = GetPSGBlobId(*data->m_Id2BlobId);
    data->m_BioseqInfoFlags |= SPSGS_ResolveRequest::fPSGS_BlobId;
    if ( data->m_Id2BlobId->IsSetVersion() ) {
        // ID2 version is minutes since UNIX epoch
        // PSG date_changed is ms since UNIX epoch
        info.SetDateChanged(data->m_Id2BlobId->GetVersion()*60000);
        data->m_BioseqInfoFlags |= SPSGS_ResolveRequest::fPSGS_DateChanged;
    }

    data->m_Id2BlobState = GetID2BlobState(seq);
    info.SetState(data->GetPSGBioseqState());
    data->m_BioseqInfoFlags |= SPSGS_ResolveRequest::fPSGS_State;
}


void CWGSClient::GetWGSData(shared_ptr<SWGSData>& data, SWGSSeqInfo& seq0)
{
    if (!data) {
        data = make_shared<SWGSData>();
        data->m_GetResult = SWGSData::eResult_Found;
    }
    SWGSSeqInfo& seq = GetRootSeq(seq0);
    
    if ( !data->m_Id2BlobId ) data->m_Id2BlobId.Reset(&GetBlobId(seq0));
    if ( data->m_BlobId.empty() ) data->m_BlobId = GetPSGBlobId(*data->m_Id2BlobId);
    data->m_Id2BlobState = GetID2BlobState(seq0);
    if ( data->IsForbidden() ) return;
    
    if ( seq.IsMaster() ) {
        data->m_Data = new CAsnBinData(*GetWGSDb(seq)->GetMasterSeq_entry());
    }
    else if ( seq.IsContig() ) {
        CWGSSeqIterator& it = GetContigIterator(seq);
        CWGSSeqIterator::TFlags flags = it.fDefaultFlags ;
        if ( !s_AddMasterDescrContig() ) {
            flags &= ~it.fMasterDescr;
        }
        else if ( s_MarkMasterDescr() ) {
            flags |= it.fMasterDescrMark;
        }
        if ( !s_SplitFeatures() ) {
            flags &= ~it.fSplitFeatures;
        }
        auto asn_data = it.GetSplitInfoDataAndVersion(flags);
        if ( asn_data.first ) {
            data->m_SplitVersion = asn_data.second;
        }
        if ( !asn_data.first ) {
            asn_data.first = it.GetSeq_entryData(flags);
        }
        if ( !asn_data.first ) {
            asn_data.first = new CAsnBinData(*it.GetSeq_entry(flags));
        }
        data->m_Data = asn_data.first;
    }
    else if ( seq.IsScaffold() ) {
        CWGSScaffoldIterator& it = GetScaffoldIterator(seq);
        CWGSScaffoldIterator::TFlags flags = it.fDefaultFlags;
        if ( !s_AddMasterDescrScaffold() ) {
            flags &= ~it.fMasterDescr;
        }
        else if ( s_MarkMasterDescr() ) {
            flags |= it.fMasterDescrMark;
        }
        data->m_Data = new CAsnBinData(*it.GetSeq_entry(flags));
    }
    else if ( seq.IsProtein() ) {
        CWGSProteinIterator& it = GetProteinIterator(seq);
        CWGSProteinIterator::TFlags flags = it.fDefaultFlags;
        if ( !s_AddMasterDescrProtein() ) {
            flags &= ~it.fMasterDescr;
        }
        else if ( s_MarkMasterDescr() ) {
            flags |= it.fMasterDescrMark;
        }
        data->m_Data = new CAsnBinData(*it.GetSeq_entry(flags));
    }
    if ( data->m_Data ) {
        data->m_Compress = GetCompress(m_Config.m_CompressData, seq, *data->m_Data);
    }
    else {
        data.reset();
    }
}


CRef<CID2_Blob_Id> CWGSClient::ParsePSGBlobId(const SPSGS_BlobId& blob_id)
{
    Int4 sat;
    Int4 subsat;
    Int4 satkey;
    auto id_str = blob_id.GetId();
    CTempString s = id_str;
    if ( !s_ParseOSGBlob(s, sat, subsat, satkey) || !s.empty() ) {
        return null;
    }
    CRef<CID2_Blob_Id> id(new CID2_Blob_Id);
    id->SetSat(sat);
    id->SetSub_sat(subsat);
    id->SetSat_key(satkey);
    return id;
}


CWGSClient::SParsedId2Info CWGSClient::ParsePSGId2Info(const string& id2_info)
{
    Int4 sat;
    Int4 subsat;
    Int4 satkey;
    TID2BlobVersion tse_version;
    TID2SplitVersion split_version;

    CTempString s = id2_info;
    if ( !s_ParseOSGBlob(s, sat, subsat, satkey) ||
         !s_Skip(s, '.') ||
         !s_ParseInt(s, tse_version) ||
         !s_Skip(s, '.') ||
         !s_ParseInt(s, split_version) ||
         !s.empty() ) {
        return SParsedId2Info{};
    }

    CRef<CID2_Blob_Id> id(new CID2_Blob_Id);
    id->SetSat(sat);
    id->SetSub_sat(subsat);
    id->SetSat_key(satkey);
    id->SetVersion(tse_version);
    return SParsedId2Info{id, split_version};
}


bool CWGSClient::CanBeWGS(int seq_id_type, const string& seq_id)
{
    try {
        CSeq_id id;
        SetSeqId(id, seq_id_type, seq_id);
        if ( id.IsGi() ) {
            return true;
        }
        else if ( id.IsGeneral() ) {
            return IsWGSGeneral(id.GetGeneral());
        }
        else if ( auto text_id = id.GetTextseq_Id() ) {
            return IsWGSAccession(*text_id);
        }
        return false;
    }
    catch ( exception& /*ignored*/ ) {
        return false;
    }
}


string CWGSClient::GetPSGBlobId(const CID2_Blob_Id& blob_id)
{
    ostringstream s;
    if ( IsOSGBlob(blob_id) ) {
        s_FormatBlobId(s, blob_id);
    }
    return s.str();
}


bool CWGSClient::IsOSGBlob(const CID2_Blob_Id& blob_id)
{
    return s_IsOSGBlob(blob_id.GetSat(), blob_id.GetSub_sat(), blob_id.GetSat_key());
}



int SWGSData::GetPSGBioseqState() const
{
    if ( m_Id2BlobState == 0 ||
         (m_Id2BlobState & (1<<eID2_Blob_State_live)) ) {
        return eLive;
    }
    else if ( m_Id2BlobState & (1<<eID2_Blob_State_suppressed) ) {
        return eReserved;
    }
    else if ( m_Id2BlobState & (1<<eID2_Blob_State_dead) ) {
        return eDead;
    }
    else if ( m_Id2BlobState & (1<<eID2_Blob_State_withdrawn) ) {
        return eDead; // assume withdrawn as dead ???
    }
    else if ( m_Id2BlobState & (1<<eID2_Blob_State_protected) ) {
        return eDead; // assume protected (unauthorized) as dead ???
    }
    else {
        return eDead;
    }
}


bool SWGSData::IsForbidden(int id2_blob_state)
{
    if ( id2_blob_state & (1<<eID2_Blob_State_withdrawn) ) {
        return true;
    }
    else if ( id2_blob_state & (1<<eID2_Blob_State_protected) ) {
        return true;
    }
    return false;
}


END_NAMESPACE(wgs);
END_NAMESPACE(psg);
END_NCBI_NAMESPACE;
