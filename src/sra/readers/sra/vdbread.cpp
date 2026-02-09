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
#include <klib/sra-release-version.h>
#include <kfg/config.h>
#include <kdb/manager.h>
#include <kdb/database.h>
#include <kdb/kdb-priv.h>
#include <kns/manager.h>
#include <kns/http.h>
#include <kns/tls.h>

#include <vfs/manager-priv.h>
#include <vfs/manager.h>
#include <vfs/path.h>
#include <vfs/resolver.h>

#include <sra/sradb-priv.h>

#include <vdb/vdb-priv.h>
#include <vdb/manager.h>
#include <vdb/database.h>
#include <vdb/schema.h>
#include <vdb/table.h>
#include <vdb/cursor.h>


#include <corelib/ncbimtx.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_param.hpp>
#include <corelib/request_ctx.hpp>
#include <sra/readers/ncbi_traces_path.hpp>
#include <objects/general/general__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqset/seqset__.hpp>
#include <objects/seqres/seqres__.hpp>
#include <sra/error_codes.hpp>
#include <util/random_gen.hpp>

#include <cstring>
#include <algorithm>
#include <thread>

BEGIN_NCBI_SCOPE

#define NCBI_USE_ERRCODE_X   VDBReader
NCBI_DEFINE_ERR_SUBCODE_X(9);

BEGIN_SCOPE(objects)

class CSeq_entry;


NCBI_PARAM_DECL(int, VDB, DIAG_HANDLER);
NCBI_PARAM_DEF(int, VDB, DIAG_HANDLER, 1);


static int s_GetDiagHandler(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(VDB, DIAG_HANDLER)> s_Value;
    return s_Value->Get();
}



NCBI_PARAM_DECL(int, VDB, DEBUG);
NCBI_PARAM_DEF_EX(int, VDB, DEBUG, 0, eParam_NoThread, VDB_DEBUG);


static int& s_GetDebugLevelVar(void)
{
    static int value = NCBI_PARAM_TYPE(VDB, DEBUG)::GetDefault();
    return value;
}


inline
static int s_GetDebugLevel()
{
    return s_GetDebugLevelVar();
}


inline
static int s_SetDebugLevel(int new_level)
{
    auto& var = s_GetDebugLevelVar();
    auto old_level = var;
    var = new_level;
    return old_level;
}


NCBI_PARAM_DECL(bool, VDB, DISABLE_PAGEMAP_THREAD);
NCBI_PARAM_DEF(bool, VDB, DISABLE_PAGEMAP_THREAD, false);



DEFINE_SRA_REF_TRAITS(VDBManager, const);
DEFINE_SRA_REF_TRAITS(VDatabase, const);
DEFINE_SRA_REF_TRAITS(VTable, const);
DEFINE_SRA_REF_TRAITS(VCursor, const);
DEFINE_SRA_REF_TRAITS(KIndex, const);
DEFINE_SRA_REF_TRAITS(KConfig, );
DEFINE_SRA_REF_TRAITS(KDBManager, const);
DEFINE_SRA_REF_TRAITS(KNSManager, );
DEFINE_SRA_REF_TRAITS(VFSManager, );
DEFINE_SRA_REF_TRAITS(VPath, const);
DEFINE_SRA_REF_TRAITS(VResolver, );

//#define VDB_RANDOM_FAILS 1

#define FAILS_VAR(name, suffix) NCBI_NAME3(VDB_, name, suffix)

#define VDB_RESOLVE_RANDOM_FAILS 1
#define VDB_OPEN_RANDOM_FAILS 1
#define VDB_SCHEMA_RANDOM_FAILS 1
#define VDB_READ_RANDOM_FAILS 1

#define VDB_RESOLVE_RANDOM_FAILS_FREQUENCY 20
#define VDB_RESOLVE_RANDOM_FAILS_RECOVER 10
#define VDB_OPEN_RANDOM_FAILS_FREQUENCY 10
#define VDB_OPEN_RANDOM_FAILS_RECOVER 5
#define VDB_SCHEMA_RANDOM_FAILS_FREQUENCY 50
#define VDB_SCHEMA_RANDOM_FAILS_RECOVER 100
#define VDB_READ_RANDOM_FAILS_FREQUENCY 20000
#define VDB_READ_RANDOM_FAILS_RECOVER 1000

#if defined(VDB_RANDOM_FAILS)
static bool s_Fails(unsigned frequency)
{
    static CSafeStatic<CRandom> s_Random([]()->CRandom*{CRandom* r=new CRandom();r->Randomize();return r;}, nullptr);
    return s_Random->GetRandIndex(frequency) == 0;
}
# define SIMULATE_ERROR(name)                                           \
    if ( !(FAILS_VAR(name, _RANDOM_FAILS)) ) {                          \
    }                                                                   \
    else {                                                              \
        static thread_local unsigned recover_counter = 0;               \
        if ( recover_counter > 0 ) {                                    \
            --recover_counter;                                          \
        }                                                               \
        else {                                                          \
            if ( s_Fails(FAILS_VAR(name, _RANDOM_FAILS_FREQUENCY)) ) {  \
                recover_counter = (FAILS_VAR(name, _RANDOM_FAILS_RECOVER)); \
                ERR_POST("VDB: Simulated " #name " failure: "<<CStackTrace()); \
                NCBI_THROW(CSraException, eOtherError,                  \
                           "Simulated " #name " failure");              \
            }                                                           \
        }                                                               \
    }
#else
# define SIMULATE_ERROR(name) ((void)0)
#endif

#define SIMULATE_RESOLVE_ERROR() SIMULATE_ERROR(RESOLVE)
#define SIMULATE_OPEN_ERROR() SIMULATE_ERROR(OPEN)
#define SIMULATE_SCHEMA_ERROR() SIMULATE_ERROR(SCHEMA)
#define SIMULATE_READ_ERROR() SIMULATE_ERROR(READ)


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
    *x_InitPtr() = const_cast<KConfig*>(VFSManagerGetConfig(CVFSManager(mgr)));
    if ( rc_t rc = KConfigAddRef(*this) ) {
        *x_InitPtr() = 0;
        NCBI_THROW2(CSraException, eInitFailed,
                    "Cannot get reference to KConfig", rc);
    }
}


CKConfig::CKConfig(EMake /*make*/)
{
    if ( rc_t rc = KConfigMake(x_InitPtr(), NULL) ) {
        NCBI_THROW2(CSraException, eInitFailed,
                    "Cannot create KConfig singleton", rc);
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


CKNSManager::CKNSManager(EMake /*make*/)
{
    if ( rc_t rc = KNSManagerMake(x_InitPtr()) ) {
        *x_InitPtr() = 0;
        NCBI_THROW2(CSraException, eInitFailed,
                    "Cannot make KNSManager", rc);
    }
}


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


bool CVPath::IsLocalFile() const
{
    char buffer[32];
    size_t size;
    if ( VPathReadScheme(*this, buffer, sizeof(buffer), &size) != 0 ) {
        return false;
    }
    if ( size == 4 && memcmp(buffer, "file", 4) == 0 ) {
        return true;
    }
    if ( size == 9 && memcmp(buffer, "ncbi-file", 9) == 0 ) {
        return true;
    }
    return false;
}


string CVPath::ToString(EType type) const
{
    const String* str = 0;
    if (type == eSys && IsLocalFile()) {
        if (rc_t rc = VPathMakeSysPath(*this, &str)) {
            NCBI_THROW2(CSraException, eOtherError,
                "Cannot get path from VPath", rc);
        }
    }
    else {
        if (rc_t rc = VPathMakeString(*this, &str)) {
            NCBI_THROW2(CSraException, eOtherError,
                "Cannot get path from VPath", rc);
        }
    }
    string ret(str->addr, str->size);
    StringWhack(str);
    return ret;
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
    SIMULATE_RESOLVE_ERROR();
    CVPath acc(m_Mgr, acc_or_path, CVPath::eAcc);
    const VPath* path;
    rc_t rc = VResolverLocal(*this, acc, &path);
    if ( rc ) {
        if ( GetRCObject(rc) == rcName &&
             GetRCState(rc) == rcNotFound &&
             GetRCContext(rc) == rcResolving ) {
            // continue with other ways to resolve
        }
        else {
            NCBI_THROW2_FMT(CSraException, eOtherError,
                            "VResolverLocal("<<acc_or_path<<") failed", rc);
        }
        rc = VResolverRemote(*this, eProtocolNone, acc, &path);
    }
    if ( rc ) {
        CHECK_VDB_TIMEOUT_FMT("Cannot find acc path: "<<acc_or_path, rc);
        if ( GetRCObject(rc) == rcName &&
             GetRCState(rc) == rcNotFound &&
             GetRCContext(rc) == rcResolving ) {
            // continue with other ways to resolve
        }
        else {
            NCBI_THROW2_FMT(CSraException, eOtherError,
                            "VResolverRemote("<<acc_or_path<<") failed", rc);
        }
        if ( CDirEntry(acc_or_path).Exists() ) {
            // local file
            return acc_or_path;
        }
        // couldnt resolve with any way
        NCBI_THROW2_FMT(CSraException, eNotFoundDb,
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


int CVDBMgr::GetDebugLevel()
{
    return s_GetDebugLevel();
}


int CVDBMgr::SetDebugLevel(int new_level)
{
    return s_SetDebugLevel(new_level);
}


string CVDBMgr::FindAccPath(const string& acc) const
{
    return m_Resolver.Resolve(acc);
}


string CVDBMgr::FindDereferencedAccPath(const string& acc_or_path) const
{
    string path;
    if ( CVPath::IsPlainAccession(acc_or_path) ) {
        // resolve VDB accessions
        if ( s_GetDebugLevel() >= CVDBMgr::eDebugResolve ) {
            LOG_POST_X(6, "CVDBMgr: resolving VDB accession: " << acc_or_path);
        }
        path = FindAccPath(acc_or_path);
        if ( s_GetDebugLevel() >= CVDBMgr::eDebugResolve ) {
            LOG_POST_X(4, "CVDBMgr: resolved VDB accession: " << acc_or_path << " -> " << path);
        }
    }
    else {
        // real path, http:, etc.
        path = acc_or_path;
    }

    // resolve symbolic links for correct timestamp and longer-living reference
    if ( s_GetDebugLevel() >= CVDBMgr::eDebugResolve ) {
        LOG_POST_X(7, "CVDBMgr: checking file symbolic link: " << acc_or_path << " -> " << path);
    }
    CDirEntry de(path);
    if ( de.Exists() ) {
        de.DereferencePath();
        if ( de.GetPath() != path ) {
            path = de.GetPath();
            if ( s_GetDebugLevel() >= CVDBMgr::eDebugResolve ) {
                LOG_POST_X(5, "CVDBMgr: resolved file symbolic link: " << acc_or_path << " -> " << path);
            }
        }
    }
    return path;
}


CTime CVDBMgr::GetTimestamp(const string& path) const
{
    CTime timestamp;
    if ( path[0] == 'h' &&
         (NStr::StartsWith(path, "http://") ||
          NStr::StartsWith(path, "https://")) ) {
        // try http:
        timestamp = GetURLTimestamp(path);
    }
    else {
        // try direct file access
        if ( !CDirEntry(path).GetTime(&timestamp) ) {
            NCBI_THROW_FMT(CSraException, eInitFailed,
                "Cannot get timestamp of local path: " << path);
        }
        timestamp.ToUniversalTime();
    }
    _ASSERT(!timestamp.IsEmpty());
    _ASSERT(timestamp.IsUniversalTime());
    if ( s_GetDebugLevel() >= CVDBMgr::eDebugResolve ) {
        LOG_POST_X(3, "CVDBMgr: timestamp of " << path << ": " << CTime(timestamp).ToLocalTime());
    }
    return timestamp;
}


DECLARE_SRA_REF_TRAITS(KClientHttpRequest, );
DECLARE_SRA_REF_TRAITS(KClientHttpResult, );

DEFINE_SRA_REF_TRAITS(KClientHttpRequest, );
DEFINE_SRA_REF_TRAITS(KClientHttpResult, );

class NCBI_SRAREAD_EXPORT CClientHttpRequest
    : public CSraRef<KClientHttpRequest>
{
public:
    CClientHttpRequest(const CKNSManager& mgr, const string& path)
    {
        const ver_t kHTTP_1_1 = 0x01010000;
        if ( rc_t rc = KNSManagerMakeClientRequest(mgr, x_InitPtr(),
                                                   kHTTP_1_1, nullptr,
                                                   "%s", path.c_str()) ) {
            *x_InitPtr() = 0;
            CHECK_VDB_TIMEOUT("Cannot create http request", rc);
            NCBI_THROW2(CSraException, eInitFailed,
                        "Cannot create http client request", rc);
        }
    }

private:
};


class NCBI_SRAREAD_EXPORT CClientHttpResult
    : public CSraRef<KClientHttpResult>
{
public:
    enum EHead {
        eHead
    };

    CClientHttpResult(const CClientHttpRequest& request, EHead)
    {
        if ( rc_t rc = KClientHttpRequestHEAD(request, x_InitPtr()) ) {
            *x_InitPtr() = 0;
            CHECK_VDB_TIMEOUT("Cannot get http HEAD", rc);
            NCBI_THROW2(CSraException, eInitFailed,
                        "Cannot get http HEAD", rc);
        }
    }

private:
};


CTime CVDBMgr::GetURLTimestamp(const string& path) const
{
    CVFSManager vfs(*this);
    CKNSManager kns(vfs);
    CClientHttpRequest request(kns, path);
    CClientHttpResult result(request, CClientHttpResult::eHead);
    char buffer[99];
    size_t size;
    if ( rc_t rc = KClientHttpResultGetHeader(result, "Last-Modified",
                                              buffer, sizeof(buffer), &size) ) {
        CHECK_VDB_TIMEOUT("No Last-Modified header in HEAD response", rc);
        NCBI_THROW2(CSraException, eNotFoundValue,
                    "No Last-Modified header in HEAD response", rc);
    }
    CTempString str(buffer, size);
    CTimeFormat fmt("w, d b Y h:m:s Z"); // standard Last-Modified HTTP header format
    return CTime(str, fmt);
}


//#define DISABLE_SRA_SDK_GUARD
#ifndef DISABLE_SRA_SDK_GUARD
# define DECLARE_SDK_GUARD() auto sdk_guard = CSraSDKLocks::GetSDKGuard();
#else
# define DECLARE_SDK_GUARD() 
#endif


//DEFINE_STATIC_MUTEX(s_SDKMutex);
static recursive_mutex s_SDKMutex;

CSraSDKLocks::TSDKMutex& CSraSDKLocks::GetSDKMutex(void)
{
    return s_SDKMutex;
}

CSraSDKLocks::TSDKGuard CSraSDKLocks::GetSDKGuard(void)
{
#ifndef DISABLE_SRA_SDK_GUARD
    return make_unique<TSDKGuardType>(s_SDKMutex);
#else
    return nullptr;
#endif
}

/////////////////////////////////////////////////////////////////////////////
// VDB library initialization code
// similar code is located in bamread.cpp
/////////////////////////////////////////////////////////////////////////////

static char s_VDBVersion[32]; // enough for 255.255.65535-dev4000000000

static
void s_InitVDBVersion()
{
    if ( !s_VDBVersion[0] ) {
        ostringstream s;
        {{ // format VDB version string
            SraReleaseVersion release_version;
            SraReleaseVersionGet(&release_version);
            s << (release_version.version>>24) << '.'
              << ((release_version.version>>16)&0xff) << '.'
              << (release_version.version&0xffff);
            if ( release_version.revision != 0 ||
                 release_version.type != SraReleaseVersion::eSraReleaseVersionTypeFinal ) {
                const char* type = "";
                switch ( release_version.type ) {
                case SraReleaseVersion::eSraReleaseVersionTypeDev:   type = "dev"; break;
                case SraReleaseVersion::eSraReleaseVersionTypeAlpha: type = "a"; break;
                case SraReleaseVersion::eSraReleaseVersionTypeBeta:  type = "b"; break;
                case SraReleaseVersion::eSraReleaseVersionTypeRC:    type = "RC"; break;
                default:                                             type = ""; break;
                }
                s << '-' << type << release_version.revision;
            }
        }}
        string v = s.str();
        if ( !v.empty() ) {
            if ( v.size() >= sizeof(s_VDBVersion) ) {
                v.resize(sizeof(s_VDBVersion)-1);
            }
            copy(v.begin()+1, v.end(), s_VDBVersion+1);
            s_VDBVersion[0] = v[0];
        }
    }
}

struct SVDBSeverityTag {
    const char* tag;
    CNcbiDiag::FManip manip;
};
static const SVDBSeverityTag kSeverityTags[] = {
    { "err:", Error },
    { "int:", Error },
    { "sys:", Error },
    { "info:", Info },
    { "warn:", Warning },
    { "debug:", Trace },
    { "fatal:", Fatal },
};
static const SVDBSeverityTag* s_GetVDBSeverityTag(CTempString token)
{
    if ( !token.empty() && token[token.size()-1] == ':' ) {
        for ( auto& tag : kSeverityTags ) {
            if ( token == tag.tag ) {
                return &tag;
            }
        }
    }
    return 0;
}

#ifndef NCBI_THREADS
static thread::id s_DiagCheckThreadID;
#endif

static inline void s_InitDiagCheck()
{
#ifndef NCBI_THREADS
    s_DiagCheckThreadID = this_thread::get_id();
#endif
}

static inline bool s_DiagIsSafe()
{
#ifndef NCBI_THREADS
    return s_DiagCheckThreadID == this_thread::get_id();
#else
    return true;
#endif
}


static
rc_t VDBLogWriter(void* /*data*/, const char* buffer, size_t size, size_t* written)
{
    CTempString msg(buffer, size);
    NStr::TruncateSpacesInPlace(msg);
    CNcbiDiag::FManip sev_manip = Error;
    
    for ( SIZE_TYPE token_pos = 0, token_end; token_pos < msg.size(); token_pos = token_end + 1 ) {
        token_end = msg.find(' ', token_pos);
        if ( token_end == NPOS ) {
            token_end = msg.size();
        }
        if ( auto tag = s_GetVDBSeverityTag(CTempString(msg, token_pos, token_end-token_pos)) ) {
            sev_manip = tag->manip;
            break;
        }
    }
    if ( sev_manip == Trace ) {
        if ( s_DiagIsSafe() ) {
            _TRACE("VDB "<<s_VDBVersion<<": "<<msg);
        }
    }
    else {
        if ( s_DiagIsSafe() ) {
            ERR_POST_X(2, sev_manip<<"VDB "<<s_VDBVersion<<": "<<msg);
        }
    }
    *written = size;
    return 0;
}


static CKConfig s_InitProxyConfig()
{
    CKConfig config(null);
    if ( CNcbiApplicationGuard app = CNcbiApplication::InstanceGuard() ) {
        string host = app->GetConfig().GetString("CONN", "HTTP_PROXY_HOST", kEmptyStr);
        int port = app->GetConfig().GetInt("CONN", "HTTP_PROXY_PORT", 0);
        if ( !host.empty() && port != 0 ) {
            config = CKConfig(CKConfig::eMake);
            string path = host + ':' + NStr::IntToString(port);
            if ( rc_t rc = KConfigWriteString(config,
                                              "/http/proxy/path", path.c_str()) ) {
                NCBI_THROW2(CSraException, eInitFailed,
                            "Cannot set KConfig proxy path", rc);
            }
            if ( rc_t rc = KConfigWriteBool(config,
                                            "/http/proxy/enabled", true) ) {
                NCBI_THROW2(CSraException, eInitFailed,
                            "Cannot set KConfig proxy enabled", rc);
            }
        }
    }
    return config;
}


static DECLARE_TLS_VAR(const CRequestContext*, s_LastRequestContext);
static DECLARE_TLS_VAR(CRequestContext::TVersion, s_LastRequestContextVersion);

/*
// Performance collecting code
static struct SReport {
    size_t count;
    double time;
    SReport() : count(0), time(0) {}
    ~SReport() {
        LOG_POST("GetRequestContext() called "<<count<<" times, spent "<<time<<"s");
    }
} dummy;
*/

static void s_UpdateVDBRequestContext(void)
{
    DECLARE_SDK_GUARD();
    // Performance collecting code
    // CStopWatch sw(CStopWatch::eStart);
    CRequestContext& req_ctx = CDiagContext::GetRequestContext();
    // Performance collecting code
    // dummy.count += 1; dummy.time += sw.Elapsed();
    auto req_ctx_version = req_ctx.GetVersion();
    if ( &req_ctx == s_LastRequestContext && req_ctx_version == s_LastRequestContextVersion ) {
        return;
    }
    _TRACE("CVDBMgr: Updating request context with version: "<<req_ctx_version);
    s_LastRequestContext = &req_ctx;
    s_LastRequestContextVersion = req_ctx_version;
    CKNSManager kns_mgr(CKNSManager::eMake);
    if ( req_ctx.IsSetSessionID() ) {
        _TRACE("CVDBMgr: Updating session ID: "<<req_ctx.GetSessionID());
        KNSManagerSetSessionID(kns_mgr, req_ctx.GetSessionID().c_str());
    }
    if ( req_ctx.IsSetClientIP() ) {
        _TRACE("CVDBMgr: Updating client IP: "<<req_ctx.GetClientIP());
        KNSManagerSetClientIP(kns_mgr, req_ctx.GetClientIP().c_str());
    }
    if ( req_ctx.IsSetHitID() ) {
        _TRACE("CVDBMgr: Updating hit ID: "<<req_ctx.GetHitID());
        KNSManagerSetPageHitID(kns_mgr, req_ctx.GetHitID().c_str());
    }
}


static void s_InitAllKNS(KNSManager* kns_mgr)
{
    CNcbiApplicationGuard app = CNcbiApplication::InstanceGuard();
    if ( app && app->GetConfig().GetBool("VDB", "ALLOW_ALL_CERTS", false) ) {
        if ( rc_t rc = KNSManagerSetAllowAllCerts(kns_mgr, true) ) {
            NCBI_THROW2(CSraException, eInitFailed,
                        "Cannot enable all HTTPS certificates in KNSManager", rc);
        }
    }
    {{ // set user agent
        CNcbiOstrstream str;
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
        KNSManagerSetUserAgent(kns_mgr, "%s; VDB %s",
                               prefix.c_str(),
                               s_VDBVersion);
    }}
}


static void s_InitStaticKNS(KNSManager* kns_mgr)
{
    s_InitAllKNS(kns_mgr);
}


static void s_InitLocalKNS(KNSManager* kns_mgr)
{
    s_InitAllKNS(kns_mgr);
}


void CVDBMgr::s_VDBInit()
{
    static bool initialized = false;
    if ( !initialized ) {
        s_InitVDBVersion();
        // redirect VDB log to C++ Toolkit
        if ( s_GetDiagHandler() ) {
            KLogInit();
            KLogLevel ask_level;
#ifdef _DEBUG
            ask_level = klogDebug;
#else
            ask_level = klogInfo;
#endif
            s_InitDiagCheck();
            KLogLevelSet(ask_level);
            KLogHandlerSet(VDBLogWriter, 0);
            KLogLibHandlerSet(VDBLogWriter, 0);
            if ( s_GetDebugLevel() >= CVDBMgr::eDebugInitialize ) {
                const char* msg = "info: VDB initialized";
                size_t written;
                VDBLogWriter(0, msg, strlen(msg), &written);
            }
        }
        CKConfig config = s_InitProxyConfig();
        CKNSManager kns_mgr(CKNSManager::eMake);
        s_InitStaticKNS(kns_mgr);
        initialized = true;
    }
}

/////////////////////////////////////////////////////////////////////////////
// end of VDB library initialization code
/////////////////////////////////////////////////////////////////////////////
        

//#define UPDATE_REQUEST_CONTEXT_FINAL

#ifndef UPDATE_REQUEST_CONTEXT_FINAL
static DECLARE_TLS_VAR(size_t, s_RequestContextUpdaterRecursion);
#endif

class CFinalRequestContextUpdater {
public:
    CFinalRequestContextUpdater();
    ~CFinalRequestContextUpdater();
    CFinalRequestContextUpdater(const CFinalRequestContextUpdater&) = delete;
};


CVDBMgr::CRequestContextUpdater::CRequestContextUpdater()
{
#ifndef UPDATE_REQUEST_CONTEXT_FINAL
    size_t r = s_RequestContextUpdaterRecursion;
    s_RequestContextUpdaterRecursion = r+1;
    if ( r != 0 ) {
        return;
    }
    s_UpdateVDBRequestContext();
#endif
}


inline CFinalRequestContextUpdater::CFinalRequestContextUpdater()
{
#ifndef UPDATE_REQUEST_CONTEXT_FINAL
    size_t r = s_RequestContextUpdaterRecursion;
    s_RequestContextUpdaterRecursion = r+1;
    if ( r != 0 ) {
        return;
    }
#endif
    s_UpdateVDBRequestContext();
}


CVDBMgr::CRequestContextUpdater::~CRequestContextUpdater()
{
#ifndef UPDATE_REQUEST_CONTEXT_FINAL
    size_t r = s_RequestContextUpdaterRecursion;
    s_RequestContextUpdaterRecursion = r-1;
#endif
}


inline CFinalRequestContextUpdater::~CFinalRequestContextUpdater()
{
#ifndef UPDATE_REQUEST_CONTEXT_FINAL
    size_t r = s_RequestContextUpdaterRecursion;
    s_RequestContextUpdaterRecursion = r-1;
#endif
}


void CVDBMgr::x_Init(void)
{
    DECLARE_SDK_GUARD();
    s_VDBInit();
    if ( rc_t rc = VDBManagerMakeRead(x_InitPtr(), 0) ) {
        *x_InitPtr() = 0;
        NCBI_THROW2(CSraException, eInitFailed,
                    "Cannot open VDBManager", rc);
    }
    CVFSManager vfs_mgr(*this);
    VFSManagerLogNamesServiceErrors(vfs_mgr, false);
    s_InitLocalKNS(CKNSManager(vfs_mgr));
    if ( NCBI_PARAM_TYPE(VDB, DISABLE_PAGEMAP_THREAD)().Get() ) {
        VDBManagerDisablePagemapThread(*this);
    }
    m_Resolver = CVResolver(vfs_mgr);
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
    return CVPath(ret).ToString(CVPath::eSys);
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
    if ( s_GetDebugLevel() >= CVDBMgr::eDebugOpen ) {
        LOG_POST_X(8, "CVDBMgr: opening VDB: " << acc_or_path);
    }
    DECLARE_SDK_GUARD();
    CFinalRequestContextUpdater ctx_updater;
    string path = CVPath::ConvertAccOrSysPathToPOSIX(acc_or_path);
    SIMULATE_OPEN_ERROR();
    if ( rc_t rc = VDBManagerOpenDBRead(mgr, x_InitPtr(), 0, "%.*s",
                                        int(path.size()), path.data()) ) {
        *x_InitPtr() = 0;
        string msg = "Cannot open VDB: "+acc_or_path;
        CHECK_VDB_TIMEOUT(msg, rc);
        if ( (GetRCObject(rc) == RCObject(rcDirectory) ||
              GetRCObject(rc) == RCObject(rcPath) ||
              GetRCObject(rc) == RCObject(rcFile)) &&
             GetRCState(rc) == rcNotFound ) {
            // no VDB accession
            NCBI_THROW2(CSraException, eNotFoundDb, msg, rc);
        }
        else if ( GetRCObject(rc) == rcName &&
                  GetRCState(rc) == rcNotFound &&
                  GetRCContext(rc) == rcResolving ) {
            // invalid VDB database
            NCBI_THROW2(CSraException, eNotFoundDb, msg, rc);
        }
        else if ( GetRCObject(rc) == RCObject(rcFile) &&
                  GetRCState(rc) == rcUnauthorized ) {
            // no permission to use VDB database
            NCBI_THROW2(CSraException, eProtectedDb, msg, rc);
        }
        else if ( GetRCObject(rc) == RCObject(rcDatabase) &&
                  GetRCState(rc) == rcIncorrect ) {
            // invalid VDB database
            NCBI_THROW2(CSraException, eDataError, msg, rc);
        }
        else {
            // other errors
            NCBI_THROW2(CSraException, eOtherError, msg, rc);
        }
    }
    if ( s_GetDebugLevel() >= CVDBMgr::eDebugOpen ) {
        LOG_POST_X(9, "CVDBMgr: opened VDB: " << acc_or_path << " (" << path << ")");
    }
    if ( 0 ) {
        const KDatabase* kdb = 0;
        if ( rc_t rc = VDatabaseOpenKDatabaseRead(*this, &kdb) ) {
            NCBI_THROW2(CSraException, eOtherError, "VDatabaseOpenKDatabaseRead() failed", rc);
        }
        const char* kdb_path = 0;
        if ( rc_t rc = KDatabaseGetPath(kdb, &kdb_path) ) {
            NCBI_THROW2(CSraException, eOtherError, "KDatabaseGetPath() failed", rc);
        }
        LOG_POST("CVDB("<<acc_or_path<<") -> "<<path<<" -> "<<kdb_path);
        if ( rc_t rc = KDatabaseRelease(kdb) ) {
            NCBI_THROW2(CSraException, eOtherError, "KDatabaseRelease() failed", rc);
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
    //DECLARE_SDK_GUARD();
    CFinalRequestContextUpdater ctx_updater;
    SIMULATE_SCHEMA_ERROR();
    if ( rc_t rc = VDatabaseOpenTableRead(db, x_InitPtr(), table_name) ) {
        *x_InitPtr() = 0;
        string msg = "Cannot open VDB: "+GetFullName();
        CHECK_VDB_TIMEOUT(msg, rc);
        RCState rc_state = GetRCState(rc);
        int rc_object = GetRCObject(rc);
        if ( rc_state == rcNotFound &&
             (rc_object == rcParam ||
              rc_object == rcPath) ) {
            // missing table in the DB
            if ( missing != eMissing_Throw ) {
                return;
            }
            NCBI_THROW2(CSraException, eNotFoundTable, msg, rc);
        }
        else {
            // other errors
            NCBI_THROW2(CSraException, eOtherError, msg, rc);
        }
    }
}


CVDBTable::CVDBTable(const CVDBMgr& mgr,
                     const string& acc_or_path,
                     EMissing missing)
    : m_Name(acc_or_path)
{
    *x_InitPtr() = 0;
    //DECLARE_SDK_GUARD();
    CFinalRequestContextUpdater ctx_updater;
    string path = CVPath::ConvertAccOrSysPathToPOSIX(acc_or_path);
    SIMULATE_OPEN_ERROR();
    if ( rc_t rc = VDBManagerOpenTableRead(mgr, x_InitPtr(), 0, "%.*s",
                                           int(path.size()), path.data()) ) {
        *x_InitPtr() = 0;
        string msg = "Cannot open VDB: "+acc_or_path;
        CHECK_VDB_TIMEOUT(msg, rc);
        if ( (GetRCObject(rc) == RCObject(rcDirectory) ||
              GetRCObject(rc) == RCObject(rcPath)) &&
             GetRCState(rc) == rcNotFound ) {
            // no SRA accession
            if ( missing != eMissing_Throw ) {
                return;
            }
            NCBI_THROW2(CSraException, eNotFoundTable, msg, rc);
        }
        else if ( GetRCObject(rc) == RCObject(rcDatabase) &&
                  GetRCState(rc) == rcIncorrect ) {
            // invalid SRA database
            NCBI_THROW2(CSraException, eDataError, msg, rc);
        }
        else {
            // other errors
            NCBI_THROW2(CSraException, eOtherError, msg, rc);
        }
    }
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
    CFinalRequestContextUpdater ctx_updater;
    SIMULATE_SCHEMA_ERROR();
    if ( rc_t rc = VTableOpenIndexRead(table, x_InitPtr(), index_name) ) {
        *x_InitPtr() = 0;
        string msg = "Cannot open VDB table index: "+GetFullName();
        CHECK_VDB_TIMEOUT(msg, rc);
        if ( GetRCObject(rc) == RCObject(rcIndex) &&
             GetRCState(rc) == rcNotFound ) {
            // no such index
            if ( missing != eMissing_Throw ) {
                return;
            }
            NCBI_THROW2(CSraException, eNotFoundIndex, msg, rc);
        }
        else {
            NCBI_THROW2(CSraException, eOtherError, msg, rc);
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
        CHECK_VDB_TIMEOUT_FMT("Cannot find value in index: "<<*this<<": "<<value, rc);
        if ( GetRCObject(rc) == RCObject(rcString) &&
             GetRCState(rc) == rcNotFound ) {
            // no such value
            range.first = range.second = 0;
        }
        else {
            NCBI_THROW2_FMT(CSraException, eOtherError,
                            "Cannot find value in index: "<<*this<<": "<<value, rc);
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
    CFinalRequestContextUpdater ctx_updater;
    if ( *this ) {
        NCBI_THROW2(CSraException, eInvalidState,
                    "Cannot init VDB cursor again",
                    RC(rcApp, rcCursor, rcConstructing, rcSelf, rcOpen));
    }
    if ( !table ) { // VTableCreateCursorRead lacks check for null table argument
        NCBI_THROW2(CSraException, eNullPtr,
                    "Cannot init VDB cursor",
                    RC(rcApp, rcCursor, rcConstructing, rcTable, rcNull));
    }
    if ( rc_t rc = VTableCreateCursorRead(table, x_InitPtr()) ) {
        *x_InitPtr() = 0;
        CHECK_VDB_TIMEOUT_FMT("Cannot create VDB cursor: "<<table, rc);
        NCBI_THROW2_FMT(CSraException, eInitFailed,
                        "Cannot create VDB cursor: "<<table, rc);
    }
    if ( rc_t rc = VCursorPermitPostOpenAdd(*this) ) {
        NCBI_THROW2_FMT(CSraException, eInitFailed,
                        "Cannot allow VDB cursor post open column add: "<<table, rc);
    }
    if ( rc_t rc = VCursorOpen(*this) ) {
        CHECK_VDB_TIMEOUT_FMT("Cannot open VDB cursor: "<<table, rc);
        NCBI_THROW2_FMT(CSraException, eInitFailed,
                        "Cannot open VDB cursor: "<<table, rc);
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
    CFinalRequestContextUpdater ctx_updater;
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
        CHECK_VDB_TIMEOUT_FMT("Cannot open VDB cursor row: "<<*this<<": "<<row_id, rc);
        NCBI_THROW2_FMT(CSraException, eInitFailed,
                        "Cannot open VDB cursor row: "<<*this<<": "<<row_id, rc);
    }
}


TVDBRowIdRange CVDBCursor::GetRowIdRange(TVDBColumnIdx column) const
{
    TVDBRowIdRange ret;
    if ( rc_t rc = VCursorIdRange(*this, column, &ret.first, &ret.second) ) {
        CHECK_VDB_TIMEOUT_FMT("Cannot get VDB cursor row range: "<<*this<<": "<<column, rc);
        NCBI_THROW2_FMT(CSraException, eInitFailed,
                        "Cannot get VDB cursor row range: "<<*this<<": "<<column, rc);
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
    CFinalRequestContextUpdater ctx_updater;
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
    //DECLARE_SDK_GET_GUARD();
    CFinalRequestContextUpdater ctx_updater;
    uint32_t read_count, remaining_count;
    if ( rc_t rc = VCursorReadBitsDirect(*this, row, column.GetIndex(),
                                         elem_bits, 0, 0, 0, 0,
                                         &read_count, &remaining_count) ) {
        CHECK_VDB_TIMEOUT_FMT("Cannot read VDB value array size: "<<*this<<column<<
                              '['<<row<<']', rc);
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
    //DECLARE_SDK_GET_GUARD();
    CFinalRequestContextUpdater ctx_updater;
    uint32_t read_count, remaining_count;
    if ( rc_t rc = VCursorReadBitsDirect(*this, row, column.GetIndex(),
                                         elem_bits, start, buffer, 0, count,
                                         &read_count, &remaining_count) ) {
        CHECK_VDB_TIMEOUT_FMT("Cannot read VDB value array: "<<*this<<column<<
                              '['<<row<<"]["<<start<<".."<<(start+count-1)<<']', rc);
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
    if ( s_GetDebugLevel() >= CVDBMgr::eDebugData ) {
        LOG_POST(Info<<"VDB "<<*this<<column<<'['<<row<<"]: @"<<start<<" #"<<count);
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
    //DECLARE_SDK_GUARD();
    CFinalRequestContextUpdater ctx_updater;
    if ( !name ) {
        if ( missing == eMissing_Throw ) {
            NCBI_THROW_FMT(CSraException, eInvalidArg,
                           "NULL column name is not allowed"<<cursor<<*this);
        }
        m_Index = kInvalidIndex;
        return;
    }
    m_Name = name;
    //SIMULATE_SCHEMA_ERROR();
    if ( rc_t rc = VCursorAddColumn(cursor, &m_Index, name) ) {
        CHECK_VDB_TIMEOUT_FMT("Cannot get VDB column: "<<cursor<<*this, rc);
        if ( backup_name &&
             (rc = VCursorAddColumn(cursor, &m_Index, backup_name)) == 0 ) {
            m_Name = backup_name;
        }
        else {
            CHECK_VDB_TIMEOUT_FMT("Cannot get VDB column: "<<cursor<<*this, rc);
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
            CHECK_VDB_TIMEOUT_FMT("Cannot get VDB column type: "<<cursor<<*this,rc);
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


bool CVDBColumn::IsStatic(const CVDBCursor& cursor) const
{
    bool static_value;
    if ( rc_t rc = VCursorIsStaticColumn(cursor, GetIndex(), &static_value) ) {
        NCBI_THROW2_FMT(CSraException, eInvalidState,
                        "Cannot get static status of "<<cursor<<*this,rc);
    }
    return static_value;
}


void CVDBColumn::ResetIfAlwaysEmpty(const CVDBCursor& cursor)
{
    try {
        if ( IsStatic(cursor) && CVDBValue(cursor, 1, *this).empty() ) {
            // value is always empty
            Reset();
        }
    }
    catch ( CException& /*ignored*/ ) {
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
    //DECLARE_SDK_GET_GUARD();
    CFinalRequestContextUpdater ctx_updater;
    uint32_t bit_offset, bit_length;
    if ( rc_t rc = VCursorCellData(cursor, column.GetIndex(),
                                   &bit_length, &m_Data, &bit_offset,
                                   &m_ElemCount) ) {
        CHECK_VDB_TIMEOUT_FMT("Cannot read VDB value: "<<cursor<<column, rc);
        NCBI_THROW2_FMT(CSraException, eNotFoundValue,
                        "Cannot read VDB value: "<<cursor<<column, rc);
    }
    if ( bit_offset ) {
        NCBI_THROW2_FMT(CSraException, eInvalidState,
                        "Cannot read VDB value with non-zero bit offset: "
                        <<cursor<<column<<": "<<bit_offset,
                        RC(rcApp, rcColumn, rcDecoding, rcOffset, rcUnsupported));
    }
    if ( s_GetDebugLevel() >= CVDBMgr::eDebugData ) {
        CNcbiOstrstream s;
        if ( bit_length == 8 ) {
            s << '"' << NStr::PrintableString(CTempString((const char*)m_Data, m_ElemCount)) << '"';
        }
        else if ( bit_length == 16 ) {
            for ( uint32_t i = 0; i < m_ElemCount; ++i ) {
                if ( i ) {
                    s << ", ";
                }
                s << ((const int16_t*)m_Data)[i];
            }
        }
        else if ( bit_length == 32 ) {
            for ( uint32_t i = 0; i < m_ElemCount; ++i ) {
                if ( i ) {
                    s << ", ";
                }
                s << ((const int32_t*)m_Data)[i];
            }
        }
        else if ( bit_length == 64 ) {
            for ( uint32_t i = 0; i < m_ElemCount; ++i ) {
                if ( i ) {
                    s << ", ";
                }
                s << ((const int64_t*)m_Data)[i];
            }
        }
        else {
            s << "*** bad bit_length="<<bit_length;
        }
        LOG_POST(Info<<"VDB "<<cursor<<column<<": "<<CNcbiOstrstreamToString(s));
    }
    m_Ref.Set(cursor, 0, column);
}


void CVDBValue::x_Get(const CVDBCursor& cursor,
                      TVDBRowId row,
                      const CVDBColumn& column,
                      EMissing missing)
{
    //DECLARE_SDK_GET_GUARD();
    CFinalRequestContextUpdater ctx_updater;
    uint32_t bit_offset, bit_length;
    if ( rc_t rc = VCursorCellDataDirect(cursor, row, column.GetIndex(),
                                         &bit_length, &m_Data, &bit_offset,
                                         &m_ElemCount) ) {
        CHECK_VDB_TIMEOUT_FMT("Cannot read VDB value: "<<cursor<<column<<'['<<row<<']', rc);
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
    if ( s_GetDebugLevel() >= CVDBMgr::eDebugData ) {
        CNcbiOstrstream s;
        if ( bit_length == 8 ) {
            s << '"' << NStr::PrintableString(CTempString((const char*)m_Data, m_ElemCount)) << '"';
        }
        else if ( bit_length == 16 ) {
            for ( uint32_t i = 0; i < m_ElemCount; ++i ) {
                if ( i ) {
                    s << ", ";
                }
                s << ((const int16_t*)m_Data)[i];
            }
        }
        else if ( bit_length == 32 ) {
            for ( uint32_t i = 0; i < m_ElemCount; ++i ) {
                if ( i ) {
                    s << ", ";
                }
                s << ((const int32_t*)m_Data)[i];
            }
        }
        else if ( bit_length == 64 ) {
            for ( uint32_t i = 0; i < m_ElemCount; ++i ) {
                if ( i ) {
                    s << ", ";
                }
                s << ((const int64_t*)m_Data)[i];
            }
        }
        else {
            s << "*** bad bit_length="<<bit_length;
        }
        LOG_POST(Info<<"VDB "<<cursor<<column<<'['<<row<<"]: "<<CNcbiOstrstreamToString(s));
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
    //DECLARE_SDK_GET_GUARD();
    CFinalRequestContextUpdater ctx_updater;
    uint32_t bit_offset, bit_length, elem_count;
    const void* data;
    if ( rc_t rc = VCursorCellDataDirect(cursor, row, column.GetIndex(),
                                         &bit_length, &data, &bit_offset,
                                         &elem_count) ) {
        CHECK_VDB_TIMEOUT_FMT("Cannot read VDB 4-bits value array: "<<
                              cursor<<column<<'['<<row<<']', rc);
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
    if ( s_GetDebugLevel() >= CVDBMgr::eDebugData ) {
        CNcbiOstrstream s;
        if ( bit_length == 4 ) {
            for ( uint32_t i = 0; i < elem_count; ++i ) {
                if ( i ) {
                    s << ", ";
                }
                s << '?';
            }
        }
        else {
            s << "*** bad bit_length="<<bit_length;
        }
        LOG_POST(Info<<"VDB "<<cursor<<column<<'['<<row<<"]: "<<CNcbiOstrstreamToString(s));
    }
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
    //DECLARE_SDK_GET_GUARD();
    CFinalRequestContextUpdater ctx_updater;
    uint32_t bit_offset, bit_length, elem_count;
    const void* data;
    if ( rc_t rc = VCursorCellDataDirect(cursor, row, column.GetIndex(),
                                         &bit_length, &data, &bit_offset,
                                         &elem_count) ) {
        CHECK_VDB_TIMEOUT_FMT("Cannot read VDB 2-bits value array: "<<
                              cursor<<column<<'['<<row<<']', rc);
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
    if ( s_GetDebugLevel() >= CVDBMgr::eDebugData ) {
        CNcbiOstrstream s;
        if ( bit_length == 2 ) {
            for ( uint32_t i = 0; i < elem_count; ++i ) {
                if ( i ) {
                    s << ", ";
                }
                s << '?';
            }
        }
        else {
            s << "*** bad bit_length="<<bit_length;
        }
        LOG_POST(Info<<"VDB "<<cursor<<column<<'['<<row<<"]: "<<CNcbiOstrstreamToString(s));
    }
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
