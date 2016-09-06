#if defined(OBJISTRASNB__HPP)  &&  !defined(OBJISTRASNB__INL)
#define OBJISTRASNB__INL

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
*/


inline
CObjectIStreamAsnBinary::TByte
CObjectIStreamAsnBinary::PeekByte(size_t index)
{
    return TByte(m_Input.PeekChar(index));
}

inline
CObjectIStreamAsnBinary::TByte
CObjectIStreamAsnBinary::PeekTagByte(size_t index)
{
#if CHECK_INSTREAM_STATE
    if ( m_CurrentTagState != eTagStart)
        ThrowError(fIllegalCall,
            "illegal PeekTagByte call: only allowed at tag start");
#endif
    return TByte(m_Input.PeekChar(index));
}

inline
CObjectIStreamAsnBinary::TByte
CObjectIStreamAsnBinary::StartTag(TByte first_tag_byte)
{
#if CHECK_INSTREAM_STATE
    if ( m_CurrentTagLength != 0 )
        ThrowError(fIllegalCall,
            "illegal StartTag call: current tag length != 0");
#endif
    _ASSERT(PeekTagByte() == first_tag_byte);
    return first_tag_byte;
}

inline
void CObjectIStreamAsnBinary::EndOfTag(void)
{
#if CHECK_INSTREAM_STATE
    if ( m_CurrentTagState != eData )
        ThrowError(fIllegalCall, "illegal EndOfTag call");
    m_CurrentTagState = eTagStart;
#endif
#if CHECK_INSTREAM_LIMITS
    // check for all bytes read
    if ( m_CurrentTagLimit != 0 ) {
#if CHECK_INSTREAM_STATE
        if ( m_Input.GetStreamPosAsInt8() != m_CurrentTagLimit ) {
            ThrowError(fIllegalCall,
                       "illegal EndOfTag call: not all data bytes read");
        }
#endif
        // restore tag limit from stack
        if ( m_Limits.empty() ) {
            m_CurrentTagLimit = 0;
        }
        else {
            m_CurrentTagLimit = m_Limits.top();
            m_Limits.pop();
        }
        _ASSERT(m_CurrentTagLimit == 0);
    }
#endif
    m_CurrentTagLength = 0;
}

inline
Uint1 CObjectIStreamAsnBinary::ReadByte(void)
{
#if CHECK_INSTREAM_STATE
    if ( m_CurrentTagState != eData )
        ThrowError(fIllegalCall, "illegal ReadByte call");
#endif
#if CHECK_INSTREAM_LIMITS
    if ( m_CurrentTagLimit != 0 &&
         m_Input.GetStreamPosAsInt8() >= m_CurrentTagLimit )
        ThrowError(fOverflow, "tag size overflow");
#endif
    return Uint1(m_Input.GetChar());
}

inline
void CObjectIStreamAsnBinary::ExpectSysTagByte(TByte byte)
{
    if ( StartTag(PeekTagByte()) != byte )
        UnexpectedSysTagByte(byte);
    m_CurrentTagLength = 1;
#if CHECK_INSTREAM_STATE
    m_CurrentTagState = eTagParsed;
#endif
}

inline
void CObjectIStreamAsnBinary::ExpectSysTag(ETagClass tag_class,
                                           ETagConstructed tag_constructed,
                                           ETagValue tag_value)
{
    _ASSERT(tag_value != eLongTag);
    ExpectSysTagByte(MakeTagByte(tag_class, tag_constructed, tag_value));
}

inline
void CObjectIStreamAsnBinary::ExpectSysTag(ETagValue tag_value)
{
    if (m_SkipNextTag) {
        m_SkipNextTag = false;
        return;
    }
    _ASSERT(tag_value != eLongTag);
    ExpectSysTagByte(MakeTagByte(eUniversal, ePrimitive, tag_value));
}

inline
void CObjectIStreamAsnBinary::ExpectIntegerTag(void)
{
    if (m_SkipNextTag) {
        m_SkipNextTag = false;
        return;
    }
    Uint1 tag = StartTag(PeekTagByte());
    Uint1 exp = MakeTagByte(  eUniversal, ePrimitive, eInteger);
    if (tag != exp) {
        if (tag != MakeTagByte(eApplication, ePrimitive, eInteger)) {
            UnexpectedSysTagByte(exp);
        }
        SetSpecialCaseUsed(CObjectIStream::eReadAsBigInt);
    }
    m_CurrentTagLength = 1;
#if CHECK_INSTREAM_STATE
    m_CurrentTagState = eTagParsed;
#endif
}

inline
void CObjectIStreamAsnBinary::ExpectTagClassByte(TByte first_tag_byte,
                                                 TByte expected_class_byte)
{
    if ( GetTagClassAndConstructed(first_tag_byte) != expected_class_byte ) {
        UnexpectedTagClassByte(first_tag_byte, expected_class_byte);
    }
}

inline
CObjectIStreamAsnBinary::TLongTag
CObjectIStreamAsnBinary::PeekTag(TByte first_tag_byte)
{
    TByte byte = StartTag(first_tag_byte);
    ETagValue sysTag = GetTagValue(byte);
    if ( sysTag == eLongTag ) {
        return PeekLongTag();
    }
#if CHECK_INSTREAM_STATE
    m_CurrentTagState = eTagParsed;
#endif
    m_CurrentTagLength = 1;
    return sysTag;
}

inline
CObjectIStreamAsnBinary::TLongTag
CObjectIStreamAsnBinary::PeekTag(TByte first_tag_byte,
                                 ETagClass tag_class,
                                 ETagConstructed tag_constructed)
{
    ExpectTagClassByte(first_tag_byte,
                       MakeTagClassAndConstructed(tag_class, tag_constructed));
    return PeekTag(first_tag_byte);
}

inline
void CObjectIStreamAsnBinary::ExpectTag(
    ETagClass tag_class, ETagConstructed tag_constructed, TLongTag tag_value)
{
    ExpectTag( PeekTagByte(), tag_class, tag_constructed, tag_value);
}

inline
void CObjectIStreamAsnBinary::ExpectTag(TByte first_tag_byte,
    ETagClass tag_class, ETagConstructed tag_constructed, TLongTag tag_value)
{
    ExpectTagClassByte(first_tag_byte,
                       MakeTagClassAndConstructed(tag_class, tag_constructed));
    TLongTag tag = PeekTag(first_tag_byte);
    if (tag != tag_value) {
        UnexpectedTagValue( tag_class, tag, tag_value);
    }
}
inline
void CObjectIStreamAsnBinary::UndoPeekTag(void)
{
#if CHECK_INSTREAM_STATE
    m_CurrentTagState = eTagStart;
#endif
    m_CurrentTagLength = 0;
}

inline
Int1 CObjectIStreamAsnBinary::ReadSByte(void)
{
    return Int1(ReadByte());
}

inline
void CObjectIStreamAsnBinary::ExpectByte(Uint1 byte)
{
    if ( ReadByte() != byte )
        UnexpectedByte(byte);
}

inline
bool CObjectIStreamAsnBinary::HaveMoreElements(void)
{
#if USE_DEF_LEN
    if (m_CurrentDataLimit != 0) {
        return m_CurrentDataLimit > m_Input.GetStreamPosAsInt8();
    }
#endif
    return PeekTagByte() != eEndOfContentsByte;
}

inline
CObjectIStreamAsnBinary::TByte
CObjectIStreamAsnBinary::FlushTag(void)
{
#if CHECK_INSTREAM_STATE
    if ( m_CurrentTagState != eTagParsed || m_CurrentTagLength == 0 )
        ThrowError(fIllegalCall, "illegal FlushTag call");
    m_CurrentTagState = eLengthValue;
#endif
    m_Input.SkipChars(m_CurrentTagLength);
    return TByte(m_Input.GetChar());
}

inline
bool CObjectIStreamAsnBinary::PeekIndefiniteLength(void)
{
#if CHECK_INSTREAM_STATE
    if ( m_CurrentTagState != eTagParsed )
        ThrowError(fIllegalCall, "illegal PeekIndefiniteLength call");
#endif
    return TByte(m_Input.PeekChar(m_CurrentTagLength)) == eIndefiniteLengthByte;
}

inline
void CObjectIStreamAsnBinary::ExpectIndefiniteLength(void)
{
    // indefinite length allowed only for constructed tags
#if CHECK_INSTREAM_LIMITS
    if ( !IsTagConstructed(m_Input.PeekChar()) )
        ThrowError(fIllegalCall, "illegal ExpectIndefiniteLength call");
    _ASSERT(m_CurrentTagLimit == 0);
    // save tag limit
    // tag limit is not changed
    if ( m_CurrentTagLimit != 0 ) {
        m_Limits.push(m_CurrentTagLimit);
    }
#endif
#if USE_DEF_LEN
    TByte len = FlushTag();
    m_DataLimits.push_back(m_CurrentDataLimit);
    if (len == eIndefiniteLengthByte) {
        m_CurrentDataLimit = 0;
    } else  if (len > eIndefiniteLengthByte) {
        m_CurrentDataLimit = m_Input.GetStreamPosAsInt8() + ReadLengthLong(len);
#if CHECK_INSTREAM_LIMITS
        m_CurrentTagLimit = 0;
#endif
    } else {
        m_CurrentDataLimit = m_Input.GetStreamPosAsInt8() + len;
    }
#else
    if ( FlushTag() != eIndefiniteLengthByte ) {
        UnexpectedFixedLength();
    }
#endif
#if CHECK_INSTREAM_STATE
    m_CurrentTagState = eTagStart;
#endif
    m_CurrentTagLength = 0;
}

inline
size_t CObjectIStreamAsnBinary::StartTagData(size_t length)
{
#if CHECK_INSTREAM_LIMITS
    Int8 cur_pos = m_Input.GetStreamPosAsInt8();
    Int8 newLimit = cur_pos + length;
    _ASSERT(newLimit >= cur_pos);
    _ASSERT(newLimit != 0);
    Int8 currentLimit = m_CurrentTagLimit;
    if ( currentLimit != 0 ) {
        if ( newLimit > currentLimit ) {
            ThrowError(fOverflow, "nested data length overflow");
        }
        m_Limits.push(currentLimit);
    }
    m_CurrentTagLimit = newLimit;
#endif
#if CHECK_INSTREAM_STATE
    m_CurrentTagState = eData;
#endif
    return length;
}

inline
size_t CObjectIStreamAsnBinary::ReadShortLength(void)
{
    TByte byte = FlushTag();
    if ( byte >= 0x80 ) {
        UnexpectedLongLength();
    }
    return StartTagData(byte);
}

inline
size_t CObjectIStreamAsnBinary::ReadLengthInlined(void)
{
    TByte byte = FlushTag();
    if ( byte >= 0x80 ) {
        return ReadLengthLong(byte);
    }
    return StartTagData(byte);
}

inline
void CObjectIStreamAsnBinary::ExpectContainer(bool random)
{
    ExpectSysTagByte(MakeContainerTagByte(random));
    ExpectIndefiniteLength();
}

inline
void CObjectIStreamAsnBinary::ExpectShortLength(size_t length)
{
    size_t got = ReadShortLength();
    if ( got != length ) {
        UnexpectedShortLength(got, length);
    }
}

inline
void CObjectIStreamAsnBinary::ExpectEndOfContent(void)
{
#if CHECK_INSTREAM_STATE
    if ( m_CurrentTagState != eTagStart || m_CurrentTagLength != 0 )
        ThrowError(fIllegalCall, "illegal ExpectEndOfContent call");
#endif
#if USE_DEF_LEN
    if (m_CurrentDataLimit != 0) {
        if (m_CurrentDataLimit != m_Input.GetStreamPosAsInt8()) {
            UnexpectedContinuation();
        }
    } else {
        if ( !m_Input.SkipExpectedChars(char(eEndOfContentsByte),
                                        char(eZeroLengthByte)) ) {
            UnexpectedContinuation();
        }
    }
    m_CurrentDataLimit = m_DataLimits.back();
    m_DataLimits.pop_back();
#else
    if ( !m_Input.SkipExpectedChars(char(eEndOfContentsByte),
                                    char(eZeroLengthByte)) ) {
        UnexpectedContinuation();
    }
#endif
#if CHECK_INSTREAM_LIMITS
    // restore tag limit from stack
    _ASSERT(m_CurrentTagLimit == 0);
    if ( m_CurrentTagLimit != 0 ) {
        if ( m_Limits.empty() ) {
            m_CurrentTagLimit = 0;
        }
        else {
            m_CurrentTagLimit = m_Limits.top();
            m_Limits.pop();
        }
        _ASSERT(m_CurrentTagLimit == 0);
    }
#endif
#if CHECK_INSTREAM_STATE
    m_CurrentTagState = eTagStart;
#endif
    m_CurrentTagLength = 0;
}

#endif /* def OBJISTRASNB__HPP  &&  ndef OBJISTRASNB__INL */
