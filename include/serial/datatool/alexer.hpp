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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.12  2005/02/09 14:20:26  gouriano
* Added GetName() method - for diagnostics
*
* Revision 1.11  2005/01/06 20:21:13  gouriano
* Added name property to lexers - for better diagnostics
*
* Revision 1.10  2001/05/17 15:00:42  lavr
* Typos corrected
*
* Revision 1.9  2000/12/24 00:04:13  vakatov
* Fixed the list of included headers
*
* Revision 1.8  2000/11/29 17:42:29  vasilche
* Added CComment class for storing/printing ASN.1/XML module comments.
* Added srcutil.hpp file to reduce file dependency.
*
* Revision 1.7  2000/11/20 17:26:10  vasilche
* Fixed warnings on 64 bit platforms.
*
* Revision 1.6  2000/11/15 21:02:13  vasilche
* Fixed error.
*
* Revision 1.5  2000/11/15 20:34:40  vasilche
* Added user comments to ENUMERATED types.
* Added storing of user comments to ASN.1 module definition.
*
* Revision 1.4  2000/11/14 21:41:11  vasilche
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
* Revision 1.1  2000/02/01 21:46:12  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.6  1999/11/19 15:48:09  vasilche
* Modified AutoPtr template to allow its use in STL containers (map, vector etc.)
*
* Revision 1.5  1999/11/15 19:36:12  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/datatool/atoken.hpp>
#include <list>


BEGIN_NCBI_SCOPE

class CComments;

class AbstractLexer {
public:
    AbstractLexer(CNcbiIstream& in, const string& name);
    virtual ~AbstractLexer(void);
    
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
    virtual void LexerWarning(const char* error);

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

    const string& GetName(void) const
        {
            return m_Name;
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

protected:
    bool TokenStarted(void) const
        {
            return m_TokenStart != 0;
        }

    CComment& AddComment(void);

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
