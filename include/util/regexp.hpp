#ifndef REGEXP__HPP
#define REGEXP__HPP

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
 * Authors:  Clifford Clausen...
 *
 */

/// @file regexp.hpp
/// C++ wrapper for the PCRE library
///
/// Class definition for class CRegexp which is a wrapper class for the
/// Perl-compatible regular expression (PCRE) library written in C.

#include <corelib/ncbistd.hpp>
#include <util/regexp/pcre.h>

BEGIN_NCBI_SCOPE

/// Specifies the maximum number of sub-patterns that can be found
const size_t kRegexpMaxSubPatterns = 100;

/// This class is a C++ wrapper for the Perl-compatible regular expression
/// library written in C. Internally, it holds a compiled regular expression
/// used for matching with char* strings passed as an argument to the
/// GetMatch member function. The regular expression is passed as a char*
/// argument to the constructor or to the Set member function.
class NCBI_XUTIL_EXPORT CRegexp
{
public:

    /// Type definitions used for code clarity.
    typedef int TCompile;
    typedef int TMatch;

    /// Flags for constructor and Set

    /// PCRE compiler flags usde in the constructor and in Set. If
    /// eCompile_ignore_case is set, matches are case insensitive. If
    /// eCompile_newline is set then ^ matches the start of a line and
    /// $ matches the end of a line. If not set, ^ matches only the start of
    /// the entire string and $ matches only the end of the entire string.
    enum ECompile {
        eCompile_default     = 0,
        eCompile_ignore_case = PCRE_CASELESS,
        eCompile_newline     = PCRE_MULTILINE,
        eCompile_both        = PCRE_MULTILINE | PCRE_CASELESS
    };

    /// Constructor.

    /// Sets and compiles the PCRE pattern specified by pat
    /// according to compile options. See ECompile above. Also
    /// allocates memory for compiled PCRE.
    CRegexp(const string &pat,  // Perl regular expression
            TCompile flags = 0);     // Flags

    /// Destructor.

    /// Deallocates compiled Perl-compatible regular expression.
    virtual ~CRegexp();

    /// Sets and compiles PCRE

    /// Sets and compiles the PCRE pattern specified by pat
    /// according to compile options. See ECompile above. Also
    /// deallocates/allocates memory for compiled PCRE.
    void Set(const string &pat, // Perl regular expression
             TCompile flags = 0);    // Options

    /// GetMatch flags

    /// Setting eMatch_not_begin causes ^ not to match before the
    /// first character of a line. Without setting eCompile_newline,
    /// ^ won't match anything if eMatch_not_begin is set. Setting
    /// eMatch_not_end causes $ not to match immediately before a new
    /// line. Without setting eCompile_newline, $ won't match anything
    /// if eMatch_not_end is set.
    enum EMatch {
        eMatch_default   = 0,
        eMatch_not_begin = PCRE_NOTBOL,           // ^ won't match string begin
        eMatch_not_end   = PCRE_NOTEOL,           // $ won't match string end
        eMatch_not_both  = PCRE_NOTBOL | PCRE_NOTEOL
    };

    /// Get matching pattern and sub-patterns.

    /// Returns a string corresponding to the match to pattern or sub-pattern.
    /// Use idx = 0 for complete pattern. Use idx > 0 for sub-patterns.
    /// Returns empty string when no match found or if noreturn is true.
    /// Set noreturn to true when GetSub or GetResults will be used
    /// to retrieve pattern and sub-patterns. Calling GetMatch causes
    /// the entire search to be performed again. If you want to retrieve
    /// a different pattern/sub-pattern from an already performed search,
    /// it is more efficient to use GetSub or GetResults.
    string GetMatch (const char *str,             // String to search
                     TSeqPos offset = 0,          // Starting offset in str
                     size_t idx = 0,              // (sub) match to return
                     TMatch flags = 0,            // Options
                     bool noreturn = false);      // Returns kEmptyStr if true

    /// Get pattern/sub-pattern from previous GetMatch

    /// Should only be called after GetMatch has been called with the
    /// same str. GetMatch internally stores locations on str where
    /// pattern and sub patterns were found. This function will return
    /// the substring at location of pattern match (idx 0) or sub pattern
    /// match (idx > 0). Returns empty string when no match.
    string GetSub (const char *str,               // String to search
                   size_t idx = 0) const;         // (sub) match to return

    /// Get number of patterns + sub-patterns.

    /// Returns the number of patterns + sub-patterns found as a result
    /// of the most recent GetMatch call.
    int NumFound() const;

    /// Get location of pattern/sub-pattern.

    /// Returns array where index 0 is location of first character in
    /// pattern/sub pattern and index 1 is 1 beyond last character in
    /// pattern/sub pattern. Throws if called with idx >= NumFound().
    /// Use idx == 0 for pattern, idx > 0 for sub patterns.
    const int* GetResults(size_t idx) const;

private:
    // Disable copy constructor and assignment operator
    CRegexp(const CRegexp &);
    void operator= (const CRegexp &);

    /// Pointer to compiled PCRE
    pcre  *m_PReg;                                  // Compiled pattern

    /// Array of locations of patterns/sub-patterns resulting from
    /// the last call to GetMatch. Also contains 1/3 extra space used
    /// internally by the PCRE C library.
    int m_Results[(kRegexpMaxSubPatterns +1) * 3];  // Results of Match

    /// Holds the total number of pattern + sub-patterns resulting
    /// from the last call to GetMatch.
    int m_NumFound;                                 // #(sub) patterns
};


END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  2003/07/16 19:15:05  clausen
* Added TCompile and TMatch and fixed comments
*
* Revision 1.3  2003/07/07 13:50:59  kuznets
* Added DLL export/import instruction
*
* Revision 1.2  2003/06/20 18:32:42  clausen
* Changed to native interface for regexp
*
* Revision 1.1  2003/06/03 14:47:46  clausen
* Initial version
*
*
* ===========================================================================
*/

#endif  /* REGEXP__HPP */
