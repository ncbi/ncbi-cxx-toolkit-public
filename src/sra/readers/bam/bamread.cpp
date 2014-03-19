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
#include <vfs/path.h>

#include <corelib/ncbifile.hpp>
#include <objects/general/general__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqset/seqset__.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <util/sequtil/sequtil_manip.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;

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


static const size_t kInitialPathSize = 256;


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
    NStr::Tokenize(rep_path, ":", m_RepPath);
}


void CSrzPath::AddVolPath(const string& vol_path)
{
    NStr::Tokenize(vol_path, ":", m_VolPath);
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
    if ( !id || (id->IsGi() && id->GetGi() < 1000 ) ) {
        id = new CSeq_id(CSeq_id::e_Local, str);
    }
    sx_MapId(*id, idmapper);
    return id;
}


static
CRef<CSeq_id> sx_GetShortSeq_id(const string& str, IIdMapper* idmapper)
{
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

static VPath* sx_GetVPath(const string& path)
{
#ifdef NCBI_OS_MSWIN
    // SRA SDK doesn't work with UNC paths with backslashes:
    // \\host\share\dir\file
    // As a workaroung we'll replace backslashes with forward slashes.
    string fixed_path = path;
    if ( path.find('\\') != NPOS && CFile(path).Exists() ) {
        replace(fixed_path.begin(), fixed_path.end(), '\\', '/');
    }
    const char* c_path = fixed_path.c_str();
#else
    const char* c_path = path.c_str();
#endif
    VPath* kpath;
    if ( rc_t rc = VPathMakeSysPath(&kpath, c_path) ) {
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

CBamDb::CBamDb(const CBamMgr& mgr,
               const string& db_name)
    : m_DbName(db_name)
{
    AutoPtr<VPath, VPathReleaser> kdb_name(sx_GetVPath(db_name));
    if ( rc_t rc = AlignAccessMgrMakeBAMDB(mgr, x_InitPtr(), kdb_name.get()) ) {
        *x_InitPtr() = 0;
        NCBI_THROW3(CBamException, eInitFailed,
                    "Cannot open BAM DB", rc, db_name);
    }
}


CBamDb::CBamDb(const CBamMgr& mgr,
               const string& db_name,
               const string& idx_name)
    : m_DbName(db_name)
{
    AutoPtr<VPath, VPathReleaser> kdb_name (sx_GetVPath(db_name));
    AutoPtr<VPath, VPathReleaser> kidx_name(sx_GetVPath(idx_name));
    if ( rc_t rc = AlignAccessMgrMakeIndexBAMDB(mgr, x_InitPtr(),
                                                kdb_name.get(),
                                                kidx_name.get()) ) {
        *x_InitPtr() = 0;
        NCBI_THROW3(CBamException, eInitFailed,
                    "Cannot open BAM DB", rc, db_name);
    }
}


CRef<CSeq_id> CBamDb::GetRefSeq_id(const string& str) const
{
    return sx_GetRefSeq_id(str, GetIdMapper());
}


CRef<CSeq_id> CBamDb::GetShortSeq_id(const string& str) const
{
    return sx_GetShortSeq_id(str, GetIdMapper());
}


TSeqPos CBamDb::GetRefSeqLength(const string& id) const
{
    if ( m_RefSeqLengths.empty() ) {
        for ( CBamRefSeqIterator it(*this); it; ++it ) {
            TSeqPos len;
            try {
                len = it.GetLength();
            }
            catch ( CBamException& /*ignored*/ ) {
                len = kInvalidSeqPos;
            }
            m_RefSeqLengths[it.GetRefSeqId()] = len;
        }
    }
    TRefSeqLengths::const_iterator it = m_RefSeqLengths.find(id);
    return it == m_RefSeqLengths.end()? kInvalidSeqPos: it->second;
}


string CBamDb::GetHeaderText(void) const
{
    const BAMFile* file;
    if ( rc_t rc = AlignAccessDBExportBAMFile(*this, &file) ) {
        NCBI_THROW2(CBamException, eOtherError,
                    "Cannot get BAMFile pointer", rc);
    }
    CBamRef<const BAMFile> file_ref;
    file_ref.SetReferencedPointer(file);
    const char* header;
    size_t size;
    if ( rc_t rc = BAMFileGetHeaderText(file, &header, &size) ) {
        NCBI_THROW2(CBamException, eOtherError,
                    "Cannot get BAM header text", rc);
    }
    return string(header, size);
}


/////////////////////////////////////////////////////////////////////////////

CBamRefSeqIterator::CBamRefSeqIterator(const CBamDb& bam_db)
    : m_IdMapper(bam_db.GetIdMapper(), eNoOwnership)
{
    AlignAccessRefSeqEnumerator* ptr = 0;
    m_LocateRC = AlignAccessDBEnumerateRefSequences(bam_db, &ptr);
    if ( !m_LocateRC ) {
        m_Iter.SetReferencedPointer(ptr);
    }
    else if ( !(GetRCObject(m_LocateRC) == rcRow &&
                GetRCState(m_LocateRC) == rcNotFound) ) {
        NCBI_THROW2(CBamException, eOtherError,
                    "Cannot find first refseq", m_LocateRC);
    }
    x_AllocBuffers();
}


void CBamRefSeqIterator::x_AllocBuffers(void)
{
    m_RefSeqId.reserve(32);
}


void CBamRefSeqIterator::x_InvalidateBuffers(void)
{
    m_RefSeqId.clear();
    m_RefSeq_id.Reset();
}


CBamRefSeqIterator::CBamRefSeqIterator(const CBamRefSeqIterator& iter)
    : m_Iter(iter.m_Iter),
      m_LocateRC(iter.m_LocateRC)
{
    x_AllocBuffers();
}


CBamRefSeqIterator& CBamRefSeqIterator::operator=(const CBamRefSeqIterator& iter)
{
    if ( this != &iter ) {
        x_InvalidateBuffers();
        m_Iter = iter.m_Iter;
        m_LocateRC = iter.m_LocateRC;
    }
    return *this;
}


void CBamRefSeqIterator::x_CheckValid(void) const
{
    if ( !m_Iter ) {
        NCBI_THROW2(CBamException, eNoData,
                    "CBamRefSeqIterator is invalid", m_LocateRC);
    }
}


CBamRefSeqIterator& CBamRefSeqIterator::operator++(void)
{
    x_CheckValid();
    x_InvalidateBuffers();
    m_LocateRC = AlignAccessRefSeqEnumeratorNext(m_Iter);
    if ( m_LocateRC != 0 &&
         !(GetRCObject(m_LocateRC) == rcRow &&
           GetRCState(m_LocateRC) == rcNotFound) ) {
        NCBI_THROW2(CBamException, eOtherError,
                    "Cannot find next refseq", m_LocateRC);
    }
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
        rc_t rc = func(m_Iter, buf.x_data(), buf.capacity(), &size);
        if ( x_CheckRC(buf, rc, size, msg) ) {
            break;
        }
    }
}


const CBamString& CBamRefSeqIterator::GetRefSeqId(void) const
{
    x_GetString(m_RefSeqId, "RefSeqId",
                AlignAccessRefSeqEnumeratorGetID);
    return m_RefSeqId;
}


CRef<CSeq_id> CBamRefSeqIterator::GetRefSeq_id(void) const
{
    if ( !m_RefSeq_id ) {
        m_RefSeq_id = sx_GetRefSeq_id(GetRefSeqId(), GetIdMapper());
    }
    return m_RefSeq_id;
}


TSeqPos CBamRefSeqIterator::GetLength(void) const
{
    uint64_t length;
    if ( rc_t rc = AlignAccessRefSeqEnumeratorGetLength(m_Iter, &length) ) {
        NCBI_THROW2(CBamException, eNoData,
                    "CBamRefSeqIterator::GetLength() cannot get length", rc);
    }
    if ( length >= kInvalidSeqPos ) {
        NCBI_THROW(CBamException, eOtherError,
                   "CBamRefSeqIterator::GetLength() length is too big");
    }
    return TSeqPos(length);
}


/////////////////////////////////////////////////////////////////////////////

CBamAlignIterator::CBamAlignIterator(void)
    : m_BamFlagsAvailability(eBamFlags_NotTried)
{
    m_LocateRC = 1;
    x_AllocBuffers();
}


CBamAlignIterator::CBamAlignIterator(const CBamDb& bam_db)
    : m_IdMapper(bam_db.GetIdMapper(), eNoOwnership),
      m_BamFlagsAvailability(eBamFlags_NotTried)
{
    AlignAccessAlignmentEnumerator* ptr = 0;
    m_LocateRC = AlignAccessDBEnumerateAlignments(bam_db, &ptr);
    if ( !m_LocateRC ) {
        m_Iter.SetReferencedPointer(ptr);
    }
    else if ( !(GetRCObject(m_LocateRC) == RCObject(rcData) &&
                GetRCState(m_LocateRC) == rcNotFound) ) {
        NCBI_THROW2(CBamException, eOtherError,
                    "Cannot find first alignment", m_LocateRC);
    }
    x_AllocBuffers();
}


CBamAlignIterator::CBamAlignIterator(const CBamDb& bam_db,
                                     const string& ref_id,
                                     TSeqPos ref_pos,
                                     TSeqPos window)
    : m_IdMapper(bam_db.GetIdMapper(), eNoOwnership),
      m_BamFlagsAvailability(eBamFlags_NotTried)
{
    AlignAccessAlignmentEnumerator* ptr = 0;
    m_LocateRC = AlignAccessDBWindowedAlignments(bam_db, &ptr,
                                                 ref_id.c_str(),
                                                 ref_pos, window);
    if ( !m_LocateRC ) {
        m_Iter.SetReferencedPointer(ptr);
    }
    else if ( !(GetRCObject(m_LocateRC) == RCObject(rcData) &&
                GetRCState(m_LocateRC) == rcNotFound) ) {
        NCBI_THROW2(CBamException, eOtherError,
                    "Cannot find first alignment", m_LocateRC);
    }
    x_AllocBuffers();
}


void CBamAlignIterator::x_AllocBuffers(void)
{
    m_RefSeqId.reserve(32);
    m_ShortSeqId.reserve(32);
    m_ShortSeqAcc.reserve(32);
    m_ShortSequence.reserve(256);
    m_CIGAR.reserve(32);
    m_Strand = eStrand_not_read;
}


void CBamAlignIterator::x_InvalidateBuffers(void)
{
    m_RefSeqId.clear();
    m_ShortSeqId.clear();
    m_ShortSeqAcc.clear();
    m_ShortSequence.clear();
    m_CIGAR.clear();
    m_RefSeq_id.Reset();
    m_ShortSeq_id.Reset();
    m_Strand = eStrand_not_read;
}


CBamAlignIterator::CBamAlignIterator(const CBamAlignIterator& iter)
    : m_Iter(iter.m_Iter),
      m_IdMapper(iter.m_IdMapper),
      m_SpotIdDetector(iter.m_SpotIdDetector),
      m_LocateRC(iter.m_LocateRC),
      m_BamFlagsAvailability(iter.m_BamFlagsAvailability)
{
    x_AllocBuffers();
}


CBamAlignIterator& CBamAlignIterator::operator=(const CBamAlignIterator& iter)
{
    if ( this != &iter ) {
        x_InvalidateBuffers();
        m_Iter = iter.m_Iter;
        m_IdMapper = iter.m_IdMapper;
        m_SpotIdDetector = iter.m_SpotIdDetector;
        m_LocateRC = iter.m_LocateRC;
        m_BamFlagsAvailability = iter.m_BamFlagsAvailability;
    }
    return *this;
}


void CBamAlignIterator::x_CheckValid(void) const
{
    if ( !*this ) {
        NCBI_THROW2(CBamException, eNoData,
                    "CBamAlignIterator is invalid", m_LocateRC);
    }
}


CBamAlignIterator& CBamAlignIterator::operator++(void)
{
    x_CheckValid();
    x_InvalidateBuffers();
    m_LocateRC = AlignAccessAlignmentEnumeratorNext(m_Iter);
    if ( m_LocateRC != 0 &&
         !(GetRCObject(m_LocateRC) == rcRow &&
           GetRCState(m_LocateRC) == rcNotFound) ) {
        NCBI_THROW2(CBamException, eOtherError,
                    "Cannot find next alignment", m_LocateRC);
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
        rc_t rc = func(m_Iter, buf.x_data(), buf.capacity(), &size);
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
        rc_t rc = func(m_Iter, &pos, buf.x_data(), buf.capacity(), &size);
        if ( x_CheckRC(buf, rc, size, msg) ) {
            break;
        }
    }
}


const CBamString& CBamAlignIterator::GetRefSeqId(void) const
{
    x_GetString(m_RefSeqId, "RefSeqId",
                AlignAccessAlignmentEnumeratorGetRefSeqID);
    return m_RefSeqId;
}


TSeqPos CBamAlignIterator::GetRefSeqPos(void) const
{
    x_CheckValid();
    uint64_t pos = 0;
    if ( rc_t rc = AlignAccessAlignmentEnumeratorGetRefSeqPos(m_Iter, &pos) ) {
        NCBI_THROW2(CBamException, eNoData,
                    "Cannot get RefSeqPos", rc);
    }
    return TSeqPos(pos);
}


const CBamString& CBamAlignIterator::GetShortSeqId(void) const
{
    x_GetString(m_ShortSeqId, "ShortSeqId",
                AlignAccessAlignmentEnumeratorGetShortSeqID);
    return m_ShortSeqId;
}


const CBamString& CBamAlignIterator::GetShortSeqAcc(void) const
{
    x_GetString(m_ShortSeqAcc, "ShortSeqAcc",
                AlignAccessAlignmentEnumeratorGetShortSeqAccessionID);
    return m_ShortSeqAcc;
}


const CBamString& CBamAlignIterator::GetShortSequence(void) const
{
    x_GetString(m_ShortSequence, "ShortSequence",
                AlignAccessAlignmentEnumeratorGetShortSequence);
    return m_ShortSequence;
}


inline void CBamAlignIterator::x_GetCIGAR(void) const
{
    x_GetString(m_CIGAR, m_CIGARPos, "CIGAR",
                AlignAccessAlignmentEnumeratorGetCIGAR);
}


TSeqPos CBamAlignIterator::GetCIGARPos(void) const
{
    x_GetCIGAR();
    return TSeqPos(m_CIGARPos);
}


const CBamString& CBamAlignIterator::GetCIGAR(void) const
{
    x_GetCIGAR();
    return m_CIGAR;
}


TSeqPos CBamAlignIterator::GetCIGARRefSize(void) const
{
    x_GetCIGAR();
    TSeqPos ref_size = 0;
    const char* ptr = m_CIGAR.data();
    const char* end = ptr + m_CIGAR.size();
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
                           "Bad CIGAR char: " << type << " in " << m_CIGAR);
        }
        if ( len == 0 ) {
            NCBI_THROW_FMT(CBamException, eBadCIGAR,
                           "Bad CIGAR length: " << type << "0 in " << m_CIGAR);
        }
    }
    return ref_size;
}


TSeqPos CBamAlignIterator::GetCIGARShortSize(void) const
{
    x_GetCIGAR();
    TSeqPos short_size = 0;
    const char* ptr = m_CIGAR.data();
    const char* end = ptr + m_CIGAR.size();
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
                           "Bad CIGAR char: " << type << " in " << m_CIGAR);
        }
        if ( len == 0 ) {
            NCBI_THROW_FMT(CBamException, eBadCIGAR,
                           "Bad CIGAR length: " << type << "0 in " << m_CIGAR);
        }
    }
    return short_size;
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
    return sx_GetShortSeq_id(str, GetIdMapper());
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
    if ( m_Strand != eStrand_not_read ) {
        return;
    }
    
    m_Strand = eStrand_not_set;
    AlignmentStrandDirection dir;
    if ( AlignAccessAlignmentEnumeratorGetStrandDirection(m_Iter, &dir) != 0 ) {
        return;
    }
    
    switch ( dir ) {
    case asd_Forward:
        m_Strand = eNa_strand_plus;
        break;
    case asd_Reverse:
        m_Strand = eNa_strand_minus;
        break;
    default:
        m_Strand = eNa_strand_unknown;
        break;
    }
}


bool CBamAlignIterator::IsSetStrand(void) const
{
    x_GetStrand();
    return m_Strand != eStrand_not_set;
}


ENa_strand CBamAlignIterator::GetStrand(void) const
{
    if ( !IsSetStrand() ) {
        NCBI_THROW(CBamException, eNoData,
                   "Strand is not set");
    }
    return ENa_strand(m_Strand);
}


Uint1 CBamAlignIterator::GetMapQuality(void) const
{
    x_CheckValid();
    uint8_t q = 0;
    if ( rc_t rc = AlignAccessAlignmentEnumeratorGetMapQuality(m_Iter, &q) ) {
        NCBI_THROW2(CBamException, eNoData,
                    "Cannot get MapQuality", rc);
    }
    return q;
}


bool CBamAlignIterator::IsPaired(void) const
{
    x_CheckValid();
    bool f;
    if ( rc_t rc = AlignAccessAlignmentEnumeratorGetIsPaired(m_Iter, &f) ) {
        NCBI_THROW2(CBamException, eNoData,
                    "Cannot get IsPaired flag", rc);
    }
    return f;
}


bool CBamAlignIterator::IsFirstInPair(void) const
{
    x_CheckValid();
    bool f;
    if ( rc_t rc=AlignAccessAlignmentEnumeratorGetIsFirstInPair(m_Iter, &f) ) {
        NCBI_THROW2(CBamException, eNoData,
                    "Cannot get IsFirstInPair flag", rc);
    }
    return f;
}


bool CBamAlignIterator::IsSecondInPair(void) const
{
    x_CheckValid();
    bool f;
    if ( rc_t rc=AlignAccessAlignmentEnumeratorGetIsSecondInPair(m_Iter, &f) ) {
        NCBI_THROW2(CBamException, eNoData,
                    "Cannot get IsSecondInPair flag", rc);
    }
    return f;
}


CBamFileAlign::CBamFileAlign(const CBamAlignIterator& iter)
{
    if ( rc_t rc = AlignAccessAlignmentEnumeratorGetBAMAlignment(iter.m_Iter, x_InitPtr()) ) {
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


bool CBamAlignIterator::TryGetFlags(Uint2& flags) const
{
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


CRef<CBioseq> CBamAlignIterator::GetShortBioseq(void) const
{
    CRef<CBioseq> seq(new CBioseq);
    seq->SetId().push_back(GetShortSeq_id());
    CSeq_inst& inst = seq->SetInst();
    inst.SetRepr(inst.eRepr_raw);
    inst.SetMol(inst.eMol_na);
    const CBamString& data = GetShortSequence();
    TSeqPos length = TSeqPos(data.size());
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
    CRef<CSeq_align> align(new CSeq_align);
    align->SetType(CSeq_align::eType_diags);
    CDense_seg& denseg = align->SetSegs().SetDenseg();
    denseg.SetIds().push_back(GetRefSeq_id());
    denseg.SetIds().push_back(GetShortSeq_id());
    CDense_seg::TStarts& starts = denseg.SetStarts();
    CDense_seg::TLens& lens = denseg.SetLens();

    TSeqPos segcount = 0;
    TSeqPos refpos = GetRefSeqPos();
    TSeqPos seqpos = GetCIGARPos();
    const char* ptr = m_CIGAR.data();
    const char* end = ptr + m_CIGAR.size();
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
        else if ( type != 'P' ) {
            NCBI_THROW_FMT(CBamException, eBadCIGAR,
                           "Bad CIGAR char: " <<type<< " in " <<m_CIGAR);
        }
        if ( seglen == 0 ) {
            NCBI_THROW_FMT(CBamException, eBadCIGAR,
                           "Bad CIGAR length: " << type <<
                           "0 in " << m_CIGAR);
        }
        starts.push_back(refstart);
        starts.push_back(seqstart);
        lens.push_back(seglen);
        ++segcount;
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
    CRef<CBioseq> seq = GetShortBioseq();
    CRef<CSeq_annot> annot = x_GetSeq_annot(annot_name);
    annot->SetData().SetAlign().push_back(GetMatchAlign());
    seq->SetAnnot().push_back(annot);
    entry->SetSeq(*seq);
    return entry;
}


/////////////////////////////////////////////////////////////////////////////
// CBamAlignIterator::ISpotIdDetector

CBamAlignIterator::ISpotIdDetector::~ISpotIdDetector(void)
{
}


END_SCOPE(objects)
END_NCBI_SCOPE
