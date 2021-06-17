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
#include "alexer.hpp"
#include "atoken.hpp"
#include <list>

BEGIN_NCBI_SCOPE

class CComments;

class AbstractParser {
public:
    AbstractParser(AbstractLexer& lexer);
    virtual ~AbstractParser(void);

    virtual string GetLocation(void);
    NCBI_NORETURN
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
