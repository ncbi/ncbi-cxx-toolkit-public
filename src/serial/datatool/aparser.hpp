#ifndef ABSTRACT_PARSER_HPP
#define ABSTRACT_PARSER_HPP

#include <corelib/ncbistd.hpp>
#include <alexer.hpp>
#include <atoken.hpp>

USING_NCBI_SCOPE;

class AbstractParser {
public:
    AbstractParser(AbstractLexer& lexer);
    virtual ~AbstractParser(void);

    virtual void ParseError(const char* error, const char* expected,
                            const AbstractToken& token);
    void ParseError(const char* error, const char* expected)
        {
            ParseError(error, expected, NextToken());
        }

    void ParseError(const char* expected)
        {
            ParseError("", expected);
        }

    string Location(void) const;

    const AbstractToken& NextToken(void) const
        {
            return m_Lexer.NextToken();
        }
    TToken Next(void) const
        {
            return NextToken().token;
        }

    const AbstractLexer& Lexer(void) const
        {
            return m_Lexer;
        }

    void Consume(void)
        {
            m_Lexer.Consume();
        }
    string ConsumeAndValue(void)
        {
            return m_Lexer.ConsumeAndValue();
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
    string ValueOf(TToken token, const char* expected)
        {
            Expect(token, expected);
            return ConsumeAndValue();
        }

    bool CheckSymbol(char symbol)
        {
            if ( Next() != T_SYMBOL )
                return false;
            return NextToken().GetSymbol() == symbol;
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
    
private:
    AbstractLexer& m_Lexer;
};

#endif
