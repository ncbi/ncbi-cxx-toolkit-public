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
* Revision 1.28  1999/10/25 20:19:51  vasilche
* Fixed strings representation in text ASN.1 files.
*
* Revision 1.27  1999/10/04 16:22:17  vasilche
* Fixed bug with old ASN.1 structures.
*
* Revision 1.26  1999/09/24 18:19:18  vasilche
* Removed dependency on NCBI toolkit.
*
* Revision 1.25  1999/09/23 21:16:07  vasilche
* Removed dependance on asn.h
*
* Revision 1.24  1999/09/23 18:56:58  vasilche
* Fixed bugs with overloaded methods in objistr*.hpp & objostr*.hpp
*
* Revision 1.23  1999/09/22 20:11:55  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.22  1999/08/17 15:13:06  vasilche
* Comments are allowed in ASN.1 text files.
* String values now parsed in accordance with ASN.1 specification.
*
* Revision 1.21  1999/08/13 15:53:50  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.20  1999/07/26 18:31:35  vasilche
* Implemented skipping of unused values.
* Added more useful error report.
*
* Revision 1.19  1999/07/22 17:33:51  vasilche
* Unified reading/writing of objects in all three formats.
*
* Revision 1.18  1999/07/21 14:20:04  vasilche
* Added serialization of bool.
*
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
#if HAVE_NCBI_C
# include <asn.h>
#endif

BEGIN_NCBI_SCOPE


CObjectIStreamAsn::CObjectIStreamAsn(CNcbiIstream& in)
    : m_Input(in), m_Line(1)
	, m_UngetChar(-1), m_UngetChar1(-1)
{
}

unsigned CObjectIStreamAsn::SetFailFlags(unsigned flags)
{
    if ( flags & ~eEOF )
        ERR_POST("ASN.1 error at " << m_Line);
    return CObjectIStream::SetFailFlags(flags);
}

inline
char CObjectIStreamAsn::GetChar(void)
{
	int unget = m_UngetChar;
	if ( unget >= 0 ) {
        m_UngetChar = m_UngetChar1;
	    m_UngetChar1 = -1;
		return char(unget);
	}
	else {
		char c;
		m_Input.get(c);
        CheckIOError(m_Input);
		return c;
	}
}

inline
char CObjectIStreamAsn::GetChar0(void)
{
	int unget = m_UngetChar;
	if ( unget < 0 )
        ThrowError(eIllegalCall, "bad GetChar0 call");

    m_UngetChar = m_UngetChar1;
    m_UngetChar1 = -1;
    return char(unget);
}

inline
void CObjectIStreamAsn::UngetChar(char c)
{
    if ( m_UngetChar1 >= 0 )
        ThrowError(eIllegalCall, "cannot unget");

    //_TRACE("UngetChar(): '" << char(m_GetChar) << "'");
	m_UngetChar1 = m_UngetChar;
    m_UngetChar = (unsigned char)c;
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
        char c = GetChar();
		if ( c == expect )
			return true;
		UngetChar(c);
	}
	return false;
}

inline
bool CObjectIStreamAsn::FirstIdChar(char c)
{
    return isalpha(c) || c == '_';
}

inline
bool CObjectIStreamAsn::IdChar(char c)
{
    return isalnum(c) || c == '_' || c == '.';
}

inline
void CObjectIStreamAsn::Expect(char expect, bool skipWhiteSpace)
{
    if ( !GetChar(expect, skipWhiteSpace) ) {
        ThrowError(eFormatError, string("'") + expect + "' expected");
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
	        UngetChar(c);
	}
    ThrowError(eFormatError, string("'") + choiceTrue +
               "' or '" + choiceFalse + "' expected");
    return false;
}

void CObjectIStreamAsn::ExpectString(const char* s, bool skipWhiteSpace)
{
    if ( skipWhiteSpace )
        SkipWhiteSpace();
    char c;
    while ( (c = *s++) ) {
        Expect(c);
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
            break;
        case '\n':
            m_Line++;
            break;
        case '-':
            // check for comments
            c = GetChar();
            if ( c != '-' ) {
                UngetChar(c);
                UngetChar('-');
                return '-';
            }
            // skip comments
            SkipComments();
            break;
        default:
            UngetChar(c);
            return c;
        }
    }
}

void CObjectIStreamAsn::SkipComments(void)
{
    for ( ;; ) {
        char c = GetChar();
        switch ( c ) {
        case '\n':
            m_Line++;
            return;
        case '-':
            c = GetChar();
            switch ( GetChar() ) {
            case '\n':
                m_Line++;
                return;
            case '-':
                return;
            }
            break;
        }
    }
}

string CObjectIStreamAsn::ReadTypeName()
{
    string name = ReadId();
    ExpectString("::=", true);
    return name;
}

string CObjectIStreamAsn::ReadEnumName()
{
    if ( FirstIdChar(SkipWhiteSpace()) )
        return ReadId();
    else
        return NcbiEmptyString;
}

template<typename T>
void ReadStdSigned(CObjectIStreamAsn& in, T& data)
{
    bool sign;
    char c = in.GetChar(true);
    switch ( c ) {
    case '-':
        sign = true;
        break;
    case '+':
        sign = false;
        break;
    default:
        in.UngetChar(c);
        sign = false;
        break;
    }
    c = in.GetChar();
    if ( c < '0' || c > '9' ) {
        in.ThrowError(in.eFormatError, "bad number");
    }
    T n = c - '0';
    while ( (c = in.GetChar()) >= '0' && c <= '9' ) {
        // TODO: check overflow
        n = n * 10 + (c - '0');
    }
    in.UngetChar(c);
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
    if ( c < '0' || c > '9' ) {
        in.ThrowError(in.eFormatError, "bad number");
    }
    T n = c - '0';
    while ( (c = in.GetChar()) >= '0' && c <= '9' ) {
        // TODO: check overflow
        n = n * 10 + (c - '0');
    }
    in.UngetChar(c);
    data = n;
}

bool CObjectIStreamAsn::ReadBool(void)
{
    string s = ReadId();
    if ( s == "TRUE" )
        return true;
    if ( s != "FALSE" )
        ThrowError(eFormatError, "TRUE or FALSE expected");
    return false;
}

char CObjectIStreamAsn::ReadChar(void)
{
    string s = ReadString();
    if ( s.size() != 1 ) {
        ThrowError(eFormatError, "one char string expected");
    }
    return s[0];
}

int CObjectIStreamAsn::ReadInt(void)
{
    int data;
    ReadStdSigned(*this, data);
    return data;
}

unsigned CObjectIStreamAsn::ReadUInt(void)
{
    unsigned data;
    ReadStdUnsigned(*this, data);
    return data;
}

long CObjectIStreamAsn::ReadLong(void)
{
#if LONG_MIN == INT_MIN && LONG_MAX == INT_MAX
    return ReadInt();
#else
    long data;
    ReadStdSigned(*this, data);
    return data;
#endif
}

unsigned long CObjectIStreamAsn::ReadULong(void)
{
#if ULONG_MAX == UINT_MAX
    return ReadUInt();
#else
    unsigned long data;
    ReadStdUnsigned(*this, data);
    return data;
#endif
}

double CObjectIStreamAsn::ReadDouble(void)
{
    ThrowError(eIllegalCall, "REAL format unsupported");
    return 0;
}

string CObjectIStreamAsn::ReadString(void)
{
    Expect('\"', true);
    string s;
    char c;
    for ( ;; ) {
        c = GetChar();
        switch ( c ) {
        case '\n':
            m_Line++;
            break;
        case '\"':
            c = GetChar();
            if ( c == '\"' ) {
                // double quote -> one quote
                s += '\"';
            }
            else {
                // end of string
                UngetChar(c);
                return s;
            }
            break;
        default:
            if ( c < ' ' && c >= 0 ) {
                ThrowError(eFormatError,
                           "bad char in string: " + NStr::IntToString(c));
            }
            else {
                s += c;
            }
            break;
        }
    }
}

CObjectIStreamAsn::TIndex CObjectIStreamAsn::ReadIndex(void)
{
    return ReadUInt();
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
	    if ( !FirstIdChar(c) ) {
		    UngetChar(c);
            ERR_POST(Warning << "unexpected char in id");
            return NcbiEmptyString;
	    }
	    s += c;
        for ( ;; ) {
            c = GetChar();
            if ( IdChar(c) ) {
                s += c;
            }
            else if ( c == '-' ) {
                c = GetChar();
                if ( IdChar(c) ) {
                    s += '-';
                    s += c;
                }
                else {
                    UngetChar(c);
                    UngetChar('-');
                    break;
                }
            }
            else {
                UngetChar(c);
                break;
            }
	    }
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
    member.Id().SetName(ReadId());
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
			UngetChar(c);
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
            ThrowError(eFormatError, "bad char in octet string");
		}
		*dst++ = cc;
		count++;
	}
	return count;
}

void CObjectIStreamAsn::End(const ByteBlock& )
{
	ExpectString("\'H");
}

CObjectIStream::EPointerType CObjectIStreamAsn::ReadPointerType(void)
{
    char c;
    switch ( (c = GetChar(true)) ) {
    case 'N':
        ExpectString("ULL");
        return eNullPointer;
    case '@':
        return eObjectPointer;
    case '{':
        UngetChar(c);
        return eThisPointer;
    case ':':
        return eOtherPointer;
    default:
        UngetChar(c);
        return eThisPointer;
    }
}

CObjectIStream::TIndex CObjectIStreamAsn::ReadObjectPointer(void)
{
    return ReadIndex();
}

string CObjectIStreamAsn::ReadOtherPointer(void)
{
    return ReadId();
}

bool CObjectIStreamAsn::HaveMemberSuffix(void)
{
    return GetChar('.', true);
}

string CObjectIStreamAsn::ReadMemberSuffix(void)
{
    return ReadId();
}

void CObjectIStreamAsn::SkipValue()
{
    int blockLevel = 0;
    for ( ;; ) {
        char c = GetChar();
        switch ( c ) {
        case '\'':
            SkipBitString();
            break;
        case '\"':
            SkipString();
            break;
        case '{':
            ++blockLevel;
            break;
        case '}':
            switch ( blockLevel ) {
            case 0:
                UngetChar(c);
                return;
            case 1:
                return;
            default:
                --blockLevel;
                break;
            }
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

void CObjectIStreamAsn::SkipBitString(void)
{
    char c;
    for ( ;; ) {
        c = GetChar();
        if ( c == '\'' ) {
            c = GetChar();
            if ( c != 'B' && c != 'H' )
                UngetChar(c);
            return;
        }
    }
}

void CObjectIStreamAsn::SkipString(void)
{
    char c;
    for ( ;; ) {
        c = GetChar();
        if ( c == '\"' ) {
            c = GetChar();
            if ( c != '\"' ) {
                UngetChar(c);
                return;
            }
        }
    }
}

#if HAVE_NCBI_C
unsigned CObjectIStreamAsn::GetAsnFlags(void)
{
    return ASNIO_TEXT;
}

void CObjectIStreamAsn::AsnOpen(AsnIo& asn)
{
    asn.m_Count = 0;
}

size_t CObjectIStreamAsn::AsnRead(AsnIo& asn, char* data, size_t length)
{
    size_t count = 0;
#if 1
    if ( asn.m_Count == 0 ) {
        // dirty hack to add structure name with '::='
        const string& name = asn.GetRootTypeName();
        SIZE_TYPE nameLength = name.size();
        count = nameLength + 3;
        if ( length < count ) {
            ThrowError(eFormatError,
                       "buffer too small to put structure name: " + name);
        }
        memcpy(data, name.data(), nameLength);
        data[nameLength] = ':';
        data[nameLength + 1] = ':';
        data[nameLength + 2] = '=';
        data += count;
        length -= count;
        asn.m_Count = 1;
    }
#endif
    m_Input.get(data, length - 1);
    CheckIOError(m_Input);
    size_t read = m_Input.gcount();
    data[read++] = m_Input.get();
    CheckIOError(m_Input);
    return count + read;
}
#endif

END_NCBI_SCOPE
