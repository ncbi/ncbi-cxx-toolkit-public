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
*   Abstract lexer class
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.20  2004/05/17 21:03:13  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.19  2003/03/11 20:06:47  kuznets
* iterate -> ITERATE
*
* Revision 1.18  2003/03/10 18:55:18  gouriano
* use new structured exceptions (based on CException)
*
* Revision 1.17  2002/05/06 19:56:12  ucko
* Check for EOF at the beginning of AbstractLexer::FillChar() to handle
* Compaq's iostream implementation.
*
* Revision 1.16  2001/06/11 14:35:02  grichenk
* Added support for numeric tags in ASN.1 specifications and data streams.
*
* Revision 1.15  2001/05/17 15:07:11  lavr
* Typos corrected
*
* Revision 1.14  2000/11/29 17:42:42  vasilche
* Added CComment class for storing/printing ASN.1/XML module comments.
* Added srcutil.hpp file to reduce file dependency.
*
* Revision 1.13  2000/11/20 17:26:31  vasilche
* Fixed warnings on 64 bit platforms.
* Updated names of config variables.
*
* Revision 1.12  2000/11/15 20:34:53  vasilche
* Added user comments to ENUMERATED types.
* Added storing of user comments to ASN.1 module definition.
*
* Revision 1.11  2000/11/14 21:41:23  vasilche
* Added preserving of ASN.1 definition comments.
*
* Revision 1.10  2000/09/26 17:38:25  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.9  2000/08/25 15:59:19  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.8  2000/04/07 19:26:22  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.7  2000/02/01 21:47:51  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.6  1999/11/15 19:36:11  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <serial/datatool/exceptions.hpp>
#include <serial/datatool/alexer.hpp>
#include <serial/datatool/atoken.hpp>
#include <serial/datatool/comments.hpp>

BEGIN_NCBI_SCOPE

#define READ_AHEAD 1024

AbstractLexer::AbstractLexer(CNcbiIstream& in)
    : m_Input(in), m_Line(1),
      m_Buffer(new char[READ_AHEAD]), m_AllocEnd(m_Buffer + READ_AHEAD),
      m_Position(m_Buffer), m_DataEnd(m_Position),
      m_TokenStart(0)
{
}

AbstractLexer::~AbstractLexer(void)
{
    delete []m_Buffer;
}

void AbstractLexer::LexerError(const char* error)
{
    NCBI_THROW(CDatatoolException,eWrongInput,
                 "LINE " + NStr::IntToString(CurrentLine()) +
                 ", TOKEN " + m_TokenText +
                 " -- lexer error: " + error);
}

void AbstractLexer::LexerWarning(const char* error)
{
    ERR_POST("LINE " << CurrentLine() <<
        ", TOKEN " << m_TokenText <<
        " -- lexer error: " << error);
}

bool AbstractLexer::CheckSymbol(char symbol)
{
    if ( TokenStarted() ) {
        return
            m_NextToken.GetToken() == T_SYMBOL &&
            m_NextToken.GetSymbol() == symbol;
    }

    LookupComments();
    if ( Char() != symbol )
        return false;
    
    FillNextToken();
    _ASSERT(m_NextToken.GetToken() == T_SYMBOL &&
            m_NextToken.GetSymbol() == symbol);
    return true;
}

const string& AbstractLexer::ConsumeAndValue(void)
{
    if ( !TokenStarted() )
        LexerError("illegal call: Consume() without NextToken()");
    m_TokenText.assign(CurrentTokenStart(), CurrentTokenEnd());
    m_TokenStart = 0;
    return m_TokenText;
}

const AbstractToken& AbstractLexer::FillNextToken(void)
{
    _ASSERT(!TokenStarted());
    FillComments();
    if ( (m_NextToken.token = LookupToken()) == T_SYMBOL ) {
        m_TokenStart = m_Position;
        m_NextToken.line = CurrentLine();
        if ( m_Position == m_DataEnd ) {
            // no more data read -> EOF
            m_NextToken.token = T_EOF;
        }
        else if ( CurrentTokenLength() == 0 ) {
            AddChar();
        }
        else {
            _ASSERT(CurrentTokenLength() == 1);
        }
    }
    m_NextToken.start = CurrentTokenStart();
    m_NextToken.length = CurrentTokenLength();
    return m_NextToken;
}

char AbstractLexer::FillChar(size_t index)
{
    if (Eof()) {
        return 0;
    }
    char* pos = m_Position + index;
    if ( pos >= m_AllocEnd ) {
        // char will lay outside of buffer
        // first try to remove unused chars
        char* used = m_Position;
        if ( m_TokenStart != 0 && m_TokenStart < used )
            used = m_TokenStart;
        // now used if the beginning of needed data in buffer
        if ( used > m_Buffer ) {
            // skip nonused data at the beginning of buffer
            size_t dataSize = m_DataEnd - used;
            if ( dataSize > 0 ) {
                //                _TRACE("memmove(" << dataSize << ")");
                memmove(m_Buffer, used, dataSize);
            }
            size_t skip = used - m_Buffer;
            m_Position -= skip;
            m_DataEnd -= skip;
            pos -= skip;
            if ( m_TokenStart != 0 )
                m_TokenStart -= skip;
        }
        if ( pos >= m_AllocEnd ) {
            // we still need longer buffer: reallocate it
            // save old offsets
            size_t position = m_Position - m_Buffer;
            size_t dataEnd = m_DataEnd - m_Buffer;
            size_t tokenStart = m_TokenStart == 0? 0: m_TokenStart - m_Buffer;
            // new buffer size
            size_t bufferSize = pos - m_Buffer + READ_AHEAD + 1;
            // new buffer
            char* buffer = new char[bufferSize];
            // copy old data
            //            _TRACE("memcpy(" << dataEnd << ")");
            memcpy(buffer, m_Buffer, dataEnd);
            // delete old buffer
            delete []m_Buffer;
            // restore offsets
            m_Buffer = buffer;
            m_AllocEnd = buffer + bufferSize;
            m_Position = buffer + position;
            m_DataEnd = buffer + dataEnd;
            if ( m_TokenStart != 0 )
                m_TokenStart = buffer + tokenStart;
            pos = m_Position + index;
        }
    }
    while ( pos >= m_DataEnd ) {
        size_t space = m_AllocEnd - m_DataEnd;
        m_Input.read(m_DataEnd, space);
        size_t read = m_Input.gcount();
        //        _TRACE("read(" << space << ") = " << read);
        if ( read == 0 )
            return 0;
        m_DataEnd += read;
    }
    return *pos;
}

void AbstractLexer::SkipNextComment(void)
{
    m_Comments.pop_front();
}

AbstractLexer::CComment& AbstractLexer::AddComment(void)
{
    m_Comments.push_back(CComment(CurrentLine()));
    return m_Comments.back();
}

void AbstractLexer::CComment::AddChar(char c)
{
    m_Value += c;
}

void AbstractLexer::FlushComments(void)
{
    m_Comments.clear();
}

void AbstractLexer::FlushCommentsTo(CComments& comments)
{
    ITERATE ( list<CComment>, i, m_Comments ) {
        comments.Add(i->GetValue());
    }
    FlushComments();
}

END_NCBI_SCOPE
