#ifndef CORELIB___NCBI_MASK__HPP
#define CORELIB___NCBI_MASK__HPP

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
 * Author:  Vladimir Ivanov
 *
 * File Description:  Classes to match a string against a set of masks.
 *
 */

/// @file ncbi_mask.hpp
/// Classes to match a string against a set of masks.

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
/// Abstract class. Base class for CMaskFileName, CMaskRegexp.
///
/// An empty mask object always corresponds to "all is included" case.
/// Throws exceptions on errors.
///

class CMask
{
public:
    /// Constructor
    CMask(void) { }
    /// Destructor
    virtual ~CMask(void) { }

    /// Add an inclusion mask
    void Add(const string& mask)             { m_Inclusions.push_back(mask); }
    /// Add an exclusion mask
    void AddExclusion(const string& mask)    { m_Exclusions.push_back(mask); }

    /// Remove an inclusion mask
    void Remove(const string& mask)          { m_Inclusions.remove(mask); }
    /// Remove an exclusion mask
    void RemoveExclusion(const string& mask) { m_Exclusions.remove(mask); }

    /// Match a string.
    ///
    /// @param str
    ///   String to match.
    /// @param use_case
    ///   Whether to do case-sensitive comparison (eCase, default), or
    ///   case-insensitive comparison (eNocase).
    /// @return
    ///   Return TRUE if the string 'str' matches one of the inclusion masks
    ///   and does not match any of the exclusion masks, or no masks at all
    ///   have been specified. Otherwise, return FALSE.
    virtual bool Match(CTempString str,
                       NStr::ECase use_case = NStr::eCase) const = 0;

protected:
    list<string>  m_Inclusions;   ///< List of inclusion masks
    list<string>  m_Exclusions;   ///< List of exclusion masks
};



//////////////////////////////////////////////////////////////////////////////
///
/// CMaskFileName --
///
/// Class to match file names against set of masks using wildcard characters.
///
/// An empty mask object always corresponds to "all is included" case.
/// Throws exceptions on errors.
///
class CMaskFileName : public CMask
{
public:
    /// Match a string.
    ///
    /// @param str
    ///   String to match.
    /// @param use_case
    ///   Whether to do case-sensitive comparison (eCase, default), or
    ///   case-insensitive comparison (eNocase).
    /// @return 
    ///   Return TRUE if the string 'str' matches one of the inclusion masks
    ///   and does not match any of the exclusion masks, or no masks at all
    ///   have been specified. Otherwise, return FALSE.
    /// @sa
    ///   NStr::MatchesMask
    bool Match(CTempString str, NStr::ECase use_case = NStr::eCase) const;
};

/* @} */


//////////////////////////////////////////////////////////////////////////////
//
// Inline
//

inline
bool CMaskFileName::Match(CTempString str, NStr::ECase use_case) const
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

#endif /* CORELIB___NCBI_MASK__HPP */
