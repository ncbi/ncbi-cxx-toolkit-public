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
#include <asn.h>

BEGIN_NCBI_SCOPE

CObjectOStreamAsn::CObjectOStreamAsn(CNcbiOstream& out)
    : m_Output(out), m_Ident(0)
{
}

CObjectOStreamAsn::~CObjectOStreamAsn(void)
{
}

void CObjectOStreamAsn::Write(TConstObjectPtr object, TTypeInfo typeInfo)
{
    if ( m_Ident == 0 ) {
        WriteOtherTypeReference(typeInfo);
    }
    CObjectOStream::Write(object, typeInfo);
}

void CObjectOStreamAsn::WriteStd(const bool& data)
{
    m_Output << (data? "TRUE": "FALSE");
}

void CObjectOStreamAsn::WriteStd(const char& data)
{
    m_Output << '\'';
    WriteEscapedChar(data);
    m_Output << '\'';
}

void CObjectOStreamAsn::WriteEscapedChar(char c)
{
    switch ( c ) {
    case '\'':
    case '\"':
    case '\\':
        m_Output << '\\' << c;
        break;
    case '\t':
        m_Output << "\\t";
        break;
    case '\r':
        m_Output << "\\r";
        break;
    case '\n':
        m_Output << "\\n";
        break;
    default:
        if ( c >= ' ' && c < 0x7f ) {
            m_Output << c;
        }
        else {
            // octal escape sequence
            m_Output << '\\'
                     << ('0' + ((c >> 6) & 3))
                     << ('0' + ((c >> 3) & 7))
                     << ('0' + (c & 7));
        }
        break;
    }
}

void CObjectOStreamAsn::WriteStd(const unsigned char& data)
{
    m_Output << unsigned(data);
}

void CObjectOStreamAsn::WriteStd(const signed char& data)
{
    m_Output << int(data);
}

void CObjectOStreamAsn::WriteStd(const short& data)
{
    m_Output << data;
}

void CObjectOStreamAsn::WriteStd(const unsigned short& data)
{
    m_Output << data;
}

void CObjectOStreamAsn::WriteStd(const int& data)
{
    m_Output << data;
}

void CObjectOStreamAsn::WriteStd(const unsigned int& data)
{
    m_Output << data;
}

void CObjectOStreamAsn::WriteStd(const long& data)
{
    m_Output << data;
}

void CObjectOStreamAsn::WriteStd(const unsigned long& data)
{
    m_Output << data;
}

void CObjectOStreamAsn::WriteStd(const float& data)
{
    m_Output << data;
}

void CObjectOStreamAsn::WriteStd(const double& data)
{
    m_Output << data;
}

void CObjectOStreamAsn::WriteNull(void)
{
    m_Output << "NULL";
}

void CObjectOStreamAsn::WriteString(const string& str)
{
    m_Output << '\"';
    for ( string::const_iterator i = str.begin(); i != str.end(); ++i ) {
        WriteEscapedChar(*i);
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

void CObjectOStreamAsn::WriteMemberSuffix(COObjectInfo& info)
{
    string memberName = info.GetMemberId().GetName();
    info.ToContainerObject();
    if ( info.IsMember() ) {
        WriteMemberSuffix(info);
    }
    m_Output << '.' << memberName;
}

void CObjectOStreamAsn::WriteNullPointer(void)
{
    WriteNull();
}

void CObjectOStreamAsn::WriteObjectReference(TIndex index)
{
    m_Output << '@' << index;
}

void CObjectOStreamAsn::WriteOtherTypeReference(TTypeInfo typeInfo)
{
    WriteId(typeInfo->GetName());
    m_Output << " ::= ";
}

void CObjectOStreamAsn::WriteNewLine(void)
{
    m_Output << endl;
    for ( int i = 0; i < m_Ident; ++i )
        m_Output << "  ";
}

void CObjectOStreamAsn::VBegin(Block& )
{
    m_Output << "{";
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

void CObjectOStreamAsn::WriteBytes(const ByteBlock& , const char* bytes, size_t length)
{
	while ( length-- > 0 ) {
		char c = *bytes++;
		m_Output << HEX[(c >> 4) & 0xf] << HEX[c & 0xf];
	}
}

void CObjectOStreamAsn::End(const ByteBlock& )
{
	m_Output << "\'H";
}

unsigned CObjectOStreamAsn::GetAsnFlags(void)
{
    return ASNIO_TEXT;
}

void CObjectOStreamAsn::AsnOpen(AsnIo& asn)
{
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

void CObjectOStreamAsn::AsnWrite(AsnIo& , const char* data, size_t length)
{
    if ( !m_Output.write(data, length) )
        THROW1_TRACE(runtime_error, "write fault");
}

END_NCBI_SCOPE
