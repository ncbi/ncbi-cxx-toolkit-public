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
* Revision 1.50  2000/07/03 20:39:55  vasilche
* Fixed comments.
*
* Revision 1.49  2000/07/03 18:42:45  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.48  2000/06/16 16:31:19  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.47  2000/06/07 19:45:59  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.46  2000/06/01 19:07:03  vasilche
* Added parsing of XML data.
*
* Revision 1.45  2000/05/24 20:08:47  vasilche
* Implemented XML dump.
*
* Revision 1.44  2000/05/09 16:38:39  vasilche
* CObject::GetTypeInfo now moved to CObjectGetTypeInfo::GetTypeInfo to reduce possible errors.
* Added write context to CObjectOStream.
* Inlined most of methods of helping class Member, Block, ByteBlock etc.
*
* Revision 1.43  2000/05/03 14:38:14  vasilche
* SERIAL: added support for delayed reading to generated classes.
* DATATOOL: added code generation for delayed reading.
*
* Revision 1.42  2000/04/28 16:58:12  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* Revision 1.41  2000/04/13 14:50:27  vasilche
* Added CObjectIStream::Open() and CObjectOStream::Open() for easier use.
*
* Revision 1.40  2000/03/29 15:55:28  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* Revision 1.39  2000/03/14 14:42:31  vasilche
* Fixed error reporting.
*
* Revision 1.38  2000/03/07 14:06:22  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
* Added generation of reference counted objects.
*
* Revision 1.37  2000/02/17 20:02:44  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.36  2000/02/11 17:10:24  vasilche
* Optimized text parsing.
*
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
#include <serial/enumvalues.hpp>
#include <serial/memberlist.hpp>
#include <serial/classinfo.hpp>
#include <math.h>
#if !defined(DBL_MAX_10_EXP) || !defined(FLT_MAX)
# include <float.h>
#endif
#if HAVE_NCBI_C
# include <asn.h>
#endif

BEGIN_NCBI_SCOPE


CObjectIStream* CreateObjectIStreamAsn(void)
{
    return new CObjectIStreamAsn();
}

CObjectIStreamAsn::CObjectIStreamAsn(void)
{
}

CObjectIStreamAsn::CObjectIStreamAsn(CNcbiIstream& in)
{
    Open(in);
}

CObjectIStreamAsn::CObjectIStreamAsn(CNcbiIstream& in, bool deleteIn)
{
    Open(in, deleteIn);
}

ESerialDataFormat CObjectIStreamAsn::GetDataFormat(void) const
{
    return eSerial_AsnText;
}

string CObjectIStreamAsn::GetPosition(void) const
{
    return "line "+NStr::UIntToString(m_Input.GetLine());
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
char CObjectIStreamAsn::PeekChar(void)
{
    return m_Input.PeekChar();
}

inline
void CObjectIStreamAsn::SkipEndOfLine(char c)
{
    m_Input.SkipEndOfLine(c);
}

inline
char CObjectIStreamAsn::SkipWhiteSpaceAndGetChar(void)
{
    char c = SkipWhiteSpace();
    m_Input.SkipChar();
    return c;
}

inline
char CObjectIStreamAsn::GetChar(bool skipWhiteSpace)
{
    return skipWhiteSpace? SkipWhiteSpaceAndGetChar(): m_Input.GetChar();
}

inline
char CObjectIStreamAsn::PeekChar(bool skipWhiteSpace)
{
    return skipWhiteSpace? SkipWhiteSpace(): m_Input.PeekChar();
}

inline
bool CObjectIStreamAsn::GetChar(char expect, bool skipWhiteSpace)
{
    if ( PeekChar(skipWhiteSpace) != expect ) {
        return false;
    }
    m_Input.SkipChar();
    return true;
}

void CObjectIStreamAsn::Expect(char expect, bool skipWhiteSpace)
{
    if ( !GetChar(expect, skipWhiteSpace) ) {
        string msg("\'");
        msg += expect;
        msg += "' expected";
        ThrowError(eFormatError, msg);
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
    m_Input.UngetChar(c);
    string msg("\'");
    msg += choiceTrue;
    msg += "' or '";
    msg += choiceFalse;
    msg += "' expected";
    ThrowError(eFormatError, msg);
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

char CObjectIStreamAsn::SkipWhiteSpace(void)
{
    for ( ;; ) {
		char c = m_Input.SkipSpaces();
        switch ( c ) {
        case '\t':
            m_Input.SkipChar();
            continue;
        case '\r':
        case '\n':
            m_Input.SkipChar();
            SkipEndOfLine(c);
            continue;
        case '-':
            // check for comments
            if ( m_Input.PeekChar(1) != '-' ) {
                return '-';
            }
            m_Input.SkipChars(2);
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
                switch ( c ) {
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
    catch ( CSerialEofException& /* ignored */ ) {
        return;
    }
}

CLightString CObjectIStreamAsn::ScanEndOfId(bool isId)
{
    if ( isId ) {
        for ( size_t i = 1; ; ++i ) {
            char c = m_Input.PeekCharNoEOF(i);
            if ( !IdChar(c) &&
                 (c != '-' || !IdChar(m_Input.PeekChar(i + 1))) ) {
                const char* ptr = m_Input.GetCurrentPos();
                m_Input.SkipChars(i);
                return CLightString(ptr, i);
            }
        }
    }
    return CLightString();
}

CLightString CObjectIStreamAsn::ReadTypeId(char c)
{
    if ( c == '[' ) {
        for ( size_t i = 1; ; ++i ) {
            switch ( m_Input.PeekChar(i) ) {
            case '\r':
            case '\n':
                ThrowError(eFormatError, "end of line: expected ']'");
                break;
            case ']':
                {
                    const char* ptr = m_Input.GetCurrentPos();
                    m_Input.SkipChars(i);
                    return CLightString(ptr + 1, i - 2);
                }
            }
        }
    }
	else {
        return ScanEndOfId(FirstIdChar(c));
    }
}

inline
CLightString CObjectIStreamAsn::ReadUCaseId(char c)
{
    return ScanEndOfId(isupper(c));
}

inline
CLightString CObjectIStreamAsn::ReadLCaseId(char c)
{
    return ScanEndOfId(islower(c));
}

inline
CLightString CObjectIStreamAsn::ReadMemberId(char c)
{
    return ScanEndOfId(islower(c));
}

void CObjectIStreamAsn::ReadNull(void)
{
    ExpectString("NULL", true);
}

string CObjectIStreamAsn::ReadTypeName()
{
    CLightString id = ReadTypeId(SkipWhiteSpace());
    string s(id);
    ExpectString("::=", true);
    return s;
}

pair<long, bool>
CObjectIStreamAsn::ReadEnum(const CEnumeratedTypeValues& values)
{
    // not integer
    CLightString id = ReadLCaseId(SkipWhiteSpace());
    if ( !id.Empty() ) {
        // enum element by name
        return make_pair(values.FindValue(id), true);
    }
    if ( values.IsInteger() ) {
        // allow any integer
        return make_pair(0l, false);
    }
    
    // enum element by value
    long value = m_Input.GetLong();
    values.FindName(value, false);
    return make_pair(value, true);
}

bool CObjectIStreamAsn::ReadBool(void)
{
    CLightString id = ReadUCaseId(SkipWhiteSpace());
    if ( id == "TRUE" )
        return true;
    if ( id == "FALSE" )
        return false;

    ThrowError(eFormatError,
               "\""+string(id)+"\": TRUE or FALSE expected");
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
    SkipWhiteSpace();
    return m_Input.GetInt();
}

unsigned CObjectIStreamAsn::ReadUInt(void)
{
    SkipWhiteSpace();
    return m_Input.GetUInt();
}

long CObjectIStreamAsn::ReadLong(void)
{
    SkipWhiteSpace();
    return m_Input.GetLong();
}

unsigned long CObjectIStreamAsn::ReadULong(void)
{
    SkipWhiteSpace();
    return m_Input.GetULong();
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
    Expect('\"', true);
    s.erase();
    size_t i = 0;
    for (;;) {
        char c = m_Input.PeekChar(i);
        switch ( c ) {
        case '\r':
        case '\n':
            // flush string
            s.append(m_Input.GetCurrentPos(), i);
            m_Input.SkipChars(i + 1);
            i = 0;
            // skip end of line
            SkipEndOfLine(c);
            break;
        case '\"':
            if ( m_Input.PeekCharNoEOF(i + 1) == '\"' ) {
                // double quote -> one quote
                s.append(m_Input.GetCurrentPos(), i + 1);
                m_Input.SkipChars(i + 2);
                i = 0;
            }
            else {
                // end of string
                s.append(m_Input.GetCurrentPos(), i);
                m_Input.SkipChars(i + 1);
                return;
            }
            break;
        default:
            if ( c < ' ' && c >= 0 ) {
                ThrowError(eFormatError,
                           "bad char in string: " + NStr::IntToString(c));
            }
            else {
                ++i;
                if ( i == 128 ) {
                    // too long string -> flush it
                    s.append(m_Input.GetCurrentPos(), i);
                    m_Input.SkipChars(i);
                    i = 0;
                }
            }
            break;
        }
    }
}

void CObjectIStreamAsn::SkipBool(void)
{
    switch ( SkipWhiteSpace() ) {
    case 'T':
        if ( m_Input.PeekCharNoEOF(1) == 'R' &&
             m_Input.PeekCharNoEOF(2) == 'U' &&
             m_Input.PeekCharNoEOF(3) == 'E' &&
             !IdChar(m_Input.PeekCharNoEOF(4)) ) {
            m_Input.SkipChars(4);
            return;
        }
        break;
    case 'F':
        if ( m_Input.PeekCharNoEOF(1) == 'A' &&
             m_Input.PeekCharNoEOF(2) == 'L' &&
             m_Input.PeekCharNoEOF(3) == 'S' &&
             m_Input.PeekCharNoEOF(4) == 'E' &&
             !IdChar(m_Input.PeekCharNoEOF(5)) ) {
            m_Input.SkipChars(5);
            return;
        }
        break;
    }
    ThrowError(eFormatError, "TRUE or FALSE expected");
}

void CObjectIStreamAsn::SkipChar(void)
{
    // TODO: check string length to be 1
    SkipString();
}

void CObjectIStreamAsn::SkipSNumber(void)
{
    size_t i;
    char c = SkipWhiteSpace();
    switch ( c ) {
    case '-':
    case '+':
        c = m_Input.PeekChar(1);
        // next char
        i = 2;
        break;
    default:
        // next char
        i = 1;
        break;
    }
    if ( c < '0' || c > '9' ) {
        ThrowError(eFormatError, "bad number");
    }
    while ( (c = m_Input.PeekChar(i)) >= '0' && c <= '9' ) {
        ++i;
    }
    m_Input.SkipChars(i);
}

void CObjectIStreamAsn::SkipUNumber(void)
{
    size_t i;
    char c = SkipWhiteSpace();
    switch ( c ) {
    case '+':
        c = m_Input.PeekChar(1);
        // next char
        i = 2;
        break;
    default:
        // next char
        i = 1;
        break;
    }
    if ( c < '0' || c > '9' ) {
        ThrowError(eFormatError, "bad number");
    }
    while ( (c = m_Input.PeekCharNoEOF(i)) >= '0' && c <= '9' ) {
        ++i;
    }
    m_Input.SkipChars(i);
}

void CObjectIStreamAsn::SkipFNumber(void)
{
    Expect('{', true);
    SkipSNumber();
    Expect(',', true);
    unsigned base = ReadUInt();
    Expect(',', true);
    SkipSNumber();
    Expect('}', true);
    if ( base != 2 && base != 10 )
        ThrowError(eFormatError, "illegal REAL base (must be 2 or 10)");
}

void CObjectIStreamAsn::SkipString(void)
{
    Expect('\"', true);
    size_t i = 0;
    for (;;) {
        char c = m_Input.PeekChar(i);
        switch ( c ) {
        case '\r':
        case '\n':
            // flush string
            m_Input.SkipChars(i + 1);
            i = 0;
            // skip end of line
            SkipEndOfLine(c);
            break;
        case '\"':
            if ( m_Input.PeekChar(i + 1) == '\"' ) {
                // double quote -> one quote
                m_Input.SkipChars(i + 2);
                i = 0;
            }
            else {
                // end of string
                m_Input.SkipChars(i + 1);
                return;
            }
            break;
        default:
            if ( c < ' ' && c >= 0 ) {
                ThrowError(eFormatError,
                           "bad char in string: " + NStr::IntToString(c));
            }
            else {
                ++i;
                if ( i == 128 ) {
                    // too long string -> flush it
                    m_Input.SkipChars(i);
                    i = 0;
                }
            }
            break;
        }
    }
}

void CObjectIStreamAsn::SkipNull(void)
{
    if ( SkipWhiteSpace() == 'N' &&
         m_Input.PeekCharNoEOF(1) == 'U' &&
         m_Input.PeekCharNoEOF(2) == 'L' &&
         m_Input.PeekCharNoEOF(3) == 'L' &&
         !IdChar(m_Input.PeekCharNoEOF(4)) ) {
        m_Input.SkipChars(4);
        return;
    }
    ThrowError(eFormatError, "NULL expected");
}

void CObjectIStreamAsn::SkipByteBlock(void)
{
	Expect('\'', true);
    for ( ;; ) {
        char c = GetChar();
        if ( c >= '0' && c <= '9' ) {
            continue;
        }
        else if ( c >= 'A' && c <= 'Z' ) {
            continue;
        }
        else if ( c >= 'a' && c <= 'z' ) {
            continue;
        }
        else if ( c == '\'' ) {
            break;
        }
        else if ( c == '\r' || c == '\n' ) {
            SkipEndOfLine(c);
        }
        else {
            m_Input.UngetChar(c);
            ThrowError(eFormatError, "bad char in octet string");
        }
    }
	Expect('H', true);
}

void CObjectIStreamAsn::ReadArray(CObjectArrayReader& reader,
                                  TTypeInfo /*arrayType*/,
                                  bool /*randomOrder*/,
                                  TTypeInfo /*elementType*/)
{
    Expect('{', true);
    
    char c = SkipWhiteSpace();
    if ( c != '}' ) {
        CObjectStackArrayElement e(*this, false);
        for ( ;; ) {
            reader.ReadElement(*this);
            c = SkipWhiteSpace();
            if ( c == ',' ) {
                // next element
                m_Input.SkipChar();
            }
            else {
                // no more elements
                break;
            }
        }
        e.End();
        if ( c != '}' )
            ThrowError(eFormatError, "'}' expected");
    }
    m_Input.SkipChar(); // '}'
}

void CObjectIStreamAsn::BeginClass(CObjectStackClass& /*cls*/)
{
    Expect('{', true);
}

void CObjectIStreamAsn::EndClass(CObjectStackClass& cls)
{
    Expect('}');
    cls.End();
}

void CObjectIStreamAsn::UnexpectedMember(const CLightString& id,
                                         const CMembers& members)
{
    string message =
        "\""+string(id)+"\": unexpected member, should be one of: ";
    iterate ( CMembers::TMembers, i, members.GetMembers() ) {
        message += '\"' + i->ToString() + "\" ";
    }
    ThrowError(eFormatError, message);
}

CObjectIStreamAsn::TMemberIndex
CObjectIStreamAsn::BeginClassMember(CObjectStackClassMember& m,
                                    const CMembers& members)
{
    char c = SkipWhiteSpace();
    if ( m.GetClassFrame().IsEmpty() ) {
        if ( c == '}' )
            return -1;
        m.GetClassFrame().SetNonEmpty();
    }
    else {
        if ( c != ',' )
            return -1;
        m_Input.SkipChar();
        c = SkipWhiteSpace();
    }

    CLightString id = ReadMemberId(c);
    TMemberIndex index = members.FindMember(id);
    if ( index < 0 )
        UnexpectedMember(id, members);
    m.SetName(members.GetMemberId(index));
    m.Begin();
    return index;
}

CObjectIStreamAsn::TMemberIndex
CObjectIStreamAsn::BeginClassMember(CObjectStackClassMember& m,
                                    const CMembers& members,
                                    CClassMemberPosition& pos)
{
    char c = SkipWhiteSpace();
    if ( pos.GetLastIndex() < 0 ) {
        if ( c == '}' )
            return -1;
    }
    else {
        if ( c != ',' )
            return -1;
        m_Input.SkipChar();
        c = SkipWhiteSpace();
    }

    CLightString id = ReadMemberId(c);
    TMemberIndex index = members.FindMember(id, pos.GetLastIndex());
    if ( index < 0 )
        UnexpectedMember(id, members);
    m.SetName(members.GetMemberId(index));
    m.Begin();
    pos.SetLastIndex(index);
    return index;
}

void CObjectIStreamAsn::ReadClassRandom(CObjectClassReader& reader,
                                        const CClassTypeInfo* /*classInfo*/,
                                        const CMembersInfo& members)
{
    Expect('{', true);

    size_t memberCount = members.GetMembersCount();

    char c = SkipWhiteSpace();
    if ( c == '}' ) {
        m_Input.SkipChar();
        // empty block
        // init all absent members
        for ( size_t index = 0; index < memberCount; ++index ) {
            reader.AssignMemberDefault(*this, members, index);
        }
    }
    else {
        vector<bool> read(memberCount);

        CObjectStackClassMember m(*this, false);

        for (;;) {
            CLightString id = ReadMemberId(c);
            TMemberIndex index = members.FindMember(id);
            if ( index < 0 )
                UnexpectedMember(id, members);
            m.SetName(members.GetMemberId(index));

            if ( read[index] )
                DuplicatedMember(members, index);
            read[index] = true;

            reader.ReadMember(*this, members, index);

            c = SkipWhiteSpace();
            if ( c == ',' ) {
                m_Input.SkipChar();
                c = SkipWhiteSpace();
            }
            else if ( c == '}' ) {
                // end of block
                m_Input.SkipChar();
                break;
            }
            else {
                ThrowError(eFormatError, "',' or '}' expected");
            }
        }
        
        m.End();

        // init all absent members
        for ( size_t index = 0; index < memberCount; ++index ) {
            if ( !read[index] ) {
                reader.AssignMemberDefault(*this, members, index);
            }
        }
    }
}

void CObjectIStreamAsn::ReadClassSequential(CObjectClassReader& reader,
                                            const CClassTypeInfo* /*classInfo*/,
                                            const CMembersInfo& members)
{
    Expect('{', true);

    TMemberIndex pos = -1;
    char c = SkipWhiteSpace();
    if ( c != '}' ) {
        CObjectStackClassMember m(*this, false);

        for (;;) {
            CLightString id = ReadMemberId(c);
            TMemberIndex index = members.FindMember(id, pos);
            if ( index < 0 )
                UnexpectedMember(id, members);
            m.SetName(members.GetMemberId(index));

            size_t end = index;
            for ( size_t i = pos + 1; i < end; ++i ) {
                // init missing member
                reader.AssignMemberDefault(*this, members, i);
            }
            pos = index;

            reader.ReadMember(*this, members, index);

            c = SkipWhiteSpace();
            if ( c == ',' ) {
                m_Input.SkipChar();
                c = SkipWhiteSpace();
            }
            else if ( c == '}' ) {
                // end of block
                break;
            }
            else {
                ThrowError(eFormatError, "',' or '}' expected");
            }
        }
        if ( c != '}' )
            ThrowError(eFormatError, "'}' expected");

        m.End();
    }
    m_Input.SkipChar();

    // init all absent members
    size_t end = members.GetMembersCount();
    for ( size_t i = pos + 1; i < end; ++i ) {
        reader.AssignMemberDefault(*this, members, i);
    }
}

void CObjectIStreamAsn::ReadChoice(CObjectChoiceReader& reader,
                                   TTypeInfo /*choiceInfo*/,
                                   const CMembersInfo& variants)
{
    CObjectStackChoiceVariant v(*this);
    CLightString id = ReadMemberId(SkipWhiteSpace());
    if ( id.Empty() )
        ThrowError(eFormatError, "choice variant id expected");
    TMemberIndex index = variants.FindMember(id);
    if ( index < 0 )
        UnexpectedMember(id, variants);
    v.SetName(variants.GetMemberId(index));
    reader.ReadChoiceVariant(*this, variants, index);
    v.End();
}

void CObjectIStreamAsn::BeginBytes(ByteBlock& )
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
            m_Input.UngetChar(c);
            ThrowError(eFormatError, "bad char in octet string");
        }
    }
}

size_t CObjectIStreamAsn::ReadBytes(ByteBlock& block,
                                    char* dst, size_t length)
{
	size_t count = 0;
	while ( length-- > 0 ) {
        int c1 = GetHexChar();
        if ( c1 < 0 ) {
            block.EndOfBlock();
            return count;
        }
        int c2 = GetHexChar();
        if ( c2 < 0 ) {
            *dst++ = c1 << 4;
            count++;
            block.EndOfBlock();
            return count;
        }
        else {
            *dst++ = (c1 << 4) | c2;
            count++;
        }
	}
	return count;
}

void CObjectIStreamAsn::EndBytes(const ByteBlock& )
{
	Expect('H');
}

CObjectIStream::EPointerType CObjectIStreamAsn::ReadPointerType(void)
{
    char c;
    switch ( (c = PeekChar(true)) ) {
    case 'N':
        m_Input.SkipChar();
        ExpectString("ULL");
        return eNullPointer;
    case '@':
        m_Input.SkipChar();
        return eObjectPointer;
    case ':':
        m_Input.SkipChar();
        return eOtherPointer;
    default:
        return eThisPointer;
    }
}

CObjectIStream::TObjectIndex CObjectIStreamAsn::ReadObjectPointer(void)
{
    return ReadInt();
}

string CObjectIStreamAsn::ReadOtherPointer(void)
{
    return ReadTypeId(SkipWhiteSpace());
}

void CObjectIStreamAsn::SkipValue()
{
    int blockLevel = 0;
    for ( ;; ) {
        char c = SkipWhiteSpace();
        switch ( c ) {
        case ':':
            m_Input.SkipChar();
            ReadTypeId(SkipWhiteSpace());
            break;
        case '\'':
            SkipByteBlock();
            break;
        case '\"':
            SkipString();
            break;
        case '{':
            m_Input.SkipChar();
            ++blockLevel;
            break;
        case '}':
            switch ( blockLevel ) {
            case 0:
                return;
            case 1:
                m_Input.SkipChar();
                return;
            default:
                m_Input.SkipChar();
                --blockLevel;
                break;
            }
            break;
        case ',':
            if ( blockLevel == 0 ) {
                return;
            }
            m_Input.SkipChar();
            break;
        default:
            m_Input.SkipChar();
            break;
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
    if ( asn.m_Count == 0 ) {
        // dirty hack to add structure name with '::='
        const string& name = asn.GetRootTypeName();
        SIZE_TYPE nameLength = name.size();
        count = nameLength + 3;
        if ( length < count ) {
            ThrowError(eFail, "buffer too small to put structure name");
        }
        memcpy(data, name.data(), nameLength);
        data[nameLength] = ':';
        data[nameLength + 1] = ':';
        data[nameLength + 2] = '=';
        data += count;
        length -= count;
        asn.m_Count = 1;
    }
    return count + m_Input.ReadLine(data, length);
}
#endif

END_NCBI_SCOPE
