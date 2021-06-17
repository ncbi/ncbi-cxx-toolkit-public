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
*   Abstract parser class
*
*/

#include <ncbi_pch.hpp>
#include "exceptions.hpp"
#include "aparser.hpp"
#include "comments.hpp"

BEGIN_NCBI_SCOPE

AbstractParser::AbstractParser(AbstractLexer& lexer)
    : m_Lexer(&lexer)
{
}

AbstractParser::~AbstractParser(void)
{
}

string AbstractParser::GetLocation(void)
{
    return Lexer().GetName() + "(" + NStr::NumericToString(LastTokenLine()) + "):";
}

void AbstractParser::ParseError(const char* error, const char* expected,
                                const AbstractToken& token)
{
    NCBI_THROW(CDatatoolException,eWrongInput,
               GetLocation() +
//               " LINE " + NStr::IntToString(token.GetLine()) +
               ", TOKEN \"" + token.GetText() + "\": " + error +
               (string(error).empty() ? "" : ": ") + expected + " expected");
}

void AbstractParser::CopyLineComment(int line, CComments& comments,
                                     int flags)
{
    if ( !(flags & eNoFetchNext) )
        Lexer().FillComments();
    _TRACE("CopyLineComment("<<line<<") current: "<<Lexer().CurrentLine());
    _TRACE("  "<<(Lexer().HaveComments()?Lexer().NextComment().GetLine():-1));
    while ( Lexer().HaveComments() ) {
        const AbstractLexer::CComment& c = Lexer().NextComment();
        if ( c.GetLine() > line ) {
            // next comment is below limit line
            return;
        }

        if ( c.GetLine() == line && (flags & eCombineNext) ) {
            // current comment is on limit line -> allow next line comment
            ++line;
        }

        comments.Add(c.GetValue());
        Lexer().SkipNextComment();
    }
}

END_NCBI_SCOPE
