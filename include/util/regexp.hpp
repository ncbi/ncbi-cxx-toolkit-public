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
* Author: Clifford Clausen
*
* File Description:
*   C++ wrapper for Perl Compatible Regular Expression (pcre) library
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <util/regexp/pcreposix.h>

BEGIN_NCBI_SCOPE

const size_t kRegexpMaxSubPatterns = 100;

class CRegexp
{
public:
    // flags for constructor and Set
    enum ECompile {
        eCompile_default     = 0,
        eCompile_ignore_case = REG_ICASE,
        eCompile_newline     = REG_NEWLINE,
        eCompile_both        = REG_NEWLINE | REG_ICASE
    };
    
    // ctors
    CRegexp(const string &pat,  // Perl regular expression
            int flags = 0);     // Flags
    virtual ~CRegexp();
    
    // Set regular expression pattern
    void Set(const string &pat, // Perl regular expression
             int flags = 0);    // Flags
    
    // struct to return Match results
    struct SMatches {
        SMatches() : m_Begin(-1), m_End(-1) {}
        int m_Begin;  // 0 based index of beginning of match
        int m_End;    // 0 based index 1 greater than the last char of match
    };
    
    // Flags for Match and GetMatch
    enum EMatch {
        eMatch_default   = 0,
        eMatch_not_begin = REG_NOTBOL,            // ^ won't match string begin
        eMatch_not_end   = REG_NOTEOL,            // $ won't match string end
        eMatch_not_both  = REG_NOTBOL | REG_NOTEOL
    };
    
    // Used to match pre-compiled perl compatible regular expression
    // compiled either by constructor or subsequent Set. Use occurrence
    // to find occurrences after the first (0 is 1st, 1 is 2nc, etc).
    // Use EMatch for flags. Retuns array of SMatches struct. The 0-th index
    // in array holds the first and one beyond last (0 based) indexes of
    // the occurrence-th match with pattern. Indicies > 0 hold matches to 
    // sub-patterns (e.g., pattern = "(d.*s) *are" has sub-pattern "(d.*s)").
    // If you are trying to find multiple matches to a pattern in cstr,
    // it is more efficient to use occurrence = 0 and manipulate where cstr
    // points before each call than to call Match multiple times, 
    // incrementing occurrence..
    const SMatches* Match(const char *cstr,       // String to search
                          size_t occurrence = 0,  // Occurrence to find
                          int flags = 0);         // Flags

    // Similar to Match, but returns a string corresponding to the match
    // to pattern or sub-pattern. Use idx = 0 for complete pattern. Use idx
    // > 0 for sub-patterns.
    string GetMatch (const char *cstr,            // String to search
                     size_t idx = 0,              // What to return
                     size_t occurrence = 0,       // Occurrence to find
                     int flags = 0);              // Flags
    
    // Should only be called after Match has been called. This function just
    // applies SMatches array calculated by Match against cstr to
    // return a string holding the pattern match. You can do the same thing
    // that this function does, this function is just for convenience. Function
    // GetMatch above is equivalent to a call to Match followed by a call to
    // GetSub.
    string GetSub (const char *cstr,              // String to search
                   size_t idx = 0) const;         // What to return

    
private:
    // Disable copy constructor and assignment operator
    CRegexp(const CRegexp &);
    void operator= (const CRegexp &);
    
    regex_t  m_PReg;                             // Compiled pattern
    SMatches m_Results[kRegexpMaxSubPatterns];   // Results of Match
};


END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2003/06/03 14:47:46  clausen
* Initial version
*
* 
* ===========================================================================
*/

#endif  /* REGEXP__HPP */
