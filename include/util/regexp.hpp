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
#include <util/regexp/pcre.h>

BEGIN_NCBI_SCOPE

const size_t kRegexpMaxSubPatterns = 100;

class NCBI_XUTIL_EXPORT CRegexp
{
public:
    // Flags for constructor and Set
    enum ECompile {
        eCompile_default     = 0,
        eCompile_ignore_case = PCRE_CASELESS,
        eCompile_newline     = PCRE_MULTILINE,
        eCompile_both        = PCRE_MULTILINE | PCRE_CASELESS
    };
    
    // ctors
    CRegexp(const string &pat,  // Perl regular expression
            int flags = 0);     // Flags
    virtual ~CRegexp();
    
    // Set regular expression pattern
    void Set(const string &pat, // Perl regular expression
             int flags = 0);    // Options
       
    // GetMatch flags
    enum EMatch {
        eMatch_default   = 0,
        eMatch_not_begin = PCRE_NOTBOL,           // ^ won't match string begin
        eMatch_not_end   = PCRE_NOTEOL,           // $ won't match string end
        eMatch_not_both  = PCRE_NOTBOL | PCRE_NOTEOL
    };
    
    // Returns a string corresponding to the match
    // to pattern or sub-pattern. Use idx = 0 for complete pattern. Use idx
    // > 0 for sub-patterns. Returns empty string when no match
    string GetMatch (const char *str,             // String to search
                     size_t offset = 0,           // Starting offset in str
                     size_t idx = 0,              // (sub) match to return
                     int flags = 0,               // Options
                     bool noreturn = false);      // Returns kEmptyStr if true
    
    // Should only be called after GetMatch has been called with the
    // same cstr. GetMatch internally stores locations on cstr where
    // pattern and sub patterns were found. This function will return
    // the substring at location of pattern match (idx 0) or sub pattern
    // match (idx > 0). Returns empty string when no match
    string GetSub (const char *str,               // String to search
                   size_t idx = 0) const;         // (sub) match to return
    
    // Returns the number of patterns + sub patterns found as a result
    // of the most recent GetMatch call
    int NumFound() const;
    
    // Returns array where index 0 is location of first character in
    // pattern/sub pattern and index 1 is 1 beyond last character in
    // pattern/sub pattern. Throws if called with idx >= NumFound(). 
    // Use idx == 0 for pattern, idx > 0 for sub patterns
    const int* GetResults(size_t idx) const;
       
private:
    // Disable copy constructor and assignment operator
    CRegexp(const CRegexp &);
    void operator= (const CRegexp &);
    
    pcre  *m_PReg;                                  // Compiled pattern
    int m_Results[(kRegexpMaxSubPatterns +1) * 3];  // Results of Match
    int m_NumFound;                                 // #(sub) patterns
};


END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
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
