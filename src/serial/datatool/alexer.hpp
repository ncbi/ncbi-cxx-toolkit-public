#ifndef ABSTRACT_LEXER_HPP
#define ABSTRACT_LEXER_HPP

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
*   Abstract lexer
*
*/

#include <corelib/ncbistd.hpp>
#include "atoken.hpp"
#include <list>


BEGIN_NCBI_SCOPE

class CComments;
class AbstractParser;

class AbstractLexer {
public:
    AbstractLexer(CNcbiIstream& in, const string& name);
    virtual ~AbstractLexer(void);
    
    void SetParser(AbstractParser* parser)
        {
            m_Parser = parser;
        }
    const AbstractToken& NextToken(void)
        {
            if ( TokenStarted() )
                return m_NextToken;
            else
                return FillNextToken();
        }
    void FillComments(void)
        {
            if ( !TokenStarted() )
                LookupComments();
        }
    bool CheckSymbol(char symbol);

    void Consume(void)
        {
            if ( !TokenStarted() )
                LexerError("illegal call: Consume() without NextToken()");
            m_TokenStart = 0;
        }
    const string& ConsumeAndValue(void);

    int CurrentLine(void) const
        {
            return m_Line;
        }
    int LastTokenLine(void) const
        {
            return m_NextToken.GetLine();
        }

    virtual void LexerError(const char* error);
    virtual void LexerWarning(const char* error, int err_subcode = 0);

    class CComment
    {
    public:
        CComment(int line)
            : m_Line(line)
            {
            }

        int GetLine(void) const
            {
                return m_Line;
            }
        const string& GetValue(void) const
            {
                return m_Value;
            }

        void AddChar(char c);

    private:
        int m_Line;
        string m_Value;
    };

    bool HaveComments(void) const
        {
            return !m_Comments.empty();
        }
    const CComment& NextComment(void) const
        {
            return m_Comments.front();
        }
    void SkipNextComment(void);

    void FlushComments(void);
    void FlushCommentsTo(CComments& comments);
    void FlushCommentsTo(AbstractLexer& lexer);

    const string& GetName(void) const
        {
            return m_Name;
        }
    bool TokenStarted(void) const
        {
            return m_TokenStart != 0;
        }

protected:
    virtual TToken LookupToken(void) = 0;
    virtual void LookupComments(void) = 0;

    void NextLine(void)
        {
            ++m_Line;
        }
    void StartToken(void)
        {
            _ASSERT(!TokenStarted());
            m_TokenStart = m_Position;
            m_NextToken.line = CurrentLine();
        }
    void AddChars(size_t count)
        {
            _ASSERT(TokenStarted());
            m_Position += count;
            _ASSERT(m_Position <= m_DataEnd);
        }
    void AddChar(void)
        {
            AddChars(1);
        }
    void SkipChars(size_t count)
        {
            _ASSERT(!TokenStarted());
            m_Position += count;
            _ASSERT(m_Position <= m_DataEnd);
        }
    void SkipChar(void)
        {
            SkipChars(1);
        }
    char Char(size_t index)
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
    size_t CurrentTokenLength(void) const
        {
            return CurrentTokenEnd() - CurrentTokenStart();
        }
    void EndCommentBlock();

protected:
    CComment& AddComment(void);
    void RemoveLastComment(void);
    AbstractParser* m_Parser;

private:
    const AbstractToken& FillNextToken(void);
    char FillChar(size_t index);

    CNcbiIstream& m_Input;
    int m_Line;  // current line in source
    char* m_Buffer;
    char* m_AllocEnd;
    char* m_Position; // current position in buffer
    char* m_DataEnd; // end of read data in buffer
    char* m_TokenStart; // token start in buffer (0: not parsed yet)
    AbstractToken m_NextToken;
    string m_TokenText;
    list<CComment> m_Comments;
    string m_Name;
};

END_NCBI_SCOPE

#endif
