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
void CObjectStreamCopier::ThrowError1(const CDiagCompileInfo& diag_info,
                                      TFailFlags flags,
                                      const char* message)
{
    Out().SetFailFlagsNoError(CObjectOStream::fInvalidData);
    In().ThrowError1(diag_info, flags, message);
}

inline
void CObjectStreamCopier::ThrowError1(const CDiagCompileInfo& diag_info,
                                      TFailFlags flags,
                                      const string& message)
{
    Out().SetFailFlagsNoError(CObjectOStream::fInvalidData);
    In().ThrowError1(diag_info, flags, message);
}

inline
void CObjectStreamCopier::ExpectedMember(const CMemberInfo* memberInfo)
{
    bool was_set = (Out().GetFailFlags() & CObjectOStream::fInvalidData) != 0;
    Out().SetFailFlagsNoError(CObjectOStream::fInvalidData);
    if (!In().ExpectedMember(memberInfo) && !was_set) {
        Out().ClearFailFlags(CObjectOStream::fInvalidData);
    }
}

inline
void CObjectStreamCopier::DuplicatedMember(const CMemberInfo* memberInfo)
{
    Out().SetFailFlagsNoError(CObjectOStream::fInvalidData);
    In().DuplicatedMember(memberInfo);
}

inline
void CObjectStreamCopier::CopyExternalObject(TTypeInfo type)
{
    In().RegisterObject(type);
    Out().RegisterObject(type);
    CopyObject(type);
}

inline
void CObjectStreamCopier::CopyString(EStringType type)
{
    Out().CopyString(In(), type);
}

inline
void CObjectStreamCopier::CopyStringStore(void)
{
    Out().CopyStringStore(In());
}

inline
void CObjectStreamCopier::CopyAnyContentObject(void)
{
    Out().CopyAnyContentObject(In());
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
    if (In().GetDataFormat() == eSerial_Json) {
        Out().CopyClassRandom(classType, *this);
    } else {
        Out().CopyClassSequential(classType, *this);
    }
}

inline
void CObjectStreamCopier::CopyChoice(const CChoiceTypeInfo* choiceType)
{
    Out().CopyChoice(choiceType, *this);
}

inline
void CObjectStreamCopier::CopyAlias(const CAliasTypeInfo* aliasType)
{
    Out().CopyAlias(aliasType, *this);
}

#endif /* def OBJCOPY__HPP  &&  ndef OBJCOPY__INL */
