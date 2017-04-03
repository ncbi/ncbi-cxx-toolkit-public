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
 *   Access to BAM files
 *
 */

#include <ncbi_pch.hpp>
#include <sra/readers/bam/bamread.hpp>
#include <sra/readers/ncbi_traces_path.hpp>

#include <klib/rc.h>
#include <klib/writer.h>
#include <klib/text.h>
#include <vfs/path.h>
#include <vfs/manager.h>
#include <align/bam.h>
#include <align/align-access.h>

#include <corelib/ncbifile.hpp>
#include <objects/general/general__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqset/seqset__.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <util/sequtil/sequtil_manip.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;


DEFINE_BAM_REF_TRAITS(AlignAccessMgr, const);
DEFINE_BAM_REF_TRAITS(AlignAccessDB,  const);
DEFINE_BAM_REF_TRAITS(AlignAccessRefSeqEnumerator, );
DEFINE_BAM_REF_TRAITS(AlignAccessAlignmentEnumerator, );
DEFINE_BAM_REF_TRAITS(BAMFile, const);
DEFINE_BAM_REF_TRAITS(BAMAlignment, const);


CBamException::CBamException(void)
    : m_RC(0)
{
}


CBamException::CBamException(const CDiagCompileInfo& info,
                             const CException* prev_exc,
                             EErrCode err_code,
                             const string& message,
                             EDiagSev severity)
    : CException(info, prev_exc, CException::EErrCode(err_code), message),
      m_RC(0)
{
    this->x_Init(info, message, prev_exc, severity);
    this->x_InitErrCode(CException::EErrCode(err_code));
}


CBamException::CBamException(const CDiagCompileInfo& info,
                             const CException* prev_exc,
                             EErrCode err_code,
                             const string& message,
                             rc_t rc,
                             EDiagSev severity)
    : CException(info, prev_exc, CException::EErrCode(err_code), message),
      m_RC(rc)
{
    this->x_Init(info, message, prev_exc, severity);
    this->x_InitErrCode(CException::EErrCode(err_code));
}


CBamException::CBamException(const CDiagCompileInfo& info,
                             const CException* prev_exc,
                             EErrCode err_code,
                             const string& message,
                             rc_t rc,
                             const string& param,
                             EDiagSev severity)
    : CException(info, prev_exc, CException::EErrCode(err_code), message),
      m_RC(rc),
      m_Param(param)
{
    this->x_Init(info, message, prev_exc, severity);
    this->x_InitErrCode(CException::EErrCode(err_code));
}


CBamException::CBamException(const CBamException& other)
    : CException( other),
      m_RC(other.m_RC),
      m_Param(other.m_Param)
{
    x_Assign(other);
}


CBamException::~CBamException(void) throw()
{
}


const CException* CBamException::x_Clone(void) const
{
    return new CBamException(*this);
}


const char* CBamException::GetType(void) const
{
    return "CBamException";
}


CBamException::TErrCode CBamException::GetErrCode(void) const
{
    return typeid(*this) == typeid(CBamException) ?
        x_GetErrCode() : CException::GetErrCode();
}


const char* CBamException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eOtherError:   return "eOtherError";
    case eNullPtr:      return "eNullPtr";
    case eAddRefFailed: return "eAddRefFailed";
    case eInvalidArg:   return "eInvalidArg";
    case eInitFailed:   return "eInitFailed";
    case eNoData:       return "eNoData";
    case eBadCIGAR:     return "eBadCIGAR";
    default:            return CException::GetErrCodeString();
    }
}


ostream& operator<<(ostream& out, const CBamRcFormatter& rc)
{
    char buffer[1024];
    size_t error_len;
    RCExplain(rc.GetRC(), buffer, sizeof(buffer), &error_len);
    out << "0x" << hex << rc.GetRC() << dec << ": " << buffer;
    return out;
}


void CBamException::ReportExtra(ostream& out) const
{
    if ( m_RC ) {
        out << CBamRcFormatter(m_RC);
    }
    if ( !m_Param.empty() ) {
        if ( m_RC ) {
            out << ": ";
        }
        out << m_Param;
    }
}


void CBamException::ReportError(const char* msg, rc_t rc)
{
    ERR_POST(msg<<": "<<CBamRcFormatter(rc));
}


void CBamString::reserve(size_t min_capacity)
{
    size_t capacity = m_Capacity;
    if ( capacity == 0 ) {
        capacity = min_capacity;
    }
    else {
        while ( capacity < min_capacity ) {
            capacity <<= 1;
        }
    }
    m_Buffer.reset(new char[capacity]);
    m_Capacity = capacity;
}


const char* CSrzException::GetErrCodeString(void) const
{
    switch ( GetErrCode() ) {
    case eBadFormat:        return "eBadFormat";
    case eNotFound:         return "eNotFound";
    case eOtherError:       return "eOtherError";
    default:                return CException::GetErrCodeString();
    }
}


CSrzPath::CSrzPath(void)
{
    x_Init();
    AddRepPath(GetDefaultRepPath());
    AddVolPath(GetDefaultVolPath());
}


CSrzPath::CSrzPath(const string& rep_path, const string& vol_path)
{
    x_Init();
    AddRepPath(rep_path.empty()? GetDefaultRepPath(): rep_path);
    AddVolPath(vol_path.empty()? GetDefaultVolPath(): vol_path);
}


void CSrzPath::x_Init(void)
{
}


void CSrzPath::AddRepPath(const string& rep_path)
{
    NStr::Split(rep_path, ":", m_RepPath);
}


void CSrzPath::AddVolPath(const string& vol_path)
{
    NStr::Split(vol_path, ":", m_VolPath);
}


NCBI_PARAM_DECL(string, SRZ, REP_PATH);
NCBI_PARAM_DEF_EX(string, SRZ, REP_PATH, NCBI_SRZ_REP_PATH,
                  eParam_NoThread, SRZ_REP_PATH);


NCBI_PARAM_DECL(string, SRZ, VOL_PATH);
NCBI_PARAM_DEF_EX(string, SRZ, VOL_PATH, NCBI_SRZ_VOL_PATH,
                  eParam_NoThread, SRZ_VOL_PATH);


string CSrzPath::GetDefaultRepPath(void)
{
    return NCBI_PARAM_TYPE(SRZ, REP_PATH)::GetDefault();
}


string CSrzPath::GetDefaultVolPath(void)
{
    return NCBI_PARAM_TYPE(SRZ, VOL_PATH)::GetDefault();
}


string CSrzPath::FindAccPath(const string& acc, EMissing missing)
{
    if ( acc.size() != 9 && acc.size() != 12 ) {
        // bad length
        if ( missing == eMissing_Throw ) {
            NCBI_THROW(CSrzException, eBadFormat,
                       "SRZ accession must be 9 or 12 chars long: "+acc);
        }
        return kEmptyStr;
    }

    string prefix = acc.substr(0, 3);
    NStr::ToUpper(prefix);
    if ( prefix != "SRZ" && prefix != "DRZ" && prefix != "ERZ" ) {
        // bad prefix
        if ( missing == eMissing_Throw ) {
            NCBI_THROW(CSrzException, eBadFormat,
                       "SRZ accession must start with SRZ, DRZ, or ERZ: "+acc);
        }
        return kEmptyStr;
    }

    unsigned num;
    try {
        num = NStr::StringToUInt(CTempString(acc).substr(3));
    }
    catch( CException& /*ignored*/ ) {
        // bad number
        if ( missing == eMissing_Throw ) {
            NCBI_THROW(CSrzException, eBadFormat,
                       "SRZ accesion is improperly formatted: "+acc);
        }
        return kEmptyStr;
    }

    unsigned level1 = num/1000;
    char sub_dir[128];
    sprintf(sub_dir, "%s/%06u/%s%s/provisional",
            prefix.c_str(), level1, prefix.c_str(), acc.c_str()+3);

    ITERATE ( vector<string>, rep_it, m_RepPath ) {
        ITERATE ( vector<string>, vol_it, m_VolPath ) {
            string dir =
                CFile::MakePath(CFile::MakePath(*rep_it, *vol_it), sub_dir);
            if ( CFile(CFile::MakePath(dir, SRZ_CONFIG_NAME)).Exists() ) {
                return dir;
            }
        }
    }
    if ( missing == eMissing_Throw ) {
        NCBI_THROW(CSrzException, eNotFound,
                   "SRZ accession not found: "+acc);
    }
    return kEmptyStr;
}


NCBI_PARAM_DECL(bool, BAM, USE_RAW_INDEX);
NCBI_PARAM_DEF_EX(bool, BAM, USE_RAW_INDEX, false,
                  eParam_NoThread, BAM_USE_RAW_INDEX);


bool CBamDb::UseRawIndex(EUseAPI use_api)
{
    if ( use_api == eUseDefaultAPI )
        return NCBI_PARAM_TYPE(BAM, USE_RAW_INDEX)::GetDefault();
    else
        return use_api == eUseRawIndex;
}


static
void sx_MapId(CSeq_id& id, IIdMapper* idmapper)
{
    if ( idmapper ) {
        try {
            idmapper->MapObject(id);
        }
        catch ( CException& /*ignored*/ ) {
        }
    }
}


static
CRef<CSeq_id> sx_GetRefSeq_id(const string& str, IIdMapper* idmapper)
{
    CRef<CSeq_id> id;
    try {
        id = new CSeq_id(str);
    }
    catch ( CException& /*ignored*/ ) {
    }
    if ( !id && str.find('|') != NPOS ) {
        try {
            CBioseq::TId ids;
            CSeq_id::ParseIDs(ids, str);
            if ( !ids.empty() ) {
                id = *ids.begin();
            }
        }
        catch ( CException& /*ignored*/ ) {
        }
    }
    if ( !id || (id->IsGi() && id->GetGi() < GI_CONST(1000) ) ) {
        id = new CSeq_id(CSeq_id::e_Local, str);
    }
    sx_MapId(*id, idmapper);
    return id;
}


static
CRef<CSeq_id> sx_GetShortSeq_id(const string& str, IIdMapper* idmapper, bool external)
{
    if ( external ) {
        try {
            CRef<CSeq_id> id(new CSeq_id(str));
            return id;
        }
        catch ( CException& /*ignored*/ ) {
            // continue with local id
        }
    }
    CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Local, str));
    //sx_MapId(*id, idmapper);
    return id;
}


CBamMgr::CBamMgr(void)
{
    if ( rc_t rc = AlignAccessMgrMake(x_InitPtr()) ) {
        *x_InitPtr() = 0;
        NCBI_THROW2(CBamException, eInitFailed,
                    "Cannot create AlignAccessMgr", rc);
    }
}


#ifdef NCBI_OS_MSWIN
static inline
bool s_HasWindowsDriveLetter(const string& s)
{
    // first symbol is letter, and second symbol is colon (':')
    return s.length() >= 2 && isalpha(Uchar(s[0])) && s[1] == ':';
}


static
bool s_IsSysPath(const string& s)
{
    if ( s_HasWindowsDriveLetter(s) ) {
        return true;
    }
    if ( s.find_first_of("/\\") == NPOS ) {
        // may be plain accession or local file
        if ( CDirEntry(s).Exists() ) {
            // file -> sys path
            return true;
        }
        else {
            // accession
            return false;
        }
    }
    else {
        // may be path or URI
        if ( s[0] == 'h' &&
             (NStr::StartsWith(s, "http://") ||
              NStr::StartsWith(s, "https://")) ) {
            // URI
            return false;
        }
        if ( s[0] == 'f' &&
             NStr::StartsWith(s, "ftp://") ) {
            // URI
            return false;
        }
        // path
        return true;
    }
}
#endif


static VPath* sx_GetVPath(const string& path)
{
#ifdef NCBI_OS_MSWIN
    // SRA SDK doesn't work with UNC paths with backslashes:
    // \\host\share\dir\file
    // As a workaroung we'll replace backslashes with forward slashes.
    string fixed_path = path;
    if ( s_IsSysPath(path) ) {
        try {
            fixed_path = CDirEntry::CreateAbsolutePath(path);
        }
        catch (exception&) {
            // CDirEntry::CreateAbsolutePath() can fail on remote access URL
        }
        replace(fixed_path.begin(), fixed_path.end(), '\\', '/');
        if ( s_HasWindowsDriveLetter(fixed_path) ) {
            // move drive letter from first symbol to second (in place of ':')
            fixed_path[1] = toupper(Uchar(fixed_path[0]));
            // add leading slash
            fixed_path[0] = '/';
        }
    }
    const char* c_path = fixed_path.c_str();
#else
    const char* c_path = path.c_str();
#endif

    CBamRef<VFSManager> mgr;
    if ( rc_t rc = VFSManagerMake(mgr.x_InitPtr())) {
        NCBI_THROW2(CBamException, eInitFailed,
                    "Cannot create VFSManager object", rc);
    }
    
    VPath* kpath;
    if ( rc_t rc = VFSManagerMakePath(mgr, &kpath, c_path) ) {
        NCBI_THROW2(CBamException, eInitFailed,
                    "Cannot create VPath object", rc);
    }
    return kpath;
}

struct VPathReleaser
{
    static void Delete(const VPath* kpath)
        { VPathRelease(kpath); }
};


CBamDb::SAADBImpl::SAADBImpl(const CBamMgr& mgr,
                             const string& db_name)
{
    AutoPtr<VPath, VPathReleaser> kdb_name(sx_GetVPath(db_name));
    if ( rc_t rc = AlignAccessMgrMakeBAMDB(mgr, m_DB.x_InitPtr(), kdb_name.get()) ) {
        *m_DB.x_InitPtr() = 0;
        NCBI_THROW3(CBamException, eInitFailed,
                    "Cannot open BAM DB", rc, db_name);
    }
}

CBamDb::SAADBImpl::SAADBImpl(const CBamMgr& mgr,
                             const string& db_name,
                             const string& idx_name)
{
    AutoPtr<VPath, VPathReleaser> kdb_name (sx_GetVPath(db_name));
    AutoPtr<VPath, VPathReleaser> kidx_name(sx_GetVPath(idx_name));
    if ( rc_t rc = AlignAccessMgrMakeIndexBAMDB(mgr, m_DB.x_InitPtr(),
                                                kdb_name.get(),
                                                kidx_name.get()) ) {
        *m_DB.x_InitPtr() = 0;
        NCBI_THROW3(CBamException, eInitFailed,
                    "Cannot open BAM DB", rc, db_name);
    }
}


CBamDb::CBamDb(const CBamMgr& mgr,
               const string& db_name,
               EUseAPI use_api)
    : m_DbName(db_name)
{
    if ( UseRawIndex(use_api) ) {
        m_RawDB = new CObjectFor<CBamRawDb>(db_name);
    }
    else {
        m_AADB = new SAADBImpl(mgr, db_name);
    }
}


CBamDb::CBamDb(const CBamMgr& mgr,
               const string& db_name,
               const string& idx_name,
               EUseAPI use_api)
    : m_DbName(db_name),
      m_IndexName(idx_name)
{
    if ( UseRawIndex(use_api) ) {
        m_RawDB = new CObjectFor<CBamRawDb>(db_name, idx_name);
    }
    else {
        m_AADB = new SAADBImpl(mgr, db_name, idx_name);
    }
}


CRef<CSeq_id> CBamDb::GetRefSeq_id(const string& str) const
{
    return sx_GetRefSeq_id(str, GetIdMapper());
}


CRef<CSeq_id> CBamDb::GetShortSeq_id(const string& str, bool external) const
{
    return sx_GetShortSeq_id(str, GetIdMapper(), external);
}


TSeqPos CBamDb::GetRefSeqLength(const string& id) const
{
    if ( !m_RefSeqLengths ) {
        DEFINE_STATIC_FAST_MUTEX(sx_RefSeqMutex);
        CFastMutexGuard guard(sx_RefSeqMutex);
        if ( !m_RefSeqLengths ) {
            AutoPtr<TRefSeqLengths> lengths(new TRefSeqLengths);
            for ( CBamRefSeqIterator it(*this); it; ++it ) {
                TSeqPos len;
                try {
                    len = it.GetLength();
                }
                catch ( CBamException& /*ignored*/ ) {
                    len = kInvalidSeqPos;
                }
                (*lengths)[it.GetRefSeqId()] = len;
            }
            m_RefSeqLengths = lengths;
        }
    }
    TRefSeqLengths::const_iterator it = m_RefSeqLengths->find(id);
    return it == m_RefSeqLengths->end()? kInvalidSeqPos: it->second;
}


string CBamDb::GetHeaderText(void) const
{
    if ( UsesRawIndex() ) {
        return m_RawDB->GetData().GetHeader().GetText();
    }
    else {
        CMutexGuard guard(m_AADB->m_Mutex);
        CBamRef<const BAMFile> file;
        if ( rc_t rc = AlignAccessDBExportBAMFile(m_AADB->m_DB, file.x_InitPtr()) ) {
            NCBI_THROW2(CBamException, eOtherError,
                        "Cannot get BAMFile pointer", rc);
        }
        const char* header;
        size_t size;
        if ( rc_t rc = BAMFileGetHeaderText(file, &header, &size) ) {
            NCBI_THROW2(CBamException, eOtherError,
                        "Cannot get BAM header text", rc);
        }
        return string(header, size);
    }
}


/////////////////////////////////////////////////////////////////////////////

CBamRefSeqIterator::CBamRefSeqIterator()
{
}


CBamRefSeqIterator::CBamRefSeqIterator(const CBamDb& bam_db)
    : m_IdMapper(bam_db.GetIdMapper(), eNoOwnership)
{
    if ( bam_db.UsesRawIndex() ) {
        m_RawDB = bam_db.m_RawDB;
        if ( m_RawDB->GetData().GetHeader().GetRefs().empty() ) {
            m_RawDB = null;
        }
        m_RefIndex = 0;
    }
    else {
        CMutexGuard guard(bam_db.m_AADB->m_Mutex);
        AlignAccessRefSeqEnumerator* ptr = 0;
        if ( rc_t rc = AlignAccessDBEnumerateRefSequences(bam_db.m_AADB->m_DB, &ptr) ) {
            if ( !(GetRCObject(rc) == rcRow &&
                   GetRCState(rc) == rcNotFound) ) {
                // error
                NCBI_THROW2(CBamException, eOtherError, "Cannot find first refseq", rc);
            }
            // no reference sequences found
        }
        else {
            // found first reference sequences
            m_AADBImpl = new SAADBImpl();
            m_AADBImpl->m_Iter.SetReferencedPointer(ptr);
            x_AllocBuffers();
        }
    }
}


void CBamRefSeqIterator::x_AllocBuffers(void)
{
    m_AADBImpl->m_RefSeqIdBuffer.reserve(32);
}


void CBamRefSeqIterator::x_InvalidateBuffers(void)
{
    m_AADBImpl->m_RefSeqIdBuffer.clear();
}


CBamRefSeqIterator::CBamRefSeqIterator(const CBamRefSeqIterator& iter)
{
    *this = iter;
}


CBamRefSeqIterator& CBamRefSeqIterator::operator=(const CBamRefSeqIterator& iter)
{
    if ( this != &iter ) {
        m_AADBImpl = iter.m_AADBImpl;
        m_RawDB = iter.m_RawDB;
        m_RefIndex = iter.m_RefIndex;
        m_IdMapper = iter.m_IdMapper;
        m_CachedRefSeq_id.Reset();
    }
    return *this;
}


void CBamRefSeqIterator::x_CheckValid(void) const
{
    if ( !*this ) {
        NCBI_THROW(CBamException, eNoData, "CBamRefSeqIterator is invalid");
    }
}


CBamRefSeqIterator& CBamRefSeqIterator::operator++(void)
{
    if ( m_AADBImpl ) {
        x_InvalidateBuffers();
        if ( rc_t rc = AlignAccessRefSeqEnumeratorNext(m_AADBImpl->m_Iter) ) {
            m_AADBImpl.Reset();
            if ( !(GetRCObject(rc) == rcRow &&
                   GetRCState(rc) == rcNotFound) ) {
                // error
                NCBI_THROW2(CBamException, eOtherError,
                            "Cannot find next refseq", rc);
            }
            // no more reference sequences
        }
    }
    else {
        if( ++m_RefIndex == m_RawDB->GetData().GetHeader().GetRefs().size() ) {
            // no more reference sequences
            m_RawDB.Reset();
        }
    }
    m_CachedRefSeq_id.Reset();
    return *this;
}


bool CBamRefSeqIterator::x_CheckRC(CBamString& buf,
                                   rc_t rc,
                                   size_t size,
                                   const char* msg) const
{
    if ( rc == 0 ) {
        // no error, update size and finish
        if ( size > 0 ) {
            // omit trailing zero char
            if ( buf[size-1] ) {
                ERR_POST("No zero at the end: " << string(buf.data(), size-1));
            }
            _ASSERT(buf[size-1] == '\0');
            --size;
            buf.x_resize(size);
        }
        else {
            buf.clear();
        }
        return true;
    }
    else if ( GetRCState(rc) == rcInsufficient && size > buf.capacity() ) {
        // buffer too small, realloc and repeat
        buf.reserve(size);
        return false;
    }
    else {
        // other failure
        NCBI_THROW3(CBamException, eNoData,
                    "Cannot get value", rc, msg);
    }
}


void CBamRefSeqIterator::x_GetString(CBamString& buf,
                                     const char* msg, TGetString func) const
{
    x_CheckValid();
    while ( buf.empty() ) {
        size_t size = 0;
        rc_t rc = func(m_AADBImpl->m_Iter, buf.x_data(), buf.capacity(), &size);
        if ( x_CheckRC(buf, rc, size, msg) ) {
            break;
        }
    }
}


CTempString CBamRefSeqIterator::GetRefSeqId(void) const
{
    if ( m_AADBImpl ) {
        x_GetString(m_AADBImpl->m_RefSeqIdBuffer, "RefSeqId",
                    AlignAccessRefSeqEnumeratorGetID);
        return m_AADBImpl->m_RefSeqIdBuffer;
    }
    else {
        return m_RawDB->GetData().GetHeader().GetRefName(m_RefIndex);
    }
}


CRef<CSeq_id> CBamRefSeqIterator::GetRefSeq_id(void) const
{
    if ( !m_CachedRefSeq_id ) {
        m_CachedRefSeq_id = sx_GetRefSeq_id(GetRefSeqId(), GetIdMapper());
    }
    return m_CachedRefSeq_id;
}


TSeqPos CBamRefSeqIterator::GetLength(void) const
{
    if ( m_AADBImpl ) {
        uint64_t length;
        if ( rc_t rc = AlignAccessRefSeqEnumeratorGetLength(m_AADBImpl->m_Iter, &length) ) {
            NCBI_THROW2(CBamException, eNoData,
                        "CBamRefSeqIterator::GetLength() cannot get length", rc);
        }
        if ( length >= kInvalidSeqPos ) {
            NCBI_THROW(CBamException, eOtherError,
                       "CBamRefSeqIterator::GetLength() length is too big");
        }
        return TSeqPos(length);
    }
    else {
        return m_RawDB->GetData().GetHeader().GetRefLength(m_RefIndex);
    }
}


/////////////////////////////////////////////////////////////////////////////

CBamAlignIterator::SRawImpl::SRawImpl(CObjectFor<CBamRawDb>& db)
    : m_RawDB(&db),
      m_Iter(db)
{
}


CBamAlignIterator::SRawImpl::SRawImpl(CObjectFor<CBamRawDb>& db,
                                      const string& ref_label,
                                      TSeqPos ref_pos,
                                      TSeqPos window)
    : m_RawDB(&db),
      m_Iter(db, ref_label, ref_pos, window)
{
}


void CBamAlignIterator::SRawImpl::x_InvalidateBuffers()
{
    m_ShortSequence.clear();
    m_CIGAR.clear();
}


CBamAlignIterator::SAADBImpl::SAADBImpl(const CBamDb::SAADBImpl& db,
                                        AlignAccessAlignmentEnumerator* ptr)
    : m_DB(&db),
      m_Guard(db.m_Mutex)
{
    m_Iter.SetReferencedPointer(ptr);
    m_RefSeqId.reserve(32);
    m_ShortSeqId.reserve(32);
    m_ShortSeqAcc.reserve(32);
    m_ShortSequence.reserve(256);
    m_CIGAR.reserve(32);
    m_Strand = eStrand_not_read;
}


void CBamAlignIterator::SAADBImpl::x_InvalidateBuffers()
{
    m_RefSeqId.clear();
    m_ShortSeqId.clear();
    m_ShortSeqAcc.clear();
    m_ShortSequence.clear();
    m_CIGAR.clear();
    m_Strand = eStrand_not_read;
}


CBamAlignIterator::CBamAlignIterator(void)
    : m_BamFlagsAvailability(eBamFlags_NotTried)
{
}


CBamAlignIterator::CBamAlignIterator(const CBamDb& bam_db)
    : m_IdMapper(bam_db.GetIdMapper(), eNoOwnership),
      m_BamFlagsAvailability(eBamFlags_NotTried)
{
    if ( bam_db.UsesRawIndex() ) {
        m_RawImpl = new SRawImpl(bam_db.m_RawDB.GetNCObject());
        if ( !m_RawImpl->m_Iter ) {
            m_RawImpl.Reset();
        }
    }
    else {
        CMutexGuard guard(bam_db.m_AADB->m_Mutex);
        AlignAccessAlignmentEnumerator* ptr = 0;
        if ( rc_t rc = AlignAccessDBEnumerateAlignments(bam_db.m_AADB->m_DB, &ptr) ) {
            if ( !AlignAccessAlignmentEnumeratorIsEOF(rc) ) {
                // error
                NCBI_THROW2(CBamException, eNoData, "Cannot find first alignment", rc);
            }
            // no alignments
        }
        else {
            // found first alignment
            m_AADBImpl = new SAADBImpl(*bam_db.m_AADB, ptr);
        }
    }
}


CBamAlignIterator::CBamAlignIterator(const CBamDb& bam_db,
                                     const string& ref_id,
                                     TSeqPos ref_pos,
                                     TSeqPos window)
    : m_IdMapper(bam_db.GetIdMapper(), eNoOwnership),
      m_BamFlagsAvailability(eBamFlags_NotTried)
{
    if ( bam_db.UsesRawIndex() ) {
        m_RawImpl = new SRawImpl(bam_db.m_RawDB.GetNCObject(), ref_id, ref_pos, window);
        if ( !m_RawImpl->m_Iter ) {
            m_RawImpl.Reset();
        }
    }
    else {
        CMutexGuard guard(bam_db.m_AADB->m_Mutex);
        AlignAccessAlignmentEnumerator* ptr = 0;
        if ( rc_t rc = AlignAccessDBWindowedAlignments(bam_db.m_AADB->m_DB, &ptr,
                                                       ref_id.c_str(), ref_pos, window) ) {
            if ( !AlignAccessAlignmentEnumeratorIsEOF(rc) ) {
                // error
                NCBI_THROW2(CBamException, eNoData, "Cannot find first alignment", rc);
            }
            // no alignments
        }
        else {
            // found first alignment
            m_AADBImpl = new SAADBImpl(*bam_db.m_AADB, ptr);
        }
    }
}


CBamAlignIterator::CBamAlignIterator(const CBamAlignIterator& iter)
{
    *this = iter;
}


CBamAlignIterator& CBamAlignIterator::operator=(const CBamAlignIterator& iter)
{
    if ( this != &iter ) {
        m_AADBImpl = iter.m_AADBImpl;
        m_RawImpl = iter.m_RawImpl;
        m_IdMapper = iter.m_IdMapper;
        m_SpotIdDetector = iter.m_SpotIdDetector;
        m_BamFlagsAvailability = iter.m_BamFlagsAvailability;
    }
    return *this;
}


void CBamAlignIterator::x_CheckValid(void) const
{
    if ( !*this ) {
        NCBI_THROW(CBamException, eNoData, "CBamAlignIterator is invalid");
    }
}


CBamAlignIterator& CBamAlignIterator::operator++(void)
{
    x_CheckValid();
    m_RefSeq_id.Reset();
    m_ShortSeq_id.Reset();
    if ( m_AADBImpl ) {
        if ( rc_t rc = AlignAccessAlignmentEnumeratorNext(m_AADBImpl->m_Iter) ) {
            m_AADBImpl.Reset();
            if ( !(GetRCObject(rc) == rcRow &&
                   GetRCState(rc) == rcNotFound) ) {
                // error
                NCBI_THROW2(CBamException, eOtherError, "Cannot find next alignment", rc);
            }
            // end of iteration, keep the error code
        }
        else {
            // next alignment
            m_AADBImpl->x_InvalidateBuffers();
        }
    }
    else {
        if ( !++m_RawImpl->m_Iter ) {
            m_RawImpl.Reset();
        }
        else {
            m_RawImpl->x_InvalidateBuffers();
        }
    }
    return *this;
}


bool CBamAlignIterator::x_CheckRC(CBamString& buf,
                                  rc_t rc,
                                  size_t size,
                                  const char* msg) const
{
    if ( rc == 0 ) {
        // no error, update size and finish
        if ( size > 0 ) {
            // omit trailing zero char
            if ( buf[size-1] ) {
                ERR_POST("No zero at the end: " << string(buf.data(), size-1));
            }
            _ASSERT(buf[size-1] == '\0');
            --size;
            buf.x_resize(size);
        }
        else {
            buf.clear();
        }
        return true;
    }
    else if ( GetRCState(rc) == rcInsufficient && size > buf.capacity() ) {
        // buffer too small, realloc and repeat
        buf.reserve(size);
        return false;
    }
    else {
        // other failure
        NCBI_THROW3(CBamException, eNoData,
                    "Cannot get value", rc, msg);
    }
}


void CBamAlignIterator::x_GetString(CBamString& buf,
                                    const char* msg, TGetString func) const
{
    x_CheckValid();
    while ( buf.empty() ) {
        size_t size = 0;
        rc_t rc = func(m_AADBImpl->m_Iter, buf.x_data(), buf.capacity(), &size);
        if ( x_CheckRC(buf, rc, size, msg) ) {
            break;
        }
    }
}


void CBamAlignIterator::x_GetString(CBamString& buf, uint64_t& pos,
                                    const char* msg, TGetString2 func) const
{
    x_CheckValid();
    while ( buf.empty() ) {
        size_t size = 0;
        rc_t rc = func(m_AADBImpl->m_Iter, &pos, buf.x_data(), buf.capacity(), &size);
        if ( x_CheckRC(buf, rc, size, msg) ) {
            break;
        }
    }
}


CTempString CBamAlignIterator::GetRefSeqId(void) const
{
    if ( m_RawImpl ) {
        return m_RawImpl->m_RawDB->GetData().GetHeader().GetRefName(m_RawImpl->m_Iter.GetRefSeqIndex());
    }
    else {
        x_GetString(m_AADBImpl->m_RefSeqId, "RefSeqId",
                    AlignAccessAlignmentEnumeratorGetRefSeqID);
        return m_AADBImpl->m_RefSeqId;
    }
}


TSeqPos CBamAlignIterator::GetRefSeqPos(void) const
{
    if ( m_RawImpl ) {
        return m_RawImpl->m_Iter.GetRefSeqPos();
    }
    else {
        x_CheckValid();
        uint64_t pos = 0;
        if ( rc_t rc = AlignAccessAlignmentEnumeratorGetRefSeqPos(m_AADBImpl->m_Iter, &pos) ) {
            if ( GetRCObject(rc) == RCObject(rcData) &&
                 GetRCState(rc) == rcNotFound ) {
                return kInvalidSeqPos;
            }
            NCBI_THROW2(CBamException, eNoData,
                        "Cannot get RefSeqPos", rc);
        }
        return TSeqPos(pos);
    }
}


CTempString CBamAlignIterator::GetShortSeqId(void) const
{
    if ( m_RawImpl ) {
        return m_RawImpl->m_Iter.GetShortSeqId();
    }
    else {
        x_GetString(m_AADBImpl->m_ShortSeqId, "ShortSeqId",
                    AlignAccessAlignmentEnumeratorGetShortSeqID);
        return m_AADBImpl->m_ShortSeqId;
    }
}


CTempString CBamAlignIterator::GetShortSeqAcc(void) const
{
    if ( m_RawImpl ) {
        return m_RawImpl->m_Iter.GetShortSeqAcc();
    }
    else {
        x_GetString(m_AADBImpl->m_ShortSeqAcc, "ShortSeqAcc",
                    AlignAccessAlignmentEnumeratorGetShortSeqAccessionID);
        return m_AADBImpl->m_ShortSeqAcc;
    }
}


CTempString CBamAlignIterator::GetShortSequence(void) const
{
    if ( m_RawImpl ) {
        if ( m_RawImpl->m_ShortSequence.empty() ) {
            m_RawImpl->m_ShortSequence = m_RawImpl->m_Iter.GetShortSequence();
        }
        return m_RawImpl->m_ShortSequence;
    }
    else {
        x_GetString(m_AADBImpl->m_ShortSequence, "ShortSequence",
                    AlignAccessAlignmentEnumeratorGetShortSequence);
        return m_AADBImpl->m_ShortSequence;
    }
}


inline void CBamAlignIterator::x_GetCIGAR(void) const
{
    x_GetString(m_AADBImpl->m_CIGAR, m_AADBImpl->m_CIGARPos, "CIGAR",
                AlignAccessAlignmentEnumeratorGetCIGAR);
}


TSeqPos CBamAlignIterator::GetCIGARPos(void) const
{
    if ( m_RawImpl ) {
        return m_RawImpl->m_Iter.GetCIGARPos();
    }
    else {
        x_GetCIGAR();
        return TSeqPos(m_AADBImpl->m_CIGARPos);
    }
}


CTempString CBamAlignIterator::GetCIGAR(void) const
{
    if ( m_RawImpl ) {
        if ( m_RawImpl->m_CIGAR.empty() ) {
            m_RawImpl->m_CIGAR = m_RawImpl->m_Iter.GetCIGAR();
        }
        return m_RawImpl->m_CIGAR;
    }
    else {
        x_GetCIGAR();
        return m_AADBImpl->m_CIGAR;
    }
}


TSeqPos CBamAlignIterator::GetCIGARRefSize(void) const
{
    if ( m_RawImpl ) {
        return m_RawImpl->m_Iter.GetCIGARRefSize();
    }
    else {
        TSeqPos ref_size = 0;
        x_GetCIGAR();
        const char* ptr = m_AADBImpl->m_CIGAR.data();
        const char* end = ptr + m_AADBImpl->m_CIGAR.size();
        char type;
        TSeqPos len;
        while ( ptr != end ) {
            type = *ptr;
            for ( len = 0; ++ptr != end; ) {
                char c = *ptr;
                if ( c >= '0' && c <= '9' ) {
                    len = len*10+(c-'0');
                }
                else {
                    break;
                }
            }
            if ( type == 'M' || type == '=' || type == 'X' ) {
                // match
                ref_size += len;
            }
            else if ( type == 'I' || type == 'S' ) {
                // insert
            }
            else if ( type == 'D' || type == 'N' ) {
                // delete
                ref_size += len;
            }
            else if ( type != 'P' ) {
                NCBI_THROW_FMT(CBamException, eBadCIGAR,
                               "Bad CIGAR char: " << type << " in " << m_AADBImpl->m_CIGAR);
            }
            if ( len == 0 ) {
                NCBI_THROW_FMT(CBamException, eBadCIGAR,
                               "Bad CIGAR length: " << type << "0 in " << m_AADBImpl->m_CIGAR);
            }
        }
        return ref_size;
    }
}


TSeqPos CBamAlignIterator::GetCIGARShortSize(void) const
{
    if ( m_RawImpl ) {
        return m_RawImpl->m_Iter.GetCIGARShortSize();
    }
    else {
        TSeqPos short_size = 0;
        x_GetCIGAR();
        const char* ptr = m_AADBImpl->m_CIGAR.data();
        const char* end = ptr + m_AADBImpl->m_CIGAR.size();
        char type;
        TSeqPos len;
        while ( ptr != end ) {
            type = *ptr;
            for ( len = 0; ++ptr != end; ) {
                char c = *ptr;
                if ( c >= '0' && c <= '9' ) {
                    len = len*10+(c-'0');
                }
                else {
                    break;
                }
            }
            if ( type == 'M' || type == '=' || type == 'X' ) {
                // match
                short_size += len;
            }
            else if ( type == 'I' || type == 'S' ) {
                // insert
                short_size += len;
            }
            else if ( type == 'D' || type == 'N' ) {
                // delete
            }
            else if ( type != 'P' ) {
                NCBI_THROW_FMT(CBamException, eBadCIGAR,
                               "Bad CIGAR char: " << type << " in " << m_AADBImpl->m_CIGAR);
            }
            if ( len == 0 ) {
                NCBI_THROW_FMT(CBamException, eBadCIGAR,
                               "Bad CIGAR length: " << type << "0 in " << m_AADBImpl->m_CIGAR);
            }
        }
        return short_size;
    }
}


CRef<CSeq_id> CBamAlignIterator::GetRefSeq_id(void) const
{
    if ( !m_RefSeq_id ) {
        m_RefSeq_id = sx_GetRefSeq_id(GetRefSeqId(), GetIdMapper());
    }
    return m_RefSeq_id;
}


CRef<CSeq_id> CBamAlignIterator::GetShortSeq_id(const string& str) const
{
    return sx_GetShortSeq_id(str, GetIdMapper(), GetShortSequence().size() == 0);
}


CRef<CSeq_id> CBamAlignIterator::GetShortSeq_id(void) const
{
    if ( !m_ShortSeq_id ) {
        string id = GetShortSeqId();
        bool paired = IsPaired(), is_1st = false, is_2nd = false;
        if ( paired ) {
            // regular way to get pairing info
            is_1st = IsFirstInPair();
            is_2nd = IsSecondInPair();
        }
        else {
            // more pairing info may be available via BAM file flags
            Uint2 flags;
            if ( TryGetFlags(flags) ) {
                // use flags to get pairing info faster
                paired = (flags & (BAMFlags_WasPaired |
                                   BAMFlags_IsMappedAsPair)) != 0;
                is_1st = (flags & BAMFlags_IsFirst) != 0;
                is_2nd = (flags & BAMFlags_IsSecond) != 0;
            }
        }
        if ( paired ) {
            if ( is_1st && !is_2nd ) {
                id += ".1";
            }
            else if ( is_2nd && !is_1st ) {
                id += ".2";
            }
            else {
                // conflict
                if ( ISpotIdDetector* detector = GetSpotIdDetector() ) {
                    detector->AddSpotId(id, this);
                }
                else {
                    id += ".?";
                }
            }
        }
        m_ShortSeq_id = GetShortSeq_id(id);
    }
    return m_ShortSeq_id;
}


void CBamAlignIterator::SetRefSeq_id(CRef<CSeq_id> seq_id)
{
    m_RefSeq_id = seq_id;
}


void CBamAlignIterator::SetShortSeq_id(CRef<CSeq_id> seq_id)
{
    m_ShortSeq_id = seq_id;
}


void CBamAlignIterator::x_GetStrand(void) const
{
    x_CheckValid();
    if ( m_AADBImpl->m_Strand != eStrand_not_read ) {
        return;
    }
    
    m_AADBImpl->m_Strand = eStrand_not_set;
    AlignmentStrandDirection dir;
    if ( AlignAccessAlignmentEnumeratorGetStrandDirection(m_AADBImpl->m_Iter, &dir) != 0 ) {
        return;
    }
    
    switch ( dir ) {
    case asd_Forward:
        m_AADBImpl->m_Strand = eNa_strand_plus;
        break;
    case asd_Reverse:
        m_AADBImpl->m_Strand = eNa_strand_minus;
        break;
    default:
        m_AADBImpl->m_Strand = eNa_strand_unknown;
        break;
    }
}


bool CBamAlignIterator::IsSetStrand(void) const
{
    if ( m_RawImpl ) {
        return m_RawImpl->m_Iter.IsSetStrand();
    }
    else {
        x_GetStrand();
        return m_AADBImpl->m_Strand != eStrand_not_set;
    }
}


ENa_strand CBamAlignIterator::GetStrand(void) const
{
    if ( m_RawImpl ) {
        return m_RawImpl->m_Iter.GetStrand();
    }
    else {
        if ( !IsSetStrand() ) {
            NCBI_THROW(CBamException, eNoData,
                       "Strand is not set");
        }
        return ENa_strand(m_AADBImpl->m_Strand);
    }
}


Uint1 CBamAlignIterator::GetMapQuality(void) const
{
    if ( m_RawImpl ) {
        return m_RawImpl->m_Iter.GetMapQuality();
    }
    else {
        x_CheckValid();
        uint8_t q = 0;
        if ( rc_t rc = AlignAccessAlignmentEnumeratorGetMapQuality(m_AADBImpl->m_Iter, &q) ) {
            NCBI_THROW2(CBamException, eNoData,
                        "Cannot get MapQuality", rc);
        }
        return q;
    }
}


bool CBamAlignIterator::IsPaired(void) const
{
    if ( m_RawImpl ) {
        return m_RawImpl->m_Iter.IsPaired();
    }
    else {
        x_CheckValid();
        bool f;
        if ( rc_t rc = AlignAccessAlignmentEnumeratorGetIsPaired(m_AADBImpl->m_Iter, &f) ) {
            NCBI_THROW2(CBamException, eNoData,
                        "Cannot get IsPaired flag", rc);
        }
        return f;
    }
}


bool CBamAlignIterator::IsFirstInPair(void) const
{
    if ( m_RawImpl ) {
        return m_RawImpl->m_Iter.IsFirstInPair();
    }
    else {
        x_CheckValid();
        bool f;
        if ( rc_t rc=AlignAccessAlignmentEnumeratorGetIsFirstInPair(m_AADBImpl->m_Iter, &f) ) {
            NCBI_THROW2(CBamException, eNoData,
                        "Cannot get IsFirstInPair flag", rc);
        }
        return f;
    }
}


bool CBamAlignIterator::IsSecondInPair(void) const
{
    if ( m_RawImpl ) {
        return m_RawImpl->m_Iter.IsSecondInPair();
    }
    else {
        x_CheckValid();
        bool f;
        if ( rc_t rc=AlignAccessAlignmentEnumeratorGetIsSecondInPair(m_AADBImpl->m_Iter, &f) ) {
            NCBI_THROW2(CBamException, eNoData,
                        "Cannot get IsSecondInPair flag", rc);
        }
        return f;
    }
}


CBamFileAlign::CBamFileAlign(const CBamAlignIterator& iter)
{
    if ( rc_t rc = AlignAccessAlignmentEnumeratorGetBAMAlignment(iter.m_AADBImpl->m_Iter, x_InitPtr()) ) {
        *x_InitPtr() = 0;
        NCBI_THROW2(CBamException, eNoData,
                    "Cannot get BAM file alignment", rc);
    }
}


Uint2 CBamFileAlign::GetFlags(void) const
{
    uint16_t flags;
    if ( rc_t rc = BAMAlignmentGetFlags(*this, &flags) ) {
        NCBI_THROW2(CBamException, eNoData,
                    "Cannot get BAM flags", rc);
    }
    return flags;
}


bool CBamFileAlign::TryGetFlags(Uint2& flags) const
{
    return BAMAlignmentGetFlags(*this, &flags) == 0;
}


Uint2 CBamAlignIterator::GetFlags(void) const
{
    if ( m_RawImpl ) {
        return m_RawImpl->m_Iter.GetFlags();
    }
    else {
        x_CheckValid();
        try {
            Uint2 flags = CBamFileAlign(*this).GetFlags();
            if ( m_BamFlagsAvailability != eBamFlags_Available ) {
                m_BamFlagsAvailability = eBamFlags_Available;
            }
            return flags;
        }
        catch ( CBamException& /* will be rethrown */ ) {
            if ( m_BamFlagsAvailability != eBamFlags_NotAvailable ) {
                m_BamFlagsAvailability = eBamFlags_NotAvailable;
            }
            throw;
        }
    }
}


bool CBamAlignIterator::TryGetFlags(Uint2& flags) const
{
    if ( m_RawImpl ) {
        flags = m_RawImpl->m_Iter.GetFlags();
        return true;
    }
    else {
        if ( !*this || m_BamFlagsAvailability == eBamFlags_NotAvailable ) {
            return false;
        }
        if ( !CBamFileAlign(*this).TryGetFlags(flags) ) {
            m_BamFlagsAvailability = eBamFlags_NotAvailable;
            return false;
        }
        if ( m_BamFlagsAvailability != eBamFlags_Available ) {
            m_BamFlagsAvailability = eBamFlags_Available;
        }
        return true;
    }
}


CRef<CBioseq> CBamAlignIterator::GetShortBioseq(void) const
{
    CTempString data = GetShortSequence();
    TSeqPos length = TSeqPos(data.size());
    if ( length == 0 ) {
        // no actual sequence
        return null;
    }
    CRef<CBioseq> seq(new CBioseq);
    seq->SetId().push_back(GetShortSeq_id());
    CSeq_inst& inst = seq->SetInst();
    inst.SetRepr(inst.eRepr_raw);
    inst.SetMol(inst.eMol_na);
    inst.SetLength(length);
    string& iupac = inst.SetSeq_data().SetIupacna().Set();
    iupac.assign(data.data(), length);
    if ( GetStrand() == eNa_strand_minus ) {
        CSeqManip::ReverseComplement(iupac, CSeqUtil::e_Iupacna, 0, length);
    }
    return seq;
}


CRef<CSeq_align> CBamAlignIterator::GetMatchAlign(void) const
{
    if ( GetRefSeqPos() == kInvalidSeqPos ) {
        return null;
    }
    CRef<CSeq_align> align(new CSeq_align);
    align->SetType(CSeq_align::eType_diags);
    CDense_seg& denseg = align->SetSegs().SetDenseg();
    denseg.SetIds().push_back(GetRefSeq_id());
    denseg.SetIds().push_back(GetShortSeq_id());
    CDense_seg::TStarts& starts = denseg.SetStarts();
    CDense_seg::TLens& lens = denseg.SetLens();

    TSeqPos segcount = 0;
    if ( m_RawImpl ) {
        m_RawImpl->m_Iter.GetSegments(starts, lens);
        segcount = lens.size();
    }
    else {
        TSeqPos refpos = GetRefSeqPos();
        TSeqPos seqpos = GetCIGARPos();
        const char* ptr = m_AADBImpl->m_CIGAR.data();
        const char* end = ptr + m_AADBImpl->m_CIGAR.size();
        char type;
        TSeqPos seglen;
        TSeqPos refstart = 0, seqstart = 0;
        while ( ptr != end ) {
            type = *ptr;
            for ( seglen = 0; ++ptr != end; ) {
                char c = *ptr;
                if ( c >= '0' && c <= '9' ) {
                    seglen = seglen*10+(c-'0');
                }
                else {
                    break;
                }
            }
            if ( type == 'M' || type == '=' || type == 'X' ) {
                // match
                refstart = refpos;
                refpos += seglen;
                seqstart = seqpos;
                seqpos += seglen;
            }
            else if ( type == 'I' || type == 'S' ) {
                refstart = kInvalidSeqPos;
                seqstart = seqpos;
                seqpos += seglen;
            }
            else if ( type == 'D' || type == 'N' ) {
                // delete
                refstart = refpos;
                refpos += seglen;
                seqstart = kInvalidSeqPos;
            }
            else if ( type == 'P' ) {
                continue;
            }
            else {
                NCBI_THROW_FMT(CBamException, eBadCIGAR,
                               "Bad CIGAR char: " <<type<< " in " <<m_AADBImpl->m_CIGAR);
            }
            if ( seglen == 0 ) {
                NCBI_THROW_FMT(CBamException, eBadCIGAR,
                               "Bad CIGAR length: " << type <<
                               "0 in " << m_AADBImpl->m_CIGAR);
            }
            starts.push_back(refstart);
            starts.push_back(seqstart);
            lens.push_back(seglen);
            ++segcount;
        }
    }
    if ( GetStrand() == eNa_strand_minus ) {
        CDense_seg::TStrands& strands = denseg.SetStrands();
        strands.reserve(2*segcount);
        TSeqPos end = TSeqPos(GetShortSequence().size());
        for ( size_t i = 0; i < segcount; ++i ) {
            strands.push_back(eNa_strand_plus);
            strands.push_back(eNa_strand_minus);
            TSeqPos pos = starts[i*2+1];
            TSeqPos len = lens[i];
            if ( pos != kInvalidSeqPos ) {
                starts[i*2+1] = end - (pos + len);
            }
        }
    }

    denseg.SetNumseg(segcount);
    return align;
}


CRef<CSeq_annot>
CBamAlignIterator::x_GetSeq_annot(const string* annot_name) const
{
    CRef<CSeq_annot> annot(new CSeq_annot);
    annot->SetData().SetAlign();
    if ( annot_name ) {
        CRef<CAnnotdesc> desc(new CAnnotdesc);
        desc->SetName(*annot_name);
        annot->SetDesc().Set().push_back(desc);
    }
    return annot;
}


CRef<CSeq_entry>
CBamAlignIterator::x_GetMatchEntry(const string* annot_name) const
{
    CRef<CSeq_entry> entry(new CSeq_entry);
    if ( CRef<CBioseq> seq = GetShortBioseq() ) {
        entry->SetSeq(*seq);
    }
    else {
        entry->SetSet().SetSeq_set();
    }
    if ( CRef<CSeq_align> align = GetMatchAlign() ) {
        CRef<CSeq_annot> annot = x_GetSeq_annot(annot_name);
        entry->SetAnnot().push_back(annot);
        annot->SetData().SetAlign().push_back(align);
    }
    return entry;
}


/////////////////////////////////////////////////////////////////////////////
// CBamAlignIterator::ISpotIdDetector

CBamAlignIterator::ISpotIdDetector::~ISpotIdDetector(void)
{
}


END_SCOPE(objects)
END_NCBI_SCOPE
