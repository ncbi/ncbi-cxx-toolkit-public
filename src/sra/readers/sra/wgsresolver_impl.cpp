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

#include <ncbi_pch.hpp>
#include <sra/readers/sra/impl/wgsresolver_impl.hpp>
#include <sra/readers/ncbi_traces_path.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_param.hpp>
#include <util/line_reader.hpp>
#include <sra/error_codes.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/data_loader.hpp>

#include <objects/id2/id2processor.hpp>

#ifdef WGS_RESOLVER_USE_ID2_CLIENT
# include <objects/id2/id2__.hpp>
# include <objects/id2/id2_client.hpp>
#endif

BEGIN_NCBI_NAMESPACE;

#define NCBI_USE_ERRCODE_X   WGSResolver
NCBI_DEFINE_ERR_SUBCODE_X(27);

BEGIN_NAMESPACE(objects);


//#define DEFAULT_WGS_INDEX_PATH "WGS_INDEX"
//#define DEFAULT_WGS_INDEX_PATH "AAAA00"
#define DEFAULT_WGS_INDEX_PATH1 NCBI_TRACES04_PATH "/wgs01/WGS/WGS_INDEX"
#define DEFAULT_WGS_INDEX_PATH2 NCBI_TRACES04_PATH "/wgs01/NEW/WGS/WGS_INDEX"

#define DEFAULT_GI_INDEX_PATH                                   \
    NCBI_TRACES04_PATH "/wgs01/wgs_aux/list.wgs_gi_ranges"

#define DEFAULT_PROT_ACC_INDEX_PATH                             \
    NCBI_TRACES04_PATH "/wgs01/wgs_aux/list.wgs_acc_ranges"


NCBI_PARAM_DECL(string, WGS, WGS_INDEX);
NCBI_PARAM_DEF(string, WGS, WGS_INDEX, "");


NCBI_PARAM_DECL(string, WGS, GI_INDEX);
NCBI_PARAM_DEF(string, WGS, GI_INDEX, DEFAULT_GI_INDEX_PATH);


NCBI_PARAM_DECL(string, WGS, PROT_ACC_INDEX);
NCBI_PARAM_DEF(string, WGS, PROT_ACC_INDEX, DEFAULT_PROT_ACC_INDEX_PATH);


// debug levels
enum EDebugLevel {
    eDebug_none     = 0,
    eDebug_error    = 1,
    eDebug_open     = 2,
    eDebug_request  = 5,
    eDebug_replies  = 6,
    eDebug_resolve  = 7,
    eDebug_data     = 8,
    eDebug_all      = 9
};

NCBI_PARAM_DECL(int, WGS, DEBUG_RESOLVE);
NCBI_PARAM_DEF_EX(int, WGS, DEBUG_RESOLVE, eDebug_error,
                  eParam_NoThread, WGS_DEBUG_RESOLVE);

static inline int s_DebugLevel(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(WGS, DEBUG_RESOLVE)> s_Value;
    return s_Value->Get();
}


static inline bool s_DebugEnabled(EDebugLevel level)
{
    return s_DebugLevel() >= level;
}


/////////////////////////////////////////////////////////////////////////////
// CWGSResolver_VDB
/////////////////////////////////////////////////////////////////////////////


// SGiIdxTableCursor is helper accessor structure for optional GI_IDX table
struct CWGSResolver_VDB::SGiIdxTableCursor : public CObject {
    explicit SGiIdxTableCursor(const CVDBTable& table);

    CVDBCursor m_Cursor;

    DECLARE_VDB_COLUMN_AS_STRING(WGS_PREFIX);
};


CWGSResolver_VDB::SGiIdxTableCursor::SGiIdxTableCursor(const CVDBTable& table)
    : m_Cursor(table),
      INIT_VDB_COLUMN(WGS_PREFIX)
{
}


// SAccIdxTableCursor is helper accessor structure for optional ACC_IDX table
struct CWGSResolver_VDB::SAccIdxTableCursor : public CObject {
    explicit SAccIdxTableCursor(const CVDBTable& table);

    CVDBCursor m_Cursor;

    typedef pair<TVDBRowId, TVDBRowId> row_range_t;
    DECLARE_VDB_COLUMN_AS(row_range_t, ACCESSION_ROW_RANGE);
    DECLARE_VDB_COLUMN_AS_STRING(WGS_PREFIX);
};


CWGSResolver_VDB::SAccIdxTableCursor::SAccIdxTableCursor(const CVDBTable& table)
    : m_Cursor(table),
      INIT_VDB_COLUMN(ACCESSION_ROW_RANGE),
      INIT_VDB_COLUMN(WGS_PREFIX)
{
}


CWGSResolver_VDB::CWGSResolver_VDB(const CVDBMgr& mgr)
{
    Open(mgr, GetDefaultWGSIndexPath());
}


CWGSResolver_VDB::CWGSResolver_VDB(const CVDBMgr& mgr, const string& path)
{
    Open(mgr, path);
}


CWGSResolver_VDB::~CWGSResolver_VDB(void)
{
    Close();
}


CRef<CWGSResolver> CWGSResolver_VDB::CreateResolver(const CVDBMgr& mgr)
{
    string path = GetDefaultWGSIndexPath();
    if ( path.empty() ) {
        return null;
    }
    if ( !CDirEntry(path).Exists() ) {
        if ( s_DebugEnabled(eDebug_error) ) {
            ERR_POST_X(9, "CWGSResolver_VDB: cannot find index file: "<<path);
        }
        return null;
    }
    CRef<CWGSResolver_VDB> ret(new CWGSResolver_VDB(mgr, path));
    if ( !ret->IsValid() ) {
        return null;
    }
    return CRef<CWGSResolver>(ret);
}


void CWGSResolver_VDB::Close(void)
{
    CMutexGuard guard(m_Mutex);
    m_Mgr.Close();
    m_Db.Close();
    m_GiIdxTable.Close();
    m_AccIdxTable.Close();
    m_GiIdx.Clear();
    m_AccIdx.Clear();
}


void CWGSResolver_VDB::Open(const CVDBMgr& mgr, const string& path)
{
    CMutexGuard guard(m_Mutex);
    Close();
    m_Mgr = mgr;
    try {
        m_Db = CVDB(m_Mgr, path);
    }
    catch ( CSraException& exc ) {
        if ( exc.GetErrCode() == exc.eNotFoundDb ) {
            return;
        }
        throw;
    }
    m_WGSIndexPath = path;
    if ( !CDirEntry(path).GetTime(&m_Timestamp) ) {
        m_Timestamp = CTime();
    }
    m_GiIdxTable = CVDBTable(m_Db, "GI_IDX");
    m_AccIdxTable = CVDBTable(m_Db, "ACC_IDX");
}


void CWGSResolver_VDB::Reopen(void)
{
    if ( CVDBMgr mgr = m_Mgr ) {
        string path = GetWGSIndexPath();
        Open(mgr, path);
    }
}


bool CWGSResolver_VDB::Update(void)
{
    CTime timestamp;
    if ( !CDirEntry(GetWGSIndexPath()).GetTime(&timestamp) ) {
        return false;
    }
    if ( timestamp == m_Timestamp ) {
        // same timestamp
        return false;
    }
    Reopen();
    return true;
}


string CWGSResolver_VDB::GetDefaultWGSIndexPath(void)
{
    string path = NCBI_PARAM_TYPE(WGS, WGS_INDEX)::GetDefault();
    if ( path.empty() ) {
        if ( CDirEntry(DEFAULT_WGS_INDEX_PATH1).Exists() ) {
            path = DEFAULT_WGS_INDEX_PATH1;
        }
        else if ( CDirEntry(DEFAULT_WGS_INDEX_PATH2).Exists() ) {
            path = DEFAULT_WGS_INDEX_PATH2;
        }
    }
    return path;
}


inline
CRef<CWGSResolver_VDB::SGiIdxTableCursor> CWGSResolver_VDB::GiIdx(TIntId row)
{
    CRef<SGiIdxTableCursor> curs = m_GiIdx.Get(row);
    if ( !curs ) {
        CMutexGuard guard(m_Mutex);
        curs = new SGiIdxTableCursor(GiIdxTable());
    }
    return curs;
}


inline
CRef<CWGSResolver_VDB::SAccIdxTableCursor> CWGSResolver_VDB::AccIdx(void)
{
    CRef<SAccIdxTableCursor> curs = m_AccIdx.Get();
    if ( !curs ) {
        CMutexGuard guard(m_Mutex);
        curs = new SAccIdxTableCursor(AccIdxTable());
    }
    return curs;
}


inline
void CWGSResolver_VDB::Put(CRef<SGiIdxTableCursor>& curs, TIntId row)
{
    m_GiIdx.Put(curs, row);
}


inline
void CWGSResolver_VDB::Put(CRef<SAccIdxTableCursor>& curs)
{
    m_AccIdx.Put(curs);
}


CWGSResolver::TWGSPrefixes CWGSResolver_VDB::GetPrefixes(TGi gi)
{
    TWGSPrefixes ret;
    if ( s_DebugEnabled(eDebug_resolve) ) {
        ERR_POST_X(24, "CWGSResolver_VDB: Resolving "<<gi);
    }
    CRef<SGiIdxTableCursor> cur = GiIdx();
    CVDBStringValue value = cur->WGS_PREFIX(gi, CVDBValue::eMissing_Allow);
    if ( !value.empty() ) {
        if ( s_DebugEnabled(eDebug_resolve) ) {
            ERR_POST_X(25, "CWGSResolver_VDB: WGS prefix "<<*value);
        }
        ret.push_back(*value);
    }
    Put(cur);
    return ret;
}


CWGSResolver::TWGSPrefixes CWGSResolver_VDB::GetPrefixes(const string& acc)
{
    TWGSPrefixes ret;
    if ( s_DebugEnabled(eDebug_resolve) ) {
        ERR_POST_X(26, "CWGSResolver_VDB: Resolving "<<acc);
    }
    string uacc = acc;
    NStr::ToUpper(uacc);
    CRef<SAccIdxTableCursor> cur = AccIdx();
    SAccIdxTableCursor::row_range_t range;
    cur->m_Cursor.SetParam("ACCESSION_QUERY", uacc);
    CVDBValueFor<SAccIdxTableCursor::row_range_t> value =
        cur->ACCESSION_ROW_RANGE(1, CVDBValue::eMissing_Allow);
    if ( !value.empty() ) {
        range = *value;
    }
    if ( range.first ) {
        for ( TVDBRowId i = range.first; i <= range.second; ++i ) {
            CTempString prefix = *cur->WGS_PREFIX(i);
            if ( s_DebugEnabled(eDebug_resolve) ) {
                ERR_POST_X(27, "CWGSResolver_VDB: WGS prefix "<<prefix);
            }
            ret.push_back(prefix);
        }
    }
    Put(cur);
    return ret;
}


/////////////////////////////////////////////////////////////////////////////
// CWGSResolver_GiRangeFile
/////////////////////////////////////////////////////////////////////////////


string CWGSResolver_GiRangeFile::GetDefaultIndexPath(void)
{
    return NCBI_PARAM_TYPE(WGS, GI_INDEX)::GetDefault();
}


CWGSResolver_GiRangeFile::CWGSResolver_GiRangeFile(void)
{
    LoadFirst(GetDefaultIndexPath());
}


CWGSResolver_GiRangeFile::CWGSResolver_GiRangeFile(const string& index_path)
{
    LoadFirst(index_path);
}


CWGSResolver_GiRangeFile::~CWGSResolver_GiRangeFile(void)
{
}


bool CWGSResolver_GiRangeFile::IsValid(void) const
{
    CMutexGuard guard(m_Mutex);
    return !m_Index.m_Index.empty();
}


void CWGSResolver_GiRangeFile::LoadFirst(const string& index_path)
{
    CMutexGuard guard(m_Mutex);
    m_IndexPath = index_path;
    if ( !x_Load(m_Index) ) {
        m_Index.m_Index.clear();
    }
}


bool CWGSResolver_GiRangeFile::Update(void)
{
    SIndexInfo index;
    if ( !x_Load(index, &m_Index.m_Timestamp) ) {
        return false;
    }
    CMutexGuard guard(m_Mutex);
    index.m_Index.swap(m_Index.m_Index);
    swap(index.m_Timestamp, m_Index.m_Timestamp);
    m_NonWGS.clear();
    return true;
}


bool CWGSResolver_GiRangeFile::x_Load(SIndexInfo& index,
                            const CTime* old_timestamp) const
{
    if ( !CDirEntry(m_IndexPath).GetTime(&index.m_Timestamp) ) {
        // failed to get timestamp
        return false;
    }
    if ( old_timestamp && index.m_Timestamp == *old_timestamp ) {
        // same timestamp
        return false;
    }

    CMemoryFile stream(m_IndexPath);
    if ( !stream.GetPtr() ) {
        // no index file
        return false;
    }

    index.m_Index.clear();
    vector<CTempString> tokens;
    size_t count = 0;
    for ( CMemoryLineReader line(&stream); !line.AtEOF(); ) {
        tokens.clear();
        NStr::Tokenize(*++line, " ", tokens, NStr::eMergeDelims);
        if ( tokens.size() != 3 ) {
            if ( tokens.empty() || tokens[0][0] == '#' ) {
                // allow empty lines and comments starting with #
                continue;
            }
            if ( s_DebugEnabled(eDebug_error) ) {
                ERR_POST_X(1, "CWGSResolver_GiRangeFile: "
                           "bad index file format: "<<
                           m_IndexPath<<":"<<line.GetLineNumber()<<": "<<
                           *line);
            }
            return false;
        }
        CTempString wgs_acc = tokens[0];
        if ( wgs_acc.size() < SWGSAccession::kMinWGSAccessionLength ||
             wgs_acc.size() > SWGSAccession::kMaxWGSAccessionLength ) {
            if ( s_DebugEnabled(eDebug_error) ) {
                ERR_POST_X(2, "CWGSResolver_GiRangeFile: "
                           "bad wgs accession length: "<<
                           m_IndexPath<<":"<<line.GetLineNumber()<<": "<<
                           *line);
            }
            return false;
        }
        TIntId id1 =
            NStr::StringToNumeric<TIntId>(tokens[1], NStr::fConvErr_NoThrow);
        TIntId id2 =
            NStr::StringToNumeric<TIntId>(tokens[2], NStr::fConvErr_NoThrow);
        if ( id1 <= 0 || id1 > id2 ) {
            if ( s_DebugEnabled(eDebug_error) ) {
                ERR_POST_X(3, "CWGSResolver_GiRangeFile: "
                           "bad gi range: "<<
                           m_IndexPath<<":"<<line.GetLineNumber()<<": "<<
                           *line);
            }
            return false;
        }
        index.m_Index
            .insert(TIndex::value_type(CRange<TIntId>(id1, id2),
                                       SWGSAccession(wgs_acc)));
        ++count;
    }
    return true;
}


void CWGSResolver_GiRangeFile::SetNonWGS(TGi gi)
{
    CMutexGuard guard(m_Mutex);
    m_NonWGS.insert(gi);
}


CWGSResolver::TWGSPrefixes
CWGSResolver_GiRangeFile::GetPrefixes(TGi gi) const
{
    TWGSPrefixes ret;
    CMutexGuard guard(m_Mutex);
    if ( m_NonWGS.find(gi) != m_NonWGS.end() ) {
        return ret;
    }
    if ( s_DebugEnabled(eDebug_resolve) ) {
        ERR_POST_X(19, "CWGSResolver_GiRangeFile: Resolving "<<gi);
    }
    for ( TIndex::const_iterator it(m_Index.m_Index.begin(CRange<TIntId>(gi, gi))); it; ++it ) {
        if ( s_DebugEnabled(eDebug_resolve) ) {
            ERR_POST_X(20, "CWGSResolver_GiRangeFile: "
                       "WGS prefix: "<<it->second.acc);
        }
        ret.push_back(it->second.acc);
    }
    return ret;
}


CWGSResolver_GiRangeFile::TIdRanges
CWGSResolver_GiRangeFile::GetIdRanges(void) const
{
    TIdRanges ret;
    CMutexGuard guard(m_Mutex);
    ITERATE ( TIndex, it, m_Index.m_Index ) {
        TIdRange range(it->first.GetFrom(), it->first.GetTo());
        ret.push_back(TIdRangePair(it->second.acc, range));
    }
    return ret;
}


/////////////////////////////////////////////////////////////////////////////
// CWGSResolver_AccRangeFile
/////////////////////////////////////////////////////////////////////////////


string CWGSResolver_AccRangeFile::GetDefaultIndexPath(void)
{
    return NCBI_PARAM_TYPE(WGS, PROT_ACC_INDEX)::GetDefault();
}


CWGSResolver_AccRangeFile::CWGSResolver_AccRangeFile(void)
{
    LoadFirst(GetDefaultIndexPath());
}


CWGSResolver_AccRangeFile::CWGSResolver_AccRangeFile(const string& index_path)
{
    LoadFirst(index_path);
}


CWGSResolver_AccRangeFile::~CWGSResolver_AccRangeFile(void)
{
}


bool CWGSResolver_AccRangeFile::IsValid(void) const
{
    CMutexGuard guard(m_Mutex);
    return !m_Index.m_Index.empty();
}


void CWGSResolver_AccRangeFile::LoadFirst(const string& index_path)
{
    CMutexGuard guard(m_Mutex);
    m_IndexPath = index_path;
    if ( !x_Load(m_Index) ) {
        m_Index.m_Index.clear();
    }
}


bool CWGSResolver_AccRangeFile::Update(void)
{
    SIndexInfo index;
    if ( !x_Load(index, &m_Index.m_Timestamp) ) {
        return false;
    }
    CMutexGuard guard(m_Mutex);
    index.m_Index.swap(m_Index.m_Index);
    swap(index.m_Timestamp, m_Index.m_Timestamp);
    m_NonWGS.clear();
    return true;
}


bool CWGSResolver_AccRangeFile::x_Load(SIndexInfo& index,
                                 const CTime* old_timestamp) const
{
    if ( !CDirEntry(m_IndexPath).GetTime(&index.m_Timestamp) ) {
        // failed to get timestamp
        return false;
    }
    if ( old_timestamp && index.m_Timestamp == *old_timestamp ) {
        // same timestamp
        return false;
    }

    CMemoryFile stream(m_IndexPath);
    if ( !stream.GetPtr() ) {
        // no index file
        return false;
    }

    vector<CTempString> tokens;
    size_t count = 0;
    for ( CMemoryLineReader line(&stream); !line.AtEOF(); ) {
        tokens.clear();
        NStr::Tokenize(*++line, " ", tokens, NStr::eMergeDelims);
        if ( tokens.size() != 3 ) {
            if ( tokens.empty() || tokens[0][0] == '#' ) {
                // allow empty lines and comments starting with #
                continue;
            }
            if ( s_DebugEnabled(eDebug_error) ) {
                ERR_POST_X(5, "CWGSResolver_AccRangeFile: "
                           "bad index file format: "<<
                           m_IndexPath<<":"<<line.GetLineNumber()<<": "<<
                           *line);
            }
            return false;
        }
        CTempString wgs_acc = tokens[0];
        if ( wgs_acc.size() < SWGSAccession::kMinWGSAccessionLength ||
             wgs_acc.size() > SWGSAccession::kMaxWGSAccessionLength ) {
            if ( s_DebugEnabled(eDebug_error) ) {
                ERR_POST_X(6, "CWGSResolver_AccRangeFile: "
                           "bad wgs accession length: "<<
                           m_IndexPath<<":"<<line.GetLineNumber()<<": "<<
                           *line);
            }
            return false;
        }
        Uint4 id1;
        SAccInfo info1(tokens[1], id1);
        Uint4 id2;
        SAccInfo info2(tokens[2], id2);
        if ( !info1 || info1 != info2 || id1 > id2 ) {
            if ( s_DebugEnabled(eDebug_error) ) {
                ERR_POST_X(7, "CWGSResolver_AccRangeFile: "
                           "bad accession range: "<<
                           m_IndexPath<<":"<<line.GetLineNumber()<<": "<<
                           *line);
            }
            return false;
        }
        index.m_Index[info1]
            .insert(TRangeIndex::value_type(CRange<Uint4>(id1, id2),
                                            SWGSAccession(wgs_acc)));
        ++count;
    }
    return true;
}


CWGSResolver_AccRangeFile::SAccInfo::SAccInfo(CTempString acc, Uint4& id)
    : m_IdLength(0)
{
    SIZE_TYPE prefix = 0;
    while ( prefix < acc.size() && isalpha(acc[prefix]&0xff) ) {
        ++prefix;
    }
    if ( prefix == acc.size() || prefix == 0 || acc.size()-prefix > 9 ) {
        // no prefix, or no digits, or too many digits
        return;
    }
    Uint4 v = 0;
    for ( SIZE_TYPE i = prefix; i < acc.size(); ++i ) {
        char c = acc[i];
        if ( c < '0' || c > '9' ) {
            return;
        }
        v = v*10 + (c-'0');
    }
    id = v;
    m_AccPrefix = acc.substr(0, prefix);
    NStr::ToUpper(m_AccPrefix);
    m_IdLength = Uint4(acc.size());
}


string CWGSResolver_AccRangeFile::SAccInfo::GetAcc(Uint4 id) const
{
    string acc = m_AccPrefix;
    acc.resize(m_IdLength, '0');
    for ( SIZE_TYPE i = m_IdLength; id; id /= 10 ) {
        acc[--i] += id % 10;
    }
    return acc;
}


void CWGSResolver_AccRangeFile::SetNonWGS(const string& acc)
{
    CMutexGuard guard(m_Mutex);
    m_NonWGS.insert(acc);
}


CWGSResolver::TWGSPrefixes
CWGSResolver_AccRangeFile::GetPrefixes(const string& acc) const
{
    TWGSPrefixes ret;
    Uint4 id;
    SAccInfo info(acc, id);
    if ( s_DebugEnabled(eDebug_resolve) ) {
        ERR_POST_X(21, "CWGSResolver_AccRangeFile: Resolving "<<acc);
    }
    if ( info ) {
        CMutexGuard guard(m_Mutex);
        if ( m_NonWGS.find(acc) != m_NonWGS.end() ) {
            return ret;
        }
        TIndex::const_iterator it = m_Index.m_Index.find(info);
        if ( it != m_Index.m_Index.end() ) {
            for ( TRangeIndex::const_iterator it2(it->second.begin(CRange<Uint4>(id, id))); it2; ++it2 ) {
                if ( s_DebugEnabled(eDebug_resolve) ) {
                    ERR_POST_X(22, "CWGSResolver_AccRangeFile: "
                               "WGS prefix: "<<it2->second.acc);
                }
                ret.push_back(it2->second.acc);
            }
        }
    }
    return ret;
}


CWGSResolver_AccRangeFile::TIdRanges
CWGSResolver_AccRangeFile::GetIdRanges(void) const
{
    TIdRanges ret;
    CMutexGuard guard(m_Mutex);
    ITERATE ( TIndex, it, m_Index.m_Index ) {
        ITERATE ( TRangeIndex, it2, it->second ) {
            TIdRange range(it->first.GetAcc(it2->first.GetFrom()),
                           it->first.GetAcc(it2->first.GetTo()));
            ret.push_back(TIdRangePair(it2->second.acc, range));
        }
    }
    return ret;
}


/////////////////////////////////////////////////////////////////////////////
// CWGSResolver_RangeFiles
/////////////////////////////////////////////////////////////////////////////


CWGSResolver_RangeFiles::CWGSResolver_RangeFiles(void)
    : m_GiResolver(new CWGSResolver_GiRangeFile),
      m_AccResolver(new CWGSResolver_AccRangeFile)
{
}


CWGSResolver_RangeFiles::~CWGSResolver_RangeFiles(void)
{
}


CRef<CWGSResolver> CWGSResolver_RangeFiles::CreateResolver(void)
{
    return CRef<CWGSResolver>(new CWGSResolver_RangeFiles);
}


CWGSResolver::TWGSPrefixes
CWGSResolver_RangeFiles::GetPrefixes(TGi gi)
{
    return m_GiResolver->GetPrefixes(gi);
}


CWGSResolver::TWGSPrefixes
CWGSResolver_RangeFiles::GetPrefixes(const string& acc)
{
    return m_AccResolver->GetPrefixes(acc);
}


void CWGSResolver_RangeFiles::SetNonWGS(TGi gi,
                                        const TWGSPrefixes& /*prefixes*/)
{
    m_GiResolver->SetNonWGS(gi);
}


void CWGSResolver_RangeFiles::SetNonWGS(const string& acc,
                                        const TWGSPrefixes& /*prefixes*/)
{
    m_AccResolver->SetNonWGS(acc);
}


bool CWGSResolver_RangeFiles::Update(void)
{
    bool ret = false;
    if ( m_GiResolver->Update() ) {
        ret = true;
    }
    if ( m_AccResolver->Update() ) {
        ret = true;
    }
    return ret;
}


/////////////////////////////////////////////////////////////////////////////
// CWGSResolver_Ids
/////////////////////////////////////////////////////////////////////////////


CWGSResolver_Ids::CWGSResolver_Ids(void)
{
}


CWGSResolver_Ids::~CWGSResolver_Ids(void)
{
}


string CWGSResolver_Ids::ParseWGSPrefix(const CDbtag& dbtag) const
{
    const string& db = dbtag.GetDb();
    if ( (db.size() != 8 && db.size() != 10) ||
         !NStr::StartsWith(db, "WGS:") ) {
        return string();
    }
    string prefix = db.substr(4);
    if ( prefix.size() == 4 ) {
        prefix += "01";
    }
    _ASSERT(prefix.size() == 6);
    for ( size_t i = 0; i < 4; ++i ) {
        if ( !isupper(Uint1(prefix[i])) ) {
            return string();
        }
    }
    for ( size_t i = 4; i < 6; ++i ) {
        if ( !isdigit(Uint1(prefix[i])) ) {
            return string();
        }
    }
    return prefix;
}


static const size_t kNumLetters = 4;
static const size_t kVersionDigits = 2;
static const size_t kPrefixLen = kNumLetters + kVersionDigits;
static const size_t kMinRowDigits = 6;
static const size_t kMaxRowDigits = 8;


string CWGSResolver_Ids::ParseWGSAcc(const string& acc, bool protein) const
{
    if ( acc.size() < kPrefixLen + kMinRowDigits ||
         acc.size() > kPrefixLen + kMaxRowDigits + 1 ) { // one for type letter
        return string();
    }
    for ( size_t i = 0; i < kNumLetters; ++i ) {
        if ( !isalpha(acc[i]&0xff) ) {
            return string();
        }
    }
    for ( size_t i = kNumLetters; i < kPrefixLen; ++i ) {
        if ( !isdigit(acc[i]&0xff) ) {
            return string();
        }
    }
    SIZE_TYPE row_pos = kPrefixLen;
    switch ( acc[row_pos] ) { // optional type letter
    case 'S':
        if ( protein ) {
            return string();
        }
        ++row_pos;
        break;
    case 'P':
        if ( !protein ) {
            return string();
        }
        ++row_pos;
        break;
    default:
        // it can be either contig or master sequence
        if ( protein ) {
            return string();
        }
        break;
    }
    for ( size_t i = row_pos; i < acc.size(); ++i ) {
        char c = acc[i];
        if ( c < '0' || c > '9' ) {
            return string();
        }
    }
    return acc.substr(0, kPrefixLen);
}


string CWGSResolver_Ids::ParseWGSPrefix(const CTextseq_id& text_id) const
{
    if ( text_id.IsSetName() ) {
        // first try name reference if it has WGS format like AAAA01P000001
        // as it directly contains WGS accession
        string wgs_acc = ParseWGSAcc(text_id.GetName(), true);
        if ( !wgs_acc.empty() ) {
            return wgs_acc;
        }
    }
    if ( text_id.IsSetAccession() ) {
        const string& acc = text_id.GetAccession();
        CSeq_id::EAccessionInfo type = CSeq_id::IdentifyAccession(acc);
        if ( !(type & CSeq_id::fAcc_prot) ) {
            switch ( type & CSeq_id::eAcc_division_mask ) {
                // accepted accession types
            case CSeq_id::eAcc_wgs:
            case CSeq_id::eAcc_wgs_intermed:
            case CSeq_id::eAcc_tsa:
                return ParseWGSAcc(acc, false);
            default:
                break;
            }
        }
    }
    return string();
}


string CWGSResolver_Ids::ParseWGSPrefix(const CSeq_id& id) const
{
    if ( id.IsGeneral() ) {
        return ParseWGSPrefix(id.GetGeneral());
    }
    else if ( const CTextseq_id* text_id = id.GetTextseq_Id() ) {
        return ParseWGSPrefix(*text_id);
    }
    return string();
}


CWGSResolver::TWGSPrefixes CWGSResolver_Ids::GetPrefixes(TGi gi)
{
    CSeq_id seq_id;
    seq_id.SetGi(gi);
    return GetPrefixes(seq_id);
}


CWGSResolver::TWGSPrefixes CWGSResolver_Ids::GetPrefixes(const string& acc)
{
    CSeq_id seq_id(acc);
    return GetPrefixes(seq_id);
}


/////////////////////////////////////////////////////////////////////////////
// CWGSResolver_DL
/////////////////////////////////////////////////////////////////////////////


CWGSResolver_DL::CWGSResolver_DL(void)
    : m_Loader(CObjectManager::GetInstance()->FindDataLoader("GBLOADER"))
{
    
}


CWGSResolver_DL::CWGSResolver_DL(CDataLoader* loader)
    : m_Loader(loader)
{
}


CWGSResolver_DL::~CWGSResolver_DL(void)
{
}


CRef<CWGSResolver>
CWGSResolver_DL::CreateResolver(CDataLoader* loader)
{
    if ( !loader ) {
        return null;
    }
    return CRef<CWGSResolver>(new CWGSResolver_DL(loader));
}


CRef<CWGSResolver>
CWGSResolver_DL::CreateResolver(void)
{
    CRef<CWGSResolver_DL> resolver(new CWGSResolver_DL());
    if ( !resolver->IsValid() ) {
        return null;
    }
    return CRef<CWGSResolver>(resolver);
}


CWGSResolver::TWGSPrefixes CWGSResolver_DL::GetPrefixes(const CSeq_id& id)
{
    TWGSPrefixes prefixes;
    if ( s_DebugEnabled(eDebug_resolve) ) {
        ERR_POST_X(10, "CWGSResolver_DL: "
                   "Asking DataLoader for ids of "<<id.AsFastaString());
    }
    CDataLoader::TIds ids;
    m_Loader->GetIds(CSeq_id_Handle::GetHandle(id), ids);
    ITERATE ( CDataLoader::TIds, rit, ids ) {
        if ( s_DebugEnabled(eDebug_resolve) ) {
            ERR_POST_X(11, "CWGSResolver_DL: Parsing Seq-id "<<*rit);
        }
        string prefix = ParseWGSPrefix(*rit->GetSeqId());
        if ( !prefix.empty() ) {
            if ( s_DebugEnabled(eDebug_resolve) ) {
                ERR_POST_X(12, "CWGSResolver_DL: WGS prefix: "<<prefix);
            }
            prefixes.push_back(prefix);
            break;
        }
    }
    return prefixes;
}


/////////////////////////////////////////////////////////////////////////////
// CWGSResolver_Proc
/////////////////////////////////////////////////////////////////////////////


CWGSResolver_Proc::CWGSResolver_Proc(CID2ProcessorResolver* resolver)
    : m_Resolver(resolver)
{
}


CWGSResolver_Proc::~CWGSResolver_Proc(void)
{
}


CRef<CWGSResolver>
CWGSResolver_Proc::CreateResolver(CID2ProcessorResolver* resolver)
{
    if ( !resolver ) {
        return null;
    }
    return CRef<CWGSResolver>(new CWGSResolver_Proc(resolver));
}


CWGSResolver::TWGSPrefixes CWGSResolver_Proc::GetPrefixes(const CSeq_id& id)
{
    TWGSPrefixes prefixes;
    if ( s_DebugEnabled(eDebug_resolve) ) {
        ERR_POST_X(13, "CWGSResolver_Proc: "
                   "Asking GB for ids of "<<id.AsFastaString());
    }
    CID2ProcessorResolver::TIds ids = m_Resolver->GetIds(id);
    ITERATE ( CID2ProcessorResolver::TIds, rit, ids ) {
        if ( s_DebugEnabled(eDebug_resolve) ) {
            ERR_POST_X(14, "CWGSResolver_Proc: "
                       "Parsing Seq-id "<<(*rit)->AsFastaString());
        }
        string prefix = ParseWGSPrefix(**rit);
        if ( !prefix.empty() ) {
            if ( s_DebugEnabled(eDebug_resolve) ) {
                ERR_POST_X(15, "CWGSResolver_Proc: WGS prefix: "<<prefix);
            }
            prefixes.push_back(prefix);
            break;
        }
    }
    return prefixes;
}


#ifdef WGS_RESOLVER_USE_ID2_CLIENT

/////////////////////////////////////////////////////////////////////////////
// CWGSResolver_ID2
/////////////////////////////////////////////////////////////////////////////


CWGSResolver_ID2::CWGSResolver_ID2(void)
    : m_ID2Client(new CID2Client())
{
}


CWGSResolver_ID2::~CWGSResolver_ID2(void)
{
}


CRef<CWGSResolver>
CWGSResolver_ID2::CreateResolver(void)
{
    CRef<CWGSResolver_ID2> resolver(new CWGSResolver_ID2);
    if ( !resolver->IsValid() ) {
        return null;
    }
    return CRef<CWGSResolver>(resolver);
}


string CWGSResolver_ID2::ParseWGSPrefix(const CID2_Reply& reply) const
{
    if ( !reply.GetReply().IsGet_seq_id() ) {
        return string();
    }
    const CID2_Reply_Get_Seq_id& reply_id = reply.GetReply().GetGet_seq_id();
    if ( !reply_id.IsSetSeq_id() ) {
        return string();
    }
    const CID2_Reply_Get_Seq_id::TSeq_id& ids = reply_id.GetSeq_id();
    ITERATE ( CID2_Reply_Get_Seq_id::TSeq_id, it, ids ) {
        string prefix = CWGSResolver_Ids::ParseWGSPrefix(**it);
        if ( !prefix.empty() ) {
            return prefix;
        }
    }
    return string();
}


bool CWGSResolver_ID2::Update(void)
{
    CMutexGuard guard(m_Mutex);
    bool ret = !m_Cache.empty();
    m_Cache.clear();
    return ret;
}


CWGSResolver::TWGSPrefixes CWGSResolver_ID2::GetPrefixes(const CSeq_id& id)
{
    TWGSPrefixes prefixes;
    CMutexGuard guard(m_Mutex);
    string id_str = id.AsFastaString();
    TCache::const_iterator iter = m_Cache.find(id_str);
    if ( iter != m_Cache.end() ) {
        if ( !iter->second.empty() ) {
            prefixes.push_back(iter->second);
        }
        return prefixes;
    }
    CID2_Request_Get_Seq_id req;
    req.SetSeq_id().SetSeq_id(const_cast<CSeq_id&>(id));
    req.SetSeq_id_type(req.eSeq_id_type_general);
    if ( s_DebugEnabled(eDebug_resolve) ) {
        ERR_POST_X(16, "CWGSResolver_ID2: "
                   "Asking ID2 for ids of "<<id.AsFastaString());
    }
    m_ID2Client->AskGet_seq_id(req);
    const CID2Client::TReplies& replies = m_ID2Client->GetAllReplies();
    ITERATE ( CID2Client::TReplies, rit, replies ) {
        if ( s_DebugEnabled(eDebug_resolve) ) {
            ERR_POST_X(17, "CWGSResolver_ID2: "
                       "Parsing ID2 reply "<<MSerial_AsnText<<**rit);
        }
        string prefix = ParseWGSPrefix(**rit);
        if ( !prefix.empty() ) {
            if ( s_DebugEnabled(eDebug_resolve) ) {
                ERR_POST_X(18, "CWGSResolver_ID2: WGS prefix: "<<prefix);
            }
            prefixes.push_back(prefix);
            break;
        }
    }
    string& save = m_Cache[id_str];
    if ( !prefixes.empty() ) {
        save = prefixes[0];
    }
    return prefixes;
}

#endif //WGS_RESOLVER_USE_ID2_CLIENT


END_NAMESPACE(objects);
END_NCBI_NAMESPACE;
