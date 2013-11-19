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

#include <ncbi_pch.hpp>
#include <serial/impl/choice.hpp>
#include <serial/objostr.hpp>
#include <serial/objistr.hpp>
#include <serial/objcopy.hpp>
#include <serial/delaybuf.hpp>
#include <serial/serialbase.hpp>
#include <serial/objhook.hpp>

BEGIN_NCBI_SCOPE

class CChoiceTypeInfoFunctions
{
public:
    static void ReadChoiceSimple(CObjectIStream& in,
                                 TTypeInfo objectType,
                                 TObjectPtr objectPtr);
    static void ReadChoiceDefault(CObjectIStream& in,
                                  TTypeInfo objectType,
                                  TObjectPtr objectPtr);
    static void WriteChoiceDefault(CObjectOStream& out,
                                   TTypeInfo objectType,
                                   TConstObjectPtr objectPtr);
    static void SkipChoiceSimple(CObjectIStream& in,
                                 TTypeInfo objectType);
    static void SkipChoiceDefault(CObjectIStream& in,
                                  TTypeInfo objectType);
    static void CopyChoiceDefault(CObjectStreamCopier& copier,
                                  TTypeInfo objectType);
    static void AssignDefault(TTypeInfo typeInfo,
                              TObjectPtr dst, TConstObjectPtr src,
                              ESerialRecursionMode how);
    static void AssignSimple(TTypeInfo typeInfo,
                             TObjectPtr dst, TConstObjectPtr src,
                             ESerialRecursionMode how);
};

typedef CChoiceTypeInfoFunctions TFunc;

CChoiceTypeInfo::CChoiceTypeInfo(size_t size, const char* name, 
                                 const void* nonCObject,
                                 TTypeCreate createFunc,
                                 const type_info& ti,
                                 TWhichFunction whichFunc,
                                 TSelectFunction selectFunc,
                                 TResetFunction resetFunc)
    : CParent(eTypeFamilyChoice, size, name, nonCObject, createFunc, ti),
      m_WhichFunction(whichFunc),
      m_ResetFunction(resetFunc), m_SelectFunction(selectFunc)
{
    InitChoiceTypeInfoFunctions();
}

CChoiceTypeInfo::CChoiceTypeInfo(size_t size, const char* name,
                                 const CObject* cObject,
                                 TTypeCreate createFunc,
                                 const type_info& ti,
                                 TWhichFunction whichFunc,
                                 TSelectFunction selectFunc,
                                 TResetFunction resetFunc)
    : CParent(eTypeFamilyChoice, size, name, cObject, createFunc, ti),
      m_WhichFunction(whichFunc),
      m_ResetFunction(resetFunc), m_SelectFunction(selectFunc)
{
    InitChoiceTypeInfoFunctions();
}

CChoiceTypeInfo::CChoiceTypeInfo(size_t size, const string& name, 
                                 const void* nonCObject,
                                 TTypeCreate createFunc,
                                 const type_info& ti,
                                 TWhichFunction whichFunc,
                                 TSelectFunction selectFunc,
                                 TResetFunction resetFunc)
    : CParent(eTypeFamilyChoice, size, name, nonCObject, createFunc, ti),
      m_WhichFunction(whichFunc),
      m_ResetFunction(resetFunc), m_SelectFunction(selectFunc)
{
    InitChoiceTypeInfoFunctions();
}

CChoiceTypeInfo::CChoiceTypeInfo(size_t size, const string& name,
                                 const CObject* cObject,
                                 TTypeCreate createFunc,
                                 const type_info& ti,
                                 TWhichFunction whichFunc,
                                 TSelectFunction selectFunc,
                                 TResetFunction resetFunc)
    : CParent(eTypeFamilyChoice, size, name, cObject, createFunc, ti),
      m_WhichFunction(whichFunc),
      m_ResetFunction(resetFunc), m_SelectFunction(selectFunc)
{
    InitChoiceTypeInfoFunctions();
}

void CChoiceTypeInfo::InitChoiceTypeInfoFunctions(void)
{
    SetReadFunction(&TFunc::ReadChoiceDefault);
    SetWriteFunction(&TFunc::WriteChoiceDefault);
    SetCopyFunction(&TFunc::CopyChoiceDefault);
    SetSkipFunction(&TFunc::SkipChoiceDefault);
    m_SelectDelayFunction = 0;
}

CVariantInfo* CChoiceTypeInfo::AddVariant(const char* memberId,
                                          const void* memberPtr,
                                          const CTypeRef& memberType)
{
    if ( GetVariants().Size() == 1 &&
         !GetVariantInfo(kFirstMemberIndex)->GetId().IsAttlist() ) {
        // simple
        SetReadFunction(&TFunc::ReadChoiceSimple);
        SetSkipFunction(&TFunc::SkipChoiceSimple);
    }
    CVariantInfo* variantInfo = new CVariantInfo(this, memberId,
                                                 TPointerOffsetType(memberPtr),
                                                 memberType);
    GetItems().AddItem(variantInfo);
    return variantInfo;
}

CVariantInfo* CChoiceTypeInfo::AddVariant(const CMemberId& memberId,
                                          const void* memberPtr,
                                          const CTypeRef& memberType)
{
    if ( GetVariants().Size() == 1 &&
         !GetVariantInfo(kFirstMemberIndex)->GetId().IsAttlist() ) {
        // simple
        SetReadFunction(&TFunc::ReadChoiceSimple);
        SetSkipFunction(&TFunc::SkipChoiceSimple);
    }
    CVariantInfo* variantInfo = new CVariantInfo(this, memberId,
                                                 TPointerOffsetType(memberPtr),
                                                 memberType);
    GetItems().AddItem(variantInfo);
    return variantInfo;
}

bool CChoiceTypeInfo::IsDefault(TConstObjectPtr object) const
{
    return GetIndex(object) == kEmptyChoice;
}


static inline
TObjectPtr GetMember(const CMemberInfo* memberInfo, TObjectPtr object)
{
    if ( memberInfo->CanBeDelayed() )
        memberInfo->GetDelayBuffer(object).Update();
    return memberInfo->GetItemPtr(object);
}

static inline
TConstObjectPtr GetMember(const CMemberInfo* memberInfo,
                          TConstObjectPtr object)
{
    if ( memberInfo->CanBeDelayed() )
        const_cast<CDelayBuffer&>(memberInfo->GetDelayBuffer(object)).Update();
    return memberInfo->GetItemPtr(object);
}

bool CChoiceTypeInfo::Equals(TConstObjectPtr object1, TConstObjectPtr object2,
                             ESerialRecursionMode how) const
{
    // User defined comparison
    if ( IsCObject() ) {
        if ( const CSerialUserOp* op1 =
             dynamic_cast<const CSerialUserOp*>
             (static_cast<const CObject*>(object1)) ) {
            if ( const CSerialUserOp* op2 =
                 dynamic_cast<const CSerialUserOp*>
                 (static_cast<const CObject*>(object2)) ) {
                if ( !op1->UserOp_Equals(*op2) )
                    return false;
            }
        }
    }

    TMemberIndex index;

    index = GetVariants().FirstIndex();
    const CVariantInfo* variantInfo = GetVariantInfo(index);
    if (variantInfo->GetId().IsAttlist()) {
        const CMemberInfo* info =
            dynamic_cast<const CMemberInfo*>(GetVariants().GetItemInfo(index));
        if ( !info->GetTypeInfo()->Equals(GetMember(info, object1),
                                          GetMember(info, object2), how) ) {
            return false;
        }
    }

    // Default comparison
    index = GetIndex(object1);
    if ( index != GetIndex(object2) )
        return false;
    if ( index == kEmptyChoice )
        return true;
    return
        GetVariantInfo(index)->GetTypeInfo()->Equals(GetData(object1, index),
                                                     GetData(object2, index), how);
}

void CChoiceTypeInfo::SetDefault(TObjectPtr dst) const
{
    ResetIndex(dst);
}

inline
void CChoiceTypeInfo::CallUserOp_Assign(TObjectPtr dst,
                                        TConstObjectPtr src) const
{
    // User defined assignment
    if ( IsCObject() ) {
        if ( const CSerialUserOp* opsrc =
             dynamic_cast<const CSerialUserOp*>
             (static_cast<const CObject*>(src)) ) {
            if ( CSerialUserOp* opdst =
                 dynamic_cast<CSerialUserOp*>
                 (static_cast<CObject*>(dst)) ) {
                opdst->UserOp_Assign(*opsrc);
            }
        }
    }
}

void CChoiceTypeInfo::Assign(TObjectPtr dst, TConstObjectPtr src,
                             ESerialRecursionMode how) const
{
    TMemberIndex index;

    index = GetVariants().FirstIndex();
    const CVariantInfo* variantInfo = GetVariantInfo(index);
    if (variantInfo->GetId().IsAttlist()) {
        const CMemberInfo* info =
            dynamic_cast<const CMemberInfo*>(GetVariants().GetItemInfo(index));
        info->GetTypeInfo()->Assign(GetMember(info, dst),
                                    GetMember(info, src),how);
    }

    index = GetIndex(src);
    if ( index == kEmptyChoice )
        ResetIndex(dst);
    else {
        _ASSERT(index >= GetVariants().FirstIndex() && 
                index <= GetVariants().LastIndex());
        SetIndex(dst, index);
        GetVariantInfo(index)->GetTypeInfo()->Assign(GetData(dst, index),
                                                     GetData(src, index), how);
    }

    // User defined assignment
    CallUserOp_Assign(dst, src);
}

void CChoiceTypeInfoFunctions::AssignDefault(TTypeInfo typeInfo,
                                             TObjectPtr dst,
                                             TConstObjectPtr src,
                                             ESerialRecursionMode how)
{
    const CChoiceTypeInfo* choiceType =
        static_cast<const CChoiceTypeInfo*>(typeInfo);
    TMemberIndex index;

    index = choiceType->GetVariants().FirstIndex();
    const CVariantInfo* variantInfo = choiceType->GetVariantInfo(index);
    if (variantInfo->GetId().IsAttlist()) {
        const CMemberInfo* info =
            static_cast<const CMemberInfo*>(choiceType->GetVariants().GetItemInfo(index));
        info->GetTypeInfo()->Assign(GetMember(info, dst),
                                    GetMember(info, src),how);
    }

    index = choiceType->GetIndex(src);
    if ( index == kEmptyChoice )
        choiceType->ResetIndex(dst);
    else {
        _ASSERT(index >= choiceType->GetVariants().FirstIndex() && 
                index <= choiceType->GetVariants().LastIndex());
        choiceType->SetIndex(dst, index);
        choiceType->GetVariantInfo(index)->GetTypeInfo()->Assign(choiceType->GetData(dst, index),
                                                                 choiceType->GetData(src, index), how);
    }

    choiceType->CallUserOp_Assign(dst, src);
}


void CChoiceTypeInfoFunctions::AssignSimple(TTypeInfo typeInfo,
                                            TObjectPtr dst,
                                            TConstObjectPtr src,
                                            ESerialRecursionMode how)
{
    const CChoiceTypeInfo* choiceType =
        static_cast<const CChoiceTypeInfo*>(typeInfo);
    TMemberIndex index = choiceType->GetIndex(src);
    if ( index == kEmptyChoice ) {
        choiceType->ResetIndex(dst);
        return;
    }
    const CVariantInfo* variantInfo = choiceType->GetVariantInfo(index);
    choiceType->SetIndex(dst, index);
    variantInfo->GetTypeInfo()->Assign(variantInfo->GetVariantPtr(dst),
                                       variantInfo->GetVariantPtr(src),
                                       how);
}


void CChoiceTypeInfo::SetSelectDelayFunction(TSelectDelayFunction func)
{
    _ASSERT(m_SelectDelayFunction == 0);
    _ASSERT(func != 0);
    m_SelectDelayFunction = func;
}

void CChoiceTypeInfo::SetDelayIndex(TObjectPtr objectPtr,
                                    TMemberIndex index) const
{
    m_SelectDelayFunction(this, objectPtr, index);
}

void CChoiceTypeInfoFunctions::ReadChoiceDefault(CObjectIStream& in,
                                                 TTypeInfo objectType,
                                                 TObjectPtr objectPtr)
{
    const CChoiceTypeInfo* choiceType =
        CTypeConverter<CChoiceTypeInfo>::SafeCast(objectType);
    BEGIN_OBJECT_FRAME_OF3(in, eFrameChoice, choiceType, objectPtr);
    in.BeginChoice(choiceType);
    BEGIN_OBJECT_FRAME_OF(in, eFrameChoiceVariant);
    TMemberIndex index = in.BeginChoiceVariant(choiceType);
    if ( index == kInvalidMember ) {
        if (in.CanSkipUnknownVariants()) {
            in.SkipAnyContentVariant();
        } else {
            in.ThrowError(in.fFormatError, "choice variant id expected");
        }
    } else {
        for (;;) {
        const CVariantInfo* variantInfo = choiceType->GetVariantInfo(index);
        if (variantInfo->GetId().IsAttlist()) {
            const CMemberInfo* memberInfo =
                static_cast<const CMemberInfo*>(
                    choiceType->GetVariants().GetItemInfo(index));
            memberInfo->ReadMember(in,objectPtr);
            in.EndChoiceVariant();
            index = in.BeginChoiceVariant(choiceType);
            if ( index == kInvalidMember ) {
                if (in.CanSkipUnknownVariants()) {
                    in.SkipAnyContentVariant();
                    break;
                } else {
                    in.ThrowError(in.fFormatError, "choice variant id expected");
                }
            }
            variantInfo = choiceType->GetVariantInfo(index);
        }
        in.SetTopMemberId(variantInfo->GetId());

        variantInfo->ReadVariant(in, objectPtr);
        in.EndChoiceVariant();
        break;
        }
    }
    END_OBJECT_FRAME_OF(in);
    in.EndChoice();
    END_OBJECT_FRAME_OF(in);
}

void CChoiceTypeInfoFunctions::ReadChoiceSimple(CObjectIStream& in,
                                                TTypeInfo objectType,
                                                TObjectPtr objectPtr)
{
    const CChoiceTypeInfo* choiceType =
        CTypeConverter<CChoiceTypeInfo>::SafeCast(objectType);
    in.ReadChoiceSimple(choiceType, objectPtr);
}

void CChoiceTypeInfoFunctions::WriteChoiceDefault(CObjectOStream& out,
                                                  TTypeInfo objectType,
                                                  TConstObjectPtr objectPtr)
{
    const CChoiceTypeInfo* choiceType =
        CTypeConverter<CChoiceTypeInfo>::SafeCast(objectType);

    BEGIN_OBJECT_FRAME_OF3(out, eFrameChoice, choiceType, objectPtr);
    out.BeginChoice(choiceType);
    TMemberIndex index = choiceType->GetVariants().FirstIndex();
    const CVariantInfo* variantInfo = choiceType->GetVariantInfo(index);
    if (variantInfo->GetId().IsAttlist()) {
        const CMemberInfo* memberInfo =
            dynamic_cast<const CMemberInfo*>(
                choiceType->GetVariants().GetItemInfo(index));
        memberInfo->WriteMember(out,objectPtr);
    }

    index = choiceType->GetIndex(objectPtr);
    if ( index == kInvalidMember ) {
        if (CItemsInfo::FindNextMandatory(objectType) != NULL) {
            out.ThrowError(out.fInvalidData, "cannot write empty choice");
        }
    } else {
        variantInfo = choiceType->GetVariantInfo(index);
        BEGIN_OBJECT_FRAME_OF2(out, eFrameChoiceVariant, variantInfo->GetId());
        out.BeginChoiceVariant(choiceType, variantInfo->GetId());

        variantInfo->WriteVariant(out, objectPtr);

        out.EndChoiceVariant();
        END_OBJECT_FRAME_OF(out);
    }

    out.EndChoice();
    END_OBJECT_FRAME_OF(out);
}

void CChoiceTypeInfoFunctions::CopyChoiceDefault(CObjectStreamCopier& copier,
                                                 TTypeInfo objectType)
{
    copier.CopyChoice(CTypeConverter<CChoiceTypeInfo>::SafeCast(objectType));
}

void CChoiceTypeInfoFunctions::SkipChoiceDefault(CObjectIStream& in,
                                                 TTypeInfo objectType)
{
    const CChoiceTypeInfo* choiceType =
        CTypeConverter<CChoiceTypeInfo>::SafeCast(objectType);
    BEGIN_OBJECT_FRAME_OF2(in, eFrameChoice, choiceType);
    in.BeginChoice(choiceType);
    BEGIN_OBJECT_FRAME_OF(in, eFrameChoiceVariant);
    TMemberIndex index = in.BeginChoiceVariant(choiceType);
    if ( index == kInvalidMember )
        in.ThrowError(in.fFormatError,"choice variant id expected");
    const CVariantInfo* variantInfo = choiceType->GetVariantInfo(index);
    if (variantInfo->GetId().IsAttlist()) {
        const CMemberInfo* memberInfo =
            dynamic_cast<const CMemberInfo*>(
                choiceType->GetVariants().GetItemInfo(index));
        memberInfo->SkipMember(in);
        in.EndChoiceVariant();
        index = in.BeginChoiceVariant(choiceType);
        if ( index == kInvalidMember )
            in.ThrowError(in.fFormatError,"choice variant id expected");
        variantInfo = choiceType->GetVariantInfo(index);
    }

    in.SetTopMemberId(variantInfo->GetId());

    variantInfo->SkipVariant(in);

    in.EndChoiceVariant();
    END_OBJECT_FRAME_OF(in);
    in.EndChoice();
    END_OBJECT_FRAME_OF(in);
}


void CChoiceTypeInfoFunctions::SkipChoiceSimple(CObjectIStream& in,
                                                 TTypeInfo objectType)
{
    const CChoiceTypeInfo* choiceType =
        CTypeConverter<CChoiceTypeInfo>::SafeCast(objectType);
    in.SkipChoiceSimple(choiceType);
}


void CChoiceTypeInfo::SetGlobalHook(const CTempString& variants,
                                    CReadChoiceVariantHook* hook_ptr)
{
    CRef<CReadChoiceVariantHook> hook(hook_ptr);
    if ( variants == "*" ) {
        for ( CIterator i(this); i.Valid(); ++i ) {
            const_cast<CVariantInfo*>(GetVariantInfo(i))->
                SetGlobalReadHook(hook);
        }
    }
    else {
        vector<CTempString> tokens;
        NStr::Tokenize(variants, ",", tokens);
        ITERATE ( vector<CTempString>, it, tokens ) {
            const_cast<CVariantInfo*>(GetVariantInfo(*it))->
                SetGlobalReadHook(hook);
        }
    }
}


END_NCBI_SCOPE
