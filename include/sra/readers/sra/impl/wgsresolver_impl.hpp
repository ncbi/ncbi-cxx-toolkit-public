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

class NCBI_SRAREAD_EXPORT CWGSResolver_VDB : public CWGSResolver
{
public:
    enum EIndexType {
        eMainIndex,
        eSecondIndex,
        eThirdIndex
    };
    explicit CWGSResolver_VDB(const CVDBMgr& mgr,
                              EIndexType index_type = eMainIndex,
                              CWGSResolver_VDB* next_resolver = 0);
    CWGSResolver_VDB(const CVDBMgr& mgr,
                     const string& path,
                     CWGSResolver_VDB* next_resolver = 0);
    ~CWGSResolver_VDB(void);

    static CRef<CWGSResolver> CreateResolver(const CVDBMgr& mgr);

    // default path to main index
    static string GetDefaultWGSIndexPath(EIndexType index_type = eMainIndex);
    static string GetDefaultWGSIndexAcc(EIndexType index_type = eMainIndex);
    
    void Open(const CVDBMgr& mgr, const string& path);
    void Reopen(void);
    void Close(void);
    
    const string& GetWGSIndexPath(void) const {
        return m_WGSIndexPath;
    }
    const string& GetWGSIndexResolvedPath(void) const {
        return m_WGSIndexResolvedPath;
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

    void x_Close(); // unguarded
    bool x_Update();
    
private:
    CVDBMgr m_Mgr;
    typedef CRWLock TDBMutex;
    TDBMutex m_DBMutex; // for update
    string m_WGSIndexPath;
    string m_WGSIndexResolvedPath;
    CTime m_Timestamp;
    CVDB m_Db;
    CVDBTable m_GiIdxTable;
    CVDBTable m_AccIdxTable;
    CVDBTableIndex m_AccIndex;
    bool m_AccIndexIsPrefix;
    CVDBObjectCache<SGiIdxTableCursor> m_GiIdxCursorCache;
    CVDBObjectCache<SAccIdxTableCursor> m_AccIdxCursorCache;
    CRef<CWGSResolver_VDB> m_NextResolver;
};


class NCBI_SRAREAD_EXPORT CWGSResolver_Ids : public CWGSResolver
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


class NCBI_SRAREAD_EXPORT CWGSResolver_DL : public CWGSResolver_Ids
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


class NCBI_SRAREAD_EXPORT CWGSResolver_Proc : public CWGSResolver_Ids
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
class NCBI_SRAREAD_EXPORT CWGSResolver_ID2 : public CWGSResolver_Ids
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


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // SRA__READER__SRA__WGSRESOLVER_IMPL__HPP
