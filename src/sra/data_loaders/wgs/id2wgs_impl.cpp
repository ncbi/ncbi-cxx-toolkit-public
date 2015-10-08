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
#include <sra/error_codes.hpp>
#include <corelib/reader_writer.hpp>
#include <corelib/rwstream.hpp>
#include <util/thread_nonstop.hpp>
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

#define NCBI_USE_ERRCODE_X   ID2WGSProcessor
NCBI_DEFINE_ERR_SUBCODE_X(24);

BEGIN_NAMESPACE(objects);


#define DEFAULT_VDB_CACHE_SIZE 10
#define DEFAULT_INDEX_UPDATE_TIME 600
#define DEFAULT_COMPRESS_DATA false


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


static const size_t kNumLetters = 4;
static const size_t kVersionDigits = 2;
static const size_t kPrefixLen = kNumLetters + kVersionDigits;
static const size_t kMinRowDigits = 6;
static const size_t kMaxRowDigits = 8;


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

#define TRACE_PROCESSING

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
        if ( !seq.IsContig() ) {
            out << seq.m_SeqType;
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
                       CRef<CWGSGiResolver> gi_resolver,
                       CRef<CWGSProtAccResolver> acc_resolver)
        : CThreadNonStop(update_delay),
          m_GiResolver(gi_resolver),
          m_AccResolver(acc_resolver)
        {
        }

protected:
    virtual void DoJob(void) {
        try {
            if ( m_GiResolver->Update() ) {
                if ( s_DebugEnabled(eDebug_open) ) {
                    LOG_POST_X(18, Info<<"ID2WGS: updated GI index");
                }
            }
        }
        catch ( CException& exc ) {
            if ( s_DebugEnabled(eDebug_error) ) {
                ERR_POST_X(20, "ID2WGS: "
                           "Exception while updating GI index: "<<exc);
            }
        }
        catch ( exception& exc ) {
            if ( s_DebugEnabled(eDebug_error) ) {
                ERR_POST_X(20, "ID2WGS: "
                           "Exception while updating GI index: "<<exc.what());
            }
        }
        try {
            if ( m_AccResolver->Update() ) {
                if ( s_DebugEnabled(eDebug_open) ) {
                    LOG_POST_X(19, Info<<"ID2WGS: updated ACC index");
                }
            }
        }
        catch ( CException& exc ) {
            if ( s_DebugEnabled(eDebug_error) ) {
                ERR_POST_X(21, "ID2WGS: "
                           "Exception while updating ACC index: "<<exc);
            }
        }
        catch ( exception& exc ) {
            if ( s_DebugEnabled(eDebug_error) ) {
                ERR_POST_X(21, "ID2WGS: "
                           "Exception while updating ACC index: "<<exc.what());
            }
        }
    }

private:
    CRef<CWGSGiResolver> m_GiResolver;
    CRef<CWGSProtAccResolver> m_AccResolver;
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


CID2WGSProcessor_Impl::CID2WGSProcessor_Impl(const CConfig::TParamTree* params,
                                             const string& driver_name)
    : m_GiResolver(new CWGSGiResolver),
      m_AccResolver(new CWGSProtAccResolver),
      m_CompressData(false)
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

    unsigned update_delay =
        conf.GetInt(driver_name,
                    NCBI_ID2PROC_WGS_PARAM_INDEX_UPDATE_TIME,
                    CConfig::eErr_NoThrow,
                    DEFAULT_INDEX_UPDATE_TIME);
    TRACE_X(24, eDebug_open, "ID2WGS: index_update_time = "<<update_delay);
    m_UpdateThread = new CIndexUpdateThread(update_delay,
                                            m_GiResolver, m_AccResolver);
    m_UpdateThread->Run();

    m_CompressData =
        conf.GetBool(driver_name,
                     NCBI_ID2PROC_WGS_PARAM_COMPRESS_DATA,
                     CConfig::eErr_NoThrow,
                     DEFAULT_COMPRESS_DATA);
    TRACE_X(23, eDebug_open, "ID2WGS: compress_data = "<<m_CompressData);
}


CID2WGSProcessor_Impl::~CID2WGSProcessor_Impl(void)
{
    m_UpdateThread->RequestStop();
    m_UpdateThread->Join();
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
        wgs_db.LoadMasterDescr();
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
    seq.m_ContigIter = CWGSSeqIterator();
    seq.m_ScaffoldIter = CWGSScaffoldIterator();
    seq.m_ProteinIter = CWGSProteinIterator();
}


CWGSSeqIterator& CID2WGSProcessor_Impl::GetContigIterator(SWGSSeqInfo& seq)
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


CRef<CID2_Blob_Id> CID2WGSProcessor_Impl::GetBlobId(SWGSSeqInfo& seq)
{
    CRef<CID2_Blob_Id> id(new CID2_Blob_Id);
    id->SetSat(1000);
    int subsat;
    if ( seq.IsScaffold() ) {
        subsat = 2;
    }
    else if ( seq.IsProtein() ) {
        subsat = 3;
    }
    else { // contig or master
        subsat = 1;
    }
    for ( size_t i = 0, bit = 2; i < seq.m_WGSAcc.size(); ++i ) {
        if ( i < kNumLetters ) {
            subsat |= (seq.m_WGSAcc[i]-'A') << bit;
            bit += 5;
        }
        else {
            subsat |= (seq.m_WGSAcc[i]-'0') << bit;
            bit += 4;
        }
    }
    id->SetSub_sat(subsat);
    id->SetSat_key(seq.m_RowId);
    return id;
}


CID2WGSProcessor_Impl::SWGSSeqInfo
CID2WGSProcessor_Impl::ResolveBlobId(const CID2_Blob_Id& id)
{
    if ( id.GetSat() != 1000 ) {
        return SWGSSeqInfo();
    }
    SWGSSeqInfo seq;
    seq.m_IsWGS = true;
    int subsat = id.GetSub_sat();
    switch ( subsat & 3 ) {
    case 2: seq.m_SeqType = 'S'; break;
    case 3: seq.m_SeqType = 'P'; break;
    default: seq.m_SeqType = '\0'; break;
    }
    bool bad = false;
    for ( size_t i = 0, bit = 2; i < kPrefixLen; ++i ) {
        if ( i < kNumLetters ) {
            int v = (subsat>>bit)&31;
            if ( v < 26 ) {
                seq.m_WGSAcc += char('A'+v);
            }
            else {
                bad = true;
                break;
            }
            bit += 5;
        }
        else {
            int v = (subsat>>bit)&15;
            if ( v < 10 ) {
                seq.m_WGSAcc += char('0'+v);
            }
            else {
                bad = true;
                break;
            }
            bit += 4;
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
    if ( !object_id.IsStr() ) {
        return SWGSSeqInfo();
    }
    const string& db = dbtag.GetDb();
    const string& tag = object_id.GetStr();
    if ( tag.empty() ||
         (db.size() != 4+4 /* WGS:AAAA */ &&
          db.size() != 4+6 /* WGS:AAAA01 */) ) {
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
    string wgs_acc = db.substr(4);
    NStr::ToUpper(wgs_acc);
    if ( wgs_acc.size() == 4 ) {
        wgs_acc += "01";
    }
    SWGSSeqInfo seq;
    seq.m_WGSAcc = wgs_acc;
    seq.m_IsWGS = true;
    if ( CWGSDb wgs_db = GetWGSDb(seq) ) {
        if ( wgs_db->IsTSA() == is_tsa ) {
            if ( uint64_t row = wgs_db.GetContigNameRowId(tag) ) {
                seq.m_ValidWGS = true;
                seq.m_SeqType = '\0';
                seq.m_RowId = row;
            }
        }
    }
    return seq;
}


CID2WGSProcessor_Impl::SWGSSeqInfo
CID2WGSProcessor_Impl::ResolveGi(TGi gi)
{
    CWGSGiResolver::TAccessionList accs = m_GiResolver->FindAll(gi);
    ITERATE ( CWGSGiResolver::TAccessionList, acc_it, accs ) {
        if ( CWGSDb wgs_db = GetWGSDb(*acc_it) ) {
            CWGSGiIterator it(wgs_db, gi);
            if ( it ) {
                SWGSSeqInfo seq;
                seq.m_WGSAcc = *acc_it;
                seq.m_IsWGS = true;
                seq.m_ValidWGS = true;
                seq.m_WGSDb = wgs_db;
                seq.m_SeqType = it.GetSeqType() == it.eProt? 'P': '\0';
                seq.m_RowDigits = Uint1(wgs_db->GetIdRowDigits());
                seq.m_RowId = it.GetRowId();
                return seq;
            }
        }
    }
    return SWGSSeqInfo();
}


CID2WGSProcessor_Impl::SWGSSeqInfo
CID2WGSProcessor_Impl::ResolveProtAcc(const string& acc, int version)
{
    CWGSProtAccResolver::TAccessionList accs = m_AccResolver->FindAll(acc);
    ITERATE ( CWGSProtAccResolver::TAccessionList, acc_it, accs ) {
        if ( CWGSDb wgs_db = GetWGSDb(*acc_it) ) {
            if ( uint64_t row = wgs_db.GetProtAccRowId(acc) ) {
                SWGSSeqInfo seq;
                seq.m_WGSAcc = *acc_it;
                seq.m_IsWGS = true;
                seq.m_ValidWGS = true;
                seq.m_WGSDb = wgs_db;
                seq.m_SeqType = 'P';
                seq.m_RowDigits = Uint1(wgs_db->GetIdRowDigits());
                seq.m_RowId = row;
                return seq;
            }
        }
    }
    return SWGSSeqInfo();
}


CID2WGSProcessor_Impl::SWGSSeqInfo
CID2WGSProcessor_Impl::ResolveWGSAcc(const string& acc, int version)
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
    if ( acc[row_pos] == 'S' || acc[row_pos] == 'P' ) { // type letter
        seq.m_SeqType = acc[row_pos++];
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
        if ( seq.m_SeqType || version < 0 ) {
            return SWGSSeqInfo();
        }
        // now, move version into version digits of the accession
        int t = version;
        for ( size_t i = kVersionDigits; i--; t /= 10) {
            if ( acc[kNumLetters+i] != '0' ) {
                return SWGSSeqInfo();
            }
            seq.m_WGSAcc[kNumLetters+i] = char('0'+t%10);
        }
        if ( t ) {
            // doesn't fit
            return SWGSSeqInfo();
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
        if ( false && GetWGSDb(seq)->GetMasterGi() ) {
            // Let master sequences with GI to be processed by ID
            seq.m_IsWGS = false;
            return seq;
        }
    }
    else if ( !IsValidRowId(seq) ) {
        return seq;
    }
    seq.m_ValidWGS = true;
    return seq;
}


CID2WGSProcessor_Impl::SWGSSeqInfo
CID2WGSProcessor_Impl::ResolveAcc(const string& acc, int version)
{
    CSeq_id::EAccessionInfo type = CSeq_id::IdentifyAccession(acc);
    switch ( type & CSeq_id::eAcc_division_mask ) {
        // accepted accession types
    case CSeq_id::eAcc_wgs:
    case CSeq_id::eAcc_wgs_intermed:
    case CSeq_id::eAcc_tsa:
        if ( type & CSeq_id::fAcc_prot ) {
            if ( acc.size() <= 10 ) { // 3+5
                return ResolveProtAcc(acc, version);
            }
            return SWGSSeqInfo();
        }
        else {
            return ResolveWGSAcc(acc, version);
        }
    default:
        // non-WGS accessions
        return SWGSSeqInfo();
    }
}


bool CID2WGSProcessor_Impl::SwitchToMainSeq(SWGSSeqInfo& seq,
                                            const string& acc)
{
    CWGSDb db = GetWGSDb(seq);
    // proteins can be located in nuc-prot set
    uint64_t cds_row_id = db->GetProductFeatRowId(acc);
    if ( !cds_row_id ) {
        return false;
    }
    CWGSFeatureIterator cds_it(db, cds_row_id);
    if ( !cds_it ) {
        return false;
    }
    if ( cds_it.GetSeqType() != NCBI_WGS_seqtype_contig ) {
        return false;
    }
    // switch to contig
    seq.m_RowId = cds_it.GetLocRowId();
    seq.m_SeqType = '\0';
    ResetIteratorCache(seq);
    return true;
}


CID2WGSProcessor_Impl::SWGSSeqInfo
CID2WGSProcessor_Impl::Resolve(const CSeq_id& id)
{
    if ( !s_Enabled() ) {
        return SWGSSeqInfo();
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
        return SWGSSeqInfo();
    default:
        break;
    }
    const CTextseq_id* text_id = id.GetTextseq_Id();
    if ( !text_id ) {
        return SWGSSeqInfo();
    }
    if ( !text_id->IsSetAccession() ) {
        return SWGSSeqInfo();
    }
    int version = 1;
    if ( text_id->IsSetVersion() ) {
        version = text_id->GetVersion();
    }
    SWGSSeqInfo seq = ResolveAcc(text_id->GetAccession(), version);
    if ( !seq ) {
        return seq;
    }
    if ( text_id->IsSetVersion() && !IsCorrectVersion(seq, version) ) {
        return seq;
    }
    seq.m_ValidWGS = true;
    if ( seq.m_SeqType == 'P' ) {
        SwitchToMainSeq(seq, text_id->GetAccession());
    }
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
                               CID2_Request_Get_Seq_id& request)
{
    if ( !request.GetSeq_id().IsSeq_id() ) {
        return SWGSSeqInfo();
    }
    SWGSSeqInfo seq = Resolve(request.GetSeq_id().GetSeq_id());
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


bool CID2WGSProcessor_Impl::ProcessGetSeqId(TReplies& replies,
                                            CID2_Request& main_request,
                                            CID2_Request_Get_Seq_id& request)
{
    START_TRACE();
    TRACE_X(4, eDebug_request, "GetSeqId: "<<MSerial_AsnText<<main_request);
    SWGSSeqInfo seq = Resolve(replies, main_request, request);
    TRACE_X(5, eDebug_resolve, "GetSeqId: done");
    return seq || seq.m_IsWGS;
}


CID2WGSProcessor_Impl::SWGSSeqInfo
CID2WGSProcessor_Impl::Resolve(TReplies& replies,
                               CID2_Request& main_request,
                               CID2_Request_Get_Blob_Id& request)
{
    SWGSSeqInfo seq = Resolve(replies, main_request, request.SetSeq_id());
    if ( !seq ) {
        return seq;
    }
    CRef<CID2_Reply> main_reply(new CID2_Reply);
    if ( main_request.IsSetSerial_number() ) {
        main_reply->SetSerial_number(main_request.GetSerial_number());
    }
    CID2_Reply_Get_Blob_Id& reply = main_reply->SetReply().SetGet_blob_id();
    reply.SetSeq_id(request.SetSeq_id().SetSeq_id().SetSeq_id());
    reply.SetBlob_id(*GetBlobId(seq));
    if ( int blob_state = GetID2BlobState(seq) ) {
        reply.SetBlob_state(blob_state);
    }
    reply.SetEnd_of_reply();
    replies.push_back(main_reply);
    TRACE_X(6, eDebug_replies, "GetBlobId: "<<MSerial_AsnText<<*main_reply);
    return seq;
}


bool CID2WGSProcessor_Impl::ProcessGetBlobId(TReplies& replies,
                                        CID2_Request& main_request,
                                        CID2_Request_Get_Blob_Id& request)
{
    START_TRACE();
    TRACE_X(7, eDebug_request, "GetBlobId: "<<MSerial_AsnText<<main_request);
    SWGSSeqInfo seq = Resolve(replies, main_request, request);
    TRACE_X(8, eDebug_resolve, "GetBlobId: done");
    return seq || seq.m_IsWGS;
}


namespace {
};


NCBI_gb_state CID2WGSProcessor_Impl::GetGBState(SWGSSeqInfo& seq)
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


CRef<CSeq_entry> CID2WGSProcessor_Impl::GetSeq_entry(SWGSSeqInfo& seq)
{
    if ( seq.IsContig() ) {
        CWGSSeqIterator& it = GetContigIterator(seq);
        CWGSSeqIterator::TFlags flags =
            (it.fDefaultFlags & ~it.fMaskDescr) | it.fSeqDescr;
        return it.GetSeq_entry(flags);
    }
    if ( seq.IsMaster() ) {
        return GetWGSDb(seq)->GetMasterSeq_entry();
    }
    if ( seq.IsScaffold() ) {
        CWGSScaffoldIterator& it = GetScaffoldIterator(seq);
        CWGSScaffoldIterator::TFlags flags =
            (it.fDefaultFlags & ~it.fMaskDescr) | it.fSeqDescr;
        CRef<CSeq_entry> entry(new CSeq_entry);
        entry->SetSeq(*it.GetBioseq(flags));
        return entry;
    }
    if ( seq.IsProtein() ) {
        CWGSProteinIterator& it = GetProteinIterator(seq);
        CWGSProteinIterator::TFlags flags =
            (it.fDefaultFlags & ~it.fMaskDescr) | it.fSeqDescr;
        CRef<CSeq_entry> entry(new CSeq_entry);
        entry->SetSeq(*it.GetBioseq(flags));
        return entry;
    }
    return null;
}


void CID2WGSProcessor_Impl::WriteData(CID2_Reply_Data& data,
                                      const CSerialObject& obj) const
{
    data.SetData_format(CID2_Reply_Data::eData_format_asn_binary);
    COSSWriter writer(data.SetData());
    CWStream writer_stream(&writer);
    AutoPtr<CNcbiOstream> str;
    if ( !m_CompressData ) {
        data.SetData_compression(CID2_Reply_Data::eData_compression_none);
        str.reset(&writer_stream, eNoOwnership);
    }
    else {
        data.SetData_compression(CID2_Reply_Data::eData_compression_gzip);
        str.reset(new CCompressionOStream(writer_stream,
                                          new CZipStreamCompressor,
                                          CCompressionIStream::fOwnProcessor));
    }
    CObjectOStreamAsnBinary objstr(*str);
    objstr << obj;
}


bool CID2WGSProcessor_Impl::ProcessGetBlobInfo(TReplies& replies,
                                               CID2_Request& main_request,
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
        seq = Resolve(replies, main_request, req_id.SetResolve().SetRequest());
    }
    if ( !seq ) {
        TRACE_X(10, eDebug_resolve, "GetBlobInfo: null");
        return seq.m_IsWGS;
    }

    if ( request.IsSetGet_data() ) {
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
        reply.SetBlob_id(*GetBlobId(seq));
        reply.SetBlob_state(GetID2BlobState(seq));
        CID2_Reply_Data& data = reply.SetData();
        data.SetData_type(CID2_Reply_Data::eData_type_seq_entry);
        WriteData(data, *entry);
        TRACE_X(12, eDebug_resolve, ""<<seq<<" data size: "<<sx_GetSize(data));
        replies.push_back(main_reply);
    }
    TRACE_X(14, eDebug_resolve,"GetBlobInfo: done");
    return true;
}


bool CID2WGSProcessor_Impl::ProcessRequest(TReplies& replies,
                                           CID2_Request& request)
{
    if ( !s_Enabled() ) {
        return false;
    }
    size_t old_size = replies.size();
    bool done;
    try {
        switch ( request.GetRequest().Which() ) {
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
            ERR_POST_X(16, "ID2WGS: Exception while processing request "<<
                       MSerial_AsnText<<request<<": "<<exc);
        }
    }
    catch ( exception& exc ) {
        if ( s_DebugEnabled(eDebug_error) ) {
            ERR_POST_X(16, "ID2WGS: Exception while processing request "<<
                       MSerial_AsnText<<request<<": "<<exc.what());
        }
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
CID2WGSProcessor_Impl::ProcessSomeRequests(CID2_Request_Packet& packet)
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
