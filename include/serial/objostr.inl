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
                                     ESerialDataFormat format)
{
    return Open(format, fileName);
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
    m_VerifyData = (verify == eSerialVerifyData_Default) ?
                   x_GetVerifyDataDefault() : verify;
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


#endif /* def OBJOSTR__HPP  &&  ndef OBJOSTR__INL */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.25  2004/05/24 18:12:24  gouriano
* In text output files make indentation optional
*
* Revision 1.24  2004/02/09 18:21:53  gouriano
* enforced checking environment vars when setting initialization
* verification parameters
*
* Revision 1.23  2003/11/26 19:59:38  vasilche
* GetPosition() and GetDataFormat() methods now are implemented
* in parent classes CObjectIStream and CObjectOStream to avoid
* pure virtual method call in destructors.
*
* Revision 1.22  2003/11/13 14:06:45  gouriano
* Elaborated data verification on read/write/get to enable skipping mandatory class data members
*
* Revision 1.21  2003/05/22 20:08:42  gouriano
* added UTF8 strings
*
* Revision 1.20  2003/04/29 18:29:06  gouriano
* object data member initialization verification
*
* Revision 1.19  2003/04/10 20:13:37  vakatov
* Rollback the "uninitialized member" verification -- it still needs to
* be worked upon...
*
* Revision 1.17  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.16  2002/10/25 14:49:29  vasilche
* NCBI C Toolkit compatibility code extracted to libxcser library.
* Serial streams flags names were renamed to fXxx.
*
* Names of flags
*
* Revision 1.15  2002/08/26 18:32:28  grichenk
* Added Get/SetAutoSeparator() to CObjectOStream to control
* output of separators.
*
* Revision 1.14  2002/03/07 22:01:59  grichenk
* Added "Separator" modifier for CObjectOStream
*
* Revision 1.13  2002/02/13 22:39:15  ucko
* Support AIX.
*
* Revision 1.12  2001/10/17 20:41:20  grichenk
* Added CObjectOStream::CharBlock class
*
* Revision 1.11  2001/05/17 14:59:47  lavr
* Typos corrected
*
* Revision 1.10  2000/12/15 21:28:48  vasilche
* Moved some typedefs/enums from corelib/ncbistd.hpp.
* Added flags to CObjectIStream/CObjectOStream: eFlagAllowNonAsciiChars.
* TByte typedef replaced by Uint1.
*
* Revision 1.9  2000/12/15 15:38:01  vasilche
* Added support of Int8 and long double.
* Enum values now have type Int4 instead of long.
*
* Revision 1.8  2000/10/20 15:51:27  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* Revision 1.7  2000/10/03 17:22:35  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.6  2000/09/18 20:00:07  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.5  2000/08/15 19:44:41  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.4  2000/05/24 20:08:14  vasilche
* Implemented XML dump.
*
* Revision 1.3  2000/05/09 16:38:34  vasilche
* CObject::GetTypeInfo now moved to CObjectGetTypeInfo::GetTypeInfo to reduce possible errors.
* Added write context to CObjectOStream.
* Inlined most of methods of helping class Member, Block, ByteBlock etc.
*
* Revision 1.2  2000/05/04 16:22:24  vasilche
* Cleaned and optimized blocks and members.
*
* Revision 1.1  1999/05/19 19:56:27  vasilche
* Commit just in case.
*
* ===========================================================================
*/
