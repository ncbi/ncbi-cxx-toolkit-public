#ifndef ABSTRACT_LEXER_HPP
#define ABSTRACT_LEXER_HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <vector>
#include <autoptr.hpp>
#include <atoken.hpp>

USING_NCBI_SCOPE;

class AbstractLexer {
public:
    AbstractLexer(CNcbiIstream& in);
    virtual ~AbstractLexer(void);
    
    const AbstractToken& NextToken(void)
        {
            if ( m_TokenStart >= 0 )
                return m_NextToken;
            else
                return FillNextToken();
        }

    void Consume(void)
        {
            if ( m_TokenStart < 0 )
                LexerError("illegal call: Consume() without NextToken()");
            m_TokenStart = -1;
        }
    string ConsumeAndValue(void)
        {
            if ( m_TokenStart < 0 )
                LexerError("illegal call: Consume() without NextToken()");
            const char* token = CurrentTokenStart();
            int length = CurrentTokenLength();
            m_TokenStart = -1;
            return string(token, length);
        }

    virtual void LexerError(const char* error);
    virtual void LexerWarning(const char* error);

    string CurrentTokenText(void) const
        {
            return string(CurrentTokenStart(), CurrentTokenLength());
        }

protected:
    virtual TToken LookupToken(void) = 0;

    void NextLine(void)
        {
            m_Line++;
        }
    void StartToken(void)
        {
            m_TokenStart = m_Position;
            m_NextToken.line = m_Line;
        }
    void AddChars(int count)
        {
            m_Position += count;
        }
    void AddChar(void)
        {
            AddChars(1);
        }
    void SkipChars(int count)
        {
            AddChars(count);
        }
    void SkipChar(void)
        {
            AddChar();
        }
    char Char(int index)
        {
            int pos = m_Position + index;
            if ( pos < m_Buffer.size() )
                return m_Buffer[pos];
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
            return &m_Buffer[m_TokenStart];
        }
    int CurrentTokenLength(void) const
        {
            return m_Position - m_TokenStart;
        }

private:
    const AbstractToken& FillNextToken();
    char FillChar(int index);

    CNcbiIstream& m_Input;
    vector<char> m_Buffer;
    int m_Line;  // current line in source
    int m_Position; // current position in buffer
    int m_TokenStart; // token start in buffer (-1: not parsed yet)
    AbstractToken m_NextToken;
};

#endif
