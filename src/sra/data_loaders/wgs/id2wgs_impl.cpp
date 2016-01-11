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
 *   Processor of ID2 requests for WGS data
 *
 */

#include <ncbi_pch.hpp>
#include <sra/data_loaders/wgs/impl/id2wgs_impl.hpp>
#include <objects/id2/id2processor_interface.hpp>
#include <sra/data_loaders/wgs/id2wgs_params.h>
#include <sra/readers/sra/wgsread.hpp>
#include <sra/readers/sra/wgsresolver.hpp>
#include <sra/error_codes.hpp>
#include <corelib/reader_writer.hpp>
#include <corelib/rwstream.hpp>
#include <util/thread_nonstop.hpp>
#include <util/compress/zlib.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/serial.hpp>
#include <objects/id2/id2__.hpp>
#include <objects/seqsplit/ID2S_Split_Info.hpp>
#include <objects/seqsplit/ID2S_Chunk.hpp>
#include <objects/general/general__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objmgr/bioseq_handle.hpp>

BEGIN_NCBI_NAMESPACE;

#define NCBI_USE_ERRCODE_X   ID2WGSProcessor
NCBI_DEFINE_ERR_SUBCODE_X(24);

BEGIN_NAMESPACE(objects);

// behavior options
#define ALLOW_SPLIT
#define TRACE_PROCESSING

enum EResolveMaster {
    eResolveMaster_never,
    eResolveMaster_without_gi,
    eResolveMaster_always
};
static const EResolveMaster kResolveMaster = eResolveMaster_never;

// default configuration parameters
#define DEFAULT_VDB_CACHE_SIZE 100
#define DEFAULT_INDEX_UPDATE_TIME 600
#define DEFAULT_COMPRESS_DATA CID2WGSContext::eCompressData_some

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

// WGS accession parameters
static const size_t kTypePrefixLen = 4; // "WGS:" or "TSA:"
static const size_t kNumLetters = 4;
static const size_t kVersionDigits = 2;
static const size_t kPrefixLen = kNumLetters + kVersionDigits;
static const size_t kMinRowDigits = 6;
static const size_t kMaxRowDigits = 8;


// parameters reading
NCBI_PARAM_DECL(bool, ID2WGS, ENABLE);
NCBI_PARAM_DEF_EX(bool, ID2WGS, ENABLE, true,
                  eParam_NoThread, ID2WGS_ENABLE);


NCBI_PARAM_DECL(int, ID2WGS, DEBUG);
NCBI_PARAM_DEF_EX(int, ID2WGS, DEBUG, eDebug_error,
                  eParam_NoThread, ID2WGS_DEBUG);


NCBI_PARAM_DECL(bool, ID2WGS, FILTER_ALL);
NCBI_PARAM_DEF_EX(bool, ID2WGS, FILTER_ALL, true,
                  eParam_NoThread, ID2WGS_FILTER_ALL);


static inline bool s_Enabled(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(ID2WGS, ENABLE)> s_Value;
    return s_Value->Get();
}


static inline int s_DebugLevel(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(ID2WGS, DEBUG)> s_Value;
    return s_Value->Get();
}


static inline bool s_DebugEnabled(EDebugLevel level)
{
    return s_DebugLevel() >= level;
}


static inline bool s_FilterAll(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(ID2WGS, FILTER_ALL)> s_Value;
    return s_Value->Get();
}


/////////////////////////////////////////////////////////////////////////////
// CID2WGSProcessor_Impl
/////////////////////////////////////////////////////////////////////////////

#ifdef TRACE_PROCESSING

static CStopWatch sw;

# define START_TRACE() do { if(s_DebugLevel()>0)sw.Restart(); } while(0)

static CNcbiOstream& operator<<(CNcbiOstream& out,
                                const CID2WGSProcessor_Impl::SWGSSeqInfo& seq)
{
    if ( seq.IsMaster() ) {
        out << seq.m_WGSAcc.substr(0, kNumLetters)<<"00";
    }
    else {
        out << seq.m_WGSAcc;
        if ( seq.IsScaffold() ) {
            out << 'S';
        }
        else if ( seq.IsProtein() ) {
            out << 'P';
        }
    }
    out << setfill('0')<<setw(seq.m_RowDigits)<<seq.m_RowId<<setfill(' ');
    if ( seq.IsMaster() ) {
        out << '.' << NStr::StringToInt(seq.m_WGSAcc.substr(kNumLetters));
    }
    return out;
}
# define TRACE_X(t,l,m)                                                 \
    do {                                                                \
        if ( s_DebugEnabled(l) ) {                                      \
            LOG_POST_X(t, Info<<sw.Elapsed()<<": ID2WGS: "<<m);         \
        }                                                               \
    } while(0)
#else
# define START_TRACE() do{}while(0)
# define TRACE_X(t,l,m) do{}while(0)
#endif


BEGIN_LOCAL_NAMESPACE;


class CIndexUpdateThread : public CThreadNonStop
{
public:
    CIndexUpdateThread(unsigned update_delay,
                       CRef<CWGSResolver> resolver)
        : CThreadNonStop(update_delay),
          m_Resolver(resolver)
        {
        }

protected:
    virtual void DoJob(void) {
        try {
            if ( m_Resolver->Update() ) {
                if ( s_DebugEnabled(eDebug_open) ) {
                    LOG_POST_X(18, Info<<"ID2WGS: updated WGS index");
                }
            }
        }
        catch ( CException& exc ) {
            if ( s_DebugEnabled(eDebug_error) ) {
                ERR_POST_X(20, "ID2WGS: "
                           "Exception while updating WGS index: "<<exc);
            }
        }
        catch ( exception& exc ) {
            if ( s_DebugEnabled(eDebug_error) ) {
                ERR_POST_X(20, "ID2WGS: "
                           "Exception while updating WGS index: "<<exc.what());
            }
        }
    }

private:
    CRef<CWGSResolver> m_Resolver;
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


size_t sx_GetSize(const CID2_Reply_Data& data)
{
    size_t size = 0;
    ITERATE ( CID2_Reply_Data::TData, it, data.GetData() ) {
        size += (*it)->size();
    }
    return size;
}


END_LOCAL_NAMESPACE;


CID2WGSContext::CID2WGSContext(void)
    : m_CompressData(eCompressData_never),
      m_ExplicitBlobState(false),
      m_AllowVDB(false)
{
}


bool CID2WGSContext::operator<(const CID2WGSContext& b) const
{
    if ( m_CompressData != b.m_CompressData ) {
        return m_CompressData < b.m_CompressData;
    }
    if ( m_ExplicitBlobState != b.m_ExplicitBlobState ) {
        return m_ExplicitBlobState < b.m_ExplicitBlobState;
    }
    if ( m_AllowVDB != b.m_AllowVDB ) {
        return m_AllowVDB < b.m_AllowVDB;
    }
    return false;
}


bool CID2WGSContext::operator==(const CID2WGSContext& b) const
{
    return m_CompressData == b.m_CompressData &&
        m_ExplicitBlobState == b.m_ExplicitBlobState &&
        m_AllowVDB == b.m_AllowVDB;
}


CID2WGSProcessor_Impl::CID2WGSProcessor_Impl(const CConfig::TParamTree* params,
                                             const string& driver_name)
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
                    NCBI_ID2PROC_WGS_PARAM_VDB_CACHE_SIZE,
                    CConfig::eErr_NoThrow,
                    DEFAULT_VDB_CACHE_SIZE);
    TRACE_X(23, eDebug_open, "ID2WGS: cache_size = "<<cache_size);
    m_WGSDbCache.set_size_limit(cache_size);

    int compress_data =
        conf.GetInt(driver_name,
                    NCBI_ID2PROC_WGS_PARAM_COMPRESS_DATA,
                    CConfig::eErr_NoThrow,
                    DEFAULT_COMPRESS_DATA);
    if ( compress_data >= CID2WGSContext::eCompressData_never &&
         compress_data <= CID2WGSContext::eCompressData_always ) {
        m_InitialContext.m_CompressData =
            CID2WGSContext::ECompressData(compress_data);
    }
    TRACE_X(23, eDebug_open, "ID2WGS: compress_data = "<<
            m_InitialContext.m_CompressData);

    m_UpdateDelay =
        conf.GetInt(driver_name,
                    NCBI_ID2PROC_WGS_PARAM_INDEX_UPDATE_TIME,
                    CConfig::eErr_NoThrow,
                    DEFAULT_INDEX_UPDATE_TIME);
    TRACE_X(24, eDebug_open, "ID2WGS: index_update_time = "<<m_UpdateDelay);
}


CRef<CWGSResolver>
CID2WGSProcessor_Impl::GetWGSResolver(CID2ProcessorResolver* resolver)
{
    if ( resolver ) {
        return CWGSResolver::CreateResolver(resolver);
    }
    if ( !m_Resolver ) {
        CMutexGuard guard(m_Mutex);
        if ( !m_Resolver ) {
            m_Resolver = CWGSResolver::CreateResolver(m_Mgr);
            m_UpdateThread = new CIndexUpdateThread(m_UpdateDelay, m_Resolver);
            m_UpdateThread->Run();
        }
    }
    return m_Resolver;
}


CID2WGSProcessor_Impl::~CID2WGSProcessor_Impl(void)
{
    if ( m_UpdateThread ) {
        m_UpdateThread->RequestStop();
        m_UpdateThread->Join();
    }
}


void CID2WGSProcessor_Impl::InitContext(CID2WGSContext& context,
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
                    if ( *it2 == "vdb-wgs" ) {
                        context.m_AllowVDB = true;
                    }
                }
            }
        }
    }
}


CWGSDb CID2WGSProcessor_Impl::GetWGSDb(const string& prefix)
{
    CMutexGuard guard(m_Mutex);
    TWGSDbCache::iterator it = m_WGSDbCache.find(prefix);
    if ( it != m_WGSDbCache.end() ) {
        return it->second;
    }
    try {
        CWGSDb wgs_db(m_Mgr, prefix);
        //wgs_db.LoadMasterDescr();
        m_WGSDbCache[prefix] = wgs_db;
        TRACE_X(1, eDebug_open, "GetWGSDb: "<<prefix);
        return wgs_db;
    }
    catch ( CSraException& exc ) {
        if ( exc.GetErrCode() == exc.eNotFoundDb ||
             exc.GetErrCode() == exc.eProtectedDb ) {
            // no such WGS table
        }
        else {
            TRACE_X(22, eDebug_error, "ID2WGS: "
                    "Exception while opening WGS DB "<<prefix<<": "<<exc);
        }
    }
    catch ( CException& exc ) {
        TRACE_X(22, eDebug_error, "ID2WGS: "
                "Exception while opening WGS DB "<<prefix<<": "<<exc);
    }
    catch ( exception& exc ) {
        TRACE_X(22, eDebug_error, "ID2WGS: "
                "Exception while opening WGS DB "<<prefix<<": "<<exc.what());
    }
    return CWGSDb();
}


CWGSDb& CID2WGSProcessor_Impl::GetWGSDb(SWGSSeqInfo& seq)
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


void CID2WGSProcessor_Impl::ResetIteratorCache(SWGSSeqInfo& seq)
{
    seq.m_ContigIter.Reset();
    seq.m_ScaffoldIter.Reset();
    seq.m_ProteinIter.Reset();
    seq.m_BlobId.Reset();
}


CWGSSeqIterator&
CID2WGSProcessor_Impl::GetContigIterator(SWGSSeqInfo& seq)
{
    if ( !seq.m_ContigIter ) {
        seq.m_ContigIter = CWGSSeqIterator(GetWGSDb(seq), seq.m_RowId,
                                           CWGSSeqIterator::eExcludeWithdrawn);
    }
    return seq.m_ContigIter;
}


CWGSScaffoldIterator&
CID2WGSProcessor_Impl::GetScaffoldIterator(SWGSSeqInfo& seq)
{
    if ( !seq.m_ScaffoldIter ) {
        seq.m_ScaffoldIter = CWGSScaffoldIterator(GetWGSDb(seq), seq.m_RowId);
    }
    return seq.m_ScaffoldIter;
}


CWGSProteinIterator&
CID2WGSProcessor_Impl::GetProteinIterator(SWGSSeqInfo& seq)
{
    if ( !seq.m_ProteinIter ) {
        seq.m_ProteinIter = CWGSProteinIterator(GetWGSDb(seq), seq.m_RowId);
    }
    return seq.m_ProteinIter;
}


bool CID2WGSProcessor_Impl::IsValidRowId(SWGSSeqInfo& seq)
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


bool CID2WGSProcessor_Impl::IsCorrectVersion(SWGSSeqInfo& seq, int version)
{
    if ( !seq ) {
        return false;
    }
    if ( seq.IsContig() ) {
        CWGSSeqIterator& it = GetContigIterator(seq);
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
// sat = 1000 : WGS
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

CID2_Blob_Id& CID2WGSProcessor_Impl::GetBlobId(SWGSSeqInfo& seq0)
{
    if ( seq0.m_BlobId ) {
        return *seq0.m_BlobId;
    }
    SWGSSeqInfo& seq = GetRootSeq(seq0);
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
    for ( size_t i = 0; i < seq.m_WGSAcc.size(); ++i ) {
        if ( i < kNumLetters ) {
            subsat |= (seq.m_WGSAcc[i]-'A') << bit;
            bit += eBlobIdBits_letter;
        }
        else {
            subsat |= (seq.m_WGSAcc[i]-'0') << bit;
            bit += eBlobIdBits_digit;
        }
    }
    id->SetSub_sat(subsat);
    id->SetSat_key(seq.m_RowId);
    seq0.m_BlobId = id;
    return *id;
}


CID2WGSProcessor_Impl::SWGSSeqInfo
CID2WGSProcessor_Impl::ResolveBlobId(const CID2_Blob_Id& id)
{
    if ( id.GetSat() != kBlobIdSat ) {
        return SWGSSeqInfo();
    }
    SWGSSeqInfo seq;
    seq.m_IsWGS = true;
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
                seq.m_WGSAcc += char('A'+v);
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
                seq.m_WGSAcc += char('0'+v);
            }
            else {
                bad = true;
                break;
            }
            bit += eBlobIdBits_digit;
        }
    }
    if ( !bad ) {
        if ( CWGSDb wgs_db = GetWGSDb(seq) ) {
            seq.m_ValidWGS = true;
            seq.m_RowId = id.GetSat_key();
        }
    }
    return seq;
}


CID2WGSProcessor_Impl::SWGSSeqInfo
CID2WGSProcessor_Impl::ResolveGeneral(const CDbtag& dbtag)
{
    const CObject_id& object_id = dbtag.GetTag();
    const string& db = dbtag.GetDb();
    if ( db.size() != kTypePrefixLen+kNumLetters /* WGS:AAAA */ &&
         db.size() != kTypePrefixLen+kPrefixLen  /* WGS:AAAA01 */ ) {
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
    if ( wgs_acc.size() == kNumLetters ) {
        wgs_acc += "01"; // add default version digits
    }
    SWGSSeqInfo seq;
    seq.m_WGSAcc = wgs_acc;
    seq.m_IsWGS = true;
    CWGSDb wgs_db = GetWGSDb(seq);
    if ( !wgs_db || wgs_db->IsTSA() != is_tsa ) {
        // TSA or WGS type must match
        return seq;
    }
    string tag;
    if ( object_id.IsStr() ) {
        tag = object_id.GetStr();
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


CID2WGSProcessor_Impl::SWGSSeqInfo
CID2WGSProcessor_Impl::ResolveGi(TGi gi,
                                 CID2ProcessorResolver* resolver)
{
    CRef<CWGSResolver> wgs_resolver = GetWGSResolver(resolver);
    CWGSResolver::TWGSPrefixes prefixes = wgs_resolver->GetPrefixes(gi);
    ITERATE ( CWGSResolver::TWGSPrefixes, it, prefixes ) {
        if ( CWGSDb wgs_db = GetWGSDb(*it) ) {
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
                if ( gi_it.GetSeqType() == gi_it.eProt ) {
                    seq.SetProtein();
                }
                else {
                    seq.SetContig();
                }
                seq.m_RowDigits = Uint1(wgs_db->GetIdRowDigits());
                seq.m_RowId = gi_it.GetRowId();
                return seq;
            }
        }
    }
    if ( !prefixes.empty() ) {
        wgs_resolver->SetNonWGS(gi, prefixes);
    }
    return SWGSSeqInfo();
}


CID2WGSProcessor_Impl::SWGSSeqInfo
CID2WGSProcessor_Impl::ResolveProtAcc(const CTextseq_id& id,
                                      CID2ProcessorResolver* resolver)
{
    CRef<CWGSResolver> wgs_resolver = GetWGSResolver(resolver);
    const string& acc = id.GetAccession();
    CWGSResolver::TWGSPrefixes prefixes = wgs_resolver->GetPrefixes(acc);
    ITERATE ( CWGSResolver::TWGSPrefixes, it, prefixes ) {
        if ( CWGSDb wgs_db = GetWGSDb(*it) ) {
            if ( TVDBRowId row = wgs_db.GetProtAccRowId(acc) ) {
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


CID2WGSProcessor_Impl::SWGSSeqInfo
CID2WGSProcessor_Impl::ResolveWGSAcc(const string& acc,
                                     const CTextseq_id& id,
                                     TAllowSeqType allow_seq_type)
{
    if ( acc.size() < kPrefixLen + kMinRowDigits ||
         acc.size() > kPrefixLen + kMaxRowDigits + 1 ) { // one for type letter
        return SWGSSeqInfo();
    }
    for ( size_t i = 0; i < kNumLetters; ++i ) {
        if ( !isalpha(acc[i]&0xff) ) {
            return SWGSSeqInfo();
        }
    }
    for ( size_t i = kNumLetters; i < kPrefixLen; ++i ) {
        if ( !isdigit(acc[i]&0xff) ) {
            return SWGSSeqInfo();
        }
    }
    SWGSSeqInfo seq;
    seq.m_WGSAcc = acc.substr(0, kPrefixLen);
    seq.m_IsWGS = true;
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
            if ( acc[kNumLetters+i] != '0' ) {
                return SWGSSeqInfo();
            }
            seq.m_WGSAcc[kNumLetters+i] = char('0'+version%10);
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
    if ( !GetWGSDb(seq) ) {
        return seq;
    }
    SIZE_TYPE row_digits = acc.size() - row_pos;
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
            if ( GetWGSDb(seq)->GetMasterGi() ) {
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


CID2WGSProcessor_Impl::SWGSSeqInfo
CID2WGSProcessor_Impl::ResolveAcc(const CTextseq_id& id,
                                  CID2ProcessorResolver* resolver)
{
    if ( id.IsSetName() ) {
        // first try name reference if it has WGS format like AAAA01P000001
        // as it directly contains WGS accession
        if ( SWGSSeqInfo seq = ResolveWGSAcc(id.GetName(), id, fAllow_aa) ) {
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
        if ( type & CSeq_id::fAcc_prot ) {
            if ( acc.size() <= 8 ) { // 3+5
                return ResolveProtAcc(id, resolver);
            }
            return SWGSSeqInfo();
        }
        else {
            return ResolveWGSAcc(acc, id, fAllow_master|fAllow_na);
        }
    default:
        // non-WGS accessions
        return SWGSSeqInfo();
    }
}


CID2WGSProcessor_Impl::SWGSSeqInfo&
CID2WGSProcessor_Impl::GetRootSeq(SWGSSeqInfo& seq0)
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
    TVDBRowId cds_row_id = GetProteinIterator(seq0).GetProductFeatRowId();
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


CID2WGSProcessor_Impl::SWGSSeqInfo
CID2WGSProcessor_Impl::Resolve(const CSeq_id& id,
                               CID2ProcessorResolver* resolver)
{
    if ( !s_Enabled() ) {
        return SWGSSeqInfo();
    }
    switch ( id.Which() ) {
    case CSeq_id::e_Gi:
        return ResolveGi(id.GetGi(), resolver);
    case CSeq_id::e_General:
        return ResolveGeneral(id.GetGeneral());
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
    SWGSSeqInfo seq = ResolveAcc(*text_id, resolver);
    if ( !seq ) {
        return seq;
    }
    if ( text_id->IsSetVersion() &&
         !IsCorrectVersion(seq, text_id->GetVersion()) ) {
        return seq;
    }
    seq.m_ValidWGS = true;
    return seq;
}


CRef<CSeq_id> CID2WGSProcessor_Impl::GetAccVer(SWGSSeqInfo& seq)
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


CRef<CSeq_id> CID2WGSProcessor_Impl::GetGeneral(SWGSSeqInfo& seq)
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


TGi CID2WGSProcessor_Impl::GetGi(SWGSSeqInfo& seq)
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
        // proteins have no GIs
        return ZERO_GI;
    }
    // master
    return ZERO_GI;
}


void CID2WGSProcessor_Impl::GetSeqIds(SWGSSeqInfo& seq,
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


string CID2WGSProcessor_Impl::GetLabel(SWGSSeqInfo& seq)
{
    return objects::GetLabel(*GetAccVer(seq));
}


int CID2WGSProcessor_Impl::GetTaxId(SWGSSeqInfo& seq)
{
    if ( !seq ) {
        return -1;
    }
    if ( seq.IsContig() ) {
        CWGSSeqIterator it = GetContigIterator(seq);
        return it.HasTaxId()? it.GetTaxId(): 0;
    }
    return 0; // taxid is not defined
}


int CID2WGSProcessor_Impl::GetHash(SWGSSeqInfo& seq)
{
    if ( !seq ) {
        return 0;
    }
    if ( seq.IsContig() ) {
        return GetContigIterator(seq).GetSeqHash();
    }
    return 0;
}


TSeqPos CID2WGSProcessor_Impl::GetLength(SWGSSeqInfo& seq)
{
    if ( !seq ) {
        return kInvalidSeqPos;
    }
    if ( seq.IsContig() ) {
        return GetContigIterator(seq).GetSeqLength();
    }
    if ( seq.IsScaffold() ) {
        return GetScaffoldIterator(seq).GetSeqLength();
    }
    if ( seq.IsProtein() ) {
        return GetProteinIterator(seq).GetSeqLength();
    }
    return kInvalidSeqPos;
}


CSeq_inst::TMol CID2WGSProcessor_Impl::GetType(SWGSSeqInfo& seq)
{
    if ( !seq ) {
        return CSeq_inst::eMol_not_set;
    }
    if ( seq.IsContig() ) {
        return CSeq_inst::eMol_na;
    }
    if ( seq.IsScaffold() ) {
        return CSeq_inst::eMol_na;
    }
    if ( seq.IsProtein() ) {
        return CSeq_inst::eMol_aa;
    }
    return CSeq_inst::eMol_not_set;
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


CID2WGSProcessor_Impl::SWGSSeqInfo
CID2WGSProcessor_Impl::Resolve(TReplies& replies,
                               CID2_Request& main_request,
                               CID2ProcessorResolver* resolver,
                               CID2_Request_Get_Seq_id& request)
{
    if ( !request.GetSeq_id().IsSeq_id() ) {
        return SWGSSeqInfo();
    }
    SWGSSeqInfo seq = Resolve(request.GetSeq_id().GetSeq_id(), resolver);
    if ( !seq ) {
        if ( seq.m_IsWGS && !s_FilterAll () ) {
            seq.m_IsWGS = false;
        }
        if ( seq.m_IsWGS ) {
            // WGS request for inexistent data
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
    if ( request.GetSeq_id_type() == request.eSeq_id_type_any ) {
        if ( CRef<CSeq_id> id = GetAccVer(seq) ) {
            ids.push_back(id);
        }
        else if ( TGi gi = GetGi(seq) ) {
            CRef<CSeq_id> gi_id(new CSeq_id);
            gi_id->SetGi(gi);
            ids.push_back(gi_id);
        }
        else if ( CRef<CSeq_id> id = GetGeneral(seq) ) {
            ids.push_back(id);
        }
    }
    if ( request.GetSeq_id_type() & request.eSeq_id_type_text ) {
        if ( CRef<CSeq_id> id = GetAccVer(seq) ) {
            ids.push_back(id);
        }
    }
    if ( request.GetSeq_id_type() & request.eSeq_id_type_gi ) {
        if ( TGi gi = GetGi(seq) ) {
            CRef<CSeq_id> gi_id(new CSeq_id);
            gi_id->SetGi(gi);
            ids.push_back(gi_id);
        }
    }
    if ( request.GetSeq_id_type() & request.eSeq_id_type_general ) {
        if ( CRef<CSeq_id> id = GetGeneral(seq) ) {
            ids.push_back(id);
        }
    }
    if ( request.GetSeq_id_type() & request.eSeq_id_type_label ) {
        s_AddSpecialId(ids, "LABEL", GetLabel(seq));
    }
    if ( request.GetSeq_id_type() & request.eSeq_id_type_taxid ) {
        s_AddSpecialId(ids, "TAXID", GetTaxId(seq));
    }
    if ( request.GetSeq_id_type() & request.eSeq_id_type_hash ) {
        s_AddSpecialId(ids, "HASH", GetHash(seq));
    }
    if ( request.GetSeq_id_type() & request.eSeq_id_type_seq_length ) {
        s_AddSpecialId(ids, "Seq-inst.length", GetLength(seq));
    }
    if ( request.GetSeq_id_type() & request.eSeq_id_type_seq_mol ) {
        s_AddSpecialId(ids, "Seq-inst.mol", GetType(seq));
    }
    reply.SetEnd_of_reply();
    replies.push_back(main_reply);
    TRACE_X(3, eDebug_replies, "GetSeqId: "<<MSerial_AsnText<<*main_reply);
    return seq;
}


bool CID2WGSProcessor_Impl::ProcessGetSeqId(CID2WGSContext& context,
                                            TReplies& replies,
                                            CID2_Request& main_request,
                                            CID2ProcessorResolver* resolver,
                                            CID2_Request_Get_Seq_id& request)
{
    START_TRACE();
    TRACE_X(4, eDebug_request, "GetSeqId: "<<MSerial_AsnText<<main_request);
    SWGSSeqInfo seq = Resolve(replies, main_request, resolver, request);
    TRACE_X(5, eDebug_resolve, "GetSeqId: done");
    return seq || seq.m_IsWGS;
}


CID2WGSProcessor_Impl::SWGSSeqInfo
CID2WGSProcessor_Impl::Resolve(CID2WGSContext& context,
                               TReplies& replies,
                               CID2_Request& main_request,
                               CID2ProcessorResolver* resolver,
                               CID2_Request_Get_Blob_Id& request)
{
    SWGSSeqInfo seq = Resolve(replies, main_request, resolver, request.SetSeq_id());
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
        SetBlobState(context, *main_reply, blob_state);
    }
    reply.SetEnd_of_reply();
    replies.push_back(main_reply);
    TRACE_X(6, eDebug_replies, "GetBlobId: "<<MSerial_AsnText<<*main_reply);
    return seq;
}


bool CID2WGSProcessor_Impl::ProcessGetBlobId(CID2WGSContext& context,
                                             TReplies& replies,
                                             CID2_Request& main_request,
                                             CID2ProcessorResolver* resolver,
                                             CID2_Request_Get_Blob_Id& request)
{
    START_TRACE();
    TRACE_X(7, eDebug_request, "GetBlobId: "<<MSerial_AsnText<<main_request);
    SWGSSeqInfo seq = Resolve(context, replies, main_request, resolver, request);
    TRACE_X(8, eDebug_resolve, "GetBlobId: done");
    return seq || seq.m_IsWGS;
}


namespace {
};


NCBI_gb_state CID2WGSProcessor_Impl::GetGBState(SWGSSeqInfo& seq0)
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


CBioseq_Handle::TBioseqStateFlags
CID2WGSProcessor_Impl::GetBioseqState(SWGSSeqInfo& seq)
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


int CID2WGSProcessor_Impl::GetID2BlobState(SWGSSeqInfo& seq)
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


void CID2WGSProcessor_Impl::SetBlobState(CID2WGSContext& context,
                                         CID2_Reply& main_reply,
                                         int blob_state) const
{
    if ( !blob_state ) {
        return;
    }
    if ( context.m_ExplicitBlobState ) {
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


CRef<CAsnBinData> CID2WGSProcessor_Impl::GetObject(SWGSSeqInfo& seq0)
{
    SWGSSeqInfo& seq = GetRootSeq(seq0);
    if ( seq.IsMaster() ) {
        return Ref(new CAsnBinData(*GetWGSDb(seq)->GetMasterSeq_entry()));
    }
    if ( seq.IsContig() ) {
        CWGSSeqIterator& it = GetContigIterator(seq);
        CWGSSeqIterator::TFlags flags =
            (it.fDefaultFlags & ~it.fMaskDescr) | it.fSeqDescr;
        CRef<CAsnBinData> obj;
#ifdef ALLOW_SPLIT
        obj = it.GetSplitInfoData(flags);
#endif
        if ( !obj ) {
            obj = it.GetSeq_entryData(flags);
        }
        if ( !obj ) {
            obj = new CAsnBinData(*it.GetSeq_entry(flags));
        }
        return obj;
    }
    if ( seq.IsScaffold() ) {
        CWGSScaffoldIterator& it = GetScaffoldIterator(seq);
        CWGSScaffoldIterator::TFlags flags =
            (it.fDefaultFlags & ~it.fMaskDescr) | it.fSeqDescr;
        return Ref(new CAsnBinData(*it.GetSeq_entry(flags)));
    }
    if ( seq.IsProtein() ) {
        CWGSProteinIterator& it = GetProteinIterator(seq);
        CWGSProteinIterator::TFlags flags =
            (it.fDefaultFlags & ~it.fMaskDescr) | it.fSeqDescr;
        return Ref(new CAsnBinData(*it.GetSeq_entry(flags)));
    }
    return null;
}


CRef<CAsnBinData> CID2WGSProcessor_Impl::GetChunk(SWGSSeqInfo& seq0,
                                                  TChunkId chunk_id)
{
    SWGSSeqInfo& seq = GetRootSeq(seq0);
    if ( seq.IsContig() ) {
        CWGSSeqIterator& it = GetContigIterator(seq);
        CWGSSeqIterator::TFlags flags =
            (it.fDefaultFlags & ~it.fMaskDescr) | it.fSeqDescr;
        return it.GetChunkData(chunk_id, flags);
    }
    return null;
}


bool CID2WGSProcessor_Impl::GetCompress(CID2WGSContext& context,
                                        const SWGSSeqInfo& seq,
                                        const CSeq_entry& entry) const
{
    if ( context.m_CompressData == context.eCompressData_always ) {
        return true;
    }
    if ( context.m_CompressData == context.eCompressData_never ) {
        return false;
    }
    if ( seq.IsMaster() ) {
        return true;
    }
    if ( 0 && entry.IsSet() ) {
        return true;
    }
    return false;
}


bool CID2WGSProcessor_Impl::GetCompress(CID2WGSContext& context,
                                        const SWGSSeqInfo& seq,
                                        const CID2S_Split_Info& split) const
{
    if ( context.m_CompressData == context.eCompressData_always ) {
        return true;
    }
    if ( context.m_CompressData == context.eCompressData_never ) {
        return false;
    }
    return true;
}


bool CID2WGSProcessor_Impl::GetCompress(CID2WGSContext& context,
                                        const SWGSSeqInfo& seq,
                                        TChunkId chunk_id,
                                        const CID2S_Chunk& chunk) const
{
    if ( context.m_CompressData == context.eCompressData_always ) {
        return true;
    }
    if ( context.m_CompressData == context.eCompressData_never ) {
        return false;
    }
    return false;
}


bool CID2WGSProcessor_Impl::GetCompress(CID2WGSContext& context,
                                        const SWGSSeqInfo& seq,
                                        TChunkId chunk_id,
                                        const CAsnBinData& obj) const
{
    const CSerialObject* ptr = &obj.GetMainObject();
    {
        typedef CSeq_entry Type;
        if ( const Type* ptr2 = dynamic_cast<const Type*>(ptr) ) {
            return GetCompress(context, seq, *ptr2);
        }
    }
    {
        typedef CID2S_Split_Info Type;
        if ( const Type* ptr2 = dynamic_cast<const Type*>(ptr) ) {
            return GetCompress(context, seq, *ptr2);
        }
    }
    {
        typedef CID2S_Chunk Type;
        if ( const Type* ptr2 = dynamic_cast<const Type*>(ptr) ) {
            return GetCompress(context, seq, chunk_id, *ptr2);
        }
    }
    return false;
}


void CID2WGSProcessor_Impl::WriteData(CID2_Reply_Data& data,
                                      const CSerialObject& obj,
                                      bool compress) const
{
    data.SetData_format(CID2_Reply_Data::eData_format_asn_binary);
    COSSWriter writer(data.SetData());
    CWStream writer_stream(&writer);
    AutoPtr<CNcbiOstream> str;
    if ( compress ) {
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


void CID2WGSProcessor_Impl::WriteData(CID2_Reply_Data& data,
                                      const CAsnBinData& obj,
                                      bool compress) const
{
    data.SetData_format(CID2_Reply_Data::eData_format_asn_binary);
    COSSWriter writer(data.SetData());
    CWStream writer_stream(&writer);
    AutoPtr<CNcbiOstream> str;
    if ( compress ) {
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
    obj.Serialize(objstr);
}


void CID2WGSProcessor_Impl::WriteData(CID2WGSContext& context,
                                      CID2_Reply_Data& data,
                                      const SWGSSeqInfo& seq,
                                      TChunkId chunk_id,
                                      const CAsnBinData& obj) const
{
    WriteData(data, obj, GetCompress(context, seq, chunk_id, obj));
}


void CID2WGSProcessor_Impl::WriteData(CID2WGSContext& context,
                                      CID2_Reply_Data& data,
                                      const SWGSSeqInfo& seq,
                                      const CSeq_entry& obj) const
{
    WriteData(data, obj, GetCompress(context, seq, obj));
}


void CID2WGSProcessor_Impl::WriteData(CID2WGSContext& context,
                                      CID2_Reply_Data& data,
                                      const SWGSSeqInfo& seq,
                                      const CID2S_Split_Info& obj) const
{
    WriteData(data, obj, GetCompress(context, seq, obj));
}


void CID2WGSProcessor_Impl::WriteData(CID2WGSContext& context,
                                      CID2_Reply_Data& data,
                                      const SWGSSeqInfo& seq,
                                      TChunkId chunk_id,
                                      const CID2S_Chunk& obj) const
{
    WriteData(data, obj, GetCompress(context, seq, chunk_id, obj));
}


bool CID2WGSProcessor_Impl::ExcludedBlob(SWGSSeqInfo& seq,
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


bool CID2WGSProcessor_Impl::ProcessGetBlobInfo(CID2WGSContext& context,
                                               TReplies& replies,
                                               CID2_Request& main_request,
                                               CID2ProcessorResolver* resolver,
                                               CID2_Request_Get_Blob_Info& request)
{
    START_TRACE();
    TRACE_X(9, eDebug_request, "GetBlobInfo: "<<MSerial_AsnText<<main_request);
    SWGSSeqInfo seq;
    CID2_Request_Get_Blob_Info::TBlob_id& req_id = request.SetBlob_id();
    if ( req_id.IsBlob_id() ) {
        seq = ResolveBlobId(req_id.GetBlob_id());
    }
    else {
        seq = Resolve(context, replies, main_request, resolver,
                      req_id.SetResolve().SetRequest());
    }
    if ( !seq ) {
        TRACE_X(10, eDebug_resolve, "GetBlobInfo: null");
        return seq.m_IsWGS;
    }

    if ( request.IsSetGet_data() && !ExcludedBlob(seq, request) ) {
        CRef<CID2_Reply> main_reply(new CID2_Reply);
        if ( main_request.IsSetSerial_number() ) {
            main_reply->SetSerial_number(main_request.GetSerial_number());
        }
        CID2_Reply_Get_Blob& reply = main_reply->SetReply().SetGet_blob();
        reply.SetBlob_id(GetBlobId(seq));
        if ( int blob_state = GetID2BlobState(seq) ) {
            SetBlobState(context, *main_reply, blob_state);
        }
        CID2_Reply_Data& data = reply.SetData();

        CRef<CAsnBinData> obj = GetObject(seq);
        if ( obj->GetMainObject().GetThisTypeInfo() == CID2S_Split_Info::GetTypeInfo() ) {
            // split info
            TRACE_X(11, eDebug_resolve, "GetSplitInfo: "<<seq);
            data.SetData_type(CID2_Reply_Data::eData_type_id2s_split_info);
        }
        else {
            TRACE_X(11, eDebug_resolve, "GetSeq_entry: "<<seq);
            data.SetData_type(CID2_Reply_Data::eData_type_seq_entry);
        }
        WriteData(context, data, seq, 0, *obj);
        TRACE_X(12, eDebug_resolve, "Seq("<<seq<<"): "<<
                " data size: "<<sx_GetSize(data));
        replies.push_back(main_reply);
    }
    TRACE_X(14, eDebug_resolve,"GetBlobInfo: done");
    return true;
}


bool CID2WGSProcessor_Impl::ProcessGetChunks(CID2WGSContext& context,
                                             TReplies& replies,
                                             CID2_Request& main_request,
                                             CID2ProcessorResolver* resolver,
                                             CID2S_Request_Get_Chunks& request)
{
    START_TRACE();
    TRACE_X(9, eDebug_request, "GetChunks: "<<MSerial_AsnText<<main_request);
    SWGSSeqInfo seq = ResolveBlobId(request.SetBlob_id());
    if ( !seq ) {
        TRACE_X(10, eDebug_resolve, "GetChunks: null");
        return seq.m_IsWGS;
    }

    ITERATE ( CID2S_Request_Get_Chunks::TChunks, it, request.GetChunks() ) {
        CRef<CID2_Reply> main_reply(new CID2_Reply);
        if ( main_request.IsSetSerial_number() ) {
            main_reply->SetSerial_number(main_request.GetSerial_number());
        }
        CID2S_Reply_Get_Chunk& reply = main_reply->SetReply().SetGet_chunk();
        reply.SetBlob_id(GetBlobId(seq));
        reply.SetChunk_id(*it);
        CID2_Reply_Data& data = reply.SetData();

        CRef<CAsnBinData> obj = GetChunk(seq, *it);
        _ASSERT(obj->GetMainObject().GetThisTypeInfo() == CID2S_Chunk::GetTypeInfo());
        TRACE_X(11, eDebug_resolve, "GetChunk: "<<seq);
        data.SetData_type(CID2_Reply_Data::eData_type_id2s_chunk);
        WriteData(context, data, seq, *it, *obj);
        TRACE_X(12, eDebug_resolve, "Seq("<<seq<<"): "<<
                " data size: "<<sx_GetSize(data));
        replies.push_back(main_reply);
    }
    TRACE_X(14, eDebug_resolve,"GetChunks: done");
    return true;
}


bool CID2WGSProcessor_Impl::ProcessRequest(CID2WGSContext& context,
                                           TReplies& replies,
                                           CID2_Request& request,
                                           CID2ProcessorResolver* resolver)
{
    if ( !s_Enabled() ) {
        return false;
    }
    if ( request.GetRequest().IsInit() ) {
        InitContext(context, request);
        return false;
    }
    if ( !context.m_AllowVDB ) {
        return false;
    }
    size_t old_size = replies.size();
    bool done;
    try {
        switch ( request.GetRequest().Which() ) {
        case CID2_Request::TRequest::e_Get_seq_id:
            done = ProcessGetSeqId(context, replies, request, resolver,
                                   request.SetRequest().SetGet_seq_id());
            break;
        case CID2_Request::TRequest::e_Get_blob_id:
            done = ProcessGetBlobId(context, replies, request, resolver,
                                    request.SetRequest().SetGet_blob_id());
            break;
        case CID2_Request::TRequest::e_Get_blob_info:
            done = ProcessGetBlobInfo(context, replies, request, resolver,
                                      request.SetRequest().SetGet_blob_info());
            break;
        case CID2_Request::TRequest::e_Get_chunks:
            done = ProcessGetChunks(context, replies, request, resolver,
                                    request.SetRequest().SetGet_chunks());
            break;
        default:
            return false;
        }
    }
    catch ( CException& exc ) {
        if ( s_DebugEnabled(eDebug_error) ) {
            ERR_POST_X(16, "ID2WGS: Exception while processing request "<<
                       MSerial_AsnText<<request<<": "<<exc);
        }
        done = false;
    }
    catch ( exception& exc ) {
        if ( s_DebugEnabled(eDebug_error) ) {
            ERR_POST_X(16, "ID2WGS: Exception while processing request "<<
                       MSerial_AsnText<<request<<": "<<exc.what());
        }
        done = false;
    }
    if ( !done ) {
        if ( replies.size() != old_size ) {
            if ( s_DebugEnabled(eDebug_error) ) {
                ERR_POST_X(17, "ID2WGS: bad replies for request "<<
                           MSerial_AsnText<<request);
            }
            replies.resize(old_size);
        }
        return false;
    }
    if ( replies.size() == old_size ) {
        if ( s_DebugEnabled(eDebug_error) ) {
            ERR_POST_X(17, "ID2WGS: no replies for request "<<
                       MSerial_AsnText<<request);
        }
        return false;
    }
    replies.back()->SetEnd_of_reply();
    return true;
}


CID2WGSProcessor_Impl::TReplies
CID2WGSProcessor_Impl::ProcessSomeRequests(CID2WGSContext& context,
                                           CID2_Request_Packet& packet,
                                           CID2ProcessorResolver* resolver)
{
    TReplies replies;
    ERASE_ITERATE ( CID2_Request_Packet::Tdata, it, packet.Set() ) {
        if ( ProcessRequest(context, replies, **it, resolver) ) {
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
