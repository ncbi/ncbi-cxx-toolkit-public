#if defined(SERIAL___OBJISTR__HPP)  &&  !defined(OBJISTR__INL)
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
    if ( m_MonitorType && !typeInfo->IsOrMayContainType(m_MonitorType) ) {
        SkipAnyContentObject();
    }
    else {
        typeInfo->SkipData(*this);
    }
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
void CObjectIStream::ReadStd(CBitString& data)
{
    ReadBitString(data);
}

inline
void CObjectIStream::SkipStd(CBitString& )
{
    SkipBitString();
}


inline
bool GoodVisibleChar(char c)
{
    return c >= ' ' && c <= '~';
}


inline
void FixVisibleChar(char& c, EFixNonPrint fix_method,
    const CObjectStack* io, const string& str)
{
    if ( !GoodVisibleChar(c) ) {
        c = ReplaceVisibleChar(c, fix_method, io, str);
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
ESerialSkipUnknown CObjectIStream::GetSkipUnknownMembers(void)
{
    ESerialSkipUnknown skip = m_SkipUnknown;
    if ( skip == eSerialSkipUnknown_Default ) {
        skip = UpdateSkipUnknownMembers();
    }
    return skip;
}

inline
ESerialSkipUnknown CObjectIStream::GetSkipUnknownVariants(void)
{
    ESerialSkipUnknown skip = m_SkipUnknownVariants;
    if ( skip == eSerialSkipUnknown_Default ) {
        skip = UpdateSkipUnknownVariants();
    }
    return skip;
}

inline
void CObjectIStream::SetSkipUnknownMembers(ESerialSkipUnknown skip)
{
    ESerialSkipUnknown old_skip = GetSkipUnknownMembers();
    if ( old_skip != eSerialSkipUnknown_Never &&
         old_skip != eSerialSkipUnknown_Always ) {
        m_SkipUnknown = skip;
    }
}

inline
void CObjectIStream::SetSkipUnknownVariants(ESerialSkipUnknown skip)
{
    ESerialSkipUnknown old_skip = GetSkipUnknownVariants();
    if ( old_skip != eSerialSkipUnknown_Never &&
         old_skip != eSerialSkipUnknown_Always ) {
        m_SkipUnknownVariants = skip;
    }
}

inline
bool CObjectIStream::CanSkipUnknownMembers(void)
{
    ESerialSkipUnknown skip = GetSkipUnknownMembers();
    return skip == eSerialSkipUnknown_Yes || skip == eSerialSkipUnknown_Always;
}

inline
bool CObjectIStream::CanSkipUnknownVariants(void)
{
    ESerialSkipUnknown skip = GetSkipUnknownVariants();
    return skip == eSerialSkipUnknown_Yes || skip == eSerialSkipUnknown_Always;
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


#endif /* def SERIAL___OBJISTR__HPP  &&  ndef OBJISTR__INL */
