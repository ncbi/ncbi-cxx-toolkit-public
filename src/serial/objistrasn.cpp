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
* Revision 1.35  2000/02/01 21:47:22  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Added buffering to CObjectIStreamAsn.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
*
* Revision 1.34  2000/01/11 14:27:42  vasilche
* Found absent DBL_* macros in float.h
*
* Revision 1.33  2000/01/11 14:16:45  vasilche
* Fixed pow ambiguity.
*
* Revision 1.32  2000/01/10 19:46:40  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.31  2000/01/05 19:43:54  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
* Revision 1.30  1999/12/17 19:05:03  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.29  1999/11/18 20:19:01  vakatov
* ExpectString() -- get rid of the CodeWarrior(MAC) C++ compiler warning
*
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
#include <corelib/ncbiutil.hpp>
#include <serial/objistrasn.hpp>
#include <serial/member.hpp>
#include <serial/classinfo.hpp>
#include <serial/enumerated.hpp>
#include <serial/memberlist.hpp>
#include <math.h>
#if !defined(DBL_MAX_10_EXP) || !defined(FLT_MAX)
# include <float.h>
#endif
#if HAVE_NCBI_C
# include <asn.h>
#endif

BEGIN_NCBI_SCOPE


CObjectIStreamAsn::CObjectIStreamAsn(CNcbiIstream& in)
    : m_Input(in)
{
}

unsigned CObjectIStreamAsn::SetFailFlags(unsigned flags)
{
    if ( flags & ~eEOF )
        ERR_POST("ASN.1 error at " << m_Input.GetLine());
    return CObjectIStream::SetFailFlags(flags);
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
char CObjectIStreamAsn::GetChar(void)
{
    return m_Input.GetChar();
}

inline
void CObjectIStreamAsn::UngetChar(void)
{
    m_Input.UngetChar();
}

inline
const char* CObjectIStreamAsn::GetMark(void) const
{
    return m_Input.GetMarkPos();
}

inline
char CObjectIStreamAsn::GetChar(bool skipWhiteSpace)
{
    return skipWhiteSpace? SkipWhiteSpaceAndGetChar(): GetChar();
}

inline
bool CObjectIStreamAsn::GetChar(char expect, bool skipWhiteSpace)
{
    if ( GetChar(skipWhiteSpace) != expect ) {
        UngetChar();
        return false;
    }
    return true;
}

void CObjectIStreamAsn::Expect(char expect, bool skipWhiteSpace)
{
    if ( !GetChar(expect, skipWhiteSpace) ) {
        ThrowError(eFormatError,
                   string("'") + m_Input.PeekChar() + "': expected '" +
                   expect + "'");
    }
}

bool CObjectIStreamAsn::Expect(char choiceTrue, char choiceFalse,
                                      bool skipWhiteSpace)
{
    char c = GetChar(skipWhiteSpace);
    if ( c == choiceTrue ) {
        return true;
    }
    else if ( c == choiceFalse ) {
        return false;
    }
    UngetChar();
    ThrowError(eFormatError,
               string("'") + m_Input.PeekChar() + "': '"+
               choiceTrue + "' or '" + choiceFalse + "' expected");
    return false;
}

void CObjectIStreamAsn::ExpectString(const char* s, bool skipWhiteSpace)
{
    Expect(*s++, skipWhiteSpace);
    char c;
    while ( (c = *s++) != 0 ) {
        Expect(c);
    }
}

void CObjectIStreamAsn::SkipEndOfLine(char c)
{
    m_Input.SkipEndOfLine(c);
}

char CObjectIStreamAsn::SkipWhiteSpaceAndGetChar(void)
{
    for ( ;; ) {
		char c = GetChar();
        switch ( c ) {
        case ' ':
        case '\t':
            continue;
        case '\r':
        case '\n':
            SkipEndOfLine(c);
            continue;
        case '-':
            // check for comments
            if ( m_Input.PeekChar() != '-' )
                return '-';
            // skip comments
            SkipComments();
            continue;
        default:
            return c;
        }
    }
}

void CObjectIStreamAsn::SkipComments(void)
{
    try {
        for ( ;; ) {
            char c = GetChar();
            switch ( c ) {
            case '\r':
            case '\n':
                SkipEndOfLine(c);
                return;
            case '-':
                c = GetChar();
                switch ( GetChar() ) {
                case '\r':
                case '\n':
                    SkipEndOfLine(c);
                    return;
                case '-':
                    return;
                }
                continue;
            default:
                continue;
            }
        }
    }
    catch ( CEofException& /* ignored */ ) {
        return;
    }
}

size_t CObjectIStreamAsn::ReadId(void)
{
    char c = SkipWhiteSpaceAndGetChar();
    if ( c == '[' ) {
        m_Input.MarkPos();
        for ( size_t i = 0; ; ++i ) {
            switch ( m_Input.PeekChar(i) ) {
            case '\r':
            case '\n':
                ThrowError(eFormatError, "end of line: expected ']'");
                break;
            case ']':
                m_Input.SkipChars(i + 1);
                return i;
            }
        }
    }
	else {
        UngetChar();
        m_Input.MarkPos();
        if ( !FirstIdChar(c) ) {
            return 0;
        }
        else {
            for ( size_t i = 1; ; ++i ) {
                c = m_Input.PeekChar(i);
                if ( !IdChar(c) &&
                     (c != '-' || !IdChar(m_Input.PeekChar(i + 1))) ) {
                    m_Input.SkipChars(i);
                    return i;
                }
            }
        }
	}
}

void CObjectIStreamAsn::ReadNull(void)
{
    ExpectString("NULL", true);
}

string CObjectIStreamAsn::ReadTypeName()
{
    size_t size = ReadId();
    ExpectString("::=", true);
    return string(GetMark(), size);
}

pair<long, bool>
CObjectIStreamAsn::ReadEnum(const CEnumeratedTypeValues& values)
{
    // not integer
    size_t size = ReadId();
    if ( size != 0 ) {
        // enum element by name
        return make_pair(values.FindValue(string(GetMark(), size)), true);
    }
    if ( values.IsInteger() ) {
        // allow any integer
        return make_pair(0l, false);
    }
    
    // enum element by value
    long value = ReadLong();
    values.FindName(value, false);
    return make_pair(value, true);
}

template<typename T>
void ReadStdSigned(CObjectIStreamAsn& in, T& data)
{
    bool sign;
    char c = in.GetChar(true);
    switch ( c ) {
    case '-':
        sign = true;
        c = in.GetChar();
        break;
    case '+':
        sign = false;
        c = in.GetChar();
        break;
    default:
        sign = false;
        break;
    }
    if ( c < '0' || c > '9' ) {
        in.ThrowError(in.eFormatError, "bad number");
    }
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
    if ( c < '0' || c > '9' ) {
        in.ThrowError(in.eFormatError, "bad number");
    }
    T n = c - '0';
    while ( (c = in.GetChar()) >= '0' && c <= '9' ) {
        // TODO: check overflow
        n = n * 10 + (c - '0');
    }
    in.UngetChar();
    data = n;
}

bool CObjectIStreamAsn::ReadBool(void)
{
    size_t size = ReadId();
    if ( size == 4 && memcmp(GetMark(), "TRUE", 4) == 0 )
        return true;
    if ( size == 5 && memcmp(GetMark(), "FALSE", 5) == 0 )
        return false;

    ThrowError(eFormatError,
               "\"" + string(GetMark(), size) + "\": TRUE or FALSE expected");
    return false;
}

char CObjectIStreamAsn::ReadChar(void)
{
    string s;
    ReadString(s);
    if ( s.size() != 1 ) {
        ThrowError(eFormatError,
                   "\"" + s + "\": one char string expected");
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
    Expect('{', true);
    long im = ReadLong();
    Expect(',', true);
    unsigned base = ReadUInt();
    Expect(',', true);
    int ic = ReadInt();
    Expect('}', true);
    if ( base != 2 && base != 10 )
        ThrowError(eFormatError, "illegal REAL base (must be 2 or 10)");

    if ( base == 10 ) {     /* range checking only on base 10, for doubles */
    	if ( ic > DBL_MAX_10_EXP )   /* exponent too big */
    		return im < 0 ? -DBL_MAX: DBL_MAX;
    	else if ( ic < DBL_MIN_10_EXP )  /* exponent too small */
    		return 0;
    }

	return im * pow(double(base), ic);
}

void CObjectIStreamAsn::ReadString(string& s)
{
    s.erase();
    Expect('\"', true);
    char c;
    for ( ;; ) {
        c = GetChar();
        switch ( c ) {
        case '\r':
        case '\n':
            SkipEndOfLine(c);
            continue;
        case '\"':
            if ( GetChar('\"') ) {
                // double quote -> one quote
                s += '\"';
            }
            else {
                // end of string
                return;
            }
            continue;
        default:
            if ( c < ' ' && c >= 0 ) {
                ThrowError(eFormatError,
                           "bad char in string: " + NStr::IntToString(c));
            }
            else {
                s += c;
            }
            continue;
        }
    }
}

CObjectIStreamAsn::TIndex CObjectIStreamAsn::ReadIndex(void)
{
    return ReadUInt();
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

void CObjectIStreamAsn::StartMember(Member& m, const CMemberId& member)
{
    size_t size = ReadId();
    if ( size != member.GetName().size() ||
         memcmp(GetMark(), member.GetName().data(), size) != 0 ) {
        ThrowError(eFormatError,
                   "\"" + string(GetMark(), size) + "\": expected " +
                   member.GetName());
    }
    SetIndex(m, 0, member);
}

void CObjectIStreamAsn::StartMember(Member& m, const CMembers& members)
{
    size_t size = ReadId();
    if ( size == 0 )
        ThrowError(eFormatError, "member id expected");
    TMemberIndex index = members.FindMember(string(GetMark(), size));
    if ( index < 0 ) {
        ThrowError(eFormatError,
                   "\"" + string(GetMark(), size) + "\": unexpected member");
    }
    SetIndex(m, index, members.GetMemberId(index));
}

void CObjectIStreamAsn::StartMember(Member& m, LastMember& lastMember)
{
    size_t size = ReadId();
    if ( size == 0 )
        ThrowError(eFormatError, "member id expected");
    TMemberIndex index =
        lastMember.GetMembers().FindMember(string(GetMark(), size).c_str(),
                                           lastMember.GetIndex());
    if ( index < 0 ) {
        string message =
            "\"" + string(GetMark(), size) + "\": unexpected member, should be one of: ";
        iterate ( CMembers::TMembers, i, lastMember.GetMembers().GetMembers() ) {
            message += '\"' + i->ToString() + "\" ";
        }
        ThrowError(eFormatError, message);
    }
    SetIndex(lastMember, index);
    SetIndex(m, index, lastMember.GetMembers().GetMemberId(index));
}

void CObjectIStreamAsn::Begin(ByteBlock& )
{
	Expect('\'', true);
}

int CObjectIStreamAsn::GetHexChar(void)
{
    for ( ;; ) {
        char c = GetChar();
        if ( c >= '0' && c <= '9' ) {
            return c - '0';
        }
        else if ( c >= 'A' && c <= 'Z' ) {
            return c - 'A' + 10;
        }
        else if ( c >= 'a' && c <= 'z' ) {
            return c - 'a' + 10;
        }
        switch ( c ) {
        case '\'':
            return -1;
        case '\r':
        case '\n':
            SkipEndOfLine(c);
            break;
        default:
            UngetChar();
            ThrowError(eFormatError,
                       string("bad char in octet string: '") + c + "'");
        }
    }
}

size_t CObjectIStreamAsn::ReadBytes(const ByteBlock& block,
                                    char* dst, size_t length)
{
	size_t count = 0;
	while ( length-- > 0 ) {
        int c1 = GetHexChar();
        if ( c1 < 0 ) {
            SetBlockLength(const_cast<ByteBlock&>(block), count);
            return count;
        }
        int c2 = GetHexChar();
        if ( c2 < 0 ) {
            *dst++ = c1 << 4;
            count++;
            SetBlockLength(const_cast<ByteBlock&>(block), count);
            return count;
        }
        else {
            *dst++ = (c1 << 4) | c2;
            count++;
        }
	}
	return count;
}

void CObjectIStreamAsn::End(const ByteBlock& )
{
	Expect('H', true);
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
        UngetChar();
        return eThisPointer;
    case ':':
        return eOtherPointer;
    default:
        UngetChar();
        return eThisPointer;
    }
}

CObjectIStream::TIndex CObjectIStreamAsn::ReadObjectPointer(void)
{
    return ReadIndex();
}

string CObjectIStreamAsn::ReadOtherPointer(void)
{
    size_t size = ReadId();
    if ( size == 0 )
        ThrowError(eFormatError, "type name expected");
    return string(GetMark(), size);
}

bool CObjectIStreamAsn::HaveMemberSuffix(void)
{
    return GetChar('.', true);
}

CObjectIStreamAsn::TMemberIndex
CObjectIStreamAsn::ReadMemberSuffix(const CMembers& members)
{
    size_t size = ReadId();
    TMemberIndex index = members.FindMember(string(GetMark(), size));
    if ( index < 0 ) {
        ThrowError(eFormatError,
                   "member not found: " + string(GetMark(), size));
    }
    return index;
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
                UngetChar();
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
                UngetChar();
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
    for ( ;; ) {
        // scan end of oectet string
        if ( GetChar('\'', true) ) {
            // ' -> end
            // get type symbol: H or B
            Expect('H', 'B', true);
            return;
        }
    }
}

void CObjectIStreamAsn::SkipString(void)
{
    for ( ;; ) {
        // scan end of string
        if ( GetChar('\"', true) ) {
            // " -> possible and of string
            if ( GetChar('\"') ) {
                // double "" not an end
            }
            else {
                // end of string
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
    return count + m_Input.ReadLine(data, length);
}
#endif

END_NCBI_SCOPE
