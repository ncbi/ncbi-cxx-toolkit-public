#ifndef ABSTRACT_LEXER_HPP
#define ABSTRACT_LEXER_HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <vector>
#include "autoptr.hpp"
#include "atoken.hpp"

USING_NCBI_SCOPE;

class AbstractLexer {
public:
    AbstractLexer(CNcbiIstream& in);
    virtual ~AbstractLexer(void);
    
    const AbstractToken& NextToken(void)
        {
            if ( TokenStarted() )
                return m_NextToken;
            else
                return FillNextToken();
        }

    void Consume(void)
        {
            if ( !TokenStarted() )
                LexerError("illegal call: Consume() without NextToken()");
            m_TokenStart = 0;
        }
    string ConsumeAndValue(void);

    string CurrentTokenText(void) const
        {
            _ASSERT(TokenStarted());
            return string(CurrentTokenStart(), CurrentTokenEnd());
        }
    const string& CurrentTokenValue(void) const
        {
            _ASSERT(TokenStarted());
            return m_TokenValue;
        }

    virtual void LexerError(const char* error);
    virtual void LexerWarning(const char* error);

protected:
    virtual TToken LookupToken(void) = 0;

    void NextLine(void)
        {
            m_Line++;
        }
    void StartToken(void)
        {
            _ASSERT(!TokenStarted());
            m_TokenStart = m_Position;
            m_NextToken.line = m_Line;
            m_TokenValue.erase();
        }
    void AddChars(int count)
        {
            _ASSERT(TokenStarted());
            m_Position += count;
            _ASSERT(m_Position <= m_DataEnd);
        }
    void AddChar(void)
        {
            AddChars(1);
        }
    void AddValueChar(char c)
        {
            m_TokenValue += c;
        }
    void SkipChars(int count)
        {
            _ASSERT(!TokenStarted());
            m_Position += count;
            _ASSERT(m_Position <= m_DataEnd);
        }
    void SkipChar(void)
        {
            SkipChars(1);
        }
    char Char(int index)
        {
            char* pos = m_Position + index;
            if ( pos < m_DataEnd )
                return *pos;
            else
                return FillChar(index);
        }
    char Char(void)
        {
            return Char(0);
        }
    bool Eof(void)
        {
            return !m_Input;
        }
    const char* CurrentTokenStart(void) const
        {
            return m_TokenStart;
        }
    const char* CurrentTokenEnd(void) const
        {
            return m_Position;
        }
    int CurrentTokenLength(void) const
        {
            return CurrentTokenEnd() - CurrentTokenStart();
        }

private:
    bool TokenStarted(void) const
        {
            return m_TokenStart != 0;
        }
    const AbstractToken& FillNextToken(void);
    char FillChar(int index);

    CNcbiIstream& m_Input;
    int m_Line;  // current line in source
    char* m_Buffer;
    char* m_AllocEnd;
    char* m_Position; // current position in buffer
    char* m_DataEnd; // end of read data in buffer
    char* m_TokenStart; // token start in buffer (0: not parsed yet)
    AbstractToken m_NextToken;
    string m_TokenValue;
};

#endif
