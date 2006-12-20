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
 * Authors:  Vladimir Ivanov
 *
 * File Description:   MS Windows specifics.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbistr.hpp>
#include "ncbi_os_mswin_p.hpp"


BEGIN_NCBI_SCOPE


string CWinSecurity::GetUserName(void)
{
    char  name[UNLEN + 1];
    DWORD name_size = UNLEN + 1;

    if ( !::GetUserName(name, &name_size) ) {
        return kEmptyStr;
    }
    return name;
}


PSID CWinSecurity::GetUserSID(const string& username)
{
    string x_username = username.empty() ? GetUserName() : username;
    if ( x_username.empty() ) {
        return NULL;
    }

    PSID         sid         = NULL;
    DWORD        sid_size    = 0;
    char*        domain      = NULL;
    DWORD        domain_size = 0;
    SID_NAME_USE use         = SidTypeUnknown;

    try {
        // First call to LookupAccountName to get the buffer sizes
        BOOL ret = LookupAccountName(NULL, x_username.c_str(),
                                     sid, &sid_size,
                                     domain, &domain_size, &use);
        if ( !ret  &&  GetLastError() != ERROR_INSUFFICIENT_BUFFER ) {
            return NULL;
        }
        // Reallocate memory for the buffers
        sid    = (PSID) LocalAlloc(LMEM_FIXED, sid_size);
        domain = (char*)malloc(domain_size);
        if ( !sid  || !domain ) {
            throw(0);
        }
        // Second call to LookupAccountName to get the account info
        ret = LookupAccountName(NULL, x_username.c_str(),
                                sid, &sid_size,
                                domain, &domain_size, &use);
        if ( !ret ) {
            // Unknown local user
            throw(0);
        }
    }
    catch (int) {
        if ( sid ) LocalFree(sid);
        sid = NULL;
    }
    // Clean up
    if ( domain ) free(domain);

    return sid;
}


void CWinSecurity::FreeUserSID(PSID sid)
{
    if ( sid ) {
        LocalFree(sid);
    }
}


// Helper function for GetOwner
bool s_LookupAccountSid(PSID sid, string* account, string* domain = 0)
{
    // Accordingly MSDN max account name size is 20, domain name size is 256.
    #define MAX_ACCOUNT_LEN  256

    char  account_name[MAX_ACCOUNT_LEN];
    char  domain_name [MAX_ACCOUNT_LEN];
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

    tp_prev.PrivilegeCount = 1;
    tp_prev.Privileges[0].Luid = luid;

    if ( enable ) {
        tp_prev.Privileges[0].Attributes |= (SE_PRIVILEGE_ENABLED);
    } else {
        tp_prev.Privileges[0].Attributes ^= 
            (SE_PRIVILEGE_ENABLED & tp_prev.Privileges[0].Attributes);
    }
    AdjustTokenPrivileges(token, FALSE, &tp_prev, tp_prev_size, NULL, NULL);
    if ( GetLastError() != ERROR_SUCCESS ) {
        return false;
    }
    // Privilege settings changed
    return true;
}


bool CWinSecurity::GetObjectOwner(const string&  objname,
                                  SE_OBJECT_TYPE objtype,
                                  string* owner, string* group)
{
    PSID sid_owner;
    PSID sid_group;
    PSECURITY_DESCRIPTOR sd = NULL;

    if ( GetNamedSecurityInfo(
            (LPSTR)objname.c_str(), objtype,
            OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION,
            &sid_owner, &sid_group, NULL, NULL, &sd )
            != ERROR_SUCCESS ) {
      return false;
    }
    // Get owner
    if ( owner  &&  !s_LookupAccountSid(sid_owner, owner) ) {
        LocalFree(sd);
        return false;
    }
    // Get group
    if ( group  &&  !s_LookupAccountSid(sid_group, group) ) {
        // This is not an error, because the group name on Windows
        // is an auxiliary information. Sometimes accounts can not
        // belongs to groups, or we don't have permissions to get
        // such information.
        *group = kEmptyStr;
    }
    LocalFree(sd);
    return true;
}


bool CWinSecurity::SetFileOwner(const string& filename, const string& owner)
{
    if ( owner.empty() ) {
        return false;
    }

    // Get access token
    HANDLE token = INVALID_HANDLE_VALUE;
    if ( !OpenProcessToken(GetCurrentProcess(),
                           TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
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

        // First call to LookupAccountName() to get the buffer sizes
        DWORD        sid_size    = 0;
        DWORD        domain_size = 0;
        SID_NAME_USE use  = SidTypeUnknown;
        BOOL res;
        res = LookupAccountName(NULL, owner.c_str(),
                                NULL, &sid_size, 
                                NULL, &domain_size, &use);
        if ( !res  &&  GetLastError() != ERROR_INSUFFICIENT_BUFFER )
            throw(0);

        // Reallocate memory for the buffers
        sid    = (PSID) LocalAlloc(LMEM_FIXED, sid_size);
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
        if ( !SetFileSecurity(filename.c_str(),
                (SECURITY_INFORMATION)(OWNER_SECURITY_INFORMATION), sd) ) {
            throw(0);
        }
    }
    catch (int) {
        success = false;
    }

    // Clean up
    if ( sid )    LocalFree(sid);
    if ( domain ) free(domain);
    CloseHandle(token);

    // Return result
    return success;
}


#define FILE_SECURITY_INFO (OWNER_SECURITY_INFORMATION | \
                            GROUP_SECURITY_INFORMATION | \
                            DACL_SECURITY_INFORMATION)


PSECURITY_DESCRIPTOR CWinSecurity::GetFileSD(const string& path)
{
    if ( path.empty() ) {
        return NULL;
    }
    PSECURITY_DESCRIPTOR sid = NULL;
    DWORD size               = 0;
    DWORD size_need          = 0;

    if ( !GetFileSecurity(path.c_str(), FILE_SECURITY_INFO,
                          sid, size, &size_need) ) {
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            return NULL;
        }
        // Allocate memory for the buffer
        sid = (PSECURITY_DESCRIPTOR) LocalAlloc(LMEM_FIXED, size_need);
        if ( !sid ) {
            return NULL;
        }
        size = size_need;
        if ( !GetFileSecurity(path.c_str(), FILE_SECURITY_INFO,
                              sid, size, &size_need) ) {
            return NULL;
        }
    }
    return sid;
}


bool CWinSecurity::GetFileDACL(const string& strPath,
                               PSECURITY_DESCRIPTOR* pFileSD, PACL* pDACL)
{
    if ( strPath.empty() ) {
        return false;
    }
    DWORD dwRet = 0;
    dwRet = GetNamedSecurityInfo((LPSTR)strPath.c_str(), SE_FILE_OBJECT,
                                 FILE_SECURITY_INFO,
                                 NULL, NULL, pDACL, NULL, pFileSD);
    if (dwRet != ERROR_SUCCESS) {
        pFileSD = NULL;
        pDACL   = NULL;
    }
    return (dwRet == ERROR_SUCCESS);
}


void CWinSecurity::FreeFileSD(PSECURITY_DESCRIPTOR sd)
{
    if ( sd ) {
        LocalFree(sd);
    }
}


// We don't use GetEffectiveRightsFromAcl() here, because it is very limited
// and very often works incorrect. Microsoft don't recommend to use it.
// So, permission can be taken for the current process thread owner only :(

bool CWinSecurity::GetFilePermissions(const string& path,
                                      ACCESS_MASK*  permissions)
{
    if ( !permissions ) {
        return false;
    }
    // Get DACL for the file
    PSECURITY_DESCRIPTOR sd = GetFileSD(path);

    if ( !sd ) {
        if ( GetLastError() == ERROR_ACCESS_DENIED ) {
            *permissions = 0;  
            return true;
        }
        return false;
    }

    HANDLE token = INVALID_HANDLE_VALUE;
    bool success = true;

    try {
        // Perform security impersonation of the user and
        // open current thread token

        if ( !ImpersonateSelf(SecurityImpersonation) ) {
            throw(0);
        }
        if ( !OpenThreadToken(GetCurrentThread(),
                              TOKEN_DUPLICATE | TOKEN_QUERY,
                              FALSE, &token) ) {
            throw(0);
        }
        RevertToSelf();

        DWORD access = MAXIMUM_ALLOWED;
        GENERIC_MAPPING mapping;
        memset(&mapping, 0, sizeof(GENERIC_MAPPING));

        PRIVILEGE_SET  privileges;
        memset(&privileges, 0, sizeof(PRIVILEGE_SET));
        DWORD          privileges_size = sizeof(PRIVILEGE_SET);
        BOOL           status = true;

        if ( !AccessCheck(sd, token, access, &mapping,
                        &privileges, &privileges_size, permissions,
                        &status)  ||  !status ) {
            throw(0);
        }
    }
    catch (int) {
        *permissions = 0;
        success = false;
    }

    // Clean up
    CloseHandle(token);
    FreeFileSD(sd);

    return success;
}


END_NCBI_SCOPE


/*
 * ==========================================================================
 * $Log$
 * Revision 1.5  2006/12/20 18:48:54  gouriano
 * Init memory buffer before using it
 *
 * Revision 1.4  2005/12/08 14:15:49  ivanov
 * Rewritten CWinSecurity::GetFilePermissions() using AccessCheck()
 * instead of GetEffectiveRightsFromAcl(), which is not recommended to use.
 * Some code cleanup.
 *
 * Revision 1.3  2005/12/05 19:23:50  ivanov
 * CWinSecurity::GetFilePermissions() -- fast fix: return false
 * if permissions is 0. Need to investigate.
 *
 * Revision 1.2  2005/11/30 17:05:14  kuznets
 * include corelib/ncbistr.hpp
 *
 * Revision 1.1  2005/11/30 11:52:28  ivanov
 * Initial draft revision
 *
 * ==========================================================================
 */
