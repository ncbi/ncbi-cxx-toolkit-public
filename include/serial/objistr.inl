#if defined(OBJISTR__HPP)  &&  !defined(OBJISTR__INL)
#define OBJISTR__INL

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
ESerialDataFormat CObjectIStream::GetDataFormat(void) const
{
    return m_DataFormat;
}


inline
CObjectIStream* CObjectIStream::Open(const string& fileName,
                                     ESerialDataFormat format)
{
    return Open(format, fileName);
}

inline
CObjectIStream::TFailFlags CObjectIStream::GetFailFlags(void) const
{
    return m_Fail;
}

inline
bool CObjectIStream::fail(void) const
{
    return GetFailFlags() != 0;
}

inline
CObjectIStream::TFailFlags CObjectIStream::ClearFailFlags(TFailFlags flags)
{
    TFailFlags old = GetFailFlags();
    m_Fail &= ~flags;
    return old;
}

inline
CObjectIStream::TFlags CObjectIStream::GetFlags(void) const
{
    return m_Flags;
}

inline
CObjectIStream::TFlags CObjectIStream::SetFlags(TFlags flags)
{
    TFlags old = GetFlags();
    m_Flags |= flags;
    return old;
}

inline
CObjectIStream::TFlags CObjectIStream::ClearFlags(TFlags flags)
{
    TFlags old = GetFlags();
    m_Flags &= ~flags;
    return old;
}

inline
void CObjectIStream::ReadObject(TObjectPtr object, TTypeInfo typeInfo)
{
    typeInfo->ReadData(*this, object);
}

inline
void CObjectIStream::SkipObject(TTypeInfo typeInfo)
{
    typeInfo->SkipData(*this);
}

inline
bool CObjectIStream::DetectLoops(void) const
{
    return m_Objects;
}

inline
CObjectIStream& CObjectIStream::ByteBlock::GetStream(void) const
{
    return m_Stream;
}

inline
bool CObjectIStream::ByteBlock::KnownLength(void) const
{
    return m_KnownLength;
}

inline
size_t CObjectIStream::ByteBlock::GetExpectedLength(void) const
{
    return m_Length;
}

inline
void CObjectIStream::ByteBlock::SetLength(size_t length)
{
    m_Length = length;
    m_KnownLength = true;
}

inline
void CObjectIStream::ByteBlock::EndOfBlock(void)
{
    _ASSERT(!KnownLength());
    m_Length = 0;
}

inline
CObjectIStream& CObjectIStream::CharBlock::GetStream(void) const
{
    return m_Stream;
}

inline
bool CObjectIStream::CharBlock::KnownLength(void) const
{
    return m_KnownLength;
}

inline
size_t CObjectIStream::CharBlock::GetExpectedLength(void) const
{
    return m_Length;
}

inline
void CObjectIStream::CharBlock::SetLength(size_t length)
{
    m_Length = length;
    m_KnownLength = true;
}

inline
void CObjectIStream::CharBlock::EndOfBlock(void)
{
    _ASSERT(!KnownLength());
    m_Length = 0;
}

// standard readers
// bool
inline
void CObjectIStream::ReadStd(bool& data)
{
    data = ReadBool();
}

inline
void CObjectIStream::SkipStd(const bool &)
{
    SkipBool();
}

// char
inline
void CObjectIStream::ReadStd(char& data)
{
    data = ReadChar();
}

inline
void CObjectIStream::SkipStd(const char& )
{
    SkipChar();
}

// integer numbers
#if SIZEOF_CHAR == 1
inline
void CObjectIStream::ReadStd(signed char& data)
{
    data = ReadInt1();
}

inline
void CObjectIStream::ReadStd(unsigned char& data)
{
    data = ReadUint1();
}

inline
void CObjectIStream::SkipStd(const signed char& )
{
    SkipInt1();
}

inline
void CObjectIStream::SkipStd(const unsigned char& )
{
    SkipUint1();
}
#else
#  error Unsupported size of char - must be 1
#endif

#if SIZEOF_SHORT == 2
inline
void CObjectIStream::ReadStd(short& data)
{
    data = ReadInt2();
}

inline
void CObjectIStream::ReadStd(unsigned short& data)
{
    data = ReadUint2();
}

inline
void CObjectIStream::SkipStd(const short& )
{
    SkipInt2();
}

inline
void CObjectIStream::SkipStd(const unsigned short& )
{
    SkipUint2();
}
#else
#  error Unsupported size of short - must be 2
#endif

#if SIZEOF_INT == 4
inline
void CObjectIStream::ReadStd(int& data)
{
    data = ReadInt4();
}

inline
void CObjectIStream::ReadStd(unsigned& data)
{
    data = ReadUint4();
}

inline
void CObjectIStream::SkipStd(const int& )
{
    SkipInt4();
}

inline
void CObjectIStream::SkipStd(const unsigned& )
{
    SkipUint4();
}
#else
#  error Unsupported size of int - must be 4
#endif

#if SIZEOF_LONG == 4
inline
void CObjectIStream::ReadStd(long& data)
{
    data = ReadInt4();
}

inline
void CObjectIStream::ReadStd(unsigned long& data)
{
    data = ReadUint4();
}

inline
void CObjectIStream::SkipStd(const long& )
{
    SkipInt4();
}

inline
void CObjectIStream::SkipStd(const unsigned long& )
{
    SkipUint4();
}
#endif

inline
void CObjectIStream::ReadStd(Int8& data)
{
    data = ReadInt8();
}

inline
void CObjectIStream::ReadStd(Uint8& data)
{
    data = ReadUint8();
}

inline
void CObjectIStream::SkipStd(const Int8& )
{
    SkipInt8();
}

inline
void CObjectIStream::SkipStd(const Uint8& )
{
    SkipUint8();
}

// float numbers
inline
void CObjectIStream::ReadStd(float& data)
{
    data = ReadFloat();
}

inline
void CObjectIStream::ReadStd(double& data)
{
    data = ReadDouble();
}

inline
void CObjectIStream::SkipStd(const float& )
{
    SkipFloat();
}

inline
void CObjectIStream::SkipStd(const double& )
{
    SkipDouble();
}

#if SIZEOF_LONG_DOUBLE != 0
inline
void CObjectIStream::ReadStd(long double& data)
{
    data = ReadLDouble();
}

inline
void CObjectIStream::SkipStd(const long double& )
{
    SkipLDouble();
}
#endif

// string
inline
void CObjectIStream::ReadStd(string& data)
{
    ReadString(data);
}

inline
void CObjectIStream::SkipStd(const string& )
{
    SkipString();
}

inline
void CObjectIStream::ReadStd(CStringUTF8& data)
{
    ReadString(data, eStringTypeUTF8);
}

inline
void CObjectIStream::SkipStd(CStringUTF8& )
{
    SkipString(eStringTypeUTF8);
}

// C string
inline
void CObjectIStream::ReadStd(char* & data)
{
    data = ReadCString();
}

inline
void CObjectIStream::ReadStd(const char* & data)
{
    data = ReadCString();
}

inline
void CObjectIStream::SkipStd(char* const& )
{
    SkipCString();
}

inline
void CObjectIStream::SkipStd(const char* const& )
{
    SkipCString();
}


inline
bool GoodVisibleChar(char c)
{
    return c >= ' ' && c <= '~';
}


inline
void FixVisibleChar(char& c, EFixNonPrint fix_method, size_t at_line)
{
    if ( !GoodVisibleChar(c) ) {
        c = ReplaceVisibleChar(c, fix_method, at_line);
    }
}

inline
void CObjectIStream::SetVerifyData(ESerialVerifyData verify)
{
    if (m_VerifyData == eSerialVerifyData_Never ||
        m_VerifyData == eSerialVerifyData_Always ||
        m_VerifyData == eSerialVerifyData_DefValueAlways) {
        return;
    }
    m_VerifyData = (verify == eSerialVerifyData_Default) ?
                   x_GetVerifyDataDefault() : verify;
}

inline
ESerialVerifyData CObjectIStream::GetVerifyData(void) const
{
    switch (m_VerifyData) {
    default:
        break;
    case eSerialVerifyData_No:
    case eSerialVerifyData_Never:
        return eSerialVerifyData_No;
    case eSerialVerifyData_Yes:
    case eSerialVerifyData_Always:
        return eSerialVerifyData_Yes;
    case eSerialVerifyData_DefValue:
    case eSerialVerifyData_DefValueAlways:
        return eSerialVerifyData_DefValue;
    }
    return ms_VerifyDataDefault;
}

inline
void CObjectIStream::SetSkipUnknownMembers(ESerialSkipUnknown skip)
{
    if (m_SkipUnknown == eSerialSkipUnknown_Never ||
        m_SkipUnknown == eSerialSkipUnknown_Always) {
        return;
    }
    m_SkipUnknown = (skip == eSerialSkipUnknown_Default) ?
                    x_GetSkipUnknownDefault() : skip;
}

inline
ESerialSkipUnknown CObjectIStream::GetSkipUnknownMembers(void)
{
    switch (m_SkipUnknown) {
    default:
        break;
    case eSerialSkipUnknown_No:
    case eSerialSkipUnknown_Never:
        return eSerialSkipUnknown_No;
    case eSerialSkipUnknown_Yes:
    case eSerialSkipUnknown_Always:
        return eSerialSkipUnknown_Yes;
    }
    return ms_SkipUnknownDefault;
}

inline
bool CObjectIStream::HaveMoreData(void)
{
    return m_Input.HasMore();
}


inline
CStreamDelayBufferGuard::CStreamDelayBufferGuard(void) 
    : m_ObjectIStream(0) 
{
}


inline
CStreamDelayBufferGuard::CStreamDelayBufferGuard(CObjectIStream& istr) 
    : m_ObjectIStream(&istr) 
{
    istr.StartDelayBuffer();
}


inline
CStreamDelayBufferGuard::~CStreamDelayBufferGuard()
{
    if ( m_ObjectIStream ) {
        m_ObjectIStream->EndDelayBuffer();
    }
}


inline
void CStreamDelayBufferGuard::StartDelayBuffer(CObjectIStream& istr)
{
    if ( m_ObjectIStream ) {
        m_ObjectIStream->EndDelayBuffer();
    }
    m_ObjectIStream = &istr;
    istr.StartDelayBuffer();
}


inline
CRef<CByteSource> CStreamDelayBufferGuard::EndDelayBuffer(void)
{
    CRef<CByteSource> ret;
    if ( m_ObjectIStream ) {
        ret = m_ObjectIStream->EndDelayBuffer();
        m_ObjectIStream = 0;
    }
    return ret;
}


inline
void CStreamDelayBufferGuard::EndDelayBuffer(CDelayBuffer& buffer,
                                             const CItemInfo* itemInfo, 
                                             TObjectPtr objectPtr)
{
    if ( m_ObjectIStream ) {
        m_ObjectIStream->EndDelayBuffer(buffer, itemInfo, objectPtr);
        m_ObjectIStream = 0;
    }
}


#endif /* def OBJISTR__HPP  &&  ndef OBJISTR__INL */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.31  2005/02/23 21:07:44  vasilche
* Allow to skip underlying stream flush.
*
* Revision 1.30  2004/03/23 15:39:52  gouriano
* Added setup options for skipping unknown data members
*
* Revision 1.29  2004/03/05 20:28:37  gouriano
* make it possible to skip unknown data fields
*
* Revision 1.28  2004/02/09 18:21:52  gouriano
* enforced checking environment vars when setting initialization
* verification parameters
*
* Revision 1.27  2003/11/26 19:59:37  vasilche
* GetPosition() and GetDataFormat() methods now are implemented
* in parent classes CObjectIStream and CObjectOStream to avoid
* pure virtual method call in destructors.
*
* Revision 1.26  2003/11/19 15:42:10  vasilche
* Added CObjectIStream::HaveMoreData().
*
* Revision 1.25  2003/11/13 14:06:45  gouriano
* Elaborated data verification on read/write/get to enable skipping mandatory class data members
*
* Revision 1.24  2003/09/24 20:55:13  kuznets
* Implemented CStreamDelayBufferGuard
*
* Revision 1.23  2003/09/10 20:57:23  gouriano
* added possibility to ignore missing mandatory members on reading
*
* Revision 1.22  2003/08/19 18:32:37  vasilche
* Optimized reading and writing strings.
* Avoid string reallocation when checking char values.
* Try to reuse old string data when string reference counting is not working.
*
* Revision 1.21  2003/05/22 20:08:41  gouriano
* added UTF8 strings
*
* Revision 1.20  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.19  2002/10/25 14:49:29  vasilche
* NCBI C Toolkit compatibility code extracted to libxcser library.
* Serial streams flags names were renamed to fXxx.
*
* Names of flags
*
* Revision 1.18  2001/10/17 20:41:19  grichenk
* Added CObjectOStream::CharBlock class
*
* Revision 1.17  2001/07/17 14:52:39  kholodov
* Fixed: replaced int argument by size_t in CheckVisibleChar() and
* ReplaceVisibleChar to avoid truncation in 64 bit mode.
*
* Revision 1.16  2001/06/11 14:34:55  grichenk
* Added support for numeric tags in ASN.1 specifications and data streams.
*
* Revision 1.15  2001/06/07 17:12:46  grichenk
* Redesigned checking and substitution of non-printable characters
* in VisibleString
*
* Revision 1.14  2001/05/17 14:59:17  lavr
* Typos corrected
*
* Revision 1.13  2000/12/15 21:28:47  vasilche
* Moved some typedefs/enums from corelib/ncbistd.hpp.
* Added flags to CObjectIStream/CObjectOStream: eFlagAllowNonAsciiChars.
* TByte typedef replaced by Uint1.
*
* Revision 1.12  2000/12/15 15:38:00  vasilche
* Added support of Int8 and long double.
* Enum values now have type Int4 instead of long.
*
* Revision 1.11  2000/10/20 15:51:26  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* Revision 1.10  2000/10/03 17:22:34  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.9  2000/09/29 16:18:13  vasilche
* Fixed binary format encoding/decoding on 64 bit compulers.
* Implemented CWeakMap<> for automatic cleaning map entries.
* Added cleaning local hooks via CWeakMap<>.
* Renamed ReadTypeName -> ReadFileHeader, ENoTypeName -> ENoFileHeader.
* Added some user interface methods to CObjectIStream, CObjectOStream and
* CObjectStreamCopier.
*
* Revision 1.8  2000/09/18 20:00:05  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.7  2000/08/15 19:44:40  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.6  2000/06/16 20:01:20  vasilche
* Avoid use of unexpected_exception() which is unimplemented on Mac.
*
* Revision 1.5  2000/05/24 20:08:13  vasilche
* Implemented XML dump.
*
* Revision 1.4  2000/05/09 16:38:33  vasilche
* CObject::GetTypeInfo now moved to CObjectGetTypeInfo::GetTypeInfo to reduce possible errors.
* Added write context to CObjectOStream.
* Inlined most of methods of helping class Member, Block, ByteBlock etc.
*
* Revision 1.3  2000/05/04 16:22:23  vasilche
* Cleaned and optimized blocks and members.
*
* ===========================================================================
*/
