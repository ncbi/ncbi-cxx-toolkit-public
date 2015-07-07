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
*   ASN.1 binary object output stream.
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.hpp>
#include <corelib/ncbi_param.hpp>

#include <serial/objostrasnb.hpp>
#include <serial/objistr.hpp>
#include <serial/objcopy.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/impl/memberid.hpp>
#include <serial/enumvalues.hpp>
#include <serial/impl/memberlist.hpp>
#include <serial/objhook.hpp>
#include <serial/impl/classinfo.hpp>
#include <serial/impl/choice.hpp>
#include <serial/impl/continfo.hpp>
#include <serial/delaybuf.hpp>
#include <serial/error_codes.hpp>

#include <stdio.h>
#include <math.h>

#undef _TRACE
#define _TRACE(arg) ((void)0)

#define NCBI_USE_ERRCODE_X   Serial_OStream

#define USE_OLD_TAGS 0
// this is backward compatibility check
#define USE_VERIFY_TAGGING 1

BEGIN_NCBI_SCOPE


CObjectOStream* CObjectOStream::OpenObjectOStreamAsnBinary(CNcbiOstream& out,
                                                           EOwnership deleteOut)
{
    return new CObjectOStreamAsnBinary(out, deleteOut);
}

CObjectOStreamAsnBinary::CObjectOStreamAsnBinary(CNcbiOstream& out,
                                                 EFixNonPrint how)
    : CObjectOStream(eSerial_AsnBinary, out),
      m_CStyleBigInt(false), m_SkipNextTag(false), m_AutomaticTagging(true)
{
    FixNonPrint(how);
#if CHECK_OUTSTREAM_INTEGRITY
    m_CurrentPosition = 0;
    m_CurrentTagState = eTagStart;
    m_CurrentTagLimit = 0;
#endif
}

CObjectOStreamAsnBinary::CObjectOStreamAsnBinary(CNcbiOstream& out,
                                                 bool deleteOut,
                                                 EFixNonPrint how)
    : CObjectOStream(eSerial_AsnBinary, out, deleteOut ? eTakeOwnership : eNoOwnership),
      m_CStyleBigInt(false), m_SkipNextTag(false), m_AutomaticTagging(true)
{
    FixNonPrint(how);
#if CHECK_OUTSTREAM_INTEGRITY
    m_CurrentPosition = 0;
    m_CurrentTagState = eTagStart;
    m_CurrentTagLimit = 0;
#endif
}

CObjectOStreamAsnBinary::CObjectOStreamAsnBinary(CNcbiOstream& out,
                                                 EOwnership deleteOut,
                                                 EFixNonPrint how)
    : CObjectOStream(eSerial_AsnBinary, out, deleteOut),
      m_CStyleBigInt(false), m_SkipNextTag(false), m_AutomaticTagging(true)
{
    FixNonPrint(how);
#if CHECK_OUTSTREAM_INTEGRITY
    m_CurrentPosition = 0;
    m_CurrentTagState = eTagStart;
    m_CurrentTagLimit = 0;
#endif
}

CObjectOStreamAsnBinary::~CObjectOStreamAsnBinary(void)
{
#if CHECK_OUTSTREAM_INTEGRITY
    if ( !m_Limits.empty() || m_CurrentTagState != eTagStart )
        ERR_POST_X(9, "CObjectOStreamAsnBinary not finished");
#endif
}

#if CHECK_OUTSTREAM_INTEGRITY
inline
void CObjectOStreamAsnBinary::StartTag(Uint1 code)
{
    m_Limits.push(m_CurrentTagLimit);
    m_CurrentTagCode = code;
    m_CurrentTagPosition = m_CurrentPosition;
    m_CurrentTagState = GetTagValue(code) == eLongTag? eTagValue: eLengthStart;
}

inline
void CObjectOStreamAsnBinary::EndTag(void)
{
    if ( m_Limits.empty() )
        ThrowError(fIllegalCall, "too many tag ends");
    m_CurrentTagState = eTagStart;
    m_CurrentTagLimit = m_Limits.top();
    m_Limits.pop();
}

inline
void CObjectOStreamAsnBinary::SetTagLength(size_t length)
{
    Int8 limit = m_CurrentPosition + 1 + length;
    if ( limit <= m_CurrentPosition ||
        (m_CurrentTagLimit != 0 && limit > m_CurrentTagLimit) )
        ThrowError(fIllegalCall, "tag will overflow enclosing tag");
    m_CurrentTagLimit = limit;
    if ( GetTagConstructed(m_CurrentTagCode) ) // constructed
        m_CurrentTagState = eTagStart;
    else
        m_CurrentTagState = eData;
    if ( length == 0 )
        EndTag();
}
#endif

#if !CHECK_OUTSTREAM_INTEGRITY
inline
#endif
void CObjectOStreamAsnBinary::WriteByte(Uint1 byte)
{
#if CHECK_OUTSTREAM_INTEGRITY
    //_TRACE("WriteByte: " << NStr::PtrToString(byte));
    if ( m_CurrentTagLimit != 0 &&
         m_CurrentPosition >= m_CurrentTagLimit )
        ThrowError(fOverflow, "tag size overflow");
    switch ( m_CurrentTagState ) {
    case eTagStart:
        StartTag(byte);
        break;
    case eTagValue:
        if ( (byte & 0x80) == 0)
            m_CurrentTagState = eLengthStart;
        break;
    case eLengthStart:
        if ( byte == 0 ) {
            SetTagLength(0);
            if ( m_CurrentTagCode == 0 )
                EndTag();
        }
        else if ( byte == 0x80 ) {
            if ( !GetTagConstructed(m_CurrentTagCode) ) {
                ThrowError(fIllegalCall,
                           "cannot use indefinite form for primitive tag");
            }
            m_CurrentTagState = eTagStart;
        }
        else if ( byte < 0x80 ) {
            SetTagLength(byte);
        }
        else {
            m_CurrentTagLengthSize = byte - 0x80;
            if ( m_CurrentTagLengthSize > sizeof(size_t) )
                ThrowError(fOverflow, "tag length is too big");
            m_CurrentTagState = eLengthValueFirst;
        }
        break;
    case eLengthValueFirst:
        if ( byte == 0 )
            ThrowError(fInvalidData, "first byte of length is zero");
        if ( --m_CurrentTagLengthSize == 0 ) {
            SetTagLength(byte);
        }
        else {
            m_CurrentTagLength = byte;
            m_CurrentTagState = eLengthValue;
        }
        break;
        // fall down to next case (no break needed)
    case eLengthValue:
        m_CurrentTagLength = (m_CurrentTagLength << 8) | byte;
        if ( --m_CurrentTagLengthSize == 0 )
            SetTagLength(m_CurrentTagLength);
        break;
    case eData:
        _ASSERT( m_CurrentTagLimit != 0);
        if ( m_CurrentPosition + 1 == m_CurrentTagLimit )
            EndTag();
        break;
    }
    m_CurrentPosition += 1;
#endif
    m_Output.PutChar(byte);
}

#if !CHECK_OUTSTREAM_INTEGRITY
inline
#endif
void CObjectOStreamAsnBinary::WriteBytes(const char* bytes, size_t size)
{
    if ( size == 0 )
        return;
#if CHECK_OUTSTREAM_INTEGRITY
    //_TRACE("WriteBytes: " << size);
    if ( m_CurrentTagState != eData )
        ThrowError(fIllegalCall, "WriteBytes only allowed in DATA");
    Int8 new_pos = m_CurrentPosition + size;
    if ( new_pos < m_CurrentPosition ||
        (m_CurrentTagLimit != 0 && new_pos > m_CurrentTagLimit) )
        ThrowError(fOverflow, "tag DATA overflow");
    m_CurrentPosition = new_pos;
    if ( new_pos == m_CurrentTagLimit )
        EndTag();
#endif
    m_Output.PutString(bytes, size);
}

template<typename T>
inline
void CObjectOStreamAsnBinary::WriteBytesOf(const T& value, size_t count)
{
    for ( size_t shift = (count - 1) * 8; shift > 0; shift -= 8 ) {
        WriteByte(Uint1(value >> shift));
    }
    WriteByte(Uint1(value));
}

inline
void CObjectOStreamAsnBinary::WriteShortTag(ETagClass tag_class,
                                            ETagConstructed tag_constructed,
                                            ETagValue tag_value)
{
    if (m_SkipNextTag) {
        m_SkipNextTag = false;
        return;
    }
    WriteByte(MakeTagByte(tag_class, tag_constructed, tag_value));
}

inline
void CObjectOStreamAsnBinary::WriteSysTag(ETagValue tag_value)
{
    if (m_SkipNextTag) {
        m_SkipNextTag = false;
        return;
    }
    WriteShortTag(eUniversal, ePrimitive, tag_value);
}

NCBI_PARAM_DECL(bool, SERIAL, WRITE_UTF8STRING_TAG);
NCBI_PARAM_DEF_EX(bool, SERIAL, WRITE_UTF8STRING_TAG, false,
                  eParam_NoThread, SERIAL_WRITE_UTF8STRING_TAG);

CObjectOStreamAsnBinary::TByte CObjectOStreamAsnBinary::MakeUTF8StringTag(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(SERIAL, WRITE_UTF8STRING_TAG)> s_WriteUTF8StringTag;
    ETagValue value = s_WriteUTF8StringTag->Get() ? eUTF8String: eVisibleString;
    return MakeTagByte(eUniversal, ePrimitive, value);
}

inline
CObjectOStreamAsnBinary::TByte CObjectOStreamAsnBinary::GetUTF8StringTag(void)
{
    static TByte s_UTF8StringTag = 0;
    if ( !s_UTF8StringTag ) {
        s_UTF8StringTag = MakeUTF8StringTag();
    }
    return s_UTF8StringTag;
}

inline
void CObjectOStreamAsnBinary::WriteStringTag(EStringType type)
{
    if (m_SkipNextTag) {
        m_SkipNextTag = false;
        return;
    }
    WriteByte(type == eStringTypeUTF8?
              GetUTF8StringTag():
              MakeTagByte(eUniversal, ePrimitive, eVisibleString));
}

void CObjectOStreamAsnBinary::WriteLongTag(ETagClass tag_class,
                                           ETagConstructed tag_constructed,
                                           TLongTag tag_value)
{
    if ( tag_value <= 0 )
        ThrowError(fInvalidData, "negative tag number");
    
    // long form
    WriteShortTag(tag_class, tag_constructed, eLongTag);
    // calculate largest shift enough for TTag to fit
    size_t shift = (sizeof(TLongTag) * 8 - 1) / 7 * 7;
    Uint1 bits;
    // find first non zero 7bits
    while ( (bits = (tag_value >> shift) & 0x7f) == 0 ) {
        shift -= 7;
    }
    
    // beginning of tag
    while ( shift != 0 ) {
        shift -= 7;
        WriteByte((tag_value >> shift) | 0x80);
    }
    // write remaining bits
    WriteByte(tag_value & 0x7f);
}

inline
void CObjectOStreamAsnBinary::WriteTag(ETagClass tag_class,
                                       ETagConstructed tag_constructed,
                                       TLongTag tag_value)
{
    if (m_SkipNextTag) {
        m_SkipNextTag = false;
        return;
    }
    if ( tag_value >= 0 && tag_value < eLongTag )
        WriteShortTag(tag_class, tag_constructed, ETagValue(tag_value));
    else
        WriteLongTag(tag_class, tag_constructed, tag_value);
}

void CObjectOStreamAsnBinary::WriteClassTag(TTypeInfo typeInfo)
{
    if (m_SkipNextTag) {
        m_SkipNextTag = false;
        return;
    }
    const string& tag = typeInfo->GetName();
    if ( tag.empty() )
        ThrowError(fInvalidData, "empty tag string");

    _ASSERT( tag[0] > eLongTag );

    // long form
    WriteShortTag(eApplication, eConstructed, eLongTag);
    SIZE_TYPE last = tag.size() - 1;
    for ( SIZE_TYPE i = 0; i <= last; ++i ) {
        char c = tag[i];
        _ASSERT( (c & 0x80) == 0 );
        if ( i != last )
            c |= 0x80;
        WriteByte(c);
    }
}

inline
void CObjectOStreamAsnBinary::WriteIndefiniteLength(void)
{
    WriteByte(eIndefiniteLengthByte);
}

inline
void CObjectOStreamAsnBinary::WriteShortLength(size_t length)
{
    WriteByte(TByte(length));
}

void CObjectOStreamAsnBinary::WriteLongLength(size_t length)
{
    // long form
    size_t count;
    if ( length <= 0xffU )
        count = 1;
    else if ( length <= 0xffffU )
        count = 2;
    else if ( length <= 0xffffffU )
        count = 3;
    else {
        count = sizeof(length);
        if ( sizeof(length) > 4 ) {
            for ( size_t shift = (count-1)*8;
                  count > 0; --count, shift -= 8 ) {
                if ( Uint1(length >> shift) != 0 )
                    break;
            }
        }
    }
    WriteByte(TByte(0x80 + count));
    WriteBytesOf(length, count);
}

inline
void CObjectOStreamAsnBinary::WriteLength(size_t length)
{
    if ( length <= 127 )
        WriteShortLength(length);
    else
        WriteLongLength(length);
}

inline
void CObjectOStreamAsnBinary::WriteEndOfContent(void)
{
    WriteSysTag(eNone);
    WriteShortLength(0);
}

void CObjectOStreamAsnBinary::WriteNull(void)
{
    WriteSysTag(eNull);
    WriteShortLength(0);
}

void CObjectOStreamAsnBinary::WriteAnyContentObject(const CAnyContentObject& )
{
    ThrowError(fNotImplemented,
        "CObjectOStreamAsnBinary::WriteAnyContentObject: "
        "unable to write AnyContent object in ASN");
}

void CObjectOStreamAsnBinary::CopyAnyContentObject(CObjectIStream& )
{
    ThrowError(fNotImplemented,
        "CObjectOStreamAsnBinary::CopyAnyContentObject: "
        "unable to copy AnyContent object in ASN");
}

void CObjectOStreamAsnBinary::WriteBitString(const CBitString& obj)
{
#if BITSTRING_AS_VECTOR
    bool compressed = false;
#else
    bool compressed = IsCompressed();
#endif
    char* buf=0;
    unsigned int len = obj.size();
    if (compressed) {
        CBitString::statistics st;
        obj.calc_stat(&st);
        buf = (char*)malloc(st.max_serialize_mem);
        bm::word_t* tmp_block = obj.allocate_tempblock();
        len = 8*bm::serialize(obj, (unsigned char*)buf, tmp_block);
        free(tmp_block);
    }

    WriteSysTag(compressed ? eOctetString : eBitString);
    if (len == 0) {
        WriteLength(0);
        return;
    }
    WriteLength((len+7)/8+(compressed ? 0 : 1));
    if (!compressed) {
        WriteByte(TByte(len%8 ? 8-len%8 : 0));
    }
    const size_t reserve=128;
    char bytes[reserve];
    size_t b=0;
    Uint1 data, mask;
    bool done=false;

#if BITSTRING_AS_VECTOR
    for ( CBitString::const_iterator i = obj.begin(); !done; ) {
        for (data=0, mask=0x80; mask != 0 && !done; mask >>= 1) {
            if (*i) {
                data |= mask;
            }
            done = (++i == obj.end());
        }
        bytes[b++] = data;
        if (b==reserve || done) {
            WriteBytes(bytes,b);
            b=0;
        }
    }
#else
    if (compressed) {
        WriteBytes(buf,len/8);
        free(buf);
        return;
    }
    CBitString::size_type i=0;
    CBitString::size_type ilast = obj.size();
    CBitString::enumerator e = obj.first();
    while (!done) {
        for (data=0, mask=0x80; !done && mask!=0; mask >>= 1) {
            if (i == *e) {
                data |= mask;
                ++e;
            }
            done = (++i == ilast);
        }
        bytes[b++] = data;
        if (b==reserve || done) {
            WriteBytes(bytes,b);
            b = 0;
        }
    }
#endif
}

void CObjectOStreamAsnBinary::CopyBitString(CObjectIStream& in)
{
    CBitString obj;
    in.ReadBitString(obj);
    WriteBitString(obj);
}

void CObjectOStreamAsnBinary::WriteNumberValue(Int4 data)
{
    size_t length;
    if ( data >= Int4(-0x80) && data <= Int4(0x7f) ) {
        // one byte
        length = 1;
    }
    else if ( data >= Int4(-0x8000) && data <= Int4(0x7fff) ) {
        // two bytes
        length = 2;
    }
    else if ( data >= Int4(-0x800000) && data <= Int4(0x7fffff) ) {
        // three bytes
        length = 3;
    }
    else {
        // full length signed
        length = sizeof(data);
    }
    WriteShortLength(length);
    WriteBytesOf(data, length);
}

void CObjectOStreamAsnBinary::WriteNumberValue(Int8 data)
{
    size_t length;
    if ( data >= -Int8(0x80) && data <= Int8(0x7f) ) {
        // one byte
        length = 1;
    }
    else if ( data >= Int8(-0x8000) && data <= Int8(0x7fff) ) {
        // two bytes
        length = 2;
    }
    else if ( data >= Int8(-0x800000) && data <= Int8(0x7fffff) ) {
        // three bytes
        length = 3;
    }
    else if ( data >= Int8(-0x7fffffffL-1) && data <= Int8(0x7fffffffL) ) {
        // four bytes
        length = 4;
    }
    else {
        // full length signed
        length = sizeof(data);
    }
    WriteShortLength(length);
    WriteBytesOf(data, length);
}

void CObjectOStreamAsnBinary::WriteNumberValue(Uint4 data)
{
    size_t length;
    if ( data <= 0x7fU ) {
        length = 1;
    }
    else if ( data <= 0x7fffU ) {
        // two bytes
        length = 2;
    }
    else if ( data <= 0x7fffffU ) {
        // three bytes
        length = 3;
    }
    else if ( data <= 0x7fffffffU ) {
        // four bytes
        length = 4;
    }
    else {
        // check for high bit to avoid storing unsigned data as negative
        if ( (data & (Uint4(1) << (sizeof(data) * 8 - 1))) != 0 ) {
            // full length unsigned - and doesn't fit in signed place
            WriteShortLength(sizeof(data) + 1);
            WriteByte(0);
            WriteBytesOf(data, sizeof(data));
            return;
        }
        else {
            // full length
            length = sizeof(data);
        }
    }
    WriteShortLength(length);
    WriteBytesOf(data, length);
}

void CObjectOStreamAsnBinary::WriteNumberValue(Uint8 data)
{
    size_t length;
    if ( data <= 0x7fUL ) {
        length = 1;
    }
    else if ( data <= 0x7fffUL ) {
        // two bytes
        length = 2;
    }
    else if ( data <= 0x7fffffUL ) {
        // three bytes
        length = 3;
    }
    else if ( data <= 0x7fffffffUL ) {
        // four bytes
        length = 4;
    }
    else {
        // check for high bit to avoid storing unsigned data as negative
        if ( (data & (Uint8(1) << (sizeof(data) * 8 - 1))) != 0 ) {
            // full length unsigned - and doesn't fit in signed place
            WriteShortLength(sizeof(data) + 1);
            WriteByte(0);
            WriteBytesOf(data, sizeof(data));
            return;
        }
        else {
            // full length
            length = sizeof(data);
        }
    }
    WriteShortLength(length);
    WriteBytesOf(data, length);
}

void CObjectOStreamAsnBinary::WriteBool(bool data)
{
    WriteSysTag(eBoolean);
    WriteShortLength(1);
    WriteByte(data);
}

void CObjectOStreamAsnBinary::WriteChar(char data)
{
    WriteSysTag(eGeneralString);
    WriteShortLength(1);
    WriteByte(data);
}

void CObjectOStreamAsnBinary::WriteInt4(Int4 data)
{
    WriteSysTag(eInteger);
    WriteNumberValue(data);
}

void CObjectOStreamAsnBinary::WriteUint4(Uint4 data)
{
    WriteSysTag(eInteger);
    WriteNumberValue(data);
}

void CObjectOStreamAsnBinary::WriteInt8(Int8 data)
{
    if ( m_CStyleBigInt ) {
        WriteShortTag(eApplication, ePrimitive, eInteger);
    } else {
        WriteSysTag(eInteger);
    }
    WriteNumberValue(data);
}

void CObjectOStreamAsnBinary::WriteUint8(Uint8 data)
{
    if ( m_CStyleBigInt ) {
        WriteShortTag(eApplication, ePrimitive, eInteger);
    } else {
        WriteSysTag(eInteger);
    }
    WriteNumberValue(data);
}

static const size_t kMaxDoubleLength = 64;

void CObjectOStreamAsnBinary::WriteDouble2(double data, unsigned digits)
{
    char buffer[kMaxDoubleLength + 16];
    int width = 0;
    Uint1 type = eDecimal;
    WriteSysTag(eReal);

#if 0
    if (isnan(data)) {
        ThrowError(fInvalidData, "invalid double: not a number");
    }
    if (!finite(data)) {
        ThrowError(fInvalidData, "invalid double: infinite");
    }
#else
    if (data == 0.) {
        double zero = 0.;
        if (memcmp(&data, &zero, sizeof(double)) == 0) {
            WriteLength(0);
            return;
        }
        type = eNegativeZero;
    } else if (isnan(data)) {
        type = eNotANumber;
    } else if (!finite(data)) {
        if (data > 0) {
            type = ePositiveInfinity;
        } else {
            type = eNegativeInfinity;
        }
    }
    else
#endif
    if (m_FastWriteDouble) {
        width = (int)NStr::DoubleToStringPosix(data, digits, buffer, sizeof(buffer));
    } else {
        int shift = 0;
        int precision = int(digits - shift);
        if ( precision < 0 )
            precision = 0;
        else if ( size_t(precision) > kMaxDoubleLength ) // limit precision of data
            precision = int(kMaxDoubleLength);

        // ensure buffer is large enough to fit result
        // (additional bytes are for sign, dot and exponent)
        width = sprintf(buffer, "%.*g", precision, data);
        if ( width <= 0 || width >= int(sizeof(buffer) - 1) )
            ThrowError(fOverflow, "buffer overflow");
        _ASSERT(strlen(buffer) == size_t(width));
        char* dot = strchr(buffer,',');
        if (dot) {
            *dot = '.'; // enforce C locale
        }
    }

    WriteLength(width + 1);
    if (width) {
#if 1
        type = eDecimal_NR1;
        for (const char* p = buffer + width - 1;  p >= buffer;  --p) {
            if (*p == '.') {
                type = eDecimal_NR2;
                break;
            } else if (*p == 'e'  ||  *p == 'E') {
                type = eDecimal_NR3;
                break;
            }
        }
#endif
        WriteByte(type);
        WriteBytes(buffer, width);
    } else {
        WriteByte(type);
    }
}

void CObjectOStreamAsnBinary::WriteDouble(double data)
{
    WriteDouble2(data, DBL_DIG);
}

void CObjectOStreamAsnBinary::WriteFloat(float data)
{
    WriteDouble2(data, FLT_DIG);
}

void CObjectOStreamAsnBinary::WriteString(const string& str, EStringType type)
{
    size_t length = str.size();
    WriteStringTag(type);
    WriteLength(length);
    if ( type == eStringTypeVisible && x_FixCharsMethod() != eFNP_Allow ) {
        size_t done = 0;
        for ( size_t i = 0; i < length; ++i ) {
            char c = str[i];
            if ( !GoodVisibleChar(c) ) {
                if ( i > done ) {
                    WriteBytes(str.data() + done, i - done);
                }
                FixVisibleChar(c, x_FixCharsMethod(), this, str);
                WriteByte(c);
                done = i + 1;
            }
        }
        if ( done < length ) {
            WriteBytes(str.data() + done, length - done);
        }
    }
    else {
        WriteBytes(str.data(), length);
    }
}

void CObjectOStreamAsnBinary::WriteStringStore(const string& str)
{
    WriteShortTag(eApplication, ePrimitive, eStringStore);
    size_t length = str.size();
    WriteLength(length);
    WriteBytes(str.data(), length);
}

void CObjectOStreamAsnBinary::CopyStringValue(CObjectIStreamAsnBinary& in,
                                              bool checkVisible)
{
    size_t length = in.ReadLength();
    WriteLength(length);
    while ( length > 0 ) {
        char buffer[1024];
        size_t c = min(length, sizeof(buffer));
        in.ReadBytes(buffer, c);
        if ( checkVisible ) {
            // Check the string for non-printable characters
            for (size_t i = 0; i < c; i++) {
                if ( !GoodVisibleChar(buffer[i]) ) {
                    FixVisibleChar(buffer[i], x_FixCharsMethod(), this, string(buffer,c));
                }
            }
        }
        WriteBytes(buffer, c);
        length -= c;
    }
    in.EndOfTag();
}

void CObjectOStreamAsnBinary::CopyString(CObjectIStream& in,
                                         EStringType type)
{
    // do we need to check symbols while copying?
    // x_FixCharsMethod() != eFNP_Allow, type == eStringTypeVisible
    const bool checkVisible = false;
    WriteStringTag(type);
    if ( in.GetDataFormat() == eSerial_AsnBinary ) {
        CObjectIStreamAsnBinary& bIn =
            *CTypeConverter<CObjectIStreamAsnBinary>::SafeCast(&in);
        bIn.ExpectStringTag(type);
        CopyStringValue(bIn, checkVisible);
    }
    else {
        string str;
        in.ReadString(str, type);
        size_t length = str.size();
        if ( checkVisible ) {
            // Check the string for non-printable characters
            NON_CONST_ITERATE(string, i, str) {
                if ( !GoodVisibleChar(*i) ) {
                    FixVisibleChar(*i, x_FixCharsMethod(), this, str);
                }
            }
        }
        WriteLength(length);
        WriteBytes(str.data(), length);
    }
}

void CObjectOStreamAsnBinary::CopyStringStore(CObjectIStream& in)
{
    WriteShortTag(eApplication, ePrimitive, eStringStore);
    if ( in.GetDataFormat() == eSerial_AsnBinary ) {
        CObjectIStreamAsnBinary& bIn =
            *CTypeConverter<CObjectIStreamAsnBinary>::SafeCast(&in);
        bIn.ExpectSysTag(eApplication, ePrimitive, eStringStore);
        CopyStringValue(bIn);
    }
    else {
        string str;
        in.ReadStringStore(str);
        size_t length = str.size();
        WriteLength(length);
        WriteBytes(str.data(), length);
    }
}

void CObjectOStreamAsnBinary::WriteCString(const char* str)
{
    if ( str == 0 ) {
        WriteSysTag(eNull);
        WriteShortLength(0);
    }
    else {
        size_t length = strlen(str);
        WriteSysTag(eVisibleString);
        WriteLength(length);
        if ( x_FixCharsMethod() != eFNP_Allow ) {
            size_t done = 0;
            for ( size_t i = 0; i < length; ++i ) {
                char c = str[i];
                if ( !GoodVisibleChar(c) ) {
                    if ( i > done ) {
                        WriteBytes(str + done, i - done);
                    }
                    FixVisibleChar(c, x_FixCharsMethod(), this, string(str,length));
                    WriteByte(c);
                    done = i + 1;
                }
            }
            if ( done < length ) {
                WriteBytes(str + done, length - done);
            }
        }
        else {
            WriteBytes(str, length);
        }
    }
}

void CObjectOStreamAsnBinary::WriteEnum(const CEnumeratedTypeValues& values,
                                        TEnumValueType value)
{
    if ( values.IsInteger() ) {
        WriteSysTag(eInteger);
    }
    else {
        values.FindName(value, false); // check value
        WriteSysTag(eEnumerated);
    }
    WriteNumberValue(value);
}

void CObjectOStreamAsnBinary::CopyEnum(const CEnumeratedTypeValues& values,
                                       CObjectIStream& in)
{
    TEnumValueType value = in.ReadEnum(values);
    if ( values.IsInteger() )
        WriteSysTag(eInteger);
    else
        WriteSysTag(eEnumerated);
    WriteNumberValue(value);
}

void CObjectOStreamAsnBinary::WriteObjectReference(TObjectIndex index)
{
    WriteTag(eApplication, ePrimitive, eObjectReference);
    if ( sizeof(TObjectIndex) == sizeof(Int4) )
        WriteNumberValue(Int4(index));
    else if ( sizeof(TObjectIndex) == sizeof(Int8) )
        WriteNumberValue(Int8(index));
    else
        ThrowError(fIllegalCall, "invalid size of TObjectIndex"
            "must be either sizeof(Int4) or sizeof(Int8)");
}

void CObjectOStreamAsnBinary::WriteNullPointer(void)
{
    WriteSysTag(eNull);
    WriteShortLength(0);
}

void CObjectOStreamAsnBinary::WriteOtherBegin(TTypeInfo typeInfo)
{
    WriteClassTag(typeInfo);
    WriteIndefiniteLength();
}

void CObjectOStreamAsnBinary::WriteOtherEnd(TTypeInfo /*typeInfo*/)
{
    WriteEndOfContent();
}

void CObjectOStreamAsnBinary::WriteOther(TConstObjectPtr object,
                                         TTypeInfo typeInfo)
{
    WriteClassTag(typeInfo);
    WriteIndefiniteLength();
    WriteObject(object, typeInfo);
    WriteEndOfContent();
}


#ifdef VIRTUAL_MID_LEVEL_IO

void CObjectOStreamAsnBinary::BeginNamedType(TTypeInfo namedTypeInfo)
{
#if !USE_OLD_TAGS
    bool need_eoc = false;
#if USE_VERIFY_TAGGING
    m_AutomaticTagging = namedTypeInfo->GetTagType() == CAsnBinaryDefs::eAutomatic;
#endif
    if (namedTypeInfo->HasTag()) {
#if USE_VERIFY_TAGGING
        if (m_AutomaticTagging) {
            ThrowError(fInvalidData, "ASN TAGGING ERROR. Report immediately!");
        }
#endif
        if (!m_SkipNextTag) {
            need_eoc = namedTypeInfo->IsTagConstructed();
            WriteTag(namedTypeInfo->GetTagClass(),
                     namedTypeInfo->GetTagConstructed(),
                     namedTypeInfo->GetTag());
            if (need_eoc) {
                WriteIndefiniteLength();
            }
        }
        m_SkipNextTag = namedTypeInfo->IsTagImplicit();
    }
    TopFrame().SetNoEOC(!need_eoc);
#endif
}

void CObjectOStreamAsnBinary::EndNamedType(void)
{
#if !USE_OLD_TAGS
    if (!TopFrame().GetNoEOC()) {
        WriteEndOfContent();
    }
#endif
}

void CObjectOStreamAsnBinary::WriteNamedType(
    TTypeInfo namedTypeInfo,TTypeInfo objectType, TConstObjectPtr objectPtr)
{

#if USE_OLD_TAGS
#ifndef VIRTUAL_MID_LEVEL_IO
    BEGIN_OBJECT_FRAME2(eFrameNamed, namedTypeInfo);
    BeginNamedType(namedTypeInfo);
#endif
    WriteObject(objectPtr, objectType);
#ifndef VIRTUAL_MID_LEVEL_IO
    EndNamedType();
    END_OBJECT_FRAME();
#endif
#else //USE_OLD_TAGS

    bool need_eoc = false;
#if USE_VERIFY_TAGGING
    m_AutomaticTagging = namedTypeInfo->GetTagType() == CAsnBinaryDefs::eAutomatic;
#endif
    if (namedTypeInfo->HasTag()) {
#if USE_VERIFY_TAGGING
        if (m_AutomaticTagging) {
            ThrowError(fInvalidData, "ASN TAGGING ERROR. Report immediately!");
        }
#endif
        if (!m_SkipNextTag) {
            need_eoc = namedTypeInfo->IsTagConstructed();
            WriteTag(namedTypeInfo->GetTagClass(),
                     namedTypeInfo->GetTagConstructed(),
                     namedTypeInfo->GetTag());
            if (need_eoc) {
                WriteIndefiniteLength();
            }
        }
        m_SkipNextTag = namedTypeInfo->IsTagImplicit();
    }
    WriteObject(objectPtr, objectType);
    if (need_eoc) {
        WriteEndOfContent();
    }
#endif // USE_OLD_TAGS
}

void CObjectOStreamAsnBinary::CopyNamedType(
    TTypeInfo namedTypeInfo,TTypeInfo objectType,CObjectStreamCopier& copier)
{
    BEGIN_OBJECT_2FRAMES_OF2(copier, eFrameNamed, namedTypeInfo);
    copier.In().BeginNamedType(namedTypeInfo);
    BeginNamedType(namedTypeInfo);

    CopyObject(objectType, copier);

    EndNamedType();
    copier.In().EndNamedType();
    END_OBJECT_2FRAMES_OF(copier);
}


void CObjectOStreamAsnBinary::BeginContainer(const CContainerTypeInfo* cType)
{
#if USE_OLD_TAGS
    WriteByte(MakeContainerTagByte(cType->RandomElementsOrder()));
    WriteIndefiniteLength();
#else
    _ASSERT(cType->HasTag());
    _ASSERT(cType->IsTagConstructed());
#if 0 //USE_VERIFY_TAGGING
    m_AutomaticTagging = cType->GetTagType() == CAsnBinaryDefs::eAutomatic;
#endif
    bool need_eoc = !m_SkipNextTag;
    if (!m_SkipNextTag) {
        WriteTag(cType->GetTagClass(), eConstructed, cType->GetTag());
        WriteIndefiniteLength();
    }
#if USE_VERIFY_TAGGING
    else if (m_AutomaticTagging) {
        ThrowError(fInvalidData, "ASN TAGGING ERROR. Report immediately!");
    }
#endif
    m_SkipNextTag = cType->IsTagImplicit();
    TopFrame().SetNoEOC(!need_eoc);
#endif
}

void CObjectOStreamAsnBinary::EndContainer(void)
{
#if USE_OLD_TAGS
    WriteEndOfContent();
#else
    if (!TopFrame().GetNoEOC()) {
        WriteEndOfContent();
    }
#endif
}

void CObjectOStreamAsnBinary::WriteContainer(const CContainerTypeInfo* cType,
                                             TConstObjectPtr containerPtr)
{
#if USE_OLD_TAGS
    WriteByte(MakeContainerTagByte(cType->RandomElementsOrder()));
    WriteIndefiniteLength();
#else
    BEGIN_OBJECT_FRAME2(eFrameArray, cType);
    _ASSERT(cType->HasTag());
    _ASSERT(cType->IsTagConstructed());
#if 0 //USE_VERIFY_TAGGING
    m_AutomaticTagging = cType->GetTagType() == CAsnBinaryDefs::eAutomatic;
#endif
    bool need_eoc = !m_SkipNextTag;
    if (!m_SkipNextTag) {
        WriteTag(cType->GetTagClass(), eConstructed, cType->GetTag());
        WriteIndefiniteLength();
    }
#if USE_VERIFY_TAGGING
    else if (m_AutomaticTagging) {
        ThrowError(fInvalidData, "ASN TAGGING ERROR. Report immediately!");
    }
#endif
    m_SkipNextTag = cType->IsTagImplicit();
#endif
    
    CContainerTypeInfo::CConstIterator i;
    if ( cType->InitIterator(i, containerPtr) ) {
        TTypeInfo elementType = cType->GetElementType();
        BEGIN_OBJECT_FRAME2(eFrameArrayElement, elementType);

        const CPointerTypeInfo* pointerType =
            dynamic_cast<const CPointerTypeInfo*>(elementType);
        do {
            TConstObjectPtr elementPtr = cType->GetElementPtr(i);
            if ( pointerType &&
                 !pointerType->GetObjectPointer(elementPtr) ) {
                if ( GetVerifyData() == eSerialVerifyData_Yes ) {
                    ThrowError(fUnassigned,
                               "NULL element while writing container "+
                               cType->GetName());
                }
                continue;
            }

            WriteObject(elementPtr, elementType);

        } while ( cType->NextElement(i) );

        END_OBJECT_FRAME();
    }

#if USE_OLD_TAGS
    WriteEndOfContent();
#else
    if (need_eoc) {
        WriteEndOfContent();
    }
    END_OBJECT_FRAME();
#endif
}

void CObjectOStreamAsnBinary::CopyContainer(const CContainerTypeInfo* cType,
                                            CObjectStreamCopier& copier)
{
#if USE_OLD_TAGS
    BEGIN_OBJECT_FRAME_OF2(copier.In(), eFrameArray, cType);
    copier.In().BeginContainer(cType);
    WriteByte(MakeContainerTagByte(cType->RandomElementsOrder()));
    WriteIndefiniteLength();
#else
    BEGIN_OBJECT_2FRAMES_OF2(copier, eFrameArray, cType);
    copier.In().BeginContainer(cType);
    CObjectOStreamAsnBinary::BeginContainer(cType);
#endif

    TTypeInfo elementType = cType->GetElementType();
    BEGIN_OBJECT_2FRAMES_OF2(copier, eFrameArrayElement, elementType);

    while ( copier.In().BeginContainerElement(elementType) ) {

        CopyObject(elementType, copier);

        copier.In().EndContainerElement();
    }

    END_OBJECT_2FRAMES_OF(copier);
    
#if USE_OLD_TAGS
    WriteEndOfContent();
    copier.In().EndContainer();
    END_OBJECT_FRAME_OF(copier.In());
#else
    CObjectOStreamAsnBinary::EndContainer();
    copier.In().EndContainer();
    END_OBJECT_2FRAMES_OF(copier);
#endif

}

#endif

void CObjectOStreamAsnBinary::BeginClass(const CClassTypeInfo* classType)
{
#if USE_OLD_TAGS
    WriteByte(MakeContainerTagByte(classType->RandomOrder()));
    WriteIndefiniteLength();
#else
    _ASSERT(classType->HasTag());
    _ASSERT(classType->IsTagConstructed());
#if USE_VERIFY_TAGGING
    m_AutomaticTagging = classType->GetTagType() == CAsnBinaryDefs::eAutomatic;
#endif
    bool need_eoc = !m_SkipNextTag;
    if (!m_SkipNextTag) {
        WriteTag(classType->GetTagClass(), eConstructed, classType->GetTag());
        WriteIndefiniteLength();
    }
#if USE_VERIFY_TAGGING
    else if (m_AutomaticTagging) {
        ThrowError(fInvalidData, "ASN TAGGING ERROR. Report immediately!");
    }
#endif
    m_SkipNextTag = classType->IsTagImplicit();
    TopFrame().SetNoEOC(!need_eoc);
#endif
}

void CObjectOStreamAsnBinary::EndClass(void)
{
#if USE_OLD_TAGS
    WriteEndOfContent();
#else
    if (!TopFrame().GetNoEOC()) {
        WriteEndOfContent();
    }
#endif
}

void CObjectOStreamAsnBinary::BeginClassMember(const CMemberId& id)
{
#if USE_OLD_TAGS
    WriteTag(eContextSpecific, eConstructed, id.GetTag());
    WriteIndefiniteLength();
#else
    if (id.HasTag()) {
        WriteTag(id.GetTagClass(), id.GetTagConstructed(), id.GetTag());
        if (id.IsTagConstructed()) {
            WriteIndefiniteLength();
        }
    }
#if USE_VERIFY_TAGGING
    else if (m_AutomaticTagging) {
        ThrowError(fInvalidData, "ASN TAGGING ERROR. Report immediately!");
    }
#endif
    m_SkipNextTag = id.HasTag() && id.IsTagImplicit();
#endif
}

void CObjectOStreamAsnBinary::EndClassMember(void)
{
#if USE_OLD_TAGS
    WriteEndOfContent();
#else
    const CMemberId& id = TopFrame().GetMemberId();
    if (id.HasTag() && id.IsTagConstructed()) {
        WriteEndOfContent();
    }
#endif
}

#ifdef VIRTUAL_MID_LEVEL_IO
void CObjectOStreamAsnBinary::WriteClass(const CClassTypeInfo* classType,
                                         TConstObjectPtr classPtr)
{
#if USE_OLD_TAGS
    WriteByte(MakeContainerTagByte(classType->RandomOrder()));
    WriteIndefiniteLength();
#else
    BEGIN_OBJECT_FRAME2(eFrameClass, classType);
    _ASSERT(classType->HasTag());
    _ASSERT(classType->IsTagConstructed());
#if USE_VERIFY_TAGGING
    m_AutomaticTagging = classType->GetTagType() == CAsnBinaryDefs::eAutomatic;
#endif
    bool need_eoc = !m_SkipNextTag;
    if (!m_SkipNextTag) {
        WriteTag(classType->GetTagClass(), eConstructed, classType->GetTag());
        WriteIndefiniteLength();
    }
#if USE_VERIFY_TAGGING
    else if (m_AutomaticTagging) {
        ThrowError(fInvalidData, "ASN TAGGING ERROR. Report immediately!");
    }
#endif
    m_SkipNextTag = classType->IsTagImplicit();
#endif
    
    for ( CClassTypeInfo::CIterator i(classType); i.Valid(); ++i ) {
        classType->GetMemberInfo(i)->WriteMember(*this, classPtr);
    }
    
#if USE_OLD_TAGS
    WriteEndOfContent();
#else
    if (need_eoc) {
        WriteEndOfContent();
    }
    END_OBJECT_FRAME();
#endif
}

void CObjectOStreamAsnBinary::WriteClassMember(const CMemberId& memberId,
                                               TTypeInfo memberType,
                                               TConstObjectPtr memberPtr)
{
    BEGIN_OBJECT_FRAME2(eFrameClassMember, memberId);
#if USE_OLD_TAGS
    WriteTag(eContextSpecific, eConstructed, memberId.GetTag());
    WriteIndefiniteLength();
#else
    bool need_eoc = false;
    if (memberId.HasTag()) {
        WriteTag(memberId.GetTagClass(), memberId.GetTagConstructed(), memberId.GetTag());
        need_eoc = memberId.IsTagConstructed();
        if (need_eoc) {
            WriteIndefiniteLength();
        }
    }
#if USE_VERIFY_TAGGING
    else if (m_AutomaticTagging) {
        ThrowError(fInvalidData, "ASN TAGGING ERROR. Report immediately!");
    }
#endif
    m_SkipNextTag = memberId.HasTag() && memberId.IsTagImplicit();
#endif
    
    WriteObject(memberPtr, memberType);
    
#if USE_OLD_TAGS
    WriteEndOfContent();
#else
    if (need_eoc) {
        WriteEndOfContent();
    }
#endif
    END_OBJECT_FRAME();
}

bool CObjectOStreamAsnBinary::WriteClassMember(const CMemberId& memberId,
                                               const CDelayBuffer& buffer)
{
    if ( !buffer.HaveFormat(eSerial_AsnBinary) )
        return false;

    BEGIN_OBJECT_FRAME2(eFrameClassMember, memberId);
#if USE_OLD_TAGS
    WriteTag(eContextSpecific, eConstructed, memberId.GetTag());
    WriteIndefiniteLength();
#else
    bool need_eoc = false;
    if (memberId.HasTag()) {
        WriteTag(memberId.GetTagClass(), memberId.GetTagConstructed(), memberId.GetTag());
        need_eoc = memberId.IsTagConstructed();
        if (need_eoc) {
            WriteIndefiniteLength();
        }
    }
#if USE_VERIFY_TAGGING
    else if (m_AutomaticTagging) {
        ThrowError(fInvalidData, "ASN TAGGING ERROR. Report immediately!");
    }
#endif
    m_SkipNextTag = memberId.HasTag() && memberId.IsTagImplicit();
#endif
    
    Write(buffer.GetSource());
    
#if USE_OLD_TAGS
    WriteEndOfContent();
#else
    if (need_eoc) {
        WriteEndOfContent();
    }
#endif
    END_OBJECT_FRAME();

    return true;
}

void CObjectOStreamAsnBinary::CopyClassRandom(const CClassTypeInfo* classType,
                                              CObjectStreamCopier& copier)
{
#if USE_OLD_TAGS
    BEGIN_OBJECT_FRAME_OF2(copier.In(), eFrameClass, classType);
    copier.In().BeginClass(classType);
    WriteByte(MakeContainerTagByte(classType->RandomOrder()));
    WriteIndefiniteLength();
#else
    BEGIN_OBJECT_2FRAMES_OF2(copier, eFrameClass, classType);
    copier.In().BeginClass(classType);
    CObjectOStreamAsnBinary::BeginClass(classType);
#endif

    vector<Uint1> read(classType->GetMembers().LastIndex() + 1);

    BEGIN_OBJECT_2FRAMES_OF(copier, eFrameClassMember);

    TMemberIndex index;
    while ( (index = copier.In().BeginClassMember(classType)) !=
            kInvalidMember ) {
        const CMemberInfo* memberInfo = classType->GetMemberInfo(index);
        copier.In().SetTopMemberId(memberInfo->GetId());
        SetTopMemberId(memberInfo->GetId());

        if ( read[index] ) {
            copier.DuplicatedMember(memberInfo);
        }
        else {
            read[index] = true;

#if USE_OLD_TAGS
            WriteTag(eContextSpecific,
                     eConstructed,
                     memberInfo->GetId().GetTag());
            WriteIndefiniteLength();
#else
            CObjectOStreamAsnBinary::BeginClassMember(memberInfo->GetId());
#endif

            memberInfo->CopyMember(copier);

#if USE_OLD_TAGS
            WriteEndOfContent();
#else
            CObjectOStreamAsnBinary::EndClassMember();
#endif
        }
        
        copier.In().EndClassMember();
    }

    END_OBJECT_2FRAMES_OF(copier);

    // init all absent members
    for ( CClassTypeInfo::CIterator i(classType); i.Valid(); ++i ) {
        if ( !read[*i] ) {
            classType->GetMemberInfo(i)->CopyMissingMember(copier);
        }
    }

#if USE_OLD_TAGS
    WriteEndOfContent();
    copier.In().EndClass();
    END_OBJECT_FRAME_OF(copier.In());
#else
    CObjectOStreamAsnBinary::EndClass();
    copier.In().EndClass();
    END_OBJECT_2FRAMES_OF(copier);
#endif
}

void CObjectOStreamAsnBinary::CopyClassSequential(const CClassTypeInfo* classType,
                                                  CObjectStreamCopier& copier)
{
#if USE_OLD_TAGS
    BEGIN_OBJECT_FRAME_OF2(copier.In(), eFrameClass, classType);
    copier.In().BeginClass(classType);
    WriteByte(MakeContainerTagByte(classType->RandomOrder()));
    WriteIndefiniteLength();
#else
    BEGIN_OBJECT_2FRAMES_OF2(copier, eFrameClass, classType);
    copier.In().BeginClass(classType);
    CObjectOStreamAsnBinary::BeginClass(classType);
#endif
    
    CClassTypeInfo::CIterator pos(classType);
    BEGIN_OBJECT_2FRAMES_OF(copier, eFrameClassMember);

    TMemberIndex index;
    while ( (index = copier.In().BeginClassMember(classType, *pos)) !=
            kInvalidMember ) {
        const CMemberInfo* memberInfo = classType->GetMemberInfo(index);
        copier.In().SetTopMemberId(memberInfo->GetId());
        SetTopMemberId(memberInfo->GetId());

        for ( TMemberIndex i = *pos; i < index; ++i ) {
            // init missing member
            classType->GetMemberInfo(i)->CopyMissingMember(copier);
        }

#if USE_OLD_TAGS
        WriteTag(eContextSpecific, eConstructed, memberInfo->GetId().GetTag());
        WriteIndefiniteLength();
#else
            CObjectOStreamAsnBinary::BeginClassMember(memberInfo->GetId());
#endif

        memberInfo->CopyMember(copier);

#if USE_OLD_TAGS
        WriteEndOfContent();
#else
        CObjectOStreamAsnBinary::EndClassMember();
#endif
        
        pos.SetIndex(index + 1);

        copier.In().EndClassMember();
    }

    END_OBJECT_2FRAMES_OF(copier);

    // init all absent members
    for ( ; pos.Valid(); ++pos ) {
        classType->GetMemberInfo(pos)->CopyMissingMember(copier);
    }

#if USE_OLD_TAGS
    WriteEndOfContent();
    copier.In().EndClass();
    END_OBJECT_FRAME_OF(copier.In());
#else
    CObjectOStreamAsnBinary::EndClass();
    copier.In().EndClass();
    END_OBJECT_2FRAMES_OF(copier);
#endif
}
#endif

void CObjectOStreamAsnBinary::BeginChoice(const CChoiceTypeInfo* choiceType)
{
    if (choiceType->GetVariantInfo(kFirstMemberIndex)->GetId().IsAttlist()) {
        TopFrame().SetNotag();
        WriteByte(MakeContainerTagByte(false));
        WriteIndefiniteLength();
    }
}

void CObjectOStreamAsnBinary::EndChoice(void)
{
    if (TopFrame().GetNotag()) {
        WriteEndOfContent();
    }
}

void CObjectOStreamAsnBinary::BeginChoiceVariant(const CChoiceTypeInfo* ,
                                                 const CMemberId& id)
{
    if (FetchFrameFromTop(1).GetNotag()) {
        WriteTag(eContextSpecific, eConstructed, kFirstMemberIndex);
        WriteIndefiniteLength();
        WriteTag(eContextSpecific, eConstructed, id.GetTag()-1);
        WriteIndefiniteLength();
    } else {
#if USE_OLD_TAGS
        WriteTag(eContextSpecific, eConstructed, id.GetTag());
        WriteIndefiniteLength();
#else
        if (id.HasTag()) {
            WriteTag(id.GetTagClass(), id.GetTagConstructed(), id.GetTag());
            if (id.IsTagConstructed()) {
                WriteIndefiniteLength();
            }
        }
#if USE_VERIFY_TAGGING
        else if (m_AutomaticTagging) {
            ThrowError(fInvalidData, "ASN TAGGING ERROR. Report immediately!");
        }
#endif
        m_SkipNextTag = id.HasTag() && id.IsTagImplicit();
#endif
    }
}

void CObjectOStreamAsnBinary::EndChoiceVariant(void)
{
    if (FetchFrameFromTop(1).GetNotag()) {
        WriteEndOfContent();
    }
#if USE_OLD_TAGS
    WriteEndOfContent();
#else
    const CMemberId& id = TopFrame().GetMemberId();
    if (id.HasTag() && id.IsTagConstructed()) {
        WriteEndOfContent();
    }
#endif
}

void CObjectOStreamAsnBinary::BeginBytes(const ByteBlock& block)
{
    WriteSysTag(eOctetString);
    WriteLength(block.GetLength());
}

void CObjectOStreamAsnBinary::WriteBytes(const ByteBlock& ,
                                         const char* bytes, size_t length)
{
    WriteBytes(bytes, length);
}

void CObjectOStreamAsnBinary::BeginChars(const CharBlock& block)
{
    if ( block.GetLength() == 0 ) {
        WriteSysTag(eNull);
        WriteShortLength(0);
        return;
    }
    WriteSysTag(eVisibleString);
    WriteLength(block.GetLength());
}


void CObjectOStreamAsnBinary::WriteChars(const CharBlock& ,
                                         const char* str, size_t length)
{
    if ( x_FixCharsMethod() != eFNP_Allow ) {
        size_t done = 0;
        for ( size_t i = 0; i < length; ++i ) {
            char c = str[i];
            if ( !GoodVisibleChar(c) ) {
                if ( i > done ) {
                    WriteBytes(str + done, i - done);
                }
                FixVisibleChar(c, x_FixCharsMethod(), this, string(str,length));
                WriteByte(c);
                done = i + 1;
            }
        }
        if ( done < length ) {
            WriteBytes(str + done, length - done);
        }
    }
    else {
        WriteBytes(str, length);
    }
}


END_NCBI_SCOPE
