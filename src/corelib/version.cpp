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
 * Authors:  Denis Vakatov, Vladimir Ivanov
 *
 * File Description:
 *   CVersionInfo -- a version info storage class
 *
 */

#include <corelib/version.hpp>


BEGIN_NCBI_SCOPE


CVersionInfo::CVersionInfo(unsigned int ver_major, unsigned int  ver_minor,
                           unsigned int  patch_level, const string& name) 
    : m_Major(ver_major),
      m_Minor(ver_minor),
      m_PatchLevel(patch_level),
      m_Name(name)

{
}


CVersionInfo::CVersionInfo(const CVersionInfo& version)
    : m_Major(version.m_Major),
      m_Minor(version.m_Minor),
      m_PatchLevel(version.m_PatchLevel),
      m_Name(version.m_Name)
{
}


string CVersionInfo::Print(void) const
{
    CNcbiOstrstream os;
    os << m_Major << "." << m_Minor << "." << m_PatchLevel;
    if ( !m_Name.empty() ) {
        os << " (" << m_Name << ")";
    }
    os << '\0';
    return CNcbiOstrstreamToString(os);
}

bool CVersionInfo::Match(const CVersionInfo& version_info, 
                         EPatchLevelCompare  pl_mode) const
{
    if (GetMajor() != version_info.GetMajor())
        return false;

    if (GetMinor() != version_info.GetMinor())
        return false;

    switch (pl_mode)
    {
    case eAnyLevel:
        return true;
    case eNewerLevel:
        return GetPatchLevel() >= version_info.GetPatchLevel();
    case eExactLevel:
        return GetPatchLevel() == version_info.GetPatchLevel();
    default:
        _ASSERT(0);
    }
    return false;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2003/10/30 16:38:50  kuznets
 * Implemented CVersionInfo::Match
 *
 * Revision 1.2  2003/01/13 20:42:50  gouriano
 * corrected the problem with ostrstream::str(): replaced such calls with
 * CNcbiOstrstreamToString(os)
 *
 * Revision 1.1  2002/12/26 17:10:47  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
