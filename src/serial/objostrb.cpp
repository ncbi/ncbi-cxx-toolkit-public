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
* Revision 1.8  1999/06/24 14:45:00  vasilche
* Added binary ASN.1 output.
*
* Revision 1.7  1999/06/16 20:35:35  vasilche
* Cleaned processing of blocks of data.
* Added input from ASN.1 text format.
*
* Revision 1.6  1999/06/15 16:19:52  vasilche
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
#include <serial/objostrb.hpp>
#include <serial/objstrb.hpp>
#include <serial/member.hpp>

BEGIN_NCBI_SCOPE

CObjectOStreamBinary::CObjectOStreamBinary(CNcbiOstream& out)
    : m_Output(out)
{
}

CObjectOStreamBinary::~CObjectOStreamBinary(void)
{
}

inline
void CObjectOStreamBinary::WriteByte(TByte byte)
{
    m_Output.put(byte);
}

inline
void CObjectOStreamBinary::WriteBytes(const char* bytes, size_t size)
{
    m_Output.write(bytes, size);
}

inline
void CObjectOStreamBinary::WriteNull(void)
{
    WriteByte(CObjectStreamBinaryDefs::eNull);
}

template<class TYPE>
void WriteNumber(CObjectOStreamBinary* out, TYPE data)
{
    typedef CObjectOStreamBinary::TByte TByte;

    if ( !(TYPE(-1) < TYPE(0)) ) { // signed
        // unsigned number
        if ( data < 0x80 ) {
            // one byte: 0xxxxxxx
            out->WriteByte(TByte(data));
        }
        else if ( data < 0x4000 ) {
            // two bytes: 10xxxxxx xxxxxxxx
            out->WriteByte(TByte((data >> 8) | 0x80));
            out->WriteByte(TByte(data));
        }
        else if ( data < 0x200000 ) {
            // three bytes: 110xxxxx xxxxxxxx xxxxxxxx
            out->WriteByte(TByte((data >> 16) | 0xC0));
            out->WriteByte(TByte(data >> 8));
            out->WriteByte(TByte(data));
        }
        else if ( data < 0x10000000 ) {
            // four bytes: 1110xxxx xxxxxxxx xxxxxxxx xxxxxxxx
            out->WriteByte(TByte((data >> 24) | 0xE0));
            out->WriteByte(TByte(data >> 16));
            out->WriteByte(TByte(data >> 8));
            out->WriteByte(TByte(data));
        }
        else {
            // 1111xxxx (xxxxxxxx)*
            out->WriteByte(TByte((sizeof(TYPE) - 4) | 0xF0));
            for ( size_t b = sizeof(TYPE) - 1; b > 0; --b ) {
                out->WriteByte(TByte(data >> (b * 8)));
            }
            out->WriteByte(TByte(data));
        }
    }
    else {
        // signed number
        if ( data < 0x40 && data >= -0x40 ) {
            // one byte: 0xxxxxxx
            out->WriteByte(TByte(data) & 0x7F);
        }
        else if ( data < 0x2000 && data >= -0x2000 ) {
            // two bytes: 10xxxxxx xxxxxxxx
            out->WriteByte(TByte(((data >> 8) & 0x3F) | 0x80));
            out->WriteByte(TByte(data));
        }
        else if ( data < 0x100000 && data >= -0x100000 ) {
            // three bytes: 110xxxxx xxxxxxxx xxxxxxxx
            out->WriteByte(TByte(((data >> 16) & 0x1F) | 0xC0));
            out->WriteByte(TByte(data >> 8));
            out->WriteByte(TByte(data));
        }
        else if ( data < 0x08000000 && data >= -0x08000000 ) {
            // four bytes: 1110xxxx xxxxxxxx xxxxxxxx xxxxxxxx
            out->WriteByte(TByte(((data >> 24) & 0x0F) | 0xE0));
            out->WriteByte(TByte(data >> 16));
            out->WriteByte(TByte(data >> 8));
            out->WriteByte(TByte(data));
        }
        else {
            // 1111xxxx (xxxxxxxx)*
            out->WriteByte(TByte((sizeof(TYPE) - 4) | 0xF0));
            for ( size_t b = sizeof(TYPE) - 1; b > 0; --b ) {
                out->WriteByte(TByte(data >> (b * 8)));
            }
            out->WriteByte(TByte(data));
        }
    }
}

void CObjectOStreamBinary::WriteStd(const char& data)
{
    if ( data == '\0' )
        WriteNull();
    else {
        WriteByte(CObjectStreamBinaryDefs::eStd_char);
        WriteByte(data);
    }
}

template<typename TYPE>
void WriteStdOrdinal(CObjectOStreamBinary* out, const TYPE& data)
{
    if ( data == 0 )
        out->WriteNull();
    else if ( sizeof(TYPE) == 1 ) {
        out->WriteByte((TYPE(-1) < TYPE(0))?
                       CObjectStreamBinaryDefs::eStd_sbyte:
                       CObjectStreamBinaryDefs::eStd_ubyte);
        out->WriteByte(data);
    }
    else {
        out->WriteByte((TYPE(-1) < TYPE(0))?
                       CObjectStreamBinaryDefs::eStd_sordinal:
                       CObjectStreamBinaryDefs::eStd_uordinal);
        WriteNumber(out, data);
    }
}

template<typename TYPE>
void WriteStdFloat(CObjectOStreamBinary* out, const TYPE& data)
{
    if ( data == 0 )
        out->WriteNull();
    else {
        out->WriteByte(CObjectStreamBinaryDefs::eStd_float);
    }
}

void CObjectOStreamBinary::WriteStd(const unsigned char& data)
{
    WriteStdOrdinal(this, data);
}

void CObjectOStreamBinary::WriteStd(const signed char& data)
{
    WriteStdOrdinal(this, data);
}

void CObjectOStreamBinary::WriteStd(const short& data)
{
    WriteStdOrdinal(this, data);
}

void CObjectOStreamBinary::WriteStd(const unsigned short& data)
{
    WriteStdOrdinal(this, data);
}

void CObjectOStreamBinary::WriteStd(const int& data)
{
    WriteStdOrdinal(this, data);
}

void CObjectOStreamBinary::WriteStd(const unsigned int& data)
{
    WriteStdOrdinal(this, data);
}

void CObjectOStreamBinary::WriteStd(const long& data)
{
    WriteStdOrdinal(this, data);
}

void CObjectOStreamBinary::WriteStd(const unsigned long& data)
{
    WriteStdOrdinal(this, data);
}

void CObjectOStreamBinary::WriteStd(const float& data)
{
    WriteStdFloat(this, data);
}

void CObjectOStreamBinary::WriteStd(const double& data)
{
    WriteStdFloat(this, data);
}

void CObjectOStreamBinary::WriteStd(const string& data)
{
    if ( data.empty() ) {
        WriteNull();
    }
    else {
        WriteByte(CObjectStreamBinaryDefs::eStd_string);
        WriteString(data);
    }
}

void CObjectOStreamBinary::WriteStd(char* const& data)
{
    if ( data == 0 ) {
        WriteNull();
    }
    else {
        WriteByte(CObjectStreamBinaryDefs::eStd_string);
        WriteString(data);
    }
}

void CObjectOStreamBinary::WriteStd(const char* const& data)
{
    if ( data == 0 ) {
        WriteNull();
    }
    else {
        WriteByte(CObjectStreamBinaryDefs::eStd_string);
        WriteString(data);
    }
}

void CObjectOStreamBinary::WriteIndex(TIndex index)
{
    WriteNumber(this, index);
}

void CObjectOStreamBinary::WriteSize(unsigned size)
{
    WriteNumber(this, size);
}

void CObjectOStreamBinary::WriteString(const string& str)
{
    TStrings::iterator i = m_Strings.find(str);
    if ( i == m_Strings.end() ) {
        // new string
        TIndex index = m_Strings.size();
        WriteIndex(index);
        m_Strings[str] = index;
        WriteSize(str.size());
        WriteBytes(str.data(), str.size());
    }
    else {
        WriteIndex(i->second);
    }
}

void CObjectOStreamBinary::WriteMemberPrefix(COObjectInfo& info)
{
    while ( info.IsMember() ) {
        WriteByte(CObjectStreamBinaryDefs::eMemberReference);
        WriteId(info.GetMemberInfo().GetName());
        info.ToContainerObject();
    }
}

void CObjectOStreamBinary::WriteNullPointer(void)
{
    WriteNull();
}

void CObjectOStreamBinary::WriteObjectReference(TIndex index)
{
    WriteByte(CObjectStreamBinaryDefs::eObjectReference);
    WriteIndex(index);
}

void CObjectOStreamBinary::WriteThisTypeReference(TTypeInfo )
{
    WriteByte(CObjectStreamBinaryDefs::eThisClass);
}

void CObjectOStreamBinary::WriteOtherTypeReference(TTypeInfo typeInfo)
{
    WriteByte(CObjectStreamBinaryDefs::eOtherClass);
    WriteId(typeInfo->GetName());
}

void CObjectOStreamBinary::FBegin(Block& block)
{
    WriteByte(CObjectStreamBinaryDefs::eBlock);
    WriteSize(block.GetSize());
}

void CObjectOStreamBinary::VNext(const Block& )
{
    WriteByte(CObjectStreamBinaryDefs::eElement);
}

void CObjectOStreamBinary::VEnd(const Block& )
{
    WriteByte(CObjectStreamBinaryDefs::eEndOfElements);
}

END_NCBI_SCOPE
