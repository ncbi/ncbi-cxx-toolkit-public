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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.18  2004/05/17 21:03:13  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.17  2003/05/14 14:41:36  gouriano
* corrected writing comments
*
* Revision 1.16  2003/03/10 18:55:18  gouriano
* use new structured exceptions (based on CException)
*
* Revision 1.15  2002/12/17 16:24:43  gouriano
* replaced _ASSERTs by throwing an exception
*
* Revision 1.14  2002/10/18 14:33:14  gouriano
* added possibility to replace lexer "on the fly"
*
* Revision 1.13  2001/06/11 14:35:02  grichenk
* Added support for numeric tags in ASN.1 specifications and data streams.
*
* Revision 1.12  2001/05/17 15:07:11  lavr
* Typos corrected
*
* Revision 1.11  2000/11/29 17:42:42  vasilche
* Added CComment class for storing/printing ASN.1/XML module comments.
* Added srcutil.hpp file to reduce file dependency.
*
* Revision 1.10  2000/11/15 20:34:53  vasilche
* Added user comments to ENUMERATED types.
* Added storing of user comments to ASN.1 module definition.
*
* Revision 1.9  2000/11/14 21:41:23  vasilche
* Added preserving of ASN.1 definition comments.
*
* Revision 1.8  2000/09/26 17:38:25  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.7  2000/08/25 15:59:19  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.6  2000/04/07 19:26:23  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.5  2000/02/01 21:47:52  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.4  1999/11/15 19:36:12  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <serial/datatool/exceptions.hpp>
#include <serial/datatool/aparser.hpp>
#include <serial/datatool/comments.hpp>

BEGIN_NCBI_SCOPE

AbstractParser::AbstractParser(AbstractLexer& lexer)
    : m_Lexer(&lexer)
{
}

AbstractParser::~AbstractParser(void)
{
}

void AbstractParser::ParseError(const char* error, const char* expected,
                                const AbstractToken& token)
{
    NCBI_THROW(CDatatoolException,eWrongInput,
                 "LINE " + NStr::IntToString(token.GetLine())+
                 ", TOKEN " + token.GetText() +
                 " -- parse error: " + error +
                 (error != "" ? ": " : "") + expected + " expected");
}

string AbstractParser::Location(void) const
{
    return NStr::IntToString(LastTokenLine()) + ':';
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
            Lexer().FlushComments();
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
