#ifndef UTIL___REGEXP__HPP
#define UTIL___REGEXP__HPP

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
 * Authors:  Clifford Clausen
 *
 */

/// @file regexp.hpp
/// C++ wrappers for the Perl-compatible regular expression (PCRE) library.
///
/// CRegexp     -  wrapper class for the PCRE library.
/// CRegexpUtil -  utility functions.
///
/// For more details see PCRE documentation: http://www.pcre.org/pcre.txt

#include <corelib/ncbistd.hpp>

/** @addtogroup Regexp
 *
 * @{
 */

BEGIN_NCBI_SCOPE


/// Specifies the maximum number of subpatterns that can be found.
const size_t kRegexpMaxSubPatterns = 100;


/////////////////////////////////////////////////////////////////////////////
///
/// CRegexp --
///
/// Define a wrapper class for the Perl-compatible regular expression (PCRE)
/// library.
///
/// Internally, this class holds a compiled regular expression used for
/// matching with strings passed as an argument to the GetMatch()
/// member function. The regular expression is passed as a argument
/// to the constructor or to the Set() member function.
///
/// Throw exception on error.

class NCBI_XREGEXP_EXPORT CRegexp
{
public:
    /// Type definitions used for code clarity.
    typedef int TCompile;     ///< Compilation options.
    typedef int TMatch;       ///< Match options.

    /// Flags for compile regular expressions.
    ///
    /// PCRE compiler flags used in the constructor and in Set().
    /// If fCompile_ignore_case is set, matches are case insensitive.
    /// If fCompile_dotall is set, a dot metacharater in the pattern matches
    /// all characters, including newlines. Without it, newlines are excluded.
    /// If fCompile_newline is set then ^ matches the start of a line and
    /// $ matches the end of a line. If not set, ^ matches only the start
    /// of the entire string and $ matches only the end of the entire string.
    /// If fCompile_ungreedy inverts the "greediness" of the quantifiers so
    /// that they are not greedy by default, but become greedy if followed by
    /// "?".
    /// It is not compatible with Perl. 
    ///
    /// The settings can be changed from within the pattern by a sequence of
    /// Perl option letters enclosed between "(?" and ")".
    /// The option letters are:
    ///   i  for PCRE_CASELESS
    ///   m  for PCRE_MULTILINE
    ///   s  for PCRE_DOTALL
    ///   x  for PCRE_EXTENDED
    ///   U  for PCRE_UNGREEDY
    enum ECompile {
        fCompile_default     = 0x80000000,
        fCompile_ignore_case = 0x80000001,
        fCompile_dotall      = 0x80000002,
        fCompile_newline     = 0x80000004,
        fCompile_ungreedy    = 0x80000008,
        fCompile_extended    = 0x80000010
    };
    // Deprecated flags
    enum ECompile_deprecated {
        eCompile_default     = fCompile_default,
        eCompile_ignore_case = fCompile_ignore_case,
        eCompile_dotall      = fCompile_dotall,
        eCompile_newline     = fCompile_newline,
        eCompile_ungreedy    = fCompile_ungreedy
    };

    /// Flags for match string against a pre-compiled pattern.
    ///
    /// Setting fMatch_not_begin causes ^ not to match before the
    /// first character of a line. Without setting fCompile_newline,
    /// ^ won't match anything if fMatch_not_begin is set.
    /// Setting fMatch_not_end causes $ not to match immediately before a new
    /// line. Without setting fCompile_newline, $ won't match anything
    /// if fMatch_not_end is set.
    enum EMatch {
        fMatch_default       = 0x80000000,
        fMatch_not_begin     = 0x80000001,   ///< ^ won't match string begin.
        fMatch_not_end       = 0x80000002,   ///< $ won't match string end.
        fMatch_not_both      = fMatch_not_begin | fMatch_not_end
    };
    // Deprecated flags
    enum EMatch_deprecated {
        eMatch_default       = fMatch_default,
        eMatch_not_begin     = fMatch_not_begin,
        eMatch_not_end       = fMatch_not_end,
        eMatch_not_both      = fMatch_not_both
    };

    /// Constructor.
    ///
    /// Set and compile the PCRE pattern specified by argument according
    /// to compile options. Also allocate memory for compiled PCRE.
    /// @param pattern
    ///   Perl regular expression to compile.
    /// @param flags
    ///   Regular expression compilation flags.
    /// @sa
    ///   ECompile
    CRegexp(const string& pattern, TCompile flags = fCompile_default);

    /// Destructor.
    ///
    /// Deallocate compiled Perl-compatible regular expression.
    virtual ~CRegexp();

    /// Set and compile PCRE.
    ///
    /// Set and compile the PCRE pattern specified by argument according
    /// to compile options. Also deallocate/allocate memory for compiled PCRE.
    /// @param pattern
    ///   Perl regular expression to compile.
    /// @param flags
    ///   Regular expression compilation flags.
    /// @sa
    ///   ECompile
    void Set(const string& pattern, TCompile flags = fCompile_default);

    /// Get matching pattern and subpatterns.
    ///
    /// Return a string corresponding to the match to pattern or subpattern.
    /// Set noreturn to true when GetSub() or GetResults() will be used
    /// to retrieve pattern and subpatterns. Calling GetMatch() causes
    /// the entire search to be performed again. If you want to retrieve
    /// a different pattern/subpattern from an already performed search,
    /// it is more efficient to use GetSub or GetResults.
    /// @param str
    ///   String to search.
    /// @param offset
    ///   Starting offset in str.
    /// @param idx
    ///   (Sub) match to return.
    ///   Use idx = 0 for complete pattern. Use idx > 0 for subpatterns.
    /// @param flags
    ///   Flags to match.
    /// @param noreturn
    ///   Return empty string if noreturn is true.
    /// @return
    ///   Return (sub) match with number idx or empty string when no match
    ///   found or if noreturn is true.
    /// @sa
    ///   EMatch, GetSub(), GetResult()
    string GetMatch(
        const string& str,
        size_t        offset   = 0,
        size_t        idx      = 0,
        TMatch        flags    = fMatch_default,
        bool          noreturn = false
    );

    /// Get pattern/subpattern from previous GetMatch().
    ///
    /// Should only be called after GetMatch() has been called with the
    /// same string. GetMatch() internally stores locations on string where
    /// pattern and subpatterns were found. 
    /// @param str
    ///   String to search.
    /// @param idx
    ///   (Sub) match to return.
    /// @return
    ///   Return the substring at location of pattern match (idx 0) or
    ///   subpattern match (idx > 0). Return empty string when no match.
    /// @sa
    ///   GetMatch(), GetResult()
    string GetSub(const string& str, size_t idx = 0) const;

    /// Get number of patterns + subpatterns.
    ///
    /// @return
    ///   Return the number of patterns + subpatterns found as a result
    ///   of the most recent GetMatch() call.
    /// @sa
    ///   GetMatch()
    int NumFound() const;

    /// Get location of pattern/subpattern.
    ///
    /// @param idx
    ///   Index of pattern/subpattern to obtaining.
    ///   Use idx = 0 for pattern, idx > 0 for sub patterns.
    /// @return
    ///   Return array where index 0 is location of first character in
    ///   pattern/sub pattern and index 1 is 1 beyond last character in
    ///   pattern/sub pattern.
    ///   Throws if called with idx >= NumFound().
    /// @sa
    ///   GetMatch(), NumFound()
    const int* GetResults(size_t idx) const;

private:
    // Disable copy constructor and assignment operator.
    CRegexp(const CRegexp &);
    void operator= (const CRegexp &);

    /// Pointer to compiled PCRE pattern.
    void*  m_PReg;

    /// Array of locations of patterns/subpatterns resulting from
    /// the last call to GetMatch(). Also contains 1/3 extra space used
    /// internally by the PCRE C library.
    int m_Results[(kRegexpMaxSubPatterns +1) * 3];

    /// The total number of pattern + subpatterns resulting from
    /// the last call to GetMatch.
    int m_NumFound;
};



/////////////////////////////////////////////////////////////////////////////
///
/// CRegexpUtil --
///
/// Throw exception on error.

class NCBI_XREGEXP_EXPORT CRegexpUtil
{
public:
    /// Constructor.
    ///
    /// Set string for processing.
    /// @param str
    ///   String to process.
    /// @sa
    ///   Exists(), Extract(), Replace(), ReplaceRange()
    CRegexpUtil(const string& str = kEmptyStr);

    /// Reset the content of the string to process.
    ///
    /// @param str
    ///   String to process.
    /// @sa
    ///   operator =
    void Reset(const string& str);

    /// Reset the content of the string to process.
    ///
    /// The same as Reset().
    /// @param str
    ///   String to process.
    /// @sa
    ///   Reset()
    void operator= (const string& str);

    /// Get result string.
    ///
    /// @sa
    ///   operator string
    string GetResult(void);

    /// Get result string.
    ///
    /// The same as GetResult().
    /// @sa
    ///   GetResult()
    operator string(void);
 
    /// Check existence substring which match a specified pattern.
    ///
    /// @param pattern
    ///   Perl regular expression to search.
    /// @param compile_flags
    ///   Regular expression compilation flags.
    /// @param match_flags
    ///   Flags to match.
    /// @return
    ///   Return TRUE if  a string corresponding to the match to pattern or
    ///   subpattern.
    /// @sa
    ///   CRegexp, CRegexp::GetMatch()
    bool Exists(
        const string&      pattern,
        CRegexp::TCompile  compile_flags = CRegexp::fCompile_default,
        CRegexp::TMatch    match_flags   = CRegexp::fMatch_default
    );

    /// Get matching pattern/subpattern from string.
    ///
    /// @param pattern
    ///   Perl regular expression to search.
    /// @param compile_flags
    ///   Regular expression compilation flags.
    /// @param match_flags
    ///   Flags to match.
    /// @param pattern_idx
    ///   Index of pattern/subpattern to extract.
    ///   Use pattern_idx = 0 for pattern, pattern_idx > 0 for sub patterns.
    /// @return
    ///   Return the substring at location of pattern/subpatter match with
    ///   index pattern_idx. Return empty string when no match.
    /// @sa
    ///   CRegexp, CRegexp::GetMatch()
    string Extract(
        const string&      pattern,
        CRegexp::TCompile  compile_flags = CRegexp::fCompile_default,
        CRegexp::TMatch    match_flags   = CRegexp::fMatch_default,
        size_t             pattern_idx   = 0
    );

    /// Replace occurrences of a substring within a string by pattern.
    ///
    /// @param search
    ///   Reqular expression to match a substring value that is replaced.
    /// @param replace
    ///   Replace "search" substring with this value. The matched subpatterns
    ///   (if any) can be found and inserted into replace string using
    ///   variables $1, $2, $3, and so forth. The variable can be enclosed
    ///   in the curly brackets {}, that will be deleted on substitution.
    /// @param compile_flags
    ///   Regular expression compilation flags.
    /// @param match_flags
    ///   Flags to match.
    /// @param max_replace
    ///   Replace no more than "max_replace" occurrences of substring "search".
    ///   If "max_replace" is zero (default), then replace all occurrences with
    ///   "replace".
    /// @return
    ///   Return the count of replacements.
    /// @sa
    ///   CRegexp, ReplaceRange()
    size_t Replace(
        const string&      search,
        const string&      replace,
        CRegexp::TCompile  compile_flags = CRegexp::fCompile_default,
        CRegexp::TMatch    match_flags   = CRegexp::fMatch_default,
        size_t             max_replace   = 0
    );


    //
    // Range functions.
    //


    /// Range processing type.
    /// Defines which part of the specified range should be processed.
    enum ERange {
        eInside,    ///< Process substrings inside range.
        eOutside    ///< Process substrings outside range.
    };

    /// Set new range for range-dependent functions.
    ///
    /// The mached string will be splitted up by "delimeter".
    /// And then in range-dependent functions every part (substring) is checked
    /// to fall into the range, specified by start and end adresses.
    ///
    /// The addresses works similare the Unix utility SED, except that regular
    /// expressions is Perl-compatible:
    ///    - empty address in the range correspond to any substring.
    ///    - command with one address correspond to any substring that matches
    ///      the address.
    ///    - command with two addresses correspond to inclusive range from the
    ///      start address to through the next pattern space that maches the
    ///      end address.
    ///
    /// Specified range have effect only for range-dependent functions.
    /// Otherwise range is ignored.
    /// @param addr_start
    ///   Regular expression which assign a starting address of range.
    /// @param addr_end
    ///   Regular expression which assign an ending address of range.
    ///   Should be empty if the start address is empty.
    /// @param delimiter
    ///   Split a source string by "delimiter.
     /// @sa
    ///   ClearRange, ReplaceRange()
    void SetRange(
        const string&  addr_start = kEmptyStr,
        const string&  addr_end   = kEmptyStr,
        const string&  delimiter  = "\n"
    );

    /// Clear range for range-dependent functions.
    ///
    /// Have the same effect as SetRange() without parameters.
    /// @sa
    ///   SetRange()
    void ClearRange(void);

    /// Replace all occurrences of a substring within a string by pattern.
    ///
    /// Use range specified by SetRange() method. Work like SED command s/.
    /// @param search
    ///   Reqular expression to match a substring value that is replaced.
    /// @param replace
    ///   Replace "search" substring with this value. The matched subpatterns
    ///   (if any) can be found and inserted into replace string using
    ///   variables $1, $2, $3, and so forth. The variable can be enclosed
    ///   in the curly brackets {}, that will be deleted on substitution.
    /// @param compile_flags
    ///   Regular expression compilation flags.
    /// @param match_flags
    ///   Flags to match.
    /// @param process_within
    ///   Define which part of the range should be processed.
    /// @param max_replace
    ///   Replace no more than "max_replace" occurrences of substring "search"
    ///   in the every substring. If "max_replace" is zero (default),
    ///   then replace all occurrences with "replace".
    /// @return
    ///   Return the count of replacements.
    /// @sa
    ///   ERange, SetRange(), ClearRange()
    size_t ReplaceRange(
        const string&       search,
        const string&       replace,
        CRegexp::TCompile   compile_flags  = CRegexp::fCompile_default,
        CRegexp::TMatch     match_flags    = CRegexp::fMatch_default,
        CRegexpUtil::ERange process_within = eInside,
        size_t              max_replace    = 0
    );

private:
    /// Divide source string to substrings by delimiter.
    /// If delimiter is empty string that use early defined delimiter.
    void x_Divide(const string& delimiter = kEmptyStr);

    /// Join substrings back to entire string.
    void x_Join(void);

private:
    string       m_Content;       ///< Content string.
    list<string> m_ContentList;   ///< Content list.
    bool         m_IsDivided;     ///< TRUE if m_ContentList is newer than
                                  ///< m_Content, and FALSE otherwise.
    string       m_RangeStart;    ///< Regexp to determine start of range.
    string       m_RangeEnd;      ///< Regexp to determine end of range.
    string       m_Delimiter;     ///< Delimiter used to split string.
};



//////////////////////////////////////////////////////////////////////////////
//
// Inline
//

//
// CRegexp
//

inline
int CRegexp::NumFound() const
{
    return m_NumFound;
}


inline
const int* CRegexp::GetResults(size_t idx) const
{
    if ((int)idx >= m_NumFound) {
        throw runtime_error("idx >= NumFound()");
    }
    return m_Results + 2 * idx;
}

//
// CRegexpUtil
//

inline
string CRegexpUtil::GetResult(void)
{
    if ( m_IsDivided ) {
        x_Join();
    }
    return m_Content;
}

inline
void CRegexpUtil::Reset(const string& str)
{
    m_Content   = str;
    m_IsDivided = false;
    m_ContentList.clear();
}

inline
CRegexpUtil::operator string(void)
{
    return GetResult();
}

inline
void CRegexpUtil::operator= (const string& str)
{
    Reset(str);
}

inline
void CRegexpUtil::ClearRange(void)
{
    SetRange();
}

inline
bool CRegexpUtil::Exists(
    const string&     pattern,
    CRegexp::TCompile compile_flags,
    CRegexp::TMatch   match_flags)
{
    // Fill shure that string is not divided
    x_Join();
    // Check the pattern existence
    CRegexp re(pattern, compile_flags);
    re.GetMatch(m_Content.c_str(), 0, 0, match_flags, true);
    return re.NumFound() > 0;
}

inline
string CRegexpUtil::Extract(
    const string&     pattern,
    CRegexp::TCompile compile_flags,
    CRegexp::TMatch   match_flags,
    size_t            pattern_idx)
{
    // Fill shure that string is not divided
    x_Join();
    // Get the pattern/subpattern
    CRegexp re(pattern, compile_flags);
    return re.GetMatch(m_Content.c_str(), 0, pattern_idx, match_flags);
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.14  2005/04/25 18:51:51  ivanov
 * Using size_t instead TSeqPos
 *
 * Revision 1.13  2004/11/22 18:02:39  ivanov
 * Added flag CRegexp::fCompile_extended.
 *
 * Revision 1.12  2004/11/22 17:15:23  ivanov
 * Introduce fCompile_* and fMatch_* flags.
 * The eCompile_* and eMatch_* are depricated now.
 *
 * Revision 1.11  2004/11/22 16:46:13  ivanov
 * Moved #include <pcre.h> from .hpp to .cpp.
 * Do not assume that default compile and match flags are equal 0,
 * use enum values.
 *
 * Revision 1.10  2004/04/05 16:54:26  ucko
 * Include the internal pcre.h when using MSVC until its build system
 * catches up.
 *
 * Revision 1.9  2004/04/05 15:54:12  ucko
 * Default to using external versions of zlib, bzlib, and libpcre if available.
 *
 * Revision 1.8  2004/03/12 13:57:13  dicuccio
 * Renamed NCBI_REGEXP_EXPORT -> NXBI_XREGEXP_EXPORT to match library name
 *
 * Revision 1.7  2004/03/11 22:54:48  gorelenk
 * Changed export prefix of all classes to NCBI_REGEXP_EXPORT.
 *
 * Revision 1.6  2003/11/07 13:38:51  ivanov
 * Comments changes
 *
 * Revision 1.5  2003/11/06 16:12:10  ivanov
 * Added CRegexpUtil class.
 * Added some new CRegExp::ECompile flags.
 * Added more comments; Some formal code rearrangement.
 *
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
 * ===========================================================================
 */

#endif  /* UTIL___REGEXP__HPP */
