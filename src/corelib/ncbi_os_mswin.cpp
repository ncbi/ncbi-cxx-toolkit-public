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
#include <corelib/ncbierror.hpp>
#include "ncbi_os_mswin_p.hpp"


// According to MSDN max account name size is 20, domain name size is 256
#define MAX_ACCOUNT_LEN  256

// Hopefully this makes UID/GID compatible with what CYGWIN reports
#define CYGWIN_MAGIC_ID_OFFSET  10000

// Security access info
#define ACCOUNT_SECURITY_INFO  (OWNER_SECURITY_INFORMATION | \
                                GROUP_SECURITY_INFORMATION)

#define FILE_SECURITY_INFO     (OWNER_SECURITY_INFORMATION | \
                                GROUP_SECURITY_INFORMATION | \
                                DACL_SECURITY_INFORMATION)

BEGIN_NCBI_SCOPE


string CWinSecurity::GetUserName(void)
{
    TXChar name[UNLEN + 1];
    DWORD  name_size = sizeof(name) / sizeof(name[0]) - 1;

    if ( !::GetUserName(name, &name_size) ) {
        CNcbiError::SetFromWindowsError();
        return kEmptyStr;
    }
    name[name_size] = _TX('\0');
    return _T_STDSTRING(name);
}


// Get SID by account name.
// Return NULL on error.
// Do not forget to free the returned SID by calling LocalFree().

static PSID x_GetAccountSidByName(const string& account, SID_NAME_USE type = (SID_NAME_USE)0)
{
    PSID         sid         = NULL;
    DWORD        sid_size    = 0;
    TXChar*      domain      = NULL;
    DWORD        domain_size = 0;
    SID_NAME_USE use;

    TXString name(_T_XSTRING(account));

    // First call to LookupAccountName() to get the buffer sizes
    if ( !LookupAccountName(NULL, name.c_str(), sid, &sid_size, domain, &domain_size, &use) 
          &&  GetLastError() != ERROR_INSUFFICIENT_BUFFER ) {
        CNcbiError::SetFromWindowsError();
        return NULL;
    }
    try {
        // Allocate buffers
        sid    = (PSID) LocalAlloc(LMEM_FIXED, sid_size);
        domain = (TXChar*) malloc(domain_size * sizeof(TXChar));
        if ( !sid  ||  !domain ) {
            throw(0);
        }
        // Second call to get the actual account info
        if ( !LookupAccountName(NULL, name.c_str(), sid, &sid_size, domain, &domain_size, &use) ) {
            CNcbiError::SetFromWindowsError();
            throw(0);
        }
        // Check type of account
        if (type  &&  type != use ) {
            CNcbiError::Set(CNcbiError::eUnknown);
            throw(0);
        }
    }
    catch (int) {
        LocalFree(sid);
        sid = NULL;
    }
    // Clean up
    if ( domain ) free(domain);

    return sid;
}


// Get account name by SID.
// Note: *domatch is reset to 0 if the account type has no domain match.
#include <stdio.h>
#include <tchar.h>

static bool x_GetAccountNameBySid(PSID sid, string* account, int* domatch = 0)
{
    _ASSERT(account);

    // Use predefined buffers for account/domain names to avoid additional
    // step to get its sizes. According to MSDN max account/domain size
    // do not exceed MAX_ACCOUNT_LEN symbols (char or wchar).

    TXChar   account_name[MAX_ACCOUNT_LEN + 2];
    TXChar   domain_name [MAX_ACCOUNT_LEN + 2];
    DWORD    account_size = sizeof(account_name)/sizeof(account_name[0]) - 1;
    DWORD    domain_size  = sizeof(domain_name)/sizeof(domain_name[0]) - 1;
    SID_NAME_USE use;

    // Always get both account & domain name, even we don't need last.
    // Because if domain name is NULL, this function can throw unhandled
    // exception in Unicode builds on some platforms.
    if ( !LookupAccountSid(NULL, sid, 
                           account_name, &account_size,
                           domain_name,  &domain_size, &use) ) {
        CNcbiError::SetFromWindowsError();
        return false;
    }

    // Save account information
    account_name[account_size] = _TX('\0');
    account->assign(_T_STDSTRING(account_name));

    if (domatch) {
        domain_name[domain_size] = _TX('\0');
        string domain(_T_STDSTRING(domain_name));
        if (*domatch != int(use)  ||  domain.empty()
            ||  NStr::EqualNocase(domain, "builtin")
            ||  NStr::FindNoCase(domain, " ") != NPOS
            /*||  x_DomainIsLocalComputer(domain_name)*/) {
            *domatch = 0;
        }
    }
    return true;
}


// Get account owner/group names and uids by SID

static bool s_GetOwnerGroupFromSIDs(PSID owner_sid, PSID group_sid,
                                    string* owner_name, string* group_name,
                                    unsigned int* uid, unsigned int* gid)
{
    bool success = true;

    // Get numeric owner
    if ( uid ) {
        int match = SidTypeUser;
        if ( !x_GetAccountNameBySid(owner_sid, owner_name, &match) ) {
            if ( owner_name )
                success = false;
            *uid = 0;
        } else {
            *uid = match ? CYGWIN_MAGIC_ID_OFFSET : 0;
        }
        owner_name = NULL;
        *uid += *GetSidSubAuthority(owner_sid, *GetSidSubAuthorityCount(owner_sid) - 1);
    }
    // Get numeric group
    if ( gid ) {
        int match = SidTypeGroup;
        if ( !x_GetAccountNameBySid(group_sid, group_name, &match) ) {
            *gid = 0;
        } else {
            *gid = match ? CYGWIN_MAGIC_ID_OFFSET : 0;
        }
        group_name = NULL;
        *gid += *GetSidSubAuthority(group_sid, *GetSidSubAuthorityCount(group_sid) - 1);
    }
    if ( !success ) {
        return false;
    }

    // Get owner name
    if ( owner_name  &&  !x_GetAccountNameBySid(owner_sid, owner_name) ) {
        return false;
    }
    // Get group name
    if ( group_name  &&  !x_GetAccountNameBySid(group_sid, group_name) ) {
        // This is not an error, because the group name on Windows
        // is an auxiliary information.  Sometimes accounts cannot
        // belong to groups, or we don't have permissions to get
        // such information.
        group_name->clear();
    }
    return true;
}


bool CWinSecurity::GetObjectOwner(HANDLE         obj_handle,
                                  SE_OBJECT_TYPE obj_type,
                                  string* owner, string* group,
                                  unsigned int* uid, unsigned int* gid)
{
    PSID sid_owner;
    PSID sid_group;
    PSECURITY_DESCRIPTOR sd;

    DWORD res = GetSecurityInfo(obj_handle, obj_type, ACCOUNT_SECURITY_INFO,
                                &sid_owner, &sid_group, NULL, NULL, &sd );
    if ( res != ERROR_SUCCESS ) {
        CNcbiError::SetWindowsError(res);
        return false;
    }
    bool retval = s_GetOwnerGroupFromSIDs(sid_owner, sid_group, owner, group, uid, gid);
    LocalFree(sd);
    return retval;
}


bool CWinSecurity::GetObjectOwner(const string&  obj_name,
                                  SE_OBJECT_TYPE obj_type,
                                  string* owner, string* group,
                                  unsigned int* uid, unsigned int* gid)
{
    PSID sid_owner;
    PSID sid_group;
    PSECURITY_DESCRIPTOR sd;

    DWORD res = GetNamedSecurityInfo(_T_XCSTRING(obj_name), obj_type,
                                     ACCOUNT_SECURITY_INFO,
                                     &sid_owner, &sid_group, NULL, NULL, &sd );
    if ( res != ERROR_SUCCESS ) {
        CNcbiError::SetWindowsError(res);
        return false;
    }
    bool retval = s_GetOwnerGroupFromSIDs(sid_owner, sid_group, owner, group, uid, gid);
    LocalFree(sd);
    return retval;
}


// Get current thread token. Return INVALID_HANDLE_VALUE on error.

static HANDLE s_GetThreadToken(DWORD access)
{
    HANDLE token;
    if ( !OpenThreadToken(GetCurrentThread(), access, FALSE, &token) ) {
        DWORD res = GetLastError();
        if ( res == ERROR_NO_TOKEN ) {
            if ( !ImpersonateSelf(SecurityImpersonation) ) {
                // Failed to obtain a token for the current thread and user
                CNcbiError::SetFromWindowsError();
                return INVALID_HANDLE_VALUE;
            }
            if ( !OpenThreadToken(GetCurrentThread(), access, FALSE, &token) ) {
                // Failed to open the current threads token with the required access rights
                CNcbiError::SetFromWindowsError();
                token = INVALID_HANDLE_VALUE;
            }
            RevertToSelf();
        } else {
            // Failed to open the current threads token with the required access rights
            CNcbiError::SetWindowsError(res);
            return NULL;
        }
    }
    return token;
}


bool CWinSecurity::SetFileOwner(const string& filename,
                                const string& owner, const string& group, 
                                unsigned int* uid, unsigned int* gid)
{
    if ( uid ) *uid = 0;
    if ( gid ) *gid = 0;

    if ( owner.empty()  &&  group.empty() ) {
        CNcbiError::Set(CNcbiError::eInvalidArgument);
        return false;
    }

    HANDLE  token     = INVALID_HANDLE_VALUE;
    PSID    owner_sid = NULL;
    PSID    group_sid = NULL;
    bool    success   = false;

    // Get SIDs for new owner and group
    if ( !owner.empty() ) {
        owner_sid = x_GetAccountSidByName(owner, SidTypeUser);
        if (!owner_sid) {
            return false;
        }
    }
    if ( !group.empty() ) {
        group_sid = x_GetAccountSidByName(group, SidTypeGroup);
        if (!group_sid) {
            goto cleanup;
        }
    }
    if (uid || gid) {
        s_GetOwnerGroupFromSIDs(owner_sid, group_sid, NULL, NULL, uid, gid);
    }

    // Change owner

    SECURITY_INFORMATION security_info = 0;
    if ( owner_sid ) {
        security_info |= OWNER_SECURITY_INFORMATION;
    }
    if ( group_sid ) {
        security_info |= GROUP_SECURITY_INFORMATION;
    }

    // Set new owner/group in the object's security descriptor
    if ( SetNamedSecurityInfo((TXChar*)_T_XCSTRING(filename),
                              SE_FILE_OBJECT, security_info,
                              owner_sid, group_sid, NULL, NULL) == ERROR_SUCCESS ) {
        success = true;
        goto cleanup;
    }

    // If the previous call failed because access was denied,
    // enable the necessary admin privileges for the current thread and try again.

    token = s_GetThreadToken(TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY);
    if ( token == INVALID_HANDLE_VALUE) {
        goto cleanup;
    }
    bool prev_ownership_name;
    bool prev_restore_name;

    if ( !SetTokenPrivilege(token, SE_TAKE_OWNERSHIP_NAME, true, &prev_ownership_name) ||
         !SetTokenPrivilege(token, SE_RESTORE_NAME, true, &prev_restore_name) ) {
        goto cleanup;
    }
    if ( SetNamedSecurityInfo((TXChar*)_T_XCSTRING(filename),
                              SE_FILE_OBJECT, security_info,
                              owner_sid, group_sid, NULL, NULL) == ERROR_SUCCESS ) {
        success = true;
    }
    // Restore privileges
    SetTokenPrivilege(token, SE_TAKE_OWNERSHIP_NAME, prev_ownership_name);
    SetTokenPrivilege(token, SE_RESTORE_NAME, prev_restore_name);


cleanup:
    if ( owner_sid ) LocalFree(owner_sid);
    if ( group_sid ) LocalFree(group_sid);
    if ( token != INVALID_HANDLE_VALUE) CloseHandle(token);

    return success;
}


bool CWinSecurity::SetTokenPrivilege(HANDLE token, LPCTSTR privilege,
                                     bool enable, bool* prev)
{
    // Get privilege unique identifier
    LUID luid;
    if ( !LookupPrivilegeValue(NULL, privilege, &luid) ) {
        CNcbiError::SetFromWindowsError();
        return false;
    }

    // Get current privilege setting

    TOKEN_PRIVILEGES tp;
    TOKEN_PRIVILEGES tp_prev;
    DWORD            tp_size = sizeof(tp);

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = 0;

    AdjustTokenPrivileges(token, FALSE, &tp, tp_size, &tp_prev, &tp_size);
    DWORD res = GetLastError();
    if ( res != ERROR_SUCCESS ) {
        // Failed to obtain the current token's privileges
        CNcbiError::SetWindowsError(res);
        return false;
    }

    // Enable/disable privilege

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    if (prev) {
        *prev = ((tp_prev.Privileges[0].Attributes & SE_PRIVILEGE_ENABLED) == SE_PRIVILEGE_ENABLED);
    }
    tp.Privileges[0].Attributes = enable ? SE_PRIVILEGE_ENABLED : 0;

    AdjustTokenPrivileges(token, FALSE, &tp, tp_size, NULL, NULL);
    res = GetLastError();
    if ( res != ERROR_SUCCESS ) {
        // Failed to change privileges
        CNcbiError::SetWindowsError(res);
        return false;
    }
    // Privilege settings changed
    return true;
}


bool CWinSecurity::SetThreadPrivilege(LPCTSTR privilege, bool enable, bool* prev)
{
    HANDLE token = s_GetThreadToken(TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY);
    if ( token == INVALID_HANDLE_VALUE) {
        return false;
    }
    bool res = SetTokenPrivilege(token, privilege, enable, prev);
    return res;
}


// Get file security descriptor. Return NULL on error.
// NOTE: Do not forget to deallocated memory for returned descriptor.

static PSECURITY_DESCRIPTOR s_GetFileSecurityDescriptor(const string& path)
{
    if ( path.empty() ) {
        CNcbiError::Set(CNcbiError::eInvalidArgument);
        return NULL;
    }
    PSECURITY_DESCRIPTOR sd = NULL;
    DWORD size              = 0;
    DWORD size_need         = 0;

    if ( !GetFileSecurity(_T_XCSTRING(path), FILE_SECURITY_INFO, sd, size, &size_need) ) {
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            CNcbiError::SetFromWindowsError();
            return NULL;
        }
        // Allocate memory for the buffer
        sd = (PSECURITY_DESCRIPTOR) LocalAlloc(LMEM_FIXED, size_need);
        if ( !sd ) {
            CNcbiError::SetFromWindowsError();
            return NULL;
        }
        size = size_need;
        if ( !GetFileSecurity(_T_XCSTRING(path), FILE_SECURITY_INFO,
                              sd, size, &size_need) ) {
            CNcbiError::SetFromWindowsError();
            LocalFree((HLOCAL) sd);
            return NULL;
        }
    }
    return sd;
}


// We don't use GetEffectiveRightsFromAcl() here because it is very limited
// and very often works incorrectly.  Microsoft doesn't recommend to use it.
// So, permissions can be taken for the current process thread owner only :(

bool CWinSecurity::GetFilePermissions(const string& path,
                                      ACCESS_MASK*  permissions)
{
    if ( !permissions ) {
        CNcbiError::Set(CNcbiError::eBadAddress);
        return false;
    }

    // Get security descriptor for the file
    PSECURITY_DESCRIPTOR sd = s_GetFileSecurityDescriptor(path);
    if ( !sd ) {
        if ( CNcbiError::GetLast().Native() == ERROR_ACCESS_DENIED ) {
            *permissions = 0;  
            return true;
        }
        return false;
    }

    HANDLE token = INVALID_HANDLE_VALUE;
    bool success = true;

    try {
        // Open current thread token
        token = s_GetThreadToken(TOKEN_DUPLICATE | TOKEN_QUERY);
        if ( token == INVALID_HANDLE_VALUE) {
            throw(0);
        }
        GENERIC_MAPPING mapping;
        memset(&mapping, 0, sizeof(mapping));

        PRIVILEGE_SET privileges;
        DWORD         privileges_size = sizeof(privileges);
        BOOL          status;

        if ( !AccessCheck(sd, token, MAXIMUM_ALLOWED, &mapping,
                          &privileges, &privileges_size, permissions,
                          &status)  ||  !status ) {
            CNcbiError::SetFromWindowsError();
            throw(0);
        }
    }
    catch (int) {
        *permissions = 0;
        success = false;
    }
    // Clean up
    CloseHandle(token);
    LocalFree(sd);

    return success;
}


END_NCBI_SCOPE
