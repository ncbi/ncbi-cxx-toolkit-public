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
#  error "ncbi_os_mswin_p.hpp must be used on MS Windows platforms only"
#endif

#include <corelib/ncbi_os_mswin.hpp>

// Access Control APIs
#include <accctrl.h>
#include <aclapi.h>
#include <Lmcons.h>


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


    /// Get user SID by name.
    ///
    /// We get user SID on local machine only. We don't use domain controller
    /// server because it require to use NetAPI and link Netapi32.lib to each
    /// application uses the C++ Toolkit, that is undesirable.
    /// NOTE: Do not forget to deallocated memory for returned
    ///       user security identifier.
    /// @param username
    ///   User account name. If empty - the owner of the current thread.
    /// @return 
    ///   Pointer to security identifier for specified account name,
    ///   or NULL if the function fails.
    ///   When you have finished using the user SID, free the returned buffer
    ///   by calling FreeUserSID() method.
    /// @sa
    ///   FreeUserSID, GetUserName
    static PSID GetUserSID(const string& username = kEmptyStr);


    /// Deallocate memory used for user SID.
    ///
    /// @param sid
    ///   Pointer to allocated buffer with user SID.
    /// @sa
    ///   GetUserSID
    static void FreeUserSID(PSID sid);


    /// Get owner name of specified system object.
    ///
    /// Retrieve the name of the named object owner and the name of the first
    /// group, which the account belongs to. The obtained group name may
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
    ///   Pointer to an int to received a(fake) group id.
    /// @return
    ///   TRUE if successful, FALSE otherwise.
    static bool GetObjectOwner(const string& objname, SE_OBJECT_TYPE objtype,
                               string* owner, string* group = 0,
                               unsigned int* uid = 0, unsigned int* gid = 0);

    /// Same as GetObjectOwner(objname) but gets the owner/group information
    /// by an arbitrary handle rather than by name.
    static bool GetObjectOwner(HANDLE        objhndl, SE_OBJECT_TYPE objtype,
                               string* owner, string* group = 0,
                               unsigned int* uid = 0, unsigned int* gid = 0);


    /// Get file owner name.
    ///
    /// @sa 
    ///   GetObjectOwner, SetFileOwner
    static bool GetFileOwner(const string& filename,
                             string* owner, string* group = 0,
                             unsigned int* uid = 0, unsigned int* gid = 0)
    {
        return GetObjectOwner(filename, SE_FILE_OBJECT, owner, group, uid,gid);
    }


    /// Set file object owner.
    ///
    /// You should have administrative rights to change an owner.
    /// Only administrative privileges (Backup, Restore and Take Ownership)
    /// grant rights to change ownership.  Without one of the privileges,
    /// an administrator cannot take ownership of any file or give ownership
    /// back to the original owner.
    /// @param filename
    ///   Filename to change the owner of.
    /// @param owner
    ///   New owner name to set.
    /// @param uid
    ///   To receive (fake) numeric user id of the prospective owner
    ///   (even if the ownership change was unsuccessful), or 0 if unknown.
    /// @return
    ///   TRUE if successful, FALSE otherwise.
    /// @sa
    ///   GetFileOwner
    static bool SetFileOwner(const string& filename, const string& owner,
                             unsigned int* uid = 0);


    /// Get the file security descriptor.
    ///
    /// Retrieves a copy of the security descriptor for a file object
    /// specified by name.
    /// NOTE: Do not forget to deallocated memory for returned
    ///       file security descriptor.
    /// @param path
    ///   Path to the file object.
    /// @return
    ///   Pointer to the security descriptor of the file object,
    ///   or NULL if the function fails.
    ///   When you have finished using security descriptor, free 
    ///   the returned pointer to allocated memory calling 
    ///   FreeFileSD() method.
    /// @sa
    ///   FreeFileSD
    static PSECURITY_DESCRIPTOR GetFileSD(const string& path);


    /// Get the file object security descriptor and DACL.
    ///
    /// Retrieves a copy of the security descriptor and DACL (discretionary
    /// access control list) for an object specified by name.
    /// NOTE: Do not forget to deallocated memory for returned
    ///       file security descriptor.
    /// @param strPath
    ///   Path to the file object.
    /// @param pFileSD
    ///   Pointer to a variable that receives a pointer to the security descriptor
    ///   of the object. When you have finished using the DACL, free 
    ///   the returned buffer by calling the FreeFileSD() method.
    /// @param pDACL
    ///   Pointer to a variable that receives a pointer to the DACL inside
    ///   the security descriptor pFileSD. The file object can contains NULL DACL,
    ///   that means that means that all access is present for this file.
    ///   Therefore user has all the permissions.
    /// @return
    ///   TRUE if the operation was completed successfully, pFileSD and pDACL
    ///   contains pointers to the file security descriptor and DACL;
    ///   FALSE, otherwise.
    /// @sa
    ///   GetFileSD, FreeFileSD
    static bool GetFileDACL(const string& path,
                            /*out*/ PSECURITY_DESCRIPTOR* file_sd,
                            /*out*/ PACL* dacl);


    /// Deallocate memory used for file security descriptor.
    ///
    /// @param pFileSD
    ///   Pointer to buffer with file security descriptor, allocated
    ///   by GetFileDACL() method.
    /// @sa
    ///   GetFileDACL
    static void FreeFileSD(PSECURITY_DESCRIPTOR file_sd);


    /// Get file access permissions.
    ///
    /// The permisiions will be taken for current process thread owner only.
    /// @param strPath
    ///   Path to the file object.
    /// @param pPermissions
    ///   Pointer to a variable that receives a file acess mask.
    ///   See MSDN or WinNT.h for all access rights constants.
    /// @return
    ///   TRUE if the operation was completed successfully; FALSE, otherwise.
    static bool GetFilePermissions(const string& path,
                                   ACCESS_MASK*  permissions);
};


END_NCBI_SCOPE


#endif  /* CORELIB___NCBI_OS_MSWIN_P__HPP */
