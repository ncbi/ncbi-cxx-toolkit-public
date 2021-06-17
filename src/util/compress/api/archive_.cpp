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
 * Authors:  Vladimir Ivanov, Anton Lavrentiev
 *
 * File Description:
 *   Compression archive API - Common base classes and interfaces.
 *
 */

#include <ncbi_pch.hpp>
#include <util/compress/archive_.hpp>
#include <util/error_codes.hpp>


#if !defined(NCBI_OS_MSWIN)  &&  !defined(NCBI_OS_UNIX)
#  error "Class CArchive can be defined on MS-Windows and UNIX platforms only!"
#endif

#if defined(NCBI_OS_UNIX)
#  include <unistd.h>
#  include <sys/types.h>
#  ifdef NCBI_OS_DARWIN
// macOS supplies these as inline functions rather than macros.
#    define major major
#    define minor minor
#    define makedev makedev
#  endif
#  if defined(HAVE_SYS_SYSMACROS_H)
#    include <sys/sysmacros.h>
#  endif
#  if defined (NCBI_OS_IRIX)
#    include <sys/mkdev.h>
#    if !defined(major)  ||  !defined(minor)  ||  !defined(makedev)
#      error "Device macros undefined in this UNIX build!"
#    endif
#  endif
#endif //NCBI_OS


#define NCBI_USE_ERRCODE_X  Util_Compress


BEGIN_NCBI_SCOPE



/////////////////////////////////////////////////////////////////////////
//
// CArchiveEntryInfo
//

mode_t CArchiveEntryInfo::GetMode(void) const
{
    // Raw mode gets returned here (as kept in the info)
    return (m_Stat.st_mode & 07777);
}


void CArchiveEntryInfo::GetMode(CDirEntry::TMode*            usr_mode,
                                CDirEntry::TMode*            grp_mode,
                                CDirEntry::TMode*            oth_mode,
                                CDirEntry::TSpecialModeBits* special_bits) const
{
    CFile::ModeFromModeT(GetMode(), usr_mode, grp_mode, oth_mode, special_bits);
}


unsigned int CArchiveEntryInfo::GetMajor(void) const
{
#ifdef major
    if (m_Type == CDirEntry::eCharSpecial  ||
        m_Type == CDirEntry::eBlockSpecial) {
        return major(m_Stat.st_rdev);
    }
#else
    if (sizeof(int) >= 4  &&  sizeof(m_Stat.st_rdev) >= 4) {
        return (*((unsigned int*) &m_Stat.st_rdev) >> 16) & 0xFFFF;
    }
#endif //major
    return (unsigned int)(-1);
}


unsigned int CArchiveEntryInfo::GetMinor(void) const
{
#ifdef minor
    if (m_Type == CDirEntry::eCharSpecial  ||
        m_Type == CDirEntry::eBlockSpecial) {
        return minor(m_Stat.st_rdev);
    }
#else
    if (sizeof(int) >= 4  &&  sizeof(m_Stat.st_rdev) >= 4) {
        return *((unsigned int*) &m_Stat.st_rdev) & 0xFFFF;
    }
#endif //minor
    return (unsigned int)(-1);
}


bool CArchiveEntryInfo::operator == (const CArchiveEntryInfo& info) const
{
    return (m_Index      == info.m_Index                       &&
            m_Type       == info.m_Type                        &&
            m_Name       == info.m_Name                        &&
            m_LinkName   == info.m_LinkName                    &&
            m_UserName   == info.m_UserName                    &&
            m_GroupName  == info.m_GroupName                   &&
            memcmp(&m_Stat,&info.m_Stat, sizeof(m_Stat)) == 0);
}


static char s_TypeAsChar(CArchiveEntryInfo::EType type)
{
    switch (type) {
    case CDirEntry::eFile:
        return '-';
    case CDirEntry::eDir:
        return 'd';
    case CDirEntry::eLink:
        return 'l';
    case CDirEntry::ePipe:
        return 'p';
    case CDirEntry::eCharSpecial:
        return 'c';
    case CDirEntry::eBlockSpecial:
        return 'b';
    default:
        break;
    }
    return '?';
}


static string s_UserGroupAsString(const CArchiveEntryInfo& info)
{
    string user(info.GetUserName());
    if (user.empty()) {
        NStr::UIntToString(user, info.GetUserId());
    }
    string group(info.GetGroupName());
    if (group.empty()) {
        NStr::UIntToString(group, info.GetGroupId());
    }
    return user + '/' + group;
}


static string s_MajorMinor(unsigned int n)
{
    return n != (unsigned int)(-1) ? NStr::UIntToString(n) : "?";
}


static string s_SizeOrMajorMinor(const CArchiveEntryInfo& info)
{
    if (info.GetType() == CDirEntry::eCharSpecial  ||
        info.GetType() == CDirEntry::eBlockSpecial) {
        unsigned int major = info.GetMajor();
        unsigned int minor = info.GetMinor();
        return s_MajorMinor(major) + ',' + s_MajorMinor(minor);
    } else if (info.GetType() == CDirEntry::eDir ||
               info.GetType() == CDirEntry::eLink) {
        return string("-");
    }
    return NStr::UInt8ToString(info.GetSize());
}


ostream& operator << (ostream& os, const CArchiveEntryInfo& info)
{
    // Entry mode
    CDirEntry::TMode usr, grp, oth;
    CDirEntry::TSpecialModeBits special;
    CDirEntry::ModeFromModeT(info.GetMode(), &usr, &grp, &oth, &special);
    
    // Entry modification time
    string mtime;
    if (info.GetModificationTime()) {
        CTime t(info.GetModificationTime());
        mtime = t.ToLocalTime().AsString("Y-M-D h:m:s");
    }

    os << s_TypeAsChar(info.GetType())
       << CDirEntry::ModeToString(usr, grp, oth, special, CDirEntry::eModeFormat_List) << ' '
       << setw(17) << s_UserGroupAsString(info) << ' '
       << setw(10) << s_SizeOrMajorMinor(info)  << ' '
       << setw(19) << mtime << "  "
       << info.GetName();

    if (info.GetType() == CDirEntry::eLink) {
        os << " -> " << info.GetLinkName();
    }
    return os;
}


END_NCBI_SCOPE
