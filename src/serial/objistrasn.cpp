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
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  1999/06/16 20:35:31  vasilche
* Cleaned processing of blocks of data.
* Added input from ASN.1 text format.
*
* Revision 1.7  1999/06/15 16:19:49  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.6  1999/06/11 19:14:58  vasilche
* Working binary serialization and deserialization of first test object.
*
* Revision 1.5  1999/06/10 21:06:46  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.4  1999/06/07 20:10:02  vasilche
* Avoid using of numeric_limits.
*
* Revision 1.3  1999/06/07 19:30:25  vasilche
* More bug fixes
*
* Revision 1.2  1999/06/04 20:51:45  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:53  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objistrasn.hpp>
#include <serial/classinfo.hpp>

BEGIN_NCBI_SCOPE


static inline
bool IsAlpha(char c)
{
    return isalpha(c) || c == '_';
}

static inline
bool IsAlphaNum(char c)
{
    return isalnum(c) || c == '_';
}

CObjectIStreamAsn::CObjectIStreamAsn(CNcbiIstream& in)
    : m_Input(in)
{
}

char CObjectIStreamAsn::GetChar(void)
{
    char c;
    if ( !m_Input.get(c) ) {
        THROW1_TRACE(runtime_error, "unexpected EOF");
    }
    return c;
}

void CObjectIStreamAsn::UngetChar(char )
{
    if ( !m_Input.unget() ) {
        THROW1_TRACE(runtime_error, "cannot unget");
    }
}

bool CObjectIStreamAsn::GetChar(char expect)
{
    char c = GetChar();
    if ( c != expect ) {
        UngetChar(c);
        return false;
    }
    return true;
}

void CObjectIStreamAsn::Expect(char expect)
{
    if ( !GetChar(expect) ) {
        THROW1_TRACE(runtime_error, string("'") + expect + "' expected");
    }
}

bool CObjectIStreamAsn::Expect(char choiceTrue, char choiceFalse)
{
    char c = GetChar();
    if ( c == choiceTrue )
        return true;
    else if ( c == choiceFalse )
        return false;
    else {
        UngetChar(c);
        THROW1_TRACE(runtime_error, string("'") + choiceTrue + "' or '" + choiceFalse + "' expected");
    }
}

void CObjectIStreamAsn::SkipWhiteSpace(void)
{
    char c;
    while ( m_Input.get(c) ) {
        switch ( c ) {
        case ' ':
        case '\r':
        case '\t':
        case '\n':
            break;
        default:
            UngetChar(c);
            return;
        }
    }
}

bool CObjectIStreamAsn::ReadEscapedChar(char& out, char terminator)
{
    char c = GetChar();
    if ( c == '\\' ) {
        c = GetChar();
        switch ( c ) {
        case 'r':
            c = '\r';
            break;
        case 'n':
            c = '\n';
            break;
        case 't':
            c = '\t';
            break;
        default:
            if ( c >= '0' && c <= '3' ) {
                // octal code
                c -= '0';
                for ( int i = 1; i < 2; i++ ) {
                    char cc = GetChar();
                    if ( cc >= '0' && cc <= '7' ) {
                        c = c << 3 + (cc - '0');
                    }
                    else {
                        UngetChar(cc);
                        break;
                    }
                }
            }
        }
        out = c;
        return true;
    }
    else if ( c < ' ' ) {
        THROW1_TRACE(runtime_error, "bad char in string");
    }
    else if ( c == terminator ) {
        return false;
    }
    else {
        out = c;
        return true;
    }
}

void CObjectIStreamAsn::ReadStd(char& data)
{
    SkipWhiteSpace();
    Expect('\'');
    if ( !ReadEscapedChar(data, '\'') )
        THROW1_TRACE(runtime_error, "empty char");
    Expect('\'');
}

void CObjectIStreamAsn::ReadStd(signed char& data)
{
    SkipWhiteSpace();
    int i;
    m_Input >> i;
    if ( i < -128 || i > 127 ) {
        THROW1_TRACE(runtime_error, "overflow error");
    }
    data = i;
}

void CObjectIStreamAsn::ReadStd(unsigned char& data)
{
    SkipWhiteSpace();
    unsigned i;
    m_Input >> i;
    if ( i > 255 ) {
        THROW1_TRACE(runtime_error, "overflow error");
    }
    data = i;
}

void CObjectIStreamAsn::ReadStd(short& data)
{
    SkipWhiteSpace();
    m_Input >> data;
    if ( !m_Input )
        THROW1_TRACE(runtime_error, "overflow error");
}

void CObjectIStreamAsn::ReadStd(unsigned short& data)
{
    SkipWhiteSpace();
    m_Input >> data;
    if ( !m_Input )
        THROW1_TRACE(runtime_error, "overflow error");
}

void CObjectIStreamAsn::ReadStd(int& data)
{
    SkipWhiteSpace();
    m_Input >> data;
    if ( !m_Input )
        THROW1_TRACE(runtime_error, "overflow error");
}

void CObjectIStreamAsn::ReadStd(unsigned int& data)
{
    SkipWhiteSpace();
    m_Input >> data;
    if ( !m_Input )
        THROW1_TRACE(runtime_error, "overflow error");
}

void CObjectIStreamAsn::ReadStd(long& data)
{
    SkipWhiteSpace();
    m_Input >> data;
    if ( !m_Input )
        THROW1_TRACE(runtime_error, "overflow error");
}

void CObjectIStreamAsn::ReadStd(unsigned long& data)
{
    SkipWhiteSpace();
    m_Input >> data;
    if ( !m_Input )
        THROW1_TRACE(runtime_error, "overflow error");
}

void CObjectIStreamAsn::ReadStd(float& data)
{
    SkipWhiteSpace();
    m_Input >> data;
    if ( !m_Input )
        THROW1_TRACE(runtime_error, "overflow error");
}

void CObjectIStreamAsn::ReadStd(double& data)
{
    SkipWhiteSpace();
    m_Input >> data;
    if ( !m_Input )
        THROW1_TRACE(runtime_error, "overflow error");
}

void CObjectIStreamAsn::ReadStd(string& data)
{
    data = ReadString();
}

CObjectIStreamAsn::TIndex CObjectIStreamAsn::ReadIndex(void)
{
    TIndex index;
    m_Input >> index;
    if ( !m_Input )
        THROW1_TRACE(runtime_error, "cannot read index");
    return index;
}

string CObjectIStreamAsn::ReadString(void)
{
    SkipWhiteSpace();
    Expect('\"');
    string s;
    char c;
    while ( ReadEscapedChar(c, '\"') ) {
        s += c;
    }
    return s;
}

string CObjectIStreamAsn::ReadId(void)
{
    SkipWhiteSpace();
    char c = GetChar();
    if ( !IsAlpha(c) ) {
        UngetChar(c);
        THROW1_TRACE(runtime_error, "unexpected char in id");
    }
    string s;
    s += c;
    while ( IsAlphaNum(c = GetChar()) ) {
        s += c;
    }
    UngetChar(c);
    return s;
}

void CObjectIStreamAsn::VBegin(Block& )
{
    SkipWhiteSpace();
    Expect('{');
}

bool CObjectIStreamAsn::VNext(const Block& block)
{
    SkipWhiteSpace();
    if ( block.GetNextIndex() == 0 ) {
        return !GetChar('}');
    }
    else {
        return Expect(',', '}');
    }
}

CTypeInfo::TObjectPtr CObjectIStreamAsn::ReadPointer(TTypeInfo declaredType)
{
    _TRACE("CObjectIStreamAsn::ReadPointer(" << declaredType->GetName() << ")");
    SkipWhiteSpace();
    char c = GetChar();
    CIObjectInfo info;
    switch ( c ) {
    case '0':
        _TRACE("CObjectIStreamAsn::ReadPointer: null");
        return 0;
    case '{':
        {
            _TRACE("CObjectIStreamAsn::ReadPointer: new");
            TObjectPtr object = declaredType->Create();
            RegisterObject(object, declaredType);
            Read(object, declaredType);
            return object;
        }
    case '@':
        {
            TIndex index = ReadIndex();
            _TRACE("CObjectIStreamAsn::ReadPointer: @" << index);
            info = GetRegisteredObject(index);
            break;
        }
    default:
        UngetChar(c);
        if ( IsAlpha(c) ) {
            string className = ReadId();
            _TRACE("CObjectIStreamAsn::ReadPointer: new " << className);
            TTypeInfo typeInfo = CTypeInfo::GetTypeInfoByName(className);
            TObjectPtr object = typeInfo->Create();
            RegisterObject(object, typeInfo);
            Read(object, typeInfo);
            info = CIObjectInfo(object, typeInfo);
            break;
        }
        else {
            THROW1_TRACE(runtime_error, "invalid object start");
        }
    }
    while ( (c = GetChar()) == '.' ) {
        string memberName = ReadId();
        _TRACE("CObjectIStreamAsn::ReadObjectPointer: member " << memberName);
        const CMemberInfo* memberInfo =
            info.GetTypeInfo()->FindMember(memberName);
        if ( memberInfo == 0 ) {
            THROW1_TRACE(runtime_error, "member not found: " +
                         info.GetTypeInfo()->GetName() + "." + memberName);
        }
        info = CIObjectInfo(memberInfo->GetMember(info.GetObject()),
                            memberInfo->GetTypeInfo());
    }
    UngetChar(c);
    if ( info.GetTypeInfo() != declaredType ) {
        THROW1_TRACE(runtime_error, "incompatible member type");
    }
    return info.GetObject();
}

void CObjectIStreamAsn::SkipValue()
{
    int blockLevel = 0;
    for ( ;; ) {
        char c, c1;
        switch ( (c = GetChar()) ) {
        case '\'':
        case '\"':
            while ( ReadEscapedChar(c1, c) )
                ;
            break;
        case '{':
            ++blockLevel;
            break;
        case '}':
            if ( blockLevel == 0 ) {
                UngetChar(c);
                return;
            }
            --blockLevel;
            break;
        case ',':
            if ( blockLevel == 0 ) {
                UngetChar(c);
                return;
            }
            break;
        default:
            break;
        }
    }
}

END_NCBI_SCOPE
