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
* Revision 1.17  1999/07/20 18:23:10  vasilche
* Added interface to old ASN.1 routines.
* Added fixed choice of subclasses to use for pointers.
*
* Revision 1.16  1999/07/19 15:50:33  vasilche
* Added interface to old ASN.1 routines.
* Added naming of key/value in STL map.
*
* Revision 1.15  1999/07/14 18:58:09  vasilche
* Fixed ASN.1 types/field naming.
*
* Revision 1.14  1999/07/13 20:18:18  vasilche
* Changed types naming.
*
* Revision 1.13  1999/07/09 16:32:54  vasilche
* Added OCTET STRING write/read.
*
* Revision 1.12  1999/07/07 21:15:02  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.11  1999/07/07 19:59:05  vasilche
* Reduced amount of data allocated on heap
* Cleaned ASN.1 structures info
*
* Revision 1.10  1999/07/07 18:18:32  vasilche
* Fixed some bugs found by MS VC++
*
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
#include <serial/classinfo.hpp>
#include <asn.h>

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
#if !USE_UNGET
	, m_GetChar(-1), m_UngetChar(-1)
#endif
{
}

inline
char CObjectIStreamAsn::GetChar(void)
{
#if USE_UNGET
	char c;
	if ( !m_Input.get(c) )
		THROW1_TRACE(runtime_error, "unexpected EOF");
	_TRACE("GetChar(): '" << c << "'");
	return c;
#else
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
#endif
}

inline
char CObjectIStreamAsn::GetChar0(void)
{
#if USE_UNGET
	return GetChar();
#else
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
#endif
}

/*
inline
CNcbiIstream& CObjectIStreamAsn::SkipWhiteSpace0(void)
{
    switch ( m_UngetChar ) {
    case -1:
        break;
    case ' ':
    case '\n':
    case '\r':
    case '\t':
    case '\v':
        _TRACE("SkipWhiteSpace0: '" << char(m_UngetChar) << "'");
        m_UngetChar = -1;
        break;
    default:
        _TRACE("SkipWhiteSpace0: '" << char(m_UngetChar) << "'");
        throw runtime_error("bad SkipWhiteSpace0 call");
    }
    m_GetChar = -1;
    return m_Input;
}
*/

inline
void CObjectIStreamAsn::UngetChar(void)
{
#if USE_UNGET
	_TRACE("UngetChar...");
	if ( !m_Input.unget() )
		THROW1_TRACE(runtime_error, "cannot unget");
#else
    if ( m_UngetChar >= 0 || m_GetChar < 0 ) {
        throw runtime_error("cannot unget");
    }
    _TRACE("UngetChar(): '" << char(m_GetChar) << "'");
	m_UngetChar = m_GetChar;
	m_GetChar = -1;
#endif
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

template<typename T>
void ReadStdSigned(CObjectIStreamAsn& in, T& data)
{
    bool sign;
    switch ( in.GetChar(true) ) {
    case '-':
        sign = true;
        break;
    case '+':
        sign = false;
        break;
    default:
        in.UngetChar();
        sign = false;
        break;
    }
    char c = in.GetChar();
    if ( c < '0' || c > '9' )
        THROW1_TRACE(runtime_error, "bad number");
    T n = c - '0';
    while ( (c = in.GetChar()) >= '0' && c <= '9' ) {
        // TODO: check overflow
        n = n * 10 + (c - '0');
    }
    in.UngetChar();
    if ( sign )
        data = -n;
    else
        data = n;
}

template<typename T>
void ReadStdUnsigned(CObjectIStreamAsn& in, T& data)
{
    char c = in.GetChar(true);
    if ( c == '+' )
        c = in.GetChar();
    if ( c < '0' || c > '9' )
        THROW1_TRACE(runtime_error, "bad number");
    T n = c - '0';
    while ( (c = in.GetChar()) >= '0' && c <= '9' ) {
        // TODO: check overflow
        n = n * 10 + (c - '0');
    }
    in.UngetChar();
    data = n;
}

template<typename T>
inline
void ReadStdNumber(CObjectIStreamAsn& in, T& data)
{
    if ( T(-1) < T(0) )
        ReadStdSigned(in, data);
    else
        ReadStdUnsigned(in, data);
}

void CObjectIStreamAsn::ReadStd(signed char& data)
{
    int i;
    ReadStdNumber(*this, i);
    if ( i < -128 || i > 127 )
        THROW1_TRACE(runtime_error, "signed char overflow error");
    data = i;
}

void CObjectIStreamAsn::ReadStd(unsigned char& data)
{
    unsigned i;
    ReadStdNumber(*this, i);
    if ( i > 255 )
        THROW1_TRACE(runtime_error, "unsigned char overflow error");
    data = i;
}

void CObjectIStreamAsn::ReadStd(short& data)
{
    ReadStdNumber(*this, data);
}

void CObjectIStreamAsn::ReadStd(unsigned short& data)
{
    ReadStdNumber(*this, data);
}

void CObjectIStreamAsn::ReadStd(int& data)
{
    ReadStdNumber(*this, data);
}

void CObjectIStreamAsn::ReadStd(unsigned int& data)
{
    ReadStdNumber(*this, data);
}

void CObjectIStreamAsn::ReadStd(long& data)
{
    ReadStdNumber(*this, data);
}

void CObjectIStreamAsn::ReadStd(unsigned long& data)
{
    ReadStdNumber(*this, data);
}

void CObjectIStreamAsn::ReadStd(float& data)
{
    ReadStdNumber(*this, data);
}

void CObjectIStreamAsn::ReadStd(double& data)
{
    ReadStdNumber(*this, data);
}

CObjectIStreamAsn::TIndex CObjectIStreamAsn::ReadIndex(void)
{
    TIndex index;
    ReadStdNumber(*this, index);
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
            ERR_POST(Warning << "unexpected char in id");
            return NcbiEmptyString;
	    }
	    s += c;
		while ( IsAlphaNum(c = GetChar()) || c == '-' ) {
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

void CObjectIStreamAsn::Begin(ByteBlock& )
{
	Expect('\'', true);
}

size_t CObjectIStreamAsn::ReadBytes(const ByteBlock& , char* dst, size_t length)
{
	size_t count = 0;
	while ( length-- > 0 ) {
		char c = GetChar();
		char cc;
		if ( c >= '0' && c <= '9' ) {
			cc = c - '0';
		}
		else if ( c >= 'A' && c <= 'F' ) {
			cc = c - 'A' + 10;
		}
		else if ( c == '\'' ) {
			UngetChar();
			return count;
		}
		else {
			THROW1_TRACE(runtime_error, "bad char in octet string");
		}
		c = GetChar();
		if ( c >= '0' && c <= '9' ) {
			cc = (cc << 4) | (c - '0');
		}
		else if ( c >= 'A' && c <= 'F' ) {
			cc = (cc << 4) | (c - 'A' + 10);
		}
		else {
			THROW1_TRACE(runtime_error, "bad char in octet string");
		}
		*dst++ = cc;
		count++;
	}
	return count;
}

void CObjectIStreamAsn::End(const ByteBlock& )
{
	Expect('\'');
	Expect('H');
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
            UngetChar();
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
            if ( className == "NULL" )
                return 0;
            ExpectString("::=", true);
            _TRACE("CObjectIStreamAsn::ReadPointer: new " << className);
            TTypeInfo typeInfo = CClassInfoTmpl::GetClassInfoByName(className);
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

unsigned CObjectIStreamAsn::GetAsnFlags(void)
{
    return ASNIO_TEXT;
}

void CObjectIStreamAsn::AsnOpen(AsnIo& )
{
}

size_t CObjectIStreamAsn::AsnRead(AsnIo& , char* data, size_t length)
{
    m_Input.get(data, length);
    if ( !m_Input ) {
        ERR_POST("AsnRead: fault");
        return 0;
    }
    size_t count = m_Input.gcount();
    data[count++] = m_Input.get();
    if ( !m_Input ) {
        ERR_POST("AsnRead: fault");
        return 0;
    }
    return count;
}

END_NCBI_SCOPE
