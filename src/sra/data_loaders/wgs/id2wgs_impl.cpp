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
#include <sra/readers/sra/wgsread.hpp>
#include <sra/error_codes.hpp>
#include <corelib/reader_writer.hpp>
#include <corelib/rwstream.hpp>
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
NCBI_DEFINE_ERR_SUBCODE_X(8);

BEGIN_NAMESPACE(objects);


NCBI_PARAM_DECL(bool, ID2WGS, ENABLE);
NCBI_PARAM_DEF_EX(bool, ID2WGS, ENABLE, true,
                  eParam_NoThread, ID2WGS_ENABLE);


NCBI_PARAM_DECL(int, ID2WGS, DEBUG);
NCBI_PARAM_DEF_EX(int, ID2WGS, DEBUG, 0,
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

# define START_TRACE() if(s_DebugLevel()<1);else sw.Restart()

static CNcbiOstream& operator<<(CNcbiOstream& out,
                                const CID2WGSProcessor_Impl::SWGSSeqInfo& seq)
{
    out << seq.m_WGSAcc;
    if ( seq.m_SeqType ) {
        out << seq.m_SeqType;
    }
    out << setfill('0')<<setw(seq.m_RowDigits)<<seq.m_RowId<<setfill(' ');
    return out;
}
# define TRACE(l,m)                                                     \
    if(s_DebugLevel()<(l));else LOG_POST(Info<<sw.Elapsed()<<": ID2WGS: "<<m)
#else
# define START_TRACE() do{}while(0)
# define TRACE(l,m) do{}while(0)
#endif

CID2WGSProcessor_Impl::CID2WGSProcessor_Impl(void)
    : m_WGSDbCache(10)
{
}


CID2WGSProcessor_Impl::CID2WGSProcessor_Impl(const CConfig::TParamTree* params,
                                             const string& driver_name)
    : m_WGSDbCache(10)
{
}


CID2WGSProcessor_Impl::~CID2WGSProcessor_Impl(void)
{
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
        TRACE(1,"GetWGSDb: "<<prefix);
        return wgs_db;
    }
    catch ( CSraException& exc ) {
        if ( exc.GetErrCode() == exc.eNotFoundDb ||
             exc.GetErrCode() == exc.eProtectedDb ) {
            // no such WGS table
            return CWGSDb();
        }
        throw;
    }
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


static const size_t kNumLetters = 4;
static const size_t kVersionDigits = 2;
static const size_t kPrefixLen = kNumLetters + kVersionDigits;
static const size_t kMinRowDigits = 6;
static const size_t kMaxRowDigits = 8;


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
    CWGSGiResolver::TAccessionList accs = m_GiResolver.FindAll(gi);
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
    CWGSProtAccResolver::TAccessionList accs = m_AccResolver.FindAll(acc);
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
CID2WGSProcessor_Impl::x_Process(TReplies& replies,
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
            TRACE(2,"GetSeqId: "<<MSerial_AsnText<<*main_reply);
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
    TRACE(2,"GetSeqId: "<<MSerial_AsnText<<*main_reply);
    return seq;
}


bool CID2WGSProcessor_Impl::ProcessGetSeqId(TReplies& replies,
                                            CID2_Request& main_request,
                                            CID2_Request_Get_Seq_id& request)
{
    START_TRACE();
    TRACE(2,"GetSeqId: "<<MSerial_AsnText<<main_request);
    SWGSSeqInfo seq = x_Process(replies, main_request, request);
    TRACE(2,"GetSeqId: done");
    return seq || seq.m_IsWGS;
}


CID2WGSProcessor_Impl::SWGSSeqInfo
CID2WGSProcessor_Impl::x_Process(TReplies& replies,
                                 CID2_Request& main_request,
                                 CID2_Request_Get_Blob_Id& request)
{
    SWGSSeqInfo seq = x_Process(replies, main_request,
                                request.SetSeq_id());
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
    TRACE(2,"GetBlobId: "<<MSerial_AsnText<<*main_reply);
    return seq;
}


bool CID2WGSProcessor_Impl::ProcessGetBlobId(TReplies& replies,
                                        CID2_Request& main_request,
                                        CID2_Request_Get_Blob_Id& request)
{
    START_TRACE();
    TRACE(2,"GetBlobId: "<<MSerial_AsnText<<main_request);
    SWGSSeqInfo seq = x_Process(replies, main_request, request);
    TRACE(2,"GetBlobId: done");
    return seq || seq.m_IsWGS;
}


namespace {
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


CRef<CBioseq> CID2WGSProcessor_Impl::GetBioseq(SWGSSeqInfo& seq)
{
    if ( seq.IsContig() ) {
        CWGSSeqIterator& it = GetContigIterator(seq);
        CWGSSeqIterator::TFlags flags =
            (it.fDefaultFlags & ~it.fMaskDescr) | it.fSeqDescr;
        return it.GetBioseq(flags);
    }
    if ( seq.IsScaffold() ) {
        CWGSScaffoldIterator& it = GetScaffoldIterator(seq);
        CWGSScaffoldIterator::TFlags flags =
            (it.fDefaultFlags & ~it.fMaskDescr) | it.fSeqDescr;
        return it.GetBioseq(flags);
    }
    if ( seq.IsProtein() ) {
        CWGSProteinIterator& it = GetProteinIterator(seq);
        CWGSProteinIterator::TFlags flags =
            (it.fDefaultFlags & ~it.fMaskDescr) | it.fSeqDescr;
        return it.GetBioseq(flags);
    }
    // master
    return GetWGSDb(seq)->GetMasterBioseq();
}


bool CID2WGSProcessor_Impl::ProcessGetBlobInfo(TReplies& replies,
                                               CID2_Request& main_request,
                                               CID2_Request_Get_Blob_Info& request)
{
    START_TRACE();
    TRACE(2,"GetBlobInfo: "<<MSerial_AsnText<<main_request);
    SWGSSeqInfo seq;
    if ( request.GetBlob_id().IsBlob_id() ) {
        seq = ResolveBlobId(request.GetBlob_id().GetBlob_id());
    }
    else {
        seq = x_Process(replies, main_request,
                        request.SetBlob_id().SetResolve().SetRequest());
    }
    if ( !seq ) {
        TRACE(2,"GetBlobInfo: null");
        return seq.m_IsWGS;
    }

    if ( request.IsSetGet_data() ) {
        CRef<CBioseq> bioseq = GetBioseq(seq);
        TRACE(8,"GetBioseq: "<<seq);
        CRef<CSeq_entry> entry(new CSeq_entry);
        entry->SetSeq(*bioseq);

        CRef<CID2_Reply> main_reply(new CID2_Reply);
        if ( main_request.IsSetSerial_number() ) {
            main_reply->SetSerial_number(main_request.GetSerial_number());
        }
        CID2_Reply_Get_Blob& reply = main_reply->SetReply().SetGet_blob();
        reply.SetBlob_id(*GetBlobId(seq));
        reply.SetBlob_state(GetID2BlobState(seq));
        CID2_Reply_Data& data = reply.SetData();
        data.SetData_type(CID2_Reply_Data::eData_type_seq_entry);
        data.SetData_format(CID2_Reply_Data::eData_format_asn_binary);

        COSSWriter writer(data.SetData());
        CWStream wstream(&writer);
        CObjectOStreamAsnBinary objstr(wstream);
        objstr << *entry;
        TRACE(2,"SetData: "<<seq);
        replies.push_back(main_reply);
        TRACE(9,"ProcessBlobId: "<<MSerial_AsnText<<*main_reply);
    }
    TRACE(2,"GetBlobInfo: done");
    return true;
}


bool CID2WGSProcessor_Impl::ProcessRequest(TReplies& replies,
                                           CID2_Request& request)
{
    if ( !s_Enabled() ) {
        return false;
    }
    switch ( request.GetRequest().Which() ) {
    case CID2_Request::TRequest::e_Get_seq_id:
        return ProcessGetSeqId(replies, request,
                               request.SetRequest().SetGet_seq_id());
    case CID2_Request::TRequest::e_Get_blob_id:
        return ProcessGetBlobId(replies, request,
                                request.SetRequest().SetGet_blob_id());
    case CID2_Request::TRequest::e_Get_blob_info:
        return ProcessGetBlobInfo(replies, request,
                                  request.SetRequest().SetGet_blob_info());
    default:
        return false;
    }
}


CID2WGSProcessor_Impl::TReplies
CID2WGSProcessor_Impl::ProcessSomeRequests(CID2_Request_Packet& packet)
{
    TReplies replies;
    ERASE_ITERATE ( CID2_Request_Packet::Tdata, it, packet.Set() ) {
        if ( ProcessRequest(replies, **it) ) {
            replies.back()->SetEnd_of_reply();
            packet.Set().erase(it);
        }
    }
    if ( !packet.Set().empty() ) {
        START_TRACE();
        TRACE(3,"Unprocessed: "<<MSerial_AsnText<<packet);
    }
    return replies;
}


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;
