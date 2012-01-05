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
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbimtx.hpp>

#include <serial/impl/classinfo.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>
#include <serial/delaybuf.hpp>
#include <serial/impl/stdtypes.hpp>
#include <serial/serialbase.hpp>

BEGIN_NCBI_SCOPE

CClassTypeInfo::CClassTypeInfo(size_t size, const char* name,
                               const void* nonCObject, TTypeCreate createFunc,
                               const type_info& ti, TGetTypeIdFunction idFunc)
    : CParent(eTypeFamilyClass, size, name, nonCObject, createFunc, ti),
      m_GetTypeIdFunction(idFunc)
{
    InitClassTypeInfo();
}

CClassTypeInfo::CClassTypeInfo(size_t size, const char* name,
                               const CObject* cObject, TTypeCreate createFunc,
                               const type_info& ti, TGetTypeIdFunction idFunc)
    : CParent(eTypeFamilyClass, size, name, cObject, createFunc, ti),
      m_GetTypeIdFunction(idFunc)
{
    InitClassTypeInfo();
}

CClassTypeInfo::CClassTypeInfo(size_t size, const string& name,
                               const void* nonCObject, TTypeCreate createFunc,
                               const type_info& ti, TGetTypeIdFunction idFunc)
    : CParent(eTypeFamilyClass, size, name, nonCObject, createFunc, ti),
      m_GetTypeIdFunction(idFunc)
{
    InitClassTypeInfo();
}

CClassTypeInfo::CClassTypeInfo(size_t size, const string& name,
                               const CObject* cObject, TTypeCreate createFunc,
                               const type_info& ti, TGetTypeIdFunction idFunc)
    : CParent(eTypeFamilyClass, size, name, cObject, createFunc, ti),
      m_GetTypeIdFunction(idFunc)
{
    InitClassTypeInfo();
}

void CClassTypeInfo::InitClassTypeInfo(void)
{
    m_ClassType = eSequential;
    m_ParentClassInfo = 0;

    UpdateFunctions();
}

CClassTypeInfo* CClassTypeInfo::SetRandomOrder(bool random)
{
    _ASSERT(!Implicit());
    m_ClassType = random? eRandom: eSequential;
    UpdateFunctions();
    return this;
}

CClassTypeInfo* CClassTypeInfo::SetImplicit(void)
{
    m_ClassType = eImplicit;
    UpdateFunctions();
    return this;
}

bool CClassTypeInfo::IsImplicitNonEmpty(void) const
{
    _ASSERT(Implicit());
    return GetImplicitMember()->NonEmpty();
}

void CClassTypeInfo::AddSubClass(const CMemberId& id,
                                 const CTypeRef& type)
{
    TSubClasses* subclasses = m_SubClasses.get();
    if ( !subclasses )
        m_SubClasses.reset(subclasses = new TSubClasses);
    subclasses->push_back(make_pair(id, type));
}

void CClassTypeInfo::AddSubClassNull(const CMemberId& id)
{
    AddSubClass(id, CTypeRef(TTypeInfo(0)));
}

void CClassTypeInfo::AddSubClass(const char* id, TTypeInfoGetter getter)
{
    AddSubClass(CMemberId(id), getter);
}

void CClassTypeInfo::AddSubClassNull(const char* id)
{
    AddSubClassNull(CMemberId(id));
}

const CClassTypeInfo* CClassTypeInfo::GetParentClassInfo(void) const
{
    return m_ParentClassInfo;
}

void CClassTypeInfo::SetParentClass(TTypeInfo parentType)
{
    if ( parentType->GetTypeFamily() != eTypeFamilyClass )
        NCBI_THROW(CSerialException,eInvalidData,
                   string("invalid parent class type: ") +
                   parentType->GetName());
    const CClassTypeInfo* parentClass =
        CTypeConverter<CClassTypeInfo>::SafeCast(parentType);
    _ASSERT(parentClass != 0);
    _ASSERT(IsCObject() == parentClass->IsCObject());
    _ASSERT(!m_ParentClassInfo);
    m_ParentClassInfo = parentClass;
    _ASSERT(GetMembers().Empty());
    AddMember(NcbiEmptyString, 0, parentType)->SetParentClass();
}

TTypeInfo CClassTypeInfo::GetRealTypeInfo(TConstObjectPtr object) const
{
    if ( !m_SubClasses.get() ) {
        // do not have subclasses -> real type is the same as our type
        return this;
    }
    const type_info* ti = GetCPlusPlusTypeInfo(object);
    if ( ti == 0 || ti == &GetId() )
        return this;
    RegisterSubClasses();
    return GetClassInfoById(*ti);
}

void CClassTypeInfo::RegisterSubClasses(void) const
{
    const TSubClasses* subclasses = m_SubClasses.get();
    if ( subclasses ) {
        for ( TSubClasses::const_iterator i = subclasses->begin();
              i != subclasses->end();
              ++i ) {
            TTypeInfo subClass = i->second.Get();
            if ( subClass->GetTypeFamily() == eTypeFamilyClass ) {
                CTypeConverter<CClassTypeInfo>::SafeCast(subClass)->RegisterSubClasses();
            }
        }
    }
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

void CClassTypeInfo::AssignMemberDefault(TObjectPtr object,
                                         const CMemberInfo* info) const
{
    // check 'set' flag
    bool haveSetFlag = info->HaveSetFlag();
    if ( haveSetFlag && info->GetSetFlagNo(object) )
        return; // member not set
    
    TObjectPtr member = GetMember(info, object);
    // assign member default
    TTypeInfo memberType = info->GetTypeInfo();
    TConstObjectPtr def = info->GetDefault();
    if ( def == 0 ) {
        if ( !memberType->IsDefault(member) )
            memberType->SetDefault(member);
    }
    else {
        memberType->Assign(member, def);
    }
    // update 'set' flag
    if ( haveSetFlag )
        info->UpdateSetFlagNo(object);
}

void CClassTypeInfo::AssignMemberDefault(TObjectPtr object,
                                         TMemberIndex index) const
{
    AssignMemberDefault(object, GetMemberInfo(index));
}


const CMemberInfo* CClassTypeInfo::GetImplicitMember(void) const
{
    _ASSERT(GetMembers().FirstIndex() == GetMembers().LastIndex());
    return GetMemberInfo(GetMembers().FirstIndex());
}

void CClassTypeInfo::UpdateFunctions(void)
{
    switch ( m_ClassType ) {
    case eSequential:
        SetReadFunction(&ReadClassSequential);
        SetWriteFunction(&WriteClassSequential);
        SetCopyFunction(&CopyClassSequential);
        SetSkipFunction(&SkipClassSequential);
        break;
    case eRandom:
        SetReadFunction(&ReadClassRandom);
        SetWriteFunction(&WriteClassRandom);
        SetCopyFunction(&CopyClassRandom);
        SetSkipFunction(&SkipClassRandom);
        break;
    case eImplicit:
        SetReadFunction(&ReadImplicitMember);
        SetWriteFunction(&WriteImplicitMember);
        SetCopyFunction(&CopyImplicitMember);
        SetSkipFunction(&SkipImplicitMember);
        break;
    }
}

void CClassTypeInfo::ReadClassSequential(CObjectIStream& in,
                                         TTypeInfo objectType,
                                         TObjectPtr objectPtr)
{
    const CClassTypeInfo* classType =
        CTypeConverter<CClassTypeInfo>::SafeCast(objectType);

    in.ReadClassSequential(classType, objectPtr);
}

void CClassTypeInfo::ReadClassRandom(CObjectIStream& in,
                                     TTypeInfo objectType,
                                     TObjectPtr objectPtr)
{
    const CClassTypeInfo* classType =
        CTypeConverter<CClassTypeInfo>::SafeCast(objectType);

    in.ReadClassRandom(classType, objectPtr);
}

void CClassTypeInfo::ReadImplicitMember(CObjectIStream& in,
                                        TTypeInfo objectType,
                                        TObjectPtr objectPtr)
{
    const CClassTypeInfo* classType =
        CTypeConverter<CClassTypeInfo>::SafeCast(objectType);

    const CMemberInfo* memberInfo = classType->GetImplicitMember();
    if( memberInfo->HaveSetFlag()) {
        memberInfo->UpdateSetFlagYes(objectPtr);
    }
    in.ReadNamedType(classType,
                     memberInfo->GetTypeInfo(),
                     memberInfo->GetItemPtr(objectPtr));
}

void CClassTypeInfo::WriteClassRandom(CObjectOStream& out,
                                      TTypeInfo objectType,
                                      TConstObjectPtr objectPtr)
{
    const CClassTypeInfo* classType =
        CTypeConverter<CClassTypeInfo>::SafeCast(objectType);

    out.WriteClassRandom(classType, objectPtr);
}

void CClassTypeInfo::WriteClassSequential(CObjectOStream& out,
                                          TTypeInfo objectType,
                                          TConstObjectPtr objectPtr)
{
    const CClassTypeInfo* classType =
        CTypeConverter<CClassTypeInfo>::SafeCast(objectType);

    out.WriteClassSequential(classType, objectPtr);
}

void CClassTypeInfo::WriteImplicitMember(CObjectOStream& out,
                                         TTypeInfo objectType,
                                         TConstObjectPtr objectPtr)
{
    const CClassTypeInfo* classType =
        CTypeConverter<CClassTypeInfo>::SafeCast(objectType);

    const CMemberInfo* memberInfo = classType->GetImplicitMember();
    if (memberInfo->HaveSetFlag() && memberInfo->GetSetFlagNo(objectPtr)) {
        if (memberInfo->Optional()) {
            return;
        }
        if (memberInfo->NonEmpty() ||
            memberInfo->GetTypeInfo()->GetTypeFamily() != eTypeFamilyContainer) {
            ESerialVerifyData verify = out.GetVerifyData();
            if (verify == eSerialVerifyData_Yes) {
                out.ThrowError(CObjectOStream::fUnassigned,
                               "implicit "+classType->GetName());
            } else if (verify == eSerialVerifyData_No) {
                return;
            }
        } 
    }
    out.WriteNamedType(classType,
                       memberInfo->GetTypeInfo(),
                       memberInfo->GetItemPtr(objectPtr));
}

void CClassTypeInfo::CopyClassRandom(CObjectStreamCopier& copier,
                                     TTypeInfo objectType)
{
    const CClassTypeInfo* classType =
        CTypeConverter<CClassTypeInfo>::SafeCast(objectType);

    copier.CopyClassRandom(classType);
}

void CClassTypeInfo::CopyClassSequential(CObjectStreamCopier& copier,
                                         TTypeInfo objectType)
{
    const CClassTypeInfo* classType =
        CTypeConverter<CClassTypeInfo>::SafeCast(objectType);

    copier.CopyClassSequential(classType);
}

void CClassTypeInfo::CopyImplicitMember(CObjectStreamCopier& copier,
                                        TTypeInfo objectType)
{
    const CClassTypeInfo* classType =
        CTypeConverter<CClassTypeInfo>::SafeCast(objectType);

    const CMemberInfo* memberInfo = classType->GetImplicitMember();
    copier.CopyNamedType(classType, memberInfo->GetTypeInfo());
}

void CClassTypeInfo::SkipClassRandom(CObjectIStream& in,
                                     TTypeInfo objectType)
{
    const CClassTypeInfo* classType =
        CTypeConverter<CClassTypeInfo>::SafeCast(objectType);

    in.SkipClassRandom(classType);
}

void CClassTypeInfo::SkipClassSequential(CObjectIStream& in,
                                         TTypeInfo objectType)
{
    const CClassTypeInfo* classType =
        CTypeConverter<CClassTypeInfo>::SafeCast(objectType);

    in.SkipClassSequential(classType);
}

void CClassTypeInfo::SkipImplicitMember(CObjectIStream& in,
                                        TTypeInfo objectType)
{
    const CClassTypeInfo* classType =
        CTypeConverter<CClassTypeInfo>::SafeCast(objectType);

    const CMemberInfo* memberInfo = classType->GetImplicitMember();
    in.SkipNamedType(classType, memberInfo->GetTypeInfo());
}

bool CClassTypeInfo::IsDefault(TConstObjectPtr /*object*/) const
{
    return false;
}

void CClassTypeInfo::SetDefault(TObjectPtr dst) const
{
    for ( TMemberIndex i = GetMembers().FirstIndex(),
              last = GetMembers().LastIndex();
          i <= last; ++i ) {
        AssignMemberDefault(dst, i);
    }
}

bool CClassTypeInfo::Equals(TConstObjectPtr object1, TConstObjectPtr object2,
                            ESerialRecursionMode how) const
{
    for ( TMemberIndex i = GetMembers().FirstIndex(),
              last = GetMembers().LastIndex();
          i <= last; ++i ) {
        const CMemberInfo* info = GetMemberInfo(i);
        if ( !info->GetTypeInfo()->Equals(GetMember(info, object1),
                                          GetMember(info, object2), how) )
            return false;
        if ( info->HaveSetFlag() ) {
            if ( !info->CompareSetFlags(object1,object2) )
                return false;
        }
    }

    // User defined comparison
    if ( IsCObject() ) {
        const CSerialUserOp* op1 =
            dynamic_cast<const CSerialUserOp*>
            (static_cast<const CObject*>(object1));
        const CSerialUserOp* op2 =
            dynamic_cast<const CSerialUserOp*>
            (static_cast<const CObject*>(object2));
        if ( op1  &&  op2 ) {
            return op1->UserOp_Equals(*op2);
        }
    }
    return true;
}

void CClassTypeInfo::Assign(TObjectPtr dst, TConstObjectPtr src,
                            ESerialRecursionMode how) const
{
    for ( TMemberIndex i = GetMembers().FirstIndex(),
              last = GetMembers().LastIndex();
          i <= last; ++i ) {
        const CMemberInfo* info = GetMemberInfo(i);
        info->GetTypeInfo()->Assign(GetMember(info, dst),
                                    GetMember(info, src), how);
        if ( info->HaveSetFlag() ) {
            info->UpdateSetFlag(dst,info->GetSetFlag(src));
        }
    }

    // User defined assignment
    if ( IsCObject() ) {
        const CSerialUserOp* opsrc =
            dynamic_cast<const CSerialUserOp*>
            (static_cast<const CObject*>(src));
        CSerialUserOp* opdst =
            dynamic_cast<CSerialUserOp*>
            (static_cast<CObject*>(dst));
        if ( opdst  &&  opsrc ) {
            opdst->UserOp_Assign(*opsrc);
        }
    }
}

bool CClassTypeInfo::IsType(TTypeInfo typeInfo) const
{
    return typeInfo == this || typeInfo->IsParentClassOf(this);
}

bool CClassTypeInfo::IsParentClassOf(const CClassTypeInfo* typeInfo) const
{
    do {
        typeInfo = typeInfo->m_ParentClassInfo;
        if ( typeInfo == this )
            return true;
    } while ( typeInfo );
    return false;
}

CTypeInfo::EMayContainType
CClassTypeInfo::CalcMayContainType(TTypeInfo typeInfo) const
{
    const CClassTypeInfoBase* parentClass = m_ParentClassInfo;
    EMayContainType ret = eMayContainType_no;
    if ( parentClass ) {
        ret = parentClass->GetMayContainType(typeInfo);
        if ( ret == eMayContainType_yes ) {
            return ret;
        }
    }
    EMayContainType ret2 = CParent::CalcMayContainType(typeInfo);
    if ( ret2 != eMayContainType_no ) {
        ret = ret2;
    }
    return ret;
}

void CClassTypeInfo::SetGlobalHook(const CTempString& members,
                                   CReadClassMemberHook* hook_ptr)
{
    CRef<CReadClassMemberHook> hook(hook_ptr);
    if ( members == "*" ) {
        for ( CIterator i(this); i.Valid(); ++i ) {
            const_cast<CMemberInfo*>(GetMemberInfo(i))->
                SetGlobalReadHook(hook);
        }
    }
    else {
        vector<CTempString> tokens;
        NStr::Tokenize(members, ",", tokens);
        ITERATE ( vector<CTempString>, it, tokens ) {
            const_cast<CMemberInfo*>(GetMemberInfo(*it))->
                SetGlobalReadHook(hook);
        }
    }
}


END_NCBI_SCOPE
