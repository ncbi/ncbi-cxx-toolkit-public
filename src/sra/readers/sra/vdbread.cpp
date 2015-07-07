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
#include <kfg/config.h>
#include <kdb/manager.h>
#include <kdb/kdb-priv.h>
#include <kns/manager.h>

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
NCBI_DEFINE_ERR_SUBCODE_X(1);

BEGIN_SCOPE(objects)

class CSeq_entry;


DEFINE_SRA_REF_TRAITS(VDBManager, const);
DEFINE_SRA_REF_TRAITS(VDatabase, const);
DEFINE_SRA_REF_TRAITS(VTable, const);
DEFINE_SRA_REF_TRAITS(VCursor, const);
DEFINE_SRA_REF_TRAITS(KIndex, const);
DEFINE_SRA_REF_TRAITS(KConfig, const);
DEFINE_SRA_REF_TRAITS(KDBManager, const);
DEFINE_SRA_REF_TRAITS(KNSManager, );
DEFINE_SRA_REF_TRAITS(VFSManager, );
DEFINE_SRA_REF_TRAITS(VPath, );
DEFINE_SRA_REF_TRAITS(VPath, const); // for constant path in CVPathRet
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
    if ( type == eSys ) {
        if ( rc_t rc = VFSManagerMakeSysPath(mgr, x_InitPtr(), path.c_str()) ) {
            *x_InitPtr() = 0;
            NCBI_THROW3(CSraException, eInitFailed,
                        "Cannot create sys VPath", rc, path);
        }
    }
    else {
        if ( rc_t rc = VFSManagerMakeAccPath(mgr, x_InitPtr(), path.c_str()) ) {
            *x_InitPtr() = 0;
            NCBI_THROW3(CSraException, eInitFailed,
                        "Cannot create acc VPath", rc, path);
        }
    }
    if ( rc_t rc = VPathGetPath(*this, &m_Str) ) {
        NCBI_THROW3(CSraException, eInitFailed,
                    "Cannot get path from VPath", rc, path);
    }
}


CVPathRet::operator string(void) const
{
    string ret;
    ret.resize(128);
    size_t size = 0;
    while ( rc_t rc = VPathReadPath(*this, &ret[0], ret.size(), &size) ) {
        if ( GetRCState(rc) == rcInsufficient ) {
            ret.resize(ret.size()*2);
        }
        else {
            NCBI_THROW2(CSraException, eInitFailed,
                        "Cannot get path string", rc);
        }
    }
    ret.resize(size);
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
    string path = CDirEntry::CreateAbsolutePath(sys_path);
    replace(path.begin(), path.end(), '\\', '/');
    if ( s_HasWindowsDriveLetter(path) ) {
        // move drive letter from first symbol to second (in place of ':')
        path[1] = toupper(path[0]&0xff);
        // add leading slash
        path[0] = '/';
    }
    return path;
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
    CVPathRet ret;
    rc_t rc = VResolverLocal(*this, acc, ret.x_InitPtr());
    if ( rc ) {
        rc = VResolverRemote(*this, eProtocolHttp, acc, ret.x_InitPtr());
    }
    if ( rc ) {
        if ( CDirEntry(acc_or_path).Exists() ) {
            // local file
            return acc_or_path;
        }
        *ret.x_InitPtr() = 0;
        NCBI_THROW3(CSraException, eNotFound,
                    "Cannot find acc path", rc, acc_or_path);
    }
    return ret;
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
}


CVDB::CVDB(const CVDBMgr& mgr, const string& acc_or_path)
{
    DECLARE_SDK_GUARD();
    string path = CVPath::ConvertAccOrSysPathToPOSIX(acc_or_path);
    if ( rc_t rc = VDBManagerOpenDBRead(mgr, x_InitPtr(), 0, "%.*s",
                                        int(path.size()), path.data()) ) {
        *x_InitPtr() = 0;
        if ( (GetRCObject(rc) == RCObject(rcDirectory) ||
              GetRCObject(rc) == RCObject(rcPath)) &&
             GetRCState(rc) == rcNotFound ) {
            // no SRA accession
            NCBI_THROW3(CSraException, eNotFoundDb,
                        "Cannot open VDB", rc, acc_or_path);
        }
        else if ( GetRCObject(rc) == RCObject(rcDatabase) &&
                  GetRCState(rc) == rcIncorrect ) {
            // invalid SRA database
            NCBI_THROW3(CSraException, eDataError,
                        "Cannot open VDB", rc, acc_or_path);
        }
        else {
            // other errors
            NCBI_THROW3(CSraException, eOtherError,
                        "Cannot open VDB", rc, acc_or_path);
        }
    }
}


CVDBTable::CVDBTable(const CVDB& db,
                     const char* table_name,
                     EMissing missing)
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
            NCBI_THROW3(CSraException, eNotFoundTable,
                        "Cannot open VDB table", rc, table_name);
        }
        else {
            // other errors
            NCBI_THROW3(CSraException, eOtherError,
                        "Cannot open VDB table", rc, table_name);
        }
    }
}


CVDBTable::CVDBTable(const CVDBMgr& mgr,
                     const string& acc_or_path,
                     EMissing missing)
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
            NCBI_THROW3(CSraException, eNotFoundTable,
                        "Cannot open SRA table", rc, acc_or_path);
        }
        else if ( GetRCObject(rc) == RCObject(rcDatabase) &&
                  GetRCState(rc) == rcIncorrect ) {
            // invalid SRA database
            NCBI_THROW3(CSraException, eDataError,
                        "Cannot open SRA table", rc, acc_or_path);
        }
        else {
            // other errors
            NCBI_THROW3(CSraException, eOtherError,
                        "Cannot open SRA table", rc, acc_or_path);
        }
    }
    VSchemaRelease(schema);
}


CVDBTableIndex::CVDBTableIndex(const CVDBTable& table,
                               const char* index_name,
                               EMissing missing)
{
    if ( rc_t rc = VTableOpenIndexRead(table, x_InitPtr(), index_name) ) {
        *x_InitPtr() = 0;
        if ( GetRCObject(rc) == RCObject(rcIndex) &&
             GetRCState(rc) == rcNotFound ) {
            // no such index
            if ( missing != eMissing_Throw ) {
                return;
            }
            NCBI_THROW3(CSraException, eNotFoundIndex,
                        "Cannot open VDB table index", rc, index_name);
        }
        else {
            NCBI_THROW3(CSraException, eOtherError,
                        "Cannot open VDB table index", rc, index_name);
        }
    }
}


pair<int64_t, uint64_t> CVDBTableIndex::Find(const string& value) const
{
    pair<int64_t, uint64_t> range;
    if ( rc_t rc = KIndexFindText(*this, value.c_str(),
                                  &range.first, &range.second, 0, 0) ) {
        if ( GetRCObject(rc) == RCObject(rcString) &&
             GetRCState(rc) == rcNotFound ) {
            // no such value
            range.first = range.second = 0;
        }
        else {
            NCBI_THROW3(CSraException, eOtherError,
                        "Cannot find value in index", rc, value);
        }
    }
    return range;
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


rc_t CVDBCursor::OpenRowRc(uint64_t row_id)
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


void CVDBCursor::OpenRow(uint64_t row_id)
{
    if ( rc_t rc = OpenRowRc(row_id) ) {
        NCBI_THROW3(CSraException, eInitFailed,
                    "Cannot open VDB cursor row", rc, row_id);
    }
}


pair<int64_t, uint64_t> CVDBCursor::GetRowIdRange(uint32_t index) const
{
    pair<int64_t, uint64_t> ret;
    if ( rc_t rc = VCursorIdRange(*this, index, &ret.first, &ret.second) ) {
        NCBI_THROW3(CSraException, eInitFailed,
                    "Cannot get VDB cursor row range", rc, index);
    }
    return ret;
}


uint64_t CVDBCursor::GetMaxRowId(void) const
{
    pair<int64_t, uint64_t> range = GetRowIdRange();
    return range.first+range.second-1;
}


void CVDBCursor::SetParam(const char* name, const CTempString& value) const
{
    if ( rc_t rc = VCursorParamsSet
         ((struct VCursorParams *)GetPointer(),
          name, "%.*s", value.size(), value.data()) ) {
        NCBI_THROW3(CSraException, eNotFound,
                    "Cannot set VDB cursor param", rc, name);
    }
}


static const size_t kCacheSize = 7;


CVDBObjectCacheBase::CVDBObjectCacheBase(void)
{
    m_Objects.reserve(kCacheSize);
}


CVDBObjectCacheBase::~CVDBObjectCacheBase(void)
{
}


DEFINE_STATIC_FAST_MUTEX(sm_CacheMutex);


CRef<CObject> CVDBObjectCacheBase::Get(void)
{
    CRef<CObject> obj;
    CFastMutexGuard guard(sm_CacheMutex);
    if ( !m_Objects.empty() ) {
        obj.Swap(m_Objects.back());
        m_Objects.pop_back();
    }
    return obj;
}


void CVDBObjectCacheBase::Put(CRef<CObject>& obj)
{
    CFastMutexGuard guard(sm_CacheMutex);
    if ( m_Objects.size() < kCacheSize ) {
        m_Objects.push_back(obj);
    }
}


void CVDBColumn::Init(const CVDBCursor& cursor,
                      size_t element_bit_size,
                      const char* name,
                      const char* backup_name,
                      EMissing missing)
{
    DECLARE_SDK_GUARD();
    if ( rc_t rc = VCursorAddColumn(cursor, &m_Index, name) ) {
        if ( backup_name &&
             (rc = VCursorAddColumn(cursor, &m_Index, backup_name)) == 0 ) {
            return;
        }
        m_Index = kInvalidIndex;
        if ( missing == eMissing_Throw ) {
            NCBI_THROW3(CSraException, eNotFoundColumn,
                        "Cannot get VDB column", rc, name);
        }
        else {
            return;
        }
    }
    if ( element_bit_size ) {
        VTypedesc type;
        if ( rc_t rc = VCursorDatatype(cursor, m_Index, 0, &type) ) {
            NCBI_THROW3(CSraException, eInvalidState,
                        "Cannot get VDB column type", rc, name);
        }
        size_t size = type.intrinsic_bits*type.intrinsic_dim;
        if ( size != element_bit_size ) {
            ERR_POST_X(1, "Wrong VDB column size "<<name<<
                       " expected "<<element_bit_size<<" bits != "<<
                       type.intrinsic_dim<<"*"<<type.intrinsic_bits<<" bits");
            NCBI_THROW3(CSraException, eInvalidState,
                        "Wrong VDB column size",
                        RC(rcApp, rcColumn, rcConstructing, rcSelf, rcIncorrect),
                        name);
        }
    }
}


void CVDBValue::x_Get(const VCursor* cursor, uint32_t column)
{
    DECLARE_SDK_GET_GUARD();
    uint32_t bit_offset, bit_length;
    if ( rc_t rc = VCursorCellData(cursor, column,
                                   &bit_length, &m_Data, &bit_offset,
                                   &m_ElemCount) ) {
        NCBI_THROW3(CSraException, eNotFoundValue,
                    "Cannot read VDB value", rc, column);
    }
    if ( bit_offset ) {
        NCBI_THROW3(CSraException, eInvalidState,
                    "Cannot read VDB value with non-zero bit offset",
                    RC(rcApp, rcColumn, rcDecoding, rcOffset, rcUnsupported),
                    bit_offset);
    }
}


void CVDBValue::x_Get(const VCursor* cursor,
                      uint64_t row,
                      uint32_t column,
                      EMissing missing)
{
    DECLARE_SDK_GET_GUARD();
    uint32_t bit_offset, bit_length;
    if ( rc_t rc = VCursorCellDataDirect(cursor, row, column,
                                         &bit_length, &m_Data, &bit_offset,
                                         &m_ElemCount) ) {
        if ( missing != eMissing_Throw ) {
            m_Data = 0;
            m_ElemCount = 0;
            return;
        }
        NCBI_THROW3(CSraException, eNotFoundValue,
                    "Cannot read VDB value", rc, row);
    }
    if ( bit_offset ) {
        NCBI_THROW3(CSraException, eInvalidState,
                    "Cannot read VDB value with non-zero bit offset",
                    RC(rcApp, rcColumn, rcDecoding, rcOffset, rcUnsupported),
                    bit_offset);
    }
}


void CVDBValue::x_ReportIndexOutOfBounds(size_t index) const
{
    if ( index >= size() ) {
        NCBI_THROW3(CSraException, eInvalidIndex,
                    "Invalid index for VDB value array",
                    RC(rcApp, rcData, rcRetrieving, rcOffset, rcTooBig),
                    index);
    }
}


void CVDBValueFor4Bits::x_Get(const VCursor* cursor,
                              uint64_t row,
                              uint32_t column)
{
    DECLARE_SDK_GET_GUARD();
    uint32_t bit_offset, bit_length, elem_count;
    const void* data;
    if ( rc_t rc = VCursorCellDataDirect(cursor, row, column,
                                         &bit_length, &data, &bit_offset,
                                         &elem_count) ) {
        NCBI_THROW3(CSraException, eNotFoundValue,
                    "Cannot read VDB 4-bits value array", rc, row);
    }
    if ( bit_offset != 0 && bit_offset != 4 ) {
        NCBI_THROW3(CSraException, eInvalidState,
                    "Cannot read VDB 4-bits value array with odd bit offset",
                    RC(rcApp, rcColumn, rcDecoding, rcOffset, rcUnsupported),
                    bit_offset);
    }
    m_RawData = static_cast<const char*>(data);
    m_ElemOffset = bit_offset >> 2;
    m_RawElemCount = elem_count + offset();
}


void CVDBValueFor4Bits::x_ReportRawIndexOutOfBounds(size_t raw_index) const
{
    if ( raw_index >= raw_size() ) {
        NCBI_THROW3(CSraException, eInvalidIndex,
                    "Invalid index for VDB 4-bits value array",
                    RC(rcApp, rcData, rcRetrieving, rcOffset, rcTooBig),
                    raw_index);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
