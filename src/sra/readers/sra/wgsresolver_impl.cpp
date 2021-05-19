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
NCBI_DEFINE_ERR_SUBCODE_X(32);

BEGIN_NAMESPACE(objects);


#define DEFAULT_WGS_INDEX_ACC "ZZZZ99"
#define DEFAULT_WGS_INDEX2_ACC "ZZZZ98"
#define DEFAULT_WGS_INDEX3_ACC "ZZZZ97"
#define DEFAULT_WGS_INDEX_PATH1 NCBI_TRACES04_PATH "/wgs03/WGS/ZZ/ZZ/ZZZZ99"
#define DEFAULT_WGS_INDEX_PATH2 NCBI_TRACES04_PATH "/wgs03/WGS/WGS_INDEX"
#define DEFAULT_WGS_INDEX2_PATH1 NCBI_TRACES04_PATH "/wgs03/WGS/ZZ/ZZ/ZZZZ98"
#define DEFAULT_WGS_INDEX2_PATH2 NCBI_TRACES04_PATH "/wgs03/WGS/WGS_INDEX_V2"
#define DEFAULT_WGS_INDEX3_PATH1 NCBI_TRACES04_PATH "/wgs03/WGS/ZZ/ZZ/ZZZZ97"
#define DEFAULT_WGS_INDEX3_PATH2 NCBI_TRACES04_PATH "/wgs03/WGS/WGS_INDEX_V3"

#define DEFAULT_WGS_RANGE_INDEX_ACC "ZZZZ79"
#define DEFAULT_WGS_RANGE_INDEX2_ACC "ZZZZ78"
#define DEFAULT_WGS_RANGE_INDEX_PATH1 NCBI_TRACES04_PATH "/wgs03/WGS/ZZ/ZZ/ZZZZ79"
#define DEFAULT_WGS_RANGE_INDEX_PATH2 NCBI_TRACES04_PATH "/wgs03/WGS/WGS_RANGE_INDEX_1"
#define DEFAULT_WGS_RANGE_INDEX2_PATH1 NCBI_TRACES04_PATH "/wgs03/WGS/ZZ/ZZ/ZZZZ78"
#define DEFAULT_WGS_RANGE_INDEX2_PATH2 NCBI_TRACES04_PATH "/wgs03/WGS/WGS_RANGE_INDEX_2"


NCBI_PARAM_DECL(bool, WGS, RESOLVER_DIRECT_WGS_INDEX);
NCBI_PARAM_DEF(bool, WGS, RESOLVER_DIRECT_WGS_INDEX, true);

NCBI_PARAM_DECL(bool, WGS, RESOLVER_GENBANK);
NCBI_PARAM_DEF(bool, WGS, RESOLVER_GENBANK, true);

NCBI_PARAM_DECL(bool, WGS, RESOLVER_WGS_RANGE_INDEX);
NCBI_PARAM_DEF(bool, WGS, RESOLVER_WGS_RANGE_INDEX, true);

static inline bool s_UseWGSRangeIndex(void)
{
    static bool value = NCBI_PARAM_TYPE(WGS, RESOLVER_WGS_RANGE_INDEX)::GetDefault();
    return value;
}

NCBI_PARAM_DECL(string, WGS, WGS_INDEX);
NCBI_PARAM_DEF(string, WGS, WGS_INDEX, "");


NCBI_PARAM_DECL(string, WGS, WGS_INDEX2);
NCBI_PARAM_DEF(string, WGS, WGS_INDEX2, "");


NCBI_PARAM_DECL(string, WGS, WGS_INDEX3);
NCBI_PARAM_DEF(string, WGS, WGS_INDEX3, "");


NCBI_PARAM_DECL(string, WGS, WGS_INDEX_ACC);
NCBI_PARAM_DEF(string, WGS, WGS_INDEX_ACC, DEFAULT_WGS_INDEX_ACC);


NCBI_PARAM_DECL(string, WGS, WGS_INDEX2_ACC);
NCBI_PARAM_DEF(string, WGS, WGS_INDEX2_ACC, DEFAULT_WGS_INDEX2_ACC);


NCBI_PARAM_DECL(string, WGS, WGS_INDEX3_ACC);
NCBI_PARAM_DEF(string, WGS, WGS_INDEX3_ACC, DEFAULT_WGS_INDEX3_ACC);


NCBI_PARAM_DECL(string, WGS, WGS_RANGE_INDEX);
NCBI_PARAM_DEF(string, WGS, WGS_RANGE_INDEX, "");


NCBI_PARAM_DECL(string, WGS, WGS_RANGE_INDEX2);
NCBI_PARAM_DEF(string, WGS, WGS_RANGE_INDEX2, "");


NCBI_PARAM_DECL(string, WGS, WGS_RANGE_INDEX_ACC);
NCBI_PARAM_DEF(string, WGS, WGS_RANGE_INDEX_ACC, DEFAULT_WGS_RANGE_INDEX_ACC);


NCBI_PARAM_DECL(string, WGS, WGS_RANGE_INDEX2_ACC);
NCBI_PARAM_DEF(string, WGS, WGS_RANGE_INDEX2_ACC, DEFAULT_WGS_RANGE_INDEX2_ACC);


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


//#define COLLECT_PROFILE
#ifdef COLLECT_PROFILE
struct SProfiler
{
    const char* name;
    size_t count;
    CStopWatch sw;
    SProfiler() : name(0), count(0) {}
    ~SProfiler() {
        if ( name )
            cout << name<<" calls: "<<count<<" time: "<<sw.Elapsed()<<endl;
    }
};
struct SProfilerGuard
{
    SProfiler& sw;
    SProfilerGuard(SProfiler& sw, const char* name)
        : sw(sw)
        {
            sw.name = name;
            sw.count += 1;
            sw.sw.Start();
        }
    ~SProfilerGuard()
        {
            sw.sw.Stop();
        }
};

static SProfiler sw_AccFind;
static SProfiler sw_AccRange;
static SProfiler sw_WGSPrefix;

# define PROFILE(var) SProfilerGuard guard(var, #var)
#else
# define PROFILE(var)
#endif

/////////////////////////////////////////////////////////////////////////////
// CWGSResolver_VDB
/////////////////////////////////////////////////////////////////////////////


// SGiIdxTableCursor is helper accessor structure for optional GI_IDX table
struct CWGSResolver_VDB::SGiIdxTableCursor : public CObject {
    explicit SGiIdxTableCursor(const CVDBTable& table);

    CVDBTable m_Table;
    CVDBCursor m_Cursor;

    DECLARE_VDB_COLUMN_AS_STRING(WGS_PREFIX);
};


CWGSResolver_VDB::SGiIdxTableCursor::SGiIdxTableCursor(const CVDBTable& table)
    : m_Table(table),
      m_Cursor(table),
      INIT_VDB_COLUMN(WGS_PREFIX)
{
}


// SAccIdxTableCursor is helper accessor structure for optional ACC_IDX table
struct CWGSResolver_VDB::SAccIdxTableCursor : public CObject {
    explicit SAccIdxTableCursor(const CVDBTable& table);

    CVDBTable m_Table;
    CVDBCursor m_Cursor;

    typedef Uint2 acc_range_number_t;
    DECLARE_VDB_COLUMN_AS(acc_range_number_t, ACCESSION_RANGE);
    DECLARE_VDB_COLUMN_AS_STRING(WGS_PREFIX);
};


CWGSResolver_VDB::SAccIdxTableCursor::SAccIdxTableCursor(const CVDBTable& table)
    : m_Table(table),
      m_Cursor(table),
      INIT_OPTIONAL_VDB_COLUMN(ACCESSION_RANGE),
      INIT_VDB_COLUMN(WGS_PREFIX)
{
}


string CWGSResolver_VDB::GetDefaultWGSIndexPath(EIndexType index_type)
{
    if ( s_UseWGSRangeIndex() ) {
        if ( index_type == eMainIndex ) {
            return NCBI_PARAM_TYPE(WGS, WGS_RANGE_INDEX)::GetDefault();
        }
        else if ( index_type == eSecondIndex ) {
            return NCBI_PARAM_TYPE(WGS, WGS_RANGE_INDEX2)::GetDefault();
        }
    }
    else {
        if ( index_type == eMainIndex ) {
            return NCBI_PARAM_TYPE(WGS, WGS_INDEX)::GetDefault();
        }
        else if ( index_type == eSecondIndex ) {
            return NCBI_PARAM_TYPE(WGS, WGS_INDEX2)::GetDefault();
        }
        else if ( index_type == eThirdIndex ) {
            return NCBI_PARAM_TYPE(WGS, WGS_INDEX3)::GetDefault();
        }
    }
    return string();
}


string CWGSResolver_VDB::GetDefaultWGSIndexAcc(EIndexType index_type)
{
    if ( s_UseWGSRangeIndex() ) {
        if ( index_type == eMainIndex ) {
            return NCBI_PARAM_TYPE(WGS, WGS_RANGE_INDEX_ACC)::GetDefault();
        }
        else if ( index_type == eSecondIndex ) {
            return NCBI_PARAM_TYPE(WGS, WGS_RANGE_INDEX2_ACC)::GetDefault();
        }
    }
    else {
        if ( index_type == eMainIndex ) {
            return NCBI_PARAM_TYPE(WGS, WGS_INDEX_ACC)::GetDefault();
        }
        else if ( index_type == eSecondIndex ) {
            return NCBI_PARAM_TYPE(WGS, WGS_INDEX2_ACC)::GetDefault();
        }
        else if ( index_type == eThirdIndex ) {
            return NCBI_PARAM_TYPE(WGS, WGS_INDEX3_ACC)::GetDefault();
        }
    }
    return string();
}


static
string GetDirectWGSIndexPath(CWGSResolver_VDB::EIndexType index_type)
{
    string path;
    if ( NCBI_PARAM_TYPE(WGS, RESOLVER_DIRECT_WGS_INDEX)::GetDefault() ) {
        const char* path1 = 0;
        const char* path2 = 0;
        if ( s_UseWGSRangeIndex() ) {
            if ( index_type == CWGSResolver_VDB::eMainIndex ) {
                path1 = DEFAULT_WGS_RANGE_INDEX_PATH1;
                path2 = DEFAULT_WGS_RANGE_INDEX_PATH2;
            }
            else if ( index_type == CWGSResolver_VDB::eSecondIndex ) {
                path1 = DEFAULT_WGS_RANGE_INDEX2_PATH1;
                path2 = DEFAULT_WGS_RANGE_INDEX2_PATH2;
            }
        }
        else {
            if ( index_type == CWGSResolver_VDB::eMainIndex ) {
                path1 = DEFAULT_WGS_INDEX_PATH1;
                path2 = DEFAULT_WGS_INDEX_PATH2;
            }
            else if ( index_type == CWGSResolver_VDB::eSecondIndex ) {
                path1 = DEFAULT_WGS_INDEX2_PATH1;
                path2 = DEFAULT_WGS_INDEX2_PATH2;
            }
            else if ( index_type == CWGSResolver_VDB::eThirdIndex ) {
                path1 = DEFAULT_WGS_INDEX3_PATH1;
                path2 = DEFAULT_WGS_INDEX3_PATH2;
            }
        }
        if ( path1 && CDirEntry(path1).Exists() ) {
            path = path1;
        }
        else if ( path2 && CDirEntry(path2).Exists() ) {
            path = path2;
        }
    }
    return path;
}


CWGSResolver_VDB::CWGSResolver_VDB(const CVDBMgr& mgr,
                                   EIndexType index_type,
                                   CWGSResolver_VDB* next_resolver)
    : m_NextResolver(next_resolver)
{
    string path = GetDefaultWGSIndexPath(index_type);
    if ( path.empty() ) {
        string acc = GetDefaultWGSIndexAcc(index_type);
        // no user-defined index path, try default locations
        // first try to open index by predefined accession, maybe remotely
        Open(mgr, acc);
        if ( IsValid() ) {
            // opened
            return;
        }
        // then try to open index by direct file acces, only locally
        path = GetDirectWGSIndexPath(index_type);
        if ( path.empty() ) {
            // VDB index is not available
            return;
        }
    }
    if ( path.find_first_of("\\/") != NPOS && !CDirEntry(path).Exists() ) {
        // not an accession (has directory separators) and not a file
        if ( s_DebugEnabled(eDebug_error) ) {
            ERR_POST_X(9, "CWGSResolver_VDB: cannot find index file: "<<path);
        }
        return;
    }
    Open(mgr, path);
}


CWGSResolver_VDB::CWGSResolver_VDB(const CVDBMgr& mgr,
                                   const string& path,
                                   CWGSResolver_VDB* next_resolver)
    : m_NextResolver(next_resolver)
{
    Open(mgr, path);
}


CWGSResolver_VDB::~CWGSResolver_VDB(void)
{
    Close();
}


CRef<CWGSResolver> CWGSResolver_VDB::CreateResolver(const CVDBMgr& mgr)
{
    CRef<CWGSResolver_VDB> ret(new CWGSResolver_VDB(mgr, eMainIndex));
    if ( !ret->IsValid() ) {
        return null;
    }
    CRef<CWGSResolver_VDB> ret2(new CWGSResolver_VDB(mgr, eSecondIndex, ret));
    if ( ret2->IsValid() ) {
        ret = ret2;
    }
    if ( !ret->m_AccIndexIsPrefix ) {
        CRef<CWGSResolver_VDB> ret3(new CWGSResolver_VDB(mgr, eThirdIndex, ret));
        if ( ret3->IsValid() ) {
            ret = ret3;
        }
    }
    return CRef<CWGSResolver>(ret);
}


void CWGSResolver_VDB::Close(void)
{
    TDBMutex::TWriteLockGuard guard(m_DBMutex);
    x_Close();
}


void CWGSResolver_VDB::x_Close()
{
    m_Mgr.Close();
    m_Db.Close();
    m_GiIdxTable.Close();
    m_AccIdxTable.Close();
    m_AccIndex.Close();
    m_GiIdxCursorCache.Clear();
    m_AccIdxCursorCache.Clear();
}


static string s_ResolveAccOrPath(const CVDBMgr& mgr, const string& acc_or_path)
{
    string path;
    if ( CVPath::IsPlainAccession(acc_or_path) ) {
        // resolve VDB accessions
        try {
            path = mgr.FindAccPath(acc_or_path);
            if ( s_DebugEnabled(eDebug_open) ) {
                LOG_POST_X(28, "CWGSResolver_VDB("<<acc_or_path<<"): -> "<<path);
            }
        }
        catch ( CSraException& /*ignored*/ ) {
            path = acc_or_path;
        }
    }
    else {
        // real path, http:, etc.
        path = acc_or_path;
    }

    // resolve symbolic links for correct timestamp and longer-living reference
    CDirEntry de(path);
    if ( de.Exists() ) {
        de.DereferenceLink();
        if ( de.GetPath() != path ) {
            path = de.GetPath();
            if ( s_DebugEnabled(eDebug_open) ) {
                LOG_POST_X(29, "CWGSResolver_VDB("<<acc_or_path<<"): "
                           "resolved index link to "<<path);
            }
        }
    }
    return path;
}


void CWGSResolver_VDB::Open(const CVDBMgr& mgr, const string& acc_or_path)
{
    string path = s_ResolveAccOrPath(mgr, acc_or_path);

    // open VDB file
    TDBMutex::TWriteLockGuard guard(m_DBMutex);
    x_Close();
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

    // save original argument for possible changes in symbolic links
    m_WGSIndexPath = acc_or_path;
    m_WGSIndexResolvedPath = path;
    if ( !CDirEntry(path).GetTime(&m_Timestamp) ) {
        m_Timestamp = CTime();
    }
    else {
        if ( s_DebugEnabled(eDebug_open) ) {
            LOG_POST_X(30, "CWGSResolver_VDB("<<acc_or_path<<"): index timestamp: "<<m_Timestamp);
        }
    }
    m_GiIdxTable = CVDBTable(m_Db, "GI_IDX");
    m_AccIdxTable = CVDBTable(m_Db, "ACC_IDX");
    m_AccIndexIsPrefix = true;
    m_AccIndex = CVDBTableIndex(m_AccIdxTable, "accession_prefix", CVDBTableIndex::eMissing_Allow);
    if ( !m_AccIndex ) {
        m_AccIndexIsPrefix = false;
        m_AccIndex = CVDBTableIndex(m_AccIdxTable, "accession");
    }
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
    bool ret = x_Update();
    if ( m_NextResolver && m_NextResolver->Update() ) {
        ret = true;
    }
    return ret;
}


bool CWGSResolver_VDB::x_Update(void)
{
    string path = s_ResolveAccOrPath(m_Mgr, GetWGSIndexPath());
    if ( path != GetWGSIndexResolvedPath() ) {
        // resolved to a different path -> new index by symbolic link
        LOG_POST_X(32, "CWGSResolver_VDB: new index path: "<<path);
        Reopen();
        return true;
    }

    CTime timestamp;
    if ( !CDirEntry(path).GetTime(&timestamp) ) {
        // cannot get timestamp -> remote reference
        return false;
    }
    if ( timestamp == m_Timestamp ) {
        // same timestamp
        return false;
    }
    if ( s_DebugEnabled(eDebug_open) ) {
        LOG_POST_X(31, "CWGSResolver_VDB: new index timestamp: "<<timestamp);
    }
    Reopen();
    return true;
}


inline
CRef<CWGSResolver_VDB::SGiIdxTableCursor> CWGSResolver_VDB::GiIdx(TIntId row)
{
    CRef<SGiIdxTableCursor> curs = m_GiIdxCursorCache.Get(row);
    if ( !curs ) {
        curs = new SGiIdxTableCursor(GiIdxTable());
    }
    return curs;
}


inline
CRef<CWGSResolver_VDB::SAccIdxTableCursor> CWGSResolver_VDB::AccIdx(void)
{
    CRef<SAccIdxTableCursor> curs = m_AccIdxCursorCache.Get();
    if ( !curs ) {
        curs = new SAccIdxTableCursor(AccIdxTable());
    }
    return curs;
}


inline
void CWGSResolver_VDB::Put(CRef<SGiIdxTableCursor>& curs, TIntId row)
{
    if ( curs->m_Table == GiIdxTable() ) {
        m_GiIdxCursorCache.Put(curs, row);
    }
}


inline
void CWGSResolver_VDB::Put(CRef<SAccIdxTableCursor>& curs)
{
    if ( curs->m_Table == AccIdxTable() ) {
        m_AccIdxCursorCache.Put(curs);
    }
}


CWGSResolver::TWGSPrefixes CWGSResolver_VDB::GetPrefixes(TGi gi)
{
    TDBMutex::TReadLockGuard guard(m_DBMutex);
    TWGSPrefixes ret;
    if ( s_DebugEnabled(eDebug_resolve) ) {
        LOG_POST_X(24, "CWGSResolver_VDB("<<GetWGSIndexPath()<<"): Resolving "<<gi);
    }
    CRef<SGiIdxTableCursor> cur = GiIdx();
    CVDBStringValue value = cur->WGS_PREFIX(GI_TO(TVDBRowId, gi), CVDBValue::eMissing_Allow);
    if ( !value.empty() ) {
        if ( s_DebugEnabled(eDebug_resolve) ) {
            LOG_POST_X(25, "CWGSResolver_VDB("<<GetWGSIndexPath()<<"): WGS prefix "<<*value);
        }
        ret.push_back(*value);
    }
    Put(cur);
    if ( ret.empty() && m_NextResolver ) {
        ret = m_NextResolver->GetPrefixes(gi);
    }
    return ret;
}


static inline bool s_SplitAccIndex(string& uacc, Uint2& key_num)
{
    size_t acc_len = uacc.size();
    if ( acc_len <= 4 ) {
        return false;
    }
    size_t prefix_len = acc_len-4;
    unsigned v = 0;
    for ( int i = 0; i < 4; ++i ) {
        char c = uacc[prefix_len+i];
        if ( c < '0' || c > '9' ) {
            return false;
        }
        v = v*10 + (c-'0');
    }
    key_num = v;
    uacc.erase(prefix_len);
    return true;
}


CWGSResolver::TWGSPrefixes CWGSResolver_VDB::GetPrefixes(const string& acc)
{
    TDBMutex::TReadLockGuard guard(m_DBMutex);
    TWGSPrefixes ret;
    if ( s_DebugEnabled(eDebug_resolve) ) {
        LOG_POST_X(26, "CWGSResolver_VDB("<<GetWGSIndexPath()<<"): Resolving "<<acc);
    }
    string uacc = acc;
    SAccIdxTableCursor::acc_range_number_t key_num = 0;
    if ( m_AccIndexIsPrefix ) {
        if ( !s_SplitAccIndex(uacc, key_num) ) {
            if ( s_DebugEnabled(eDebug_resolve) ) {
                LOG_POST_X(27, "CWGSResolver_VDB("<<GetWGSIndexPath()<<"): invalid accession");
            }
            return ret;
        }
    }
    NStr::ToUpper(uacc);
    TVDBRowIdRange range;
    {{
        PROFILE(sw_AccFind);
        range = m_AccIndex.Find(uacc);
    }}
    if ( s_DebugEnabled(eDebug_resolve) ) {
        LOG_POST_X(27, "CWGSResolver_VDB("<<GetWGSIndexPath()<<"): "
                   "range "<<range.first<<"-"<<range.second);
    }
    if ( range.second ) {
        CRef<SAccIdxTableCursor> cur = AccIdx();
        for ( TVDBRowCount i = 0; i < range.second; ++i ) {
            TVDBRowId row_id = range.first+i;
            if ( m_AccIndexIsPrefix ) {
                PROFILE(sw_AccRange);
                CVDBValueFor<SAccIdxTableCursor::acc_range_number_t> v =
                    cur->ACCESSION_RANGE(row_id);
                if ( v[0] > key_num ) {
                    // current range is past the requested id, end of scan
                    break;
                }
                if ( v[1] < key_num ) {
                    // current range is before the requested id, check next range
                    continue;
                }
            }
            PROFILE(sw_WGSPrefix);
            CTempString prefix = *cur->WGS_PREFIX(row_id);
            if ( s_DebugEnabled(eDebug_resolve) ) {
                LOG_POST_X(27, "CWGSResolver_VDB("<<GetWGSIndexPath()<<"): WGS prefix "<<prefix);
            }
            ret.push_back(prefix);
        }
        Put(cur);
    }
    if ( ret.empty() && m_NextResolver ) {
        ret = m_NextResolver->GetPrefixes(acc);
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
    if ( !NCBI_PARAM_TYPE(WGS, RESOLVER_GENBANK)::GetDefault() ) {
        return null;
    }
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
        LOG_POST_X(10, "CWGSResolver_DL: "
                   "Asking DataLoader for ids of "<<id.AsFastaString());
    }
    CDataLoader::TIds ids;
    m_Loader->GetIds(CSeq_id_Handle::GetHandle(id), ids);
    ITERATE ( CDataLoader::TIds, rit, ids ) {
        if ( s_DebugEnabled(eDebug_resolve) ) {
            LOG_POST_X(11, "CWGSResolver_DL: Parsing Seq-id "<<*rit);
        }
        string prefix = ParseWGSPrefix(*rit->GetSeqId());
        if ( !prefix.empty() ) {
            if ( s_DebugEnabled(eDebug_resolve) ) {
                LOG_POST_X(12, "CWGSResolver_DL: WGS prefix: "<<prefix);
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
        LOG_POST_X(13, "CWGSResolver_Proc: "
                   "Asking GB for ids of "<<id.AsFastaString());
    }
    CID2ProcessorResolver::TIds ids = m_Resolver->GetIds(id);
    ITERATE ( CID2ProcessorResolver::TIds, rit, ids ) {
        if ( s_DebugEnabled(eDebug_resolve) ) {
            LOG_POST_X(14, "CWGSResolver_Proc: "
                       "Parsing Seq-id "<<(*rit)->AsFastaString());
        }
        string prefix = ParseWGSPrefix(**rit);
        if ( !prefix.empty() ) {
            if ( s_DebugEnabled(eDebug_resolve) ) {
                LOG_POST_X(15, "CWGSResolver_Proc: WGS prefix: "<<prefix);
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
        LOG_POST_X(16, "CWGSResolver_ID2: "
                   "Asking ID2 for ids of "<<id.AsFastaString());
    }
    m_ID2Client->AskGet_seq_id(req);
    const CID2Client::TReplies& replies = m_ID2Client->GetAllReplies();
    ITERATE ( CID2Client::TReplies, rit, replies ) {
        if ( s_DebugEnabled(eDebug_resolve) ) {
            LOG_POST_X(17, "CWGSResolver_ID2: "
                       "Parsing ID2 reply "<<MSerial_AsnText<<**rit);
        }
        string prefix = ParseWGSPrefix(**rit);
        if ( !prefix.empty() ) {
            if ( s_DebugEnabled(eDebug_resolve) ) {
                LOG_POST_X(18, "CWGSResolver_ID2: WGS prefix: "<<prefix);
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
