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
 * File Description:
 *         C++ wrappers for Perl Compatible Regular Expression (pcre) library
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbi_limits.h>
#include <corelib/ncbistl.hpp>
#include <util/regexp.hpp>
#include <pcre.h>

#include <memory>
#include <stdlib.h>

BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////////////////
//
//  CRegexpException
//

class CRegexpException : public CException
{
public:
    enum EErrCode {
        eCompile,
        eBadFlags
    };
    virtual const char* GetErrCodeString(void) const {
        switch ( GetErrCode() ) {
        case eCompile:    return "eCompile";
        case eBadFlags:   return "eBadFlags";
        default:          return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT(CRegexpException,CException);
};


//////////////////////////////////////////////////////////////////////////////
//
//  CRegexp
//

// Macro to check bits
#define F_ISSET(flags, mask) ((flags & (mask)) == (mask))

// Auxiliary functions to convert CRegexp flags to real flags.
static int s_GetRealCompileFlags(CRegexp::TCompile compile_flags)
{
    int flags = 0;

    if ( !compile_flags  &&
         !F_ISSET(compile_flags, CRegexp::fCompile_default )) {
        NCBI_THROW(CRegexpException, eBadFlags,
                   "Bad regular expression compilation flags");
    }
    if ( F_ISSET(compile_flags, CRegexp::fCompile_ignore_case) ) {
        flags |= PCRE_CASELESS;
    }
    if ( F_ISSET(compile_flags, CRegexp::fCompile_dotall) ) {
        flags |= PCRE_DOTALL;
    }
    if ( F_ISSET(compile_flags, CRegexp::fCompile_newline) ) {
        flags |= PCRE_MULTILINE;
    }
    if ( F_ISSET(compile_flags, CRegexp::fCompile_ungreedy) ) {
        flags |= PCRE_UNGREEDY;
    }
    if ( F_ISSET(compile_flags, CRegexp::fCompile_extended) ) {
        flags |= PCRE_EXTENDED;
    }
    return flags;
}

static int s_GetRealMatchFlags(CRegexp::TMatch match_flags)
{
    int flags = 0;

    if ( !match_flags  &&
         !F_ISSET(match_flags, CRegexp::fMatch_default) ) {
        NCBI_THROW(CRegexpException, eBadFlags,
                   "Bad regular expression match flags");
    }
    if ( F_ISSET(match_flags, CRegexp::fMatch_not_begin) ) {
        flags |= PCRE_NOTBOL;
    }
    if ( F_ISSET(match_flags, CRegexp::fMatch_not_end) ) {
        flags |= PCRE_NOTEOL;
    }
    return flags;
}


CRegexp::CRegexp(const string& pattern, TCompile flags)
    : m_NumFound(0)
{
    const char *err;
    int err_offset;
    int x_flags = s_GetRealCompileFlags(flags);

    m_PReg = pcre_compile(pattern.c_str(), x_flags, &err, &err_offset, NULL);
    if (m_PReg == NULL) {
        NCBI_THROW(CRegexpException, eCompile, "Compilation of the pattern '" +
                   pattern + "' failed: " + err);
    }
}


CRegexp::~CRegexp()
{
    (*pcre_free)(m_PReg);
}


void CRegexp::Set(const string& pattern, TCompile flags)
{
    if (m_PReg != NULL) {
        (*pcre_free)(m_PReg);
    }
    const char *err;
    int err_offset;
    int x_flags = s_GetRealCompileFlags(flags);

    m_PReg = pcre_compile(pattern.c_str(), x_flags, &err, &err_offset, NULL);
    if (m_PReg == NULL) {
        NCBI_THROW(CRegexpException, eCompile, "Compilation of the pattern '" +
                   pattern + "' failed: " + err);
    }
}


string CRegexp::GetSub(const string& str, size_t idx) const
{
    int start = m_Results[2 * idx];
    int end   = m_Results[2 * idx + 1];

    if ((int)idx >= m_NumFound  ||  start == -1  ||  end == -1) {
        return kEmptyStr;
    }
    return str.substr(start, end - start);
}


string CRegexp::GetMatch(
    const string& str,
    TSeqPos       offset,
    size_t        idx,
    TMatch        flags,
    bool          noreturn)
{
    int x_flags = s_GetRealMatchFlags(flags);
    m_NumFound = pcre_exec((pcre*)m_PReg, NULL, str.c_str(), (int)str.length(),
                           (int)offset, x_flags, m_Results,
                           (int)(kRegexpMaxSubPatterns +1) * 3);
    if ( noreturn ) {
        return kEmptyStr;
    } else {
        return GetSub(str, idx);
    }
}



//////////////////////////////////////////////////////////////////////////////
//
//  CRegexpUtil
//

CRegexpUtil::CRegexpUtil(const string& str) 
    : m_Content(str), m_IsDivided(false),
      m_RangeStart(kEmptyStr), m_RangeEnd(kEmptyStr), m_Delimiter("\n")
{
    return;
}


void CRegexpUtil::SetRange(
        const string& addr_start,
        const string& addr_end,
        const string& delimiter)
{
    m_RangeStart = addr_start;
    m_RangeEnd   = addr_end;
    x_Divide(delimiter);
}


size_t CRegexpUtil::Replace(
    const string&     search,
    const string&     replace,
    CRegexp::TCompile compile_flags,
    CRegexp::TMatch   match_flags,
    size_t            max_replace)
{
    if ( search.empty() ) {
        return 0;
    }
    size_t n_replace = 0;

    // Fill shure that string is not divided.
    x_Join();

    // Compile regular expression.
    CRegexp re(search, compile_flags);
    size_t  start_pos = 0;

    for (size_t count = 0; !(max_replace && count >= max_replace); count++) {

        // Match pattern.
        re.GetMatch(m_Content.c_str(), (int)start_pos, 0, match_flags, true);
        int num_found = re.NumFound();
        if (num_found <= 0) {
            break;
        }

        // Substitute all subpatterns "$<digit>" to values in the "replace"
        // string.
        const int* result;
        string     x_replace = replace;
        size_t     pos = 0;

        for (;;) {
            // Find "$"
            pos = x_replace.find("$", pos);
            if (pos == NPOS) {
                break;
            }
            // Try to convert string after the "$" to number
            errno = 0;
            const char* startptr = x_replace.c_str() + pos + 1;
            char* endptr = 0;
            long value = strtol(startptr, &endptr, 10);

            if ( errno  ||  endptr == startptr  ||  !endptr  ||
                 value < kMin_Int  ||  value > kMax_Int) {
                // Format error, skip single "$".
                pos++;
                continue;

            }
            int n = (int)value;

            // Get subpattern value
            string subpattern;
            if ( n > 0  &&  n < num_found ) {
                result = re.GetResults(n);
                if (result[0] >= 0  &&  result[1] >= 0) {
                    subpattern = m_Content.substr(result[0],
                                                  result[1] - result[0]);
                }
            }

            // Check braces {$...}
            size_t sp_start = pos;
            size_t sp_end   = endptr - x_replace.c_str();
            if ( sp_start > 0  &&  x_replace[sp_start-1] == '{') {
                sp_start--;
                if ( sp_end <  x_replace.length()  &&
                     x_replace[sp_end] == '}') {
                    sp_end++;
                } else {
                    // Format error -- missed closed brace.
                    sp_start++;
                }
            }
            // Replace $n with subpattern value.
            x_replace.replace(sp_start, sp_end - sp_start, subpattern);

            pos += subpattern.length();
        }

        // Replace pattern with "x_replace".
        result = re.GetResults(0);
        m_Content.replace(result[0], result[1] - result[0], x_replace);
        n_replace++;
        start_pos = result[0] + max(x_replace.length(), (size_t) 1);
    }
    return n_replace;
}


size_t CRegexpUtil::ReplaceRange(
    const string&       search,
    const string&       replace,
    CRegexp::TCompile   compile_flags,
    CRegexp::TMatch     match_flags,
    CRegexpUtil::ERange process_inside,
    size_t              max_replace
    )
{
    if ( search.empty() ) {
        return 0;
    }
    size_t n_replace = 0;

    // Split source string to parts by delimiter
    x_Divide();

    // Flag which denote that current line is inside "range"
    bool inside = m_RangeStart.empty();

    NON_CONST_ITERATE (list<string>, i, m_ContentList) {
        // Get new line
        string line = *i;

        // Check beginning of block [addr_re_start:addr_re_end]
        if ( !inside  &&  !m_RangeStart.empty() ) {
            CRegexp re(m_RangeStart.c_str());
            re.GetMatch(line.c_str(), 0, 0, CRegexp::fMatch_default, true);
            inside = (re.NumFound() > 0);
        } else {
            inside = true;
        }

        // Process current line
        if ( (inside  &&  process_inside == eInside)  ||
             (!inside  &&  process_inside == eOutside) ) {
            CRegexpUtil re(line);
            n_replace += re.Replace(search, replace,
                                    compile_flags, match_flags, max_replace);
            *i = re;
        }

        // Check ending of block [addr_re_start:addr_re_end]
        if ( inside  &&  !m_RangeEnd.empty() ) {
            // Two addresses
            CRegexp re(m_RangeEnd.c_str());
            re.GetMatch(line.c_str(), 0, 0, CRegexp::fMatch_default, true);
            inside = (re.NumFound() <= 0);
        } else {
            // One address -- process one current string only
            inside = false;
        }
    }

    return n_replace;
}


void CRegexpUtil::x_Divide(const string& delimiter)
{
    string x_delimiter = delimiter.empty() ? m_Delimiter : delimiter;
    if ( m_IsDivided  ) {
        if ( x_delimiter == m_Delimiter ) {
            return;
        }
        x_Join();
    }
    m_ContentList.clear();

    // Split source string to parts by delimiter
    size_t pos;
    size_t start_pos = 0;
    for (;;) {
        pos = m_Content.find(x_delimiter, start_pos);
        if (pos == NPOS) {
            m_ContentList.push_back(m_Content.substr(start_pos));
            break;
        } else {
            m_ContentList.push_back(m_Content.substr(start_pos,
                                                     pos - start_pos));
            start_pos = pos + x_delimiter.length();
        }
    }
    m_IsDivided = true;
    // Save delimiter for consecutive joining
    m_Delimiter = x_delimiter;
}


void CRegexpUtil::x_Join(void)
{
    if ( m_IsDivided ) {
        m_Content = NStr::Join(m_ContentList, m_Delimiter);
        m_IsDivided = false;
    }
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.12  2005/03/04 16:27:36  ivanov
 * Fixed number of round brackets in the F_ISSET macro
 *
 * Revision 1.11  2004/11/22 18:03:01  ivanov
 * Added flag CRegexp::fCompile_extended.
 *
 * Revision 1.10  2004/11/22 17:15:23  ivanov
 * Introduce fCompile_* and fMatch_* flags.
 * The eCompile_* and eMatch_* are depricated now.
 *
 * Revision 1.9  2004/11/22 16:45:40  ivanov
 * Moved #include <pcre.h> from .hpp to .cpp.
 * Do not assume that default compile and match flags are equal 0,
 * use enum values. Added runtime flags check.
 *
 * Revision 1.8  2004/07/20 12:18:01  jcherry
 * Guard against endless loop in Replace() when regular expression
 * can match the empty string.
 *
 * Revision 1.7  2004/05/17 21:06:02  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.6  2003/11/07 17:16:23  ivanov
 * Fixed  warnings on 64-bit Workshop compiler
 *
 * Revision 1.5  2003/11/07 13:39:56  ivanov
 * Fixed lines wrapped at 79th columns
 *
 * Revision 1.4  2003/11/06 16:13:04  ivanov
 * Added CRegexpUtil class. Some formal code rearrangement.
 *
 * Revision 1.3  2003/07/16 19:13:50  clausen
 * Added TCompile and TMatch
 *
 * Revision 1.2  2003/06/20 18:26:37  clausen
 * Switched to native regexp interface
 *
 * Revision 1.1  2003/06/03 14:46:23  clausen
 * Initial version
 *
 * ===========================================================================
 */

