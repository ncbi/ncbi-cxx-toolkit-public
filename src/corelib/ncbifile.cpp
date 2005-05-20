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
 * Author: Vladimir Ivanov
 *
 * File Description:   Files and directories accessory functions
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_limits.h>
#include <corelib/ncbi_system.hpp>

#include <stdio.h>

#if defined(NCBI_OS_MSWIN)
#  include <corelib/ncbi_os_mswin.hpp>
#  include <corelib/ncbi_limits.hpp>
#  include <io.h>
#  include <direct.h>
#  include <sys/utime.h>
// for CDirEntry::GetOwner()
#  include <accctrl.h>
#  include <aclapi.h>

#elif defined(NCBI_OS_UNIX)
#  include <unistd.h>
#  include <dirent.h>
#  include <pwd.h>
#  include <fcntl.h>
#  include <sys/time.h>
#  include <sys/mman.h>
#  include <utime.h>
#  include <pwd.h>
#  include <grp.h>
#  if !defined(MAP_FAILED)
#    define MAP_FAILED ((void *) -1)
#  endif

#  if defined(NCBI_COMPILER_MW_MSL)
#    include <ncbi_mslextras.h>
#  endif

#  if !defined(HAVE_LCHOWN)
#    define lchown chown
#  endif

#else
#  error "File API defined only for MS Windows and UNIX platforms"

#endif  /* NCBI_OS_MSWIN, NCBI_OS_UNIX */


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
const size_t kDefaultBufferSize = 32*1024;

// List of files for CFileDeleteAtExit class
static CSafeStaticRef< CFileDeleteList > s_DeleteAtExitFileList;


//////////////////////////////////////////////////////////////////////////////
//
// Static functions
//

// Construct real entry mode from parts. Parameters can not have "fDefault" 
// value.
static CDirEntry::TMode s_ConstructMode(CDirEntry::TMode user_mode, 
                                        CDirEntry::TMode group_mode, 
                                        CDirEntry::TMode other_mode)
{
    CDirEntry::TMode mode = 0;
    mode |= (user_mode  << 6);
    mode |= (group_mode << 3);
    mode |= other_mode;
    return mode;
}


//////////////////////////////////////////////////////////////////////////////
//
// CDirEntry
//

CDirEntry::CDirEntry(const string& name)
{
    Reset(name);
    m_DefaultMode[eUser]  = m_DefaultModeGlobal[eFile][eUser];
    m_DefaultMode[eGroup] = m_DefaultModeGlobal[eFile][eGroup];
    m_DefaultMode[eOther] = m_DefaultModeGlobal[eFile][eOther];
}


CDirEntry::CDirEntry(const CDirEntry& other)
    : m_Path(other.m_Path)
{
    m_DefaultMode[eUser]  = other.m_DefaultMode[eUser];
    m_DefaultMode[eGroup] = other.m_DefaultMode[eGroup];
    m_DefaultMode[eOther] = other.m_DefaultMode[eOther];
}


CDirEntry::~CDirEntry(void)
{
    return;
}


CDirEntry* CDirEntry::CreateObject(EType type, const string& path)
{
    CDirEntry *ptr = 0;
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
    if ( !ptr ) {
        NCBI_THROW(CCoreException, eNullPtr,
            "CDirEntry::CreateObject(): Cannot allocate memory for object");
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


CDirEntry::TMode CDirEntry::m_DefaultModeGlobal[eUnknown][3] =
{
    // eFile
    { CDirEntry::fDefaultUser, CDirEntry::fDefaultGroup, 
          CDirEntry::fDefaultOther },
    // eDir
    { CDirEntry::fDefaultDirUser, CDirEntry::fDefaultDirGroup, 
          CDirEntry::fDefaultDirOther },
    // ePipe
    { CDirEntry::fDefaultUser, CDirEntry::fDefaultGroup, 
          CDirEntry::fDefaultOther },
    // eLink
    { CDirEntry::fDefaultUser, CDirEntry::fDefaultGroup, 
          CDirEntry::fDefaultOther },
    // eSocket
    { CDirEntry::fDefaultUser, CDirEntry::fDefaultGroup, 
          CDirEntry::fDefaultOther },
    // eDoor
    { CDirEntry::fDefaultUser, CDirEntry::fDefaultGroup, 
          CDirEntry::fDefaultOther },
    // eBlockSpecial
    { CDirEntry::fDefaultUser, CDirEntry::fDefaultGroup, 
          CDirEntry::fDefaultOther },
    // eCharSpecial
    { CDirEntry::fDefaultUser, CDirEntry::fDefaultGroup, 
          CDirEntry::fDefaultOther }
};


// Default backup suffix
string CDirEntry::m_BackupSuffix = ".bak";


CDirEntry& CDirEntry::operator= (const CDirEntry& other)
{
    m_Path = other.m_Path;
    m_DefaultMode[eUser]  = other.m_DefaultMode[eUser];
    m_DefaultMode[eGroup] = other.m_DefaultMode[eGroup];
    m_DefaultMode[eOther] = other.m_DefaultMode[eOther];
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


bool CDirEntry::IsAbsolutePath(const string& path)
{
    if ( path.empty() )
        return false;
    char first = path[0];

    if ( IsPathSeparator(first) )
        return true;
#if defined(DISK_SEPARATOR)
    if ( path.find(DISK_SEPARATOR) != NPOS )
        return true;
#endif
    return false;
}


bool CDirEntry::IsAbsolutePathEx(const string& path)
{
    if ( path.empty() )
        return false;
    char first = path[0];

    // MAC or WIN absolute
    if ( path.find(':') != NPOS  &&  first != ':' )
        return true;

    // MAC relative path
    if ( first == ':' )
        return false;

    // UNIX or WIN absolute
    if ( first == '\\' || first == '/' )
        return true;

    // Else - relative
    return false;
}


/// Helper : strips dir to parts:
///     c:\a\b\     will be <c:><a><b>
///     /usr/bin/   will be </><usr><bin>
static void s_StripDir(const string& dir, vector<string> * dir_parts)
{
    dir_parts->clear();
    if ( dir.empty() ) 
        return;

    const char sep = CDirEntry::GetPathSeparator();

    size_t sep_pos = 0;
    size_t last_ind = dir.length() - 1;
    size_t part_start = 0;
    for (;;) {
        sep_pos = dir.find(sep, sep_pos);
        if (sep_pos == NPOS) {
            dir_parts->push_back(string(dir, part_start,
                                        dir.length() - part_start));
            break;
        }

        // If path starts from '/' - it's a root directory
        if (sep_pos == 0) {
            dir_parts->push_back(string(1, sep));
        } else {
            dir_parts->push_back(string(dir, part_start, sep_pos -part_start));
        }

        sep_pos++;
        part_start = sep_pos;
        if (sep_pos >= last_ind) 
            break;
    }

}


string CDirEntry::CreateRelativePath( const string& path_from, 
                                      const string& path_to )
{

#if defined(NCBI_OS_MSWIN)  ||  defined(NCBI_OS_UNIX)
    string path; // the result    
    
    if ( !IsAbsolutePath(path_from) ) {
        NCBI_THROW(CFileException, eRelativePath, 
                   "path_from is not absolute path");
    }

    if ( !IsAbsolutePath(path_to) ) {
        NCBI_THROW(CFileException, eRelativePath, 
                   "path_to is not absolute path");
    }

    // Split and strip FROM
    string dir_from;
    SplitPath(AddTrailingPathSeparator(path_from), &dir_from);
    vector<string> dir_from_parts;
    s_StripDir(dir_from, &dir_from_parts);
    if ( dir_from_parts.empty() ) {
        NCBI_THROW(CFileException, eRelativePath, 
                   "path_from is empty path");
    }

    // Split and strip TO
    string dir_to;
    string base_to;
    string ext_to;
    SplitPath(path_to, &dir_to, &base_to, &ext_to);    
    vector<string> dir_to_parts;
    s_StripDir(dir_to, &dir_to_parts);
    if ( dir_to_parts.empty() ) {
        NCBI_THROW(CFileException, eRelativePath, 
                   "path_to is empty path");
    }

// Platform-dependent compare mode
#ifdef NCBI_OS_MSWIN
#  define DIR_PARTS_CMP_MODE NStr::eNocase
#endif
#ifdef NCBI_OS_UNIX
#  define DIR_PARTS_CMP_MODE NStr::eCase
#endif
    // Roots must be the same to create relative path from one to another

    if (NStr::Compare(dir_from_parts.front(), 
                      dir_to_parts.front(), 
                      DIR_PARTS_CMP_MODE) != 0) {
        NCBI_THROW(CFileException, eRelativePath, 
                   "roots of input paths are different");
    }

    size_t min_parts = min(dir_from_parts.size(), dir_to_parts.size());
    size_t common_length = min_parts;
    for (size_t i = 0; i < min_parts; i++) {
        if (NStr::Compare(dir_from_parts[i], 
                          dir_to_parts[i],
                          DIR_PARTS_CMP_MODE) != 0) {
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
#else
    NCBI_THROW(CFileException, eRelativePath, 
               "not implemented");
    return string();
#endif
}


string CDirEntry::ConvertToOSPath(const string& path)
{
    // Not process empty and absolute path
    if ( path.empty() || IsAbsolutePathEx(path) ) {
        return path;
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
        if ( c == '\\' || c == '/' || c == ':') {
            xpath[i] = DIR_SEPARATOR;
        }
    }
    // Fix current and parent refs in the path after conversion from MAC path
    // Replace all "::" to "/../"
    string xsearch  = string(2,DIR_SEPARATOR);
    string xreplace = string(1,DIR_SEPARATOR) + DIR_PARENT + DIR_SEPARATOR;
    size_t pos = 0;
    while ((pos = xpath.find(xsearch, pos)) != NPOS ) {
        xpath.replace(pos, xsearch.length(), xreplace);
    }
    // Remove leading ":" in the relative path on non-MAC platforms 
    if ( xpath[0] == DIR_SEPARATOR ) {
        xpath.erase(0,1);
    }
    // Replace something like "../aaa/../bbb/ccc" with "../bbb/ccc"
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
    string path = NStr::TruncateSpaces(first);

    // Add trailing path separator to first part (OS independence)

    size_t pos = path.length();
    if ( pos  &&  string(ALL_OS_SEPARATORS).find(path.at(pos-1)) == NPOS ) {
        // Find used path separator
        char sep = GetPathSeparator();
        size_t sep_pos = path.find_last_of(ALL_OS_SEPARATORS);
        if ( sep_pos != NPOS ) {
            sep = path.at(sep_pos);
        }
        path += sep;
    }
    // Remove leading separator in "second" part
    string part = NStr::TruncateSpaces(second);
    if ( part.length() > 0  &&
         string(ALL_OS_SEPARATORS).find(part[0]) != NPOS ) {
        part.erase(0,1);
    }
    // Add second part
    path += part;
    return path;
}


string CDirEntry::NormalizePath(const string& path, EFollowLinks follow_links)
{
#if defined(NCBI_OS_MSWIN)  ||  defined(NCBI_OS_UNIX)
    static const char kSeps[] = { DIR_SEPARATOR,
#  ifdef DIR_SEPARATOR_ALT
                                  DIR_SEPARATOR_ALT,
#  endif
                                  '\0' };

    list<string> head;              // already resolved to our satisfaction
    string       current    = path; // to resolve next
    list<string> tail;              // to resolve afterwards
    int          link_depth = 0;

    while ( !current.empty()  ||  !tail.empty() ) {
        list<string> pretail;
        if ( !current.empty() ) {
            NStr::Split(current, kSeps, pretail, NStr::eNoMergeDelims);
            current.erase();
            if (pretail.front().empty()
#  ifdef DISK_SEPARATOR
                ||  pretail.front().find(DISK_SEPARATOR) != NPOS
#  endif
                ) {
                // Absolute path
                head.clear();
#  if defined(NCBI_OS_MSWIN)
                // Remove leading "\\?\". Replace leading "\\?\UNC\" with "\\".
                static const char* const kUNC[] = { "", "", "?", "UNC" };
                list<string>::iterator it = pretail.begin();
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
                case 1: case 3/*?*/: // normal volume-less absolute path
                    head.push_back(kEmptyStr);
                    break;
                }
#  endif
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
                head.pop_back();
                _ASSERT(head.empty());
            } else if (next == DIR_CURRENT) {
                // Leave out, since we already have content
                continue;
#  ifdef DISK_SEPARATOR
            } else if (last[last.size()-1] == DISK_SEPARATOR) {
                // Allow almost anything right after a volume specification
#  endif
            } else if (next.empty()) {
                continue; // leave out empty components in most cases
            } else if (next == DIR_PARENT) {
#  ifdef DISK_SEPARATOR
                SIZE_TYPE pos;
#  endif
                // Back up if possible, assuming existing path to be "physical"
                if (last.empty()) {
                    // Already at the root; .. is a no-op
                    continue;
#  ifdef DISK_SEPARATOR
                } else if ((pos = last.find(DISK_SEPARATOR) != NPOS)) {
                    last.erase(pos + 1);
#  endif
                } else if (last != DIR_PARENT) {
                    head.pop_back();
                    continue;
                }
            }
        }
#  if defined(NCBI_OS_UNIX)
        // Is there a Windows equivalent for readlink?
        if ( follow_links ) {
            string s(head.empty() ? next
                     : NStr::Join(head, string(1, DIR_SEPARATOR))
                     + DIR_SEPARATOR + next);
            char buf[PATH_MAX];
            int  length = readlink(s.c_str(), buf, sizeof(buf));
            if (length > 0) {
                current.assign(buf, length);
                if (++link_depth >= 1024) {
                    ERR_POST(Warning << "CDirEntry::NormalizePath: "
                             "Reached symlink depth limit " << link_depth
                             << " when resolving " << path);
                    follow_links = eIgnoreLinks;
                }
                continue;
            }
        }
#  endif
        // Normal case: just append the next element to head
        head.push_back(next);
    }
    if (head.size() == 1  &&  head.front().empty() ) {
        // root dir
        return string(1, DIR_SEPARATOR);
    }
    return NStr::Join(head, string(1, DIR_SEPARATOR));

#else // Not Unix or  Windows
    // NOT implemented!
    return path;
#endif
}


bool CDirEntry::GetMode(TMode* user_mode, TMode* group_mode, TMode* other_mode)
    const
{
    struct stat st;
    if (stat(GetPath().c_str(), &st) != 0) {
        return false;
    }
    // Other
    if (other_mode) {
        *other_mode = st.st_mode & 0007;
    }
    st.st_mode >>= 3;
    // Group
    if (group_mode) {
        *group_mode = st.st_mode & 0007;
    }
    st.st_mode >>= 3;
    // User
    if (user_mode) {
        *user_mode = st.st_mode & 0007;
    }
    return true;
}


bool CDirEntry::SetMode(TMode user_mode, TMode group_mode, TMode other_mode)
    const
{
    if (user_mode == fDefault) {
        user_mode = m_DefaultMode[eUser];
    }
    if (group_mode == fDefault) {
        group_mode = m_DefaultMode[eGroup];
    }
    if (other_mode == fDefault) {
        other_mode = m_DefaultMode[eOther];
    }
    TMode mode = s_ConstructMode(user_mode, group_mode, other_mode);

    return chmod(GetPath().c_str(), mode) == 0;
}


void CDirEntry::SetDefaultModeGlobal(EType entry_type, TMode user_mode, 
                                     TMode group_mode, TMode other_mode)
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
    m_DefaultModeGlobal[entry_type][eUser]  = user_mode;
    m_DefaultModeGlobal[entry_type][eGroup] = group_mode;
    m_DefaultModeGlobal[entry_type][eOther] = other_mode;
}


void CDirEntry::SetDefaultMode(EType entry_type, TMode user_mode, 
                               TMode group_mode, TMode other_mode)
{
    if ( user_mode == fDefault ) {
        user_mode = m_DefaultModeGlobal[entry_type][eUser];
    }
    if ( group_mode == fDefault ) {
        group_mode = m_DefaultModeGlobal[entry_type][eGroup];
    }
    if ( other_mode == fDefault ) {
        other_mode = m_DefaultModeGlobal[entry_type][eOther];
    }
    m_DefaultMode[eUser]  = user_mode;
    m_DefaultMode[eGroup] = group_mode;
    m_DefaultMode[eOther] = other_mode;
}


void CDirEntry::GetDefaultModeGlobal(EType  entry_type, TMode* user_mode,
                                     TMode* group_mode, TMode* other_mode)
{
    if ( user_mode ) {
        *user_mode = m_DefaultModeGlobal[entry_type][eUser];
    }
    if ( group_mode ) {
        *group_mode = m_DefaultModeGlobal[entry_type][eGroup];
    }
    if ( other_mode ) {
        *other_mode = m_DefaultModeGlobal[entry_type][eOther];
    }
}


void CDirEntry::GetDefaultMode(TMode* user_mode, TMode* group_mode,
                               TMode* other_mode) const
{
    if ( user_mode ) {
        *user_mode = m_DefaultMode[eUser];
    }
    if ( group_mode ) {
        *group_mode = m_DefaultMode[eGroup];
    }
    if ( other_mode ) {
        *other_mode = m_DefaultMode[eOther];
    }
}


#if defined(NCBI_OS_MSWIN)

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
    if ( !FileTimeToLocalFileTime(&filetime, &local) ) {
        return false;
    }
    // Convert the local file time from UTC to system time.
    if ( !FileTimeToSystemTime(&local, &system) ) {
        return false;
    }

    // Construct new time
    CTime newtime(system.wYear,
                  system.wMonth,
                  system.wDay,
                  system.wHour,
                  system.wMinute,
                  system.wSecond,
                  system.wMilliseconds *
                         (kNanoSecondsPerSecond / kMilliSecondsPerSecond),
                  CTime::eLocal,
                  t.GetTimeZonePrecision());

    // And assign it
    if ( t.GetTimeZoneFormat() == CTime::eLocal ) {
        t = newtime;
    } else {
        t = newtime.GetGmtTime();
    }
    return true;
}


bool s_CTimeToFileTime(const CTime& t, FILETIME& filetime) 
{
    return false;
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

#endif


bool CDirEntry::GetTime(CTime* modification,
                        CTime* creation, CTime* last_access) const
{
#if defined(NCBI_OS_MSWIN)
    HANDLE handle;
    WIN32_FIND_DATA buf;

    // Get file times using FindFile
    handle = FindFirstFile(GetPath().c_str(), &buf);
    if ( handle == INVALID_HANDLE_VALUE ) {
        return false;
    }
    FindClose(handle);

    // Convert file UTC times into CTime format
    if ( modification  &&
        !s_FileTimeToCTime(buf.ftLastWriteTime, *modification) ) {
        return false;
    }
    if ( creation  &&
        !s_FileTimeToCTime(buf.ftCreationTime, *creation) ) {
        return false;
    }
    if ( last_access  &&
         !s_FileTimeToCTime(buf.ftLastAccessTime, *last_access) ) {
        return false;
    }
    return true;

#elif defined(NCBI_OS_UNIX)

    struct SStat st;
    if ( Stat(&st) != 0 ) {
        return false;
    }

    if ( modification ) {
        modification->SetTimeT(st.orig.st_mtime);
	    if ( st.mtime_nsec )
            modification->SetNanoSecond(st.mtime_nsec);
    }
    if ( creation ) {
        creation->SetTimeT(st.orig.st_ctime);
	    if ( st.ctime_nsec )
            creation->SetNanoSecond(st.ctime_nsec);
    }
    if ( last_access ) {
        last_access->SetTimeT(st.orig.st_atime);
    	if ( st.atime_nsec )
            last_access->SetNanoSecond(st.atime_nsec);
    }
    return true;
#endif
}


bool CDirEntry::SetTime(CTime* modification,
                        CTime* creation, CTime* last_access) const
{
    if ( !modification  &&  !creation  &&  !last_access ) {
        return true;
    }

#if defined(NCBI_OS_MSWIN)

    FILETIME   x_modification, x_creation, x_lastaccess;
    LPFILETIME p_modification = NULL, p_creation= NULL, p_lastaccess = NULL;

    // Convert times to FILETIME format
    if ( modification ) {
        s_UnixTimeToFileTime(modification->GetTimeT(),
                             modification->NanoSecond(), x_modification);
        p_modification = &x_modification;
    }
    if ( creation ) {
        s_UnixTimeToFileTime(creation->GetTimeT(),
                             creation->NanoSecond(), x_creation);
        p_creation = &x_creation;
    }
    if ( last_access ) {
        s_UnixTimeToFileTime(last_access->GetTimeT(),
                             last_access->NanoSecond(), x_lastaccess);
        p_lastaccess = &x_lastaccess;
    }

    // Change times
    HANDLE h = CreateFile(GetPath().c_str(), FILE_WRITE_ATTRIBUTES,
                          FILE_SHARE_READ, NULL, OPEN_EXISTING,
                          FILE_FLAG_BACKUP_SEMANTICS /*for dirs*/, NULL); 
    if ( h == INVALID_HANDLE_VALUE ) {
        return false;
    }
    if ( !SetFileTime(h, p_creation, p_lastaccess, p_modification) ) {
        CloseHandle(h);
        return false;
    }
    CloseHandle(h);

    return true;

#elif defined(NCBI_OS_UNIX)

#  if defined(HAVE_UTIMES)
    // Get current times
    CTime x_modification, x_lastaccess;
    GetTime(modification ? &x_modification : 0,
            0 /* creation */,
	    last_access  ? &x_lastaccess : 0);

    if ( !modification ) {
        modification = &x_modification;
    }
    if ( !last_access ) {
        last_access = &x_lastaccess;
    }
    // Change times
    struct timeval tvp[2];
    tvp[0].tv_sec  = last_access->GetTimeT();
    tvp[0].tv_usec = last_access->NanoSecond() / 1000;
    tvp[1].tv_sec  = modification->GetTimeT();;
    tvp[1].tv_usec = modification->NanoSecond() / 1000;

#    if defined(HAVE_LUTIMES)
    return lutimes(GetPath().c_str(), tvp) == 0;
#    else
    return utimes(GetPath().c_str(), tvp) == 0;
#    endif

# else
    // utimes() does not exists on current platform,
    // so use less accurate utime().

    // Get current times
    time_t x_modification, x_lastaccess;
    GetTimeT(&x_modification, 0, &x_lastaccess);

    // Change times to new
    struct utimbuf times;
    times.modtime  = modification ? modification->GetTimeT() : x_modification;
    times.actime   = last_access  ? last_access->GetTimeT()  : x_lastaccess;
    return utime(GetPath().c_str(), &times) == 0;

#  endif // HAVE_UTIMES

#endif // OS selection
}


bool CDirEntry::GetTimeT(time_t* modification,
                         time_t* creation, time_t* last_access) const
{
    struct stat st;
    if (stat(GetPath().c_str(), &st) != 0) {
        return false;
    }
    if ( modification ) {
        *modification = st.st_mtime;
    }
    if ( creation ) {
        *creation = st.st_ctime;
    }
    if ( last_access ) {
        *last_access = st.st_atime;
    }
    return true;
}


bool CDirEntry::SetTimeT(time_t* modification,
                         time_t* creation, time_t* last_access) const
{
    if ( !modification  &&  !creation  &&  !last_access ) {
        return true;
    }

#if defined(NCBI_OS_MSWIN)

    FILETIME   x_modification, x_creation, x_lastaccess;
    LPFILETIME p_modification = NULL, p_creation= NULL, p_lastaccess = NULL;

    // Convert times to FILETIME format
    if ( modification ) {
        s_UnixTimeToFileTime(*modification, 0, x_modification);
        p_modification = &x_modification;
    }
    if ( creation ) {
        s_UnixTimeToFileTime(*creation, 0, x_creation);
        p_creation = &x_creation;
    }
    if ( last_access ) {
        s_UnixTimeToFileTime(*last_access, 0, x_lastaccess);
        p_lastaccess = &x_lastaccess;
    }

    // Change times
    HANDLE h = CreateFile(GetPath().c_str(), FILE_WRITE_ATTRIBUTES,
                          FILE_SHARE_READ, NULL, OPEN_EXISTING,
                          FILE_FLAG_BACKUP_SEMANTICS /*for dirs*/, NULL); 
    if ( h == INVALID_HANDLE_VALUE ) {
        return false;
    }
    if ( !SetFileTime(h, p_creation, p_lastaccess, p_modification) ) {
        CloseHandle(h);
        return false;
    }
    CloseHandle(h);

    return true;

#elif defined(NCBI_OS_UNIX)
    // Get current times
    time_t x_modification, x_lastaccess;
    GetTimeT(&x_modification, 0, &x_lastaccess);

    // Change times to new
    struct utimbuf times;
    times.modtime  = modification ? *modification : x_modification;
    times.actime   = last_access  ? *last_access  : x_lastaccess;
    return utime(GetPath().c_str(), &times) == 0;

#endif
}


int CDirEntry::Stat(struct SStat *buffer, EFollowLinks follow_links) const
{
    if ( !buffer ) {
        errno = EFAULT;
        return -1;
    }
    
    int errcode;
#if defined(NCBI_OS_MSWIN)
    errcode = stat(GetPath().c_str(), &buffer->orig);
#elif defined(NCBI_OS_UNIX)
    if (follow_links == eFollowLinks) {
        errcode = stat(GetPath().c_str(), &buffer->orig);
    } else {
        errcode = lstat(GetPath().c_str(), &buffer->orig);
    }
#endif
    if (errcode != 0) {
        return errcode;
    }
   
    // Assign additional fields
    buffer->mtime_nsec = 0;
    buffer->ctime_nsec = 0;
    buffer->atime_nsec = 0;
    
#if defined(NCBI_OS_MSWIN)
    return 0;

#elif defined(NCBI_OS_UNIX)

    // UNIX:
    // Some systems have additional fields in the stat structure to store
    // nanoseconds. If you know one more platform which have nanoseconds
    // support for file times, add it here.

#  if !defined(__GLIBC_PREREQ)
#    define __GLIBC_PREREQ(x, y) 0
#  endif

#  if defined(NCBI_OS_LINUX)  &&  __GLIBC_PREREQ(2,3)
#    if defined(__USE_MISC)
    buffer->mtime_nsec = buffer->orig.st_mtim.tv_nsec;
    buffer->ctime_nsec = buffer->orig.st_ctim.tv_nsec;
    buffer->atime_nsec = buffer->orig.st_atim.tv_nsec;
#    else
    buffer->mtime_nsec = buffer->orig.st_mtimensec;
    buffer->ctime_nsec = buffer->orig.st_ctimensec;
    buffer->atime_nsec = buffer->orig.st_atimensec;
#    endif
#  endif


#  if defined(NCBI_OS_SOLARIS)
#    if !defined(_XOPEN_SOURCE) && !defined(_POSIX_C_SOURCE) || \
	 defined(__EXTENSIONS__)
    buffer->mtime_nsec = buffer->orig.st_mtim.tv_nsec;
    buffer->ctime_nsec = buffer->orig.st_ctim.tv_nsec;
    buffer->atime_nsec = buffer->orig.st_atim.tv_nsec;
#    else
    buffer->mtime_nsec = buffer->orig.st_mtim.__tv_nsec;
    buffer->ctime_nsec = buffer->orig.st_ctim.__tv_nsec;
    buffer->atime_nsec = buffer->orig.st_atim.__tv_nsec;
#    endif
#  endif

   
#  if defined(NCBI_OS_BSD) || defined(NCBI_OS_DARWIN)
#    if defined(_POSIX_SOURCE)
    buffer->mtime_nsec = buffer->orig.st_mtimensec;
    buffer->ctime_nsec = buffer->orig.st_ctimensec;
    buffer->atime_nsec = buffer->orig.st_atimensec;
#    else
    buffer->mtime_nsec = buffer->orig.st_mtimespec.tv_nsec;
    buffer->ctime_nsec = buffer->orig.st_ctimespec.tv_nsec;
    buffer->atime_nsec = buffer->orig.st_atimespec.tv_nsec;
#    endif
#  endif


#  if defined(NCBI_OS_IRIX)
#    if defined(tv_sec)
    buffer->mtime_nsec = buffer->orig.st_mtim.__tv_nsec;
    buffer->ctime_nsec = buffer->orig.st_ctim.__tv_nsec;
    buffer->atime_nsec = buffer->orig.st_atim.__tv_nsec;
#    else
    buffer->mtime_nsec = buffer->orig.st_mtim.tv_nsec;
    buffer->ctime_nsec = buffer->orig.st_ctim.tv_nsec;
    buffer->atime_nsec = buffer->orig.st_atim.tv_nsec;
#    endif
#  endif
    
    return 0;
    
#endif  // NCBI_OS_*
}


CDirEntry::EType CDirEntry::GetType(EFollowLinks follow) const
{
    struct stat st;
    int errcode;

#if defined(NCBI_OS_MSWIN)
    errcode = stat(GetPath().c_str(), &st);
#elif defined(NCBI_OS_UNIX)
    if (follow == eFollowLinks) {
        errcode = stat(GetPath().c_str(), &st);
    } else {
        errcode = lstat(GetPath().c_str(), &st);
    }
#endif
    if (errcode != 0) {
        return eUnknown;
    }
    unsigned int mode = st.st_mode & S_IFMT;
    switch (mode) {
    case S_IFDIR:
        return eDir;
    case S_IFCHR:
        return eCharSpecial;
#if defined(NCBI_OS_MSWIN)
    case _S_IFIFO:
        return ePipe;
#elif defined(NCBI_OS_UNIX)
    case S_IFIFO:
        return ePipe;
    case S_IFLNK:
        return eLink;
    case S_IFSOCK:
        return eSocket;
    case S_IFBLK:
        return eBlockSpecial;
#  if defined(S_IFDOOR) /* only Solaris seems to have this */
    case S_IFDOOR:
        return eDoor;
#  endif
#endif
    }
    // Check regular file bit last
    if ( (st.st_mode & S_IFREG) == S_IFREG ) {
        return eFile;
    }
    return eUnknown;
}


string CDirEntry::LookupLink(void)
{
#if defined(NCBI_OS_UNIX)
    char buf[PATH_MAX];
    string name;
    int length = readlink(GetPath().c_str(), buf, sizeof(buf));
    if (length > 0) {
        name.assign(buf, length);
    }
    return name;
#else
    return kEmptyStr;
#endif
}


void CDirEntry::DereferenceLink(void)
{
    while ( IsLink() ) {
        string name = LookupLink();
        if ( name.empty() ) {
            return;
        }
        if ( IsAbsolutePath(name) ) {
            Reset(name);
        } else {
            string path = NormalizePath(MakePath(GetDir(), name));
            Reset(path);
        }
    }
}


bool CDirEntry::Rename(const string& newname, TRenameFlags flags)
{
    CFile src(*this);
    CFile dst(newname);

    // Dereference links
    if ( F_ISSET(flags, fRF_FollowLinks) ) {
        src.DereferenceLink();
        dst.DereferenceLink();
    }
    // The source entry must exists
    EType src_type = src.GetType();
    if ( src_type == eUnknown)  {
        return false;
    }
    EType dst_type   = dst.GetType();
    bool  dst_exists = (dst_type != eUnknown);
    
    // If destination exists...
    if ( dst_exists ) {
        // Can rename entries with different types?
        if ( F_ISSET(flags, fRF_EqualTypes)  &&  (src_type != dst_type) ) {
            return false;
        }
        // Can overwrite entry?
        if ( !F_ISSET(flags, fRF_Overwrite) ) {
            return false;
        }
        // Rename only if destination is older, otherwise just remove source
        if ( F_ISSET(flags, fRF_Update)  &&  !src.IsNewer(dst.GetPath())) {
            return src.Remove();
        }
        // Backup destination entry first
        if ( F_ISSET(flags, fRF_Backup) ) {
            // Use new CDirEntry object for 'dst', because its path
            // will be changed after backup
            CDirEntry dst_tmp(dst);
            if ( !dst_tmp.Backup(GetBackupSuffix(), eBackup_Rename) ) {
                return false;
            }
        }
        // Overwrite destination entry
        if ( F_ISSET(flags, fRF_Overwrite) ) {
            dst.Remove();
        } 
    }

    // On some platform rename() fails if destination entry exists, 
    // on others it can overwrite destination.
    // For consistency return FALSE if destination already exists.
    if ( dst.Exists() ) {
        return false;
    }
    // Rename
    if ( rename(src.GetPath().c_str(), dst.GetPath().c_str()) != 0 ) {
        return false;
    }
    Reset(newname);
    return true;
}


bool CDirEntry::Remove(EDirRemoveMode mode) const
{
    if ( IsDir(eIgnoreLinks) ) {
        if (mode == eOnlyEmpty) {
            return rmdir(GetPath().c_str()) == 0;
        } else {
            CDir dir(GetPath());
            return dir.Remove(eRecursive);
        }
    } else {
        return remove(GetPath().c_str()) == 0;
    }
}


bool CDirEntry::Backup(const string& suffix, EBackupMode mode,
                       TCopyFlags copyflags, size_t copybufsize)
{
    string backup_name = DeleteTrailingPathSeparator(GetPath()) +
                         (suffix.empty() ? GetBackupSuffix() : suffix);
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
            break;
    }
    return false;
}


bool CDirEntry::IsNewer(const string& entry_name) const
{
    CDirEntry entry(entry_name);
    CTime entry_time;
    if ( !entry.GetTime(&entry_time) ) {
        return true;
    }
    return IsNewer(entry_time);
}


bool CDirEntry::IsNewer(time_t tm) const
{
    time_t current;
    if ( !GetTimeT(&current) ) {
        return false;
    }
    return current > tm;
}


bool CDirEntry::IsNewer(const CTime& tm) const
{
    CTime current;
    if ( !GetTime(&current) ) {
        return false;
    }
    return current > tm;
}


#if defined(NCBI_OS_MSWIN)

// Helper function for GetOwner
bool s_LookupAccountSid(PSID sid, string* account, string* domain = 0)
{
    // Accordingly MSDN max account name size is 20, domain name size is 256.
    #define MAX_ACCOUNT_LEN  256

    char account_name[MAX_ACCOUNT_LEN];
    char domain_name [MAX_ACCOUNT_LEN];
    DWORD account_size = MAX_ACCOUNT_LEN;
    DWORD domain_size  = MAX_ACCOUNT_LEN;
    SID_NAME_USE use   = SidTypeUnknown;

    if ( !LookupAccountSid(NULL /*local computer*/, sid, 
                            account_name, (LPDWORD)&account_size,
                            domain_name,  (LPDWORD)&domain_size,
                            &use) ) {
        return false;
    }
    // Save account information
    if ( account )
        *account = account_name;
    if ( domain )
        *domain = domain_name;

    // Return result
    return true;
}


// Helper function for SetOwner
bool s_EnablePrivilege(HANDLE token, LPCTSTR privilege, BOOL enable = TRUE)
{
    // Get priviledge unique identifier
    LUID luid;
    if ( !LookupPrivilegeValue(NULL, privilege, &luid) ) {
        return false;
    }

    // Get current privilege setting

    TOKEN_PRIVILEGES tp;
    TOKEN_PRIVILEGES tp_prev;
    DWORD            tp_prev_size = sizeof(TOKEN_PRIVILEGES);
    
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = 0;

    AdjustTokenPrivileges(token, FALSE, &tp, sizeof(TOKEN_PRIVILEGES),
                            &tp_prev, &tp_prev_size);
    if ( GetLastError() != ERROR_SUCCESS ) {
        return false;
    }

    // Set privilege based on received setting

    tp_prev.PrivilegeCount     = 1;
    tp_prev.Privileges[0].Luid = luid;

    if ( enable ) {
        tp_prev.Privileges[0].Attributes |= (SE_PRIVILEGE_ENABLED);
    } else {
        tp_prev.Privileges[0].Attributes ^= 
            (SE_PRIVILEGE_ENABLED & tp_prev.Privileges[0].Attributes);
    }
    AdjustTokenPrivileges(token, FALSE, &tp_prev, tp_prev_size,
                            NULL, NULL);
    if ( GetLastError() != ERROR_SUCCESS ) {
        return false;
    }

    // Privilege settings changed
    return true;
}
#endif


bool CDirEntry::GetOwner(string* owner, string* group,
                         EFollowLinks follow) const
{
    if ( !owner  &&  !group ) {
        return false;
    }

#if defined(NCBI_OS_MSWIN)

    PSID sid_owner;
    PSID sid_group;
    PSECURITY_DESCRIPTOR sd = NULL;

    if ( GetNamedSecurityInfo(
            (LPTSTR)GetPath().c_str(),
            SE_FILE_OBJECT,
            OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION,
            &sid_owner,
            &sid_group,
            NULL, NULL,
            &sd ) != ERROR_SUCCESS ) {
      return 1;
    }
    // Get owner
    if ( owner  &&  !s_LookupAccountSid(sid_owner, owner) ) {
        LocalFree(sd);
        return false;
    }
    // Get group
    if ( group  &&  !s_LookupAccountSid(sid_group, group) ) {
        // This is not an error, because the group name on WINDOWS
        // is an auxiliary information. Sometimes accounts can not
        // belongs to groups, or we dont have permissions to get
        // such information.
        *group = kEmptyStr;
    }
    LocalFree(sd);
    return true;

#elif defined(NCBI_OS_UNIX)

    struct stat st;
    int errcode;
    
    if ( follow == eFollowLinks ) {
        errcode = stat(GetPath().c_str(), &st);
    } else {
        errcode = lstat(GetPath().c_str(), &st);
    }
    if ( errcode != 0 ) {
        return false;
    }
    
    if ( owner ) {
        struct passwd *pw = getpwuid(st.st_uid);
        if ( !pw )
	    return false;
        *owner = pw->pw_name;
    }
    if ( group ) {
        struct group *gr = getgrgid(st.st_gid);
        if ( !gr )
	    return false;
        *group = gr->gr_name;
    }
    return true;
#endif
}


bool CDirEntry::SetOwner(const string& owner, const string& group,
                         EFollowLinks follow) const
{
#if defined(NCBI_OS_MSWIN)

    if ( owner.empty() ) {
        return false;
    }

    // Get access token

    HANDLE process = GetCurrentProcess();
    if ( !process ) {
        return false;
    }
    HANDLE token = INVALID_HANDLE_VALUE;
    if ( !OpenProcessToken(process, TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                           &token) ) {
        return false;
    }

    // Enable privilegies, if failed try without it
    s_EnablePrivilege(token, SE_TAKE_OWNERSHIP_NAME);
    s_EnablePrivilege(token, SE_RESTORE_NAME);
    s_EnablePrivilege(token, SE_BACKUP_NAME);

    PSID  sid     = NULL;
    char* domain  = NULL;
    bool  success = true;

    try {

        //
        // Get SID for new owner
        //

        // First call to LookupAccountName to get the buffer sizes
        DWORD sid_size    = 0;
        DWORD domain_size = 0;
        SID_NAME_USE use  = SidTypeUnknown;
        BOOL res;
        res = LookupAccountName(NULL, owner.c_str(),
                                NULL, &sid_size, 
                                NULL, &domain_size, &use);
        if ( !res  &&  GetLastError() != ERROR_INSUFFICIENT_BUFFER )
            throw(0);

        // Reallocate memory for the buffers
        sid    = (PSID) malloc(sid_size);
        domain = (char*)malloc(domain_size);
        if ( !sid  || !domain ) {
            throw(0);
        }

        // Second call to LookupAccountName to get the account info
        if ( !LookupAccountName(NULL, owner.c_str(),
                                sid, &sid_size, 
                                domain, &domain_size, &use) ) {
            // Unknown local user
            throw(0);
        }

        //
        // Change owner
        //

        // Security descriptor (absolute format)
        UCHAR sd_abs_buf [SECURITY_DESCRIPTOR_MIN_LENGTH];
        PSECURITY_DESCRIPTOR sd = (PSECURITY_DESCRIPTOR)&sd_abs_buf;

        // Build security descriptor in absolute format
        if ( !InitializeSecurityDescriptor(
                sd, SECURITY_DESCRIPTOR_REVISION)) {
            throw(0);
        }
        // Modify security descriptor owner.
        // FALSE - because new owner was explicitly specified.
        if ( !SetSecurityDescriptorOwner(sd, sid, FALSE)) {
            throw(0);
        }
        // Check security descriptor
        if ( !IsValidSecurityDescriptor(sd) ) {
            throw(0);
        }
        // Set new security information for the file object
        if ( !SetFileSecurity(GetPath().c_str(),
                (SECURITY_INFORMATION)(OWNER_SECURITY_INFORMATION), sd) ) {
            throw(0);
        }
    }
    catch (int) {
        success = false;
    }

    // Clean up
    if ( sid )    free(sid);
    if ( domain ) free(domain);
    CloseHandle(token);

    // Return result
    return success;

#elif defined(NCBI_OS_UNIX)

    if ( owner.empty() &&  group.empty() ) {
        return false;
    }

    struct stat st;
    int errcode;
    
    if ( follow == eFollowLinks ) {
        errcode = stat(GetPath().c_str(), &st);
    } else {
        errcode = lstat(GetPath().c_str(), &st);
    }
    if ( errcode != 0 ) {
        return false;
    }
    
    uid_t uid = uid_t(-1);
    gid_t gid = gid_t(-1);
    
    if ( !owner.empty() ) {
        struct passwd *pw = getpwnam(owner.c_str());
        if ( !pw )
            return false;
        uid = pw->pw_uid;
    }
    if ( !group.empty() ) {
        struct group *gr = getgrnam(group.c_str());
        if ( !gr )
	    return false;
        gid = gr->gr_gid;
    }
    
    if ( follow == eFollowLinks ) {
        if ( chown(GetPath().c_str(), uid, gid) ) {
            return false;
        }
    } else {
#  if defined(HAVE_LCHOWN)
        if ( lchown(GetPath().c_str(), uid, gid) ) {
            return false;
        }
#  endif
    }
    return true;

#endif
}


// Helper: Copy attributes (owner/date/time) from one entry to another.
// Both entries should have equal type.
//
// UNIX:
//     In mostly cases only super-user can change owner for
//     destination entry.  The owner of a file may change the group of
//     the file to any group of which that owner is a member.
// WINDOWS:
//     This function doesn't support ownerhip change yet.
//
static bool s_CopyAttrs(const char* from, const char* to,
                        CDirEntry::EType type, CDirEntry::TCopyFlags flags)
{
#if defined(NCBI_OS_UNIX)

    CDirEntry::SStat st;
    if ( CDirEntry(from).Stat(&st) != 0 ) {
        return false;
    }

    // Date/time.
    // Set time before chmod() call, because on some platforms
    // setting time can affect file mode also.
    if ( F_ISSET(flags, CDirEntry::fCF_PreserveTime) ) {
#  if defined(HAVE_UTIMES)
        struct timeval tvp[2];
        tvp[0].tv_sec  = st.orig.st_atime;
        tvp[0].tv_usec = st.atime_nsec / 1000;
        tvp[1].tv_sec  = st.orig.st_mtime;
        tvp[1].tv_usec = st.mtime_nsec / 1000;
#    if defined(HAVE_LUTIMES)
        return lutimes(to, tvp) == 0;
#    else
        return utimes(to, tvp) == 0;
#    endif
# else  // !HAVE_UTIMES
        // utimes() does not exists on current platform,
        // so use less accurate utime().
        struct utimbuf times;
        times.modtime = st.orig.st_mtime;
        times.actime  = st.orig.st_atime;
        return utime(to, &times) == 0;
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
                    return false;
                }
            }
#  endif
            // We cannot change permissions for sym.links.
            return true;
        } else {
            // Changing the ownership probably can fails, unless we're super-user.
            // The uid/gid bits can be cleared by OS. If chown() fails,
            // lose uid/gid bits.
            if ( chown(to, st.orig.st_uid, st.orig.st_gid) ) {
                if ( errno != EPERM ) {
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
            return false;
        }
    }
    return true;


#elif defined(NCBI_OS_MSWIN)

    CDirEntry efrom(from), eto(to);

    WIN32_FILE_ATTRIBUTE_DATA attr;
    if ( !::GetFileAttributesEx(from, GetFileExInfoStandard, &attr) ) {
        return false;
    }

    // Date/time
    if ( F_ISSET(flags, CDirEntry::fCF_PreserveTime) ) {
        HANDLE h = CreateFile(to, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING,
                              FILE_FLAG_BACKUP_SEMANTICS /*for dirs*/, NULL); 
        if ( h == INVALID_HANDLE_VALUE ) {
            return false;
        }
        if ( !SetFileTime(h, &attr.ftCreationTime, &attr.ftLastAccessTime,
                        &attr.ftLastWriteTime) ) {
            CloseHandle(h);
            return false;
        }
        CloseHandle(h);
    }

    // Permissions
    if ( F_ISSET(flags, CDirEntry::fCF_PreservePerm) ) {
        if ( !::SetFileAttributes(to, attr.dwFileAttributes) ) {
            return false;
        }
    }

    // Owner
    if ( F_ISSET(flags, CDirEntry::fCF_PreserveOwner) ) {
        string owner, group;
        if ( !efrom.GetOwner(&owner, &group) ) {
            return false;
        }
        // We dont check result here, because often is not impossible
        // to save an original owner name without administators rights.
        eto.SetOwner(owner, group);
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
    struct stat buf;
    if ( stat(GetPath().c_str(), &buf) != 0 ) {
        return -1;
    }
    return buf.st_size;
}


string CFile::GetTmpName(ETmpFileCreationMode mode)
{
#if defined(NCBI_OS_MSWIN)  ||  defined(NCBI_OS_UNIX)
    return GetTmpNameEx(kEmptyStr, kEmptyStr, mode);
#else
    if (mode == eTmpFileCreate) {
        ERR_POST(Warning << "CFile::GetTmpNameEx: "
                 "The temporary file cannot be auto-created on this " \
                 "platform, returns its name only");
    }
    char* filename = tempnam(0,0);
    if ( !filename ) {
        return kEmptyStr;
    }
    string res(filename);
    free(filename);
    return res;
#endif
}


#if !defined(NCBI_OS_MSWIN)

// Auxiliary function to copy file
bool s_CopyFile(const char* src, const char* dst, size_t buf_size)
{
    CNcbiIfstream is(src, IOS_BASE::binary | IOS_BASE::in);
    CNcbiOfstream os(dst, IOS_BASE::binary | IOS_BASE::out | IOS_BASE::trunc);

    if ( !buf_size ) {
        buf_size = kDefaultBufferSize;
    }
    char* buf = new char[buf_size];
    bool failed = false;

    streamsize nread;
    do {
        nread = is.rdbuf()->sgetn(buf, buf_size);
        if ( nread ) {
            streamsize nwrite = os.rdbuf()->sputn(buf, nread);
            if ( nwrite != nread ) {
                failed = true;
                break;
            }
        }
    } while ( nread );

    // Clean memory
    delete buf;
    // Return copy result
    return !failed;
}
#endif


bool CFile::Copy(const string& newname, TCopyFlags flags, size_t buf_size)
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
        return false;
    }
    EType dst_type   = dst.GetType();
    bool  dst_exists = (dst_type != eUnknown);
    
    // If destination exists...
    if ( dst_exists ) {
        // Can copy entries with different types?
        // The Destination must be a file too.
        if ( F_ISSET(flags, fCF_EqualTypes)  &&  (src_type != dst_type) ) {
            return false;
        }
        // Can overwrite entry?
        if ( !F_ISSET(flags, fCF_Overwrite) ) {
            return false;
        }
        // Copy only if destination is older, otherwise just remove source
        if ( F_ISSET(flags, fCF_Update)  &&  !src.IsNewer(dst.GetPath())) {
            return src.Remove();
        }
        // Backup destination entry first
        if ( F_ISSET(flags, fCF_Backup) ) {
            // Use new CDirEntry object for 'dst', because its path
            // will be changed after backup
            CDirEntry dst_tmp(dst);
            if ( !dst_tmp.Backup(GetBackupSuffix(), eBackup_Rename) ) {
                return false;
            }
        }
    }

    // Copy
#if defined(NCBI_OS_MSWIN)
    if ( !::CopyFile(src.GetPath().c_str(), dst.GetPath().c_str(), FALSE) )
        return false;
#else
    if ( !s_CopyFile(src.GetPath().c_str(), dst.GetPath().c_str(), buf_size)){
        return false;
    }
#endif

    // Verify copied data
    if ( F_ISSET(flags, fCF_Verify)  &&  !src.Compare(dst.GetPath()) ) {
        return false;
    }

    // Preserve attributes.
#if defined(NCBI_OS_MSWIN)
    // On MS Windows ::CopyFile() already preserved file attributes
    // and all date/times.
    flags &= ~(fCF_PreservePerm | fCF_PreserveTime);
#endif
    if ( flags & fCF_PreserveAll ) {
        if ( !s_CopyAttrs(src.GetPath().c_str(),
                          dst.GetPath().c_str(), eFile, flags) ) {
            return false;
        }
    } else {
        if ( !dst.SetMode(fDefault, fDefault, fDefault) ) {
            return false;
        }
    }
    return true;
}


 bool CFile::Compare(const string& file, size_t buf_size)
{
    if ( CFile(GetPath()).GetLength() != CFile(file).GetLength() ) {
        return false;
    }
    CNcbiIfstream f1(GetPath().c_str(), IOS_BASE::binary | IOS_BASE::in);
    CNcbiIfstream f2(file.c_str(),      IOS_BASE::binary | IOS_BASE::in);

    if ( !buf_size ) {
        buf_size = kDefaultBufferSize;
    }
    char* buf1  = new char[buf_size];
    char* buf2  = new char[buf_size];
    bool  equal = true;

    while ( f1.good()  &&  f2.good() ) {
        // Fill buffers
        f1.read(buf1, buf_size);
        f2.read(buf2, buf_size);
        if ( f1.gcount() != f2.gcount() ) {
            equal = false;
            break;
        }
        // Compare
        if ( memcmp(buf1, buf2, f1.gcount()) != 0 ) {
            equal = false;
            break;
        }
    }
    // Clean memory
    delete buf1;
    delete buf2;

    // Both files should be in the EOF state
    return equal  &&  f1.eof()  &&  f2.eof();
}


#if !defined(NCBI_OS_UNIX)

static string s_StdGetTmpName(const char* dir, const char* prefix)
{
    char* filename = tempnam(dir, prefix);
    if ( !filename ) {
        return kEmptyStr;
    }
    string str(filename);
    free(filename);
    return str;
}

#endif


string CFile::GetTmpNameEx(const string&        dir, 
                           const string&        prefix,
                           ETmpFileCreationMode mode)
{
#if defined(NCBI_OS_MSWIN)  ||  defined(NCBI_OS_UNIX)
    string x_dir = dir;
    if ( x_dir.empty() ) {
        x_dir = CDir::GetTmpDir();
    }
    if ( !x_dir.empty() ) {
        x_dir = AddTrailingPathSeparator(x_dir);
    }

    string fn;

#  if defined(NCBI_OS_UNIX)
    string pattern = x_dir + prefix + "XXXXXX";
    AutoPtr<char, CDeleter<char> > filename(strdup(pattern.c_str()));
    int fd = mkstemp(filename.get());
    close(fd);
    if (mode != eTmpFileCreate) {
        remove(filename.get());
    }
    fn = filename.get();

#  elif defined(NCBI_OS_MSWIN)
    if (mode == eTmpFileGetName) {
        fn = s_StdGetTmpName(dir.c_str(), prefix.c_str());
    } else {
        char   buffer[MAX_PATH];
        HANDLE hFile = INVALID_HANDLE_VALUE;

        srand((unsigned)time(0));
        unsigned long ofs = rand();

        while ( ofs < numeric_limits<unsigned long>::max() ) {
            _ultoa((unsigned long)ofs, buffer, 24);
            fn = x_dir + prefix + buffer;
            hFile = CreateFile(fn.c_str(), GENERIC_ALL, 0, NULL,
                               CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY, NULL);
            if (hFile != INVALID_HANDLE_VALUE) {
                break;
            }
            ofs++;
        }
        CloseHandle(hFile);
        if (ofs == numeric_limits<unsigned long>::max() ) {
            return kEmptyStr;
        }
        if (mode != eTmpFileCreate) {
            remove(fn.c_str());
        }
    }

#  endif

#else // defined(NCBI_OS_MSWIN)  ||  defined(NCBI_OS_UNIX)
    if (mode == eTmpFileCreate) {
        ERR_POST(Warning << "CFile::GetTmpNameEx: "
                 "The file cannot be auto-created on this platform, " \
                 "return its name only");
    }
    fn = s_StdGetTmpName(dir.c_str(), prefix.c_str());
#endif
    return fn;
}


class CTmpStream : public fstream
{
public:
    CTmpStream(const char* s, IOS_BASE::openmode mode) : fstream(s, mode) 
    {
        m_FileName = s; 
    }
    virtual ~CTmpStream(void) 
    { 
        close();
        CFile(m_FileName).Remove();
    }
protected:
    string m_FileName;
};


fstream* CFile::CreateTmpFile(const string& filename, 
                              ETextBinary text_binary,
                              EAllowRead  allow_read)
{
    ios::openmode mode = ios::out | ios::trunc;
    if ( text_binary == eBinary ) {
        mode = mode | ios::binary;
    }
    if ( allow_read == eAllowRead ) {
        mode = mode | ios::in;
    }
    string tmpname = filename.empty() ? GetTmpName(eTmpFileCreate) : filename;
    if ( tmpname.empty() ) {
        return 0;
    }
    fstream* stream = new CTmpStream(tmpname.c_str(), mode);
    // Try to remove file and OS will automatically delete it after
    // the last file descriptor to it is closed (works only on UNIXes)
    CFile(tmpname).Remove();
    return stream;
}


fstream* CFile::CreateTmpFileEx(const string& dir, const string& prefix,
                                ETextBinary text_binary, 
                                EAllowRead allow_read)
{
    return CreateTmpFile(GetTmpNameEx(dir, prefix, eTmpFileCreate),
                         text_binary, allow_read);
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
        return false;
    }
    home = pwd->pw_dir;
    return true;
}

static bool s_GetHomeByLOGIN(string& home)
{
    char* ptr = 0;
    // Get user name
    if ( !(ptr = getenv("USER")) ) {
        if ( !(ptr = getenv("LOGNAME")) ) {
            if ( !(ptr = getlogin()) ) {
                return false;
            }
        }
    }
    // Get home dir for this user
    struct passwd* pwd = getpwnam(ptr);
    if ( !pwd ||  pwd->pw_dir[0] == '\0') {
        return false;
    }
    home = pwd->pw_dir;
    return true;
}
#endif // NCBI_OS_UNIX


string CDir::GetHome(void)
{
    char*  str;
    string home;

#if defined(NCBI_OS_MSWIN)
    // Get home dir from environment variables
    // like - C:\Documents and Settings\user\Application Data
    str = getenv("APPDATA");
    if ( str ) {
        home = str;
    } else {
        // like - C:\Documents and Settings\user
        str = getenv("USERPROFILE");
        if ( str ) {
            home = str;
        }
    }
#elif defined(NCBI_OS_UNIX)
    // Try get home dir from environment variable
    str = getenv("HOME");
    if ( str ) {
        home = str;
    } else {
        // Try to retrieve the home dir -- first use user's ID,
        //  and if failed, then use user's login name.
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

    char* tmpdir = getenv("TEMP");
    if ( tmpdir ) {
        tmp = tmpdir;
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


string CDir::GetCwd()
{
    string cwd;

#if defined(NCBI_OS_UNIX)
    char buf[4096];
    if ( getcwd(buf, sizeof(buf) - 1) ) {
        cwd = buf;
    }
#elif defined(NCBI_OS_MSWIN)
    char buf[4096];
    if ( _getcwd(buf, sizeof(buf) - 1) ) {
        cwd = buf;
    }
#endif
    return cwd;
}


CDir::~CDir(void)
{
    return;
}


// Helper function for GetEntries(). Add entry to the list.
static void s_AddDirEntry(CDir::TEntries& contents, const string& path)
{
    contents.push_back(
        CDirEntry::CreateObject(CDirEntry(path).GetType(), path)
    );
}


CDir::TEntries CDir::GetEntries(const string&   mask,
                                EGetEntriesMode mode,
                                NStr::ECase     use_case) const
{
    CMaskFileName masks;
    if ( !mask.empty() ) {
        masks.Add(mask);
    }
    return GetEntries(masks, mode, use_case);
}


CDir::TEntries CDir::GetEntries(const vector<string>&  masks,
                                EGetEntriesMode        mode,
                                NStr::ECase            use_case) const
{
    if ( masks.empty() ) {
        return GetEntries("", mode, use_case);
    }
    TEntries contents;
    string path_base = AddTrailingPathSeparator(GetPath());

#if defined(NCBI_OS_MSWIN)

    // Append to the "path" mask for all files in directory
    string pattern = path_base + string("*");

    bool skip_recursive_entry;

    // Open directory stream and try read info about first entry
    struct _finddata_t entry;
    long desc = _findfirst(pattern.c_str(), &entry);
    if ( desc != -1 ) {
        skip_recursive_entry = (mode == eIgnoreRecursive)  &&
                                ((::strcmp(entry.name, ".") == 0) ||
                                    (::strcmp(entry.name, "..") == 0));
        // check all masks
        if ( !skip_recursive_entry ) {
            ITERATE(vector<string>, it, masks) {
                const string& mask = *it;
                if ( mask.empty()  ||
                     MatchesMask(entry.name, mask.c_str(), use_case) ) {
                    s_AddDirEntry(contents, path_base + entry.name);
                    break;
                }                
            } // ITERATE
        }

        while ( _findnext(desc, &entry) != -1 ) {
            if ( (mode == eIgnoreRecursive)  &&
                 ((::strcmp(entry.name, ".") == 0) ||
                  (::strcmp(entry.name, "..") == 0)) ) {
                continue;
            }
            ITERATE(vector<string>, it, masks) {
                const string& mask = *it;
                if ( mask.empty()  ||
                     MatchesMask(entry.name, mask.c_str(), use_case) ) {
                    s_AddDirEntry(contents, path_base + entry.name);
                    break;
                }
            } // ITERATE
        }
        _findclose(desc);
    }


#elif defined(NCBI_OS_UNIX)
    DIR* dir = opendir(GetPath().c_str());
    if ( dir ) {
        while (struct dirent* entry = readdir(dir)) {
            if ( (mode == eIgnoreRecursive) &&
                 ((::strcmp(entry->d_name, ".") == 0) ||
                  (::strcmp(entry->d_name, "..") == 0)) ) {
                continue;
            }
            ITERATE(vector<string>, it, masks) {
                const string& mask = *it;
                if ( mask.empty()  ||
                     MatchesMask(entry->d_name, mask.c_str(), use_case) ) {
                    s_AddDirEntry(contents, path_base + entry->d_name);
                    break;
                }
            } // ITERATE
        } // while
        closedir(dir);
    }
#endif
    return contents;
}


CDir::TEntries CDir::GetEntries(const CMask&    masks,
                                EGetEntriesMode mode,
                                NStr::ECase     use_case) const
{
    TEntries contents;
    string path_base = AddTrailingPathSeparator(GetPath());

#if defined(NCBI_OS_MSWIN)
    // Append to the "path" mask for all files in directory
    string pattern = path_base + "*";

    // Open directory stream and try read info about first entry
    struct _finddata_t entry;
    long desc = _findfirst(pattern.c_str(), &entry);
    if ( desc != -1 ) {
        if ( masks.Match(entry.name, use_case) &&
             !( (mode == eIgnoreRecursive)  && 
                ((::strcmp(entry.name, ".") == 0) ||
                 (::strcmp(entry.name, "..") == 0)) ) ) {
            s_AddDirEntry(contents, path_base + entry.name);
        }
        while ( _findnext(desc, &entry) != -1 ) {
            if ( masks.Match(entry.name, use_case) ) {
                if ( (mode == eIgnoreRecursive)  &&
                     ((::strcmp(entry.name, ".")  == 0) ||
                      (::strcmp(entry.name, "..") == 0)) ) {
                      continue;
                }
                s_AddDirEntry(contents, path_base + entry.name);
            }
        }
        _findclose(desc);
    }

#elif defined(NCBI_OS_UNIX)
    DIR* dir = opendir(GetPath().c_str());
    if ( dir ) {
        while (struct dirent* entry = readdir(dir)) {
            if ( masks.Match(entry->d_name, use_case) ) {
                if ( (mode == eIgnoreRecursive)  &&
                     ((::strcmp(entry->d_name, ".")  == 0) ||
                      (::strcmp(entry->d_name, "..") == 0)) ) {
                      continue;
                }
                s_AddDirEntry(contents, path_base + entry->d_name);
            }
        }
        closedir(dir);
    }
#endif
    return contents;
}


bool CDir::Create(void) const
{
    TMode user_mode, group_mode, other_mode;
    GetDefaultMode(&user_mode, &group_mode, &other_mode);
    TMode mode = s_ConstructMode(user_mode, group_mode, other_mode);

#if defined(NCBI_OS_MSWIN)
    errno = 0;
    if ( mkdir(GetPath().c_str()) != 0  &&  errno != EEXIST ) {
        return false;
    }
    return chmod(GetPath().c_str(), mode) == 0;

#elif defined(NCBI_OS_UNIX)
    errno = 0;
    if ( mkdir(GetPath().c_str(), mode) != 0  &&  errno != EEXIST ) {
        return false;
    }
    return true;
#endif
}

bool CDir::CreatePath(void) const
{
    if ( Exists() ) {
        return true;
    }
    string path(GetPath());
    if ( path.empty() ) {
        return true;
    }
    if ( path[path.length()-1] == GetPathSeparator() ) {
        path.erase(path.length() - 1);
    }
    CDir dir_this(path);
    if ( dir_this.Exists() ) {
        return true;
    }
    string path_up = dir_this.GetDir();
    if ( path_up == path ) {
        // special case: is this a disk name?
        return true;
    } 
    CDir dir_up(path_up);
    if ( dir_up.CreatePath() ) {
        return dir_this.Create();
    }
    return false;
}


bool CDir::Copy(const string& newname, TCopyFlags flags, size_t buf_size)
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
        return false;
    }
    EType dst_type   = dst.GetType();
    bool  dst_exists = (dst_type != eUnknown);
    
    // If destination exists...
    if ( dst_exists ) {
        // Can rename entries with different types?
        if ( F_ISSET(flags, fCF_EqualTypes)  &&  (src_type != dst_type) ) {
            return false;
        }

        // Some operation can be made for top directory only

        if ( F_ISSET(flags, fCF_TopDirOnly) ) {
            // Can overwrite entry?
            if ( !F_ISSET(flags, fCF_Overwrite) ) {
                return false;
            }
            // Copy only if destination is older, otherwise just remove source
            if ( F_ISSET(flags, fCF_Update)  && !src.IsNewer(dst.GetPath())) {
                return src.Remove(eRecursive);
            }
            // Backup destination entry first
            if ( F_ISSET(flags, fCF_Backup) ) {
                // Use new CDirEntry object for 'dst', because its path
                // will be changed after backup
                CDirEntry dst_tmp(dst);
                if ( !dst_tmp.Backup(GetBackupSuffix(), eBackup_Rename) ) {
                    return false;
                }
                // Create target directory
                if ( !dst.CreatePath() ) {
                    return false;
                }
            }
            // Remove unneeded flags.
            // All dir entries can be overwritten.
            flags &= ~(fCF_TopDirOnly | fCF_Update | fCF_Backup);
        }
    } else {
        // Create target directory
        if ( !dst.CreatePath() ) {
            return false;
        }
    }

    // Read all entries in source directory
    TEntries contents = src.GetEntries("*", eIgnoreRecursive);

    // And copy each of them to target directory
    ITERATE(TEntries, e, contents) {
        CDirEntry& entry = **e;
        if ( !F_ISSET(flags, fCF_Recursive)  &&
             entry.IsDir(follow ? eFollowLinks : eIgnoreLinks)) {
            continue;
        }
        // Copy entry
        if ( !entry.CopyToDir(dst.GetPath(), flags, buf_size) ) {
            return false;
        }
    }

    // Preserve attributes
    if ( flags & fCF_PreserveAll ) {
        if ( !s_CopyAttrs(src.GetPath().c_str(),
                          dst.GetPath().c_str(), eDir, flags) ) {
            return false;
        }
    } else {
        if ( !dst.SetMode(fDefault, fDefault, fDefault) ) {
            return false;
        }
    }
    return true;
}


bool CDir::Remove(EDirRemoveMode mode) const
{
    // Remove directory as empty
    if ( mode == eOnlyEmpty ) {
        return CParent::Remove(eOnlyEmpty);
    }
    // Read all entries in derectory
    TEntries contents = GetEntries();

    // Remove
    ITERATE(TEntries, entry, contents) {
        string name = (*entry)->GetName();
        if ( name == "."  ||  name == ".."  ||  
             name == string(1,GetPathSeparator()) ) {
            continue;
        }
        // Get entry item with full pathname
        CDirEntry item(GetPath() + GetPathSeparator() + name);

        if ( mode == eRecursive ) {
            if ( !item.Remove(eRecursive) ) {
                return false;
            }
        } else {
            if ( item.IsDir(eIgnoreLinks) ) {
                continue;
            }
            if ( !item.Remove() ) {
                return false;
            }
        }
    }
    // Remove main directory
    return CParent::Remove(eOnlyEmpty);
}


//////////////////////////////////////////////////////////////////////////////
//
// CSymLink
//

CSymLink::~CSymLink(void)
{ 
    return;
}


bool CSymLink::Create(const string& path)
{
#if defined(NCBI_OS_UNIX)
    char buf[PATH_MAX];
    int  len = readlink(GetPath().c_str(), buf, sizeof(buf));
    if ( len != -1 ) {
        return false;
    }
    if ( symlink(path.c_str(), GetPath().c_str()) ) {
        return false;
    }
    return true;
#else
    return false;
#endif
}


bool CSymLink::Copy(const string& new_path, TCopyFlags flags, size_t buf_size)
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
    if ( src_type == eUnknown)  {
        return false;
    }
    CSymLink dst(new_path);
    EType dst_type   = dst.GetType(eIgnoreLinks);
    bool  dst_exists = (dst_type != eUnknown);

    // If destination exists...
    if ( dst_exists ) {
        // Can copy entries with different types?
        if ( F_ISSET(flags, fCF_EqualTypes)  &&  (src_type != dst_type) ) {
            return false;
        }
        // Can overwrite entry?
        if ( !F_ISSET(flags, fCF_Overwrite) ) {
            return false;
        }
        // Copy only if destination is older, otherwise just remove source
        if ( F_ISSET(flags, fCF_Update)  &&  !IsNewer(dst.GetPath())) {
            return Remove();
        }
        // Backup destination entry first
        if ( F_ISSET(flags, fCF_Backup) ) {
            // Use a new CDirEntry object for 'dst', because its path
            // will be changed after backup
            CDirEntry dst_tmp(dst);
            if ( !dst_tmp.Backup(GetBackupSuffix(), eBackup_Rename) ) {
                return false;
            }
        }
        // Overwrite destination entry
        if ( F_ISSET(flags, fCF_Overwrite) ) {
            dst.Remove();
        } 
    }

    // Copy symbolic link (create new one)
    char buf[PATH_MAX];
    int  len = readlink(GetPath().c_str(), buf, sizeof(buf));
    if ( len < 1 ) {
        return false;
    }
    buf[len] = '\0';
    if ( symlink(buf, new_path.c_str()) ) {
        return false;
    }

    // Preserve attributes
    if ( flags & fCF_PreserveAll ) {
        if (!s_CopyAttrs(GetPath().c_str(), new_path.c_str(), eLink, flags)) {
            return false;
        }
    }
    return true;
#else
    return CParent::Copy(new_path, flags, buf_size);
#endif
}


//////////////////////////////////////////////////////////////////////////////
//
// CFileDeleteList / CFileDeleteAtExit
//

CFileDeleteList::~CFileDeleteList()
{
    ITERATE (TNames, name, m_Names) {
        CDirEntry entry(*name);
        if ( entry.IsDir()) {
            CDir(*name).Remove(CDir::eRecursive);
        } else {
            entry.Remove();        
        }
    }
}


void CFileDeleteAtExit::Add(const string& entryname)
{
    s_DeleteAtExitFileList->Add(entryname);
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
// CMemoryFile
//

// Cached system's memory virtual page size.
static unsigned long s_VirtualMemoryPageSize = 0;  


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
            //+++ Like to UNIX: do not lock a file/page in the private mode.
            //if ( share_attr == CMemoryFile_Base::eMMS_Shared ) {
            // Enable to write to mapped file/page somewhere else...
            attrs->map_protect = PAGE_READWRITE;
            attrs->file_access = GENERIC_READ | GENERIC_WRITE;
            //} else {
            //    attrs->map_protect = PAGE_READONLY;
            //    attrs->file_access = GENERIC_READ;
            //}
            //---
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
        attrs->file_share = 0;
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
                   "Memory-mapping is not supported by the C++ Toolkit"
                   "on this platform");
    }
    if ( !s_VirtualMemoryPageSize ) {
        s_VirtualMemoryPageSize = GetVirtualMemoryPageSize();
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


#if !defined(HAVE_MADVISE)
bool CMemoryFile_Base::MemMapAdviseAddr(void*, size_t, EMemMapAdvise) {
    return true;
}
#else  /* HAVE_MADVISE */
bool CMemoryFile_Base::MemMapAdviseAddr(void* addr, size_t len,
                                        EMemMapAdvise advise)
{
    int adv;
    if ( !addr || !len ) {
        return false;
    }
    switch (advise) {
    case eMMA_Random:
        adv = MADV_RANDOM;     break;
    case eMMA_Sequential:
        adv = MADV_SEQUENTIAL; break;
    case eMMA_WillNeed:
        adv = MADV_WILLNEED;   break;
    case eMMA_DontNeed:
        adv = MADV_DONTNEED;   break;
    default:
        adv = MADV_NORMAL;
    }
    // Conversion type of "addr" to char* -- Sun Solaris fix
    return madvise((char*)addr, len, adv) == 0;
}
#endif  /* HAVE_MADVISE */



CMemoryFileSegment::CMemoryFileSegment(SMemoryFileHandle& handle,
                                       SMemoryFileAttrs&  attrs,
                                       off_t              offset,
                                       size_t             length)
    : m_DataPtr(0), m_Offset(offset), m_Length(length),
      m_DataPtrReal(0), m_OffsetReal(offset), m_LengthReal(length)
{
    if ( m_Offset < 0 ) {
        NCBI_THROW(CFileException, eMemoryMap,
            "CMemoryFileSegment: The file offset cannot be negative");
    }
    if ( !m_Length ) {
        NCBI_THROW(CFileException, eMemoryMap,
            "CMemoryFileSegment: The length of file mapping region "
            "must be above 0");
    }
    // Get system's memory allocation granularity.
    if ( !s_VirtualMemoryPageSize ) {
        NCBI_THROW(CFileException, eMemoryMap,
            "CMemoryFileSegment: Cannot determine size of virtual page");
    }    
    // Adjust mapped length and offset.
    if ( m_Offset % s_VirtualMemoryPageSize ) {
        m_OffsetReal -= (m_Offset % s_VirtualMemoryPageSize);
        m_LengthReal += (m_Offset % s_VirtualMemoryPageSize);
    }

    // Map file view to memory
    string errmsg;
#if defined(NCBI_OS_MSWIN)
    DWORD offset_hi  = DWORD(Int8(m_OffsetReal) >> 32);
    DWORD offset_low = DWORD(Int8(m_OffsetReal) & 0xFFFFFFFF);
    m_DataPtrReal = MapViewOfFile(handle.hMap, attrs.map_access,
                                  offset_hi, offset_low, m_LengthReal);
    if ( !m_DataPtrReal ) {
        char* ptr = NULL;
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                    FORMAT_MESSAGE_FROM_SYSTEM | 
                    FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL, GetLastError(), 
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    (LPTSTR) &ptr, 0, NULL);
        errmsg = ptr ? ptr : "unknown reason";
        LocalFree(ptr);
    }

#elif defined(NCBI_OS_UNIX)
    errno = 0;
    m_DataPtrReal = mmap(0, m_LengthReal, attrs.map_protect,
                         attrs.map_access, handle.hMap, m_OffsetReal);
    if ( m_DataPtrReal == MAP_FAILED ) {
        m_DataPtrReal = 0;
        errmsg = strerror(errno);
    }
#endif
    // Calculate user's pointer to data
    m_DataPtr = (char*)m_DataPtrReal + (m_Offset - m_OffsetReal);
    if ( !m_DataPtr ) {
        NCBI_THROW(CFileException, eMemoryMap,
            "CMemoryFileSegment: Unable to map a view of a file '" + 
            handle.sFileName + "' into memory (offset=" +
            NStr::Int8ToString(m_Offset) + ", length=" +
            NStr::Int8ToString(m_Length) + "): " + errmsg);
    }
}


CMemoryFileSegment::~CMemoryFileSegment(void)
{
    Unmap();
}


bool CMemoryFileSegment::Flush(void) const
{
    if ( !m_DataPtr ) {
        return false;
    }
    bool status;
#if defined(NCBI_OS_MSWIN)
    status = (FlushViewOfFile(m_DataPtrReal, m_LengthReal) != 0);
#elif defined(NCBI_OS_UNIX)
    status = (msync((char*)m_DataPtrReal, m_LengthReal, MS_SYNC) == 0);
#endif
    return status;
}


bool CMemoryFileSegment::Unmap(void)
{
    // If file view is not mapped do nothing
    if ( !m_DataPtr ) {
        return true;
    }
    bool status;
#if defined(NCBI_OS_MSWIN)
    status = (UnmapViewOfFile(m_DataPtrReal) != 0);
#elif defined(NCBI_OS_UNIX)
    status = (munmap((char*)m_DataPtrReal, (size_t) m_Length) == 0);
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
    NCBI_THROW(CFileException, eMemoryMap,
               "CMemoryFileSegment: File view is not mapped");
}


bool CMemoryFileSegment::MemMapAdvise(EMemMapAdvise advise) const
{
    if ( !m_DataPtr ) {
        return false;
    }
    return MemMapAdviseAddr(m_DataPtr, m_Length, advise);
}


CMemoryFileMap::CMemoryFileMap(const string&  file_name,
                               EMemMapProtect protect,
                               EMemMapShare   share)
    : m_FileName(file_name), m_Handle(0), m_Attrs(0)
{
    // Translate attributes 
    m_Attrs = s_TranslateAttrs(protect, share);

    // Check file size
    Int8 file_size = GetFileSize();
    if ( file_size < 0 ) {
        NCBI_THROW(CFileException, eMemoryMap,
            "CMemoryFileMap: The mapped file \"" + m_FileName +
            "\" must exists");
    }
    // Open file
    if ( file_size == 0 ) {
        // Special case -- file is empty
        m_Handle = new SMemoryFileHandle();
        m_Handle->hMap = 0;
        m_Handle->sFileName = m_FileName;
        return;
    }
    x_Open();
}


CMemoryFileMap::~CMemoryFileMap(void)
{
    // Unmap used memory and close file
    x_Close();
}


void* CMemoryFileMap::Map(off_t offset, size_t length)
{
    if ( !m_Handle  ||  !m_Handle->hMap ) {
        // Special case.
        // Always return 0 if a file is unmapped or have zero length.
        return 0;
    }
    // Map view of file
    CMemoryFileSegment* segment =  
        new CMemoryFileSegment(*m_Handle, *m_Attrs, offset, length);
    void* ptr = segment->GetPtr();
    if ( !ptr ) {
        NCBI_THROW(CFileException, eMemoryMap,
                   "CMemoryFileMap: Map() failed (file \"" + m_FileName +"\", "
                   "offset=" + NStr::Int8ToString(offset) + ", "\
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
    return status;
}


void CMemoryFileMap::x_Open(void)
{
    m_Handle = new SMemoryFileHandle();
    m_Handle->hMap = 0;
    m_Handle->sFileName = m_FileName;

    for (;;) { // quasi-TRY block

#if defined(NCBI_OS_MSWIN)
        // Name of a file-mapping object cannot contain '\'
        string x_name = NStr::Replace(m_FileName, "\\", "/");

        // If failed to attach to an existing file-mapping object then
        // create a new one (based on the specified file)
        m_Handle->hMap = OpenFileMapping(m_Attrs->map_access, false,
                                         x_name.c_str());
        if ( !m_Handle->hMap ) { 
            HANDLE hFile = CreateFile(x_name.c_str(), m_Attrs->file_access, 
                                      m_Attrs->file_share, NULL,
                                      OPEN_EXISTING,
                                      FILE_ATTRIBUTE_NORMAL, NULL);
            if ( hFile == INVALID_HANDLE_VALUE )
                break;

            m_Handle->hMap = CreateFileMapping(hFile, NULL,
                                               m_Attrs->map_protect,
                                               0, 0, x_name.c_str());
            CloseHandle(hFile);
            if ( !m_Handle->hMap ) {
                break;
            }
        }

#elif defined(NCBI_OS_UNIX)
        // Open file
        m_Handle->hMap = open(m_FileName.c_str(), m_Attrs->file_access);
        if ( m_Handle->hMap < 0 ) {
            break;
        }
#endif
        // Success
        return;
    }
    // Error: close and cleanup
    x_Close();
    NCBI_THROW(CFileException, eMemoryMap,
        "CMemoryFile: Unable to map file \"" + m_FileName + "\" into memory");
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
        if ( m_Handle->hMap ) { 
#if defined(NCBI_OS_MSWIN)
            CloseHandle(m_Handle->hMap);
#elif defined(NCBI_OS_UNIX)
            close(m_Handle->hMap);
#endif
        }
        delete m_Handle;
        m_Handle  = 0;
    }
    if ( m_Attrs ) {
        delete m_Attrs;
        m_Attrs = 0;
    }
}


CMemoryFileSegment* CMemoryFileMap::x_Get(void* ptr) const
{
    if ( !m_Handle  &&  !m_Handle->hMap ) {
        NCBI_THROW(CFileException, eMemoryMap,
                   "CMemoryFileMap: File is not mapped");
    }
    TSegments::const_iterator segment = m_Segments.find(ptr);
    if ( segment == m_Segments.end() ) {
        NCBI_THROW(CFileException, eMemoryMap,
                   "CMemoryFileMap: Cannot find mapped file segment "
                   "with specified address");
    }
    return segment->second;
}

   
CMemoryFile::CMemoryFile(const string&  file_name,
                         EMemMapProtect protect,
                         EMemMapShare   share,
                         off_t          offset,
                         size_t         length)
    : CMemoryFileMap(file_name, protect, share), m_Ptr(0)
{
    // Check that file is ready for mapping to memory
    if ( !m_Handle  ||  !m_Handle->hMap ) {
        return;
    }
    // Map file wholly if the length of mapped region is not specified
    if ( !length ) {
        Int8 file_size = GetFileSize();
        if ( file_size > kMax_UInt ) {
            length = kMax_UInt;
        } else {
            length = (size_t)file_size;
        }
    }
    m_Ptr = Map(offset, length);
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


void CMemoryFile::x_Verify(void) const
{
    if ( m_Ptr ) {
        return;
    }
    NCBI_THROW(CFileException, eMemoryMap,"CMemoryFile: File is not mapped");
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.98  2005/05/20 11:23:46  ivanov
 * Added new classes CFileDeleteList and CFileDeleteAtExit.
 * CMemoryFile[Map](): changed default share attribute from eMMS_Shared.
 * s_TranslateAttrs(): changed read private mapping mode for MS Windows
 * alike UNIX (do not lock file exclusively).
 *
 * Revision 1.97  2005/04/28 14:08:26  ivanov
 * Added time_t and CTime versions of CDirEntry::IsNewer()
 *
 * Revision 1.96  2005/04/25 20:21:55  ivanov
 * Get rid of Workshop compilation warnings
 *
 * Revision 1.95  2005/04/12 16:51:32  ucko
 * Define a dummy fallback version of __GLIBC_PREREQ if necessary.
 *
 * Revision 1.94  2005/04/12 14:24:05  ivanov
 * Fix: On Linux nanoseconds file time precision is defined in
 * the GNU C Library 2.3 and above
 *
 * Revision 1.93  2005/04/12 11:25:25  ivanov
 * CDirEntry: added struct SStat and method Stat() to get additional non-posix
 * OS-dependent info. Now it can get only nanoseconds for entry times.
 * CDirEntry::SetTime[T]() -- added parameter to change creation time
 * (where possible). Minor comments changes and cosmetics.
 *
 * Revision 1.92  2005/03/23 15:37:13  ivanov
 * + CDirEntry:: CreateObject, Get/SetTimeT
 * Changed Copy/Rename in accordance that flags "Update" and "Backup"
 * also means "Overwrite".
 *
 * Revision 1.91  2005/03/22 15:58:53  ivanov
 * Fixed MSVC compilation error
 *
 * Revision 1.90  2005/03/22 15:45:57  ucko
 * Always use regular chown rather than lchown when the latter is unavailable.
 *
 * Revision 1.89  2005/03/22 14:20:48  ivanov
 * + CDirEntry:: operator=, Copy, CopyToDir, Get/SetBackupSuffix, Backup,
 *               IsLink, LookupLink, DereferenceLink, IsNewer, Get/SetOwner
 * + CFile:: Copy, Compare
 * + CDir:: Copy
 * + CSymLink
 * Added default constructors to all CDirEntry based classes.
 * CDirEntry::Rename -- added flags parameter.
 * CDir::GetEntries, CDirEntry::MatchesMask - added parameter for case
 * sensitive/insensitive matching.
 * Added additional flag for find files algorithms, EFindFiles += fFF_Nocase.
 * Dropped MacOS 9 support.
 *
 * Revision 1.88  2005/03/15 15:05:33  dicuccio
 * Fixed typo: pathes -> paths
 *
 * Revision 1.87  2005/02/23 19:18:53  ivanov
 * CMemoryFileSegment: added mapped file name and error explanation
 * to exception diagnostic message
 *
 * Revision 1.86  2005/01/31 11:49:14  ivanov
 * Added CMask versions of:
 *     CDirEntries::MatchesMask(), CDirEntries::GetEntries().
 * Some cosmetics.
 *
 * Revision 1.85  2004/12/14 17:50:28  ivanov
 * CDir::Create(): return TRUE if creating directory already exists
 *
 * Revision 1.84  2004/10/08 12:43:53  ivanov
 * Fixed CDirEntry::Reset() -- delete trailing path separator always except
 * some special cases, like root dir and PC's disk names.
 * CDirEntry::NormalizePath() -- 2 small fixes to prevent crashing on some
 * path strings and correct processing root dirs variations.
 *
 * Revision 1.83  2004/08/10 16:57:31  grichenk
 * Fixed leading / in ConcatPath()
 *
 * Revision 1.82  2004/08/03 12:04:31  ivanov
 * + CMemoryFile_Base   - base class for all memory mapping classes.
 * + CMemoryFileSegment - auxiliary class for mapping a memory file region.
 * + CMemoryFileMap     - class for support a partial file memory mapping.
 * * CMemoryFile        - now is the same as CMemoryFileMap but have only
 *                        one big mapped segment with offset 0 and length
 *                        equal to length of file.
 *
 * Revision 1.81  2004/07/28 20:31:11  ucko
 * Portability fix: try getpagesize() if _SC_PAGESIZE is unavailable.
 *
 * Revision 1.80  2004/07/28 16:22:41  ivanov
 * Renamed CMemoryFileMap -> CMemoryFileSegment
 *
 * Revision 1.79  2004/07/28 15:47:08  ivanov
 * + CMemoryFile_Base, CMemoryFileMap.
 * Added "offset" and "length" parameters to CMemoryFile constructor to map
 * a part of file.
 *
 * Revision 1.78  2004/05/18 16:51:34  ivanov
 * Added CDir::GetTmpDir()
 *
 * Revision 1.77  2004/05/14 13:59:27  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 1.76  2004/04/29 15:14:34  kuznets
 * + Generic FindFile algorithm capable of recursive searches
 * CDir::GetEntries received additional parameter to ignore self
 * recursive directory entries (".", "..")
 *
 * Revision 1.75  2004/04/28 19:04:29  ucko
 * Give GetType(), IsFile(), and IsDir() an optional EFollowLinks
 * parameter (currently only honored on Unix).
 *
 * Revision 1.74  2004/04/28 15:56:49  ivanov
 * CDirEntry::GetType(): fixed bug with incorrect entry type determination
 * on some platforms
 *
 * Revision 1.73  2004/04/21 11:24:53  ivanov
 * Define s_StdGetTmpName() for all platforms except NCBI_OS_UNIX
 *
 * Revision 1.72  2004/03/17 15:39:54  ivanov
 * CFile:: Fixed possible race condition concerned with temporary file name
 * generation. Added ETmpFileCreationMode enum. Fixed GetTmpName[Ex] and
 * CreateTmpFile[Ex] class methods.
 *
 * Revision 1.71  2004/03/05 12:26:43  ivanov
 * Moved CDirEntry::MatchesMask() to NStr class.
 *
 * Revision 1.70  2004/02/12 19:50:34  ivanov
 * Fixed CDirEntry::DeleteTrailingPathSeparator and CDir::CreatePath()
 * to avoid creating empty directories with disk name in the case if
 * specified path contains it.
 *
 * Revision 1.69  2004/02/11 20:49:57  gorelenk
 * Implemented case-insensitivity of CreateRelativePath on NCBI_OS_MSWIN.
 *
 * Revision 1.68  2004/02/11 20:30:21  gorelenk
 * Added case-insensitive test of root dirs inside CreateRelativePath.
 *
 * Revision 1.67  2004/01/07 13:58:28  ucko
 * Add missing first argument to string constructor in s_StripDir.
 *
 * Revision 1.66  2004/01/05 21:40:54  gorelenk
 * += Exception throwing in CDirEntry::CreateRelativePath()
 *
 * Revision 1.65  2004/01/05 20:04:05  gorelenk
 * + CDirEntry::CreateRelativePath()
 *
 * Revision 1.64  2003/11/28 16:51:48  ivanov
 * Fixed CDirEntry::SetTime() to set current time if parameter is empty
 *
 * Revision 1.63  2003/11/28 16:22:42  ivanov
 * + CDirEntry::SetTime()
 *
 * Revision 1.62  2003/11/05 15:36:23  kuznets
 * Implemented new variant of CDir::GetEntries()
 *
 * Revision 1.61  2003/10/23 12:10:51  ucko
 * Use AutoPtr with CDeleter rather than auto_ptr, which is *not*
 * guaranteed to be appropriate for malloc()ed memory.
 *
 * Revision 1.60  2003/10/22 13:25:47  ivanov
 * GetTmpName[Ex]:  replaced tempnam() with mkstemp() for UNIX
 *
 * Revision 1.59  2003/10/08 15:45:09  ivanov
 * Added CDirEntry::DeleteTrailingPathSeparator()
 *
 * Revision 1.58  2003/10/01 14:32:09  ucko
 * +EFollowLinks
 *
 * Revision 1.57  2003/09/30 15:08:51  ucko
 * Reworked CDirEntry::NormalizePath, which now handles .. correctly in
 * all cases and optionally resolves symlinks (on Unix).
 *
 * Revision 1.56  2003/09/17 20:55:02  ivanov
 * NormalizePath: added more processing for MS Windows paths
 *
 * Revision 1.55  2003/09/16 18:54:26  ivanov
 * NormalizePath(): added replacing double dir separators with single one
 *
 * Revision 1.54  2003/09/16 16:03:00  ivanov
 * MakePath():  don't add separator to directory name if it is an empty string
 *
 * Revision 1.53  2003/09/16 15:17:16  ivanov
 * + CDirEntry::NormalizePath()
 *
 * Revision 1.52  2003/08/29 16:54:28  ivanov
 * GetTmpName(): use tempname() instead tmpname()
 *
 * Revision 1.51  2003/08/21 20:32:48  ivanov
 * CDirEntry::GetType(): use lstat() instead stat() on UNIX
 *
 * Revision 1.50  2003/08/08 13:37:17  siyan
 * Changed GetTmpNameExt to GetTmpNameEx in CFile, as this is the more
 * appropriate name.
 *
 * Revision 1.49  2003/06/18 18:57:43  rsmith
 * alternate implementation of GetTmpNameExt replacing tempnam with mktemp for
 * library missing tempnam.
 *
 * Revision 1.48  2003/05/29 17:21:04  gouriano
 * added CreatePath() which creates directories recursively
 *
 * Revision 1.47  2003/04/11 14:04:49  ivanov
 * Fixed CDirEntry::GetMode (Mac OS) - store a mode only for a notzero pointers.
 * Fixed CDir::GetEntries -- do not try to close a dir handle if it was
 * not opened.
 *
 * Revision 1.46  2003/04/04 16:02:37  lavr
 * Lines wrapped at 79th column; some minor reformatting
 *
 * Revision 1.45  2003/04/03 14:15:48  rsmith
 * combine pp symbols NCBI_COMPILER_METROWERKS & _MSL_USING_MW_C_HEADERS
 * into NCBI_COMPILER_MW_MSL
 *
 * Revision 1.44  2003/04/02 16:22:34  rsmith
 * clean up metrowerks ifdefs.
 *
 * Revision 1.43  2003/04/02 13:29:29  rsmith
 * include ncbi_mslextras.h when compiling with MSL libs in Codewarrior.
 *
 * Revision 1.42  2003/03/10 18:57:08  kuznets
 * iterate->ITERATE
 *
 * Revision 1.41  2003/02/28 21:06:02  ivanov
 * Added close() call to CTmpStream destructor before the file deleting.
 *
 * Revision 1.40  2003/02/14 19:30:50  ivanov
 * Added mode ios::trunc by default for files creates in CFile::CreateTmpFile().
 * Get read of some warnings - initialize variables in CMemoryFile::x_Map().
 *
 * Revision 1.39  2003/02/06 16:14:28  ivanov
 * CMemomyFile::x_Map(): changed file access attribute from O_RDWR to
 * O_RDONLY in case of private sharing.
 *
 * Revision 1.38  2003/02/05 22:07:15  ivanov
 * Added protect and sharing parameters to the CMemoryFile constructor.
 * Added CMemoryFile::Flush() method.
 *
 * Revision 1.37  2003/01/16 13:27:56  dicuccio
 * Added CDir::GetCwd()
 *
 * Revision 1.36  2002/11/05 15:46:49  dicuccio
 * Changed CDir::GetEntries() to store fully qualified path names - this permits
 * CDirEntry::GetType() to work on files outside of the local directory.
 *
 * Revision 1.35  2002/10/01 17:23:57  gouriano
 * more fine-tuning of ConvertToOSPath
 *
 * Revision 1.34  2002/10/01 17:02:53  gouriano
 * minor modification of ConvertToOSPath
 *
 * Revision 1.33  2002/10/01 14:18:45  gouriano
 * "optimize" result of ConvertToOSPath
 *
 * Revision 1.32  2002/09/19 20:05:42  vasilche
 * Safe initialization of static mutexes
 *
 * Revision 1.31  2002/09/17 20:44:58  lavr
 * Add comment in unreachable return from CDirEntry::MatchesMask()
 *
 * Revision 1.30  2002/09/05 18:35:38  vakatov
 * Formal code rearrangement to get rid of comp.warnings;  some nice-ification
 *
 * Revision 1.29  2002/07/18 20:22:59  lebedev
 * NCBI_OS_MAC: fcntl.h added
 *
 * Revision 1.28  2002/07/15 18:17:24  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 1.27  2002/07/11 19:28:30  ivanov
 * Removed test stuff from MemMapAdvise[Addr]
 *
 * Revision 1.26  2002/07/11 19:21:30  ivanov
 * Added CMemoryFile::MemMapAdvise[Addr]()
 *
 * Revision 1.25  2002/07/11 14:18:27  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 1.24  2002/06/07 16:11:37  ivanov
 * Chenget GetTime() -- using CTime instead time_t, modification time
 * by default
 *
 * Revision 1.23  2002/06/07 15:21:06  ivanov
 * Added CDirEntry::GetTime()
 *
 * Revision 1.22  2002/05/01 22:59:00  vakatov
 * A couple of (size_t) type casts to avoid compiler warnings in 64-bit
 *
 * Revision 1.21  2002/04/11 21:08:02  ivanov
 * CVS log moved to end of the file
 *
 * Revision 1.20  2002/04/01 18:49:54  ivanov
 * Added class CMemoryFile. Added including <windows.h> under MS Windows
 *
 * Revision 1.19  2002/03/25 17:08:17  ucko
 * Centralize treatment of Cygwin as Unix rather than Windows in configure.
 *
 * Revision 1.18  2002/03/22 20:00:01  ucko
 * Tweak to work on Cygwin.  (Use Unix rather than MSWIN code).
 *
 * Revision 1.17  2002/02/07 21:05:47  kans
 * implemented GetHome (FindFolder kPreferencesFolderType) for Mac
 *
 * Revision 1.16  2002/01/24 22:18:02  ivanov
 * Changed CDirEntry::Remove() and CDir::Remove()
 *
 * Revision 1.15  2002/01/22 21:21:09  ivanov
 * Fixed typing error
 *
 * Revision 1.14  2002/01/22 19:27:39  ivanov
 * Added realization ConcatPathEx()
 *
 * Revision 1.13  2002/01/20 06:13:34  vakatov
 * Fixed warning;  formatted the source code
 *
 * Revision 1.12  2002/01/10 16:46:36  ivanov
 * Added CDir::GetHome() and some CDirEntry:: path processing functions
 *
 * Revision 1.11  2001/12/26 21:21:05  ucko
 * Conditionalize deletion of m_FSS on NCBI_OS_MAC.
 *
 * Revision 1.10  2001/12/26 20:58:22  juran
 * Use an FSSpec* member instead of an FSSpec, so a forward declaration can
 * be used.
 * Add copy constructor and assignment operator for CDirEntry on Mac OS,
 * thus avoiding memberwise copy which would blow up upon deleting the
 * pointer twice.
 *
 * Revision 1.9  2001/12/18 21:36:38  juran
 * Remove unneeded Mac headers.
 * (Required functions copied to ncbi_os_mac.cpp)
 * MoveRename PStr to PString in ncbi_os_mac.hpp.
 * Don't use MoreFiles xxxCompat functions.  They're for System 6.
 * Don't use global scope operator on functions copied into NCBI scope.
 *
 * Revision 1.8  2001/11/19 23:38:44  vakatov
 * Fix to compile with SUN WorkShop (and maybe other) compiler(s)
 *
 * Revision 1.7  2001/11/19 18:10:13  juran
 * Whitespace.
 *
 * Revision 1.6  2001/11/15 16:34:12  ivanov
 * Moved from util to corelib
 *
 * Revision 1.5  2001/11/06 14:34:11  ivanov
 * Fixed compile errors in CDir::Contents() under MS Windows
 *
 * Revision 1.4  2001/11/01 21:02:25  ucko
 * Fix to work on non-MacOS platforms again.
 *
 * Revision 1.3  2001/11/01 20:06:48  juran
 * Replace directory streams with Contents() method.
 * Implement and test Mac OS platform.
 *
 * Revision 1.2  2001/09/19 16:22:18  ucko
 * S_IFDOOR is nonportable; make sure it exists before using it.
 * Fix type of second argument to CTmpStream's constructor (caught by gcc 3).
 *
 * Revision 1.1  2001/09/19 13:06:09  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
