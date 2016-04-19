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
#include <objects/general/general__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objmgr/bioseq_handle.hpp>

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
#define DEFAULT_COMPRESS_DATA eCompressData_some

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
static const size_t kTypePrefixLen = 4; // "SNP:" or "TSA:"
static const size_t kNumLetters = 4;
static const size_t kVersionDigits = 2;
static const size_t kPrefixLen = kNumLetters + kVersionDigits;
static const size_t kMinRowDigits = 6;
static const size_t kMaxRowDigits = 8;


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

#ifdef TRACE_PROCESSING

static CStopWatch sw;

# define START_TRACE() do { if(s_DebugLevel()>0)sw.Restart(); } while(0)

static CNcbiOstream& operator<<(CNcbiOstream& out,
                                const CID2SNPProcessor_Impl::SSNPSeqInfo& seq)
{
    out << seq.m_SNPAcc << '/' << seq.m_SeqId;
    return out;
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


BEGIN_LOCAL_NAMESPACE;


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


CID2SNPProcessor_Impl::CID2SNPProcessor_Impl(const CConfig::TParamTree* params,
                                             const string& driver_name)
    : m_DefaultCompressData(eCompressData_never),
      m_DefaultExplicitBlobState(false)
{
    auto_ptr<CConfig::TParamTree> app_params;
    if ( !params ) {
        CMutexGuard guard(CNcbiApplication::GetInstanceMutex());
        if ( CNcbiApplication* app = CNcbiApplication::Instance() ) {
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
    if ( compress_data >= eCompressData_never &&
         compress_data <= eCompressData_always ) {
        m_DefaultCompressData = ECompressData(compress_data);
    }
    ResetParameters();
    TRACE_X(23, eDebug_open, "ID2SNP: compress_data = "<<m_CompressData);
}


CID2SNPProcessor_Impl::~CID2SNPProcessor_Impl(void)
{
}


void CID2SNPProcessor_Impl::ResetParameters(void)
{
    m_CompressData = m_DefaultCompressData;
    m_ExplicitBlobState = m_DefaultExplicitBlobState;
}


void CID2SNPProcessor_Impl::ProcessInit(const CID2_Request& request)
{
    ResetParameters();
    if ( !request.IsSetParams() ) {
        return;
    }
    // check if blob-state field is allowed
    ITERATE ( CID2_Request::TParams::Tdata, it, request.GetParams().Get() ) {
        const CID2_Param& param = **it;
        if ( param.GetName() == "id2:allow" && param.IsSetValue() ) {
            ITERATE ( CID2_Param::TValue, it2, param.GetValue() ) {
                if ( *it2 == "*.blob-state" ) {
                    m_ExplicitBlobState = true;
                }
            }
        }
    }
}


CSNPDb CID2SNPProcessor_Impl::GetSNPDb(const string& prefix)
{
    CMutexGuard guard(m_Mutex);
    TSNPDbCache::iterator it = m_SNPDbCache.find(prefix);
    if ( it != m_SNPDbCache.end() ) {
        return it->second;
    }
    try {
        CSNPDb snp_db(m_Mgr, prefix);
        snp_db.LoadMasterDescr();
        m_SNPDbCache[prefix] = snp_db;
        TRACE_X(1, eDebug_open, "GetSNPDb: "<<prefix);
        return snp_db;
    }
    catch ( CSraException& exc ) {
        if ( exc.GetErrCode() == exc.eNotFoundDb ||
             exc.GetErrCode() == exc.eProtectedDb ) {
            // no such SNP table
        }
        else {
            TRACE_X(22, eDebug_error, "ID2SNP: "
                    "Exception while opening SNP DB "<<prefix<<": "<<exc);
        }
    }
    catch ( CException& exc ) {
        TRACE_X(22, eDebug_error, "ID2SNP: "
                "Exception while opening SNP DB "<<prefix<<": "<<exc);
    }
    catch ( exception& exc ) {
        TRACE_X(22, eDebug_error, "ID2SNP: "
                "Exception while opening SNP DB "<<prefix<<": "<<exc.what());
    }
    return CSNPDb();
}


CSNPDb& CID2SNPProcessor_Impl::GetSNPDb(SSNPSeqInfo& seq)
{
    if ( !seq.m_SNPDb ) {
        seq.m_SNPDb = GetSNPDb(seq.m_SNPAcc);
        if ( seq.m_SNPDb ) {
            seq.m_IsSNP = true;
            seq.m_RowDigits = Uint1(seq.m_SNPDb->GetIdRowDigits());
        }
    }
    return seq.m_SNPDb;
}


void CID2SNPProcessor_Impl::ResetIteratorCache(SSNPSeqInfo& seq)
{
    seq.m_ContigIter.Reset();
    seq.m_ScaffoldIter.Reset();
    seq.m_ProteinIter.Reset();
    seq.m_BlobId.Reset();
}


CSNPSeqIterator& CID2SNPProcessor_Impl::GetContigIterator(SSNPSeqInfo& seq)
{
    if ( !seq.m_ContigIter ) {
        seq.m_ContigIter = CSNPSeqIterator(GetSNPDb(seq), seq.m_RowId,
                                           CSNPSeqIterator::eExcludeWithdrawn);
    }
    return seq.m_ContigIter;
}


CSNPScaffoldIterator&
CID2SNPProcessor_Impl::GetScaffoldIterator(SSNPSeqInfo& seq)
{
    if ( !seq.m_ScaffoldIter ) {
        seq.m_ScaffoldIter = CSNPScaffoldIterator(GetSNPDb(seq), seq.m_RowId);
    }
    return seq.m_ScaffoldIter;
}


CSNPProteinIterator&
CID2SNPProcessor_Impl::GetProteinIterator(SSNPSeqInfo& seq)
{
    if ( !seq.m_ProteinIter ) {
        seq.m_ProteinIter = CSNPProteinIterator(GetSNPDb(seq), seq.m_RowId);
    }
    return seq.m_ProteinIter;
}


bool CID2SNPProcessor_Impl::IsValidRowId(SSNPSeqInfo& seq)
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


bool CID2SNPProcessor_Impl::IsCorrectVersion(SSNPSeqInfo& seq, int version)
{
    if ( !seq ) {
        return false;
    }
    if ( seq.IsContig() ) {
        CSNPSeqIterator& it = GetContigIterator(seq);
        return it && version == it.GetAccVersion();
    }
    else if ( seq.IsMaster() ) {
        // master version is already checked
        return true;
    }
    else {
        // scaffolds and proteins can have only version 1
        return version == 1;
    }
}


// Blob id
// sat = 1000 : SNP
// subsat bits   0-1: seq type: 1 - contig, 2 - scaffold, 3 - protein
// subsat bits   2-6: ASCII(letter 0) - 'A'
// subsat bits  7-11: ASCII(letter 1) - 'A'
// subsat bits 12-16: ASCII(letter 2) - 'A'
// subsat bits 17-21: ASCII(letter 3) - 'A'
// subsat bits 22-25: ASCII(digit 4) - '0'
// subsat bits 26-29: ASCII(digit 5) - '0'
// satkey: row-id
static const int kBlobIdSat = 1000;
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

CID2_Blob_Id& CID2SNPProcessor_Impl::GetBlobId(SSNPSeqInfo& seq0)
{
    if ( seq0.m_BlobId ) {
        return *seq0.m_BlobId;
    }
    SSNPSeqInfo seq = GetRootSeq(seq0);
    CRef<CID2_Blob_Id> id(new CID2_Blob_Id);
    id->SetSat(kBlobIdSat);
    int subsat;
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
    for ( size_t i = 0; i < seq.m_SNPAcc.size(); ++i ) {
        if ( i < kNumLetters ) {
            subsat |= (seq.m_SNPAcc[i]-'A') << bit;
            bit += eBlobIdBits_letter;
        }
        else {
            subsat |= (seq.m_SNPAcc[i]-'0') << bit;
            bit += eBlobIdBits_digit;
        }
    }
    id->SetSub_sat(subsat);
    id->SetSat_key(seq.m_RowId);
    seq0.m_BlobId = id;
    return *id;
}


CID2SNPProcessor_Impl::SSNPSeqInfo
CID2SNPProcessor_Impl::ResolveBlobId(const CID2_Blob_Id& id)
{
    if ( id.GetSat() != kBlobIdSat ) {
        return SSNPSeqInfo();
    }
    SSNPSeqInfo seq;
    seq.m_IsSNP = true;
    int subsat = id.GetSub_sat();
    switch ( subsat & ((1<<eBlobIdBits_type)-1) ) {
    case eBlobType_scaffold: seq.SetScaffold(); break;
    case eBlobType_protein: seq.SetProtein(); break;
    default: seq.SetContig(); break;
    }
    bool bad = false;
    int bit = eBlobIdBits_type;
    for ( size_t i = 0; i < kPrefixLen; ++i ) {
        if ( i < kNumLetters ) {
            int v = (subsat>>bit)&((1<<eBlobIdBits_letter)-1);
            if ( v < 26 ) {
                seq.m_SNPAcc += char('A'+v);
            }
            else {
                bad = true;
                break;
            }
            bit += eBlobIdBits_letter;
        }
        else {
            int v = (subsat>>bit)&((1<<eBlobIdBits_digit)-1);
            if ( v < 10 ) {
                seq.m_SNPAcc += char('0'+v);
            }
            else {
                bad = true;
                break;
            }
            bit += eBlobIdBits_digit;
        }
    }
    if ( !bad ) {
        if ( CSNPDb snp_db = GetSNPDb(seq) ) {
            seq.m_ValidSNP = true;
            seq.m_RowId = id.GetSat_key();
        }
    }
    return seq;
}


CID2SNPProcessor_Impl::SSNPSeqInfo
CID2SNPProcessor_Impl::ResolveGeneral(const CDbtag& dbtag)
{
    const CObject_id& object_id = dbtag.GetTag();
    const string& db = dbtag.GetDb();
    if ( db.size() != kTypePrefixLen+kNumLetters /* SNP:AAAA */ &&
         db.size() != kTypePrefixLen+kPrefixLen  /* SNP:AAAA01 */ ) {
        return SSNPSeqInfo();
    }
    bool is_tsa = false;
    if ( NStr::StartsWith(db, "SNP:", NStr::eNocase) ) {
    }
    else if ( NStr::StartsWith(db, "TSA:", NStr::eNocase) ) {
        is_tsa = true;
    }
    else {
        return SSNPSeqInfo();
    }
    string snp_acc = db.substr(kTypePrefixLen); // remove "SNP:" or "TSA:"

    NStr::ToUpper(snp_acc);
    if ( snp_acc.size() == kNumLetters ) {
        snp_acc += "01"; // add default version digits
    }
    SSNPSeqInfo seq;
    seq.m_SNPAcc = snp_acc;
    seq.m_IsSNP = true;
    CSNPDb snp_db = GetSNPDb(seq);
    if ( !snp_db || snp_db->IsTSA() != is_tsa ) {
        // TSA or SNP type must match
        return seq;
    }
    string tag;
    if ( object_id.IsStr() ) {
        tag = object_id.GetStr();
    }
    else {
        tag = NStr::NumericToString(object_id.GetId());
    }
    if ( TVDBRowId row = snp_db.GetContigNameRowId(tag) ) {
        seq.m_ValidSNP = true;
        seq.SetContig();
        seq.m_RowId = row;
    }
    if ( TVDBRowId row = snp_db.GetScaffoldNameRowId(tag) ) {
        seq.m_ValidSNP = true;
        seq.SetScaffold();
        seq.m_RowId = row;
    }
    if ( TVDBRowId row = snp_db.GetProteinNameRowId(tag) ) {
        seq.m_ValidSNP = true;
        seq.SetProtein();
        seq.m_RowId = row;
    }
    return seq;
}


CID2SNPProcessor_Impl::SSNPSeqInfo
CID2SNPProcessor_Impl::ResolveSNPAcc(const string& acc,
                                     const CTextseq_id& id,
                                     TAllowSeqType allow_seq_type)
{
    if ( acc.size() < kPrefixLen + kMinRowDigits ||
         acc.size() > kPrefixLen + kMaxRowDigits + 1 ) { // one for type letter
        return SSNPSeqInfo();
    }
    for ( size_t i = 0; i < kNumLetters; ++i ) {
        if ( !isalpha(acc[i]&0xff) ) {
            return SSNPSeqInfo();
        }
    }
    for ( size_t i = kNumLetters; i < kPrefixLen; ++i ) {
        if ( !isdigit(acc[i]&0xff) ) {
            return SSNPSeqInfo();
        }
    }
    SSNPSeqInfo seq;
    seq.m_SNPAcc = acc.substr(0, kPrefixLen);
    seq.m_IsSNP = true;
    SIZE_TYPE row_pos = kPrefixLen;
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
    Uint8 row = 0;
    for ( size_t i = row_pos; i < acc.size(); ++i ) {
        char c = acc[i];
        if ( c < '0' || c > '9' ) {
            return SSNPSeqInfo();
        }
        row = row*10+(c-'0');
    }
    seq.m_RowId = row;
    if ( !row ) {
        // zero row might be master SNP sequence
        // it mustn't have type letter, version digits and row must be zero
        // version must be positive
        if ( !seq.IsMaster() ) {
            return SSNPSeqInfo();
        }
        if ( !(allow_seq_type & fAllow_master) ) {
            return seq;
        }
        // now, move version into version digits of the accession
        int version = id.IsSetVersion()? id.GetVersion(): 1;
        if ( version <= 0 ) {
            return SSNPSeqInfo();
        }
        for ( size_t i = kVersionDigits; i--; version /= 10) {
            if ( acc[kNumLetters+i] != '0' ) {
                return SSNPSeqInfo();
            }
            seq.m_SNPAcc[kNumLetters+i] = char('0'+version%10);
        }
        if ( version ) {
            // doesn't fit
            return SSNPSeqInfo();
        }
    }
    else if ( seq.IsContig() ) {
        if ( !(allow_seq_type & fAllow_contig) ) {
            return seq;
        }
    }
    if ( !GetSNPDb(seq) ) {
        return seq;
    }
    SIZE_TYPE row_digits = acc.size() - row_pos;
    if ( seq.m_SNPDb->GetIdRowDigits() != row_digits ) {
        return seq;
    }
    if ( !row ) {
        if ( kResolveMaster == eResolveMaster_never ) {
            // no master resolution
            seq.m_IsSNP = false;
            return seq;
        }
        else if ( kResolveMaster == eResolveMaster_without_gi ) {
            // only master sequences w/o GI are resolved
            if ( GetSNPDb(seq)->GetMasterGi() ) {
                // Let master sequences with GI to be processed by ID
                seq.m_IsSNP = false;
                return seq;
            }
        }
    }
    else if ( !IsValidRowId(seq) ) {
        return seq;
    }
    seq.m_ValidSNP = true;
    return seq;
}


CID2SNPProcessor_Impl::SSNPSeqInfo
CID2SNPProcessor_Impl::ResolveAcc(const CTextseq_id& id)
{
    if ( id.IsSetName() ) {
        // first try name reference if it has SNP format like AAAA01P000001
        // as it directly contains SNP accession
        if ( SSNPSeqInfo seq = ResolveSNPAcc(id.GetName(), id, fAllow_aa) ) {
            _ASSERT(seq.IsProtein());
            if ( !id.IsSetAccession() ||
                 NStr::EqualNocase(GetProteinIterator(seq).GetAccession(),
                                   id.GetAccession()) ) {
                return seq;
            }
        }
    }
    if ( !id.IsSetAccession() ) {
        return SSNPSeqInfo();
    }
    const string& acc = id.GetAccession();
    CSeq_id::EAccessionInfo type = CSeq_id::IdentifyAccession(acc);
    switch ( type & CSeq_id::eAcc_division_mask ) {
        // accepted accession types
    case CSeq_id::eAcc_snp:
    case CSeq_id::eAcc_snp_intermed:
    case CSeq_id::eAcc_tsa:
        if ( type & CSeq_id::fAcc_prot ) {
            if ( acc.size() <= 8 ) { // 3+5
                return ResolveProtAcc(id);
            }
            return SSNPSeqInfo();
        }
        else {
            return ResolveSNPAcc(acc, id, fAllow_master|fAllow_na);
        }
    default:
        // non-SNP accessions
        return SSNPSeqInfo();
    }
}


CID2SNPProcessor_Impl::SSNPSeqInfo
CID2SNPProcessor_Impl::GetRootSeq(const SSNPSeqInfo& seq0)
{
    SSNPSeqInfo seq = seq0;
    if ( !seq.IsProtein() ) {
        return seq;
    }
    // proteins can be located in nuc-prot set
    TVDBRowId cds_row_id = GetProteinIterator(seq).GetProductFeatRowId();
    if ( !cds_row_id ) {
        return seq;
    }
    CSNPFeatureIterator cds_it(GetSNPDb(seq), cds_row_id);
    if ( !cds_it ) {
        return seq;
    }
    switch ( cds_it.GetLocSeqType() ) {
    case NCBI_SNP_seqtype_contig:
        // switch to contig
        seq.SetContig();
        seq.m_RowId = cds_it.GetLocRowId();
        ResetIteratorCache(seq);
        return seq;
    case NCBI_SNP_seqtype_scaffold:
        // switch to scaffold
        seq.SetScaffold();
        seq.m_RowId = cds_it.GetLocRowId();
        ResetIteratorCache(seq);
        return seq;
    default:
        return seq;
    }
}


CID2SNPProcessor_Impl::SSNPSeqInfo
CID2SNPProcessor_Impl::Resolve(const CSeq_id& id)
{
    if ( !s_Enabled() ) {
        return SSNPSeqInfo();
    }
    switch ( id.Which() ) {
    case CSeq_id::e_Gi:
        return ResolveGi(id.GetGi());
    case CSeq_id::e_General:
        return ResolveGeneral(id.GetGeneral());
    case CSeq_id::e_not_set:
    case CSeq_id::e_Local:
    case CSeq_id::e_Gibbsq:
    case CSeq_id::e_Gibbmt:
    case CSeq_id::e_Giim:
    case CSeq_id::e_Patent:
    case CSeq_id::e_Pdb:
        return SSNPSeqInfo();
    default:
        break;
    }
    const CTextseq_id* text_id = id.GetTextseq_Id();
    if ( !text_id ) {
        return SSNPSeqInfo();
    }
    SSNPSeqInfo seq = ResolveAcc(*text_id);
    if ( !seq ) {
        return seq;
    }
    if ( text_id->IsSetVersion() &&
         !IsCorrectVersion(seq, text_id->GetVersion()) ) {
        return seq;
    }
    seq.m_ValidSNP = true;
    return seq;
}


CRef<CSeq_id> CID2SNPProcessor_Impl::GetAccVer(SSNPSeqInfo& seq)
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
    return GetSNPDb(seq)->GetMasterSeq_id();
}


CRef<CSeq_id> CID2SNPProcessor_Impl::GetGeneral(SSNPSeqInfo& seq)
{
    if ( seq.IsContig() ) {
        return GetContigIterator(seq).GetGeneralSeq_id();
    }
    if ( seq.IsScaffold() ) {
        return GetScaffoldIterator(seq).GetGeneralSeq_id();
    }
    if ( seq.IsProtein() ) {
        return GetProteinIterator(seq).GetGeneralSeq_id();
    }
    // master
    return null;
}


TGi CID2SNPProcessor_Impl::GetGi(SSNPSeqInfo& seq)
{
    if ( seq.IsContig() ) {
        CSNPSeqIterator it = GetContigIterator(seq);
        return it.HasGi()? it.GetGi(): ZERO_GI;
    }
    if ( seq.IsScaffold() ) {
        // scaffolds have no GIs
        return ZERO_GI;
    }
    if ( seq.IsProtein() ) {
        // proteins have no GIs
        return ZERO_GI;
    }
    // master
    return ZERO_GI;
}


void CID2SNPProcessor_Impl::GetSeqIds(SSNPSeqInfo& seq,
                                      list<CRef<CSeq_id> >& ids)
{
    ids.push_back(GetAccVer(seq));
    if ( CRef<CSeq_id> id = GetGeneral(seq) ) {
        ids.push_back(id);
    }
    if ( TGi gi = GetGi(seq) ) {
        CRef<CSeq_id> gi_id(new CSeq_id);
        gi_id->SetGi(gi);
        ids.push_back(gi_id);
    }
}


string CID2SNPProcessor_Impl::GetLabel(SSNPSeqInfo& seq)
{
    return objects::GetLabel(*GetAccVer(seq));
}


int CID2SNPProcessor_Impl::GetTaxId(SSNPSeqInfo& seq)
{
    if ( !seq ) {
        return -1;
    }
    if ( seq.IsContig() ) {
        CSNPSeqIterator it = GetContigIterator(seq);
        return it.HasTaxId()? it.GetTaxId(): 0;
    }
    return 0; // taxid is not defined
}


int CID2SNPProcessor_Impl::GetHash(SSNPSeqInfo& seq)
{
    if ( !seq ) {
        return 0;
    }
    if ( seq.IsContig() ) {
        return GetContigIterator(seq).GetSeqHash();
    }
    return 0;
}


static CObject_id& s_AddSpecialId(CID2_Reply_Get_Seq_id::TSeq_id& ids,
                                  const char* name)
{
    CRef<CSeq_id> ret_id(new CSeq_id);
    ids.push_back(ret_id);
    CDbtag& dbtag = ret_id->SetGeneral();
    dbtag.SetDb(name);
    return dbtag.SetTag();
}


static void s_AddSpecialId(CID2_Reply_Get_Seq_id::TSeq_id& ids,
                           const char* name,
                           const string& value)
{
    s_AddSpecialId(ids, name).SetStr(value);
}


static void s_AddSpecialId(CID2_Reply_Get_Seq_id::TSeq_id& ids,
                           const char* name,
                           int value)
{
    s_AddSpecialId(ids, name).SetId(value);
}


CID2SNPProcessor_Impl::SSNPSeqInfo
CID2SNPProcessor_Impl::Resolve(TReplies& replies,
                               CID2_Request& main_request,
                               CID2_Request_Get_Seq_id& request)
{
    if ( !request.GetSeq_id().IsSeq_id() ) {
        return SSNPSeqInfo();
    }
    SSNPSeqInfo seq = Resolve(request.GetSeq_id().GetSeq_id());
    if ( !seq ) {
        if ( seq.m_IsSNP && !s_FilterAll () ) {
            seq.m_IsSNP = false;
        }
        if ( seq.m_IsSNP ) {
            // SNP request for inexistent data
            CRef<CID2_Reply> main_reply(new CID2_Reply);
            if ( main_request.IsSetSerial_number() ) {
                main_reply->SetSerial_number(main_request.GetSerial_number());
            }
            CID2_Reply_Get_Seq_id& reply =
                main_reply->SetReply().SetGet_seq_id();
            reply.SetRequest(request);
            reply.SetEnd_of_reply();
            CRef<CID2_Error> error(new CID2_Error);
            error->SetSeverity(CID2_Error::eSeverity_no_data);
            main_reply->SetError().push_back(error);
            replies.push_back(main_reply);
            TRACE_X(2, eDebug_replies,
                    "GetSeqId: "<<MSerial_AsnText<<*main_reply);
        }
        return seq;
    }
    CRef<CID2_Reply> main_reply(new CID2_Reply);
    if ( main_request.IsSetSerial_number() ) {
        main_reply->SetSerial_number(main_request.GetSerial_number());
    }
    CID2_Reply_Get_Seq_id& reply =
        main_reply->SetReply().SetGet_seq_id();
    reply.SetRequest(request);
    CID2_Reply_Get_Seq_id::TSeq_id& ids = reply.SetSeq_id();
    switch ( request.GetSeq_id_type() ) {
    case CID2_Request_Get_Seq_id::eSeq_id_type_any:
    case CID2_Request_Get_Seq_id::eSeq_id_type_text:
        if ( CRef<CSeq_id> id = GetAccVer(seq) ) {
            ids.push_back(id);
        }
        break;
    case CID2_Request_Get_Seq_id::eSeq_id_type_gi:
        if ( TGi gi = GetGi(seq) ) {
            CRef<CSeq_id> gi_id(new CSeq_id);
            gi_id->SetGi(gi);
            ids.push_back(gi_id);
        }
        break;
    case CID2_Request_Get_Seq_id::eSeq_id_type_general:
        if ( CRef<CSeq_id> id = GetGeneral(seq) ) {
            ids.push_back(id);
        }
        break;
    case CID2_Request_Get_Seq_id::eSeq_id_type_all:
        GetSeqIds(seq, ids);
        break;
    case CID2_Request_Get_Seq_id::eSeq_id_type_label:
        s_AddSpecialId(ids, "LABEL", GetLabel(seq));
        break;
    case CID2_Request_Get_Seq_id::eSeq_id_type_taxid:
        s_AddSpecialId(ids, "TAXID", GetTaxId(seq));
        break;
    case CID2_Request_Get_Seq_id::eSeq_id_type_hash:
        s_AddSpecialId(ids, "HASH", GetHash(seq));
        break;
    default:
        break;
    }
    reply.SetEnd_of_reply();
    replies.push_back(main_reply);
    TRACE_X(3, eDebug_replies, "GetSeqId: "<<MSerial_AsnText<<*main_reply);
    return seq;
}


bool CID2SNPProcessor_Impl::ProcessGetSeqId(TReplies& replies,
                                            CID2_Request& main_request,
                                            CID2_Request_Get_Seq_id& request)
{
    START_TRACE();
    TRACE_X(4, eDebug_request, "GetSeqId: "<<MSerial_AsnText<<main_request);
    SSNPSeqInfo seq = Resolve(replies, main_request, request);
    TRACE_X(5, eDebug_resolve, "GetSeqId: done");
    return seq || seq.m_IsSNP;
}


CID2SNPProcessor_Impl::SSNPSeqInfo
CID2SNPProcessor_Impl::Resolve(TReplies& replies,
                               CID2_Request& main_request,
                               CID2_Request_Get_Blob_Id& request)
{
    SSNPSeqInfo seq = Resolve(replies, main_request, request.SetSeq_id());
    if ( !seq ) {
        return seq;
    }
    CRef<CID2_Reply> main_reply(new CID2_Reply);
    if ( main_request.IsSetSerial_number() ) {
        main_reply->SetSerial_number(main_request.GetSerial_number());
    }
    CID2_Reply_Get_Blob_Id& reply = main_reply->SetReply().SetGet_blob_id();
    reply.SetSeq_id(request.SetSeq_id().SetSeq_id().SetSeq_id());
    reply.SetBlob_id(GetBlobId(seq));
    if ( int blob_state = GetID2BlobState(seq) ) {
        SetBlobState(*main_reply, blob_state);
    }
    reply.SetEnd_of_reply();
    replies.push_back(main_reply);
    TRACE_X(6, eDebug_replies, "GetBlobId: "<<MSerial_AsnText<<*main_reply);
    return seq;
}


bool CID2SNPProcessor_Impl::ProcessGetBlobId(TReplies& replies,
                                        CID2_Request& main_request,
                                        CID2_Request_Get_Blob_Id& request)
{
    START_TRACE();
    TRACE_X(7, eDebug_request, "GetBlobId: "<<MSerial_AsnText<<main_request);
    SSNPSeqInfo seq = Resolve(replies, main_request, request);
    TRACE_X(8, eDebug_resolve, "GetBlobId: done");
    return seq || seq.m_IsSNP;
}


namespace {
};


NCBI_gb_state CID2SNPProcessor_Impl::GetGBState(SSNPSeqInfo& seq)
{
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


CBioseq_Handle::TBioseqStateFlags
CID2SNPProcessor_Impl::GetBioseqState(SSNPSeqInfo& seq)
{
    CBioseq_Handle::TBioseqStateFlags state = 0;
    switch ( GetGBState(seq) ) {
    case 1:
        state |= CBioseq_Handle::fState_suppress_perm;
        break;
    case 2:
        state |= CBioseq_Handle::fState_dead;
        break;
    case 3:
        state |= (CBioseq_Handle::fState_withdrawn |
                  CBioseq_Handle::fState_no_data);
        break;
    default:
        break;
    }
    return state;
}


int CID2SNPProcessor_Impl::GetID2BlobState(SSNPSeqInfo& seq)
{
    int state = 0;
    switch ( GetGBState(seq) ) {
    case 1:
        state |= 1<<eID2_Blob_State_suppressed;
        break;
    case 2:
        state |= 1<<eID2_Blob_State_dead;
        break;
    case 3:
        state |= 1<<eID2_Blob_State_withdrawn;
        break;
    default:
        break;
    }
    return state;
}


void CID2SNPProcessor_Impl::SetBlobState(CID2_Reply& main_reply,
                                         int blob_state)
{
    if ( !blob_state ) {
        return;
    }
    if ( m_ExplicitBlobState ) {
        CID2_Reply::TReply& reply = main_reply.SetReply();
        switch ( reply.Which() ) {
        case CID2_Reply::TReply::e_Get_blob_id:
            reply.SetGet_blob_id().SetBlob_state(blob_state);
            return;
        case CID2_Reply::TReply::e_Get_blob:
            reply.SetGet_blob().SetBlob_state(blob_state);
            return;
        case CID2_Reply::TReply::e_Get_split_info:
            reply.SetGet_split_info().SetBlob_state(blob_state);
            return;
        default:
            break;
        }
    }
    // set blob state in warning string
    string warning;
    if ( blob_state & (1<<eID2_Blob_State_dead) ) {
        if ( !warning.empty() ) {
            warning += ", ";
        }
        warning += "obsolete";
    }
    if ( blob_state & (1<<eID2_Blob_State_suppressed) ) {
        if ( !warning.empty() ) {
            warning += ", ";
        }
        warning += "suppressed";
    }
    if ( !warning.empty() ) {
        CRef<CID2_Error> error(new CID2_Error);
        error->SetSeverity(CID2_Error::eSeverity_warning);
        error->SetMessage(warning);
        main_reply.SetError().push_back(error);
    }
    if ( blob_state & (1<<eID2_Blob_State_withdrawn) ) {
        CRef<CID2_Error> error(new CID2_Error);
        error->SetSeverity(CID2_Error::eSeverity_restricted_data);
        error->SetMessage("withdrawn");
        main_reply.SetError().push_back(error);
    }
}


CRef<CSeq_entry> CID2SNPProcessor_Impl::GetSeq_entry(SSNPSeqInfo& seq0)
{
    SSNPSeqInfo seq = GetRootSeq(seq0);
    if ( seq.IsMaster() ) {
        return GetSNPDb(seq)->GetMasterSeq_entry();
    }
    if ( seq.IsContig() ) {
        CSNPSeqIterator& it = GetContigIterator(seq);
        CSNPSeqIterator::TFlags flags =
            (it.fDefaultFlags & ~it.fMaskDescr) | it.fSeqDescr;
        return it.GetSeq_entry(flags);
    }
    if ( seq.IsScaffold() ) {
        CSNPScaffoldIterator& it = GetScaffoldIterator(seq);
        CSNPScaffoldIterator::TFlags flags =
            (it.fDefaultFlags & ~it.fMaskDescr) | it.fSeqDescr;
        return it.GetSeq_entry(flags);
    }
    if ( seq.IsProtein() ) {
        CSNPProteinIterator& it = GetProteinIterator(seq);
        CSNPProteinIterator::TFlags flags =
            (it.fDefaultFlags & ~it.fMaskDescr) | it.fSeqDescr;
        return it.GetSeq_entry(flags);
    }
    return null;
}


bool CID2SNPProcessor_Impl::WorthCompressing(const SSNPSeqInfo& seq)
{
    if ( seq.IsMaster() ) {
        return true;
    }
    return false;
}


void CID2SNPProcessor_Impl::WriteData(const SSNPSeqInfo& seq,
                                      CID2_Reply_Data& data,
                                      const CSerialObject& obj)
{
    data.SetData_format(CID2_Reply_Data::eData_format_asn_binary);
    COSSWriter writer(data.SetData());
    CWStream writer_stream(&writer);
    AutoPtr<CNcbiOstream> str;
    if ( (m_CompressData == eCompressData_always) ||
         (m_CompressData == eCompressData_some && WorthCompressing(seq)) ) {
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


bool CID2SNPProcessor_Impl::ExcludedBlob(SSNPSeqInfo& seq,
                                         const CID2_Request_Get_Blob_Info& request)
{
    const CID2_Request_Get_Blob_Info::TBlob_id& req_id = request.GetBlob_id();
    if ( !req_id.IsResolve() ) {
        return false;
    }
    const CID2_Request_Get_Blob_Info::TBlob_id::TResolve& resolve = req_id.GetResolve();
    if ( !resolve.IsSetExclude_blobs() ) {
        return false;
    }
    CID2_Blob_Id& blob_id = GetBlobId(seq);
    ITERATE ( CID2_Request_Get_Blob_Info::TBlob_id::TResolve::TExclude_blobs, it, resolve.GetExclude_blobs() ) {
        if ( blob_id.Equals(**it) ) {
            return true;
        }
    }
    return false;
}


bool CID2SNPProcessor_Impl::ProcessGetBlobInfo(TReplies& replies,
                                               CID2_Request& main_request,
                                               CID2_Request_Get_Blob_Info& request)
{
    START_TRACE();
    TRACE_X(9, eDebug_request, "GetBlobInfo: "<<MSerial_AsnText<<main_request);
    SSNPSeqInfo seq;
    CID2_Request_Get_Blob_Info::TBlob_id& req_id = request.SetBlob_id();
    if ( req_id.IsBlob_id() ) {
        seq = ResolveBlobId(req_id.GetBlob_id());
    }
    else {
        seq = Resolve(replies, main_request, req_id.SetResolve().SetRequest());
    }
    if ( !seq ) {
        TRACE_X(10, eDebug_resolve, "GetBlobInfo: null");
        return seq.m_IsSNP;
    }

    if ( request.IsSetGet_data() && !ExcludedBlob(seq, request) ) {
        CRef<CSeq_entry> entry = GetSeq_entry(seq);
        TRACE_X(11, eDebug_resolve, "GetSeq_entry: "<<seq);
        if ( entry ) {
            TRACE_X(13, eDebug_data, "Seq("<<seq<<"): "<<
                    MSerial_AsnText<<*entry);
        }
        CRef<CID2_Reply> main_reply(new CID2_Reply);
        if ( main_request.IsSetSerial_number() ) {
            main_reply->SetSerial_number(main_request.GetSerial_number());
        }
        CID2_Reply_Get_Blob& reply = main_reply->SetReply().SetGet_blob();
        reply.SetBlob_id(GetBlobId(seq));
        if ( int blob_state = GetID2BlobState(seq) ) {
            SetBlobState(*main_reply, blob_state);
        }
        CID2_Reply_Data& data = reply.SetData();
        data.SetData_type(CID2_Reply_Data::eData_type_seq_entry);
        WriteData(seq, data, *entry);
        TRACE_X(12, eDebug_resolve, "Seq("<<seq<<"): "<<
                " data size: "<<sx_GetSize(data));
        replies.push_back(main_reply);
    }
    TRACE_X(14, eDebug_resolve,"GetBlobInfo: done");
    return true;
}


bool CID2SNPProcessor_Impl::ProcessRequest(TReplies& replies,
                                           CID2_Request& request)
{
    if ( !s_Enabled() ) {
        return false;
    }
    size_t old_size = replies.size();
    bool done;
    try {
        switch ( request.GetRequest().Which() ) {
        case CID2_Request::TRequest::e_Init:
            ProcessInit(request);
            return false;
        case CID2_Request::TRequest::e_Get_seq_id:
            done = ProcessGetSeqId(replies, request,
                                   request.SetRequest().SetGet_seq_id());
            break;
        case CID2_Request::TRequest::e_Get_blob_id:
            done = ProcessGetBlobId(replies, request,
                                    request.SetRequest().SetGet_blob_id());
            break;
        case CID2_Request::TRequest::e_Get_blob_info:
            done = ProcessGetBlobInfo(replies, request,
                                      request.SetRequest().SetGet_blob_info());
            break;
        default:
            return false;
        }
    }
    catch ( CException& exc ) {
        if ( s_DebugEnabled(eDebug_error) ) {
            ERR_POST_X(16, "ID2SNP: Exception while processing request "<<
                       MSerial_AsnText<<request<<": "<<exc);
        }
        done = false;
    }
    catch ( exception& exc ) {
        if ( s_DebugEnabled(eDebug_error) ) {
            ERR_POST_X(16, "ID2SNP: Exception while processing request "<<
                       MSerial_AsnText<<request<<": "<<exc.what());
        }
        done = false;
    }
    if ( !done ) {
        if ( replies.size() != old_size ) {
            if ( s_DebugEnabled(eDebug_error) ) {
                ERR_POST_X(17, "ID2SNP: bad replies for request "<<
                           MSerial_AsnText<<request);
            }
            replies.resize(old_size);
        }
        return false;
    }
    if ( replies.size() == old_size ) {
        if ( s_DebugEnabled(eDebug_error) ) {
            ERR_POST_X(17, "ID2SNP: no replies for request "<<
                       MSerial_AsnText<<request);
        }
        return false;
    }
    replies.back()->SetEnd_of_reply();
    return true;
}


CID2SNPProcessor_Impl::TReplies
CID2SNPProcessor_Impl::ProcessSomeRequests(CID2_Request_Packet& packet)
{
    TReplies replies;
    ERASE_ITERATE ( CID2_Request_Packet::Tdata, it, packet.Set() ) {
        if ( ProcessRequest(replies, **it) ) {
            packet.Set().erase(it);
        }
    }
    if ( !packet.Set().empty() ) {
        START_TRACE();
        TRACE_X(15, eDebug_request, "Unprocessed: "<<MSerial_AsnText<<packet);
    }
    return replies;
}


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;
