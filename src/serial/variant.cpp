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
* Revision 1.3  2000/09/26 19:24:58  vasilche
* Added user interface for setting read/write/copy hooks.
*
* Revision 1.2  2000/09/26 17:38:23  vasilche
* Fixed incomplete choiceptr implementation.
* Removed temporary comments.
*
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
#include <serial/choiceptr.hpp>
#include <serial/ptrinfo.hpp>

BEGIN_NCBI_SCOPE

class CVariantInfoFunctions
{
public:

    static
    TConstObjectPtr GetConstInlineVariant(const CVariantInfo* variantInfo,
                                          TConstObjectPtr choicePtr);
    static
    TConstObjectPtr GetConstPointerVariant(const CVariantInfo* variantInfo,
                                           TConstObjectPtr choicePtr);
    static
    TConstObjectPtr GetConstDelayedVariant(const CVariantInfo* variantInfo,
                                           TConstObjectPtr choicePtr);
    static
    TConstObjectPtr GetConstSubclassVariant(const CVariantInfo* variantInfo,
                                            TConstObjectPtr choicePtr);
    static TObjectPtr GetInlineVariant(const CVariantInfo* variantInfo,
                                       TObjectPtr choicePtr);
    static TObjectPtr GetPointerVariant(const CVariantInfo* variantInfo,
                                        TObjectPtr choicePtr);
    static TObjectPtr GetDelayedVariant(const CVariantInfo* variantInfo,
                                        TObjectPtr choicePtr);
    static TObjectPtr GetSubclassVariant(const CVariantInfo* variantInfo,
                                         TObjectPtr choicePtr);

    static void ReadInlineVariant(CObjectIStream& in,
                                  const CVariantInfo* variantInfo,
                                  TObjectPtr choicePtr);
    static void ReadPointerVariant(CObjectIStream& in,
                                   const CVariantInfo* variantInfo,
                                   TObjectPtr choicePtr);
    static void ReadObjectPointerVariant(CObjectIStream& in,
                                         const CVariantInfo* variantInfo,
                                         TObjectPtr choicePtr);
    static void ReadDelayedVariant(CObjectIStream& in,
                                   const CVariantInfo* variantInfo,
                                   TObjectPtr choicePtr);
    static void ReadSubclassVariant(CObjectIStream& in,
                                    const CVariantInfo* variantInfo,
                                    TObjectPtr choicePtr);
    static void ReadHookedVariant(CObjectIStream& in,
                                  const CVariantInfo* variantInfo,
                                  TObjectPtr choicePtr);
    static void WriteInlineVariant(CObjectOStream& out,
                                   const CVariantInfo* variantInfo,
                                   TConstObjectPtr choicePtr);
    static void WritePointerVariant(CObjectOStream& out,
                                    const CVariantInfo* variantInfo,
                                    TConstObjectPtr choicePtr);
    static void WriteObjectPointerVariant(CObjectOStream& out,
                                          const CVariantInfo* variantInfo,
                                          TConstObjectPtr choicePtr);
    static void WriteSubclassVariant(CObjectOStream& out,
                                     const CVariantInfo* variantInfo,
                                     TConstObjectPtr choicePtr);
    static void WriteDelayedVariant(CObjectOStream& out,
                                    const CVariantInfo* variantInfo,
                                    TConstObjectPtr choicePtr);
    static void WriteHookedVariant(CObjectOStream& out,
                                   const CVariantInfo* variantInfo,
                                   TConstObjectPtr choicePtr);
    static void CopyNonObjectVariant(CObjectStreamCopier& copier,
                                     const CVariantInfo* variantInfo);
    static void CopyObjectPointerVariant(CObjectStreamCopier& copier,
                                         const CVariantInfo* variantInfo);
    static void CopyHookedVariant(CObjectStreamCopier& copier,
                                  const CVariantInfo* variantInfo);
    static void SkipNonObjectVariant(CObjectIStream& in,
                                     const CVariantInfo* variantInfo);
    static void SkipObjectPointerVariant(CObjectIStream& in,
                                         const CVariantInfo* variantInfo);
};

typedef CVariantInfoFunctions TFunc;

CVariantInfo::CVariantInfo(const CChoiceTypeInfo* choiceType,
                           const CMemberId& id, TOffset offset,
                           const CTypeRef& type)
    : CParent(id, offset, type), m_ChoiceType(choiceType),
      m_VariantType(eInlineVariant), m_DelayOffset(TOffset(eNoOffset)),
      m_GetConstFunction(&TFunc::GetConstInlineVariant),
      m_GetFunction(&TFunc::GetInlineVariant),
      m_ReadHookData(&TFunc::ReadInlineVariant, &TFunc::ReadHookedVariant),
      m_WriteHookData(&TFunc::WriteInlineVariant, &TFunc::WriteHookedVariant),
      m_CopyHookData(&TFunc::CopyNonObjectVariant, &TFunc::CopyHookedVariant),
      m_SkipFunction(&TFunc::SkipNonObjectVariant)
{
}

CVariantInfo::CVariantInfo(const CChoiceTypeInfo* choiceType,
                           const CMemberId& id, TOffset offset, TTypeInfo type)
    : CParent(id, offset, type), m_ChoiceType(choiceType),
      m_VariantType(eInlineVariant), m_DelayOffset(TOffset(eNoOffset)),
      m_GetConstFunction(&TFunc::GetConstInlineVariant),
      m_GetFunction(&TFunc::GetInlineVariant),
      m_ReadHookData(&TFunc::ReadInlineVariant, &TFunc::ReadHookedVariant),
      m_WriteHookData(&TFunc::WriteInlineVariant, &TFunc::WriteHookedVariant),
      m_CopyHookData(&TFunc::CopyNonObjectVariant, &TFunc::CopyHookedVariant),
      m_SkipFunction(&TFunc::SkipNonObjectVariant)
{
}

CVariantInfo::CVariantInfo(const CChoiceTypeInfo* choiceType,
                           const char* id, TOffset offset,
                           const CTypeRef& type)
    : CParent(id, offset, type), m_ChoiceType(choiceType),
      m_VariantType(eInlineVariant), m_DelayOffset(TOffset(eNoOffset)),
      m_GetConstFunction(&TFunc::GetConstInlineVariant),
      m_GetFunction(&TFunc::GetInlineVariant),
      m_ReadHookData(&TFunc::ReadInlineVariant, &TFunc::ReadHookedVariant),
      m_WriteHookData(&TFunc::WriteInlineVariant, &TFunc::WriteHookedVariant),
      m_CopyHookData(&TFunc::CopyNonObjectVariant, &TFunc::CopyHookedVariant),
      m_SkipFunction(&TFunc::SkipNonObjectVariant)
{
}

CVariantInfo::CVariantInfo(const CChoiceTypeInfo* choiceType,
                           const char* id, TOffset offset, TTypeInfo type)
    : CParent(id, offset, type), m_ChoiceType(choiceType),
      m_VariantType(eInlineVariant), m_DelayOffset(TOffset(eNoOffset)),
      m_GetConstFunction(&TFunc::GetConstInlineVariant),
      m_GetFunction(&TFunc::GetInlineVariant),
      m_ReadHookData(&TFunc::ReadInlineVariant, &TFunc::ReadHookedVariant),
      m_WriteHookData(&TFunc::WriteInlineVariant, &TFunc::WriteHookedVariant),
      m_CopyHookData(&TFunc::CopyNonObjectVariant, &TFunc::CopyHookedVariant),
      m_SkipFunction(&TFunc::SkipNonObjectVariant)
{
}

CVariantInfo::~CVariantInfo(void)
{
}

CVariantInfo* CVariantInfo::SetPointer(void)
{
    if ( !IsInline() )
        THROW1_TRACE(runtime_error, "SetPointer() is not first call");
    m_VariantType = eNonObjectPointerVariant;
    UpdateFunctions();
    return this;
}

CVariantInfo* CVariantInfo::SetObjectPointer(void)
{
    if ( !IsInline() )
        THROW1_TRACE(runtime_error, "SetObjectPointer() is not first call");
    m_VariantType = eObjectPointerVariant;
    UpdateFunctions();
    return this;
}

CVariantInfo* CVariantInfo::SetSubClass(void)
{
    if ( !IsInline() )
        THROW1_TRACE(runtime_error, "SetSubClass() is not first call");
    if ( CanBeDelayed() )
        THROW1_TRACE(runtime_error, "sub class cannot be delayed");
    m_VariantType = eSubClassVariant;
    UpdateFunctions();
    return this;
}

CVariantInfo* CVariantInfo::SetDelayBuffer(CDelayBuffer* buffer)
{
    if ( IsSubClass() )
        THROW1_TRACE(runtime_error, "sub class cannot be delayed");
    m_DelayOffset = size_t(buffer);
    UpdateFunctions();
    return this;
}

void CVariantInfo::UpdateFunctions(void)
{
    TVariantRead& readFunc = m_ReadHookData.GetDefaultFunction();
    TVariantWrite& writeFunc = m_WriteHookData.GetDefaultFunction();
    TVariantCopy& copyFunc = m_CopyHookData.GetDefaultFunction();

    // read/write/get
    if ( CanBeDelayed() ) {
        _ASSERT(!IsSubClass());
        m_GetConstFunction = &TFunc::GetConstDelayedVariant;
        m_GetFunction = &TFunc::GetDelayedVariant;
        readFunc = &TFunc::ReadDelayedVariant;
        writeFunc = &TFunc::WriteDelayedVariant;
    }
    else if ( IsInline() ) {
        m_GetConstFunction = &TFunc::GetConstInlineVariant;
        m_GetFunction = &TFunc::GetInlineVariant;
        readFunc = &TFunc::ReadInlineVariant;
        writeFunc = &TFunc::WriteInlineVariant;
    }
    else if ( IsObjectPointer() ) {
        m_GetConstFunction = &TFunc::GetConstPointerVariant;
        m_GetFunction = &TFunc::GetPointerVariant;
        readFunc = &TFunc::ReadObjectPointerVariant;
        writeFunc = &TFunc::WriteObjectPointerVariant;
    }
    else if ( IsNonObjectPointer() ) {
        m_GetConstFunction = &TFunc::GetConstPointerVariant;
        m_GetFunction = &TFunc::GetPointerVariant;
        readFunc = &TFunc::ReadPointerVariant;
        writeFunc = &TFunc::WritePointerVariant;
    }
    else { // subclass
        m_GetConstFunction = &TFunc::GetConstSubclassVariant;
        m_GetFunction = &TFunc::GetSubclassVariant;
        readFunc = &TFunc::ReadSubclassVariant;
        writeFunc = &TFunc::WriteSubclassVariant;
    }

    // copy/skip
    if ( IsObject() ) {
        copyFunc = &TFunc::CopyObjectPointerVariant;
        m_SkipFunction = &TFunc::SkipObjectPointerVariant;
    }
    else {
        copyFunc = &TFunc::CopyNonObjectVariant;
        m_SkipFunction = &TFunc::SkipNonObjectVariant;
    }
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

TConstObjectPtr
CVariantInfoFunctions::GetConstInlineVariant(const CVariantInfo* variantInfo,
                                             TConstObjectPtr choicePtr)
{
    _ASSERT(!variantInfo->CanBeDelayed());
    _ASSERT(variantInfo->IsInline());
    _ASSERT(variantInfo->GetChoiceType()->GetIndex(choicePtr) ==
            variantInfo->GetIndex());
    return variantInfo->GetItemPtr(choicePtr);
}

TConstObjectPtr
CVariantInfoFunctions::GetConstPointerVariant(const CVariantInfo* variantInfo,
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
CVariantInfoFunctions::GetConstDelayedVariant(const CVariantInfo* variantInfo,
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

TConstObjectPtr
CVariantInfoFunctions::GetConstSubclassVariant(const CVariantInfo* variantInfo,
                                               TConstObjectPtr choicePtr)
{
    _ASSERT(variantInfo->IsSubClass());
    _ASSERT(variantInfo->GetChoiceType()->GetIndex(choicePtr) ==
            variantInfo->GetIndex());
    const CChoiceTypeInfo* choiceType = variantInfo->GetChoiceType();
    const CChoicePointerTypeInfo* choicePtrType =
        CTypeConverter<CChoicePointerTypeInfo>::SafeCast(choiceType);
    TConstObjectPtr variantPtr =
        choicePtrType->GetPointerTypeInfo()->GetObjectPointer(choicePtr);
    _ASSERT(variantPtr);
    return variantPtr;
}

TObjectPtr
CVariantInfoFunctions::GetInlineVariant(const CVariantInfo* variantInfo,
                                        TObjectPtr choicePtr)
{
    _ASSERT(!variantInfo->CanBeDelayed());
    _ASSERT(variantInfo->IsInline());
    _ASSERT(variantInfo->GetChoiceType()->GetIndex(choicePtr) ==
            variantInfo->GetIndex());
    return variantInfo->GetItemPtr(choicePtr);
}

TObjectPtr
CVariantInfoFunctions::GetPointerVariant(const CVariantInfo* variantInfo,
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

TObjectPtr
CVariantInfoFunctions::GetDelayedVariant(const CVariantInfo* variantInfo,
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

TObjectPtr
CVariantInfoFunctions::GetSubclassVariant(const CVariantInfo* variantInfo,
                                          TObjectPtr choicePtr)
{
    _ASSERT(variantInfo->IsSubClass());
    _ASSERT(variantInfo->GetChoiceType()->GetIndex(choicePtr) ==
            variantInfo->GetIndex());
    const CChoiceTypeInfo* choiceType = variantInfo->GetChoiceType();
    const CChoicePointerTypeInfo* choicePtrType =
        CTypeConverter<CChoicePointerTypeInfo>::SafeCast(choiceType);
    TObjectPtr variantPtr =
        choicePtrType->GetPointerTypeInfo()->GetObjectPointer(choicePtr);
    _ASSERT(variantPtr);
    return variantPtr;
}

void CVariantInfoFunctions::ReadInlineVariant(CObjectIStream& in,
                                              const CVariantInfo* variantInfo,
                                              TObjectPtr choicePtr)
{
    _ASSERT(!variantInfo->CanBeDelayed());
    _ASSERT(variantInfo->IsInline());
    const CChoiceTypeInfo* choiceType = variantInfo->GetChoiceType();
    TMemberIndex index = variantInfo->GetIndex();
    choiceType->SetIndex(choicePtr, index);
    in.ReadObject(variantInfo->GetItemPtr(choicePtr),
                  variantInfo->GetTypeInfo());
}

void CVariantInfoFunctions::ReadPointerVariant(CObjectIStream& in,
                                               const CVariantInfo* variantInfo,
                                               TObjectPtr choicePtr)
{
    _ASSERT(!variantInfo->CanBeDelayed());
    _ASSERT(variantInfo->IsNonObjectPointer());
    const CChoiceTypeInfo* choiceType = variantInfo->GetChoiceType();
    TMemberIndex index = variantInfo->GetIndex();
    choiceType->SetIndex(choicePtr, index);
    TObjectPtr variantPtr = variantInfo->GetItemPtr(choicePtr);
    variantPtr = CTypeConverter<TObjectPtr>::Get(variantPtr);
    _ASSERT(variantPtr != 0 );
    in.ReadObject(variantPtr, variantInfo->GetTypeInfo());
}

void CVariantInfoFunctions::ReadObjectPointerVariant(CObjectIStream& in,
                                                     const CVariantInfo* variantInfo,
                                                     TObjectPtr choicePtr)
{
    _ASSERT(!variantInfo->CanBeDelayed());
    _ASSERT(variantInfo->IsObjectPointer());
    const CChoiceTypeInfo* choiceType = variantInfo->GetChoiceType();
    TMemberIndex index = variantInfo->GetIndex();
    choiceType->SetIndex(choicePtr, index);
    TObjectPtr variantPtr = variantInfo->GetItemPtr(choicePtr);
    variantPtr = CTypeConverter<TObjectPtr>::Get(variantPtr);
    _ASSERT(variantPtr != 0 );
    in.ReadExternalObject(variantPtr, variantInfo->GetTypeInfo());
}

void CVariantInfoFunctions::ReadSubclassVariant(CObjectIStream& in,
                                                const CVariantInfo* variantInfo,
                                                TObjectPtr choicePtr)
{
    _ASSERT(!variantInfo->CanBeDelayed());
    _ASSERT(variantInfo->IsSubClass());
    const CChoiceTypeInfo* choiceType = variantInfo->GetChoiceType();
    TMemberIndex index = variantInfo->GetIndex();
    choiceType->SetIndex(choicePtr, index);
    const CChoicePointerTypeInfo* choicePtrType =
        CTypeConverter<CChoicePointerTypeInfo>::SafeCast(choiceType);
    TObjectPtr variantPtr =
        choicePtrType->GetPointerTypeInfo()->GetObjectPointer(choicePtr);
    _ASSERT(variantPtr);
    in.ReadExternalObject(variantPtr, variantInfo->GetTypeInfo());
}

void CVariantInfoFunctions::ReadDelayedVariant(CObjectIStream& in,
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

void CVariantInfoFunctions::WriteInlineVariant(CObjectOStream& out,
                                               const CVariantInfo* variantInfo,
                                               TConstObjectPtr choicePtr)
{
    _ASSERT(!variantInfo->CanBeDelayed());
    _ASSERT(variantInfo->IsInline());
    _ASSERT(variantInfo->GetChoiceType()->GetIndex(choicePtr) ==
            variantInfo->GetIndex());
    out.WriteObject(variantInfo->GetItemPtr(choicePtr),
                    variantInfo->GetTypeInfo());
}

void CVariantInfoFunctions::WritePointerVariant(CObjectOStream& out,
                                                const CVariantInfo* variantInfo,
                                                TConstObjectPtr choicePtr)
{
    _ASSERT(!variantInfo->CanBeDelayed());
    _ASSERT(variantInfo->IsNonObjectPointer());
    _ASSERT(variantInfo->GetChoiceType()->GetIndex(choicePtr) ==
            variantInfo->GetIndex());
    TConstObjectPtr variantPtr = variantInfo->GetItemPtr(choicePtr);
    variantPtr = CTypeConverter<TConstObjectPtr>::Get(variantPtr);
    _ASSERT(variantPtr != 0 );
    out.WriteObject(variantPtr, variantInfo->GetTypeInfo());
}

void CVariantInfoFunctions::WriteObjectPointerVariant(CObjectOStream& out,
                                                      const CVariantInfo* variantInfo,
                                                      TConstObjectPtr choicePtr)
{
    _ASSERT(!variantInfo->CanBeDelayed());
    _ASSERT(variantInfo->IsObjectPointer());
    _ASSERT(variantInfo->GetChoiceType()->GetIndex(choicePtr) ==
            variantInfo->GetIndex());
    TConstObjectPtr variantPtr = variantInfo->GetItemPtr(choicePtr);
    variantPtr = CTypeConverter<TConstObjectPtr>::Get(variantPtr);
    _ASSERT(variantPtr != 0 );
    out.WriteExternalObject(variantPtr, variantInfo->GetTypeInfo());
}

void CVariantInfoFunctions::WriteSubclassVariant(CObjectOStream& out,
                                                 const CVariantInfo* variantInfo,
                                                 TConstObjectPtr choicePtr)
{
    _ASSERT(!variantInfo->CanBeDelayed());
    _ASSERT(variantInfo->IsSubClass());
    _ASSERT(variantInfo->GetChoiceType()->GetIndex(choicePtr) ==
            variantInfo->GetIndex());
    const CChoiceTypeInfo* choiceType = variantInfo->GetChoiceType();
    const CChoicePointerTypeInfo* choicePtrType =
        CTypeConverter<CChoicePointerTypeInfo>::SafeCast(choiceType);
    TConstObjectPtr variantPtr =
        choicePtrType->GetPointerTypeInfo()->GetObjectPointer(choicePtr);
    _ASSERT(variantPtr);
    out.WriteExternalObject(variantPtr, variantInfo->GetTypeInfo());
}

void CVariantInfoFunctions::WriteDelayedVariant(CObjectOStream& out,
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

void CVariantInfoFunctions::CopyNonObjectVariant(CObjectStreamCopier& copier,
                                                 const CVariantInfo* variantInfo)
{
    _ASSERT(!variantInfo->IsNotObject());
    copier.CopyObject(variantInfo->GetTypeInfo());
}

void CVariantInfoFunctions::CopyObjectPointerVariant(CObjectStreamCopier& copier,
                                                     const CVariantInfo* variantInfo)
{
    _ASSERT(variantInfo->IsObject());
    copier.CopyExternalObject(variantInfo->GetTypeInfo());
}

void CVariantInfoFunctions::SkipNonObjectVariant(CObjectIStream& in,
                                                 const CVariantInfo* variantInfo)
{
    _ASSERT(!variantInfo->IsNotObject());
    in.SkipObject(variantInfo->GetTypeInfo());
}

void CVariantInfoFunctions::SkipObjectPointerVariant(CObjectIStream& in,
                                            const CVariantInfo* variantInfo)
{
    _ASSERT(variantInfo->IsObject());
    in.SkipExternalObject(variantInfo->GetTypeInfo());
}

void CVariantInfoFunctions::ReadHookedVariant(CObjectIStream& stream,
                                              const CVariantInfo* variantInfo,
                                              TObjectPtr choicePtr)
{
    CReadChoiceVariantHook* hook =
        variantInfo->m_ReadHookData.GetHook(&stream);
    if ( hook ) {
        CObjectInfo type(choicePtr, variantInfo->GetChoiceType(),
                         CConstObjectInfo::eNonCObject);
        TMemberIndex index = variantInfo->GetIndex();
        hook->ReadChoiceVariant(stream,
                                CObjectInfo::CChoiceVariant(type,
                                                            index));
    }
    else
        variantInfo->DefaultReadVariant(stream, choicePtr);
}

void CVariantInfoFunctions::WriteHookedVariant(CObjectOStream& stream,
                                               const CVariantInfo* variantInfo,
                                               TConstObjectPtr choicePtr)
{
    CWriteChoiceVariantHook* hook =
        variantInfo->m_WriteHookData.GetHook(&stream);
    if ( hook ) {
        CConstObjectInfo type(choicePtr, variantInfo->GetChoiceType(),
                              CConstObjectInfo::eNonCObject);
        TMemberIndex index = variantInfo->GetIndex();
        hook->WriteChoiceVariant(stream,
                                 CConstObjectInfo::CChoiceVariant(type,
                                                                  index));
    }
    else
        variantInfo->DefaultWriteVariant(stream, choicePtr);
}

void CVariantInfoFunctions::CopyHookedVariant(CObjectStreamCopier& stream,
                                              const CVariantInfo* variantInfo)
{
    CCopyChoiceVariantHook* hook =
        variantInfo->m_CopyHookData.GetHook(&stream);
    if ( hook ) {
        CObjectTypeInfo type(variantInfo->GetChoiceType());
        TMemberIndex index = variantInfo->GetIndex();
        hook->CopyChoiceVariant(stream,
                                CObjectTypeInfo::CVariantIterator(type,
                                                                  index));
    }
    else
        variantInfo->DefaultCopyVariant(stream);
}

void CVariantInfo::SetReadHook(CObjectIStream* stream,
                               CReadChoiceVariantHook* hook)
{
    m_ReadHookData.SetHook(stream, hook);
}

void CVariantInfo::ResetReadHook(CObjectIStream* stream)
{
    m_ReadHookData.ResetHook(stream);
}

void CVariantInfo::SetWriteHook(CObjectOStream* stream,
                                CWriteChoiceVariantHook* hook)
{
    m_WriteHookData.SetHook(stream, hook);
}

void CVariantInfo::ResetWriteHook(CObjectOStream* stream)
{
    m_WriteHookData.ResetHook(stream);
}

void CVariantInfo::SetCopyHook(CObjectStreamCopier* stream,
                               CCopyChoiceVariantHook* hook)
{
    m_CopyHookData.SetHook(stream, hook);
}

void CVariantInfo::ResetCopyHook(CObjectStreamCopier* stream)
{
    m_CopyHookData.ResetHook(stream);
}

END_NCBI_SCOPE
