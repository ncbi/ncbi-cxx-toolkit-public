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


// Hopefully this makes UID/GID compatible with what CYGWIN reports
#define CYGWIN_MAGIC_ID_OFFSET  10000


BEGIN_NCBI_SCOPE


string CWinSecurity::GetUserName(void)
{
    TXChar  name[UNLEN + 1];
    DWORD name_size = UNLEN + 1;

    if ( !::GetUserName(name, &name_size) ) {
        CNcbiError::SetFromWindowsError();
        return kEmptyStr;
    }
    return _T_STDSTRING(name);
}


PSID CWinSecurity::GetUserSID(const string& username)
{
    string t_username = username.empty() ? GetUserName() : username;
    if ( t_username.empty() ) {
        return NULL;
    }
    TXString x_username( _T_XSTRING(t_username) );

    PSID         sid         = NULL;
    DWORD        sid_size    = 0;
    TXChar*      domain      = NULL;
    DWORD        domain_size = 0;
    SID_NAME_USE use;

    try {
        // First call to LookupAccountName to get the buffer sizes
        BOOL ret = LookupAccountName(NULL, x_username.c_str(),
                                     sid,    &sid_size,
                                     domain, &domain_size, &use);
        if ( !ret  &&  GetLastError() != ERROR_INSUFFICIENT_BUFFER ) {
            CNcbiError::SetFromWindowsError();
            return NULL;
        }
        // Allocate buffers
        sid    = (PSID) LocalAlloc(LMEM_FIXED, sid_size);
        domain = (TXChar*) malloc(domain_size * sizeof(TXChar));
        if ( !sid  ||  !domain ) {
            throw(0);
        }
        // Second call to LookupAccountName to get the actual account info
        ret = LookupAccountName(NULL, x_username.c_str(),
                                sid,    &sid_size,
                                domain, &domain_size, &use);
        if ( !ret ) {
            // Unknown local user
            throw(0);
        }
    }
    catch (int) {
        if ( sid ) {
            LocalFree(sid);
            sid = NULL;
        }
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


// Helper function for SetOwner
static bool s_EnablePrivilege(HANDLE token, LPCTSTR priv, BOOL enable = TRUE)
{
    // Get priviledge unique identifier
    LUID luid;
    if ( !LookupPrivilegeValue(NULL, priv, &luid) ) {
        CNcbiError::SetFromWindowsError();
        return false;
    }

    // Get current privilege setting

    TOKEN_PRIVILEGES tp;
    TOKEN_PRIVILEGES tp_prev;
    DWORD            tp_prev_size = sizeof(tp_prev);

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = 0;

    AdjustTokenPrivileges(token, FALSE,
                          &tp, sizeof(tp),
                          &tp_prev, &tp_prev_size);
    if ( GetLastError() != ERROR_SUCCESS ) {
        CNcbiError::SetFromWindowsError();
        return false;
    }

    // Set privilege based on received setting

    tp_prev.PrivilegeCount = 1;
    tp_prev.Privileges[0].Luid = luid;

    if ( enable ) {
        tp_prev.Privileges[0].Attributes |=  SE_PRIVILEGE_ENABLED;
    } else {
        tp_prev.Privileges[0].Attributes &= ~SE_PRIVILEGE_ENABLED;
    }
    AdjustTokenPrivileges(token, FALSE, &tp_prev, tp_prev_size, NULL, NULL);
    if ( GetLastError() != ERROR_SUCCESS ) {
        CNcbiError::SetFromWindowsError();
        return false;
    }

    // Privilege settings changed
    return true;
}


// Helper function for GetOwnerGroupFromSIDs().
// NB:  *domatch is reset to 0 if the account type has no domain match.
static bool x_LookupAccountSid(PSID sid, string* account, int* domatch = 0)
{
    // According to MSDN max account name size is 20, domain name size is 256.
    #define MAX_ACCOUNT_LEN  256

    TXChar       account_name[MAX_ACCOUNT_LEN + 1];
    TXChar       domain_name [MAX_ACCOUNT_LEN + 1];
    DWORD        account_size = sizeof(account_name) / sizeof(account_name[0]);
    DWORD        domain_size  = sizeof(domain_name)  / sizeof(domain_name[0]);
    SID_NAME_USE use;

    if ( !LookupAccountSid(NULL, sid,
                           account_name, &account_size,
                           domain_name,  &domain_size,
                           &use) ) {
        CNcbiError::SetFromWindowsError();
        return false;
    }
    // Save account information
    if (account) {
        account_name[account_size] = _TX('\0');
        account->assign(_T_STDSTRING(account_name));
    }
    if (domatch) {
        domain_name[domain_size] = _TX('\0');
        string domain(_T_STDSTRING(domain_name));
        if (*domatch != int(use)  ||  domain.empty()
            ||  NStr::EqualNocase(domain, "builtin")
            ||  NStr::FindNoCase(domain, " ") != NPOS) {
            *domatch = 0;
        }
    }
    return true;
}


static bool s_GetOwnerGroupFromSIDs(PSID sid_owner, PSID sid_group,
                                    string* owner, string* group,
                                    unsigned int* uid, unsigned int* gid)
{
    bool success = true;

    // Get numeric owner
    if ( uid ) {
        int match = SidTypeUser;
        if ( !x_LookupAccountSid(sid_owner, owner, &match) ) {
            if ( owner )
                success = false;
            *uid = 0;
        } else {
            *uid = match ? CYGWIN_MAGIC_ID_OFFSET : 0;
        }
        owner = 0;
        *uid += *GetSidSubAuthority(sid_owner,
                                    *GetSidSubAuthorityCount(sid_owner) - 1);
    }
    // Get numeric group
    if ( gid ) {
        int match = SidTypeGroup;
        if ( !x_LookupAccountSid(sid_group, group, &match) ) {
            *gid = 0;
        } else {
            *gid = match ? CYGWIN_MAGIC_ID_OFFSET : 0;
        }
        group = 0;
        *gid += *GetSidSubAuthority(sid_group,
                                    *GetSidSubAuthorityCount(sid_group) - 1);
    }
    if ( !success ) {
        return false;
    }

    // Get owner
    if ( owner  &&  !x_LookupAccountSid(sid_owner, owner) ) {
        return false;
    }
    // Get group
    if ( group  &&  !x_LookupAccountSid(sid_group, group) ) {
        // This is not an error, because the group name on Windows
        // is an auxiliary information.  Sometimes accounts cannot
        // belong to groups, or we don't have permissions to get
        // such information.
        group->clear();
    }
	return true;
}


bool CWinSecurity::GetObjectOwner(HANDLE         objhndl,
                                  SE_OBJECT_TYPE objtype,
                                  string* owner, string* group,
                                  unsigned int* uid, unsigned int* gid)
{
    PSID sid_owner;
    PSID sid_group;
    PSECURITY_DESCRIPTOR sd;

    DWORD res = GetSecurityInfo
        ( objhndl, objtype,
          OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION,
          &sid_owner, &sid_group, NULL, NULL, &sd );

    if ( res != ERROR_SUCCESS ) {
        CNcbiError::SetWindowsError(res);
        return false;
    }

    bool retval = s_GetOwnerGroupFromSIDs(sid_owner, sid_group,
                                          owner, group, uid, gid);
    LocalFree(sd);
    return retval;
}


bool CWinSecurity::GetObjectOwner(const string&  objname,
                                  SE_OBJECT_TYPE objtype,
                                  string* owner, string* group,
                                  unsigned int* uid, unsigned int* gid)
{
    PSID sid_owner;
    PSID sid_group;
    PSECURITY_DESCRIPTOR sd;

    DWORD res = GetNamedSecurityInfo
        ( _T_XCSTRING(objname), objtype,
          OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION,
          &sid_owner, &sid_group, NULL, NULL, &sd );

    if ( res != ERROR_SUCCESS ) {
        CNcbiError::SetWindowsError(res);
        return false;
    }

    bool retval = s_GetOwnerGroupFromSIDs(sid_owner, sid_group,
                                          owner, group, uid, gid);
    LocalFree(sd);
    return retval;
}


bool CWinSecurity::SetFileOwner(const string& filename,
                                const string& owner, unsigned int* uid)
{
    if ( uid ) {
        *uid = 0;
    }

    _ASSERT( !owner.empty() );

    // Get access token
    HANDLE token;
    if ( !OpenProcessToken(GetCurrentProcess(),
                           TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                           &token) ) {
        CNcbiError::SetFromWindowsError();
        return false;
    }

    // Enable privileges.  If failed, then try without them
    s_EnablePrivilege(token, SE_TAKE_OWNERSHIP_NAME);
    s_EnablePrivilege(token, SE_RESTORE_NAME);
    s_EnablePrivilege(token, SE_BACKUP_NAME);

    PSID    sid     = NULL;
    TXChar* domain  = NULL;
    bool    success = true;

    try {

        //
        // Get SID for new owner
        //

        // First call to LookupAccountName() to get the buffer sizes
        DWORD        sid_size    = 0;
        DWORD        domain_size = 0;
        SID_NAME_USE use;
        BOOL res = LookupAccountName(NULL, _T_XCSTRING(owner),
                                     NULL, &sid_size, 
                                     NULL, &domain_size, &use);
        if ( !res  &&  GetLastError() != ERROR_INSUFFICIENT_BUFFER ) {
            throw(0);
        }
        // Allocate buffers
        sid = (PSID) LocalAlloc(LMEM_FIXED, sid_size);
        domain = (TXChar*) malloc(domain_size * sizeof(TXChar));
        if ( !sid  ||  !domain ) {
            throw(0);
        }

        // Second call to LookupAccountName to get the actual account info
        if ( !LookupAccountName(NULL, _T_XCSTRING(owner),
                                sid,    &sid_size, 
                                domain, &domain_size, &use) ) {
            // Unknown local user
            throw(0);
        }
        s_GetOwnerGroupFromSIDs(sid, NULL, NULL, NULL, uid, NULL);

        //
        // Change owner
        //

        // Security descriptor (absolute format)
        UCHAR sd_abs_buf[SECURITY_DESCRIPTOR_MIN_LENGTH];
        PSECURITY_DESCRIPTOR sd = (PSECURITY_DESCRIPTOR) &sd_abs_buf;

        // Build security descriptor in absolute format
        if ( !InitializeSecurityDescriptor
             (sd, SECURITY_DESCRIPTOR_REVISION) ) {
            throw(0);
        }
        // Modify security descriptor owner.
        // FALSE - because new owner was explicitly specified.
        if ( !SetSecurityDescriptorOwner(sd, sid, FALSE) ) {
            throw(0);
        }
        // Check security descriptor
        if ( !IsValidSecurityDescriptor(sd) ) {
            throw(0);
        }
        // Set new security information for the file object
        if ( !SetFileSecurity
             (_T_XCSTRING(filename), OWNER_SECURITY_INFORMATION, sd) ) {
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

    if ( !success ) {
        CNcbiError::SetFromWindowsError();
    }
    return success;
}


#define FILE_SECURITY_INFO  (OWNER_SECURITY_INFORMATION | \
                             GROUP_SECURITY_INFORMATION | \
                             DACL_SECURITY_INFORMATION)


PSECURITY_DESCRIPTOR CWinSecurity::GetFileSD(const string& path)
{
    if ( path.empty() ) {
        CNcbiError::Set(CNcbiError::eInvalidArgument);
        return NULL;
    }
    PSECURITY_DESCRIPTOR sid = NULL;
    DWORD size               = 0;
    DWORD size_need          = 0;

    if ( !GetFileSecurity(_T_XCSTRING(path), FILE_SECURITY_INFO,
                          sid, size, &size_need) ) {
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            CNcbiError::SetFromWindowsError();
            return NULL;
        }
        // Allocate memory for the buffer
        sid = (PSECURITY_DESCRIPTOR) LocalAlloc(LMEM_FIXED, size_need);
        if ( !sid ) {
            CNcbiError::SetFromWindowsError();
            return NULL;
        }
        size = size_need;
        if ( !GetFileSecurity(_T_XCSTRING(path), FILE_SECURITY_INFO,
                              sid, size, &size_need) ) {
            CNcbiError::SetFromWindowsError();
            LocalFree((HLOCAL) sid);
            return NULL;
        }
    }
    return sid;
}


bool CWinSecurity::GetFileDACL(const string& strPath,
                               PSECURITY_DESCRIPTOR* pFileSD, PACL* pDACL)
{
    if ( strPath.empty() ) {
        CNcbiError::Set(CNcbiError::eInvalidArgument);
        return false;
    }
    DWORD dwRet = GetNamedSecurityInfo(_T_XCSTRING(strPath),
                                       SE_FILE_OBJECT, FILE_SECURITY_INFO,
                                       NULL, NULL, pDACL, NULL, pFileSD);
    if ( dwRet != ERROR_SUCCESS ) {
        pFileSD = NULL;
        pDACL   = NULL;
        CNcbiError::SetWindowsError(dwRet);
        return false;
    }
    return true;
}


void CWinSecurity::FreeFileSD(PSECURITY_DESCRIPTOR sd)
{
    if ( sd ) {
        LocalFree(sd);
    }
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
    // Get DACL for the file
    PSECURITY_DESCRIPTOR sd = GetFileSD(path);

    if ( !sd ) {
        if ( GetLastError() == ERROR_ACCESS_DENIED ) {
            CNcbiError::SetWindowsError(ERROR_ACCESS_DENIED);
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
            RevertToSelf();
            throw(0);
        }
        RevertToSelf();

        GENERIC_MAPPING mapping;
        memset(&mapping, 0, sizeof(mapping));

        PRIVILEGE_SET privileges;
        DWORD         privileges_size = sizeof(privileges);
        BOOL          status;

        if ( !AccessCheck(sd, token, MAXIMUM_ALLOWED, &mapping,
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

    if ( !success ) {
        CNcbiError::SetFromWindowsError();
    }
    return success;
}


END_NCBI_SCOPE
