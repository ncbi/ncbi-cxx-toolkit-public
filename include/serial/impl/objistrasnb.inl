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
CObjectIStreamAsnBinary::PeekTagByte(size_t index)
{
#if CHECK_INSTREAM_STATE
    if ( m_CurrentTagState != eTagStart )
        ThrowError(fIllegalCall,
            "illegal PeekTagByte call: only allowed at tag start");
#endif
    return TByte(m_Input.PeekChar(index));
}

inline
CObjectIStreamAsnBinary::TByte
CObjectIStreamAsnBinary::StartTag(TByte first_tag_byte)
{
    if ( m_CurrentTagLength != 0 )
        ThrowError(fIllegalCall,
            "illegal StartTag call: current tag length != 0");
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
    if ( m_CurrentTagLimit != 0 &&
         m_Input.GetStreamPosAsInt8() != m_CurrentTagLimit ) {
            ThrowError(fIllegalCall,
                       "illegal EndOfTag call: not all data bytes read");
    }
    // restore tag limit from stack
    m_CurrentTagLimit = m_Limits.top();
    m_Limits.pop();
    _ASSERT(m_CurrentTagLimit == 0);
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
    _ASSERT(tag_value != eLongTag);
    ExpectSysTagByte(MakeTagByte(eUniversal, ePrimitive, tag_value));
}

inline
void CObjectIStreamAsnBinary::ExpectContainer(bool random)
{
    ExpectSysTagByte(MakeContainerTagByte(random));
    ExpectIndefiniteLength();
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
CObjectIStreamAsnBinary::PeekTag(TByte first_tag_byte,
                                 ETagClass tag_class,
                                 ETagConstructed tag_constructed)
{
    ExpectTagClassByte(first_tag_byte,
                       MakeTagClassAndConstructed(tag_class, tag_constructed));
    return PeekTag(first_tag_byte);
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
    return PeekTagByte() != eEndOfContentsByte;
}

#endif /* def OBJISTRASNB__HPP  &&  ndef OBJISTRASNB__INL */
