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
 *   Access to WGS files
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <util/range.hpp>
#include <sra/readers/sra/vdbread.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_data.hpp>
#include <ncbi/ncbi.h>
#include <ncbi/wgs-contig.h>

typedef int32_t NCBI_WGS_gap_linkage;

#include <map>
#include <list>

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(objects);

class CSeq_entry;
class CSeq_annot;
class CSeq_align;
class CSeq_graph;
class CBioseq;
class CUser_object;
class CUser_field;

class CWGSSeqIterator;
class CWGSScaffoldIterator;

class NCBI_SRAREAD_EXPORT CWGSDb_Impl : public CObject
{
public:
    CWGSDb_Impl(void) {}
    CWGSDb_Impl(CVDBMgr& mgr,
                CTempString path_or_acc,
                CTempString vol_path = CTempString());

    const string& GetIdPrefix(void) const {
        return m_IdPrefix;
    }
    const string& GetIdPrefixWithVersion(void) const {
        return m_IdPrefixWithVersion;
    }
    const string& GetWGSPath(void) const {
        return m_WGSPath;
    }

    // normalize accession to the form acceptable by SRA SDK
    // 0. if the argument looks like path - do not change it
    // 1. exclude contig/scaffold id
    // 2. add default version (1)
    static string NormalizePathOrAccession(CTempString path_or_acc,
                                           CTempString vol_path = CTempString());

    // parse row id from accession
    // returns 0 if accession is in wrong format
    // if is_scaffold flag pointer is not null, then scaffold ids are also
    // accepted and the flag is set appropriately
    uint64_t ParseRow(CTempString acc, bool* is_scaffold = 0) const;
    // parse scaffold row id from accession
    // returns 0 if accession is in wrong format
    uint64_t ParseScaffoldRow(CTempString acc) const {
        bool is_scaffold;
        uint64_t row = ParseRow(acc, &is_scaffold);
        return is_scaffold? row: 0;
    }
    SIZE_TYPE GetIdRowDigits(void) const {
        return m_IdRowDigits;
    }

    CRef<CSeq_id> GetMasterSeq_id(void) const;
    CRef<CSeq_id> GetContigSeq_id(uint64_t row_id) const;
    CRef<CSeq_id> GetScaffoldSeq_id(uint64_t row_id) const;

    typedef vector< CRef<CSeqdesc> > TMasterDescr;
    void SetMasterDescr(const TMasterDescr& descr);
    const TMasterDescr& GetMasterDescr(void) const {
        return m_MasterDescr;
    }

protected:
    friend class CWGSSeqIterator;
    friend class CWGSScaffoldIterator;

    // SSeqTableCursor is helper accessor structure for SEQUENCE table
    struct SSeqTableCursor : public CObject {
        SSeqTableCursor(const CVDB& db);

        CVDBTable m_Table;
        CVDBCursor m_Cursor;

        DECLARE_VDB_COLUMN_AS(NCBI_gi, GI);
        DECLARE_VDB_COLUMN_AS_STRING(ACCESSION);
        DECLARE_VDB_COLUMN_AS(uint32_t, ACC_VERSION);
        DECLARE_VDB_COLUMN_AS_STRING(CONTIG_NAME);
        DECLARE_VDB_COLUMN_AS_STRING(NAME);
        DECLARE_VDB_COLUMN_AS_STRING(TITLE);
        DECLARE_VDB_COLUMN_AS_STRING(LABEL);
        DECLARE_VDB_COLUMN_AS(INSDC_coord_zero, READ_START);
        DECLARE_VDB_COLUMN_AS(INSDC_coord_len, READ_LEN);
        DECLARE_VDB_COLUMN_AS_4BITS(READ);
        DECLARE_VDB_COLUMN_AS(NCBI_taxid, TAXID);
        DECLARE_VDB_COLUMN_AS_STRING(DESCR);
        DECLARE_VDB_COLUMN_AS_STRING(ANNOT);
        DECLARE_VDB_COLUMN_AS(NCBI_gb_state, GB_STATE);
        DECLARE_VDB_COLUMN_AS(INSDC_coord_zero, GAP_START);
        DECLARE_VDB_COLUMN_AS(INSDC_coord_len, GAP_LEN);
        DECLARE_VDB_COLUMN_AS(NCBI_WGS_component_props, GAP_PROPS);
        DECLARE_VDB_COLUMN_AS(NCBI_WGS_gap_linkage, GAP_LINKAGE);
    };

    // SSeqTableCursor is helper accessor structure for SCAFFOLD table
    struct SScfTableCursor : public CObject {
        SScfTableCursor(const CVDB& db);

        CVDBTable m_Table;
        CVDBCursor m_Cursor;

        DECLARE_VDB_COLUMN_AS_STRING(SCAFFOLD_NAME);
        DECLARE_VDB_COLUMN_AS_STRING(ACCESSION);
        DECLARE_VDB_COLUMN_AS(uint64_t, COMPONENT_ID);
        DECLARE_VDB_COLUMN_AS(INSDC_coord_one, COMPONENT_START);
        DECLARE_VDB_COLUMN_AS(INSDC_coord_len, COMPONENT_LEN);
        DECLARE_VDB_COLUMN_AS(NCBI_WGS_component_props, COMPONENT_PROPS);
        DECLARE_VDB_COLUMN_AS(NCBI_WGS_gap_linkage, COMPONENT_LINKAGE);
    };

    // SSeqTableCursor is helper accessor structure for SEQUENCE table
    struct SIdxTableCursor : public CObject {
        SIdxTableCursor(const CVDB& db);

        CVDBTable m_Table;
        CVDBCursor m_Cursor;

        DECLARE_VDB_COLUMN_AS(uint64_t, NUC_ROW_ID);
    };

    // get table accessor object for exclusive access
    CRef<SSeqTableCursor> Seq(void);
    CRef<SScfTableCursor> Scf(void);
    CRef<SIdxTableCursor> Idx(void);
    // return table accessor object for reuse
    void Put(CRef<SSeqTableCursor>& curs) {
        m_Seq.Put(curs);
    }
    void Put(CRef<SScfTableCursor>& curs) {
        m_Scf.Put(curs);
    }
    void Put(CRef<SIdxTableCursor>& curs) {
        m_Idx.Put(curs);
    }

protected:
    void x_InitIdParams(void);

private:
    CVDBMgr m_Mgr;
    string m_WGSPath;
    CVDB m_Db;
    string m_IdPrefixWithVersion;
    string m_IdPrefix;
    int m_IdVersion;
    SIZE_TYPE m_IdRowDigits;

    CVDBObjectCache<SSeqTableCursor> m_Seq;
    CVDBObjectCache<SScfTableCursor> m_Scf;
    CVDBObjectCache<SIdxTableCursor> m_Idx;

    TMasterDescr m_MasterDescr;

private:
    CWGSDb_Impl(const CWGSDb_Impl&);
    void operator=(const CWGSDb_Impl&);
};


class CWGSDb : public CRef<CWGSDb_Impl>
{
public:
    CWGSDb(void)
        {
        }
    CWGSDb(CWGSDb_Impl* impl)
        : CRef<CWGSDb_Impl>(impl)
        {
        }
    CWGSDb(CVDBMgr& mgr,
           CTempString path_or_acc,
           CTempString vol_path = CTempString())
        : CRef<CWGSDb_Impl>(new CWGSDb_Impl(mgr, path_or_acc, vol_path))
        {
        }

    const string& GetWGSPath(void) const {
        return GetObject().GetWGSPath();
    }

    // parse row id from accession
    // returns 0 if accession is in wrong format
    // if is_scaffold flag pointer is not null, then scaffold ids are also
    // accepted and the flag is set appropriately
    uint64_t ParseRow(CTempString acc, bool* is_scaffold = 0) const {
        return GetObject().ParseRow(acc, is_scaffold);
    }
    // parse scaffold row id from accession
    // returns 0 if accession is in wrong format
    uint64_t ParseScaffoldRow(CTempString acc) const {
        return GetObject().ParseScaffoldRow(acc);
    }
};


class NCBI_SRAREAD_EXPORT CWGSSeqIterator
{
public:
    enum EWithdrawn {
        eExcludeWithdrawn,
        eIncludeWithdrawn
    };
    CWGSSeqIterator(void);
    CWGSSeqIterator(const CWGSDb& wgs_db,
                    EWithdrawn withdrawn = eExcludeWithdrawn);
    CWGSSeqIterator(const CWGSDb& wgs_db, uint64_t row,
                    EWithdrawn withdrawn = eExcludeWithdrawn);
    CWGSSeqIterator(const CWGSDb& wgs_db, CTempString acc,
                    EWithdrawn withdrawn = eExcludeWithdrawn);
    ~CWGSSeqIterator(void);

    DECLARE_SAFE_BOOL_METHOD(m_CurrId < m_FirstBadId);

    CWGSSeqIterator& operator++(void);

    uint64_t GetCurrentRowId(void) const {
        return m_CurrId;
    }
    uint64_t GetLastRowId(void) const {
        return m_FirstBadId - 1;
    }

    bool HasGi(void) const;
    CSeq_id::TGi GetGi(void) const;
    CTempString GetAccession(void) const;
    int GetAccVersion(void) const;
    CRef<CSeq_id> GetAccSeq_id(void) const;
    //CTempString GetGeneralId(void) const;
    CTempString GetContigName(void) const;

    bool HasTaxId(void) const;
    int GetTaxId(void) const;

    TSeqPos GetSeqLength(void) const;

    enum EFlags {
        fIds_gi       = 1<<0,
        fIds_acc      = 1<<1,
        fIds_gnl      = 1<<2,
        fMaskIds      = fIds_gi|fIds_acc|fIds_gnl,
        fDefaultIds   = fIds_gi|fIds_acc|fIds_gnl,

        fInst_ncbi4na = 0<<3,
        fInst_delta   = 1<<3,
        fMaskInst     = fInst_ncbi4na|fInst_delta,
        fDefaultInst  = fInst_delta,

        fDefaultFlags = fDefaultIds|fDefaultInst
    };
    typedef int TFlags;

    void GetIds(CBioseq::TId& ids, TFlags flags = fDefaultFlags) const;

    bool HasSeq_descr(void) const;
    CRef<CSeq_descr> GetSeq_descr(void) const;

    bool HasAnnotSet(void) const;
    typedef CBioseq::TAnnot TAnnotSet;
    void GetAnnotSet(TAnnotSet& annot_set) const;

    NCBI_gb_state GetGBState(void) const;

    CRef<CSeq_inst> GetSeq_inst(TFlags flags = fDefaultFlags) const;

    CRef<CBioseq> GetBioseq(TFlags flags = fDefaultFlags) const;

protected:
    void x_Init(const CWGSDb& wgs_db, EWithdrawn withdrawn);

    CWGSDb_Impl& GetDb(void) const {
        return m_Db.GetNCObject();
    }
    
    bool x_Excluded(void) const {
        return m_Withdrawn == eExcludeWithdrawn && *this &&
            GetGBState() == 3 /* withdrawn */;
    }

    void x_ReportInvalid(const char* method) const;
    void x_CheckValid(const char* method) const {
        if ( !*this ) {
            x_ReportInvalid(method);
        }
    }

private:
    CWGSDb m_Db;
    CRef<CWGSDb_Impl::SSeqTableCursor> m_Seq; // VDB seq table accessor
    uint64_t m_CurrId, m_FirstBadId;
    EWithdrawn m_Withdrawn;
};


class NCBI_SRAREAD_EXPORT CWGSScaffoldIterator
{
public:
    CWGSScaffoldIterator(void);
    CWGSScaffoldIterator(const CWGSDb& wgs_db);
    CWGSScaffoldIterator(const CWGSDb& wgs_db, uint64_t row);
    CWGSScaffoldIterator(const CWGSDb& wgs_db, CTempString acc);
    ~CWGSScaffoldIterator(void);

    DECLARE_SAFE_BOOL_METHOD(m_CurrId < m_FirstBadId);

    CWGSScaffoldIterator& operator++(void) {
        ++m_CurrId;
        return *this;
    }

    uint64_t GetCurrentRowId(void) const {
        return m_CurrId;
    }
    uint64_t GetLastRowId(void) const {
        return m_FirstBadId - 1;
    }

    CTempString GetAccession(void) const;
    CRef<CSeq_id> GetAccSeq_id(void) const;

    CTempString GetScaffoldName(void) const;

    void GetIds(CBioseq::TId& ids) const;

    TSeqPos GetSeqLength(void) const;

    CRef<CSeq_inst> GetSeq_inst(void) const;

    CRef<CBioseq> GetBioseq(void) const;

protected:
    void x_Init(const CWGSDb& wgs_db);

    CWGSDb_Impl& GetDb(void) const {
        return m_Db.GetNCObject();
    }
    
    void x_ReportInvalid(const char* method) const;
    void x_CheckValid(const char* method) const {
        if ( !*this ) {
            x_ReportInvalid(method);
        }
    }

private:
    CWGSDb m_Db;
    CRef<CWGSDb_Impl::SScfTableCursor> m_Scf; // VDB scaffold table accessor
    uint64_t m_CurrId, m_FirstBadId;
};


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // SRA__READER__SRA__WGSREAD__HPP
