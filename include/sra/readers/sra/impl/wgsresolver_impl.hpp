#ifndef SRA__READER__SRA__WGSRESOLVER_IMPL__HPP
#define SRA__READER__SRA__WGSRESOLVER_IMPL__HPP
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
 *   Resolve WGS accessions
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>
#include <sra/readers/sra/wgsresolver.hpp>
#include <sra/readers/sra/vdbread.hpp>
#include <util/rangemap.hpp>

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(objects);

class CSeq_id;
class CTextseq_id;
class CDbtag;
class CID2_Reply;
class CID2Client;
class CDataLoader;

class CWGSResolver_VDB : public CWGSResolver
{
public:
    explicit CWGSResolver_VDB(const CVDBMgr& mgr);
    CWGSResolver_VDB(const CVDBMgr& mgr, const string& path);
    ~CWGSResolver_VDB(void);

    static CRef<CWGSResolver> CreateResolver(const CVDBMgr& mgr);
    
    static string GetDefaultWGSIndexPath(void);

    void Open(const CVDBMgr& mgr, const string& path);
    void Reopen(void);
    void Close(void);
    
    const string& GetWGSIndexPath(void) const {
        return m_WGSIndexPath;
    }
    
    bool IsValid(void) const {
        return m_Db;
    }

    const CTime& GetTimestamp(void) const {
        return m_Timestamp;
    }

    // return all WGS accessions that could contain gi or accession
    virtual TWGSPrefixes GetPrefixes(TGi gi);
    virtual TWGSPrefixes GetPrefixes(const string& acc);

    // force update of indexes from files
    virtual bool Update(void);

protected:
    // helper accessor structures for index tables
    struct SGiIdxTableCursor;
    struct SAccIdxTableCursor;

    const CVDBTable& GiIdxTable(void) {
        return m_GiIdxTable;
    }
    const CVDBTable& AccIdxTable(void) {
        return m_AccIdxTable;
    }

    // get table accessor object for exclusive access
    CRef<SGiIdxTableCursor> GiIdx(TIntId gi = 0);
    CRef<SAccIdxTableCursor> AccIdx(void);
    // return table accessor object for reuse
    void Put(CRef<SGiIdxTableCursor>& curs, TIntId gi = 0);
    void Put(CRef<SAccIdxTableCursor>& curs);
    
private:
    CVDBMgr m_Mgr;
    CMutex m_Mutex; // for update
    string m_WGSIndexPath;
    CTime m_Timestamp;
    CVDB m_Db;
    CVDBTable m_GiIdxTable;
    CVDBTable m_AccIdxTable;
    CVDBObjectCache<SGiIdxTableCursor> m_GiIdx;
    CVDBObjectCache<SAccIdxTableCursor> m_AccIdx;
};


class CWGSResolver_Ids : public CWGSResolver
{
public:
    CWGSResolver_Ids(void);
    ~CWGSResolver_Ids(void);
    
    // return all WGS accessions that could contain gi or accession
    virtual TWGSPrefixes GetPrefixes(TGi gi);
    virtual TWGSPrefixes GetPrefixes(const string& acc);

protected:
    string ParseWGSAcc(const string& acc, bool protein) const;
    string ParseWGSPrefix(const CDbtag& dbtag) const;
    string ParseWGSPrefix(const CTextseq_id& text_id) const;
    string ParseWGSPrefix(const CSeq_id& id) const;

    virtual TWGSPrefixes GetPrefixes(const CSeq_id& seq_id) = 0;
};


class CWGSResolver_DL : public CWGSResolver_Ids
{
public:
    CWGSResolver_DL(void); // find GenBank loader
    explicit
    CWGSResolver_DL(CDataLoader* loader);
    ~CWGSResolver_DL(void);
    
    static CRef<CWGSResolver> CreateResolver(void); // find GenBank loader
    static CRef<CWGSResolver> CreateResolver(CDataLoader* loader);
    
    bool IsValid(void) const {
        return m_Loader;
    }

protected:
    virtual TWGSPrefixes GetPrefixes(const CSeq_id& seq_id);

    CRef<CDataLoader> m_Loader;
};


class CWGSResolver_Proc : public CWGSResolver_Ids
{
public:
    explicit
    CWGSResolver_Proc(CID2ProcessorResolver* resolver);
    ~CWGSResolver_Proc(void);
    
    static CRef<CWGSResolver> CreateResolver(CID2ProcessorResolver* resolver);
    
    bool IsValid(void) const {
        return m_Resolver;
    }

protected:
    virtual TWGSPrefixes GetPrefixes(const CSeq_id& seq_id);

    CRef<CID2ProcessorResolver> m_Resolver;
};


//#define WGS_RESOLVER_USE_ID2_CLIENT

#ifdef WGS_RESOLVER_USE_ID2_CLIENT
class CWGSResolver_ID2 : public CWGSResolver_Ids
{
public:
    CWGSResolver_ID2(void);
    ~CWGSResolver_ID2(void);
    
    static CRef<CWGSResolver> CreateResolver(void);
    
    bool IsValid(void) const {
        return m_ID2Client;
    }

    // force update of indexes from files
    virtual bool Update(void);

protected:
    string ParseWGSPrefix(const CID2_Reply& reply) const;

    virtual TWGSPrefixes GetPrefixes(const CSeq_id& seq_id);

    CMutex m_Mutex; // for cache
    typedef map<string, string> TCache;
    TCache m_Cache;
    CRef<CID2Client> m_ID2Client;
};
#endif


class NCBI_SRAREAD_EXPORT CWGSResolver_GiRangeFile : public CObject
{
public:
    CWGSResolver_GiRangeFile(void);
    explicit CWGSResolver_GiRangeFile(const string& index_path);
    ~CWGSResolver_GiRangeFile(void);

    static string GetDefaultIndexPath(void);

    const string& GetIndexPath(void) const {
        return m_IndexPath;
    }

    bool IsValid(void) const;

    const CTime& GetTimestamp(void) const {
        return m_Index.m_Timestamp;
    }

    // return all WGS accessions that could contain gi
    typedef CWGSResolver::TWGSPrefixes TWGSPrefixes;
    TWGSPrefixes GetPrefixes(TGi gi) const;

    void SetNonWGS(TGi gi);

    struct SSeqInfo {
        string wgs_acc;
        char type;
        TVDBRowId row;
    };
    typedef vector<SSeqInfo> TSeqInfoList;
    TSeqInfoList FindAll(TGi gi, const CVDBMgr& mgr) const;

    // return unordered list of WGS accessions and GI ranges
    typedef pair<TIntId, TIntId> TIdRange;
    typedef pair<string, TIdRange> TIdRangePair;
    typedef vector<TIdRangePair> TIdRanges;
    TIdRanges GetIdRanges(void) const;

    struct SWGSAccession {
        enum {
            kMinWGSAccessionLength = 6, // AAAA01
            kMaxWGSAccessionLength = 9  // AAAA01.11
        };

        SWGSAccession(void) {
            acc[0] = '\0';
        }
        explicit SWGSAccession(CTempString str) {
            size_t len = str.size();
            for ( size_t i = 0; i < len; ++i ) {
                acc[i] = str[i];
            }
            acc[len] = '\0';
        }

        char acc[kMaxWGSAccessionLength+1];
    };

    void LoadFirst(const string& index_path);
    bool Update(void);

private:
    typedef CRangeMultimap<SWGSAccession, TIntId> TIndex;
    struct SIndexInfo
    {
        TIndex m_Index;
        CTime  m_Timestamp;
    };

    bool x_Load(SIndexInfo& index, const CTime* old_timestamp = 0) const;

    string m_IndexPath;
    mutable CMutex m_Mutex;
    SIndexInfo m_Index;
    set<TGi> m_NonWGS;
};


class NCBI_SRAREAD_EXPORT CWGSResolver_AccRangeFile : public CObject
{
public:
    CWGSResolver_AccRangeFile(void);
    explicit CWGSResolver_AccRangeFile(const string& index_path);
    ~CWGSResolver_AccRangeFile(void);

    static string GetDefaultIndexPath(void);

    const string& GetIndexPath(void) const {
        return m_IndexPath;
    }

    bool IsValid(void) const;

    const CTime& GetTimestamp(void) const {
        return m_Index.m_Timestamp;
    }

    struct NCBI_SRAREAD_EXPORT SAccInfo
    {
        SAccInfo(void)
            : m_IdLength(0)
            {
            }
        SAccInfo(CTempString acc, Uint4& id);
        
        string GetAcc(Uint4 id) const;

        DECLARE_OPERATOR_BOOL(m_IdLength != 0);

        bool operator<(const SAccInfo& b) const {
            if ( m_IdLength != b.m_IdLength ) {
                return m_IdLength < b.m_IdLength;
            }
            return m_AccPrefix < b.m_AccPrefix;
        }

        bool operator==(const SAccInfo& b) const {
            return m_IdLength == b.m_IdLength &&
                m_AccPrefix == b.m_AccPrefix;
        }
        bool operator!=(const SAccInfo& b) const {
            return !(*this == b);
        }

        string m_AccPrefix;
        Uint4  m_IdLength;
    };

    // return all WGS accessions that could contain protein accession
    typedef CWGSResolver::TWGSPrefixes TWGSPrefixes;
    TWGSPrefixes GetPrefixes(const string& acc) const;

    void SetNonWGS(const string& acc);

    // return unordered list of WGS accessions and GI ranges
    typedef pair<string, string> TIdRange;
    typedef pair<string, TIdRange> TIdRangePair;
    typedef vector<TIdRangePair> TIdRanges;
    TIdRanges GetIdRanges(void) const;

    typedef CWGSResolver_GiRangeFile::SWGSAccession SWGSAccession;

    void LoadFirst(const string& index_path);
    bool Update(void);

private:
    typedef CRangeMultimap<SWGSAccession, Uint4> TRangeIndex;
    typedef map<SAccInfo, TRangeIndex> TIndex; // by acc prefix
    struct SIndexInfo
    {
        TIndex m_Index;
        CTime  m_Timestamp;
    };

    bool x_Load(SIndexInfo& index, const CTime* old_timestamp = 0) const;

    string m_IndexPath;
    mutable CMutex m_Mutex;
    SIndexInfo m_Index;
    set<string> m_NonWGS;
};

    
class CWGSResolver_RangeFiles : public CWGSResolver
{
public:
    CWGSResolver_RangeFiles(void);
    ~CWGSResolver_RangeFiles(void);

    static CRef<CWGSResolver> CreateResolver(void);
    
    // return all WGS accessions that could contain gi or accession
    virtual TWGSPrefixes GetPrefixes(TGi gi);
    virtual TWGSPrefixes GetPrefixes(const string& acc);

    // remember that the id finally happened to be non-WGS
    // the info can be used to return empty candidates set in the future
    virtual void SetNonWGS(TGi gi,
                           const TWGSPrefixes& prefixes);
    virtual void SetNonWGS(const string& acc,
                           const TWGSPrefixes& prefixes);

    // force update of indexes from files
    virtual bool Update(void);

protected:
    CRef<CWGSResolver_GiRangeFile> m_GiResolver;
    CRef<CWGSResolver_AccRangeFile> m_AccResolver;
};


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // SRA__READER__SRA__WGSRESOLVER_IMPL__HPP
