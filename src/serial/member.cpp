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
* Revision 1.12  2000/09/26 17:38:21  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
* Revision 1.11  2000/09/22 20:01:19  vasilche
* Forgot to uncomment some code.
*
* Revision 1.10  2000/09/19 14:10:25  vasilche
* Added files to MSVC project
* Updated shell scripts to use new datattool path on MSVC
* Fixed internal compiler error on MSVC
*
* Revision 1.9  2000/09/18 20:00:22  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.8  2000/06/07 19:45:58  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.7  2000/02/01 21:47:22  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Added buffering to CObjectIStreamAsn.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
*
* Revision 1.6  1999/09/14 18:54:17  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.5  1999/09/01 17:38:12  vasilche
* Fixed vector<char> implementation.
* Added explicit naming of class info.
* Moved IMPLICIT attribute from member info to class info.
*
* Revision 1.4  1999/08/31 17:50:08  vasilche
* Implemented several macros for specific data types.
* Added implicit members.
* Added multimap and set.
*
* Revision 1.3  1999/08/13 15:53:50  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.2  1999/07/01 17:55:28  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.1  1999/06/30 16:04:50  vasilche
* Added support for old ASN.1 structures.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/member.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>
#include <serial/delaybuf.hpp>
#include <memory>

BEGIN_NCBI_SCOPE

class CMemberInfoFunctions
{
public:
    static TConstObjectPtr GetConstSimpleMember(const CMemberInfo* memberInfo,
                                                TConstObjectPtr classPtr);
    static TConstObjectPtr GetConstDelayedMember(const CMemberInfo* memberInfo,
                                                 TConstObjectPtr classPtr);
    static TObjectPtr GetSimpleMember(const CMemberInfo* memberInfo,
                                      TObjectPtr classPtr);
    static TObjectPtr GetWithSetFlagMember(const CMemberInfo* memberInfo,
                                           TObjectPtr classPtr);
    static TObjectPtr GetDelayedMember(const CMemberInfo* memberInfo,
                                       TObjectPtr classPtr);
    

    static void ReadSimpleMember(CObjectIStream& in,
                                 const CMemberInfo* memberInfo,
                                 TObjectPtr classPtr);
    static void ReadWithSetFlagMember(CObjectIStream& in,
                                        const CMemberInfo* memberInfo,
                                        TObjectPtr classPtr);
    static void ReadLongMember(CObjectIStream& in,
                                 const CMemberInfo* memberInfo,
                                 TObjectPtr classPtr);
    static void ReadHookedMember(CObjectIStream& in,
                                 const CMemberInfo* memberInfo,
                                 TObjectPtr classPtr);
    static void ReadMissingSimpleMember(CObjectIStream& in,
                                        const CMemberInfo* memberInfo,
                                        TObjectPtr classPtr);
    static void ReadMissingOptionalMember(CObjectIStream& in,
                                            const CMemberInfo* memberInfo,
                                            TObjectPtr classPtr);
    static void ReadMissingHookedMember(CObjectIStream& in,
                                        const CMemberInfo* memberInfo,
                                        TObjectPtr classPtr);
    static void WriteSimpleMember(CObjectOStream& out,
                                  const CMemberInfo* memberInfo,
                                  TConstObjectPtr classPtr);
    static void WriteOptionalMember(CObjectOStream& out,
                                      const CMemberInfo* memberInfo,
                                      TConstObjectPtr classPtr);
    static void WriteWithDefaultMember(CObjectOStream& out,
                                         const CMemberInfo* memberInfo,
                                         TConstObjectPtr classPtr);
    static void WriteWithSetFlagMember(CObjectOStream& out,
                                         const CMemberInfo* memberInfo,
                                         TConstObjectPtr classPtr);
    static void WriteLongMember(CObjectOStream& out,
                                  const CMemberInfo* memberInfo,
                                  TConstObjectPtr classPtr);
    static void WriteHookedMember(CObjectOStream& out,
                                  const CMemberInfo* memberInfo,
                                  TConstObjectPtr classPtr);
    static void CopySimpleMember(CObjectStreamCopier& copier,
                                 const CMemberInfo* memberInfo);
    static void CopyHookedMember(CObjectStreamCopier& copier,
                                 const CMemberInfo* memberInfo);
    static void CopyMissingSimpleMember(CObjectStreamCopier& copier,
                                        const CMemberInfo* memberInfo);
    static void CopyMissingOptionalMember(CObjectStreamCopier& copier,
                                            const CMemberInfo* memberInfo);
    static void CopyMissingHookedMember(CObjectStreamCopier& copier,
                                        const CMemberInfo* memberInfo);
    static void SkipSimpleMember(CObjectIStream& in,
                                 const CMemberInfo* memberInfo);
    static void SkipMissingSimpleMember(CObjectIStream& in,
                                        const CMemberInfo* memberInfo);
    static void SkipMissingOptionalMember(CObjectIStream& in,
                                          const CMemberInfo* memberInfo);


    static void ReadParentClass(CObjectIStream& in,
                                const CMemberInfo* memberInfo,
                                TObjectPtr objectPtr);
    static void ReadMissingParentClass(CObjectIStream& in,
                                       const CMemberInfo* memberInfo,
                                       TObjectPtr objectPtr);
    static void WriteParentClass(CObjectOStream& out,
                                 const CMemberInfo* memberInfo,
                                 TConstObjectPtr objectPtr);
    static void CopyParentClass(CObjectStreamCopier& copier,
                                const CMemberInfo* memberInfo);
    static void CopyMissingParentClass(CObjectStreamCopier& copier,
                                       const CMemberInfo* memberInfo);
    static void SkipParentClass(CObjectIStream& in,
                                const CMemberInfo* memberInfo);
    static void SkipMissingParentClass(CObjectIStream& in,
                                       const CMemberInfo* memberInfo);
};

typedef CMemberInfoFunctions TFunc;

CMemberInfo::CMemberInfo(const CClassTypeInfo* classType,
                         const CMemberId& id, TOffset offset,
                         const CTypeRef& type)
    : CParent(id, offset, type),
      m_ClassType(classType), m_Optional(false), m_Default(0),
      m_SetFlagOffset(TOffset(eNoOffset)), m_DelayOffset(TOffset(eNoOffset)),
      m_GetConstFunction(&TFunc::GetConstSimpleMember),
      m_GetFunction(&TFunc::GetSimpleMember),
      m_ReadHookData(SMemberRead(&TFunc::ReadSimpleMember,
                                 &TFunc::ReadMissingSimpleMember),
                     SMemberRead(&TFunc::ReadHookedMember,
                                 &TFunc::ReadMissingHookedMember)),
      m_WriteHookData(&TFunc::WriteSimpleMember, &TFunc::WriteHookedMember),
      m_CopyHookData(SMemberCopy(&TFunc::CopySimpleMember,
                                 &TFunc::CopyMissingSimpleMember),
                     SMemberCopy(&TFunc::CopyHookedMember,
                                 &TFunc::CopyMissingHookedMember)),
      m_SkipFunction(&TFunc::SkipSimpleMember),
      m_SkipMissingFunction(&TFunc::SkipMissingSimpleMember)
{
}

CMemberInfo::CMemberInfo(const CClassTypeInfo* classType,
                         const CMemberId& id, TOffset offset, TTypeInfo type)
    : CParent(id, offset, type),
      m_ClassType(classType), m_Optional(false), m_Default(0),
      m_SetFlagOffset(TOffset(eNoOffset)), m_DelayOffset(TOffset(eNoOffset)),
      m_GetConstFunction(&TFunc::GetConstSimpleMember),
      m_GetFunction(&TFunc::GetSimpleMember),
      m_ReadHookData(SMemberRead(&TFunc::ReadSimpleMember,
                                 &TFunc::ReadMissingSimpleMember),
                     SMemberRead(&TFunc::ReadHookedMember,
                                 &TFunc::ReadMissingHookedMember)),
      m_WriteHookData(&TFunc::WriteSimpleMember, &TFunc::WriteHookedMember),
      m_CopyHookData(SMemberCopy(&TFunc::CopySimpleMember,
                                 &TFunc::CopyMissingSimpleMember),
                     SMemberCopy(&TFunc::CopyHookedMember,
                                 &TFunc::CopyMissingHookedMember)),
      m_SkipFunction(&TFunc::SkipSimpleMember),
      m_SkipMissingFunction(&TFunc::SkipMissingSimpleMember)
{
}

CMemberInfo::CMemberInfo(const CClassTypeInfo* classType,
                         const char* id, TOffset offset, const CTypeRef& type)
    : CParent(id, offset, type),
      m_ClassType(classType), m_Optional(false), m_Default(0),
      m_SetFlagOffset(TOffset(eNoOffset)), m_DelayOffset(TOffset(eNoOffset)),
      m_GetConstFunction(&TFunc::GetConstSimpleMember),
      m_GetFunction(&TFunc::GetSimpleMember),
      m_ReadHookData(SMemberRead(&TFunc::ReadSimpleMember,
                                 &TFunc::ReadMissingSimpleMember),
                     SMemberRead(&TFunc::ReadHookedMember,
                                 &TFunc::ReadMissingHookedMember)),
      m_WriteHookData(&TFunc::WriteSimpleMember, &TFunc::WriteHookedMember),
      m_CopyHookData(SMemberCopy(&TFunc::CopySimpleMember,
                                 &TFunc::CopyMissingSimpleMember),
                     SMemberCopy(&TFunc::CopyHookedMember,
                                 &TFunc::CopyMissingHookedMember)),
      m_SkipFunction(&TFunc::SkipSimpleMember),
      m_SkipMissingFunction(&TFunc::SkipMissingSimpleMember)
{
}

CMemberInfo::CMemberInfo(const CClassTypeInfo* classType,
                         const char* id, TOffset offset, TTypeInfo type)
    : CParent(id, offset, type),
      m_ClassType(classType), m_Optional(false), m_Default(0),
      m_SetFlagOffset(TOffset(eNoOffset)), m_DelayOffset(TOffset(eNoOffset)),
      m_GetConstFunction(&TFunc::GetConstSimpleMember),
      m_GetFunction(&TFunc::GetSimpleMember),
      m_ReadHookData(SMemberRead(&TFunc::ReadSimpleMember,
                                 &TFunc::ReadMissingSimpleMember),
                     SMemberRead(&TFunc::ReadHookedMember,
                                 &TFunc::ReadMissingHookedMember)),
      m_WriteHookData(&TFunc::WriteSimpleMember, &TFunc::WriteHookedMember),
      m_CopyHookData(SMemberCopy(&TFunc::CopySimpleMember,
                                 &TFunc::CopyMissingSimpleMember),
                     SMemberCopy(&TFunc::CopyHookedMember,
                                 &TFunc::CopyMissingHookedMember)),
      m_SkipFunction(&TFunc::SkipSimpleMember),
      m_SkipMissingFunction(&TFunc::SkipMissingSimpleMember)
{
}

CMemberInfo::~CMemberInfo(void)
{
}

void CMemberInfo::SetParentClass(void)
{
    m_ReadHookData.GetDefaultFunction() =
        SMemberRead(&TFunc::ReadParentClass, &TFunc::ReadMissingParentClass);
    m_WriteHookData.GetDefaultFunction() = &TFunc::WriteParentClass;
    m_CopyHookData.GetDefaultFunction() =
        SMemberCopy(&TFunc::CopyParentClass, &TFunc::CopyMissingParentClass);
    m_SkipFunction = &TFunc::SkipParentClass;
    m_SkipMissingFunction = &TFunc::SkipMissingParentClass;
}

CMemberInfo* CMemberInfo::SetDelayBuffer(CDelayBuffer* buffer)
{
    m_DelayOffset = size_t(buffer);
    UpdateFunctions();
    return this;
}

CMemberInfo* CMemberInfo::SetOptional(void)
{
    m_Optional = true;
    UpdateFunctions();
    return this;
}

CMemberInfo* CMemberInfo::SetDefault(TConstObjectPtr def)
{
    m_Optional = true;
    m_Default = def;
    UpdateFunctions();
    return this;
}

CMemberInfo* CMemberInfo::SetSetFlag(const bool* setFlag)
{
    _ASSERT(Optional());
    m_SetFlagOffset = size_t(setFlag);
    UpdateFunctions();
    return this;
}

CMemberInfo* CMemberInfo::SetOptional(const bool* setFlag)
{
    m_Optional = true;
    return SetSetFlag(setFlag);
}

void CMemberInfo::UpdateFunctions(void)
{
    SMemberRead& readFuncs = m_ReadHookData.GetDefaultFunction();
    TMemberWrite& writeFunc = m_WriteHookData.GetDefaultFunction();
    SMemberCopy& copyFuncs = m_CopyHookData.GetDefaultFunction();

    // get/readmain/write
    if ( CanBeDelayed() ) {
        m_GetConstFunction = &TFunc::GetConstDelayedMember;
        m_GetFunction = &TFunc::GetDelayedMember;
        readFuncs.m_Main = &TFunc::ReadLongMember;
        writeFunc = &TFunc::WriteLongMember;
    }
    else if ( !HaveSetFlag() ) {
        m_GetConstFunction = &TFunc::GetConstSimpleMember;
        m_GetFunction = &TFunc::GetSimpleMember;
        readFuncs.m_Main = &TFunc::ReadSimpleMember;

        if ( GetDefault() )
            writeFunc = &TFunc::WriteWithDefaultMember;
        else if ( Optional() )
            writeFunc = &TFunc::WriteOptionalMember;
        else
            writeFunc = &TFunc::WriteSimpleMember;
    }
    else {
        // have set flag
        m_GetConstFunction = &TFunc::GetConstSimpleMember;
        m_GetFunction = &TFunc::GetWithSetFlagMember;
        readFuncs.m_Main = &TFunc::ReadWithSetFlagMember;
        writeFunc = &TFunc::WriteWithSetFlagMember;
    }

    // copymain/skipmain
    copyFuncs.m_Main = &TFunc::CopySimpleMember;
    m_SkipFunction = &TFunc::SkipSimpleMember;

    // readmissing/copymissing/skipmissing
    if ( Optional() ) {
        readFuncs.m_Missing = &TFunc::ReadMissingOptionalMember;
        copyFuncs.m_Missing = &TFunc::CopyMissingOptionalMember;
        m_SkipMissingFunction = &TFunc::SkipMissingOptionalMember;
    }
    else {
        readFuncs.m_Missing = &TFunc::ReadMissingSimpleMember;
        copyFuncs.m_Missing = &TFunc::CopyMissingSimpleMember;
        m_SkipMissingFunction = &TFunc::SkipMissingSimpleMember;
    }

}

void CMemberInfo::UpdateDelayedBuffer(CObjectIStream& in,
                                      TObjectPtr classPtr) const
{
    _ASSERT(CanBeDelayed());
    _ASSERT(GetDelayBuffer(classPtr).GetIndex() == GetIndex());

    GetTypeInfo()->ReadData(in, GetItemPtr(classPtr));
}

void CMemberInfo::SetReadFunction(TMemberRead func)
{
    m_ReadHookData.GetDefaultFunction().m_Main = func;
}

void CMemberInfo::SetReadMissingFunction(TMemberRead func)
{
    m_ReadHookData.GetDefaultFunction().m_Missing = func;
}

void CMemberInfo::SetWriteFunction(TMemberWrite func)
{
    m_WriteHookData.GetDefaultFunction() = func;
}

void CMemberInfo::SetCopyFunction(TMemberCopy func)
{
    m_CopyHookData.GetDefaultFunction().m_Main = func;
}

void CMemberInfo::SetCopyMissingFunction(TMemberCopy func)
{
    m_CopyHookData.GetDefaultFunction().m_Missing = func;
}

void CMemberInfo::SetSkipFunction(TMemberSkip func)
{
    m_SkipFunction = func;
}

void CMemberInfo::SetSkipMissingFunction(TMemberSkip func)
{
    m_SkipMissingFunction = func;
}

void CMemberInfo::SetReadHook(CObjectIStream* stream,
                              CReadClassMemberHook* hook)
{
    m_ReadHookData.SetHook(stream, hook);
}

void CMemberInfo::ResetReadHook(CObjectIStream* stream)
{
    m_ReadHookData.ResetHook(stream);
}

void CMemberInfo::SetWriteHook(CObjectOStream* stream,
                               CWriteClassMemberHook* hook)
{
    m_WriteHookData.SetHook(stream, hook);
}

void CMemberInfo::ResetWriteHook(CObjectOStream* stream)
{
    m_WriteHookData.ResetHook(stream);
}

void CMemberInfo::SetCopyHook(CObjectStreamCopier* stream,
                              CCopyClassMemberHook* hook)
{
    m_CopyHookData.SetHook(stream, hook);
}

void CMemberInfo::ResetCopyHook(CObjectStreamCopier* stream)
{
    m_CopyHookData.ResetHook(stream);
}

TConstObjectPtr
CMemberInfoFunctions::GetConstSimpleMember(const CMemberInfo* memberInfo,
                                           TConstObjectPtr classPtr)
{
    _ASSERT(!memberInfo->CanBeDelayed());
    return memberInfo->GetItemPtr(classPtr);
}

TConstObjectPtr
CMemberInfoFunctions::GetConstDelayedMember(const CMemberInfo* memberInfo,
                                            TConstObjectPtr classPtr)
{
    _ASSERT(memberInfo->CanBeDelayed());
    const_cast<CDelayBuffer&>(memberInfo->GetDelayBuffer(classPtr)).Update();
    return memberInfo->GetItemPtr(classPtr);
}

TObjectPtr CMemberInfoFunctions::GetSimpleMember(const CMemberInfo* memberInfo,
                                                 TObjectPtr classPtr)
{
    _ASSERT(!memberInfo->CanBeDelayed());
    _ASSERT(!memberInfo->HaveSetFlag());
    return memberInfo->GetItemPtr(classPtr);
}

TObjectPtr CMemberInfoFunctions::GetWithSetFlagMember(const CMemberInfo* memberInfo,
                                                      TObjectPtr classPtr)
{
    _ASSERT(!memberInfo->CanBeDelayed());
    _ASSERT(memberInfo->HaveSetFlag());
    memberInfo->GetSetFlag(classPtr) = true;
    return memberInfo->GetItemPtr(classPtr);
}

TObjectPtr CMemberInfoFunctions::GetDelayedMember(const CMemberInfo* memberInfo,
                                                  TObjectPtr classPtr)
{
    _ASSERT(memberInfo->CanBeDelayed());
    memberInfo->GetDelayBuffer(classPtr).Update();
    memberInfo->UpdateSetFlag(classPtr, true);
    return memberInfo->GetItemPtr(classPtr);
}

void CMemberInfoFunctions::ReadSimpleMember(CObjectIStream& in,
                                            const CMemberInfo* memberInfo,
                                            TObjectPtr classPtr)
{
    _ASSERT(!memberInfo->CanBeDelayed());
    _ASSERT(!memberInfo->HaveSetFlag());
    in.ReadObject(memberInfo->GetItemPtr(classPtr),
                  memberInfo->GetTypeInfo());
}

void CMemberInfoFunctions::ReadWithSetFlagMember(CObjectIStream& in,
                                                 const CMemberInfo* memberInfo,
                                                 TObjectPtr classPtr)
{
    _ASSERT(!memberInfo->CanBeDelayed());
    _ASSERT(memberInfo->HaveSetFlag());
    in.ReadObject(memberInfo->GetItemPtr(classPtr),
                  memberInfo->GetTypeInfo());
    memberInfo->GetSetFlag(classPtr) = true;
}

void CMemberInfoFunctions::ReadLongMember(CObjectIStream& in,
                                          const CMemberInfo* memberInfo,
                                          TObjectPtr classPtr)
{
    if ( memberInfo->CanBeDelayed() ) {
        CDelayBuffer& buffer = memberInfo->GetDelayBuffer(classPtr);
        if ( !buffer ) {
            in.StartDelayBuffer();
            memberInfo->GetTypeInfo()->SkipData(in);
            in.EndDelayBuffer(buffer, memberInfo, classPtr);
            // update 'set' flag
            memberInfo->UpdateSetFlag(classPtr, true);
            return;
        }
        buffer.Update();
    }
    
    in.ReadObject(memberInfo->GetItemPtr(classPtr),
                  memberInfo->GetTypeInfo());
    memberInfo->UpdateSetFlag(classPtr, true);
}

void CMemberInfoFunctions::ReadMissingSimpleMember(CObjectIStream& in,
                                                   const CMemberInfo* memberInfo,
                                                   TObjectPtr /*classPtr*/)
{
    _ASSERT(!memberInfo->Optional());
    in.ExpectedMember(memberInfo);
}

void CMemberInfoFunctions::ReadMissingOptionalMember(CObjectIStream& /*in*/,
                                                     const CMemberInfo* _DEBUG_ARG(memberInfo),
                                                     TObjectPtr /*classPtr*/)
{
    _ASSERT(memberInfo->Optional());
}

void CMemberInfoFunctions::WriteSimpleMember(CObjectOStream& out,
                                             const CMemberInfo* memberInfo,
                                             TConstObjectPtr classPtr)
{
    _ASSERT(!memberInfo->CanBeDelayed());
    _ASSERT(!memberInfo->Optional());
    out.WriteClassMember(memberInfo->GetId(),
                         memberInfo->GetTypeInfo(),
                         memberInfo->GetItemPtr(classPtr));
}

void CMemberInfoFunctions::WriteOptionalMember(CObjectOStream& out,
                                               const CMemberInfo* memberInfo,
                                               TConstObjectPtr classPtr)
{
    _ASSERT(!memberInfo->CanBeDelayed());
    _ASSERT(memberInfo->Optional());
    TTypeInfo memberType = memberInfo->GetTypeInfo();
    TConstObjectPtr memberPtr = memberInfo->GetItemPtr(classPtr);
    if ( memberType->IsDefault(memberPtr) )
        return;

    out.WriteClassMember(memberInfo->GetId(), memberType, memberPtr);
}

void CMemberInfoFunctions::WriteWithDefaultMember(CObjectOStream& out,
                                                  const CMemberInfo* memberInfo,
                                                  TConstObjectPtr classPtr)
{
    _ASSERT(!memberInfo->CanBeDelayed());
    _ASSERT(memberInfo->GetDefault());
    TTypeInfo memberType = memberInfo->GetTypeInfo();
    TConstObjectPtr memberPtr = memberInfo->GetItemPtr(classPtr);
    if ( memberType->Equals(memberPtr, memberInfo->GetDefault()) )
        return;

    out.WriteClassMember(memberInfo->GetId(), memberType, memberPtr);
}

void CMemberInfoFunctions::WriteWithSetFlagMember(CObjectOStream& out,
                                                  const CMemberInfo* memberInfo,
                                                  TConstObjectPtr classPtr)
{
    _ASSERT(!memberInfo->CanBeDelayed());
    _ASSERT(memberInfo->HaveSetFlag());
    if ( !memberInfo->GetSetFlag(classPtr) )
        return;

    out.WriteClassMember(memberInfo->GetId(),
                         memberInfo->GetTypeInfo(),
                         memberInfo->GetItemPtr(classPtr));
}

void CMemberInfoFunctions::WriteLongMember(CObjectOStream& out,
                                           const CMemberInfo* memberInfo,
                                           TConstObjectPtr classPtr)
{
    bool haveSetFlag = memberInfo->HaveSetFlag();
    if ( haveSetFlag && !memberInfo->GetSetFlag(classPtr) ) {
        // not set -> skip this member
        return;
    }
    
    if ( memberInfo->CanBeDelayed() ) {
        const CDelayBuffer& buffer = memberInfo->GetDelayBuffer(classPtr);
        if ( buffer ) {
            if ( out.WriteClassMember(memberInfo->GetId(), buffer) )
                return;

            // cannot write delayed buffer -> proceed after update
            const_cast<CDelayBuffer&>(buffer).Update();
        }
    }
    
    TTypeInfo memberType = memberInfo->GetTypeInfo();
    TConstObjectPtr memberPtr = memberInfo->GetItemPtr(classPtr);
    if ( !haveSetFlag && memberInfo->Optional() ) {
        TConstObjectPtr defaultPtr = memberInfo->GetDefault();
        if ( !defaultPtr ) {
            if ( memberType->IsDefault(memberPtr) )
                return; // DEFAULT
        }
        else {
            if ( memberType->Equals(memberPtr, defaultPtr) )
                return; // OPTIONAL
        }
    }
    
    out.WriteClassMember(memberInfo->GetId(), memberType, memberPtr);
}

void CMemberInfoFunctions::CopySimpleMember(CObjectStreamCopier& copier,
                                            const CMemberInfo* memberInfo)
{
    copier.CopyObject(memberInfo->GetTypeInfo());
}

void CMemberInfoFunctions::CopyMissingSimpleMember(CObjectStreamCopier& copier,
                                                   const CMemberInfo* memberInfo)
{
    _ASSERT(!memberInfo->Optional());
    copier.ExpectedMember(memberInfo);
}

void CMemberInfoFunctions::CopyMissingOptionalMember(CObjectStreamCopier& /*copier*/,
                                                     const CMemberInfo* _DEBUG_ARG(memberInfo))
{
    _ASSERT(memberInfo->Optional());
}

void CMemberInfoFunctions::SkipSimpleMember(CObjectIStream& in,
                                            const CMemberInfo* memberInfo)
{
    in.SkipObject(memberInfo->GetTypeInfo());
}

void CMemberInfoFunctions::SkipMissingSimpleMember(CObjectIStream& in,
                                                   const CMemberInfo* memberInfo)
{
    _ASSERT(!memberInfo->Optional());
    in.ExpectedMember(memberInfo);
}

void CMemberInfoFunctions::SkipMissingOptionalMember(CObjectIStream& /*in*/,
                                                     const CMemberInfo* _DEBUG_ARG(memberInfo))
{
    _ASSERT(memberInfo->Optional());
}

void CMemberInfoFunctions::ReadHookedMember(CObjectIStream& stream,
                                            const CMemberInfo* memberInfo,
                                            TObjectPtr classPtr)
{
    CReadClassMemberHook* hook = memberInfo->m_ReadHookData.GetHook(&stream);
    if ( hook ) {
        CObjectInfo object(classPtr, memberInfo->GetClassType(),
                           CObjectInfo::eNonCObject);
        TMemberIndex index = memberInfo->GetIndex();
        hook->ReadClassMember(stream,
                              CObjectInfo::CMemberIterator(object,
                                                           index));
    }
    else
        memberInfo->DefaultReadMember(stream, classPtr);
}

void CMemberInfoFunctions::ReadMissingHookedMember(CObjectIStream& stream,
                                                   const CMemberInfo* memberInfo,
                                                   TObjectPtr classPtr)
{
    CReadClassMemberHook* hook = memberInfo->m_ReadHookData.GetHook(&stream);
    if ( hook ) {
        CObjectInfo object(classPtr, memberInfo->GetClassType(),
                           CObjectInfo::eNonCObject);
        TMemberIndex index = memberInfo->GetIndex();
        hook->ReadMissingClassMember(stream,
                                     CObjectInfo::CMemberIterator(object,
                                                                  index));
    }
    else
        memberInfo->DefaultReadMissingMember(stream, classPtr);
}

void CMemberInfoFunctions::WriteHookedMember(CObjectOStream& stream,
                                             const CMemberInfo* memberInfo,
                                             TConstObjectPtr classPtr)
{
    CWriteClassMemberHook* hook = memberInfo->m_WriteHookData.GetHook(&stream);
    if ( hook ) {
        CConstObjectInfo object(classPtr, memberInfo->GetClassType(),
                                CObjectInfo::eNonCObject);
        TMemberIndex index = memberInfo->GetIndex();
        hook->WriteClassMember(stream,
                               CConstObjectInfo::CMemberIterator(object,
                                                                 index));
    }
    else
        memberInfo->DefaultWriteMember(stream, classPtr);
}

void CMemberInfoFunctions::CopyHookedMember(CObjectStreamCopier& stream,
                                            const CMemberInfo* memberInfo)
{
    CCopyClassMemberHook* hook = memberInfo->m_CopyHookData.GetHook(&stream);
    if ( hook ) {
        CObjectTypeInfo type(memberInfo->GetClassType());
        TMemberIndex index = memberInfo->GetIndex();
        hook->CopyClassMember(stream,
                              CObjectTypeInfo::CMemberIterator(type,
                                                               index));
    }
    else
        memberInfo->DefaultCopyMember(stream);
}

void CMemberInfoFunctions::CopyMissingHookedMember(CObjectStreamCopier& stream,
                                                   const CMemberInfo* memberInfo)
{
    CCopyClassMemberHook* hook = memberInfo->m_CopyHookData.GetHook(&stream);
    if ( hook ) {
        CObjectTypeInfo type(memberInfo->GetClassType());
        TMemberIndex index = memberInfo->GetIndex();
        hook->CopyMissingClassMember(stream,
                                     CObjectTypeInfo::CMemberIterator(type,
                                                                      index));
    }
    else
        memberInfo->DefaultCopyMissingMember(stream);
}

void CMemberInfoFunctions::ReadParentClass(CObjectIStream& in,
                                           const CMemberInfo* /*memberInfo*/,
                                           TObjectPtr /*objectPtr*/)
{
    in.ThrowError(in.eFormatError, "cannot read parent class");
}

void CMemberInfoFunctions::ReadMissingParentClass(CObjectIStream& /*in*/,
                                                  const CMemberInfo* /*memberInfo*/,
                                                  TObjectPtr /*objectPtr*/)
{
}

void CMemberInfoFunctions::WriteParentClass(CObjectOStream& out,
                                            const CMemberInfo* /*memberInfo*/,
                                            TConstObjectPtr /*objectPtr*/)
{
}

void CMemberInfoFunctions::CopyParentClass(CObjectStreamCopier& copier,
                                           const CMemberInfo* /*memberInfo*/)
{
    copier.In().ThrowError(CObjectIStream::eFormatError,
                           "cannot copy parent class");
}

void CMemberInfoFunctions::CopyMissingParentClass(CObjectStreamCopier& /*copier*/,
                                                  const CMemberInfo* /*memberInfo*/)
{
}

void CMemberInfoFunctions::SkipParentClass(CObjectIStream& in,
                                           const CMemberInfo* /*memberInfo*/)
{
    in.ThrowError(in.eFormatError, "cannot skip parent class");
}

void CMemberInfoFunctions::SkipMissingParentClass(CObjectIStream& /*in*/,
                                                  const CMemberInfo* /*memberInfo*/)
{
}


END_NCBI_SCOPE
