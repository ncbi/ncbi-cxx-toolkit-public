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


CRegexp::CRegexp(const string &pat, int flags) : m_NumFound(0)
{
    const char *error;
    int erroffset;
    m_PReg = pcre_compile(pat.c_str(), flags, &error, &erroffset, NULL);
    if (m_PReg == NULL) {
        throw runtime_error(error);
    }
}


CRegexp::~CRegexp()
{
    (*pcre_free)(m_PReg);    
}


void CRegexp::Set(const string &pat, int flags)
{
    if (m_PReg != NULL) {
        (*pcre_free)(m_PReg);
    }
            
    const char *error;
    int erroffset;
    m_PReg = pcre_compile(pat.c_str(), flags, &error, &erroffset, NULL);
    if (m_PReg == NULL) {
        throw runtime_error(error);
    }
}


string CRegexp::GetSub(const char *str, size_t idx) const
{
    if ((int)idx >= m_NumFound)
    {
        return kEmptyStr;
    }
    size_t len = m_Results[2 * idx + 1] - m_Results[2 * idx];
    return string(str + m_Results[2 * idx], len);
}


string CRegexp::GetMatch
(const char *str,
 size_t offset,
 size_t idx,
 int flags,
 bool noreturn)
{
    m_NumFound = pcre_exec(m_PReg, NULL, str, strlen(str), offset,
        flags, m_Results, (kRegexpMaxSubPatterns +1) * 3);
    if (noreturn) {
        return kEmptyStr;
    } else {   
        return GetSub(str, idx);
    }
}


int CRegexp::NumFound() const
{
    return m_NumFound;
}


const int* CRegexp::GetResults(size_t idx) const
{
    if ((int)idx >= m_NumFound) {
        throw runtime_error("idx >= NumFound()");
    }
    return m_Results + 2 * idx;
}

END_NCBI_SCOPE
 
/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2003/06/20 18:26:37  clausen
* Switched to native regexp interface
*
* Revision 1.1  2003/06/03 14:46:23  clausen
* Initial version
*
* 
* ===========================================================================
*/

