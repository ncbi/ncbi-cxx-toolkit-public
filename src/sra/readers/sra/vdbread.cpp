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
 *   Access to SRA files
 *
 */

#include <ncbi_pch.hpp>
#include <common/ncbi_package_ver.h>
#include <common/ncbi_source_ver.h>
#include <sra/readers/sra/vdbread.hpp>

#include <klib/rc.h>
#include <klib/log.h>
#include <klib/text.h>
#include <kfg/config.h>
#include <kdb/manager.h>
#include <kdb/kdb-priv.h>
#include <kns/manager.h>
#include <kns/http.h>

#include <vfs/manager-priv.h>
#include <vfs/manager.h>
#include <vfs/path.h>
#include <vfs/resolver.h>

#include <sra/sradb-priv.h>
#include <sra/sraschema.h>

#include <vdb/vdb-priv.h>
#include <vdb/manager.h>
#include <vdb/database.h>
#include <vdb/schema.h>
#include <vdb/table.h>
#include <vdb/cursor.h>


#include <corelib/ncbimtx.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_param.hpp>
#include <sra/readers/ncbi_traces_path.hpp>
#include <objects/general/general__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqset/seqset__.hpp>
#include <objects/seqres/seqres__.hpp>
#include <sra/error_codes.hpp>

#include <cstring>
#include <algorithm>

BEGIN_NCBI_SCOPE

#define NCBI_USE_ERRCODE_X   VDBReader
NCBI_DEFINE_ERR_SUBCODE_X(2);

BEGIN_SCOPE(objects)

class CSeq_entry;


NCBI_PARAM_DECL(int, VDB, DIAG_HANDLER);
NCBI_PARAM_DEF(int, VDB, DIAG_HANDLER, 1);


static int s_GetDiagHandler(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(VDB, DIAG_HANDLER)> s_Value;
    return s_Value->Get();
}



DEFINE_SRA_REF_TRAITS(VDBManager, const);
DEFINE_SRA_REF_TRAITS(VDatabase, const);
DEFINE_SRA_REF_TRAITS(VTable, const);
DEFINE_SRA_REF_TRAITS(VCursor, const);
DEFINE_SRA_REF_TRAITS(KIndex, const);
DEFINE_SRA_REF_TRAITS(KConfig, const);
DEFINE_SRA_REF_TRAITS(KDBManager, const);
DEFINE_SRA_REF_TRAITS(KNSManager, );
DEFINE_SRA_REF_TRAITS(VFSManager, );
DEFINE_SRA_REF_TRAITS(VPath, const);
DEFINE_SRA_REF_TRAITS(VResolver, );

/////////////////////////////////////////////////////////////////////////////
// CKConfig

CKConfig::CKConfig(void)
{
    KConfig* cfg;
    if ( rc_t rc = KConfigMake(&cfg, 0) ) {
        *x_InitPtr() = 0;
        NCBI_THROW2(CSraException, eInitFailed,
                    "Cannot create KConfig", rc);
    }
    *x_InitPtr() = cfg;
}


CKConfig::CKConfig(const CVDBMgr& mgr)
{
    *x_InitPtr() = VFSManagerGetConfig(CVFSManager(mgr));
    if ( rc_t rc = KConfigAddRef(*this) ) {
        *x_InitPtr() = 0;
        NCBI_THROW2(CSraException, eInitFailed,
                    "Cannot get reference to KConfig", rc);
    }
}


void CKConfig::Commit() const
{
    if ( rc_t rc = KConfigCommit(const_cast<KConfig*>(GetPointer())) ) {
        NCBI_THROW2(CSraException, eOtherError,
                    "CKConfig: Cannot commit config changes", rc);
    }
}


/////////////////////////////////////////////////////////////////////////////
// CVFSManager

CVFSManager::CVFSManager(void)
{
    x_InitNew();
}


CVFSManager::CVFSManager(ECreateNew)
{
    x_InitNew();
}


void CVFSManager::x_InitNew(void)
{
    if ( rc_t rc = VFSManagerMake(x_InitPtr()) ) {
        *x_InitPtr() = 0;
        NCBI_THROW2(CSraException, eInitFailed,
                    "Cannot create VFSManager", rc);
    }
}


CVFSManager::CVFSManager(const CVDBMgr& mgr)
{
    if ( rc_t rc = KDBManagerGetVFSManager(CKDBManager(mgr), x_InitPtr()) ) {
        *x_InitPtr() = 0;
        NCBI_THROW2(CSraException, eInitFailed,
                    "Cannot get VFSManager", rc);
    }
}


/////////////////////////////////////////////////////////////////////////////
// CKDBManager


CKDBManager::CKDBManager(const CVDBMgr& mgr)
{
    if ( rc_t rc = VDBManagerGetKDBManagerRead(mgr, x_InitPtr()) ) {
        *x_InitPtr() = 0;
        NCBI_THROW2(CSraException, eInitFailed,
                    "Cannot get KDBManager", rc);
    }
}


/////////////////////////////////////////////////////////////////////////////
// CKNSManager


CKNSManager::CKNSManager(const CVFSManager& mgr)
{
    if ( rc_t rc = VFSManagerGetKNSMgr(mgr, x_InitPtr()) ) {
        *x_InitPtr() = 0;
        NCBI_THROW2(CSraException, eInitFailed,
                    "Cannot get KNSManager", rc);
    }
}


/////////////////////////////////////////////////////////////////////////////
// CVPath

CVPath::CVPath(const CVFSManager& mgr, const string& path, EType type)
{
    x_Init(mgr, path, type);
}


CVPath::CVPath(const string& path, EType type)
{
    x_Init(CVFSManager(CVFSManager::eCreateNew), path, type);
}


void CVPath::x_Init(const CVFSManager& mgr, const string& path, EType type)
{
    VPath* vpath = 0;
    if ( type == eSys ) {
        if ( rc_t rc = VFSManagerMakeSysPath(mgr, &vpath, path.c_str()) ) {
            *x_InitPtr() = 0;
            NCBI_THROW2_FMT(CSraException, eInitFailed,
                            "Cannot create sys VPath: "<<path, rc);
        }
    }
    else if ( type == eAcc ) {
        if ( rc_t rc = VFSManagerMakeAccPath(mgr, &vpath, path.c_str()) ) {
            *x_InitPtr() = 0;
            NCBI_THROW2_FMT(CSraException, eInitFailed,
                            "Cannot create acc VPath: "<<path, rc);
        }
    }
    else {
        if ( rc_t rc = VFSManagerMakePath(mgr, &vpath, path.c_str()) ) {
            *x_InitPtr() = 0;
            NCBI_THROW2_FMT(CSraException, eInitFailed,
                            "Cannot create VPath: "<<path, rc);
        }
    }
    *x_InitPtr() = vpath;
}


CTempString CVPath::ToString() const
{
    String str;
    if ( rc_t rc = VPathGetPath(*this, &str) ) {
        NCBI_THROW2(CSraException, eOtherError,
                    "Cannot get path from VPath", rc);
    }
    return CTempString(str.addr, str.size);
}


#ifdef NCBI_OS_MSWIN
static inline
bool s_HasWindowsDriveLetter(const string& s)
{
    // first symbol is letter, and second symbol is colon (':')
    return s.length() >= 2 && isalpha(s[0]&0xff) && s[1] == ':';
}
#endif


bool CVPath::IsPlainAccession(const string& acc_or_path)
{
#ifdef NCBI_OS_MSWIN
    return !s_HasWindowsDriveLetter(acc_or_path) &&
        acc_or_path.find_first_of("/\\") == NPOS;
#else
    return acc_or_path.find('/') == NPOS;
#endif
}


#ifdef NCBI_OS_MSWIN
string CVPath::ConvertSysPathToPOSIX(const string& sys_path)
{
    // Convert Windows path with drive letter
    // C:\Users\Public -> /C/Users/Public
    if ( sys_path[0] == 'h' &&
         (NStr::StartsWith(sys_path, "http://") ||
          NStr::StartsWith(sys_path, "https://")) ) {
        return sys_path;
    }
    try {
        string path = CDirEntry::CreateAbsolutePath(sys_path);
        replace(path.begin(), path.end(), '\\', '/');
        if (s_HasWindowsDriveLetter(path)) {
            // move drive letter from first symbol to second (in place of ':')
            path[1] = toupper(path[0] & 0xff);
            // add leading slash
            path[0] = '/';
        }
        return path;
    }
    catch (exception&) {
        // CDirEntry::CreateAbsolutePath() can fail on URL for remote access
        return sys_path;
    }
}


string CVPath::ConvertAccOrSysPathToPOSIX(const string& acc_or_path)
{
    return IsPlainAccession(acc_or_path) ?
        acc_or_path :
        ConvertSysPathToPOSIX(acc_or_path);
}
#endif


/////////////////////////////////////////////////////////////////////////////
// CVResolver

CVResolver::CVResolver(const CVFSManager& mgr)
    : m_Mgr(mgr)
{
    if ( rc_t rc = VFSManagerGetResolver(mgr, x_InitPtr()) ) {
        *x_InitPtr() = 0;
        NCBI_THROW2(CSraException, eInitFailed,
                    "Cannot get VResolver", rc);
    }
}


CVResolver::CVResolver(const CVFSManager& mgr, const CKConfig& cfg)
    : m_Mgr(mgr)
{
    if ( rc_t rc = VFSManagerMakeResolver(mgr, x_InitPtr(), cfg) ) {
        *x_InitPtr() = 0;
        NCBI_THROW2(CSraException, eInitFailed,
                    "Cannot create VResolver", rc);
    }
}


string CVResolver::Resolve(const string& acc_or_path) const
{
    if ( !CVPath::IsPlainAccession(acc_or_path) ) {
        // already a path
        return acc_or_path;
    }
    CVPath acc(m_Mgr, acc_or_path, CVPath::eAcc);
    const VPath* path;
    rc_t rc = VResolverLocal(*this, acc, &path);
    if ( rc ) {
        rc = VResolverRemote(*this, eProtocolNone, acc, &path);
    }
    if ( rc ) {
        if ( CDirEntry(acc_or_path).Exists() ) {
            // local file
            return acc_or_path;
        }
        NCBI_THROW2_FMT(CSraException, eNotFound,
                        "Cannot find acc path: "<<acc_or_path, rc);
    }
    return CVPath(path).ToString();
}


/////////////////////////////////////////////////////////////////////////////
// CVDBMgr

CVDBMgr::CVDBMgr(void)
    : m_Resolver(null)
{
    x_Init();
}


string CVDBMgr::FindAccPath(const string& acc) const
{
    if ( !m_Resolver ) {
        m_Resolver = CVResolver(CVFSManager(*this));
    }
    return m_Resolver.Resolve(acc);
}


//#define GUARD_SDK
#ifdef NCBI_COMPILER_MSVC
//# define GUARD_SDK_GET
#endif

#if defined GUARD_SDK || defined GUARD_SDK_GET
DEFINE_STATIC_FAST_MUTEX(sx_SDKMutex);
#endif

#ifdef GUARD_SDK
# define DECLARE_SDK_GUARD() CFastMutexGuard guard(sx_SDKMutex)
#else
# define DECLARE_SDK_GUARD() 
#endif

#ifdef GUARD_SDK_GET
# define DECLARE_SDK_GET_GUARD() CFastMutexGuard guard(sx_SDKMutex)
#else
# define DECLARE_SDK_GET_GUARD() 
#endif


static
rc_t VDBLogWriter(void* data, const char* buffer, size_t size, size_t* written)
{
    CTempString msg(buffer, size);
    NStr::TruncateSpacesInPlace(msg);
    LOG_POST_X(2, "VDB: "<<msg);
    *written = size;
    return 0;
}


void CVDBMgr::x_Init(void)
{
    if ( rc_t rc = VDBManagerMakeRead(x_InitPtr(), 0) ) {
        *x_InitPtr() = 0;
        NCBI_THROW2(CSraException, eInitFailed,
                    "Cannot open VDBManager", rc);
    }
    uint32_t sdk_ver;
    if ( rc_t rc = VDBManagerVersion(*this, &sdk_ver) ) {
        NCBI_THROW2(CSraException, eInitFailed,
                    "Cannot get VDBManager version", rc);
    }
    CKNSManager kns_mgr(CVFSManager(*this));
    CNcbiOstrstream str;
    CNcbiApplication* app = CNcbiApplication::Instance();
    if ( app ) {
        str << app->GetAppName() << ": " << app->GetVersion().Print() << "; ";
    }
#if NCBI_PACKAGE
    str << "Package: " << NCBI_PACKAGE_NAME << ' ' <<
        NCBI_PACKAGE_VERSION << "; ";
#endif
    str << "C++ ";
#ifdef NCBI_PRODUCTION_VER
    str << NCBI_PRODUCTION_VER << "/";
#endif
#ifdef NCBI_DEVELOPMENT_VER
    str << NCBI_DEVELOPMENT_VER;
#endif
    string prefix = CNcbiOstrstreamToString(str);
    KNSManagerSetUserAgent(kns_mgr, "%s; SRA Toolkit %V",
                           prefix.c_str(),
                           sdk_ver);

    // redirect VDB log to C++ Toolkit
    if ( s_GetDiagHandler() ) {
        KLogInit();
        KLogLevelSet(klogDebug);
        KLogLibHandlerSet(VDBLogWriter, 0);
    }

    if ( app ) {
        string host = app->GetConfig().GetString("CONN", "HTTP_PROXY_HOST", kEmptyStr);
        int port = app->GetConfig().GetInt("CONN", "HTTP_PROXY_PORT", 0);
        if ( !host.empty() && port != 0 ) {
            if ( rc_t rc = KNSManagerSetHTTPProxyPath(kns_mgr, "%s:%d", host.c_str(), port) ) {
                NCBI_THROW2(CSraException, eInitFailed,
                            "Cannot set KNSManager proxy parameters", rc);
            }
            KNSManagerSetHTTPProxyEnabled(kns_mgr, true);
        }
    }
}


string CVDBMgr::GetCacheRoot() const
{
    const VPath* ret;
    if ( rc_t rc = VDBManagerGetCacheRoot(*this, &ret) ) {
        if ( GetRCObject(rc) == RCObject(rcPath) &&
             GetRCState(rc) == rcNotFound ) {
            return kEmptyStr;
        }
        NCBI_THROW2(CSraException, eOtherError,
                    "CVDBMgr: Cannot get cache root", rc);
    }
    return CVPath(ret).ToString();
}


void CVDBMgr::SetCacheRoot(const string& path)
{
    CVPath vpath(CVFSManager(*this), path, CVPath::eSys);
    if ( rc_t rc = VDBManagerSetCacheRoot(*this, vpath) ) {
        NCBI_THROW2(CSraException, eOtherError,
                    "CVDBMgr: Cannot set cache root", rc);
    }
}


void CVDBMgr::DeleteCacheOlderThan(Uint4 days)
{
    if ( rc_t rc = VDBManagerDeleteCacheOlderThan(*this, days) ) {
        NCBI_THROW2(CSraException, eOtherError,
                    "CVDBMgr: Cannot delete old cache files", rc);
    }
}


void CVDBMgr::CommitConfig() const
{
    CKConfig(*this).Commit();
}


/////////////////////////////////////////////////////////////////////////////
// CVDB

CVDB::CVDB(const CVDBMgr& mgr, const string& acc_or_path)
    : m_Name(acc_or_path)
{
    DECLARE_SDK_GUARD();
    string path = CVPath::ConvertAccOrSysPathToPOSIX(acc_or_path);
    if ( rc_t rc = VDBManagerOpenDBRead(mgr, x_InitPtr(), 0, "%.*s",
                                        int(path.size()), path.data()) ) {
        *x_InitPtr() = 0;
        if ( (GetRCObject(rc) == RCObject(rcDirectory) ||
              GetRCObject(rc) == RCObject(rcPath) ||
              GetRCObject(rc) == RCObject(rcFile)) &&
             GetRCState(rc) == rcNotFound ) {
            // no SRA accession
            NCBI_THROW2_FMT(CSraException, eNotFoundDb,
                            "Cannot open VDB: "<<acc_or_path, rc);
        }
        else if ( GetRCObject(rc) == rcName &&
                  GetRCState(rc) == rcNotFound &&
                  GetRCContext(rc) == rcResolving ) {
            // invalid SRA database
            NCBI_THROW2_FMT(CSraException, eNotFoundDb,
                            "Cannot open VDB: "<<acc_or_path, rc);
        }
        else if ( GetRCObject(rc) == RCObject(rcFile) &&
                  GetRCState(rc) == rcUnauthorized ) {
            // invalid SRA database
            NCBI_THROW2_FMT(CSraException, eProtectedDb,
                            "Cannot open VDB: "<<acc_or_path, rc);
        }
        else if ( GetRCObject(rc) == RCObject(rcDatabase) &&
                  GetRCState(rc) == rcIncorrect ) {
            // invalid SRA database
            NCBI_THROW2_FMT(CSraException, eDataError,
                            "Cannot open VDB: "<<acc_or_path, rc);
        }
        else {
            // other errors
            NCBI_THROW2_FMT(CSraException, eOtherError,
                            "Cannot open VDB: "<<acc_or_path, rc);
        }
    }
}


CNcbiOstream& CVDB::PrintFullName(CNcbiOstream& out) const
{
    return out << GetName();
}


/////////////////////////////////////////////////////////////////////////////
// CVDBTable

static inline
CNcbiOstream& operator<<(CNcbiOstream& out, const CVDBTable& obj)
{
    return obj.PrintFullName(out);
}


CVDBTable::CVDBTable(const CVDB& db,
                     const char* table_name,
                     EMissing missing)
    : m_Db(db),
      m_Name(table_name)
{
    DECLARE_SDK_GUARD();
    if ( rc_t rc = VDatabaseOpenTableRead(db, x_InitPtr(), table_name) ) {
        *x_InitPtr() = 0;
        RCState rc_state = GetRCState(rc);
        int rc_object = GetRCObject(rc);
        if ( rc_state == rcNotFound &&
             (rc_object == rcParam ||
              rc_object == rcPath) ) {
            // missing table in the DB
            if ( missing != eMissing_Throw ) {
                return;
            }
            NCBI_THROW2_FMT(CSraException, eNotFoundTable,
                            "Cannot open VDB table: "<<*this, rc);
        }
        else {
            // other errors
            NCBI_THROW2_FMT(CSraException, eOtherError,
                            "Cannot open VDB table: "<<*this, rc);
        }
    }
}


CVDBTable::CVDBTable(const CVDBMgr& mgr,
                     const string& acc_or_path,
                     EMissing missing)
    : m_Name(acc_or_path)
{
    *x_InitPtr() = 0;
    VSchema *schema;
    DECLARE_SDK_GUARD();
    if ( rc_t rc = SRASchemaMake(&schema, mgr) ) {
        NCBI_THROW2(CSraException, eInitFailed,
                    "Cannot make default SRA schema", rc);
    }
    string path = CVPath::ConvertAccOrSysPathToPOSIX(acc_or_path);
    if ( rc_t rc = VDBManagerOpenTableRead(mgr, x_InitPtr(), schema, "%.*s",
                                           int(path.size()), path.data()) ) {
        *x_InitPtr() = 0;
        VSchemaRelease(schema);
        if ( (GetRCObject(rc) == RCObject(rcDirectory) ||
              GetRCObject(rc) == RCObject(rcPath)) &&
             GetRCState(rc) == rcNotFound ) {
            // no SRA accession
            if ( missing != eMissing_Throw ) {
                return;
            }
            NCBI_THROW2_FMT(CSraException, eNotFoundTable,
                            "Cannot open SRA table: "<<acc_or_path, rc);
        }
        else if ( GetRCObject(rc) == RCObject(rcDatabase) &&
                  GetRCState(rc) == rcIncorrect ) {
            // invalid SRA database
            NCBI_THROW2_FMT(CSraException, eDataError,
                            "Cannot open SRA table: "<<acc_or_path, rc);
        }
        else {
            // other errors
            NCBI_THROW2_FMT(CSraException, eOtherError,
                            "Cannot open SRA table: "<<acc_or_path, rc);
        }
    }
    VSchemaRelease(schema);
}


string CVDBTable::GetFullName(void) const
{
    string ret;
    if ( GetDb() ) {
        ret = GetDb().GetFullName();
        ret += '.';
    }
    ret += GetName();
    return ret;
}


CNcbiOstream& CVDBTable::PrintFullName(CNcbiOstream& out) const
{
    if ( GetDb() ) {
        GetDb().PrintFullName(out) << '.';
    }
    return out << GetName();
}


/////////////////////////////////////////////////////////////////////////////
// CVDBTableIndex

static inline
CNcbiOstream& operator<<(CNcbiOstream& out, const CVDBTableIndex& obj)
{
    return obj.PrintFullName(out);
}


CVDBTableIndex::CVDBTableIndex(const CVDBTable& table,
                               const char* index_name,
                               EMissing missing)
    : m_Table(table),
      m_Name(index_name)
{
    if ( rc_t rc = VTableOpenIndexRead(table, x_InitPtr(), index_name) ) {
        *x_InitPtr() = 0;
        if ( GetRCObject(rc) == RCObject(rcIndex) &&
             GetRCState(rc) == rcNotFound ) {
            // no such index
            if ( missing != eMissing_Throw ) {
                return;
            }
            NCBI_THROW2_FMT(CSraException, eNotFoundIndex,
                            "Cannot open VDB table index: "<<*this, rc);
        }
        else {
            NCBI_THROW2_FMT(CSraException, eOtherError,
                            "Cannot open VDB table index: "<<*this, rc);
        }
    }
}


string CVDBTableIndex::GetFullName(void) const
{
    return GetTable().GetFullName()+'.'+GetName();
}


CNcbiOstream& CVDBTableIndex::PrintFullName(CNcbiOstream& out) const
{
    return GetTable().PrintFullName(out) << '.' << GetName();
}


TVDBRowIdRange CVDBTableIndex::Find(const string& value) const
{
    TVDBRowIdRange range;
    if ( rc_t rc = KIndexFindText(*this, value.c_str(),
                                  &range.first, &range.second, 0, 0) ) {
        if ( GetRCObject(rc) == RCObject(rcString) &&
             GetRCState(rc) == rcNotFound ) {
            // no such value
            range.first = range.second = 0;
        }
        else {
            NCBI_THROW2_FMT(CSraException, eOtherError,
                            "Cannot find value in index: "<<*this<<": "<<value,
                            rc);
        }
    }
    return range;
}


/////////////////////////////////////////////////////////////////////////////
// CVDBCursor

static inline
CNcbiOstream& operator<<(CNcbiOstream& out, const CVDBCursor& obj)
{
    return out << obj.GetTable();
}


void CVDBCursor::Init(const CVDBTable& table)
{
    if ( *this ) {
        NCBI_THROW2(CSraException, eInvalidState,
                    "Cannot init VDB cursor again",
                    RC(rcApp, rcCursor, rcConstructing, rcSelf, rcOpen));
    }
    if ( rc_t rc = VTableCreateCursorRead(table, x_InitPtr()) ) {
        *x_InitPtr() = 0;
        NCBI_THROW2(CSraException, eInitFailed,
                    "Cannot create VDB cursor", rc);
    }
    if ( rc_t rc = VCursorPermitPostOpenAdd(*this) ) {
        NCBI_THROW2(CSraException, eInitFailed,
                    "Cannot allow VDB cursor post open column add", rc);
    }
    if ( rc_t rc = VCursorOpen(*this) ) {
        NCBI_THROW2(CSraException, eInitFailed,
                    "Cannot open VDB cursor", rc);
    }
    m_Table = table;
}


void CVDBCursor::CloseRow(void)
{
    if ( !RowIsOpened() ) {
        return;
    }
    if ( rc_t rc = VCursorCloseRow(*this) ) {
        NCBI_THROW2(CSraException, eInitFailed,
                    "Cannot close VDB cursor row", rc);
    }
    m_RowOpened = false;
}


rc_t CVDBCursor::OpenRowRc(TVDBRowId row_id)
{
    CloseRow();
    if ( rc_t rc = VCursorSetRowId(*this, row_id) ) {
        return rc;
    }
    if ( rc_t rc = VCursorOpenRow(*this) ) {
        return rc;
    }
    m_RowOpened = true;
    return 0;
}


void CVDBCursor::OpenRow(TVDBRowId row_id)
{
    if ( rc_t rc = OpenRowRc(row_id) ) {
        NCBI_THROW2_FMT(CSraException, eInitFailed,
                        "Cannot open VDB cursor row: "<<*this<<": "<<row_id,
                        rc);
    }
}


TVDBRowIdRange CVDBCursor::GetRowIdRange(TVDBColumnIdx column) const
{
    TVDBRowIdRange ret;
    if ( rc_t rc = VCursorIdRange(*this, column, &ret.first, &ret.second) ) {
        NCBI_THROW2_FMT(CSraException, eInitFailed,
                        "Cannot get VDB cursor row range: "<<*this<<": "<<column,
                        rc);
    }
    return ret;
}


TVDBRowId CVDBCursor::GetMaxRowId(void) const
{
    TVDBRowIdRange range = GetRowIdRange();
    return range.first+range.second-1;
}


void CVDBCursor::SetParam(const char* name, const CTempString& value) const
{
    if ( rc_t rc = VCursorParamsSet
         ((struct VCursorParams *)GetPointer(),
          name, "%.*s", value.size(), value.data()) ) {
        NCBI_THROW2_FMT(CSraException, eNotFound,
                        "Cannot set VDB cursor param: "<<*this<<": "<<name,
                        rc);
    }
}


uint32_t CVDBCursor::GetElementCount(TVDBRowId row, const CVDBColumn& column,
                                     uint32_t elem_bits) const
{
    DECLARE_SDK_GET_GUARD();
    uint32_t read_count, remaining_count;
    if ( rc_t rc = VCursorReadBitsDirect(*this, row, column.GetIndex(),
                                         elem_bits, 0, 0, 0, 0,
                                         &read_count, &remaining_count) ) {
        NCBI_THROW2_FMT(CSraException, eNotFoundValue,
                        "Cannot read VDB value array size: "<<*this<<column<<
                        '['<<row<<']', rc);
    }
    return remaining_count;
}


void CVDBCursor::ReadElements(TVDBRowId row, const CVDBColumn& column,
                              uint32_t elem_bits,
                              uint32_t start, uint32_t count,
                              void* buffer) const
{
    DECLARE_SDK_GET_GUARD();
    uint32_t read_count, remaining_count;
    if ( rc_t rc = VCursorReadBitsDirect(*this, row, column.GetIndex(),
                                         elem_bits, start, buffer, 0, count,
                                         &read_count, &remaining_count) ) {
        NCBI_THROW2_FMT(CSraException, eNotFoundValue,
                        "Cannot read VDB value array: "<<*this<<column<<
                        '['<<row<<"]["<<start<<".."<<(start+count-1)<<']', rc);
    }
    if ( read_count != count ) {
        NCBI_THROW_FMT(CSraException, eNotFoundValue,
                       "Cannot read VDB value array: "<<*this<<column<<
                       '['<<row<<"]["<<start<<".."<<(start+count-1)<<
                       "] only "<<read_count<<" elements are read");
    }
}


/////////////////////////////////////////////////////////////////////////////
// CVDBObjectCache

static const size_t kCacheSize = 7;


CVDBObjectCacheBase::CVDBObjectCacheBase(void)
{
    m_Objects.reserve(kCacheSize);
}


CVDBObjectCacheBase::~CVDBObjectCacheBase(void)
{
}


DEFINE_STATIC_FAST_MUTEX(sm_CacheMutex);


void CVDBObjectCacheBase::Clear(void)
{
    CFastMutexGuard guard(sm_CacheMutex);
    m_Objects.clear();
}


CObject* CVDBObjectCacheBase::Get(TVDBRowId row)
{
    CFastMutexGuard guard(sm_CacheMutex);
    if ( m_Objects.empty() ) {
        return 0;
    }
    TObjects::iterator best_it;
    TVDBRowId best_d = numeric_limits<TVDBRowId>::max();
    NON_CONST_ITERATE ( TObjects, it, m_Objects ) {
        TVDBRowId slot_row = it->first;
        if ( slot_row >= row ) {
            TVDBRowId d = slot_row - row;
            if ( d <= best_d ) {
                best_d = d;
                best_it = it;
            }
        }
        else {
            TVDBRowId d = row - slot_row;
            if ( d < best_d ) {
                best_d = d;
                best_it = it;
            }
        }
    }
    CObject* obj = best_it->second.Release();
    *best_it = m_Objects.back();
    m_Objects.pop_back();
    _ASSERT(!obj->Referenced());
    return obj;
}


void CVDBObjectCacheBase::Put(CObject* obj, TVDBRowId row)
{
    if ( obj->Referenced() ) {
        return;
    }
    //row = 0;
    CFastMutexGuard guard(sm_CacheMutex);
    if ( m_Objects.size() < kCacheSize ) {
        m_Objects.push_back(TSlot());
        m_Objects.back().first = row;
        m_Objects.back().second = obj;
    }
    else {
        CRef<CObject> ref(obj); // delete the object
    }
}


/////////////////////////////////////////////////////////////////////////////
// CVDBColumn

static inline
CNcbiOstream& operator<<(CNcbiOstream& out, const CVDBColumn& column)
{
    return out << '.' << column.GetName();
}


void CVDBColumn::Init(const CVDBCursor& cursor,
                      size_t element_bit_size,
                      const char* name,
                      const char* backup_name,
                      EMissing missing)
{
    DECLARE_SDK_GUARD();
    m_Name = name;
    if ( rc_t rc = VCursorAddColumn(cursor, &m_Index, name) ) {
        if ( backup_name &&
             (rc = VCursorAddColumn(cursor, &m_Index, backup_name)) == 0 ) {
            m_Name = backup_name;
        }
        else {
            m_Index = kInvalidIndex;
            if ( missing == eMissing_Throw ) {
                NCBI_THROW2_FMT(CSraException, eNotFoundColumn,
                                "Cannot get VDB column: "<<cursor<<*this,rc);
            }
            else {
                return;
            }
        }
    }
    if ( element_bit_size ) {
        VTypedesc type;
        if ( rc_t rc = VCursorDatatype(cursor, m_Index, 0, &type) ) {
            NCBI_THROW2_FMT(CSraException, eInvalidState,
                            "Cannot get VDB column type: "<<cursor<<*this,rc);
        }
        size_t size = type.intrinsic_bits*type.intrinsic_dim;
        if ( size != element_bit_size ) {
            ERR_POST_X(1, "Wrong VDB column size "<<cursor<<*this<<
                       " expected "<<element_bit_size<<" bits != "<<
                       type.intrinsic_dim<<"*"<<type.intrinsic_bits<<" bits");
            NCBI_THROW2_FMT(CSraException, eInvalidState,
                            "Wrong VDB column size: "<<cursor<<*this<<": "<<size,
                            RC(rcApp, rcColumn, rcConstructing, rcSelf, rcIncorrect));
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
// CVDBValue

CNcbiOstream& CVDBValue::SSaveRef::PrintFullName(CNcbiOstream& out) const
{
    if ( m_Table ) {
        m_Table->PrintFullName(out);
    }
    if ( m_Row ) {
        out << '[' << m_Row << ']';
    }
    if ( m_ColumnName ) {
        out << '.' << m_ColumnName;
    }
    return out;
}


static inline
CNcbiOstream& operator<<(CNcbiOstream& out, const CVDBValue& obj)
{
    return obj.PrintFullName(out);
}


void CVDBValue::x_Get(const CVDBCursor& cursor, const CVDBColumn& column)
{
    DECLARE_SDK_GET_GUARD();
    uint32_t bit_offset, bit_length;
    if ( rc_t rc = VCursorCellData(cursor, column.GetIndex(),
                                   &bit_length, &m_Data, &bit_offset,
                                   &m_ElemCount) ) {
        NCBI_THROW2_FMT(CSraException, eNotFoundValue,
                        "Cannot read VDB value: "<<cursor<<column, rc);
    }
    if ( bit_offset ) {
        NCBI_THROW2_FMT(CSraException, eInvalidState,
                        "Cannot read VDB value with non-zero bit offset: "
                        <<cursor<<column<<": "<<bit_offset,
                        RC(rcApp, rcColumn, rcDecoding, rcOffset, rcUnsupported));
    }
    m_Ref.Set(cursor, 0, column);
}


void CVDBValue::x_Get(const CVDBCursor& cursor,
                      TVDBRowId row,
                      const CVDBColumn& column,
                      EMissing missing)
{
    DECLARE_SDK_GET_GUARD();
    uint32_t bit_offset, bit_length;
    if ( rc_t rc = VCursorCellDataDirect(cursor, row, column.GetIndex(),
                                         &bit_length, &m_Data, &bit_offset,
                                         &m_ElemCount) ) {
        if ( missing != eMissing_Throw ) {
            m_Data = 0;
            m_ElemCount = 0;
            return;
        }
        NCBI_THROW2_FMT(CSraException, eNotFoundValue,
                        "Cannot read VDB value: "<<cursor<<column<<'['<<row<<']', rc);
    }
    if ( bit_offset ) {
        NCBI_THROW2_FMT(CSraException, eInvalidState,
                        "Cannot read VDB value with non-zero bit offset: "<<
                        cursor<<column<<'['<<row<<"]: "<<bit_offset,
                        RC(rcApp, rcColumn, rcDecoding, rcOffset, rcUnsupported));
    }
    m_Ref.Set(cursor, row, column);
}


void CVDBValue::x_ReportIndexOutOfBounds(size_t index) const
{
    if ( index >= size() ) {
        NCBI_THROW2_FMT(CSraException, eInvalidIndex,
                        "Invalid index for VDB value array: "<<
                        *this<<'['<<index<<']',
                        RC(rcApp, rcData, rcRetrieving, rcOffset, rcTooBig));
    }
}


void CVDBValue::x_ReportNotOneValue(void) const
{
    if ( size() != 1 ) {
        NCBI_THROW2_FMT(CSraException, eDataError,
                        "VDB value array doen't have single value: "<<
                        *this<<'['<<size()<<']',
                        RC(rcApp, rcData, rcRetrieving, rcSize, rcIncorrect));
    }
}


void CVDBValue::x_CheckRange(size_t pos, size_t len) const
{
    if ( pos > size() ) {
        NCBI_THROW2_FMT(CSraException, eInvalidIndex,
                        "Invalid index for VDB value array: "<<
                        *this<<'['<<pos<<']',
                        RC(rcApp, rcData, rcRetrieving, rcOffset, rcTooBig));
    }
    if ( pos+len < pos ) {
        NCBI_THROW2_FMT(CSraException, eInvalidIndex,
                        "Invalid length for VDB value sub-array: "<<
                        *this<<'['<<pos<<','<<len<<']',
                        RC(rcApp, rcData, rcRetrieving, rcOffset, rcTooBig));
    }
    if ( pos+len > size() ) {
        NCBI_THROW2_FMT(CSraException, eInvalidIndex,
                        "Invalid end of VDB value sub-array: "<<
                        *this<<'['<<pos<<','<<len<<']',
                        RC(rcApp, rcData, rcRetrieving, rcOffset, rcTooBig));
    }
}


static inline
CNcbiOstream& operator<<(CNcbiOstream& out, const CVDBValueFor4Bits& obj)
{
    return obj.PrintFullName(out);
}


void CVDBValueFor4Bits::x_Get(const CVDBCursor& cursor,
                              TVDBRowId row,
                              const CVDBColumn& column)
{
    DECLARE_SDK_GET_GUARD();
    uint32_t bit_offset, bit_length, elem_count;
    const void* data;
    if ( rc_t rc = VCursorCellDataDirect(cursor, row, column.GetIndex(),
                                         &bit_length, &data, &bit_offset,
                                         &elem_count) ) {
        NCBI_THROW2_FMT(CSraException, eNotFoundValue,
                        "Cannot read VDB 4-bits value array: "<<
                        cursor<<column<<'['<<row<<']', rc);
    }
    if ( bit_offset >= 8 || (bit_offset&3) ) {
        NCBI_THROW2_FMT(CSraException, eInvalidState,
                        "Cannot read VDB 4-bits value array with odd bit offset"<<
                        cursor<<column<<'['<<row<<"]: "<<bit_offset,
                        RC(rcApp, rcColumn, rcDecoding, rcOffset, rcUnsupported));
    }
    m_RawData = static_cast<const char*>(data);
    m_ElemOffset = bit_offset >> 2;
    m_ElemCount = elem_count;
    m_Ref.Set(cursor, row, column);
}


void CVDBValueFor4Bits::x_ReportIndexOutOfBounds(size_t index) const
{
    if ( index >= size() ) {
        NCBI_THROW2_FMT(CSraException, eInvalidIndex,
                        "Invalid index for VDB 4-bits value array: "<<
                        *this<<'['<<index<<']',
                        RC(rcApp, rcData, rcRetrieving, rcOffset, rcTooBig));
    }
}


inline
void CVDBValueFor4Bits::x_CheckRange(size_t pos, size_t len) const
{
    if ( pos > size() ) {
        NCBI_THROW2_FMT(CSraException, eInvalidIndex,
                        "Invalid index for VDB 4-bits value array: "<<
                        *this<<'['<<pos<<']',
                        RC(rcApp, rcData, rcRetrieving, rcOffset, rcTooBig));
    }
    if ( pos+len < pos ) {
        NCBI_THROW2_FMT(CSraException, eInvalidIndex,
                        "Invalid length for VDB 4-bits value sub-array: "<<
                        *this<<'['<<pos<<','<<len<<']',
                        RC(rcApp, rcData, rcRetrieving, rcOffset, rcTooBig));
    }
    if ( pos+len > size() ) {
        NCBI_THROW2_FMT(CSraException, eInvalidIndex,
                        "Invalid end of VDB 4-bits value sub-array: "<<
                        *this<<'['<<pos<<','<<len<<']',
                        RC(rcApp, rcData, rcRetrieving, rcOffset, rcTooBig));
    }
}


CVDBValueFor4Bits CVDBValueFor4Bits::substr(size_t pos, size_t len) const
{
    x_CheckRange(pos, len);
    size_t offset = m_ElemOffset + pos;
    const char* raw_data = m_RawData + offset/2;
    offset %= 2;
    // limits are checked above
    return CVDBValueFor4Bits(m_Ref, raw_data, uint32_t(offset), uint32_t(len));
}


static inline
CNcbiOstream& operator<<(CNcbiOstream& out, const CVDBValueFor2Bits& obj)
{
    return obj.PrintFullName(out);
}


void CVDBValueFor2Bits::x_Get(const CVDBCursor& cursor,
                              TVDBRowId row,
                              const CVDBColumn& column)
{
    DECLARE_SDK_GET_GUARD();
    uint32_t bit_offset, bit_length, elem_count;
    const void* data;
    if ( rc_t rc = VCursorCellDataDirect(cursor, row, column.GetIndex(),
                                         &bit_length, &data, &bit_offset,
                                         &elem_count) ) {
        NCBI_THROW2_FMT(CSraException, eNotFoundValue,
                        "Cannot read VDB 2-bits value array: "<<
                        cursor<<column<<'['<<row<<']', rc);
    }
    if ( bit_offset >= 8 || (bit_offset&1) ) {
        NCBI_THROW2_FMT(CSraException, eInvalidState,
                        "Cannot read VDB 2-bits value array with odd bit offset"<<
                        cursor<<column<<'['<<row<<"]: "<<bit_offset,
                        RC(rcApp, rcColumn, rcDecoding, rcOffset, rcUnsupported));
    }
    m_RawData = static_cast<const char*>(data);
    m_ElemOffset = bit_offset >> 1;
    m_ElemCount = elem_count;
    m_Ref.Set(cursor, row, column);
}


void CVDBValueFor2Bits::x_ReportIndexOutOfBounds(size_t index) const
{
    if ( index >= size() ) {
        NCBI_THROW2_FMT(CSraException, eInvalidIndex,
                        "Invalid index for VDB 2-bits value array: "<<
                        *this<<'['<<index<<']',
                        RC(rcApp, rcData, rcRetrieving, rcOffset, rcTooBig));
    }
}


inline
void CVDBValueFor2Bits::x_CheckRange(size_t pos, size_t len) const
{
    if ( pos > size() ) {
        NCBI_THROW2_FMT(CSraException, eInvalidIndex,
                        "Invalid index for VDB 2-bits value array: "<<
                        *this<<'['<<pos<<']',
                        RC(rcApp, rcData, rcRetrieving, rcOffset, rcTooBig));
    }
    if ( pos+len < pos ) {
        NCBI_THROW2_FMT(CSraException, eInvalidIndex,
                        "Invalid length for VDB 2-bits value sub-array: "<<
                        *this<<'['<<pos<<','<<len<<']',
                        RC(rcApp, rcData, rcRetrieving, rcOffset, rcTooBig));
    }
    if ( pos+len > size() ) {
        NCBI_THROW2_FMT(CSraException, eInvalidIndex,
                        "Invalid end of VDB 2-bits value sub-array: "<<
                        *this<<'['<<pos<<','<<len<<']',
                        RC(rcApp, rcData, rcRetrieving, rcOffset, rcTooBig));
    }
}


CVDBValueFor2Bits CVDBValueFor2Bits::substr(size_t pos, size_t len) const
{
    x_CheckRange(pos, len);
    size_t offset = m_ElemOffset + pos;
    const char* raw_data = m_RawData + offset/4;
    offset %= 4;
    // limits are checked above
    return CVDBValueFor2Bits(m_Ref, raw_data, uint32_t(offset), uint32_t(len));
}


END_SCOPE(objects)
END_NCBI_SCOPE
