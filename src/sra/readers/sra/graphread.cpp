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
#include <sra/readers/sra/graphread.hpp>
#include <sra/readers/ncbi_traces_path.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbi_param.hpp>
#include <objects/general/general__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqset/seqset__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/seqres/seqres__.hpp>
#include <objects/seqtable/seqtable__.hpp>
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

#define NCBI_USE_ERRCODE_X   VDBGraphReader
NCBI_DEFINE_ERR_SUBCODE_X(1);

BEGIN_NAMESPACE(objects);


/////////////////////////////////////////////////////////////////////////////
// CVDBGraphDb_Impl
/////////////////////////////////////////////////////////////////////////////


CVDBGraphDb_Impl::SGraphTableCursor::SGraphTableCursor(const CVDBGraphDb_Impl& db)
    : m_Table(db.m_Mgr, db.GetPath()),
      m_Cursor(m_Table),
      INIT_VDB_COLUMN(SID),
      INIT_VDB_COLUMN(START),
      INIT_VDB_COLUMN(LEN),
      INIT_VDB_COLUMN(GR_Q0),
      INIT_VDB_COLUMN(GR_Q10),
      INIT_VDB_COLUMN(GR_Q50),
      INIT_VDB_COLUMN(GR_Q90),
      INIT_VDB_COLUMN(GR_Q100),
      INIT_OPTIONAL_VDB_COLUMN(GR_ZOOM_Q0),
      INIT_OPTIONAL_VDB_COLUMN(GR_ZOOM_Q10),
      INIT_OPTIONAL_VDB_COLUMN(GR_ZOOM_Q50),
      INIT_OPTIONAL_VDB_COLUMN(GR_ZOOM_Q90),
      INIT_OPTIONAL_VDB_COLUMN(GR_ZOOM_Q100),
      INIT_VDB_COLUMN(GRAPH),
      INIT_VDB_COLUMN(SCALE),
      INIT_OPTIONAL_VDB_COLUMN(NUM_SWITCHES)
{
}


CVDBGraphDb_Impl::CVDBGraphDb_Impl(CVDBMgr& mgr, CTempString path)
    : m_Mgr(mgr),
      m_Path(path)
{
    CRef<SGraphTableCursor> curs = Graph();

    uint64_t last_row = curs->m_Cursor.GetMaxRowId();
    SSeqInfo info;
    const KIndex* idx;
    if ( rc_t rc = VTableOpenIndexRead(curs->m_Table, &idx, "sid") ) {
        LOG_POST(Warning<<"CVDBGraphDb: cannot open SID index: "<<
                 CSraRcFormatter(rc)<<". Scanning sequentially.");
        for ( uint64_t row = 1; row <= last_row; ++row ) {
            // read range and names
            TSeqPos len = *curs->LEN(row), row_size = *curs->LEN(row);
            CVDBStringValue seq_id = curs->SID(row);
            if ( *seq_id == info.m_SeqId ) {
                info.m_SeqLength += len;
                continue;
            }
            if ( !info.m_SeqId.empty() ) {
                info.m_RowLast = row-1;
                m_SeqList.push_back(info);
            }
            info.m_SeqId = *seq_id;
            info.m_Seq_id_Handle = CSeq_id_Handle::GetHandle(info.m_SeqId);
            info.m_SeqLength = len;
            info.m_RowSize = row_size;
            info.m_StartBase = *curs->START(row);
            info.m_RowFirst = row;
        }
        if ( !info.m_SeqId.empty() ) {
            info.m_RowLast = last_row;
            m_SeqList.push_back(info);
        }
    }
    else {
        for ( uint64_t row = 1; row <= last_row; ++row ) {
            CVDBStringValue seq_id = curs->SID(row);
            info.m_SeqId = *seq_id;
            info.m_Seq_id_Handle = CSeq_id_Handle::GetHandle(info.m_SeqId);
            info.m_RowSize = *curs->LEN(row);
            info.m_StartBase = *curs->START(row);
            info.m_RowFirst = row;
            int64_t first;
            uint64_t count;
            if ( rc_t rc = KIndexFindText(idx, info.m_SeqId.c_str(),
                                          &first, &count, 0, 0) ) {
                NCBI_THROW2(CSraException, eInitFailed,
                            "Cannot read SID index", rc);
            }
            _ASSERT(int64_t(row) == first);
            _ASSERT(count > 0);
            info.m_RowLast = row += count-1;
            info.m_SeqLength =
                TSeqPos(*curs->START(row)+*curs->LEN(row)-info.m_StartBase);
            m_SeqList.push_back(info);
        }
        if ( rc_t rc = KIndexRelease(idx) ) {
            NCBI_THROW2(CSraException, eInitFailed,
                        "Cannot release SID index", rc);
        }
    }
    Put(curs);
    
    NON_CONST_ITERATE ( TSeqInfoList, it, m_SeqList ) {
        m_SeqMapBySeq_id.insert
            (TSeqInfoMapBySeq_id::value_type(it->m_Seq_id_Handle, it));
    }
}


CRef<CVDBGraphDb_Impl::SGraphTableCursor> CVDBGraphDb_Impl::Graph(void)
{
    CRef<SGraphTableCursor> curs = m_Graph.Get();
    if ( !curs ) {
        curs = new SGraphTableCursor(*this);
    }
    return curs;
}


bool CVDBGraphDb_Impl::HasMidZoomGraphs(void)
{
    CRef<SGraphTableCursor> curs = Graph();
    bool ret = curs->m_GR_ZOOM_Q100;
    Put(curs);
    return ret;
}


/////////////////////////////////////////////////////////////////////////////
// CVDBGraphSeqIterator
/////////////////////////////////////////////////////////////////////////////

CVDBGraphSeqIterator::CVDBGraphSeqIterator(const CVDBGraphDb& db)
    : m_Db(db),
      m_Iter(db->GetSeqInfoList().begin())
{
}


CVDBGraphSeqIterator::CVDBGraphSeqIterator(const CVDBGraphDb& db,
                                           const CSeq_id_Handle& seq_id)
{
    CVDBGraphDb_Impl::TSeqInfoMapBySeq_id::const_iterator iter =
        db->m_SeqMapBySeq_id.find(seq_id);
    if ( iter != db->m_SeqMapBySeq_id.end() ) {
        m_Db = db;
        m_Iter = iter->second;
    }
}


const CVDBGraphDb_Impl::SSeqInfo& CVDBGraphSeqIterator::GetInfo(void) const
{
    if ( !*this ) {
        NCBI_THROW(CSraException, eInvalidState,
                   "CVDBGraphSeqIterator is invalid");
    }
    return *m_Iter;
}


template<class DstVector, class SrcVector>
static void sx_Assign(DstVector& dst, const SrcVector& src)
{
    dst.clear();
    dst.reserve(src.size());
    ITERATE ( typename SrcVector, it, src ) {
        dst.push_back(*it);
    }
}


CRef<CSeq_graph>
CVDBGraphSeqIterator::x_MakeGraph(const string& annot_name,
                                  CSeq_loc& loc,
                                  const SSeqInfo& info,
                                  const COpenRange<TSeqPos>& range,
                                  TSeqPos step,
                                  SGraphTableCursor& cursor,
                                  CVDBColumn& column,
                                  int level) const
{
    if ( !column ) {
        return null;
    }

    CRef<CSeq_graph> graph(new CSeq_graph);
    if ( !annot_name.empty() ) {
        graph->SetTitle(annot_name);
    }
    graph->SetLoc(loc);
    if ( level >= 0 ) {
        graph->SetComment(NStr::IntToString(level)+"%");
    }

    typedef SGraphTableCursor::TGraphQ TValue;
    const TValue kMaxIntValue = kMax_I4;
    const TValue kMaxByteValue = kMax_UI1;

    TValue max_v = 0, min_v = numeric_limits<TValue>::max();
    CInt_graph* int_graph = &graph->SetGraph().SetInt();
    CReal_graph* real_graph = 0;
    CInt_graph::TValues* int_vv = &int_graph->SetValues();
    CReal_graph::TValues* real_vv = 0;
    TSeqPos row_size = info.m_RowSize;
    TSeqPos pos = range.GetFrom();
    for ( uint64_t row = info.m_RowFirst + pos/row_size;
          pos < range.GetToOpen();
          ++row ) {
        CVDBValueFor<TValue> values(cursor.m_Cursor, row, column);
        for ( size_t index = pos%row_size/step;
              index < values.size() && pos < range.GetToOpen();
              ++index, pos += step ) {
            TValue v = values[index];
            if ( v < min_v ) {
                min_v = v;
            }
            if ( v > max_v ) {
                max_v = v;
                if ( max_v > kMaxIntValue ) {
                    // switch to real graph
                    CRef<CInt_graph> save_int_graph(int_graph);
                    real_graph = &graph->SetGraph().SetReal();
                    int_graph = 0;
                    real_vv = &real_graph->SetValues();
                    sx_Assign(*real_vv, *int_vv);
                    int_vv = 0;
                }
            }
            if ( int_vv ) {
                int_vv->push_back(int(v));
            }
            else {
                real_vv->push_back(double(v));
            }
        }
        if ( pos < range.GetToOpen() &&
             pos < (row-info.m_RowFirst+1)*row_size ) {
            NCBI_THROW(CSraException, eNotFoundValue,
                       "CVDBGraphSeqIterator: graph data array is too short");
        }
    }
    size_t numval = 0;
    if ( max_v <= kMaxByteValue ) {
        // use smaller byte representation
        numval = int_vv->size();
        CRef<CByte_graph> byte_graph(new CByte_graph);
        byte_graph->SetAxis(0);
        byte_graph->SetMin(int(min_v));
        byte_graph->SetMax(int(max_v));
        sx_Assign(byte_graph->SetValues(), *int_vv);
        graph->SetGraph().SetByte(*byte_graph);
        int_graph = 0;
        int_vv = 0;
    }
    else if ( max_v > kMaxIntValue ) {
        // need bigger double representation
        numval = real_vv->size();
        real_graph->SetAxis(0);
        real_graph->SetMin(double(min_v));
        real_graph->SetMax(double(max_v));
    }
    else {
        // int graph
        numval = int_vv->size();
        int_graph->SetAxis(0);
        int_graph->SetMin(int(min_v));
        int_graph->SetMax(int(max_v));
    }
    if ( step > 1 ) {
        graph->SetComp(step);
    }
    uint32_t scale = cursor.SCALE(info.m_RowFirst);
    if ( scale != 1 ) {
        graph->SetA(1./scale);
    }
    if ( numval > size_t(kMax_Int) ) {
        NCBI_THROW(CSraException, eDataError,
                   "CVDBGraphSeqIterator::x_MakeGraph: graph too big");
    }
    graph->SetNumval(int(numval));
    return graph;
}


CRef<CSeq_table>
CVDBGraphSeqIterator::x_MakeTable(const string& annot_name,
                                  CSeq_loc& loc,
                                  const SSeqInfo& info,
                                  const COpenRange<TSeqPos>& range,
                                  SGraphTableCursor& cursor) const
{
    TSeqPos size = range.GetLength();
    TSeqPos row_size = info.m_RowSize;
    typedef SGraphTableCursor::TGraphQ TValue;
    const TValue kMaxIntValue = kMax_I4;

    TValue max_v = 0;
    vector<TValue> vv;
    vv.reserve(size);
    TSeqPos pos = range.GetFrom();
    uint64_t row = pos/row_size;
    for ( ; pos < range.GetToOpen(); ++row, pos += row_size ) {
        CVDBValueFor<TValue> vv_arr(cursor.GRAPH(info.m_RowFirst+row));
        TSeqPos off = TSeqPos(pos - row*row_size);
        TSeqPos cnt = min(row_size-off, range.GetToOpen()-pos);
        for ( TSeqPos i = 0; i < cnt; ++i ) {
            TValue v = vv_arr[off+i];
            vv.push_back(v);
            if ( v > max_v ) {
                max_v = v;
            }
        }
    }

    CRef<CSeq_table> table(new CSeq_table);
    table->SetFeat_type(0);

    { // Seq-table location
        CRef<CSeqTable_column> col_id(new CSeqTable_column);
        table->SetColumns().push_back(col_id);
        col_id->SetHeader().SetField_name("Seq-table location");
        col_id->SetDefault().SetLoc(loc);
    }
    { // Seq-id
        CRef<CSeqTable_column> col_id(new CSeqTable_column);
        table->SetColumns().push_back(col_id);
        col_id->SetHeader().SetField_id(CSeqTable_column_info::eField_id_location_id);
        col_id->SetDefault().SetId(loc.SetInt().SetId());
    }

    // position
    CRef<CSeqTable_column> col_pos(new CSeqTable_column);
    table->SetColumns().push_back(col_pos);
    col_pos->SetHeader().SetField_id(CSeqTable_column_info::eField_id_location_from);
    CSeqTable_multi_data::TInt& arr_pos = col_pos->SetData().SetInt();

    // span
    CRef<CSeqTable_column> col_span(new CSeqTable_column);
    table->SetColumns().push_back(col_span);
    col_span->SetHeader().SetField_name("span");
    CSeqTable_multi_data::TInt& arr_span = col_span->SetData().SetInt();

    CRef<CSeqTable_column> col_val(new CSeqTable_column);
    table->SetColumns().push_back(col_val);
    col_val->SetHeader().SetField_name("values");

    TSeqPos cur_i = 0;
    TValue cur_v = vv[0];
    if ( max_v > kMaxIntValue ) {
        CSeqTable_multi_data::TReal& arr_vv = col_val->SetData().SetReal();
        for ( TSeqPos i = 0; i < size; ++i ) {
            TValue v = vv[i];
            if ( v != cur_v ) {
                arr_pos.push_back(cur_i+range.GetFrom());
                arr_span.push_back(i-cur_i);
                arr_vv.push_back(double(cur_v));
                cur_i = i;
                cur_v = v;
            }
        }
        arr_pos.push_back(cur_i+range.GetFrom());
        arr_span.push_back(size-cur_i);
        arr_vv.push_back(double(cur_v));
    }
    else {
        CSeqTable_multi_data::TInt& arr_vv = col_val->SetData().SetInt();
        for ( TSeqPos i = 0; i < size; ++i ) {
            TValue v = vv[i];
            if ( v != cur_v ) {
                arr_pos.push_back(cur_i+range.GetFrom());
                arr_span.push_back(i-cur_i);
                arr_vv.push_back(int(cur_v));
                cur_i = i;
                cur_v = v;
            }
        }
        arr_pos.push_back(cur_i+range.GetFrom());
        arr_span.push_back(size-cur_i);
        arr_vv.push_back(int(cur_v));
    }

    uint32_t scale = cursor.SCALE(info.m_RowFirst+row);
    if ( scale != 1 ) {
        CRef<CSeqTable_column> col_step(new CSeqTable_column);
        table->SetColumns().push_back(col_step);
        col_step->SetHeader().SetField_name("value_step");
        col_step->SetDefault().SetReal(1./scale);
    }
    if ( arr_pos.size() > size_t(kMax_Int) ) {
        NCBI_THROW(CSraException, eDataError,
                   "CVDBGraphSeqIterator::x_MakeTable: graph too big");
    }
    table->SetNum_rows(int(arr_pos.size()));
    return table;
}


bool CVDBGraphSeqIterator::x_SeqTableIsSmaller(COpenRange<TSeqPos> range,
                                               SGraphTableCursor& cursor) const
{
    if ( !cursor.m_NUM_SWITCHES ) {
        // no info about value changes
        return false;
    }
    typedef SGraphTableCursor::TGraphV TValue;
    const TValue kMaxIntValue = kMax_I4;
    const TValue kMaxByteValue = kMax_UI1;
    TValue max_v = 0;
    size_t values = 0, switches = 0;
    TSeqPos pos = range.GetFrom();
    const SSeqInfo& info = GetInfo();
    TSeqPos row_size = info.m_RowSize;
    uint64_t row = pos/row_size;
    for ( ; pos < range.GetToOpen(); ++row, pos += row_size ) {
        values += row_size;
        switches += cursor.NUM_SWITCHES(info.m_RowFirst+row);
        TValue v = cursor.GR_Q100(info.m_RowFirst+row);
        if ( v > max_v ) {
            max_v = v;
        }
    }
    size_t table_value_size =
        max_v > kMaxIntValue? sizeof(double): sizeof(int);
    size_t graph_value_size =
        max_v > kMaxByteValue? table_value_size: 1;
    size_t table_size =
        (table_value_size+2*sizeof(int))*switches; //+pos+span
    size_t graph_size =
        graph_value_size*values;
    return table_size < graph_size;
}


bool CVDBGraphSeqIterator::SeqTableIsSmaller(COpenRange<TSeqPos> range) const
{
    const SSeqInfo& info = GetInfo();
    if ( range.GetToOpen() > info.m_SeqLength ) {
        range.SetToOpen(info.m_SeqLength);
    }
    if ( range.Empty() ) {
        return false;
    }
    CRef<SGraphTableCursor> curs(GetDb().Graph());
    bool seq_table_is_smaller = x_SeqTableIsSmaller(range, *curs);
    GetDb().Put(curs);
    return seq_table_is_smaller;
}


CRef<CSeq_annot>
CVDBGraphSeqIterator::GetAnnot(COpenRange<TSeqPos> range0,
                               const string& annot_name,
                               TContentFlags content) const
{
    const SSeqInfo& info = GetInfo();
    if ( range0.GetToOpen() > info.m_SeqLength ) {
        range0.SetToOpen(info.m_SeqLength);
    }
    if ( range0.Empty() ) {
        return null;
    }

    CRef<CSeq_annot> annot(new CSeq_annot);

    if ( !annot_name.empty() ) {
        CRef<CAnnotdesc> desc(new CAnnotdesc);
        desc->SetName(annot_name);
        annot->SetDesc().Set().push_back(desc);
    }

    CRef<SGraphTableCursor> curs(GetDb().Graph());

    if ( content & fGraphQAll ) {
        if ( content & (fGraphZoomQAll|fGraphMain) ) {
            NCBI_THROW(CSraException, eInvalidArg, "CVDBGraphSeqIterator: "
                       "several zoom tracks are requested");
        }

        TSeqPos step = info.m_RowSize;
        
        COpenRange<TSeqPos> range = range0;
        if ( TSeqPos adjust = range.GetFrom() % step ) {
            range.SetFrom(range.GetFrom() - adjust);
        }
        if ( TSeqPos adjust = range.GetToOpen() % step ) {
            range.SetToOpen(min(info.m_SeqLength,
                                range.GetToOpen() + (step - adjust)));
        }

        CRef<CSeq_loc> loc(new CSeq_loc);
        loc->SetInt().SetId(*SerialClone(*info.m_Seq_id_Handle.GetSeqId()));
        loc->SetInt().SetFrom(range.GetFrom());
        loc->SetInt().SetTo(range.GetTo());

        // describe statistics
        {
            CRef<CAnnotdesc> desc(new CAnnotdesc);
            CUser_object& obj = desc->SetUser();
            obj.SetType().SetStr("AnnotationTrack");
            obj.AddField("ZoomLevel", int(step));
            obj.AddField("StatisticsType", "Percentiles");
            annot->SetDesc().Set().push_back(desc);
        }

        for ( TContentFlags f = fGraphQ0; f & fGraphQAll; f <<= 1 ) {
            if ( !(content & f) ) {
                continue;
            }
            CRef<CSeq_graph> graph;
            switch ( f ) {
            case fGraphQ0  :
                graph = x_MakeGraph(annot_name, *loc, info, range,
                                    step, *curs, curs->m_GR_Q0  ,   0);
                break;
            case fGraphQ10 :
                graph = x_MakeGraph(annot_name, *loc, info, range,
                                    step, *curs, curs->m_GR_Q10 ,  10);
                break;
            case fGraphQ50 :
                graph = x_MakeGraph(annot_name, *loc, info, range,
                                    step, *curs, curs->m_GR_Q50 ,  50);
                break;
            case fGraphQ90 :
                graph = x_MakeGraph(annot_name, *loc, info, range,
                                    step, *curs, curs->m_GR_Q90 ,  90);
                break;
            case fGraphQ100:
                graph = x_MakeGraph(annot_name, *loc, info, range,
                                    step, *curs, curs->m_GR_Q100, 100);
                break;
            default:
                break;
            }
            if ( graph ) {
                annot->SetData().SetGraph().push_back(graph);
            }
        }
    }

    if ( content & fGraphZoomQAll ) {
        if ( content & (fGraphQAll|fGraphMain) ) {
            NCBI_THROW(CSraException, eInvalidArg, "CVDBGraphSeqIterator: "
                       "several zoom tracks are requested");
        }

        TSeqPos step = 100;

        COpenRange<TSeqPos> range = range0;
        if ( TSeqPos adjust = range.GetFrom() % step ) {
            range.SetFrom(range.GetFrom() - adjust);
        }
        if ( TSeqPos adjust = range.GetToOpen() % step ) {
            range.SetToOpen(min(info.m_SeqLength,
                                range.GetToOpen() + (step - adjust)));
        }

        CRef<CSeq_loc> loc(new CSeq_loc);
        loc->SetInt().SetId(*SerialClone(*info.m_Seq_id_Handle.GetSeqId()));
        loc->SetInt().SetFrom(range.GetFrom());
        loc->SetInt().SetTo(range.GetTo());

        // describe statistics
        {
            CRef<CAnnotdesc> desc(new CAnnotdesc);
            CUser_object& obj = desc->SetUser();
            obj.SetType().SetStr("AnnotationTrack");
            obj.AddField("ZoomLevel", int(step));
            obj.AddField("StatisticsType", "Percentiles");
            annot->SetDesc().Set().push_back(desc);
        }

        for ( TContentFlags f = fGraphZoomQ0; f & fGraphZoomQAll; f <<= 1 ) {
            if ( !(content & f) ) {
                continue;
            }
            CRef<CSeq_graph> graph;
            switch ( f ) {
            case fGraphZoomQ0  :
                graph = x_MakeGraph(annot_name, *loc, info, range,
                                    step, *curs, curs->m_GR_ZOOM_Q0  ,   0);
                break;
            case fGraphZoomQ10 :
                graph = x_MakeGraph(annot_name, *loc, info, range,
                                    step, *curs, curs->m_GR_ZOOM_Q10 ,  10);
                break;
            case fGraphZoomQ50 :
                graph = x_MakeGraph(annot_name, *loc, info, range,
                                    step, *curs, curs->m_GR_ZOOM_Q50 ,  50);
                break;
            case fGraphZoomQ90 :
                graph = x_MakeGraph(annot_name, *loc, info, range,
                                    step, *curs, curs->m_GR_ZOOM_Q90 ,  90);
                break;
            case fGraphZoomQ100:
                graph = x_MakeGraph(annot_name, *loc, info, range,
                                    step, *curs, curs->m_GR_ZOOM_Q100, 100);
                break;
            default:
                break;
            }
            if ( graph ) {
                annot->SetData().SetGraph().push_back(graph);
            }
        }
    }

    if ( content & fGraphMain ) {
        COpenRange<TSeqPos> range = range0;

        CRef<CSeq_loc> loc(new CSeq_loc);
        loc->SetInt().SetId(*SerialClone(*info.m_Seq_id_Handle.GetSeqId()));
        loc->SetInt().SetFrom(range.GetFrom());
        loc->SetInt().SetTo(range.GetTo());
        
        bool as_table = false;
        typedef SGraphTableCursor::TGraphV TValue;
        if ( content & fGraphMainAsTable ) {
            if ( content & fGraphMainAsGraph ) {
                // select best
                as_table = x_SeqTableIsSmaller(range, *curs);
            }
            else {
                as_table = true;
            }
        }
        if ( as_table ) {
            {
                CRef<CAnnotdesc> desc(new CAnnotdesc);
                CUser_object& obj = desc->SetUser();
                obj.SetType().SetStr("Track Data");
                obj.AddField("track type", "graph");
                annot->SetDesc().Set().push_back(desc);
            }

            CRef<CSeq_table> table = x_MakeTable(annot_name, *loc, info, range,
                                                 *curs);
            annot->SetData().SetSeq_table(*table);
        }
        else {
            CRef<CSeq_graph> graph = x_MakeGraph(annot_name, *loc, info, range,
                                                 1, *curs, curs->m_GRAPH, -1);
            annot->SetData().SetGraph().push_back(graph);
        }
    }

    GetDb().Put(curs);

    if ( !annot->IsSetData() ) {
        return null;
    }

    return annot;
}


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;
