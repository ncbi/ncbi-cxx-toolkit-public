#if defined(OBJCOPY__HPP)  &&  !defined(OBJCOPY__INL)
#define OBJCOPY__INL

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
* Revision 1.3  2000/09/29 16:18:13  vasilche
* Fixed binary format encoding/decoding on 64 bit compulers.
* Implemented CWeakMap<> for automatic cleaning map entries.
* Added cleaning local hooks via CWeakMap<>.
* Renamed ReadTypeName -> ReadFileHeader, ENoTypeName -> ENoFileHeader.
* Added some user interface methods to CObjectIStream, CObjectOStream and
* CObjectStreamCopier.
*
* Revision 1.2  2000/09/18 20:00:04  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.1  2000/09/01 13:15:59  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* ===========================================================================
*/

inline
CObjectIStream& CObjectStreamCopier::In(void) const
{
    return m_In;
}

inline
CObjectOStream& CObjectStreamCopier::Out(void) const
{
    return m_Out;
}

inline
void CObjectStreamCopier::CopyObject(TTypeInfo type)
{
    Out().CopyObject(type, *this);
}

inline
void CObjectStreamCopier::ExpectedMember(const CMemberInfo* memberInfo)
{
    In().ExpectedMember(memberInfo);
}

inline
void CObjectStreamCopier::CopyExternalObject(TTypeInfo type)
{
    In().RegisterObject(type);
    Out().RegisterObject(type);
    CopyObject(type);
}

inline
void CObjectStreamCopier::CopyString(void)
{
    Out().CopyString(In());
}

inline
void CObjectStreamCopier::CopyStringStore(void)
{
    Out().CopyStringStore(In());
}

inline
void CObjectStreamCopier::CopyNamedType(TTypeInfo namedTypeInfo,
                                        TTypeInfo objectType)
{
    Out().CopyNamedType(namedTypeInfo, objectType, *this);
}

inline
void CObjectStreamCopier::CopyContainer(const CContainerTypeInfo* cType)
{
    Out().CopyContainer(cType, *this);
}

inline
void CObjectStreamCopier::CopyClassRandom(const CClassTypeInfo* classType)
{
    Out().CopyClassRandom(classType, *this);
}

inline
void CObjectStreamCopier::CopyClassSequential(const CClassTypeInfo* classType)
{
    Out().CopyClassSequential(classType, *this);
}

inline
void CObjectStreamCopier::CopyChoice(const CChoiceTypeInfo* choiceType)
{
    Out().CopyChoice(choiceType, *this);
}

#endif /* def OBJCOPY__HPP  &&  ndef OBJCOPY__INL */
