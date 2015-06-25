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
#include <util/range.hpp>
#include <util/rangemap.hpp>
#include <sra/readers/sra/vdbread.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_data.hpp>
#include <map>
#include <list>

#include <ncbi/ncbi.h>
#include <ncbi/wgs-contig.h>
#include <insdc/insdc.h>

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
class CWGSGiIterator;
class CWGSProteinIterator;


class NCBI_SRAREAD_EXPORT CWGSGiResolver
{
public:
    CWGSGiResolver(void);
    ~CWGSGiResolver(void);

    bool IsValid(void) const {
        return !m_GiIndex.empty();
    }

    // return all WGS accessions that could contain gi
    typedef vector<CTempString> TAccessionList;
    TAccessionList FindAll(TGi gi) const;

    // return single WGS accession that could contain gi
    // return empty string if there are no accession candidates
    // throw an exception if there are more than one candidate
    CTempString Find(TGi gi) const;

private:
    void x_Load(const string& file_name);

    enum {
        kMinAccessionLength = 6,
        kMaxAccessionLength = 6
    };
    struct SAccession {
        char accession[kMaxAccessionLength+1];
    };
    typedef CRangeMultimap<SAccession, TIntId> TGiIndex;
    TGiIndex m_GiIndex;
};


class NCBI_SRAREAD_EXPORT CWGSProtAccResolver
{
public:
    CWGSProtAccResolver(void);
    ~CWGSProtAccResolver(void);

    bool IsValid(void) const {
        return !m_ProtAccIndex.empty();
    }

    struct SAccInfo {
        SAccInfo(void);
        explicit SAccInfo(const string& acc);

        string GetAcc(void) const;

        string m_Prefix;
        uint64_t m_Id;
        size_t m_Length;
    };

    // return all WGS accessions that could contain protein accession
    typedef vector<CTempString> TAccessionList;
    TAccessionList FindAll(const string& acc) const;

    // return single WGS accession that could contain protein accession
    // return empty string if there are no accession candidates
    // throw an exception if there are more than one candidate
    CTempString Find(const string& acc) const;

private:
    void x_Load(const string& file_name);

    enum {
        kMinAccessionLength = 6,
        kMaxAccessionLength = 6
    };
    struct SAccession {
        char accession[kMaxAccessionLength+1];
    };
    typedef CRangeMultimap<SAccession, uint64_t> TProtAccIndex2;
    typedef map<string, TProtAccIndex2> TProtAccIndex;
    TProtAccIndex m_ProtAccIndex;
};


class NCBI_SRAREAD_EXPORT CWGSDb_Impl : public CObject
{
public:
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

    enum ERowType {
        eRowType_contig   = 0,
        eRowType_scaffold = 'S',
        eRowType_protein  = 'P'
    };
    typedef char TRowType;
    enum EAllowRowType {
        fAllowRowType_contig   = 1<<0,
        fAllowRowType_scaffold = 1<<1,
        fAllowRowType_protein  = 1<<2
    };
    typedef int TAllowRowType;
    // parse row id from accession
    // returns (row, accession_type) pair
    // row will be 0 if accession is in wrong format
    pair<uint64_t, TRowType> ParseRowType(CTempString acc,
                                          TAllowRowType allow) const;
    // parse row id from accession
    // returns 0 if accession is in wrong format
    // if is_scaffold flag pointer is not null, then scaffold ids are also
    // accepted and the flag is set appropriately
    uint64_t ParseRow(CTempString acc, bool* is_scaffold) const;
    // parse contig row id from accession
    // returns 0 if accession is in wrong format
    uint64_t ParseContigRow(CTempString acc) const {
        return ParseRowType(acc, fAllowRowType_contig).first;
    }
    // parse scaffold row id from accession
    // returns 0 if accession is in wrong format
    uint64_t ParseScaffoldRow(CTempString acc) const {
        return ParseRowType(acc, fAllowRowType_scaffold).first;
    }
    // parse protein row id from accession
    // returns 0 if accession is in wrong format
    uint64_t ParseProteinRow(CTempString acc) const {
        return ParseRowType(acc, fAllowRowType_protein).first;
    }
    SIZE_TYPE GetIdRowDigits(void) const {
        return m_IdRowDigits;
    }

    bool IsTSA(void) const;

    CRef<CSeq_id> GetGeneralSeq_id(CTempString tag) const;
    CRef<CSeq_id> GetAccSeq_id(CTempString acc,
                               int version) const;
    CRef<CSeq_id> GetAccSeq_id(ERowType type,
                               uint64_t row_id,
                               int version) const;
    CRef<CSeq_id> GetMasterSeq_id(void) const;
    CRef<CSeq_id> GetContigSeq_id(uint64_t row_id) const;
    CRef<CSeq_id> GetScaffoldSeq_id(uint64_t row_id) const;
    CRef<CSeq_id> GetProteinSeq_id(uint64_t row_id) const;

    typedef list< CRef<CSeqdesc> > TMasterDescr;

    bool IsSetMasterDescr(void) const {
        return m_IsSetMasterDescr;
    }
    const TMasterDescr& GetMasterDescr(void) const {
        return m_MasterDescr;
    }

    void ResetMasterDescr(void);
    void SetMasterDescr(const TMasterDescr& descr, int filter);
    bool LoadMasterDescr(int filter);

    // get GI range of nucleotide sequences
    pair<TGi, TGi> GetNucGiRange(void);
    // get GI range of proteine sequences
    pair<TGi, TGi> GetProtGiRange(void);
    // get row_id for a given GI or 0 if there is no GI
    // the second value in returned value is true if the sequence is protein
    pair<uint64_t, bool> GetGiRowId(TGi gi);
    // get nucleotide row_id (SEQUENCE) for a given GI or 0 if there is no GI
    uint64_t GetNucGiRowId(TGi gi);
    // get protein row_id (PROTEIN) for a given GI or 0 if there is no GI
    uint64_t GetProtGiRowId(TGi gi);

    // get protein row_id (PROTEIN) for GB accession or 0 if there is no acc
    uint64_t GetProtAccRowId(const string& acc);

protected:
    friend class CWGSSeqIterator;
    friend class CWGSScaffoldIterator;
    friend class CWGSGiIterator;
    friend class CWGSProtAccIterator;
    friend class CWGSProteinIterator;

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
        DECLARE_VDB_COLUMN_AS(INSDC_quality_phred, QUALITY);
        DECLARE_VDB_COLUMN_AS(bool, CIRCULAR);
        DECLARE_VDB_COLUMN_AS(Uint4 /*NCBI_WGS_hash*/, HASH);
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
        DECLARE_VDB_COLUMN_AS(bool, CIRCULAR);
    };

    // SSeqTableCursor is helper accessor structure for SEQUENCE table
    struct SIdxTableCursor : public CObject {
        SIdxTableCursor(const CVDB& db);

        CVDBTable m_Table;
        CVDBCursor m_Cursor;

        DECLARE_VDB_COLUMN_AS(uint64_t, NUC_ROW_ID);
        DECLARE_VDB_COLUMN_AS(uint64_t, PROT_ROW_ID);
    };

    // SProtTableCursor is helper accessor structure for optional PROTEIN table
    struct SProtTableCursor : public CObject {
        SProtTableCursor(const CVDB& db);

        CVDBTable m_Table;
        CVDBCursor m_Cursor;

        DECLARE_VDB_COLUMN_AS_STRING(ACCESSION);
        DECLARE_VDB_COLUMN_AS(uint32_t, ACC_VERSION);
        DECLARE_VDB_COLUMN_AS_STRING(GB_ACCESSION);
        DECLARE_VDB_COLUMN_AS_STRING(DESCR);
        DECLARE_VDB_COLUMN_AS_STRING(ANNOT);
        DECLARE_VDB_COLUMN_AS(NCBI_gb_state, GB_STATE);
        DECLARE_VDB_COLUMN_AS(INSDC_coord_len, PROTEIN_LEN);
        DECLARE_VDB_COLUMN_AS_STRING(PROTEIN_NAME);
        DECLARE_VDB_COLUMN_AS_STRING(TITLE);
        DECLARE_VDB_COLUMN_AS_STRING(SEQ_ID);
        DECLARE_VDB_COLUMN_AS_STRING(SEQ_ID_GNL);
        DECLARE_VDB_COLUMN_AS(NCBI_gi, REF_GI);
        DECLARE_VDB_COLUMN_AS_STRING(REF_ACC);
    };

    // get table accessor object for exclusive access
    CRef<SSeqTableCursor> Seq(void);
    CRef<SScfTableCursor> Scf(void);
    CRef<SIdxTableCursor> Idx(void);
    CRef<SProtTableCursor> Prot(void);
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
    void Put(CRef<SProtTableCursor>& curs) {
        m_Prot.Put(curs);
    }

protected:
    void x_InitIdParams(void);
    void x_LoadMasterDescr(const CVDBTable& table, int filter);

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
    CVDBObjectCache<SProtTableCursor> m_Prot;
    CVDBTableIndex m_ProtAccIndex;

    bool m_IsSetMasterDescr;
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
    explicit CWGSDb(CWGSDb_Impl* impl)
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
    NCBI_DEPRECATED
    uint64_t ParseRow(CTempString acc, bool* is_scaffold = 0) const {
        return GetObject().ParseRow(acc, is_scaffold);
    }
    // parse contig row id from accession
    // returns 0 if accession is in wrong format
    uint64_t ParseContigRow(CTempString acc) const {
        return GetObject().ParseContigRow(acc);
    }
    // parse scaffold row id from accession
    // returns 0 if accession is in wrong format
    uint64_t ParseScaffoldRow(CTempString acc) const {
        return GetObject().ParseScaffoldRow(acc);
    }
    // parse protein row id from accession
    // returns 0 if accession is in wrong format
    uint64_t ParseProteinRow(CTempString acc) const {
        return GetObject().ParseProteinRow(acc);
    }

    // get GI range of nucleotide sequences
    pair<TGi, TGi> GetNucGiRange(void) const {
        return GetNCObject().GetNucGiRange();
    }
    // get GI range of proteine sequences
    pair<TGi, TGi> GetProtGiRange(void) const {
        return GetNCObject().GetProtGiRange();
    }
    // get row_id for a given GI or 0 if there is no GI
    // the second value in returned value is true if the sequence is protein
    pair<uint64_t, bool> GetGiRowId(TGi gi) const {
        return GetNCObject().GetGiRowId(gi);
    }
    // get nucleotide row_id (SEQUENCE) for a given GI or 0 if there is no GI
    uint64_t GetNucGiRowId(TGi gi) const {
        return GetNCObject().GetNucGiRowId(gi);
    }
    // get protein row_id (PROTEIN) for a given GI or 0 if there is no GI
    uint64_t GetProtGiRowId(TGi gi) const {
        return GetNCObject().GetProtGiRowId(gi);
    }

    // get protein row_id (PROTEIN) for GB accession or 0 if there is no acc
    uint64_t GetProtAccRowId(const string& acc) const {
        return GetNCObject().GetProtAccRowId(acc);
    }

    enum EDescrFilter {
        eDescrNoFilter,
        eDescrDefaultFilter
    };
    // load master descriptors from VDB metadata (if any)
    // doesn't try to load if master descriptors already set
    // returns true if descriptors are set at the end, or false if not
    bool LoadMasterDescr(EDescrFilter filter = eDescrDefaultFilter) const {
        return GetNCObject().LoadMasterDescr(filter);
    }
    // set master descriptors
    typedef list< CRef<CSeqdesc> > TMasterDescr;
    void SetMasterDescr(const TMasterDescr& descr,
                        EDescrFilter filter = eDescrDefaultFilter) const {
        GetNCObject().SetMasterDescr(descr, filter);
    }
    enum EDescrType {
        eDescr_skip,
        eDescr_default,
        eDescr_force
    };
    // return type of master descriptor propagation
    static EDescrType GetMasterDescrType(const CSeqdesc& desc);
};


class NCBI_SRAREAD_EXPORT CWGSSeqIterator
{
public:
    enum EWithdrawn {
        eExcludeWithdrawn,
        eIncludeWithdrawn
    };
    CWGSSeqIterator(void);
    explicit
    CWGSSeqIterator(const CWGSDb& wgs_db,
                    EWithdrawn withdrawn = eExcludeWithdrawn);
    CWGSSeqIterator(const CWGSDb& wgs_db,
                    uint64_t row,
                    EWithdrawn withdrawn = eExcludeWithdrawn);
    CWGSSeqIterator(const CWGSDb& wgs_db,
                    uint64_t first_row, uint64_t last_row,
                    EWithdrawn withdrawn = eExcludeWithdrawn);
    CWGSSeqIterator(const CWGSDb& wgs_db,
                    CTempString acc,
                    EWithdrawn withdrawn = eExcludeWithdrawn);
    ~CWGSSeqIterator(void);

    DECLARE_SAFE_BOOL_METHOD(m_CurrId < m_FirstBadId);

    CWGSSeqIterator& operator++(void)
        {
            x_CheckValid("CWGSSeqIterator::operator++");
            ++m_CurrId;
            x_Settle();
            return *this;
        }

    uint64_t GetCurrentRowId(void) const {
        return m_CurrId;
    }
    uint64_t GetFirstBadRowId(void) const {
        return m_FirstBadId;
    }
    uint64_t GetLastRowId(void) const {
        return GetFirstBadRowId() - 1;
    }

    bool HasGi(void) const;
    CSeq_id::TGi GetGi(void) const;
    CTempString GetAccession(void) const;
    int GetAccVersion(void) const;

    // return corresponding kind of Seq-id if exists
    // return null if there is no such Seq-id
    CRef<CSeq_id> GetAccSeq_id(void) const;
    CRef<CSeq_id> GetGiSeq_id(void) const;
    CRef<CSeq_id> GetGeneralSeq_id(void) const;

    //CTempString GetGeneralId(void) const;
    CTempString GetContigName(void) const;

    bool HasTaxId(void) const;
    int GetTaxId(void) const;

    TSeqPos GetSeqLength(void) const;

    bool HasSeqHash(void) const;
    int GetSeqHash(void) const;

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

    CRef<CSeq_id> GetRefId(TFlags flags = fDefaultFlags) const;
    void GetIds(CBioseq::TId& ids, TFlags flags = fDefaultFlags) const;

    bool HasSeq_descr(void) const;
    CRef<CSeq_descr> GetSeq_descr(void) const;

    bool HasAnnotSet(void) const;
    typedef CBioseq::TAnnot TAnnotSet;
    void GetAnnotSet(TAnnotSet& annot_set) const;

    bool HasQualityGraph(void) const;
    void GetQualityAnnot(TAnnotSet& annot_set,
                         TFlags flags = fDefaultFlags) const;

    NCBI_gb_state GetGBState(void) const;

    bool IsCircular(void) const;

    CRef<CSeq_inst> GetSeq_inst(TFlags flags = fDefaultFlags) const;

    CRef<CBioseq> GetBioseq(TFlags flags = fDefaultFlags) const;

protected:
    void x_Init(const CWGSDb& wgs_db, EWithdrawn withdrawn);

    CWGSDb_Impl& GetDb(void) const {
        return m_Db.GetNCObject();
    }
    
    void x_Settle(void);
    bool x_Excluded(void) const;

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
    explicit CWGSScaffoldIterator(const CWGSDb& wgs_db);
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
    uint64_t GetFirstBadRowId(void) const {
        return m_FirstBadId;
    }
    uint64_t GetLastRowId(void) const {
        return GetFirstBadRowId() - 1;
    }

    CTempString GetAccession(void) const;
    CRef<CSeq_id> GetAccSeq_id(void) const;
    CRef<CSeq_id> GetGeneralSeq_id(void) const;

    CTempString GetScaffoldName(void) const;

    void GetIds(CBioseq::TId& ids) const;

    bool HasSeq_descr(void) const;
    CRef<CSeq_descr> GetSeq_descr(void) const;

    TSeqPos GetSeqLength(void) const;

    bool IsCircular(void) const;

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


class NCBI_SRAREAD_EXPORT CWGSGiIterator
{
public:
    enum ESeqType {
        eNuc  = 1 << 0,
        eProt = 1 << 1,
        eAll  = eNuc | eProt
    };
    CWGSGiIterator(void);
    explicit
    CWGSGiIterator(const CWGSDb& wgs_db, ESeqType seq_type = eAll);
    CWGSGiIterator(const CWGSDb& wgs_db, TGi gi, ESeqType seq_type = eAll);
    ~CWGSGiIterator(void);

    DECLARE_SAFE_BOOL_METHOD(m_CurrGi < m_FirstBadGi);

    CWGSGiIterator& operator++(void) {
        ++m_CurrGi;
        x_Settle();
        return *this;
    }

    // get currently selected gi
    TGi GetGi(void) const {
        return m_CurrGi;
    }

    // get currently selected gi type, eNuc or eProt
    ESeqType GetSeqType(void) const {
        return m_CurrSeqType;
    }

    // get currently selected gi row id in corresponding nuc or prot table
    uint64_t GetRowId(void) const {
        return m_CurrRowId;
    }

protected:
    void x_Init(const CWGSDb& wgs_db, ESeqType seq_type);

    CWGSDb_Impl& GetDb(void) const {
        return m_Db.GetNCObject();
    }

    void x_Settle(void);
    bool x_Excluded(void);
    
private:
    CWGSDb m_Db;
    CRef<CWGSDb_Impl::SIdxTableCursor> m_Idx; // VDB GI index table accessor
    TGi m_CurrGi, m_FirstBadGi;
    uint64_t m_CurrRowId;
    ESeqType m_CurrSeqType, m_FilterSeqType;
};


class NCBI_SRAREAD_EXPORT CWGSProteinIterator
{
public:
    CWGSProteinIterator(void);
    explicit CWGSProteinIterator(const CWGSDb& wgs_db);
    CWGSProteinIterator(const CWGSDb& wgs_db, uint64_t row);
    CWGSProteinIterator(const CWGSDb& wgs_db, CTempString acc);
    ~CWGSProteinIterator(void);

    DECLARE_SAFE_BOOL_METHOD(m_CurrId < m_FirstBadId);

    CWGSProteinIterator& operator++(void) {
        ++m_CurrId;
        return *this;
    }

    uint64_t GetCurrentRowId(void) const {
        return m_CurrId;
    }
    uint64_t GetFirstBadRowId(void) const {
        return m_FirstBadId;
    }
    uint64_t GetLastRowId(void) const {
        return GetFirstBadRowId() - 1;
    }

    CTempString GetAccession(void) const;
    int GetAccVersion(void) const;
    CRef<CSeq_id> GetAccSeq_id(void) const;
    CRef<CSeq_id> GetGeneralSeq_id(void) const;

    CTempString GetProteinName(void) const;

    void GetIds(CBioseq::TId& ids) const;
    bool HasRefGi(void) const;
    CSeq_id::TGi GetRefGi(void) const;
    bool HasRefAcc(void) const;
    CTempString GetRefAcc(void) const;

    TSeqPos GetSeqLength(void) const;

    bool HasSeq_descr(void) const;
    CRef<CSeq_descr> GetSeq_descr(void) const;

    bool HasAnnotSet(void) const;
    typedef CBioseq::TAnnot TAnnotSet;
    void GetAnnotSet(TAnnotSet& annot_set) const;

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
    CRef<CWGSDb_Impl::SProtTableCursor> m_Prot; // VDB scaffold table accessor
    uint64_t m_CurrId, m_FirstBadId;
};


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // SRA__READER__SRA__WGSREAD__HPP
