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
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>

#include <serial/member.hpp>
#include <serial/objectinfo.hpp>
#include <serial/objectiter.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>
#include <serial/delaybuf.hpp>
#include <serial/serialimpl.hpp>
#ifdef _DEBUG
#include <serial/serialbase.hpp>
#endif
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
    static void ReadMissingWithSetFlagMember(CObjectIStream& in,
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
    static void SkipSimpleMember(CObjectIStream& in,
                                 const CMemberInfo* memberInfo);
    static void SkipMissingSimpleMember(CObjectIStream& in,
                                        const CMemberInfo* memberInfo);
    static void SkipMissingOptionalMember(CObjectIStream& in,
                                          const CMemberInfo* memberInfo);
    static void SkipHookedMember(CObjectIStream& in,
                                 const CMemberInfo* memberInfo);
    static void SkipMissingHookedMember(CObjectIStream& in,
                                        const CMemberInfo* memberInfo);
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

CMemberInfo::CMemberInfo(const CClassTypeInfoBase* classType,
                         const CMemberId& id, TPointerOffsetType offset,
                         const CTypeRef& type)
    : CParent(id, offset, type),
      m_ClassType(classType), m_Optional(false), m_NonEmpty(false), m_Default(0),
      m_SetFlagOffset(eNoOffset), m_BitSetFlag(false),
      m_DelayOffset(eNoOffset),
      m_GetConstFunction(&TFunc::GetConstSimpleMember),
      m_GetFunction(&TFunc::GetSimpleMember),
      m_ReadHookData(SMemberReadFunctions(&TFunc::ReadSimpleMember,
                                          &TFunc::ReadMissingSimpleMember),
                     SMemberReadFunctions(&TFunc::ReadHookedMember,
                                          &TFunc::ReadMissingHookedMember)),
      m_WriteHookData(&TFunc::WriteSimpleMember, &TFunc::WriteHookedMember),
      m_SkipHookData(SMemberSkipFunctions(&TFunc::SkipSimpleMember,
                                          &TFunc::SkipMissingSimpleMember),
                     SMemberSkipFunctions(&TFunc::SkipHookedMember,
                                          &TFunc::SkipMissingHookedMember)),
      m_CopyHookData(SMemberCopyFunctions(&TFunc::CopySimpleMember,
                                          &TFunc::CopyMissingSimpleMember),
                     SMemberCopyFunctions(&TFunc::CopyHookedMember,
                                          &TFunc::CopyMissingHookedMember))
{
}

CMemberInfo::CMemberInfo(const CClassTypeInfoBase* classType,
                         const CMemberId& id, TPointerOffsetType offset,
                         TTypeInfo type)
    : CParent(id, offset, type),
      m_ClassType(classType), m_Optional(false), m_NonEmpty(false), m_Default(0),
      m_SetFlagOffset(eNoOffset), m_BitSetFlag(false),
      m_DelayOffset(eNoOffset),
      m_GetConstFunction(&TFunc::GetConstSimpleMember),
      m_GetFunction(&TFunc::GetSimpleMember),
      m_ReadHookData(SMemberReadFunctions(&TFunc::ReadSimpleMember,
                                          &TFunc::ReadMissingSimpleMember),
                     SMemberReadFunctions(&TFunc::ReadHookedMember,
                                          &TFunc::ReadMissingHookedMember)),
      m_WriteHookData(&TFunc::WriteSimpleMember, &TFunc::WriteHookedMember),
      m_SkipHookData(SMemberSkipFunctions(&TFunc::SkipSimpleMember,
                                          &TFunc::SkipMissingSimpleMember),
                     SMemberSkipFunctions(&TFunc::SkipHookedMember,
                                          &TFunc::SkipMissingHookedMember)),
      m_CopyHookData(SMemberCopyFunctions(&TFunc::CopySimpleMember,
                                          &TFunc::CopyMissingSimpleMember),
                     SMemberCopyFunctions(&TFunc::CopyHookedMember,
                                          &TFunc::CopyMissingHookedMember))
{
}

CMemberInfo::CMemberInfo(const CClassTypeInfoBase* classType,
                         const char* id, TPointerOffsetType offset,
                         const CTypeRef& type)
    : CParent(id, offset, type),
      m_ClassType(classType), m_Optional(false), m_NonEmpty(false), m_Default(0),
      m_SetFlagOffset(eNoOffset), m_BitSetFlag(false),
      m_DelayOffset(eNoOffset),
      m_GetConstFunction(&TFunc::GetConstSimpleMember),
      m_GetFunction(&TFunc::GetSimpleMember),
      m_ReadHookData(SMemberReadFunctions(&TFunc::ReadSimpleMember,
                                          &TFunc::ReadMissingSimpleMember),
                     SMemberReadFunctions(&TFunc::ReadHookedMember,
                                          &TFunc::ReadMissingHookedMember)),
      m_WriteHookData(&TFunc::WriteSimpleMember, &TFunc::WriteHookedMember),
      m_SkipHookData(SMemberSkipFunctions(&TFunc::SkipSimpleMember,
                                          &TFunc::SkipMissingSimpleMember),
                     SMemberSkipFunctions(&TFunc::SkipHookedMember,
                                          &TFunc::SkipMissingHookedMember)),
      m_CopyHookData(SMemberCopyFunctions(&TFunc::CopySimpleMember,
                                          &TFunc::CopyMissingSimpleMember),
                     SMemberCopyFunctions(&TFunc::CopyHookedMember,
                                          &TFunc::CopyMissingHookedMember))
{
}

CMemberInfo::CMemberInfo(const CClassTypeInfoBase* classType,
                         const char* id, TPointerOffsetType offset,
                         TTypeInfo type)
    : CParent(id, offset, type),
      m_ClassType(classType), m_Optional(false), m_NonEmpty(false), m_Default(0),
      m_SetFlagOffset(eNoOffset), m_BitSetFlag(false),
      m_DelayOffset(eNoOffset),
      m_GetConstFunction(&TFunc::GetConstSimpleMember),
      m_GetFunction(&TFunc::GetSimpleMember),
      m_ReadHookData(SMemberReadFunctions(&TFunc::ReadSimpleMember,
                                          &TFunc::ReadMissingSimpleMember),
                     SMemberReadFunctions(&TFunc::ReadHookedMember,
                                          &TFunc::ReadMissingHookedMember)),
      m_WriteHookData(&TFunc::WriteSimpleMember, &TFunc::WriteHookedMember),
      m_SkipHookData(SMemberSkipFunctions(&TFunc::SkipSimpleMember,
                                          &TFunc::SkipMissingSimpleMember),
                     SMemberSkipFunctions(&TFunc::SkipHookedMember,
                                          &TFunc::SkipMissingHookedMember)),
      m_CopyHookData(SMemberCopyFunctions(&TFunc::CopySimpleMember,
                                          &TFunc::CopyMissingSimpleMember),
                     SMemberCopyFunctions(&TFunc::CopyHookedMember,
                                          &TFunc::CopyMissingHookedMember))
{
}

void CMemberInfo::SetParentClass(void)
{
    GetId().SetParentTag();
    m_ReadHookData.SetDefaultFunction(
        SMemberReadFunctions(&TFunc::ReadParentClass,
                             &TFunc::ReadMissingParentClass));
    m_WriteHookData.SetDefaultFunction(&TFunc::WriteParentClass);
    m_SkipHookData.SetDefaultFunction(
        SMemberSkipFunctions(&TFunc::SkipParentClass,
                             &TFunc::SkipMissingParentClass));
    m_CopyHookData.SetDefaultFunction(
        SMemberCopyFunctions(&TFunc::CopyParentClass,
                             &TFunc::CopyMissingParentClass));
}

CMemberInfo* CMemberInfo::SetDelayBuffer(CDelayBuffer* buffer)
{
    m_DelayOffset = TPointerOffsetType(buffer);
    UpdateFunctions();
    return this;
}

CMemberInfo* CMemberInfo::SetOptional(void)
{
    m_Optional = true;
    UpdateFunctions();
    return this;
}

CMemberInfo* CMemberInfo::SetNonEmpty(void)
{
    m_NonEmpty = true;
    return this;
}

CMemberInfo* CMemberInfo::SetNoPrefix(void)
{
    GetId().SetNoPrefix();
    return this;
}

CMemberInfo* CMemberInfo::SetAttlist(void)
{
    GetId().SetAttlist();
    return this;
}

CMemberInfo* CMemberInfo::SetNotag(void)
{
    GetId().SetNotag();
    return this;
}

CMemberInfo* CMemberInfo::SetAnyContent(void)
{
    GetId().SetAnyContent();
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
    m_SetFlagOffset = TPointerOffsetType(setFlag);
    m_BitSetFlag = false;
    UpdateFunctions();
    return this;
}

CMemberInfo* CMemberInfo::SetSetFlag(const Uint4* setFlag)
{
    m_SetFlagOffset = TPointerOffsetType(setFlag);
    m_BitSetFlag = true;
    UpdateFunctions();
    return this;
}

bool CMemberInfo::CompareSetFlags(TConstObjectPtr object1,
                                  TConstObjectPtr object2) const
{
    return GetSetFlagNo(object1) == GetSetFlagNo(object2);
}

CMemberInfo* CMemberInfo::SetOptional(const bool* setFlag)
{
    m_Optional = true;
    return SetSetFlag(setFlag);
}

void CMemberInfo::UpdateFunctions(void)
{
    // determine function pointers
    TMemberGetConst getConstFunc;
    TMemberGet getFunc;
    SMemberReadFunctions readFuncs;
    TMemberWriteFunction writeFunc;
    SMemberSkipFunctions skipFuncs;
    SMemberCopyFunctions copyFuncs;

    // get/readmain/write
    if ( CanBeDelayed() ) {
        getConstFunc = &TFunc::GetConstDelayedMember;
        getFunc = &TFunc::GetDelayedMember;
        readFuncs.m_Main = &TFunc::ReadLongMember;
        writeFunc = &TFunc::WriteLongMember;
    }
    else if ( !HaveSetFlag() ) {
        getConstFunc = &TFunc::GetConstSimpleMember;
        getFunc = &TFunc::GetSimpleMember;
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
        getConstFunc = &TFunc::GetConstSimpleMember;
        getFunc = &TFunc::GetWithSetFlagMember;
        readFuncs.m_Main = &TFunc::ReadWithSetFlagMember;
        writeFunc = &TFunc::WriteWithSetFlagMember;
    }

    // copymain/skipmain
    copyFuncs.m_Main = &TFunc::CopySimpleMember;
    skipFuncs.m_Main = &TFunc::SkipSimpleMember;

    // readmissing/copymissing/skipmissing
    if ( Optional() ) {
        if ( HaveSetFlag() ) {
            readFuncs.m_Missing = &TFunc::ReadMissingWithSetFlagMember;
        }
        else {
            readFuncs.m_Missing = &TFunc::ReadMissingOptionalMember;
        }
        copyFuncs.m_Missing = &TFunc::CopyMissingOptionalMember;
        skipFuncs.m_Missing = &TFunc::SkipMissingOptionalMember;
    }
    else {
        readFuncs.m_Missing = &TFunc::ReadMissingSimpleMember;
        copyFuncs.m_Missing = &TFunc::CopyMissingSimpleMember;
        skipFuncs.m_Missing = &TFunc::SkipMissingSimpleMember;
    }

    // update function pointers
    m_GetConstFunction = getConstFunc;
    m_GetFunction = getFunc;
    m_ReadHookData.SetDefaultFunction(readFuncs);
    m_WriteHookData.SetDefaultFunction(writeFunc);
    m_SkipHookData.SetDefaultFunction(skipFuncs);
    m_CopyHookData.SetDefaultFunction(copyFuncs);
}

void CMemberInfo::UpdateDelayedBuffer(CObjectIStream& in,
                                      TObjectPtr classPtr) const
{
    _ASSERT(CanBeDelayed());
    _ASSERT(GetDelayBuffer(classPtr).GetIndex() == GetIndex());

    GetTypeInfo()->ReadData(in, GetItemPtr(classPtr));
}

void CMemberInfo::SetReadFunction(TMemberReadFunction func)
{
    SMemberReadFunctions funcs = m_ReadHookData.GetDefaultFunction();
    funcs.m_Main = func;
    m_ReadHookData.SetDefaultFunction(funcs);
}

void CMemberInfo::SetReadMissingFunction(TMemberReadFunction func)
{
    SMemberReadFunctions funcs = m_ReadHookData.GetDefaultFunction();
    funcs.m_Missing = func;
    m_ReadHookData.SetDefaultFunction(funcs);
}

void CMemberInfo::SetWriteFunction(TMemberWriteFunction func)
{
    m_WriteHookData.SetDefaultFunction(func);
}

void CMemberInfo::SetSkipFunction(TMemberSkipFunction func)
{
    SMemberSkipFunctions funcs = m_SkipHookData.GetDefaultFunction();
    funcs.m_Main = func;
    m_SkipHookData.SetDefaultFunction(funcs);
}

void CMemberInfo::SetSkipMissingFunction(TMemberSkipFunction func)
{
    SMemberSkipFunctions funcs = m_SkipHookData.GetDefaultFunction();
    funcs.m_Missing = func;
    m_SkipHookData.SetDefaultFunction(funcs);
}

void CMemberInfo::SetCopyFunction(TMemberCopyFunction func)
{
    SMemberCopyFunctions funcs = m_CopyHookData.GetDefaultFunction();
    funcs.m_Main = func;
    m_CopyHookData.SetDefaultFunction(funcs);
}

void CMemberInfo::SetCopyMissingFunction(TMemberCopyFunction func)
{
    SMemberCopyFunctions funcs = m_CopyHookData.GetDefaultFunction();
    funcs.m_Missing = func;
    m_CopyHookData.SetDefaultFunction(funcs);
}

void CMemberInfo::SetGlobalReadHook(CReadClassMemberHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_ReadHookData.SetGlobalHook(hook);
}

void CMemberInfo::SetLocalReadHook(CObjectIStream& stream,
                                   CReadClassMemberHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_ReadHookData.SetLocalHook(stream.m_ClassMemberHookKey, hook);
}

void CMemberInfo::ResetGlobalReadHook(void)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_ReadHookData.ResetGlobalHook();
}

void CMemberInfo::ResetLocalReadHook(CObjectIStream& stream)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_ReadHookData.ResetLocalHook(stream.m_ClassMemberHookKey);
}

void CMemberInfo::SetPathReadHook(CObjectIStream* in, const string& path,
                                  CReadClassMemberHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_ReadHookData.SetPathHook(in,path,hook);
}

void CMemberInfo::SetGlobalWriteHook(CWriteClassMemberHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_WriteHookData.SetGlobalHook(hook);
}

void CMemberInfo::SetLocalWriteHook(CObjectOStream& stream,
                                    CWriteClassMemberHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_WriteHookData.SetLocalHook(stream.m_ClassMemberHookKey, hook);
}

void CMemberInfo::ResetGlobalWriteHook(void)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_WriteHookData.ResetGlobalHook();
}

void CMemberInfo::ResetLocalWriteHook(CObjectOStream& stream)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_WriteHookData.ResetLocalHook(stream.m_ClassMemberHookKey);
}

void CMemberInfo::SetPathWriteHook(CObjectOStream* out, const string& path,
                                   CWriteClassMemberHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_WriteHookData.SetPathHook(out,path,hook);
}

void CMemberInfo::SetGlobalSkipHook(CSkipClassMemberHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_SkipHookData.SetGlobalHook(hook);
}

void CMemberInfo::SetLocalSkipHook(CObjectIStream& stream,
                                   CSkipClassMemberHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_SkipHookData.SetLocalHook(stream.m_ClassMemberSkipHookKey, hook);
}

void CMemberInfo::ResetGlobalSkipHook(void)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_SkipHookData.ResetGlobalHook();
}

void CMemberInfo::ResetLocalSkipHook(CObjectIStream& stream)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_SkipHookData.ResetLocalHook(stream.m_ClassMemberSkipHookKey);
}

void CMemberInfo::SetPathSkipHook(CObjectIStream* in, const string& path,
                                  CSkipClassMemberHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_SkipHookData.SetPathHook(in,path,hook);
}

void CMemberInfo::SetGlobalCopyHook(CCopyClassMemberHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_CopyHookData.SetGlobalHook(hook);
}

void CMemberInfo::SetLocalCopyHook(CObjectStreamCopier& stream,
                                   CCopyClassMemberHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_CopyHookData.SetLocalHook(stream.m_ClassMemberHookKey, hook);
}

void CMemberInfo::ResetGlobalCopyHook(void)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_CopyHookData.ResetGlobalHook();
}

void CMemberInfo::ResetLocalCopyHook(CObjectStreamCopier& stream)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_CopyHookData.ResetLocalHook(stream.m_ClassMemberHookKey);
}

void CMemberInfo::SetPathCopyHook(CObjectStreamCopier* stream, const string& path,
                                  CCopyClassMemberHook* hook)
{
    CMutexGuard guard(GetTypeInfoMutex());
    m_CopyHookData.SetPathHook(stream ? &(stream->In()) : 0,path,hook);
}

TObjectPtr CMemberInfo::CreateClass(void) const
{
    return GetClassType()->Create();
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
    return memberInfo->GetItemPtr(classPtr);
}

TObjectPtr CMemberInfoFunctions::GetDelayedMember(const CMemberInfo* memberInfo,
                                                  TObjectPtr classPtr)
{
    _ASSERT(memberInfo->CanBeDelayed());
    memberInfo->GetDelayBuffer(classPtr).Update();
    memberInfo->UpdateSetFlagYes(classPtr);
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
    memberInfo->UpdateSetFlagYes(classPtr);
    try {
        in.ReadObject(memberInfo->GetItemPtr(classPtr),
                      memberInfo->GetTypeInfo());
    }
    catch (CSerialException& e) {
        if (e.GetErrCode() == CSerialException::eMissingValue) {
            if ( memberInfo->Optional() && memberInfo->HaveSetFlag() ) {
                in.SetFailFlags(CObjectIStream::fNoError,0);
                if ( memberInfo->UpdateSetFlagNo(classPtr) ) {
                    memberInfo->GetTypeInfo()->SetDefault(
                        memberInfo->GetItemPtr(classPtr));
                }
            } else {
                NCBI_RETHROW(e, CSerialException, eFormatError,
                    "error while reading " + memberInfo->GetId().GetName());
            }
        } else {
            NCBI_RETHROW_SAME(e,
                "error while reading " + memberInfo->GetId().GetName());
        }
    }
}

void CMemberInfoFunctions::ReadLongMember(CObjectIStream& in,
                                          const CMemberInfo* memberInfo,
                                          TObjectPtr classPtr)
{
    if ( memberInfo->CanBeDelayed() ) {
        CDelayBuffer& buffer = memberInfo->GetDelayBuffer(classPtr);
        if ( !buffer ) {
            memberInfo->UpdateSetFlagYes(classPtr);
            in.StartDelayBuffer();
            memberInfo->GetTypeInfo()->SkipData(in);
            in.EndDelayBuffer(buffer, memberInfo, classPtr);
            return;
        }
        buffer.Update();
    }
    
    memberInfo->UpdateSetFlagYes(classPtr);
    in.ReadObject(memberInfo->GetItemPtr(classPtr),
                  memberInfo->GetTypeInfo());
}

void CMemberInfoFunctions::ReadMissingSimpleMember(CObjectIStream& in,
                                                   const CMemberInfo* memberInfo,
                                                   TObjectPtr classPtr)
{
    _ASSERT(!memberInfo->Optional());
    in.ExpectedMember(memberInfo);

    if (memberInfo->HaveSetFlag()) {
        memberInfo->UpdateSetFlagNo(classPtr);
    }
    memberInfo->GetTypeInfo()->SetDefault(memberInfo->GetItemPtr(classPtr));
#ifdef _DEBUG
    if (in.GetVerifyData() == eSerialVerifyData_No) {
        if (memberInfo->GetTypeInfo()->GetTypeFamily() == eTypeFamilyPrimitive) {
            CObjectInfo objinfo(memberInfo->GetItemPtr(classPtr),
                                memberInfo->GetTypeInfo());
            if (objinfo.GetPrimitiveValueType() == ePrimitiveValueString) {
                objinfo.SetPrimitiveValueString(CSerialObject::ms_UnassignedStr);
            } else {
                size_t size = memberInfo->GetTypeInfo()->GetSize();
                if (size <= sizeof(long)) {
                    char* tmp = static_cast<char*>(
                        memberInfo->GetItemPtr(classPtr));
                    for (; size != 0; --size, ++tmp) {
                        *tmp = CSerialObject::ms_UnassignedByte;
                    }
                }
            }
        }
    }
#endif
}

void
CMemberInfoFunctions::ReadMissingOptionalMember(CObjectIStream& /*in*/,
                                                const CMemberInfo* memberInfo,
                                                TObjectPtr classPtr)
{
    _ASSERT(memberInfo->Optional());
    memberInfo->GetTypeInfo()->SetDefault(memberInfo->GetItemPtr(classPtr));
}

void
CMemberInfoFunctions::ReadMissingWithSetFlagMember(CObjectIStream& /*in*/,
                                                   const CMemberInfo* memberInfo,
                                                   TObjectPtr classPtr)
{
    _ASSERT(!memberInfo->CanBeDelayed());
    _ASSERT(memberInfo->HaveSetFlag());
    if ( memberInfo->UpdateSetFlagNo(classPtr) ) {
        memberInfo->GetTypeInfo()->SetDefault(memberInfo->GetItemPtr(classPtr));
    }
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
    if ( memberInfo->GetSetFlagNo(classPtr) ) {
        if (memberInfo->Optional()) {
            return;
        }
        ESerialVerifyData verify = out.GetVerifyData();
        if (verify == eSerialVerifyData_Yes) {
            out.ThrowError(CObjectOStream::fUnassigned,
                string("Unassigned member: ")+memberInfo->GetId().GetName());
        } else if (verify == eSerialVerifyData_No) {
            return;
        }
    }
#ifdef _DEBUG
    if (memberInfo->GetSetFlag(classPtr) == CMemberInfo::eSetMaybe &&
        memberInfo->GetTypeInfo()->GetTypeFamily() == eTypeFamilyPrimitive) {
        bool do_err_post = false;
        CConstObjectInfo objinfo(memberInfo->GetItemPtr(classPtr),
                                 memberInfo->GetTypeInfo());
        if (objinfo.GetPrimitiveValueType() == ePrimitiveValueString) {
            string tmp;
            objinfo.GetPrimitiveValueString(tmp);
            do_err_post = (tmp == CSerialObject::ms_UnassignedStr);
        } else {
            size_t size = memberInfo->GetTypeInfo()->GetSize();
            const char* tmp = static_cast<const char*>(
                memberInfo->GetItemPtr(classPtr));
            for (; size != 0 && *tmp == CSerialObject::ms_UnassignedByte; --size, ++tmp)
                ;
            do_err_post = (size == 0);
        }
        if (do_err_post) {
            ERR_POST(Error << "CObjectOStream: at "<< out.GetPosition()<<
                     ": Member \""<< memberInfo->GetId().GetName()<<
                     "\" seems to be unassigned");
        }
    }
#endif
    out.WriteClassMember(memberInfo->GetId(),
                         memberInfo->GetTypeInfo(),
                         memberInfo->GetItemPtr(classPtr));
}

void CMemberInfoFunctions::WriteLongMember(CObjectOStream& out,
                                           const CMemberInfo* memberInfo,
                                           TConstObjectPtr classPtr)
{
    bool haveSetFlag = memberInfo->HaveSetFlag();
    if ( haveSetFlag && memberInfo->GetSetFlagNo(classPtr) ) {
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
    CReadClassMemberHook* hook =
        memberInfo->m_ReadHookData.GetHook(stream.m_ClassMemberHookKey);
    if ( !hook ) {
        hook = memberInfo->m_ReadHookData.GetPathHook(stream);
    }
    if ( hook ) {
        CObjectInfo object(classPtr, memberInfo->GetClassType());
        TMemberIndex index = memberInfo->GetIndex();
        CObjectInfo::CMemberIterator member(object, index);
        _ASSERT(member.Valid());
        if (memberInfo->HaveSetFlag()) {
            memberInfo->UpdateSetFlagYes(classPtr);
        }
        hook->ReadClassMember(stream, member);
    }
    else
        memberInfo->DefaultReadMember(stream, classPtr);
}

void CMemberInfoFunctions::ReadMissingHookedMember(CObjectIStream& stream,
                                                   const CMemberInfo* memberInfo,
                                                   TObjectPtr classPtr)
{
    CReadClassMemberHook* hook =
        memberInfo->m_ReadHookData.GetHook(stream.m_ClassMemberHookKey);
    if ( !hook ) {
        hook = memberInfo->m_ReadHookData.GetPathHook(stream);
    }
    if ( hook ) {
        memberInfo->GetTypeInfo()->
            SetDefault(memberInfo->GetItemPtr(classPtr));
        CObjectInfo object(classPtr, memberInfo->GetClassType());
        TMemberIndex index = memberInfo->GetIndex();
        CObjectInfo::CMemberIterator member(object, index);
        _ASSERT(member.Valid());
        hook->ReadMissingClassMember(stream, member);
    }
    else
        memberInfo->DefaultReadMissingMember(stream, classPtr);
}

void CMemberInfoFunctions::WriteHookedMember(CObjectOStream& stream,
                                             const CMemberInfo* memberInfo,
                                             TConstObjectPtr classPtr)
{
    CWriteClassMemberHook* hook =
        memberInfo->m_WriteHookData.GetHook(stream.m_ClassMemberHookKey);
    if ( !hook ) {
        hook = memberInfo->m_WriteHookData.GetPathHook(stream);
    }
    if ( hook ) {
        CConstObjectInfo object(classPtr, memberInfo->GetClassType());
        TMemberIndex index = memberInfo->GetIndex();
        CConstObjectInfo::CMemberIterator member(object, index);
        _ASSERT(member.Valid());
        hook->WriteClassMember(stream, member);
    }
    else
        memberInfo->DefaultWriteMember(stream, classPtr);
}

void CMemberInfoFunctions::SkipHookedMember(CObjectIStream& stream,
                                            const CMemberInfo* memberInfo)
{
    CSkipClassMemberHook* hook =
        memberInfo->m_SkipHookData.GetHook(stream.m_ClassMemberSkipHookKey);
    if ( !hook ) {
        hook = memberInfo->m_SkipHookData.GetPathHook(stream);
    }
    if ( hook ) {
        CObjectTypeInfo type(memberInfo->GetClassType());
        TMemberIndex index = memberInfo->GetIndex();
        CObjectTypeInfo::CMemberIterator member(type, index);
        _ASSERT(member.Valid());
        hook->SkipClassMember(stream, member);
    }
    else
        memberInfo->DefaultSkipMember(stream);
}

void CMemberInfoFunctions::SkipMissingHookedMember(CObjectIStream& stream,
                                                   const CMemberInfo* memberInfo)
{
    CSkipClassMemberHook* hook =
        memberInfo->m_SkipHookData.GetHook(stream.m_ClassMemberSkipHookKey);
    if ( !hook ) {
        hook = memberInfo->m_SkipHookData.GetPathHook(stream);
    }
    if ( hook ) {
        CObjectTypeInfo type(memberInfo->GetClassType());
        TMemberIndex index = memberInfo->GetIndex();
        CObjectTypeInfo::CMemberIterator member(type, index);
        _ASSERT(member.Valid());
        hook->SkipMissingClassMember(stream, member);
    }
    else
        memberInfo->DefaultSkipMissingMember(stream);
}

void CMemberInfoFunctions::CopyHookedMember(CObjectStreamCopier& stream,
                                            const CMemberInfo* memberInfo)
{
    CCopyClassMemberHook* hook =
        memberInfo->m_CopyHookData.GetHook(stream.m_ClassMemberHookKey);
    if ( !hook ) {
        hook = memberInfo->m_CopyHookData.GetPathHook(stream.In());
    }
    if ( hook ) {
        CObjectTypeInfo type(memberInfo->GetClassType());
        TMemberIndex index = memberInfo->GetIndex();
        CObjectTypeInfo::CMemberIterator member(type, index);
        _ASSERT(member.Valid());
        hook->CopyClassMember(stream, member);
    }
    else
        memberInfo->DefaultCopyMember(stream);
}

void CMemberInfoFunctions::CopyMissingHookedMember(CObjectStreamCopier& stream,
                                                   const CMemberInfo* memberInfo)
{
    CCopyClassMemberHook* hook =
        memberInfo->m_CopyHookData.GetHook(stream.m_ClassMemberHookKey);
    if ( !hook ) {
        hook = memberInfo->m_CopyHookData.GetPathHook(stream.In());
    }
    if ( hook ) {
        CObjectTypeInfo type(memberInfo->GetClassType());
        TMemberIndex index = memberInfo->GetIndex();
        CObjectTypeInfo::CMemberIterator member(type, index);
        _ASSERT(member.Valid());
        hook->CopyMissingClassMember(stream, member);
    }
    else
        memberInfo->DefaultCopyMissingMember(stream);
}

void CMemberInfoFunctions::ReadParentClass(CObjectIStream& in,
                                           const CMemberInfo* memberInfo,
                                           TObjectPtr objectPtr)
{
    _ASSERT(!memberInfo->CanBeDelayed());
    _ASSERT(!memberInfo->HaveSetFlag());
    in.ReadObject(memberInfo->GetItemPtr(objectPtr),
                  memberInfo->GetTypeInfo());
}

void CMemberInfoFunctions::ReadMissingParentClass(CObjectIStream& in,
                                                  const CMemberInfo* memberInfo,
                                                  TObjectPtr /*objectPtr*/)
{
    _ASSERT(!memberInfo->Optional());
    in.ExpectedMember(memberInfo);
}

void CMemberInfoFunctions::WriteParentClass(CObjectOStream& out,
                                            const CMemberInfo* memberInfo,
                                            TConstObjectPtr objectPtr)
{
    _ASSERT(!memberInfo->CanBeDelayed());
    _ASSERT(!memberInfo->Optional());
    out.WriteClassMember(memberInfo->GetId(),
                         memberInfo->GetTypeInfo(),
                         memberInfo->GetItemPtr(objectPtr));
}

void CMemberInfoFunctions::CopyParentClass(CObjectStreamCopier& copier,
                                           const CMemberInfo* memberInfo)
{
    copier.CopyObject(memberInfo->GetTypeInfo());
}

void CMemberInfoFunctions::CopyMissingParentClass(CObjectStreamCopier& copier,
                                                  const CMemberInfo* memberInfo)
{
    _ASSERT(!memberInfo->Optional());
    copier.ExpectedMember(memberInfo);
}

void CMemberInfoFunctions::SkipParentClass(CObjectIStream& in,
                                           const CMemberInfo* memberInfo)
{
    in.SkipObject(memberInfo->GetTypeInfo());
}

void CMemberInfoFunctions::SkipMissingParentClass(CObjectIStream& in,
                                                  const CMemberInfo* memberInfo)
{
    _ASSERT(!memberInfo->Optional());
    in.ExpectedMember(memberInfo);
}


END_NCBI_SCOPE

/*
* $Log$
* Revision 1.38  2004/05/17 21:03:02  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.37  2004/01/22 20:45:41  gouriano
* Corrected reading of class members with possibly empty contents (bool,enum)
*
* Revision 1.36  2004/01/05 14:25:20  gouriano
* Added possibility to set serialization hooks by stack path
*
* Revision 1.35  2003/11/13 14:07:37  gouriano
* Elaborated data verification on read/write/get to enable skipping mandatory class data members
*
* Revision 1.34  2003/10/01 14:40:12  vasilche
* Fixed CanGet() for members wihout 'set' flag.
*
* Revision 1.33  2003/09/16 14:48:35  gouriano
* Enhanced AnyContent objects to support XML namespaces and attribute info items.
*
* Revision 1.32  2003/08/26 19:25:58  gouriano
* added possibility to discard a member of an STL container
* (from a read hook)
*
* Revision 1.31  2003/08/14 20:03:58  vasilche
* Avoid memory reallocation when reading over preallocated object.
* Simplified CContainerTypeInfo iterators interface.
*
* Revision 1.30  2003/07/29 18:47:47  vasilche
* Fixed thread safeness of object stream hooks.
*
* Revision 1.29  2003/06/24 20:57:36  gouriano
* corrected code generation and serialization of non-empty unnamed containers (XML)
*
* Revision 1.28  2003/05/06 16:23:42  gouriano
* write unassigned mandatory member when assignment verification is off
*
* Revision 1.27  2003/04/29 18:30:36  gouriano
* object data member initialization verification
*
* Revision 1.26  2003/04/10 20:13:39  vakatov
* Rollback the "uninitialized member" verification -- it still needs to
* be worked upon...
*
* Revision 1.24  2002/11/14 20:57:22  gouriano
* modified constructor to use CClassTypeInfoBase,
* added Attlist and Notag flags
*
* Revision 1.23  2002/09/25 19:37:36  gouriano
* added the possibility of having no tag prefix in XML I/O streams
*
* Revision 1.22  2002/09/09 18:14:02  grichenk
* Added CObjectHookGuard class.
* Added methods to be used by hooks for data
* reading and skipping.
*
* Revision 1.21  2001/07/25 19:14:24  grichenk
* Implemented functions for reading/writing parent classes
*
* Revision 1.20  2001/05/22 13:44:47  grichenk
* CMemberInfoFunctions::GetWithSetFlagMember() -- do not change SetFlag
*
* Revision 1.19  2001/05/17 15:07:06  lavr
* Typos corrected
*
* Revision 1.18  2000/10/20 15:51:38  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* Revision 1.17  2000/10/17 18:45:33  vasilche
* Added possibility to turn off object cross reference detection in
* CObjectIStream and CObjectOStream.
*
* Revision 1.16  2000/10/13 20:22:55  vasilche
* Fixed warnings on 64 bit compilers.
* Fixed missing typename in templates.
*
* Revision 1.15  2000/10/03 17:22:42  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.14  2000/09/29 16:18:22  vasilche
* Fixed binary format encoding/decoding on 64 bit compulers.
* Implemented CWeakMap<> for automatic cleaning map entries.
* Added cleaning local hooks via CWeakMap<>.
* Renamed ReadTypeName -> ReadFileHeader, ENoTypeName -> ENoFileHeader.
* Added some user interface methods to CObjectIStream, CObjectOStream and
* CObjectStreamCopier.
*
* Revision 1.13  2000/09/26 18:09:48  vasilche
* Fixed some warnings.
*
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
