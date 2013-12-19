#ifndef SRA__READER__SRA__CSRAREAD__HPP
#define SRA__READER__SRA__CSRAREAD__HPP
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
 *   Access to cSRA files
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <util/range.hpp>
#include <sra/readers/sra/vdbread.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <map>
#include <list>

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(objects);

class CCSraRefSeqIterator;
class CCSraAlignIterator;
class CCSraShortReadIterator;
class CSeq_entry;
class CSeq_annot;
class CSeq_align;
class CSeq_graph;
class CBioseq;
class CUser_object;
class CUser_field;
class IIdMapper;

class NCBI_SRAREAD_EXPORT CCSraDb_Impl : public CObject
{
public:
    CCSraDb_Impl(void) : m_RowSize(0) {}
    CCSraDb_Impl(CVDBMgr& mgr, const string& csra_path,
                 IIdMapper* ref_id_mapper,
                 int ref_id_type);

    // SRefInfo holds cached refseq information - ids, len, rows
    struct SRefInfo {
        string m_Name, m_SeqId;
        mutable volatile TSeqPos m_SeqLength; // actual len will be updated
        CBioseq::TId m_Seq_ids;
        CSeq_id_Handle m_Seq_id_Handle;
        CRef<CSeq_id>& GetMainSeq_id(void) {
            return m_Seq_ids.front();
        }
        const CRef<CSeq_id>& GetMainSeq_id(void) const {
            return m_Seq_ids.front();
        }
        const CSeq_id_Handle& GetMainSeq_id_Handle(void) const {
            return m_Seq_id_Handle;
        }
        uint64_t m_RowFirst, m_RowLast;
        bool m_Circular;
    };
    typedef list<SRefInfo> TRefInfoList;
    typedef map<string, TRefInfoList::iterator, PNocase> TRefInfoMapByName;
    typedef map<CSeq_id_Handle, TRefInfoList::iterator> TRefInfoMapBySeq_id;
    
    const TRefInfoList& GetRefInfoList(void) const {
        return m_RefList;
    }
    const TRefInfoMapByName& GetRefInfoMapByName(void) const {
        return m_RefMapByName;
    }
    const TRefInfoMapBySeq_id& GetRefInfoMapBySeq_id(void) const {
        return m_RefMapBySeq_id;
    }

    TSeqPos GetRowSize(void) const {
        return m_RowSize;
    }

    typedef vector<string> TSpotGroups;
    void GetSpotGroups(TSpotGroups& spot_groups);

    const string& GetCSraPath(void) const {
        return m_CSraPath;
    }

    const string& GetSraIdPart(void) const {
        return m_SraIdPart;
    }
    void SetSraIdPart(const string& s) {
        m_SraIdPart = s;
    }

    CRef<CSeq_id> MakeShortReadId(uint64_t id1, uint32_t id2) const;
    void SetShortReadId(string& str, uint64_t id1, uint32_t id2) const;

protected:
    friend class CCSraRefSeqIterator;
    friend class CCSraAlignIterator;
    friend class CCSraShortReadIterator;

    void x_CalcSeqLength(const SRefInfo& info);

    // SRefTableCursor is helper accessor structure for refseq table
    struct SRefTableCursor : public CObject {
        SRefTableCursor(const CVDB& db);

        CVDBTable m_Table;
        CVDBCursor m_Cursor;

        DECLARE_VDB_COLUMN_AS(Uint1, CGRAPH_HIGH);
        DECLARE_VDB_COLUMN_AS(uint64_t, PRIMARY_ALIGNMENT_IDS);
        DECLARE_VDB_COLUMN_AS(uint64_t, SECONDARY_ALIGNMENT_IDS);
        DECLARE_VDB_COLUMN_AS_STRING(NAME);
        typedef pair<uint64_t, uint64_t> row_range_t;
        DECLARE_VDB_COLUMN_AS(row_range_t, NAME_RANGE);
        DECLARE_VDB_COLUMN_AS_STRING(SEQ_ID);
        DECLARE_VDB_COLUMN_AS(INSDC_coord_len, SEQ_LEN);
        DECLARE_VDB_COLUMN_AS(INSDC_coord_len, MAX_SEQ_LEN);
        DECLARE_VDB_COLUMN_AS_STRING(READ);
        DECLARE_VDB_COLUMN_AS(bool, CIRCULAR);
    };

    // SAlnTableCursor is helper accessor structure for align table
    struct SAlnTableCursor : public CObject {
        SAlnTableCursor(const CVDB& db, bool is_secondary);

        CVDBTable m_Table;
        CVDBCursor m_Cursor;
        bool m_IsSecondary;
        
        DECLARE_VDB_COLUMN_AS_STRING(REF_NAME);
        DECLARE_VDB_COLUMN_AS_STRING(REF_SEQ_ID);
        DECLARE_VDB_COLUMN_AS(INSDC_coord_zero, REF_POS);
        DECLARE_VDB_COLUMN_AS(INSDC_coord_len, REF_LEN);
        DECLARE_VDB_COLUMN_AS(bool, REF_ORIENTATION);
        DECLARE_VDB_COLUMN_AS(char, HAS_REF_OFFSET);
        DECLARE_VDB_COLUMN_AS(char, HAS_MISMATCH);
        DECLARE_VDB_COLUMN_AS(int32_t, REF_OFFSET);
        DECLARE_VDB_COLUMN_AS_STRING(CIGAR_SHORT);
        DECLARE_VDB_COLUMN_AS_STRING(CIGAR_LONG);
        DECLARE_VDB_COLUMN_AS_STRING(RAW_READ);
        DECLARE_VDB_COLUMN_AS_STRING(MISMATCH_READ);
        DECLARE_VDB_COLUMN_AS_STRING(MISMATCH);
        DECLARE_VDB_COLUMN_AS(INSDC_coord_len, SPOT_LEN);
        DECLARE_VDB_COLUMN_AS(uint64_t, SEQ_SPOT_ID);
        DECLARE_VDB_COLUMN_AS(INSDC_coord_one, SEQ_READ_ID);
        DECLARE_VDB_COLUMN_AS(int32_t, MAPQ);
        DECLARE_VDB_COLUMN_AS(uint64_t, MATE_ALIGN_ID);
        DECLARE_VDB_COLUMN_AS(INSDC_quality_phred, QUALITY);
        DECLARE_VDB_COLUMN_AS_STRING(SPOT_GROUP);
        DECLARE_VDB_COLUMN_AS_STRING(NAME);
    };

    // SSeqTableCursor is helper accessor structure for sequence table
    struct SSeqTableCursor : public CObject {
        SSeqTableCursor(const CVDB& db);
        SSeqTableCursor(const CCSraDb_Impl& db);
        SSeqTableCursor(const CVDBTable& table);

        CVDBTable m_Table;
        CVDBCursor m_Cursor;
        
        DECLARE_VDB_COLUMN_AS_STRING(SPOT_GROUP);
        DECLARE_VDB_COLUMN_AS(INSDC_read_type, READ_TYPE);
        DECLARE_VDB_COLUMN_AS(INSDC_coord_len, READ_LEN);
        DECLARE_VDB_COLUMN_AS(INSDC_coord_zero, READ_START);
        DECLARE_VDB_COLUMN_AS_STRING(READ);
        DECLARE_VDB_COLUMN_AS(INSDC_quality_phred, QUALITY);
        DECLARE_VDB_COLUMN_AS(uint64_t, PRIMARY_ALIGNMENT_ID);
        DECLARE_VDB_COLUMN_AS(INSDC_coord_len, TRIM_LEN);
        DECLARE_VDB_COLUMN_AS(INSDC_coord_zero, TRIM_START);
        DECLARE_VDB_COLUMN_AS_STRING(NAME);
    };
    friend struct SSeqTableCursor;

    // get table accessor object for exclusive access
    CRef<SRefTableCursor> Ref(void);
    CRef<SAlnTableCursor> Aln(bool is_secondary);
    CRef<SSeqTableCursor> Seq(void);
    // return table accessor object for reuse
    void Put(CRef<SRefTableCursor>& curs) {
        m_Ref.Put(curs);
    }
    void Put(CRef<SAlnTableCursor>& curs) {
        if ( curs ) {
            m_Aln[curs->m_IsSecondary].Put(curs);
        }
    }
    void Put(CRef<SSeqTableCursor>& curs) {
        m_Seq.Put(curs);
    }

protected:
    void x_MakeRefSeq_ids(SRefInfo& info,
                          IIdMapper* ref_id_mapper,
                          int ref_id_type);

private:
    CVDBMgr m_Mgr;
    CVDB m_Db;
    string m_CSraPath;
    string m_SraIdPart;

    CVDBObjectCache<SRefTableCursor> m_Ref;
    CVDBObjectCache<SAlnTableCursor> m_Aln[2];
    CVDBTable m_SeqTable;
    CVDBObjectCache<SSeqTableCursor> m_Seq;

    TSeqPos m_RowSize; // cached size of refseq row in bases
    TRefInfoList m_RefList; // list of cached refseqs' information
    TRefInfoMapByName m_RefMapByName; // index for refseq info lookup
    TRefInfoMapBySeq_id m_RefMapBySeq_id; // index for refseq info lookup
};


class CCSraDb : public CRef<CCSraDb_Impl>
{
public:
    enum ERefIdType {
        eRefId_SEQ_ID,
        eRefId_gnl_NAME
    };

    CCSraDb(void)
        {
        }
    CCSraDb(CVDBMgr& mgr, const string& csra_path,
            IIdMapper* ref_id_mapper = 0,
            ERefIdType ref_id_type = eRefId_SEQ_ID)
        : CRef<CCSraDb_Impl>(new CCSraDb_Impl(mgr, csra_path,
                                              ref_id_mapper,
                                              ref_id_type))
        {
        }
    
    TSeqPos GetRowSize(void) const
        {
            return GetObject().GetRowSize();
        }
    
    typedef CCSraDb_Impl::TSpotGroups TSpotGroups;
    void GetSpotGroups(TSpotGroups& spot_groups)
        {
            GetObject().GetSpotGroups(spot_groups);
        }
};


class NCBI_SRAREAD_EXPORT CCSraRefSeqIterator
{
public:
    CCSraRefSeqIterator(void)
        {
        }
    CCSraRefSeqIterator(const CCSraDb& csra_db,
                        CCSraDb_Impl::TRefInfoList::const_iterator iter)
        : m_Db(csra_db),
          m_Iter(iter)
        {
        }
    CCSraRefSeqIterator(const CCSraDb& csra_db);
    NCBI_DEPRECATED
    CCSraRefSeqIterator(const CCSraDb& csra_db, const string& seq_id);
    enum EByName {
        eByName
    };
    CCSraRefSeqIterator(const CCSraDb& csra_db, const string& name,
                        EByName /*by_name*/);
    CCSraRefSeqIterator(const CCSraDb& csra_db, const CSeq_id_Handle& seq_id);

    bool operator!(void) const {
        return !m_Db || m_Iter == m_Db->GetRefInfoList().end();
    }
    operator const void*(void) const {
        return !*this? 0: this;
    }

    const CCSraDb_Impl::SRefInfo& GetInfo(void) const;
    const CCSraDb_Impl::SRefInfo& operator*(void) const {
        return GetInfo();
    }
    const CCSraDb_Impl::SRefInfo* operator->(void) const {
        return &GetInfo();
    }

    CCSraRefSeqIterator& operator++(void) {
        ++m_Iter;
        return *this;
    }

    const string& GetRefSeqId(void) const {
        return m_Iter->m_SeqId;
    }
    CRef<CSeq_id> GetRefSeq_id(void) const {
        return m_Iter->GetMainSeq_id();
    }
    const CSeq_id_Handle& GetRefSeq_id_Handle(void) const {
        return m_Iter->GetMainSeq_id_Handle();
    }
    const CBioseq::TId& GetRefSeq_ids(void) const {
        return m_Iter->m_Seq_ids;
    }

    bool IsCircular(void) const;

    TSeqPos GetSeqLength(void) const;
    size_t GetRowAlignCount(int64_t row) const;

    CRef<CSeq_graph> GetCoverageGraph(void) const;
    CRef<CSeq_annot> GetCoverageAnnot(void) const;
    CRef<CSeq_annot> GetCoverageAnnot(const string& annot_name) const;

    CRef<CSeq_annot> GetSeq_annot(void) const;
    CRef<CSeq_annot> GetSeq_annot(const string& annot_name) const;

    enum ELoadData {
        eLoadData,
        eOmitData
    };
    CRef<CBioseq> GetRefBioseq(ELoadData load = eLoadData) const;
    typedef list< CRef<CSeq_literal> > TLiterals;
    typedef CRange<TSeqPos> TRange;
    void GetRefLiterals(TLiterals& literals,
                        const TRange& range,
                        ELoadData load = eLoadData) const;

protected:
    friend class CCSraAlignIterator;

    CRef<CSeq_annot> x_GetSeq_annot(const string* annot_name) const;

    static CRef<CSeq_annot> MakeSeq_annot(const string& annot_name);

    CCSraDb_Impl& GetDb(void) const {
        return m_Db.GetNCObject();
    }

private:
    CCSraDb m_Db;
    CCSraDb_Impl::TRefInfoList::const_iterator m_Iter;
};


class NCBI_SRAREAD_EXPORT CCSraAlignIterator
{
public:
    CCSraAlignIterator(void);
    
    NCBI_DEPRECATED
    CCSraAlignIterator(const CCSraDb& csra_db,
                       const string& ref_id,
                       TSeqPos ref_pos,
                       TSeqPos window = 0);
    CCSraAlignIterator(const CCSraDb& csra_db,
                       const CSeq_id_Handle& ref_id,
                       TSeqPos ref_pos,
                       TSeqPos window = 0);
    ~CCSraAlignIterator(void);

    void Select(TSeqPos ref_pos, TSeqPos window = 0);
    
    operator const void*(void) const {
        return m_Error? 0: this;
    }
    bool operator!(void) const {
        return m_Error != 0;
    }

    CCSraAlignIterator& operator++(void) {
        x_Next();
        return *this;
    }

    uint64_t GetAlignmentId(void) const;

    bool IsSecondary(void) const {
        return m_AlnRowIsSecondary;
    }

    CTempString GetRefSeqId(void) const;
    TSeqPos GetRefSeqPos(void) const {
        return m_CurRefPos;
    }
    TSeqPos GetRefSeqLen(void) const {
        return m_CurRefLen;
    }
    bool GetRefMinusStrand(void) const;

    int GetMapQuality(void) const;

    uint64_t GetShortId1(void) const;
    uint32_t GetShortId2(void) const;
    TSeqPos GetShortPos(void) const;
    TSeqPos GetShortLen(void) const;

    CTempString GetSpotGroup(void) const;

    bool IsSetName(void) const;
    CTempString GetName(void) const;

    CTempString GetCIGAR(void) const;
    // returns long form of CIGAR
    CTempString GetCIGARLong(void) const;
    // GetMismatchRead() returns difference of the short read and reference
    // sequence. Matching bases are represented as '='.
    // The short read is reversed to match direction of the reference seq.
    CTempString GetMismatchRead(void) const;
    // GetMismatchRaw() returns only mismatched and inserted/mismatched bases.
    CTempString GetMismatchRaw(void) const;
    // MakeFullMismatch() generates all mismatched and all inserted bases.
    void MakeFullMismatch(string& str,
                          CTempString cigar,
                          CTempString mismatch) const;

    CRef<CSeq_id> GetRefSeq_id(void) const {
        return m_RefIter->GetMainSeq_id();
    }
    CRef<CSeq_id> GetShortSeq_id(void) const;
    CRef<CSeq_id> GetMateShortSeq_id(void) const;
    CRef<CBioseq> GetShortBioseq(void) const;
    CRef<CSeq_align> GetMatchAlign(void) const;
    CRef<CSeq_graph> GetQualityGraph(void) const;
    CRef<CSeq_annot> GetEmptyMatchAnnot(void) const;
    CRef<CSeq_annot> GetEmptyMatchAnnot(const string& annot_name) const;
    CRef<CSeq_annot> GetMatchAnnot(void) const;
    CRef<CSeq_annot> GetMatchAnnot(const string& annot_name) const;
    CRef<CSeq_annot> GetQualityGraphAnnot(void) const;
    CRef<CSeq_annot> GetQualityGraphAnnot(const string& annot_name) const;
    CRef<CSeq_entry> GetMatchEntry(void) const;
    CRef<CSeq_entry> GetMatchEntry(const string& annot_name) const;
    CRef<CSeq_annot> GetSeq_annot(void) const;
    CRef<CSeq_annot> GetSeq_annot(const string& annot_name) const;

    static CRef<CSeq_annot> MakeSeq_annot(const string& annot_name);
    static CRef<CSeq_annot> MakeEmptyMatchAnnot(const string& annot_name);
    static CRef<CAnnotdesc> MakeMatchAnnotIndicator(void);

protected:
    CCSraDb_Impl& GetDb(void) const {
        return m_RefIter.GetDb();
    }

    void x_Settle(void); // skip all non-matching elements
    void x_Next(void) {
        ++m_AlnRowCur;
        x_Settle();
    }

    CRef<CSeq_entry> x_GetMatchEntry(const string* annot_name) const;
    CRef<CSeq_annot> x_GetEmptyMatchAnnot(const string* annot_name) const;
    CRef<CSeq_annot> x_GetMatchAnnot(const string* annot_name) const;
    CRef<CSeq_annot> x_GetQualityGraphAnnot(const string* annot_name) const;
    CRef<CSeq_annot> x_GetSeq_annot(const string* annot_name) const;
    
    typedef CRef<CObject_id> TObjectIdCache;
    typedef map<CTempString, CRef<CUser_field> > TUserFieldCache;
    CRef<CUser_object> x_GetSecondaryIndicator(void) const;
    CObject_id& x_GetObject_id(const char* name, TObjectIdCache& cache) const;
    CUser_field& x_AddField(CUser_object& obj,
                            const char* name,
                            TObjectIdCache& cache) const;
    void x_AddField(CUser_object& obj, const char* name, CTempString value,
                    TObjectIdCache& cache) const;
    void x_AddField(CUser_object& obj, const char* name, int value,
                    TObjectIdCache& cache) const;
    void x_AddField(CUser_object& obj, const char* name, CTempString value,
                    TObjectIdCache& id_cache, TUserFieldCache& cache,
                    size_t max_value_length, size_t max_cache_size) const;

private:
    CCSraRefSeqIterator m_RefIter; // refseq selector
    CRef<CCSraDb_Impl::SRefTableCursor> m_Ref; // VDB ref table accessor
    CRef<CCSraDb_Impl::SAlnTableCursor> m_Aln; // VDB align table accessor

    rc_t m_Error; // result of VDB access
    TSeqPos m_ArgRefPos, m_ArgRefLast; // requested refseq range
    TSeqPos m_CurRefPos, m_CurRefLen; // current alignment refseq range

    uint64_t m_RefRowNext; // refseq row range
    uint64_t m_RefRowLast;
    bool m_AlnRowIsSecondary;
    const uint64_t* m_AlnRowCur; // current refseq row alignments ids
    const uint64_t* m_AlnRowEnd;

    mutable CRef<CAnnotdesc> m_MatchAnnotIndicator;

    mutable TObjectIdCache m_ObjectIdMateRead;
    mutable TObjectIdCache m_ObjectIdRefId;
    mutable TObjectIdCache m_ObjectIdRefPos;
    mutable TObjectIdCache m_ObjectIdLcl;
    mutable TObjectIdCache m_ObjectIdTracebacks;
    mutable TObjectIdCache m_ObjectIdCIGAR;
    mutable TObjectIdCache m_ObjectIdMISMATCH;
    mutable TUserFieldCache m_UserFieldCacheCigar;
    mutable TUserFieldCache m_UserFieldCacheMismatch;
    mutable CRef<CUser_object> m_SecondaryIndicator;
};


class NCBI_SRAREAD_EXPORT CCSraShortReadIterator
{
public:
    enum EClipType {
        eDefaultClip,  // as defined by config
        eNoClip,       // force no clipping
        eClipByQuality // force clipping
    };

    CCSraShortReadIterator(void);
    explicit
    CCSraShortReadIterator(const CCSraDb& csra_db,
                           EClipType clip_type = eDefaultClip);
    // The last constructor parameter was changed from zero-based mate_index
    // to one-based read_id to reflect standardization of short read ids
    // in form gnl|SRA|<SRA accesion>.<Spot id>.<Read id>.
    CCSraShortReadIterator(const CCSraDb& csra_db,
                           uint64_t spot_id,
                           EClipType clip_type = eDefaultClip);
    CCSraShortReadIterator(const CCSraDb& csra_db,
                           uint64_t spot_id,
                           uint32_t read_id,
                           EClipType clip_type = eDefaultClip);
    ~CCSraShortReadIterator(void);

    bool Select(uint64_t spot_id);
    bool Select(uint64_t spot_id, uint32_t read_id);
    
    operator const void*(void) const {
        return m_Error? 0: this;
    }
    bool operator!(void) const {
        return m_Error != 0;
    }

    CCSraShortReadIterator& operator++(void) {
        x_Next();
        return *this;
    }

    uint64_t GetSpotId(void) const {
        return m_SpotId;
    }
    uint64_t GetMaxSpotId(void) const {
        return m_MaxSpotId;
    }
    uint32_t GetReadId(void) const {
        return m_ReadId;
    }
    uint32_t GetMaxReadId(void) const {
        return m_MaxReadId;
    }

    // Use GetReadId() instead of GetMateIndex().
    // Note that GetReadId() is one-based and GetMateIndex() is zero-based.
    NCBI_DEPRECATED uint32_t GetMateIndex(void) const {
        return GetReadId()-1;
    }
    // Use GetMaxReadId() instead of GetMateCount().
    // The read_id goes from 1 to GetMaxReadId().
    NCBI_DEPRECATED uint32_t GetMateCount(void) const {
        return GetMaxReadId();
    }

    CTempString GetSpotGroup(void) const;

    bool IsSetName(void) const;
    CTempString GetName(void) const;

    // Returns true if current read has clipping info that can or does 
    // reduce sequence length.
    bool HasClippingInfo(void) const;
    // Returns true if current read is actually clipped by quality.
    // It can be true only if clipping by quality is on.
    bool IsClippedByQuality(void) const {
        return m_ClipByQuality && HasClippingInfo();
    }
    // Returns true if current read has actual clipping info that is not
    // applied because clipping by quality is off.
    bool ShouldBeClippedByQuality(void) const {
        return !m_ClipByQuality && HasClippingInfo();
    }

    CTempString GetReadData(EClipType clip_type = eDefaultClip) const;

    uint64_t GetShortId1(void) const {
        return GetSpotId();
    }
    uint32_t GetShortId2(void) const {
        return GetReadId();
    }

    bool IsTechnicalRead(void) const;

    // returns current read range inside spot
    typedef COpenRange<TSeqPos> TOpenRange;
    TOpenRange GetReadRange(EClipType clip_type = eDefaultClip) const;

    TSeqPos GetShortLen(void) const {
        return GetReadRange().GetLength();
    }

    CRef<CSeq_id> GetShortSeq_id(void) const;

    // clip coordinate (inclusive)
    TSeqPos GetClipQualityLeft(void) const;
    TSeqPos GetClipQualityRight(void) const;

    CRef<CSeq_graph> GetQualityGraph(void) const;
    CRef<CSeq_annot> GetQualityGraphAnnot(void) const;
    CRef<CSeq_annot> GetQualityGraphAnnot(const string& annot_name) const;

    enum EBioseqFlags {
        fQualityGraph = 1<<0,
        fDefaultBioseqFlags = 0
    };
    typedef int TBioseqFlags;
    CRef<CBioseq> GetShortBioseq(TBioseqFlags flags = fDefaultBioseqFlags) const;

    CCSraRefSeqIterator GetRefSeqIter(TSeqPos* ref_pos_ptr = 0) const;

protected:
    CCSraDb_Impl& GetDb(void) const {
        return m_Db.GetNCObject();
    }
    
    void x_Init(const CCSraDb& csra_db, EClipType clip_type);
    bool x_ValidRead(void) const;
    bool x_Settle(bool single_spot = false);
    bool x_Next(void) {
        ++m_ReadId;
        return x_Settle();
    }
    

    void x_GetMaxReadId(void);
    CTempString x_GetReadData(const TOpenRange& range) const;

    CRef<CSeq_annot> x_GetSeq_annot(const string* annot_name) const;
    CRef<CSeq_annot> x_GetQualityGraphAnnot(const string* annot_name) const;
    CRef<CSeq_annot> x_GetQualityGraphAnnot(const TOpenRange& range,
                                            const string* annot_name) const;
    CRef<CSeq_graph> x_GetQualityGraph(const TOpenRange& range) const;
    
private:
    CCSraDb m_Db; // refseq selector
    CRef<CCSraDb_Impl::SSeqTableCursor> m_Seq; // VDB sequence table accessor

    uint64_t m_SpotId;
    uint64_t m_MaxSpotId;
    uint32_t m_ReadId;
    uint32_t m_MaxReadId;
    bool m_IncludeTechnicalReads;
    bool m_ClipByQuality;

    rc_t m_Error; // result of VDB access
};


/////////////////////////////////////////////////////////////////////////////
// CCSraRefSeqIterator

inline
CRef<CSeq_annot>
CCSraRefSeqIterator::GetSeq_annot(const string& annot_name) const
{
    return x_GetSeq_annot(&annot_name);
}


inline
CRef<CSeq_annot>
CCSraRefSeqIterator::GetSeq_annot(void) const
{
    return x_GetSeq_annot(0);
}


/////////////////////////////////////////////////////////////////////////////
// CCSraAlignIterator

inline
CRef<CSeq_entry>
CCSraAlignIterator::GetMatchEntry(const string& annot_name) const
{
    return x_GetMatchEntry(&annot_name);
}


inline
CRef<CSeq_entry>
CCSraAlignIterator::GetMatchEntry(void) const
{
    return x_GetMatchEntry(0);
}


inline
CRef<CSeq_annot>
CCSraAlignIterator::GetEmptyMatchAnnot(const string& annot_name) const
{
    return x_GetEmptyMatchAnnot(&annot_name);
}


inline
CRef<CSeq_annot>
CCSraAlignIterator::GetEmptyMatchAnnot(void) const
{
    return x_GetEmptyMatchAnnot(0);
}


inline
CRef<CSeq_annot>
CCSraAlignIterator::GetMatchAnnot(const string& annot_name) const
{
    return x_GetMatchAnnot(&annot_name);
}


inline
CRef<CSeq_annot>
CCSraAlignIterator::GetMatchAnnot(void) const
{
    return x_GetMatchAnnot(0);
}


inline
CRef<CSeq_annot>
CCSraAlignIterator::GetQualityGraphAnnot(const string& annot_name) const
{
    return x_GetQualityGraphAnnot(&annot_name);
}


inline
CRef<CSeq_annot>
CCSraAlignIterator::GetQualityGraphAnnot(void) const
{
    return x_GetQualityGraphAnnot(0);
}


inline
CRef<CSeq_annot>
CCSraAlignIterator::x_GetSeq_annot(const string* annot_name) const
{
    return m_RefIter.x_GetSeq_annot(annot_name);
}


inline
CRef<CSeq_annot>
CCSraAlignIterator::GetSeq_annot(const string& annot_name) const
{
    return x_GetSeq_annot(&annot_name);
}


inline
CRef<CSeq_annot>
CCSraAlignIterator::GetSeq_annot(void) const
{
    return x_GetSeq_annot(0);
}


inline
CRef<CSeq_annot>
CCSraAlignIterator::MakeSeq_annot(const string& annot_name)
{
    return CCSraRefSeqIterator::MakeSeq_annot(annot_name);
}


/////////////////////////////////////////////////////////////////////////////
// CCSraShortReadIterator

inline
CRef<CSeq_annot>
CCSraShortReadIterator::GetQualityGraphAnnot(const string& annot_name) const
{
    return x_GetQualityGraphAnnot(&annot_name);
}


inline
CRef<CSeq_annot>
CCSraShortReadIterator::GetQualityGraphAnnot(void) const
{
    return x_GetQualityGraphAnnot(0);
}


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // SRA__READER__SRA__CSRAREAD__HPP
