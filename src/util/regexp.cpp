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
*  C++ wrapper for Perl Compatible Regular Expression (pcre) library
*
* ===========================================================================
*/

#include <util/regexp.hpp>
#include <memory>
 
BEGIN_NCBI_SCOPE


CRegexp::CRegexp(const string &pat, int flags) 
{
    if (regcomp(&m_PReg, pat.c_str(), flags)) {
        throw runtime_error("Could not compile pattern");
    }
}


CRegexp::~CRegexp()
{
    regfree(&m_PReg);    
}


void CRegexp::Set(const string &pat, int flags)
{
    regfree(&m_PReg);
            
    if (regcomp(&m_PReg, pat.c_str(), flags)) {
        throw runtime_error("Could not compile pattern");
    }
}


const CRegexp::SMatches* CRegexp::Match
(const char *cstr,
 size_t occurrence, 
 int flags)
{
    const char *cptr = cstr;
    regmatch_t rslts[kRegexpMaxSubPatterns];
    size_t count = 0, new_offset = 0, old_offset = 0;
    int rtn = 0;
    do {
        rtn = regexec(&m_PReg, cptr, kRegexpMaxSubPatterns, rslts, flags);
        if (rtn != 0 && rtn != REG_NOMATCH) {
            throw runtime_error("Could not execute match");
        }
        if (rtn != REG_NOMATCH) {
            old_offset = new_offset;
            new_offset += rslts[0].rm_eo;
            cptr += rslts[0].rm_eo;
        }
        count++;
    } while(count <= occurrence && rtn != REG_NOMATCH);
    if (rtn == REG_NOMATCH) {
        m_Results[0].m_Begin = -1;
        m_Results[0].m_End = -1;
        return m_Results;
    }
    for (size_t i = 0; i < kRegexpMaxSubPatterns; i++) {
        if (m_Results[i].m_Begin == -1 && rslts[i].rm_so == -1) {
            break;
        }
        m_Results[i].m_Begin = rslts[i].rm_so + old_offset;
        m_Results[i].m_End = rslts[i].rm_eo + old_offset;
    }       
    return m_Results;
}


string CRegexp::GetSub(const char *cstr, size_t idx) const
{
    if (idx >= kRegexpMaxSubPatterns || m_Results[idx].m_Begin < 0 || 
        m_Results[idx].m_End < 0)
    {
        return kEmptyStr;
    }
    string str(cstr);
    size_t len = m_Results[idx].m_End - m_Results[idx].m_Begin;
    return str.substr(m_Results[idx].m_Begin, len);
}


string CRegexp::GetMatch
(const char *cstr,
 size_t idx,
 size_t occurrence,
 int flags)
{
    Match(cstr, occurrence, flags);
    return GetSub(cstr, idx);
}


END_NCBI_SCOPE
 
/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2003/06/03 14:46:23  clausen
* Initial version
*
* 
* ===========================================================================
*/

