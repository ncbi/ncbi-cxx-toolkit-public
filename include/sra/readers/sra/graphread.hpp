#ifndef SRA__READER__SRA__WGSREAD__HPP
#define SRA__READER__SRA__WGSREAD__HPP
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
 *   Access to VDB Graph files
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>
#include <util/range.hpp>
#include <sra/readers/sra/vdbread.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/seqres/Seq_graph.hpp>
#include <map>
#include <list>

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(objects);

class CSeq_entry;
class CSeq_graph;
class CSeq_table;
class CUser_object;
class CUser_field;
class CVDBGraphSeqIterator;

class NCBI_SRAREAD_EXPORT CVDBGraphDb_Impl : public CObject
{
public:
    CVDBGraphDb_Impl(CVDBMgr& mgr, CTempString path);
    virtual ~CVDBGraphDb_Impl(void);

    // check if there are graph track of intermediate zoom level
    bool HasMidZoomGraphs(void);

protected:
    friend class CVDBGraphSeqIterator;

    // SSeqTableCursor is helper accessor structure for SEQUENCE table
    struct SGraphTableCursor : public CObject {
        SGraphTableCursor(const CVDBTable& table);

        CVDBCursor m_Cursor;

        typedef int64_t TGraphV;
        typedef int64_t TGraphQ;

        DECLARE_VDB_COLUMN_AS_STRING(SID);
        DECLARE_VDB_COLUMN_AS(int64_t, START);
        DECLARE_VDB_COLUMN_AS(uint32_t, LEN);
        DECLARE_VDB_COLUMN_AS(TGraphQ, GR_Q0);
        DECLARE_VDB_COLUMN_AS(TGraphQ, GR_Q10);
        DECLARE_VDB_COLUMN_AS(TGraphQ, GR_Q50);
        DECLARE_VDB_COLUMN_AS(TGraphQ, GR_Q90);
        DECLARE_VDB_COLUMN_AS(TGraphQ, GR_Q100);
        DECLARE_VDB_COLUMN_AS(TGraphQ, GR_ZOOM_Q0);
        DECLARE_VDB_COLUMN_AS(TGraphQ, GR_ZOOM_Q10);
        DECLARE_VDB_COLUMN_AS(TGraphQ, GR_ZOOM_Q50);
        DECLARE_VDB_COLUMN_AS(TGraphQ, GR_ZOOM_Q90);
        DECLARE_VDB_COLUMN_AS(TGraphQ, GR_ZOOM_Q100);
        DECLARE_VDB_COLUMN_AS(TGraphV, GRAPH);
        DECLARE_VDB_COLUMN_AS(uint32_t, SCALE);
        DECLARE_VDB_COLUMN_AS(int64_t, NUM_SWITCHES);
    };

    const string& GetPath(void) const {
        return m_Path;
    }

    // SSeqInfo holds cached refseq information - ids, len, rows
    struct SSeqInfo {
        string m_SeqId;
        CSeq_id_Handle m_Seq_id_Handle;
        TSeqPos m_SeqLength;
        TSeqPos m_RowSize;
        uint64_t m_RowFirst, m_RowLast;
        uint64_t m_StartBase;
    };
    typedef list<SSeqInfo> TSeqInfoList;
    typedef map<string, TSeqInfoList::iterator, PNocase> TSeqInfoMapByName;
    typedef map<CSeq_id_Handle, TSeqInfoList::iterator> TSeqInfoMapBySeq_id;
    
    const TSeqInfoList& GetSeqInfoList(void) const {
        return m_SeqList;
    }
    const TSeqInfoMapByName& GetSeqInfoMapByName(void) const {
        return m_SeqMapByName;
    }
    const TSeqInfoMapBySeq_id& GetSeqInfoMapBySeq_id(void) const {
        return m_SeqMapBySeq_id;
    }

    // get table object
    const CVDBTable& GraphTable(void) {
        return m_GraphTable;
    }
    // get table accessor object for exclusive access
    CRef<SGraphTableCursor> Graph(void);
    // return table accessor object for reuse
    void Put(CRef<SGraphTableCursor>& curs) {
        m_Graph.Put(curs);
    }

private:
    CVDBMgr m_Mgr;
    string m_Path;

    CVDBTable m_GraphTable;
    CVDBObjectCache<SGraphTableCursor> m_Graph;

    TSeqInfoList m_SeqList; // list of cached refseqs' information
    TSeqInfoMapByName m_SeqMapByName; // index for refseq info lookup
    TSeqInfoMapBySeq_id m_SeqMapBySeq_id; // index for refseq info lookup
};


class CVDBGraphDb : public CRef<CVDBGraphDb_Impl>
{
public:
    CVDBGraphDb(void)
        {
        }
    explicit CVDBGraphDb(CVDBGraphDb_Impl* impl)
        : CRef<CVDBGraphDb_Impl>(impl)
        {
        }
    CVDBGraphDb(CVDBMgr& mgr, CTempString path)
        : CRef<CVDBGraphDb_Impl>(new CVDBGraphDb_Impl(mgr, path))
        {
        }
};


class NCBI_SRAREAD_EXPORT CVDBGraphSeqIterator
{
public:
    typedef CVDBGraphDb_Impl::SSeqInfo SSeqInfo;
    typedef CVDBGraphDb_Impl::SGraphTableCursor SGraphTableCursor;
    typedef CVDBGraphDb_Impl::TSeqInfoList::const_iterator TSeqInfoIter;

    CVDBGraphSeqIterator(void)
        {
        }
    CVDBGraphSeqIterator(const CVDBGraphDb& db, TSeqInfoIter pos)
        : m_Db(db),
          m_Iter(pos)
        {
        }
    explicit CVDBGraphSeqIterator(const CVDBGraphDb& db);
    CVDBGraphSeqIterator(const CVDBGraphDb& db,
                         const CSeq_id_Handle& seq_id);

    bool operator!(void) const {
        return !m_Db || m_Iter == m_Db->GetSeqInfoList().end();
    }
    operator const void*(void) const {
        return !*this? 0: this;
    }

    const SSeqInfo& GetInfo(void) const;
    const SSeqInfo& operator*(void) const {
        return GetInfo();
    }
    const SSeqInfo* operator->(void) const {
        return &GetInfo();
    }

    CVDBGraphSeqIterator& operator++(void) {
        ++m_Iter;
        return *this;
    }

    const string& GetSeqId(void) const {
        return GetInfo().m_SeqId;
    }
    const CSeq_id_Handle& GetSeq_id_Handle(void) const {
        return GetInfo().m_Seq_id_Handle;
    }

    TSeqPos GetSeqLength(void) const {
        return GetInfo().m_SeqLength;
    }

    // Do not mix graphs of different zoom levels
    enum EContentFlags {
        // original detailed graph
        fGraphMain = 1<<0,

        // overview graphs (percentiles)
        fGraphQ0   = 1<<1,
        fGraphQ10  = 1<<2,
        fGraphQ50  = 1<<3,
        fGraphQ90  = 1<<4,
        fGraphQ100 = 1<<5,
        fGraphQAll = (fGraphQ0 | fGraphQ10 | fGraphQ50 | 
                      fGraphQ90 | fGraphQ100),

        // main graph representation - either Seq-table or Seq-graph,
        // set both *As* flags to get more compact representation
        fGraphMainAsTable = 1<<8,
        fGraphMainAsGraph = 1<<9,
        fGraphMainAsBest = (fGraphMainAsTable | fGraphMainAsGraph),

        // zoom graphs if available (percentiles)
        fGraphZoomQ0   = 1<<11,
        fGraphZoomQ10  = 1<<12,
        fGraphZoomQ50  = 1<<13,
        fGraphZoomQ90  = 1<<14,
        fGraphZoomQ100 = 1<<15,
        fGraphZoomQAll = (fGraphZoomQ0 | fGraphZoomQ10 | fGraphZoomQ50 |
                          fGraphZoomQ90 | fGraphZoomQ100),

        // overview graphs by default
        fDefaultContent = fGraphQAll
    };
    typedef int TContentFlags;

    // Returns annot containing graphs over the specified range
    // (CRange<TSeqPos> or COpenRange<TSeqPos>).
    CRef<CSeq_annot> GetAnnot(COpenRange<TSeqPos> range,
                              const string& annot_name = kEmptyStr,
                              TContentFlags content = fDefaultContent) const;

    bool SeqTableIsSmaller(COpenRange<TSeqPos> range) const;

protected:
    CVDBGraphDb_Impl& GetDb(void) const {
        return m_Db.GetNCObject();
    }

    CRef<CSeq_graph> x_MakeGraph(const string& annot_name,
                                 CSeq_loc& loc,
                                 const SSeqInfo& info,
                                 const COpenRange<TSeqPos>& range,
                                 TSeqPos step,
                                 SGraphTableCursor& cursor,
                                 CVDBColumn& column,
                                 int level) const;
    CRef<CSeq_table> x_MakeTable(const string& annot_name,
                                 CSeq_loc& loc,
                                 const SSeqInfo& info,
                                 const COpenRange<TSeqPos>& range,
                                 SGraphTableCursor& cursor) const;

    bool x_SeqTableIsSmaller(COpenRange<TSeqPos> range,
                             SGraphTableCursor& cursor) const;

private:
    CVDBGraphDb m_Db;
    TSeqInfoIter m_Iter;
};


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // SRA__READER__SRA__WGSREAD__HPP
