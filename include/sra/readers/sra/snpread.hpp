#ifndef SRA__READER__SRA__SNPREAD__HPP
#define SRA__READER__SRA__SNPREAD__HPP
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

#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>
#include <util/range.hpp>
#include <util/rangemap.hpp>
#include <util/simple_buffer.hpp>
#include <sra/readers/sra/vdbread.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_data.hpp>
#include <map>
#include <list>

#include <ncbi/ncbi.h>
#include <insdc/insdc.h>

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(objects);

class CSeq_entry;
class CSeq_annot;
class CSeq_graph;
class CSeq_feat;
class CSeq_interval;
class CSeq_point;
class CUser_object;
class CUser_field;
class CBioseq;
class CSeq_literal;
class CSeq_annot_SNP_Info;

class CSNPDbTrackIterator;
class CSNPDbSeqIterator;
class CSNPDbPageIterator;
class CSNPDbGraphIterator;
class CSNPDbFeatIterator;

struct SSNPDb_Defs
{
    enum ESearchMode {
        eSearchByOverlap,
        eSearchByStart
    };

    typedef Uint8 TFilter;

    struct SFilter {
        SFilter()
            : m_Filter(0),
              m_FilterMask(0)
            {
            }
        SFilter(TFilter filter,
                TFilter filter_mask = TFilter(-1))
            : m_Filter(filter),
              m_FilterMask(filter_mask)
            {
            }

        void SetNoFilter(void)
            {
                m_Filter = 0;
                m_FilterMask = 0;
            }
        void SetFilter(TFilter filter,
                       TFilter filter_mask = TFilter(-1))
            {
                m_Filter = filter;
                m_FilterMask = filter_mask;
            }

        void Normalize(void)
            {
                m_Filter &= m_FilterMask;
            }

        bool IsSet(void) const
            {
                return m_FilterMask != 0;
            }
        bool Matches(TFilter bits) const
            {
                return (bits & m_FilterMask) == m_Filter;
            }

        TFilter m_Filter;
        TFilter m_FilterMask;
    };

    struct SSelector {
        SSelector(const SFilter& filter = SFilter())
            : m_SearchMode(eSearchByOverlap),
              m_Filter(filter)
            {
            }
        SSelector(ESearchMode search_mode,
                  const SFilter& filter = SFilter())
            : m_SearchMode(search_mode),
              m_Filter(filter)
            {
            }

        SSelector& SetNoFilter(void)
            {
                m_Filter = SFilter();
                return *this;
            }
        SSelector& SetFilter(const SFilter& filter)
            {
                m_Filter = filter;
                return *this;
            }
        SSelector& SetFilter(TFilter filter,
                             TFilter filter_mask = TFilter(-1))
            {
                m_Filter = SFilter(filter, filter_mask);
                return *this;
            }

        ESearchMode m_SearchMode;
        SFilter m_Filter;
    };
};


class NCBI_SRAREAD_EXPORT CSNPDb_Impl : public CObject, public SSNPDb_Defs
{
public:
    CSNPDb_Impl(CVDBMgr& mgr,
                CTempString path_or_acc);
    virtual ~CSNPDb_Impl(void);

    // SSeqInfo holds cached refseq information - ids, len, rows
    struct SSeqInfo {
        string m_SeqId;
        CBioseq::TId m_Seq_ids;
        CSeq_id_Handle m_Seq_id_Handle;
        TSeqPos m_SeqLength;
        TVDBRowId m_GraphRowId;

        CRef<CSeq_id>& GetMainSeq_id(void) {
            return m_Seq_ids.front();
        }
        const CRef<CSeq_id>& GetMainSeq_id(void) const {
            return m_Seq_ids.front();
        }
        const CSeq_id_Handle& GetMainSeq_id_Handle(void) const {
            return m_Seq_id_Handle;
        }
        struct SPageSet {
            TSeqPos m_SeqPos;
            TSeqPos m_PageCount;
            TVDBRowId m_RowId;

            TSeqPos GetSeqPosEnd(TSeqPos page_size) const {
                return m_SeqPos + m_PageCount*page_size;
            }
            TVDBRowId GetRowIdEnd(void) const {
                return m_RowId + m_PageCount;
            }
        };
        CRange<TVDBRowId> GetPageVDBRowRange() const {
            return COpenRange<TVDBRowId>(m_PageSets.front().m_RowId,
                                         m_PageSets.back().GetRowIdEnd());
        }
        
        typedef vector<SPageSet> TPageSets; // sorted by sequence position
        TPageSets m_PageSets;
        bool m_Circular;
        //vector<TSeqPos> m_AlnOverStarts; // relative to m_RowFirst
    };
    typedef vector<SSeqInfo> TSeqInfoList;
    typedef map<CSeq_id_Handle, size_t> TSeqInfoMapBySeq_id;

    // STrackInfo holds cached filter track information - name, bits
    struct STrackInfo {
        string m_Name;
        SFilter m_Filter;
    };
    typedef vector<STrackInfo> TTrackInfoList;
    typedef map<string, size_t> TTrackInfoMapByName;

    const string& GetDbPath(void) const {
        return m_DbPath;
    }
    
    const TSeqInfoList& GetSeqInfoList(void) const {
        return m_SeqList;
    }
    const TSeqInfoMapBySeq_id& GetSeqInfoMapBySeq_id(void) const {
        return m_SeqMapBySeq_id;
    }

    const TTrackInfoList& GetTrackInfoList(void) const {
        return m_TrackList;
    }
    const TTrackInfoMapByName& GetTrackInfoMapByName(void) const {
        return m_TrackMapByName;
    }

    TTrackInfoList::const_iterator FindTrack(const string& name) const;
    TSeqInfoList::const_iterator FindSeq(const CSeq_id_Handle& seq_id) const;

    TSeqPos GetPageSize(void) const;

protected:
    friend class CSNPDbTrackIterator;
    friend class CSNPDbSeqIterator;
    friend class CSNPDbPageIterator;
    friend class CSNPDbGraphIterator;
    friend class CSNPDbFeatIterator;

    // SSeqTableCursor is helper accessor structure for SEQUENCE table
    struct SSeqTableCursor;
    // STrackTableCursor is helper accessor structure for TRACK_FILTER table
    struct STrackTableCursor;
    // SPageTableCursor is helper accessor structure for PAGE table
    struct SPageTableCursor;
    // SGraphTableCursor is helper accessor structure for COVERAGE_GRAPH table
    struct SGraphTableCursor;
    // SFeatTableCursor is helper accessor structure for SNP feature table
    struct SFeatTableCursor;
    // SExtraTableCursor is helper accessor structure for extra info (alleles)
    struct SExtraTableCursor;

    // open tables
    void OpenTable(CVDBTable& table,
                   const char* table_name,
                   volatile bool& table_is_opened);

    const CVDBTable& GraphTable(void) {
        return m_GraphTable;
    }

    const CVDBTable& PageTable(void) {
        return m_PageTable;
    }

    const CVDBTable& FeatTable(void) {
        return m_FeatTable;
    }

    const CVDBTable& ExtraTable(void) {
        return m_ExtraTable;
    }

    // get table accessor object for exclusive access
    CRef<SGraphTableCursor> Graph(TVDBRowId row = 0);
    // return table accessor object for reuse
    void Put(CRef<SGraphTableCursor>& curs, TVDBRowId row = 0);

    // get table accessor object for exclusive access
    CRef<SPageTableCursor> Page(TVDBRowId row = 0);
    // return table accessor object for reuse
    void Put(CRef<SPageTableCursor>& curs, TVDBRowId row = 0);

    // get table accessor object for exclusive access
    CRef<SFeatTableCursor> Feat(TVDBRowId row = 0);
    // return table accessor object for reuse
    void Put(CRef<SFeatTableCursor>& curs, TVDBRowId row = 0);

    // get extra table accessor object for exclusive access
    CRef<SExtraTableCursor> Extra(TVDBRowId row = 0);
    // return table accessor object for reuse
    void Put(CRef<SExtraTableCursor>& curs, TVDBRowId row = 0);

    size_t x_GetSeqVDBIndex(TSeqInfoList::const_iterator seq) const {
        return seq - GetSeqInfoList().begin();
    }
    size_t x_GetTrackVDBIndex(TTrackInfoList::const_iterator track) const {
        return track - GetTrackInfoList().begin();
    }
    TVDBRowId x_GetSeqVDBRowId(TSeqInfoList::const_iterator seq) const {
        return TVDBRowId(x_GetSeqVDBIndex(seq) + 1);
    }
    TVDBRowId x_GetTrackVDBRowId(TTrackInfoList::const_iterator track) const {
        return TVDBRowId(x_GetTrackVDBIndex(track) + 1);
    }

    CRange<TVDBRowId>
    x_GetPageVDBRowRange(TSeqInfoList::const_iterator seq);
    TVDBRowId x_GetGraphVDBRowId(TSeqInfoList::const_iterator seq,
                                 TTrackInfoList::const_iterator track);
private:
    CVDBMgr m_Mgr;
    string m_DbPath;
    CVDB m_Db;
    CVDBTable m_GraphTable;
    CVDBTable m_PageTable;
    CVDBTable m_FeatTable;
    CVDBTable m_ExtraTable;

    CVDBObjectCache<SGraphTableCursor> m_Graph;
    CVDBObjectCache<SPageTableCursor> m_Page;
    CVDBObjectCache<SFeatTableCursor> m_Feat;
    CVDBObjectCache<SExtraTableCursor> m_Extra;

    TSeqInfoList m_SeqList; // list of cached refseqs' information
    TSeqInfoMapBySeq_id m_SeqMapBySeq_id; // index for refseq info lookup

    TTrackInfoList m_TrackList; // list of cached filter track information
    TTrackInfoMapByName m_TrackMapByName; // index for track lookup
};


class CSNPDb : public CRef<CSNPDb_Impl>, public SSNPDb_Defs
{
public:
    CSNPDb(void)
        {
        }
    explicit CSNPDb(CSNPDb_Impl* impl)
        : CRef<CSNPDb_Impl>(impl)
        {
        }
    CSNPDb(CVDBMgr& mgr,
           CTempString path_or_acc)
        : CRef<CSNPDb_Impl>(new CSNPDb_Impl(mgr, path_or_acc))
        {
        }
};


template<class Enum>
class CSafeFlags {
public:
    typedef Enum enum_type;
    typedef typename underlying_type<enum_type>::type storage_type;

    CSafeFlags()
        : m_Flags(0)
        {
        }
    CSafeFlags(enum_type flags)
        : m_Flags(flags)
        {
        }

    storage_type get() const
        {
            return m_Flags;
        }

    DECLARE_OPERATOR_BOOL(m_Flags != 0);

    bool operator==(const CSafeFlags& b) const
        {
            return get() == b.get();
        }
    bool operator!=(const CSafeFlags& b) const
        {
            return get() != b.get();
        }

    CSafeFlags operator&(const CSafeFlags& b) const
        {
            return get() & b.get();
        }
    CSafeFlags operator|(const CSafeFlags& b) const
        {
            return get() | b.get();
        }
    CSafeFlags operator^(const CSafeFlags& b) const
        {
            return get() ^ b.get();
        }
    CSafeFlags& operator&=(const CSafeFlags& b)
        {
            m_Flags &= b.get();
            return *this;
        }
    CSafeFlags& operator|(const CSafeFlags& b)
        {
            m_Flags |= b.get();
            return *this;
        }
    CSafeFlags& operator^(const CSafeFlags& b)
        {
            m_Flags ^= b.get();
            return *this;
        }
    CSafeFlags without(const CSafeFlags& b) const
        {
            return CSafeFlags(get()&~b.get());
        }
    CSafeFlags& reset(const CSafeFlags& b)
        {
            m_Flags &= ~b.get();
            return *this;
        }

    CSafeFlags operator~() const
        {
            return CSafeFlags(~get());
        }

private:
    CSafeFlags(storage_type flags)
        : m_Flags(flags)
        {
        }

    storage_type m_Flags;
};
template<class Enum> inline CSafeFlags<Enum> ToFlags(Enum v)
{
    return CSafeFlags<Enum>(v);
}


// iterate filtered tracks in VDB object
class NCBI_SRAREAD_EXPORT CSNPDbTrackIterator : public SSNPDb_Defs
{
public:
    typedef CSNPDb_Impl::TTrackInfoList TList;
    typedef TList::value_type TInfo;

    CSNPDbTrackIterator(void)
        {
        }
    explicit CSNPDbTrackIterator(const CSNPDb& db);
    CSNPDbTrackIterator(const CSNPDb& db, size_t track_index);
    CSNPDbTrackIterator(const CSNPDb& db, const string& name);

    void Reset(void);

    DECLARE_OPERATOR_BOOL(m_Db && m_Iter != GetList().end());

    const TInfo& operator*(void) const {
        return GetInfo();
    }
    const TInfo* operator->(void) const {
        return &GetInfo();
    }

    TVDBRowId GetVDBRowId(void) const {
        return GetDb().x_GetTrackVDBRowId(m_Iter);
    }
    size_t GetVDBTrackIndex(void) const {
        return GetDb().x_GetTrackVDBIndex(m_Iter);
    }

    CSNPDbTrackIterator& operator++(void) {
        ++m_Iter;
        return *this;
    }

    const string& GetName(void) const {
        return GetInfo().m_Name;
    }
    const SFilter& GetFilter(void) const {
        return GetInfo().m_Filter;
    }
    TFilter GetFilterBits(void) const {
        return GetFilter().m_Filter;
    }
    TFilter GetFilterMask(void) const {
        return GetFilter().m_FilterMask;
    }

protected:
    friend class CSNPDbSeqIterator;

    CSNPDb_Impl& GetDb(void) const {
        return m_Db.GetNCObject();
    }
    const TList& GetList() const {
        return GetDb().GetTrackInfoList();
    }
    const TInfo& GetInfo() const;

    friend class CSNPDbPageIterator;
    friend class CSNPDbGraphIterator;

private:
    CSNPDb m_Db;
    TList::const_iterator m_Iter;
};


// iterate sequences in VDB object
class NCBI_SRAREAD_EXPORT CSNPDbSeqIterator : public SSNPDb_Defs
{
public:
    typedef CSNPDb_Impl::TSeqInfoList TList;
    typedef TList::value_type TInfo;

    CSNPDbSeqIterator(void)
        {
        }
    explicit CSNPDbSeqIterator(const CSNPDb& db);
    CSNPDbSeqIterator(const CSNPDb& db, size_t seq_index);
    CSNPDbSeqIterator(const CSNPDb& db, const CSeq_id_Handle& seq_id);

    void Reset(void);

    DECLARE_OPERATOR_BOOL(m_Db && m_Iter != GetList().end());

    const TInfo& operator*(void) const {
        return GetInfo();
    }
    const TInfo* operator->(void) const {
        return &GetInfo();
    }

    CSNPDbSeqIterator& operator++(void) {
        ++m_Iter;
        return *this;
    }

    const string& GetAccession(void) const {
        return m_Iter->m_SeqId;
    }
    CRef<CSeq_id> GetSeqId(void) const {
        return m_Iter->GetMainSeq_id();
    }
    const CSeq_id_Handle& GetSeqIdHandle(void) const {
        return m_Iter->GetMainSeq_id_Handle();
    }
    const CBioseq::TId& GetSeqIds(void) const {
        return m_Iter->m_Seq_ids;
    }

    TSeqPos GetSeqLength(void) const {
        return m_Iter->m_SeqLength;
    }

    bool IsCircular(void) const;

    TSeqPos GetPageSize(void) const {
        return GetDb().GetPageSize();
    }
    TSeqPos GetMaxSNPLength(void) const;

    // return count of SNP features that intersect with the range
    Uint8 GetSNPCount(CRange<TSeqPos> range) const;
    // return count of all SNP features
    Uint8 GetSNPCount(void) const;
    CRange<TSeqPos> GetSNPRange(void) const;

    TVDBRowId GetVDBRowId(void) const {
        return GetDb().x_GetSeqVDBRowId(m_Iter);
    }
    size_t GetVDBSeqIndex(void) const {
        return GetDb().x_GetSeqVDBIndex(m_Iter);
    }
    CRange<TVDBRowId> GetPageVDBRowRange(void) const {
        return GetDb().x_GetPageVDBRowRange(m_Iter);
    }
    TVDBRowId GetGraphVDBRowId() const {
        return GetDb().x_GetGraphVDBRowId(m_Iter, m_TrackIter);
    }

    enum EFlags {
        fZoomPage       = 1<<0,
        fNoGaps         = 1<<1,
        fDefaultFlags   = 0
    };
    typedef CSafeFlags<EFlags> TFlags;

    CRef<CSeq_graph> GetOverviewGraph(CRange<TSeqPos> range,
                                      TFlags flags = fDefaultFlags) const;

    CRef<CSeq_annot> GetOverviewAnnot(CRange<TSeqPos> range,
                                      const string& annot_name,
                                      TFlags flags = fDefaultFlags) const;
    CRef<CSeq_annot> GetOverviewAnnot(CRange<TSeqPos> range,
                                      TFlags flags = fDefaultFlags) const;

    CRef<CSeq_graph> GetCoverageGraph(CRange<TSeqPos> range) const;

    CRef<CSeq_annot> GetCoverageAnnot(CRange<TSeqPos> range,
                                      const string& annot_name,
                                      TFlags flags = fDefaultFlags) const;
    CRef<CSeq_annot> GetCoverageAnnot(CRange<TSeqPos> range,
                                      TFlags flags = fDefaultFlags) const;

    CRef<CSeq_annot> GetFeatAnnot(CRange<TSeqPos> range,
                                  const SFilter& filter,
                                  TFlags flags = fDefaultFlags) const;
    CRef<CSeq_annot> GetFeatAnnot(CRange<TSeqPos> range,
                                  TFlags flags = fDefaultFlags) const;
    typedef vector< CRef<CSeq_annot> > TAnnotSet;
    TAnnotSet GetTableFeatAnnots(CRange<TSeqPos> range,
                                 const string& annot_name,
                                 const SFilter& filter,
                                 TFlags flags = fDefaultFlags) const;
    TAnnotSet GetTableFeatAnnots(CRange<TSeqPos> range,
                                 const SFilter& filter,
                                 TFlags flags = fDefaultFlags) const;
    TAnnotSet GetTableFeatAnnots(CRange<TSeqPos> range,
                                 TFlags flags = fDefaultFlags) const;
    TAnnotSet GetTableFeatAnnots(CRange<TSeqPos> range,
                                 const string& annot_name,
                                 TFlags flags = fDefaultFlags) const;
    typedef pair< CRef<CSeq_annot>, CRef<CSeq_annot_SNP_Info> > TPackedAnnot;
    TPackedAnnot GetPackedFeatAnnot(CRange<TSeqPos> range,
                                    const SFilter& filter,
                                    TFlags flags = fDefaultFlags) const;
    TPackedAnnot GetPackedFeatAnnot(CRange<TSeqPos> range,
                                    TFlags flags = fDefaultFlags) const;

    CSNPDb_Impl& GetDb(void) const {
        return m_Db.GetNCObject();
    }

    const SFilter& GetFilter() const {
        return x_GetTrackIter()->m_Filter;
    }

    void SetTrack(const CSNPDbTrackIterator& track);

protected:
    friend class CSNPDbPageIterator;
    friend class CSNPDbGraphIterator;

    const TList& GetList() const {
        return GetDb().GetSeqInfoList();
    }
    const TInfo& GetInfo() const;

    TList::const_iterator x_GetSeqIter() const {
        return m_Iter;
    }
    CSNPDb_Impl::TTrackInfoList::const_iterator x_GetTrackIter() const {
        return m_TrackIter;
    }

private:
    CSNPDb m_Db;
    TList::const_iterator m_Iter;
    CSNPDb_Impl::TTrackInfoList::const_iterator m_TrackIter;
};


// iterate sequence pages of predefined fixed size
class NCBI_SRAREAD_EXPORT CSNPDbPageIterator : public SSNPDb_Defs
{
public:
    CSNPDbPageIterator(void);

    CSNPDbPageIterator(const CSNPDb& db,
                       const CSeq_id_Handle& ref_id,
                       TSeqPos ref_pos = 0,
                       TSeqPos window = 0,
                       ESearchMode search_mode = eSearchByOverlap);
    CSNPDbPageIterator(const CSNPDb& db,
                       const CSeq_id_Handle& ref_id,
                       COpenRange<TSeqPos> ref_range,
                       ESearchMode search_mode = eSearchByOverlap);
    CSNPDbPageIterator(const CSNPDbSeqIterator& seq,
                       COpenRange<TSeqPos> ref_range,
                       ESearchMode search_mode = eSearchByOverlap);
    CSNPDbPageIterator(const CSNPDbPageIterator& iter);
    ~CSNPDbPageIterator(void);

    CSNPDbPageIterator& operator=(const CSNPDbPageIterator& iter);

    CSNPDbPageIterator& Select(COpenRange<TSeqPos> ref_range,
                               ESearchMode search_mode = eSearchByOverlap);

    void Reset(void);

    DECLARE_OPERATOR_BOOL(m_CurrPagePos < m_SearchRange.GetToOpen());

    CSNPDbPageIterator& operator++(void) {
        x_Next();
        return *this;
    }

    const CRange<TSeqPos>& GetSearchRange(void) const {
        return m_SearchRange;
    }
    ESearchMode GetSearchMode(void) const {
        return m_SearchMode;
    }

    const string& GetAccession(void) const {
        return GetRefIter().GetAccession();
    }
    CRef<CSeq_id> GetSeqId(void) const {
        return GetRefIter().GetSeqId();
    }
    CSeq_id_Handle GetSeqIdHandle(void) const {
        return GetRefIter().GetSeqIdHandle();
    }

    TSeqPos GetPageSize(void) const {
        return GetDb().GetPageSize();
    }
    TSeqPos GetPagePos(void) const {
        return m_CurrPagePos;
    }
    CRange<TSeqPos> GetPageRange(void) const {
        TSeqPos pos = GetPagePos();
        return COpenRange<TSeqPos>(pos, pos+GetPageSize());
    }

    TVDBRowId GetPageRowId(void) const {
        return m_CurrPageRowId;
    }

    const CSNPDbSeqIterator& GetRefIter(void) const {
        return m_SeqIter;
    }

    TVDBRowId GetFirstFeatRowId(void) const;
    TVDBRowCount GetFeatCount(void) const;

    void SetTrack(const CSNPDbTrackIterator& track) {
        m_SeqIter.SetTrack(track);
    }

protected:
    friend class CSNPDbFeatIterator;

    CSNPDb_Impl& GetDb(void) const {
        return GetRefIter().GetDb();
    }
    const CSNPDb_Impl::SPageTableCursor& Cur(void) const {
        return *m_Cur;
    }

    CSNPDb_Impl::TSeqInfoList::const_iterator x_GetSeqIter() const {
        return GetRefIter().x_GetSeqIter();
    }
    CSNPDb_Impl::TTrackInfoList::const_iterator x_GetTrackIter() const {
        return GetRefIter().x_GetTrackIter();
    }
    
    TVDBRowId x_GetSeqVDBRowId() const {
        return GetDb().x_GetSeqVDBRowId(x_GetSeqIter());
    }
    TVDBRowId x_GetTrackVDBRowId() const {
        return GetDb().x_GetTrackVDBRowId(x_GetTrackIter());
    }

    CRange<TVDBRowId> x_GetPageVDBRowRange() const {
        return GetRefIter().GetPageVDBRowRange();
    }
    TVDBRowId x_GetGraphVDBRowId() const {
        return GetRefIter().GetGraphVDBRowId();
    }

    void x_ReportInvalid(const char* method) const;
    void x_CheckValid(const char* method) const {
        if ( !*this ) {
            x_ReportInvalid(method);
        }
    }

    void x_Next(void);
    
private:
    CSNPDbSeqIterator m_SeqIter; // refseq selector

    CRef<CSNPDb_Impl::SPageTableCursor> m_Cur; // page table accessor
    mutable CRef<CSNPDb_Impl::SGraphTableCursor> m_GraphCur; // graph table accessor
    mutable TVDBRowId m_LastGraphRowId;
    
    CRange<TSeqPos> m_SearchRange; // requested refseq range
    
    size_t m_CurrPageSet;
    TVDBRowId m_CurrPageRowId;
    TSeqPos m_CurrPagePos;
    
    ESearchMode m_SearchMode;
};


// iterate non-zero pages of predefined fixed size for a sequence/track pair
class NCBI_SRAREAD_EXPORT CSNPDbGraphIterator : public SSNPDb_Defs
{
public:
    CSNPDbGraphIterator(void);

    explicit
    CSNPDbGraphIterator(const CSNPDbSeqIterator& iter,
                        COpenRange<TSeqPos> ref_range);
    CSNPDbGraphIterator(const CSNPDbGraphIterator& iter);
    ~CSNPDbGraphIterator(void);

    CSNPDbGraphIterator& operator=(const CSNPDbGraphIterator& iter);

    CSNPDbGraphIterator& Select(const CSNPDbSeqIterator& iter,
                                COpenRange<TSeqPos> ref_range);

    void Reset(void);

    DECLARE_OPERATOR_BOOL(m_CurrPagePos != kInvalidSeqPos);

    CSNPDbGraphIterator& operator++(void) {
        x_Next();
        return *this;
    }

    const CRange<TSeqPos>& GetSearchRange(void) const {
        return m_SearchRange;
    }

    TSeqPos GetPageSize(void) const {
        return GetDb().GetPageSize();
    }
    TSeqPos GetPagePos(void) const {
        return m_CurrPagePos;
    }
    CRange<TSeqPos> GetPageRange(void) const {
        TSeqPos pos = GetPagePos();
        return COpenRange<TSeqPos>(pos, pos+GetPageSize());
    }

    TVDBRowId GetGraphRowId(void) const {
        return m_CurrPageRowId;
    }

    Uint4 GetTotalValue(void) const;
    CVDBValueFor<Uint4> GetCoverageValues(void) const;

protected:
    CSNPDb_Impl& GetDb(void) const {
        return m_Db.GetNCObject();
    }
    const CSNPDb_Impl::SGraphTableCursor& Cur(void) const {
        return *m_Cur;
    }

    void x_ReportInvalid(const char* method) const;
    void x_CheckValid(const char* method) const {
        if ( !*this ) {
            x_ReportInvalid(method);
        }
    }

    void x_Next(void);
    
private:
    CSNPDb m_Db;
    CRef<CSNPDb_Impl::SGraphTableCursor> m_Cur; // graph table accessor

    TVDBRowId m_SeqRowId;
    TVDBRowId m_TrackRowId;
    CRange<TSeqPos> m_SearchRange; // requested refseq range

    TVDBRowId m_CurrPageRowId;
    TSeqPos m_CurrPagePos;
};


// iterate SNP features
class NCBI_SRAREAD_EXPORT CSNPDbFeatIterator : public SSNPDb_Defs
{
public:
    CSNPDbFeatIterator(void);
    CSNPDbFeatIterator(const CSNPDb& db,
                       const CSeq_id_Handle& ref_id,
                       TSeqPos ref_pos = 0,
                       TSeqPos window = 0,
                       const SSelector& sel = SSelector());
    CSNPDbFeatIterator(const CSNPDb& db,
                       const CSeq_id_Handle& ref_id,
                       COpenRange<TSeqPos> ref_range,
                       const SSelector& sel = SSelector());
    CSNPDbFeatIterator(const CSNPDbSeqIterator& seq,
                       COpenRange<TSeqPos> ref_range,
                       const SSelector& sel = SSelector());
    CSNPDbFeatIterator(const CSNPDbFeatIterator& iter);
    ~CSNPDbFeatIterator(void);

    CSNPDbFeatIterator& operator=(const CSNPDbFeatIterator& iter);

    CSNPDbFeatIterator& Select(COpenRange<TSeqPos> ref_range,
                               const SSelector& sel = SSelector());

    void Reset(void);

    DECLARE_OPERATOR_BOOL(m_CurrFeatId < m_FirstBadFeatId);

    CSNPDbFeatIterator& operator++(void) {
        x_Next();
        return *this;
    }

    CTempString GetFeatType(void) const;

    typedef pair<TVDBRowId, size_t> TExtraRange;
    TExtraRange GetExtraRange(void) const;
    CTempString GetAllele(const TExtraRange& range, size_t index) const;

    const CSNPDbPageIterator& GetPageIter(void) const {
        return m_PageIter;
    }
    const CSNPDbSeqIterator& GetRefIter(void) const {
        return GetPageIter().GetRefIter();
    }

    const string& GetAccession(void) const {
        return GetPageIter().GetAccession();
    }
    CRef<CSeq_id> GetSeqId(void) const {
        return GetPageIter().GetSeqId();
    }
    CSeq_id_Handle GetSeqIdHandle(void) const {
        return GetPageIter().GetSeqIdHandle();
    }
    const CRange<TSeqPos>& GetSearchRange(void) const {
        return GetPageIter().GetSearchRange();
    }
    ESearchMode GetSearchMode(void) const {
        return GetPageIter().GetSearchMode();
    }
 
    TSeqPos GetSNPPosition(void) const {
        return m_CurRange.GetFrom();
    }
    TSeqPos GetSNPLength(void) const {
        return m_CurRange.GetLength();
    }

    enum EFeatIdPrefix {
        eFeatIdPrefix_none = 0,
        eFeatIdPrefix_rs = 1,
        eFeatIdPrefix_ss = 2
    };
    Uint4 GetFeatIdPrefix(void) const;
    Uint8 GetFeatId(void) const;

    Uint8 GetQualityCodes(void) const;
    void GetQualityCodes(vector<char>& codes) const;

    enum EFlags {
        fIncludeAlleles      = 1<<0,
        fIncludeRsId         = 1<<1,
        fIncludeQualityCodes = 1<<2,
        fIncludeNeighbors    = 1<<3,
        fUseSharedObjects    = 1<<8,
        fDefaultFlags = ( fIncludeAlleles |
                          fIncludeRsId |
                          fIncludeQualityCodes |
                          fIncludeNeighbors |
                          fUseSharedObjects )
    };
    typedef CSafeFlags<EFlags> TFlags;
    
    CRef<CSeq_feat> GetSeq_feat(TFlags flags = fDefaultFlags) const;

protected:
    CSNPDb_Impl& GetDb(void) const {
        return GetPageIter().GetDb();
    }
    const CSNPDb_Impl::SFeatTableCursor& Cur(void) const {
        return *m_Feat;
    }

    TVDBRowId GetPageRowId(void) const {
        return GetPageIter().GetPageRowId();
    }

    TSeqPos x_GetFrom(void) const;
    TSeqPos x_GetLength(void) const;
    
    void x_Init(const CSNPDb& snp_db);

    void x_ReportInvalid(const char* method) const;
    void x_CheckValid(const char* method) const {
        if ( !*this ) {
            x_ReportInvalid(method);
        }
    }

    void x_Next(void);
    void x_Settle(void);
    enum EExcluded {
        eIncluded,
        eExluded,
        ePassedTheRegion
    };
    EExcluded x_Excluded(void);

    void x_SetFilter(const SSelector& sel);
    void x_InitPage(void);

    TVDBRowId x_GetGraphVDBRowId() const;

    CSNPDb_Impl::TSeqInfoList::const_iterator x_GetSeqIter() const {
        return GetPageIter().x_GetSeqIter();
    }

private:
    CSNPDbPageIterator m_PageIter;
    mutable CRef<CSNPDb_Impl::SFeatTableCursor> m_Feat;
    mutable CRef<CSNPDb_Impl::SExtraTableCursor> m_Extra; // for alleles
    mutable TVDBRowId m_ExtraRowId;
    mutable CRef<CSNPDb_Impl::SGraphTableCursor> m_Graph; // for page filtering
    mutable TVDBRowId m_GraphBaseRowId;

    COpenRange<TSeqPos> m_CurRange; // current SNP refseq range
    SFilter m_Filter;

    TVDBRowId m_CurrFeatId, m_FirstBadFeatId;

    typedef CRef<CObject_id> TObjectIdCache;
    typedef map<CTempString, CRef<CUser_field> > TUserFieldCache;

    struct SCreateCache;
    mutable AutoPtr<SCreateCache> m_CreateCache;
    SCreateCache& x_GetCreateCache(void) const;
};


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // SRA__READER__SRA__SNPREAD__HPP
