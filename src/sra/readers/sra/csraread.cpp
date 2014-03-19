/* ===========================================================================
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
 *   Access to SRA files
 *
 */

#include <ncbi_pch.hpp>
#include <sra/readers/sra/csraread.hpp>
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
#include <objtools/readers/iidmapper.hpp>
#include <sra/error_codes.hpp>

#include <sra/readers/sra/kdbread.hpp>
#include <vdb/vdb-priv.h>
#include <kdb/table.h>
#include <kdb/meta.h>
#include <kdb/namelist.h>

#include <sstream>

BEGIN_NCBI_NAMESPACE;

#define NCBI_USE_ERRCODE_X   cSRAReader
NCBI_DEFINE_ERR_SUBCODE_X(1);

BEGIN_NAMESPACE(objects);


NCBI_PARAM_DECL(bool, CSRA, EXPLICIT_MATE_INFO);
NCBI_PARAM_DEF_EX(bool, CSRA, EXPLICIT_MATE_INFO, false,
                  eParam_NoThread, CSRA_EXPLICIT_MATE_INFO);


NCBI_PARAM_DECL(bool, CSRA, CIGAR_IN_ALIGN_EXT);
NCBI_PARAM_DEF_EX(bool, CSRA, CIGAR_IN_ALIGN_EXT, true,
                  eParam_NoThread, CSRA_CIGAR_IN_ALIGN_EXT);


NCBI_PARAM_DECL(bool, CSRA, INCLUDE_TECHNICAL_READS);
NCBI_PARAM_DEF_EX(bool, CSRA, INCLUDE_TECHNICAL_READS, false,
                  eParam_NoThread, CSRA_INCLUDE_TECHNICAL_READS);


NCBI_PARAM_DECL(bool, CSRA, CLIP_BY_QUALITY);
NCBI_PARAM_DEF_EX(bool, CSRA, CLIP_BY_QUALITY, true,
                  eParam_NoThread, CSRA_CLIP_BY_QUALITY);


NCBI_PARAM_DECL(bool, CSRA, PATH_IN_ID);
NCBI_PARAM_DEF_EX(bool, CSRA, PATH_IN_ID, true,
                  eParam_NoThread, CSRA_PATH_IN_ID);


static bool s_GetExplicitMateInfoParam(void)
{
    static NCBI_PARAM_TYPE(CSRA, EXPLICIT_MATE_INFO) s_Value;
    return s_Value.Get();
}


static bool s_GetCigarInAlignExt(void)
{
    static NCBI_PARAM_TYPE(CSRA, CIGAR_IN_ALIGN_EXT) s_Value;
    return s_Value.Get();
}


static bool s_GetIncludeTechnicalReads(void)
{
    static NCBI_PARAM_TYPE(CSRA, INCLUDE_TECHNICAL_READS) s_Value;
    return s_Value.Get();
}


static bool s_GetClipByQuality(void)
{
    static NCBI_PARAM_TYPE(CSRA, CLIP_BY_QUALITY) s_Value;
    return s_Value.Get();
}


static bool s_GetPathInId(void)
{
    NCBI_PARAM_TYPE(CSRA, PATH_IN_ID) s_Value;
    return s_Value.Get();
}


#define RC_NO_MORE_ALIGNMENTS RC(rcApp, rcQuery, rcSearching, rcRow, rcNotFound)


CCSraDb_Impl::SRefTableCursor::SRefTableCursor(const CVDB& db)
    : m_Table(db, "REFERENCE"),
      m_Cursor(m_Table),
      INIT_VDB_COLUMN(CGRAPH_HIGH),
      INIT_VDB_COLUMN(PRIMARY_ALIGNMENT_IDS),
      INIT_VDB_COLUMN(SECONDARY_ALIGNMENT_IDS),
      INIT_VDB_COLUMN(NAME),
      INIT_VDB_COLUMN(NAME_RANGE),
      INIT_VDB_COLUMN(SEQ_ID),
      INIT_VDB_COLUMN(SEQ_LEN),
      INIT_VDB_COLUMN(MAX_SEQ_LEN),
      INIT_VDB_COLUMN(READ),
      INIT_VDB_COLUMN(CIRCULAR)
{
}


CCSraDb_Impl::SAlnTableCursor::SAlnTableCursor(const CVDB& db,
                                               bool is_secondary)
    : m_Table(db, is_secondary? "SECONDARY_ALIGNMENT": "PRIMARY_ALIGNMENT"),
      m_Cursor(m_Table),
      m_IsSecondary(is_secondary),
      INIT_VDB_COLUMN(REF_NAME),
      INIT_VDB_COLUMN(REF_SEQ_ID),
      INIT_VDB_COLUMN(REF_POS),
      INIT_VDB_COLUMN(REF_LEN),
      INIT_VDB_COLUMN(REF_ORIENTATION),
      INIT_VDB_COLUMN(HAS_REF_OFFSET),
      INIT_VDB_COLUMN(HAS_MISMATCH),
      INIT_VDB_COLUMN(REF_OFFSET),
      INIT_VDB_COLUMN(CIGAR_SHORT),
      INIT_VDB_COLUMN(CIGAR_LONG),
      INIT_VDB_COLUMN(RAW_READ),
      INIT_VDB_COLUMN_BACKUP(MISMATCH_READ, READ),
      INIT_VDB_COLUMN(MISMATCH),
      INIT_VDB_COLUMN(SPOT_LEN),
      INIT_VDB_COLUMN(SEQ_SPOT_ID),
      INIT_VDB_COLUMN(SEQ_READ_ID),
      INIT_VDB_COLUMN(MAPQ),
      INIT_VDB_COLUMN(MATE_ALIGN_ID),
      INIT_VDB_COLUMN(QUALITY),
      INIT_VDB_COLUMN(SPOT_GROUP),
      INIT_OPTIONAL_VDB_COLUMN(NAME)
{
}


CCSraDb_Impl::SSeqTableCursor::SSeqTableCursor(const CVDB& db)
    : m_Table(db, "SEQUENCE"),
      m_Cursor(m_Table),
      INIT_VDB_COLUMN(SPOT_GROUP),
      INIT_VDB_COLUMN(READ_TYPE),
      INIT_VDB_COLUMN(READ_LEN),
      INIT_VDB_COLUMN(READ_START),
      INIT_VDB_COLUMN(READ),
      INIT_VDB_COLUMN(QUALITY),
      INIT_OPTIONAL_VDB_COLUMN(PRIMARY_ALIGNMENT_ID),
      INIT_VDB_COLUMN(TRIM_LEN),
      INIT_VDB_COLUMN(TRIM_START),
      INIT_OPTIONAL_VDB_COLUMN(NAME)
{
}


CCSraDb_Impl::SSeqTableCursor::SSeqTableCursor(const CCSraDb_Impl& db)
    : m_Table(db.m_Mgr, db.GetCSraPath()),
      m_Cursor(m_Table),
      INIT_VDB_COLUMN(SPOT_GROUP),
      INIT_VDB_COLUMN(READ_TYPE),
      INIT_VDB_COLUMN(READ_LEN),
      INIT_VDB_COLUMN(READ_START),
      INIT_VDB_COLUMN(READ),
      INIT_VDB_COLUMN(QUALITY),
      INIT_OPTIONAL_VDB_COLUMN(PRIMARY_ALIGNMENT_ID),
      INIT_VDB_COLUMN(TRIM_LEN),
      INIT_VDB_COLUMN(TRIM_START),
      INIT_OPTIONAL_VDB_COLUMN(NAME)
{
}


CCSraDb_Impl::SSeqTableCursor::SSeqTableCursor(const CVDBTable& table)
    : m_Cursor(table),
      INIT_VDB_COLUMN(SPOT_GROUP),
      INIT_VDB_COLUMN(READ_TYPE),
      INIT_VDB_COLUMN(READ_LEN),
      INIT_VDB_COLUMN(READ_START),
      INIT_VDB_COLUMN(READ),
      INIT_VDB_COLUMN(QUALITY),
      INIT_OPTIONAL_VDB_COLUMN(PRIMARY_ALIGNMENT_ID),
      INIT_VDB_COLUMN(TRIM_LEN),
      INIT_VDB_COLUMN(TRIM_START),
      INIT_OPTIONAL_VDB_COLUMN(NAME)
{
}


CCSraDb_Impl::CCSraDb_Impl(CVDBMgr& mgr, const string& csra_path,
                           IIdMapper* ref_id_mapper,
                           int ref_id_type)
    : m_Mgr(mgr),
      m_CSraPath(csra_path),
      m_RowSize(0)
{
    if ( s_GetPathInId() ) {
        m_SraIdPart = csra_path;
        // to avoid conflict with ref seq ids like gnl|SRA|SRR452437/scaffold_1
        // we replace all slashes ('/') with backslashes ('\\')
        NON_CONST_ITERATE ( string, it, m_SraIdPart ) {
            if ( *it == '/' ) {
                *it = '\\';
            }
        }
    }
    else {
        CDirEntry::SplitPath(csra_path, 0, &m_SraIdPart);
    }
    try {
        m_Db = CVDB(mgr, csra_path);
    }
    catch ( CSraException& exc ) {
        // We want to open old SRA schema files too.
        // In this case the DB object is not found, and the error is eDataError
        // any other error cannot allow to work with this accession
        if ( exc.GetErrCode() != CSraException::eDataError ) {
            // report all other exceptions as is
            throw;
        }
    }
    CRef<SRefTableCursor> ref;
    if ( m_Db ) {
        try {
            ref = Ref();
        }
        catch ( CSraException& exc ) {
            // allow missing REFERENCE table
            // any error except eNotFoundTable is not good
            if ( exc.GetErrCode() != CSraException::eNotFoundTable ) {
                throw;
            }
        }
    }
    if ( ref ) {
        SRefInfo info;
        m_RowSize = *ref->MAX_SEQ_LEN(1);
        for ( uint64_t row = 1, max_row = ref->m_Cursor.GetMaxRowId();
              row <= max_row; ) {
            // read range and names
            CVDBStringValue name(ref->NAME(row));
            ref->m_Cursor.SetParam("QUERY_SEQ_NAME", name);
            info.m_Name = *name;
            info.m_SeqId = *ref->SEQ_ID(row);
            x_MakeRefSeq_ids(info, ref_id_mapper, ref_id_type);
            _ASSERT(!info.m_Seq_ids.empty());
            CBioseq::TId::iterator best_id = info.m_Seq_ids.begin();
            // put the best id at front as the main id
            swap(info.GetMainSeq_id(), *best_id);
            info.m_Seq_id_Handle =
                CSeq_id_Handle::GetHandle(*info.GetMainSeq_id());

            const SRefTableCursor::row_range_t& range = *ref->NAME_RANGE(row);
            info.m_RowFirst = range.first;
            info.m_RowLast = range.second;
            info.m_SeqLength = kInvalidSeqPos;
            info.m_Circular = *ref->CIRCULAR(row);
            m_RefList.push_back(info);
            row = info.m_RowLast+1;
        }
        Put(ref);
        NON_CONST_ITERATE ( TRefInfoList, it, m_RefList ) {
            m_RefMapByName.insert
                (TRefInfoMapByName::value_type(it->m_Name, it));
            ITERATE ( CBioseq::TId, id_it, it->m_Seq_ids ) {
                CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(**id_it);
                m_RefMapBySeq_id.insert
                    (TRefInfoMapBySeq_id::value_type(idh, it));
            }
        }
    }
    else {
        // we'll work with SEQUENCE only in this case
        // check if the SEQUENCE table exists
        // They are opened with direct table access,
        // but the caller expects to see 'no data' error as eNotFoundDb.
        CRef<SSeqTableCursor> seq;
        try {
            seq = Seq();
        }
        catch ( CSraException& exc ) {
            // missing table is reported as eNotFoundTable
            if ( exc.GetErrCode() == CSraException::eNotFoundTable ) {
                // replace with eNotFoundDb, expected by caller
                NCBI_THROW3(CSraException, eNotFoundDb,
                            exc.GetMsg(), exc.GetRC(), exc.GetParam());
            }
            // report all other excetions as is
            throw;
        }
        Put(seq);
    }
}


void CCSraDb_Impl::GetSpotGroups(TSpotGroups& spot_groups)
{
    CKMDataNode parent(CKMetadata(m_Db, "SEQUENCE"), "STATS/SPOT_GROUP");
    CKNameList names(parent);
    for ( uint32_t i = 0; i < names.size(); ++i ) {
        const char* name = names[i];
        if ( CKMDataNode(CKMDataNode(parent, name), "SPOT_COUNT").GetUint8() ) {
            spot_groups.push_back(name);
        }
    }
}


void CCSraDb_Impl::x_MakeRefSeq_ids(SRefInfo& info,
                                    IIdMapper* ref_id_mapper,
                                    int ref_id_type)
{
    info.m_Seq_ids.clear();
    CRef<CSeq_id> ret;
    if ( ref_id_mapper ) {
        // first try NAME -> Seq-id mapping
        ret = new CSeq_id(CSeq_id::e_Local, info.m_Name);
        try {
            ref_id_mapper->MapObject(*ret);
            if ( !ret->IsLocal() ) {
                // mapped
                info.m_Seq_ids.push_back(ret);
                return;
            }
            CRef<CSeq_id> old(new CSeq_id(CSeq_id::e_Local, info.m_Name));
            if ( !ret->Equals(*old) ) {
                // mapped
                info.m_Seq_ids.push_back(ret);
                return;
            }
        }
        catch ( CException& /*ignored*/ ) {
        }
    }
    // second try SEQ_ID -> Seq-id mapping
    if ( ref_id_type == CCSraDb::eRefId_SEQ_ID ) {
        if ( info.m_SeqId.find('|') != NPOS ) {
            // try multiple FASTA ids
            try {
                CSeq_id::ParseIDs(info.m_Seq_ids, info.m_SeqId);
            }
            catch ( CSeqIdException& /*ignored*/ ) {
                info.m_Seq_ids.clear();
            }
        }
        if ( info.m_Seq_ids.empty() ) {
            try {
                ret = new CSeq_id(info.m_SeqId);
            }
            catch ( CSeqIdException& /*ignored*/ ) {
                ret = new CSeq_id(CSeq_id::e_Local, info.m_SeqId);
            }
        }
    }
    else {
        ret = new CSeq_id(CSeq_id::e_General,
                          "SRA",
                          m_CSraPath+'/'+info.m_Name);
    }
    if ( ret ) {
        info.m_Seq_ids.push_back(ret);
    }
    if ( ref_id_mapper ) {
        NON_CONST_ITERATE( CBioseq::TId, it, info.m_Seq_ids ) {
            try {
                ref_id_mapper->MapObject(**it);
            }
            catch ( CException& /*ignored*/ ) {
            }
        }
    }
}


CRef<CCSraDb_Impl::SRefTableCursor> CCSraDb_Impl::Ref(void)
{
    CRef<SRefTableCursor> curs = m_Ref.Get();
    if ( !curs ) {
        curs = new SRefTableCursor(m_Db);
    }
    return curs;
}


CRef<CCSraDb_Impl::SAlnTableCursor> CCSraDb_Impl::Aln(bool is_secondary)
{
    CRef<SAlnTableCursor> curs = m_Aln[is_secondary].Get();
    if ( !curs ) {
        curs = new SAlnTableCursor(m_Db, is_secondary);
    }
    return curs;
}


CRef<CCSraDb_Impl::SSeqTableCursor> CCSraDb_Impl::Seq(void)
{
    CRef<SSeqTableCursor> curs = m_Seq.Get();
    if ( !curs ) {
        if ( 1 ) {
            if ( m_Db ) {
                curs = new SSeqTableCursor(m_Db);
            }
            else {
                curs = new SSeqTableCursor(*this);
            }
        }
        else {
            if ( !m_SeqTable ) {
                if ( m_Db ) {
                    m_SeqTable = CVDBTable(m_Db, "SEQUENCE");
                }
                else {
                    m_SeqTable = CVDBTable(m_Mgr, GetCSraPath());
                }
            }
            curs = new SSeqTableCursor(m_SeqTable);
        }
    }
    return curs;
}


void CCSraDb_Impl::x_CalcSeqLength(const SRefInfo& info)
{
    CRef<SRefTableCursor> ref(Ref());
    TSeqPos last_len = *ref->SEQ_LEN(info.m_RowLast);
    info.m_SeqLength =
        GetRowSize()*TSeqPos(info.m_RowLast-info.m_RowFirst)+last_len;
    Put(ref);
}


CCSraRefSeqIterator::CCSraRefSeqIterator(const CCSraDb& csra_db)
    : m_Db(csra_db),
      m_Iter(csra_db->GetRefInfoList().begin())
{
}


CCSraRefSeqIterator::CCSraRefSeqIterator(const CCSraDb& csra_db,
                                         const string& seq_id)
{
    CCSraDb_Impl::TRefInfoMapBySeq_id::const_iterator iter =
        csra_db->m_RefMapBySeq_id.find(CSeq_id_Handle::GetHandle(seq_id));
    if ( iter != csra_db->m_RefMapBySeq_id.end() ) {
        m_Db = csra_db;
        m_Iter = iter->second;
    }
    else {
        ERR_POST_X(1, "RefSeq \""<<seq_id<<"\" not found.");
    }
}


CCSraRefSeqIterator::CCSraRefSeqIterator(const CCSraDb& csra_db,
                                         const string& name,
                                         EByName)
{
    CCSraDb_Impl::TRefInfoMapByName::const_iterator iter =
        csra_db->m_RefMapByName.find(name);
    if ( iter != csra_db->m_RefMapByName.end() ) {
        m_Db = csra_db;
        m_Iter = iter->second;
    }
}


CCSraRefSeqIterator::CCSraRefSeqIterator(const CCSraDb& csra_db,
                                         const CSeq_id_Handle& seq_id)
{
    CCSraDb_Impl::TRefInfoMapBySeq_id::const_iterator iter =
        csra_db->m_RefMapBySeq_id.find(seq_id);
    if ( iter != csra_db->m_RefMapBySeq_id.end() ) {
        m_Db = csra_db;
        m_Iter = iter->second;
    }
}


const CCSraDb_Impl::SRefInfo& CCSraRefSeqIterator::GetInfo(void) const
{
    if ( !*this ) {
        NCBI_THROW(CSraException, eInvalidState,
                   "CCSraRefSeqIterator is invalid");
    }
    return *m_Iter;
}


bool CCSraRefSeqIterator::IsCircular(void) const
{
    return GetInfo().m_Circular;
}


TSeqPos CCSraRefSeqIterator::GetSeqLength(void) const
{
    const CCSraDb_Impl::SRefInfo& info = **this;
    if ( info.m_SeqLength == kInvalidSeqPos ) {
        GetDb().x_CalcSeqLength(info);
    }
    return info.m_SeqLength;
}


size_t CCSraRefSeqIterator::GetRowAlignCount(int64_t row) const
{
    CRef<CCSraDb_Impl::SRefTableCursor> ref(GetDb().Ref());
    size_t ret = ref->PRIMARY_ALIGNMENT_IDS(row).size();
    GetDb().Put(ref);
    return ret;
}


CRef<CSeq_graph> CCSraRefSeqIterator::GetCoverageGraph(void) const
{
    CRef<CSeq_graph> graph(new CSeq_graph);

    const CCSraDb_Impl::SRefInfo& info = **this;
    TSeqPos size = TSeqPos(info.m_RowLast-info.m_RowFirst+1);
    CSeq_interval& loc_int = graph->SetLoc().SetInt();
    loc_int.SetId(info.GetMainSeq_id().GetNCObject());
    loc_int.SetFrom(0);
    loc_int.SetTo(GetSeqLength()-1);
    graph->SetNumval(size);
    TSeqPos row_size = m_Db->GetRowSize();
    graph->SetComp(row_size);
    CByte_graph& b_graph = graph->SetGraph().SetByte();
    b_graph.SetAxis(0);
    CByte_graph::TValues& values = b_graph.SetValues();
    values.resize(size);
    CRef<CCSraDb_Impl::SRefTableCursor> ref(GetDb().Ref());
    typedef CByte_graph::TValues::value_type TValue;
    TValue min_q = numeric_limits<TValue>::max();
    TValue max_q = 0;
    for ( size_t i = 0; i < size; ++i ) {
        uint64_t row = info.m_RowFirst+i;
        TValue q = *ref->CGRAPH_HIGH(row);
        values[i] = q;
        if ( q < min_q ) {
            min_q = q;
        }
        if ( q > max_q ) {
            max_q = q;
        }
        if ( row == info.m_RowLast ) {
            TSeqPos len = *ref->SEQ_LEN(row);
            loc_int.SetTo((size-1)*row_size+len);
        }
    }
    b_graph.SetMin(min_q);
    b_graph.SetMax(max_q);
    GetDb().Put(ref);
    return graph;
}


CRef<CSeq_annot> CCSraRefSeqIterator::GetCoverageAnnot(void) const
{
    CRef<CSeq_annot> annot = GetSeq_annot();
    annot->SetData().SetGraph().push_back(GetCoverageGraph());
    return annot;
}


CRef<CSeq_annot> CCSraRefSeqIterator::GetCoverageAnnot(const string& annot_name) const
{
    CRef<CSeq_annot> annot = GetSeq_annot(annot_name);
    annot->SetData().SetGraph().push_back(GetCoverageGraph());
    return annot;
}


CRef<CSeq_annot>
CCSraRefSeqIterator::MakeSeq_annot(const string& annot_name)
{
    CRef<CSeq_annot> annot(new CSeq_annot);
    annot->SetData().SetAlign();
    CRef<CAnnotdesc> desc(new CAnnotdesc);
    desc->SetName(annot_name);
    annot->SetDesc().Set().push_back(desc);
    return annot;
}


CRef<CSeq_annot>
CCSraRefSeqIterator::x_GetSeq_annot(const string* annot_name) const
{
    CRef<CSeq_annot> annot(new CSeq_annot);
    annot->SetData().SetAlign();
    if ( annot_name ) {
        CRef<CAnnotdesc> desc(new CAnnotdesc);
        desc->SetName(*annot_name);
        annot->SetDesc().Set().push_back(desc);
    }
    return annot;
}


CRef<CBioseq> CCSraRefSeqIterator::GetRefBioseq(ELoadData load) const
{
    CRef<CBioseq> seq(new CBioseq);
    const CCSraDb_Impl::SRefInfo& info = GetInfo();
    seq->SetId() = GetRefSeq_ids();
    const string& csra_path = m_Db->GetCSraPath();
    if ( csra_path.find('/') == NPOS && csra_path.find('\\') == NPOS ) {
        // SRR accession
        CRef<CSeqdesc> desc(new CSeqdesc);
        string title =
            info.m_Name + 
            " - reference sequence from ShortRead RUN " +
            csra_path;
        desc->SetTitle(title);
        seq->SetDescr().Set().push_back(desc);
    }
    CSeq_inst& inst = seq->SetInst();
    inst.SetRepr(inst.eRepr_delta);
    inst.SetMol(inst.eMol_na);
    inst.SetLength(GetSeqLength());
    inst.SetTopology
        (info.m_Circular? inst.eTopology_circular: inst.eTopology_linear);
    TLiterals literals;
    GetRefLiterals(literals, TRange::GetWhole(), load);
    NON_CONST_ITERATE ( TLiterals, it, literals ) {
        CRef<CDelta_seq> seq(new CDelta_seq);
        seq->SetLiteral(**it);
        inst.SetExt().SetDelta().Set().push_back(seq);
    }
    return seq;
}


void CCSraRefSeqIterator::GetRefLiterals(TLiterals& literals,
                                         const TRange& range,
                                         ELoadData load) const
{
    const CCSraDb_Impl::SRefInfo& info = GetInfo();
    TSeqPos segment_len = GetDb().GetRowSize();
    CRef<CCSraDb_Impl::SRefTableCursor> ref(GetDb().Ref());
    for ( uint64_t row = info.m_RowFirst+range.GetFrom()/segment_len;
          row <= info.m_RowLast; ++row ) {
        TSeqPos pos = TSeqPos(row-info.m_RowFirst)*segment_len;
        if ( pos >= range.GetToOpen() ) {
            break;
        }
        CRef<CSeq_literal> literal(new CSeq_literal);
        if ( row == info.m_RowLast ) {
            segment_len = *ref->SEQ_LEN(row);
        }
        literal->SetLength(segment_len);
        if ( load == eLoadData ) {
            literal->SetSeq_data().SetIupacna().Set() = *ref->READ(row);
        }
        literals.push_back(literal);
    }
    GetDb().Put(ref);
}


/////////////////////////////////////////////////////////////////////////////
// CCSraAlignIterator
/////////////////////////////////////////////////////////////////////////////

CCSraAlignIterator::CCSraAlignIterator(void)
    : m_Error(RC_NO_MORE_ALIGNMENTS)
{
}


CCSraAlignIterator::CCSraAlignIterator(const CCSraDb& csra_db,
                                       const string& ref_id,
                                       TSeqPos ref_pos,
                                       TSeqPos window)
    : m_RefIter(csra_db, CSeq_id_Handle::GetHandle(ref_id)),
      m_Ref(m_RefIter.GetDb().Ref()),
      m_Error(RC_NO_MORE_ALIGNMENTS),
      m_ArgRefPos(0),
      m_ArgRefLast(0) 
{
    Select(ref_pos, window);
}


CCSraAlignIterator::CCSraAlignIterator(const CCSraDb& csra_db,
                                       const CSeq_id_Handle& ref_id,
                                       TSeqPos ref_pos,
                                       TSeqPos window)
    : m_RefIter(csra_db, ref_id),
      m_Ref(m_RefIter.GetDb().Ref()),
      m_Error(RC_NO_MORE_ALIGNMENTS),
      m_ArgRefPos(0),
      m_ArgRefLast(0) 
{
    Select(ref_pos, window);
}


CCSraAlignIterator::~CCSraAlignIterator(void)
{
    if ( m_Ref ) {
        m_RefIter.GetDb().Put(m_Ref);
    }
    if ( m_Aln ) {
        m_RefIter.GetDb().Put(m_Aln);
    }
}


void CCSraAlignIterator::Select(TSeqPos ref_pos,
                                TSeqPos window)
{
    m_Error = RC_NO_MORE_ALIGNMENTS;
    m_ArgRefPos = m_ArgRefLast = 0;

    m_RefRowNext = m_RefRowLast = 0;
    m_AlnRowIsSecondary = true;
    m_AlnRowCur = m_AlnRowEnd = 0;

    if ( !m_Ref ) {
        return;
    }

    m_ArgRefPos = ref_pos;
    m_ArgRefLast = ref_pos+window-1 < ref_pos? kInvalidSeqPos: ref_pos+window-1;
    
    TSeqPos row_size = GetDb().GetRowSize();
    const CCSraDb_Impl::SRefInfo& info = *m_RefIter;
    m_RefRowNext = info.m_RowFirst + max(m_ArgRefPos, row_size)/row_size - 1;
    m_RefRowLast = min(info.m_RowLast, info.m_RowFirst + m_ArgRefLast/row_size);
    m_AlnRowCur = m_AlnRowEnd = 0;
    m_AlnRowIsSecondary = true;
    x_Settle();
}


void CCSraAlignIterator::x_Settle(void)
{
    m_Error = RC_NO_MORE_ALIGNMENTS;
    for ( ;; ) {
        if ( m_AlnRowCur == m_AlnRowEnd ) {
            if ( m_RefRowNext > m_RefRowLast ) {
                return;
            }
            
            if ( m_AlnRowIsSecondary ) {
                m_AlnRowIsSecondary = false;
                CVDBValueFor<uint64_t> ids =
                    m_Ref->PRIMARY_ALIGNMENT_IDS(m_RefRowNext);
                m_AlnRowCur = ids.data();
                m_AlnRowEnd = m_AlnRowCur + ids.size();
            }
            else {
                m_AlnRowIsSecondary = true;
                CVDBValueFor<uint64_t> ids =
                    m_Ref->SECONDARY_ALIGNMENT_IDS(m_RefRowNext);
                m_AlnRowCur = ids.data();
                m_AlnRowEnd = m_AlnRowCur + ids.size();
                ++m_RefRowNext;
            }
            if ( m_AlnRowCur != m_AlnRowEnd ) {
                if ( !m_Aln || m_Aln->m_IsSecondary != m_AlnRowIsSecondary ) {
                    m_RefIter.GetDb().Put(m_Aln);
                    m_Aln = m_RefIter.GetDb().Aln(m_AlnRowIsSecondary);
                }
            }
        }
        else {
            m_CurRefPos = *m_Aln->REF_POS(*m_AlnRowCur);
            if ( m_CurRefPos > m_ArgRefLast ) {
                // skip remaining non-overlapping
                m_AlnRowCur = m_AlnRowEnd;
                continue;
            }
            m_CurRefLen = *m_Aln->REF_LEN(*m_AlnRowCur);
            TSeqPos end = m_CurRefPos + m_CurRefLen;
            if ( end <= m_ArgRefPos ) {
                // skip non-overlapping
                ++m_AlnRowCur;
                continue;
            }
            m_Error = 0;
            break;
        }
    }
}


uint64_t CCSraAlignIterator::GetAlignmentId(void) const
{
    return *m_AlnRowCur;
}


CTempString CCSraAlignIterator::GetRefSeqId(void) const
{
    return *CVDBStringValue(m_Aln->REF_SEQ_ID(*m_AlnRowCur));
}


bool CCSraAlignIterator::GetRefMinusStrand(void) const
{
    return *m_Aln->REF_ORIENTATION(*m_AlnRowCur);
}


CTempString CCSraAlignIterator::GetSpotGroup(void) const
{
    return *CVDBStringValue(m_Aln->SPOT_GROUP(*m_AlnRowCur));
}


bool CCSraAlignIterator::IsSetName(void) const
{
    return m_Aln->m_NAME && !GetName().empty();
}


CTempString CCSraAlignIterator::GetName(void) const
{
    return *CVDBStringValue(m_Aln->NAME(*m_AlnRowCur));
}


CTempString CCSraAlignIterator::GetCIGAR(void) const
{
    return *CVDBStringValue(m_Aln->CIGAR_SHORT(*m_AlnRowCur));
}


CTempString CCSraAlignIterator::GetCIGARLong(void) const
{
    return *CVDBStringValue(m_Aln->CIGAR_LONG(*m_AlnRowCur));
}


CTempString CCSraAlignIterator::GetMismatchRead(void) const
{
    return *CVDBStringValue(m_Aln->MISMATCH_READ(*m_AlnRowCur));
}


CTempString CCSraAlignIterator::GetMismatchRaw(void) const
{
    return *CVDBStringValue(m_Aln->MISMATCH(*m_AlnRowCur));
}


uint64_t CCSraAlignIterator::GetShortId1(void) const
{
    return *m_Aln->SEQ_SPOT_ID(*m_AlnRowCur);
}


uint32_t CCSraAlignIterator::GetShortId2(void) const
{
    return *m_Aln->SEQ_READ_ID(*m_AlnRowCur);
}


CRef<CSeq_id> CCSraDb_Impl::MakeShortReadId(uint64_t id1, uint32_t id2) const
{
    CRef<CSeq_id> ret(new CSeq_id);
    CDbtag& dbtag = ret->SetGeneral();
    dbtag.SetDb("SRA");
    SetShortReadId(dbtag.SetTag().SetStr(), id1, id2);
    return ret;
}


void CCSraDb_Impl::SetShortReadId(string& str,
                                  uint64_t id1, uint32_t id2) const
{
    ostringstream s;
    s << m_SraIdPart << '.' << id1 << '.' << id2;
    str = s.str();
}


CRef<CSeq_id> CCSraAlignIterator::GetShortSeq_id(void) const
{
    return GetDb().MakeShortReadId(GetShortId1(), GetShortId2());
}


CRef<CSeq_id> CCSraAlignIterator::GetMateShortSeq_id(void) const
{
    CVDBValueFor<uint64_t> value = m_Aln->MATE_ALIGN_ID(*m_AlnRowCur);
    if ( value.size() == 0 ) {
        return null;
    }
    _ASSERT(value.size() == 1);
    uint64_t mate_id = *value;
    _ASSERT(mate_id);
    return GetDb().MakeShortReadId(*m_Aln->SEQ_SPOT_ID(mate_id),
                                   *m_Aln->SEQ_READ_ID(mate_id));
}


TSeqPos CCSraAlignIterator::GetShortPos(void) const
{
    bool has_off = m_Aln->HAS_REF_OFFSET(*m_AlnRowCur)[0] == '1';
    if ( has_off ) {
        int off = m_Aln->REF_OFFSET(*m_AlnRowCur)[0];
        return -off;
    }
    else {
        return 0;
    }
}


TSeqPos CCSraAlignIterator::GetShortLen(void) const
{
    return *m_Aln->SPOT_LEN(*m_AlnRowCur);
}


int CCSraAlignIterator::GetMapQuality(void) const
{
    return *m_Aln->MAPQ(*m_AlnRowCur);
}


void CCSraAlignIterator::MakeFullMismatch(string& ret,
                                          CTempString cigar,
                                          CTempString mismatch) const
{
    CVDBValueFor<char> has_mismatch = m_Aln->HAS_MISMATCH(*m_AlnRowCur);
    ret.reserve(mismatch.size()+2);
    const char* cptr = cigar.data();
    const char* cend = cigar.end();
    const char* mptr = mismatch.data();
    const char* mend = mismatch.end();
    
    TSeqPos seqpos = 0;
    while ( cptr != cend ) {
        char type = 0;
        TSeqPos seglen = 0;
        while ( cptr != cend ) {
            char c = *cptr++;
            if ( c >= '0' && c <= '9' ) {
                seglen = seglen*10+(c-'0');
            }
            else {
                type = c;
                break;
            }
        }
        if ( !type ) {
            NCBI_THROW_FMT(CSraException, eDataError,
                           "Incomplete CIGAR: " << cigar);
        }
        if ( seglen == 0 ) {
            NCBI_THROW_FMT(CSraException, eDataError,
                           "Bad CIGAR length: " << type <<
                           "0 in " << cigar);
        }
        if ( type == '=' ) {
            // match
            seqpos += seglen;
        }
        else if ( type == 'X' ) {
            // mismatch
            if ( mptr + seglen > mend ) {
                NCBI_THROW_FMT(CSraException, eDataError,
                               "CIGAR mismatch segment beyond MISMATCH: "
                               <<cigar<<" vs "<<mismatch);
            }
            ret.append(mptr, seglen);
            mptr += seglen;
            seqpos += seglen;
        }
        else if ( type == 'I' ) {
            if ( seqpos + seglen > has_mismatch.size() ) {
                NCBI_THROW_FMT(CSraException, eDataError,
                               "CIGAR insert segment beyond HAS_MISMATCH: "
                               <<cigar<<" vs "<<mismatch);
            }
            for ( TSeqPos i = 0; i < seglen; ++i, ++seqpos ) {
                char c;
                if ( has_mismatch[seqpos] == '1' ) {
                    if ( mptr == mend ) {
                        NCBI_THROW_FMT(CSraException, eDataError,
                                       "CIGAR insert/mismatch segment beyond MISMATCH: "
                                       <<cigar<<" vs "<<mismatch);
                        
                    }
                    c = *mptr++;
                }
                else {
                    c = '=';
                }
                ret += c;
            }
        }
        else if ( type == 'D' || type == 'N' || type == 'S' || type == 'P' ) {
            continue;
        }
        else {
            NCBI_THROW_FMT(CSraException, eDataError,
                           "Bad CIGAR char: " <<type<< " in " <<cigar);
        }
    }
    /* soft clips S are not accounted yet
    if ( mptr != mend ) {
        NCBI_THROW_FMT(CSraException, eDataError,
                       "Extra mismatch chars: " <<cigar<< " vs " <<mismatch);
    }
    */
}


CRef<CSeq_align> CCSraAlignIterator::GetMatchAlign(void) const
{
    CRef<CSeq_align> align(new CSeq_align);
    align->SetType(CSeq_align::eType_diags);
    CDense_seg& denseg = align->SetSegs().SetDenseg();
    CDense_seg::TIds& ids = denseg.SetIds();
    ids.reserve(2);
    ids.push_back(GetRefSeq_id());
    ids.push_back(GetShortSeq_id());

    TSeqPos refpos = GetRefSeqPos();
    TSeqPos seqpos = GetShortPos();
    CTempString cigar = GetCIGAR();
    const char* ptr = cigar.data();
    const char* end = cigar.end();

    TSeqPos segcount = 0;
    for ( const char* p = ptr; p != end; ++p ) {
        char c = *p;
        if ( c < '0' || c > '9' ) {
            if ( c == 'S' || c == 'N' ) {
                // soft clipping already accounted in seqpos
                // skip N segments
                continue;
            }
            ++segcount;
        }
    }
    
    CDense_seg::TStarts& starts = denseg.SetStarts();
    CDense_seg::TLens& lens = denseg.SetLens();
    starts.reserve(2*segcount);
    lens.reserve(segcount);
    
    TSeqPos insert_size = 0;
    TSeqPos refstart = 0, seqstart = 0;
    while ( ptr != end ) {
        char type = 0;
        TSeqPos seglen = 0;
        while ( ptr != end ) {
            char c = *ptr++;
            if ( c >= '0' && c <= '9' ) {
                seglen = seglen*10+(c-'0');
            }
            else {
                type = c;
                break;
            }
        }
        if ( !type ) {
            NCBI_THROW_FMT(CSraException, eDataError,
                           "Incomplete CIGAR: " << cigar);
        }
        if ( seglen == 0 ) {
            NCBI_THROW_FMT(CSraException, eDataError,
                           "Bad CIGAR length: " << type <<
                           "0 in " << cigar);
        }
        if ( type == 'M' || type == '=' || type == 'X' ) {
            // match
            refstart = refpos;
            refpos += seglen;
            seqstart = seqpos;
            seqpos += seglen;
        }
        else if ( type == 'I' || type == 'S' ) {
            if ( type == 'S' ) {
                // soft clipping already accounted in seqpos
                continue;
            }
            insert_size += seglen;
            refstart = kInvalidSeqPos;
            seqstart = seqpos;
            seqpos += seglen;
        }
        else if ( type == 'D' || type == 'N' ) {
            // delete
            refstart = refpos;
            refpos += seglen;
            if ( type == 'N' ) {
                // skip N segments
                continue;
            }
            seqstart = kInvalidSeqPos;
        }
        else if ( type != 'P' ) {
            NCBI_THROW_FMT(CSraException, eDataError,
                           "Bad CIGAR char: " <<type<< " in " <<cigar);
        }
        starts.push_back(refstart);
        starts.push_back(seqstart);
        lens.push_back(seglen);
    }
    if ( GetRefMinusStrand() ) {
        CDense_seg::TStrands& strands = denseg.SetStrands();
        strands.reserve(2*segcount);
        TSeqPos end = GetShortLen();
        for ( size_t i = 0; i < segcount; ++i ) {
            strands.push_back(eNa_strand_plus);
            strands.push_back(eNa_strand_minus);
            TSeqPos pos = starts[i*2+1];
            TSeqPos len = lens[i];
            if ( pos != kInvalidSeqPos ) {
                starts[i*2+1] = end - (pos + len);
            }
        }
    }

    denseg.SetNumseg(segcount);

    if ( s_GetExplicitMateInfoParam() ) {
        CVDBValueFor<uint64_t> mate_id_v = m_Aln->MATE_ALIGN_ID(*m_AlnRowCur);
        if ( mate_id_v.size() != 0 ) {
            _ASSERT(mate_id_v.size() == 1);
            uint64_t mate_id = *mate_id_v;
            _ASSERT(mate_id);
            CRef<CUser_object> obj(new CUser_object());
            obj->SetType(x_GetObject_id("Mate read", m_ObjectIdMateRead));
            CTempString mate_name = *CVDBStringValue(m_Aln->REF_NAME(mate_id));
            if ( mate_name != m_RefIter->m_Name ) {
                CCSraRefSeqIterator mate_iter
                    (m_RefIter.m_Db, mate_name, CCSraRefSeqIterator::eByName);
                if ( mate_iter ) {
                    x_AddField(*obj,
                               "RefId", mate_iter.GetRefSeq_id()->AsFastaString(),
                               m_ObjectIdRefId);
                }
                else {
                    CTempString mate_ref_id =
                        *CVDBStringValue(m_Aln->REF_SEQ_ID(mate_id));
                    x_AddField(*obj,
                               "RefId", mate_ref_id,
                               m_ObjectIdRefId);
                }
            }
            x_AddField(*obj,
                       "RefPos", *m_Aln->REF_POS(mate_id),
                       m_ObjectIdRefPos);
            
            CRef<CUser_field> field(new CUser_field());
            field->SetLabel(x_GetObject_id("gnl|SRA|", m_ObjectIdLcl));
            GetDb().SetShortReadId(field->SetData().SetStr(),
                                   *m_Aln->SEQ_SPOT_ID(mate_id),
                                   *m_Aln->SEQ_READ_ID(mate_id));
            obj->SetData().push_back(field);

            align->SetExt().push_back(obj);
        }
    }

    if ( s_GetCigarInAlignExt() ) {
        CRef<CUser_object> obj(new CUser_object);
        obj->SetType(x_GetObject_id("Tracebacks", m_ObjectIdTracebacks));
        CTempString cigar = GetCIGARLong();
        CTempString mismatch = GetMismatchRaw();
        x_AddField(*obj, "CIGAR", cigar,
                   m_ObjectIdCIGAR, m_UserFieldCacheCigar, 8, 8192);
        if ( insert_size == 0 ) {
            x_AddField(*obj, "MISMATCH", mismatch,
                       m_ObjectIdMISMATCH, m_UserFieldCacheMismatch, 8, 8192);
        }
        else {
            CUser_field& field = x_AddField(*obj, "MISMATCH",
                                            m_ObjectIdMISMATCH);
            MakeFullMismatch(field.SetData().SetStr(), cigar, mismatch);
        }
        align->SetExt().push_back(obj);
    }

    if ( IsSecondary() ) {
        align->SetExt().push_back(x_GetSecondaryIndicator());
    }

    return align;
}


CRef<CSeq_graph> CCSraAlignIterator::GetQualityGraph(void) const
{
    CRef<CSeq_graph> graph(new CSeq_graph);
    graph->SetTitle("Phred Quality");
    CSeq_interval& loc_int = graph->SetLoc().SetInt();
    loc_int.SetId(*GetShortSeq_id());
    TSeqPos size = GetShortLen();
    loc_int.SetFrom(0);
    loc_int.SetTo(size-1);
    if ( GetRefMinusStrand() ) {
        loc_int.SetStrand(eNa_strand_minus);
    }
    graph->SetNumval(size);
    CByte_graph& b_graph = graph->SetGraph().SetByte();
    b_graph.SetAxis(0);
    CByte_graph::TValues& values = b_graph.SetValues();
    values.resize(size);
    CVDBValueFor<INSDC_quality_phred> qual = m_Aln->QUALITY(*m_AlnRowCur);
    typedef CByte_graph::TValues::value_type TValue;
    TValue min_q = numeric_limits<TValue>::max();
    TValue max_q = 0;
    for ( size_t i = 0; i < size; ++i ) {
        TValue q = qual[i];
        values[i] = q;
        if ( q < min_q ) {
            min_q = q;
        }
        if ( q > max_q ) {
            max_q = q;
        }
    }
    b_graph.SetMin(min_q);
    b_graph.SetMax(max_q);
    return graph;
}


CRef<CBioseq> CCSraAlignIterator::GetShortBioseq(void) const
{
    CRef<CBioseq> seq(new CBioseq);
    seq->SetId().push_back(GetShortSeq_id());
    if ( IsSetName() ) {
        CRef<CSeqdesc> desc(new CSeqdesc);
        desc->SetTitle(GetName());
        seq->SetDescr().Set().push_back(desc);
    }
    CSeq_inst& inst = seq->SetInst();
    inst.SetRepr(inst.eRepr_raw);
    inst.SetMol(inst.eMol_na);
    CVDBStringValue data(m_Aln->RAW_READ(*m_AlnRowCur));
    TSeqPos length = TSeqPos(data.size());
    inst.SetLength(length);
    string& iupac = inst.SetSeq_data().SetIupacna().Set();
    iupac.assign(data.data(), length);
    return seq;
}


CRef<CAnnotdesc> CCSraAlignIterator::MakeMatchAnnotIndicator(void)
{
    CRef<CAnnotdesc> desc(new CAnnotdesc);
    CUser_object& obj = desc->SetUser();
    obj.SetType().SetStr("Mate read");
    obj.AddField("Match by local Seq-id", true);
    return desc;
}


CRef<CSeq_annot>
CCSraAlignIterator::x_GetEmptyMatchAnnot(const string* annot_name) const
{
    CRef<CSeq_annot> annot = x_GetSeq_annot(annot_name);
    if ( !s_GetExplicitMateInfoParam() ) {
        if ( !m_MatchAnnotIndicator ) {
            m_MatchAnnotIndicator = MakeMatchAnnotIndicator();
        }
        annot->SetDesc().Set().push_back(m_MatchAnnotIndicator);
    }
    return annot;
}


CRef<CSeq_annot>
CCSraAlignIterator::MakeEmptyMatchAnnot(const string& annot_name)
{
    CRef<CSeq_annot> annot = MakeSeq_annot(annot_name);
    annot->SetDesc().Set().push_back(MakeMatchAnnotIndicator());
    return annot;
}


CRef<CSeq_annot>
CCSraAlignIterator::x_GetMatchAnnot(const string* annot_name) const
{
    CRef<CSeq_annot> annot = x_GetEmptyMatchAnnot(annot_name);
    annot->SetData().SetAlign().push_back(GetMatchAlign());
    return annot;
}


CRef<CSeq_annot>
CCSraAlignIterator::x_GetQualityGraphAnnot(const string* annot_name) const
{
    CRef<CSeq_annot> annot = x_GetSeq_annot(annot_name);
    annot->SetData().SetGraph().push_back(GetQualityGraph());
    return annot;
}


CRef<CSeq_entry>
CCSraAlignIterator::x_GetMatchEntry(const string* annot_name) const
{
    CRef<CSeq_entry> entry(new CSeq_entry);
    CRef<CBioseq> seq = GetShortBioseq();
    seq->SetAnnot().push_back(x_GetMatchAnnot(annot_name));
    entry->SetSeq(*seq);
    return entry;
}


CRef<CUser_object> CCSraAlignIterator::x_GetSecondaryIndicator(void) const
{
    if ( !m_SecondaryIndicator ) {
        m_SecondaryIndicator = new CUser_object();
        m_SecondaryIndicator->SetType().SetStr("Secondary");
        m_SecondaryIndicator->SetData();
    }
    return m_SecondaryIndicator;
}


CObject_id& CCSraAlignIterator::x_GetObject_id(const char* name,
                                               TObjectIdCache& cache) const
{
    if ( !cache ) {
        cache = new CObject_id();
        cache->SetStr(name);
    }
    return *cache;
}


CUser_field& CCSraAlignIterator::x_AddField(CUser_object& obj,
                                            const char* name,
                                            TObjectIdCache& cache) const
{
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel(x_GetObject_id(name, cache));
    obj.SetData().push_back(field);
    return *field;
}


void CCSraAlignIterator::x_AddField(CUser_object& obj,
                                    const char* name,
                                    CTempString value,
                                    TObjectIdCache& cache) const
{
    x_AddField(obj, name, cache).SetData().SetStr(value);
}


void CCSraAlignIterator::x_AddField(CUser_object& obj,
                                    const char* name,
                                    int value,
                                    TObjectIdCache& cache) const
{
    x_AddField(obj, name, cache).SetData().SetInt(value);
}


void CCSraAlignIterator::x_AddField(CUser_object& obj,
                                    const char* name,
                                    CTempString value,
                                    TObjectIdCache& id_cache,
                                    TUserFieldCache& cache,
                                    size_t max_value_length,
                                    size_t max_cache_size) const
{
    if ( value.size() > max_value_length || cache.size() > max_cache_size ) {
        x_AddField(obj, name, value, id_cache);
        return;
    }
    TUserFieldCache::iterator iter = cache.lower_bound(value);
    if ( iter == cache.end() || iter->first != value ) {
        CRef<CUser_field> field(new CUser_field());
        field->SetLabel(x_GetObject_id(name, id_cache));
        field->SetData().SetStr(value);
        CTempString key = field->GetData().GetStr();
        iter = cache.insert(iter, TUserFieldCache::value_type(key, field));
    }
    obj.SetData().push_back(iter->second);
}


/////////////////////////////////////////////////////////////////////////////
// CCSraShortReadIterator

CCSraShortReadIterator::CCSraShortReadIterator(void)
    : m_SpotId(0),
      m_MaxSpotId(0),
      m_ReadId(1),
      m_MaxReadId(0),
      m_IncludeTechnicalReads(false),
      m_ClipByQuality(false),
      m_Error(RC_NO_MORE_ALIGNMENTS)
{
}


CCSraShortReadIterator::CCSraShortReadIterator(const CCSraDb& csra_db,
                                               EClipType clip_type)
{
    x_Init(csra_db, clip_type);
    if ( *this ) {
        x_GetMaxReadId();
        x_Settle();
    }
}


CCSraShortReadIterator::CCSraShortReadIterator(const CCSraDb& csra_db,
                                               uint64_t spot_id,
                                               EClipType clip_type)
{
    x_Init(csra_db, clip_type);
    Select(spot_id);
}


CCSraShortReadIterator::CCSraShortReadIterator(const CCSraDb& csra_db,
                                               uint64_t spot_id,
                                               uint32_t read_id,
                                               EClipType clip_type)
{
    x_Init(csra_db, clip_type);
    Select(spot_id, read_id);
}


void CCSraShortReadIterator::x_Init(const CCSraDb& csra_db,
                                    EClipType clip_type)
{
    m_Db = csra_db;
    m_Seq = csra_db.GetNCObject().Seq();
    m_ReadId = 1;
    m_MaxReadId = 0;
    m_IncludeTechnicalReads = s_GetIncludeTechnicalReads();
    switch ( clip_type ) {
    case eNoClip:
        m_ClipByQuality = false;
        break;
    case eClipByQuality: 
        m_ClipByQuality = true;
        break;
    default:
        m_ClipByQuality = s_GetClipByQuality();
        break;
    }
    pair<int64_t, uint64_t> range = m_Seq->m_Cursor.GetRowIdRange();
    m_SpotId = range.first;
    m_MaxSpotId = range.first+range.second-1;
    m_Error = m_SpotId <= m_MaxSpotId? 0: RC_NO_MORE_ALIGNMENTS;
}


bool CCSraShortReadIterator::Select(uint64_t spot_id)
{
    m_SpotId = spot_id;
    m_ReadId = 1;
    if ( spot_id < 1 || spot_id > m_MaxSpotId ) {
        // bad spot id
        m_Error = RC_NO_MORE_ALIGNMENTS;
        return false;
    }
    x_GetMaxReadId();
    return x_Settle(true); // settle within single spot id
}


bool CCSraShortReadIterator::Select(uint64_t spot_id,
                                    uint32_t read_id)
{
    m_SpotId = spot_id;
    m_ReadId = read_id;
    if ( spot_id < 1 || spot_id > m_MaxSpotId ) {
        // bad spot id
        m_Error = RC_NO_MORE_ALIGNMENTS;
        return false;
    }
    x_GetMaxReadId();
    if ( m_ReadId > m_MaxReadId ) {
        // bad read id
        m_Error = RC_NO_MORE_ALIGNMENTS;
        return false;
    }
    if ( !x_ValidRead() ) {
        // bad read type
        m_Error = RC_NO_MORE_ALIGNMENTS;
        return false;
    }
    m_Error = 0;
    return true;
}


CCSraShortReadIterator::~CCSraShortReadIterator(void)
{
    if ( m_Seq ) {
        GetDb().Put(m_Seq);
    }
}


bool CCSraShortReadIterator::x_ValidRead(void) const
{
    if ( !m_IncludeTechnicalReads && IsTechnicalRead() ) {
        return false;
    }
    if ( GetReadRange().Empty() ) {
        return false;
    }
    return true;
}


bool CCSraShortReadIterator::x_Settle(bool single_spot)
{
    for ( ;; ) {
        if ( m_ReadId > m_MaxReadId ) {
            // next spot, first read
            if ( single_spot ) {
                // bad read id
                m_Error = RC_NO_MORE_ALIGNMENTS;
                return false;
            }
            m_ReadId = 1;
            if ( ++m_SpotId > m_MaxSpotId ) {
                // bad spot id
                m_MaxReadId = 0;
                m_Error = RC_NO_MORE_ALIGNMENTS;
                return false;
            }
            x_GetMaxReadId();
        }
        else {
            if ( x_ValidRead() ) {
                // found
                m_Error = 0;
                return true;
            }
            // next read id
            ++m_ReadId;
        }
    }
}


void CCSraShortReadIterator::x_GetMaxReadId(void)
{
    m_MaxReadId = uint32_t(m_Seq->READ_TYPE(m_SpotId).size());
}


CRef<CSeq_id> CCSraShortReadIterator::GetShortSeq_id(void) const
{
    return GetDb().MakeShortReadId(GetShortId1(), GetShortId2());
}


CTempString CCSraShortReadIterator::GetSpotGroup(void) const
{
    return *CVDBStringValue(m_Seq->SPOT_GROUP(m_SpotId));
}


bool CCSraShortReadIterator::IsSetName(void) const
{
    return m_Seq->m_NAME && !GetName().empty();
}


CTempString CCSraShortReadIterator::GetName(void) const
{
    return *CVDBStringValue(m_Seq->NAME(m_SpotId));
}


bool CCSraShortReadIterator::IsTechnicalRead(void) const
{
    INSDC_read_type type = m_Seq->READ_TYPE(m_SpotId)[m_ReadId-1];
    return (type & (SRA_READ_TYPE_TECHNICAL | SRA_READ_TYPE_BIOLOGICAL)) ==
        SRA_READ_TYPE_TECHNICAL;
}


TSeqPos CCSraShortReadIterator::GetClipQualityLeft(void) const
{
    return m_Seq->TRIM_START(m_SpotId);
}


TSeqPos CCSraShortReadIterator::GetClipQualityRight(void) const
{
    // inclusive
    return m_Seq->TRIM_START(m_SpotId) + m_Seq->TRIM_LEN(m_SpotId) - 1;
}


bool CCSraShortReadIterator::HasClippingInfo(void) const
{
    TSeqPos pos = m_Seq->READ_START(m_SpotId)[m_ReadId-1];
    TSeqPos trim_pos = m_Seq->TRIM_START(m_SpotId);
    if ( trim_pos > pos ) {
        return true;
    }
    TSeqPos len = m_Seq->READ_LEN(m_SpotId)[m_ReadId-1];
    TSeqPos end = pos + len;
    TSeqPos trim_len = m_Seq->TRIM_LEN(m_SpotId);
    TSeqPos trim_end = trim_pos + trim_len;
    if ( trim_end < end ) {
        return true;
    }
    return false;
}


COpenRange<TSeqPos>
CCSraShortReadIterator::GetReadRange(EClipType clip_type) const
{
    TSeqPos len = m_Seq->READ_LEN(m_SpotId)[m_ReadId-1];
    if ( !len ) {
        return TOpenRange::GetEmpty();
    }
    TSeqPos pos = m_Seq->READ_START(m_SpotId)[m_ReadId-1];
    TSeqPos end = pos + len;
    bool clip_by_quality = clip_type == eDefaultClip?
        m_ClipByQuality: clip_type == eClipByQuality;
    if ( clip_by_quality ) {
        TSeqPos trim_pos = m_Seq->TRIM_START(m_SpotId);
        pos = max(pos, trim_pos);
        TSeqPos trim_len = m_Seq->TRIM_LEN(m_SpotId);
        TSeqPos trim_end = trim_pos + trim_len;
        end = min(end, trim_end);
    }
    return TOpenRange(pos, end);
}


CTempString
CCSraShortReadIterator::x_GetReadData(const TOpenRange& range) const
{
    CTempString s = *m_Seq->READ(m_SpotId);
    return s.substr(range.GetFrom(), range.GetLength());
}


CTempString CCSraShortReadIterator::GetReadData(EClipType clip_type) const
{
    return x_GetReadData(GetReadRange(clip_type));
}


CRef<CBioseq>
CCSraShortReadIterator::GetShortBioseq(TBioseqFlags flags) const
{
    CRef<CBioseq> seq(new CBioseq);
    seq->SetId().push_back(GetShortSeq_id());
    if ( IsSetName() ) {
        CRef<CSeqdesc> desc(new CSeqdesc);
        desc->SetTitle(GetName());
        seq->SetDescr().Set().push_back(desc);
    }
    CSeq_inst& inst = seq->SetInst();
    inst.SetRepr(inst.eRepr_raw);
    inst.SetMol(inst.eMol_na);
    TOpenRange range = GetReadRange();
    CTempString data = x_GetReadData(range);
    inst.SetLength(TSeqPos(data.size()));
    string& iupac = inst.SetSeq_data().SetIupacna().Set();
    iupac.assign(data.data(), data.size());
    if ( flags & fQualityGraph ) {
        seq->SetAnnot().push_back(x_GetQualityGraphAnnot(range, 0));
    }
    return seq;
}


CRef<CSeq_graph>
CCSraShortReadIterator::x_GetQualityGraph(const TOpenRange& range) const
{
    CRef<CSeq_graph> graph(new CSeq_graph);
    graph->SetTitle("Phred Quality");
    CSeq_interval& loc_int = graph->SetLoc().SetInt();
    loc_int.SetId(*GetShortSeq_id());
    TSeqPos size = range.GetLength();
    loc_int.SetFrom(0);
    loc_int.SetTo(size-1);
    graph->SetNumval(size);
    CByte_graph& b_graph = graph->SetGraph().SetByte();
    b_graph.SetAxis(0);
    CByte_graph::TValues& values = b_graph.SetValues();
    values.resize(size);
    CVDBValueFor<INSDC_quality_phred> qual = m_Seq->QUALITY(m_SpotId);
    TSeqPos start = range.GetFrom();
    typedef CByte_graph::TValues::value_type TValue;
    TValue min_q = numeric_limits<TValue>::max();
    TValue max_q = 0;
    for ( size_t i = 0; i < size; ++i ) {
        TValue q = qual[start+i];
        values[i] = q;
        if ( q < min_q ) {
            min_q = q;
        }
        if ( q > max_q ) {
            max_q = q;
        }
    }
    b_graph.SetMin(min_q);
    b_graph.SetMax(max_q);
    return graph;
}


CRef<CSeq_graph>
CCSraShortReadIterator::GetQualityGraph(void) const
{
    return x_GetQualityGraph(GetReadRange());
}


CRef<CSeq_annot>
CCSraShortReadIterator::x_GetSeq_annot(const string* annot_name) const
{
    CRef<CSeq_annot> annot(new CSeq_annot);
    annot->SetData().SetAlign();
    if ( annot_name ) {
        CRef<CAnnotdesc> desc(new CAnnotdesc);
        desc->SetName(*annot_name);
        annot->SetDesc().Set().push_back(desc);
    }
    return annot;
}


CRef<CSeq_annot>
CCSraShortReadIterator::x_GetQualityGraphAnnot(const TOpenRange& range,
                                               const string* annot_name) const
{
    CRef<CSeq_annot> annot = x_GetSeq_annot(annot_name);
    annot->SetData().SetGraph().push_back(x_GetQualityGraph(range));
    return annot;
}


CRef<CSeq_annot>
CCSraShortReadIterator::x_GetQualityGraphAnnot(const string* annot_name) const
{
    return x_GetQualityGraphAnnot(GetReadRange(), annot_name);
}


CCSraRefSeqIterator CCSraShortReadIterator::GetRefSeqIter(TSeqPos* ref_pos_ptr) const
{
    CCSraRefSeqIterator ret;
    if ( m_Seq->m_PRIMARY_ALIGNMENT_ID ) {
        CVDBValueFor<uint64_t> align_ids = m_Seq->PRIMARY_ALIGNMENT_ID(m_SpotId);
        if ( uint64_t row_id = align_ids[m_ReadId-1] ) {
            CRef<CCSraDb_Impl::SAlnTableCursor> aln = GetDb().Aln(false);
            ret = CCSraRefSeqIterator(m_Db, aln->REF_NAME(row_id),
                                      CCSraRefSeqIterator::eByName);
            if ( ref_pos_ptr ) {
                *ref_pos_ptr = aln->REF_POS(row_id);
            }
            GetDb().Put(aln);
        }
    }
    return ret;
}


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;
