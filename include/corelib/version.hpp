#ifndef CORELIB___VERSION__HPP
#define CORELIB___VERSION__HPP

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
 * Authors:  Denis Vakatov, Vladimir Ivanov, Anatoliy Kuznetsov
 *
 *
 */

/// @file version.hpp
/// Define CVersionInfo, a version info storage class.


#include <corelib/ncbistd.hpp>


/** @addtogroup Version
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
// CVersionInfo


/////////////////////////////////////////////////////////////////////////////
///
/// CVersionInfo --
///
/// Define class for storing version information.

class NCBI_XNCBI_EXPORT CVersionInfo
{
public:
    /// Constructor.
    CVersionInfo(unsigned int  ver_major,
                 unsigned int  ver_minor,
                 unsigned int  patch_level = 0,
                 const string& name        = kEmptyStr);

    /// Constructor.
    CVersionInfo(const CVersionInfo& version);

    /// Destructor.
    virtual ~CVersionInfo() {}

    /// Print version information.
    ///
    /// Version information is printed in the following forms:
    /// - <ver_major>.<ver_minor>.<patch_level>
    /// - <ver_major>.<ver_minor>.<patch_level> (<name>)
    virtual string Print(void) const;

    /// Major version
    unsigned int GetMajor(void) const { return m_Major; }
    /// Minor version
    unsigned int GetMinor(void) const { return m_Minor; }
    /// Patch level
    unsigned int GetPatchLevel(void) const { return m_PatchLevel; }

    const string& GetName(void) const { return m_Name; }

    /// Patch level comparison mode
    /// @sa Match
    enum EPatchLevelCompare 
    {
        eAnyLevel,
        eNewerLevel,
        eExactLevel
    };

    /// Check if version matches another version.
    /// @param version_info
    ///   Version Info to compare with
    /// @param pl_mode
    ///   Patch level comparison mode
    bool Match(const CVersionInfo& version_info, 
               EPatchLevelCompare  pl_mode) const;

protected:
    unsigned  int    m_Major;       ///< Major number
    unsigned  int    m_Minor;       ///< Minor number
    unsigned  int    m_PatchLevel;  ///< Patch level
    const     string m_Name;        ///< Name
};


/// Algorithm function to find version in the container
///
/// Scans the provided iterator for version with the same major and
/// minor version and the newest patch level.
///
/// @param first
///    first iterator to start search 
/// @param last
///    ending iterator (typically returned by end() fnction of an STL
///    container)
/// @return 
///    iterator on the best version or last
template<class It>
It FindVersion(It first, It last, const CVersionInfo& info)
{
    It best_version = last;  // not found by default
    unsigned int max_patch_level = 0;

    for (;first != last; ++first) {
        const CVersionInfo& vinfo = *first;
        if (info.Match(vinfo, CVersionInfo::eNewerLevel)) {
            // Candidate?
            if (vinfo.GetPatchLevel() > max_patch_level) {
                best_version = first;
                max_patch_level = vinfo.GetPatchLevel();
            }
        }
    
    } // for
    
    return best_version; 
}

/* @} */


END_NCBI_SCOPE




/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2003/10/30 16:38:08  kuznets
 * CVersionInfo changed:
 *  - added accessors for major, minor and patch level
 *  - Match function to compare versions
 * Added FindVersion algorithm to scan containers for the best version
 *
 * Revision 1.4  2003/09/08 12:17:42  siyan
 * Documentation changes.
 *
 * Revision 1.3  2003/04/01 19:19:52  siyan
 * Added doxygen support
 *
 * Revision 1.2  2002/12/26 19:18:07  dicuccio
 * Added empty virtual destructor
 *
 * Revision 1.1  2002/12/26 17:11:10  ivanov
 * Initial revision
 *
 * ===========================================================================
 */

#endif // CORELIB___VERSION__HPP



