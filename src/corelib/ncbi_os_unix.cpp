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
 * Authors:  Anton Lavrentiev
 *
 * File Description:   UNIX specifics
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/error_codes.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbifile.hpp>
#include "ncbi_os_unix_p.hpp"

#include <string.h>

#include <errno.h>
#include <dirent.h>        // opendir(), readdir()
#include <unistd.h>


#define NCBI_USE_ERRCODE_X   Corelib_Unix


#define _STRINGIFY(x)            #x
#define  STRINGIFY(x)  _STRINGIFY(x)


// Some initial defaults for faster lookups
#define PWD_BUF  1024
#define GRP_BUF  4096
#define MAX_TRY  3


BEGIN_NCBI_SCOPE


string CUnixFeature::GetUserNameByUID(uid_t uid)
{
    string user;

#if defined(NCBI_OS_SOLARIS)  ||                                    \
    (defined(HAVE_GETPWUID)  &&  !defined(NCBI_HAVE_GETPWUID_R))
    // NB:  getpwuid() is MT-safe on Solaris
    const struct passwd* pwd = getpwuid(uid);
    if (pwd  &&  pwd->pw_name) {
        user.assign(pwd->pw_name);
    }

#elif defined(NCBI_HAVE_GETPWUID_R)
    struct passwd* pwd;

    char x_buf[PWD_BUF + sizeof(*pwd)];
    size_t size = sizeof(x_buf);
    char*  buf  = x_buf;

    for (int n = 0;  n < MAX_TRY;  ++n) {
#  if   NCBI_HAVE_GETPWUID_R == 4
        // obsolete but still existent
        pwd = getpwuid_r(uid,
                         (struct passwd*) buf, buf + sizeof(*pwd),
                         size - sizeof(*pwd));

#  elif NCBI_HAVE_GETPWUID_R == 5
        /* POSIX-conforming */
        int x_errno = getpwuid_r(uid,
                                 (struct passwd*) buf, buf + sizeof(*pwd),
                                 size - sizeof(*pwd), &pwd);
        if (x_errno) {
            errno = x_errno;
            pwd = 0;
        }

#  else
#    error "Unknown value of NCBI_HAVE_GETPWUID_R: 4 or 5 expected."

#  endif //NCBI_HAVE_GETPWUID_R

        if (pwd  ||  errno != ERANGE) {
            break;
        }

        if (n == 0) {
            size_t maxsize;
#  ifdef _SC_GETPW_R_SIZE_MAX
            long sc = sysconf(_SC_GETPW_R_SIZE_MAX);
            maxsize = sc < 0 ? 0 : (size_t) sc + sizeof(*pwd);
#  else
            maxsize = size << 1;
#  endif //_SC_GETPW_R_SIZE_MAX
            ERR_POST_ONCE((size < maxsize ? Error : Critical)
                          << "getpwuid_r() parse buffer too small ("
                          STRINGIFY(PWD_BUF) "), please enlarge it!");
            _ASSERT(buf == x_buf);
            if (size < maxsize) {
                size = maxsize;
                buf = new char[size];
                continue;
            }
        } else if (n == MAX_TRY - 1) {
            ERR_POST_ONCE(Critical << "getpwuid_r() parse buffer too small ("
                          << NStr::NumericToString(size) << ")!");
            break;
        } else {
            _ASSERT(buf != x_buf);
            delete[] buf;
        }

        buf = new char[size <<= 1];
    }

    if (pwd  &&  pwd->pw_name) {
        user.assign(pwd->pw_name);
    }
    if (buf != x_buf) {
        delete[] buf;
    }

#endif

    return user;
}


uid_t CUnixFeature::GetUserUIDByName(const string& user)
{
    uid_t uid;

#if defined(NCBI_OS_SOLARIS)   ||                                   \
    (defined(HAVE_GETPWUID)  &&  !defined(NCBI_HAVE_GETPWUID_R))
    // NB:  getpwnam() is MT-safe on Solaris
    const struct passwd* pwd = getpwnam(user.c_str());
    uid = pwd ? pwd->pw_uid : (uid_t)(-1);

#elif defined(NCBI_HAVE_GETPWUID_R)
    struct passwd* pwd;

    char x_buf[PWD_BUF + sizeof(*pwd)];
    size_t size = sizeof(x_buf);
    char*  buf  = x_buf;

    for (int n = 0;  n < MAX_TRY;  ++n) {
#  if   NCBI_HAVE_GETPWUID_R == 4
        // obsolete but still existent
        pwd = getpwnam_r(user.c_str(),
                         (struct passwd*) buf, buf + sizeof(*pwd),
                         size - sizeof(*pwd));

#  elif NCBI_HAVE_GETPWUID_R == 5
        // POSIX-conforming
        int x_errno = getpwnam_r(user.c_str(),
                                 (struct passwd*) buf, buf + sizeof(*pwd),
                                 size - sizeof(*pwd), &pwd);
        if (x_errno) {
            errno = x_errno;
            pwd = 0;
        }

#  else
#    error "Unknown value of NCBI_HAVE_GETPWUID_R: 4 or 5 expected."

#  endif //NCBI_HAVE_GETPWUID_R

        if (pwd  ||  errno != ERANGE) {
            break;
        }

        if (n == 0) {
            size_t maxsize;
#  ifdef _SC_GETPW_R_SIZE_MAX
            long sc = sysconf(_SC_GETPW_R_SIZE_MAX);
            maxsize = sc < 0 ? 0 : (size_t) sc + sizeof(*pwd);
#  else
            maxsize = size << 1;
#  endif //_SC_GETPW_R_SIZE_MAX
            ERR_POST_ONCE((size < maxsize ? Error : Critical)
                          << "getpwnam_r() parse buffer too small ("
                          STRINGIFY(PWD_BUF) "), please enlarge it!");
            _ASSERT(buf == x_buf);
            if (size < maxsize) {
                size = maxsize;
                buf = new char[size];
                continue;
            }
        } else if (n == MAX_TRY - 1) {
            ERR_POST_ONCE(Critical << "getpwnam_r() parse buffer too small ("
                          << NStr::NumericToString(size) << ")!");
            break;
        } else {
            _ASSERT(buf != x_buf);
            delete[] buf;
        }

        buf = new char[size <<= 1];
    }

    uid = pwd ? pwd->pw_uid : (uid_t)(-1);

    if (buf != x_buf) {
        delete[] buf;
    }

#else
    uid = (uid_t)(-1);

#endif

    return uid;
}


string CUnixFeature::GetGroupNameByGID(gid_t gid)
{
    string group;

#if defined(NCBI_OS_SOLARIS)  ||                                    \
    (defined(HAVE_GETPWUID)  &&  !defined(NCBI_HAVE_GETPWUID_R))
    // NB:  getgrgid() is MT-safe on Solaris
    const struct group* grp = getgrgid(gid);
    if (grp  &&  grp->gr_name) {
        group.assign(grp->gr_name);
    }

#elif defined(NCBI_HAVE_GETPWUID_R)
    struct group* grp;

    char x_buf[GRP_BUF + sizeof(*grp)];
    size_t size = sizeof(x_buf);
    char*  buf  = x_buf;

    for (int n = 0;  n < MAX_TRY;  ++n) {
#  if   NCBI_HAVE_GETPWUID_R == 4
        // obsolete but still existent
        grp = getgrgid_r(gid,
                         (struct group*) buf, buf + sizeof(*grp),
                         size - sizeof(*grp));

#  elif NCBI_HAVE_GETPWUID_R == 5
        // POSIX-conforming
        int x_errno  = getgrgid_r(gid,
                                  (struct group*) buf, buf + sizeof(*grp),
                                  size - sizeof(*grp), &grp);
        if (x_errno) {
            errno = x_errno;
            grp = 0;
        }

#  else
#      error "Unknown value of NCBI_HAVE_GETPWUID_R: 4 or 5 expected."

#  endif //NCBI_HAVE_GETPWUID_R

        if (grp  ||  errno != ERANGE) {
            break;
        }

        if (n == 0) {
            size_t maxsize;
#  ifdef _SC_GETGR_R_SIZE_MAX
            long sc = sysconf(_SC_GETGR_R_SIZE_MAX);
            maxsize = sc < 0 ? 0 : (size_t) sc + sizeof(*grp);
#  else
            maxsize = size << 1;
#  endif //_SC_GETGR_R_SIZE_MAX
            ERR_POST_ONCE((size < maxsize ? Error : Critical)
                          << "getgrgid_r() parse buffer too small ("
                          STRINGIFY(GRP_BUF) "), please enlarge it!");
            _ASSERT(buf == x_buf);
            if (size < maxsize) {
                size = maxsize;
                buf = new char[size];
                continue;
            }
        } else if (n == MAX_TRY - 1) {
            ERR_POST_ONCE(Critical << "getgrgid_r() parse buffer too small ("
                          << NStr::NumericToString(size) << ")!");
            break;
        } else {
            _ASSERT(buf != x_buf);
            delete[] buf;
        }

        buf = new char[size <<= 1];
    }

    if (grp  &&  grp->gr_name) {
        group.assign(grp->gr_name);
    }

    if (buf != x_buf) {
        delete[] buf;
    }

#endif

    return group;
}


gid_t CUnixFeature::GetGroupGIDByName(const string& group)
{
    gid_t gid;

#if defined(NCBI_OS_SOLARIS)  ||                                    \
    (defined(HAVE_GETPWUID)  &&  !defined(NCBI_HAVE_GETPWUID_R))
    // NB:  getgrnam() is MT-safe on Solaris
    const struct group* grp = getgrnam(group.c_str());
    gid = grp ? grp->gr_gid : (gid_t)(-1);

#elif defined(NCBI_HAVE_GETPWUID_R)
    struct group* grp;

    char x_buf[GRP_BUF + sizeof(*grp)];
    size_t size = sizeof(x_buf);
    char*  buf  = x_buf;

    for (int n = 0;  n < MAX_TRY;  ++n) {
#  if   NCBI_HAVE_GETPWUID_R == 4
        // obsolete but still existent
        grp = getgrnam_r(group.c_str(),
                         (struct group*) buf, buf + sizeof(*grp),
                         size - sizeof(*grp));

#  elif NCBI_HAVE_GETPWUID_R == 5
        // POSIX-conforming
        int x_errno = getgrnam_r(group.c_str(),
                                 (struct group*) buf, buf + sizeof(*grp),
                                 size - sizeof(*grp), &grp);
        if (x_errno) {
            errno = x_errno;
            grp = 0;
        }

#  else
#      error "Unknown value of NCBI_HAVE_GETPWUID_R: 4 or 5 expected."

#  endif //NCBI_HAVE_GETPWUID_R

        if (grp  ||  errno != ERANGE) {
            break;
        }

        if (n == 0) {
            size_t maxsize;
#  ifdef _SC_GETGR_R_SIZE_MAX
            long sc = sysconf(_SC_GETGR_R_SIZE_MAX);
            maxsize = sc < 0 ? 0 : (size_t) sc + sizeof(*grp);
#  else
            maxsize = size << 1;
#  endif //_SC_GETGR_R_SIZE_MAX
            ERR_POST_ONCE((size < maxsize ? Error : Critical)
                          << "getgrnam_r() parse buffer too small ("
                          STRINGIFY(GRP_BUF) "), please enlarge it!");
            _ASSERT(buf == x_buf);
            if (size < maxsize) {
                size = maxsize;
                buf = new char[size];
                continue;
            }
        } else if (n == MAX_TRY - 1) {
            ERR_POST_ONCE(Critical << "getgrnam_r() parse buffer too small ("
                          << NStr::NumericToString(size) << ")!");
            break;
        } else {
            _ASSERT(buf != x_buf);
            delete[] buf;
        }

        buf = new char[size <<= 1];
    }

    gid = grp ? grp->gr_gid : (gid_t)(-1);

    if (buf != x_buf) {
        delete[] buf;
    }

#else
    gid = (gid_t)(-1);

#endif
    return gid;
}



#if defined(NCBI_OS_LINUX)

#define PROCFS(pid, file) \
    "/proc/" + (!pid ? "self" : NStr::NumericToString(pid)) + "/" + file

// Helper method to read file, to avoid races
inline
size_t s_ReadFile(const string& filename, char* buf, size_t size)
{
    size_t n = 0;
    CFileIO f;
    try {
        f.Open(filename, CFileIO::eOpen, CFileIO::eRead);
        n = f.Read(buf, size);
        buf[n] = '\0';
    }
    catch(CFileException& e) {
    }
    f.Close();
    return n;
}

// Find "name" string in "buf" and parse value following it.
// Put result to "value". But it shouldn't be less than "min_value".
// All values are in kB.
inline
size_t s_ParseStatusVmValue(const char* name, char* buf, size_t min_value = 0)
{
    const NStr::TStringToNumFlags flags = NStr::fConvErr_NoThrow | 
                                          NStr::fAllowLeadingSpaces | 
                                          NStr::fAllowTrailingSymbols;
    char* pos = strstr(buf, name);
    if (!pos) {
        return 0;
    }
    size_t n = NStr::StringToNumeric<size_t>(pos+strlen(name)+1/*:*/, flags) * 1024;
    return (n < min_value) ? min_value : n;
}

bool CLinuxFeature::GetMemoryUsage(pid_t pid, CProcess::SMemoryUsage& usage)
{
    char buf[2048];
    size_t n;

    // Parse /proc/<pid>/statm

    n = s_ReadFile(PROCFS(pid,"statm"), buf, sizeof(buf));
    if ( n ) {
        CNcbiIstrstream is((const char*)buf, n);
        is >> usage.total >> usage.resident >> usage.shared >> usage.text >> usage.lib /*>> usage.data*/;
        const unsigned long page_size = CSystemInfo::GetVirtualMemoryPageSize();
        usage.total    *= page_size;
        usage.resident *= page_size;
        usage.shared   *= page_size;
        usage.text     *= page_size;
        usage.lib      *= page_size;
        //usage.data     *= page_size; 
    }
  
    // Additionally parse /proc/<pid>/status
    
    n = s_ReadFile(PROCFS(pid,"status"), buf, sizeof(buf));
    if ( n ) {
        // Find starting point for all "Vm*" values
        char* pos = strstr(buf, "Vm");
        if (!pos) {
            return true;
        }
        usage.total_peak    = s_ParseStatusVmValue("VmPeak", pos, usage.total);
        usage.resident_peak = s_ParseStatusVmValue("VmHWM",  pos, usage.resident);
        usage.data          = s_ParseStatusVmValue("VmData", pos);
        usage.swap          = s_ParseStatusVmValue("VmSwap", pos);
        if (!usage.text) {
            usage.text = s_ParseStatusVmValue("VmExe", pos);
        }
        if (!usage.lib) {
            usage.lib  = s_ParseStatusVmValue("VmLib", pos);
        }
        if (!usage.stack) {
            usage.stack = s_ParseStatusVmValue("VmStk", pos);
        }
    }
    return true;
}


int CLinuxFeature::GetThreadCount(pid_t pid)
{
    int n = 0;
    string name = PROCFS(pid, "task");
    DIR* dir = opendir(name.c_str());
    if (dir) {
        while (readdir(dir) != NULL) {
            ++n;
        }
        closedir(dir);
        n -= 2;  // '.' and '..'
        if (n > 0) {
            return n;
        }
    }
    CNcbiError::Set(CNcbiError::eUnknown);
    return -1;
}


int CLinuxFeature::GetFileDescriptorsCount(pid_t pid)
{
    int n = 0;
    string name = PROCFS(pid, "fd");
    DIR* dir = opendir(name.c_str());
    if (dir) {
        while (readdir(dir) != NULL) {
            ++n;
        }
        closedir(dir);
        n -= 3;  // '.', '..'  and the one for opendir()
        if (n >= 0) {
            return n;
        }
    }
    CNcbiError::Set(CNcbiError::eUnknown);
    return -1;
}


CLinuxFeature::CProcStat::CProcStat(pid_t pid)
{
    m_Parsed = false;

    char buf[2048];
    size_t n = s_ReadFile(PROCFS(pid,"stat"), buf, sizeof(buf));

    m_Storage.reserve(n);
    m_Storage.assign(buf, n);
    m_List.clear();    
    m_List.reserve(55);

    size_t p1 = m_Storage.find('(');
    if (p1 == NPOS) {
        return;
    }
    m_List.push_back(CTempString(m_Storage, 0, p1-1));
    size_t p2 = m_Storage.find(')', p1+1);
    if (p2 == NPOS) {
        return;
    }
    m_List.push_back(CTempString(m_Storage, p1+1, p2-p1-1));
    NStr::Split(m_Storage.data() + p2 + 1, " ", m_List);

    m_Parsed = true;    
    return;
}

#endif


END_NCBI_SCOPE
