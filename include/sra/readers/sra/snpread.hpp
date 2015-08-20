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

class CSNPDbRefSeqIterator;
class CSNPDbIterator;

class NCBI_SRAREAD_EXPORT CSNPDb_Impl : public CObject
{
public:
    CSNPDb_Impl(CVDBMgr& mgr,
                CTempString path_or_acc);
    virtual ~CSNPDb_Impl(void);

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
        vector<TSeqPos> m_AlnOverStarts; // relative to m_RowFirst
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

protected:
    friend class CSNPDbRefSeqIterator;
    friend class CSNPDbIterator;

    // SSNPTableCursor is helper accessor structure for SNP table
    struct SSNPTableCursor : public CObject {
        explicit SSNPTableCursor(const CVDBTable& table);

        CVDBCursor m_Cursor;

        DECLARE_VDB_COLUMN_AS_STRING(ACCESSION);
        DECLARE_VDB_COLUMN_AS(INSDC_coord_one, FROM);
        DECLARE_VDB_COLUMN_AS(INSDC_coord_len, LEN);
        DECLARE_VDB_COLUMN_AS_STRING(RS_ALLELE);
        DECLARE_VDB_COLUMN_AS_STRING(RS_ALT_ALLELE);
        DECLARE_VDB_COLUMN_AS(Uint4, RS_ID);
        DECLARE_VDB_COLUMN_AS(Uint1, BIT_FIELD);
    };

    // open tables
    void OpenTable(CVDBTable& table,
                   const char* table_name,
                   volatile bool& table_is_opened);

    void OpenSNPTable(void);

    const CVDBTable& SNPTable(void) {
        return m_SNPTable;
    }

    // get table accessor object for exclusive access
    CRef<SSNPTableCursor> SNP(void);
    // return table accessor object for reuse
    void Put(CRef<SSNPTableCursor>& curs) {
        m_SNP.Put(curs);
    }

    void x_CalcSeqLength(const SRefInfo& info);

private:
    CVDBMgr m_Mgr;

    CVDBTable m_SNPTable;

    CVDBObjectCache<SSNPTableCursor> m_SNP;

    TRefInfoList m_RefList; // list of cached refseqs' information
    TRefInfoMapByName m_RefMapByName; // index for refseq info lookup
    TRefInfoMapBySeq_id m_RefMapBySeq_id; // index for refseq info lookup
};


class CSNPDb : public CRef<CSNPDb_Impl>
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


class NCBI_SRAREAD_EXPORT CSNPDbRefSeqIterator
{
public:
    CSNPDbRefSeqIterator(void)
        {
        }
    CSNPDbRefSeqIterator(const CSNPDb& db,
                         CSNPDb_Impl::TRefInfoList::const_iterator iter)
        : m_Db(db),
          m_Iter(iter)
        {
        }
    explicit CSNPDbRefSeqIterator(const CSNPDb& db);
    enum EByName {
        eByName
    };
    CSNPDbRefSeqIterator(const CSNPDb& db, const string& name,
                         EByName /*by_name*/);
    CSNPDbRefSeqIterator(const CSNPDb& db, const CSeq_id_Handle& seq_id);

    bool operator!(void) const {
        return !m_Db || m_Iter == m_Db->GetRefInfoList().end();
    }
    operator const void*(void) const {
        return !*this? 0: this;
    }

    const CSNPDb_Impl::SRefInfo& GetInfo(void) const;
    const CSNPDb_Impl::SRefInfo& operator*(void) const {
        return GetInfo();
    }
    const CSNPDb_Impl::SRefInfo* operator->(void) const {
        return &GetInfo();
    }

    CSNPDbRefSeqIterator& operator++(void) {
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
                        TRange range,
                        ELoadData load = eLoadData) const;

protected:
    friend class CSNPDbIterator;

    CRef<CSeq_annot> x_GetSeq_annot(const string* annot_name) const;

    static CRef<CSeq_annot> MakeSeq_annot(const string& annot_name);

    CSNPDb_Impl& GetDb(void) const {
        return m_Db.GetNCObject();
    }

private:
    CSNPDb m_Db;
    CSNPDb_Impl::TRefInfoList::const_iterator m_Iter;
};


class NCBI_SRAREAD_EXPORT CSNPDbIterator
{
public:
    CSNPDbIterator(void);

    enum ESearchMode {
        eSearchByOverlap,
        eSearchByStart
    };

    CSNPDbIterator(const CSNPDb& db,
                   const CSeq_id_Handle& ref_id,
                   TSeqPos ref_pos = 0,
                   TSeqPos window = 0,
                   ESearchMode search_mode = eSearchByOverlap);
    CSNPDbIterator(const CSNPDb& db,
                   const CSeq_id_Handle& ref_id,
                   COpenRange<TSeqPos> ref_range,
                   ESearchMode search_mode = eSearchByOverlap);
    ~CSNPDbIterator(void);

    void Select(COpenRange<TSeqPos> ref_range,
                ESearchMode search_mode = eSearchByOverlap);

    DECLARE_OPERATOR_BOOL(m_CurSNPId < m_FirstBadSNPId);

    CSNPDbIterator& operator++(void) {
        x_Next();
        return *this;
    }

    uint64_t GetCurrentRowId(void) const {
        return m_CurSNPId;
    }

    const string& GetAccession(void) const {
        return m_RefIter.GetRefSeqId();
    }
    CRef<CSeq_id> GetSeqId(void) const {
        return m_RefIter.GetRefSeq_id();
    }
    CSeq_id_Handle GetSeqIdHandle(void) const {
        return m_RefIter.GetRefSeq_id_Handle();
    }
 
    TSeqPos GetSNPPosition(void) const {
        return m_CurRange.GetFrom();
    }
    TSeqPos GetSNPLength(void) const {
        return m_CurRange.GetLength();
    }

    CTempString GetAllele(void) const;
    CTempString GetAltAllele(void) const;

    Uint4 GetRsId(void) const;

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
    typedef int TFlags;
    
    CRef<CSeq_feat> GetSeq_feat(TFlags flags = fDefaultFlags) const;

protected:
    CSNPDb_Impl& GetDb(void) const {
        return m_RefIter.GetDb();
    }

    void x_Init(const CSNPDb& snp_db);

    void x_ReportInvalid(const char* method) const;
    void x_CheckValid(const char* method) const {
        if ( !*this ) {
            x_ReportInvalid(method);
        }
    }

    void x_Next(void);
    void x_Settle(void);
    bool x_Excluded(void);

private:
    CSNPDbRefSeqIterator m_RefIter; // refseq selector
    CRef<CSNPDb_Impl::SSNPTableCursor> m_SNP; // SNP table accessor

    rc_t m_Error; // result of VDB access
    COpenRange<TSeqPos> m_ArgRange; // requested refseq range
    COpenRange<TSeqPos> m_CurRange; // current SNP refseq range

    uint64_t m_CurSNPId, m_FirstBadSNPId;

    ESearchMode m_SearchMode;

    typedef CRef<CObject_id> TObjectIdCache;
    typedef map<CTempString, CRef<CUser_field> > TUserFieldCache;

    struct SCreateCache;
    mutable AutoPtr<SCreateCache> m_CreateCache;
    SCreateCache& x_GetCreateCache(void) const;
};


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // SRA__READER__SRA__SNPREAD__HPP
