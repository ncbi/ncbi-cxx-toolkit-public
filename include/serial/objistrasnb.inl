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

#if !CHECK_STREAM_INTEGRITY
inline
Uint1
CObjectIStreamAsnBinary::PeekTagByte(size_t index)
{
    return static_cast<Uint1>(m_Input.PeekChar(index));
}

inline
Uint1
CObjectIStreamAsnBinary::StartTag(Uint1 first_tag_byte)
{
    return first_tag_byte;
}

inline
Uint1 CObjectIStreamAsnBinary::FlushTag(void)
{
    m_Input.SkipChars(m_CurrentTagLength);
    return static_cast<Uint1>(m_Input.GetChar());
}

inline
bool CObjectIStreamAsnBinary::PeekIndefiniteLength(void)
{
    return Uint1(m_Input.PeekChar(m_CurrentTagLength)) == 0x80;
}

inline
void CObjectIStreamAsnBinary::ExpectIndefiniteLength(void)
{
    if ( !m_Input.SkipExpectedChar(char(eIndefiniteLengthByte),
                                   m_CurrentTagLength) ) {
        ThrowError(eFormatError, "indefinite length is expected");
    }
}

inline
size_t CObjectIStreamAsnBinary::StartTagData(size_t length)
{
    return length;
}
#endif

inline
void CObjectIStreamAsnBinary::ExpectSysTagByte(Uint1 byte)
{
    if ( StartTag(PeekTagByte()) != byte )
        UnexpectedSysTagByte(byte);
    m_CurrentTagLength = 1;
#if CHECK_STREAM_INTEGRITY
    m_CurrentTagState = eTagParsed;
#endif
}

inline
void CObjectIStreamAsnBinary::ExpectSysTag(EClass c, bool constructed,
                                           ETag tag)
{
    _ASSERT(tag != CObjectStreamAsnBinaryDefs::eLongTag);
    ExpectSysTagByte(CObjectStreamAsnBinaryDefs::MakeTagByte
                     (c, constructed, tag));
}

inline
void CObjectIStreamAsnBinary::ExpectSysTag(ETag tag)
{
    _ASSERT(tag != CObjectStreamAsnBinaryDefs::eLongTag);
    ExpectSysTagByte(CObjectStreamAsnBinaryDefs::MakeTagByte
                     (CObjectStreamAsnBinaryDefs::eUniversal, false, tag));
}

inline
void CObjectIStreamAsnBinary::ExpectContainer(bool random)
{
    ExpectSysTagByte(CObjectStreamAsnBinaryDefs::MakeContainerTagByte(random));
    ExpectIndefiniteLength();
}

inline
void CObjectIStreamAsnBinary::ExpectTagClassByte(Uint1 first_tag_byte,
                                                 Uint1 expected_class_byte)
{
    if ( CObjectStreamAsnBinaryDefs::ExtractClassAndConstructed(first_tag_byte)
         != expected_class_byte ) {
        UnexpectedTagClassByte(first_tag_byte, expected_class_byte);
    }
}

inline
CObjectIStreamAsnBinary::TTag
CObjectIStreamAsnBinary::PeekTag(Uint1 first_tag_byte,
                                 EClass cls, bool constructed)
{
    ExpectTagClassByte(first_tag_byte, MakeTagByte(cls, constructed));
    return PeekTag(first_tag_byte);
}

#if !CHECK_STREAM_INTEGRITY
inline
void CObjectIStreamAsnBinary::EndOfTag(void)
{
}

inline
void CObjectIStreamAsnBinary::ExpectEndOfContent(void)
{
    if ( !m_Input.SkipExpectedChars(0, 0) ) {
        ThrowError(eFormatError, "end of content expected");
    }
    m_CurrentTagLength = 0;
}

inline
Uint1 CObjectIStreamAsnBinary::ReadByte(void)
{
    return static_cast<Uint1>(m_Input.GetChar());
}
#endif

inline
Int1 CObjectIStreamAsnBinary::ReadSByte(void)
{
    return static_cast<Int1>(ReadByte());
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
    return PeekTagByte() != CObjectStreamAsnBinaryDefs::eEndOfContentsByte;
}

#endif /* def OBJISTRASNB__HPP  &&  ndef OBJISTRASNB__INL */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.6  2005/04/26 14:55:48  vasilche
* Use named constant for indefinite length byte.
*
* Revision 1.5  2005/04/26 14:13:27  vasilche
* Optimized binary ASN.1 parsing.
*
* Revision 1.4  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.3  2001/08/15 20:53:02  juran
* Heed warnings.
*
* Revision 1.2  2000/12/15 21:28:47  vasilche
* Moved some typedefs/enums from corelib/ncbistd.hpp.
* Added flags to CObjectIStream/CObjectOStream: eFlagAllowNonAsciiChars.
* TByte typedef replaced by Uint1.
*
* Revision 1.1  2000/09/18 20:00:05  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* ===========================================================================
*/
