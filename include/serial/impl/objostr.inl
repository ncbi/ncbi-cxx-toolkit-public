#if defined(OBJOSTR__HPP)  &&  !defined(OBJOSTR__INL)
#define OBJOSTR__INL

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
ESerialDataFormat CObjectOStream::GetDataFormat(void) const
{
    return m_DataFormat;
}


inline
CObjectOStream* CObjectOStream::Open(const string& fileName,
                                     ESerialDataFormat format,
                                     TSerial_Format_Flags formatFlags)
{
    return Open(format, fileName, 0, formatFlags);
}

inline
void CObjectOStream::FlushBuffer(void)
{
    m_Output.FlushBuffer();
}

inline
void CObjectOStream::Flush(void)
{
    m_Output.Flush();
}

inline
CObjectOStream::TFailFlags CObjectOStream::GetFailFlags(void) const
{
    return m_Fail;
}

inline
bool CObjectOStream::fail(void) const
{
    return GetFailFlags() != 0;
}

inline
CObjectOStream::TFailFlags CObjectOStream::ClearFailFlags(TFailFlags flags)
{
    TFailFlags old = GetFailFlags();
    m_Fail &= ~flags;
    return old;
}

inline
CObjectOStream::TFlags CObjectOStream::GetFlags(void) const
{
    return m_Flags;
}

inline
CObjectOStream::TFlags CObjectOStream::SetFlags(TFlags flags)
{
    TFlags old = GetFlags();
    m_Flags |= flags;
    return old;
}

inline
CObjectOStream::TFlags CObjectOStream::ClearFlags(TFlags flags)
{
    TFlags old = GetFlags();
    m_Flags &= ~flags;
    return old;
}

inline
void CObjectOStream::WriteObject(TConstObjectPtr objectPtr,
                                 TTypeInfo objectType)
{
    objectType->WriteData(*this, objectPtr);
}

inline
void CObjectOStream::CopyObject(TTypeInfo objectType,
                                CObjectStreamCopier& copier)
{
    objectType->CopyData(copier);
}

inline
void CObjectOStream::WriteClassRandom(const CClassTypeInfo* classType,
                                      TConstObjectPtr classPtr)
{
    WriteClass(classType, classPtr);
}

inline
void CObjectOStream::WriteClassSequential(const CClassTypeInfo* classType,
                                          TConstObjectPtr classPtr)
{
    WriteClass(classType, classPtr);
}

// std C types readers
// bool
inline
void CObjectOStream::WriteStd(const bool& data)
{
    WriteBool(data);
}

// char
inline
void CObjectOStream::WriteStd(const char& data)
{
    WriteChar(data);
}

// integer numbers
inline
void CObjectOStream::WriteStd(const signed char& data)
{
    WriteInt4(data);
}

inline
void CObjectOStream::WriteStd(const unsigned char& data)
{
    WriteUint4(data);
}

inline
void CObjectOStream::WriteStd(const short& data)
{
    WriteInt4(data);
}

inline
void CObjectOStream::WriteStd(const unsigned short& data)
{
    WriteUint4(data);
}

#if SIZEOF_INT == 4
inline
void CObjectOStream::WriteStd(const int& data)
{
    WriteInt4(data);
}

inline
void CObjectOStream::WriteStd(const unsigned int& data)
{
    WriteUint4(data);
}
#else
#  error Unsupported size of int - must be 4
#endif

#if SIZEOF_LONG == 4
inline
void CObjectOStream::WriteStd(const long& data)
{
    WriteInt4(Int4(data));
}

inline
void CObjectOStream::WriteStd(const unsigned long& data)
{
    WriteUint4(Uint4(data));
}
#endif

inline
void CObjectOStream::WriteStd(const Int8& data)
{
    WriteInt8(data);
}

inline
void CObjectOStream::WriteStd(const Uint8& data)
{
    WriteUint8(data);
}

// float numbers
inline
void CObjectOStream::WriteStd(const float& data)
{
    WriteFloat(data);
}

inline
void CObjectOStream::WriteStd(const double& data)
{
    WriteDouble(data);
}

#if SIZEOF_LONG_DOUBLE != 0
inline
void CObjectOStream::WriteStd(const long double& data)
{
    WriteLDouble(data);
}
#endif

// string
inline
void CObjectOStream::WriteStd(const string& data)
{
    WriteString(data);
}

inline
void CObjectOStream::WriteStd(const CStringUTF8& data)
{
    WriteString(data,eStringTypeUTF8);
}

// C string
inline
void CObjectOStream::WriteStd(const char* const data)
{
    WriteCString(data);
}

inline
void CObjectOStream::WriteStd(char* const data)
{
    WriteCString(data);
}

inline
void CObjectOStream::WriteStd(const CBitString& data)
{
    WriteBitString(data);
}

inline
CObjectOStream::ByteBlock::ByteBlock(CObjectOStream& out, size_t length)
    : m_Stream(out), m_Length(length), m_Ended(false)
{
    out.BeginBytes(*this);
}

inline
CObjectOStream& CObjectOStream::ByteBlock::GetStream(void) const
{
    return m_Stream;
}

inline
size_t CObjectOStream::ByteBlock::GetLength(void) const
{
    return m_Length;
}

inline
void CObjectOStream::ByteBlock::Write(const void* bytes, size_t length)
{
    _ASSERT( length <= m_Length );
    GetStream().WriteBytes(*this, static_cast<const char*>(bytes), length);
    m_Length -= length;
}

inline
CObjectOStream::CharBlock::CharBlock(CObjectOStream& out, size_t length)
    : m_Stream(out), m_Length(length), m_Ended(false)
{
    out.BeginChars(*this);
}

inline
CObjectOStream& CObjectOStream::CharBlock::GetStream(void) const
{
    return m_Stream;
}

inline
size_t CObjectOStream::CharBlock::GetLength(void) const
{
    return m_Length;
}

inline
void CObjectOStream::CharBlock::Write(const char* chars, size_t length)
{
    _ASSERT( length <= m_Length );
    GetStream().WriteChars(*this, chars, length);
    m_Length -= length;
}

inline
CObjectOStream& Separator(CObjectOStream& os)
{
    os.WriteSeparator();
    return os;
}

inline
CObjectOStream& CObjectOStream::operator<<
    (CObjectOStream& (*mod)(CObjectOStream& os))
{
    return mod(*this);
}

inline
string CObjectOStream::GetSeparator(void) const
{
    return m_Separator;
}

inline
void CObjectOStream::SetSeparator(const string sep)
{
    m_Separator = sep;
}

inline
bool CObjectOStream::GetAutoSeparator(void)
{
    return m_AutoSeparator;
}

inline
void CObjectOStream::SetAutoSeparator(bool value)
{
    m_AutoSeparator = value;
}

inline
void CObjectOStream::SetVerifyData(ESerialVerifyData verify)
{
    if (m_VerifyData == eSerialVerifyData_Never ||
        m_VerifyData == eSerialVerifyData_Always ||
        m_VerifyData == eSerialVerifyData_DefValueAlways) {
        return;
    }
    if (verify == eSerialVerifyData_Default) {
        verify = x_GetVerifyDataDefault();
    }
    if (m_VerifyData != verify &&
        (verify == eSerialVerifyData_No || verify == eSerialVerifyData_Never)) {
        ERR_POST_XX_ONCE(Serial_OStream, 1, Info <<
            "CObjectOStream::SetVerifyData: data verification disabled");
    }
    m_VerifyData = verify;
}

inline
ESerialVerifyData CObjectOStream::GetVerifyData(void) const
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
void CObjectOStream::SetUseIndentation(bool set)
{
    m_Output.SetUseIndentation(set);
}

inline
bool CObjectOStream::GetUseIndentation(void) const
{
    return m_Output.GetUseIndentation();
}

inline
void CObjectOStream::SetUseEol(bool set)
{
    m_Output.SetUseEol(set);
}

inline
bool CObjectOStream::GetUseEol(void) const
{
    return m_Output.GetUseEol();
}

inline
void CObjectOStream::SetWriteNamedIntegersByValue(bool set)
{
    m_WriteNamedIntegersByValue = set;
}

inline
bool CObjectOStream::GetWriteNamedIntegersByValue(void) const
{
    return m_WriteNamedIntegersByValue;
}


#endif /* def OBJOSTR__HPP  &&  ndef OBJOSTR__INL */
