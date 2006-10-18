#ifndef ABSTRACT_PARSER_HPP
#define ABSTRACT_PARSER_HPP

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
*   Abstract parser
*
*/

#include <corelib/ncbistd.hpp>
#include <serial/datatool/alexer.hpp>
#include <serial/datatool/atoken.hpp>
#include <list>

BEGIN_NCBI_SCOPE

class CComments;

class AbstractParser {
public:
    AbstractParser(AbstractLexer& lexer);
    virtual ~AbstractParser(void);

    virtual string GetLocation(void);
    virtual void ParseError(const char* error, const char* expected,
                            const AbstractToken& token);
    virtual void EndCommentBlock(void) {}

    void ParseError(const char* error, const char* expected)
        {
            ParseError(error, expected, NextToken());
        }

    void ParseError(const char* expected)
        {
            ParseError("", expected);
        }

    string Location(void) const;

    AbstractLexer& Lexer(void)
        {
            return *m_Lexer;
        }
    const AbstractLexer& Lexer(void) const
        {
            return *m_Lexer;
        }

    const AbstractToken& NextToken(void) const
        {
            return m_Lexer->NextToken();
        }
    TToken Next(void) const
        {
            return NextToken().GetToken();
        }
    int NextTokenLine(void) const
        {
            return NextToken().GetLine();
        }
    int LastTokenLine(void) const
        {
            return Lexer().LastTokenLine();
        }

    void Consume(void)
        {
            Lexer().Consume();
        }
    const string& ConsumeAndValue(void)
        {
            return Lexer().ConsumeAndValue();
        }

    bool Check(TToken token)
        {
            return Next() == token;
        }

    void Expect(TToken token, const char* expected)
        {
            if ( !Check(token) )
                ParseError(expected);
        }
    bool ConsumeIf(TToken token)
        {
            if ( !Check(token) )
                return false;
            Consume();
            return true;
        }
    void Consume(TToken token, const char* expected)
        {
            Expect(token, expected);
            Consume();
        }
    const string& ValueOf(TToken token, const char* expected)
        {
            Expect(token, expected);
            return ConsumeAndValue();
        }

    bool CheckSymbol(char symbol)
        {
            return Lexer().CheckSymbol(symbol);
        }

    void ExpectSymbol(char symbol)
        {
            if ( !CheckSymbol(symbol) ) {
                char expected[2] = { symbol, '\0' };
                ParseError(expected);
            }
        }
    bool ConsumeIfSymbol(char symbol)
        {
            if ( !CheckSymbol(symbol) )
                return false;
            Consume();
            return true;
        }
    void ConsumeSymbol(char symbol)
        {
            ExpectSymbol(symbol);
            Consume();
        }

    char CheckSymbols(char symbol1, char symbol2)
        {
            if ( Next() == T_SYMBOL ) {
                char symbol = NextToken().GetSymbol();
                if ( symbol == symbol1 || symbol == symbol2 )
                    return symbol;
            }
            return '\0';
        }
    
    void CopyComments(CComments& comments)
        {
            Lexer().FlushCommentsTo(comments);
        }
    enum {
        eNoFetchNext = 1,
        eCombineNext = 2
    };
    void CopyLineComment(int line, CComments& comments, int flags = 0);

protected:
    void SetLexer(AbstractLexer* lexer)
    {
        m_Lexer = lexer;
    }

private:
    AbstractLexer* m_Lexer;
};

END_NCBI_SCOPE

#endif
/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.11  2006/10/18 12:58:51  gouriano
* Moved Log to bottom
*
* Revision 1.10  2006/07/24 18:57:13  gouriano
* Preserve comments when parsing DTD
*
* Revision 1.9  2005/01/06 20:24:51  gouriano
* Added GetLocation method - for better diagnostics
*
* Revision 1.8  2002/10/18 14:32:17  gouriano
* added possibility to replace lexer "on the fly"
*
* Revision 1.7  2001/05/17 15:00:42  lavr
* Typos corrected
*
* Revision 1.6  2000/11/29 17:42:29  vasilche
* Added CComment class for storing/printing ASN.1/XML module comments.
* Added srcutil.hpp file to reduce file dependency.
*
* Revision 1.5  2000/11/15 20:34:40  vasilche
* Added user comments to ENUMERATED types.
* Added storing of user comments to ASN.1 module definition.
*
* Revision 1.4  2000/11/14 21:41:12  vasilche
* Added preserving of ASN.1 definition comments.
*
* Revision 1.3  2000/08/25 15:58:45  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.2  2000/04/07 19:26:06  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.1  2000/02/01 21:46:13  vasilche
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
