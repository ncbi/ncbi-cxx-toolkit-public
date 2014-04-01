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
 *   Access to WGS files
 *
 */

#include <ncbi_pch.hpp>
#include <sra/readers/sra/wgsread.hpp>
#include <sra/readers/ncbi_traces_path.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_param.hpp>
#include <objects/general/general__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqset/seqset__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/seqres/seqres__.hpp>
#include <klib/rc.h>
#include <serial/objistrasnb.hpp>
#include <serial/serial.hpp>
#include <sra/error_codes.hpp>

#include <sra/readers/sra/kdbread.hpp>
#include <vdb/vdb-priv.h>
#include <kdb/table.h>
#include <kdb/meta.h>
#include <kdb/namelist.h>

BEGIN_NCBI_NAMESPACE;

#define NCBI_USE_ERRCODE_X   WGSReader
NCBI_DEFINE_ERR_SUBCODE_X(1);

BEGIN_NAMESPACE(objects);


/////////////////////////////////////////////////////////////////////////////
// CWGSDb_Impl
/////////////////////////////////////////////////////////////////////////////


CWGSDb_Impl::SSeqTableCursor::SSeqTableCursor(const CVDB& db)
    : m_Table(db, "SEQUENCE"),
      m_Cursor(m_Table),
      INIT_OPTIONAL_VDB_COLUMN(GI),
      INIT_VDB_COLUMN(ACCESSION),
      INIT_VDB_COLUMN(ACC_VERSION),
      //INIT_VDB_COLUMN(SEQ_ID_GNL),
      INIT_VDB_COLUMN(CONTIG_NAME),
      INIT_VDB_COLUMN(NAME),
      INIT_VDB_COLUMN(TITLE),
      INIT_VDB_COLUMN(LABEL),
      INIT_VDB_COLUMN(READ_START),
      INIT_VDB_COLUMN(READ_LEN),
      INIT_VDB_COLUMN_AS(READ, INSDC:4na:packed),
      INIT_OPTIONAL_VDB_COLUMN(TAXID),
      INIT_OPTIONAL_VDB_COLUMN(DESCR),
      INIT_OPTIONAL_VDB_COLUMN(ANNOT),
      INIT_OPTIONAL_VDB_COLUMN(GB_STATE),
      INIT_OPTIONAL_VDB_COLUMN(GAP_START),
      INIT_OPTIONAL_VDB_COLUMN(GAP_LEN),
      INIT_OPTIONAL_VDB_COLUMN(GAP_PROPS),
      INIT_OPTIONAL_VDB_COLUMN(GAP_LINKAGE)
{
}


CWGSDb_Impl::SScfTableCursor::SScfTableCursor(const CVDB& db)
    : m_Table(db, "SCAFFOLD"),
      m_Cursor(m_Table),
      INIT_VDB_COLUMN(SCAFFOLD_NAME),
      INIT_OPTIONAL_VDB_COLUMN(ACCESSION),
      INIT_VDB_COLUMN(COMPONENT_ID),
      INIT_VDB_COLUMN(COMPONENT_START),
      INIT_VDB_COLUMN(COMPONENT_LEN),
      INIT_VDB_COLUMN(COMPONENT_PROPS),
      INIT_OPTIONAL_VDB_COLUMN(COMPONENT_LINKAGE)
{
}


CWGSDb_Impl::SIdxTableCursor::SIdxTableCursor(const CVDB& db)
    : m_Table(db, "GI_IDX"),
      m_Cursor(m_Table),
      INIT_VDB_COLUMN(NUC_ROW_ID)
{
}


CWGSDb_Impl::CWGSDb_Impl(CVDBMgr& mgr,
                         CTempString path_or_acc,
                         CTempString vol_path)
    : m_Mgr(mgr),
      m_WGSPath(NormalizePathOrAccession(path_or_acc, vol_path)),
      m_Db(mgr, m_WGSPath)
{
    x_InitIdParams();
}


void CWGSDb_Impl::x_InitIdParams(void)
{
    CRef<SSeqTableCursor> seq = Seq();
    if ( !seq->m_Cursor.TryOpenRow(1) ) {
        m_IdPrefixWithVersion.erase();
        m_IdPrefix.erase();
        m_IdVersion = 1;
        m_IdRowDigits = 0;
        return;
    }
    CTempString acc = *seq->ACCESSION(1);
    SIZE_TYPE prefix_len = NStr::StartsWith(acc, "NZ_")? 7: 4;
    m_IdRowDigits = acc.size() - (prefix_len + 2);
    if ( m_IdRowDigits < 6 || m_IdRowDigits > 8 ) {
        NCBI_THROW_FMT(CSraException, eInitFailed,
                       "CWGSDb: bad WGS accession format: "<<acc);
    }
    m_IdPrefixWithVersion = acc.substr(0, prefix_len+2);
    m_IdPrefix = acc.substr(0, prefix_len);
    m_IdVersion = NStr::StringToNumeric<int>(acc.substr(prefix_len, 2));
    Put(seq);
}


string CWGSDb_Impl::NormalizePathOrAccession(CTempString path_or_acc,
                                             CTempString vol_path)
{
    if ( !vol_path.empty() ) {
        list<CTempString> dirs;
        NStr::Split(vol_path, ":", dirs);
        ITERATE ( list<CTempString>, it, dirs ) {
            string path = CDirEntry::MakePath(*it, path_or_acc);
            if ( CDirEntry(path).Exists() ) {
                return path;
            }
        }
        return CDirEntry::MakePath(vol_path, path_or_acc);
    }
    if ( path_or_acc.find_first_of("/\\") == NPOS ) {
        // parse WGS accession
        // optional "NZ_" prefix
        SIZE_TYPE start = NStr::StartsWith(path_or_acc, "NZ_")? 3: 0;
        // then there should be "ABCD01" or "ABCD"
        if ( path_or_acc.size() == start + 4 ) {
            // add default version 1
            return string(path_or_acc) + "01";
        }
        if ( path_or_acc.size() > start + 6 ) {
            // remove contig/scaffold id
            return path_or_acc.substr(0, start+6);
        }
    }
    return path_or_acc;
}


CRef<CWGSDb_Impl::SSeqTableCursor> CWGSDb_Impl::Seq(void)
{
    CRef<SSeqTableCursor> curs = m_Seq.Get();
    if ( !curs ) {
        curs = new SSeqTableCursor(m_Db);
    }
    return curs;
}


CRef<CWGSDb_Impl::SScfTableCursor> CWGSDb_Impl::Scf(void)
{
    CRef<SScfTableCursor> curs = m_Scf.Get();
    if ( !curs ) {
        try {
            curs = new SScfTableCursor(m_Db);
        }
        catch ( CSraException& exc ) {
            if ( exc.GetErrCode() != CSraException::eNotFoundTable ) {
                throw;
            }
        }
    }
    return curs;
}


CRef<CWGSDb_Impl::SIdxTableCursor> CWGSDb_Impl::Idx(void)
{
    CRef<SIdxTableCursor> curs = m_Idx.Get();
    if ( !curs ) {
        try {
            curs = new SIdxTableCursor(m_Db);
        }
        catch ( CSraException& exc ) {
            if ( exc.GetErrCode() != CSraException::eNotFoundTable ) {
                throw;
            }
        }
    }
    return curs;
}


uint64_t CWGSDb_Impl::ParseRow(CTempString acc, bool* is_scaffold) const
{
    SIZE_TYPE start = NStr::StartsWith(acc, "NZ_")? 3: 0;
    CTempString number = acc.substr(start+6);
    if ( number[0] == 'S' ) {
        if ( !is_scaffold ) {
            // only non-scaffolds are accepted if is_scaffold flag is absent
            return 0;
        }
        *is_scaffold = true;
        number = number.substr(1); // skip scaffold prefix
    }
    else {
        if ( is_scaffold ) {
            *is_scaffold = false;
        }
    }
    return NStr::StringToNumeric<uint64_t>(number,
                                           NStr::fConvErr_NoThrow);
}


BEGIN_LOCAL_NAMESPACE;

bool sx_SetVersion(CSeq_id& id, int version)
{
    if ( const CTextseq_id* text_id = id.GetTextseq_Id() ) {
        const_cast<CTextseq_id*>(text_id)->SetVersion(version);
        return true;
    }
    return false;
}

END_LOCAL_NAMESPACE;


CRef<CSeq_id> CWGSDb_Impl::GetMasterSeq_id(void) const
{
    CRef<CSeq_id> id;
    if ( m_IdPrefix.empty() ) {
        return id;
    }
    string master_acc = m_IdPrefix;
    master_acc.resize(master_acc.size() + 2 + m_IdRowDigits, '0');
    id = new CSeq_id(master_acc);
    if ( !sx_SetVersion(*id, m_IdVersion) ) {
        id = null;
    }
    return id;
}


CRef<CSeq_id> CWGSDb_Impl::GetContigSeq_id(uint64_t row_id) const
{
    CRef<CSeq_id> id;
    if ( m_IdPrefixWithVersion.empty() ) {
        return id;
    }
    CNcbiOstrstream str;
    str << m_IdPrefixWithVersion
        << setfill('0') << setw(m_IdRowDigits) << row_id;
    string id_str = CNcbiOstrstreamToString(str);
    id = new CSeq_id(id_str);
    sx_SetVersion(*id, 1);
    return id;
}


CRef<CSeq_id> CWGSDb_Impl::GetScaffoldSeq_id(uint64_t row_id) const
{
    CRef<CSeq_id> id;
    if ( m_IdPrefixWithVersion.empty() ) {
        return id;
    }
    CNcbiOstrstream str;
    str << m_IdPrefixWithVersion << 'S'
        << setfill('0') << setw(m_IdRowDigits) << row_id << ".1";
    string id_str = CNcbiOstrstreamToString(str);
    id = new CSeq_id(id_str);
    return id;
}


void CWGSDb_Impl::SetMasterDescr(const TMasterDescr& descr)
{
    m_MasterDescr.clear();
    ITERATE ( TMasterDescr, it, descr ) {
        m_MasterDescr.push_back(Ref(SerialClone(**it)));
    }
}


/////////////////////////////////////////////////////////////////////////////
// CWGSSeqIterator
/////////////////////////////////////////////////////////////////////////////


CWGSSeqIterator::CWGSSeqIterator(void)
    : m_CurrId(0), m_FirstBadId(0), m_Withdrawn(eExcludeWithdrawn)
{
}


CWGSSeqIterator::CWGSSeqIterator(const CWGSDb& wgs_db,
                                 EWithdrawn withdrawn)
{
    x_Init(wgs_db, withdrawn);
    if ( x_Excluded() ) {
        ++*this;
    }
}


CWGSSeqIterator::CWGSSeqIterator(const CWGSDb& wgs_db, uint64_t row,
                                 EWithdrawn withdrawn)
{
    x_Init(wgs_db, withdrawn);
    if ( row < m_CurrId ) {
        // before the first id
        m_FirstBadId = 0;
    }
    m_CurrId = row;
    if ( x_Excluded() ) {
        m_CurrId = 0;
        m_FirstBadId = 0;
    }
}


CWGSSeqIterator::CWGSSeqIterator(const CWGSDb& wgs_db, CTempString acc,
                                 EWithdrawn withdrawn)
{
    if ( uint64_t row = wgs_db.ParseRow(acc) ) {
        x_Init(wgs_db, withdrawn);
        if ( row < m_CurrId ) {
            // before the first id
            m_FirstBadId = 0;
        }
        m_CurrId = row;
        if ( x_Excluded() ) {
            m_CurrId = 0;
            m_FirstBadId = 0;
        }
    }
    else {
        // bad format
        m_CurrId = 0;
        m_FirstBadId = 0;
    }
}


CWGSSeqIterator::~CWGSSeqIterator(void)
{
    if ( m_Seq ) {
        GetDb().Put(m_Seq);
    }
}


void CWGSSeqIterator::x_Init(const CWGSDb& wgs_db, EWithdrawn withdrawn)
{
    m_Db = wgs_db;
    m_Seq = wgs_db.GetNCObject().Seq();
    m_Withdrawn = withdrawn;
    pair<int64_t, uint64_t> range = m_Seq->m_Cursor.GetRowIdRange();
    m_CurrId = range.first;
    m_FirstBadId = range.first+range.second;
}


CWGSSeqIterator& CWGSSeqIterator::operator++(void)
{
    do {
        ++m_CurrId;
    } while ( x_Excluded() );
    return *this;
}


void CWGSSeqIterator::x_ReportInvalid(const char* method) const
{
    NCBI_THROW_FMT(CSraException, eInvalidState,
                   "CWGSSeqIterator::"<<method<<"(): Invalid iterator state");
}


bool CWGSSeqIterator::HasGi(void) const
{
    return m_Seq->m_GI;
}


CSeq_id::TGi CWGSSeqIterator::GetGi(void) const
{
    x_CheckValid("GetGi");
    NCBI_gi gi = *m_Seq->GI(m_CurrId);
    if ( sizeof(CSeq_id::TGi) != sizeof(NCBI_gi) &&
         NCBI_gi(CSeq_id::TGi(gi)) != gi ) {
        NCBI_THROW_FMT(CSraException, eDataError,
                       "CWGSSeqIterator::GetGi() GI is too big: "<<gi);
    }
    return CSeq_id::TGi(gi);
}


CTempString CWGSSeqIterator::GetAccession(void) const
{
    x_CheckValid("GetAccession");
    return *CVDBStringValue(m_Seq->ACCESSION(m_CurrId));
}


int CWGSSeqIterator::GetAccVersion(void) const
{
    x_CheckValid("GetAccVersion");
    return *m_Seq->ACC_VERSION(m_CurrId);
}


CRef<CSeq_id> CWGSSeqIterator::GetAccSeq_id(void) const
{
    CRef<CSeq_id> id;
    CTempString acc = GetAccession();
    if ( !acc.empty() ) {
        id = new CSeq_id(GetAccession());
        sx_SetVersion(*id, GetAccVersion());
    }
    return id;
}


CTempString CWGSSeqIterator::GetContigName(void) const
{
    x_CheckValid("GetContigName");
    return *m_Seq->CONTIG_NAME(m_CurrId);
}


bool CWGSSeqIterator::HasTaxId(void) const
{
    return m_Seq->m_TAXID;
}


int CWGSSeqIterator::GetTaxId(void) const
{
    x_CheckValid("GetTaxId");
    return *m_Seq->TAXID(m_CurrId);
}


TSeqPos CWGSSeqIterator::GetSeqLength(void) const
{
    x_CheckValid("GetSeqLength");
    return *m_Seq->READ_LEN(m_CurrId);
}


void CWGSSeqIterator::GetIds(CBioseq::TId& ids, TFlags flags) const
{
    if ( flags & fIds_acc ) {
        // acc.ver
        if ( CRef<CSeq_id> id = GetAccSeq_id() ) {
            ids.push_back(id);
        }
    }

    if ( (flags & fIds_gnl) && !GetDb().m_IdPrefixWithVersion.empty() ) {
        // gnl
        CTempString str = GetContigName();
        if ( !str.empty() ) {
            CRef<CSeq_id> id(new CSeq_id);
            CDbtag& tag = id->SetGeneral();
            tag.SetDb("WGS:"+GetDb().m_IdPrefixWithVersion);
            tag.SetTag().SetStr(str);
            ids.push_back(id);
        }
    }

    if ( (flags & fIds_gi) && HasGi() ) {
        CRef<CSeq_id> id(new CSeq_id);
        id->SetGi(GetGi());
        ids.push_back(id);
    }
}


bool CWGSSeqIterator::HasSeq_descr(void) const
{
    x_CheckValid("HasSeq_descr");

    return m_Seq->m_DESCR && m_Seq->DESCR(m_CurrId).size();
}


CRef<CSeq_descr> CWGSSeqIterator::GetSeq_descr(void) const
{
    x_CheckValid("GetSeq_descr");

    CRef<CSeq_descr> ret(new CSeq_descr);
    CTempString descr_bytes = *CVDBStringValue(m_Seq->DESCR(m_CurrId));
    CObjectIStreamAsnBinary in(descr_bytes.data(), descr_bytes.size());
    // hack to determine if the data is of type Seq-descr (starts with byte 49)
    // or of type Seqdesc (starts with byte >= 160)
    if ( descr_bytes[0] == 49 ) {
        in >> *ret;
    }
    else {
        CRef<CSeqdesc> desc(new CSeqdesc);
        in >> *desc;
        ret->Set().push_back(desc);
    }
    ITERATE ( CWGSDb_Impl::TMasterDescr, it, GetDb().GetMasterDescr() ) {
        ret->Set().push_back(*it);
    }
    return ret;
}


bool CWGSSeqIterator::HasAnnotSet(void) const
{
    x_CheckValid("HasAnnotSet");

    return m_Seq->m_ANNOT && m_Seq->ANNOT(m_CurrId).size();
}


void CWGSSeqIterator::GetAnnotSet(TAnnotSet& annot_set) const
{
    x_CheckValid("GetAnnotSet");

    CTempString annot_bytes = *CVDBStringValue(m_Seq->ANNOT(m_CurrId));
    CObjectIStreamAsnBinary in(annot_bytes.data(), annot_bytes.size());
    while ( in.HaveMoreData() ) {
        CRef<CSeq_annot> annot(new CSeq_annot);
        in >> *annot;
        annot_set.push_back(annot);
    }
}


NCBI_gb_state CWGSSeqIterator::GetGBState(void) const
{
    x_CheckValid("GetGBState");

    return m_Seq->m_GB_STATE? *m_Seq->GB_STATE(m_CurrId): 0;
}


enum
{
    NCBI_WGS_gap_linkage_linked                      = 1, // linkage exists
    NCBI_WGS_gap_linkage_evidence_paired_ends        = 2, // evidence type
    NCBI_WGS_gap_linkage_evidence_align_genus        = 4, // in ASN spec order
    NCBI_WGS_gap_linkage_evidence_align_xgenus       = 8,
    NCBI_WGS_gap_linkage_evidence_align_trnscpt      = 16,
    NCBI_WGS_gap_linkage_evidence_within_clone       = 32,
    NCBI_WGS_gap_linkage_evidence_clone_contig       = 64,
    NCBI_WGS_gap_linkage_evidence_map                = 128,
    NCBI_WGS_gap_linkage_evidence_strobe             = 256,
    NCBI_WGS_gap_linkage_evidence_unspecified        = 512,
    NCBI_WGS_gap_linkage_evidence_pcr                = 1024
};


static
void sx_AddEvidence(CSeq_gap& gap, CLinkage_evidence::TType type)
{
    CRef<CLinkage_evidence> evidence(new CLinkage_evidence);
    evidence->SetType(type);
    gap.SetLinkage_evidence().push_back(evidence);
}


static
void sx_MakeGap(CDelta_seq& seg,
                TSeqPos len,
                NCBI_WGS_component_props props,
                NCBI_WGS_gap_linkage gap_linkage)
{
    static const int kLenTypeMask =
        -NCBI_WGS_gap_known |
        -NCBI_WGS_gap_unknown;
    static const int kGapTypeMask =
        -NCBI_WGS_gap_scaffold |
        -NCBI_WGS_gap_contig |
        -NCBI_WGS_gap_centromere |
        -NCBI_WGS_gap_short_arm |
        -NCBI_WGS_gap_heterochromatin |
        -NCBI_WGS_gap_telomere |
        -NCBI_WGS_gap_repeat;
    _ASSERT(props < 0);
    CSeq_literal& literal = seg.SetLiteral();
    int len_type    = -(-props & kLenTypeMask);
    int gap_type    = -(-props & kGapTypeMask);
    literal.SetLength(len);
    if ( len_type == NCBI_WGS_gap_unknown ) {
        literal.SetFuzz().SetLim(CInt_fuzz::eLim_unk);
    }
    if ( gap_type || gap_linkage ) {
        CSeq_gap& gap = literal.SetSeq_data().SetGap();
        switch ( gap_type ) {
        case 0:
            gap.SetType(CSeq_gap::eType_unknown);
            break;
        case NCBI_WGS_gap_scaffold:
            gap.SetType(CSeq_gap::eType_scaffold);
            break;
        case NCBI_WGS_gap_contig:
            gap.SetType(CSeq_gap::eType_contig);
            break;
        case NCBI_WGS_gap_centromere:
            gap.SetType(CSeq_gap::eType_centromere);
            break;
        case NCBI_WGS_gap_short_arm:
            gap.SetType(CSeq_gap::eType_short_arm);
            break;
        case NCBI_WGS_gap_heterochromatin:
            gap.SetType(CSeq_gap::eType_heterochromatin);
            break;
        case NCBI_WGS_gap_telomere:
            gap.SetType(CSeq_gap::eType_telomere);
            break;
        case NCBI_WGS_gap_repeat:
            gap.SetType(CSeq_gap::eType_repeat);
            break;
        default:
            break;
        }
        // linkage-evidence bits should be in order of ASN.1 specification
        if ( gap_linkage & NCBI_WGS_gap_linkage_linked ) {
            gap.SetLinkage(gap.eLinkage_linked);
        }
        CLinkage_evidence::TType type = CLinkage_evidence::eType_paired_ends;
        NCBI_WGS_gap_linkage bit = NCBI_WGS_gap_linkage_evidence_paired_ends;
        for ( ; bit <= gap_linkage; bit<<=1, ++type ) {
            if ( gap_linkage & bit ) {
                sx_AddEvidence(gap, type);
            }
        }
    }
}


// For 4na data in byte {base0 4 (highest) bits, base1 4 bits} the bits are:
// 5: base0 is not 2na
// 4: base1 is not 2na
// 3-2: base0 2na encoding, or 1 if gap
// 1-0: base1 2na encoding, or 1 if gap
enum {
    f1st_4na      = 1<<5,
    f1st_2na_max  = 3<<2,
    f1st_gap_bit  = 1<<2,
    f1st_gap      = f1st_4na|f1st_gap_bit,
    f1st_mask     = f1st_4na|f1st_2na_max,
    f2nd_4na      = 1<<4,
    f2nd_2na_max  = 3<<0,
    f2nd_gap_bit  = 1<<0,
    f2nd_gap      = f2nd_4na|f2nd_gap_bit,
    f2nd_mask     = f2nd_4na|f2nd_2na_max,
    fBoth_2na_max = f1st_2na_max|f2nd_2na_max,
    fBoth_4na     = f1st_4na|f2nd_4na,
    fBoth_gap     = f1st_gap|f2nd_gap
};
static const Uint1 sx_4naFlags[256] = {
    // 2nd (low 4 bits) meaning (default=16)                        // 1st high
    //  =0  =1      =2              =3                         =17  // =32
    48, 32, 33, 48, 34, 48, 48, 48, 35, 48, 48, 48, 48, 48, 48, 49, // 
    16,  0,  1, 16,  2, 16, 16, 16,  3, 16, 16, 16, 16, 16, 16, 17, // =0
    20,  4,  5, 20,  6, 20, 20, 20,  7, 20, 20, 20, 20, 20, 20, 21, // =4
    48, 32, 33, 48, 34, 48, 48, 48, 35, 48, 48, 48, 48, 48, 48, 49, // 
    24,  8,  9, 24, 10, 24, 24, 24, 11, 24, 24, 24, 24, 24, 24, 25, // =8
    48, 32, 33, 48, 34, 48, 48, 48, 35, 48, 48, 48, 48, 48, 48, 49, // 
    48, 32, 33, 48, 34, 48, 48, 48, 35, 48, 48, 48, 48, 48, 48, 49, // 
    48, 32, 33, 48, 34, 48, 48, 48, 35, 48, 48, 48, 48, 48, 48, 49, // 
    28, 12, 13, 28, 14, 28, 28, 28, 15, 28, 28, 28, 28, 28, 28, 29, // =12
    48, 32, 33, 48, 34, 48, 48, 48, 35, 48, 48, 48, 48, 48, 48, 49, // 
    48, 32, 33, 48, 34, 48, 48, 48, 35, 48, 48, 48, 48, 48, 48, 49, // 
    48, 32, 33, 48, 34, 48, 48, 48, 35, 48, 48, 48, 48, 48, 48, 49, // 
    48, 32, 33, 48, 34, 48, 48, 48, 35, 48, 48, 48, 48, 48, 48, 49, // 
    48, 32, 33, 48, 34, 48, 48, 48, 35, 48, 48, 48, 48, 48, 48, 49, // 
    48, 32, 33, 48, 34, 48, 48, 48, 35, 48, 48, 48, 48, 48, 48, 49, // 
    52, 36, 37, 52, 38, 52, 52, 52, 39, 52, 52, 52, 52, 52, 52, 53  // =36
};


static const bool kRecoverGaps = true;
    
static inline
bool sx_4naIs2naBoth(Uint1 b4na)
{
    return sx_4naFlags[b4na] <= fBoth_2na_max;
}


static inline
bool sx_4naIs2na1st(Uint1 b4na)
{
    return (sx_4naFlags[b4na]&f1st_mask) <= f1st_2na_max;
}


static inline
bool sx_4naIs2na2nd(Uint1 b4na)
{
    return (sx_4naFlags[b4na]&f2nd_mask) <= f2nd_2na_max;
}


static inline
bool sx_4naIsGapBoth(Uint1 b4na)
{
    return b4na == 0xff;
}


static inline
bool sx_4naIsGap1st(Uint1 b4na)
{
    return (b4na&0xf0) == 0xf0;
}


static inline
bool sx_4naIsGap2nd(Uint1 b4na)
{
    return (b4na&0x0f) == 0x0f;
}


static inline
bool sx_Is2na(const CVDBValueFor4Bits& read,
              TSeqPos pos,
              TSeqPos len)
{
    TSeqPos raw_pos = pos+read.offset();
    const char* raw_ptr = read.raw_data()+(raw_pos/2);
    if ( raw_pos % 2 != 0 ) {
        // check odd base
        if ( !sx_4naIs2na2nd(*raw_ptr) ) {
            return false;
        }
        ++raw_ptr;
        --len;
        if ( len == 0 ) {
            return true;
        }
    }
    while ( len >= 2 ) {
        // check both bases
        if ( !sx_4naIs2naBoth(*raw_ptr) ) {
            return false;
        }
        ++raw_ptr;
        len -= 2;
    }
    if ( len > 0 ) {
        // check one more base
        if ( !sx_4naIs2na1st(*raw_ptr) ) {
            return false;
        }
    }
    return true;
}


static inline
bool sx_IsGap(const CVDBValueFor4Bits& read,
              TSeqPos pos,
              TSeqPos len)
{
    _ASSERT(len > 0);
    TSeqPos raw_pos = pos+read.offset();
    const char* raw_ptr = read.raw_data()+(raw_pos/2);
    if ( raw_pos % 2 != 0 ) {
        // check odd base
        if ( !sx_4naIsGap2nd(*raw_ptr) ) {
            return false;
        }
        ++raw_ptr;
        --len;
        if ( len == 0 ) {
            return true;
        }
    }
    while ( len >= 2 ) {
        // check both bases
        if ( !sx_4naIsGapBoth(*raw_ptr) ) {
            return false;
        }
        ++raw_ptr;
        len -= 2;
    }
    if ( len > 0 ) {
        // check one more base
        if ( !sx_4naIsGap1st(*raw_ptr) ) {
            return false;
        }
    }
    return true;
}


static inline
TSeqPos sx_Get2naLength(const CVDBValueFor4Bits& read,
                        TSeqPos pos,
                        TSeqPos len)
{
    TSeqPos raw_pos = pos+read.offset();
    const char* raw_ptr = read.raw_data()+(raw_pos/2);
    TSeqPos rem_len = len;
    if ( raw_pos % 2 != 0 ) {
        // check odd base
        if ( !sx_4naIs2na2nd(*raw_ptr) ) {
            return 0;
        }
        ++raw_ptr;
        --rem_len;
        if ( rem_len == 0 ) {
            return 1;
        }
    }
    while ( rem_len >= 2 ) {
        // check both bases
        Uint1 flags = sx_4naFlags[Uint1(*raw_ptr)];
        if ( flags & fBoth_4na ) {
            if ( !(flags & f1st_4na) ) {
                --rem_len;
            }
            return len-rem_len;
        }
        ++raw_ptr;
        rem_len -= 2;
    }
    if ( rem_len > 0 ) {
        // check one more base
        if ( sx_4naIs2na1st(*raw_ptr) ) {
            --rem_len;
        }
    }
    return len-rem_len;
}


static inline
TSeqPos sx_Get4naLength(const CVDBValueFor4Bits& read,
                        TSeqPos pos, TSeqPos len,
                        TSeqPos stop_2na_len, TSeqPos stop_gap_len)
{
    if ( len < stop_2na_len ) {
        return len;
    }
    TSeqPos raw_pos = pos+read.offset();
    const char* raw_ptr = read.raw_data()+(raw_pos/2);
    TSeqPos rem_len = len, len2na = 0, gap_len = 0;
    // |-------------------- len -----------------|
    // |- 4na -|- len2na -|- gap_len -$- rem_len -|
    // $ is current position
    // only one of len2na and gap_len can be above zero

    if ( raw_pos % 2 != 0 ) {
        // check odd base
        Uint1 b4na = *raw_ptr;
        if ( kRecoverGaps && sx_4naIsGap2nd(b4na) ) {
            gap_len = 1;
            if ( stop_gap_len == 1 ) { // gap length limit
                return 0;
            }
        }
        if ( sx_4naIs2na2nd(b4na) ) {
            len2na = 1;
            if ( stop_2na_len == 1 ) { // 2na length limit
                return 0;
            }
        }
        ++raw_ptr;
        --rem_len;
        if ( rem_len == 0 ) {
            return 1;
        }
    }
    if ( (len2na+rem_len < stop_2na_len) &&
         (!kRecoverGaps || gap_len+rem_len < stop_gap_len) ) {
        // not enough bases to reach stop_2na_len or stop_gap_len
        return len;
    }

    while ( rem_len >= 2 ) {
        // check both bases
        Uint1 flags = sx_4naFlags[Uint1(*raw_ptr)];
        if ( !(flags & fBoth_4na) ) { // both 2na
            ++raw_ptr;
            rem_len -= 2;
            len2na += 2;
            if ( len2na >= stop_2na_len ) { // reached limit on 2na len
                return len-(rem_len+len2na);
            }
            if ( kRecoverGaps ) {
                gap_len = 0;
                if ( (rem_len < stop_gap_len) &&
                     (len2na+rem_len < stop_2na_len) ) {
                    // not enough bases to reach stop_2na_len or stop_gap_len
                    return len;
                }
            }
        }
        else {
            if ( !(flags & f1st_4na) ) { // 1st 2na
                if ( len2na == stop_2na_len-1 ) { // 1 more 2na is enough
                    return len-(rem_len+len2na);
                }
                ++len2na;
                if ( kRecoverGaps ) {
                    gap_len = 0;
                }
            }
            else {
                if ( kRecoverGaps && (flags & f1st_gap_bit) ) {
                    if ( gap_len == stop_gap_len-1 ) { // 1 more gap is enough
                        return len-(rem_len+gap_len);
                    }
                    ++gap_len;
                }
                len2na = 0;
            }
            --rem_len;
            if ( !(flags & f2nd_4na) ) { // 2nd 2na
                if ( len2na == stop_2na_len-1 ) { // 1 more 2na is enough
                    return len-(rem_len+len2na);
                }
                ++len2na;
                if ( kRecoverGaps ) {
                    gap_len = 0;
                }
            }
            else {
                if ( kRecoverGaps && (flags & f2nd_gap_bit) ) {
                    if ( gap_len == stop_gap_len-1 ) { // 1 more gap is enough
                        return len-(rem_len+gap_len);
                    }
                    ++gap_len;
                }
                len2na = 0;
            }
            --rem_len;
            ++raw_ptr;
            if ( (len2na+rem_len < stop_2na_len) &&
                 (!kRecoverGaps || gap_len+rem_len < stop_gap_len) ) {
                // not enough bases to reach stop_2na_len or stop_gap_len
                return len;
            }
        }
    }
    if ( rem_len > 0 ) {
        // check one more base
        Uint1 flags = sx_4naFlags[Uint1(*raw_ptr)];
        if ( !(flags & f1st_4na) ) { // 1st 2na
            if ( len2na == stop_2na_len-1 ) { // 1 more 2na is enough
                return len-(rem_len+len2na);
            }
            ++len2na;
            if ( kRecoverGaps ) {
                gap_len = 0;
            }
        }
        else {
            if ( kRecoverGaps && (flags & f1st_gap_bit) ) {
                if ( gap_len == stop_gap_len-1 ) { // 1 more gap is enough
                    return len-(rem_len+gap_len);
                }
                ++gap_len;
            }
            len2na = 0;
        }
        --rem_len;
    }
    _ASSERT(len2na < stop_2na_len);
    _ASSERT(!kRecoverGaps || gap_len < stop_gap_len);
    return len;
}


static inline
TSeqPos sx_GetGapLength(const CVDBValueFor4Bits& read,
                        TSeqPos pos,
                        TSeqPos len)
{
    TSeqPos raw_pos = pos+read.offset();
    const char* raw_ptr = read.raw_data()+(raw_pos/2);
    TSeqPos rem_len = len;
    if ( raw_pos % 2 != 0 ) {
        // check odd base
        if ( !sx_4naIsGap2nd(*raw_ptr) ) {
            return 0;
        }
        ++raw_ptr;
        --rem_len;
        if ( rem_len == 0 ) {
            return 1;
        }
    }
    while ( rem_len >= 2 ) {
        // check both bases
        Uint1 b4na = *raw_ptr;
        if ( !sx_4naIsGapBoth(b4na) ) {
            if ( sx_4naIsGap1st(b4na) ) {
                --rem_len;
            }
            return len-rem_len;
        }
        ++raw_ptr;
        rem_len -= 2;
    }
    if ( rem_len > 0 ) {
        // check one more base
        if ( sx_4naIsGap1st(*raw_ptr) ) {
            --rem_len;
        }
    }
    return len-rem_len;
}


// Return 2na Seq-data for specified range.
// The data mustn't have ambiguities.
static
CRef<CSeq_data> sx_Get2na(const CVDBValueFor4Bits& read,
                          TSeqPos pos,
                          TSeqPos len)
{
    _ASSERT(len);
    CRef<CSeq_data> ret(new CSeq_data);
    vector<char>& data = ret->SetNcbi2na().Set();
    size_t dst_len = (len+3)/4;
    data.reserve(dst_len);
    TSeqPos raw_pos = pos+read.offset();
    const char* raw_ptr = read.raw_data()+(raw_pos/2);
    if ( raw_pos % 2 == 0 ) {
        // even - no shift
        // 4na (01,23) -> 2na (0123)
        while ( len >= 4 ) {
            // combine 4 bases
            Uint1 b2na0 = sx_4naFlags[Uint1(raw_ptr[0])]; // 01
            Uint1 b2na1 = sx_4naFlags[Uint1(raw_ptr[1])]; // 23
            _ASSERT(!(b2na0 & fBoth_4na));
            _ASSERT(!(b2na1 & fBoth_4na));
            raw_ptr += 2;
            Uint1 b2na = (b2na0<<4)+b2na1; // 0123
            data.push_back(b2na);
            len -= 4;
        }
        if ( len > 0 ) {
            // trailing odd bases 1..3
            Uint1 b2na;
            Uint1 b2na0 = sx_4naFlags[Uint1(raw_ptr[0])]; // 01
            if ( len == 1 ) {
                _ASSERT(!(b2na0 & f1st_4na));
                b2na = (b2na0<<4) & 0xc0; // 0xxx
            }
            else {
                _ASSERT(!(b2na0 & fBoth_4na));
                b2na = (b2na0<<4) & 0xff; // 01xx
                if ( len == 3 ) {
                    Uint1 b2na1 = sx_4naFlags[Uint1(raw_ptr[1])]; // 2x
                    _ASSERT(!(b2na1 & f1st_4na));
                    b2na += b2na1 & 0xc; // 012x
                }
            }
            data.push_back(b2na);
        }
    }
    else {
        // odd - shift
        // 4na (x0,12,3x) -> 2na (0123)
        while ( len >= 4 ) {
            // combine 4 bases
            Uint1 b2na0 = sx_4naFlags[Uint1(raw_ptr[0])]; // x0
            Uint1 b2na1 = sx_4naFlags[Uint1(raw_ptr[1])]; // 12
            Uint1 b2na2 = sx_4naFlags[Uint1(raw_ptr[2])]; // 3x
            raw_ptr += 2;
            _ASSERT(!(b2na0 & f2nd_4na));
            _ASSERT(!(b2na1 & fBoth_4na));
            _ASSERT(!(b2na2 & f1st_4na));
            Uint1 b2na = ((b2na0<<6)+(b2na1<<2)+((b2na2>>2)&3))&0xff; // 0123
            data.push_back(b2na);
            len -= 4;
        }
        if ( len > 0 ) {
            // trailing odd bases 1..3
            Uint1 b2na;
            Uint1 b2na0 = sx_4naFlags[Uint1(raw_ptr[0])]; // x0
            _ASSERT(!(b2na0 & f2nd_4na));
            b2na = b2na0<<6;
            if ( len > 1 ) {
                Uint1 b2na1 = sx_4naFlags[Uint1(raw_ptr[1])]; // 12
                if ( len == 2 ) {
                    _ASSERT(!(b2na1 & f1st_4na));
                    b2na1 &= 0xc; // 1x
                }
                else {
                    _ASSERT(!(b2na1 & fBoth_4na));
                }
                b2na += b2na1<<2; // 01xx or 012x
            }
            b2na &= 0xff;
            data.push_back(b2na);
        }
    }
    return ret;
}


// return 4na Seq-data for specified range
static
CRef<CSeq_data> sx_Get4na(const CVDBValueFor4Bits& read,
                          TSeqPos pos,
                          TSeqPos len)
{
    _ASSERT(len);
    CRef<CSeq_data> ret(new CSeq_data);
    vector<char>& data = ret->SetNcbi4na().Set();
    size_t dst_len = (len+1)/2;
    TSeqPos raw_pos = pos+read.offset();
    const char* raw_ptr = read.raw_data()+(raw_pos/2);
    if ( raw_pos % 2 == 0 ) {
        data.assign(raw_ptr, raw_ptr+dst_len);
        if ( len&1 ) {
            // mask unused nibble
            data.back() &= 0xf0;
        }
    }
    else {
        data.reserve(dst_len);
        Uint1 pv = *raw_ptr++;
        for ( ; len >= 2; len -= 2 ) {
            Uint1 v = *raw_ptr++;
            Uint1 b = (pv<<4)+(v>>4);
            data.push_back(b);
            pv = v;
        }
        if ( len ) {
            Uint1 b = (pv<<4);
            data.push_back(b);
        }
    }
    return ret;
}


// add raw data as delta segments
static
void sx_AddDelta(const CSeq_id& id,
                 CDelta_ext::Tdata& delta,
                 const CVDBValueFor4Bits& read,
                 TSeqPos pos,
                 TSeqPos len,
                 size_t gaps_count,
                 const INSDC_coord_one* gaps_start,
                 const INSDC_coord_len* gaps_len,
                 const NCBI_WGS_component_props* gaps_props,
                 const NCBI_WGS_gap_linkage* gaps_linkage)
{
    // kMin2naSize is the minimal size of 2na segment that will
    // save memory if inserted in between 4na segments.
    // It's determined by formula MinLen = 8*MemoryOverfeadOfSegment.
    // The memory overhead of a segment in total is
    // (assuming allocation overhead equal to 2 pointers):
    // 18*sizeof(void*)+7*sizeof(int)
    // (+sizeof(int) on some 64-bit platforms due to alignment).
    // So one segment memory overhead is 100 bytes on 32-bit platform,
    // and 176 bytes on 64-bit platform.
    // This leads to threshold size of 800 bases on 32-bit platforms and
    // 1408 bases on most 64-bit platforms.
    // We'll use slightly bigger threshold to take into account
    // possible CPU overhead for 2na operations.
    const TSeqPos kMin2naSize = 2048;

    // size of chinks if the segment is split
    const TSeqPos kChunk4naSize = 1<<16; // 64Ki bases or 32KiB
    const TSeqPos kChunk2naSize = 1<<17; // 128Ki bases or 32KiB

    // min size of segment to split
    const TSeqPos kSplit4naSize = kChunk4naSize+kChunk4naSize/4;
    const TSeqPos kSplit2naSize = kChunk2naSize+kChunk2naSize/4;

    // max size of gap segment
    const TSeqPos kMinGapSize = 20;
    const TSeqPos kMaxGapSize = 200;
    // size of gap segment if its actual size is unknown
    const TSeqPos kUnknownGapSize = 100;
    
    for ( ; len > 0; ) {
        CRef<CDelta_seq> seg(new CDelta_seq);
        TSeqPos gap_start = kInvalidSeqPos;
        if ( gaps_count ) {
            gap_start = *gaps_start;
            if ( pos == gap_start ) {
                TSeqPos gap_len = *gaps_len;
                if ( gap_len > len ) {
                    NCBI_THROW_FMT(CSraException, eDataError,
                                   "CWGSSeqIterator: "<<id.AsFastaString()<<
                                   ": gap at "<< pos << " is too long");
                }
                if ( !sx_IsGap(read, pos, gap_len) ) {
                    NCBI_THROW_FMT(CSraException, eDataError,
                                   "CWGSSeqIterator: "<<id.AsFastaString()<<
                                   ": gap at "<< pos << " has non-gap base");
                }
                sx_MakeGap(*seg, gap_len, *gaps_props,
                           gaps_linkage? *gaps_linkage: 0);
                --gaps_count;
                ++gaps_start;
                ++gaps_len;
                ++gaps_props;
                if ( gaps_linkage ) {
                    ++gaps_linkage;
                }
                delta.push_back(seg);
                len -= gap_len;
                pos += gap_len;
                continue;
            }
        }
        CSeq_literal& literal = seg->SetLiteral();

        TSeqPos rem_len = len;
        if ( gaps_count ) {
            rem_len = min(rem_len, gap_start-pos);
        }
        TSeqPos seg_len = sx_Get2naLength(read, pos, min(rem_len, kSplit2naSize));
        if ( seg_len >= kMin2naSize || seg_len == len ) {
            if ( seg_len >= kSplit2naSize ) {
                seg_len = kChunk2naSize;
            }
            literal.SetSeq_data(*sx_Get2na(read, pos, seg_len));
            _ASSERT(literal.GetSeq_data().GetNcbi2na().Get().size() == (seg_len+3)/4);
        }
        else {
            TSeqPos seg_len_2na = seg_len;
            seg_len += sx_Get4naLength(read, pos+seg_len,
                                       min(rem_len, kSplit4naSize)-seg_len,
                                       kMin2naSize, kMinGapSize);
            if ( kRecoverGaps && seg_len == 0 ) {
                seg_len = sx_GetGapLength(read, pos, min(rem_len, kMaxGapSize));
                _ASSERT(seg_len > 0);
                if ( seg_len == kUnknownGapSize ) {
                    literal.SetFuzz().SetLim(CInt_fuzz::eLim_unk);
                }
            }
            else if ( seg_len == seg_len_2na ) {
                literal.SetSeq_data(*sx_Get2na(read, pos, seg_len));
                _ASSERT(literal.GetSeq_data().GetNcbi2na().Get().size() == (seg_len+3)/4);
            }
            else {
                if ( seg_len >= kSplit4naSize ) {
                    seg_len = kChunk4naSize;
                }
                literal.SetSeq_data(*sx_Get4na(read, pos, seg_len));
                _ASSERT(literal.GetSeq_data().GetNcbi4na().Get().size() == (seg_len+1)/2);
            }
        }

        literal.SetLength(seg_len);
        delta.push_back(seg);
        pos += seg_len;
        len -= seg_len;
    }
}


CRef<CSeq_inst> CWGSSeqIterator::GetSeq_inst(TFlags flags) const
{
    x_CheckValid("GetSeq_inst");

    CRef<CSeq_inst> inst(new CSeq_inst);
    TSeqPos length = GetSeqLength();
    inst->SetLength(length);
    inst->SetMol(CSeq_inst::eMol_dna);
    inst->SetStrand(CSeq_inst::eStrand_ds);
    inst->SetRepr(CSeq_inst::eRepr_delta);
    CVDBValueFor4Bits read(m_Seq->READ(m_CurrId));
    if ( (flags & fMaskInst) == fInst_ncbi4na ) {
        inst->SetSeq_data(*sx_Get4na(read, 0, length));
    }
    else {
        CRef<CSeq_id> id = GetAccSeq_id();
        size_t gaps_count = 0;
        const INSDC_coord_one* gaps_start = 0;
        const INSDC_coord_len* gaps_len = 0;
        const NCBI_WGS_component_props* gaps_props = 0;
        const NCBI_WGS_gap_linkage* gaps_linkage = 0;
        if ( m_Seq->m_GAP_START ) {
            CVDBValueFor<INSDC_coord_one> start = m_Seq->GAP_START(m_CurrId);
            if ( start.size() ) {
                gaps_count = start.size();
                gaps_start = start.data();
                CVDBValueFor<INSDC_coord_len> len =
                    m_Seq->GAP_LEN(m_CurrId);
                CVDBValueFor<NCBI_WGS_component_props> props =
                    m_Seq->GAP_PROPS(m_CurrId);
                if ( len.size() != gaps_count || props.size() != gaps_count ) {
                    NCBI_THROW(CSraException, eDataError,
                               "CWGSSeqIterator: "+id->AsFastaString()+
                               " inconsistent gap info");
                }
                gaps_len = len.data();
                gaps_props = props.data();
                if ( m_Seq->m_GAP_LINKAGE ) {
                    CVDBValueFor<NCBI_WGS_gap_linkage> linkage =
                        m_Seq->GAP_LINKAGE(m_CurrId);
                    if ( linkage.size() != gaps_count ) {
                        NCBI_THROW(CSraException, eDataError,
                                   "CWGSSeqIterator: "+id->AsFastaString()+
                                   " inconsistent gap info");
                    }
                    gaps_linkage = linkage.data();
                }
            }
        }
        sx_AddDelta(*id, inst->SetExt().SetDelta().Set(), read, 0, length,
                    gaps_count, gaps_start, gaps_len, gaps_props, gaps_linkage);
    }
    return inst;
}


CRef<CBioseq> CWGSSeqIterator::GetBioseq(TFlags flags) const
{
    x_CheckValid("GetBioseq");

    CRef<CBioseq> ret(new CBioseq());
    GetIds(ret->SetId(), flags);
    if ( HasSeq_descr() ) {
        ret->SetDescr(*GetSeq_descr());
    }
    if ( HasAnnotSet() ) {
        GetAnnotSet(ret->SetAnnot());
    }
    ret->SetInst(*GetSeq_inst(flags));
    return ret;
}


/////////////////////////////////////////////////////////////////////////////
// CWGSScaffoldIterator
/////////////////////////////////////////////////////////////////////////////


CWGSScaffoldIterator::CWGSScaffoldIterator(void)
    : m_CurrId(0), m_FirstBadId(0)
{
}


CWGSScaffoldIterator::CWGSScaffoldIterator(const CWGSDb& wgs_db)
{
    x_Init(wgs_db);
}


CWGSScaffoldIterator::CWGSScaffoldIterator(const CWGSDb& wgs_db, uint64_t row)
{
    x_Init(wgs_db);
    if ( row < m_CurrId ) {
        m_FirstBadId = 0;
    }
    m_CurrId = row;
}


CWGSScaffoldIterator::CWGSScaffoldIterator(const CWGSDb& wgs_db, CTempString acc)
{
    if ( uint64_t row = wgs_db.ParseScaffoldRow(acc) ) {
        x_Init(wgs_db);
        if ( row < m_CurrId ) {
            m_FirstBadId = 0;
        }
        m_CurrId = row;
    }
    else {
        // bad format
        m_CurrId = 0;
        m_FirstBadId = 0;
    }
}


CWGSScaffoldIterator::~CWGSScaffoldIterator(void)
{
    if ( m_Scf ) {
        GetDb().Put(m_Scf);
    }
}


void CWGSScaffoldIterator::x_Init(const CWGSDb& wgs_db)
{
    m_Db = wgs_db;
    m_Scf = wgs_db.GetNCObject().Scf();
    if ( !m_Scf ) {
        m_CurrId = 0;
        m_FirstBadId = 0;
        return;
    }
    pair<int64_t, uint64_t> range = m_Scf->m_Cursor.GetRowIdRange();
    m_CurrId = range.first;
    m_FirstBadId = range.first+range.second;
}


void CWGSScaffoldIterator::x_ReportInvalid(const char* method) const
{
    NCBI_THROW_FMT(CSraException, eInvalidState,
                   "CWGSScaffoldIterator::"<<method<<"(): "
                   "Invalid iterator state");
}


CTempString CWGSScaffoldIterator::GetAccession(void) const
{
    x_CheckValid("GetAccession");
    if ( !m_Scf->m_ACCESSION ) {
        return CTempString();
    }
    return *CVDBStringValue(m_Scf->ACCESSION(m_CurrId));
}


CRef<CSeq_id> CWGSScaffoldIterator::GetAccSeq_id(void) const
{
    CRef<CSeq_id> id;
    CTempString acc = GetAccession();
    if ( !acc.empty() ) {
        id = new CSeq_id(acc);
        sx_SetVersion(*id, 1);
    }
    else {
        id = GetDb().GetScaffoldSeq_id(m_CurrId);
    }
    return id;
}


void CWGSScaffoldIterator::GetIds(CBioseq::TId& ids) const
{
    if ( CRef<CSeq_id> id = GetAccSeq_id() ) {
        // acc.ver
        ids.push_back(id);
    }
    if ( !GetDb().m_IdPrefixWithVersion.empty() ) {
        // gnl
        CTempString str = GetScaffoldName();
        if ( !str.empty() ) {
            CRef<CSeq_id> id(new CSeq_id);
            CDbtag& tag = id->SetGeneral();
            tag.SetDb("WGS:"+GetDb().m_IdPrefixWithVersion);
            tag.SetTag().SetStr(str);
            ids.push_back(id);
        }
    }
}


CTempString CWGSScaffoldIterator::GetScaffoldName(void) const
{
    x_CheckValid("GetScaffoldName");
    return *CVDBStringValue(m_Scf->SCAFFOLD_NAME(m_CurrId));
}


TSeqPos CWGSScaffoldIterator::GetSeqLength(void) const
{
    x_CheckValid("GetSeqLength");

    TSeqPos length = 0;
    CVDBValueFor<INSDC_coord_len> lens = m_Scf->COMPONENT_LEN(m_CurrId);
    for ( size_t i = 0; i < lens.size(); ++i ) {
        TSeqPos len = lens[i];
        length += len;
    }
    return length;
}


CRef<CSeq_inst> CWGSScaffoldIterator::GetSeq_inst(void) const
{
    x_CheckValid("GetSeq_inst");

    CRef<CSeq_inst> inst(new CSeq_inst);
    TSeqPos length = 0;
    inst->SetMol(CSeq_inst::eMol_dna);
    inst->SetStrand(CSeq_inst::eStrand_ds);
    inst->SetRepr(CSeq_inst::eRepr_delta);
    int id_ind = 0;
    CVDBValueFor<uint64_t> ids = m_Scf->COMPONENT_ID(m_CurrId);
    CVDBValueFor<INSDC_coord_len> lens = m_Scf->COMPONENT_LEN(m_CurrId);
    CVDBValueFor<INSDC_coord_one> starts = m_Scf->COMPONENT_START(m_CurrId);
    CVDBValueFor<NCBI_WGS_component_props> propss = m_Scf->COMPONENT_PROPS(m_CurrId);
    const NCBI_WGS_gap_linkage* linkages = 0;
    if ( m_Scf->m_COMPONENT_LINKAGE ) {
        linkages = m_Scf->COMPONENT_LINKAGE(m_CurrId).data();
    }
    CDelta_ext::Tdata& delta = inst->SetExt().SetDelta().Set();
    for ( size_t i = 0; i < lens.size(); ++i ) {
        TSeqPos len = lens[i];
        NCBI_WGS_component_props props = propss[i];
        CRef<CDelta_seq> seg(new CDelta_seq);
        if ( props < 0 ) {
            // gap
            sx_MakeGap(*seg, len, props, linkages? *linkages++: 0);
        }
        else {
            // contig
            TSeqPos start = starts[i];
            if ( start == 0 || len == 0 ) {
                NCBI_THROW_FMT(CSraException, eDataError,
                               "CWGSScaffoldIterator: component is bad for "+
                               GetAccSeq_id()->AsFastaString());
            }
            --start; // make start zero-based
            uint64_t row_id = ids[id_ind++];
            CSeq_interval& interval = seg->SetLoc().SetInt();
            interval.SetFrom(start);
            interval.SetTo(start+len-1);
            interval.SetId(*GetDb().GetContigSeq_id(row_id));
            if ( props & NCBI_WGS_strand_minus ) {
                interval.SetStrand(eNa_strand_minus);
            }
            else if ( props & NCBI_WGS_strand_plus ) {
                // default Seq-interval strand is plus
            }
            else {
                interval.SetStrand(eNa_strand_unknown);
            }
        }
        delta.push_back(seg);
        length += len;
    }
    inst->SetLength(length);
    return inst;
}


CRef<CBioseq> CWGSScaffoldIterator::GetBioseq(void) const
{
    x_CheckValid("GetBioseq");

    CRef<CBioseq> ret(new CBioseq());
    GetIds(ret->SetId());
    ret->SetInst(*GetSeq_inst());
    return ret;
}


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;
