#ifndef CORELIB___NCBI_MASK__HPP
#define CORELIB___NCBI_MASK__HPP

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
 * File Description:  Classes to match string against set of masks.
 *
 */

/// @file ncbi_mask.hpp
/// Classes to match string against set of masks.

#include <corelib/ncbistd.hpp>


BEGIN_NCBI_SCOPE

/** @addtogroup Utility
 *
 * @{
 */

//////////////////////////////////////////////////////////////////////////////
///
/// CMask --
///
/// Abstract class. Basic class for CMaskFileName, CMaskRegexp.
///
/// The empty mask object always correspond to "all is included" case.
/// Throw exceptions on error.
///

class CMask
{
public:
    /// Constructor
    CMask(void) {};
    /// Destructor
    virtual ~CMask(void) {};

    /// Add inclusion mask
    void Add(const string& mask)             { m_Inclusions.push_back(mask); }
    /// Add exclusion mask
    void AddExclusion(const string& mask)    { m_Exclusions.push_back(mask); }

    /// Remove inclusion mask
    void Remove(const string& mask)          { m_Inclusions.remove(mask); }
    /// Remove exclusion mask
    void RemoveExclusion(const string& mask) { m_Exclusions.remove(mask); }

    /// Match string
    ///
    /// @param str
    ///   String to match.
    /// @param use_case
    ///   Whether to do a case sensitive compare(eCase -- default), or a
    ///   case-insensitive compare (eNocase).
    /// @return 
    ///   Return TRUE if string 'str' matches to one of inclusion masks
    ///   and not matches none of exclusion masks, or match masks are
    ///   not specified. Otherwise return FALSE.
    virtual bool Match(const string& str,
                       NStr::ECase use_case = NStr::eCase) const = 0;

protected:
    list<string>  m_Inclusions;   ///< Inclusion masks list
    list<string>  m_Exclusions;   ///< Exclusion masks list
};



//////////////////////////////////////////////////////////////////////////////
///
/// CMaskFileName --
///
/// Class to match file names against set of masks using wildcards characters.
///
/// The empty mask object always correspond to "all is included" case.
/// Throw exceptions on error.
///
class CMaskFileName : public CMask
{
public:
    /// Match string
    ///
    /// @param str
    ///   String to match.
    /// @param use_case
    ///   Whether to do a case sensitive compare(eCase -- default), or a
    ///   case-insensitive compare (eNocase).
    /// @return 
    ///   Return TRUE if string 'str' matches to one of inclusion masks
    ///   and not matches none of exclusion masks, or match masks are
    ///   not specified. Otherwise return FALSE.
    /// @sa
    ///   NStr::MatchesMask
    bool Match(const string& str, NStr::ECase use_case = NStr::eCase) const;
};

/* @} */


//////////////////////////////////////////////////////////////////////////////
//
// Inline
//

inline
bool CMaskFileName::Match(const string& str, NStr::ECase use_case) const
{
    bool found = m_Inclusions.empty();

    ITERATE(list<string>, it, m_Inclusions) {
        if ( NStr::MatchesMask(str, *it, use_case) ) {
            found = true;
            break;
        }                
    }
    if ( found ) {
        ITERATE(list<string>, it, m_Exclusions) {
            if ( NStr::MatchesMask(str, *it, use_case) ) {
                found = false;
                break;
            }                
        }
    }
    return found;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2005/03/16 15:29:29  ivanov
 * Match(): Added parameter for case sensitive/insensitive matching
 *
 * Revision 1.1  2005/01/31 11:42:25  ivanov
 * Initial revision
 *
 * ===========================================================================
 */

#endif /* CORELIB___NCBI_MASK__HPP */
