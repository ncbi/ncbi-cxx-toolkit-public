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
 * Author:  Denis Vakatov
 *
 */

/// @file arg_regexp.cpp
/// 
/// CArgAllow_Regexp -- regexp based constraint for argument value

#include <ncbi_pch.hpp>
#include <util/xregexp/arg_regexp.hpp>


BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////
//  CArgAllow_Regexp::
//

CArgAllow_Regexp::CArgAllow_Regexp(const string& pattern)
    : CArgAllow(),
      m_Pattern(pattern),
      m_Regexp (pattern)
{
    return;
}


CArgAllow_Regexp::~CArgAllow_Regexp(void)
{
    return;
}


bool CArgAllow_Regexp::Verify(const string& value) const
{
    return value.compare(const_cast<CRegexp&>(m_Regexp).GetMatch(value)) == 0;
}


string CArgAllow_Regexp::GetUsage(void) const
{
    return "to match Perl regular expression: '" + m_Pattern + "'";
}


void CArgAllow_Regexp::PrintUsageXml(CNcbiOstream& out) const
{
    out << "<" << "Regexp" << ">" << endl;
    out << m_Pattern;
    out << "</" << "Regexp" << ">" << endl;
}


CArgAllow* CArgAllow_Regexp::Clone(void) const
{
    CArgAllow_Regexp* clone = new CArgAllow_Regexp(m_Pattern);
    return clone;
}



END_NCBI_SCOPE
