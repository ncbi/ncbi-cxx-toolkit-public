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
* Revision 1.1  2000/09/18 20:00:26  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/variant.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>
#include <serial/delaybuf.hpp>

BEGIN_NCBI_SCOPE

CVariantInfo::CVariantInfo(const CChoiceTypeInfo* choiceType,
                           const CMemberId& id, TOffset offset,
                           const CTypeRef& type)
    : CParent(id, offset, type), m_ChoiceType(choiceType),
      m_VariantType(eInlineVariant), m_DelayOffset(TOffset(eNoOffset)),
      m_GetConstFunction(&GetConstInlineVariant),
      m_GetFunction(&GetInlineVariant),
      m_ReadHookData(&ReadInlineVariant, &ReadHookedVariant),
      m_WriteHookData(&WriteInlineVariant, &WriteHookedVariant),
      m_CopyHookData(&CopyNonObjectVariant, &CopyHookedVariant),
      m_SkipFunction(&SkipNonObjectVariant)
{
}

CVariantInfo::CVariantInfo(const CChoiceTypeInfo* choiceType,
                           const CMemberId& id, TOffset offset, TTypeInfo type)
    : CParent(id, offset, type), m_ChoiceType(choiceType),
      m_VariantType(eInlineVariant), m_DelayOffset(TOffset(eNoOffset)),
      m_GetConstFunction(&GetConstInlineVariant),
      m_GetFunction(&GetInlineVariant),
      m_ReadHookData(&ReadInlineVariant, &ReadHookedVariant),
      m_WriteHookData(&WriteInlineVariant, &WriteHookedVariant),
      m_CopyHookData(&CopyNonObjectVariant, &CopyHookedVariant),
      m_SkipFunction(&SkipNonObjectVariant)
{
}

CVariantInfo::CVariantInfo(const CChoiceTypeInfo* choiceType,
                           const char* id, TOffset offset,
                           const CTypeRef& type)
    : CParent(id, offset, type), m_ChoiceType(choiceType),
      m_VariantType(eInlineVariant), m_DelayOffset(TOffset(eNoOffset)),
      m_GetConstFunction(&GetConstInlineVariant),
      m_GetFunction(&GetInlineVariant),
      m_ReadHookData(&ReadInlineVariant, &ReadHookedVariant),
      m_WriteHookData(&WriteInlineVariant, &WriteHookedVariant),
      m_CopyHookData(&CopyNonObjectVariant, &CopyHookedVariant),
      m_SkipFunction(&SkipNonObjectVariant)
{
}

CVariantInfo::CVariantInfo(const CChoiceTypeInfo* choiceType,
                           const char* id, TOffset offset, TTypeInfo type)
    : CParent(id, offset, type), m_ChoiceType(choiceType),
      m_VariantType(eInlineVariant), m_DelayOffset(TOffset(eNoOffset)),
      m_GetConstFunction(&GetConstInlineVariant),
      m_GetFunction(&GetInlineVariant),
      m_ReadHookData(&ReadInlineVariant, &ReadHookedVariant),
      m_WriteHookData(&WriteInlineVariant, &WriteHookedVariant),
      m_CopyHookData(&CopyNonObjectVariant, &CopyHookedVariant),
      m_SkipFunction(&SkipNonObjectVariant)
{
}

CVariantInfo::~CVariantInfo(void)
{
}

CVariantInfo* CVariantInfo::SetPointer(void)
{
    m_VariantType = ePointerVariant;
    UpdateGetFunction();
    UpdateReadFunction();
    UpdateWriteFunction();
    UpdateCopyFunction();
    UpdateSkipFunction();
    return this;
}

CVariantInfo* CVariantInfo::SetObjectPointer(void)
{
    m_VariantType = eObjectPointerVariant;
    UpdateGetFunction();
    UpdateReadFunction();
    UpdateWriteFunction();
    UpdateCopyFunction();
    UpdateSkipFunction();
    return this;
}

CVariantInfo* CVariantInfo::SetDelayBuffer(CDelayBuffer* buffer)
{
    m_DelayOffset = size_t(buffer);
    UpdateGetFunction();
    UpdateReadFunction();
    UpdateWriteFunction();
    return this;
}

void CVariantInfo::UpdateGetFunction(void)
{
    if ( CanBeDelayed() ) {
        m_GetConstFunction = &GetConstDelayedVariant;
        m_GetFunction = &GetDelayedVariant;
    }
    else if ( IsPointer() ) {
        m_GetConstFunction = &GetConstPointerVariant;
        m_GetFunction = &GetPointerVariant;
    }
    else {
        m_GetConstFunction = &GetConstInlineVariant;
        m_GetFunction = &GetInlineVariant;
    }
}

void CVariantInfo::UpdateReadFunction(void)
{
    m_ReadHookData.GetDefaultFunction() =
        CanBeDelayed()? &ReadDelayedVariant:
        !IsPointer()? &ReadInlineVariant:
        IsObjectPointer()? &ReadObjectPointerVariant:
        &ReadPointerVariant;
}

void CVariantInfo::UpdateWriteFunction(void)
{
    m_WriteHookData.GetDefaultFunction() =
        CanBeDelayed()? &WriteDelayedVariant:
        !IsPointer()? &WriteInlineVariant:
        IsObjectPointer()? &WriteObjectPointerVariant:
        &WritePointerVariant;
}

void CVariantInfo::UpdateCopyFunction(void)
{
    m_CopyHookData.GetDefaultFunction() =
        IsObjectPointer()? &CopyObjectPointerVariant:
        &CopyNonObjectVariant;
}

void CVariantInfo::UpdateSkipFunction(void)
{
    m_SkipFunction =
        IsObjectPointer()? &SkipObjectPointerVariant:
        &SkipNonObjectVariant;
}

TConstObjectPtr
CVariantInfo::GetConstInlineVariant(const CVariantInfo* variantInfo,
                                    TConstObjectPtr choicePtr)
{
    _ASSERT(!variantInfo->CanBeDelayed());
    _ASSERT(!variantInfo->IsPointer());
    _ASSERT(variantInfo->GetChoiceType()->GetIndex(choicePtr) ==
            variantInfo->GetIndex());
    return variantInfo->GetItemPtr(choicePtr);
}

TConstObjectPtr
CVariantInfo::GetConstPointerVariant(const CVariantInfo* variantInfo,
                                     TConstObjectPtr choicePtr)
{
    _ASSERT(!variantInfo->CanBeDelayed());
    _ASSERT(variantInfo->IsPointer());
    _ASSERT(variantInfo->GetChoiceType()->GetIndex(choicePtr) ==
            variantInfo->GetIndex());
    TConstObjectPtr variantPtr = variantInfo->GetItemPtr(choicePtr);
    variantPtr = CTypeConverter<TConstObjectPtr>::Get(variantPtr);
    _ASSERT(variantPtr);
    return variantPtr;
}

TConstObjectPtr
CVariantInfo::GetConstDelayedVariant(const CVariantInfo* variantInfo,
                                     TConstObjectPtr choicePtr)
{
    _ASSERT(variantInfo->CanBeDelayed());
    _ASSERT(variantInfo->GetChoiceType()->GetIndex(choicePtr) ==
            variantInfo->GetIndex());
    const_cast<CDelayBuffer&>(variantInfo->GetDelayBuffer(choicePtr)).Update();
    TConstObjectPtr variantPtr = variantInfo->GetItemPtr(choicePtr);
    if ( variantInfo->IsPointer() ) {
        variantPtr = CTypeConverter<TConstObjectPtr>::Get(variantPtr);
        _ASSERT(variantPtr);
    }
    return variantPtr;
}

TObjectPtr CVariantInfo::GetInlineVariant(const CVariantInfo* variantInfo,
                                          TObjectPtr choicePtr)
{
    _ASSERT(!variantInfo->CanBeDelayed());
    _ASSERT(!variantInfo->IsPointer());
    _ASSERT(variantInfo->GetChoiceType()->GetIndex(choicePtr) ==
            variantInfo->GetIndex());
    return variantInfo->GetItemPtr(choicePtr);
}

TObjectPtr CVariantInfo::GetPointerVariant(const CVariantInfo* variantInfo,
                                           TObjectPtr choicePtr)
{
    _ASSERT(!variantInfo->CanBeDelayed());
    _ASSERT(variantInfo->IsPointer());
    _ASSERT(variantInfo->GetChoiceType()->GetIndex(choicePtr) ==
            variantInfo->GetIndex());
    TObjectPtr variantPtr = variantInfo->GetItemPtr(choicePtr);
    variantPtr = CTypeConverter<TObjectPtr>::Get(variantPtr);
    _ASSERT(variantPtr);
    return variantPtr;
}

TObjectPtr CVariantInfo::GetDelayedVariant(const CVariantInfo* variantInfo,
                                           TObjectPtr choicePtr)
{
    _ASSERT(variantInfo->CanBeDelayed());
    _ASSERT(variantInfo->GetChoiceType()->GetIndex(choicePtr) ==
            variantInfo->GetIndex());
    variantInfo->GetDelayBuffer(choicePtr).Update();
    TObjectPtr variantPtr = variantInfo->GetItemPtr(choicePtr);
    if ( variantInfo->IsPointer() ) {
        variantPtr = CTypeConverter<TObjectPtr>::Get(variantPtr);
        _ASSERT(variantPtr);
    }
    return variantPtr;
}

void CVariantInfo::ReadInlineVariant(CObjectIStream& in,
                                     const CVariantInfo* variantInfo,
                                     TObjectPtr choicePtr)
{
    _ASSERT(!variantInfo->IsPointer());
    const CChoiceTypeInfo* choiceType = variantInfo->GetChoiceType();
    TMemberIndex index = variantInfo->GetIndex();
    choiceType->SetIndex(choicePtr, index);
    in.ReadObject(variantInfo->GetItemPtr(choicePtr),
                  variantInfo->GetTypeInfo());
}

void CVariantInfo::ReadPointerVariant(CObjectIStream& in,
                                      const CVariantInfo* variantInfo,
                                      TObjectPtr choicePtr)
{
    _ASSERT(variantInfo->IsPointer());
    _ASSERT(!variantInfo->IsObjectPointer());
    const CChoiceTypeInfo* choiceType = variantInfo->GetChoiceType();
    TMemberIndex index = variantInfo->GetIndex();
    choiceType->SetIndex(choicePtr, index);
    TObjectPtr variantPtr = variantInfo->GetItemPtr(choicePtr);
    variantPtr = CTypeConverter<TObjectPtr>::Get(variantPtr);
    _ASSERT(variantPtr != 0 );
    in.ReadObject(variantPtr, variantInfo->GetTypeInfo());
}

void CVariantInfo::ReadObjectPointerVariant(CObjectIStream& in,
                                            const CVariantInfo* variantInfo,
                                            TObjectPtr choicePtr)
{
    _ASSERT(variantInfo->IsObjectPointer());
    const CChoiceTypeInfo* choiceType = variantInfo->GetChoiceType();
    TMemberIndex index = variantInfo->GetIndex();
    choiceType->SetIndex(choicePtr, index);
    TObjectPtr variantPtr = variantInfo->GetItemPtr(choicePtr);
    variantPtr = CTypeConverter<TObjectPtr>::Get(variantPtr);
    _ASSERT(variantPtr != 0 );
    in.ReadExternalObject(variantPtr, variantInfo->GetTypeInfo());
}

void CVariantInfo::ReadDelayedVariant(CObjectIStream& in,
                                      const CVariantInfo* variantInfo,
                                      TObjectPtr choicePtr)
{
    _ASSERT(variantInfo->CanBeDelayed());
    const CChoiceTypeInfo* choiceType = variantInfo->GetChoiceType();
    TMemberIndex index = variantInfo->GetIndex();
    TTypeInfo variantType = variantInfo->GetTypeInfo();
    if ( index != choiceType->GetIndex(choicePtr) ) {
        // index is differnet from current -> first, reset choice
        choiceType->ResetIndex(choicePtr);
        CDelayBuffer& buffer = variantInfo->GetDelayBuffer(choicePtr);
        if ( !buffer ) {
            in.StartDelayBuffer();
            if ( variantInfo->IsObjectPointer() )
                in.SkipExternalObject(variantType);
            else
                in.SkipObject(variantType);
            in.EndDelayBuffer(buffer, variantInfo, choicePtr);
            // update index
            choiceType->SetDelayIndex(choicePtr, index);
            return;
        }
        buffer.Update();
        _ASSERT(!variantInfo->GetDelayBuffer(choicePtr));
    }
    // select for reading
    choiceType->SetIndex(choicePtr, index);

    TObjectPtr variantPtr = variantInfo->GetItemPtr(choicePtr);
    if ( variantInfo->IsPointer() ) {
        variantPtr = CTypeConverter<TObjectPtr>::Get(variantPtr);
        _ASSERT(variantPtr != 0 );
        if ( variantInfo->IsObjectPointer() ) {
            in.ReadExternalObject(variantPtr, variantType);
            return;
        }
    }
    in.ReadObject(variantPtr, variantType);
}

void CVariantInfo::WriteInlineVariant(CObjectOStream& out,
                                      const CVariantInfo* variantInfo,
                                      TConstObjectPtr choicePtr)
{
    _ASSERT(!variantInfo->IsPointer());
    _ASSERT(variantInfo->GetChoiceType()->GetIndex(choicePtr) ==
            variantInfo->GetIndex());
    out.WriteObject(variantInfo->GetItemPtr(choicePtr),
                    variantInfo->GetTypeInfo());
}

void CVariantInfo::WritePointerVariant(CObjectOStream& out,
                                       const CVariantInfo* variantInfo,
                                       TConstObjectPtr choicePtr)
{
    _ASSERT(variantInfo->IsPointer());
    _ASSERT(!variantInfo->IsObjectPointer());
    _ASSERT(variantInfo->GetChoiceType()->GetIndex(choicePtr) ==
            variantInfo->GetIndex());
    TConstObjectPtr variantPtr = variantInfo->GetItemPtr(choicePtr);
    variantPtr = CTypeConverter<TConstObjectPtr>::Get(variantPtr);
    _ASSERT(variantPtr != 0 );
    out.WriteObject(variantPtr, variantInfo->GetTypeInfo());
}

void CVariantInfo::WriteObjectPointerVariant(CObjectOStream& out,
                                             const CVariantInfo* variantInfo,
                                             TConstObjectPtr choicePtr)
{
    _ASSERT(variantInfo->IsObjectPointer());
    _ASSERT(variantInfo->GetChoiceType()->GetIndex(choicePtr) ==
            variantInfo->GetIndex());
    TConstObjectPtr variantPtr = variantInfo->GetItemPtr(choicePtr);
    variantPtr = CTypeConverter<TConstObjectPtr>::Get(variantPtr);
    _ASSERT(variantPtr != 0 );
    out.WriteExternalObject(variantPtr, variantInfo->GetTypeInfo());
}

void CVariantInfo::WriteDelayedVariant(CObjectOStream& out,
                                       const CVariantInfo* variantInfo,
                                       TConstObjectPtr choicePtr)
{
    _ASSERT(variantInfo->CanBeDelayed());
    _ASSERT(variantInfo->GetChoiceType()->GetIndex(choicePtr) ==
            variantInfo->GetIndex());
    const CDelayBuffer& buffer = variantInfo->GetDelayBuffer(choicePtr);
    if ( buffer.GetIndex() == variantInfo->GetIndex() ) {
        if ( buffer.HaveFormat(out.GetDataFormat()) ) {
            out.Write(buffer.GetSource());
            return;
        }
        const_cast<CDelayBuffer&>(buffer).Update();
        _ASSERT(!variantInfo->GetDelayBuffer(choicePtr));
    }
    TConstObjectPtr variantPtr = variantInfo->GetItemPtr(choicePtr);
    if ( variantInfo->IsPointer() ) {
        variantPtr = CTypeConverter<TConstObjectPtr>::Get(variantPtr);
        _ASSERT(variantPtr != 0 );
        if ( variantInfo->IsObjectPointer() ) {
            out.WriteExternalObject(variantPtr, variantInfo->GetTypeInfo());
            return;
        }
    }
    out.WriteObject(variantPtr, variantInfo->GetTypeInfo());
}

void CVariantInfo::CopyNonObjectVariant(CObjectStreamCopier& copier,
                                        const CVariantInfo* variantInfo)
{
    copier.CopyObject(variantInfo->GetTypeInfo());
}

void CVariantInfo::CopyObjectPointerVariant(CObjectStreamCopier& copier,
                                            const CVariantInfo* variantInfo)
{
    copier.CopyExternalObject(variantInfo->GetTypeInfo());
}

void CVariantInfo::SkipNonObjectVariant(CObjectIStream& in,
                                        const CVariantInfo* variantInfo)
{
    in.SkipObject(variantInfo->GetTypeInfo());
}

void CVariantInfo::SkipObjectPointerVariant(CObjectIStream& in,
                                            const CVariantInfo* variantInfo)
{
    in.SkipExternalObject(variantInfo->GetTypeInfo());
}

void CVariantInfo::UpdateDelayedBuffer(CObjectIStream& in,
                                       TObjectPtr choicePtr) const
{
    _ASSERT(CanBeDelayed());
    _ASSERT(GetDelayBuffer(choicePtr).GetIndex() == GetIndex());

    TObjectPtr variantPtr = GetItemPtr(choicePtr);
    TTypeInfo variantType = GetTypeInfo();
    if ( IsPointer() ) {
        // create object itself
        variantPtr = CTypeConverter<TObjectPtr>::Get(variantPtr) =
            variantType->Create();
        if ( IsObjectPointer() ) {
            _TRACE("Should check for real pointer type (CRef...)");
            CTypeConverter<CObject>::Get(variantPtr).AddReference();
        }
    }

    variantType->ReadData(in, variantPtr);
}

void CVariantInfo::SetReadFunction(TVariantRead func)
{
    m_ReadHookData.GetDefaultFunction() = func;
}

void CVariantInfo::SetWriteFunction(TVariantWrite func)
{
    m_WriteHookData.GetDefaultFunction() = func;
}

void CVariantInfo::SetCopyFunction(TVariantCopy func)
{
    m_CopyHookData.GetDefaultFunction() = func;
}

void CVariantInfo::SetSkipFunction(TVariantSkip func)
{
    m_SkipFunction = func;
}

void CVariantInfo::ReadHookedVariant(CObjectIStream& stream,
                                     const CVariantInfo* variantInfo,
                                     TObjectPtr choicePtr)
{
    CReadChoiceVariantHook* hook = variantInfo->m_ReadHookData.GetHook(&stream);
    if ( hook )
        hook->ReadChoiceVariant(stream,
                                CObjectInfo(choicePtr,
                                            variantInfo->GetChoiceType(),
                                            CObjectInfo::eNonCObject),
                                variantInfo->GetIndex());
    else
        variantInfo->DefaultReadVariant(stream, choicePtr);
}

void CVariantInfo::WriteHookedVariant(CObjectOStream& stream,
                                      const CVariantInfo* variantInfo,
                                      TConstObjectPtr choicePtr)
{
    CWriteChoiceVariantHook* hook = variantInfo->m_WriteHookData.GetHook(&stream);
    if ( hook )
        hook->WriteChoiceVariant(stream,
                                 CConstObjectInfo(choicePtr,
                                                  variantInfo->GetChoiceType(),
                                                  CObjectInfo::eNonCObject),
                                 variantInfo->GetIndex());
    else
        variantInfo->DefaultWriteVariant(stream, choicePtr);
}

void CVariantInfo::CopyHookedVariant(CObjectStreamCopier& stream,
                                     const CVariantInfo* variantInfo)
{
    CCopyChoiceVariantHook* hook = variantInfo->m_CopyHookData.GetHook(&stream);
    if ( hook )
        hook->CopyChoiceVariant(stream,
                                variantInfo->GetChoiceType(),
                                variantInfo->GetIndex());
    else
        variantInfo->DefaultCopyVariant(stream);
}

END_NCBI_SCOPE
