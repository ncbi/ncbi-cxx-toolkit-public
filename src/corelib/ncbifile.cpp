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
 * Author:  Vladimir Ivanov
 *
 * File Description:   Files and directories accessory functions
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_limits.h>
#include <corelib/ncbi_limits.hpp>
#include <corelib/ncbi_safe_static.hpp>
#include <corelib/error_codes.hpp>
#include <corelib/ncbierror.hpp>
#include <atomic>
#include <stdio.h>

#if defined(NCBI_OS_MSWIN)
#  include "ncbi_os_mswin_p.hpp"
#  include <direct.h>
#  include <io.h>
#  include <fcntl.h> // for _O_* flags
#  include <sys/utime.h>

#elif defined(NCBI_OS_UNIX)
#  include "ncbi_os_unix_p.hpp"
#  include <corelib/impl/ncbi_panfs.h>
#  include <dirent.h>
#  include <fcntl.h>
#  include <unistd.h>
#  include <utime.h>
#  include <sys/mman.h>
#  include <sys/time.h>
#  ifdef HAVE_SYS_STATVFS_H
#    include <sys/statvfs.h>
#  endif
#  include <sys/param.h>
#  ifdef HAVE_SYS_MOUNT_H
#    include <sys/mount.h>
#  endif
#  ifdef HAVE_SYS_VFS_H
#    include <sys/vfs.h>
#  endif
#  if !defined(MAP_FAILED)
#    define MAP_FAILED ((void *)(-1L))
#  endif
#  include <sys/ioctl.h>

#else
#  error "File API defined for MS Windows and UNIX platforms only"

#endif  /* NCBI_OS_MSWIN, NCBI_OS_UNIX */


// Define platforms on which we support PANFS
#if defined(NCBI_OS_UNIX)  &&  !defined(NCBI_OS_CYGWIN)
#  define SUPPORT_PANFS
#  include <sys/types.h>
#  include <sys/wait.h>
#endif


#define NCBI_USE_ERRCODE_X   Corelib_File


BEGIN_NCBI_SCOPE


// Path separators

#undef  DIR_SEPARATOR
#undef  DIR_SEPARATOR_ALT
#undef  DIR_SEPARATORS
#undef  DISK_SEPARATOR
#undef  ALL_SEPARATORS
#undef  ALL_OS_SEPARATORS

#define DIR_PARENT  ".."
#define DIR_CURRENT "."
#define ALL_OS_SEPARATORS   ":/\\"

#if defined(NCBI_OS_MSWIN)
#  define DIR_SEPARATOR     '\\'
#  define DIR_SEPARATOR_ALT '/'
#  define DISK_SEPARATOR    ':'
#  define DIR_SEPARATORS    "/\\"
#  define ALL_SEPARATORS    ":/\\"
#elif defined(NCBI_OS_UNIX)
#  define DIR_SEPARATOR     '/'
#  define DIR_SEPARATORS    "/"
#  define ALL_SEPARATORS    "/"
#endif

// Macro to check bits
#define F_ISSET(flags, mask) ((flags & (mask)) == (mask))

// Default buffer size, used to read/write files
const size_t kDefaultBufferSize = 64*1024;

// List of files for CFileDeleteAtExit class
static CSafeStatic< CFileDeleteList > s_DeleteAtExitFileList;


// Declare the parameter to get directory for temporary files.
// Registry file:
//     [NCBI]
//     TmpDir = ...
// Environment variable:
//     NCBI_CONFIG__NCBI__TmpDir
//
NCBI_PARAM_DECL(string, NCBI, TmpDir); 
NCBI_PARAM_DEF (string, NCBI, TmpDir, "");


// Define how read-only files are treated on Windows.
// Registry file:
//     [NCBI]
//     DeleteReadOnlyFiles = true/false
// Environment variable:
//     NCBI_CONFIG__DELETEREADONLYFILES
//
NCBI_PARAM_DECL(bool, NCBI, DeleteReadOnlyFiles);
NCBI_PARAM_DEF_EX(bool, NCBI, DeleteReadOnlyFiles, false,
    eParam_NoThread, NCBI_CONFIG__DELETEREADONLYFILES);


// Declare how umask settings on Unix affect creating files/directories 
// in the File API.
// Registry file:
//     [NCBI]
//     FileAPIHonorUmask = true/false
// Environment variable:
//     NCBI_CONFIG__FILEAPIHONORUMASK
//
// On WINDOWS: umask affect only CRT function, the part of API that
// use Windows API directly just ignore umask setting.
#define DEFAULT_HONOR_UMASK_VALUE false

NCBI_PARAM_DECL(bool, NCBI, FileAPIHonorUmask);
NCBI_PARAM_DEF_EX(bool, NCBI, FileAPIHonorUmask, DEFAULT_HONOR_UMASK_VALUE,
    eParam_NoThread, NCBI_CONFIG__FILEAPIHONORUMASK);


// Declare the parameter to turn on logging from CFile,
// CDirEntry, etc. classes.
// Registry file:
//     [NCBI]
//     FileAPILogging = true/false
// Environment variable:
//     NCBI_CONFIG__FILEAPILOGGING
//
#define DEFAULT_LOGGING_VALUE false

NCBI_PARAM_DECL(bool, NCBI, FileAPILogging);
NCBI_PARAM_DEF_EX(bool, NCBI, FileAPILogging, DEFAULT_LOGGING_VALUE,
    eParam_NoThread, NCBI_CONFIG__FILEAPILOGGING);


// NOTE:
//
// Some common and very often used methods like
//    CDirEntry::GetType()
//    CDirEntry::Exists()
// don't use LOG_ERROR* to report errors, and just set an error codes to CNcbiError.

// Report error without error code.
// Used when error code is unknown, or in conjunction with methods
// that already report errors and codes itself.
#define LOG_ERROR(subcode, log_message) \
    { \
        if (NCBI_PARAM_TYPE(NCBI, FileAPILogging)::GetDefault()) { \
            ERR_POST_X(subcode, log_message); \
        } \
    }

// Report general NCBI error
#define LOG_ERROR_NCBI(subcode, log_message, ncbierr) \
    { \
        CNcbiError::Set(ncbierr, log_message); \
        if (NCBI_PARAM_TYPE(NCBI, FileAPILogging)::GetDefault()) { \
            ERR_POST_X(subcode, log_message); \
        } \
    }

// Report Windows API error.
// Should be called immediately after failed Windows API call.
#define LOG_ERROR_WIN(subcode, log_message) \
    { \
        CNcbiError::SetFromWindowsError(log_message); \
        if (NCBI_PARAM_TYPE(NCBI, FileAPILogging)::GetDefault()) { \
            ERR_POST_X(subcode, log_message); \
        } \
    }

// Report errno-based error.
// Should be called immediately after system call.
#define LOG_ERROR_ERRNO(subcode, log_message) \
    { \
        int saved_error = errno; \
        CNcbiError::SetErrno(saved_error, log_message); \
        if (NCBI_PARAM_TYPE(NCBI, FileAPILogging)::GetDefault()) { \
            ERR_POST_X(subcode, log_message << ": " << _T_STDSTRING(NcbiSys_strerror(saved_error))); \
        } \
        errno = saved_error; \
    }
    
// Macro to silence GCC's __wur (warn unused result)
#define _no_warning(expr)  while ( expr ) break

// Get an error string for last error on Windows
#define WIN_LAST_ERROR_STR CLastErrorAdapt::GetErrCodeString(::GetLastError())

// tv_usec part for struct timeval have an 'int' type on Darwin, so we need a conversion from 'long' to avoid warnings
#if defined(NCBI_OS_DARWIN)
#   define TV_USEC(x) (int)(x)
#else
#   define TV_USEC(x) x
#endif



//////////////////////////////////////////////////////////////////////////////
//
// CDirEntry
//

CDirEntry::CDirEntry(const string& name)
{
    Reset(name);
    m_DefaultMode[eUser]    = m_DefaultModeGlobal[eFile][eUser];
    m_DefaultMode[eGroup]   = m_DefaultModeGlobal[eFile][eGroup];
    m_DefaultMode[eOther]   = m_DefaultModeGlobal[eFile][eOther];
    m_DefaultMode[eSpecial] = m_DefaultModeGlobal[eFile][eSpecial];
}


CDirEntry::CDirEntry(const CDirEntry& other)
    : m_Path(other.m_Path)
{
    m_DefaultMode[eUser]    = other.m_DefaultMode[eUser];
    m_DefaultMode[eGroup]   = other.m_DefaultMode[eGroup];
    m_DefaultMode[eOther]   = other.m_DefaultMode[eOther];
    m_DefaultMode[eSpecial] = other.m_DefaultMode[eSpecial];
}


CDirEntry::~CDirEntry(void)
{
    return;
}


CDirEntry* CDirEntry::CreateObject(EType type, const string& path)
{
    CDirEntry *ptr;
    switch ( type ) {
        case eFile:
            ptr = new CFile(path);
            break;
        case eDir:
            ptr = new CDir(path);
            break;
        case eLink:
            ptr = new CSymLink(path);
            break;
        default:
            ptr = new CDirEntry(path);
            break;
    }
    return ptr;
}


void CDirEntry::Reset(const string& path)
{
    m_Path = path;
    size_t len = path.length();
    // Root dir
    if ((len == 1)  &&  IsPathSeparator(path[0])) {
        return;
    }
    // Disk name
#  if defined(DISK_SEPARATOR)
    if ( (len == 2 || len == 3) && (path[1] == DISK_SEPARATOR) ) {
        return;
    }
#  endif
    m_Path = DeleteTrailingPathSeparator(path);
}


CDirEntry::TMode CDirEntry::m_DefaultModeGlobal[eUnknown][4] =
{
    // eFile
    { CDirEntry::fDefaultUser, CDirEntry::fDefaultGroup, CDirEntry::fDefaultOther, 0 },
    // eDir
    { CDirEntry::fDefaultDirUser, CDirEntry::fDefaultDirGroup, CDirEntry::fDefaultDirOther, 0 },
    // ePipe
    { CDirEntry::fDefaultUser, CDirEntry::fDefaultGroup, CDirEntry::fDefaultOther, 0 },
    // eLink
    { CDirEntry::fDefaultUser, CDirEntry::fDefaultGroup, CDirEntry::fDefaultOther, 0 },
    // eSocket
    { CDirEntry::fDefaultUser, CDirEntry::fDefaultGroup, CDirEntry::fDefaultOther, 0 },
    // eDoor
    { CDirEntry::fDefaultUser, CDirEntry::fDefaultGroup, CDirEntry::fDefaultOther, 0 },
    // eBlockSpecial
    { CDirEntry::fDefaultUser, CDirEntry::fDefaultGroup, CDirEntry::fDefaultOther, 0 },
    // eCharSpecial
    { CDirEntry::fDefaultUser, CDirEntry::fDefaultGroup, CDirEntry::fDefaultOther, 0 }
};


// Default backup suffix
const char* CDirEntry::m_BackupSuffix = ".bak";

// Part of the temporary name used for "safe copy"
static const char* kTmpSafeSuffix = ".tmp.";



CDirEntry& CDirEntry::operator= (const CDirEntry& other)
{
    if (this != &other) {
        m_Path                  = other.m_Path;
        m_DefaultMode[eUser]    = other.m_DefaultMode[eUser];
        m_DefaultMode[eGroup]   = other.m_DefaultMode[eGroup];
        m_DefaultMode[eOther]   = other.m_DefaultMode[eOther];
        m_DefaultMode[eSpecial] = other.m_DefaultMode[eSpecial];
    }
    return *this;
}


void CDirEntry::SplitPath(const string& path, string* dir,
                          string* base, string* ext)
{
    // Get file name
    size_t pos = path.find_last_of(ALL_SEPARATORS);
    string filename = (pos == NPOS) ? path : path.substr(pos+1);

    // Get dir
    if ( dir ) {
        *dir = (pos == NPOS) ? kEmptyStr : path.substr(0, pos+1);
    }
    // Split file name to base and extension
    pos = filename.rfind('.');
    if ( base ) {
        *base = (pos == NPOS) ? filename : filename.substr(0, pos);
    }
    if ( ext ) {
        *ext = (pos == NPOS) ? kEmptyStr : filename.substr(pos);
    }
}


void CDirEntry::SplitPathEx(const string& path,
                            string* disk, string* dir,
                            string* base, string* ext)
{
    size_t start_pos = 0;

    // Get disk
    if ( disk ) {
        if ( isalpha((unsigned char)path[0])  &&  path[1] == ':' ) {
            *disk = path.substr(0, 2);
            start_pos = 2;
        } else {
            *disk = kEmptyStr;
        }
    }
    // Get file name
    size_t pos = path.find_last_of(ALL_OS_SEPARATORS);
    string filename = (pos == NPOS) ? path : path.substr(pos+1);
    // Get dir
    if ( dir ) {
        *dir = (pos == NPOS) ? kEmptyStr : path.substr(start_pos, pos - start_pos + 1);
    }
    // Split file name to base and extension
    pos = filename.rfind('.');
    if ( base ) {
        *base = (pos == NPOS) ? filename : filename.substr(0, pos);
    }
    if ( ext ) {
        *ext = (pos == NPOS) ? kEmptyStr : filename.substr(pos);
    }
}


string CDirEntry::MakePath(const string& dir, const string& base, 
                           const string& ext)
{
    string path;

    // Adding "dir" and file base
    if ( dir.length() ) {
        path = AddTrailingPathSeparator(dir);
    }
    path += base;
    // Adding extension
    if ( ext.length()  &&  ext.at(0) != '.' ) {
        path += '.';
    }
    path += ext;
    // Return result
    return path;
}


char CDirEntry::GetPathSeparator(void) 
{
    return DIR_SEPARATOR;
}


bool CDirEntry::IsPathSeparator(const char c)
{
#if defined(DISK_SEPARATOR)
    if ( c == DISK_SEPARATOR ) {
        return true;
    }
#endif
#if defined(DIR_SEPARATOR_ALT)
    if ( c == DIR_SEPARATOR_ALT ) {
        return true;
    }
#endif
    return c == DIR_SEPARATOR;
}


string CDirEntry::AddTrailingPathSeparator(const string& path)
{
    size_t len = path.length();
    if (len  &&  string(ALL_SEPARATORS).rfind(path.at(len - 1)) == NPOS) {
        return path + GetPathSeparator();
    }
    return path;
}


string CDirEntry::DeleteTrailingPathSeparator(const string& path)
{
    size_t pos = path.find_last_not_of(DIR_SEPARATORS);
    if (pos + 1 < path.length()) {
        return path.substr(0, pos + 1);
    }
    return path;
}


string CDirEntry::GetDir(EIfEmptyPath mode) const
{
    string dir;
    SplitPath(GetPath(), &dir);
    if ( dir.empty()  &&  mode == eIfEmptyPath_Current  &&  !GetPath().empty() ) {
        return string(DIR_CURRENT) + DIR_SEPARATOR;
    }
    return dir;
}


// Windows: 
inline bool s_Win_IsDiskPath(const string& path)
{
    if ((isalpha((unsigned char)path[0]) && path[1] == ':')  &&
         (path[2] == '/' || path[2] == '\\')) {
        return true;
    }
    return false;
}

// Windows: 
inline bool s_Win_IsNetworkPath(const string& path)
{
    // Any combination of slashes in 2 first symbols
    if ((path[0] == '\\' || path[0] == '/')  &&
        (path[1] == '\\' || path[1] == '/')) {
        return true;
    }
    return false;
}


bool CDirEntry::IsAbsolutePath(const string& path)
{
    if (path.empty()) {
        return false;
    }
#if defined(NCBI_OS_MSWIN)
    if (s_Win_IsDiskPath(path) || s_Win_IsNetworkPath(path)) {
        return true;
    }
#elif defined(NCBI_OS_UNIX)
    if ( path[0] == '/' ) {
        return true;
    }
#endif
    return false;
}


bool CDirEntry::IsAbsolutePathEx(const string& path)
{
    if ( path.empty() )
        return false;

    // Windows: 
    if (s_Win_IsDiskPath(path) || s_Win_IsNetworkPath(path)) {
        return true;
    }
    // Unix
    if (path[0] == '/') {
        // FIXME:
        // This is an Unix absolute path or MS Windows relative.
        // But Unix have favor here, because '/' is native dir separator.
        return true;
    }
    // Else - relative
    return false;
}


string CDirEntry::GetNearestExistingParentDir(const string& path)
{
    CDirEntry entry(NormalizePath(path));

    // Find closest existing directory
    while (!entry.Exists()) {
        string dir = entry.GetDir();
        if (dir.empty()) {
            NCBI_THROW(CFileException, eNotExists,
                       "Failed to find existing containing directory for: " + entry.GetPath());
        }
        entry.Reset(dir);
    }
    return entry.GetPath();
}


/// Helper -- strips dir to parts:
///     c:\a\b\     will be <c:><a><b>
///     /usr/bin/   will be </><usr><bin>
static void s_StripDir(const string& dir, vector<string> * dir_parts)
{
    dir_parts->clear();
    if (dir.empty()) {
        return;
    }
    const char sep = CDirEntry::GetPathSeparator();

    size_t sep_pos = 0;
    size_t last_ind = dir.length() - 1;
    size_t part_start = 0;
    for (;;) {
        sep_pos = dir.find(sep, sep_pos);
        if (sep_pos == NPOS) {
            dir_parts->push_back(string(dir, part_start, dir.length() - part_start));
            break;
        }
        // If path starts from '/' - it's a root directory
        if (sep_pos == 0) {
            dir_parts->push_back(string(1, sep));
        } else {
            dir_parts->push_back(string(dir, part_start, sep_pos - part_start));
        }
        sep_pos++;
        part_start = sep_pos;
        if (sep_pos >= last_ind) {
            break;
        }
    }
}


string CDirEntry::CreateRelativePath( const string& path_from, 
                                      const string& path_to )
{
    string path; // the result    
    
    if ( !IsAbsolutePath(path_from) ) {
        NCBI_THROW(CFileException, eRelativePath, "path_from is not absolute path");
    }

    if ( !IsAbsolutePath(path_to) ) {
        NCBI_THROW(CFileException, eRelativePath, "path_to is not absolute path");
    }

    // Split and strip FROM
    string dir_from;
    SplitPath(AddTrailingPathSeparator(path_from), &dir_from);
    vector<string> dir_from_parts;
    s_StripDir(dir_from, &dir_from_parts);
    if ( dir_from_parts.empty() ) {
        NCBI_THROW(CFileException, eRelativePath, "path_from is empty path");
    }

    // Split and strip TO
    string dir_to, base_to, ext_to;
    SplitPath(path_to, &dir_to, &base_to, &ext_to);    
    vector<string> dir_to_parts;
    s_StripDir(dir_to, &dir_to_parts);
    if ( dir_to_parts.empty() ) {
        NCBI_THROW(CFileException, eRelativePath, "path_to is empty path");
    }

    // Platform-dependent compare mode
#ifdef NCBI_OS_MSWIN
#  define DIR_PARTS_CMP_MODE NStr::eNocase
#else /*NCBI_OS_UNIX*/
#  define DIR_PARTS_CMP_MODE NStr::eCase
#endif
    // Roots must be the same to create relative path from one to another

    if (NStr::Compare(dir_from_parts.front(), 
                      dir_to_parts.front(), 
                      DIR_PARTS_CMP_MODE) != 0) {
        NCBI_THROW(CFileException, eRelativePath, "roots of input paths are different");
    }

    size_t min_parts = min(dir_from_parts.size(), dir_to_parts.size());
    size_t common_length = min_parts;
    for (size_t i = 0; i < min_parts; i++) {
        if (NStr::Compare(dir_from_parts[i], dir_to_parts[i], DIR_PARTS_CMP_MODE) != 0) {
            common_length = i;
            break;
        }
    }
    for (size_t i = common_length; i < dir_from_parts.size(); i++) {
        path += "..";
        path += GetPathSeparator();
    }
    for (size_t i = common_length; i < dir_to_parts.size(); i++) {
        path += dir_to_parts[i];
        path += GetPathSeparator();
    }
    
    return path + base_to + ext_to;
}


string CDirEntry::CreateAbsolutePath(const string& path, ERelativeToWhat rtw)
{
    if ( IsAbsolutePath(path) ) {
        return NormalizePath(path);
    }
    string result;

#if defined(NCBI_OS_MSWIN)
    if ( path.find(DISK_SEPARATOR) != NPOS ) {
        NCBI_THROW(CFileException, eRelativePath, 
                   "Path must not contain disk separator: " + path);
    }
    // Path started with slash
    if (!path.empty() && (path[0] == '/' || path[0] == '\\')) {
        // network path ?
        if ( s_Win_IsNetworkPath(path) ) {
            NCBI_THROW(CFileException, eRelativePath,
                       "Cannot use network path: " + path);
        }
        // relative to current drive only
        if (rtw != eRelativeToCwd) {
            NCBI_THROW(CFileException, eRelativePath,
                       "Path can be used as relative to current drive only: " + path);
        }
        return CDir::GetCwd().substr(0,3) + path;
    }
#endif

    switch (rtw) {
        case eRelativeToCwd:
            result = ConcatPath(CDir::GetCwd(), path);
            break;
        case eRelativeToExe:
          {
            string dir;
            SplitPath(CNcbiApplication::GetAppName(CNcbiApplication::eFullName), &dir);
            result = ConcatPath(dir, path);
            if ( !CDirEntry(result).Exists() ) {
                SplitPath(CNcbiApplication::GetAppName(CNcbiApplication::eRealName), &dir);
                result = ConcatPath(dir, path);
            }
            break;
          }
    }
    return NormalizePath(result);
}


string CDirEntry::CreateAbsolutePath(const string& path, const string& rtw)
{
    if ( IsAbsolutePath(path) ) {
        return NormalizePath(path);
    }

#if defined(NCBI_OS_MSWIN)
    if ( path.find(DISK_SEPARATOR) != NPOS ) {
        NCBI_THROW(CFileException, eRelativePath, 
                   "Path must not contain disk separator: " + path);
    }
    // Path started with slash
    if (!path.empty() && (path[0] == '/' || path[0] == '\\')) {
        // network path ?
        if ( s_Win_IsNetworkPath(path) ) {
            NCBI_THROW(CFileException, eRelativePath,
                       "Cannot use network path: " + path);
        }
        // relative to current drive only -- error
        NCBI_THROW(CFileException, eRelativePath,
                    "Path can be used as relative to current drive only: " + path);
    }
#endif

    if ( !IsAbsolutePath(rtw) ) {
        NCBI_THROW(CFileException, eRelativePath,
                   "2nd parameter must represent absolute path: " + rtw);
    }
    return NormalizePath(ConcatPath(rtw, path));
}


string CDirEntry::ConvertToOSPath(const string& path)
{
    // Not process empty or absolute path
    if ( path.empty()  ||  IsAbsolutePathEx(path)) {
        return NormalizePath(path);
    }
    // Now we have relative "path"
    string xpath = path;
    // Add trailing separator if path ends with DIR_PARENT or DIR_CURRENT
#if defined(DIR_PARENT)
    if ( NStr::EndsWith(xpath, DIR_PARENT) )  {
        xpath += DIR_SEPARATOR;
    }
#endif
#if defined(DIR_CURRENT)
    if ( NStr::EndsWith(xpath, DIR_CURRENT) )  {
        xpath += DIR_SEPARATOR;
    }
#endif
    // Replace each path separator with the current OS separator character
    for (size_t i = 0; i < xpath.length(); i++) {
        char c = xpath[i];
        if ( c == '\\' || c == '/' ) {
            xpath[i] = DIR_SEPARATOR;
        }
    }
    xpath = NormalizePath(xpath);
    return xpath;
}


string CDirEntry::ConcatPath(const string& first, const string& second)
{
    // Prepare first part of path
    string path = AddTrailingPathSeparator(NStr::TruncateSpaces(first));
    // Remove leading separator in "second" part
    string part = NStr::TruncateSpaces(second);
    if ( !path.empty()  &&  part.length() > 0  &&  part[0] == DIR_SEPARATOR ) {
        part.erase(0,1);
    }
    // Add second part
    path += part;
    return path;
}


string CDirEntry::ConcatPathEx(const string& first, const string& second)
{
    // Prepare first part of path
    CTempString path = NStr::TruncateSpaces_Unsafe(first);

    // Add trailing path separator to first part (OS independence)
    char sep;
    size_t pos = path.size();
    if ( pos  &&  CTempString(ALL_OS_SEPARATORS,
                             sizeof(ALL_OS_SEPARATORS) - 1).find(path[pos - 1]) == NPOS ) {
        // Find used path separator
        size_t sep_pos = path.find_last_of(ALL_OS_SEPARATORS);
        sep = sep_pos != NPOS ? path[sep_pos] : GetPathSeparator();
    } else
        sep = '\0';

    // Remove leading separator in "second" part
    CTempString part = NStr::TruncateSpaces_Unsafe(second);
    if ( part.size() > 0  &&
         CTempString(ALL_OS_SEPARATORS,
                     sizeof(ALL_OS_SEPARATORS) - 1).find(part[0]) != NPOS ) {
        part = part.substr(1);
    }

    // Add it all together
    return string(path) + (sep ? string(1, sep) : kEmptyStr) + string(part);
}


string CDirEntry::NormalizePath(const string& path, EFollowLinks follow_links)
{
    if ( path.empty() ) {
        return path;
    }
    static const char kSep[] = { DIR_SEPARATOR, '\0' };

    std::list<string> head;       // already resolved to our satisfaction
    std::list<string> tail;       // to resolve afterwards
    string  current;              // to resolve next
    int     link_depth = 0;

    // Delete trailing slash for all paths except similar to 'd:\'
#  ifdef DISK_SEPARATOR
    if ( path.find(DISK_SEPARATOR) == NPOS ) {
        current = DeleteTrailingPathSeparator(path);
    } else {
        current = path;
    }
#  else
    current = DeleteTrailingPathSeparator(path);
#  endif

    if ( current.empty() ) {
        // root dir
        return string(1, DIR_SEPARATOR);
    }

#ifdef NCBI_OS_MSWIN
    NStr::ReplaceInPlace(current, "/", "\\");
#endif

    while ( !current.empty()  ||  !tail.empty() ) {
        std::list<string> pretail;
        if ( !current.empty() ) {
            NStr::Split(current, kSep, pretail);
            current.erase();
            if (pretail.front().empty()
#ifdef DISK_SEPARATOR
                ||  pretail.front().find(DISK_SEPARATOR) != NPOS
#endif
                ) {
                // Absolute path
                head.clear();
#ifdef NCBI_OS_MSWIN
                // Remove leading "\\?\". Replace leading "\\?\UNC\" with "\\".
                static const char* const kUNC[] = { "", "", "?", "UNC" };
                std::list<string>::iterator it = pretail.begin();
                unsigned int matched = 0;
                while (matched < 4  &&  it != pretail.end()
                       &&  !NStr::CompareNocase(*it, kUNC[matched])) {
                    ++it;
                    ++matched;
                }
                pretail.erase(pretail.begin(), it);
                switch (matched) {
                case 2: case 4: // got a UNC path (\\... or \\?\UNC\...)
                    head.push_back(kEmptyStr);
                    // fall through
                case 1:         // normal volume-less absolute path
                    head.push_back(kEmptyStr);
                    break;
                case 3/*?*/:    // normal path, absolute or relative
                    break;
                }
#endif
            }
            tail.splice(tail.begin(), pretail);
        }

        string next;
        if (!tail.empty()) {
            next = tail.front();
            tail.pop_front();
        }
        if ( !head.empty() ) { // empty heads should accept anything
            string& last = head.back();
            if (last == DIR_CURRENT) {
                if (!next.empty()) {
                    head.pop_back();
                }
#ifdef DISK_SEPARATOR
            } else if (!last.empty() && last[last.size()-1] == DISK_SEPARATOR) {
                // Allow almost anything right after a volume specification
#endif
            } else if (next == DIR_CURRENT) {
                // Leave out, since we already have content
                continue;
            } else if (next.empty()) {
                continue; // leave out empty components in most cases
            } else if (next == DIR_PARENT) {
#ifdef DISK_SEPARATOR
                SIZE_TYPE pos;
#endif
                // Back up if possible, assuming existing path to be "physical"
                if (last.empty()) {
                    // Already at the root; .. is a no-op
                    continue;
#ifdef DISK_SEPARATOR
                } else if ((pos = last.find(DISK_SEPARATOR) != NPOS)) {
                    last.erase(pos + 1);
#endif
                } else if (last != DIR_PARENT) {
                    head.pop_back();
                    continue;
                }
            }
        }
#ifdef NCBI_OS_UNIX
        // Is there a Windows equivalent for readlink?
        if ( follow_links ) {
            string s(head.empty() ? next : NStr::Join(head, string(1, DIR_SEPARATOR)) + DIR_SEPARATOR + next);
            char buf[PATH_MAX];
            int  length = (int)readlink(s.c_str(), buf, sizeof(buf));
            if (length > 0) {
                current.assign(buf, length);
                if (++link_depth >= 1024) {
                    ERR_POST_X(1, Warning << "CDirEntry::NormalizePath(): "
                               "Reached symlink depth limit " <<
                               link_depth << " when resolving " << path);
                    CNcbiError::Set(CNcbiError::eTooManySymbolicLinkLevels);
                    follow_links = eIgnoreLinks;
                }
                continue;
            }
        }
#endif
        // Normal case: just append the next element to head
        head.push_back(next);
    }

    // Special cases
    if ( (head.size() == 0)  ||
         (head.size() == 2  &&  head.front() == DIR_CURRENT  &&  head.back().empty()) ) {
        // current dir
        return DIR_CURRENT;
    }
    if (head.size() == 1  &&  head.front().empty()) {
        // root dir
        return string(1, DIR_SEPARATOR);
    }
#ifdef DISK_SEPARATOR
    if (head.front().find(DISK_SEPARATOR) != NPOS) {
        if ((head.size() == 2  &&  head.back() == DIR_CURRENT) || 
            (head.size() == 3  &&  *(++head.begin()) == DIR_CURRENT  &&  head.back().empty()) ) {
            // root dir on drive X:
            return head.front() + DIR_SEPARATOR;
        }
    }
#endif
    // Compose path
    return NStr::Join(head, string(1, DIR_SEPARATOR));
}


bool CDirEntry::GetMode(TMode* user_mode, TMode* group_mode,
                        TMode* other_mode, TSpecialModeBits* special) const
{
    TNcbiSys_stat st;
    if (NcbiSys_stat(_T_XCSTRING(GetPath()), &st) != 0) {
        LOG_ERROR_ERRNO(5, "CDirEntry::GetMode(): stat() failed for: " + GetPath());
        return false;
    }
    ModeFromModeT(st.st_mode, user_mode, group_mode, other_mode, special);
    return true;
}


// Auxiliary macro to set/clear/replace current permissions

#define UPDATE_PERMS(mode, perms) \
    { \
        _ASSERT( !F_ISSET(perms, fModeNoChange | fModeAdd)    ); \
        _ASSERT( !F_ISSET(perms, fModeNoChange | fModeRemove) ); \
        _ASSERT( !F_ISSET(perms, fModeAdd      | fModeRemove) ); \
        \
        if ( perms & fModeNoChange ) { \
            \
        } else  \
        if ( perms & fModeAdd ) { \
            mode |= perms; \
        } else  \
        if ( perms & fModeRemove ) { \
            mode &= ~perms; \
        } \
        else { \
            mode = perms; \
        } \
        /* clear auxiliary bits */ \
        mode &= ~(fDefault | static_cast<TMode>( \
                       fModeAdd | fModeRemove | fModeNoChange)); \
    }


bool CDirEntry::SetMode(TMode user_mode, TMode group_mode,
                        TMode other_mode, TSpecialModeBits special_mode,
                        TSetModeFlags flags) const
{
    // Assumption
    _ASSERT(eEntryOnly == fEntry);

    // Is this a directory ? (and processing not entry only)
    if ( (flags & (fDir_All | fDir_Recursive)) != eEntryOnly  &&  IsDir(eIgnoreLinks) ) {
        return CDir(GetPath()).SetMode(user_mode, group_mode, other_mode, special_mode, flags);
    }
    // Other entries
    return SetModeEntry(user_mode, group_mode, other_mode, special_mode, flags);
}


bool CDirEntry::SetModeEntry(TMode user_mode, TMode group_mode,
                             TMode other_mode, TSpecialModeBits special_mode,
                             TSetModeFlags flags) const
{
    // Check on defaults modes
    if (user_mode & fDefault) {
        user_mode = m_DefaultMode[eUser];
    }
    if (group_mode & fDefault) {
        group_mode = m_DefaultMode[eGroup];
    }
    if (other_mode & fDefault) {
        other_mode = m_DefaultMode[eOther];
    }
    if (special_mode == 0) {
        special_mode = m_DefaultMode[eSpecial];
    }
    
    TMode user  = 0;
    TMode group = 0;
    TMode other = 0;
    TSpecialModeBits special = 0;
    TMode relative_mask = fModeNoChange | fModeAdd | fModeRemove;

    // relative permissions
    
    if ( (user_mode    & relative_mask) ||
         (group_mode   & relative_mask) ||
         (other_mode   & relative_mask) ||
         (special_mode & relative_mask) ) {

        TNcbiSys_stat st;
        if (NcbiSys_stat(_T_XCSTRING(GetPath()), &st) != 0) {
            if ( (flags & fIgnoreMissing)  &&  (errno == ENOENT) ) {
                return true;
            }
            LOG_ERROR_ERRNO(6, "CDirEntry::SetModeEntry(): stat() failed for: " + GetPath());
            return false;
        }
        ModeFromModeT(st.st_mode, &user, &group, &other);
    }
    
    UPDATE_PERMS(user,    user_mode);
    UPDATE_PERMS(group,   group_mode);
    UPDATE_PERMS(other,   other_mode);
    UPDATE_PERMS(special, special_mode);
   
    // change permissions
    
    mode_t mode = MakeModeT(user, group, other, special);

    if ( NcbiSys_chmod(_T_XCSTRING(GetPath()), mode) != 0 ) {
        if ( (flags & fIgnoreMissing)  &&  (errno == ENOENT) ) {
            return true;
        }
        LOG_ERROR_ERRNO(7, "CDirEntry::SetModeEntry(): chmod() failed: set mode " +
                            CDirEntry::ModeToString(user, group, other, special) +
                            " for: " + GetPath());     
        return false;
    }
    return true;
}


void CDirEntry::SetDefaultModeGlobal(EType entry_type, TMode user_mode, 
                                     TMode group_mode, TMode other_mode,
                                     TSpecialModeBits special)
{
    if ( entry_type >= eUnknown ) {
        return;
    }
    if ( entry_type == eDir ) {
        if ( user_mode == fDefault ) {
            user_mode = fDefaultDirUser;
        }
        if ( group_mode == fDefault ) {
            group_mode = fDefaultDirGroup;
        }
        if ( other_mode == fDefault ) {
            other_mode = fDefaultDirOther;
        }
    } else {
        if ( user_mode == fDefault ) {
            user_mode = fDefaultUser;
        }
        if ( group_mode == fDefault ) {
            group_mode = fDefaultGroup;
        }
        if ( other_mode == fDefault ) {
            other_mode = fDefaultOther;
        }
    }
    if ( special == 0 ) {
        special = m_DefaultModeGlobal[entry_type][eSpecial];
    }
    m_DefaultModeGlobal[entry_type][eUser]    = user_mode;
    m_DefaultModeGlobal[entry_type][eGroup]   = group_mode;
    m_DefaultModeGlobal[entry_type][eOther]   = other_mode;
    m_DefaultModeGlobal[entry_type][eSpecial] = special;
}


void CDirEntry::SetDefaultMode(EType entry_type, TMode user_mode, 
                               TMode group_mode, TMode other_mode,
                               TSpecialModeBits special)
{
    if ( user_mode == fDefault ) {
        user_mode  = m_DefaultModeGlobal[entry_type][eUser];
    }
    if ( group_mode == fDefault ) {
        group_mode = m_DefaultModeGlobal[entry_type][eGroup];
    }
    if ( other_mode == fDefault ) {
        other_mode = m_DefaultModeGlobal[entry_type][eOther];
    }
    if ( special == 0 ) {
        special = m_DefaultModeGlobal[entry_type][eSpecial];
    }
    m_DefaultMode[eUser]    = user_mode;
    m_DefaultMode[eGroup]   = group_mode;
    m_DefaultMode[eOther]   = other_mode;
    m_DefaultMode[eSpecial] = special;
}


void CDirEntry::GetDefaultModeGlobal(EType  entry_type, TMode* user_mode,
                                     TMode* group_mode, TMode* other_mode,
                                     TSpecialModeBits* special)
{
    if ( user_mode ) {
        *user_mode  = m_DefaultModeGlobal[entry_type][eUser];
    }
    if ( group_mode ) {
        *group_mode = m_DefaultModeGlobal[entry_type][eGroup];
    }
    if ( other_mode ) {
        *other_mode = m_DefaultModeGlobal[entry_type][eOther];
    }
    if ( special ) {
        *special  = m_DefaultModeGlobal[entry_type][eSpecial];
    }
}


void CDirEntry::GetDefaultMode(TMode* user_mode, TMode* group_mode,
                               TMode* other_mode,
                               TSpecialModeBits* special) const
{
    if ( user_mode ) {
        *user_mode  = m_DefaultMode[eUser];
    }
    if ( group_mode ) {
        *group_mode = m_DefaultMode[eGroup];
    }
    if ( other_mode ) {
        *other_mode = m_DefaultMode[eOther];
    }
    if ( special ) {
        *special   = m_DefaultMode[eSpecial];
    }
}


mode_t CDirEntry::MakeModeT(TMode user_mode, TMode group_mode,
                            TMode other_mode, TSpecialModeBits special)
{
    mode_t mode = (
    // special bits
#ifdef S_ISUID
                   (special & fSetUID     ? S_ISUID  : 0) |
#endif
#ifdef S_ISGID
                   (special & fSetGID     ? S_ISGID  : 0) |
#endif
#ifdef S_ISVTX
                   (special & fSticky     ? S_ISVTX  : 0) |
#endif
    // modes
#if   defined(S_IRUSR)
                   (user_mode & fRead     ? S_IRUSR  : 0) |
#elif defined(S_IREAD) 
                   (user_mode & fRead     ? S_IREAD  : 0) |
#endif
#if   defined(S_IWUSR)
                   (user_mode & fWrite    ? S_IWUSR  : 0) |
#elif defined(S_IWRITE)
                   (user_mode & fWrite    ? S_IWRITE : 0) |
#endif
#if   defined(S_IXUSR)
                   (user_mode & fExecute  ? S_IXUSR  : 0) |
#elif defined(S_IEXEC)
                   (user_mode & fExecute  ? S_IEXEC  : 0) |
#endif
#ifdef S_IRGRP
                   (group_mode & fRead    ? S_IRGRP  : 0) |
#endif
#ifdef S_IWGRP
                   (group_mode & fWrite   ? S_IWGRP  : 0) |
#endif
#ifdef S_IXGRP
                   (group_mode & fExecute ? S_IXGRP  : 0) |
#endif
#ifdef S_IROTH
                   (other_mode & fRead    ? S_IROTH  : 0) |
#endif
#ifdef S_IWOTH
                   (other_mode & fWrite   ? S_IWOTH  : 0) |
#endif
#ifdef S_IXOTH
                   (other_mode & fExecute ? S_IXOTH  : 0) |
#endif
                   0);
    return mode;
}


void CDirEntry::ModeFromModeT(mode_t mode, 
                              TMode* user_mode, TMode* group_mode, 
                              TMode* other_mode, TSpecialModeBits* special)
{
    // Owner
    if (user_mode) {
        *user_mode = (
#if   defined(S_IRUSR)
                     (mode & S_IRUSR  ? fRead    : 0) |
#elif defined(S_IREAD)
                     (mode & S_IREAD  ? fRead    : 0) |
#endif
#if   defined(S_IWUSR)
                     (mode & S_IWUSR  ? fWrite   : 0) |
#elif defined(S_IWRITE)
                     (mode & S_IWRITE ? fWrite   : 0) |
#endif
#if   defined(S_IXUSR)
                     (mode & S_IXUSR  ? fExecute : 0) |
#elif defined(S_IEXEC)
                     (mode & S_IEXEC  ? fExecute : 0) |
#endif
                     0);
    }

#ifdef NCBI_OS_MSWIN
    if (group_mode) *group_mode = 0;
    if (other_mode) *other_mode = 0;
    if (special)    *special    = 0;

#else
    // Group
    if (group_mode) {
        *group_mode = (
#ifdef S_IRGRP
                     (mode & S_IRGRP  ? fRead    : 0) |
#endif
#ifdef S_IWGRP
                     (mode & S_IWGRP  ? fWrite   : 0) |
#endif
#ifdef S_IXGRP
                     (mode & S_IXGRP  ? fExecute : 0) |
#endif
                     0);
    }
    // Others
    if (other_mode) {
        *other_mode = (
#ifdef S_IROTH
                     (mode & S_IROTH  ? fRead    : 0) |
#endif
#ifdef S_IWOTH
                     (mode & S_IWOTH  ? fWrite   : 0) |
#endif
#ifdef S_IXOTH
                     (mode & S_IXOTH  ? fExecute : 0) |
#endif
                     0);
    }
    // Special bits
    if (special) {
        *special = (
#ifdef S_ISUID
                    (mode & S_ISUID   ? fSetUID  : 0) |
#endif
#ifdef S_ISGID
                    (mode & S_ISGID   ? fSetGID  : 0) |
#endif
#ifdef S_ISVTX
                    (mode & S_ISVTX   ? fSticky  : 0) |
#endif
                    0);
    }
#endif // NCBI_OS_MSWIN
}


// Convert permission mode to "rw[xsStT]" string.
string CDirEntry::x_ModeToSymbolicString(CDirEntry::EWho who, CDirEntry::TMode mode, bool special_bit, char filler)
{
    string out;
    out.reserve(3);

    char c;
    c = (mode & CDirEntry::fRead  ? 'r' : filler);
    if (c) {
        out += c;
    }
    c = (mode & CDirEntry::fWrite ? 'w' : filler);
    if (c) {
        out += c;
    }
    c = filler;
    if ( special_bit ) {
        if (who == CDirEntry::eOther) {
            c = (mode & CDirEntry::fExecute) ? 't' : 'T';
        } else {
            c = (mode & CDirEntry::fExecute) ? 's' : 'S';
        }
    } else if (mode & CDirEntry::fExecute) {
        c = 'x';
    }
    if (c) {
        out += c;
    }
    return out;
}


string CDirEntry::ModeToString(TMode user_mode, TMode group_mode, 
                               TMode other_mode, TSpecialModeBits special,
                               EModeStringFormat format)
{
    string out;
    switch (format) {
    case eModeFormat_Octal:
        {
            int i = 0;
            if (special > 0) {
                out = "0000";
                out[0] = char(special + '0');
                i++;
            } else {
                out = "000";
            }
            out[i++] = char(user_mode  + '0');
            out[i++] = char(group_mode + '0');
            out[i++] = char(other_mode + '0');
        }
        break;
    case eModeFormat_Symbolic:
        {
            out.reserve(17);
            out =   "u=" + x_ModeToSymbolicString(eUser,  user_mode,  (special & fSetUID) > 0, '\0');
            out += ",g=" + x_ModeToSymbolicString(eGroup, group_mode, (special & fSetGID) > 0, '\0');
            out += ",o=" + x_ModeToSymbolicString(eOther, other_mode, (special & fSticky) > 0, '\0');
        }
        break;
    case eModeFormat_List:
        {
            out.reserve(9);
            out =  x_ModeToSymbolicString(eUser,  user_mode,  (special & fSetUID) > 0, '-');
            out += x_ModeToSymbolicString(eGroup, group_mode, (special & fSetGID) > 0, '-');
            out += x_ModeToSymbolicString(eOther, other_mode, (special & fSticky) > 0, '-');
        }
        break;
    default:
        _TROUBLE;
    }

    return out;
}


bool CDirEntry::StringToMode(const CTempString& mode, 
                             TMode* user_mode, TMode* group_mode, 
                             TMode* other_mode, TSpecialModeBits* special)
{
    if ( mode.empty() ) {
        CNcbiError::Set(CNcbiError::eInvalidArgument);
        return false;
    }
    if ( isdigit((unsigned char)(mode[0])) ) {
        // eModeFormat_Octal
        unsigned int oct = NStr::StringToUInt(mode, NStr::fConvErr_NoThrow, 8);
        if ((oct > 07777) || (!oct  &&  errno != 0)) {
            CNcbiError::Set(CNcbiError::eInvalidArgument, mode);
            return false;
        }
        if (other_mode) {
            *other_mode = TMode(oct & 7);
        }
        oct >>= 3;
        if (group_mode) {
            *group_mode = TMode(oct & 7);
        }
        oct >>= 3;
        if (user_mode) {
            *user_mode = TMode(oct & 7);
        }
        if (special) {
            oct >>= 3;
            *special = TSpecialModeBits(oct);
        }

    } else {

        if (user_mode) 
            *user_mode = 0;
        if (group_mode)
            *group_mode = 0;
        if (other_mode)
            *other_mode = 0;
        if (special)
            *special = 0;

        // eModeFormat_List:
        if (mode.find('=') == NPOS  &&  mode.length() == 9) {
            for (int i = 0; i < 3; i++) {
                TMode m = 0;
                bool is_special = false;

                switch (mode[i*3]) {
                    case 'r':
                        m |= fRead;
                        break;
                    case '-':
                        break;
                    default:
                        CNcbiError::Set(CNcbiError::eInvalidArgument, mode);
                        return false;
                }
                switch (mode[i*3 + 1]) {
                    case 'w':
                        m |= fWrite;
                        break;
                    case '-':
                        break;
                    default:
                        CNcbiError::Set(CNcbiError::eInvalidArgument, mode);
                        return false;
                }
                switch (mode[i*3 + 2]) {
                    case 'S':
                    case 'T':
                        is_special = true;
                        break;
                    case 's':
                    case 't':
                        is_special = true;
                        // fall through
                    case 'x':
                        m |= fExecute;
                        break;
                    case '-':
                        break;
                    default:
                        CNcbiError::Set(CNcbiError::eInvalidArgument, mode);
                        return false;
                }
                switch (i) {
                    case 0: // user
                        if (user_mode) 
                            *user_mode = m;
                        if (is_special  &&  special)
                            *special |= fSetUID;
                        break;
                    case 1: // group
                        if (group_mode)
                            *group_mode = m;
                        if (is_special  &&  special)
                            *special |= fSetGID;
                        break;
                    case 2: // other
                        if (other_mode)
                            *other_mode = m;
                        if (is_special  &&  special)
                            *special |= fSticky;
                        break;
                }
            }

        // eModeFormat_Symbolic
        } else {
            std::list<string> parts;
            NStr::Split(mode, ",", parts, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
            if ( parts.empty() ) {
                CNcbiError::Set(CNcbiError::eInvalidArgument, mode);
                return false;
            }
            bool have_user  = false;
            bool have_group = false;
            bool have_other = false;

            ITERATE(std::list<string>, it, parts) {
                string accessor, perm;
                if ( !NStr::SplitInTwo(*it, "=", accessor, perm) ) {
                    CNcbiError::Set(CNcbiError::eInvalidArgument, mode);
                    return false;
                }
                TMode m = 0;
                bool is_special = false;
                // Permission mode(s) (rwx)
                ITERATE(string, s, perm) {
                    switch(char(*s)) {
                    case 'r':
                        m |= fRead;
                        break;
                    case 'w':
                        m |= fWrite;
                        break;
                    case 'S':
                    case 'T':
                        is_special = true;
                        break;
                    case 's':
                    case 't':
                        is_special = true;
                        // fall through
                    case 'x':
                        m |= fExecute;
                        break;
                    default:
                       CNcbiError::Set(CNcbiError::eInvalidArgument, mode);
                       return false;
                    }
                }
                // Permission group category (ugoa)
                ITERATE(string, s, accessor) {
                    switch(char(*s)) {
                    case 'u':
                        if (have_user) {
                            CNcbiError::Set(CNcbiError::eInvalidArgument, mode);
                            return false;
                        }
                        if (user_mode)
                            *user_mode = m;
                        if (is_special  &&  special)
                            *special |= fSetUID;
                        have_user = true;
                        break;
                    case 'g':
                        if (have_group) {
                            CNcbiError::Set(CNcbiError::eInvalidArgument, mode);
                            return false;
                        }
                        if (group_mode)
                            *group_mode = m;
                        if (is_special  &&  special)
                            *special |= fSetGID;
                        have_group = true;
                        break;
                    case 'o':
                        if (have_other) {
                            CNcbiError::Set(CNcbiError::eInvalidArgument, mode);
                            return false;
                        }
                        if (other_mode)
                            *other_mode = m;
                        if (is_special  &&  special)
                            *special |= fSticky;
                        have_other = true;
                        break;
                    case 'a':
                        if (is_special || have_user || have_group || have_other) {
                            CNcbiError::Set(CNcbiError::eInvalidArgument, mode);
                            return false;
                        }
                        have_user  = true;
                        have_group = true;
                        have_other = true;
                        if (user_mode)
                            *user_mode = m;
                        if (group_mode)
                            *group_mode = m;
                        if (other_mode)
                            *other_mode = m;
                        break;
                    default:
                        CNcbiError::Set(CNcbiError::eInvalidArgument, mode);
                        return false;
                    }
                }
            }
        }

    }
    return true;
}


void CDirEntry::GetUmask(TMode* user_mode, TMode* group_mode, 
                         TMode* other_mode, TSpecialModeBits* special)
{
#ifdef HAVE_GETUMASK
    mode_t mode = getumask();
#else
    mode_t mode = NcbiSys_umask(0);
    NcbiSys_umask(mode);
#endif //HAVE_GETUMASK
    ModeFromModeT(mode, user_mode, group_mode, other_mode, special);
}


void CDirEntry::SetUmask(TMode user_mode, TMode group_mode, 
                         TMode other_mode, TSpecialModeBits special)
{
    mode_t mode = MakeModeT((user_mode  == fDefault) ? 0 : user_mode,
                            (group_mode == fDefault) ? 0 : group_mode,
                            (other_mode == fDefault) ? 0 : other_mode,
                            special);
    NcbiSys_umask(mode);
}


#if defined(NCBI_OS_UNIX) && !defined(HAVE_EUIDACCESS) && !defined(EFF_ONLY_OK)

static bool s_CheckAccessStat(struct stat* p, int mode)
{
    const struct stat& st = *p;
    uid_t uid = geteuid();

    // Check user permissions
    if (uid == st.st_uid) {
        return (!(mode & R_OK)  ||  (st.st_mode & S_IRUSR))  &&
               (!(mode & W_OK)  ||  (st.st_mode & S_IWUSR))  &&
               (!(mode & X_OK)  ||  (st.st_mode & S_IXUSR));
    }

    // Initialize list of group IDs for effective user
    int ngroups = 0;
    gid_t gids[NGROUPS_MAX + 1];
    gids[0] = getegid();
    ngroups = getgroups((int)(sizeof(gids)/sizeof(gids[0])) - 1, gids + 1);
    if (ngroups < 0) {
        ngroups = 1;
    } else {
        ngroups++;
    }
    for (int i = 1;  i < ngroups;  i++) {
        if (gids[i] == uid) {
            if (i < --ngroups) {
                memmove(&gids[i], &gids[i + 1], sizeof(gids[0])*(ngroups-i));
            }
            break;
        }
    }
    // Check group permissions
    for (int i = 0;  i < ngroups;  i++) {
        if (gids[i] == st.st_gid) {
            return  (!(mode & R_OK)  ||  (st.st_mode & S_IRGRP))  &&
                    (!(mode & W_OK)  ||  (st.st_mode & S_IWGRP))  &&
                    (!(mode & X_OK)  ||  (st.st_mode & S_IXGRP));
        }
    }
    // Check other permissions
    if ( (!(mode & R_OK)  ||  (st.st_mode & S_IROTH))  &&
         (!(mode & W_OK)  ||  (st.st_mode & S_IWOTH))  &&
         (!(mode & X_OK)  ||  (st.st_mode & S_IXOTH)) ) {
        return true;
    }

    // Permissions not granted
    return false;
}


static bool s_CheckAccessPath(const char* path, int mode)
{
    if (!path) {
        errno = 0;
        CNcbiError::Set(CNcbiError::eInvalidArgument);
        return false;
    }
    if (!*path) {
        CNcbiError::SetErrno(errno = ENOENT, path);
        return false;
    }
    struct stat st;
    if (stat(path, &st) != 0) {
        CNcbiError::SetFromErrno(path);
        return false;
    }
    if (!s_CheckAccessStat(&st, mode)) {
        CNcbiError::SetErrno(errno = EACCES, path);
        return false;
    }
    // Permissions granted
    return true;
}

#endif  // defined(NCBI_OS_UNIX)


bool CDirEntry::CheckAccess(TMode access_mode) const
{
#if defined(NCBI_OS_MSWIN)
    // Try to get effective access rights on this file object for
    // the current process owner.
    ACCESS_MASK mask = 0;
    if ( CWinSecurity::GetFilePermissions(GetPath(), &mask) ) {
        TMode perm = ( (mask & FILE_READ_DATA  ? fRead    : 0) |
                       (mask & FILE_WRITE_DATA ? fWrite   : 0) |
                       (mask & FILE_EXECUTE    ? fExecute : 0) );
        return (access_mode & perm) > 0;
     }
     return false;

#elif defined(NCBI_OS_UNIX)
    const char* path = GetPath().c_str();
    int         mode = F_OK;

    if ( access_mode & fRead)    mode |= R_OK;
    if ( access_mode & fWrite)   mode |= W_OK;
    if ( access_mode & fExecute) mode |= X_OK;
    
    // Use euidaccess() where possible
#  if defined(HAVE_EUIDACCESS)
    if (euidaccess(path, mode) != 0) {
        CNcbiError::SetFromErrno(path);
        return false;
    }
    return true;

#  elif defined(EFF_ONLY_OK)
    // Some Unix have special flag for access() to use effective user ID.
    mode |= EFF_ONLY_OK;
    if (access(path, mode) != 0) {
        CNcbiError::SetFromErrno(path);
        return false;
    }
    return true;

#  else
    // We can use access() only if effective and real user/group IDs are equal.
    // access() operate with real IDs only, but we should check access
    // for effective IDs.
    if (getuid() == geteuid()  &&  getgid() == getegid()) {
        if (access(path, mode) != 0) {
            CNcbiError::SetFromErrno(path);
            return false;
        }
        return true;
    }
    // Otherwise, try to check permissions itself.
    // Note, that this function is not perfect, it doesn't work with ACL,
    // which implementation can differ for each platform.
    // But in most cases it works.
    return s_CheckAccessPath(path, mode);

#  endif
#endif // NCBI_OS
}


#ifdef NCBI_OS_MSWIN

bool s_FileTimeToCTime(const FILETIME& filetime, CTime& t) 
{
    // Clear time object
    t.Clear();

    if ( !filetime.dwLowDateTime  &&  !filetime.dwHighDateTime ) {
        // File time is undefined, just return "empty" time
        return true;
    }
    SYSTEMTIME system;
    FILETIME   local;

    // Convert the file time to local time
    if ( !::FileTimeToLocalFileTime(&filetime, &local) ) {
        CNcbiError::SetFromWindowsError();
        return false;
    }
    // Convert the local file time from UTC to system time.
    if ( !::FileTimeToSystemTime(&local, &system) ) {
        CNcbiError::SetFromWindowsError();
        return false;
    }

    // Construct new time
    CTime newtime(system.wYear,
                  system.wMonth,
                  system.wDay,
                  system.wHour,
                  system.wMinute,
                  system.wSecond,
                  system.wMilliseconds * (kNanoSecondsPerSecond / kMilliSecondsPerSecond),
                  CTime::eLocal,
                  t.GetTimeZonePrecision());

    // And assign it
    if ( t.GetTimeZone() == CTime::eLocal ) {
        t = newtime;
    } else {
        t = newtime.GetGmtTime();
    }
    return true;
}


void s_UnixTimeToFileTime(time_t t, long nanosec, FILETIME& filetime) 
{
    // Note that LONGLONG is a 64-bit value
    LONGLONG res;
    // This algorithm was found in MSDN
    res = Int32x32To64(t, 10000000) + 116444736000000000 + nanosec/100;
    filetime.dwLowDateTime  = (DWORD)res;
    filetime.dwHighDateTime = (DWORD)(res >> 32);
}

#endif // NCBI_OS_MSWIN


bool CDirEntry::GetTime(CTime* modification,
                        CTime* last_access,
                        CTime* creation) const
{
#ifdef NCBI_OS_MSWIN
    HANDLE handle;
    WIN32_FIND_DATA buf;

    // Get file times using FindFile
    handle = ::FindFirstFile(_T_XCSTRING(GetPath()), &buf);
    if ( handle == INVALID_HANDLE_VALUE ) {
        LOG_ERROR_WIN(8, "CDirEntry::GetTime(): Cannot find: " + GetPath());
        return false;
    }
    ::FindClose(handle);

    // Convert file UTC times into CTime format
    if ( modification  &&
        !s_FileTimeToCTime(buf.ftLastWriteTime, *modification) ) {
        LOG_ERROR(9, "CDirEntry::GetTime(): Cannot get modification time for: " + GetPath());
        return false;
    }
    if ( last_access  &&
         !s_FileTimeToCTime(buf.ftLastAccessTime, *last_access) ) {
        LOG_ERROR(9, "CDirEntry::GetTime(): Cannot get access time for: " + GetPath());
        return false;
    }
    if ( creation  &&
        !s_FileTimeToCTime(buf.ftCreationTime, *creation) ) {
        LOG_ERROR(9, "CDirEntry::GetTime(): Cannot get creation time for: " + GetPath());
        return false;
    }
    return true;

#else // NCBI_OS_UNIX

    struct SStat st;
    if ( !Stat(&st) ) {
        LOG_ERROR(8, "CDirEntry::GetTime(): Cannot get time for: " + GetPath());
        return false;
    }
    if ( modification ) {
        modification->SetTimeT(st.orig.st_mtime);
        if ( st.mtime_nsec )
            modification->SetNanoSecond(st.mtime_nsec);
    }
    if ( last_access ) {
        last_access->SetTimeT(st.orig.st_atime);
        if ( st.atime_nsec )
            last_access->SetNanoSecond(st.atime_nsec);
    }
    if ( creation ) {
        creation->SetTimeT(st.orig.st_ctime);
        if ( st.ctime_nsec )
            creation->SetNanoSecond(st.ctime_nsec);
    }
    return true;
#endif
}


bool CDirEntry::SetTime(const CTime* modification,
                        const CTime* last_access,
                        const CTime* creation) const
{
#ifdef NCBI_OS_MSWIN
    if ( !modification  &&  !last_access  &&  !creation ) {
        return true;
    }

    FILETIME   x_modification,        x_last_access,        x_creation;
    LPFILETIME p_modification = NULL, p_last_access = NULL, p_creation = NULL;

    // Convert times to FILETIME format
    if ( modification ) {
        s_UnixTimeToFileTime(modification->GetTimeT(), modification->NanoSecond(), x_modification);
        p_modification = &x_modification;
    }
    if ( last_access ) {
        s_UnixTimeToFileTime(last_access->GetTimeT(), last_access->NanoSecond(), x_last_access);
        p_last_access = &x_last_access;
    }
    if ( creation ) {
        s_UnixTimeToFileTime(creation->GetTimeT(), creation->NanoSecond(), x_creation);
        p_creation = &x_creation;
    }

    // Change times
    HANDLE h = ::CreateFile(_T_XCSTRING(GetPath()), FILE_WRITE_ATTRIBUTES,
                            FILE_SHARE_READ, NULL, OPEN_EXISTING,
                            FILE_FLAG_BACKUP_SEMANTICS /*for dirs*/, NULL); 
    if ( h == INVALID_HANDLE_VALUE ) {
        LOG_ERROR_WIN(10, "CDirEntry::SetTime(): Cannot open: " + GetPath());
        return false;
    }
    if ( !::SetFileTime(h, p_creation, p_last_access, p_modification) ) {
        LOG_ERROR_WIN(11, "CDirEntry::SetTime(): Cannot set new time for: " + GetPath());
        ::CloseHandle(h);
        return false;
    }
    ::CloseHandle(h);

    return true;

#else // NCBI_OS_UNIX

    // Creation time doesn't used on Unix
    creation = NULL;  /* DUMMY, to avoid warnings */

    if ( !modification  &&  !last_access  /*&&  !creation*/ ) {
        return true;
    }

#  ifdef HAVE_UTIMES
    // Get current times
    CTime x_modification, x_last_access;

    if ( !modification  ||  !last_access ) {
        if ( !GetTime(modification ? NULL : &x_modification,
                      last_access  ? NULL : &x_last_access,
                      NULL /* creation */) ) {
            return false;
        }
        if (!modification) {
            modification = &x_modification;
        } else {
            last_access = &x_last_access;
        }
    }

    // Change times
    struct timeval tvp[2];
    tvp[0].tv_sec  = last_access->GetTimeT();
    tvp[0].tv_usec = TV_USEC(last_access->NanoSecond() / (kNanoSecondsPerSecond / kMicroSecondsPerSecond));
    tvp[1].tv_sec  = modification->GetTimeT();
    tvp[1].tv_usec = TV_USEC(modification->NanoSecond() / (kNanoSecondsPerSecond / kMicroSecondsPerSecond));

#    ifdef HAVE_LUTIMES
    bool ut_res = lutimes(GetPath().c_str(), tvp) == 0;
#    else
    bool ut_res = utimes(GetPath().c_str(), tvp) == 0;
#    endif
    if ( !ut_res ) {
        LOG_ERROR_ERRNO(12, "CDirEntry::SetTime(): Cannot change time for: " + GetPath());
        return false;
    }
    return true;

# else
    // utimes() does not exist on current platform,
    // so use less accurate utime().

    // Get current times
    time_t x_modification, x_last_access;

    if ((!modification  ||  !last_access)
        &&  !GetTimeT(&x_modification, &x_last_access, NULL /* creation */)) {
        LOG_ERROR(12, "CDirEntry::SetTime(): Cannot get current time for: " + GetPath());
        return false;
    }

    // Change times to new
    struct utimbuf times;
    times.modtime  = modification ? modification->GetTimeT() : x_modification;
    times.actime   = last_access  ? last_access->GetTimeT()  : x_last_access;
    if ( utime(GetPath().c_str(), &times) != 0 ) {
        LOG_ERROR_ERRNO(12, "CDirEntry::SetTime(): Cannot change time for: " + GetPath());
        return false;
    }
    return true;

#  endif // HAVE_UTIMES

#endif
}


bool CDirEntry::GetTimeT(time_t* modification,
                         time_t* last_access,
                         time_t* creation) const
{
    TNcbiSys_stat st;
    if (NcbiSys_stat(_T_XCSTRING(GetPath()), &st) != 0) {
        LOG_ERROR_ERRNO(13, "CDirEntry::GetTimeT(): stat() failed for: " + GetPath());
        return false;
    }
    if ( modification ) {
        *modification = st.st_mtime;
    }
    if ( last_access ) {
        *last_access = st.st_atime;
    }
    if ( creation ) {
        *creation = st.st_ctime;
    }
    return true;
}


bool CDirEntry::SetTimeT(const time_t* modification,
                         const time_t* last_access,
                         const time_t* creation) const
{
#ifdef NCBI_OS_MSWIN
    if ( !modification  &&  !last_access  &&  !creation ) {
        return true;
    }

    FILETIME   x_modification,        x_last_access,        x_creation;
    LPFILETIME p_modification = NULL, p_last_access = NULL, p_creation = NULL;

    // Convert times to FILETIME format
    if ( modification ) {
        s_UnixTimeToFileTime(*modification, 0, x_modification);
        p_modification = &x_modification;
    }
    if ( last_access ) {
        s_UnixTimeToFileTime(*last_access, 0, x_last_access);
        p_last_access = &x_last_access;
    }
    if ( creation ) {
        s_UnixTimeToFileTime(*creation, 0, x_creation);
        p_creation = &x_creation;
    }

    // Change times
    HANDLE h = ::CreateFile(_T_XCSTRING(GetPath()), FILE_WRITE_ATTRIBUTES,
                            FILE_SHARE_READ, NULL, OPEN_EXISTING,
                            FILE_FLAG_BACKUP_SEMANTICS /*for dirs*/, NULL); 
    if ( h == INVALID_HANDLE_VALUE ) {
        LOG_ERROR_WIN(14, "CDirEntry::SetTimeT(): Cannot open: " + GetPath());
        return false;
    }
    if ( !::SetFileTime(h, p_creation, p_last_access, p_modification) ) {
        LOG_ERROR_WIN(15, "CDirEntry::SetTimeT(): Cannot change time for: " + GetPath());
        ::CloseHandle(h);
        return false;
    }
    ::CloseHandle(h);

    return true;

#else // NCBI_OS_UNIX

    // Creation time doesn't used on Unix
    creation = NULL;  /* DUMMY, to avoid warnings */

    if ( !modification  &&  !last_access  /*&&  !creation*/ )
        return true;

    time_t x_modification, x_last_access;
    if ((!modification  ||  !last_access)
        &&  !GetTimeT(&x_modification, &x_last_access, NULL /* creation */) ) {
          LOG_ERROR(15, "CDirEntry::SetTimeT(): Cannot get current time for: " + GetPath());
          return false;
    }

    // Change times to new
    struct utimbuf times;
    times.modtime = modification ? *modification : x_modification;
    times.actime  = last_access  ? *last_access  : x_last_access;
    if ( utime(GetPath().c_str(), &times) != 0 ) {
        LOG_ERROR_ERRNO(15, "CDirEntry::SetTimeT(): Cannot change time for: " + GetPath());
        return false;
    }
    return true;

#endif
}


bool CDirEntry::Stat(struct SStat *buffer, EFollowLinks follow_links) const
{
    if ( !buffer ) {
        errno = EFAULT;
        LOG_ERROR_ERRNO(16, "CDirEntry::Stat(): NULL stat buffer passed for: " + GetPath());
        return false;
    }
    int errcode;
#ifdef NCBI_OS_MSWIN
    errcode = NcbiSys_stat(_T_XCSTRING(GetPath()), &buffer->orig);
#else // NCBI_OS_UNIX
    if (follow_links == eFollowLinks) {
        errcode = stat(GetPath().c_str(), &buffer->orig);
    } else {
        errcode = lstat(GetPath().c_str(), &buffer->orig);
    }
#endif
    if (errcode != 0) {
        LOG_ERROR_ERRNO(16, "CDirEntry::Stat(): stat() failed for: " + GetPath());
        return false;
    }
   
    // Assign additional fields
    buffer->atime_nsec = 0;
    buffer->mtime_nsec = 0;
    buffer->ctime_nsec = 0;
    
#ifdef NCBI_OS_UNIX
    // UNIX:
    // Some systems have additional fields in the stat structure to store
    // nanoseconds. If you know one more platform which have nanoseconds
    // support for file times, add it here.

#  if !defined(__GLIBC_PREREQ)
#    define __GLIBC_PREREQ(x, y) 0
#  endif

#  if defined(NCBI_OS_LINUX)  &&  __GLIBC_PREREQ(2,3)
#    if defined(__USE_MISC)
    buffer->atime_nsec = buffer->orig.st_atim.tv_nsec;
    buffer->mtime_nsec = buffer->orig.st_mtim.tv_nsec;
    buffer->ctime_nsec = buffer->orig.st_ctim.tv_nsec;
#    else
    buffer->atime_nsec = buffer->orig.st_atimensec;
    buffer->mtime_nsec = buffer->orig.st_mtimensec;
    buffer->ctime_nsec = buffer->orig.st_ctimensec;
#    endif
#  endif

#  if defined(NCBI_OS_SOLARIS)
#    if !defined(_XOPEN_SOURCE) && !defined(_POSIX_C_SOURCE) || \
     defined(__EXTENSIONS__)
    buffer->atime_nsec = buffer->orig.st_atim.tv_nsec;
    buffer->mtime_nsec = buffer->orig.st_mtim.tv_nsec;
    buffer->ctime_nsec = buffer->orig.st_ctim.tv_nsec;
#    else
    buffer->atime_nsec = buffer->orig.st_atim.__tv_nsec;
    buffer->mtime_nsec = buffer->orig.st_mtim.__tv_nsec;
    buffer->ctime_nsec = buffer->orig.st_ctim.__tv_nsec;
#    endif
#  endif
   
#  if defined(NCBI_OS_BSD) || defined(NCBI_OS_DARWIN)
#    if defined(_POSIX_SOURCE)
    buffer->atime_nsec = buffer->orig.st_atimensec;
    buffer->mtime_nsec = buffer->orig.st_mtimensec;
    buffer->ctime_nsec = buffer->orig.st_ctimensec;
#    else
    buffer->atime_nsec = buffer->orig.st_atimespec.tv_nsec;
    buffer->mtime_nsec = buffer->orig.st_mtimespec.tv_nsec;
    buffer->ctime_nsec = buffer->orig.st_ctimespec.tv_nsec;
#    endif
#  endif

#  if defined(NCBI_OS_IRIX)
#    if defined(tv_sec)
    buffer->atime_nsec = buffer->orig.st_atim.__tv_nsec;
    buffer->mtime_nsec = buffer->orig.st_mtim.__tv_nsec;
    buffer->ctime_nsec = buffer->orig.st_ctim.__tv_nsec;
#    else
    buffer->atime_nsec = buffer->orig.st_atim.tv_nsec;
    buffer->mtime_nsec = buffer->orig.st_mtim.tv_nsec;
    buffer->ctime_nsec = buffer->orig.st_ctim.tv_nsec;
#    endif
#  endif
    
#endif  // NCBI_OS_UNIX

    return true;
}


CDirEntry::EType CDirEntry::GetType(EFollowLinks follow) const
{
    TNcbiSys_stat st;
    int errcode;

#if defined(NCBI_OS_MSWIN)
    errcode = NcbiSys_stat(_T_XCSTRING(GetPath()), &st);
    if (errcode != 0) {
        // Make additional checks for UNC paths, because 
        // stat() cannot handle path that looks like \\Server\Share.
        if (s_Win_IsNetworkPath(GetPath())) {
            DWORD attr = ::GetFileAttributes(_T_XCSTRING(GetPath()));
            if (attr == INVALID_FILE_ATTRIBUTES) {
                // Don't report an error here, just set CNcbiError.
                // eUnknown is a legit return value.
                CNcbiError::SetFromWindowsError(GetPath());
                return eUnknown;
            }
            if ( F_ISSET(attr, FILE_ATTRIBUTE_DIRECTORY) ) {
                return eDir;
            }
            return eFile;
        }
    }
#else // NCBI_OS_UNIX
    if (follow == eFollowLinks) {
        errcode = stat(GetPath().c_str(), &st);
    } else {
        errcode = lstat(GetPath().c_str(), &st);
    }
#endif
    if (errcode != 0) {
        // Don't report an error here, just set CNcbiError.
        // eUnknown is a legit return value.
        CNcbiError::SetFromErrno(GetPath());
        return eUnknown;
    }
    return GetType(st);
}


/// Test macro for file types
#define NCBI_IS_TYPE(mode, mask)  (((mode) & S_IFMT) == (mask))

CDirEntry::EType CDirEntry::GetType(const TNcbiSys_stat& st)
{
    unsigned int mode = (unsigned int)st.st_mode;

#ifdef S_ISDIR
    if (S_ISDIR(mode))
#else
    if (NCBI_IS_TYPE(mode, S_IFDIR))
#endif
        return eDir;

#ifdef S_ISCHR
    if (S_ISCHR(mode))
#else
    if (NCBI_IS_TYPE(mode, S_IFCHR))
#endif
        return eCharSpecial;

#ifdef NCBI_OS_MSWIN
    if (NCBI_IS_TYPE(mode, _S_IFIFO))
        return ePipe;
#else
    // NCBI_OS_UNIX
#  ifdef S_ISFIFO
    if (S_ISFIFO(mode))
#  else
    if (NCBI_IS_TYPE(mode, S_IFIFO))
#  endif
        return ePipe;

#  ifdef S_ISLNK
    if (S_ISLNK(mode))
#  else
    if (NCBI_IS_TYPE(mode, S_IFLNK))
#  endif
        return eLink;

#  ifdef S_ISSOCK
    if (S_ISSOCK(mode))
#  else
    if (NCBI_IS_TYPE(mode, S_IFSOCK))
#  endif
        return eSocket;

#  ifdef S_ISBLK
    if (S_ISBLK(mode))
#  else
    if (NCBI_IS_TYPE(mode, S_IFBLK))
#  endif
        return eBlockSpecial;

#  ifdef S_IFDOOR 
    // only Solaris seems to have this one
#    ifdef S_ISDOOR
    if (S_ISDOOR(mode))
#    else
    if (NCBI_IS_TYPE(mode, S_IFDOOR))
#    endif
        return eDoor;
#  endif

#endif //NCBI_OS_MSWIN

    // Check on regular file last
#ifdef S_ISREG
    if (S_ISREG(mode))
#else
    if (NCBI_IS_TYPE(mode, S_IFREG))
#endif
        return eFile;

    return eUnknown;
}


#if defined(NCBI_OS_MSWIN)

// Windows-specific implementation. See default implementation in the .hpp file
bool CDirEntry::Exists(void) const
{
    HANDLE h = ::CreateFile(_T_XCSTRING(GetPath()),
                            GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                            OPEN_EXISTING,
                            FILE_FLAG_BACKUP_SEMANTICS /*for dirs*/, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        // Don't report an error here, just set CNcbiError.
        CNcbiError::SetFromWindowsError(GetPath());
        return false;
    }
    ::CloseHandle(h);
    return true;
}

#endif


string CDirEntry::LookupLink(void) const
{
#ifdef NCBI_OS_MSWIN
    return kEmptyStr;

#else  // NCBI_OS_UNIX
    char buf[PATH_MAX];
    string name;
    int length = (int)readlink(_T_XCSTRING(GetPath()), buf, sizeof(buf));
    if (length > 0) {
        name.assign(buf, length);
    }
    return name;
#endif
}


void CDirEntry::DereferenceLink(ENormalizePath normalize)
{
#ifdef NCBI_OS_MSWIN
    // Not implemented
    return;
#endif
    string prev;
    while ( IsLink() ) {
        string name = LookupLink();
        if ( name.empty() ||  name == prev ) {
            return;
        }
        prev = name;
        if ( IsAbsolutePath(name) ) {
            Reset(name);
        } else {
            string path = MakePath(GetDir(), name);
            if (normalize == eNormalizePath) {
                Reset(NormalizePath(path));
            } else {
                Reset(path);
            }
        }
    }
}


void s_DereferencePath(CDirEntry& entry)
{
#ifdef NCBI_OS_MSWIN
    // Not implemented
    return;
#endif
    // Dereference each path components starting from last one
    entry.DereferenceLink(eNotNormalizePath);

    // Get dir and file names
    string path = entry.GetPath();
    size_t pos = path.find_last_of(ALL_SEPARATORS);
    if (pos == NPOS) {
        return; 
    }
    string filename = path.substr(pos+1);
    string dirname  = path.substr(0, pos);
    if ( dirname.empty() ) {
        return;
    }
    // Dereference path one level up
    entry.Reset(dirname);
    s_DereferencePath(entry);
    // Create new path
    entry.Reset(CDirEntry::MakePath(entry.GetPath(), filename));
}


void CDirEntry::DereferencePath(void)
{
#ifdef NCBI_OS_MSWIN
    // Not implemented
    return;
#endif
    // Use s_DereferencePath() recursively and normalize result only once
    CDirEntry e(GetPath());
    s_DereferencePath(e);
    Reset(NormalizePath(e.GetPath()));
}


bool CDirEntry::Copy(const string& path, TCopyFlags flags, size_t buf_size)
    const
{
    // Dereference link if specified
    bool follow = F_ISSET(flags, fCF_FollowLinks);
    EType type = GetType(follow ? eFollowLinks : eIgnoreLinks);
    switch (type) {
        case eFile:
            return CFile(GetPath()).Copy(path, flags, buf_size);
        case eDir: 
            return CDir(GetPath()).Copy(path, flags, buf_size);
        case eLink:
            return CSymLink(GetPath()).Copy(path, flags, buf_size);
        case eUnknown:
            {
                CNcbiError::Set(CNcbiError::eNoSuchFileOrDirectory, GetPath());
                return false;
            }
        default:
            CNcbiError::Set(CNcbiError::eNotSupported, GetPath());
            break;
    }
    // We "don't know" how to copy entry of other type, by default.
    // Use overloaded Copy() method in derived classes.
    return (flags & fCF_SkipUnsupported) == fCF_SkipUnsupported;
}


bool CDirEntry::Rename(const string& newname, TRenameFlags flags)
{
    CDirEntry src(*this);
    CDirEntry dst(newname);

    // Dereference links
    if ( F_ISSET(flags, fRF_FollowLinks) ) {
        src.DereferenceLink();
        dst.DereferenceLink();
    }
    // The source entry must exists
    EType src_type = src.GetType();
    if ( src_type == eUnknown ) {
        LOG_ERROR_NCBI(17, "CDirEntry::Rename():"
                           " Source path does not exist: " + src.GetPath(),
                           CNcbiError::eNoSuchFileOrDirectory);
        return false;
    }

    // Try to "move" in one atomic operation if possible to avoid race
    // conditions between check on destination existence and renaming.

#ifdef NCBI_OS_MSWIN
    // On Windows we can try to move file or whole directory, even across volumes
    if ( ::MoveFileEx(_T_XCSTRING(src.GetPath()),
                      _T_XCSTRING(dst.GetPath()), MOVEFILE_COPY_ALLOWED) ) {
        Reset(newname);
        return true;
    }
#else
    // On Unix we can use "link" technique for files.
    
    // link() have different behavior on some flavors of Unix
    // regarding symbolic links handling, and can automatically
    // dereference both source and destination as POSIX required, 
    // or not (Linux with kernel 2.0 and up behavior).
    // We need to rename symlink itself, if not dereferenced yet
    // (see fRF_FollowLinks), and the destination should remain
    // a symlink. So just dont use link() in this case and,
    // fall back to regular rename() instead.
    
    if ( src_type == eFile  && 
         link(_T_XCSTRING(src.GetPath()),
              _T_XCSTRING(dst.GetPath())) == 0 ) {
        // Hard link successfully created, so we can just remove source file
        if ( src.RemoveEntry() ) {
            Reset(newname);
            return true;
        }
    }
#endif
    // On error, symlink, or destination existence --
    // continue on regular processing below

    EType dst_type = dst.GetType();

    // If destination exists...

    if ( dst_type != eUnknown ) {
        // Can rename entries with different types?
        if ( F_ISSET(flags, fRF_EqualTypes)  &&  (src_type != dst_type) ) {
            LOG_ERROR_NCBI(18, "CDirEntry::Rename():"
                               " Both source and destination exist and have different types: "
                               + src.GetPath() + " and " + dst.GetPath(),
                               CNcbiError::eOperationNotPermitted);
            return false;
        }
        // Can overwrite entry?
        if ( !F_ISSET(flags, fRF_Overwrite) ) {
            LOG_ERROR_NCBI(19, "CDirEntry::Rename(): Destination path already exists: "
                               + dst.GetPath(), 
                               CNcbiError::eOperationNotPermitted);
            return false;
        }
        // Rename only if destination is older, otherwise just remove source
        if ( F_ISSET(flags, fRF_Update)  &&  !src.IsNewer(dst.GetPath(), 0)) {
            return src.Remove();
        }
        // Backup destination entry first
        if ( F_ISSET(flags, fRF_Backup) ) {
            // Use new CDirEntry object instead of 'dst', because its path
            // will be changed after backup
            CDirEntry dst_tmp(dst);
            if ( !dst_tmp.Backup(GetBackupSuffix(), eBackup_Rename) ) {
                LOG_ERROR(20, "CDirEntry::Rename(): Cannot backup: " + dst.GetPath());
                return false;
            }
        }
        // Overwrite destination entry
        if ( F_ISSET(flags, fRF_Overwrite) ) {
            if ( dst.Exists() ) {
                dst.Remove();
            }
        }
    }

    // On some platform rename() fails if destination entry exists, 
    // on others it can overwrite destination.
    // For consistency return FALSE if destination already exists.
    if ( dst.Exists() ) {
        // this means Remove() has failed, and error is set already
        LOG_ERROR(21, "CDirEntry::Rename(): Destination path exists: " + GetPath());
        return false;
    }
    
    // Rename
    
    if ( NcbiSys_rename(_T_XCSTRING(src.GetPath()),
                        _T_XCSTRING(dst.GetPath())) != 0 ) {
#ifdef NCBI_OS_MSWIN
        if ( errno != EACCES ) {
#else
        if ( errno != EXDEV ) {
#endif
            LOG_ERROR_ERRNO(21, "CDirEntry::Rename(): rename() failed for " + GetPath());
            return false;
        }
        // Note that rename() fails in the case of cross-device renaming.
        // So, try to make a copy and remove the original later.
        unique_ptr<CDirEntry> e(CDirEntry::CreateObject(src_type, src.GetPath()));
        if ( !e->Copy(dst.GetPath(), fCF_Recursive | fCF_PreserveAll ) ) {
            LOG_ERROR(102, "CDirEntry::Rename(): Renaming via Copy() failed for " + GetPath());
            unique_ptr<CDirEntry> tmp(CDirEntry::CreateObject(src_type, dst.GetPath()));
            tmp->Remove(eRecursive);
            return false;
        }
        // Remove 'src'
        if ( !e->Remove(eRecursive) ) {
            LOG_ERROR(102, "CDirEntry::Rename(): Renaming via Copy() failed for " + GetPath());
            // Do not delete 'dst' here because in case of directories the
            // source may be already partially removed, so we can lost data.
            return false;
        }
    }
    Reset(newname);
    return true;
}


bool CDirEntry::Remove(TRemoveFlags flags) const
{
    // Is this a directory ? (and processing not entry only)
    if ( (flags & (fDir_All | fDir_Recursive)) != eEntryOnly  &&  IsDir(eIgnoreLinks) ) {
        return CDir(GetPath()).Remove(flags);
    }
    // Other entries
    return RemoveEntry(flags);
}


bool CDirEntry::RemoveEntry(TRemoveFlags flags) const
{

    if ( NcbiSys_remove(_T_XCSTRING(GetPath())) != 0 ) {
        switch (errno) {
        case ENOENT:
            if ( flags & fIgnoreMissing )
                return true;
            break;

#if defined(NCBI_OS_MSWIN)
        case EACCES:
            if ( NCBI_PARAM_TYPE(NCBI, DeleteReadOnlyFiles)::GetDefault() ) {
                SetModeEntry(fDefault);
                if ( NcbiSys_remove(_T_XCSTRING(GetPath())) == 0 ) {
                    return true;
                }
            }
            errno = EACCES;
#endif
        }
        LOG_ERROR_ERRNO(22, "CDirEntry::RemoveEntry(): remove() failed for: " + GetPath());
        return false;
    }
    return true;
}


bool CDirEntry::Backup(const string& suffix, EBackupMode mode,
                       TCopyFlags copyflags, size_t copybufsize)
{
    string backup_name = DeleteTrailingPathSeparator(GetPath()) +
                         (suffix.empty() ? string(GetBackupSuffix()) : suffix);
    switch (mode) {
        case eBackup_Copy:
            {
                TCopyFlags flags = copyflags;
                flags &= ~(fCF_Update | fCF_Backup);
                flags |=  (fCF_Overwrite | fCF_TopDirOnly);
                return Copy(backup_name, flags, copybufsize);
            }
        case eBackup_Rename:
            return Rename(backup_name, fRF_Overwrite);
        default:
            _TROUBLE;
    }
    // Unreachable
    return false;
}


bool CDirEntry::IsNewer(const string& entry_name, TIfAbsent2 if_absent) const
{
    CDirEntry entry(entry_name);
    CTime this_time;
    CTime entry_time;
    int v = 0;

    if ( !GetTime(&this_time) ) {
        v += 1;
    }
    if ( !entry.GetTime(&entry_time) ) {
        v += 2;
    }
    if ( v == 0 ) {
        return this_time > entry_time;
    }
    if ( if_absent ) {
        switch(v) {
        case 1:  // NoThis - HasPath
            if ( if_absent &
                 (fNoThisHasPath_Newer | fNoThisHasPath_NotNewer) )
                return (if_absent & fNoThisHasPath_Newer) > 0;
            break;
        case 2:  // HasThis - NoPath
            if ( if_absent &
                 (fHasThisNoPath_Newer | fHasThisNoPath_NotNewer) )
                return (if_absent & fHasThisNoPath_Newer) > 0;
            break;
        case 3:  // NoThis - NoPath
            if ( if_absent &
                 (fNoThisNoPath_Newer | fNoThisNoPath_NotNewer) )
                return (if_absent & fNoThisNoPath_Newer) > 0;
            break;
        }
    }
    // throw an exception by default
    NCBI_THROW(CFileException, eNotExists, 
               "Directory entry does not exist");
    /*NOTREACHED*/
    return false;
}


bool CDirEntry::IsNewer(time_t tm, EIfAbsent if_absent) const
{
    time_t current;
    if ( !GetTimeT(&current) ) {
        switch(if_absent) {
        case eIfAbsent_Newer:
            return true;
        case eIfAbsent_NotNewer:
            return false;
        case eIfAbsent_Throw:
        default:
            NCBI_THROW(CFileException, eNotExists,
                       "Directory entry does not exist");
        }
    }
    return current > tm;
}


bool CDirEntry::IsNewer(const CTime& tm, EIfAbsent if_absent) const
{
    CTime current;
    if ( !GetTime(&current) ) {
        switch(if_absent) {
        case eIfAbsent_Newer:
            return true;
        case eIfAbsent_NotNewer:
            return false;
        case eIfAbsent_Throw:
        default:
            NCBI_THROW(CFileException, eNotExists, 
                       "Directory entry does not exist");
        }
    }
    return current > tm;
}


bool CDirEntry::IsIdentical(const string& entry_name,
                            EFollowLinks  follow_links) const
{
#if defined(NCBI_OS_UNIX)
    struct SStat st1, st2;
    if ( !Stat(&st1, follow_links) ) {
        LOG_ERROR(23, "CDirEntry::IsIdentical(): Cannot find: " + GetPath());
        return false;
    }
    if ( !CDirEntry(entry_name).Stat(&st2, follow_links) ) {
        LOG_ERROR(23, "CDirEntry::IsIdentical(): Cannot find: " + entry_name);
        return false;
    }
    return st1.orig.st_dev == st2.orig.st_dev  &&
           st1.orig.st_ino == st2.orig.st_ino;
#else
    return NormalizePath(GetPath(),  follow_links) ==
           NormalizePath(entry_name, follow_links);
#endif
}


bool CDirEntry::GetOwner(string* owner, string* group,
                         EFollowLinks follow, 
                         unsigned int* uid, unsigned int* gid) const
{
    if ( uid ) *uid = 0;
    if ( gid ) *gid = 0;

    if ( !owner  &&  !group ) {
        LOG_ERROR_NCBI(24, "CDirEntry::GetOwner(): Parameters are NULL for: " + GetPath(), CNcbiError::eInvalidArgument);
        return false;
    }

#if defined(NCBI_OS_MSWIN)

    bool res = CWinSecurity::GetFileOwner(GetPath(), owner, group, uid, gid);
    if (!res) {
        // CWinSecurity already set CNcbiError
        LOG_ERROR(24, "CDirEntry::GetOwner(): Unable to get owner for: " + GetPath());
    }
    return res;

#elif defined(NCBI_OS_UNIX)
    struct stat st;
    int errcode;
    
    if ( follow == eFollowLinks ) {
        errcode = stat(GetPath().c_str(), &st);
    } else {
        errcode = lstat(GetPath().c_str(), &st);
    }
    if ( errcode != 0 ) {
        LOG_ERROR_ERRNO(24, "CDirEntry::GetOwner(): stat() failed for: " + GetPath());
        return false;
    }
    if ( uid ) {
        *uid = st.st_uid;
    }
    if ( gid ) {
        *gid = st.st_gid;
    }
    if ( owner ) {
        CUnixFeature::GetUserNameByUID(st.st_uid).swap(*owner);
        if (owner->empty()) {
            NStr::NumericToString(*owner, st.st_uid, 0 /* flags */, 10);
        }
    }
    if ( group ) {
        CUnixFeature::GetGroupNameByGID(st.st_gid).swap(*group);
        if (group->empty()) {
            NStr::NumericToString(*group, st.st_gid, 0 /* flags */, 10);
        }
    }
    return true;

#endif
}


bool CDirEntry::SetOwner(const string& owner, const string& group,
                         EFollowLinks follow,
                         unsigned int* uid, unsigned int* gid) const
{
    if ( uid ) *uid = (unsigned int)(-1);
    if ( gid ) *gid = (unsigned int)(-1);

    if ( owner.empty()  &&  group.empty() ) {
        LOG_ERROR_NCBI(103, "CDirEntry::SetOwner(): Parameters are empty for: " + GetPath(), CNcbiError::eInvalidArgument);
        return false;
    }

#if defined(NCBI_OS_MSWIN)
    bool res = CWinSecurity::SetFileOwner(GetPath(), owner, group, uid, gid);
    if (!res) {
        // CWinSecurity already set CNcbiError
        LOG_ERROR(104, "CDirEntry::SetOwner(): Unable to set owner \""
                  + owner + ':' + group + "\" for: " + GetPath());
    }
    return res;

#elif defined(NCBI_OS_UNIX)

    uid_t temp_uid;
    if ( !owner.empty() ) {
        temp_uid = CUnixFeature::GetUserUIDByName(owner);
        if (temp_uid == (uid_t)(-1)){
            CNcbiError::SetFromErrno();
            unsigned int temp;
            if (!NStr::StringToNumeric(owner, &temp, NStr::fConvErr_NoThrow, 0)) {
                LOG_ERROR(25, "CDirEntry::SetOwner(): Invalid owner name \""
                          + owner + "\" for: " + GetPath());
                return false;
            }
            temp_uid = (uid_t) temp;
        }
        if ( uid ) {
            *uid = temp_uid;
        }
    } else {
        temp_uid = (uid_t)(-1);  // no change
    }

    gid_t temp_gid;
    if ( !group.empty() ) {
        temp_gid = CUnixFeature::GetGroupGIDByName(group);
        if (temp_gid == (gid_t)(-1)) {
            CNcbiError::SetFromErrno();
            unsigned int temp;
            if (!NStr::StringToNumeric(group, &temp, NStr::fConvErr_NoThrow, 0)) {
                LOG_ERROR(26, "CDirEntry::SetOwner(): Invalid group name \""
                          + group + "\" for: " + GetPath());
                return false;
            }
            temp_gid = (gid_t) temp;
        }
        if ( gid ) {
            *gid = temp_gid;
        }
    } else {
        temp_gid = (gid_t)(-1);  // no change
    }

    if (follow == eFollowLinks  ||  GetType(eIgnoreLinks) != eLink) {
        if ( chown(GetPath().c_str(), temp_uid, temp_gid) ) {
            LOG_ERROR_ERRNO(27, "CDirEntry::SetOwner(): Cannot change owner \""
                            + owner + ':' + group + "\" for: " + GetPath());
            return false;
        }
    }
#  if defined(HAVE_LCHOWN)
    else {
        if ( lchown(GetPath().c_str(), temp_uid, temp_gid) ) {
            LOG_ERROR_ERRNO(28, "CDirEntry::SetOwner(): Cannot change symlink owner \""
                            + owner + ':' + group + "\" for: " + GetPath());
            return false;
        }
    }
#  endif

    return true;
#endif
}


string CDirEntry::GetTmpName(ETmpFileCreationMode mode)
{
#if defined(NCBI_OS_MSWIN)  ||  defined(NCBI_OS_UNIX)
    return GetTmpNameEx(kEmptyStr, kEmptyStr, mode);
#else
    if (mode == eTmpFileCreate) {
        ERR_POST_X(2, Warning << 
                   "Temporary file cannot be auto-created on this platform, "
                   "return its name only");
    }
    TXChar* filename = NcbiSys_tempnam(0,0);
    if ( !filename ) {
        LOG_ERROR_ERRNO(91, "CDirEntry::GetTmpName(): tempnam() failed");
        return kEmptyStr;
    }
    string res(_T_CSTRING(filename));
    free(filename);
    return res;
#endif
}


#if !defined(NCBI_OS_UNIX)  &&  !defined(NCBI_OS_MSWIN)
static string s_StdGetTmpName(const char* dir, const char* prefix)
{
    char* filename = tempnam(dir, prefix);
    if ( !filename ) {
        LOG_ERROR_ERRNO(92, "CDirEntry::s_StdGetTmpName(): tempnam() failed");
        return kEmptyStr;
    }
    string str(filename);
    free(filename);
    return str;
}
#endif


string CDirEntry::GetTmpNameEx(const string&        dir,
                               const string&        prefix,
                               ETmpFileCreationMode mode)
{
    CFileIO temp_file;
    temp_file.CreateTemporary(dir, prefix,
                              mode == eTmpFileCreate ? CFileIO::eDoNotRemove : 
                                                       CFileIO::eRemoveInClose);
    temp_file.Close();
    return temp_file.GetPathname();
}


class CTmpStream : public fstream
{
public:

    CTmpStream(const char* s, IOS_BASE::openmode mode) : fstream(s, mode)
    {
        m_FileName = s;
        // Try to remove file and OS will automatically delete it after
        // the last file descriptor to it is closed (works only on UNIXes)
        CFile(m_FileName).Remove();
    }

#if defined(NCBI_OS_MSWIN)
    CTmpStream(const char* s, FILE* file) : fstream(file)
    {
        m_FileName = s; 
    }
#endif    

    virtual ~CTmpStream(void) 
    { 
        close();
        if ( !m_FileName.empty() ) {
            CFile(m_FileName).Remove();
        }
    }

protected:
    string m_FileName;  // Temporary file name
};


fstream* CDirEntry::CreateTmpFile(const string& filename, 
                                  ETextBinary   text_binary,
                                  EAllowRead    allow_read)
{
    string tmpname = filename.empty() ? GetTmpName(eTmpFileCreate) : filename;
    if ( tmpname.empty() ) {
        LOG_ERROR(29, "CDirEntry::CreateTmpFile(): Cannot get temporary file name");
        return 0;
    }
#if defined(NCBI_OS_MSWIN)
    // Open file manually, because we cannot say to fstream
    // to use some specific flags for file opening.
    // MS Windows should delete created file automatically
    // after closing all opened file descriptors.

    // We cannot enable "only write" mode here,
    // so ignore 'allow_read' flag.
    // Specify 'TD' (_O_SHORT_LIVED | _O_TEMPORARY)
    char mode[6] = "w+TDb";
    if (text_binary != eBinary) {
        mode[4] = '\0';
    }
    FILE* file = NcbiSys_fopen(_T_XCSTRING(tmpname), _T_XCSTRING(mode));
    if ( !file ) {
        LOG_ERROR_ERRNO(105, "CDirEntry::CreateTmpFile(): Cannot create temporary file: " + tmpname);
        return 0;
    }
    // Create FILE* based fstream.
    fstream* stream = new CTmpStream(tmpname.c_str(), file);
    // We dont need to close FILE*, it will be closed in the fstream

#else
    // Create filename based fstream
    ios::openmode mode = ios::out | ios::trunc;
    if ( text_binary == eBinary ) {
        mode = mode | ios::binary;
    }
    if ( allow_read == eAllowRead ) {
        mode = mode | ios::in;
    }
    fstream* stream = new CTmpStream(tmpname.c_str(), mode);
#endif

    if ( !stream->good() ) {
        delete stream;
        LOG_ERROR_NCBI(106, "CDirEntry::CreateTmpFile(): Cannot create temporary file stream for: " + tmpname,
                       CNcbiError::eNoSuchFileOrDirectory);
        return 0;
    }
    return stream;
}


fstream* CDirEntry::CreateTmpFileEx(const string& dir, const string& prefix,
                                    ETextBinary text_binary, 
                                    EAllowRead allow_read)
{
    return CreateTmpFile(GetTmpNameEx(dir, prefix, eTmpFileCreate),
                         text_binary, allow_read);
}


// Helper: Copy attributes (owner/date/time) from one entry to another.
// Both entries should have equal type.
//
// UNIX:
//     In mostly cases only super-user can change owner for
//     destination entry.  The owner of a file may change the group of
//     the file to any group of which that owner is a member.
// WINDOWS:
//     This function doesn't support ownership change yet.
//
static bool s_CopyAttrs(const char* from, const char* to,
                        CDirEntry::EType type, CDirEntry::TCopyFlags flags)
{
#if defined(NCBI_OS_UNIX)
    CDirEntry::SStat st;
    if ( !CDirEntry(from).Stat(&st) ) {
        LOG_ERROR(30, "s_CopyAttrs(): cannot get attributes for: " + string(from));
        return false;
    }

    // Date/time.
    // Set time before chmod() call, because on some platforms
    // setting time can affect file mode also.
    if ( F_ISSET(flags, CDirEntry::fCF_PreserveTime) ) {
#  if defined(HAVE_UTIMES)
        struct timeval tvp[2];
        tvp[0].tv_sec  = st.orig.st_atime;
        tvp[0].tv_usec = TV_USEC(st.atime_nsec / 1000);
        tvp[1].tv_sec  = st.orig.st_mtime;
        tvp[1].tv_usec = TV_USEC(st.mtime_nsec / 1000);
#    if defined(HAVE_LUTIMES)
        if (lutimes(to, tvp)) {
            LOG_ERROR_ERRNO(31, "CDirEntry::s_CopyAttrs(): lutimes() failed for: " + string(to));
            return false;
        }
#    else
        if (utimes(to, tvp)) {
            LOG_ERROR_ERRNO(32, "CDirEntry::s_CopyAttrs(): utimes() failed for: " + string(to));
            return false;
        }
#    endif
# else  // !HAVE_UTIMES
        // utimes() does not exists on current platform,
        // so use less accurate utime().
        struct utimbuf times;
        times.actime  = st.orig.st_atime;
        times.modtime = st.orig.st_mtime;
        if (utime(to, &times)) {
            LOG_ERROR_ERRNO(33, "CDirEntry::s_CopyAttrs(): utime() failed for: " + string(to));
            return false;
        }
#  endif // HAVE_UTIMES
    }

    // Owner. 
    // To improve performance change it right here,
    // do not use GetOwner/SetOwner.

    if ( F_ISSET(flags, CDirEntry::fCF_PreserveOwner) ) {
        if ( type == CDirEntry::eLink ) {
#  if defined(HAVE_LCHOWN)
            if ( lchown(to, st.orig.st_uid, st.orig.st_gid) ) {
                if (errno != EPERM) {
                    LOG_ERROR_ERRNO(34, "CDirEntry::s_CopyAttrs(): lchown() failed for: " + string(to));
                    return false;
                }
            }
#  endif
            // We cannot change permissions for sym.links (below),
            // so just exit from the function.
            return true;
        } else {
            // Changing the ownership will probably fail, unless we're root.
            // The setuid/gid bits can be cleared by OS.  If chown() fails,
            // strip the setuid/gid bits.
            if ( chown(to, st.orig.st_uid, st.orig.st_gid) ) {
                if ( errno != EPERM ) {
                    LOG_ERROR_ERRNO(35, "CDirEntry::s_CopyAttrs(): chown() failed for: " + string(to));
                    return false;
                }
                st.orig.st_mode &= ~(S_ISUID | S_ISGID);
            }
        }
    }

    // Permissions
    if ( F_ISSET(flags, CDirEntry::fCF_PreservePerm)  &&
        type != CDirEntry::eLink ) {
        if ( chmod(to, st.orig.st_mode) ) {
            LOG_ERROR_ERRNO(36, "CDirEntry::s_CopyAttrs(): chmod() failed for: " + string(to));
            return false;
        }
    }
    return true;


#elif defined(NCBI_OS_MSWIN)

    CDirEntry efrom(from), eto(to);

    WIN32_FILE_ATTRIBUTE_DATA attr;
    if ( !::GetFileAttributesEx(_T_XCSTRING(from), GetFileExInfoStandard, &attr) ) {
        LOG_ERROR_WIN(30, "CDirEntry::s_CopyAttrs(): cannot get attributes for: " + string(from));
        return false;
    }

    // Date/time
    if ( F_ISSET(flags, CDirEntry::fCF_PreserveTime) ) {
        HANDLE h = ::CreateFile(_T_XCSTRING(to),
                                FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, NULL,
                                OPEN_EXISTING,
                                FILE_FLAG_BACKUP_SEMANTICS /*for dirs*/, NULL); 
        if ( h == INVALID_HANDLE_VALUE ) {
            LOG_ERROR_WIN(37, "CDirEntry::s_CopyAttrs(): Cannot open: " + string(to));
            return false;
        }
        if ( !::SetFileTime(h, &attr.ftCreationTime, &attr.ftLastAccessTime, &attr.ftLastWriteTime) ) {
            LOG_ERROR_WIN(38, "CDirEntry::s_CopyAttrs(): Cannot change time for: " + string(to));
            ::CloseHandle(h);
            return false;
        }
        ::CloseHandle(h);
    }
    // Permissions
    if ( F_ISSET(flags, CDirEntry::fCF_PreservePerm) ) {
        if ( !::SetFileAttributes(_T_XCSTRING(to), attr.dwFileAttributes) ) {
            LOG_ERROR_WIN(39, "CDirEntry::s_CopyAttrs(): Cannot change pemissions for: " + string(to));
            return false;
        }
    }
    // Owner
    if ( F_ISSET(flags, CDirEntry::fCF_PreserveOwner) ) {
        string owner, group;
        // We don't check the result here, because often is impossible
        // to set the original owner name without administrator's rights.
        if ( efrom.GetOwner(&owner, &group) ) {
            eto.SetOwner(owner, group);
        }
    }

    return true;
#endif
}


//////////////////////////////////////////////////////////////////////////////
//
// CFile
//


CFile::~CFile(void)
{ 
    return;
}


Int8 CFile::GetLength(void) const
{
    TNcbiSys_stat st;
    if (NcbiSys_stat(_T_XCSTRING(GetPath()), &st) != 0) {
        LOG_ERROR_ERRNO(40, "CFile:GetLength(): stat() failed for: " + GetPath());
        return -1L;
    }
    if ( GetType(st) != eFile ) {
        LOG_ERROR_NCBI(40, "CFile:GetLength(): Not a file: " + GetPath(), CNcbiError::eOperationNotPermitted);
        return -1L;
    }
    return st.st_size;
}


#if !defined(NCBI_OS_MSWIN)

// Close file handle
//
static int s_CloseFile(int fd)
{
    while (close(fd) != 0) {
        if (errno != EINTR)
            return errno;
    }
    // Success
    return 0; 
}


// Copy file "src" to "dst"
//
static bool s_CopyFile(const char* src, const char* dst, size_t buf_size)
{
    int fs;  // source file descriptor
    int fd;  // destination file descriptor
    
    if ((fs = NcbiSys_open(src, O_RDONLY)) == -1) {
        CNcbiError::SetFromErrno(src);
        return false;
    }

    TNcbiSys_fstat st;
    if (NcbiSys_fstat(fs, &st) != 0  ||
        (fd = NcbiSys_open(dst, O_WRONLY|O_CREAT|O_TRUNC, st.st_mode & 0777)) == -1) {
        int x_errno = errno;
        s_CloseFile(fs);
        CNcbiError::SetErrno(errno = x_errno, src);
        return false;
    }

    // To prevent unnecessary memory (re-)allocations,
    // use the on-stack buffer if either the specified
    // "buf_size" or the size of the copied file is small.
    char  x_buf[4096];
    char* buf;

    if (3 * sizeof(x_buf) >= (Uint8) st.st_size) {
        // Use on-stack buffer for any files smaller than 3x of buffer size.
        buf_size = sizeof(x_buf);
        buf = x_buf;
    } else {
        if (buf_size == 0) {
            buf_size = kDefaultBufferSize;
        }
        // Use allocated buffer no bigger than the size of the file to copy.
        if (buf_size > (Uint8) st.st_size) {
            buf_size = st.st_size;
        }
        buf = buf_size > sizeof(x_buf) ? new char[buf_size] : x_buf;
    }

    // Copy files
    int x_errno = 0;
    do {
        ssize_t n_read = read(fs, buf, buf_size);
        if (n_read == 0) {
            break;
        }
        if (n_read < 0) {
            if (errno == EINTR) {
                continue;
            }
            x_errno = errno;
            break;
        }
        // Write to the output file
        const char* ptr = buf;
        do {
            ssize_t n_written = write(fd, ptr, n_read);
            if (n_written == 0) {
                x_errno = EINVAL;
                break;
            }
            if ( n_written < 0 ) {
                if (errno == EINTR) {
                    continue;
                }
                x_errno = errno;
                break;
            }
            n_read -= n_written;
            ptr    += n_written;
        } while (n_read > 0);
        
        if (n_read != 0) {
            if (x_errno == 0) {
                x_errno = EIO;
            }
        }
    } while (!x_errno);

    s_CloseFile(fs);
    int xx_err = s_CloseFile(fd);
    if (x_errno == 0) {
        x_errno = xx_err;
    }
    if (buf != x_buf) {
        delete [] buf;
    }
    if (x_errno != 0) {
        CNcbiError::SetErrno(errno = x_errno, src);
        return false;
    }
    return true;
}

#endif


bool CFile::Copy(const string& newname, TCopyFlags flags, size_t buf_size) const
{
    CFile src(*this);
    CFile dst(newname);

    // Dereference links
    if ( F_ISSET(flags, fCF_FollowLinks) ) {
        src.DereferenceLink();
        dst.DereferenceLink();
    }
    // The source file must exists
    EType src_type = src.GetType();
    if ( src_type != eFile )  {
        LOG_ERROR_NCBI(41, "CFile::Copy(): Source is not a file: " + GetPath(),
                       CNcbiError::eOperationNotPermitted);
        return false;
    }

    EType  dst_type   = dst.GetType();
    bool   dst_exists = (dst_type != eUnknown);
    string dst_safe_path;  // saved path for fCF_Safe

    // If destination exists...
    if ( dst_exists ) {
        // UNIX: check on copying file into yourself.
        // MS Window's ::CopyFile() can recognize such case.
#if defined(NCBI_OS_UNIX)
        if ( src.IsIdentical(dst.GetPath()) ) {
            LOG_ERROR_NCBI(41, "CFile::Copy(): Cannot copy into itself: " + src.GetPath(),
                           CNcbiError::eOperationNotPermitted);
            return false;
        }
#endif
        // Can copy entries with different types?
        // Destination must be a file too.
        if ( F_ISSET(flags, fCF_EqualTypes)  &&  (src_type != dst_type) ) {
            LOG_ERROR_NCBI(41, "CFile::Copy(): Destination is not a file: " + dst.GetPath(),
                           CNcbiError::eOperationNotPermitted);
            return false;
        }
        // Can overwrite entry?
        if ( !F_ISSET(flags, fCF_Overwrite) ) {
            LOG_ERROR_NCBI(42, "CFile::Copy(): Destination file exists: " + dst.GetPath(), 
                           CNcbiError::eOperationNotPermitted);
            return false;
        }
        // Copy only if destination is older
        if ( F_ISSET(flags, fCF_Update)  &&  !src.IsNewer(dst.GetPath(),0) ) {
            return true;
        }
        // Backup destination entry first
        if ( F_ISSET(flags, fCF_Backup) ) {
            // Use new CDirEntry object for 'dst', because its path
            // will be changed after backup
            CDirEntry dst_tmp(dst);
            if ( !dst_tmp.Backup(GetBackupSuffix(), eBackup_Rename) ) {
                LOG_ERROR(43, "CFile::Copy(): Cannot backup: " + dst.GetPath());
                return false;
            }
        }
    }
    // Safe copy -- copy to temporary file and rename later
    if (F_ISSET(flags, fCF_Safe)) {
        // Get new temporary name in the same directory
        string path, name, ext;
        SplitPath(dst.GetPath(), &path, &name, &ext);
        string tmp = GetTmpNameEx(path.empty() ? CDir::GetCwd() : path, name + ext + kTmpSafeSuffix);
        // Set new destination
        dst_safe_path = dst.GetPath();
        dst.Reset(tmp);
    }

    // Copy
#if defined(NCBI_OS_MSWIN)
    if ( !::CopyFile(_T_XCSTRING(src.GetPath()),
                     _T_XCSTRING(dst.GetPath()), FALSE) ) {
        LOG_ERROR_WIN(44, "CFile::Copy(): Cannot copy " + src.GetPath() + " to " + dst.GetPath());
        dst.RemoveEntry();
        return false;
    }
#else
    if ( !s_CopyFile(src.GetPath().c_str(), dst.GetPath().c_str(), buf_size) ) {
        LOG_ERROR_ERRNO(44, "CFile::Copy(): Cannot copy " + src.GetPath() + " to " + dst.GetPath());
        dst.Remove();
        return false;
    }
#endif

    // Safe copy -- renaming
    if (F_ISSET(flags, fCF_Safe)) {
        if (!dst.Rename(dst_safe_path, fRF_Overwrite)) {
            dst.RemoveEntry();
            LOG_ERROR_NCBI(45, "CFile:Copy():"
                               " Cannot rename temporary file " + dst.GetPath() +
                               " to " + dst_safe_path, CNcbiError::eIoError);
            return false;
        }
    }
    // Verify copied data
    if ( F_ISSET(flags, fCF_Verify)  &&  !src.Compare(dst.GetPath()) ) {
        LOG_ERROR_NCBI(46, "CFile::Copy(): Verification for " + src.GetPath() + 
                           " and " + dst.GetPath() + " failed", CNcbiError::eIoError);
        return false;
    }

    // Preserve attributes
    // s_CopyFile() preserve permissions on Unix, MS-Windows don't need it at all.

#if defined(NCBI_OS_MSWIN)
    // On MS Windows ::CopyFile() already preserved file attributes
    // and all date/times.
    flags &= ~(fCF_PreservePerm | fCF_PreserveTime);
#endif
    if ( flags & fCF_PreserveAll ) {
        if ( !s_CopyAttrs(src.GetPath().c_str(),
                          dst.GetPath().c_str(), eFile, flags) ) {
            LOG_ERROR(95, "CFile::Copy(): Cannot copy permissions from " + 
                          src.GetPath() + " to " + dst.GetPath());
            return false;
        }
    }
    return true;
}


bool CFile::Compare(const string& filename, size_t buf_size) const
{
    // To prevent unnecessary memory (re-)allocations,
    // use the on-stack buffer if either the specified
    // "buf_size" or the size of the compared files is small.
    char   x_buf[4096*2];
    size_t x_size = sizeof(x_buf)/2;
    char*  buf1   = 0;
    char*  buf2   = 0;
    bool   equal  = false;
    
    try {
        CFileIO f1;
        CFileIO f2;
        f1.Open(GetPath(), CFileIO::eOpen, CFileIO::eRead);
        f2.Open(filename,  CFileIO::eOpen, CFileIO::eRead);
 
        Uint8 s1 = f1.GetFileSize();
        Uint8 s2 = f2.GetFileSize();
        
        // Files should have equal sizes
        if (s1 != s2) {
            LOG_ERROR_NCBI(93, "CFile::Compare(): files have different size: " + 
                               GetPath() + " and " + filename, 
                               CNcbiError::eOperationNotPermitted);
            return false;
        }
        if (s1 == 0) {
            return true;
        }
        // Use on-stack buffer for any files smaller than 3x of buffer size.
        if (s1 <= 3 * x_size) {
            buf_size = x_size;
            buf1 = x_buf;
            buf2 = x_buf + x_size;
        } else {
            if (buf_size == 0) {
                buf_size = kDefaultBufferSize;
            }
            // Use allocated buffer no bigger than the size of the file to compare.
            // Align buffer in memory to 8 byte boundary.
            if (buf_size > s1) {
                buf_size = (size_t)s1 + (8 - s1 % 8);
            }
            if (buf_size > x_size) {
                buf1 = new char[buf_size*2];
                buf2 = buf1 + buf_size;
            } else {
                buf1 = x_buf;
                buf2 = x_buf + x_size;
            }
        }

        size_t n1 = 0;
        size_t n2 = 0;
        size_t s  = 0;

        // Compare files
        for (;;) {
            size_t n;
            if (n1 < buf_size) {
                n = f1.Read(buf1 + n1, buf_size - n1);
                if (n == 0) {
                    break;
                }
                n1 += n;
            }
            if (n2 < buf_size) {
                n = f2.Read(buf2 + n2, buf_size - n2);
                if (n == 0) {
                    break;
                }
                n2 += n;
            }
            size_t m = min(n1, n2);
            if ( memcmp(buf1, buf2, m) != 0 ) {
                break;
            }
            if (n1 > m) {
                memmove(buf1, buf1 + m, n1 - m);
                n1 -= m;
            } else {
                n1 = 0;
            }
            if (n2 > m) {
                memmove(buf2, buf2 + m, n2 - m);
                n2 -= m;
            } else {
                n2 = 0;
            }
            s += m;
        }
        equal = (s1 == s);
    }
    catch (const CFileErrnoException& ex) {
        LOG_ERROR_NCBI(47, "CFile::Compare(): error comparing files " + GetPath() + 
                           " and " + filename + " : " + ex.what(), 
                           CNcbiError::eIoError);
    }
    if (buf1 != x_buf) {
        delete [] buf1;
    }
    return equal;
}


bool CFile::CompareTextContents(const string& file, ECompareText mode,
                                size_t buf_size) const
{
    CNcbiIfstream f1(GetPath().c_str(), IOS_BASE::in);
    CNcbiIfstream f2(file.c_str(),      IOS_BASE::in);

    if ( !buf_size ) {
        buf_size = kDefaultBufferSize;
    }
    return NcbiStreamCompareText(f1, f2, (ECompareTextMode)mode, (streamsize)buf_size);
}



//////////////////////////////////////////////////////////////////////////////
//
// CDir
//

#if defined(NCBI_OS_UNIX)

static bool s_GetHomeByUID(string& home)
{
    // Get the info using user ID
    struct passwd* pwd;

    if ((pwd = getpwuid(getuid())) == 0) {
        LOG_ERROR_ERRNO(48, "s_GetHomeByUID(): getpwuid() failed");
        return false;
    }
    home = pwd->pw_dir;
    return true;
}

static bool s_GetHomeByLOGIN(string& home)
{
    const TXChar* ptr = 0;
    // Get user name
    if ( !(ptr = NcbiSys_getenv(_TX("USER"))) ) {
        if ( !(ptr = NcbiSys_getenv(_TX("LOGNAME"))) ) {
            if ( !(ptr = getlogin()) ) {
                LOG_ERROR_ERRNO(49, "s_GetHomeByLOGIN(): Unable to get user name");
                return false;
            }
        }
    }
    // Get home dir for this user
    struct passwd* pwd = getpwnam(ptr);
    if ( !pwd ||  pwd->pw_dir[0] == '\0') {
        LOG_ERROR_ERRNO(50, "s_GetHomeByLOGIN(): getpwnam() failed");
        return false;
    }
    home = pwd->pw_dir;
    return true;
}

#endif // NCBI_OS_UNIX


string CDir::GetHome(EHomeWindows home_check_order)
{
    string home;

#if defined(NCBI_OS_MSWIN)
    // Get home dir from environment variables

    const TXChar* str = NcbiSys_getenv((home_check_order == eHomeWindows_AppData) ? _TX("APPDATA") : _TX("USERPROFILE"));
    if ( str ) {
        home = _T_CSTRING(str);
    } else {
        str = NcbiSys_getenv((home_check_order == eHomeWindows_AppData) ? _TX("USERPROFILE") : _TX("APPDATA"));
        if ( str ) {
            home = _T_CSTRING(str);
        }
    }
#elif defined(NCBI_OS_UNIX)
    // Try get home dir from environment variable
    char* str = NcbiSys_getenv(_TX("HOME"));
    if ( str ) {
        home = str;
    } else {
        // Try to retrieve the home dir -- first use user's ID,
        // and if failed, then use user's login name.
        if ( !s_GetHomeByUID(home) ) { 
            s_GetHomeByLOGIN(home);
        }
    }
#endif 

    // Add trailing separator if needed
    return AddTrailingPathSeparator(home);
}


string CDir::GetTmpDir(void)
{
    string tmp;

#if defined(NCBI_OS_UNIX)

    char* tmpdir = getenv("TMPDIR");
    if ( tmpdir ) {
        tmp = tmpdir;
    } else  {
#  if defined(P_tmpdir)
        tmp = P_tmpdir;
#  else
        tmp = "/tmp";
#  endif
    }

#elif defined(NCBI_OS_MSWIN)

    const TXChar* tmpdir = NcbiSys_getenv(_TX("TEMP"));
    if ( tmpdir ) {
        tmp = _T_CSTRING(tmpdir);
    } else  {
#  if defined(P_tmpdir)
        tmp = P_tmpdir;
#  else
        tmp = CDir::GetHome();
#  endif
    }

#endif
    return tmp;
}


string CDir::GetAppTmpDir(void)
{
    // Get application specific temporary directory name
    string tmp = NCBI_PARAM_TYPE(NCBI, TmpDir)::GetThreadDefault();
    if ( !tmp.empty() ) {
        return tmp;
    }
    // Use default TMP directory specified by OS
    return CDir::GetTmpDir();
}


string CDir::GetCwd(void)
{
    TXChar buf[4096];
    if ( NcbiSys_getcwd(buf, sizeof(buf)/sizeof(TXChar) - 1) ) {
        return _T_CSTRING(buf);
    }
    LOG_ERROR_ERRNO(90, "CDir::GetCwd(): Cannot get current directory");
    return kEmptyCStr;
}


bool CDir::SetCwd(const string& dir)
{
    if ( NcbiSys_chdir(_T_XCSTRING(dir)) != 0 ) {
        LOG_ERROR_ERRNO(51, "CDir::SetCwd(): Cannot change directory to: " + dir);
        return false;
    }
    return true;
}


CDir::~CDir(void)
{
    return;
}


bool CDirEntry::MatchesMask(const string& name,
                            const vector<string>& masks,
                            NStr::ECase use_case)
{
    if ( masks.empty() ) {
        return true;
    }
    ITERATE(vector<string>, itm, masks) {
        const string& mask = *itm;
        if ( MatchesMask(name, mask, use_case) ) {
            return true;
        }
    }
    return false;
}


// Helpers functions and macro for GetEntries().

#if defined(NCBI_OS_MSWIN)

// Set errno for failed FindFirstFile/FindNextFile
// TODO: 
//   This method should be removed, we dont need to translate
//   Windows error to errno, we already have CNcbiErrno for this,
//   please use it.

static void s_SetFindFileError(DWORD err)
{
    ::SetLastError(err); // set Windows error back
    switch (err) {
        case ERROR_NO_MORE_FILES:
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
            errno = ENOENT;
            break;
        case ERROR_NOT_ENOUGH_MEMORY:
            errno = ENOMEM;
            break;
        case ERROR_ACCESS_DENIED:
            errno = EACCES;
            break;
        default:
            errno = EINVAL;
            break;
    }
}

#  define IS_RECURSIVE_ENTRY                     \
    ( (flags & CDir::fIgnoreRecursive)  &&       \
      ((NcbiSys_strcmp(entry.cFileName, _TX("."))  == 0) ||  \
       (NcbiSys_strcmp(entry.cFileName, _TX("..")) == 0)) )

static void s_AddEntry(CDir::TEntries*        contents,
                       const string&          base_path,
                       const WIN32_FIND_DATA& entry,
                       CDir::TGetEntriesFlags flags)
{
    const string name = (flags & CDir::fIgnorePath) ?
                         _T_CSTRING(entry.cFileName) :
                         base_path + _T_CSTRING(entry.cFileName);
        
    if (flags & CDir::fCreateObjects) {
        CDirEntry::EType type = (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) 
                                 ? CDirEntry::eDir : CDirEntry::eFile;
        contents->push_back(CDirEntry::CreateObject(type, name));
    } else {
        contents->push_back(new CDirEntry(name));
    }
}

#else // NCBI_OS_UNIX

#  define IS_RECURSIVE_ENTRY                   \
    ( (flags & CDir::fIgnoreRecursive)  &&     \
      ((::strcmp(entry->d_name, ".")  == 0) || \
       (::strcmp(entry->d_name, "..") == 0)) )

static void s_AddEntry(CDir::TEntries*        contents,
                       const string&          base_path,
                       const struct dirent*   entry,
                       CDir::TGetEntriesFlags flags)
{
    const string name = (flags & CDir::fIgnorePath) ? entry->d_name : base_path + entry->d_name;

    if (flags & CDir::fCreateObjects) {
        CDirEntry::EType type = CDir::eUnknown;
#  if defined(_DIRENT_HAVE_D_TYPE)
        struct stat st;
        if (entry->d_type) {
            st.st_mode = DTTOIF(entry->d_type);
            type = CDirEntry::GetType(st);
        }
#  endif
        if (type == CDir::eUnknown) {
            if (flags & CDir::fIgnorePath) {
                const string path = base_path + entry->d_name;
                type = CDirEntry(path).GetType();
            } else {
                type = CDirEntry(name).GetType();
            }
        }
        contents->push_back(CDirEntry::CreateObject(type, name));
    } else {
        contents->push_back(new CDirEntry(name));
    }
}

#endif


CDir::TEntries CDir::GetEntries(const string& mask, TGetEntriesFlags flags) const
{
    CMaskFileName masks;
    if ( !mask.empty() ) {
        masks.Add(mask);
    }
    return GetEntries(masks, flags);
}


CDir::TEntries* CDir::GetEntriesPtr(const string& mask, TGetEntriesFlags flags) const
{
    CMaskFileName masks;
    if ( !mask.empty() ) {
        masks.Add(mask);
    }
    return GetEntriesPtr(masks, flags);
}


CDir::TEntries CDir::GetEntries(const vector<string>& masks, TGetEntriesFlags flags) const
{
    unique_ptr<TEntries> contents(GetEntriesPtr(masks, flags));
    return contents.get() ? *contents.get() : TEntries();
}


CDir::TEntries* CDir::GetEntriesPtr(const vector<string>& masks, TGetEntriesFlags flags) const
{
    if ( masks.empty() ) {
        return GetEntriesPtr(kEmptyStr, flags);
    }
    TEntries* contents = new TEntries;
    string base_path =  AddTrailingPathSeparator(GetPath().empty() ? DIR_CURRENT : GetPath());
    NStr::ECase use_case = (flags & fNoCase) ? NStr::eNocase : NStr::eCase;

#if defined(NCBI_OS_MSWIN)

    // Append to the "path" mask for all files in directory
    string pattern = base_path + "*";

    WIN32_FIND_DATA entry;
    HANDLE          handle;

    handle = ::FindFirstFile(_T_XCSTRING(pattern), &entry);
    if (handle != INVALID_HANDLE_VALUE) {
        // Check all masks
        do {
            if (!IS_RECURSIVE_ENTRY) {
                ITERATE(vector<string>, it, masks) {
                    const string& mask = *it;
                    if ( mask.empty()  ||
                         MatchesMask(_T_CSTRING(entry.cFileName), mask, use_case) ) {
                        s_AddEntry(contents, base_path, entry, flags);
                        break;
                    }                
                }
            }
        } while (::FindNextFile(handle, &entry));
        CNcbiError::SetFromWindowsError();
        ::FindClose(handle);

    } else {
        DWORD err = ::GetLastError();
        CNcbiError::SetWindowsError(err);
        s_SetFindFileError(err);
        delete contents;
        if ( F_ISSET(flags, fThrowOnError) ) {
            NCBI_THROW(CFileErrnoException, eFile, "Cannot read directory " + base_path);
        }
        return NULL;
    }

#elif defined(NCBI_OS_UNIX)

    DIR* dir = opendir(base_path.c_str());
    if ( !dir ) {
        CNcbiError::SetFromErrno();
        delete contents;
        if ( F_ISSET(flags, fThrowOnError) ) {
            NCBI_THROW(CFileErrnoException, eFile, "Cannot read directory " + base_path);
        }
        return NULL;
    }
    while (struct dirent* entry = readdir(dir)) {
        if (IS_RECURSIVE_ENTRY) {
            continue;
        }
        ITERATE(vector<string>, it, masks) {
            const string& mask = *it;
            if ( mask.empty()  ||  MatchesMask(entry->d_name, mask, use_case) ) {
                s_AddEntry(contents, base_path, entry, flags);
                break;
            }
        } // ITERATE
    } // while
    CNcbiError::SetFromErrno();
    closedir(dir);

#endif

    return contents;
}


CDir::TEntries CDir::GetEntries(const CMask& masks, TGetEntriesFlags flags) const
{
    unique_ptr<TEntries> contents(GetEntriesPtr(masks, flags));
    return contents.get() ? *contents.get() : TEntries();
}


CDir::TEntries* CDir::GetEntriesPtr(const CMask& masks, TGetEntriesFlags flags) const
{
    TEntries* contents = new TEntries;
    string base_path = AddTrailingPathSeparator(GetPath().empty() ? DIR_CURRENT : GetPath());
    NStr::ECase use_case = (flags & fNoCase) ? NStr::eNocase : NStr::eCase;

#if defined(NCBI_OS_MSWIN)

    // Append to the "path" mask for all files in directory
    string pattern = base_path + "*";

    WIN32_FIND_DATA entry;
    HANDLE          handle;

    handle = ::FindFirstFile(_T_XCSTRING(pattern), &entry);
    if (handle != INVALID_HANDLE_VALUE) {
        do {
            if ( !IS_RECURSIVE_ENTRY  &&
                 masks.Match(_T_CSTRING(entry.cFileName), use_case) ) {
                s_AddEntry(contents, base_path, entry, flags);
            }
        } while ( ::FindNextFile(handle, &entry) );
        CNcbiError::SetFromWindowsError();
        ::FindClose(handle);

    } else {
        DWORD err = ::GetLastError();
        CNcbiError::SetWindowsError(err);
        s_SetFindFileError(err);
        delete contents;
        if ( F_ISSET(flags, fThrowOnError) ) {
            NCBI_THROW(CFileErrnoException, eFile, string("Cannot read directory ") + base_path);
        }
        return NULL;
    }

#elif defined(NCBI_OS_UNIX)

    DIR* dir = opendir(base_path.c_str());
    if ( !dir ) {
        CNcbiError::SetFromErrno();
        delete contents;
        if ( F_ISSET(flags, fThrowOnError) ) {
            NCBI_THROW(CFileErrnoException, eFile, string("Cannot read directory ") + base_path);
        }
        return NULL;
    }
    while (struct dirent* entry = readdir(dir)) {
        if ( !IS_RECURSIVE_ENTRY  &&
             masks.Match(entry->d_name, use_case) ) {
            s_AddEntry(contents, base_path, entry, flags);
        }
    }
    CNcbiError::SetFromErrno();
    closedir(dir);

#endif

    return contents;
}


// Helper function for CDir::Create[Path]()
inline bool s_DirCreate(const string&path, CDir::TCreateFlags flags, mode_t mode)
{
    errno = 0;
#if defined(NCBI_OS_MSWIN)
    int res = NcbiSys_mkdir(_T_XCSTRING(path));
#elif defined(NCBI_OS_UNIX)
    int res = NcbiSys_mkdir(_T_XCSTRING(path), mode);
#endif
    if (res != 0) {
        if (errno != EEXIST) {
            LOG_ERROR_ERRNO(52, "s_DirCreate(): Cannot create directory: " + path);
            return false;
        }
        // Entry with such name already exists, check its type
        CDirEntry::EType type = CDirEntry(path).GetType();
        if (type != CDirEntry::eDir) {
            LOG_ERROR_NCBI(53, "s_DirCreate(): Path already exist and is not a directory: " + path, CNcbiError::eNotADirectory);
            return false;
        }
        if (F_ISSET(flags, CDir::fCreate_ErrorIfExists)) {
            LOG_ERROR_NCBI(54, "s_DirCreate(): Directory already exist: " + path, CNcbiError::eFileExists);
            return false;
        }
        if (!F_ISSET(flags, CDir::fCreate_UpdateIfExists)) {
            return true;
        }
    }
    // The permissions for the created directory is controlled by umask and is (mode & ~umask & 0777).
    // We need to call chmod() directly if we need other behavior.

    _ASSERT(CDir::fCreate_Default      == 0  &&
            CDir::fCreate_PermByUmask  != 0  &&
            CDir::fCreate_PermAsParent != 0);
    _ASSERT(!F_ISSET(flags, CDir::fCreate_PermByUmask | CDir::fCreate_PermAsParent));

    if ( F_ISSET(flags, CDir::fCreate_PermByUmask)  ||  
          (!F_ISSET(flags, CDir::fCreate_PermByUmask)  &&  !F_ISSET(flags, CDir::fCreate_PermAsParent)  &&
           NCBI_PARAM_TYPE(NCBI, FileAPIHonorUmask)::GetDefault()) ) {
        // nothing to do if (umask) or (default mode with "honor umask" global flag)
        return true;
    }
    // Change directory permissions
    if (NcbiSys_chmod(_T_XCSTRING(path), mode) != 0) {
        LOG_ERROR_ERRNO(55, "CDir::Create(): Cannot set mode for directory: " + path);
        return false;
    }
    return true;
}


bool CDir::Create(TCreateFlags flags) const
{
    if (GetPath().empty()) {
        LOG_ERROR_NCBI(56, "CDir::Create(): Path is empty", CNcbiError::eInvalidArgument);
        return false;
    }
    mode_t mode = GetDefaultModeT();

    // Get parent permissions
    if (F_ISSET(flags, fCreate_PermAsParent)) {
        CDir d(CreateAbsolutePath(GetPath()));
        string path_up(d.GetDir());
        if ( path_up.empty()  ||  path_up == d.GetPath() ) {
            LOG_ERROR_NCBI(57, "CDir::Create(): Cannot get parent directory for: " + GetPath(), 
                           CNcbiError::eNoSuchFileOrDirectory);
            return false;
        }
#if defined(NCBI_OS_MSWIN)
        // Special case -- stat() dont works if directory have trailing path
        // separator, except it is a root directory with a disk name, like "C:\".
        if (path_up.length() > 3) {
            path_up = DeleteTrailingPathSeparator(path_up);
        }
#endif
        TNcbiSys_stat st;
        if (NcbiSys_stat(_T_XCSTRING(path_up), &st) != 0) {
            LOG_ERROR_ERRNO(58, "CDir::Create(): stat() failed for: " + GetPath());
            return false;
        }
        mode = st.st_mode;
    }
    return s_DirCreate(GetPath(), flags, mode);
}


bool CDir::CreatePath(TCreateFlags flags) const
{
    if (GetPath().empty()) {
        LOG_ERROR_NCBI(59, "CDir::CreatePath(): Path is empty", CNcbiError::eInvalidArgument);
        return false;
    }
    string path(CreateAbsolutePath(GetPath()));
    if (path.empty()) {
        LOG_ERROR_NCBI(60, "CDir::CreatePath(): Cannot create absolute path from: " + path, CNcbiError::eInvalidArgument);
        return false;
    }
    if (path[path.length()-1] == GetPathSeparator()
#if defined(NCBI_OS_MSWIN)
        &&  path.length() != 3
        // Special case -- for path like "C:\" dont remove a last separator, it represent a root directory
#endif
        ) {
        path.erase(path.length() - 1);
    }

    // Find all missed parts of the path

    CTempString tmp(path); // existent part of a path
    std::list<CTempString> missed_parts;

    while (!tmp.empty()  &&  !CDirEntry(tmp).Exists()) {
        size_t pos = tmp.find_last_of(DIR_SEPARATORS);
        if (pos == NPOS) {
            break;
        }
        CTempString part(tmp.substr(pos+1));
        missed_parts.push_front(part);
        tmp.erase(pos);
    }

    mode_t mode = GetDefaultModeT();

    // Get parent permissions
    if (F_ISSET(flags, fCreate_PermAsParent)) {
        string parent;
        if (missed_parts.empty()) {
            parent.assign(CDir(tmp).GetDir());
        } else {
            parent.assign(tmp);
        }
#if defined(NCBI_OS_MSWIN)
        // Special case -- for paths like "C:" add slash to represent a root directory
        if (parent.length() == 2) {
            parent += GetPathSeparator();
        }
#endif
        TNcbiSys_stat st;
        if (NcbiSys_stat(_T_XCSTRING(parent), &st) != 0) {
            LOG_ERROR_ERRNO(61, "CDir::CreatePath(): stat() failed for: " + parent);
            return false;
        }
        mode = st.st_mode;
    }

    // Path exists?
    if (missed_parts.empty()) {
        // check existence and behave depends on flags
        if (!s_DirCreate(path, flags, mode)) {
            LOG_ERROR(96, "CDir::CreatePath(): Cannot create path: " + GetPath());
            return false;
        }
        return true;
    }

    // Create missed subdirectories
    string p = tmp;
    for (auto i : missed_parts) {
        p += GetPathSeparator();
        p += i;
        if (!s_DirCreate(p, flags, mode)) {
            LOG_ERROR(97, "CDir::CreatePath(): Cannot create path: " + GetPath());
            return false;
        }
    }
    return true;
}


bool CDir::Copy(const string& newname, TCopyFlags flags, size_t buf_size) const
{
    CDir src(*this);
    CDir dst(newname);

    // Dereference links
    bool follow = F_ISSET(flags, fCF_FollowLinks);
    if ( follow ) {
        src.DereferenceLink();
        dst.DereferenceLink();
    }
    // The source dir must exists
    EType src_type = src.GetType();
    if ( src_type != eDir )  {
        LOG_ERROR_NCBI(62, "CDir::Copy(): Source is not a directory: " + src.GetPath(),
                       CNcbiError::eNoSuchFileOrDirectory);
        return false;
    }
    EType  dst_type        = dst.GetType();
    bool   dst_exists      = (dst_type != eUnknown);
    bool   need_create_dst = !dst_exists;
    string dst_safe_path;  // saved path for fCF_Safe

    // Safe copy? 
    // Don't use it if fCF_TopDirOnly is not specified. If target directory
    // exists it will be just "updated" and safe copying will be applied
    // on a file level for every copied entry.
    bool need_safe_copy = F_ISSET(flags, fCF_Safe | fCF_TopDirOnly);

    // If destination exists...
    if ( dst_exists ) {
        // Check on copying dir into yourself
        if ( src.IsIdentical(dst.GetPath()) ) {
            LOG_ERROR_NCBI(63, "CDir::Copy(): Source and destination are the same: " + src.GetPath(),
                           CNcbiError::eOperationNotPermitted);
            return false;
        }
        // Can rename entries with different types?
        if ( F_ISSET(flags, fCF_EqualTypes)  &&  (src_type != dst_type) ) {
            LOG_ERROR_NCBI(64, "CDir::Copy(): Destination is not a directory: " + dst.GetPath(), 
                           CNcbiError::eOperationNotPermitted);
            return false;
        }

        // Some operation can be made for top directory only

        if ( F_ISSET(flags, fCF_TopDirOnly) ) {
            // Can overwrite entry?
            if ( !F_ISSET(flags, fCF_Overwrite) ) {
                LOG_ERROR_NCBI(65, "CDir::Copy(): Destination directory already exists: " + dst.GetPath(), 
                               CNcbiError::eOperationNotPermitted);
                return false;
            }
            // Copy only if destination is older
            if ( F_ISSET(flags, fCF_Update)  &&  !src.IsNewer(dst.GetPath(), 0) ) {
                return true;
            }
            // Backup destination directory
            if (F_ISSET(flags, fCF_Backup)) {
                // Use new CDirEntry object instead of 'dst', because its path
                // will be changed after backup
                CDirEntry dst_tmp(dst);
                if ( !dst_tmp.Backup(GetBackupSuffix(), eBackup_Rename) ) {
                    LOG_ERROR(66, "CDir::Copy(): Cannot backup destination directory: " + dst.GetPath());
                    return false;
                }
                need_create_dst = true;
            }
            // Clear flags not needed anymore.
            // Keep fCF_Overwrite if it is set (fCF_Backup is a compound flag).
            flags &= ~(fCF_TopDirOnly | (fCF_Backup - fCF_Overwrite));
        }
    }

    // Safe copy for top directory -- copy to temporary directory in the same
    // parent directory and rename later.

    if ( need_safe_copy ) {
        // Get new temporary name in the same directory
        string path, name, ext;
        SplitPath(dst.GetPath(), &path, &name, &ext);
        string tmp = GetTmpNameEx(path.empty() ? CDir::GetCwd() : path, name + ext + kTmpSafeSuffix);
        // Set new destination
        dst_safe_path = dst.GetPath();
        dst.Reset(tmp);
        need_create_dst = true;
        // Clear safe flag, we already have a temporary top directory
        flags &= ~fCF_Safe;
    }

    // Create target directory if needed
    if ( need_create_dst ) {
        if ( !dst.CreatePath() ) {
            LOG_ERROR(67, "CDir::Copy(): Cannot create " << 
                          (dst_safe_path.empty() ? "target" : "temporary") <<
                          " directory: " << dst.GetPath());
            return false;
        }
    }

    // Read all entries in source directory
    unique_ptr<TEntries> contents(src.GetEntriesPtr(kEmptyStr, fIgnoreRecursive));
    if ( !contents.get() ) {
        LOG_ERROR(68, "CDir::Copy(): Cannot get content of " + src.GetPath());
        return false;
    }

    // And copy each of them to target directory
    ITERATE(TEntries, e, *contents.get()) {
        CDirEntry& entry = **e;
        if (!F_ISSET(flags, fCF_Recursive) &&
            entry.IsDir(follow ? eFollowLinks : eIgnoreLinks)) {
            continue;
        }
        // Copy entry
        if (!entry.CopyToDir(dst.GetPath(), flags, buf_size)) {
            LOG_ERROR(69, "CDir::Copy(): Cannot copy " + 
                      entry.GetPath() + " to directory " + dst.GetPath());
            return false;
        }
    }

    // Safe copy for top directory -- renaming temporary to target
    if (!dst_safe_path.empty()) {
        if (!dst.Rename(dst_safe_path, fRF_Overwrite)) {
            dst.Remove();
            LOG_ERROR(70, "CDir:Copy(): Cannot rename temporary directory " +
                      dst.GetPath() + " to " + dst_safe_path);
            return false;
        }
    }
    // Preserve attributes
    if ( flags & fCF_PreserveAll ) {
        if ( !s_CopyAttrs(src.GetPath().c_str(),
                          dst.GetPath().c_str(), eDir, flags) ) {
            LOG_ERROR(98, "CDir:Copy(): Cannot copy attributes from " +
                      src.GetPath() + " to " + dst.GetPath());
            return false;
        }
    } else {
        // Set default permissions for directory, if we should not
        // honor umask settings.
        if ( !NCBI_PARAM_TYPE(NCBI, FileAPIHonorUmask)::GetDefault()) {
            if ( !dst.SetMode(fDefault, fDefault, fDefault) ) {
                LOG_ERROR(99, "CDir:Copy(): Cannot set default directory permissions: " + dst.GetPath());
                return false;
            }
        }
    }
    return true;
}


bool CDir::Remove(TRemoveFlags flags) const
{
    // Assumption
    _ASSERT(fDir_Self  == fEntry);
    _ASSERT(eOnlyEmpty == fEntry);
    
    // Remove directory as empty
    if ( (flags & (fDir_All | fDir_Recursive)) == eOnlyEmpty ) {
        if ( NcbiSys_rmdir(_T_XCSTRING(GetPath())) != 0 ) {
            if ( (flags & fIgnoreMissing)  &&  (errno == ENOENT) ) {
                return true;
            }
            LOG_ERROR_ERRNO(71, "CDir::Remove(): Cannot remove (by implication empty)"
                                " directory: " + GetPath());
            return false;
        }
        return true;
    }

#if !defined(NCBI_OS_MSWIN)
    // Make directory writable for user to remove any entry inside
    SetMode(CDirEntry::fWrite | static_cast<TMode>(CDirEntry::fModeAdd), 
            CDirEntry::fModeNoChange,
            CDirEntry::fModeNoChange);
#endif

    // Read all entries in directory
    unique_ptr<TEntries> contents(GetEntriesPtr());
    if (!contents.get()) {
        LOG_ERROR(72, "CDir::Remove(): Cannot get content of: " + GetPath());
        return false;
    }
 
    bool success = true;
    try {
        // Remove each entry
        ITERATE(TEntries, entry, *contents.get()) {
            string name = (*entry)->GetName();
            if (name == "." || name == ".." || name == string(1, GetPathSeparator())) {
                continue;
            }
            // Get entry item with full pathname
            CDirEntry item(GetPath() + GetPathSeparator() + name);

            if (flags & fDir_Recursive) {
                // Update flags to process subdirectories itself,
                // because the top directory entry may not have
                // such flag.
                int f = (flags & fDir_Subdirs) ? (flags | fDir_Self) : flags;
                if (item.IsDir(eIgnoreLinks)) {
                    if (!CDir(item.GetPath()).Remove(f)) {
                        if (flags & fProcessAll) {
                            success = false;
                        } else {
                            throw "Removing subdirectory failed";
                        }
                    }
                }
                else if (flags & fDir_Files) {
                    if (!item.Remove(f)) {
                        if (flags & fProcessAll) {
                            success = false;
                        } else {
                            throw "Removing directory entry failed";
                        }
                    }
                }
            }
            else if (item.IsDir(eIgnoreLinks)) {
                // Non-recursive directory removal
                if (flags & fDir_Subdirs) {
                    // Clear all flags to go inside directory,
                    // and try to remove it as "empty".
                    if (!item.Remove((flags & ~fDir_All) | fDir_Self)) {
                        if (flags & fProcessAll) {
                            success = false;
                        } else {
                            throw "Removing directory entry (non-recursive) failed";
                        }
                    }
                }
                continue;
            }
            else {
                if (flags & fDir_Files) {
                    if (!item.Remove(flags)) {
                        if (flags & fProcessAll) {
                            success = false;
                        } else {
                            throw "";
                        }
                    }
                }
            }
        }
        // Remove top directory
        if ((flags & fDir_Self) && NcbiSys_rmdir(_T_XCSTRING(GetPath())) != 0) {
            if ((flags & fIgnoreMissing) && (errno == ENOENT)) {
                return true;
            }
            if (flags & fProcessAll) {
                success = false;
            } else {
                throw "Cannot remove directory entry";
            }
        }
    }
    catch (const char* what) {
        // The error is may be reported already in child .Remove() for recursive calls,
        // but add an additional error log for the current directory itself. 
        LOG_ERROR(73, "CDir::Remove(): Cannot remove directory: " + GetPath() + ": " + what);
        return false;
    }
    return success;
}


bool CDir::SetMode(TMode user_mode,  TMode group_mode,
                   TMode other_mode, TSpecialModeBits special_mode,
                   TSetModeFlags flags) const
{
    // Assumption
    _ASSERT(fDir_Self == fEntry);
    _ASSERT(eEntryOnly == fEntry);

    // Default mode (backward compatibility) -- top entry only
    if ( (flags & (fDir_All | fDir_Recursive)) == eEntryOnly ) {
        return SetModeEntry(user_mode, group_mode, other_mode, special_mode, flags);
    }
    
    // Read all entries in directory
    unique_ptr<TEntries> contents(GetEntriesPtr());
    if (!contents.get()) {
        LOG_ERROR(74, "CDir::SetMode(): Cannot get content of: " + GetPath());
        return false;
    }

    bool success = true;
    try {
        // Process each entry
        ITERATE(TEntries, entry, *contents.get()) {
            string name = (*entry)->GetName();
            if (name == "." || name == ".." ||
                name == string(1, GetPathSeparator())) {
                continue;
            }
            // Get entry item with full pathname.
            CDirEntry item(GetPath() + GetPathSeparator() + name);
            if (flags & fDir_Recursive) {
                // Update flags to process subdirectories itself,
                // because the top directory entry may not have such flag.
                int f = (flags & fDir_Subdirs) ? (flags | fDir_Self) : flags;
                if (item.IsDir(eIgnoreLinks)) {
                    if (!CDir(item.GetPath()).SetMode(user_mode, group_mode, other_mode, special_mode, f)) {
                        if (flags & fProcessAll) {
                            success = false;
                        } else {
                            throw "Changing mode for subdirectory failed";
                        }
                    }
                }
                else if (flags & fDir_Files) {
                    if (!item.SetModeEntry(user_mode, group_mode, other_mode, special_mode, f)) {
                        if (flags & fProcessAll) {
                            success = false;
                        } else {
                            throw "Changing mode for subdirectory entry failed";
                        }
                    }
                }
            }
            else if (item.IsDir(eIgnoreLinks)) {
                // Non-recursive directory processing
                if (flags & fDir_Subdirs) {
                    // Clear all flags to go inside directory,
                    // and try to change modes for entry only.
                    if (!CDir(item.GetPath()).SetMode(user_mode, group_mode, other_mode, special_mode,
                        (flags & ~fDir_All) | fDir_Self)) {
                        if (flags & fProcessAll) {
                            success = false;
                        } else {
                            throw "Changing mode for subdirectory (non-recursive) failed";
                        }
                    }
                }
                continue;
            }
            else {
                if (flags & fDir_Files) {
                    if (!item.SetModeEntry(user_mode, group_mode, other_mode, special_mode, flags)) {
                        // Changing mode for a regular file entry failed
                        if (flags & fProcessAll) {
                            success = false;
                        } else {
                            throw "Changing mode for subdirectory entry failed";
                        }
                    }
                }
            }
        }
    }
    catch (const char* what) {
        // The error is may be reported already in child .SetMode() for recursive calls,
        // but add an additional error log for the current directory itself. 
        LOG_ERROR(94, "CDir::SetMode(): Cannot change mode for directory: " + GetPath() + ": " + what);
        return false;
    }

    // Process directory entry
    if (flags & fDir_Self) {
        // Change mode for entry/directory itself.
        // Clear all flags to disable to go inside directory, but allow to pass all additional flags.
        if (!SetModeEntry(user_mode, group_mode, other_mode, special_mode, (flags & ~fDir_All) | fEntry)) {
            success = false;
        };
    }
    return success;
}



//////////////////////////////////////////////////////////////////////////////
//
// CSymLink
//

CSymLink::~CSymLink(void)
{ 
    return;
}


bool CSymLink::Create(const string& path) const
{
#if defined(NCBI_OS_UNIX)
    char buf[PATH_MAX + 1];
    int len = (int)readlink(_T_XCSTRING(GetPath()), buf, sizeof(buf) - 1);
    if (len >= 0) {
        buf[len] = '\0';
        if (strcmp(buf, path.c_str()) == 0) {
            return true;
        }
    }
    // Leave it to the kernel to decide whether the symlink can be recreated
    if ( symlink(_T_XCSTRING(path), _T_XCSTRING(GetPath())) == 0 ) {
        return true;
    }
    LOG_ERROR_ERRNO(75, "CSymLink::Create(): failed: " + path);
#else
    LOG_ERROR_NCBI(76, "CSymLink::Create():"
                       " Symbolic links not supported on this platform: "
                       + path, CNcbiError::eNotSupported);
#endif
    return false;
}


bool CSymLink::Copy(const string& new_path, TCopyFlags flags, size_t buf_size) const
{
#if defined(NCBI_OS_UNIX)

    // Dereference link if specified
    if ( F_ISSET(flags, fCF_FollowLinks) ) {
        switch ( GetType(eFollowLinks) ) {
            case eFile:
                return CFile(*this).Copy(new_path, flags, buf_size);
            case eDir:
                return CDir(*this).Copy(new_path, flags, buf_size);
            case eLink:
                return CSymLink(*this).Copy(new_path, flags, buf_size);
            default:
                return CDirEntry(*this).Copy(new_path, flags, buf_size);
        }
        // not reached
    }

    // The source link must exists
    EType src_type = GetType(eIgnoreLinks);
    if ( src_type == eUnknown )  {
        LOG_ERROR_NCBI(77, "CSymLink::Copy(): Unknown entry type " + GetPath(),
                       CNcbiError::eNoSuchFileOrDirectory);
        return false;
    }

    CSymLink dst(new_path);
    EType    dst_type = dst.GetType(eIgnoreLinks);
    bool     dst_exists = (dst_type != eUnknown);
    string   dst_safe_path;  // saved path for fCF_Safe

    // If destination exists...
    if ( dst_exists ) {
        // Check on copying link into yourself.
        if ( IsIdentical(dst.GetPath()) ) {
            LOG_ERROR_NCBI(78, "CSymLink::Copy(): Source and destination are the same: " + GetPath(),
                           CNcbiError::eInvalidArgument);
            return false;
        }
        // Can copy entries with different types?
        if ( F_ISSET(flags, fCF_EqualTypes)  &&  (src_type != dst_type) ) {
            LOG_ERROR_NCBI(79, "CSymLink::Copy(): Cannot copy entries with different types: " + GetPath(),
                           CNcbiError::eOperationNotPermitted);
            return false;
        }
        // Can overwrite entry?
        if ( !F_ISSET(flags, fCF_Overwrite) ) {
            LOG_ERROR_NCBI(80, "CSymLink::Copy(): Destination already exists: " + dst.GetPath(),
                           CNcbiError::eOperationNotPermitted);
            return false;
        }
        // Copy only if destination is older
        if ( F_ISSET(flags, fCF_Update)  &&  !IsNewer(dst.GetPath(), 0)) {
            return true;
        }
        // Backup destination entry first
        if ( F_ISSET(flags, fCF_Backup) ) {
            // Use a new CDirEntry object for 'dst', because its path
            // will be changed after backup
            CDirEntry dst_tmp(dst);
            if ( !dst_tmp.Backup(GetBackupSuffix(), eBackup_Rename) ) {
                LOG_ERROR(81, "CSymLink::Copy(): Cannot backup destination: " + dst.GetPath());
                return false;
            }
        }
        // Overwrite destination entry
        if ( F_ISSET(flags, fCF_Overwrite) ) {
            dst.Remove();
        } 
    }
    // Safe copy -- create temporary symlink and rename later
    if (F_ISSET(flags, fCF_Safe)) {
        // Get new temporary name in the same directory
        string path, name, ext;
        SplitPath(dst.GetPath(), &path, &name, &ext);
        string tmp = GetTmpNameEx(path.empty() ? CDir::GetCwd() : path, name + ext + kTmpSafeSuffix);
        // Set new destination
        dst_safe_path = dst.GetPath();
        dst.Reset(tmp);
    }
    else {
        // Overwrite destination entry
        if (dst_exists  &&  F_ISSET(flags, fCF_Overwrite)) {
            dst.Remove();
        }
    }
    // Copy symbolic link (create new one)
    char buf[PATH_MAX + 1];
    int  len = (int)readlink(_T_XCSTRING(GetPath()), buf, sizeof(buf)-1);
    if (len < 1) {
        LOG_ERROR_ERRNO(82, "CSymLink::Copy(): Cannot read symbolic link: " + GetPath());
        return false;
    }
    buf[len] = '\0';
    if (symlink(buf, _T_XCSTRING(dst.GetPath()))) {
        LOG_ERROR_ERRNO(83, "CSymLink::Copy():"
                            " Cannot create symbolic link " + dst.GetPath() + 
                            " to " + string(buf));
        return false;
    }

    // Safe copy -- renaming
    if (F_ISSET(flags, fCF_Safe)) {
        if (!dst.Rename(dst_safe_path, fRF_Overwrite)) {
            dst.Remove();
            LOG_ERROR_NCBI(84, "CSymLink:Copy():"
                               " Cannot rename temporary symlink " + dst.GetPath() +
                               " to " + dst_safe_path, CNcbiError::eIoError);
            return false;
        }
    }
    // Preserve attributes
    if (flags & fCF_PreserveAll) {
        if (!s_CopyAttrs(GetPath().c_str(), new_path.c_str(), eLink, flags)) {
            LOG_ERROR(100, "CSymLink::Copy(): Cannot copy permissions from " + 
                          GetPath() + " to " + new_path);
            return false;
        }
    }
    return true;

#else
    // Windows -- regular copy
    return CParent::Copy(new_path, flags, buf_size);
#endif
}


//////////////////////////////////////////////////////////////////////////////
//
// CFileUtil
//

/// Flags to get information about file system.
/// Each flag corresponds to one or some fields in
/// the CFileUtil::SFileSystemInfo structure.
enum EFileSystemInfo {
    fFSI_Type        = (1<<1),    ///< fs_type
    fFSI_DiskSpace   = (1<<2),    ///< total_space, free_space
    fFSI_BlockSize   = (1<<3),    ///< block_size
    fFSI_FileNameMax = (1<<4),    ///< filename_max
    fFSI_All         = 0xFF       ///< get all possible information
};
typedef int TFileSystemInfo;      ///< Binary OR of "EFileSystemInfo"

// File system identification strings
struct SFileSystem {
    const char*                name;
    CFileUtil::EFileSystemType type;
};

// File system identification table
static const SFileSystem s_FileSystem[] = {
    { "ADFS",    CFileUtil::eADFS    },
    { "ADVFS",   CFileUtil::eAdvFS   },
    { "AFFS",    CFileUtil::eAFFS    },
    { "AUTOFS",  CFileUtil::eAUTOFS  },
    { "CACHEFS", CFileUtil::eCacheFS },
    { "CD9669",  CFileUtil::eCDFS    },
    { "CDFS",    CFileUtil::eCDFS    },
    { "DEVFS",   CFileUtil::eDEVFS   },
    { "DFS",     CFileUtil::eDFS     },
    { "DOS",     CFileUtil::eFAT     },
    { "EXT",     CFileUtil::eExt     },
    { "EXT2",    CFileUtil::eExt2    },
    { "EXT3",    CFileUtil::eExt3    },
    { "FAT",     CFileUtil::eFAT     },
    { "FAT32",   CFileUtil::eFAT32   },
    { "FDFS",    CFileUtil::eFDFS    },
    { "FFM",     CFileUtil::eFFM     },
    { "FFS",     CFileUtil::eFFS     },
    { "HFS",     CFileUtil::eHFS     },
    { "HSFS",    CFileUtil::eHSFS    },
    { "HPFS",    CFileUtil::eHPFS    },
    { "JFS",     CFileUtil::eJFS     },
    { "LOFS",    CFileUtil::eLOFS    },
    { "MFS",     CFileUtil::eMFS     },
    { "MSFS",    CFileUtil::eMSFS    },
    { "NFS",     CFileUtil::eNFS     },
    { "NFS2",    CFileUtil::eNFS     },
    { "NFSV2",   CFileUtil::eNFS     },
    { "NFS3",    CFileUtil::eNFS     },
    { "NFSV3",   CFileUtil::eNFS     },
    { "NFS4",    CFileUtil::eNFS     },
    { "NFSV4",   CFileUtil::eNFS     },
    { "NTFS",    CFileUtil::eNTFS    },
    { "PCFS",    CFileUtil::eFAT     },
    { "PROC",    CFileUtil::ePROC    },
    { "PROCFS",  CFileUtil::ePROC    },
    { "RFS",     CFileUtil::eRFS     },
    { "SMBFS",   CFileUtil::eSMBFS   },
    { "SPECFS",  CFileUtil::eSPECFS  },
    { "TMP",     CFileUtil::eTMPFS   },
    { "UFS",     CFileUtil::eUFS     },
    { "VXFS",    CFileUtil::eVxFS    },
    { "XFS",     CFileUtil::eXFS     }
};


// Macros to get filesystem status information

#define GET_STATVFS_INFO                                       \
    struct statvfs st;                                         \
    memset(&st, 0, sizeof(st));                                \
    if (statvfs(path.c_str(), &st) != 0) {                     \
        CNcbiError::SetFromErrno();                            \
        NCBI_THROW(CFileErrnoException, eFileSystemInfo, string(msg) + path); \
    }                                                          \
    info->total_space  = (Uint8)st.f_bsize * st.f_blocks;      \
    if (st.f_frsize) {                                         \
        info->free_space = (Uint8)st.f_frsize * st.f_bavail;   \
        info->block_size = (unsigned long)st.f_frsize;         \
    } else {                                                   \
        info->free_space = (Uint8)st.f_bsize * st.f_bavail;    \
        info->block_size = (unsigned long)st.f_bsize;          \
    }                                                          \
    info->used_space   = info->total_space - info->free_space


#define GET_STATFS_INFO                                        \
    struct statfs st;                                          \
    memset(&st, 0, sizeof(st));                                \
    if (statfs(path.c_str(), &st) != 0) {                      \
        CNcbiError::SetFromErrno();                            \
        NCBI_THROW(CFileErrnoException, eFileSystemInfo,  string(msg) + path); \
    }                                                          \
    info->total_space  = (Uint8)st.f_bsize * st.f_blocks;      \
    info->free_space   = (Uint8)st.f_bsize * st.f_bavail;      \
    info->used_space   = info->total_space - info->free_space; \
    info->block_size   = (unsigned long)st.f_bsize



#if defined(SUPPORT_PANFS)

// Auxiliary function to exit from forked process with reporting errno
// on errors to specified file descriptor 
static void s_PipeExit(int status, int fd)
{
    int errcode = errno;
    _no_warning(::write(fd, &errcode, sizeof(errcode)));
    ::close(fd);
    ::_exit(status);
}

// Close pipe handle
#define CLOSE_PIPE_END(fd) \
    if (fd != -1) {        \
        ::close(fd);       \
        fd = -1;           \
    }

// Standard kernel calls cannot get correct information 
// about PANFS mounts, so we use workaround for that.
//
// Use external method fist, if 'ncbi_panfs.so' exists and can be loaded.
// Fall back to 'pan_df' utuility (if present).
// Fall back to use standard OS info if none of above works.
//
void s_GetDiskSpace_PANFS(const string& path, CFileUtil::SFileSystemInfo* info)
{
    DEFINE_STATIC_FAST_MUTEX(s_Mutex);
    CFastMutexGuard guard_mutex(s_Mutex);

    // TRUE if initialization has done for EXE method
    static bool s_InitEXE  = false;
    static bool s_ExistEXE = false;
    
#if defined(ALLOW_USE_NCBI_PANFS_DLL)

    // TRUE if initialization has done for DLL method
    static bool s_InitDLL = false;
    static FGetDiskSpace_PANFS f_GetDiskSpace = NULL;
   
    if ( !s_InitDLL ) {
        s_InitDLL = true;

#  define _TOSTRING(x) #x
#  define  TOSTRING(x) _TOSTRING(x)
        const char* kNcbiPanfsDLL = "/opt/ncbi/" TOSTRING(NCBI_PLATFORM_BITS) "/lib/ncbi_panfs.so";
        
        // Check if 'ncbi_panfs.so' exists and can be loaded
        if ( CFile(kNcbiPanfsDLL).Exists() ) {
            void* handle = ::dlopen(kNcbiPanfsDLL, RTLD_NOW | RTLD_GLOBAL);
            const char* err = NULL;
            if ( handle ) {
                f_GetDiskSpace = (FGetDiskSpace_PANFS) ::dlsym(handle, "ncbi_GetDiskSpace_PANFS");
                if ( !f_GetDiskSpace ) {
                    err = "Undefined symbol";
                }
            } else {
                err = "Cannot open shared object file";
            }
            if ( err ) {
                char* dlerr = dlerror();
                string msg = "Trying to get ncbi_GetDiskSpace_PANFS() function from '" +
                             string(kNcbiPanfsDLL) + "': " + err;
                if ( dlerr ) {
                    msg = msg + " (" + dlerr + ")";
                }
                LOG_ERROR_NCBI(85, msg, CNcbiError::eUnknown);
                if ( handle) {
                    dlclose(handle);
                }
            }
        }
    }
   
    if ( f_GetDiskSpace ) {
        const char* err_msg = NULL;
        bool do_throw = false;
        
        int res = f_GetDiskSpace(path.c_str(), &info->total_space, &info->free_space, &err_msg);
        switch ( res ) {

        case NCBI_PANFS_OK:
            info->used_space = info->total_space - info->free_space;
            // All done, return
            return;

        case NCBI_PANFS_THROW:
             do_throw = true;
             /*FALLTHRU*/
             
        // Same processing for all errors codes, but could be detailed
        case NCBI_PANFS_ERR:
        case NCBI_PANFS_ERR_OPEN:
        case NCBI_PANFS_ERR_QUERY:
        case NCBI_PANFS_ERR_VERSION:
        default:
            {
                string msg = "Cannot get information for PANFS mount '"+ path + "'";
                if ( err_msg ) {
                    msg += string(": ") + err_msg;
                }
                if ( do_throw ) {
                    NCBI_THROW(CFileException, eFileSystemInfo, msg);
                }
                LOG_ERROR_NCBI(86, msg, CNcbiError::eUnknown);
            }
        }
    }
#endif // defined(ALLOW_USE_NCBI_PANFS_DLL)


    // Cannot use DLL, so -- fall through to use pan_df.

    // -----------------------------------------
    // Call 'pan_df' utility and parse results
    // -----------------------------------------

    const char* kPanDF = "/opt/panfs/bin/pan_df";

    if ( !s_InitEXE ) {
        s_InitEXE = true;
        // Check if 'pan_df' exists
        if ( CFile(kPanDF).Exists() ) {
            s_ExistEXE = true;
        }
    }
    
    if ( s_ExistEXE ) {
        // Child process I/O handles
        int status_pipe[2] = {-1,-1};
        int pipe_fd[2]     = {-1,-1};
        
        try {
            ::fflush(NULL);
            // Create pipe for child's stdout
            if (::pipe(pipe_fd) < 0) {
                throw "failed to create pipe for stdout";
            }
            // Create temporary pipe to get status of execution
            // of the child process
            if (::pipe(status_pipe) < 0) {
                throw "failed to create status pipe";
            }
            if (::fcntl(status_pipe[1], F_SETFD, 
                ::fcntl(status_pipe[1], F_GETFD, 0) | FD_CLOEXEC) < 0) {
                throw "failed to set close-on-exec mode for status pipe";
            }

            // Fork child process
            pid_t pid = ::fork();
            if (pid == -1) {
                throw "fork() failed";
            }
            if (pid == 0) {
                // -- Now we are in the child process

                // Close unused pipe handle
                ::close(status_pipe[0]);
                // stdin/stderr -- don't use
                _no_warning(::freopen("/dev/null", "r", stdin));
                _no_warning(::freopen("/dev/null", "a", stderr));
                // stdout
                if (pipe_fd[1] != STDOUT_FILENO) {
                    if (::dup2(pipe_fd[1], STDOUT_FILENO) < 0) {
                        s_PipeExit(-1, status_pipe[1]);
                    }
                    ::close(pipe_fd[1]);
                }
                ::close(pipe_fd[0]);
                int status = ::execl(kPanDF, kPanDF, "--block-size=1", path.c_str(), NULL);
                s_PipeExit(status, status_pipe[1]);

                // -- End of child process
            }
        
            // Close unused pipes' ends
            CLOSE_PIPE_END(pipe_fd[1]);
            CLOSE_PIPE_END(status_pipe[1]);

            // Check status pipe.
            // If it have some data, this is an errno from the child process.
            // If EOF in status pipe, that child executed successful.
            // Retry if either blocked or interrupted

            // Try to read errno from forked process
            ssize_t n;
            int errcode;
            while ((n = read(status_pipe[0], &errcode, sizeof(errcode))) < 0) {
                if (errno != EINTR)
                    break;
            }
            CLOSE_PIPE_END(status_pipe[0]);
            if (n > 0) {
                // Child could not run -- reap it and exit with error
                ::waitpid(pid, 0, 0);
                errno = (size_t) n >= sizeof(errcode) ? errcode : 0;
                throw "failed to run pan_df";
            }

            // Read data from pipe
            char buf[1024];
            while ((n = read(pipe_fd[0], &buf, sizeof(buf)-1)) < 0) {
                if (errno != EINTR)
                    break;
            }
            CLOSE_PIPE_END(pipe_fd[0]);
            if ( !n ) {
                throw "error reading from pipe";
            }
            buf[n] = '\0';
            
            // Parse resilt
            const char* kParseError = "results parse error";
            const char* data = strchr(buf, '\n');
            if ( !data ) {
                throw kParseError;
            }
            vector<string> tokens;
            NStr::Split(data + 1, " ", tokens, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
            if ( tokens.size() != 6 ) {
                throw kParseError;
            }
            Uint8 x_total = 1, x_free = 2, x_used = 3; // dummy values
            try {
                x_total = NStr::StringToUInt8(tokens[1]);
                x_free  = NStr::StringToUInt8(tokens[2]);
                x_used  = NStr::StringToUInt8(tokens[3]);
            }
            catch (const CException& e) {
                throw kParseError;
            }
            // Check
            if ( x_free + x_used != x_total ) {
                throw kParseError;
            }
            info->total_space = x_total;
            info->free_space  = x_free;
            info->used_space  = x_used;
            return;
        }
        catch (const char* what) {
            CLOSE_PIPE_END(pipe_fd[0]);
            CLOSE_PIPE_END(pipe_fd[1]);
            CLOSE_PIPE_END(status_pipe[0]);
            CLOSE_PIPE_END(status_pipe[1]);
            ERR_POST_X_ONCE(3, Warning << "Failed to use 'pan_df': " << what);
        }           
    } // if ( s_ExistEXE ) 
    
    // Failed
    ERR_POST_X_ONCE(3, Warning << 
                    "Cannot use any external method to get information about "
                    "PANFS mount, fall back to use standard OS info "
                    "(NOTE: it can be incorrect)");
    return;
}

#endif // defined(SUPPORT_PANFS)



void s_GetFileSystemInfo(const string&               path,
                         CFileUtil::SFileSystemInfo* info,
                         TFileSystemInfo             flags)
{
    if ( !info ) {
        NCBI_THROW(CCoreException, eInvalidArg,
                   "s_GetFileSystemInfo(path, NULL) is not allowed");
    }
    memset(info, 0, sizeof(*info));
    const char* msg = "Cannot get system information for ";
    const char* fs_name_ptr = 0;

#if defined(NCBI_OS_MSWIN)
    // Try to get a root disk directory from given path
    string xpath = path;
    // Not UNC path
    if (!s_Win_IsNetworkPath(path)) {
        if ( !isalpha((unsigned char)path[0]) || path[1] != DISK_SEPARATOR ) {
            // absolute or relative path without disk name -- current disk,
            // dir entry should exists
            if ( CDirEntry(path).Exists() ) {
                xpath = CDir::GetCwd();
            }
        }
        // Get disk root directory name from the path
        xpath[2] = '\\';
        xpath.resize(3);
    }

    // Get volume information
    TXChar fs_name[MAX_PATH+1];
    string ufs_name;
    if (flags & (fFSI_Type | fFSI_FileNameMax))  {
        DWORD filename_max;
        DWORD fs_flags;

        if ( !::GetVolumeInformation(_T_XCSTRING(xpath),
                                     NULL, 0, // Name of the volume
                                     NULL,    // and its serial number
                                     &filename_max,
                                     &fs_flags,
                                     fs_name,
                                     sizeof(fs_name)/sizeof(fs_name[0])) ) {
            NCBI_THROW(CFileErrnoException, eFileSystemInfo, string(msg) + path);
        }
        info->filename_max = filename_max;
        ufs_name = _T_CSTRING(fs_name);
        fs_name_ptr = ufs_name.c_str();
    }
        
    // Get disk spaces
    if (flags & fFSI_DiskSpace) {
        if ( !::GetDiskFreeSpaceEx(_T_XCSTRING(xpath),
                                   (PULARGE_INTEGER)&info->free_space,
                                   (PULARGE_INTEGER)&info->total_space, 0) ) {
            NCBI_THROW(CFileErrnoException, eFileSystemInfo, string(msg) + path);
        }
    }

    // Get volume cluster size
    if (flags & fFSI_BlockSize) {
        DWORD dwSectPerClust; 
        DWORD dwBytesPerSect;
        if ( !::GetDiskFreeSpace(_T_XCSTRING(xpath),
                                 &dwSectPerClust, &dwBytesPerSect,
                                 NULL, NULL) ) {
            NCBI_THROW(CFileErrnoException, eFileSystemInfo, string(msg) + path);
        }
        info->block_size = dwBytesPerSect * dwSectPerClust;
    }

#else // defined(NCBI_OS_MSWIN)

    bool need_name_max = true;
#  ifdef _PC_NAME_MAX
    long r_name_max = pathconf(path.c_str(), _PC_NAME_MAX);
    if (r_name_max != -1) {
        info->filename_max = (unsigned long)r_name_max;
        need_name_max = false;
    }
#  endif

#  if (defined(NCBI_OS_LINUX) || defined(NCBI_OS_CYGWIN))  &&  defined(HAVE_STATFS)
    
    GET_STATFS_INFO;
    if (flags & (fFSI_Type | fFSI_DiskSpace)) {
        switch (st.f_type) {
            case 0xADF5:      info->fs_type = CFileUtil::eADFS;     break;
            case 0xADFF:      info->fs_type = CFileUtil::eAFFS;     break;
            case 0x5346414F:  info->fs_type = CFileUtil::eAFS;      break;
            case 0x0187:      info->fs_type = CFileUtil::eAUTOFS;   break;
            case 0x1BADFACE:  info->fs_type = CFileUtil::eBFS;      break;
            case 0x4004:
            case 0x4000:
            case 0x9660:      info->fs_type = CFileUtil::eCDFS;     break;
            case 0xF15F:      info->fs_type = CFileUtil::eCryptFS;  break;
            case 0xFF534D42:  info->fs_type = CFileUtil::eCIFS;     break;
            case 0x73757245:  info->fs_type = CFileUtil::eCODA;     break;
            case 0x012FF7B7:  info->fs_type = CFileUtil::eCOH;      break;
            case 0x28CD3D45:  info->fs_type = CFileUtil::eCRAMFS;   break;
            case 0x1373:      info->fs_type = CFileUtil::eDEVFS;    break;
            case 0x414A53:    info->fs_type = CFileUtil::eEFS;      break;
            case 0x5DF5:      info->fs_type = CFileUtil::eEXOFS;    break;
            case 0x137D:      info->fs_type = CFileUtil::eExt;      break;
            case 0xEF51:
            case 0xEF53:      info->fs_type = CFileUtil::eExt2;     break;
            case 0x4d44:      info->fs_type = CFileUtil::eFAT;      break;
            case 0x65735546:  info->fs_type = CFileUtil::eFUSE;     break;
            case 0x65735543:  info->fs_type = CFileUtil::eFUSE_CTL; break;
            case 0x01161970:  info->fs_type = CFileUtil::eGFS2;     break;
            case 0x4244:      info->fs_type = CFileUtil::eHFS;      break;
            case 0x482B:      info->fs_type = CFileUtil::eHFSPLUS;  break;
            case 0xF995E849:  info->fs_type = CFileUtil::eHPFS;     break;
            case 0x3153464A:  info->fs_type = CFileUtil::eJFS;      break;
            case 0x07C0:      info->fs_type = CFileUtil::eJFFS;     break;
            case 0x72B6:      info->fs_type = CFileUtil::eJFFS2;    break;
            case 0x47504653:  info->fs_type = CFileUtil::eGPFS;     break;
            case 0x137F:
            case 0x138F:      info->fs_type = CFileUtil::eMinix;    break;
            case 0x2468:
            case 0x2478:      info->fs_type = CFileUtil::eMinix2;   break;
            case 0x4D5A:      info->fs_type = CFileUtil::eMinix3;   break;
            case 0x564C:      info->fs_type = CFileUtil::eNCPFS;    break;
            case 0x6969:      info->fs_type = CFileUtil::eNFS;      break;
            case 0x5346544E:  info->fs_type = CFileUtil::eNTFS;     break;
            case 0x7461636F:  info->fs_type = CFileUtil::eOCFS2;    break;
            case 0x9fA1:      info->fs_type = CFileUtil::eOPENPROM; break;
            case 0xAAD7AAEA:  info->fs_type = CFileUtil::ePANFS;    break;
            case 0x9fA0:      info->fs_type = CFileUtil::ePROC;     break;
            case 0x20030528:  info->fs_type = CFileUtil::ePVFS2;    break;
            case 0x002F:      info->fs_type = CFileUtil::eQNX4;     break;
            case 0x52654973:  info->fs_type = CFileUtil::eReiserFS; break;
            case 0x7275:      info->fs_type = CFileUtil::eROMFS;    break;
            case 0xF97CFF8C:  info->fs_type = CFileUtil::eSELINUX;  break;
            case 0x517B:      info->fs_type = CFileUtil::eSMBFS;    break;
            case 0x73717368:  info->fs_type = CFileUtil::eSquashFS; break;
            case 0x62656572:  info->fs_type = CFileUtil::eSYSFS;    break;
            case 0x012FF7B6:  info->fs_type = CFileUtil::eSYSV2;    break;
            case 0x012FF7B5:  info->fs_type = CFileUtil::eSYSV4;    break;
            case 0x01021994:  info->fs_type = CFileUtil::eTMPFS;    break;
            case 0x24051905:  info->fs_type = CFileUtil::eUBIFS;    break;
            case 0x15013346:  info->fs_type = CFileUtil::eUDF;      break;
            case 0x00011954:  info->fs_type = CFileUtil::eUFS;      break;
            case 0x19540119:  info->fs_type = CFileUtil::eUFS2;     break;
            case 0x9fA2:      info->fs_type = CFileUtil::eUSBDEVICE;break;
            case 0x012FF7B8:  info->fs_type = CFileUtil::eV7;       break;
            case 0xa501FCF5:  info->fs_type = CFileUtil::eVxFS;     break;
            case 0x565a4653:  info->fs_type = CFileUtil::eVZFS;     break;
            case 0x012FF7B4:  info->fs_type = CFileUtil::eXENIX;    break;
            case 0x58465342:  info->fs_type = CFileUtil::eXFS;      break;
            case 0x012FD16D:  info->fs_type = CFileUtil::eXIAFS;    break;
            default:          info->fs_type = CFileUtil::eUnknown;  break;
        }
    }
    if (need_name_max) {
        info->filename_max = (unsigned long)st.f_namelen;
    }

#  elif (defined(NCBI_OS_SOLARIS) || defined(NCBI_OS_IRIX) || defined(NCBI_OS_OSF1)) \
         &&  defined(HAVE_STATVFS)

    GET_STATVFS_INFO;
    if (need_name_max) {
        info->filename_max = (unsigned long)st.f_namemax;
    }
    fs_name_ptr = st.f_basetype;

#  elif defined(NCBI_OS_DARWIN)  &&  defined(HAVE_STATFS)

    GET_STATFS_INFO;
    // Seems statfs structure on Darwin doesn't have any information 
    // about name length, so rely on pathconf() only (see above).
    // empty if - to avoid compilation warning on defined but unused variable
    if (need_name_max) {
    //    info->filename_max = (unsigned long)st.f_namelen;
    }
    fs_name_ptr = st.f_fstypename;

#  elif defined(NCBI_OS_BSD)  &&  defined(HAVE_STATFS)

    GET_STATFS_INFO;
    fs_name_ptr = st.f_fstypename;
    if (need_name_max) {
        info->filename_max = (unsigned long)st.f_namemax;
    }

#  elif defined(NCBI_OS_OSF1)  &&  defined(HAVE_STATVFS)

    GET_STATVFS_INFO;
    if (need_name_max) {
        info->filename_max = (unsigned long)st.f_namelen;
    }
    fs_name_ptr = st.f_fstypename;

#  else

     // Unknown UNIX OS
    #if defined(HAVE_STATVFS)
        GET_STATVFS_INFO;
    #elif defined(HAVE_STATFS)
        GET_STATFS_INFO;
    #endif

#  endif
#endif

    // Try to define file system type by name
    if ((flags & fFSI_Type)  &&  fs_name_ptr) {
        for (size_t i=0; 
             i < sizeof(s_FileSystem)/sizeof(s_FileSystem[0]); i++) {
            if ( NStr::EqualNocase(fs_name_ptr, s_FileSystem[i].name) ) {
                info->fs_type = s_FileSystem[i].type;
                break;
            }
        }
    }

#if defined(SUPPORT_PANFS)
    // Standard kernel calls cannot get correct information 
    // about PANFS mounts, so we use workaround for that.
    if ((info->fs_type == CFileUtil::ePANFS) && (flags & fFSI_DiskSpace)) {
        s_GetDiskSpace_PANFS(path, info);
    }
#endif
}


void CFileUtil::GetFileSystemInfo(const string& path, CFileUtil::SFileSystemInfo* info)
{
    s_GetFileSystemInfo(path, info, fFSI_All);
}


Uint8 CFileUtil::GetFreeDiskSpace(const string& path)
{
    SFileSystemInfo info;
    s_GetFileSystemInfo(path, &info, fFSI_DiskSpace);
    return info.free_space;
}


Uint8 CFileUtil::GetUsedDiskSpace(const string& path)
{
    SFileSystemInfo info;
    s_GetFileSystemInfo(path, &info, fFSI_DiskSpace);
    return info.used_space;
}


Uint8 CFileUtil::GetTotalDiskSpace(const string& path)
{
    SFileSystemInfo info;
    s_GetFileSystemInfo(path, &info, fFSI_DiskSpace);
    return info.total_space;
}



//////////////////////////////////////////////////////////////////////////////
//
// CFileDeleteList / CFileDeleteAtExit
//

CFileDeleteList::~CFileDeleteList()
{
    ITERATE (TList, path, m_Paths) {
        if (!CDirEntry(*path).Remove(CDirEntry::eRecursiveIgnoreMissing)) {
            ERR_POST_X(5, Warning << "CFileDeleteList: failed to remove path: " << *path);
        }
    }
}

void CFileDeleteAtExit::Add(const string& path)
{
    s_DeleteAtExitFileList->Add(path);
}

const CFileDeleteList& CFileDeleteAtExit::GetDeleteList(void)
{
    return *s_DeleteAtExitFileList;
}

void CFileDeleteAtExit::SetDeleteList(CFileDeleteList& list)
{
    *s_DeleteAtExitFileList = list;
}


//////////////////////////////////////////////////////////////////////////////
//
// CTmpFile
//


CTmpFile::CTmpFile(ERemoveMode remove_file)
{
    m_FileName = CFile::GetTmpName();
    if ( m_FileName.empty() ) {
        NCBI_THROW(CFileException, eTmpFile, "Cannot generate temporary file name");
    }
    m_RemoveOnDestruction = remove_file;
}

CTmpFile::CTmpFile(const string& file_name, ERemoveMode remove_file)
    : m_FileName(file_name), 
      m_RemoveOnDestruction(remove_file)
{
    return;
}

CTmpFile::~CTmpFile()
{
    // First, close and delete created streams.
    m_InFile.reset();
    m_OutFile.reset();

    // Remove file if specified
    if (m_RemoveOnDestruction == eRemove) {
        NcbiSys_unlink(_T_XCSTRING(m_FileName));
    }
}

    enum EIfExists {
        /// You can make call of AsInputFile/AsOutputFile only once,
        /// on each following call throws CFileException exception.
        eIfExists_Throw,
        /// Delete previous stream and return reference to new object.
        eIfExists_Reset,
        /// Return reference to current stream, or new if this is first call.
        eIfExists_ReturnCurrent
    };

    // CTmpFile

const string& CTmpFile::GetFileName(void) const
{
    return m_FileName;
}


CNcbiIstream& CTmpFile::AsInputFile(EIfExists if_exists, IOS_BASE::openmode mode)
{
    if ( m_InFile.get() ) {
        switch (if_exists) {
        case eIfExists_Throw:
            NCBI_THROW(CFileException, eTmpFile, "AsInputFile() is already called");
            /*NOTREACHED*/
            break;
        case eIfExists_Reset:
            // see below
            break;
        case eIfExists_ReturnCurrent:
            return *m_InFile;
        }
    }
    m_InFile.reset(new CNcbiIfstream(_T_XCSTRING(m_FileName), IOS_BASE::in | mode));
    return *m_InFile;
}


CNcbiOstream& CTmpFile::AsOutputFile(EIfExists if_exists, IOS_BASE::openmode mode)
{
    if ( m_OutFile.get() ) {
        switch (if_exists) {
        case eIfExists_Throw:
            NCBI_THROW(CFileException, eTmpFile, "AsOutputFile() is already called");
            /*NOTREACHED*/
            break;
        case eIfExists_Reset:
            // see below
            break;
        case eIfExists_ReturnCurrent:
            return *m_OutFile;
        }
    }
    m_OutFile.reset(new CNcbiOfstream(_T_XCSTRING(m_FileName), IOS_BASE::out | mode));
    return *m_OutFile;
}


//////////////////////////////////////////////////////////////////////////////
//
// CMemoryFile
//

// Platform-dependent memory file handle definition
struct SMemoryFileHandle {
#if defined(NCBI_OS_MSWIN)
    HANDLE  hMap;   // File-mapping handle (see ::[Open/Create]FileMapping())
#else /* UNIX */
    int     hMap;   // File handle
#endif
    string  sFileName;
};

// Platform-dependent memory file attributes
struct SMemoryFileAttrs {
#if defined(NCBI_OS_MSWIN)
    DWORD map_protect;
    DWORD map_access;
    DWORD file_share;
    DWORD file_access;
#else
    int   map_protect;
    int   map_access;
    int   file_access;
#endif
};


// Translate memory mapping attributes into OS specific flags.
static SMemoryFileAttrs*
s_TranslateAttrs(CMemoryFile_Base::EMemMapProtect protect_attr, 
                 CMemoryFile_Base::EMemMapShare   share_attr)
{
    SMemoryFileAttrs* attrs = new SMemoryFileAttrs();
    memset(attrs, 0, sizeof(SMemoryFileAttrs));

#if defined(NCBI_OS_MSWIN)

    switch (protect_attr) {
        case CMemoryFile_Base::eMMP_Read:
            attrs->map_access  = FILE_MAP_READ;
            attrs->map_protect = PAGE_READONLY;
            attrs->file_access = GENERIC_READ;
            break;
        case CMemoryFile_Base::eMMP_Write:
        case CMemoryFile_Base::eMMP_ReadWrite:
            // On MS Windows platform Write & ReadWrite access
            // to the mapped memory is equivalent
            if  (share_attr == CMemoryFile_Base::eMMS_Shared ) {
                attrs->map_access = FILE_MAP_ALL_ACCESS;
            } else {
                attrs->map_access = FILE_MAP_COPY;
            }
            attrs->map_protect = PAGE_READWRITE;
            // So the file also must be open for reading and writing
            attrs->file_access = GENERIC_READ | GENERIC_WRITE;
            break;
        default:
            _TROUBLE;
    }
    if ( share_attr == CMemoryFile_Base::eMMS_Shared ) {
        attrs->file_share = FILE_SHARE_READ | FILE_SHARE_WRITE;
    } else {
        attrs->file_share = FILE_SHARE_READ;
    }

#elif defined(NCBI_OS_UNIX)

    switch (share_attr) {
        case CMemoryFile_Base::eMMS_Shared:
            attrs->map_access  = MAP_SHARED;
            // Read + write except, eMMP_Read mode
            attrs->file_access = O_RDWR;
            break;
        case CMemoryFile_Base::eMMS_Private:
            attrs->map_access  = MAP_PRIVATE;
            // In the private mode writing to the mapped region
            // do not affect the original file, so we can open it
            // in the read-only mode.
            attrs->file_access = O_RDONLY;
            break;
        default:
            _TROUBLE;
    }
    switch (protect_attr) {
        case CMemoryFile_Base::eMMP_Read:
            attrs->map_protect = PROT_READ;
            attrs->file_access = O_RDONLY;
            break;
        case CMemoryFile_Base::eMMP_Write:
            attrs->map_protect = PROT_WRITE;
            break;
        case CMemoryFile_Base::eMMP_ReadWrite:
            attrs->map_protect = PROT_READ | PROT_WRITE;
            break;
        default:
            _TROUBLE;
    }

#endif
    return attrs;
}


CMemoryFile_Base::CMemoryFile_Base(void)
{
    // Check if memory-mapping is supported on this platform
    if ( !IsSupported() ) {
        NCBI_THROW(CFileException, eMemoryMap,
                   "Memory-mapping is not supported by the C++ Toolkit on this platform");
    }
}


bool CMemoryFile_Base::IsSupported(void)
{
#if defined(NCBI_OS_MSWIN)  ||  defined(NCBI_OS_UNIX)
    return true;
#else
    return false;
#endif
}


CMemoryFileSegment::CMemoryFileSegment(SMemoryFileHandle& handle,
                                       SMemoryFileAttrs&  attrs,
                                       TOffsetType        offset,
                                       size_t             length)
    : m_DataPtr(0), m_Offset(offset), m_Length(length),
      m_DataPtrReal(0), m_OffsetReal(offset), m_LengthReal(length)
{
    if ( m_Offset < 0 ) {
        NCBI_THROW(CFileException, eMemoryMap,
                   "File offset may not be negative");
    }
    if ( !m_Length ) {
        NCBI_THROW(CFileException, eMemoryMap,
                   "File mapping region size must be greater than 0");
    }
    // Get system's memory allocation granularity.
    // Note, GetVirtualMemoryAllocationGranularity() cache it. 
    unsigned long vm_gran = CSystemInfo::GetVirtualMemoryAllocationGranularity();

    if ( !vm_gran ) {
        NCBI_THROW(CFileException, eMemoryMap,
                   "Cannot determine virtual memory allocation granularity");
    }
    // Adjust mapped length and offset.
    if ( m_Offset % vm_gran ) {
        m_OffsetReal -= m_Offset % vm_gran;
        m_LengthReal += m_Offset % vm_gran;
    }
    // Map file view to memory
    string errmsg;

#if defined(NCBI_OS_MSWIN)
    DWORD offset_hi  = DWORD(Int8(m_OffsetReal) >> 32);
    DWORD offset_low = DWORD(Int8(m_OffsetReal) & 0xFFFFFFFF);
    m_DataPtrReal = ::MapViewOfFile(handle.hMap, attrs.map_access, offset_hi, offset_low, m_LengthReal);
    if ( !m_DataPtrReal ) {
        errmsg = WIN_LAST_ERROR_STR;
    }

#elif defined(NCBI_OS_UNIX)
    errno = 0;
    m_DataPtrReal = mmap(0, m_LengthReal, attrs.map_protect,
                         attrs.map_access, handle.hMap, m_OffsetReal);
    if ( m_DataPtrReal == MAP_FAILED ) {
        m_DataPtrReal = 0;
        errmsg = NcbiSys_strerror(errno);
    }
#endif
    if ( !m_DataPtrReal ) {
        NCBI_THROW(CFileException, eMemoryMap,
                   "Cannot map file '" + 
                   handle.sFileName + "' to memory (offset=" +
                   NStr::Int8ToString(m_Offset) + ", length=" +
                   NStr::Int8ToString(m_Length) + "): " + errmsg);
    }
    // Calculate user's pointer to data
    m_DataPtr = (char*)m_DataPtrReal + (m_Offset - m_OffsetReal);
}


CMemoryFileSegment::~CMemoryFileSegment(void)
{
    Unmap();
}


bool CMemoryFileSegment::Flush(void) const
{
    if ( !m_DataPtr ) {
        CNcbiError::Set(CNcbiError::eBadAddress);
        return false;
    }
    bool status;
#if defined(NCBI_OS_MSWIN)
    status = (::FlushViewOfFile(m_DataPtrReal, m_LengthReal) != 0);
    if ( !status ) {
        LOG_ERROR_WIN(87, "CMemoryFileSegment::Flush(): Cannot flush memory segment");
        return false;
    }
#elif defined(NCBI_OS_UNIX)
    status = (msync((char*)m_DataPtrReal, m_LengthReal, MS_SYNC) == 0);
    if ( !status ) {
        LOG_ERROR_ERRNO(87, "CMemoryFileSegment::Flush(): Cannot flush memory segment");
        return false;
    }
#endif
    return status;
}


bool CMemoryFileSegment::Unmap(void)
{
    // If file view is not mapped do nothing
    if ( !m_DataPtr ) {
        CNcbiError::Set(CNcbiError::eBadAddress);
        return true;
    }
    bool status;
#if defined(NCBI_OS_MSWIN)
    status = (::UnmapViewOfFile(m_DataPtrReal) != 0);
    if (!status) {
        LOG_ERROR_WIN(88, "CMemoryFileSegment::Unmap(): Cannot unmap memory segment");
        return false;
    }
#elif defined(NCBI_OS_UNIX)
    status = (munmap((char*)m_DataPtrReal, (size_t) m_LengthReal) == 0);
    if (!status) {
        LOG_ERROR_ERRNO(88, "CMemoryFileSegment::Unmap(): Cannot unmap memory segment");
        return false;
    }
#endif
    if ( status ) {
        m_DataPtr = 0;
    }
    return status;
}


void CMemoryFileSegment::x_Verify(void) const
{
    if ( m_DataPtr ) {
        return;
    }
    NCBI_THROW(CFileException, eMemoryMap, "File not mapped");
}


CMemoryFileMap::CMemoryFileMap(const string&  file_name,
                               EMemMapProtect protect,
                               EMemMapShare   share,
                               EOpenMode      mode,
                               Uint8          max_file_len)
    : m_FileName(file_name), m_Handle(0), m_Attrs(0)
{
#if defined(NCBI_OS_MSWIN)
    // Name of a file-mapping object cannot contain '\'
    NStr::ReplaceInPlace(m_FileName, "\\", "/");
#endif

    // Translate attributes 
    m_Attrs = s_TranslateAttrs(protect, share);

    // Create file if necessary
    if ( mode == eCreate ) {
        x_Create(max_file_len);
    }
    // Check file size
    Int8 file_size = GetFileSize();
    if ( file_size < 0 ) {
        if ( m_Attrs ) {
            delete m_Attrs;
            m_Attrs = 0;
        }
        NCBI_THROW(CFileException, eMemoryMap,
                   "To be memory mapped the file must exist: '" + m_FileName +"'");
    }
    // Extend file size if necessary
    if ( mode == eExtend  &&  max_file_len > (Uint8)file_size) {
        x_Extend(file_size, max_file_len);
        file_size = (Int8)max_file_len;
    }

    // Open file
    if ( file_size == 0 ) {
        // Special case -- file is empty
        m_Handle = new SMemoryFileHandle();
        m_Handle->hMap = kInvalidHandle;
        m_Handle->sFileName = m_FileName;
        return;
    }
    x_Open();
}


CMemoryFileMap::~CMemoryFileMap(void)
{
    // Unmap used memory and close file
    x_Close();
    // Clean up allocated memory
    if ( m_Attrs ) {
        delete m_Attrs;
    }
}


void* CMemoryFileMap::Map(TOffsetType offset, size_t length)
{
    if ( !m_Handle  ||  (m_Handle->hMap == kInvalidHandle) ) {
        // Special case.
        // Always return 0 if a file is unmapped or have zero length.
        return 0;
    }
    // Map file wholly if the length of the mapped region is not specified
    if ( !length ) {
        Int8 file_size = GetFileSize() - offset;
        if ( (Uint8)file_size > get_limits(length).max() ) {
            NCBI_THROW(CFileException, eMemoryMap,
                       "File too big for memory mapping "   \
                       "(file '" + m_FileName +"', "
                       "offset=" + NStr::Int8ToString(offset) + ", "    \
                       "length=" + NStr::Int8ToString(length) + ")");
        } else if ( file_size > 0 ) {
            length = (size_t)file_size;
        } else {
            NCBI_THROW(CFileException, eMemoryMap,
                       "Mapping region offset specified beyond file size");
        }
    }
    // Map file segment
    CMemoryFileSegment* segment =  new CMemoryFileSegment(*m_Handle, *m_Attrs, offset, length);
    void* ptr = segment->GetPtr();
    if ( !ptr ) {
        delete segment;
        NCBI_THROW(CFileException, eMemoryMap,
                   "Cannot map (file '" + m_FileName +"', "
                   "offset=" + NStr::Int8ToString(offset) + ", "    \
                   "length=" + NStr::Int8ToString(length) + ")");
    }
    m_Segments[ptr] = segment;
    return ptr;
}


bool CMemoryFileMap::Unmap(void* ptr)
{
    // Unmap mapped view of a file
    bool status = false;
    TSegments::iterator segment = m_Segments.find(ptr);
    if ( segment != m_Segments.end() ) {
        status = segment->second->Unmap();
        if ( status ) {
            delete segment->second;
            m_Segments.erase(segment);
        }
    }
    if ( !status ) {
        LOG_ERROR(89, "CMemoryFileMap::Unmap(): Memory segment not found");
        return false;
    }
    return status;
}


bool CMemoryFileMap::UnmapAll(void)
{
    bool status = true;
    void* key_to_delete = 0;
    ITERATE(TSegments, it, m_Segments) {
        if ( key_to_delete ) {
            m_Segments.erase(key_to_delete);
        }
        bool unmapped = it->second->Unmap();
        if ( status ) {
            status = unmapped;
        }
        if ( unmapped ) {
            key_to_delete = it->first;
            delete it->second;
        } else {
            key_to_delete = 0;
        }
    }
    if ( key_to_delete ) {
        m_Segments.erase(key_to_delete);
    }
    if ( !status ) {
        LOG_ERROR(89, "CMemoryFileMap::UnmapAll(): Memory segment not found");
        return false;
    }
    return status;
}


Int8 CMemoryFileMap::GetFileSize(void) const
{
    // On Unix -- mapping handle is the same as file handle,
    // use it to get file size if file is already opened.
    #if defined(NCBI_OS_UNIX)
        if ( m_Handle  &&  (m_Handle->hMap != kInvalidHandle) ) {
            TNcbiSys_fstat st;
            if ( NcbiSys_fstat(m_Handle->hMap, &st) != 0 ) {
                LOG_ERROR_ERRNO(101, "CMemoryFileMap::GetFileSize(): unable to get file size of the mapped file: " + m_FileName);
                return -1L;
            }
            return st.st_size;
        }
    #endif
    // On Windows -- file handle is already closed, use file name. 
    return CFile(m_FileName).GetLength();
}


void CMemoryFileMap::x_Open(void)
{
    m_Handle = new SMemoryFileHandle();
    m_Handle->hMap = kInvalidHandle;
    m_Handle->sFileName = m_FileName;

    string errmsg;

    for (;;) { // quasi-TRY block

#if defined(NCBI_OS_MSWIN)
        errmsg = ": ";
        TXString filename(_T_XSTRING(m_FileName));

        // If failed to attach to an existing file-mapping object then
        // create a new one (based on the specified file)
        HANDLE hMap = ::OpenFileMapping(m_Attrs->map_access, false, filename.c_str());
        if ( !hMap ) {
            // Open file
            HANDLE hFile;
            hFile = ::CreateFile(filename.c_str(), m_Attrs->file_access, 
                                 m_Attrs->file_share, NULL,
                                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if ( hFile == INVALID_HANDLE_VALUE ) {
                errmsg += WIN_LAST_ERROR_STR;
                break;
            }
            // Create mapping
            hMap = ::CreateFileMapping(hFile, NULL, m_Attrs->map_protect, 0, 0, filename.c_str());
            if ( !hMap ) {
                errmsg += WIN_LAST_ERROR_STR;
                ::CloseHandle(hFile);
                break;
            }
            ::CloseHandle(hFile);
        }
        m_Handle->hMap = hMap;

#elif defined(NCBI_OS_UNIX)
        // Open file
        errno = 0;
        m_Handle->hMap = NcbiSys_open(_T_XCSTRING(m_FileName), m_Attrs->file_access);
        if ( m_Handle->hMap < 0 ) {
            errmsg = NcbiSys_strerror(errno);
            break;
        }
#endif
        // Success
        return;
    }
    // Error: close and cleanup
    x_Close();
    NCBI_THROW(CFileException, eMemoryMap,
               "CMemoryFile: Cannot memory map file '" + m_FileName + "':" + errmsg);
}


void CMemoryFileMap::x_Close()
{
    // Unmap all mapped segments with error ignoring
    ITERATE(TSegments, it, m_Segments) {
        delete it->second;
    }
    m_Segments.clear();

    // Close handle and cleanup
    if ( m_Handle ) {
        if ( m_Handle->hMap != kInvalidHandle ) { 
#if defined(NCBI_OS_MSWIN)
            CloseHandle(m_Handle->hMap);
#elif defined(NCBI_OS_UNIX)
            close(m_Handle->hMap);
#endif
        }
        delete m_Handle;
        m_Handle  = 0;
    }
}


// Extend/truncate file size to 'new_size' bytes.
// Do not change position in the file.
// Return 0 on success, or errno value.
// NOTE: Unix only.
//
#if defined(NCBI_OS_UNIX)
int s_FTruncate(int fd, Uint8 new_size)
{
    int errcode = 0;
    // ftruncate() add zeros
    while (ftruncate(fd, (off_t)new_size) < 0) {
        if (errno != EINTR) {
            errcode = errno;
            break;
        }
    }
    return errcode;
}
#endif

// Extend file size to 'new_size' bytes, 'new_size' should be > current file size.
// Do not change position in the file.
// Return 0 on success, or errno value.
// Similar to s_FTruncate() but can extend file size only. 
// NOTE: 
//   Works on both Windows and Unix, but file should not be opened in append mode,
//   or write() will ignore current position in the file and starts with EOF.
//
int s_FExtend(int fd, Uint8 new_size)
{
    if (!new_size) {
        return 0;
    }
    // Save current position
    off_t current_pos = NcbiSys_lseek(fd, 0, SEEK_CUR);
    if (current_pos < 0) {
        return errno;
    }
    // Set position beyond EOF, one byte less than necessary,
    // and write single zero byte.
    off_t pos = NcbiSys_lseek(fd, (off_t)new_size - 1, SEEK_SET);
    if (pos < 0) {
        return errno;
    }
    while (NcbiSys_write(fd, "\0", 1) < 0) {
        if (errno != EINTR) {
            return errno;
        }
    }
    // Restore current position
    pos = NcbiSys_lseek(fd, current_pos, SEEK_SET);
    if (pos < 0) {
        return errno;
    }
    return 0;
}


void CMemoryFileMap::x_Create(Uint8 size)
{
    int pmode = S_IREAD;
#if defined(NCBI_OS_MSWIN)
    if (m_Attrs->file_access & (GENERIC_READ | GENERIC_WRITE)) 
#elif defined(NCBI_OS_UNIX)
    if (m_Attrs->file_access & O_RDWR) 
#endif
        pmode |= S_IWRITE;

    // Create new file
    int fd = NcbiSys_creat(_T_XCSTRING(m_FileName), pmode);
    if ( fd < 0 ) {
        NCBI_THROW(CFileException, eMemoryMap, "CMemoryFileMap:"
                   " Cannot create file '" + m_FileName + "'");
    }
    // and fill it with zeros
    int errcode = s_FExtend(fd, size);
    NcbiSys_close(fd);
    if (errcode) {
#if defined(NCBI_OS_MSWIN)
        string errmsg = _T_STDSTRING(NcbiSys_strerror(errcode));
#elif defined(NCBI_OS_UNIX)
        string errmsg = NcbiSys_strerror(errcode);
#endif
        NCBI_THROW(CFileException, eMemoryMap, "CMemoryFileMap:"
                   " Cannot create file with specified size: " + errmsg);
    }
}


void CMemoryFileMap::x_Extend(Uint8 size, Uint8 new_size)
{
    if (size >= new_size) {
        return;
    }
    // Open file for writing.
    // Note: do not use append mode, or s_FExtend() will work incorrectly.
    int fd = NcbiSys_open(_T_XCSTRING(m_FileName), O_WRONLY, 0);
    if ( fd < 0 ) {
        int errcode = errno;
        NCBI_THROW(CFileException, eMemoryMap, "CMemoryFileMap:"
                   " Cannot open file '" + m_FileName +
                   "' to change its size: " +
			       _T_STDSTRING(NcbiSys_strerror(errcode)));
    }
    // and extend it with zeros
    int errcode = s_FExtend(fd, new_size);
    NcbiSys_close(fd);
    if (errcode) {
#if defined(NCBI_OS_MSWIN)
        string errmsg = _T_STDSTRING(NcbiSys_strerror(errcode));
#elif defined(NCBI_OS_UNIX)
        string errmsg = NcbiSys_strerror(errcode);
#endif
        NCBI_THROW(CFileException, eMemoryMap, "CMemoryFileMap:"
                   " Cannot extend file size: " + errmsg);
    }
}


CMemoryFileSegment* 
CMemoryFileMap::x_GetMemoryFileSegment(void* ptr) const
{
    if ( !m_Handle  ||  (m_Handle->hMap == kInvalidHandle) ) {
        NCBI_THROW(CFileException, eMemoryMap, "CMemoryFileMap:"
                   " File is not mapped");
    }
    TSegments::const_iterator segment = m_Segments.find(ptr);
    if ( segment == m_Segments.end() ) {
        NCBI_THROW(CFileException, eMemoryMap, "CMemoryFileMap:"
                   " Cannot find mapped file segment"
                   " with specified address");
    }
    return segment->second;
}

   
CMemoryFile::CMemoryFile(const string&  file_name,
                         EMemMapProtect protect,
                         EMemMapShare   share,
                         TOffsetType    offset,
                         size_t         length,
                         EOpenMode      mode,
                         Uint8          max_file_len)

    : CMemoryFileMap(file_name, protect, share, mode, max_file_len), m_Ptr(0)
{
    // Check that file is ready for mapping to memory
    if ( !m_Handle  ||  (m_Handle->hMap == kInvalidHandle) ) {
        return;
    }
    Map(offset, length);
}


void* CMemoryFile::Map(TOffsetType offset, size_t length)
{
    // Unmap if already mapped
    if ( m_Ptr ) {
        Unmap();
    }
    m_Ptr = CMemoryFileMap::Map(offset, length);
    return m_Ptr;
}


bool CMemoryFile::Unmap()
{
    if ( !m_Ptr ) {
        return true;
    }
    bool status = CMemoryFileMap::Unmap(m_Ptr);
    m_Ptr = 0;
    return status;
}


void* CMemoryFile::Extend(size_t length)
{
    x_Verify();

    // Get current mapped segment
    CMemoryFileSegment* segment = x_GetMemoryFileSegment(m_Ptr);
    TOffsetType offset = segment->GetOffset();

    // Get file size
    Int8 file_size = GetFileSize();

    // Map file wholly if the length of the mapped region is not specified
    if ( !length ) {
        Int8 fs = file_size - offset;
        if ( (Uint8)fs > get_limits(length).max() ) {
            NCBI_THROW(CFileException, eMemoryMap, "CMemoryFile: "
                       "Specified length of the mapping region is too big "
                       "(length=" + NStr::Int8ToString(length) + ')');
        } else if ( fs > 0 ) {
            length = (size_t)fs;
        } else {
            NCBI_THROW(CFileException, eMemoryMap, "CMemoryFile: "
                       "Specified offset of the mapping region"
                       " exceeds the file size");
        }
    }

    // Changing file size is necessary
    if (Int8(offset + length) > file_size) {
        x_Close();
        m_Ptr = 0;
        x_Extend(file_size, offset + length);
        x_Open();
    }
    // Remap current region
    Map(offset, length);
    return GetPtr();
}


void CMemoryFile::x_Verify(void) const
{
    if ( m_Ptr ) {
        return;
    }
    NCBI_THROW(CFileException, eMemoryMap, "CMemoryFile: File is not mapped");
}



//////////////////////////////////////////////////////////////////////////////
//
// CFileException
//

const char* CFileException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eMemoryMap:    return "eMemoryMap";
    case eRelativePath: return "eRelativePath";
    case eNotExists:    return "eNotExists";
    case eFileIO:       return "eFileIO";
    case eTmpFile:      return "eTmpFile";
    default:            return CException::GetErrCodeString();
    }
}

const char* CFileErrnoException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eFile:            return "eFile";
    case eFileSystemInfo:  return "eFileSystemInfo";
    case eFileLock:        return "eFileLock";
    case eFileIO:          return "eFileIO";
    default:               return CException::GetErrCodeString();
    }
}



//////////////////////////////////////////////////////////////////////////////
//
// Find files
//

void x_Glob(const string& path,
            const std::list<string>& parts,
            std::list<string>::const_iterator next,
            std::list<string>& result,
            TFindFiles flags)
{
    vector<string> paths;
    paths.push_back(path);
    vector<string> masks;
    masks.push_back(*next);
    bool last = ++next == parts.end();
    TFindFiles ff = flags;
    if ( !last ) {
        ff &= ~(fFF_File | fFF_Recursive);
        ff |= fFF_Dir;
    }
    std::list<string> found;
    FindFiles(found, paths.begin(), paths.end(), masks, ff);
    if ( last ) {
        result.insert(result.end(), found.begin(), found.end());
    }
    else {
        if ( !found.empty() ) {
            ITERATE(std::list<string>, it, found) {
                x_Glob(CDirEntry::AddTrailingPathSeparator(*it), parts, next, result, flags);
            }
        }
        else {
            x_Glob(CDirEntry::AddTrailingPathSeparator(path + masks.front()), parts, next, result, flags);
        }
    }
}


void FindFiles(const string& pattern,  std::list<string>& result, TFindFiles flags)
{
    TFindFiles find_type = flags & fFF_All;
    if ( find_type == 0 ) {
        flags |= fFF_All;
    }

    string kDirSep(1, CDirEntry::GetPathSeparator());
    string abs_path = CDirEntry::CreateAbsolutePath(pattern);
    string search_path = kDirSep;

    std::list<string> parts;
    NStr::Split(abs_path, kDirSep, parts, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
    if ( parts.empty() ) {
        return;
    }

#if defined(DISK_SEPARATOR)
    // Network paths on Windows start with double back-slash and
    // need special processing.
    // Note: abs_path has normalized in CreateAbsolutePath().

    string kNetSep(2, CDirEntry::GetPathSeparator());
    bool is_network = abs_path.find(kNetSep) == 0;
    if ( is_network ) {
        search_path = kNetSep + parts.front() + kDirSep;
        parts.erase(parts.begin());
    }
    else {
        string disk;
        CDirEntry::SplitPathEx(abs_path, &disk);
        if ( disk.empty() ) {
            // Disk is missing in the absolute path, add it.
            CDirEntry::SplitPathEx(CDir::GetCwd(), &disk);
            if ( !disk.empty() ) {
                search_path = disk + kDirSep;
            }
        }
        else {
            search_path = disk;
            // Disk is present but may be missing dir separator
            if (abs_path[disk.size()] == DIR_SEPARATOR) {
                parts.erase(parts.begin()); // Remove disk from parts
                search_path += kDirSep;
            }
            else {
                // Disk is included in the first part, remove it.
                string temp = parts.front().substr(disk.size());
                parts.erase(parts.begin());
                parts.insert(parts.begin(), temp);
            }
        }
    }
#endif

    x_Glob(search_path, parts, parts.begin(), result, flags);
}


#define UNDEFINED_SORT_MODE kMax_Int

SCompareDirEntries::SCompareDirEntries(ESort s1)
{
    m_Sort[0] = s1;
    m_Sort[1] = UNDEFINED_SORT_MODE;
    m_Sort[2] = UNDEFINED_SORT_MODE;
}
SCompareDirEntries::SCompareDirEntries(ESort s1, ESort s2)
{
    m_Sort[0] = s1;
    m_Sort[1] = s2;
    m_Sort[2] = UNDEFINED_SORT_MODE;
}
SCompareDirEntries::SCompareDirEntries(ESort s1, ESort s2, ESort s3)
{
    m_Sort[0] = s1;
    m_Sort[1] = s2;
    m_Sort[2] = s3;
}

bool SCompareDirEntries::operator()(const string& p1, const string& p2)
{
    // Default case
    if (m_Sort[0] == ePath) {
        return (p1 < p2);
    }
    string d1, n1, e1;
    string d2, n2, e2;
    CDirEntry::SplitPath(p1, &d1, &n1, &e1);
    CDirEntry::SplitPath(p2, &d2, &n2, &e2);

    int nc = 0;

    for (int i = 0; i < 3; i++) {
        if (m_Sort[i] == UNDEFINED_SORT_MODE) {
            break;
        }
        switch (m_Sort[i]) {
        case ePath:
            // usually we shouldn't get here, so just compare and exit
            return (p1 < p2);
        case eDir:
            nc = NStr::CompareCase(d1, d2);
            break;
        case eName:
            nc = NStr::CompareCase(n1 + e1, n2 + e2);
            break;
        case eBase:
            nc = NStr::CompareCase(n1, n2);
            break;
        case eExt:
            nc = NStr::CompareCase(e1, e2);
            break;
        default:
            NCBI_THROW(CCoreException, eInvalidArg, "Unknown sorting mode");
        }
        if (nc != 0)
            break;
    }
    return nc < 0;
}



//////////////////////////////////////////////////////////////////////////////
//
// CFileIO
//

CFileIO::CFileIO(void) :
    m_Handle(kInvalidHandle),
    m_AutoClose(false),
    m_AutoRemove(CFileIO::eDoNotRemove)
{
}


CFileIO::~CFileIO()
{
    if (m_Handle != kInvalidHandle && m_AutoClose) {
        try {
            Close();
        }
        NCBI_CATCH_ALL("Error while closing file [IGNORED]");
    }
}


void CFileIO::Open(const string& filename,
                   EOpenMode     open_mode,
                   EAccessMode   access_mode,
                   EShareMode    share_mode)
{
    string errmsg;

#if defined(NCBI_OS_MSWIN)

    // Translate parameters
    DWORD dwAccessMode, dwShareMode, dwOpenMode;

    switch (open_mode) {
        case eCreate:
            dwOpenMode = CREATE_ALWAYS;
            break;
        case eCreateNew:
            dwOpenMode = CREATE_NEW;
            break;
        case eOpen:
            dwOpenMode = OPEN_EXISTING;
            break;
        case eOpenAlways:
            dwOpenMode = OPEN_ALWAYS;
            break;
        case eTruncate:
            dwOpenMode = TRUNCATE_EXISTING;
            break;
        default:
            _TROUBLE;
    }
    switch (access_mode) {
        case eRead:
            dwAccessMode = GENERIC_READ;
            break;
        case eWrite:
            dwAccessMode = GENERIC_WRITE;
            break;
        case eReadWrite:
            dwAccessMode = GENERIC_READ | GENERIC_WRITE;
            break;
        default:
            _TROUBLE;
    };
    switch (share_mode) {
        case eShareRead:
            dwShareMode = FILE_SHARE_READ;
            break;
        case eShareWrite:
            dwShareMode = FILE_SHARE_WRITE;
            break;
        case eShare:
            dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
            break;
        case eExclusive:
            dwShareMode = 0;
            break;
        default:
            _TROUBLE;
    }

    m_Handle = ::CreateFile(_T_XCSTRING(filename),
                            dwAccessMode, dwShareMode, NULL, dwOpenMode,
                            FILE_ATTRIBUTE_NORMAL, NULL);
    if (m_Handle == kInvalidHandle) {
        errmsg = WIN_LAST_ERROR_STR;
    }

#elif defined(NCBI_OS_UNIX)

    // Translate parameters
# if defined(O_BINARY)
    int flags = O_BINARY;
# else
    int flags = 0; 
# endif
    mode_t mode = 0;

    switch (open_mode) {
        case eCreate:
            flags |= (O_CREAT | O_TRUNC);
            break;
        case eCreateNew:
            if ( CFile(filename).Exists() ) {
                NCBI_THROW(CFileException, eFileIO,
                           "Open mode is eCreateNew but file already exists: "
                           + filename );
            }
            flags |= O_CREAT;
            break;
        case eOpen:
            // by default
            break;
        case eOpenAlways:
            if ( !CFile(filename).Exists() ) {
                flags |= O_CREAT;
            }
            break;
        case eTruncate:
            flags |= O_TRUNC;
            break;
        default:
            _TROUBLE;
    }
    switch (access_mode) {
        case eRead:
            flags |= O_RDONLY;
            mode  |= CDirEntry::MakeModeT(CDirEntry::fRead,
                                          CDirEntry::fRead,
                                          CDirEntry::fRead, 0);
            break;
        case eWrite:
            flags |= O_WRONLY;
            mode  |= CDirEntry::MakeModeT(CDirEntry::fWrite,
                                          CDirEntry::fWrite,
                                          CDirEntry::fWrite, 0);
            break;
        case eReadWrite:
            flags |= O_RDWR;
            mode  |= CDirEntry::MakeModeT(CDirEntry::fRead | CDirEntry::fWrite,
                                          CDirEntry::fRead | CDirEntry::fWrite,
                                          CDirEntry::fRead | CDirEntry::fWrite, 0);
            break;
        default:
            _TROUBLE;
    };
    // Dummy, ignore 'share_mode' on UNIX, to avoid warnings
    std::ignore = share_mode;

    // Try to open/create file
    m_Handle = NcbiSys_open(_T_XCSTRING(filename), flags, mode);
    if (m_Handle == kInvalidHandle) {
        errmsg = NcbiSys_strerror(errno);
    }

#endif
    if (m_Handle == kInvalidHandle) {
        NCBI_THROW(CFileErrnoException, eFileIO,
                   "Cannot open file '" + filename + "': " + errmsg);
    }
    m_Pathname = filename;
    m_AutoClose = true;
}


void CFileIO::CreateTemporary(const string& dir,
                              const string& prefix,
                              EAutoRemove auto_remove)
{
    if (m_Handle != kInvalidHandle) {
       NCBI_THROW(CFileException, eFileIO,
                  "Cannot create temporary: Handle already open");
    }
    static atomic<int> s_Count = 0;
    string x_dir = dir;
    if (x_dir.empty()) {
        // Get application specific temporary directory name
        x_dir = CDir::GetAppTmpDir();
    }
    if (!x_dir.empty()) {
        x_dir = CDirEntry::AddTrailingPathSeparator(x_dir);
    }
    Uint8 x_tid = (Uint8) GetCurrentThreadSystemID();
    unsigned int tid = (unsigned int) x_tid;
    string x_prefix = prefix
        + NStr::NumericToString(CCurrentProcess::GetPid())
        + NStr::NumericToString(s_Count++)
        + NStr::NumericToString(tid);

#if defined(NCBI_OS_MSWIN)  ||  defined(NCBI_OS_UNIX)
    string pattern = x_dir + x_prefix;

#  if defined(NCBI_OS_UNIX)
    pattern += "XXXXXX";
    if (pattern.size() > PATH_MAX) {
        NCBI_THROW(CFileErrnoException, eFileIO,
                   "Pattern too long '" + pattern + "'");
    }
    char pathname[PATH_MAX+1];
    memcpy(pathname, pattern.c_str(), pattern.size()+1);
    if ((m_Handle = mkstemp(pathname)) == kInvalidHandle) {
        NCBI_THROW(CFileErrnoException, eFileIO,
                   "mkstemp() failed for '" + pattern + "'");
    }
    m_Pathname = pathname;
    if (auto_remove == eRemoveASAP) {
        NcbiSys_remove(_T_XCSTRING(m_Pathname));
    }

#  elif defined(NCBI_OS_MSWIN)
    unsigned long ofs = (unsigned long) int(rand());
    while (ofs < numeric_limits<unsigned long>::max()) {
        char buffer[40];
        _ultoa(ofs, buffer, 36);
        string pathname = pattern + buffer;
        m_Handle = ::CreateFile(_T_XCSTRING(pathname), GENERIC_ALL, 0, NULL,
                                CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY, NULL);
        if (m_Handle != kInvalidHandle) {
            m_Pathname.swap(pathname);
            break;
        }
        if (::GetLastError() != ERROR_FILE_EXISTS) {
            break;
        }
        ++ofs;
    }

    if (m_Handle == kInvalidHandle) {
        NCBI_THROW(CFileErrnoException, eFileIO,
                   "Unable to create temporary file '" + pattern + "'");
    }

#  endif

#else // defined(NCBI_OS_MSWIN)  ||  defined(NCBI_OS_UNIX)
    x_prefix += NStr::NumericToString((unsigned int) rand());
    Open(s_StdGetTmpName(x_dir.c_str(), x_prefix.c_str()), eCreateNew, eReadWrite);
#endif

    m_AutoClose = true;
    m_AutoRemove = auto_remove;
}


void CFileIO::Close(void)
{
    if (m_Handle != kInvalidHandle) {
#if defined(NCBI_OS_MSWIN)
        if (!::CloseHandle(m_Handle)) {
            NCBI_THROW(CFileErrnoException, eFileIO, "CloseHandle() failed");
        }
#elif defined(NCBI_OS_UNIX)
        while (close(m_Handle) < 0) {
            if (errno != EINTR) {
                NCBI_THROW(CFileErrnoException, eFileIO, "close() failed");
            }
        }
#endif
        m_Handle = kInvalidHandle;

        if (m_AutoRemove != eDoNotRemove)
            NcbiSys_remove(_T_XCSTRING(m_Pathname));
    }
}


size_t CFileIO::Read(void* buf, size_t count) const
{
    if (count == 0) {
        return 0;
    }
    char* ptr = (char*) buf;
    
#if defined(NCBI_OS_MSWIN)
    const DWORD   kMax = numeric_limits<DWORD>::max();
#elif defined(NCBI_OS_UNIX)
    const ssize_t kMax = numeric_limits<ssize_t>::max();
#endif   
    
    while (count) {

#if defined(NCBI_OS_MSWIN)
        DWORD nmax = count > kMax ? kMax : (DWORD) count;
        DWORD n = 0;
        if ( ::ReadFile(m_Handle, ptr, nmax, &n, NULL) == 0 ) {
            if (::GetLastError() == ERROR_HANDLE_EOF) {
                break;
            }
            NCBI_THROW(CFileErrnoException, eFileIO, "ReadFile() failed");
        }
        if ( n == 0 ) {
            break;
        }
#elif defined(NCBI_OS_UNIX)
        ssize_t nmax = count > kMax ? kMax : (ssize_t) count;
        ssize_t n = ::read(int(m_Handle), ptr, nmax);
        if (n == 0) {
            break;
        }
        if ( n < 0 ) {
            if (errno == EINTR) {
                continue;
            }
            NCBI_THROW(CFileErrnoException, eFileIO, "read() failed");
        }
#endif
        count -= n;
        ptr += n;
    }
    return ptr - (char*)buf;
}


size_t CFileIO::Write(const void* buf, size_t count) const
{
    if (count == 0) {
        return 0;
    }
    const char* ptr = (const char*) buf;
    
#if defined(NCBI_OS_MSWIN)
    const DWORD   kMax = numeric_limits<DWORD>::max();
#elif defined(NCBI_OS_UNIX)
    const ssize_t kMax = numeric_limits<ssize_t>::max();
#endif   
    
    while (count) {

#if defined(NCBI_OS_MSWIN)
        DWORD nmax = count > kMax ? kMax : (DWORD) count;
        DWORD n = 0;
        if ( ::WriteFile(m_Handle, ptr, nmax, &n, NULL) == 0 ) {
            NCBI_THROW(CFileErrnoException, eFileIO, "WriteFile() failed");
        }
#elif defined(NCBI_OS_UNIX)
        ssize_t nmax = count > kMax ? kMax : (ssize_t) count;
        ssize_t n = ::write(int(m_Handle), ptr, nmax);
        if ( n < 0  &&  errno == EINTR ) {
            continue;
        }
        if ( n <= 0 ) {
            NCBI_THROW(CFileErrnoException, eFileIO, "write() failed");
        }
#endif
        count -= n;
        ptr += n;
    }
    return ptr - (char*)buf;
}


void CFileIO::Flush(void) const
{
    bool res;
#if defined(NCBI_OS_MSWIN)
    res = (::FlushFileBuffers(m_Handle) == TRUE);
#elif defined(NCBI_OS_UNIX)
    res = (fsync(m_Handle) == 0);
#endif
    if (!res) {
        NCBI_THROW(CFileErrnoException, eFileIO, "Cannot flush");
    }
}


void CFileIO::SetFileHandle(TFileHandle handle)
{
    // Close previous handle if needed
    if (m_AutoClose) {
        Close();
        m_AutoClose = false;
    }
    // Use given handle for all I/O
    m_Handle = handle;
}


Uint8 CFileIO::GetFilePos(void) const
{
#if defined(NCBI_OS_MSWIN)
    LARGE_INTEGER ofs;
    LARGE_INTEGER pos;
    ofs.QuadPart = 0;
    pos.QuadPart = 0;
    BOOL res = SetFilePointerEx(m_Handle, ofs, &pos, FILE_CURRENT);
    if (res) {
        return (Uint8)pos.QuadPart;
    }
#elif defined(NCBI_OS_UNIX)
    off_t pos = NcbiSys_lseek(m_Handle, 0, SEEK_CUR);
    if (pos != -1L) {
        return (Uint8)pos;
    }
#endif
    NCBI_THROW(CFileErrnoException, eFileIO, "Cannot get file position");
    // Unreachable
    return 0;
}


void CFileIO::SetFilePos(Uint8 position) const
{
#if defined(NCBI_OS_MSWIN)
    LARGE_INTEGER ofs;
    ofs.QuadPart = position;
    bool res = (SetFilePointerEx(m_Handle, ofs, NULL, FILE_BEGIN) == TRUE);
#elif defined(NCBI_OS_UNIX)
    bool res = (NcbiSys_lseek(m_Handle, (off_t)position, SEEK_SET) != -1);
#endif
    if (!res) {
        NCBI_THROW(CFileErrnoException, eFileIO,
                   "Cannot change file positon"
                   " (position=" + NStr::UInt8ToString(position) + ')');
    }
}


void CFileIO::SetFilePos(Int8 offset, EPositionMoveMethod move_method) const
{
#if defined(NCBI_OS_MSWIN)
    DWORD from = 0;
    switch (move_method) {
        case eBegin:
            from = FILE_BEGIN;
            break;
        case eCurrent:
            from = FILE_CURRENT;
            break;
        case eEnd:
            from = FILE_END;
            break;
        default:
            _TROUBLE;
    }
    LARGE_INTEGER ofs;
    ofs.QuadPart = offset;
    bool res = (SetFilePointerEx(m_Handle, ofs, NULL, from) == TRUE);

#elif defined(NCBI_OS_UNIX)
    int from = 0;
    switch (move_method) {
        case eBegin:
            from = SEEK_SET;
            break;
        case eCurrent:
            from = SEEK_CUR;
            break;
        case eEnd:
            from = SEEK_END;
            break;
        default:
            _TROUBLE;
    }
    bool res = (NcbiSys_lseek(m_Handle, (off_t)offset, from) != -1);
#endif
    if (!res) {
        NCBI_THROW(CFileErrnoException, eFileIO,
                   "Cannot change file positon"
                   " (offset=" + NStr::Int8ToString(offset) +
                   ", method=" + NStr::IntToString(move_method) + ')');
    }
}


Uint8 CFileIO::GetFileSize(void) const
{
#if defined(NCBI_OS_MSWIN)
    DWORD size_hi = 0;
    DWORD size_lo = ::GetFileSize(m_Handle, &size_hi);
    if (size_lo != INVALID_FILE_SIZE) {
        return ((unsigned __int64)size_hi << 32) | size_lo;
    }
#elif defined(NCBI_OS_UNIX)
    TNcbiSys_fstat st;
    if (NcbiSys_fstat(m_Handle, &st) != -1) {
        return st.st_size;
    }
#endif
    NCBI_THROW(CFileErrnoException, eFileIO, "Cannot get file size");
    // Unreachable
    return 0;
}


void CFileIO::SetFileSize(Uint8 length, EPositionMoveMethod pos) const
{
#if defined(NCBI_OS_MSWIN)
    BOOL res = true;
    // Get current position if needed
    LARGE_INTEGER ofs;
    LARGE_INTEGER saved;
    ofs.QuadPart = 0;
    saved.QuadPart = 0;
    // Save current file position if needed
    if (pos == eCurrent) {
        res = SetFilePointerEx(m_Handle, ofs, &saved, FILE_CURRENT);
    }
    if (res) {
        // Set file position to specified length (new EOF)
        ofs.QuadPart = length;
        res = SetFilePointerEx(m_Handle, ofs, NULL, FILE_BEGIN);
        // And change file size
        if (res) {
            res = SetEndOfFile(m_Handle);
        }
        // Set file pointer if other than eEnd
        if (res) {
            if (pos == eBegin) {
                // eBegin
                ofs.QuadPart = 0;
                res = SetFilePointerEx(m_Handle, ofs, NULL, FILE_BEGIN);
            }
            else if (pos == eCurrent) {
                res = SetFilePointerEx(m_Handle, saved, NULL, FILE_BEGIN);
            }
            // Nothing todo if eEnd, because we already at the EOF position
        }
    }
#elif defined(NCBI_OS_UNIX)
    bool res = true;
    int errcode = s_FTruncate(m_Handle, length);
    if (!errcode  &&  pos != eCurrent) {
        // Move file offset to the start/end of the file.
        SetFilePos(0, pos);
    }
    if (errcode) {
       // Restore errno for CFileErrnoException exception
       errno = errcode;
       res = false;
    }
   
#endif
    if (!res) {
        NCBI_THROW(CFileErrnoException, eFileIO,
                   "Cannot change file size"
                   " (length=" + NStr::NumericToString(length) + ')');
    }
}



//////////////////////////////////////////////////////////////////////////////
//
// CFileReader
//

CFileReader::CFileReader(const string& filename, EShareMode share_mode)
{
    m_File.Open(filename, eOpen, eRead, share_mode);
}


CFileReader::CFileReader(const char* filename, EShareMode share_mode)
{
    m_File.Open(filename, eOpen, eRead, share_mode);
}


CFileReader::CFileReader(TFileHandle handle)
{
    m_File.SetFileHandle(handle);
}


IReader* CFileReader::New(const string& filename, EShareMode share_mode)
{
    if ( filename == "-" ) {
#if defined(NCBI_OS_MSWIN)
        TFileHandle handle = GetStdHandle(STD_INPUT_HANDLE);
#elif defined(NCBI_OS_UNIX)
#  ifdef STDIN_FILENO
        TFileHandle handle = STDIN_FILENO;
#  else
        TFileHandle handle = 0;
#  endif //STDIN_FILENO
#endif
        return new CFileReader(handle);
    }
    return new CFileReader(filename, share_mode);
}


ERW_Result CFileReader::Read(void* buf, size_t count, size_t* bytes_read)
{
    if ( bytes_read ) {
        *bytes_read = 0;
    }
    if ( !count ) {
        return eRW_Success;
    }
    size_t n;
    try {
        n = m_File.Read(buf, count);
    }
    catch (const CFileErrnoException& ex) {
        CNcbiError::SetErrno(ex.GetErrno());
        return eRW_Error;
    }
    if ( bytes_read ) {
        *bytes_read = n;
    }
    return n? eRW_Success : eRW_Eof;
}


ERW_Result CFileReader::PendingCount(size_t* /*count*/)
{
    return eRW_NotImplemented;
}


//////////////////////////////////////////////////////////////////////////////
//
// CFileWriter
//

CFileWriter::CFileWriter(const string& filename,
                         EOpenMode  open_mode,
                         EShareMode share_mode)
{
    m_File.Open(filename, open_mode, eWrite, share_mode);
}


CFileWriter::CFileWriter(const char* filename,
                         EOpenMode  open_mode,
                         EShareMode share_mode)
{
    m_File.Open(filename, open_mode, eWrite, share_mode);
}


CFileWriter::CFileWriter(TFileHandle handle)
{
    m_File.SetFileHandle(handle);
    return;
}


IWriter* CFileWriter::New(const string& filename,
                          EOpenMode  open_mode,
                          EShareMode share_mode)
{
    if ( filename == "-" ) {
#if defined(NCBI_OS_MSWIN)
        TFileHandle handle = GetStdHandle(STD_OUTPUT_HANDLE);
#elif defined(NCBI_OS_UNIX)
#  ifdef STDOUT_FILENO
        TFileHandle handle = STDOUT_FILENO;
#  else
        TFileHandle handle = 1;
#  endif //STDOUT_FILENO
#endif
        return new CFileWriter(handle);
    }
    return new CFileWriter(filename, open_mode, share_mode);
}


ERW_Result CFileWriter::Write(const void* buf,
                              size_t count, size_t* bytes_written)
{
    if ( bytes_written ) {
        *bytes_written = 0;
    }
    if ( !count ) {
        return eRW_Success;
    }
    size_t n;
    try {
        n = m_File.Write(buf, count);
    }
    catch (const CFileErrnoException& ex) {
        CNcbiError::SetErrno(ex.GetErrno());
        return eRW_Error;
    }
    if ( bytes_written ) {
        *bytes_written = n;
    }
    return n? eRW_Success : eRW_Error;
}


ERW_Result CFileWriter::Flush(void)
{
    try {
        m_File.Flush();
    }
    catch (const CFileErrnoException& ex) {
        CNcbiError::SetErrno(ex.GetErrno());
        return eRW_Error;
    }
    return eRW_Success;
}


//////////////////////////////////////////////////////////////////////////////
//
// CFileReaderWriter
//

CFileReaderWriter::CFileReaderWriter(const string& filename,
                                     EOpenMode  open_mode,
                                     EShareMode share_mode)
{
    m_File.Open(filename, open_mode, eReadWrite, share_mode);
}


CFileReaderWriter::CFileReaderWriter(const char* filename,
                                     EOpenMode  open_mode,
                                     EShareMode share_mode)
{
    m_File.Open(filename, open_mode, eReadWrite, share_mode);
}


CFileReaderWriter::CFileReaderWriter(TFileHandle handle)
{
    m_File.SetFileHandle(handle);
    return;
}


IReaderWriter* CFileReaderWriter::New(const string& filename,
                                      EOpenMode  open_mode,
                                      EShareMode share_mode)
{
    return new CFileReaderWriter(filename, open_mode, share_mode);
}


ERW_Result CFileReaderWriter::Read(void* buf,
                                   size_t count, size_t* bytes_read)
{
    if ( bytes_read ) {
        *bytes_read = 0;
    }
    if ( !count ) {
        return eRW_Success;
    }
    size_t n;
    try {
        n = m_File.Read(buf, count);
    }
    catch (const CFileErrnoException& ex) {
        CNcbiError::SetErrno(ex.GetErrno());
        return eRW_Error;
    }
    if ( bytes_read ) {
        *bytes_read = n;
    }
    return n? eRW_Success : eRW_Eof;
}


ERW_Result CFileReaderWriter::PendingCount(size_t* /*count*/)
{
    return eRW_NotImplemented;
}


ERW_Result CFileReaderWriter::Write(const void* buf,
                                    size_t count, size_t* bytes_written)
{
    if ( bytes_written ) {
        *bytes_written = 0;
    }
    if ( !count ) {
        return eRW_Success;
    }
    size_t n;
    try {
        n = m_File.Write(buf, count);
    }
    catch (const CFileErrnoException& ex) {
        CNcbiError::SetErrno(ex.GetErrno());
        return eRW_Error;
    }
    if ( bytes_written ) {
        *bytes_written = n;
    }
    return n? eRW_Success : eRW_Error;
}


ERW_Result CFileReaderWriter::Flush(void)
{
    try {
        m_File.Flush();
    }
    catch (const CFileException&) { 
       return eRW_Error;
    }
    return eRW_Success;
}



//////////////////////////////////////////////////////////////////////////////
//
// CFileLock
//

// Clean up an all non-default bits in group if all bits are set
#define F_CLEAN_REDUNDANT(group) \
    if (F_ISSET(m_Flags, (group))) \
        m_Flags &= ~unsigned((group) & ~unsigned(fDefault))

// Platform-dependent structure to store file locking information
struct SLock {
    SLock(void) {};
    SLock(CFileLock::TOffsetType off, size_t len) {
        Reset(off, len);
    }
#if defined(NCBI_OS_MSWIN)
    void Reset(CFileLock::TOffsetType off, size_t len) 
    {
        offset_lo = (DWORD)(off & 0xFFFFFFFF);
        offset_hi = (DWORD)((Int8(off) >> 32) & 0xFFFFFFFF);
        // Locking a region that goes beyond the current EOF position
        // is not an error.
        if (len) {
            length_lo = (DWORD)(len & 0xFFFFFFFF);
            length_hi = (DWORD)((Int8(len) >> 32) & 0xFFFFFFFF);
        } else {
            // Lock a whole file
            length_lo = 0;
            length_hi = 0xFFFFFFFF;
        }
    };
    DWORD offset_lo;
    DWORD offset_hi;
    DWORD length_lo;
    DWORD length_hi;
#elif defined(NCBI_OS_UNIX)
    void Reset(CFileLock::TOffsetType off, size_t len) {
        offset = off;
        length = len;
    }
    off_t  offset;
    size_t length;
#endif
};


CFileLock::CFileLock(const string& filename, TFlags flags, EType type,
                     TOffsetType offset, size_t length)
    : m_Handle(kInvalidHandle), m_CloseHandle(false), m_Flags(flags),
      m_IsLocked(false), m_Lock(0)
{
    x_Init(filename.c_str(), type, offset, length);
}


CFileLock::CFileLock(const char* filename, TFlags flags, EType type,
                     TOffsetType offset, size_t length)
    : m_Handle(kInvalidHandle), m_CloseHandle(false), m_Flags(flags),
      m_IsLocked(false), m_Lock(0)
{
    x_Init(filename, type, offset, length);
}


CFileLock::CFileLock(TFileHandle handle, TFlags flags, EType type,
                     TOffsetType offset, size_t length)
    : m_Handle(handle), m_CloseHandle(false), m_Flags(flags),
      m_IsLocked(false), m_Lock(0)
{
    x_Init(0, type, offset, length);
}


void CFileLock::x_Init(const char* filename, EType type, TOffsetType offset, size_t length)
{
    // Reset redundant flags
    F_CLEAN_REDUNDANT(fLockNow | fLockLater);
    F_CLEAN_REDUNDANT(fAutoUnlock | fNoAutoUnlock);

    // Open file
    if (filename) {
#if defined(NCBI_OS_MSWIN)
        m_Handle = ::CreateFile(_T_XCSTRING(filename), GENERIC_READ,
                                FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
#elif defined(NCBI_OS_UNIX)
        m_Handle = NcbiSys_open(_T_XCSTRING(filename), O_RDWR);
#endif
    }
    if (m_Handle == kInvalidHandle) {
        NCBI_THROW(CFileErrnoException, eFileLock,
                   "Cannot open file '" + (filename ? string(filename) : "NULL") + "'");
    }
    if (filename) {
        m_CloseHandle = true;
    }
    m_Lock = new SLock;

    // Lock file if necessary
    if (F_ISSET(m_Flags, fLockNow)) {
         Lock(type, offset, length);
    }
}


CFileLock::~CFileLock()
{
    if (m_Handle == kInvalidHandle) {
        return;
    }
    try {
        // Remove lock automaticaly
        if (F_ISSET(m_Flags, fAutoUnlock)) {
            Unlock();
        }
    }
    catch (const CException& e) {
        NCBI_REPORT_EXCEPTION_X(4, "CFileLock::~CFileLock(): Cannot unlock", e);
    }

    if (m_CloseHandle) {
#if defined(NCBI_OS_MSWIN)
        ::CloseHandle(m_Handle);
#elif defined(NCBI_OS_UNIX)
        ::close(m_Handle);
#endif
    }
    return;
}


void CFileLock::Lock(EType type, TOffsetType offset, size_t length)
{
    // Remove previous lock
    if (m_IsLocked) {
        Unlock();
    }
    // Set new one
    m_Lock->Reset(offset, length);
    
#if defined(NCBI_OS_MSWIN)
    DWORD flags = LOCKFILE_FAIL_IMMEDIATELY;
    if (type == eExclusive) {
        flags |= LOCKFILE_EXCLUSIVE_LOCK;
    }
    OVERLAPPED overlapped;
    overlapped.hEvent     = 0;
    overlapped.Offset     = m_Lock->offset_lo;
    overlapped.OffsetHigh = m_Lock->offset_hi;
    bool res = LockFileEx(m_Handle, flags, 0, 
                            m_Lock->length_lo, m_Lock->length_hi,
                            &overlapped) == TRUE;
#elif defined(NCBI_OS_UNIX)
    struct flock fl;
    fl.l_type   = (type == eShared) ? F_RDLCK : F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start  = m_Lock->offset;
    fl.l_len    = m_Lock->length;   // 0 - lock to EOF 
    fl.l_pid    = getpid();
    
    int err;
    do {
        err = fcntl(m_Handle, F_SETLK, &fl);
    } while (err && (errno == EINTR));
    bool res = (err == 0);

#endif
    if (!res) {
        NCBI_THROW(CFileErrnoException, eFileLock, "Cannot lock file");
    }
    m_IsLocked = true;
    return;
}


void CFileLock::Unlock(void)
{
    if (!m_IsLocked) {
        return;
    }
#if defined(NCBI_OS_MSWIN)
    OVERLAPPED overlapped;
    overlapped.hEvent     = 0;
    overlapped.Offset     = m_Lock->offset_lo;
    overlapped.OffsetHigh = m_Lock->offset_hi;
    bool res = UnlockFileEx(m_Handle, 0,
                            m_Lock->length_lo, m_Lock->length_hi,
                            &overlapped) == TRUE;

#elif defined(NCBI_OS_UNIX)
    struct flock fl;
    fl.l_type   = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start  = m_Lock->offset;
    fl.l_len    = m_Lock->length;
    fl.l_pid    = getpid();
    
    int err;
    do {
        err = fcntl(m_Handle, F_SETLK, &fl);
    } while (err && (errno == EINTR));
    bool res = (err == 0);

#endif
    if (!res) {
        NCBI_THROW(CFileErrnoException, eFileLock, "Cannot unlock");
    }
    m_IsLocked = false;
    return;
}


//////////////////////////////////////////////////////////////////////////////
//
// Misc
//

void CFileAPI::SetLogging(ESwitch on_off_default)
{
    NCBI_PARAM_TYPE(NCBI, FileAPILogging)::SetDefault(
        on_off_default != eDefault ?
        on_off_default != eOff : DEFAULT_LOGGING_VALUE);
}

void CFileAPI::SetHonorUmask(ESwitch on_off_default)
{
    NCBI_PARAM_TYPE(NCBI, FileAPIHonorUmask)::SetDefault(
        on_off_default != eDefault ?
        on_off_default != eOff : DEFAULT_HONOR_UMASK_VALUE);
}

void CFileAPI::SetDeleteReadOnlyFiles(ESwitch on_off_default)
{
    NCBI_PARAM_TYPE(NCBI, DeleteReadOnlyFiles)::SetDefault(
        on_off_default == eOn);
}


END_NCBI_SCOPE
