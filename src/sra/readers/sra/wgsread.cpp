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


#define NCBI_WGS_VOL_PATH NCBI_TRACES04_PATH "/wgs01/WGS/"


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
      INIT_OPTIONAL_VDB_COLUMN(GB_STATE)
{
}


CWGSDb_Impl::SScfTableCursor::SScfTableCursor(const CVDB& db)
    : m_Table(db, "SCAFFOLD"),
      m_Cursor(m_Table),
      INIT_OPTIONAL_VDB_COLUMN(ACCESSION),
      INIT_VDB_COLUMN(COMPONENT_ID),
      INIT_VDB_COLUMN(COMPONENT_START),
      INIT_VDB_COLUMN(COMPONENT_LEN),
      INIT_VDB_COLUMN(COMPONENT_PROPS),
      INIT_VDB_COLUMN(SCAFFOLD_NAME)
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
    NCBI_THROW_FMT(CSraException, eOtherError,
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
        NCBI_THROW_FMT(CSraException, eOtherError,
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


static const Int1 sx_4na_to_2na_two[256] = {
    //  +0  +1      +2              +3
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 0
    -1,  0,  1, -1,  2, -1, -1, -1,  3, -1, -1, -1, -1, -1, -1, -1, // 1 +0
    -1,  4,  5, -1,  6, -1, -1, -1,  7, -1, -1, -1, -1, -1, -1, -1, // 2 +4
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 3
    -1,  8,  9, -1, 10, -1, -1, -1, 11, -1, -1, -1, -1, -1, -1, -1, // 4 +8
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 5
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 6
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 7
    -1, 12, 13, -1, 14, -1, -1, -1, 15, -1, -1, -1, -1, -1, -1, -1, // 8 +12
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 9
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 10
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 11
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 12
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 13
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, // 14
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1  // 15
};


static inline
bool sx_Is2naBoth(Uint1 b4na)
{
    return sx_4na_to_2na_two[b4na] >= 0;
}


static inline
bool sx_Is2na1st(Uint1 b4na)
{
    return sx_Is2naBoth((b4na&0xf0)+0x01);
}


static inline
bool sx_Is2na2nd(Uint1 b4na)
{
    return sx_Is2naBoth((b4na&0x0f)+0x10);
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
        if ( !sx_Is2na2nd(*raw_ptr) ) {
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
        if ( !sx_Is2naBoth(*raw_ptr) ) {
            return false;
        }
        ++raw_ptr;
        len -= 2;
    }
    if ( len > 0 ) {
        // check one more base
        if ( !sx_Is2na1st(*raw_ptr) ) {
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
        if ( !sx_Is2na2nd(*raw_ptr) ) {
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
        if ( !sx_Is2naBoth(b4na) ) {
            if ( sx_Is2na1st(b4na) ) {
                --rem_len;
            }
            return len-rem_len;
        }
        ++raw_ptr;
        rem_len -= 2;
    }
    if ( rem_len > 0 ) {
        // check one more base
        if ( sx_Is2na1st(*raw_ptr) ) {
            --rem_len;
        }
    }
    return len-rem_len;
}


static inline
TSeqPos sx_Get4naLength(const CVDBValueFor4Bits& read,
                        TSeqPos pos,
                        TSeqPos len,
                        TSeqPos stop_2na_len)
{
    if ( len < stop_2na_len ) {
        return len;
    }
    TSeqPos raw_pos = pos+read.offset();
    const char* raw_ptr = read.raw_data()+(raw_pos/2);
    TSeqPos rem_len = len, len2na = 0;
    if ( raw_pos % 2 != 0 ) {
        // check odd base
        if ( sx_Is2na2nd(*raw_ptr) ) {
            len2na = 1;
        }
        else if ( len == stop_2na_len ) {
            return len;
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
        if ( sx_Is2naBoth(b4na) ) {
            ++raw_ptr;
            rem_len -= 2;
            len2na += 2;
            if ( len2na >= stop_2na_len ) {
                return len-(rem_len+len2na);
            }
        }
        else {
            if ( sx_Is2na1st(b4na) && len2na == stop_2na_len-1 ) {
                return len-(rem_len+len2na);
            }
            ++raw_ptr;
            rem_len -= 2;
            len2na = sx_Is2na2nd(b4na);
            if ( len2na+rem_len < stop_2na_len ) {
                return len;
            }
        }
    }
    if ( rem_len > 0 ) {
        // check one more base
        --rem_len;
        if ( sx_Is2na1st(*raw_ptr) ) {
            ++len2na;
        }
        else {
            return len;
        }
    }
    return len2na >= stop_2na_len? len-len2na: len;
}


static
CRef<CSeq_data> sx_Get2na(const CVDBValueFor4Bits& read,
                          TSeqPos pos,
                          TSeqPos len)
{
    CRef<CSeq_data> ret(new CSeq_data);
    vector<char>& data = ret->SetNcbi2na().Set();
    size_t dst_len = (len+3)/4;
    data.reserve(dst_len);
    TSeqPos raw_pos = pos+read.offset();
    const char* raw_ptr = read.raw_data()+(raw_pos/2);
    if ( raw_pos % 2 == 0 ) {
        // even - no shift
        while ( len >= 4 ) {
            // combine 4 b
            Uint1 b4na0 = raw_ptr[0]; // 01
            Uint1 b4na1 = raw_ptr[1]; // 23
            raw_ptr += 2;
            Uint1 b2na0 = sx_4na_to_2na_two[b4na0]; // 01
            Uint1 b2na1 = sx_4na_to_2na_two[b4na1]; // 23
            Uint1 b2na = b2na0*16+b2na1; // 0123
            data.push_back(b2na);
            len -= 4;
        }
        if ( len > 0 ) {
            Uint1 b2na;
            Uint1 b4na0 = raw_ptr[0]; // 01
            if ( len == 1 ) {
                b4na0 &= 0xf0; // 0x
                Uint1 b2na0 = sx_4na_to_2na_two[b4na0+0x01]; // 0x
                b2na = b2na0*16; // 0xxx
            }
            else if ( len >= 2 ) {
                Uint1 b2na0 = sx_4na_to_2na_two[b4na0];   // 01
                b2na = b2na0*16; // 01xx
                if ( len == 3 ) {
                    Uint1 b4na1 = raw_ptr[1]; // 2x
                    Uint1 b2na1 = sx_4na_to_2na_two[(b4na1&0xf0)+0x01];
                    b2na += b2na1; // 012x
                }
            }
            data.push_back(b2na);
        }
    }
    else {
        // odd - shift
        while ( len >= 4 ) {
            // combine 4 b
            Uint1 b4na0 = raw_ptr[0]; // x0
            Uint1 b4na1 = raw_ptr[1]; // 12
            Uint1 b4na2 = raw_ptr[2]; // 2x
            raw_ptr += 2;
            b4na0 = b4na0*16 + (b4na1>>4); // 01
            b4na1 = b4na1*16 + (b4na2>>4); // 23
            Uint1 b2na0 = sx_4na_to_2na_two[b4na0]; // 01
            Uint1 b2na1 = sx_4na_to_2na_two[b4na1]; // 23
            Uint1 b2na = b2na0*16+b2na1; // 0123
            data.push_back(b2na);
            len -= 4;
        }
        if ( len > 0 ) {
            Uint1 b2na;
            Uint1 b4na0 = raw_ptr[0]; // x0
            if ( len == 1 ) {
                b4na0 *= 16; // 0x
                b2na = sx_4na_to_2na_two[b4na0+0x01]; // 0x
                b2na *= 16; // 0xxx
            }
            else if ( len >= 2 ) {
                Uint1 b4na1 = raw_ptr[1]; // 12
                b4na0 = b4na0*16 + (b4na1>>4); // 01
                b2na = sx_4na_to_2na_two[b4na0]; // 01
                b2na *= 16; // 01xx
                if ( len == 3 ) {
                    b4na1 *= 16; // 2x
                    b2na += sx_4na_to_2na_two[b4na1+0x01]; // 012x
                }
            }
            data.push_back(b2na);
        }
    }
    return ret;
}


static
CRef<CSeq_data> sx_Get4na(const CVDBValueFor4Bits& read,
                          TSeqPos pos,
                          TSeqPos len)
{
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
void sx_AddDelta(CDelta_ext::Tdata& delta,
                 const CVDBValueFor4Bits& read,
                 TSeqPos pos,
                 TSeqPos len)
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
        
    for ( ; len > 0; ) {
        TSeqPos limit_len = min(len, kSplit2naSize);
        TSeqPos seg_len = sx_Get2naLength(read, pos, limit_len);
        CRef<CDelta_seq> seg(new CDelta_seq);
        CSeq_literal& literal = seg->SetLiteral();
        if ( seg_len >= kMin2naSize || seg_len == len ) {
            if ( seg_len >= kSplit2naSize ) {
                seg_len = min(seg_len, kChunk2naSize);
            }
            literal.SetSeq_data(*sx_Get2na(read, pos, seg_len));
            _ASSERT(literal.GetSeq_data().GetNcbi2na().Get().size() == (seg_len+3)/4);
        }
        else {
            TSeqPos limit2_len = min(len, kSplit4naSize);
            seg_len += sx_Get4naLength(read, pos+seg_len,
                                       limit2_len-seg_len, kMin2naSize);
            if ( seg_len >= kSplit4naSize ) {
                seg_len = min(seg_len, kChunk4naSize);
            }
            literal.SetSeq_data(*sx_Get4na(read, pos, seg_len));
            _ASSERT(literal.GetSeq_data().GetNcbi4na().Get().size() == (seg_len+1)/2);
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
    if ( flags & fInst_ncbi4na ) {
        inst->SetSeq_data(*sx_Get4na(read, 0, length));
    }
    else {
        sx_AddDelta(inst->SetExt().SetDelta().Set(), read, 0, length);
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
    NCBI_THROW_FMT(CSraException, eOtherError,
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


static
void sx_AddEvidence(CSeq_gap& gap, CLinkage_evidence::TType type)
{
    CRef<CLinkage_evidence> evidence(new CLinkage_evidence);
    evidence->SetType(type);
    gap.SetLinkage_evidence().push_back(evidence);
}


static
void sx_MakeGap(CDelta_seq& seg, TSeqPos len, NCBI_WGS_component_props props)
{
    _ASSERT(props < 0);
    CSeq_literal& literal = seg.SetLiteral();
    int len_type    = -(-props & ( 3* 1));
    int gap_type    = -(-props & (15* 4));
    int gap_linkage = -(-props & (15*64));
    literal.SetLength(len);
    if ( len_type == NCBI_WGS_gap_unknown ) {
        literal.SetFuzz().SetLim(CInt_fuzz::eLim_unk);
    }
    if ( gap_type || gap_linkage ) {
        CSeq_gap& gap = literal.SetSeq_data().SetGap();
        switch ( gap_type ) {
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
        int linkage_bits = (-gap_linkage)/(-NCBI_WGS_gap_linkage_paired_ends);
        _ASSERT(linkage_bits >= 0);
        for ( int i = 0; linkage_bits >= (1<<i); ++i ) {
            if ( linkage_bits & (1<<i) ) {
                sx_AddEvidence(gap, i+CLinkage_evidence::eType_paired_ends);
            }
        }
    }
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
    CDelta_ext::Tdata& delta = inst->SetExt().SetDelta().Set();
    for ( size_t i = 0; i < lens.size(); ++i ) {
        TSeqPos len = lens[i];
        TSeqPos start = starts[i];
        NCBI_WGS_component_props props = propss[i];
        CRef<CDelta_seq> seg(new CDelta_seq);
        if ( props < 0 ) {
            // gap
            sx_MakeGap(*seg, len, props);
        }
        else {
            // contig
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
