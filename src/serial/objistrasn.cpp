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
* Revision 1.9  1999/07/02 21:31:55  vasilche
* Implemented reading from ASN.1 binary format.
*
* Revision 1.8  1999/07/01 17:55:30  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.7  1999/06/30 16:04:55  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.6  1999/06/24 14:44:55  vasilche
* Added binary ASN.1 output.
*
* Revision 1.5  1999/06/18 16:26:49  vasilche
* Fixed bug with unget() in MSVS
*
* Revision 1.4  1999/06/17 21:08:51  vasilche
* Fixed bug with unget()
*
* Revision 1.3  1999/06/17 20:42:05  vasilche
* Fixed storing/loading of pointers.
*
* Revision 1.2  1999/06/16 20:58:04  vasilche
* Added GetPtrTypeRef to avoid conflict in MSVS.
*
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
#include <serial/member.hpp>

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
    : m_Input(in), m_GetChar(-1), m_UngetChar(-1)
{
}

inline
char CObjectIStreamAsn::GetChar(void)
{
	int unget = m_UngetChar;
	if ( unget >= 0 ) {
	    m_UngetChar = -1;
		m_GetChar = unget;
        _TRACE("GetChar(): '" << char(unget) << "'");
		return char(unget);
	}
	else {
		char c;
		if ( !m_Input.get(c) ) {
			throw runtime_error("unexpected EOF");
		}
		m_GetChar = (unsigned char)c;
        _TRACE("GetChar(): '" << c << "'");
		return c;
	}
}

inline
char CObjectIStreamAsn::GetChar0(void)
{
	int unget = m_UngetChar;
	if ( unget >= 0 ) {
        _TRACE("GetChar0(): '" << char(unget) << "'");
	    m_UngetChar = -1;
		m_GetChar = unget;
		return char(unget);
	}
	else {
		throw runtime_error("bad GetChar0 call");
	}
}

inline
void CObjectIStreamAsn::UngetChar(void)
{
    if ( m_UngetChar >= 0 || m_GetChar < 0 ) {
        throw runtime_error("cannot unget");
    }
    _TRACE("UngetChar(): '" << char(m_GetChar) << "'");
	m_UngetChar = m_GetChar;
	m_GetChar = -1;
}

inline
char CObjectIStreamAsn::GetChar(bool skipWhiteSpace)
{
    if ( skipWhiteSpace ) {
        SkipWhiteSpace();
		return GetChar0();
	}
	else {
		return GetChar();
	}
}

inline
bool CObjectIStreamAsn::GetChar(char expect, bool skipWhiteSpace)
{
	if ( skipWhiteSpace ) {
		if ( SkipWhiteSpace() == expect ) {
			GetChar0();
			return true;
		}
	}
	else {
		if ( GetChar() == expect )
			return true;
		UngetChar();
	}
	return false;
}

inline
void CObjectIStreamAsn::Expect(char expect, bool skipWhiteSpace)
{
    if ( !GetChar(expect, skipWhiteSpace) ) {
        THROW1_TRACE(runtime_error, string("'") + expect + "' expected");
    }
}

inline
bool CObjectIStreamAsn::Expect(char choiceTrue, char choiceFalse,
                                      bool skipWhiteSpace)
{
	if ( skipWhiteSpace ) {
		char c = SkipWhiteSpace();
		if ( c == choiceTrue ) {
			GetChar0();
			return true;
		}
		else if ( c == choiceFalse ) {
			GetChar0();
			return false;
		}
	}
	else {
		char c = GetChar(skipWhiteSpace);
	    if ( c == choiceTrue )
		    return true;
	    else if ( c == choiceFalse )
		    return false;
		else
	        UngetChar();
	}
    THROW1_TRACE(runtime_error, string("'") + choiceTrue +
                 "' or '" + choiceFalse + "' expected");
}

void CObjectIStreamAsn::ExpectString(const string& s, bool skipWhiteSpace)
{
    if ( skipWhiteSpace )
        SkipWhiteSpace();
    for ( string::const_iterator i = s.begin(); i != s.end(); ++i ) {
        Expect(*i);
    }
}

char CObjectIStreamAsn::SkipWhiteSpace(void)
{
    for ( ;; ) {
		char c = GetChar();
        switch ( c ) {
        case ' ':
        case '\r':
        case '\t':
        case '\n':
            break;
        default:
            UngetChar();
            return c;
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
                        c = (c << 3) + (cc - '0');
                    }
                    else {
                        UngetChar();
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

void CObjectIStreamAsn::Read(TObjectPtr object, TTypeInfo typeInfo)
{
	char c = SkipWhiteSpace();
    if ( c == typeInfo->GetName()[0] || c == '[' ) {
        string id = ReadId();
        if ( id != typeInfo->GetName() ) {
            THROW1_TRACE(runtime_error, "invalid object type: " + id +
                         ", expected: " + typeInfo->GetName());
        }
        ExpectString("::=", true);
    }
    CObjectIStream::Read(object, typeInfo);
}

void CObjectIStreamAsn::ReadStd(char& data)
{
    Expect('\'', true);
    if ( !ReadEscapedChar(data, '\'') )
        THROW1_TRACE(runtime_error, "empty char");
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
//    SkipWhiteSpace();
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
    //    SkipWhiteSpace();
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

void CObjectIStreamAsn::ReadStd(char*& data)
{
    data = strdup(ReadString().c_str());
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
    Expect('\"', true);
    string s;
    char c;
    while ( ReadEscapedChar(c, '\"') ) {
        s += c;
    }
    return s;
}

string CObjectIStreamAsn::ReadId(void)
{
	string s;
    char c = GetChar(true);
	if ( c == '[' ) {
		while ( (c = GetChar()) != ']' ) {
			s += c;
		}
	}
	else {
	    if ( !IsAlpha(c) ) {
		    UngetChar();
			THROW1_TRACE(runtime_error, "unexpected char in id");
	    }
	    s += c;
		while ( IsAlphaNum(c = GetChar()) ) {
			s += c;
	    }
		UngetChar();
	}
    return s;
}

void CObjectIStreamAsn::VBegin(Block& )
{
    Expect('{', true);
}

bool CObjectIStreamAsn::VNext(const Block& block)
{
    if ( block.First() ) {
        return !GetChar('}', true);
    }
    else {
        return Expect(',', '}', true);
    }
}

void CObjectIStreamAsn::StartMember(Member& member)
{
    member.SetName(ReadId());
}

TObjectPtr CObjectIStreamAsn::ReadPointer(TTypeInfo declaredType)
{
    _TRACE("CObjectIStreamAsn::ReadPointer(" << declaredType->GetName() << ")");
    char c = GetChar(true);
    CIObjectInfo info;
    switch ( c ) {
    case '0':
        _TRACE("CObjectIStreamAsn::ReadPointer: null");
        return 0;
    case '{':
        {
            _TRACE("CObjectIStreamAsn::ReadPointer: new");
            TObjectPtr object = declaredType->Create();
            ReadExternalObject(object, declaredType);
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
        UngetChar();
        if ( IsAlpha(c) || c == '[' ) {
            string className = ReadId();
            ExpectString("::=", true);
            _TRACE("CObjectIStreamAsn::ReadPointer: new " << className);
            TTypeInfo typeInfo = CTypeInfo::GetTypeInfoByName(className);
            TObjectPtr object = typeInfo->Create();
            ReadExternalObject(object, typeInfo);
            info = CIObjectInfo(object, typeInfo);
            break;
        }
        else {
            _TRACE("CObjectIStreamAsn::ReadPointer: default new");
            TObjectPtr object = declaredType->Create();
            ReadExternalObject(object, declaredType);
            return object;
        }
    }
    while ( GetChar('.', true) ) {
        string memberName = ReadId();
        _TRACE("CObjectIStreamAsn::ReadObjectPointer: member " << memberName);
        CTypeInfo::TMemberIndex index =
            info.GetTypeInfo()->FindMember(memberName);
        if ( index < 0 ) {
            THROW1_TRACE(runtime_error, "member not found: " +
                         info.GetTypeInfo()->GetName() + "." + memberName);
        }
        const CMemberInfo* memberInfo =
            info.GetTypeInfo()->GetMemberInfo(index);
        info = CIObjectInfo(memberInfo->GetMember(info.GetObject()),
                            memberInfo->GetTypeInfo());
    }
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
                UngetChar();
                return;
            }
            --blockLevel;
            break;
        case ',':
            if ( blockLevel == 0 ) {
                UngetChar();
                return;
            }
            break;
        default:
            break;
        }
    }
}

END_NCBI_SCOPE
