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
*
* ---------------------------------------------------------------------------
* $Log$
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

#if HAVE_NCBI_C
inline
CObjectIStream& CObjectIStream::AsnIo::GetStream(void) const
{
    return m_Stream;
}

inline
CObjectIStream::AsnIo::operator asnio*(void)
{
    return m_AsnIo;
}

inline
asnio* CObjectIStream::AsnIo::operator->(void)
{
    return m_AsnIo;
}

inline
const string& CObjectIStream::AsnIo::GetRootTypeName(void) const
{
    return m_RootTypeName;
}

inline
size_t CObjectIStream::AsnIo::Read(char* data, size_t length)
{
    return GetStream().AsnRead(*this, data, length);
}
#endif

#endif /* def OBJISTR__HPP  &&  ndef OBJISTR__INL */
