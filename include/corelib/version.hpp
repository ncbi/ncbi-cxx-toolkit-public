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
 * Authors:  Denis Vakatov, Vladimir Ivanov
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

protected:
    unsigned     int m_Major;       ///< Major number
    unsigned     int m_Minor;       ///< Minor number
    unsigned     int m_PatchLevel;  ///< Patch level
    const string m_Name;            ///< Name
};



END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
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



