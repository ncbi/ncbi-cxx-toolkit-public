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
* Revision 1.33  2000/02/17 20:02:45  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.32  2000/02/01 21:47:23  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Added buffering to CObjectIStreamAsn.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
*
* Revision 1.31  2000/01/11 14:16:46  vasilche
* Fixed pow ambiguity.
*
* Revision 1.30  2000/01/10 19:46:41  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.29  1999/12/17 19:05:04  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.28  1999/10/25 20:19:51  vasilche
* Fixed strings representation in text ASN.1 files.
*
* Revision 1.27  1999/10/04 16:22:18  vasilche
* Fixed bug with old ASN.1 structures.
*
* Revision 1.26  1999/09/24 18:19:19  vasilche
* Removed dependency on NCBI toolkit.
*
* Revision 1.25  1999/09/23 21:16:08  vasilche
* Removed dependance on asn.h
*
* Revision 1.24  1999/09/23 20:25:04  vasilche
* Added support HAVE_NCBI_C
*
* Revision 1.23  1999/09/23 18:57:01  vasilche
* Fixed bugs with overloaded methods in objistr*.hpp & objostr*.hpp
*
* Revision 1.22  1999/08/13 20:22:58  vasilche
* Fixed lot of bugs in datatool
*
* Revision 1.21  1999/08/13 15:53:52  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.20  1999/07/22 19:48:57  vasilche
* Reversed hack with embedding old ASN.1 output to new.
*
* Revision 1.19  1999/07/22 19:40:57  vasilche
* Fixed bug with complex object graphs (pointers to members of other objects).
*
* Revision 1.18  1999/07/22 17:33:56  vasilche
* Unified reading/writing of objects in all three formats.
*
* Revision 1.17  1999/07/21 14:20:07  vasilche
* Added serialization of bool.
*
* Revision 1.16  1999/07/20 18:23:12  vasilche
* Added interface to old ASN.1 routines.
* Added fixed choice of subclasses to use for pointers.
*
* Revision 1.15  1999/07/19 15:50:36  vasilche
* Added interface to old ASN.1 routines.
* Added naming of key/value in STL map.
*
* Revision 1.14  1999/07/15 19:35:30  vasilche
* Implemented map<K, V>.
* Changed ASN.1 text formatting.
*
* Revision 1.13  1999/07/15 16:54:49  vasilche
* Implemented vector<X> & vector<char> as special case.
*
* Revision 1.12  1999/07/14 18:58:10  vasilche
* Fixed ASN.1 types/field naming.
*
* Revision 1.11  1999/07/13 20:18:20  vasilche
* Changed types naming.
*
* Revision 1.10  1999/07/09 16:32:54  vasilche
* Added OCTET STRING write/read.
*
* Revision 1.9  1999/07/07 21:15:03  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.8  1999/07/02 21:31:59  vasilche
* Implemented reading from ASN.1 binary format.
*
* Revision 1.7  1999/07/01 17:55:34  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.6  1999/06/30 16:05:00  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.5  1999/06/24 14:44:58  vasilche
* Added binary ASN.1 output.
*
* Revision 1.4  1999/06/18 16:26:49  vasilche
* Fixed bug with unget() in MSVS
*
* Revision 1.3  1999/06/17 20:42:07  vasilche
* Fixed storing/loading of pointers.
*
* Revision 1.2  1999/06/16 20:35:34  vasilche
* Cleaned processing of blocks of data.
* Added input from ASN.1 text format.
*
* Revision 1.1  1999/06/15 16:19:51  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.5  1999/06/10 21:06:49  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.4  1999/06/07 20:10:03  vasilche
* Avoid using of numeric_limits.
*
* Revision 1.3  1999/06/07 19:30:27  vasilche
* More bug fixes
*
* Revision 1.2  1999/06/04 20:51:47  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:55  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objostrasn.hpp>
#include <serial/memberid.hpp>
#include <serial/enumerated.hpp>
#include <serial/memberlist.hpp>
#include <math.h>
#if HAVE_WINDOWS_H
// In MSVC limits.h doesn't define FLT_MIN & FLT_MAX
# include <float.h>
#endif
#if HAVE_NCBI_C
# include <asn.h>
#endif

BEGIN_NCBI_SCOPE

CObjectOStreamAsn::CObjectOStreamAsn(CNcbiOstream& out)
    : m_Output(out), m_Ident(0)
{
}

CObjectOStreamAsn::~CObjectOStreamAsn(void)
{
}

void CObjectOStreamAsn::WriteTypeName(const string& name)
{
    if ( m_Ident == 0 ) {
        WriteId(name);
        m_Output << " ::= ";
    }
}

bool CObjectOStreamAsn::WriteEnum(const CEnumeratedTypeValues& values,
                                  long value)
{
    const string& name = values.FindName(value, values.IsInteger());
    if ( !name.empty() ) {
        WriteId(name);
        return true;
    }
    return false;
}

inline
void CObjectOStreamAsn::WriteEscapedChar(char c)
{
    if ( c == '"' ) {
        m_Output << "\"\"";
    }
    else if ( c >= 0 && c < ' ' ) {
        THROW1_TRACE(runtime_error,
                     "bad char in string: " + NStr::IntToString(c));
    }
    else {
        m_Output << c;
    }
}

void CObjectOStreamAsn::WriteBool(bool data)
{
    m_Output << (data? "TRUE": "FALSE");
}

void CObjectOStreamAsn::WriteChar(char data)
{
    m_Output << '\'' << data << '\'';
}

void CObjectOStreamAsn::WriteInt(int data)
{
    m_Output << data;
}

void CObjectOStreamAsn::WriteUInt(unsigned data)
{
    m_Output << data;
}

void CObjectOStreamAsn::WriteLong(long data)
{
    m_Output << data;
}

void CObjectOStreamAsn::WriteULong(unsigned long data)
{
    m_Output << data;
}

void CObjectOStreamAsn::WriteDouble(double data)
{
	if ( data == 0.0 ) {
        m_Output << "{ 0, 10, 0 }";
        return;
	}

    bool minus;
    if ( data < 0.0 ) {
        minus = true;
        data = -data;
    }
    else {
        minus = false;
    }

    double thelog = log10(data);
    int characteristic;
    if ( thelog >= 0.0 )
        characteristic = 8 - int(thelog);/* give it 9 significant digits */
    else
        characteristic = 8 + int(ceil(-thelog));
    
    double mantissa = data * pow(double(10), characteristic);
    int ic = -characteristic; /* reverse direction */
    
    long im;
    if ( mantissa >= LONG_MAX )
        im = LONG_MAX;
    else
        im = long(mantissa);
    
    /* strip trailing 0 */
    while ( im % 10 == 0 ) {
        im /= 10;
        ic++;
    }
    
    if (minus)
        im = -im;
	m_Output << "{ " << im << ", 10, " << ic << " }";
}

void CObjectOStreamAsn::WriteNull(void)
{
    m_Output << "NULL";
}

void CObjectOStreamAsn::WriteString(const string& str)
{
    m_Output << '\"';
    size_t col = 20;
    for ( string::const_iterator i = str.begin(); i != str.end(); ++i ) {
        char c = *i;
        if ( col >= 72 && c == ' ' || col >= 78 ) {
            m_Output << '\n';
            col = 0;
        }
        WriteEscapedChar(c);
        ++col;
    }
    m_Output << '\"';
}

void CObjectOStreamAsn::WriteCString(const char* str)
{
    if ( str == 0 ) {
        WriteNull();
    }
    else {
	    m_Output << '\"';
		while ( *str ) {
			WriteEscapedChar(*str++);
		}
		m_Output << '\"';
    }
}

void CObjectOStreamAsn::WriteId(const string& str)
{
	if ( str.find(' ') != NPOS || str.find('<') != NPOS ||
         str.find(':') != NPOS ) {
		m_Output << '[' << str << ']';
	}
	else {
		m_Output << str;
	}
}

void CObjectOStreamAsn::WriteMemberSuffix(const CMemberId& id)
{
    m_Output << '.' << id.GetName();
}

void CObjectOStreamAsn::WriteNullPointer(void)
{
    WriteNull();
}

void CObjectOStreamAsn::WriteObjectReference(TIndex index)
{
    m_Output << '@' << index;
}

void CObjectOStreamAsn::WriteOther(TConstObjectPtr object, TTypeInfo typeInfo)
{
    m_Output << ": ";
    WriteId(typeInfo->GetName());
    m_Output << ' ';
    WriteExternalObject(object, typeInfo);
}

void CObjectOStreamAsn::WriteNewLine(void)
{
    m_Output << '\n';
    for ( int i = 0; i < m_Ident; ++i )
        m_Output << "  ";
}

void CObjectOStreamAsn::VBegin(Block& )
{
    m_Output << '{';
    ++m_Ident;
}

void CObjectOStreamAsn::VNext(const Block& block)
{
    if ( !block.First() ) {
        m_Output << ',';
    }
    WriteNewLine();
}

void CObjectOStreamAsn::VEnd(const Block& )
{
    --m_Ident;
    WriteNewLine();
    m_Output << '}';
}

void CObjectOStreamAsn::StartMember(Member& , const CMemberId& id)
{
    m_Output << id.GetName() << ' ';
}

void CObjectOStreamAsn::EndMember(const Member& )
{
}

void CObjectOStreamAsn::Begin(const ByteBlock& )
{
	m_Output << '\'';
}

static const char* const HEX = "0123456789ABCDEF";

void CObjectOStreamAsn::WriteBytes(const ByteBlock& ,
                                   const char* bytes, size_t length)
{
    size_t col = 20;
	while ( length-- > 0 ) {
		char c = *bytes++;
        if ( col >= 76 ) {
            m_Output << '\n';
            col = 0;
        }
		m_Output << HEX[(c >> 4) & 0xf] << HEX[c & 0xf];
        col += 2;
	}
}

void CObjectOStreamAsn::End(const ByteBlock& )
{
	m_Output << "\'H";
}

#if HAVE_NCBI_C
unsigned CObjectOStreamAsn::GetAsnFlags(void)
{
    return ASNIO_TEXT;
}

void CObjectOStreamAsn::AsnOpen(AsnIo& asn)
{
    asn.m_Count = 0;
    size_t indent = asn->indent_level = m_Ident;
    size_t max_indent = asn->max_indent;
    if ( indent >= max_indent ) {
        Boolean* tmp = asn->first;
        asn->first = (BoolPtr) MemNew((sizeof(Boolean) * (indent + 10)));
        MemCopy(asn->first, tmp, (size_t)(sizeof(Boolean) * max_indent));
        MemFree(tmp);
        asn->max_indent = indent;
    }
}

void CObjectOStreamAsn::AsnWrite(AsnIo& asn, const char* data, size_t length)
{
    if ( asn.m_Count == 0 ) {
        // dirty hack to skip structure name with '::='
        const char* p = (const char*)memchr(data, ':', length);
        if ( p && p[1] == ':' && p[2] == '=' ) {
            // check type name
            const char* beg = data;
            const char* end = p;
            while ( beg < end && isspace(beg[0]) )
                beg++;
            while ( end > beg && isspace(end[-1]) )
                end--;
            if ( string(beg, end) != asn.GetRootTypeName() ) {
                ERR_POST("AsnWrite: wrong ASN.1 type name: must be \"" <<
                         asn.GetRootTypeName() << "\"");
            }
            // skip header
            size_t skip = p + 3 - data;
            _TRACE(Warning <<
                   "AsnWrite: skipping \"" << string(data, skip) << "\"");
            data += skip;
            length -= skip;
        }
        else {
            ERR_POST("AsnWrite: no \"Asn-Type ::=\" header");
        }
        asn.m_Count = 1;
    }
    if ( !m_Output.write(data, length) )
        THROW1_TRACE(runtime_error, "write fault");
}
#endif

END_NCBI_SCOPE
