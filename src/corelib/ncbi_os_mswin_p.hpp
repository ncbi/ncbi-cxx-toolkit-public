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
    /// @param strUserName
    ///   User account name.
    /// @return 
    ///   Pointer to security identifier for specified account name,
    ///   or NULL if the function fails.
    ///   When you have finished using the user SID, free the returned buffer
    ///   by calling FreeUserSID() method.
    /// @sa
    ///   FreeUserSID, GetUserName
    static PSID GetUserSID(const string& strUserName);


    /// Deallocate memory used for user SID.
    ///
    /// @param pUserSID
    ///   Pointer to buffer with user SID.
    /// @sa
    ///   GetUserSID
    static void FreeUserSID(PSID pUserSID);


    /// Get owner for specified system object.
    ///
    /// Retrieve the name of the account and the name of the first
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
    /// @return
    ///   TRUE if successful, FALSE otherwise.
    static bool GetObjectOwner(const string& objname, SE_OBJECT_TYPE objtype,
                               string* owner, string* group = 0);


    /// Get file objects owner.
    ///
    /// @sa SetFileOwner
    static bool GetFileOwner(const string& filename,
                             string* owner, string* group = 0)
        { return GetObjectOwner(filename, SE_FILE_OBJECT, owner, group);  }


    /// Set file object owner.
    ///
    /// You should have administrative rights to change an owner.
    /// Only administrative privileges (Backup, Restore and Take Ownership)
    /// grant rights to change ownership. Without one of the privileges, an
    /// administrator cannot take ownership of any file or give ownership
    /// back to the original owner. Also, we cannot change user group here,
    /// so it will be ignored.
    /// @param owner
    ///   New owner name to set.
    /// @return
    ///   TRUE if successful, FALSE otherwise.
    /// @sa
    ///   GetFileOwner
    static bool SetFileOwner(const string& filename, const string& owner);

    /// Get the file object DACL.
    ///
    /// Retrieves a copy of the security descriptor and DACL (discretionary
    /// access control list) for an object specified by name.
    /// NOTE: Do not forget to deallocated memory for returned
    ///       file security descriptor.
    /// @param strPath
    ///   Path to the file or directory.
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
    ///   FreeFileSD
    static bool GetFileDACL(const string& strPath,
                            /*out*/ PSECURITY_DESCRIPTOR* pFileSD,
                            /*out*/ PACL* pDACL);


    /// Deallocate memory used for file security descriptor.
    ///
    /// @param pFileSD
    ///   Pointer to buffer with file security descriptor, allocated
    ///   by GetFileDACL() method.
    /// @sa
    ///   GetFileDACL
    static void FreeFileSD(PSECURITY_DESCRIPTOR pFileSD);


    /// Get file access permissions for specified user.
    ///
    /// @param strPath
    ///   Path to the file or directory.
    /// @param strUserName
    ///   User account name. If it was not specified or is empty, that
    ///   name of the current process thread owner will be used.
    /// @param pPermissions
    ///   Pointer to a variable that receives a file acess mask.
    ///   See MSDN or WinNT.h for all access rights constants.
    /// @return
    ///   TRUE if the operation was completed successfully; FALSE, otherwise.
    /// @sa
    ///   GetUserName, GetUserSID, GetFileDACL
    static bool GetFilePermissions(const string& strPath,
                                   const string& strUserName,
                                   ACCESS_MASK*  pPermissions);


    /// Get file access permissions for current logged user.
    ///
    /// @param strPath
    ///   Path to the file or directory.
    /// @param pPermissions
    ///   Pointer to a variable that receives a file acess mask.
    ///   See MSDN or WinNT.h for all access rights constants.
    /// @return
    ///   TRUE if the operation was completed successfully; FALSE, otherwise.
    /// @sa
    ///   GetUserName, GetUserSID, GetFileDACL
    static bool GetFilePermissions(const string& strPath,
                                   ACCESS_MASK*  pPermissions);  


    /// Get file access permissions for specified user security descriptor.
    ///
    /// @param strPath
    ///   Path to the file or directory.
    /// @param pUserSID
    ///   User security descriptor.
    /// @param pPermissions
    ///   Pointer to a variable that receives a file acess mask.
    ///   See MSDN or WinNT.h for all access rights constants.
    /// @return
    ///   TRUE if the operation was completed successfully; FALSE, otherwise.
    /// @sa
    ///   GetUserSID, GetFileDACL
    static bool GetFilePermissions(const string& strPath,
                                   PSID          pUserSID,
                                   ACCESS_MASK*  pPermissions);
};

END_NCBI_SCOPE


#endif  /* CORELIB___NCBI_OS_MSWIN_P__HPP */
