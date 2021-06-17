#ifndef CORELIB___NCBI_OS_UNIX_P__HPP
#define CORELIB___NCBI_OS_UNIX_P__HPP

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
 * Author:  Anton Lavrentiev
 *
 *
 */

/// @file ncbi_os_unix_p.hpp
///
/// Private UNIX specific features.
///

#include <ncbiconf.h>
#if !defined(NCBI_OS_UNIX)
#   error "ncbi_os_unix_p.hpp can be used on UNIX platforms only"
#endif

#include <grp.h>
#include <pwd.h>

#if defined(NCBI_OS_LINUX)
#   include <corelib/tempstr.hpp>
#   include <corelib/ncbi_process.hpp>
#   include <sys/types.h>
#endif


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
/// CUnixFeature --
///
/// Utility class with wrappers for UNIX features.

class CUnixFeature
{
public:
    /// Look up user name by given numeric user ID.
    ///
    /// @param user
    ///   Numeric user ID
    /// @return
    ///   Non-empty string on success, empty string on error
    static string GetUserNameByUID(uid_t uid);

    /// Look up numeric user ID by symbolic user name.
    ///
    /// @param user
    ///   User name whose ID is looked up
    /// @return
    ///   Numeric user ID when succeeded, or (uid_t)(-1) when failed
    static uid_t GetUserUIDByName(const string& user);

    /// Look up group name by given numeric group ID.
    ///
    /// @param gid
    ///   Numeric group ID
    /// @return
    ///   Non-empty string on success, empty string on error
    static string GetGroupNameByGID(gid_t gid);

    /// Look up numeric group ID by symbolic group name.
    ///
    /// @param group
    ///   Group name whose ID is looked up
    /// @return
    ///   Numeric group ID when succeeded, or (gid_t)(-1) when failed
    static gid_t GetGroupGIDByName(const string& group);
};


#if defined(NCBI_OS_LINUX)

/////////////////////////////////////////////////////////////////////////////
///
/// CLinuxFeature --
///
/// Utility class with wrappers for Linux specific features.

class CLinuxFeature : public CUnixFeature
{
public:
    /// Get thread count.
    ///
    /// Read and parse /proc/<pid>/task/.
    /// If "pid" not specified or zero, use pid for the current process.
    static int GetThreadCount(pid_t pid = 0);
    
    /// Get consumed file descriptors count.
    ///
    /// Read and parse /proc/<pid>/fd/.
    /// If "pid" not specified or zero, use pid for the current process.
    static int GetFileDescriptorsCount(pid_t pid = 0);

    /// Get process memory usage.
    /// 
    /// Read and parse /proc/<pid>/statm and /status.
    /// If "pid" is zero, use pid for the current process.
    /// @sa
    ///   CProcess::GetMemoryUsage, CProcess::SMemoryUsage
    static bool GetMemoryUsage(pid_t pid, CProcess::SMemoryUsage& usage);

    /// Read and parse /proc/<pid>/stat file.
    class CProcStat
    {
    public:
        /// Read and parse /proc/<pid>/stat file.
        /// If "pid" not specified or zero, use pid for the current process.
        CProcStat(pid_t pid = 0);

        bool good() const
            {
                return m_Parsed && size() > 2;
            }
        
        /// Returns a reference to the element at position n.
        CTempString at(size_t n) const
            {
                if (n >= size()) {
                    return CTempString();
                }
                return m_List.at(n);
            }

        /// Returns the number of elements in the file.
        size_t size(void) const
            {
                return m_List.size();
            }

    private:
        string m_Storage;
        vector<CTempString> m_List;
        bool   m_Parsed;
    };
    
};

#endif



END_NCBI_SCOPE


#endif  /* CORELIB___NCBI_OS_UNIX_P__HPP */
