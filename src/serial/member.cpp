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

BEGIN_NCBI_SCOPE

CMemberInfo::CMemberInfo(const CClassTypeInfo* classType,
                         const CMemberId& id, TOffset offset,
                         const CTypeRef& type)
    : CParent(id, offset, type),
      m_ClassType(classType), m_Optional(false), m_Default(0),
      m_SetFlagOffset(TOffset(eNoOffset)), m_DelayOffset(TOffset(eNoOffset)),
      m_GetConstFunction(&GetConstSimpleMember),
      m_GetFunction(&GetSimpleMember),
      m_ReadHookData(make_pair(&ReadSimpleMember, &ReadMissingSimpleMember),
                     make_pair(&ReadHookedMember, &ReadMissingHookedMember)),
      m_WriteHookData(&WriteSimpleMember, &WriteHookedMember),
      m_CopyHookData(make_pair(&CopySimpleMember, &CopyMissingSimpleMember),
                     make_pair(&CopyHookedMember, &CopyMissingHookedMember)),
      m_SkipFunction(&SkipSimpleMember),
      m_SkipMissingFunction(&SkipMissingSimpleMember)
{
}

CMemberInfo::CMemberInfo(const CClassTypeInfo* classType,
                         const CMemberId& id, TOffset offset, TTypeInfo type)
    : CParent(id, offset, type),
      m_ClassType(classType), m_Optional(false), m_Default(0),
      m_SetFlagOffset(TOffset(eNoOffset)), m_DelayOffset(TOffset(eNoOffset)),
      m_GetConstFunction(&GetConstSimpleMember),
      m_GetFunction(&GetSimpleMember),
      m_ReadHookData(make_pair(&ReadSimpleMember, &ReadMissingSimpleMember),
                     make_pair(&ReadHookedMember, &ReadMissingHookedMember)),
      m_WriteHookData(&WriteSimpleMember, &WriteHookedMember),
      m_CopyHookData(make_pair(&CopySimpleMember, &CopyMissingSimpleMember),
                     make_pair(&CopyHookedMember, &CopyMissingHookedMember)),
      m_SkipFunction(&SkipSimpleMember),
      m_SkipMissingFunction(&SkipMissingSimpleMember)
{
}

CMemberInfo::CMemberInfo(const CClassTypeInfo* classType,
                         const char* id, TOffset offset, const CTypeRef& type)
    : CParent(id, offset, type),
      m_ClassType(classType), m_Optional(false), m_Default(0),
      m_SetFlagOffset(TOffset(eNoOffset)), m_DelayOffset(TOffset(eNoOffset)),
      m_GetConstFunction(&GetConstSimpleMember),
      m_GetFunction(&GetSimpleMember),
      m_ReadHookData(make_pair(&ReadSimpleMember, &ReadMissingSimpleMember),
                     make_pair(&ReadHookedMember, &ReadMissingHookedMember)),
      m_WriteHookData(&WriteSimpleMember, &WriteHookedMember),
      m_CopyHookData(make_pair(&CopySimpleMember, &CopyMissingSimpleMember),
                     make_pair(&CopyHookedMember, &CopyMissingHookedMember)),
      m_SkipFunction(&SkipSimpleMember),
      m_SkipMissingFunction(&SkipMissingSimpleMember)
{
}

CMemberInfo::CMemberInfo(const CClassTypeInfo* classType,
                         const char* id, TOffset offset, TTypeInfo type)
    : CParent(id, offset, type),
      m_ClassType(classType), m_Optional(false), m_Default(0),
      m_SetFlagOffset(TOffset(eNoOffset)), m_DelayOffset(TOffset(eNoOffset)),
      m_GetConstFunction(&GetConstSimpleMember),
      m_GetFunction(&GetSimpleMember),
      m_ReadHookData(make_pair(&ReadSimpleMember, &ReadMissingSimpleMember),
                     make_pair(&ReadHookedMember, &ReadMissingHookedMember)),
      m_WriteHookData(&WriteSimpleMember, &WriteHookedMember),
      m_CopyHookData(make_pair(&CopySimpleMember, &CopyMissingSimpleMember),
                     make_pair(&CopyHookedMember, &CopyMissingHookedMember)),
      m_SkipFunction(&SkipSimpleMember),
      m_SkipMissingFunction(&SkipMissingSimpleMember)
{
}

CMemberInfo::~CMemberInfo(void)
{
}

CMemberInfo* CMemberInfo::SetDelayBuffer(CDelayBuffer* buffer)
{
    m_DelayOffset = size_t(buffer);
    UpdateGetFunction();
    UpdateReadFunction();
    UpdateWriteFunction();
    return this;
}

CMemberInfo* CMemberInfo::SetOptional(void)
{
    m_Optional = true;
    UpdateReadMissingFunction();
    UpdateWriteFunction();
    UpdateCopyMissingFunction();
    UpdateSkipMissingFunction();
    return this;
}

CMemberInfo* CMemberInfo::SetDefault(TConstObjectPtr def)
{
    m_Optional = true;
    m_Default = def;
    UpdateReadMissingFunction();
    UpdateWriteFunction();
    UpdateCopyMissingFunction();
    UpdateSkipMissingFunction();
    return this;
}

CMemberInfo* CMemberInfo::SetSetFlag(const bool* setFlag)
{
    _ASSERT(Optional());
    m_SetFlagOffset = size_t(setFlag);
    UpdateGetFunction();
    UpdateReadFunction();
    UpdateReadMissingFunction();
    UpdateWriteFunction();
    UpdateCopyMissingFunction();
    UpdateSkipMissingFunction();
    return this;
}

CMemberInfo* CMemberInfo::SetOptional(const bool* setFlag)
{
    m_Optional = true;
    return SetSetFlag(setFlag);
}

void CMemberInfo::UpdateGetFunction(void)
{
    if ( CanBeDelayed() ) {
        m_GetConstFunction = &GetConstDelayedMember;
        m_GetFunction = &GetDelayedMember;
    }
    else {
        m_GetConstFunction = &GetConstSimpleMember;
        m_GetFunction = HaveSetFlag()? &GetWithSetFlagMember: &GetSimpleMember;
    }
}

void CMemberInfo::UpdateReadFunction(void)
{
    m_ReadHookData.GetDefaultFunction().first =
        CanBeDelayed()? &ReadLongMember:
        HaveSetFlag()? &ReadWithSetFlagMember: &ReadSimpleMember;
}

void CMemberInfo::UpdateReadMissingFunction(void)
{
    m_ReadHookData.GetDefaultFunction().second =
        Optional()? &ReadMissingOptionalMember: &ReadMissingSimpleMember;
}

void CMemberInfo::UpdateWriteFunction(void)
{
    m_WriteHookData.GetDefaultFunction() =
        CanBeDelayed()? &WriteLongMember:
        HaveSetFlag()? &WriteWithSetFlagMember:
        GetDefault()? &WriteWithDefaultMember:
        Optional()? &WriteOptionalMember: &WriteSimpleMember;
}

void CMemberInfo::UpdateCopyMissingFunction(void)
{
    m_CopyHookData.GetDefaultFunction().second =
        Optional()? &CopyMissingOptionalMember: &CopyMissingSimpleMember;
}

void CMemberInfo::UpdateSkipMissingFunction(void)
{
    m_SkipMissingFunction =
        Optional()? &SkipMissingOptionalMember: &SkipMissingSimpleMember;
}

TConstObjectPtr
CMemberInfo::GetConstSimpleMember(const CMemberInfo* memberInfo,
                                  TConstObjectPtr classPtr)
{
    _ASSERT(!memberInfo->CanBeDelayed());
    return memberInfo->GetItemPtr(classPtr);
}

TConstObjectPtr
CMemberInfo::GetConstDelayedMember(const CMemberInfo* memberInfo,
                                   TConstObjectPtr classPtr)
{
    _ASSERT(memberInfo->CanBeDelayed());
    const_cast<CDelayBuffer&>(memberInfo->GetDelayBuffer(classPtr)).Update();
    return memberInfo->GetItemPtr(classPtr);
}

TObjectPtr CMemberInfo::GetSimpleMember(const CMemberInfo* memberInfo,
                                        TObjectPtr classPtr)
{
    _ASSERT(!memberInfo->CanBeDelayed());
    _ASSERT(!memberInfo->HaveSetFlag());
    return memberInfo->GetItemPtr(classPtr);
}

TObjectPtr CMemberInfo::GetWithSetFlagMember(const CMemberInfo* memberInfo,
                                             TObjectPtr classPtr)
{
    _ASSERT(!memberInfo->CanBeDelayed());
    _ASSERT(memberInfo->HaveSetFlag());
    memberInfo->GetSetFlag(classPtr) = true;
    return memberInfo->GetItemPtr(classPtr);
}

TObjectPtr CMemberInfo::GetDelayedMember(const CMemberInfo* memberInfo,
                                         TObjectPtr classPtr)
{
    _ASSERT(memberInfo->CanBeDelayed());
    memberInfo->GetDelayBuffer(classPtr).Update();
    memberInfo->UpdateSetFlag(classPtr, true);
    return memberInfo->GetItemPtr(classPtr);
}

void CMemberInfo::ReadSimpleMember(CObjectIStream& in,
                                   const CMemberInfo* memberInfo,
                                   TObjectPtr classPtr)
{
    _ASSERT(!memberInfo->CanBeDelayed());
    _ASSERT(!memberInfo->HaveSetFlag());
    in.ReadObject(memberInfo->GetItemPtr(classPtr),
                  memberInfo->GetTypeInfo());
}

void CMemberInfo::ReadWithSetFlagMember(CObjectIStream& in,
                                        const CMemberInfo* memberInfo,
                                        TObjectPtr classPtr)
{
    _ASSERT(!memberInfo->CanBeDelayed());
    _ASSERT(memberInfo->HaveSetFlag());
    in.ReadObject(memberInfo->GetItemPtr(classPtr),
                  memberInfo->GetTypeInfo());
    memberInfo->GetSetFlag(classPtr) = true;
}

void CMemberInfo::ReadLongMember(CObjectIStream& in,
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

void CMemberInfo::ReadMissingSimpleMember(CObjectIStream& in,
                                          const CMemberInfo* memberInfo,
                                          TObjectPtr /*classPtr*/)
{
    _ASSERT(!memberInfo->Optional());
    in.ExpectedMember(memberInfo);
}

void CMemberInfo::ReadMissingOptionalMember(CObjectIStream& /*in*/,
                                            const CMemberInfo* _DEBUG_ARG(memberInfo),
                                            TObjectPtr /*classPtr*/)
{
    _ASSERT(memberInfo->Optional());
}

void CMemberInfo::WriteSimpleMember(CObjectOStream& out,
                                    const CMemberInfo* memberInfo,
                                    TConstObjectPtr classPtr)
{
    _ASSERT(!memberInfo->CanBeDelayed());
    _ASSERT(!memberInfo->Optional());
    out.WriteClassMember(memberInfo->GetId(),
                         memberInfo->GetTypeInfo(),
                         memberInfo->GetItemPtr(classPtr));
}

void CMemberInfo::WriteOptionalMember(CObjectOStream& out,
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

void CMemberInfo::WriteWithDefaultMember(CObjectOStream& out,
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

void CMemberInfo::WriteWithSetFlagMember(CObjectOStream& out,
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

void CMemberInfo::WriteLongMember(CObjectOStream& out,
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

void CMemberInfo::CopySimpleMember(CObjectStreamCopier& copier,
                                   const CMemberInfo* memberInfo)
{
    copier.CopyObject(memberInfo->GetTypeInfo());
}

void CMemberInfo::CopyMissingSimpleMember(CObjectStreamCopier& copier,
                                          const CMemberInfo* memberInfo)
{
    _ASSERT(!memberInfo->Optional());
    copier.ExpectedMember(memberInfo);
}

void CMemberInfo::CopyMissingOptionalMember(CObjectStreamCopier& /*copier*/,
                                            const CMemberInfo* _DEBUG_ARG(memberInfo))
{
    _ASSERT(memberInfo->Optional());
}

void CMemberInfo::SkipSimpleMember(CObjectIStream& in,
                                   const CMemberInfo* memberInfo)
{
    in.SkipObject(memberInfo->GetTypeInfo());
}

void CMemberInfo::SkipMissingSimpleMember(CObjectIStream& in,
                                          const CMemberInfo* memberInfo)
{
    _ASSERT(!memberInfo->Optional());
    in.ExpectedMember(memberInfo);
}

void CMemberInfo::SkipMissingOptionalMember(CObjectIStream& /*in*/,
                                            const CMemberInfo* _DEBUG_ARG(memberInfo))
{
    _ASSERT(memberInfo->Optional());
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
    m_ReadHookData.GetDefaultFunction().first = func;
}

void CMemberInfo::SetReadMissingFunction(TMemberRead func)
{
    m_ReadHookData.GetDefaultFunction().second = func;
}

void CMemberInfo::SetWriteFunction(TMemberWrite func)
{
    m_WriteHookData.GetDefaultFunction() = func;
}

void CMemberInfo::SetCopyFunction(TMemberCopy func)
{
    m_CopyHookData.GetDefaultFunction().first = func;
}

void CMemberInfo::SetCopyMissingFunction(TMemberCopy func)
{
    m_CopyHookData.GetDefaultFunction().second = func;
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

void CMemberInfo::ReadHookedMember(CObjectIStream& stream,
                                   const CMemberInfo* memberInfo,
                                   TObjectPtr classPtr)
{
    CReadClassMemberHook* hook = memberInfo->m_ReadHookData.GetHook(&stream);
    if ( hook )
        hook->ReadClassMember(stream,
                              CObjectInfo(classPtr,
                                          memberInfo->GetClassType(),
                                          CObjectInfo::eNonCObject),
                              memberInfo->GetIndex());
    else
        memberInfo->DefaultReadMember(stream, classPtr);
}

void CMemberInfo::ReadMissingHookedMember(CObjectIStream& stream,
                                          const CMemberInfo* memberInfo,
                                          TObjectPtr classPtr)
{
    CReadClassMemberHook* hook = memberInfo->m_ReadHookData.GetHook(&stream);
    if ( hook )
        hook->ReadMissingClassMember(stream,
                                     CObjectInfo(classPtr,
                                                 memberInfo->GetClassType(),
                                                 CObjectInfo::eNonCObject),
                                     memberInfo->GetIndex());
    else
        memberInfo->DefaultReadMissingMember(stream, classPtr);
}

void CMemberInfo::WriteHookedMember(CObjectOStream& stream,
                                    const CMemberInfo* memberInfo,
                                    TConstObjectPtr classPtr)
{
    CWriteClassMemberHook* hook = memberInfo->m_WriteHookData.GetHook(&stream);
    if ( hook )
        hook->WriteClassMember(stream,
                               CConstObjectInfo(classPtr,
                                                memberInfo->GetClassType(),
                                                CObjectInfo::eNonCObject),
                               memberInfo->GetIndex());
    else
        memberInfo->DefaultWriteMember(stream, classPtr);
}

void CMemberInfo::CopyHookedMember(CObjectStreamCopier& stream,
                                   const CMemberInfo* memberInfo)
{
    CCopyClassMemberHook* hook = memberInfo->m_CopyHookData.GetHook(&stream);
    if ( hook )
        hook->CopyClassMember(stream,
                              memberInfo->GetClassType(),
                              memberInfo->GetIndex());
    else
        memberInfo->DefaultCopyMember(stream);
}

void CMemberInfo::CopyMissingHookedMember(CObjectStreamCopier& stream,
                                          const CMemberInfo* memberInfo)
{
    CCopyClassMemberHook* hook = memberInfo->m_CopyHookData.GetHook(&stream);
    if ( hook )
        hook->CopyMissingClassMember(stream,
                                     memberInfo->GetClassType(),
                                     memberInfo->GetIndex());
    else
        memberInfo->DefaultCopyMissingMember(stream);
}

END_NCBI_SCOPE
