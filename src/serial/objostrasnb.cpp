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

#include <serial/objostrasnb.hpp>
#include <serial/objistr.hpp>
#include <serial/objcopy.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/memberid.hpp>
#include <serial/enumvalues.hpp>
#include <serial/memberlist.hpp>
#include <serial/objhook.hpp>
#include <serial/classinfo.hpp>
#include <serial/choice.hpp>
#include <serial/continfo.hpp>
#include <serial/delaybuf.hpp>

#include <stdio.h>
#include <math.h>

#undef _TRACE
#define _TRACE(arg) ((void)0)

BEGIN_NCBI_SCOPE

CObjectOStream* CObjectOStream::OpenObjectOStreamAsnBinary(CNcbiOstream& out,
                                                           bool deleteOut)
{
    return new CObjectOStreamAsnBinary(out, deleteOut);
}

CObjectOStreamAsnBinary::CObjectOStreamAsnBinary(CNcbiOstream& out,
                                                 EFixNonPrint how)
    : CObjectOStream(eSerial_AsnBinary, out), m_FixMethod(how)
{
#if CHECK_STREAM_INTEGRITY
    m_CurrentPosition = 0;
    m_CurrentTagState = eTagStart;
    m_CurrentTagLimit = numeric_limits<CNcbiStreamoff>::max();
#endif
}

CObjectOStreamAsnBinary::CObjectOStreamAsnBinary(CNcbiOstream& out,
                                                 bool deleteOut,
                                                 EFixNonPrint how)
    : CObjectOStream(eSerial_AsnBinary, out, deleteOut), m_FixMethod(how)
{
#if CHECK_STREAM_INTEGRITY
    m_CurrentPosition = 0;
    m_CurrentTagState = eTagStart;
    m_CurrentTagLimit = numeric_limits<CNcbiStreamoff>::max();
#endif
}

CObjectOStreamAsnBinary::~CObjectOStreamAsnBinary(void)
{
#if CHECK_STREAM_INTEGRITY
    if ( !m_Limits.empty() || m_CurrentTagState != eTagStart )
        ERR_POST("CObjectOStreamAsnBinary not finished");
#endif
}

#if CHECK_STREAM_INTEGRITY
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
    CNcbiStreamoff limit = m_CurrentPosition + 1 + length;
    if ( limit <= m_CurrentPosition || limit > m_CurrentTagLimit )
        ThrowError(fIllegalCall, "tag will overflow enclosing tag");
    else
        m_CurrentTagLimit = limit;
    if ( GetTagConstructed(m_CurrentTagCode) ) // constructed
        m_CurrentTagState = eTagStart;
    else
        m_CurrentTagState = eData;
    if ( length == 0 )
        EndTag();
}
#endif

#if !CHECK_STREAM_INTEGRITY
inline
#endif
void CObjectOStreamAsnBinary::WriteByte(Uint1 byte)
{
#if CHECK_STREAM_INTEGRITY
    //_TRACE("WriteByte: " << NStr::PtrToString(byte));
    if ( m_CurrentPosition >= m_CurrentTagLimit )
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
            ThrowError(fFormatError, "first byte of length is zero");
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
        if ( m_CurrentPosition + 1 == m_CurrentTagLimit )
            EndTag();
        break;
    }
    m_CurrentPosition += 1;
#endif
    m_Output.PutChar(byte);
}

#if !CHECK_STREAM_INTEGRITY
inline
#endif
void CObjectOStreamAsnBinary::WriteBytes(const char* bytes, size_t size)
{
    if ( size == 0 )
        return;
#if CHECK_STREAM_INTEGRITY
    //_TRACE("WriteBytes: " << size);
    if ( m_CurrentTagState != eData )
        ThrowError(fFormatError, "WriteBytes only allowed in DATA");
    CNcbiStreamoff new_pos = m_CurrentPosition + size;
    if ( new_pos < m_CurrentPosition || new_pos > m_CurrentTagLimit )
        ThrowError(fFormatError, "tag DATA overflow");
    m_CurrentPosition = new_pos;
    if ( new_pos == m_CurrentTagLimit )
        EndTag();
#endif
    m_Output.PutString(bytes, size);
}

template<typename T>
void WriteBytesOf(CObjectOStreamAsnBinary& out, const T& value, size_t count)
{
    for ( size_t shift = (count - 1) * 8; shift > 0; shift -= 8 ) {
        out.WriteByte(Uint1(value >> shift));
    }
    out.WriteByte(Uint1(value));
}

inline
void CObjectOStreamAsnBinary::WriteShortTag(ETagClass tag_class,
                                            ETagConstructed tag_constructed,
                                            ETagValue tag_value)
{
    WriteByte(MakeTagByte(tag_class, tag_constructed, tag_value));
}

inline
void CObjectOStreamAsnBinary::WriteSysTag(ETagValue tag_value)
{
    WriteShortTag(eUniversal, ePrimitive, tag_value);
}

void CObjectOStreamAsnBinary::WriteLongTag(ETagClass tag_class,
                                           ETagConstructed tag_constructed,
                                           TLongTag tag_value)
{
    if ( tag_value <= 0 )
        ThrowError(fFormatError, "negative tag number");
    
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
    if ( tag_value >= 0 && tag_value < eLongTag )
        WriteShortTag(tag_class, tag_constructed, ETagValue(tag_value));
    else
        WriteLongTag(tag_class, tag_constructed, tag_value);
}

void CObjectOStreamAsnBinary::WriteClassTag(TTypeInfo typeInfo)
{
    const string& tag = typeInfo->GetName();
    if ( tag.empty() )
        ThrowError(fFormatError, "empty tag string");

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
    WriteBytesOf(*this, length, count);
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
    NCBI_THROW(CSerialException,eNotImplemented,
        "CObjectOStreamAsnBinary::WriteAnyContentObject: "
        "unable to write AnyContent object in ASN");
}

void CObjectOStreamAsnBinary::CopyAnyContentObject(CObjectIStream& )
{
    NCBI_THROW(CSerialException,eNotImplemented,
        "CObjectOStreamAsnBinary::CopyAnyContentObject: "
        "unable to copy AnyContent object in ASN");
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
    WriteBytesOf(*this, data, length);
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
    WriteBytesOf(*this, data, length);
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
            WriteBytesOf(*this, data, sizeof(data));
            return;
        }
        else {
            // full length
            length = sizeof(data);
        }
    }
    WriteShortLength(length);
    WriteBytesOf(*this, data, length);
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
            WriteBytesOf(*this, data, sizeof(data));
            return;
        }
        else {
            // full length
            length = sizeof(data);
        }
    }
    WriteShortLength(length);
    WriteBytesOf(*this, data, length);
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
    WriteSysTag(eInteger);
    WriteNumberValue(data);
}

void CObjectOStreamAsnBinary::WriteUint8(Uint8 data)
{
    WriteSysTag(eInteger);
    WriteNumberValue(data);
}

static const size_t kMaxDoubleLength = 64;

void CObjectOStreamAsnBinary::WriteDouble2(double data, size_t digits)
{
    if (isnan(data)) {
        ThrowError(fInvalidData, "invalid double: not a number");
    }
    if (!finite(data)) {
        ThrowError(fInvalidData, "invalid double: infinite");
    }
    int shift = int(ceil(log10(fabs(data))));
    int precision = int(digits - shift);
    if ( precision < 0 )
        precision = 0;
    else if ( size_t(precision) > kMaxDoubleLength ) // limit precision of data
        precision = int(kMaxDoubleLength);

    // ensure buffer is large enough to fit result
    // (additional bytes are for sign, dot and exponent)
    char buffer[kMaxDoubleLength + 16];
    int width = sprintf(buffer, "%.*g", precision, data);
    if ( width <= 0 || width >= int(sizeof(buffer) - 1) )
        ThrowError(fOverflow, "buffer overflow");
    _ASSERT(strlen(buffer) == size_t(width));

    WriteSysTag(eReal);
    WriteLength(width + 1);
    WriteByte(eDecimal);
    WriteBytes(buffer, width);
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
    WriteSysTag(eVisibleString);
    WriteLength(length);
    if ( type == eStringTypeVisible && m_FixMethod != eFNP_Allow ) {
        size_t done = 0;
        for ( size_t i = 0; i < length; ++i ) {
            char c = str[i];
            if ( !GoodVisibleChar(c) ) {
                if ( i > done ) {
                    WriteBytes(str.data() + done, i - done);
                }
                FixVisibleChar(c, m_FixMethod);
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
                FixVisibleChar(buffer[i], m_FixMethod);
            }
        }
        WriteBytes(buffer, c);
        length -= c;
    }
    in.EndOfTag();
}

void CObjectOStreamAsnBinary::CopyString(CObjectIStream& in)
{
    WriteSysTag(eVisibleString);
    if ( in.GetDataFormat() == eSerial_AsnBinary ) {
        CObjectIStreamAsnBinary& bIn =
            *CTypeConverter<CObjectIStreamAsnBinary>::SafeCast(&in);
        bIn.ExpectSysTag(eVisibleString);
        CopyStringValue(bIn, true);
    }
    else {
        string str;
        in.ReadStd(str);
        size_t length = str.size();
        if ( m_FixMethod != eFNP_Allow ) {
            // Check the string for non-printable characters
            NON_CONST_ITERATE(string, i, str) {
                FixVisibleChar(*i, m_FixMethod);
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
        if ( m_FixMethod != eFNP_Allow ) {
            size_t done = 0;
            for ( size_t i = 0; i < length; ++i ) {
                char c = str[i];
                if ( !GoodVisibleChar(c) ) {
                    if ( i > done ) {
                        WriteBytes(str + done, i - done);
                    }
                    FixVisibleChar(c, m_FixMethod);
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
            "must be either sizeof(Int4) or sizeof(Int4)");
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

void CObjectOStreamAsnBinary::BeginContainer(const CContainerTypeInfo* cType)
{
    WriteByte(MakeContainerTagByte(cType->RandomElementsOrder()));
    WriteIndefiniteLength();
}

void CObjectOStreamAsnBinary::EndContainer(void)
{
    WriteEndOfContent();
}

#ifdef VIRTUAL_MID_LEVEL_IO
void CObjectOStreamAsnBinary::WriteContainer(const CContainerTypeInfo* cType,
                                             TConstObjectPtr containerPtr)
{
    WriteByte(MakeContainerTagByte(cType->RandomElementsOrder()));
    WriteIndefiniteLength();
    
    CContainerTypeInfo::CConstIterator i;
    if ( cType->InitIterator(i, containerPtr) ) {
        TTypeInfo elementType = cType->GetElementType();
        BEGIN_OBJECT_FRAME2(eFrameArrayElement, elementType);

        do {
            if (elementType->GetTypeFamily() == eTypeFamilyPointer) {
                const CPointerTypeInfo* pointerType =
                    CTypeConverter<CPointerTypeInfo>::SafeCast(elementType);
                _ASSERT(pointerType->GetObjectPointer(cType->GetElementPtr(i)));
                if ( !pointerType->GetObjectPointer(cType->GetElementPtr(i)) ) {
                    ERR_POST(Warning << " NULL pointer found in container: skipping");
                    continue;
                }
            }
            WriteObject(cType->GetElementPtr(i), elementType);

        } while ( cType->NextElement(i) );

        END_OBJECT_FRAME();
    }

    WriteEndOfContent();
}

void CObjectOStreamAsnBinary::CopyContainer(const CContainerTypeInfo* cType,
                                            CObjectStreamCopier& copier)
{
    BEGIN_OBJECT_FRAME_OF2(copier.In(), eFrameArray, cType);
    copier.In().BeginContainer(cType);

    WriteByte(MakeContainerTagByte(cType->RandomElementsOrder()));
    WriteIndefiniteLength();

    TTypeInfo elementType = cType->GetElementType();
    BEGIN_OBJECT_2FRAMES_OF2(copier, eFrameArrayElement, elementType);

    while ( copier.In().BeginContainerElement(elementType) ) {

        CopyObject(elementType, copier);

        copier.In().EndContainerElement();
    }

    END_OBJECT_2FRAMES_OF(copier);
    
    WriteEndOfContent();

    copier.In().EndContainer();
    END_OBJECT_FRAME_OF(copier.In());
}

#endif

void CObjectOStreamAsnBinary::BeginClass(const CClassTypeInfo* classType)
{
    WriteByte(MakeContainerTagByte(classType->RandomOrder()));
    WriteIndefiniteLength();
}

void CObjectOStreamAsnBinary::EndClass(void)
{
    WriteEndOfContent();
}

void CObjectOStreamAsnBinary::BeginClassMember(const CMemberId& id)
{
    WriteTag(eContextSpecific, eConstructed, id.GetTag());
    WriteIndefiniteLength();
}

void CObjectOStreamAsnBinary::EndClassMember(void)
{
    WriteEndOfContent();
}

#ifdef VIRTUAL_MID_LEVEL_IO
void CObjectOStreamAsnBinary::WriteClass(const CClassTypeInfo* classType,
                                         TConstObjectPtr classPtr)
{
    WriteByte(MakeContainerTagByte(classType->RandomOrder()));
    WriteIndefiniteLength();
    
    for ( CClassTypeInfo::CIterator i(classType); i.Valid(); ++i ) {
        classType->GetMemberInfo(i)->WriteMember(*this, classPtr);
    }
    
    WriteEndOfContent();
}

void CObjectOStreamAsnBinary::WriteClassMember(const CMemberId& memberId,
                                               TTypeInfo memberType,
                                               TConstObjectPtr memberPtr)
{
    BEGIN_OBJECT_FRAME2(eFrameClassMember, memberId);
    WriteTag(eContextSpecific, eConstructed, memberId.GetTag());
    WriteIndefiniteLength();
    
    WriteObject(memberPtr, memberType);
    
    WriteEndOfContent();
    END_OBJECT_FRAME();
}

bool CObjectOStreamAsnBinary::WriteClassMember(const CMemberId& memberId,
                                               const CDelayBuffer& buffer)
{
    if ( !buffer.HaveFormat(eSerial_AsnBinary) )
        return false;

    BEGIN_OBJECT_FRAME2(eFrameClassMember, memberId);
    WriteTag(eContextSpecific, eConstructed, memberId.GetTag());
    WriteIndefiniteLength();
    
    Write(buffer.GetSource());
    
    WriteEndOfContent();
    END_OBJECT_FRAME();

    return true;
}

void CObjectOStreamAsnBinary::CopyClassRandom(const CClassTypeInfo* classType,
                                              CObjectStreamCopier& copier)
{
    BEGIN_OBJECT_FRAME_OF2(copier.In(), eFrameClass, classType);
    copier.In().BeginClass(classType);

    WriteByte(MakeContainerTagByte(classType->RandomOrder()));
    WriteIndefiniteLength();

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

            WriteTag(eContextSpecific,
                     eConstructed,
                     memberInfo->GetId().GetTag());
            WriteIndefiniteLength();

            memberInfo->CopyMember(copier);

            WriteEndOfContent();
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

    WriteEndOfContent();

    copier.In().EndClass();
    END_OBJECT_FRAME_OF(copier.In());
}

void CObjectOStreamAsnBinary::CopyClassSequential(const CClassTypeInfo* classType,
                                                  CObjectStreamCopier& copier)
{
    BEGIN_OBJECT_FRAME_OF2(copier.In(), eFrameClass, classType);
    copier.In().BeginClass(classType);

    WriteByte(MakeContainerTagByte(classType->RandomOrder()));
    WriteIndefiniteLength();
    
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

        WriteTag(eContextSpecific, eConstructed, memberInfo->GetId().GetTag());
        WriteIndefiniteLength();

        memberInfo->CopyMember(copier);

        WriteEndOfContent();
        
        pos.SetIndex(index + 1);

        copier.In().EndClassMember();
    }

    END_OBJECT_2FRAMES_OF(copier);

    // init all absent members
    for ( ; pos.Valid(); ++pos ) {
        classType->GetMemberInfo(pos)->CopyMissingMember(copier);
    }

    WriteEndOfContent();

    copier.In().EndClass();
    END_OBJECT_FRAME_OF(copier.In());
}
#endif

void CObjectOStreamAsnBinary::BeginChoiceVariant(const CChoiceTypeInfo* ,
                                                 const CMemberId& id)
{
    WriteTag(eContextSpecific, eConstructed, id.GetTag());
    WriteIndefiniteLength();
}

void CObjectOStreamAsnBinary::EndChoiceVariant(void)
{
    WriteEndOfContent();
}

#ifdef VIRTUAL_MID_LEVEL_IO
void CObjectOStreamAsnBinary::WriteChoice(const CChoiceTypeInfo* choiceType,
                                          TConstObjectPtr choicePtr)
{
    TMemberIndex index = choiceType->GetIndex(choicePtr);
    const CVariantInfo* variantInfo = choiceType->GetVariantInfo(index);
    BEGIN_OBJECT_FRAME2(eFrameChoiceVariant, variantInfo->GetId());
    WriteTag(eContextSpecific, eConstructed, variantInfo->GetId().GetTag());
    WriteIndefiniteLength();
    
    variantInfo->WriteVariant(*this, choicePtr);
    
    WriteEndOfContent();
    END_OBJECT_FRAME();
}

void CObjectOStreamAsnBinary::CopyChoice(const CChoiceTypeInfo* choiceType,
                                         CObjectStreamCopier& copier)
{
    BEGIN_OBJECT_FRAME_OF2(copier.In(), eFrameChoice, choiceType);
    copier.In().BeginChoice(choiceType);
    BEGIN_OBJECT_2FRAMES_OF(copier, eFrameChoiceVariant);
    TMemberIndex index = copier.In().BeginChoiceVariant(choiceType);
    if ( index == kInvalidMember ) {
        copier.ThrowError(CObjectIStream::fFormatError,
                          "choice variant id expected");
    }

    const CVariantInfo* variantInfo = choiceType->GetVariantInfo(index);
    copier.In().SetTopMemberId(variantInfo->GetId());
    copier.Out().SetTopMemberId(variantInfo->GetId());
    WriteTag(eContextSpecific, eConstructed, variantInfo->GetId().GetTag());
    WriteIndefiniteLength();

    variantInfo->CopyVariant(copier);

    WriteEndOfContent();

    copier.In().EndChoiceVariant();
    END_OBJECT_2FRAMES_OF(copier);
    copier.In().EndChoice();
    END_OBJECT_FRAME_OF(copier.In());
}
#endif

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
    if ( m_FixMethod != eFNP_Allow ) {
        size_t done = 0;
        for ( size_t i = 0; i < length; ++i ) {
            char c = str[i];
            if ( !GoodVisibleChar(c) ) {
                if ( i > done ) {
                    WriteBytes(str + done, i - done);
                }
                FixVisibleChar(c, m_FixMethod);
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

/*
* ===========================================================================
*
* $Log$
* Revision 1.94  2005/04/27 17:02:17  vasilche
* Converted namespace CObjectStreamAsnBinaryDefs to class CAsnBinaryDefs.
* Used enums to represent ASN.1 constants whenever possible.
* Use vector<Uint1> instead of inefficient vector<bool>.
*
* Revision 1.93  2005/04/26 14:55:48  vasilche
* Use named constant for indefinite length byte.
*
* Revision 1.92  2004/09/01 14:22:53  ucko
* Tweak for our LFS-enabled version of GCC 3.3.3, which doesn't provide
* ++ or -- for streamoff.
*
* Revision 1.91  2004/08/30 18:19:39  gouriano
* Use CNcbiStreamoff instead of size_t for stream offset operations
*
* Revision 1.90  2004/08/04 14:45:25  vasilche
* Fixed wrong constant.
*
* Revision 1.89  2004/06/08 20:26:05  gouriano
* Made functions, that are not visible from the outside, protected
*
* Revision 1.88  2004/05/17 21:03:03  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.87  2004/05/04 17:04:43  gouriano
* Check double for being finite
*
* Revision 1.86  2004/01/12 22:39:33  ucko
* Drop trailing-zero removal code altogether; it could still mangle
* other cases, and %g doesn't seem to generate any extra zeros.
*
* Revision 1.85  2004/01/12 22:35:37  ucko
* WriteDouble2: represent 0 as "0", not ""!
*
* Revision 1.84  2004/01/05 14:25:21  gouriano
* Added possibility to set serialization hooks by stack path
*
* Revision 1.83  2003/12/23 19:36:24  ucko
* Use "g" rather than "f" format for REALs to improve range.
* Move CVS log to end.
*
* Revision 1.82  2003/11/26 19:59:41  vasilche
* GetPosition() and GetDataFormat() methods now are implemented
* in parent classes CObjectIStream and CObjectOStream to avoid
* pure virtual method call in destructors.
*
* Revision 1.81  2003/10/24 15:54:28  grichenk
* Removed or blocked exceptions in destructors
*
* Revision 1.80  2003/10/15 17:50:55  vasilche
* Fixed interger overflow when binary asn output stream excedes 2GB.
*
* Revision 1.79  2003/08/19 18:32:38  vasilche
* Optimized reading and writing strings.
* Avoid string reallocation when checking char values.
* Try to reuse old string data when string reference counting is not working.
*
* Revision 1.78  2003/08/13 15:47:45  gouriano
* implemented serialization of AnyContent objects
*
* Revision 1.77  2003/05/22 20:10:02  gouriano
* added UTF8 strings
*
* Revision 1.76  2003/05/16 18:02:18  gouriano
* revised exception error messages
*
* Revision 1.75  2003/04/10 20:13:39  vakatov
* Rollback the "uninitialized member" verification -- it still needs to
* be worked upon...
*
* Revision 1.73  2003/03/26 16:14:23  vasilche
* Removed TAB symbols. Some formatting.
*
* Revision 1.72  2003/02/04 17:06:26  gouriano
* added check for NaN in WriteDouble
*
* Revision 1.71  2002/12/13 21:50:41  gouriano
* corrected reading of choices
*
* Revision 1.70  2002/11/14 20:59:48  gouriano
* added BeginChoice/EndChoice methods
*
* Revision 1.69  2002/10/25 14:49:27  vasilche
* NCBI C Toolkit compatibility code extracted to libxcser library.
* Serial streams flags names were renamed to fXxx.
*
* Names of flags
*
* Revision 1.68  2002/10/08 18:59:38  grichenk
* Check for null pointers in containers (assert in debug mode,
* warning in release).
*
* Revision 1.67  2002/08/30 16:22:22  vasilche
* Removed excessive _TRACEs
*
* Revision 1.66  2002/08/08 14:15:23  grichenk
* Fixed WriteByte()
*
* Revision 1.65  2002/01/17 20:52:35  grichenk
* Fixed another bug in long binary tags processing
*
* Revision 1.64  2002/01/14 17:58:11  grichenk
* Fixed long tags processing
*
* Revision 1.63  2001/12/10 16:57:24  grichenk
* Fixed WriteLongTag()
*
* Revision 1.62  2001/12/09 07:45:08  vakatov
* Fixed a warning
*
* Revision 1.61  2001/10/17 20:41:25  grichenk
* Added CObjectOStream::CharBlock class
*
* Revision 1.60  2001/06/07 17:12:51  grichenk
* Redesigned checking and substitution of non-printable characters
* in VisibleString
*
* Revision 1.59  2001/05/17 15:07:08  lavr
* Typos corrected
*
* Revision 1.58  2001/04/25 20:41:53  vakatov
* <limits.h>, <float.h>  --->  <corelib/ncbi_limits.h>
*
* Revision 1.57  2001/01/03 15:22:27  vasilche
* Fixed limited buffer size for REAL data in ASN.1 binary format.
* Fixed processing non ASCII symbols in ASN.1 text format.
*
* Revision 1.56  2000/12/26 23:01:54  vakatov
* Another tiny Mac specific fix
*
* Revision 1.55  2000/12/26 22:24:14  vasilche
* Fixed errors of compilation on Mac.
*
* Revision 1.54  2000/12/15 21:29:03  vasilche
* Moved some typedefs/enums from corelib/ncbistd.hpp.
* Added flags to CObjectIStream/CObjectOStream: eFlagAllowNonAsciiChars.
* TByte typedef replaced by Uint1.
*
* Revision 1.53  2000/12/15 15:38:45  vasilche
* Added support of Int8 and long double.
* Enum values now have type Int4 instead of long.
*
* Revision 1.52  2000/12/04 19:02:41  beloslyu
* changes for FreeBSD
*
* Revision 1.51  2000/10/20 15:51:43  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* Revision 1.50  2000/10/13 20:22:55  vasilche
* Fixed warnings on 64 bit compilers.
* Fixed missing typename in templates.
*
* Revision 1.49  2000/10/13 16:28:40  vasilche
* Reduced header dependency.
* Avoid use of templates with virtual methods.
* Reduced amount of different maps used.
* All this lead to smaller compiled code size (libraries and programs).
*
* Revision 1.48  2000/10/05 15:52:50  vasilche
* Avoid using snprintf because it's missing on osf1_gcc
*
* Revision 1.47  2000/10/05 13:17:17  vasilche
* Added missing #include <stdio.h>
*
* Revision 1.46  2000/10/04 19:18:59  vasilche
* Fixed processing floating point data.
*
* Revision 1.45  2000/10/03 17:22:45  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.44  2000/09/29 16:18:24  vasilche
* Fixed binary format encoding/decoding on 64 bit compulers.
* Implemented CWeakMap<> for automatic cleaning map entries.
* Added cleaning local hooks via CWeakMap<>.
* Renamed ReadTypeName -> ReadFileHeader, ENoTypeName -> ENoFileHeader.
* Added some user interface methods to CObjectIStream, CObjectOStream and
* CObjectStreamCopier.
*
* Revision 1.43  2000/09/26 17:38:22  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.42  2000/09/18 20:00:25  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.41  2000/09/01 13:16:19  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.40  2000/08/15 19:44:50  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.39  2000/07/03 18:42:46  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.38  2000/06/16 16:31:21  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.37  2000/06/07 19:46:00  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.36  2000/06/01 19:07:05  vasilche
* Added parsing of XML data.
*
* Revision 1.35  2000/05/24 20:08:48  vasilche
* Implemented XML dump.
*
* Revision 1.34  2000/05/09 16:38:39  vasilche
* CObject::GetTypeInfo now moved to CObjectGetTypeInfo::GetTypeInfo to reduce possible errors.
* Added write context to CObjectOStream.
* Inlined most of methods of helping class Member, Block, ByteBlock etc.
*
* Revision 1.33  2000/04/28 16:58:14  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* Revision 1.32  2000/04/13 14:50:28  vasilche
* Added CObjectIStream::Open() and CObjectOStream::Open() for easier use.
*
* Revision 1.31  2000/04/06 16:11:00  vasilche
* Fixed bug with iterators in choices.
* Removed unneeded calls to ReadExternalObject/WriteExternalObject.
* Added output buffering to text ASN.1 data.
*
* Revision 1.30  2000/03/29 15:55:29  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* Revision 1.29  2000/03/10 21:16:47  vasilche
* Removed EOF workaround code.
*
* Revision 1.28  2000/03/10 17:59:20  vasilche
* Fixed error reporting.
* Added EOF bug workaround on MIPSpro compiler (not finished).
*
* Revision 1.27  2000/01/10 19:46:42  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.26  1999/12/28 18:55:51  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.25  1999/12/17 19:05:04  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.24  1999/10/21 15:44:22  vasilche
* Added helper class CNcbiOstreamToString to convert CNcbiOstrstream buffer
* to string.
*
* Revision 1.23  1999/10/05 14:08:34  vasilche
* Fixed class name under GCC and MSVC.
*
* Revision 1.22  1999/09/24 18:19:19  vasilche
* Removed dependency on NCBI toolkit.
*
* Revision 1.21  1999/09/24 14:21:52  vasilche
* Fixed usage of strstreams.
*
* Revision 1.20  1999/09/23 21:16:09  vasilche
* Removed dependence on asn.h
*
* Revision 1.19  1999/09/23 18:57:01  vasilche
* Fixed bugs with overloaded methods in objistr*.hpp & objostr*.hpp
*
* Revision 1.18  1999/09/22 20:11:55  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.17  1999/08/16 16:08:31  vasilche
* Added ENUMERATED type.
*
* Revision 1.16  1999/08/13 20:22:58  vasilche
* Fixed lot of bugs in datatool
*
* Revision 1.15  1999/08/13 15:53:52  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.14  1999/07/26 18:31:39  vasilche
* Implemented skipping of unused values.
* Added more useful error report.
*
* Revision 1.13  1999/07/22 20:36:38  vasilche
* Fixed 'using namespace' declaration for MSVC.
*
* Revision 1.12  1999/07/22 19:40:59  vasilche
* Fixed bug with complex object graphs (pointers to members of other objects).
*
* Revision 1.11  1999/07/22 17:33:57  vasilche
* Unified reading/writing of objects in all three formats.
*
* Revision 1.10  1999/07/21 20:02:57  vasilche
* Added embedding of ASN.1 binary output from ToolKit to our binary format.
* Fixed bugs with storing pointers into binary ASN.1
*
* Revision 1.9  1999/07/21 14:20:08  vasilche
* Added serialization of bool.
*
* Revision 1.8  1999/07/19 15:50:38  vasilche
* Added interface to old ASN.1 routines.
* Added naming of key/value in STL map.
*
* Revision 1.7  1999/07/09 20:27:08  vasilche
* Fixed some bugs
*
* Revision 1.6  1999/07/09 16:32:54  vasilche
* Added OCTET STRING write/read.
*
* Revision 1.5  1999/07/07 21:15:03  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.4  1999/07/02 21:32:00  vasilche
* Implemented reading from ASN.1 binary format.
*
* Revision 1.3  1999/07/01 17:55:35  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.2  1999/06/30 16:05:01  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.1  1999/06/24 14:44:59  vasilche
* Added binary ASN.1 output.
*
* ===========================================================================
*/
