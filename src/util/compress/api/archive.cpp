/* $Id$
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
 * Authors:  Vladimir Ivanov,  Anton Lavrentiev
 *
 * File Description:
 *   Compression archive API.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <util/compress/archive.hpp>
#include <util/error_codes.hpp>
#include "archive_zip.hpp"

#if !defined(NCBI_OS_MSWIN)  &&  !defined(NCBI_OS_UNIX)
#  error "Class CArchive can be defined on MS-Windows and UNIX platforms only!"
#endif

#define NCBI_USE_ERRCODE_X  Util_Compress


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
// Helper routines
//

static string s_FormatMessage(CArchiveException::TErrCode errcode,
                              const string& message,
                              const CArchiveEntryInfo& info)
{
    string msg;
    switch (errcode) {
    case CArchiveException::eUnsupportedEntryType:
        if (message.empty()) {
            msg = "Unsupported entry type for '" + info.GetName() + "'";
            break;
        }
    case CArchiveException::eList:
    case CArchiveException::eExtract:
    case CArchiveException::eBackup:
    case CArchiveException::eRestoreAttrs:
        msg = message;
        if (!info.GetName().empty()) {
            msg += ", while in '" + info.GetName() + '\'';
        }
        break;
    case CArchiveException::eMemory:
    case CArchiveException::eCreate:
    case CArchiveException::eBadName:
    case CArchiveException::eAppend:
    case CArchiveException::eOpen:
    case CArchiveException::eClose:
    default:
        msg = message;
        break;
    }
    return msg;
}


static string s_OSReason(int x_errno)
{
    const char* strerr = x_errno ? strerror(x_errno) : 0;
    return strerr  &&  *strerr ? string(": ") + strerr : kEmptyStr;
}


//////////////////////////////////////////////////////////////////////////////
//
// Constants / macros / typedefs
//

// Throw exception
#define ARCHIVE_THROW1(errcode) \
    NCBI_THROW(CArchiveException, errcode, s_FormatMessage(CArchiveException::errcode, kEmptyStr, this->m_Current))
#define ARCHIVE_THROW(errcode, message) \
    NCBI_THROW(CArchiveException, errcode, s_FormatMessage(CArchiveException::errcode, message, this->m_Current))
#define ARCHIVE_THROW_INFO(errcode, message, info) \
    NCBI_THROW(CArchiveException, errcode, s_FormatMessage(CArchiveException::errcode, message, info))

/*
// Post message
#define ARCHIVE_POST(subcode, severity, message) \
    ERR_POST_X(subcode, (severity) << s_FormatMessage(this->m_Current, message)
*/
// Get archive handle
#define ARCHIVE        m_Archive.get()
// Check archive handle
#define ARCHIVE_CHECK  _ASSERT(m_Archive.get() != NULL)

// Macro to check flags bits
#define F_ISSET(mask) ((m_Flags & (mask)) == (mask))



//////////////////////////////////////////////////////////////////////////////
//
// Auxiliary functions 
//

/* Create path from entry name and base directory.
*/
static string s_ToFilesystemPath(const string& base_dir, const string& name)
{
    string path(CDirEntry::IsAbsolutePath(name)  ||  base_dir.empty()
                ? name : CDirEntry::ConcatPath(base_dir, name));
    return CDirEntry::NormalizePath(path);
}


/* Create archive name from path.
*/
static string s_ToArchiveName(const string& base_dir, const string& path, bool is_absolute_allowed)
{
    // NB: Path assumed to have been normalized
    string retval = CDirEntry::AddTrailingPathSeparator(path);

#ifdef NCBI_OS_MSWIN
    // Convert to Unix format with forward slashes
    NStr::ReplaceInPlace(retval, "\\", "/");
    const NStr::ECase how = NStr::eNocase;
#else
    const NStr::ECase how = NStr::eCase;
#endif //NCBI_OS_MSWIN

    bool absolute;
    // Remove leading base dir from the path
    if (!base_dir.empty()  &&  NStr::StartsWith(retval, base_dir, how)) {
        if (retval.size() > base_dir.size()) {
            retval.erase(0, base_dir.size()/*separator too*/);
        } else {
            retval.assign(1, '.');
        }
        absolute = false;
    } else {
        absolute = CDirEntry::IsAbsolutePath(retval);
    }
    SIZE_TYPE pos = 0;

#ifdef NCBI_OS_MSWIN
    // Remove a disk name if present
    if (retval.size() > 1
        &&  isalpha((unsigned char) retval[0])  &&  retval[1] == ':') {
        pos = 2;
    }
#endif //NCBI_OS_MSWIN

    // Remove any leading and trailing slashes
    while (pos < retval.size()  &&  retval[pos] == '/') {
        pos++;
    }
    if (pos) {
        retval.erase(0, pos);
    }
    pos = retval.size();
    while (pos > 0  &&  retval[pos - 1] == '/') {
        --pos;
    }
    if (pos < retval.size()) {
        retval.erase(pos);
    }

    // Add leading slash back, if allowed
    if (absolute  &&  is_absolute_allowed) {
        retval.insert((SIZE_TYPE) 0, 1, '/');
    }
    return retval;
}



//////////////////////////////////////////////////////////////////////////////
//
// CArchive
//

CArchive::CArchive(EFormat format)
    : m_Format(format),
      m_Flags(fDefault),
      m_OpenMode(eNone),
      m_Modified(false)
{
    // Create a new archive object
    switch (format) {
        case eZip:
            m_Archive.reset(new CArchiveZip());
            break;
        default:
            _TROUBLE;
    }
    if ( !ARCHIVE ) {
        ARCHIVE_THROW(eMemory, "Cannot create archive object");
    }
}


CArchive::~CArchive()
{
    try {
        Close();
        // Archive interface should be closed on this moment, just delete it.
        if ( ARCHIVE ) {
            m_Archive.reset();
        }
        // Delete owned masks
        UnsetMask();
    }
    COMPRESS_HANDLE_EXCEPTIONS(93, "CArchive::~CArchive");
}


void CArchive::Close(void)
{
    if (m_OpenMode == eNone) {
        return;
    }
    ARCHIVE_CHECK;
    ARCHIVE->Close();
    m_OpenMode = eNone;
    m_Modified = false;
}


void CArchive::SetMask(CMask* mask, EOwnership  own, EMaskType type, NStr::ECase acase)
{
    SMask* m = NULL;
    switch (type) {
        case eFullPathMask:
            m = &m_MaskFullPath;
            break;
        case ePatternMask:
            m = &m_MaskPattern;
            break;
        default:
            _TROUBLE;
    }
    if (m->owned) {
        delete m->mask;
    }
    m->mask  = mask;
    m->acase = acase;
    m->owned = mask ? own : eNoOwnership;
}


void CArchive::UnsetMask(EMaskType type)
{
    SetMask(NULL, eNoOwnership, type);
}


void CArchive::UnsetMask(void)
{
    SetMask(NULL, eNoOwnership, eFullPathMask);
    SetMask(NULL, eNoOwnership, ePatternMask);
}


void CArchive::SetBaseDir(const string& dirname)
{
    string s = CDirEntry::AddTrailingPathSeparator(dirname);
#ifdef NCBI_OS_MSWIN
    // Always use forward slashes internally
    NStr::ReplaceInPlace(s, "\\", "/");
#endif
    s.swap(m_BaseDir);
}


bool CArchive::HaveSupport(ESupport feature, int param)
{
    ARCHIVE_CHECK;
    switch (feature) {
    case eType:
        return ARCHIVE->HaveSupport_Type((CDirEntry::EType)param);
    case eAbsolutePath:
        return ARCHIVE->HaveSupport_AbsolutePath();
    }
    return false;
}


void CArchive::Create(void)
{
    ARCHIVE_CHECK;
    x_Open(eCreate);
}


auto_ptr<CArchive::TEntries> CArchive::List(void)
{
    ARCHIVE_CHECK;
    x_Open(eList);
    return x_ReadAndProcess(eList);
}


auto_ptr<CArchive::TEntries> CArchive::Test(void)
{
    ARCHIVE_CHECK;
    x_Open(eTest);
    return x_ReadAndProcess(eTest);
}


auto_ptr<CArchive::TEntries> CArchive::Extract(void)
{
    ARCHIVE_CHECK;
    x_Open(eExtract);
    auto_ptr<TEntries> entries = x_ReadAndProcess(eExtract);
    // Restore attributes of "postponed" directory entries
    if (F_ISSET(fPreserveAll)) {
        ITERATE(TEntries, e, *entries) {
            if (e->GetType() == CDirEntry::eDir) {
                x_RestoreAttrs(*e);
            }
        }
    }
    return entries;
}


void CArchive::ExtractFileToMemory(const CArchiveEntryInfo& info, void* buf, size_t buf_size, size_t* out_size)
{
    ARCHIVE_CHECK;
    if (!buf  || !buf_size) {
        NCBI_THROW(CCoreException, eInvalidArg, "Bad memory buffer");
    }
    if (out_size) {
        *out_size = 0;
    }
    CDirEntry::EType type = info.GetType();
    if (type == CDirEntry::eUnknown  &&  !F_ISSET(fSkipUnsupported)) {
        // Conform to POSIX-mandated behavior to extract as files
        type = CDirEntry::eFile;
    }
    if (type != CDirEntry::eFile) {
        ARCHIVE_THROW_INFO(eUnsupportedEntryType, kEmptyStr, info);
    }
    x_Open(eExtract);
    ARCHIVE->ExtractEntryToMemory(info, buf, buf_size);
    if (out_size) {
        *out_size = (size_t)info.GetSize();
    }
    return;
}


void CArchive::ExtractFileToHeap(const CArchiveEntryInfo& info, void** buf_ptr, size_t* buf_size_ptr)
{
    ARCHIVE_CHECK;
    if (!buf_ptr  || !buf_size_ptr) {
        NCBI_THROW(CCoreException, eInvalidArg, "Bad pointers to memory buffer");
    }
    *buf_ptr = NULL;
    *buf_size_ptr = 0;

    CDirEntry::EType type = info.GetType();
    if (type == CDirEntry::eUnknown  &&  !F_ISSET(fSkipUnsupported)) {
        // Conform to POSIX-mandated behavior to extract as files
        type = CDirEntry::eFile;
    }
    if (type != CDirEntry::eFile) {
        ARCHIVE_THROW_INFO(eUnsupportedEntryType, kEmptyStr, info);
    }
    // Get size of buffer for memory allocation
    Uint8 uncompressed_size = info.GetSize();
    if (!uncompressed_size) {
        // File is empty, do nothing
        return;
    }
    if ( uncompressed_size > get_limits(*buf_size_ptr).max() ) {
        ARCHIVE_THROW(eMemory, "File is too big to extract to memory, its size is " +
                                NStr::Int8ToString(uncompressed_size));
    }
    // Allocate memory
    size_t x_uncompressed_size = (size_t)uncompressed_size;
    void* ptr = malloc(x_uncompressed_size);
    if (!ptr) {
        ARCHIVE_THROW(eMemory, "Cannot allocate " + 
                               NStr::Int8ToString(uncompressed_size) +
                               " bytes on heap");
    }
    try {
        // Extract file
        ExtractFileToMemory(info, ptr, x_uncompressed_size, NULL);
    } catch(...) {
        free(ptr);
        throw;
    }
    // Return result
    *buf_ptr = ptr;
    *buf_size_ptr = x_uncompressed_size;
    return;
}


void CArchive::ExtractFileToCallback(const CArchiveEntryInfo& info,
                                     IArchive::Callback_Write callback)
{
    CDirEntry::EType type = info.GetType();
    if (type == CDirEntry::eUnknown  &&  !F_ISSET(fSkipUnsupported)) {
        // Conform to POSIX-mandated behavior to extract as files
        type = CDirEntry::eFile;
    }
    if (type != CDirEntry::eFile) {
        ARCHIVE_THROW_INFO(eUnsupportedEntryType, kEmptyStr, info);
    }
    x_Open(eExtract);
    ARCHIVE->ExtractEntryToCallback(info, callback);
    return;
}


auto_ptr<CArchive::TEntries> CArchive::Append(const string& path, ELevel level,
                                              const string& comment)
{
    ARCHIVE_CHECK;
    x_Open(eAppend);
    return x_Append(path, level, comment);
}


auto_ptr<CArchive::TEntries>
CArchive::AppendFileFromMemory(const string& name_in_archive, void* buf, size_t buf_size,
                               ELevel level, const string& comment)
{
    ARCHIVE_CHECK;
    if (!buf  || !buf_size) {
        NCBI_THROW(CCoreException, eInvalidArg, "Bad memory buffer");
    }
    x_Open(eAppend);
    auto_ptr<TEntries> entries(new TEntries);

    // Clear the entry info
    m_Current = CArchiveEntryInfo();

    // Get name of the current entry in archive
    string temp = s_ToArchiveName(kEmptyStr, name_in_archive, HaveSupport(eAbsolutePath));
    if (temp.empty()) {
        ARCHIVE_THROW(eBadName, "Empty entry name is not allowed");
    }

    // Fill out entry information
    m_Current.m_Name.swap(temp);
    m_Current.m_Type    = CDirEntry::eFile;
    m_Current.m_Comment = comment;
    entries->push_back(m_Current);

#if 0
    if (m_Format == eZip) {
//???
    } else {
        _TROUBLE;
    }
#endif
    ARCHIVE->AddEntryFromMemory(m_Current, buf, buf_size, level);
    return entries;
}


void CArchive::SkipEntry(void)
{
    ARCHIVE->SkipEntry(m_Current);
    return;
}


void CArchive::TestEntry(void)
{
    CDirEntry::EType type = m_Current.GetType();
    if (type == CDirEntry::eUnknown  &&  !F_ISSET(fSkipUnsupported)) {
        // Conform to POSIX-mandated behavior to extract as files
        type = CDirEntry::eFile;
    }
    switch (type) {
    case CDirEntry::eFile:
        ARCHIVE->TestEntry(m_Current);
        break;
    case CDirEntry::eDir:
        break;
    case CDirEntry::eLink:
    case CDirEntry::ePipe:
    case CDirEntry::eCharSpecial:
    case CDirEntry::eBlockSpecial:
        // Cannot be tested, do nothing
        break;

    default:
        ARCHIVE_THROW1(eUnsupportedEntryType);
    }
    return;
}


void CArchive::ExtractEntry(const CDirEntry& dst)
{
    CDirEntry::EType type = m_Current.GetType();
    switch (type) {
    case CDirEntry::eFile:
        ARCHIVE->ExtractEntryToFileSystem(m_Current, dst.GetPath());
        break;

    case CDirEntry::eDir:
        // Directory should be already created in x_ExtractEntry().
        // Attributes for a directory will be set only when all
        // its files have been already extracted.
        break;

    case CDirEntry::eLink:
    case CDirEntry::ePipe:
    case CDirEntry::eCharSpecial:
    case CDirEntry::eBlockSpecial:
    default:
        ARCHIVE_THROW1(eUnsupportedEntryType);
    }
    return;
}


void CArchive::AppendEntry(const string& path, ELevel level)
{
    ARCHIVE->AddEntryFromFileSystem(m_Current, path, level);
    return;
}


void CArchive::x_Open(EAction action)
{
    EOpenMode new_open_mode = EOpenMode(int(action) & eRW);

    if (m_OpenMode != eWO  &&  action == eAppend) {
        // Appending to an existing archive is not implemented yet
        _TROUBLE;
    }
    if (new_open_mode != m_OpenMode) {
        Close();
        Open(action);
        m_OpenMode = new_open_mode;
    }
#if 0
/*
//    bool isread = (action & eRO) > 0;
???
    if (!m_Modified) {
        // Check if Create() is followed by Append()
        if (m_OpenMode != eWO  &&  action == eAppend) {
            toend = true;
        }
    } else if (action != eAppend) {
        // Previous action shouldn't be eCreate
        _ASSERT(m_OpenMode != eWO);
        if (m_Modified) {
            m_Modified = false;
        }
    }
*/
#endif
    return;
}


auto_ptr<CArchive::TEntries> CArchive::x_ReadAndProcess(EAction action)
{
    _ASSERT(action);
    auto_ptr<TEntries> entries(new TEntries);

    // Get number of files in archive
    size_t n = ARCHIVE->GetNumEntries();

    // Process all entries
    for (size_t i = 0;  i < n;  i++) {
        m_Current.Reset();
        // Get next entry
        ARCHIVE->GetEntryInfo(i, &m_Current);
        if ( m_Current.m_Name.empty() ) {
            ARCHIVE_THROW(eBadName, "Empty entry name in archive");
        }

        // Match file name with the set of masks

        bool match = true;
        // Replace backslashes with forward slashes
        string path = m_Current.m_Name;
        if ( m_MaskFullPath.mask ) {
            match = m_MaskFullPath.mask->Match(path, m_MaskFullPath.acase);
        }
        if ( match  &&  m_MaskPattern.mask ) {
            match = false;
            list<CTempString> elems;
            NStr::Split(path, "/", elems);
            ITERATE(list<CTempString>, it, elems) {
                if (m_MaskPattern.mask->Match(*it, m_MaskPattern.acase)) {
                    match = true;
                    break;
                }
            }
        }
        if ( !match ) {
            continue;
        }

        // User callback
        if (!Checkpoint(m_Current, action)) {
            // Skip current entry
            SkipEntry();
            continue;
        }

        // Process current entry
        switch (action) {
            case eList:
                SkipEntry();
                break;
            case eExtract:
                // It calls ExtractEntry() inside
                x_ExtractEntry(entries.get());
                break;
            case eTest:
                TestEntry();
                break;
            default:
                // Undefined action
               _TROUBLE;
        }
        // Add entry into the list of processed entries
        entries->push_back(m_Current);
    }
    return entries;
}


// Deleter for temporary file for safe extraction
struct CTmpEntryDeleter {
    static void Delete(CDirEntry* entry) { 
        if ( entry->GetPath().empty() ) {
            return;
        }
        entry->Remove();
    }
};

void CArchive::x_ExtractEntry(const TEntries* prev_entries)
{
    CDirEntry::EType type = m_Current.GetType();

    // Destination for extraction
    auto_ptr<CDirEntry> dst(
        CDirEntry::CreateObject(type,
            CDirEntry::NormalizePath(CDirEntry::ConcatPath(m_BaseDir, m_Current.GetName()))));
    // Dereference link if requested
    if (type == CDirEntry::eLink  &&  F_ISSET(fFollowLinks)) {
        dst->DereferenceLink();
    }
    // Actual type in file system (if exists)
    CDirEntry::EType dst_type = dst->GetType();

    // Look if extraction is allowed (when the destination exists)
    bool found = false;  

    if (dst_type != CDirEntry::eUnknown) {
        // Check if destination entry is ours (previous revision of the same file)
        if (prev_entries) {
            ITERATE(TEntries, e, *prev_entries) {
                if (e->GetName() == m_Current.GetName()  &&
                    e->GetType() == m_Current.GetType()) {
                    found = true;
                    break;
                }
            }
        }
        // Not ours
        if (!found) {
            // Can overwrite it?
            if (!F_ISSET(fOverwrite)) {
                // File already exists, and cannot be changed
                return;
            } else {
                // Can update?
                // Note, that we update directories always, because the archive
                // can contain other subtree of this existing directory.
                if (F_ISSET(fUpdate)  &&  type != CDirEntry::eDir) {
                    // Make sure that destination entry is not older than current entry
                    time_t dst_time;
                    if (dst->GetTimeT(&dst_time)
                        &&  m_Current.GetModificationTime() <= dst_time) {
                        return;
                    }
                }
                // Have equal types?
                if (F_ISSET(fEqualTypes)  &&  type != dst_type) {
                    ARCHIVE_THROW(eExtract, "Cannot overwrite '" + dst->GetPath() + 
                        "' with an archive entry of different type");
                }
            }
            if (F_ISSET(fBackup)) {
                // Need to backup the existing destination
                CDirEntry backup(*dst);
                if (!backup.Backup(kEmptyStr, CDirEntry::eBackup_Rename)) {
                    int x_errno = errno;
                    ARCHIVE_THROW(eBackup, "Failed to backup '" + dst->GetPath() + '\'' + s_OSReason(x_errno));
                }
            }
        }
        // Entry with the same name exists and can be overwritten
        found = true;
    }
  
    // Extraction

    CDirEntry tmp;
#  ifdef NCBI_OS_UNIX
    // Set private settings for newly created files,
    // only current user can read or modify it.
    mode_t u = umask(0);
    umask(u & 077);
    try {
#  endif

    // Create directory
    string dirname = dst->GetDir();
    if (!dirname.empty()) {
        if (!CDir(dirname).CreatePath()) {
            int x_errno = errno;
            ARCHIVE_THROW(eExtract, "Cannot create directory '" + dirname + '\'' + s_OSReason(x_errno));
        }
    }
    if (type == CDirEntry::eFile) {
        // Always use temporary file for safe file extraction
        AutoPtr<CDirEntry, CTmpEntryDeleter> tmp_deleter;
        tmp.Reset(CDirEntry::GetTmpNameEx(dst->GetDir(), ".tmp_ncbiarch_", CDirEntry::eTmpFileCreate));
        tmp_deleter.reset(&tmp);
        // Extract file
        ExtractEntry(tmp);
        // Rename it to destination name
        if (!tmp.Rename(dst->GetPath(), found ? CDirEntry::fRF_Overwrite : CDirEntry::fRF_Default)) {
            int x_errno = errno;
            ARCHIVE_THROW(eExtract, "Cannot rename temporary file to '" + 
                dst->GetPath() + "' back in place" + s_OSReason(x_errno));
        }
        // Restore attributes after renaming
        x_RestoreAttrs(m_Current, &(*dst));
        // Reset temporary object to prevent file deletion after its successful renaming
        tmp.Reset(kEmptyStr);

    } else if (type == CDirEntry::eDir) {
        // Do nothing
    } else {
        //???
        ARCHIVE_THROW1(eUnsupportedEntryType);
    }

#  ifdef NCBI_OS_UNIX
    } catch (...) {
        umask(u);
        throw;
    }
    umask(u);
#  endif
}


void CArchive::x_RestoreAttrs(const CArchiveEntryInfo& info,
                              const CDirEntry*         dst) const
{
    auto_ptr<CDirEntry> path_ptr;  // deleter
    if (!dst) {
        path_ptr.reset(CDirEntry::CreateObject(CDirEntry::EType(info.GetType()),
                       CDirEntry::NormalizePath(CDirEntry::ConcatPath(m_BaseDir, info.GetName()))));
        dst = path_ptr.get();
    }

    // Date/time.
    // Set the time before permissions because on some platforms
    // this setting can also affect file permissions.
    if (F_ISSET(fPreserveTime)) {
        time_t mtime(info.GetModificationTime());
        time_t atime(info.GetLastAccessTime());
        time_t ctime(info.GetCreationTime());
        if (!dst->SetTimeT(&mtime, &atime, &ctime)) {
            int x_errno = errno;
            ARCHIVE_THROW(eRestoreAttrs, "Cannot restore date/time for '" +
                dst->GetPath() + '\'' + s_OSReason(x_errno));
        }
    }

    // Owner.
    // This must precede changing permissions because on some
    // systems chown() clears the set[ug]id bits for non-superusers
    // thus resulting in incorrect permissions.
    if (F_ISSET(fPreserveOwner)) {
        unsigned int uid, gid;
        // 2-tier trial:  first using the names, then using numeric IDs.
        // Note that it is often impossible to restore the original owner
        // without the super-user rights so no error checking is done here.
        if (!dst->SetOwner(info.GetUserName(), info.GetGroupName(),
                           eIgnoreLinks, &uid, &gid)  &&
            !dst->SetOwner(kEmptyStr, info.GetGroupName(), eIgnoreLinks)) {

            if (uid != info.GetUserId()  ||  gid != info.GetGroupId()) {
                string user  = NStr::UIntToString(info.GetUserId());
                string group = NStr::UIntToString(info.GetGroupId());
                if (!dst->SetOwner(user, group, eIgnoreLinks)) {
                     dst->SetOwner(kEmptyStr, group, eIgnoreLinks);
                }
            }
        }
    }

    // Mode.
    // Set them last.
    if ((F_ISSET(fPreserveMode))  && 
        info.GetType() != CDirEntry::ePipe  &&
        info.GetType() != CDirEntry::eCharSpecial &&
        info.GetType() != CDirEntry::eBlockSpecial)
    {
        bool failed = false;
        int x_errno;
#ifdef NCBI_OS_UNIX
        // We cannot change permissions for sym.links because lchmod()
        // is not portable and is not implemented on majority of platforms.
        if (info.GetType() != CDirEntry::eLink) {
            // Use raw mode here to restore most of the bits
            mode_t mode = info.m_Stat.st_mode;
            if (mode  &&  chmod(dst->GetPath().c_str(), mode) != 0) {
                // May fail due to setuid/setgid bits -- strip'em and try again
                if (mode &   (S_ISUID | S_ISGID)) {
                    mode &= ~(S_ISUID | S_ISGID);
                    failed = chmod(dst->GetPath().c_str(), mode) != 0;
                } else {
                    failed = true;
                }
                x_errno = errno;
            }
        }
#else
        mode_t mode = info.GetMode();
        // Do not try to set zero permissions, it is just not defined
        if ( mode != 0 ) {
            CDirEntry::TMode user, group, other;
            CDirEntry::TSpecialModeBits special_bits;
            CDirEntry::ModeFromModeT(mode, &user, &group, &other, &special_bits);
            failed = !dst->SetMode(user, group, other, special_bits);
            x_errno = errno;
        }
#endif
        if (failed) {
            ARCHIVE_THROW(eRestoreAttrs, "Cannot change mode for '" + dst->GetPath() + '\'' + s_OSReason(x_errno));
        }
    }
}


auto_ptr<CArchive::TEntries> CArchive::x_Append(const string&   src_path,
                                                ELevel          level,
                                                const string&   comment,
                                                const TEntries* toc)
{
    auto_ptr<TEntries> entries(new TEntries);

    const EFollowLinks follow_links = (m_Flags & fFollowLinks) ? eFollowLinks : eIgnoreLinks;
    bool update = true;

    // Clear the entry info
    m_Current = CArchiveEntryInfo();
    // Compose entry name for relative names
    string path = s_ToFilesystemPath(m_BaseDir, src_path);

    // Get dir entry information
    CDirEntry entry(path);
    CDirEntry::SStat st;
    if (!entry.Stat(&st, follow_links)) {
        int x_errno = errno;
        ARCHIVE_THROW(eOpen, "Cannot get status of '" + path + '\''+ s_OSReason(x_errno));
    }
    CDirEntry::EType type = CDirEntry::GetType(st.orig);

    // Get name of the current entry in archive
    string temp = s_ToArchiveName(m_BaseDir, path, HaveSupport(eAbsolutePath));
    if (temp.empty()) {
        ARCHIVE_THROW(eBadName, "Empty entry name in archive");
    }

    // Match masks

    bool match = true;
    if ( m_MaskFullPath.mask ) {
        match = m_MaskFullPath.mask->Match(temp, m_MaskFullPath.acase);
    }
    if ( match  &&  m_MaskPattern.mask ) {
        list<CTempString> elems;
        NStr::Split(temp, "/", elems);
        ITERATE(list<CTempString>, it, elems) {
            if (*it == "..") {
                ARCHIVE_THROW(eBadName, "Name '" + temp + "' embeds parent directory ('..')");
            }
            if (m_MaskPattern.mask->Match(*it, m_MaskPattern.acase)) {
                match = true;
                break;
            }
        }
    }
    if ( !match ) {
        goto out;
    }

    // Check support for this entry type by current archive format
    if (type == CDirEntry::eUnknown  ||  !ARCHIVE->HaveSupport_Type(type)) {
        if (F_ISSET(fSkipUnsupported)) {
            goto out;
        }
        ARCHIVE_THROW(eUnsupportedEntryType, "Cannot append to archive, unsupported entry type for '" + path + "', ");
    }

    if (type == CDirEntry::eDir  &&  temp != "/"  &&  temp != ".") {
        temp += '/';
    }

    // Fill out entry information
    m_Current.m_Name.swap(temp);
    m_Current.m_Type    = type;
    m_Current.m_Comment = comment;

#if 0
    if (m_Format == eZip) {

    } else {
        _TROUBLE;
    }
#endif
#if 0
    if (type == CDirEntry::eLink) {
        _ASSERT(!follow_links);
        m_Current.m_LinkName = entry.LookupLink();
        if (m_Current.m_LinkName.empty()) {
            ARCHIVE_THROW(eBadName, "Empty link name is not allowed");
        }
    }
    unsigned int uid = 0, gid = 0;
    entry.GetOwner(&m_Current.m_UserName, &m_Current.m_GroupName, follow_links, &uid, &gid);
#ifdef NCBI_OS_UNIX
    if (NStr::UIntToString(uid) == m_Current.GetUserName()) {
        m_Current.m_UserName.erase();
    }
    if (NStr::UIntToString(gid) == m_Current.GetGroupName()) {
        m_Current.m_GroupName.erase();
    }
#endif //NCBI_OS_UNIX
#ifdef NCBI_OS_MSWIN
    // These are fake but we don't want to leave plain 0 (root) in there
    st.orig.st_uid = (uid_t) uid;
    st.orig.st_gid = (gid_t) gid;
#endif //NCBI_OS_MSWIN

    m_Current.m_Stat = st.orig;
    // Fixup for mode bits
    m_Current.m_Stat.st_mode = (mode_t) s_ModeToTar(st.orig.st_mode);
#endif

    // Check if we need to update this entry in the archive
#if 0
    if (toc) {
        bool found = false;

        if (type != CDirEntry::eUnknown) {
            // Start searching from the end of the list, to find
            // the most recent entry (if any) first
            _ASSERT(temp.empty());
            REVERSE_ITERATE(TEntries, e, *toc) {
                if (!temp.empty()) {
                    if (e->GetType() == CTarEntryInfo::eHardLink  ||
                        temp != s_ToFilesystemPath(m_BaseDir, e->GetName())) {
                        continue;
                    }
                } else if (path == s_ToFilesystemPath(m_BaseDir,e->GetName())){
                    found = true;
                    if (e->GetType() == CTarEntryInfo::eHardLink) {
                        temp = s_ToFilesystemPath(m_BaseDir, e->GetLinkName());
                        continue;
                    }
                } else {
                    continue;
                }
                if (m_Current.GetType() != e->GetType()) {
                    if (m_Flags & fEqualTypes) {
                        goto out;
                    }
                } else if (m_Current.GetType() == CTarEntryInfo::eLink
                           &&  m_Current.GetLinkName() == e->GetLinkName()) {
                    goto out;
                }
                if (m_Current.GetModificationTime() <=
                    e->GetModificationTime()) {
                    update = false;  // same(or older), no update
                }
                break;
            }
        }

        if (!update  ||  (!found  &&  (m_Flags & (fUpdate & ~fOverwrite)))) {
            if (type != CDirEntry::eDir  &&  type != CDirEntry::eUnknown) {
                goto out;
            }
            // Directories always get recursive treatment later
            update = false;
        }
    }
#endif

    // Append the entry

    switch (type) {
    case CDirEntry::eFile:
        _ASSERT(update);
        if (x_AppendEntry(path, level)) {
            m_Modified = true;
            entries->push_back(m_Current);
        }
        break;

    case CDirEntry::eLink:
    case CDirEntry::eBlockSpecial:
    case CDirEntry::eCharSpecial:
    case CDirEntry::ePipe:
    case CDirEntry::eDoor:
    case CDirEntry::eSocket:
        _ASSERT(update);
        m_Current.m_Stat.st_size = 0;
        if (x_AppendEntry(path)) {
            entries->push_back(m_Current);
        }
        break;

    case CDirEntry::eDir:
        if (update  &&  m_Current.m_Name != ".") {
            // Add information about directory itself
            m_Current.m_Stat.st_size = 0;
            if (x_AppendEntry(path)) {
                entries->push_back(m_Current);
            }
        }
        if (type == CDirEntry::eDir) {
            // Append/update all files from that directory
            CDir::TEntries dir = CDir(path).GetEntries("*", CDir::eIgnoreRecursive);
            ITERATE(CDir::TEntries, e, dir) {
                auto_ptr<TEntries> add = x_Append((*e)->GetPath(), level, kEmptyStr, toc);
                entries->splice(entries->end(), *add);
            }
        }
        break;

    default:
        _TROUBLE;
    }
 out:
    return entries;
}


bool CArchive::x_AppendEntry(const string& path, ELevel level)
{
    // User callback
    if (!Checkpoint(m_Current, eAppend)) {
        return false;
    }
    AppendEntry(path, level);
    m_Modified = true;
    return true;
}



//////////////////////////////////////////////////////////////////////////////
//
// CArchiveFile
//

CArchiveFile::CArchiveFile(EFormat format, const string& filename)
    : CArchive(format)
{
    // CArchive
    m_Location = IArchive::eFile;
    m_Flags    = fDefault;
    // CArchiveFile
    m_FileName = filename;
    return;
}


void CArchiveFile::Open(EAction action)
{
    bool isread = (action & eRO) > 0;
    if (isread) {
        ARCHIVE->OpenFile(m_FileName);
    } else {
        ARCHIVE->CreateFile(m_FileName);
    }
    return;
}



//////////////////////////////////////////////////////////////////////////////
//
// CArchiveMemory
//

CArchiveMemory::CArchiveMemory(EFormat format, const void* ptr, size_t size)
    : CArchive(format)
{
    // CArchive
    m_Location = IArchive::eMemory;
    m_Flags    = fDefault;
    // CArchiveMemory
    m_Buf      = ptr;
    m_BufSize  = size;
    m_InitialAllocationSize = 0;
    return;
}


void CArchiveMemory::Create(void)
{
    Create(0);
}


void CArchiveMemory::Create(size_t initial_allocation_size)
{
    ARCHIVE_CHECK;
    m_InitialAllocationSize = initial_allocation_size;
    m_Buf = NULL;
    m_OwnBuf.reset();
    x_Open(eCreate);
    return;
}


void CArchiveMemory::Open(EAction action)
{
    bool isread = (action & eRO) > 0;
    if (isread) {
        ARCHIVE->OpenMemory(m_Buf, m_BufSize);
    } else {
        ARCHIVE->CreateMemory(m_InitialAllocationSize);
    }
    return;
}


void CArchiveMemory::Finalize(void** buf_ptr, size_t* buf_size_ptr)
{
    if (!buf_ptr  || !buf_size_ptr) {
        NCBI_THROW(CCoreException, eInvalidArg, "Bad memory buffer");
    }
    ARCHIVE_CHECK;
    ARCHIVE->FinalizeMemory(buf_ptr, buf_size_ptr);
    m_Buf     = *buf_ptr;
    m_BufSize = *buf_size_ptr;
    return;
}


void CArchiveMemory::Save(const string& filename)
{
    ARCHIVE_CHECK;
    if (!m_Buf || !m_BufSize) {
        NCBI_THROW(CCoreException, eInvalidArg, "Bad memory buffer");
    }
    CFileIO fio;
    fio.Open(filename, CFileIO::eCreate, CFileIO::eReadWrite);
    size_t n_written = fio.Write(m_Buf, m_BufSize);
    if (n_written != m_BufSize) {
        ARCHIVE_THROW(eWrite, "Failed to write archive to file");
    }
    fio.Close();
}


void CArchiveMemory::Load(const string& filename)
{
    // Close current archive, if any
    Close();

    // Get file size and allocate memory to load it
    CFile f(filename);
    Int8 filesize = f.GetLength();
    if (filesize < 0) {
        int x_errno = errno;
        ARCHIVE_THROW(eOpen, "Cannot get status of '" + filename + '\''+ s_OSReason(x_errno));
    }
    if (!filesize) {
        ARCHIVE_THROW(eOpen, "Cannot load empty file '" + filename + "' to memory");
    }
    AutoArray<char> tmp((size_t)filesize);

    // Read file into temporary buffer
    CFileIO fio;
    fio.Open(filename, CFileIO::eOpen, CFileIO::eRead);
    size_t n_read = fio.Read(tmp.get(), (size_t)filesize);
    if (n_read != (size_t)filesize) {
        ARCHIVE_THROW(eWrite, "Failed to load archive to memory");
    }
    fio.Close();

    // Set new buffer
    m_OwnBuf  = tmp;
    m_Buf     = m_OwnBuf.get();
    m_BufSize = n_read;
}


END_NCBI_SCOPE
