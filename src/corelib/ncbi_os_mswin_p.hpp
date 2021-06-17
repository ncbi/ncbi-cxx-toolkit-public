#ifndef CORELIB___NCBI_OS_MSWIN_P__HPP
#define CORELIB___NCBI_OS_MSWIN_P__HPP

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
 *
 */

/// @file ncbi_os_mswin_p.hpp
///
/// Defines MS Windows specific private functions and classes.
///

#include <ncbiconf.h>
#if !defined(NCBI_OS_MSWIN)
#  error "ncbi_os_mswin_p.hpp can be used on MS Windows platforms only"
#endif

#include <corelib/ncbi_os_mswin.hpp>

// Access Control APIs
#include <accctrl.h>
#include <aclapi.h>
#include <Lmcons.h>

#include <tlhelp32.h>   // CreateToolhelp32Snapshot


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
/// CWinSecurity --
///
/// Utility class with wrappers for MS Windows security functions.

class CWinSecurity
{
public:
    /// Get name of the current user.
    ///
    /// Retrieves the user name of the current thread.
    /// This is the name of the user currently logged onto the system.
    /// @return
    ///   Current user name, or empty string if there was an error.
    static string GetUserName(void);

    /// Get owner name of specified system object.
    ///
    /// Retrieve the name of the named object owner and the name of the first
    /// group, which the account belongs to.  The obtained group name may
    /// be an empty string, if we don't have permissions to get it.
    /// Win32 really does not use groups, but they exist for the sake
    /// of POSIX compatibility.
    /// Windows 2000/XP: In addition to looking up for local accounts,
    /// local domain accounts, and explicitly trusted domain accounts,
    /// it also can look for any account in any known domain around.
    /// @param owner
    ///   Pointer to a string to receive an owner name.
    /// @param group
    ///   Pointer to a string to receive a group name. 
    /// @param uid
    ///   Pointer to an int to receive a (fake) user id.
    /// @param gid
    ///   Pointer to an int to receive a (fake) group id.
    /// @return
    ///   TRUE if successful, FALSE otherwise.
    static bool GetObjectOwner(const string& obj_name, SE_OBJECT_TYPE obj_type,
                               string* owner, string* group,
                               unsigned int* uid = 0, unsigned int* gid = 0);

    /// Same as GetObjectOwner(objname) but gets the owner/group information
    /// by an arbitrary handle rather than by name.
    static bool GetObjectOwner(HANDLE        obj_handle, SE_OBJECT_TYPE obj_type,
                               string* owner, string* group,
                               unsigned int* uid = 0, unsigned int* gid = 0);


    /// Get file owner name.
    ///
    /// @sa 
    ///   GetObjectOwner, SetFileOwner
    static bool GetFileOwner(const string& filename,
                             string* owner, string* group,
                             unsigned int* uid = 0, unsigned int* gid = 0)
    {
        return GetObjectOwner(filename, SE_FILE_OBJECT, owner, group, uid, gid);
    }


    /// Set file object owner.
    ///
    /// You should have administrative rights to change an owner.
    /// Only administrative privileges (Restore and Take Ownership)
    /// grant rights to change ownership. Without one of the privileges,
    /// an administrator cannot take ownership of any file or give ownership
    /// back to the original owner.
    /// @param filename
    ///   Filename to change the owner of.
    /// @param owner
    ///   New owner name to set. If specified as empty, then is not changed.
    /// @param group
    ///   New group name to set. If specified as empty, then is not changed.
    /// @param uid
    ///   To receive (fake) numeric user id of the prospective owner
    ///   (even if the ownership change was unsuccessful), or 0 if unknown.
    /// @param gid
    ///   To receive (fake) numeric user id of the prospective group
    ///   (even if the ownership change was unsuccessful), or 0 if unknown.
    /// @return
    ///   TRUE if successful, FALSE otherwise.
    /// @sa
    ///   GetFileOwner, SetThreadPrivilege, SetTokenPrivilege
    static bool SetFileOwner(/* in  */ const string& filename, 
                             /* in  */ const string& owner, const string& group = kEmptyStr,
                             /* out */ unsigned int* uid = 0 , unsigned int* gid = 0);


    /// Enables or disables privileges in the specified access token.
    ///
    /// In most case you should have administrative rights to change
    /// some privileges.
    /// @param token
    ///   A handle to the access token that contains the privileges
    ///   to be modified. The handle must have TOKEN_ADJUST_PRIVILEGES
    ///   and TOKEN_QUERY access to the token.
    /// @param privilege
    ///   Name of privilege to enable/disable.
    /// @param enable
    ///   TRUE/FALSE, to enable or disable privilege.
    /// @param prev
    ///   To receive previous state of chnaged privilege (if specified).
    /// @return
    ///   TRUE if successful, FALSE otherwise.
    /// @sa
    ///   SetFileOwner, SetThreadPrivilege
    static bool SetTokenPrivilege(HANDLE token, LPCTSTR privilege,
                                  bool enable, bool* prev = 0);

    /// Enables or disables privileges for the current thread.
    ///
    /// In most case you should have administrative rights to change
    /// some privileges.
    /// @param privilege
    ///   Name of privilege to enable/disable.
    /// @param enable
    ///   TRUE/FALSE, to enable or disable privilege.
    /// @param prev
    ///   To receive previous state of chnaged privilege (if specified).
    /// @return
    ///   TRUE if successful, FALSE otherwise.
    /// @sa
    ///   SetFileOwner, SetTokenPrivilege
    static bool SetThreadPrivilege(LPCTSTR privilege,
                                   bool enable, bool* prev = 0);

    /// Get file access permissions.
    ///
    /// The permissions will be taken for current process thread owner only.
    /// @param strPath
    ///   Path to the file object.
    /// @param pPermissions
    ///   Pointer to a variable that receives a file access mask.
    ///   See MSDN or WinNT.h for all access rights constants.
    /// @return
    ///   TRUE if the operation was completed successfully; FALSE, otherwise.
    static bool GetFilePermissions(const string& path, ACCESS_MASK*  permissions);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CWinFeature --
///
/// Utility class with wrappers for MS Windows specific features.

class CWinFeature
{
public:
    /// Find process entry information by process identifier (pid).
    ///
    /// @param id
    ///   Process identifier to look for.
    /// @param entry
    ///   Entry to store retrieved information.
    /// @return
    ///   TRUE if the operation was completed successfully; FALSE, otherwise.
    static bool FindProcessEntry(DWORD id, PROCESSENTRY32& entry);
};


END_NCBI_SCOPE


#endif  /* CORELIB___NCBI_OS_MSWIN_P__HPP */
