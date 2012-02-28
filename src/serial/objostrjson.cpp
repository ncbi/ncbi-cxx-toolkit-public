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
* Author: Andrei Gourianov
*
* File Description:
*   JSON object output stream
*
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.h>

#include <serial/objostrjson.hpp>
#include <serial/objistr.hpp>
#include <serial/objcopy.hpp>
#include <serial/impl/memberid.hpp>
#include <serial/impl/memberlist.hpp>
#include <serial/enumvalues.hpp>
#include <serial/objhook.hpp>
#include <serial/impl/classinfo.hpp>
#include <serial/impl/choice.hpp>
#include <serial/impl/continfo.hpp>
#include <serial/delaybuf.hpp>
#include <serial/impl/ptrinfo.hpp>
#include <serial/error_codes.hpp>

#include <stdio.h>
#include <math.h>


#define NCBI_USE_ERRCODE_X   Serial_OStream

BEGIN_NCBI_SCOPE


CObjectOStream* CObjectOStream::OpenObjectOStreamJson(CNcbiOstream& out,
                                                     bool deleteOut)
{
    return new CObjectOStreamJson(out, deleteOut);
}



CObjectOStreamJson::CObjectOStreamJson(CNcbiOstream& out, bool deleteOut)
    : CObjectOStream(eSerial_Json, out, deleteOut),
    m_BlockStart(false),
    m_ExpectValue(false),
    m_StringEncoding(eEncoding_Unknown),
    m_BinaryFormat(eDefault)
{
}

CObjectOStreamJson::~CObjectOStreamJson(void)
{
}

void CObjectOStreamJson::SetDefaultStringEncoding(EEncoding enc)
{
    m_StringEncoding = enc;
}

EEncoding CObjectOStreamJson::GetDefaultStringEncoding(void) const
{
    return m_StringEncoding;
}

CObjectOStreamJson::EBinaryDataFormat
CObjectOStreamJson::GetBinaryDataFormat(void) const
{
    return m_BinaryFormat;
}
void CObjectOStreamJson::SetBinaryDataFormat(CObjectOStreamJson::EBinaryDataFormat fmt)
{
    m_BinaryFormat = fmt;
}

void CObjectOStreamJson::SetJsonpMode(const string& function_name)
{
    m_JsonpPrefix = function_name + "(";
    m_JsonpSuffix = ")";
}

void CObjectOStreamJson::SetJsonpMode(const string& prefix, const string& suffix)
{
    m_JsonpPrefix = prefix;
    m_JsonpSuffix = suffix;
}

void CObjectOStreamJson::GetJsonpPadding(string* prefix, string* suffix) const
{
    if (prefix) {*prefix = m_JsonpPrefix;}
    if (suffix) {*suffix = m_JsonpSuffix;}
}

string CObjectOStreamJson::GetPosition(void) const
{
    return "line "+NStr::SizetToString(m_Output.GetLine());
}

void CObjectOStreamJson::WriteFileHeader(TTypeInfo type)
{
    if (!m_JsonpPrefix.empty() || !m_JsonpSuffix.empty()) {
        m_Output.PutString(m_JsonpPrefix);
    } else {
        StartBlock();
        if (!type->GetName().empty()) {
            m_Output.PutEol();
            WriteKey(type->GetName());
        }
    }
}

void CObjectOStreamJson::EndOfWrite(void)
{
    if (!m_JsonpPrefix.empty() || !m_JsonpSuffix.empty()) {
        m_Output.PutString(m_JsonpSuffix);
    } else {
        EndBlock();
        m_Output.PutEol();
    }
    CObjectOStream::EndOfWrite();
}

void CObjectOStreamJson::WriteBool(bool data)
{
    WriteKeywordValue( data ? "true" : "false");
}

void CObjectOStreamJson::WriteChar(char data)
{
    string s;
    s += data;
    WriteString(s);
}

void CObjectOStreamJson::WriteInt4(Int4 data)
{
    WriteKeywordValue(NStr::IntToString(data));
}

void CObjectOStreamJson::WriteUint4(Uint4 data)
{
    WriteKeywordValue(NStr::UIntToString(data));
}

void CObjectOStreamJson::WriteInt8(Int8 data)
{
    WriteKeywordValue(NStr::Int8ToString(data));
}

void CObjectOStreamJson::WriteUint8(Uint8 data)
{
    WriteKeywordValue(NStr::UInt8ToString(data));
}

void CObjectOStreamJson::WriteFloat(float data)
{
    WriteDouble2(data,FLT_DIG);
}

void CObjectOStreamJson::WriteDouble(double data)
{
    WriteDouble2(data,DBL_DIG);
}

void CObjectOStreamJson::WriteDouble2(double data, size_t digits)
{
    if (m_FastWriteDouble) {
        char buffer[64];
        WriteKeywordValue( string(buffer,
            NStr::DoubleToStringPosix(data, digits, buffer, sizeof(buffer))));
    } else {
        WriteKeywordValue(NStr::DoubleToString(data,digits, NStr::fDoublePosix));
    }
}

void CObjectOStreamJson::WriteCString(const char* str)
{
    WriteValue(str);
}

void CObjectOStreamJson::WriteString(const string& str,
                            EStringType type)
{
    WriteValue(str,type);
}

void CObjectOStreamJson::WriteStringStore(const string& s)
{
    WriteString(s);
}

void CObjectOStreamJson::CopyString(CObjectIStream& in,
                                    EStringType type)
{
    string s;
    in.ReadString(s, type);
    WriteString(s, type);
}

void CObjectOStreamJson::CopyStringStore(CObjectIStream& in)
{
    string s;
    in.ReadStringStore(s);
    WriteStringStore(s);
}

void CObjectOStreamJson::WriteNullPointer(void)
{
    if (m_ExpectValue ||
        TopFrame().GetFrameType() == CObjectStackFrame::eFrameArrayElement) {
        WriteKeywordValue("null");
    }
}

void CObjectOStreamJson::WriteObjectReference(TObjectIndex /*index*/)
{
    ThrowError(fNotImplemented, "Not Implemented");
}

void CObjectOStreamJson::WriteOtherBegin(TTypeInfo /*typeInfo*/)
{
    ThrowError(fNotImplemented, "Not Implemented");
}

void CObjectOStreamJson::WriteOtherEnd(TTypeInfo /*typeInfo*/)
{
    ThrowError(fNotImplemented, "Not Implemented");
}

void CObjectOStreamJson::WriteOther(TConstObjectPtr /*object*/, TTypeInfo /*typeInfo*/)
{
    ThrowError(fNotImplemented, "Not Implemented");
}

void CObjectOStreamJson::WriteNull(void)
{
    if (m_ExpectValue) {
        WriteKeywordValue("null");
    }
}

void CObjectOStreamJson::WriteAnyContentObject(const CAnyContentObject& obj)
{
    string obj_name = obj.GetName();
    if (obj_name.empty()) {
        if (!StackIsEmpty() && TopFrame().HasMemberId()) {
            obj_name = TopFrame().GetMemberId().GetName();
        }
        if (obj_name.empty()) {
            ThrowError(fInvalidData, "AnyContent object must have name");
        }
    }
    NextElement();
    WriteKey(obj_name);
    const vector<CSerialAttribInfoItem>& attlist = obj.GetAttributes();
    if (attlist.empty()) {
        WriteValue(obj.GetValue());
        return;
    }
    StartBlock();
    vector<CSerialAttribInfoItem>::const_iterator it;
    for ( it = attlist.begin(); it != attlist.end(); ++it) {
        NextElement();
        WriteKey(it->GetName());
        WriteValue(it->GetValue());
    }
    m_SkippedMemberId = obj_name;
    WriteValue(obj.GetValue());
    EndBlock();
}

void CObjectOStreamJson::CopyAnyContentObject(CObjectIStream& in)
{
    CAnyContentObject obj;
    in.ReadAnyContentObject(obj);
    WriteAnyContentObject(obj);
}


void CObjectOStreamJson::WriteBitString(const CBitString& obj)
{
    m_Output.PutChar('\"');
#if BITSTRING_AS_VECTOR
    static const char ToHex[] = "0123456789ABCDEF";
    Uint1 data, mask;
    bool done = false;
    for ( CBitString::const_iterator i = obj.begin(); !done; ) {
        for (data=0, mask=0x8; !done && mask!=0; mask >>= 1) {
            if (*i) {
                data |= mask;
            }
            done = (++i == obj.end());
        }
        m_Output.PutChar(ToHex[data]);
    }
#else
    if (TopFrame().HasMemberId() && TopFrame().GetMemberId().IsCompressed()) {
        bm::word_t* tmp_block = obj.allocate_tempblock();
        CBitString::statistics st;
        obj.calc_stat(&st);
        char* buf = (char*)malloc(st.max_serialize_mem);
        unsigned int len = bm::serialize(obj, (unsigned char*)buf, tmp_block);
        WriteBytes(buf,len);
        free(buf);
        free(tmp_block);
    } else {
        CBitString::size_type i=0;
        CBitString::size_type ilast = obj.size();
        CBitString::enumerator e = obj.first();
        for (; i < ilast; ++i) {
            m_Output.PutChar( (i == *e) ? '1' : '0');
            if (i == *e) {
                ++e;
            }
        }
    }
#endif
    m_Output.PutString("B\"");
}

void CObjectOStreamJson::CopyBitString(CObjectIStream& /*in*/)
{
    ThrowError(fNotImplemented, "Not Implemented");
}

void CObjectOStreamJson::WriteEnum(const CEnumeratedTypeValues& values,
                        TEnumValueType value)
{
    string value_str;
    if (values.IsInteger()) {
        value_str = NStr::IntToString(value);
        const string& name = values.FindName(value, values.IsInteger());
        if (name.empty() || GetWriteNamedIntegersByValue()) {
            WriteKeywordValue(value_str);
        } else {
            WriteValue(name);
        }
    } else {
        value_str = values.FindName(value, values.IsInteger());
        WriteValue(value_str);
    }
}

void CObjectOStreamJson::CopyEnum(const CEnumeratedTypeValues& values,
                        CObjectIStream& in)
{
    TEnumValueType value = in.ReadEnum(values);
    WriteEnum(values, value);
}

void CObjectOStreamJson::WriteClassMember(const CMemberId& memberId,
                                          TTypeInfo memberType,
                                          TConstObjectPtr memberPtr)
{
    CObjectOStream::WriteClassMember(memberId,memberType,memberPtr);
}

bool CObjectOStreamJson::WriteClassMember(const CMemberId& memberId,
                                          const CDelayBuffer& buffer)
{
    return CObjectOStream::WriteClassMember(memberId,buffer);
}


void CObjectOStreamJson::BeginNamedType(TTypeInfo namedTypeInfo)
{
    CObjectOStream::BeginNamedType(namedTypeInfo);
}

void CObjectOStreamJson::EndNamedType(void)
{
    CObjectOStream::EndNamedType();
}


void CObjectOStreamJson::BeginContainer(const CContainerTypeInfo* /*containerType*/)
{
    BeginArray();
}

void CObjectOStreamJson::EndContainer(void)
{
    EndArray();
}

void CObjectOStreamJson::BeginContainerElement(TTypeInfo /*elementType*/)
{
    NextElement();
}

void CObjectOStreamJson::EndContainerElement(void)
{
}


void CObjectOStreamJson::BeginClass(const CClassTypeInfo* /*classInfo*/)
{
    if (GetStackDepth() > 1 && FetchFrameFromTop(1).GetNotag()) {
        return;
    }
    StartBlock();
}


void CObjectOStreamJson::EndClass(void)
{
    if (GetStackDepth() > 1 && FetchFrameFromTop(1).GetNotag()) {
        return;
    }
    EndBlock();
}

void CObjectOStreamJson::BeginClassMember(const CMemberId& id)
{
    if (id.HasNotag() || id.IsAttlist()) {
        m_SkippedMemberId = id.GetName();
        TopFrame().SetNotag();
        return;
    }
    if (id.HasAnyContent()) {
        return;
    }
    NextElement();
    WriteMemberId(id);
}

void CObjectOStreamJson::EndClassMember(void)
{
    if (TopFrame().GetNotag()) {
        TopFrame().SetNotag(false);
    }
}


void CObjectOStreamJson::BeginChoice(const CChoiceTypeInfo* /*choiceType*/)
{
    if (GetStackDepth() > 1 && FetchFrameFromTop(1).GetNotag()) {
        return;
    }
    StartBlock();
}

void CObjectOStreamJson::EndChoice(void)
{
    if (GetStackDepth() > 1 && FetchFrameFromTop(1).GetNotag()) {
        return;
    }
    EndBlock();
}

void CObjectOStreamJson::BeginChoiceVariant(const CChoiceTypeInfo* /*choiceType*/,
                                const CMemberId& id)
{
    if (id.HasNotag() || id.IsAttlist()) {
        m_SkippedMemberId = id.GetName();
        TopFrame().SetNotag();
        return;
    }
    NextElement();
    WriteMemberId(id);
}

void CObjectOStreamJson::EndChoiceVariant(void)
{
    if (TopFrame().GetNotag()) {
        TopFrame().SetNotag(false);
    }
}


static const char* const HEX = "0123456789ABCDEF";

void CObjectOStreamJson::BeginBytes(const ByteBlock& )
{
    if (m_BinaryFormat == eArray_Bool ||
        m_BinaryFormat == eArray_01 ||
        m_BinaryFormat == eArray_Uint) {
        m_Output.PutChar('[');
    } else {
        m_Output.PutChar('\"');
    }
}

void CObjectOStreamJson::WriteBytes(const ByteBlock& block,
                        const char* bytes, size_t length)
{
    if (m_BinaryFormat != CObjectOStreamJson::eDefault) {
        WriteCustomBytes(bytes,length);
        return;
    }
    if (TopFrame().HasMemberId() && TopFrame().GetMemberId().IsCompressed()) {
        WriteBase64Bytes(bytes,length);
        return;
    }
    WriteBytes(bytes,length);
}

void CObjectOStreamJson::EndBytes(const ByteBlock& )
{
    if (m_BinaryFormat == eArray_Bool ||
        m_BinaryFormat == eArray_01 ||
        m_BinaryFormat == eArray_Uint) {
        m_Output.BackChar(',');
        m_Output.PutEol();
        m_Output.PutChar(']');
    } else {
        if (m_BinaryFormat == eString_01B) {
           m_Output.PutChar('B');
        }
        m_Output.PutChar('\"');
    }
}

void CObjectOStreamJson::WriteBase64Bytes(const char* bytes, size_t length)
{
    const size_t chunk_in  = 57;
    const size_t chunk_out = 80;
    if (length > chunk_in) {
        m_Output.PutEol(false);
    }
    char dst_buf[chunk_out];
    size_t bytes_left = length;
    size_t  src_read=0, dst_written=0, line_len=0;
    while (bytes_left > 0 && bytes_left <= length) {
        BASE64_Encode(bytes,  min(bytes_left,chunk_in),  &src_read,
                        dst_buf, chunk_out, &dst_written, &line_len);
        m_Output.PutString(dst_buf,dst_written);
        bytes_left -= src_read;
        bytes += src_read;
        if (bytes_left > 0) {
            m_Output.PutEol(false);
        }
    }
    if (length > chunk_in) {
        m_Output.PutEol(false);
    }
}

void CObjectOStreamJson::WriteBytes(const char* bytes, size_t length)
{
    while ( length-- > 0 ) {
        char c = *bytes++;
        m_Output.PutChar(HEX[(c >> 4) & 0xf]);
        m_Output.PutChar(HEX[c & 0xf]);
    }
}

void CObjectOStreamJson::WriteCustomBytes(const char* bytes, size_t length)
{
    if (m_BinaryFormat == eString_Base64) {
        WriteBase64Bytes(bytes, length);
        return;
    } else if (m_BinaryFormat == eString_Hex) {
        WriteBytes(bytes, length);
        return;
    }
    if (m_BinaryFormat != eString_Hex &&
        m_BinaryFormat != eString_01 && 
        m_BinaryFormat != eString_01B) {
        m_Output.PutEol(false);
    }
    while ( length-- > 0 ) {
        Uint1 c = *bytes++;
        Uint1 mask=0x80;
        switch (m_BinaryFormat) {
        case eArray_Bool:
            for (; mask!=0; mask >>= 1) {
                m_Output.WrapAt(78, false);
                m_Output.PutString( (mask & c) ? "true" : "false");
                m_Output.PutChar(',');
            }
            break;
        case eArray_01:
            for (; mask!=0; mask >>= 1) {
                m_Output.WrapAt(78, false);
                m_Output.PutChar( (mask & c) ? '1' : '0');
                m_Output.PutChar(',');
            }
            break;
        default:
        case eArray_Uint:
            m_Output.WrapAt(78, false);
            m_Output.PutString( NStr::UIntToString((unsigned int)c));
            m_Output.PutChar(',');
            break;
        case eString_01:
        case eString_01B:
            for (; mask!=0; mask >>= 1) {
                m_Output.PutChar( (mask & c) ? '1' : '0');
            }
            break;
        }
    }
}

void CObjectOStreamJson::WriteChars(const CharBlock& /*block*/,
                        const char* /*chars*/, size_t /*length*/)
{
    ThrowError(fNotImplemented, "Not Implemented");
}


void CObjectOStreamJson::WriteSeparator(void)
{
}

void CObjectOStreamJson::WriteMemberId(const CMemberId& id)
{
    WriteKey(id.GetName());
    m_SkippedMemberId.erase();
}

void CObjectOStreamJson::WriteSkippedMember(void)
{
    string name("#");
    name += m_SkippedMemberId;
    NextElement();
    WriteKey(name);
    m_SkippedMemberId.erase();
}


void CObjectOStreamJson::WriteEscapedChar(char c, EEncoding enc_in)
{
    switch ( c ) {
    case '"':
        m_Output.PutString("\\\"");
        break;
    case '\\':
        m_Output.PutString("\\\\");
        break;
    default:
        if ( (unsigned int)c <  0x20 ||
            ((unsigned int)c >= 0x80 && enc_in != eEncoding_UTF8) ) {
            m_Output.PutString("\\u00");
            Uint1 ch = c;
            unsigned hi = ch >> 4;
            unsigned lo = ch & 0xF;
            m_Output.PutChar(HEX[hi]);
            m_Output.PutChar(HEX[lo]);
        } else {
            m_Output.PutChar(c);
        }
        break;
    }
}

void CObjectOStreamJson::WriteEncodedChar(const char*& src, EStringType type)
{
    EEncoding enc_in( type == eStringTypeUTF8 ? eEncoding_UTF8 : m_StringEncoding);
    EEncoding enc_out(eEncoding_UTF8);

    if (enc_in == enc_out || enc_in == eEncoding_Unknown || (*src & 0x80) == 0) {
        WriteEscapedChar(*src, enc_in);
    } else {
        CStringUTF8 tmp;
        tmp.Assign(*src,enc_in);
        for ( string::const_iterator t = tmp.begin(); t != tmp.end(); ++t ) {
            m_Output.PutChar(*t);
        }
    }
}

void CObjectOStreamJson::x_WriteString(const string& value, EStringType type)
{
    m_Output.PutChar('\"');
    for (const char* src = value.c_str(); *src; ++src) {
        WriteEncodedChar(src,type);
    }
    m_Output.PutChar('\"');
}

void CObjectOStreamJson::WriteKey(const string& key)
{
    string s(key);
    NStr::ReplaceInPlace(s,"-","_");
    x_WriteString(s);
    NameSeparator();
}

void CObjectOStreamJson::WriteValue(const string& value, EStringType type)
{
    if (!m_ExpectValue && !m_SkippedMemberId.empty()) {
        WriteSkippedMember();
    }
    x_WriteString(value,type);
    m_ExpectValue = false;
}

void CObjectOStreamJson::WriteKeywordValue(const string& value)
{
    m_Output.PutString(value);
    m_ExpectValue = false;
}

void CObjectOStreamJson::StartBlock(void)
{
    if (!m_ExpectValue && !m_SkippedMemberId.empty()) {
        WriteSkippedMember();
    }
    m_Output.PutChar('{');
    m_Output.IncIndentLevel();
    m_BlockStart = true;
    m_ExpectValue = false;
}

void CObjectOStreamJson::EndBlock(void)
{
    m_Output.DecIndentLevel();
    m_Output.PutEol();
    m_Output.PutChar('}');
    m_BlockStart = false;
    m_ExpectValue = false;
}

void CObjectOStreamJson::NextElement(void)
{
    if ( m_BlockStart ) {
        m_BlockStart = false;
    } else {
        m_Output.PutChar(',');
    }
    m_Output.PutEol();
    m_ExpectValue = false;
}

void CObjectOStreamJson::BeginArray(void)
{
    if (!m_ExpectValue && !m_SkippedMemberId.empty()) {
        WriteSkippedMember();
    }
    m_Output.PutChar('[');
    m_Output.IncIndentLevel();
    m_BlockStart = true;
    m_ExpectValue = false;
}

void CObjectOStreamJson::EndArray(void)
{
    m_Output.DecIndentLevel();
    m_Output.PutEol();
    m_Output.PutChar(']');
    m_BlockStart = false;
    m_ExpectValue = false;
}

void CObjectOStreamJson::NameSeparator(void)
{
    m_Output.PutChar(':');
    if (m_Output.GetUseIndentation()) {
        m_Output.PutChar(' ');
    }
    m_ExpectValue = true;
}

END_NCBI_SCOPE
