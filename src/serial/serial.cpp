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
*   Serialization classes.
*
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>
#include <serial/serial.hpp>
#include <serial/serialimpl.hpp>
#include <serial/serialbase.hpp>
#include <serial/impl/ptrinfo.hpp>
#include <serial/impl/classinfo.hpp>
#include <serial/impl/choice.hpp>
#include <serial/objostr.hpp>
#include <serial/objistr.hpp>
#include <serial/objectiter.hpp>
#include <serial/impl/memberlist.hpp>
#include <corelib/ncbi_safe_static.hpp>

BEGIN_NCBI_SCOPE

TTypeInfo CPointerTypeInfoGetTypeInfo(TTypeInfo type)
{
    return CPointerTypeInfo::GetTypeInfo(type);
}

void Write(CObjectOStream& out, TConstObjectPtr object, const CTypeRef& type)
{
    out.Write(object, type.Get());
}

void Read(CObjectIStream& in, TObjectPtr object, const CTypeRef& type)
{
    //type.Get()->SetDefault(object);
    in.Read(object, type.Get());
}

void Write(CObjectOStream& out, TConstObjectPtr object, TTypeInfo type)
{
    out.Write(object, type);
}

void Read(CObjectIStream& in, TObjectPtr object, TTypeInfo type)
{
    in.Read(object, type);
}

void Serial_FilterSkip(CObjectIStream& in, CObjectTypeInfo& ctype)
{
    in.Skip(ctype);
}

DEFINE_STATIC_FAST_MUTEX(s_ModuleNameMutex);

static const string& GetModuleName(const char* moduleName)
{
    CFastMutexGuard GUARD(s_ModuleNameMutex);
    static CSafeStaticPtr< set<string> > s_ModuleNames;
    const string& s = *s_ModuleNames.Get().insert(moduleName).first;
    CClassTypeInfoBase::RegisterModule(s);
    return s;
}

void SetModuleName(CTypeInfo* info, const char* moduleName)
{
    info->SetModuleName(GetModuleName(moduleName));
}

void SetModuleName(CEnumeratedTypeValues* info, const char* moduleName)
{
    info->SetModuleName(GetModuleName(moduleName));
}

void SetInternalName(CTypeInfo* info,
                     const char* owner_name, const char* member_name)
{
    string name = owner_name;
    if ( member_name && *member_name ) {
        name += '.';
        name += member_name;
    }
    info->SetInternalName(name);
}

void SetInternalName(CEnumeratedTypeValues* info,
                     const char* owner_name, const char* member_name)
{
    string name = owner_name;
    if ( member_name && *member_name ) {
        name += '.';
        name += member_name;
    }
    info->SetInternalName(name);
}

// add member functions
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       const CTypeRef& r)
{
    return info->AddMember(name, member, r);
}
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfo t)
{
    return AddMember(info, name, member, CTypeRef(t));
}
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter f)
{
    return AddMember(info, name, member, CTypeRef(f));
}

// two arguments:
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f1,
                       const CTypeRef& r)
{
    return AddMember(info, name, member, CTypeRef(f1, r));
}
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f1,
                       TTypeInfo t)
{
    return AddMember(info, name, member, f1, CTypeRef(t));
}
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f1,
                       TTypeInfoGetter f)
{
    return AddMember(info, name, member, f1, CTypeRef(f));
}

// three arguments:
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       const CTypeRef& r)
{
    return AddMember(info, name, member, f2, CTypeRef(f1, r));
}
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfo t)
{
    return AddMember(info, name, member, f2, f1, CTypeRef(t));
}
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfoGetter f)
{
    return AddMember(info, name, member, f2, f1, CTypeRef(f));
}

// four arguments:
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       const CTypeRef& r)
{
    return AddMember(info, name, member, f3, f2, CTypeRef(f1, r));
}
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfo t)
{
    return AddMember(info, name, member, f3, f2, f1, CTypeRef(t));
}
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfoGetter f)
{
    return AddMember(info, name, member, f3, f2, f1, CTypeRef(f));
}

// five arguments:
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f4,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       const CTypeRef& r)
{
    return AddMember(info, name, member, f4, f3, f2, CTypeRef(f1, r));
}
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f4,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfo t)
{
    return AddMember(info, name, member, f4, f3, f2, f1, CTypeRef(t));
}
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f4,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfoGetter f)
{
    return AddMember(info, name, member, f4, f3, f2, f1, CTypeRef(f));
}

// add variant functions
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       const CTypeRef& r)
{
    return info->AddVariant(name, member, r);
}
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfo t)
{
    return AddVariant(info, name, member, CTypeRef(t));
}
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter f)
{
    return AddVariant(info, name, member, CTypeRef(f));
}

// two arguments:
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f1,
                       const CTypeRef& r)
{
    return AddVariant(info, name, member, CTypeRef(f1, r));
}
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f1,
                       TTypeInfo t)
{
    return AddVariant(info, name, member, f1, CTypeRef(t));
}
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f1,
                       TTypeInfoGetter f)
{
    return AddVariant(info, name, member, f1, CTypeRef(f));
}

// three arguments:
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       const CTypeRef& r)
{
    return AddVariant(info, name, member, f2, CTypeRef(f1, r));
}
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfo t)
{
    return AddVariant(info, name, member, f2, f1, CTypeRef(t));
}
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfoGetter f)
{
    return AddVariant(info, name, member, f2, f1, CTypeRef(f));
}

// four arguments:
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       const CTypeRef& r)
{
    return AddVariant(info, name, member, f3, f2, CTypeRef(f1, r));
}
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfo t)
{
    return AddVariant(info, name, member, f3, f2, f1, CTypeRef(t));
}
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfoGetter f)
{
    return AddVariant(info, name, member, f3, f2, f1, CTypeRef(f));
}

// five arguments:
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f4,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       const CTypeRef& r)
{
    return AddVariant(info, name, member, f4, f3, f2, CTypeRef(f1, r));
}
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f4,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfo t)
{
    return AddVariant(info, name, member, f4, f3, f2, f1, CTypeRef(t));
}
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f4,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfoGetter f)
{
    return AddVariant(info, name, member, f4, f3, f2, f1, CTypeRef(f));
}



CChoiceTypeInfo*
CClassInfoHelperBase::CreateChoiceInfo(const char* name, size_t size,
                                       const void* nonCObject,
                                       TCreateFunction createFunc,
                                       const type_info& ti,
                                       TWhichFunction whichFunc,
                                       TSelectFunction selectFunc,
                                       TResetFunction resetFunc)
{
    return new CChoiceTypeInfo(size, name, nonCObject, createFunc,
                               ti, whichFunc, selectFunc, resetFunc);
}

CChoiceTypeInfo*
CClassInfoHelperBase::CreateChoiceInfo(const char* name, size_t size,
                                       const CObject* cObject,
                                       TCreateFunction createFunc,
                                       const type_info& ti,
                                       TWhichFunction whichFunc,
                                       TSelectFunction selectFunc,
                                       TResetFunction resetFunc)
{
    return new CChoiceTypeInfo(size, name, cObject, createFunc,
                               ti, whichFunc, selectFunc, resetFunc);
}

CClassTypeInfo*
CClassInfoHelperBase::CreateClassInfo(const char* name, size_t size,
                                      const void* nonCObject,
                                      TCreateFunction createFunc,
                                      const type_info& id,
                                      TGetTypeIdFunction idFunc)
{
    return new CClassTypeInfo(size, name, nonCObject, createFunc, id, idFunc);
}

CClassTypeInfo*
CClassInfoHelperBase::CreateClassInfo(const char* name, size_t size,
                                      const CObject* cObject,
                                      TCreateFunction createFunc,
                                      const type_info& id,
                                      TGetTypeIdFunction idFunc)
{
    return new CClassTypeInfo(size, name, cObject, createFunc, id, idFunc);
}

void SetPreWrite(CClassTypeInfo* info, TPreWriteFunction func)
{
    info->SetPreWriteFunction(func);
}

void SetPostWrite(CClassTypeInfo* info, TPostWriteFunction func)
{
    info->SetPostWriteFunction(func);
}

void SetPreRead(CClassTypeInfo* info, TPreReadFunction func)
{
    info->SetPreReadFunction(func);
}

void SetPostRead(CClassTypeInfo* info, TPostReadFunction func)
{
    info->SetPostReadFunction(func);
}

void SetPreWrite(CChoiceTypeInfo* info, TPreWriteFunction func)
{
    info->SetPreWriteFunction(func);
}

void SetPostWrite(CChoiceTypeInfo* info, TPostWriteFunction func)
{
    info->SetPostWriteFunction(func);
}

void SetPreRead(CChoiceTypeInfo* info, TPreReadFunction func)
{
    info->SetPreReadFunction(func);
}

void SetPostRead(CChoiceTypeInfo* info, TPostReadFunction func)
{
    info->SetPostReadFunction(func);
}

static void s_ResolveItems(CTypeInfo*& info, const char*& name,
                           ETypeFamily req_family)
{
    TTypeInfo info0 = info;
    const char* name0 = name;
    while ( const char* dot = strchr(name, '.') ) {
        CTempString item_name(name, dot-name);
        TTypeInfo new_info;
        switch ( info->GetTypeFamily() ) {
        case eTypeFamilyClass:
            new_info = dynamic_cast<CClassTypeInfo*>(info)
                ->GetMemberInfo(item_name)->GetTypeInfo();
            break;
        case eTypeFamilyChoice:
            new_info = dynamic_cast<CChoiceTypeInfo*>(info)
                ->GetVariantInfo(item_name)->GetTypeInfo();
            break;
        case eTypeFamilyContainer:
            if ( item_name != "E" ) {
                NCBI_THROW_FMT(CSerialException,eInvalidData,
                               info0->GetName()<<'.'<<
                               CTempString(name0, name-name0)<<
                               ": element name must be 'E'");
            }
            new_info = dynamic_cast<CContainerTypeInfo*>(info)
                ->GetElementType();
            break;
        default:
            new_info = info;
            break;
        }
        // skip all pointers (CRef<>)
        while ( new_info->GetTypeFamily() == eTypeFamilyPointer ) {
            new_info = dynamic_cast<const CPointerTypeInfo*>(new_info)
                ->GetPointedType();
        }
        info = const_cast<CTypeInfo*>(new_info);
        name = dot+1;
    }
    if ( info->GetTypeFamily() != req_family ) {
        NCBI_THROW_FMT(CSerialException,eInvalidData,
                       info0->GetName()<<'.'<<
                       CTempString(name0, name-name0)<<
                       ": not a "<<
                       (req_family == eTypeFamilyClass? "class": "choice"));
    }
}

void SetGlobalReadMemberHook(CTypeInfo* start_info,
                             const char* member_names,
                             CReadClassMemberHook* hook_ptr)
{
    CRef<CReadClassMemberHook> hook(hook_ptr);
    s_ResolveItems(start_info, member_names, eTypeFamilyClass);
    dynamic_cast<CClassTypeInfo*>(start_info)
        ->SetGlobalHook(member_names, hook);
}

void SetGlobalReadVariantHook(CTypeInfo* start_info,
                              const char* variant_names,
                              CReadChoiceVariantHook* hook_ptr)
{
    CRef<CReadChoiceVariantHook> hook(hook_ptr);
    s_ResolveItems(start_info, variant_names, eTypeFamilyChoice);
    dynamic_cast<CChoiceTypeInfo*>(start_info)
        ->SetGlobalHook(variant_names, hook);
}

TObjectPtr GetClassObjectPtr(const CObjectInfoMI& member)
{
    return member.GetClassObject().GetObjectPtr();
}

TObjectPtr GetChoiceObjectPtr(const CObjectInfoCV& variant)
{
    return variant.GetChoiceObject().GetObjectPtr();
}

// Functions preventing memory leaks due to undestroyed type info objects

void RegisterEnumTypeValuesObject(CEnumeratedTypeValues* /*object*/)
{
/*
    typedef AutoPtr<CEnumeratedTypeValues> TEnumTypePtr;
    typedef list<TEnumTypePtr>             TEnumTypeList;

    static CSafeStaticPtr<TEnumTypeList> s_EnumTypeList;

    TEnumTypePtr ap(object);
    s_EnumTypeList.Get().push_back(ap);
*/
}

void RegisterTypeInfoObject(CTypeInfo* /*object*/)
{
/*
    typedef AutoPtr<CTypeInfo> TTypeInfoPtr;
    typedef list<TTypeInfoPtr> TTypeInfoList;

    static CSafeStaticPtr<TTypeInfoList> s_TypeInfoList;

    TTypeInfoPtr ap(object);
    s_TypeInfoList.Get().push_back(ap);
*/
}

END_NCBI_SCOPE
