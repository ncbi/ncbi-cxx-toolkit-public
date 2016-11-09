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
 *   Access to SNP files
 *
 */

#include <ncbi_pch.hpp>
#include <sra/readers/sra/snpread.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbi_param.hpp>
#include <objects/general/general__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <objects/seqres/seqres__.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seqtable/seqtable__.hpp>
#include <sra/error_codes.hpp>
#include <objmgr/impl/snp_annot_info.hpp>
#include <unordered_map>

#include <sra/readers/sra/kdbread.hpp>

BEGIN_STD_NAMESPACE;

template<>
struct hash<ncbi::CTempString>
{
    size_t operator()(ncbi::CTempString val) const
        {
            unsigned long __h = 5381;
            for ( auto c : val ) {
                __h = __h*17 + c;
            }
            return size_t(__h);
        }
};

END_STD_NAMESPACE;

BEGIN_NCBI_NAMESPACE;

#define NCBI_USE_ERRCODE_X   SNPReader
NCBI_DEFINE_ERR_SUBCODE_X(1);

BEGIN_NAMESPACE(objects);


#define RC_NO_MORE_ALIGNMENTS RC(rcApp, rcQuery, rcSearching, rcRow, rcNotFound)

static const TSeqPos kPageSize = 5000;
static const TSeqPos kMaxSNPLength = 256;
static const TSeqPos kOverviewZoom = kPageSize;
static const TSeqPos kCoverageZoom = 100;
static const size_t kMax_AlleleLength  = 32;

static const char kDefaultAnnotName[] = "SNP";

static const char kFeatSubtypesToChars[] = "U-VSMLDIR";

/////////////////////////////////////////////////////////////////////////////
// CSNPDb_Impl
/////////////////////////////////////////////////////////////////////////////


struct CSNPDb_Impl::SSeqTableCursor : public CObject {
    explicit SSeqTableCursor(const CVDBTable& table);
    
    CVDBCursor m_Cursor;

    DECLARE_VDB_COLUMN_AS_STRING(ACCESSION);
    DECLARE_VDB_COLUMN_AS(INSDC_coord_len, LEN);
};


struct CSNPDb_Impl::STrackTableCursor : public CObject {
    explicit STrackTableCursor(const CVDBTable& table);
    
    CVDBCursor m_Cursor;

    DECLARE_VDB_COLUMN_AS(Uint8, BITS);
    DECLARE_VDB_COLUMN_AS(Uint8, MASK);
    DECLARE_VDB_COLUMN_AS_STRING(NAME);
};


struct CSNPDb_Impl::SGraphTableCursor : public CObject {
    explicit SGraphTableCursor(const CVDBTable& table);
    
    CVDBCursor m_Cursor;

    DECLARE_VDB_COLUMN_AS(TVDBRowId, FILTER_ID_ROW_NUM);
    DECLARE_VDB_COLUMN_AS(TVDBRowId, SEQ_ID_ROW_NUM);
    DECLARE_VDB_COLUMN_AS(INSDC_coord_zero, BLOCK_FROM);
    DECLARE_VDB_COLUMN_AS(Uint4, GR_TOTAL);
    DECLARE_VDB_COLUMN_AS(Uint4, GR_ZOOM);
};


struct CSNPDb_Impl::SPageTableCursor : public CObject {
    explicit SPageTableCursor(const CVDBTable& table);
    
    CVDBCursor m_Cursor;

    DECLARE_VDB_COLUMN_AS(TVDBRowId, SEQ_ID_ROW_NUM);
    DECLARE_VDB_COLUMN_AS(INSDC_coord_zero, PAGE_FROM);
    DECLARE_VDB_COLUMN_AS(TVDBRowId, FEATURE_ROW_FROM);
    DECLARE_VDB_COLUMN_AS(TVDBRowCount, FEATURE_ROWS_COUNT);
};


struct CSNPDb_Impl::SFeatTableCursor : public CObject {
    explicit SFeatTableCursor(const CVDBTable& table);
    
    CVDBCursor m_Cursor;

    DECLARE_VDB_COLUMN_AS(TVDBRowId, SEQ_ID_ROW_NUM);
    DECLARE_VDB_COLUMN_AS(INSDC_coord_zero, FROM);
    DECLARE_VDB_COLUMN_AS(INSDC_coord_len, LEN);
    DECLARE_VDB_COLUMN_AS_STRING(FEAT_TYPE);
    DECLARE_VDB_COLUMN_AS(Uint4, FEAT_SUBTYPE);
    DECLARE_VDB_COLUMN_AS(Uint4, FEAT_ID_PREFIX);
    DECLARE_VDB_COLUMN_AS(Uint8, FEAT_ID_VALUE);
    DECLARE_VDB_COLUMN_AS(Uint8, BIT_FLAGS);
    DECLARE_VDB_COLUMN_AS(TVDBRowId, EXTRA_ROW_FROM);
    DECLARE_VDB_COLUMN_AS(TVDBRowCount, EXTRA_ROWS_COUNT);
};


struct CSNPDb_Impl::SExtraTableCursor : public CObject {
    explicit SExtraTableCursor(const CVDBTable& table);
    
    CVDBCursor m_Cursor;

    DECLARE_VDB_COLUMN_AS_STRING(RS_ALLELE);
};


CSNPDb_Impl::SSeqTableCursor::SSeqTableCursor(const CVDBTable& table)
    : m_Cursor(table),
      INIT_VDB_COLUMN(ACCESSION),
      INIT_VDB_COLUMN(LEN)
{
}


CSNPDb_Impl::STrackTableCursor::STrackTableCursor(const CVDBTable& table)
    : m_Cursor(table),
      INIT_VDB_COLUMN(BITS),
      INIT_VDB_COLUMN(MASK),
      INIT_VDB_COLUMN(NAME)
{
}


CSNPDb_Impl::SGraphTableCursor::SGraphTableCursor(const CVDBTable& table)
    : m_Cursor(table),
      INIT_VDB_COLUMN(FILTER_ID_ROW_NUM),
      INIT_VDB_COLUMN(SEQ_ID_ROW_NUM),
      INIT_VDB_COLUMN(BLOCK_FROM),
      INIT_VDB_COLUMN(GR_TOTAL),
      INIT_VDB_COLUMN(GR_ZOOM)
{
}


CSNPDb_Impl::SPageTableCursor::SPageTableCursor(const CVDBTable& table)
    : m_Cursor(table),
      INIT_VDB_COLUMN(SEQ_ID_ROW_NUM),
      INIT_VDB_COLUMN(PAGE_FROM),
      INIT_VDB_COLUMN(FEATURE_ROW_FROM),
      INIT_VDB_COLUMN(FEATURE_ROWS_COUNT)
{
}


CSNPDb_Impl::SFeatTableCursor::SFeatTableCursor(const CVDBTable& table)
    : m_Cursor(table),
      INIT_VDB_COLUMN(SEQ_ID_ROW_NUM),
      INIT_VDB_COLUMN(FROM),
      INIT_VDB_COLUMN(LEN),
      INIT_VDB_COLUMN(FEAT_TYPE),
      INIT_VDB_COLUMN(FEAT_SUBTYPE),
      INIT_VDB_COLUMN(FEAT_ID_PREFIX),
      INIT_VDB_COLUMN(FEAT_ID_VALUE),
      INIT_VDB_COLUMN(BIT_FLAGS),
      INIT_VDB_COLUMN(EXTRA_ROW_FROM),
      INIT_VDB_COLUMN(EXTRA_ROWS_COUNT)
{
}


CSNPDb_Impl::SExtraTableCursor::SExtraTableCursor(const CVDBTable& table)
    : m_Cursor(table),
      INIT_VDB_COLUMN(RS_ALLELE)
{
}


CRef<CSNPDb_Impl::SGraphTableCursor> CSNPDb_Impl::Graph(TVDBRowId row)
{
    CRef<SGraphTableCursor> curs = m_Graph.Get(row);
    if ( !curs ) {
        curs = new SGraphTableCursor(GraphTable());
    }
    return curs;
}


CRef<CSNPDb_Impl::SPageTableCursor> CSNPDb_Impl::Page(TVDBRowId row)
{
    CRef<SPageTableCursor> curs = m_Page.Get(row);
    if ( !curs ) {
        curs = new SPageTableCursor(PageTable());
    }
    return curs;
}


CRef<CSNPDb_Impl::SFeatTableCursor> CSNPDb_Impl::Feat(TVDBRowId row)
{
    CRef<SFeatTableCursor> curs = m_Feat.Get(row);
    if ( !curs ) {
        curs = new SFeatTableCursor(FeatTable());
    }
    return curs;
}


CRef<CSNPDb_Impl::SExtraTableCursor> CSNPDb_Impl::Extra(TVDBRowId row)
{
    CRef<SExtraTableCursor> curs = m_Extra.Get(row);
    if ( !curs ) {
        if ( const CVDBTable& table = ExtraTable() ) {
            curs = new SExtraTableCursor(table);
        }
    }
    return curs;
}


void CSNPDb_Impl::Put(CRef<SGraphTableCursor>& curs, TVDBRowId row)
{
    m_Graph.Put(curs, row);
}


void CSNPDb_Impl::Put(CRef<SPageTableCursor>& curs, TVDBRowId row)
{
    m_Page.Put(curs, row);
}


void CSNPDb_Impl::Put(CRef<SFeatTableCursor>& curs, TVDBRowId row)
{
    m_Feat.Put(curs, row);
}


void CSNPDb_Impl::Put(CRef<SExtraTableCursor>& curs, TVDBRowId row)
{
    m_Extra.Put(curs, row);
}


CSNPDb_Impl::CSNPDb_Impl(CVDBMgr& mgr,
                         CTempString path_or_acc)
    : m_Mgr(mgr),
      m_DbPath(path_or_acc)
{
    // SNP VDB are multi-table VDB objects.
    // However, there could be other VDBs in the same namespace (NA*)
    // so we have to check this situation and return normal eNotFoundDb error.
    try {
        m_Db = CVDB(m_Mgr, path_or_acc);
    }
    catch ( CSraException& exc ) {
        bool another_vdb_table = false;
        if ( exc.GetErrCode() != exc.eNotFoundDb ) {
            // check if the accession refers some other VDB object
            try {
                CVDBTable table(mgr, path_or_acc);
                another_vdb_table = true;
            }
            catch ( CSraException& /*exc2*/ ) {
            }
        }
        if ( another_vdb_table ) {
            // It's some other VDB table object
            // report eNotFoundDb with original rc
            NCBI_THROW2_FMT(CSraException, eNotFoundDb,
                            "Cannot open VDB: "<<path_or_acc,
                            exc.GetRC());
        }
        else {
            // neither VDB nor another VDB table
            // report original exception
            throw;
        }
    }

    {
        // load track list
        CVDBTable track_table(m_Db, "TRACK_FILTER");
        STrackTableCursor cur(track_table);
        size_t track_count = cur.m_Cursor.GetMaxRowId();
        m_TrackList.resize(track_count);
        for ( size_t i = 0; i < track_count; ++i ) {
            STrackInfo& info = m_TrackList[i];
            TVDBRowId row = i+1;
            info.m_Name = *cur.NAME(row);
            NStr::TrimSuffixInPlace(info.m_Name, "\r");
            info.m_Filter.m_Filter = *cur.BITS(row);
            info.m_Filter.m_FilterMask = *cur.MASK(row);
            m_TrackMapByName[info.m_Name] = i;
        }
        if ( track_count == 0 ) {
            // default track without filtering
            m_TrackList.resize(1);
        }
        m_TrackMapByName[""] = 0;
    }

    {
        // load sequence list
        CVDBTable seq_table(m_Db, "SEQUENCE");
        SSeqTableCursor cur(seq_table);
        size_t seq_count = cur.m_Cursor.GetMaxRowId();
        m_SeqList.resize(seq_count);
        for ( size_t i = 0; i < seq_count; ++i ) {
            SSeqInfo& info = m_SeqList[i];
            TVDBRowId row = i+1;
            CTempString ref_id = *cur.ACCESSION(row);
            info.m_SeqId = ref_id;
            info.m_Seq_ids.assign(1, Ref(new CSeq_id(ref_id)));
            info.m_Seq_id_Handle =
                CSeq_id_Handle::GetHandle(*info.GetMainSeq_id());
            info.m_SeqLength = *cur.LEN(row);
            info.m_Circular = false;
            // index
            for ( auto& id : info.m_Seq_ids ) {
                m_SeqMapBySeq_id[CSeq_id_Handle::GetHandle(*id)] = i;
            }
        }
    }

    m_PageTable = CVDBTable(m_Db, "PAGE");
    {
        // prepare page index
        CRef<SPageTableCursor> cur = Page();
        
        for ( TVDBRowId row = 1, max_row = cur->m_Cursor.GetMaxRowId();
              row <= max_row; ++row ) {
            SSeqInfo& info = m_SeqList.at(size_t(*cur->SEQ_ID_ROW_NUM(row)-1));
            SSeqInfo::SPageSet pset;
            pset.m_SeqPos = *cur->PAGE_FROM(row);
            pset.m_PageCount = 1;
            pset.m_RowId = row;
            if ( !info.m_PageSets.empty() ) {
                SSeqInfo::SPageSet& prev_pset = info.m_PageSets.back();
                if ( prev_pset.GetSeqPosEnd(kPageSize) == pset.m_SeqPos &&
                     prev_pset.GetRowIdEnd() == pset.m_RowId ) {
                    prev_pset.m_PageCount += 1;
                    continue;
                }
            }
            info.m_PageSets.push_back(pset);
        }
        
        Put(cur);
    }

    // update length and graph positions
    {
        size_t track_count = m_TrackList.size();
        TVDBRowId graph_row = 1;
        for ( auto& info : m_SeqList ) {
            if ( !info.m_SeqLength ) {
                info.m_SeqLength = info.m_PageSets.back().GetSeqPosEnd(kPageSize);
            }
            info.m_GraphRowId = graph_row;
            TSeqPos pages = (info.m_SeqLength - 1)/kPageSize + 1;
            graph_row += pages * track_count;
        }
    }

    // open other tables
    m_GraphTable = CVDBTable(m_Db, "COVERAGE_GRAPH");
    m_FeatTable = CVDBTable(m_Db, "FEAT");
    m_ExtraTable = CVDBTable(m_Db, "EXTRA");
}


CSNPDb_Impl::~CSNPDb_Impl(void)
{
}


TSeqPos CSNPDb_Impl::GetPageSize(void) const
{
    return kPageSize;
}


CSNPDb_Impl::TTrackInfoList::const_iterator
CSNPDb_Impl::FindTrack(const string& name) const
{
    TTrackInfoMapByName::const_iterator it = m_TrackMapByName.find(name);
    if ( it == m_TrackMapByName.end() ) {
        return m_TrackList.end();
    }
    else {
        return m_TrackList.begin()+it->second;
    }
}


CSNPDb_Impl::TSeqInfoList::const_iterator
CSNPDb_Impl::FindSeq(const CSeq_id_Handle& seq_id) const
{
    TSeqInfoMapBySeq_id::const_iterator it = m_SeqMapBySeq_id.find(seq_id);
    if ( it == m_SeqMapBySeq_id.end() ) {
        return m_SeqList.end();
    }
    else {
        return m_SeqList.begin()+it->second;
    }
}


CRange<TVDBRowId>
CSNPDb_Impl::x_GetPageVDBRowRange(TSeqInfoList::const_iterator seq)
{
    if ( seq == GetSeqInfoList().end() ) {
        NCBI_THROW_FMT(CSraException, eInvalidIndex,
                       "Sequence index is out of bounds: "<<
                       GetDbPath());
    }
    return seq->GetPageVDBRowRange();
}


TVDBRowId
CSNPDb_Impl::x_GetGraphVDBRowId(TSeqInfoList::const_iterator seq,
                                TTrackInfoList::const_iterator track)
{
    if ( seq == GetSeqInfoList().end() ) {
        NCBI_THROW_FMT(CSraException, eInvalidIndex,
                       "Sequence index is out of bounds: "<<
                       GetDbPath());
    }
    if ( track == GetTrackInfoList().end() ) {
        NCBI_THROW_FMT(CSraException, eInvalidIndex,
                       "Filter track index is out of bounds: "<<
                       GetDbPath());
    }
    TVDBRowId start = seq->m_GraphRowId;
    TVDBRowId len = (seq->m_SeqLength-1)/kPageSize + 1;
    start += (track - GetTrackInfoList().begin())*len;
    return start;
}


/////////////////////////////////////////////////////////////////////////////
// CSNPDbTrackIterator
/////////////////////////////////////////////////////////////////////////////


CSNPDbTrackIterator::CSNPDbTrackIterator(const CSNPDb& db)
    : m_Db(db),
      m_Iter(GetList().begin())
{
}


CSNPDbTrackIterator::CSNPDbTrackIterator(const CSNPDb& db,
                                         size_t track_index)
    : m_Db(db)
{
    if ( track_index >= GetList().size() ) {
        NCBI_THROW_FMT(CSraException, eInvalidIndex,
                       "Track index is out of bounds: "<<
                       db->GetDbPath()<<"."<<track_index);
    }
    m_Iter = GetList().begin()+track_index;
}


CSNPDbTrackIterator::CSNPDbTrackIterator(const CSNPDb& db,
                                         const string& name)
    : m_Db(db),
      m_Iter(db->FindTrack(name))
{
}


const CSNPDbTrackIterator::TInfo& CSNPDbTrackIterator::GetInfo(void) const
{
    if ( !*this ) {
        NCBI_THROW(CSraException, eInvalidState,
                   "CSNPDbTrackIterator is invalid");
    }
    return *m_Iter;
}


void CSNPDbTrackIterator::Reset(void)
{
    m_Db.Reset();
    m_Iter = TList::const_iterator();
}


/////////////////////////////////////////////////////////////////////////////
// CSNPDbSeqIterator
/////////////////////////////////////////////////////////////////////////////


CSNPDbSeqIterator::CSNPDbSeqIterator(const CSNPDb& db)
    : m_Db(db),
      m_Iter(db->GetSeqInfoList().begin()),
      m_TrackIter(db->GetTrackInfoList().begin())
{
}


CSNPDbSeqIterator::CSNPDbSeqIterator(const CSNPDb& db,
                                     size_t seq_index)
{
    if ( seq_index >= db->m_SeqList.size() ) {
        NCBI_THROW_FMT(CSraException, eInvalidIndex,
                       "Sequence index is out of bounds: "<<
                       db->GetDbPath()<<"."<<seq_index);
    }
    m_Db = db;
    m_Iter = db->m_SeqList.begin()+seq_index;
    m_TrackIter = db->GetTrackInfoList().begin();
}


CSNPDbSeqIterator::CSNPDbSeqIterator(const CSNPDb& db,
                                     const CSeq_id_Handle& seq_id)
    : m_Db(db),
      m_Iter(db->FindSeq(seq_id)),
      m_TrackIter(db->GetTrackInfoList().begin())
{
}


void CSNPDbSeqIterator::SetTrack(const CSNPDbTrackIterator& track)
{
    m_TrackIter = track.m_Iter;
}


const CSNPDbSeqIterator::TInfo& CSNPDbSeqIterator::GetInfo(void) const
{
    if ( !*this ) {
        NCBI_THROW(CSraException, eInvalidState,
                   "CSNPDbSeqIterator is invalid");
    }
    return *m_Iter;
}


void CSNPDbSeqIterator::Reset(void)
{
    m_Iter = CSNPDb_Impl::TSeqInfoList::const_iterator();
    m_TrackIter = CSNPDb_Impl::TTrackInfoList::const_iterator();
    m_Db.Reset();
}


bool CSNPDbSeqIterator::IsCircular(void) const
{
    return GetInfo().m_Circular;
}


TSeqPos CSNPDbSeqIterator::GetMaxSNPLength(void) const
{
    return kMaxSNPLength;
}


Uint8 CSNPDbSeqIterator::GetSNPCount(void) const
{
    CRange<TVDBRowId> row_ids = GetPageVDBRowRange();
    CRef<CSNPDb_Impl::SPageTableCursor> cur = GetDb().Page();
    TVDBRowId begin = *cur->FEATURE_ROW_FROM(row_ids.GetFrom());
    TVDBRowId end = *cur->FEATURE_ROW_FROM(row_ids.GetTo());
    end += *cur->FEATURE_ROWS_COUNT(row_ids.GetTo());
    GetDb().Put(cur);
    return end - begin;
}


Uint8 CSNPDbSeqIterator::GetSNPCount(CRange<TSeqPos> range) const
{
    return GetSNPCount();
}


CRange<TSeqPos> CSNPDbSeqIterator::GetSNPRange(void) const
{
    const CSNPDb_Impl::SSeqInfo::TPageSets& psets = GetInfo().m_PageSets;
    return COpenRange<TSeqPos>(psets.front().m_SeqPos,
                               psets.back().GetSeqPosEnd(kPageSize));
}


BEGIN_LOCAL_NAMESPACE;


inline unsigned x_SetBitCount(Uint8 v)
{
    v = (NCBI_CONST_UINT8(0x5555555555555555) & (v>>1)) +
        (NCBI_CONST_UINT8(0x5555555555555555) & v);
    v = (NCBI_CONST_UINT8(0x3333333333333333) & (v>>2)) +
        (NCBI_CONST_UINT8(0x3333333333333333) & v);
    v = (NCBI_CONST_UINT8(0x0f0f0f0f0f0f0f0f) & (v>>4)) +
        (NCBI_CONST_UINT8(0x0f0f0f0f0f0f0f0f) & v);
    v = (NCBI_CONST_UINT8(0x00ff00ff00ff00ff) & (v>>8)) +
        (NCBI_CONST_UINT8(0x00ff00ff00ff00ff) & v);
    v = (NCBI_CONST_UINT8(0x0000ffff0000ffff) & (v>>16)) +
        (NCBI_CONST_UINT8(0x0000ffff0000ffff) & v);
    return unsigned(v>>32)+unsigned(v);
}


inline void x_SetOS8(vector<char>& os, Uint8 data)
{
    os.resize(8);
    char* dst = os.data();
    for ( int i = 0; i < 8; ++i ) {
        dst[i] = data>>(8*i);
    }
}


inline void x_AdjustRange(CRange<TSeqPos>& range,
                          const CSNPDbSeqIterator& it)
{
    range = range.IntersectionWith(it.GetSNPRange());
}


inline TSeqPos x_RoundPos(TSeqPos pos, TSeqPos step)
{
    return pos - pos%step;
}


inline TSeqPos x_RoundPosUp(TSeqPos pos, TSeqPos step)
{
    return x_RoundPos(pos+step-1, step);
}


inline void x_RoundRange(CRange<TSeqPos>& range, TSeqPos step)
{
    range.SetFrom(x_RoundPos(range.GetFrom(), step));
    range.SetToOpen(x_RoundPosUp(range.GetToOpen(), step));
}


inline void x_AdjustGraphRange(CRange<TSeqPos>& range,
                               const CSNPDbSeqIterator& it,
                               const TSeqPos comp)
{
    x_AdjustRange(range, it);
    x_RoundRange(range, comp);
}


struct SGraphMaker {
    static const TSeqPos kMinGraphGap = 1000;

    enum EGraphSet {
        eMultipleGraphs,
        eSingleGraph
    };
    enum EGapsType {
        eAllowGaps,
        eNoGaps
    };

    EGraphSet m_GraphSet;
    EGapsType m_GapsType;
    CRef<CSeq_graph> m_Graph;
    typedef list< CRef<CSeq_graph> > TGraphs;
    TGraphs m_Graphs;
    CRef<CSeq_id> m_Id;
    CRange<TSeqPos> m_Range;
    TSeqPos m_Comp;
    TSeqPos m_EmptyCount;
    Uint4 m_MaxValue;

    void Start(const CSNPDbSeqIterator& it,
               CRange<TSeqPos>& range,
               TSeqPos comp,
               EGraphSet graph_set = eMultipleGraphs,
               EGapsType gaps_type = eAllowGaps)
        {
            m_GraphSet = graph_set;
            m_GapsType = gaps_type;
            m_Graph = null;
            m_Graphs.clear();
            m_Id = it.GetSeqId();
            x_AdjustGraphRange(range, it, comp);
            m_Range = range;
            m_Comp = comp;
            m_EmptyCount = 0;
            m_MaxValue = 0;
            _ASSERT(!range.Empty());
            _ASSERT(range.GetFrom()%comp == 0);
            _ASSERT(range.GetToOpen()%comp == 0);
        }

    void x_NewGraph()
        {
            _ASSERT(!m_Graph);
            m_Graph = new CSeq_graph();
            m_MaxValue = 0;
        }
    void x_EndGraph(bool save = true)
        {
            _ASSERT(m_Graph);
            CSeq_graph& graph = *m_Graph;
            graph.SetTitle("SNP Density");
            size_t count;
            if ( m_MaxValue <= 255 ) {
                auto& gr = graph.SetGraph().SetByte();
                gr.SetMin(1);
                gr.SetMax(m_MaxValue);
                gr.SetAxis(0);
                count = gr.GetValues().size();
            }
            else {
                auto& gr = graph.SetGraph().SetInt();
                gr.SetMin(1);
                gr.SetMax(m_MaxValue);
                gr.SetAxis(0);
                count = gr.GetValues().size();
            }
            TSeqPos length = TSeqPos(count*m_Comp);
            CSeq_interval& loc = graph.SetLoc().SetInt();
            loc.SetId(*m_Id);
            loc.SetFrom(m_Range.GetFrom());
            loc.SetTo(m_Range.GetFrom()+length-1);
            graph.SetComp(m_Comp);
            graph.SetNumval(int(count));
            m_Range.SetFrom(m_Range.GetFrom()+length);
            if ( save ) {
                m_Graphs.push_back(m_Graph);
            }
            m_Graph = null;
        }
    CSeq_graph& x_GetGraph()
        {
            if ( !m_Graph ) {
                x_NewGraph();
            }
            return *m_Graph;
        }

    void AddActualGap()
        {
            _ASSERT(m_EmptyCount);
            _ASSERT(m_GapsType == eAllowGaps);
            if ( m_Graph ) {
                x_EndGraph();
            }
            m_Range.SetFrom(m_Range.GetFrom()+m_EmptyCount*m_Comp);
            m_EmptyCount = 0;
        }

    void AddActualZeroes(TSeqPos count)
        {
            _ASSERT(count);
            CSeq_graph& graph = x_GetGraph();
            if ( m_MaxValue <= 255 ) {
                auto& vv = graph.SetGraph().SetByte().SetValues();
                vv.resize(vv.size() + count);
            }
            else {
                auto& vv = graph.SetGraph().SetInt().SetValues();
                vv.resize(vv.size() + count);
            }
        }
    void AddActualValues(TSeqPos count, const Uint4* values)
        {
            _ASSERT(count);
            if ( m_EmptyCount ) {
                if ( !m_Graph ||
                     (m_GraphSet == eMultipleGraphs &&
                      m_EmptyCount >= kMinGraphGap) ) {
                    AddActualGap();
                }
                else {
                    AddActualZeroes(m_EmptyCount);
                    m_EmptyCount = 0;
                }
            }
            CSeq_graph& graph = x_GetGraph();
            m_MaxValue = max(m_MaxValue, *max_element(values, values+count));
            if ( m_MaxValue <= 255 ) {
                auto& vv = graph.SetGraph().SetByte().SetValues();
                vv.insert(vv.end(), values, values+count);
                return;
            }
            if ( graph.GetGraph().IsByte() ) {
                CConstRef<CByte_graph> old_data(&graph.GetGraph().GetByte());
                auto& old_vv = old_data->GetValues();
                auto& vv = graph.SetGraph().SetInt().SetValues();
                const Uint1* bb = reinterpret_cast<const Uint1*>(old_vv.data());
                vv.assign(bb, bb+old_vv.size());
            }
            auto& vv = graph.SetGraph().SetInt().SetValues();
            vv.insert(vv.end(), values, values+count);
        }
    void AddActualValue(Uint4 value)
        {
            AddActualValues(1, &value);
        }

    void AddEmpty(TSeqPos count)
        {
            _ASSERT(count);
            if ( m_GapsType == eNoGaps ) {
                AddActualZeroes(count);
            }
            else {
                m_EmptyCount += count;
            }
        }
    void AddValues(TSeqPos count, const Uint4* values)
        {
            TSeqPos empty_before = 0;
            while ( count && *values == 0 ) {
                ++empty_before;
                --count;
                ++values;
            }
            if ( empty_before ) {
                AddEmpty(empty_before);
            }
            TSeqPos empty_after = 0;
            while ( count && values[count-1] == 0 ) {
                ++empty_after;
                --count;
            }
            if ( count ) {
                AddActualValues(count, values);
            }
            if ( empty_after ) {
                AddEmpty(empty_after);
            }
        }
    void AddValue(Uint4 value)
        {
            if ( !value ) {
                AddEmpty(1);
            }
            else {
                AddActualValue(value);
            }
        }
    TGraphs& FinishAnnot()
        {
            if ( m_Graph ) {
                x_EndGraph();
            }
            return m_Graphs;
        }
    CRef<CSeq_graph> FinishGraph()
        {
            CRef<CSeq_graph> ret = m_Graph;
            if ( ret ) {
                x_EndGraph(false);
            }
            return ret;
        }
};


CRef<CSeq_annot> x_NewAnnot(const string& annot_name = kDefaultAnnotName)
{
    CRef<CSeq_annot> annot(new CSeq_annot);
    annot->SetNameDesc(annot_name);
    return annot;
}


void x_CollectOverviewGraph(SGraphMaker& g,
                            const CSNPDbSeqIterator& seq_it,
                            CRange<TSeqPos> range,
                            SGraphMaker::EGraphSet graph_set,
                            SGraphMaker::EGapsType gaps_type)
{
    g.Start(seq_it, range, kOverviewZoom, graph_set, gaps_type);
    for ( CSNPDbGraphIterator it(seq_it, range); it; ++it ) {
        g.AddValue(it.GetTotalValue());
    }
}


void x_CollectCoverageGraph(SGraphMaker& g,
                            const CSNPDbSeqIterator& seq_it,
                            CRange<TSeqPos> range,
                            SGraphMaker::EGraphSet graph_set)
{
    g.Start(seq_it, range, kCoverageZoom, graph_set);
    for ( CSNPDbGraphIterator it(seq_it, range); it; ++it ) {
        CRange<TSeqPos> page = it.GetPageRange();
        TSeqPos skip_beg = 0;
        if ( range.GetFrom() > page.GetFrom() ) {
            skip_beg = (range.GetFrom() - page.GetFrom())/kCoverageZoom;
        }
        TSeqPos skip_end = 0;
        if ( range.GetToOpen() < page.GetToOpen() ) {
            skip_end = (page.GetToOpen() - range.GetToOpen())/kCoverageZoom;
        }
        TSeqPos count = kPageSize/kCoverageZoom - skip_beg - skip_end;
        if ( !it.GetTotalValue() ) {
            g.AddEmpty(count);
        }
        else {
            CVDBValueFor<Uint4> values = it.GetCoverageValues();
            _ASSERT(values.size()*kCoverageZoom == kPageSize);
            g.AddValues(count, values.data()+skip_beg);
        }
    }
}


END_LOCAL_NAMESPACE;


CRef<CSeq_graph>
CSNPDbSeqIterator::GetOverviewGraph(CRange<TSeqPos> range,
                                    TFlags flags) const
{
    SGraphMaker g;
    x_CollectOverviewGraph(g, *this, range,
                           g.eSingleGraph,
                           flags & fNoGaps? g.eNoGaps: g.eAllowGaps);
    return g.FinishGraph();
}


CRef<CSeq_annot>
CSNPDbSeqIterator::GetOverviewAnnot(CRange<TSeqPos> range,
                                    const string& annot_name,
                                    TFlags flags) const
{
    SGraphMaker g;
    x_CollectOverviewGraph(g, *this, range,
                           flags & fNoGaps? g.eSingleGraph: g.eMultipleGraphs,
                           flags & fNoGaps? g.eNoGaps: g.eAllowGaps);
    CRef<CSeq_annot> annot = x_NewAnnot(annot_name);
    annot->SetData().SetGraph().swap(g.FinishAnnot());
    return annot;
}


CRef<CSeq_annot>
CSNPDbSeqIterator::GetOverviewAnnot(CRange<TSeqPos> range,
                                    TFlags flags) const
{
    return GetOverviewAnnot(range, kDefaultAnnotName, flags);
}


CRef<CSeq_graph>
CSNPDbSeqIterator::GetCoverageGraph(CRange<TSeqPos> range) const
{
    SGraphMaker g;
    x_CollectCoverageGraph(g, *this, range, g.eSingleGraph);
    return g.FinishGraph();
}


CRef<CSeq_annot>
CSNPDbSeqIterator::GetCoverageAnnot(CRange<TSeqPos> range,
                                    const string& annot_name,
                                    TFlags flags) const
{
    SGraphMaker g;
    x_CollectCoverageGraph(g, *this, range, g.eMultipleGraphs);
    CRef<CSeq_annot> annot = x_NewAnnot(annot_name);
    annot->SetData().SetGraph().swap(g.FinishAnnot());
    return annot;
}


CRef<CSeq_annot>
CSNPDbSeqIterator::GetCoverageAnnot(CRange<TSeqPos> range,
                                    TFlags flags) const
{
    return GetCoverageAnnot(range, kDefaultAnnotName, flags);
}


CRef<CSeq_annot>
CSNPDbSeqIterator::GetFeatAnnot(CRange<TSeqPos> range,
                                const SFilter& filter,
                                TFlags flags) const
{
    CRef<CSeq_annot> annot = x_NewAnnot();
    x_AdjustRange(range, *this);
    CSeq_annot::TData::TFtable& feats = annot->SetData().SetFtable();
    SSelector sel(eSearchByStart, filter);
    for ( CSNPDbFeatIterator it(*this, range, sel); it; ++it ) {
        feats.push_back(it.GetSeq_feat());
    }
    if ( feats.empty() ) {
        return null;
    }
    return annot;
}


CRef<CSeq_annot>
CSNPDbSeqIterator::GetFeatAnnot(CRange<TSeqPos> range,
                                TFlags flags) const
{
    return GetFeatAnnot(range, GetFilter(), flags);
}


BEGIN_LOCAL_NAMESPACE;


CRef<CSeqTable_column> x_MakeColumn(CSeqTable_column_info::EField_id id,
                                    const char* name = 0)
{
    CRef<CSeqTable_column> col(new CSeqTable_column);
    col->SetHeader().SetField_id(id);
    if ( name ) {
        col->SetHeader().SetField_name(name);
    }
    return col;
}


CRef<CSeqTable_column> x_MakeColumn(const char* name)
{
    CRef<CSeqTable_column> col(new CSeqTable_column);
    col->SetHeader().SetField_name(name);
    return col;
}


struct SColumn
{
    int id;
    const char* name;

    CRef<CSeqTable_column> column;

    SColumn(void)
        : id(-1),
          name(0)
        {
        }
    explicit
    SColumn(CSeqTable_column_info::EField_id id,
            const char* name = 0)
        : id(id),
          name(name)
        {
        }

    void Init(CSeqTable_column_info::EField_id id,
              const char* name = 0)
        {
            this->id = id;
            this->name = name;
        }

    CSeqTable_column* x_GetColumn(void)
        {
            if ( !column ) {
                _ASSERT(id >= 0);
                column =
                    x_MakeColumn(CSeqTable_column_info::EField_id(id), name);
            }
            return column;
        }
    CRef<CSeqTable_column> GetColumn(void)
        {
            return Ref(x_GetColumn());
        }

    void Attach(CSeq_table& table)
        {
            if ( column ) {
                table.SetColumns().push_back(column);
            }
        }

    DECLARE_OPERATOR_BOOL_REF(column);
};

struct SIntColumn : public SColumn
{
    CSeqTable_multi_data::TInt* values;

    explicit
    SIntColumn(CSeqTable_column_info::EField_id id, const char* name = 0)
        : SColumn(id, name),
          values(0)
        {
        }

    void Add(int value)
        {
            if ( !values ) {
                values = &x_GetColumn()->SetData().SetInt();
            }
            values->push_back(value);
        }
};


struct SInt8Column : public SIntColumn
{
    CSeqTable_multi_data::TInt8* values8;

    explicit
    SInt8Column(CSeqTable_column_info::EField_id id, const char* name = 0)
        : SIntColumn(id, name),
          values8(0)
        {
        }

    void Add(Int8 value)
        {
            if ( !values8 && int(value) == value ) {
                SIntColumn::Add(int(value));
            }
            else {
                if ( !values8 ) {
                    CSeqTable_column* col = x_GetColumn();
                    if ( col->IsSetData() ) {
                        col->SetData().ChangeToInt8();
                    }
                    values8 = &col->SetData().SetInt8();
                }
                values8->push_back(value);
            }
        }
};


struct SSparseIndex
{
    SColumn& column;
    CSeqTable_sparse_index::TIndexes* indexes;
    int size;

    SSparseIndex(SColumn& column)
        : column(column),
          indexes(0),
          size(0)
        {
        }
    
    void Add(int index)
        {
            if ( index != size && !indexes ) {
                indexes = &column.x_GetColumn()->SetSparse().SetIndexes();
                for ( int i = 0; i < size; ++i ) {
                    indexes->push_back(i);
                }
            }
            if ( indexes ) {
                indexes->push_back(index);
            }
            ++size;
        }

    void Optimize(SIntColumn& column, const SIntColumn& backup_column)
        {
            _ASSERT(&column == &this->column);
            if ( !indexes ) {
                return;
            }

            size_t sparse_size = column.values->size();
            _ASSERT(sparse_size == indexes->size());
            size_t total_size = backup_column.values->size();
            _ASSERT(indexes->back() < total_size);
            if ( sparse_size >= total_size/3 ) {
                // sparse index is too big, replace with plain column
                CSeqTable_multi_data::TInt values;
                values.reserve(total_size);
                for ( size_t i = 0, j = 0; i < total_size; ++i ) {
                    TSeqPos to;
                    if ( j < indexes->size() && i == (*indexes)[j] ) {
                        to = (*column.values)[j++];
                    }
                    else {
                        to = (*backup_column.values)[i];
                    }
                    values.push_back(to);                
                }
                swap(values, *column.values);
                indexes = 0;
                column.x_GetColumn()->ResetSparse();
            }
        }
};


struct SCommonStrings : public SColumn
{
    CCommonString_table::TStrings* values;
    CCommonString_table::TIndexes* indexes;
    typedef unordered_map<CTempString, int> TIndex;
    TIndex index;
    list<string> index_strings;

    SCommonStrings()
        : values(0),
          indexes(0)
        {
        }
    explicit
    SCommonStrings(CSeqTable_column_info::EField_id id,
                   const char* name = 0)
        : SColumn(id, name),
          values(0),
          indexes(0)
        {
        }

    void Add(CTempString val)
        {
            if ( !values ) {
                CSeqTable_column* col = x_GetColumn();
                values = &col->SetData().SetCommon_string().SetStrings();
                indexes = &col->SetData().SetCommon_string().SetIndexes();
            }
            int ind;
            TIndex::const_iterator it = index.find(val);
            if ( it == index.end() ) {
                ind = int(values->size());
                values->push_back(val);
                index_strings.push_back(val);
                val = index_strings.back();
                index.insert(TIndex::value_type(val, ind));
            }
            else {
                ind = it->second;
            }
            indexes->push_back(ind);
        }

    void Attach(CSeq_table& table)
        {
            if ( values && values->size() == 1 ) {
                CSeqTable_column* col = x_GetColumn();
                col->SetDefault().SetString().swap(values->front());
                col->ResetData();
                values = 0;
                indexes = 0;
            }
            SColumn::Attach(table);
        }
};


struct SCommon8Bytes : public SColumn
{
    CCommonBytes_table::TBytes* values;
    CCommonBytes_table::TIndexes* indexes;
    typedef map<Uint8, size_t> TIndex;
    TIndex index;

    explicit
    SCommon8Bytes(CSeqTable_column_info::EField_id id,
                  const char* name = 0)
        : SColumn(id, name),
          values(0),
          indexes(0)
        {
        }

    void Add(Uint8 val)
        {
            if ( !values ) {
                CSeqTable_column* col = x_GetColumn();
                values = &col->SetData().SetCommon_bytes().SetBytes();
                indexes = &col->SetData().SetCommon_bytes().SetIndexes();
            }
            pair<TIndex::iterator, bool> ins =
                index.insert(TIndex::value_type(val, 0));
            if ( ins.second ) {
                ins.first->second = values->size();
                vector<char>* data = new vector<char>();
                values->push_back(data);
                x_SetOS8(*data, val);
            }
            auto value_index = ins.first->second;
            if ( value_index > kMax_Int ) {
                NCBI_THROW(CSraException, eDataError,
                           "CSNPDbSeqIterator: common bytes table is too big");
            }
            indexes->push_back(int(value_index));
        }

    void Attach(CSeq_table& table)
        {
            if ( values && values->size() == 1 ) {
                CSeqTable_column* col = x_GetColumn();
                col->SetDefault().SetBytes().swap(*values->front());
                col->ResetData();
                values = 0;
                indexes = 0;
            }
            SColumn::Attach(table);
        }
};


static const int kMaxTableAlleles = 4;


struct SSeqTableContent
{
    SSeqTableContent(void);

    void Add(const CSNPDbFeatIterator& it);

    CRef<CSeq_annot> GetAnnot(const string& annot_name,
                              CSeq_id& seq_id);

    int m_TableSize;

    // columns
    SIntColumn col_from;
    SIntColumn col_to;
    SSparseIndex ind_to;

    SCommonStrings col_alleles[kMaxTableAlleles];

    SCommonStrings col_subtype;
    SCommon8Bytes col_bitfield;

    SInt8Column col_dbxref;

    static void AddFixedString(CSeq_table& table,
                               CSeqTable_column_info::EField_id id,
                               const string& value)
        {
            CRef<CSeqTable_column> col = x_MakeColumn(id);
            col->SetDefault().SetString(value);
            table.SetColumns().push_back(col);
        }

    static void AddFixedSeq_id(CSeq_table& table,
                               CSeqTable_column_info::EField_id id,
                               CSeq_id& value)
        {
            CRef<CSeqTable_column> col = x_MakeColumn(id);
            col->SetDefault().SetId(value);
            table.SetColumns().push_back(col);
        }

    static void AddFixedSeq_loc(CSeq_table& table,
                                const char* name,
                                CSeq_loc& value)
        {
            CRef<CSeqTable_column> col = x_MakeColumn(name);
            col->SetDefault().SetLoc(value);
            table.SetColumns().push_back(col);
        }

    static void AddFixedInt(CSeq_table& table,
                            const char* name,
                            int value)
        {
            CRef<CSeqTable_column> col = x_MakeColumn(name);
            col->SetDefault().SetInt(value);
            table.SetColumns().push_back(col);
        }
};


SSeqTableContent::SSeqTableContent(void)
    : m_TableSize(0),
      col_from(CSeqTable_column_info::eField_id_location_from),
      col_to(CSeqTable_column_info::eField_id_location_to),
      ind_to(col_to),
      col_subtype(CSeqTable_column_info::eField_id_ext, "E.VariationClass"),
      col_bitfield(CSeqTable_column_info::eField_id_ext, "E.Bitfield"),
      col_dbxref(CSeqTable_column_info::eField_id_dbxref, "D.dbSNP")
{
    for ( int i = 0; i < kMaxTableAlleles; ++i ) {
        col_alleles[i].Init(CSeqTable_column_info::eField_id_qual,
                            "Q.replace");
    }
}


inline
void SSeqTableContent::Add(const CSNPDbFeatIterator& it)
{
    TSeqPos from = it.GetSNPPosition();
    TSeqPos len = it.GetSNPLength();
    
    col_from.Add(from);
    if ( len != 1 ) {
        col_to.Add(from + len - 1);
        ind_to.Add(m_TableSize);
    }

    CSNPDbFeatIterator::TExtraRange range = it.GetExtraRange();
    for ( size_t i = 0; i < range.second; ++i ) {
        _ASSERT(i < kMaxTableAlleles);
        col_alleles[i].Add(it.GetAllele(range, i));
    }

    col_subtype.Add(it.GetFeatSubtypeString());
    col_bitfield.Add(it.GetBitfield());

    col_dbxref.Add(it.GetFeatId());

    ++m_TableSize;
}


CRef<CSeq_annot> SSeqTableContent::GetAnnot(const string& annot_name,
                                            CSeq_id& seq_id)
{
    if ( !m_TableSize ) {
        return null;
    }
    
    CRef<CSeq_annot> table_annot = x_NewAnnot(annot_name);
    
    CSeq_table& table = table_annot->SetData().SetSeq_table();
    table.SetFeat_type(CSeqFeatData::e_Imp);
    table.SetFeat_subtype(CSeqFeatData::eSubtype_variation);
    table.SetNum_rows(m_TableSize);

    AddFixedString(table,
                   CSeqTable_column_info::eField_id_data_imp_key,
                   "variation");

    if ( 1 ) {
        TSeqPos total_from = col_from.values->front();
        TSeqPos total_to = col_from.values->back();
        TSeqPos max_len = 1;
        if ( col_to.values ) {
            max_len = kMaxSNPLength;
            total_to += max_len-1; 
        }
        CRef<CSeq_loc> total_loc(new CSeq_loc);
        total_loc->SetInt().SetId(seq_id);
        total_loc->SetInt().SetFrom(total_from);
        total_loc->SetInt().SetTo(total_to);
        AddFixedSeq_loc(table,
                        "Seq-table location",
                        *total_loc);

        AddFixedInt(table,
                    "Sorted, max length",
                    max_len);

        AddFixedSeq_id(table,
                       CSeqTable_column_info::eField_id_location_id,
                       seq_id);
    }

    col_from.Attach(table);
    ind_to.Optimize(col_to, col_from);
    col_to.Attach(table);
    for ( int i = 0; i < kMaxTableAlleles; ++i ) {
        col_alleles[i].Attach(table);
    }

    if ( col_subtype || col_bitfield ) {
        AddFixedString(table,
                       CSeqTable_column_info::eField_id_ext_type,
                       "dbSnpQAdata");
        if ( col_subtype ) {
            col_subtype.Attach(table);
        }
        if ( col_bitfield ) {
            col_bitfield.Attach(table);
        }
    }

    col_dbxref.Attach(table);

    return table_annot;
}


struct SSeqTableConverter
{
    SSeqTableConverter(const CSNPDbSeqIterator& it);
    
    bool AddToTable(const CSNPDbFeatIterator& it);

    SSeqTableContent m_Tables[2][kMaxTableAlleles];

    void Add(const CSNPDbFeatIterator& it);

    vector< CRef<CSeq_annot> > GetAnnots(const string& annot_name);

    CRef<CSeq_id> m_Seq_id;
    CRef<CSeq_annot> m_RegularAnnot;
};


SSeqTableConverter::SSeqTableConverter(const CSNPDbSeqIterator& it)
    : m_Seq_id(it.GetSeqId())
{
}


vector< CRef<CSeq_annot> >
SSeqTableConverter::GetAnnots(const string& annot_name)
{
    vector< CRef<CSeq_annot> > ret;
    for ( int k = 0; k < 2; ++k ) {
        for ( int i = 0; i < kMaxTableAlleles; ++i ) {
            CRef<CSeq_annot> annot =
                m_Tables[k][i].GetAnnot(annot_name, *m_Seq_id);
            if ( annot ) {
                ret.push_back(annot);
            }
        }
    }
    if ( m_RegularAnnot ) {
        m_RegularAnnot->SetNameDesc(annot_name);
        ret.push_back(m_RegularAnnot);
    }
    return ret;
}


inline
bool SSeqTableConverter::AddToTable(const CSNPDbFeatIterator& it)
{
    CSNPDbFeatIterator::TExtraRange range = it.GetExtraRange();
    size_t last_index = range.second - 1;
    if ( last_index >= kMaxTableAlleles ) {
        return false;
    }
    m_Tables[it.GetSNPLength() != 1][last_index].Add(it);
    return true;
}


inline
void SSeqTableConverter::Add(const CSNPDbFeatIterator& it)
{
    if ( AddToTable(it) ) {
        return;
    }
    if ( !m_RegularAnnot ) {
        m_RegularAnnot = x_NewAnnot();
    }
    m_RegularAnnot->SetData().SetFtable().push_back(it.GetSeq_feat());
}


END_LOCAL_NAMESPACE;


CSNPDbSeqIterator::TAnnotSet
CSNPDbSeqIterator::GetTableFeatAnnots(CRange<TSeqPos> range,
                                      const string& annot_name,
                                      const SFilter& filter,
                                      TFlags flags) const
{
    x_AdjustRange(range, *this);
    SSeqTableConverter cvt(*this);
    SSelector sel(eSearchByStart, filter);
    for ( CSNPDbFeatIterator it(*this, range, sel); it; ++it ) {
        cvt.Add(it);
    }
    return cvt.GetAnnots(annot_name);
}


CSNPDbSeqIterator::TAnnotSet
CSNPDbSeqIterator::GetTableFeatAnnots(CRange<TSeqPos> range,
                                      const SFilter& filter,
                                      TFlags flags) const
{
    return GetTableFeatAnnots(range, kDefaultAnnotName, filter, flags);
}


CSNPDbSeqIterator::TAnnotSet
CSNPDbSeqIterator::GetTableFeatAnnots(CRange<TSeqPos> range,
                                      const string& annot_name,
                                      TFlags flags) const
{
    return GetTableFeatAnnots(range, annot_name, GetFilter(), flags);
}


CSNPDbSeqIterator::TAnnotSet
CSNPDbSeqIterator::GetTableFeatAnnots(CRange<TSeqPos> range,
                                      TFlags flags) const
{
    return GetTableFeatAnnots(range, GetFilter(), flags);
}


BEGIN_LOCAL_NAMESPACE;


inline
void x_InitSNP_Info(SSNP_Info& info)
{
    info.m_Flags = info.fQualityCodesOs | info.fAlleleReplace;
    info.m_CommentIndex = info.kNo_CommentIndex;
    info.m_Weight = 0;
    info.m_ExtraIndex = info.kNo_ExtraIndex;
}


inline
bool x_ParseSNP_Info(SSNP_Info& info,
                     const CSNPDbFeatIterator& it,
                     CSeq_annot_SNP_Info& packed)
{
    TSeqPos len = it.GetSNPLength();
    if ( len > info.kMax_PositionDelta+1 ) {
        return false;
    }
    info.m_PositionDelta = len-1;
    info.m_ToPosition = it.GetSNPPosition()+len-1;

    CSNPDbFeatIterator::TExtraRange range = it.GetExtraRange();
    if ( range.second > info.kMax_AllelesCount ) {
        return false;
    }
    size_t index = 0;
    for ( ; index < range.second; ++index ) {
        CTempString allele = it.GetAllele(range, index);
        if ( allele.size() > kMax_AlleleLength ) {
            return false;
        }
        SSNP_Info::TAlleleIndex a_index = packed.x_GetAlleleIndex(allele);
        if ( a_index == info.kNo_AlleleIndex ) {
            return false;
        }
        info.m_AllelesIndices[index] = a_index;
    }
    for ( ; index < info.kMax_AllelesCount; ++index ) {
        info.m_AllelesIndices[index] = info.kNo_AlleleIndex;
    }

    vector<char> os;
    it.GetBitfieldOS(os);
    info.m_QualityCodesIndex = packed.x_GetQualityCodesIndex(os);
    if ( info.m_QualityCodesIndex == info.kNo_QualityCodesIndex ) {
        return false;
    }

    auto feat_id = it.GetFeatId();
    if ( feat_id > kMax_Int ) {
        NCBI_THROW(CSraException, eDataError,
                   "CSNPDbSeqIterator: FEAT_ID doesn't fit into table SNPId");
    }
    info.m_SNP_Id = SSNP_Info::TSNPId(feat_id);

    packed.x_AddSNP(info);
    return true;
}        


END_LOCAL_NAMESPACE;


CSNPDbSeqIterator::TPackedAnnot
CSNPDbSeqIterator::GetPackedFeatAnnot(CRange<TSeqPos> range,
                                      const SFilter& filter,
                                      TFlags flags) const
{
    x_AdjustRange(range, *this);
    CRef<CSeq_annot> annot = x_NewAnnot();
    CRef<CSeq_annot_SNP_Info> packed(new CSeq_annot_SNP_Info);
    CSeq_annot::TData::TFtable& feats = annot->SetData().SetFtable();

    SSNP_Info info;
    x_InitSNP_Info(info);
    SSelector sel(eSearchByStart, filter);
    for ( CSNPDbFeatIterator it(*this, range, sel); it; ++it ) {
        if ( !x_ParseSNP_Info(info, it, *packed) ) {
            feats.push_back(it.GetSeq_feat());
        }
    }
    if ( packed->empty() ) {
        packed = null;
        if ( feats.empty() ) {
            annot = null;
        }
    }
    else {
        packed->SetSeq_id(*GetSeqId());
    }
    return TPackedAnnot(annot, packed);
}


CSNPDbSeqIterator::TPackedAnnot
CSNPDbSeqIterator::GetPackedFeatAnnot(CRange<TSeqPos> range,
                                      TFlags flags) const
{
    return GetPackedFeatAnnot(range, GetFilter(), flags);
}


/////////////////////////////////////////////////////////////////////////////
// CSNPDbPageIterator
/////////////////////////////////////////////////////////////////////////////


void CSNPDbPageIterator::Reset(void)
{
    if ( m_Cur ) {
        GetDb().Put(m_Cur, m_CurrPageRowId);
        _ASSERT(!m_Cur);
    }
    if ( m_GraphCur ) {
        GetDb().Put(m_GraphCur, m_LastGraphRowId);
        _ASSERT(!m_Cur);
    }
    m_SeqIter.Reset();
    m_CurrPagePos = kInvalidSeqPos;
}


CSNPDbPageIterator::CSNPDbPageIterator(void)
    : m_CurrPageSet(0),
      m_CurrPageRowId(0),
      m_CurrPagePos(kInvalidSeqPos),
      m_SearchMode(eSearchByOverlap)
{
}


CSNPDbPageIterator::CSNPDbPageIterator(const CSNPDb& db,
                                       const CSeq_id_Handle& ref_id,
                                       TSeqPos ref_pos,
                                       TSeqPos window,
                                       ESearchMode search_mode)
    : m_SeqIter(db, ref_id)
{
    TSeqPos ref_end = window? ref_pos+window: kInvalidSeqPos;
    Select(COpenRange<TSeqPos>(ref_pos, ref_end), search_mode);
}


CSNPDbPageIterator::CSNPDbPageIterator(const CSNPDb& db,
                                       const CSeq_id_Handle& ref_id,
                                       COpenRange<TSeqPos> range,
                                       ESearchMode search_mode)
    : m_SeqIter(db, ref_id)
{
    Select(range, search_mode);
}


CSNPDbPageIterator::CSNPDbPageIterator(const CSNPDbSeqIterator& seq,
                                       COpenRange<TSeqPos> range,
                                       ESearchMode search_mode)
    : m_SeqIter(seq)
{
    Select(range, search_mode);
}


CSNPDbPageIterator::CSNPDbPageIterator(const CSNPDbPageIterator& iter)
{
    *this = iter;
}


CSNPDbPageIterator&
CSNPDbPageIterator::operator=(const CSNPDbPageIterator& iter)
{
    if ( this != &iter ) {
        Reset();
        m_SeqIter = iter.m_SeqIter;
        m_Cur = iter.m_Cur;
        m_GraphCur = iter.m_GraphCur;
        m_LastGraphRowId = iter.m_LastGraphRowId;
        m_SearchRange = iter.m_SearchRange;
        m_CurrPageSet = iter.m_CurrPageSet;
        m_CurrPageRowId = iter.m_CurrPageRowId;
        m_CurrPagePos = iter.m_CurrPagePos;
        m_SearchMode = iter.m_SearchMode;
    }
    return *this;
}


CSNPDbPageIterator::~CSNPDbPageIterator(void)
{
    Reset();
}


CSNPDbPageIterator&
CSNPDbPageIterator::Select(COpenRange<TSeqPos> ref_range,
                           ESearchMode search_mode)
{
    m_SearchRange = ref_range;
    m_SearchMode = search_mode;

    if ( !m_SeqIter || ref_range.Empty() ) {
        m_CurrPagePos = kInvalidSeqPos;
        return *this;
    }
    
    if ( !m_Cur ) {
        m_Cur = GetDb().Page(m_CurrPageRowId);
    }

    TSeqPos pos = ref_range.GetFrom();
    if ( m_SearchMode == eSearchByOverlap ) {
        // SNP may start before requested position
        pos = pos < kMaxSNPLength? 0: pos - (kMaxSNPLength-1);
    }

    const CSNPDb_Impl::SSeqInfo::TPageSets& psets = m_SeqIter->m_PageSets;
    for ( m_CurrPageSet = 0; m_CurrPageSet < psets.size(); ++m_CurrPageSet ) {
        const CSNPDb_Impl::SSeqInfo::SPageSet& pset = psets[m_CurrPageSet];
        TSeqPos skip = pos<pset.m_SeqPos? 0: (pos-pset.m_SeqPos)/kPageSize;
        if ( skip < pset.m_PageCount ) {
            m_CurrPageRowId = pset.m_RowId + skip;
            m_CurrPagePos = pset.m_SeqPos + skip * kPageSize;
            return *this;
        }
    }
    m_CurrPageRowId = TVDBRowId(-1);
    m_CurrPagePos = kInvalidSeqPos;
    return *this;
}


void CSNPDbPageIterator::x_Next(void)
{
    x_CheckValid("CSNPDbPageIterator::operator++");

    const CSNPDb_Impl::SSeqInfo::TPageSets& psets = m_SeqIter->m_PageSets;
    if ( ++m_CurrPageRowId < psets[m_CurrPageSet].GetRowIdEnd() ) {
        // next page in the set
        m_CurrPagePos += kPageSize;
        return;
    }
    
    // no more pages in the set, next page set
    if ( ++m_CurrPageSet < psets.size() ) {
        // first page in the next set
        m_CurrPageRowId = psets[m_CurrPageSet].m_RowId;
        m_CurrPagePos = psets[m_CurrPageSet].m_SeqPos;
        return;
    }
    
    // no more page sets
    m_CurrPagePos = kInvalidSeqPos;
}


void CSNPDbPageIterator::x_ReportInvalid(const char* method) const
{
    NCBI_THROW_FMT(CSraException, eInvalidState,
                   "CSNPDbPageIterator::"<<method<<"(): "
                   "Invalid iterator state");
}


TVDBRowId CSNPDbPageIterator::GetFirstFeatRowId(void) const
{
    x_CheckValid("CSNPDbPageIterator::GetFirstFeatRowId");
    return *Cur().FEATURE_ROW_FROM(GetPageRowId());
}


TVDBRowCount CSNPDbPageIterator::GetFeatCount(void) const
{
    x_CheckValid("CSNPDbPageIterator::GetFeatCount");
    return *Cur().FEATURE_ROWS_COUNT(GetPageRowId());
}


/////////////////////////////////////////////////////////////////////////////
// CSNPDbGraphIterator
/////////////////////////////////////////////////////////////////////////////


void CSNPDbGraphIterator::Reset(void)
{
    if ( m_Cur ) {
        GetDb().Put(m_Cur, m_CurrPageRowId);
        _ASSERT(!m_Cur);
    }
    m_Db.Reset();
    m_CurrPagePos = kInvalidSeqPos;
}


CSNPDbGraphIterator::CSNPDbGraphIterator(void)
    : m_CurrPageRowId(0),
      m_CurrPagePos(kInvalidSeqPos)
{
}


CSNPDbGraphIterator::CSNPDbGraphIterator(const CSNPDbSeqIterator& seq,
                                         COpenRange<TSeqPos> ref_range)
{
    Select(seq, ref_range);
}


CSNPDbGraphIterator::CSNPDbGraphIterator(const CSNPDbGraphIterator& iter)
{
    *this = iter;
}


CSNPDbGraphIterator&
CSNPDbGraphIterator::operator=(const CSNPDbGraphIterator& iter)
{
    if ( this != &iter ) {
        Reset();
        m_Db = iter.m_Db;
        m_Cur = iter.m_Cur;
        m_SeqRowId = iter.m_SeqRowId;
        m_TrackRowId = iter.m_TrackRowId;
        m_SearchRange = iter.m_SearchRange;
        m_CurrPageRowId = iter.m_CurrPageRowId;
        m_CurrPagePos = iter.m_CurrPagePos;
    }
    return *this;
}


CSNPDbGraphIterator::~CSNPDbGraphIterator(void)
{
    Reset();
}


CSNPDbGraphIterator&
CSNPDbGraphIterator::Select(const CSNPDbSeqIterator& iter,
                            COpenRange<TSeqPos> ref_range)
{
    m_Db = iter.m_Db;
    m_SeqRowId = m_Db->x_GetSeqVDBRowId(iter.x_GetSeqIter());
    m_TrackRowId = m_Db->x_GetTrackVDBRowId(iter.x_GetTrackIter());
    m_SearchRange = ref_range;

    if ( !iter || ref_range.Empty() ) {
        m_CurrPagePos = kInvalidSeqPos;
        return *this;
    }
    
    if ( !m_Cur ) {
        m_Cur = GetDb().Graph(m_CurrPageRowId);
    }

    TSeqPos page = ref_range.GetFrom()/kPageSize;
    m_CurrPageRowId = iter.GetGraphVDBRowId() + page;
    m_CurrPagePos = page*kPageSize;
    return *this;
}


void CSNPDbGraphIterator::x_Next(void)
{
    x_CheckValid("CSNPDbGraphIterator::operator++");

    if ( ++m_CurrPageRowId > m_Cur->m_Cursor.GetMaxRowId() ||
         *m_Cur->FILTER_ID_ROW_NUM(m_CurrPageRowId) != m_TrackRowId ||
         *m_Cur->SEQ_ID_ROW_NUM(m_CurrPageRowId) != m_SeqRowId ) {
        // end of track
        m_CurrPagePos = kInvalidSeqPos;
        return;
    }

    m_CurrPagePos = *m_Cur->BLOCK_FROM(m_CurrPageRowId);
    if ( m_CurrPagePos >= m_SearchRange.GetToOpen() ) {
        // out of range
        m_CurrPagePos = kInvalidSeqPos;
        return;
    }
}


void CSNPDbGraphIterator::x_ReportInvalid(const char* method) const
{
    NCBI_THROW_FMT(CSraException, eInvalidState,
                   "CSNPDbGraphIterator::"<<method<<"(): "
                   "Invalid iterator state");
}


Uint4 CSNPDbGraphIterator::GetTotalValue() const
{
    x_CheckValid("CSNPDbGraphIterator::GetTotalValue");
    return *m_Cur->GR_TOTAL(m_CurrPageRowId);
}


CVDBValueFor<Uint4> CSNPDbGraphIterator::GetCoverageValues() const
{
    x_CheckValid("CSNPDbGraphIterator::GetCoverageValues");
    return m_Cur->GR_ZOOM(m_CurrPageRowId);
}


/////////////////////////////////////////////////////////////////////////////
// CSNPDbFeatIterator
/////////////////////////////////////////////////////////////////////////////


void CSNPDbFeatIterator::Reset(void)
{
    if ( m_Graph ) {
        GetDb().Put(m_Graph, x_GetGraphVDBRowId());
    }
    if ( m_Extra ) {
        GetDb().Put(m_Extra, m_ExtraRowId);
    }
    if ( m_Feat ) {
        GetDb().Put(m_Feat, m_CurrFeatId);
    }
    m_PageIter.Reset();
    m_CurrFeatId = m_FirstBadFeatId = 0;
}


inline
void CSNPDbFeatIterator::x_InitPage(void)
{
    m_CurrFeatId = 0;
    m_FirstBadFeatId = 0;
    if ( m_PageIter ) {
        if ( m_Graph && !*m_Graph->GR_TOTAL(x_GetGraphVDBRowId()) ) {
            // track graph says there's no matching features on current page
            return;
        }
        if ( TVDBRowCount count = GetPageIter().GetFeatCount() ) {
            if ( !m_Feat ) {
                m_Feat = GetDb().Feat();
            }
            TVDBRowId first = GetPageIter().GetFirstFeatRowId();
            m_CurrFeatId = first;
            m_FirstBadFeatId = first + count;
        }
    }
}


CSNPDbFeatIterator::CSNPDbFeatIterator(void)
    : m_CurrFeatId(0),
      m_FirstBadFeatId(0)
{
}


CSNPDbFeatIterator::CSNPDbFeatIterator(const CSNPDb& db,
                                       const CSeq_id_Handle& ref_id,
                                       TSeqPos ref_pos,
                                       TSeqPos window,
                                       const SSelector& sel)
    : m_PageIter(db, ref_id, ref_pos, window, sel.m_SearchMode)
{
    x_SetFilter(sel);
    x_InitPage();
    x_Settle();
}


CSNPDbFeatIterator::CSNPDbFeatIterator(const CSNPDb& db,
                                       const CSeq_id_Handle& ref_id,
                                       COpenRange<TSeqPos> range,
                                       const SSelector& sel)
    : m_PageIter(db, ref_id, range, sel.m_SearchMode)
{
    x_SetFilter(sel);
    x_InitPage();
    x_Settle();
}


CSNPDbFeatIterator::CSNPDbFeatIterator(const CSNPDbSeqIterator& seq,
                                       COpenRange<TSeqPos> range,
                                       const SSelector& sel)
    : m_PageIter(seq, range, sel.m_SearchMode)
{
    x_SetFilter(sel);
    x_InitPage();
    x_Settle();
}


CSNPDbFeatIterator::CSNPDbFeatIterator(const CSNPDbFeatIterator& iter)
{
    *this = iter;
}


CSNPDbFeatIterator&
CSNPDbFeatIterator::operator=(const CSNPDbFeatIterator& iter)
{
    if ( this != &iter ) {
        Reset();
        // params
        m_CurRange = iter.m_CurRange;
        m_Filter = iter.m_Filter;
        // page iter
        m_PageIter = iter.m_PageIter;
        // feat iter
        m_Feat = iter.m_Feat;
        m_CurrFeatId = iter.m_CurrFeatId;
        m_FirstBadFeatId = iter.m_FirstBadFeatId;
        // extra iter
        m_Extra = iter.m_Extra;
        m_ExtraRowId = iter.m_ExtraRowId;
        // page track
        m_Graph = iter.m_Graph;
        m_GraphBaseRowId = iter.m_GraphBaseRowId;
    }
    return *this;
}


CSNPDbFeatIterator::~CSNPDbFeatIterator(void)
{
    Reset();
}


TVDBRowId CSNPDbFeatIterator::x_GetGraphVDBRowId() const
{
    return m_GraphBaseRowId + m_PageIter.GetPagePos()/kPageSize;
}


void CSNPDbFeatIterator::x_SetFilter(const SSelector& sel)
{
    m_Filter = sel.m_Filter;
    m_Filter.Normalize();
    m_GraphBaseRowId = 0;
    if ( m_Filter.m_FilterMask ) {
        // find best track for page filtering
        Uint8 best_bits_count = 0;
        CSNPDb_Impl::TTrackInfoList::const_iterator best_track;
        ITERATE ( CSNPDb_Impl::TTrackInfoList, it, GetDb().GetTrackInfoList() ) {
            TFilter mask = it->m_Filter.m_FilterMask;
            if ( mask & ~m_Filter.m_FilterMask ) {
                // track filter by other bits than requested
                continue;
            }
            if ( !m_Filter.Matches(it->m_Filter.m_Filter) ) {
                // track's bits differ from requested
                continue;
            }
            Uint8 bits_count = x_SetBitCount(mask);
            if ( bits_count > best_bits_count ) {
                best_bits_count = bits_count;
                best_track = it;
            }
        }
        if ( best_bits_count ) {
            m_GraphBaseRowId =
                GetDb().x_GetGraphVDBRowId(x_GetSeqIter(), best_track);
            m_Graph = GetDb().Graph(x_GetGraphVDBRowId());
        }
    }
}


CSNPDbFeatIterator&
CSNPDbFeatIterator::Select(COpenRange<TSeqPos> ref_range,
                           const SSelector& sel)
{
    m_PageIter.Select(ref_range, sel.m_SearchMode);
    x_SetFilter(sel);
    x_InitPage();
    x_Settle();
    return *this;
}


CTempString CSNPDbFeatIterator::GetFeatType(void) const
{
    x_CheckValid("CSNPDbFeatIterator::GetFeatType");
    return *Cur().FEAT_TYPE(m_CurrFeatId);
}


CSNPDbFeatIterator::EFeatSubtype CSNPDbFeatIterator::GetFeatSubtype(void) const
{
    x_CheckValid("CSNPDbFeatIterator::GetFeatSubtype");
    return EFeatSubtype(*Cur().FEAT_SUBTYPE(m_CurrFeatId));
}


char CSNPDbFeatIterator::GetFeatSubtypeChar(EFeatSubtype subtype)
{
    return kFeatSubtypesToChars[subtype];
}


CTempString CSNPDbFeatIterator::GetFeatSubtypeString(EFeatSubtype subtype)
{
    return CTempString(kFeatSubtypesToChars+subtype, 1);
}


CTempString CSNPDbFeatIterator::GetFeatSubtypeString(void) const
{
    return GetFeatSubtypeString(GetFeatSubtype());
}


TSeqPos CSNPDbFeatIterator::x_GetFrom(void) const
{
    return *Cur().FROM(m_CurrFeatId);
}


TSeqPos CSNPDbFeatIterator::x_GetLength(void) const
{
    return *Cur().LEN(m_CurrFeatId);
}


inline
CSNPDbFeatIterator::EExcluded CSNPDbFeatIterator::x_Excluded(void)
{
    TSeqPos ref_pos = x_GetFrom();
    if ( ref_pos >= GetSearchRange().GetToOpen() ) {
        // no more
        return ePassedTheRegion;
    }
    if ( GetSearchMode() == eSearchByStart &&
         ref_pos < GetSearchRange().GetFrom() ) {
        return eExluded;
    }
    TSeqPos ref_len = x_GetLength();
    if ( ref_len == 0 ) { // insertion SNP
        // make 2-base interval with insertion point in the middle, if possible
        if ( ref_pos > 0 ) {
            --ref_pos;
            ref_len = 2;
        }
    }
    TSeqPos ref_end = ref_pos + ref_len;
    if ( ref_end <= GetSearchRange().GetFrom() ) {
        return eExluded;
    }
    if ( m_Filter.IsSet() ) {
        if ( !m_Filter.Matches(GetBitfield()) ) {
            return eExluded;
        }
    }
    m_CurRange.SetFrom(ref_pos);
    m_CurRange.SetToOpen(ref_end);
    return eIncluded;
}


void CSNPDbFeatIterator::x_Settle(void)
{
    while ( m_PageIter ) {
        while ( m_CurrFeatId < m_FirstBadFeatId ) {
            EExcluded exc = x_Excluded();
            if ( exc == eIncluded ) {
                // found
                return;
            }
            if ( exc == ePassedTheRegion ) {
                // passed the region
                break;
            }
            // next feat in page
            ++m_CurrFeatId;
        }

        ++m_PageIter;
        x_InitPage();
    }
}


void CSNPDbFeatIterator::x_Next(void)
{
    x_CheckValid("CSNPDbFeatIterator::operator++");
    ++m_CurrFeatId;
    x_Settle();
}


void CSNPDbFeatIterator::x_ReportInvalid(const char* method) const
{
    NCBI_THROW_FMT(CSraException, eInvalidState,
                   "CSNPDbFeatIterator::"<<method<<"(): "
                   "Invalid iterator state");
}


Uint4 CSNPDbFeatIterator::GetFeatIdPrefix(void) const
{
    x_CheckValid("CSNPDbFeatIterator::GetFeatIdPrefix");
    return *Cur().FEAT_ID_PREFIX(m_CurrFeatId);
}


Uint8 CSNPDbFeatIterator::GetFeatId(void) const
{
    x_CheckValid("CSNPDbFeatIterator::GetFeatId");
    return *Cur().FEAT_ID_VALUE(m_CurrFeatId);
}


CSNPDbFeatIterator::TExtraRange CSNPDbFeatIterator::GetExtraRange(void) const
{
    x_CheckValid("CSNPDbFeatIterator::x_GetExtraRange");
    TVDBRowId first = 0;
    TVDBRowCount count = *Cur().EXTRA_ROWS_COUNT(m_CurrFeatId);
    if ( count ) {
        first = *Cur().EXTRA_ROW_FROM(m_CurrFeatId);
        if ( !m_Extra ) {
            m_Extra = GetDb().Extra(first);
        }
        m_ExtraRowId = first + count - 1;
    }
    return TExtraRange(first, count);
}


CTempString CSNPDbFeatIterator::GetAllele(const TExtraRange& range,
                                          size_t index) const
{
    _ASSERT(index < range.second);
    return *m_Extra->RS_ALLELE(range.first + index);
}


Uint8 CSNPDbFeatIterator::GetBitfield(void) const
{
    x_CheckValid("CSNPDbFeatIterator::GetBitfield");
    return *Cur().BIT_FLAGS(m_CurrFeatId);
}


void CSNPDbFeatIterator::GetBitfieldOS(vector<char>& os) const
{
    x_SetOS8(os, GetBitfield());
}


template<size_t ValueSize>
static inline
bool x_IsStringConstant(const string& str, const char (&value)[ValueSize])
{
    return str.size() == ValueSize-1 && str == value;
}

#define x_SetStringConstant(obj, Field, value)                          \
    if ( !(obj).NCBI_NAME2(IsSet,Field)() ||                            \
         !x_IsStringConstant((obj).NCBI_NAME2(Get,Field)(), value) ) {  \
        (obj).NCBI_NAME2(Set,Field)((value));                           \
    }


template<class T>
static inline
T& x_GetPrivate(CRef<T>& ref)
{
    T* ptr = ref.GetPointerOrNull();
    if ( !ptr || !ptr->ReferencedOnlyOnce() ) {
        ref = ptr = new T;
    }
    return *ptr;
}


struct CSNPDbFeatIterator::SCreateCache {
    CRef<CSeq_feat> m_Feat;
    CRef<CImp_feat> m_Imp;
    CRef<CSeq_interval> m_LocInt;
    CRef<CSeq_point> m_LocPnt;
    CRef<CGb_qual> m_Allele[4];
    CRef<CDbtag> m_Dbtag;
    CRef<CUser_object> m_Ext;
    CRef<CObject_id> m_ObjectIdQAdata;
    CRef<CObject_id> m_ObjectIdBitfield;
    CRef<CObject_id> m_ObjectIdSubtype;
    CRef<CUser_field> m_Bitfield;
    CRef<CUser_field> m_Subtype;

#define ALLELE_CACHE
#ifdef ALLELE_CACHE
    CRef<CGb_qual> m_AlleleCache_empty;
    CRef<CGb_qual> m_AlleleCache_minus;
    CRef<CGb_qual> m_AlleleCacheA;
    CRef<CGb_qual> m_AlleleCacheC;
    CRef<CGb_qual> m_AlleleCacheG;
    CRef<CGb_qual> m_AlleleCacheT;
#endif

    CGb_qual& x_GetCommonAllele(CRef<CGb_qual>& cache, CTempString val)
        {
            CGb_qual* qual = cache.GetPointerOrNull();
            if ( !qual ) {
                cache = qual = new CGb_qual;
                qual->SetQual("replace");
                qual->SetVal(val);
            }
            return *qual;
        }
    CGb_qual& x_GetCachedAllele(CRef<CGb_qual>& cache, CTempString val)
        {
            CGb_qual& qual = x_GetPrivate(cache);
            x_SetStringConstant(qual, Qual, "replace");
            qual.SetVal(val);
            return qual;
        }
    CGb_qual& GetAllele(CRef<CGb_qual>& cache, CTempString val)
        {
#ifdef ALLELE_CACHE
            if ( val.size() == 1 ) {
                switch ( val[0] ) {
                case 'A': return x_GetCommonAllele(m_AlleleCacheA, val);
                case 'C': return x_GetCommonAllele(m_AlleleCacheC, val);
                case 'G': return x_GetCommonAllele(m_AlleleCacheG, val);
                case 'T': return x_GetCommonAllele(m_AlleleCacheT, val);
                case '-': return x_GetCommonAllele(m_AlleleCache_minus, val);
                default: break;
                }
            }
            if ( val.size() == 0 ) {
                return x_GetCommonAllele(m_AlleleCache_empty, val);
            }
#endif
            return x_GetCachedAllele(cache, val);
        }
};


inline
CSNPDbFeatIterator::SCreateCache& CSNPDbFeatIterator::x_GetCreateCache(void) const
{
    if ( !m_CreateCache ) {
        m_CreateCache = new SCreateCache;
    }
    return *m_CreateCache;
}


static inline
CObject_id& x_GetObject_id(CRef<CObject_id>& cache, const char* name)
{
    if ( !cache ) {
        cache = new CObject_id();
        cache->SetStr(name);
    }
    return *cache;
}


CRef<CSeq_feat> CSNPDbFeatIterator::GetSeq_feat(TFlags flags) const
{
    x_CheckValid("CSNPDbFeatIterator::GetSeq_feat");

    if ( !(flags & fUseSharedObjects) ) {
        m_CreateCache.reset();
    }
    SCreateCache& cache = x_GetCreateCache();
    CSeq_feat& feat = x_GetPrivate(cache.m_Feat);
    {{
        CSeqFeatData& data = feat.SetData();
        data.Reset();
        CImp_feat& imp = x_GetPrivate(cache.m_Imp);
        x_SetStringConstant(imp, Key, "variation");
        imp.ResetLoc();
        imp.ResetDescr();
        data.SetImp(imp);
    }}
    {{
        CSeq_loc& loc = feat.SetLocation();
        TSeqPos len = GetSNPLength();
        loc.Reset();
        if ( len == 1 ) {
            CSeq_point& loc_pnt = x_GetPrivate(cache.m_LocPnt);
            loc_pnt.SetId(*GetSeqId());
            TSeqPos pos = GetSNPPosition();
            loc_pnt.SetPoint(pos);
            loc.SetPnt(loc_pnt);
        }
        else {
            CSeq_interval& loc_int = x_GetPrivate(cache.m_LocInt);
            loc_int.SetId(*GetSeqId());
            TSeqPos pos = GetSNPPosition();
            loc_int.SetFrom(pos);
            loc_int.SetTo(pos+len-1);
            loc.SetInt(loc_int);
        }
    }}
    if ( flags & fIncludeAlleles ) {
        CSeq_feat::TQual& quals = feat.SetQual();
        pair<TVDBRowId, size_t> range = GetExtraRange();
        quals.assign(range.second, null);
        for ( size_t i = 0; i < range.second; ++i ) {
            CTempString allele = GetAllele(range, i);
            size_t cache_index = min(i, ArraySize(cache.m_Allele)-1);
            CGb_qual& qual =
                cache.GetAllele(cache.m_Allele[cache_index], allele);
            quals[i] = &qual;
        }
    }
    else {
        feat.ResetQual();
    }
    if ( flags & fIncludeRsId ) {
        CSeq_feat::TDbxref& dbxref = feat.SetDbxref();
        dbxref.resize(1);
        dbxref[0] = null;
        CDbtag& dbtag = x_GetPrivate(cache.m_Dbtag);
        x_SetStringConstant(dbtag, Db, "dbSNP");
        Uint8 feat_id = GetFeatId();
        switch ( GetFeatIdPrefix() ) {
        case eFeatIdPrefix_rs:
            dbtag.SetTag().SetStr("rs"+NStr::NumericToString(feat_id));
            break;
        case eFeatIdPrefix_ss:
            dbtag.SetTag().SetStr("ss"+NStr::NumericToString(feat_id));
            break;
        default:
            dbtag.SetTag().SetId8(feat_id);
            break;
        }
        dbxref[0] = &dbtag;
    }
    else {
        feat.ResetDbxref();
    }
    feat.ResetExt();
    TFlags ext_flags =
        ToFlags(fIncludeBitfield) | fIncludeNeighbors | fIncludeSubtype;
    if ( flags & ext_flags ) {
        CUser_object& ext = x_GetPrivate(cache.m_Ext);
        ext.SetType(x_GetObject_id(cache.m_ObjectIdQAdata,
                                   "dbSnpQAdata"));
        CUser_object::TData& data = ext.SetData();
        data.clear();
        if ( flags & fIncludeNeighbors ) {
        }
        if ( flags & fIncludeSubtype ) {
            CUser_field& field = x_GetPrivate(cache.m_Subtype);
            field.SetLabel(x_GetObject_id(cache.m_ObjectIdSubtype,
                                          "VariationClass"));
            field.SetData().SetStr(GetFeatSubtypeString());
            ext.SetData().push_back(Ref(&field));
        }
        if ( flags & fIncludeBitfield ) {
            CUser_field& field = x_GetPrivate(cache.m_Bitfield);
            field.SetLabel(x_GetObject_id(cache.m_ObjectIdBitfield,
                                          "Bitfield"));
            GetBitfieldOS(field.SetData().SetOs());
            ext.SetData().push_back(Ref(&field));
        }
        feat.SetExt(ext);
    }
    return Ref(&feat);
}


/////////////////////////////////////////////////////////////////////////////


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;
