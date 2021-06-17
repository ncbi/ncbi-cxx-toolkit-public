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
* Author: Eugene Vasilchenko
*
* File Description:
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>
#include "comments.hpp"
#include "srcutil.hpp"

BEGIN_NCBI_SCOPE

CComments::CComments(void)
{
}

CComments::~CComments(void)
{
}

CComments& CComments::operator= (const CComments& other)
{
    m_Comments = other.m_Comments;
    return *this;
}

void CComments::Add(const string& s)
{
    m_Comments.push_back(s);
}

CNcbiOstream& CComments::Print(CNcbiOstream& out,
                               const string& before,
                               const string& between,
                               const string& after) const
{
    out << before;

    ITERATE ( TComments, i, m_Comments ) {
        if ( i != m_Comments.begin() )
            out << between;
        out << *i;
    }

    return out << after;
}

CNcbiOstream& CComments::PrintHPPEnum(CNcbiOstream& out) const
{
    return Empty() ? out : Print(out, "  ///<"," ",kEmptyStr);
}

CNcbiOstream& CComments::PrintHPPClass(CNcbiOstream& out) const
{
    return Empty() ? out : Print(out, "///","\n///","\n");
}

CNcbiOstream& CComments::PrintHPPMember(CNcbiOstream& out) const
{
    return Empty() ? out : Print(out, "    ///","\n    ///","\n");
}

bool CComments::PrintSchemaComments(CNcbiOstream& out, int indent, int /*flags*/) const
{
    if ( Empty() ) {
        return false;
    }
    
    out << '>'; // close 'element' tag
#if 0
    PrintASNNewLine(out, indent) << "<xs:annotation><xs:documentation>";
    ITERATE ( TComments, i, m_Comments ) {
        out << '\n' << *i;
    }
    PrintASNNewLine(out, indent) << "</xs:documentation></xs:annotation>";
#else
    if ( OneLine() ) {
        PrintASNNewLine(out, indent) << "<xs:annotation><xs:documentation>";
        out << NStr::TruncateSpaces(m_Comments.front());
        out << "</xs:documentation></xs:annotation>";
    }
    else {
        PrintASNNewLine(out, indent) << "<xs:annotation><xs:documentation>";
        ITERATE ( TComments, i, m_Comments ) {
            out << '\n' << *i;
        }
        PrintASNNewLine(out, indent) << "</xs:documentation></xs:annotation>";
    }
#endif
    return true;
}

CNcbiOstream& CComments::PrintDTD(CNcbiOstream& out, int flags) const
{
    if ( Empty() ) // no comments
        return out;

    if ( !(flags & eDoNotWriteBlankLine) ) {
        // prepend comments by empty line to separate from previous comments
        out << '\n';
    }

    // comments start
    out <<
        "<!--";

    if ( !(flags & eAlwaysMultiline) && OneLine() ) {
        // one line comment
        out << m_Comments.front() << ' ';
    }
    else {
        // multiline comments
        out << '\n';
        ITERATE ( TComments, i, m_Comments ) {
            out << *i << '\n';
        }
    }

    // comments end
    out << "-->";
    
    if ( !(flags & eNoEOL) )
        out << '\n';

    return out;
}

CNcbiOstream& CComments::PrintASN(CNcbiOstream& out,
                                  int indent, int flags) const
{
    if ( Empty() ) // no comments
        return out;

    bool newLine = (flags & eDoNotWriteBlankLine) == 0;
    // prepend comments by empty line to separate from previous comments

    ITERATE ( TComments, i, m_Comments ) {
        if ( newLine )
            PrintASNNewLine(out, indent);
        out << "--" << NStr::Replace(*i, "--", "");
        newLine = true;
    }

    if ( (flags & eNoEOL) == 0 )
        PrintASNNewLine(out, indent);

    return out;
}

END_NCBI_SCOPE
