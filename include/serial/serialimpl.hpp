#ifndef SERIALIMPL__HPP
#define SERIALIMPL__HPP

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
*   File to be included in modules implementing GetTypeInfo methods.
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <serial/typeinfo.hpp>
#include <serial/member.hpp>
#include <serial/stdtypes.hpp>
#include <serial/stltypes.hpp>
#include <serial/ptrinfo.hpp>
#include <serial/enumerated.hpp>
#include <serial/classinfob.hpp>
#include <serial/classinfo.hpp>
#include <serial/choice.hpp>
#include <serial/autoptrinfo.hpp>
#include <serial/serialbase.hpp>
#include <typeinfo>

struct valnode;

BEGIN_NCBI_SCOPE

// forward declaration
class CMemberInfo;
class CClassTypeInfoBase;
class CClassTypeInfo;
class CChoiceTypeInfo;
class CDelayBufferData;

// these methods are external to avoid inclusion of big headers
class CClassInfoHelperBase
{
protected:
    typedef const type_info* (*TGetTypeIdFunction)(TConstObjectPtr object);
    typedef TObjectPtr (*TCreateFunction)(TTypeInfo info);
    typedef int (*TWhichFunction)(TConstObjectPtr object);
    typedef void (*TResetFunction)(TObjectPtr object);
    typedef void (*TSelectFunction)(TObjectPtr object, int index);
    typedef void (*TSelectDelayFunction)(TObjectPtr object, int index);

    static CChoiceTypeInfo* CreateChoiceInfo(const char* name, size_t size,
                                             const type_info& ti,
                                             TCreateFunction createFunc,
                                             TWhichFunction whichFunc,
                                             TSelectFunction selectFunc,
                                             TResetFunction resetFunc = 0);

public:
#if HAVE_NCBI_C
    static CChoiceTypeInfo* CreateAsnChoiceInfo(const char* name);
    static CClassTypeInfo* CreateAsnStructInfo(const char* name, size_t size,
                                               const type_info& id);
#endif
    
protected:
    static void SetCreateFunction(CClassTypeInfo* info, TCreateFunction func);
    static void UpdateCObject(CClassTypeInfoBase* /*info*/,
                              const void* /*object*/)
        {
            // do nothing
        }
    static void UpdateCObject(CClassTypeInfoBase* info,
                              const CObject* object);

    static CClassTypeInfo* CreateClassInfo(const char* name, size_t size,
                                           const type_info& id,
                                           TGetTypeIdFunction func);
private:
    static CClassTypeInfo* CreateClassInfo(const char* name, size_t size,
                                           const type_info& id);
};

// template collecting all helper methods for generated classes
template<class C>
class CClassInfoHelper : public CClassInfoHelperBase
{
    typedef CClassInfoHelperBase CParent;
public:
    typedef C CClassType;

    static CClassType& Get(void* object)
        {
            return *static_cast<CClassType*>(object);
        }
    static const CClassType& Get(const void* object)
        {
            return *static_cast<const CClassType*>(object);
        }

    static void* Create(TTypeInfo /*typeInfo*/)
        {
            return new CClassType();
        }

    static const type_info* GetTypeId(const void* object)
        {
            return &typeid(Get(object));
        }
    static void Reset(void* object)
        {
            Get(object).Reset();
        }

    static int Which(const void* object)
        {
            return Get(object).Which() - 1;
        }
    static void ResetChoice(void* object)
        {
            if ( Which(object) != -1 )
                Reset(object);
        }
    static void Select(void* object, int index)
        {
            typedef typename CClassType::E_Choice E_Choice;
            Get(object).Select(E_Choice(index+1));
        }
    static void SelectDelayBuffer(void* object, int index)
        {
            typedef typename CClassType::E_Choice E_Choice;
            Get(object).SelectDelayBuffer(E_Choice(index+1));
        }

    static void SetReadWriteMethods(NCBI_NS_NCBI::CClassTypeInfo* info)
        {
            const CClassType* object = 0;
            UpdateCObject(info, object);
            NCBISERSetPostRead(object, info);
            NCBISERSetPreWrite(object, info);
        }
    static void SetReadWriteMethods(NCBI_NS_NCBI::CChoiceTypeInfo* info)
        {
            const CClassType* object = 0;
            UpdateCObject(info, object);
            NCBISERSetPostRead(object, info);
            NCBISERSetPreWrite(object, info);
        }

    static CClassTypeInfo* CreateAbstractClassInfo(const char* name)
        {
            CClassTypeInfo* info =
                CParent::CreateClassInfo(name, sizeof(CClassType),
                                         typeid(CClassType), &GetTypeId);
            SetReadWriteMethods(info);
            return info;
        }
    static CClassTypeInfo* CreateClassInfo(const char* name)
        {
            CClassTypeInfo* info = CreateAbstractClassInfo(name);
            SetCreateFunction(info, &Create);
            return info;
        }

    static CChoiceTypeInfo* CreateChoiceInfo(const char* name)
        {
            CChoiceTypeInfo* info =
                CParent::CreateChoiceInfo(name, sizeof(CClassType),
                                          typeid(CClassType), &Create,
                                          &Which, &Select, &ResetChoice);
            SetReadWriteMethods(info);
            return info;
        }

    static CClassTypeInfo* CreateAsnStructInfo(const char* name)
        {
            return CParent::CreateAsnStructInfo(name,
                                                sizeof(CClassType),
                                                typeid(CClassType));
        }
};

//
// define type info getter for standard classes
template<typename T>
inline
TTypeInfoGetter GetStdTypeInfoGetter(const T* )
{
    return &CStdTypeInfo<T>::GetTypeInfo;
}

// some compilers cannot resolve overloading by
// (char* const*) and (const char* const*) in template
// so we'll add explicit implemetations:

inline
TTypeInfoGetter GetStdTypeInfoGetter(char* const* )
{
    return &GetStdTypeInfo_char_ptr;
}

inline
TTypeInfoGetter GetStdTypeInfoGetter(const char* const* )
{
    return &GetStdTypeInfo_const_char_ptr;
}


// macros used in ADD_*_MEMBER macros to specify complex type
// example: ADD_MEMBER(member, STL_set, (STD, (string)))
#define SERIAL_TYPE(TypeMacro) NCBI_NAME2(SERIAL_TYPE_,TypeMacro)
#define SERIAL_REF(TypeMacro) NCBI_NAME2(SERIAL_REF_,TypeMacro)

#define SERIAL_TYPE_CLASS(ClassName) ClassName
#define SERIAL_REF_CLASS(ClassName) &ClassName::GetTypeInfo

#define SERIAL_TYPE_STD(CType) CType
#define SERIAL_REF_STD(CType) &NCBI_NS_NCBI::CStdTypeInfo<CType>::GetTypeInfo

#define SERIAL_TYPE_StringStore() NCBI_NS_STD::string
#define SERIAL_REF_StringStore() \
    &NCBI_NS_NCBI::GetTypeInfoStringStore

#define SERIAL_TYPE_null() bool
#define SERIAL_REF_null() \
    &NCBI_NS_NCBI::GetTypeInfoNullBool

#define SERIAL_TYPE_ENUM(CType, EnumName) CType
#define SERIAL_REF_ENUM(CType, EnumName) \
    NCBI_NS_NCBI::CreateEnumeratedTypeInfo(CType(0), ENUM_METHOD_NAME(EnumName)())

#define SERIAL_TYPE_ENUM_IN(CType, CppContext, EnumName) CppContext CType
#define SERIAL_REF_ENUM_IN(CType, CppContext, EnumName) \
    NCBI_NS_NCBI::CreateEnumeratedTypeInfo(CppContext CType(0), CppContext ENUM_METHOD_NAME(EnumName)())

#define SERIAL_TYPE_POINTER(TypeMacro,TypeMacroArgs) \
    SERIAL_TYPE(TypeMacro)TypeMacroArgs*
#define SERIAL_REF_POINTER(TypeMacro,TypeMacroArgs) \
    &NCBI_NS_NCBI::CPointerTypeInfo::GetTypeInfo, SERIAL_REF(TypeMacro)TypeMacroArgs

#define SERIAL_TYPE_STL_multiset(TypeMacro,TypeMacroArgs) \
    NCBI_NS_STD::multiset<SERIAL_TYPE(TypeMacro)TypeMacroArgs >
#define SERIAL_REF_STL_multiset(TypeMacro,TypeMacroArgs) \
    &NCBI_NS_NCBI::CStlClassInfo_multiset<SERIAL_TYPE(TypeMacro)TypeMacroArgs >::GetTypeInfo, SERIAL_REF(TypeMacro)TypeMacroArgs

#define SERIAL_TYPE_STL_set(TypeMacro,TypeMacroArgs) \
    NCBI_NS_STD::set<SERIAL_TYPE(TypeMacro)TypeMacroArgs >
#define SERIAL_REF_STL_set(TypeMacro,TypeMacroArgs) \
    &NCBI_NS_NCBI::CStlClassInfo_set<SERIAL_TYPE(TypeMacro)TypeMacroArgs >::GetTypeInfo, SERIAL_REF(TypeMacro)TypeMacroArgs

#define SERIAL_TYPE_STL_multimap(KeyTypeMacro,KeyTypeMacroArgs,ValueTypeMacro,ValueTypeMacroArgs) \
    NCBI_NS_STD::multimap<SERIAL_TYPE(KeyTypeMacro)KeyTypeMacroArgs,SERIAL_TYPE(ValueTypeMacro)ValueTypeMacroArgs >
#define SERIAL_REF_STL_multimap(KeyTypeMacro,KeyTypeMacroArgs,ValueTypeMacro,ValueTypeMacroArgs) \
    &NCBI_NS_NCBI::CStlClassInfo_multimap<SERIAL_TYPE(KeyTypeMacro)KeyTypeMacroArgs,SERIAL_TYPE(ValueTypeMacro)ValueTypeMacroArgs >::GetTypeInfo, SERIAL_REF(KeyTypeMacro)KeyTypeMacroArgs,SERIAL_REF(ValueTypeMacro)ValueTypeMacroArgs

#define SERIAL_TYPE_STL_map(KeyTypeMacro,KeyTypeMacroArgs,ValueTypeMacro,ValueTypeMacroArgs) \
    NCBI_NS_STD::map<SERIAL_TYPE(KeyTypeMacro)KeyTypeMacroArgs,SERIAL_TYPE(ValueTypeMacro)ValueTypeMacroArgs >
#define SERIAL_REF_STL_map(KeyTypeMacro,KeyTypeMacroArgs,ValueTypeMacro,ValueTypeMacroArgs) \
    &NCBI_NS_NCBI::CStlClassInfo_map<SERIAL_TYPE(KeyTypeMacro)KeyTypeMacroArgs,SERIAL_TYPE(ValueTypeMacro)ValueTypeMacroArgs >::GetTypeInfo, SERIAL_REF(KeyTypeMacro)KeyTypeMacroArgs,SERIAL_REF(ValueTypeMacro)ValueTypeMacroArgs

#define SERIAL_TYPE_STL_list(TypeMacro,TypeMacroArgs) \
    NCBI_NS_STD::list<SERIAL_TYPE(TypeMacro)TypeMacroArgs >
#define SERIAL_REF_STL_list(TypeMacro,TypeMacroArgs) \
    &NCBI_NS_NCBI::CStlClassInfo_list<SERIAL_TYPE(TypeMacro)TypeMacroArgs >::GetTypeInfo, SERIAL_REF(TypeMacro)TypeMacroArgs

#define SERIAL_TYPE_STL_list_set(TypeMacro,TypeMacroArgs) \
    NCBI_NS_STD::list<SERIAL_TYPE(TypeMacro)TypeMacroArgs >
#define SERIAL_REF_STL_list_set(TypeMacro,TypeMacroArgs) \
    &NCBI_NS_NCBI::CStlClassInfo_list<SERIAL_TYPE(TypeMacro)TypeMacroArgs >::GetSetTypeInfo, SERIAL_REF(TypeMacro)TypeMacroArgs

#define SERIAL_TYPE_STL_vector(TypeMacro,TypeMacroArgs) \
    NCBI_NS_STD::vector<SERIAL_TYPE(TypeMacro)TypeMacroArgs >
#define SERIAL_REF_STL_vector(TypeMacro,TypeMacroArgs) \
    &NCBI_NS_NCBI::CStlClassInfo_vector<SERIAL_TYPE(TypeMacro)TypeMacroArgs >::GetTypeInfo, SERIAL_REF(TypeMacro)TypeMacroArgs

#define SERIAL_TYPE_STL_CHAR_vector(CharType) NCBI_NS_STD::vector<CharType>
#define SERIAL_REF_STL_CHAR_vector(CharType) \
    &NCBI_NS_NCBI::CCharVectorTypeInfo<CharType>::GetTypeInfo

#define SERIAL_TYPE_STL_auto_ptr(TypeMacro,TypeMacroArgs) \
    NCBI_NS_STD::auto_ptr<SERIAL_TYPE(TypeMacro)TypeMacroArgs >
#define SERIAL_REF_STL_auto_ptr(TypeMacro,TypeMacroArgs) \
    &NCBI_NS_NCBI::CStlClassInfo_auto_ptr<SERIAL_TYPE(TypeMacro)TypeMacroArgs >::GetTypeInfo, SERIAL_REF(TypeMacro)TypeMacroArgs

#define SERIAL_TYPE_STL_AutoPtr(TypeMacro,TypeMacroArgs) NCBI_NS_NCBI::AutoPtr<SERIAL_TYPE(TypeMacro)TypeMacroArgs >
#define SERIAL_REF_STL_AutoPtr(TypeMacro,TypeMacroArgs) \
    &NCBI_NS_NCBI::CAutoPtrTypeInfo<SERIAL_TYPE(TypeMacro)TypeMacroArgs >::GetTypeInfo, SERIAL_REF(TypeMacro)TypeMacroArgs

#define SERIAL_TYPE_STL_CRef(TypeMacro,TypeMacroArgs) NCBI_NS_NCBI::CRef<SERIAL_TYPE(TypeMacro)TypeMacroArgs >
#define SERIAL_REF_STL_CRef(TypeMacro,TypeMacroArgs) \
    &NCBI_NS_NCBI::CRefTypeInfo<SERIAL_TYPE(TypeMacro)TypeMacroArgs >::GetTypeInfo, SERIAL_REF(TypeMacro)TypeMacroArgs

#define SERIAL_REF_CHOICE_POINTER(TypeMacro,TypeMacroArgs) \
    &NCBI_NS_NCBI::CChoicePointerTypeInfo::GetTypeInfo,\
    SERIAL_REF(TypeMacro)TypeMacroArgs

#define SERIAL_REF_CHOICE_STL_auto_ptr(TypeMacro,TypeMacroArgs) \
    &NCBI_NS_NCBI::CChoiceStlClassInfo_auto_ptr<SERIAL_TYPE(TypeMacro)TypeMacroArgs >::GetTypeInfo, SERIAL_REF(TypeMacro)TypeMacroArgs

#define SERIAL_REF_CHOICE_STL_AutoPtr(TypeMacro,TypeMacroArgs) \
    &NCBI_NS_NCBI::CChoiceAutoPtrTypeInfo<SERIAL_TYPE(TypeMacro)TypeMacroArgs >::GetTypeInfo, SERIAL_REF(TypeMacro)TypeMacroArgs

#define SERIAL_TYPE_CHOICE(TypeMacro,TypeMacroArgs) \
    SERIAL_TYPE(TypeMacro)TypeMacroArgs
#define SERIAL_REF_CHOICE(TypeMacro,TypeMacroArgs) \
    SERIAL_REF(NCBI_NAME2(CHOICE_,Type))Args

template<typename T>
struct Check
{
    static CMemberInfo* Member(const T* member, const CTypeRef& type)
        {
            return new CRealMemberInfo(size_t(member), type);
        }
    static const void* Ptr(const T* member)
        {
            return member;
        }
    static const void* PtrPtr(T*const* member)
        {
            return member;
        }
    static const void* ObjectPtrPtr(T*const* member)
        {
            return member;
        }
    static const void* ObjectPtrPtr(CObject*const* member)
        {
            return member;
        }
private:
    Check(void);
    ~Check(void);
    Check(const Check<T>&);
    Check<T>& operator=(const Check<T>&);
};

template<typename T>
inline
TTypeInfo EnumTypeInfo(const T* member, const CEnumeratedTypeValues* enumInfo)
{
    return CreateEnumeratedTypeInfo(*member, enumInfo);
}

// internal macros for implementing BEGIN_*_INFO and ADD_*_MEMBER
#define DECLARE_BASE_OBJECT(ClassName) 
#define BASE_OBJECT() static_cast<const CClass_Base*>(static_cast<const CClass*>(0))
#define MEMBER_PTR(MemberName) &BASE_OBJECT()->MemberName
#define CLASS_PTR(ClassName) static_cast<const ClassName*>(BASE_OBJECT())

#define BEGIN_BASE_TYPE_INFO(ClassName,BaseClassName,Method,InfoType,Code) \
const NCBI_NS_NCBI::CTypeInfo* Method(void) \
{ \
    typedef ClassName CClass; \
	typedef BaseClassName CClass_Base; \
    static InfoType* info; \
    if ( !info ) { \
        DECLARE_BASE_OBJECT(CClass); \
        info = Code;
#define BEGIN_TYPE_INFO(ClassName, Method, InfoType, Code) \
	BEGIN_BASE_TYPE_INFO(ClassName, ClassName, Method, InfoType, Code)

#define END_TYPE_INFO \
    } \
    return info; \
}

// macros for specifying differents members
#define SERIAL_MEMBER(MemberName,TypeMacro,TypeMacroArgs) \
    NCBI_NS_NCBI::Check<SERIAL_TYPE(TypeMacro)TypeMacroArgs >::Ptr(MEMBER_PTR(MemberName)), SERIAL_REF(TypeMacro)TypeMacroArgs
#define SERIAL_STD_MEMBER(MemberName) \
    MEMBER_PTR(MemberName),NCBI_NS_NCBI::GetStdTypeInfoGetter(MEMBER_PTR(MemberName))
#define SERIAL_CLASS_MEMBER(MemberName) \
    MEMBER_PTR(MemberName),&MEMBER_PTR(MemberName).GetTypeInfo)
#define SERIAL_ENUM_MEMBER(MemberName,EnumName) \
    MEMBER_PTR(MemberName), NCBI_NS_NCBI::EnumTypeInfo(MEMBER_PTR(MemberName), ENUM_METHOD_NAME(EnumName)())
#define SERIAL_ENUM_IN_MEMBER(MemberName,CppContext,EnumName) \
    MEMBER_PTR(MemberName), NCBI_NS_NCBI::EnumTypeInfo(MEMBER_PTR(MemberName),CppContext ENUM_METHOD_NAME(EnumName)())
#define SERIAL_REF_MEMBER(MemberName,ClassName) \
    SERIAL_MEMBER(MemberName,STL_CRef,(CLASS,(ClassName)))
#define SERIAL_PTR_CHOICE_VARIANT(MemberName,TypeMacro,TypeMacroArgs) \
    NCBI_NS_NCBI::Check<SERIAL_TYPE(TypeMacro)TypeMacroArgs >::PtrPtr(MEMBER_PTR(MemberName)), SERIAL_REF(TypeMacro)TypeMacroArgs
#define SERIAL_REF_CHOICE_VARIANT(MemberName,ClassName) \
    NCBI_NS_NCBI::Check<SERIAL_TYPE(CLASS)(ClassName)>::ObjectPtrPtr(MEMBER_PTR(MemberName)), SERIAL_REF(CLASS)(ClassName)

// ADD_NAMED_*_MEMBER macros    
#define ADD_NAMED_MEMBER(MemberAlias,MemberName,TypeMacro,TypeMacroArgs) \
    NCBI_NS_NCBI::AddMember(info->GetMembers(),MemberAlias, \
                            SERIAL_MEMBER(MemberName,TypeMacro,TypeMacroArgs))
#define ADD_NAMED_STD_MEMBER(MemberAlias,MemberName) \
    NCBI_NS_NCBI::AddMember(info->GetMembers(),MemberAlias, \
                            SERIAL_STD_MEMBER(MemberName))
#define ADD_NAMED_CLASS_MEMBER(MemberAlias,MemberName) \
    NCBI_NS_NCBI::AddMember(info->GetMembers(),MemberAlias, \
                            SERIAL_CLASS_MEMBER(MemberName))
#define ADD_NAMED_ENUM_MEMBER(MemberAlias,MemberName,EnumName) \
    NCBI_NS_NCBI::AddMember(info->GetMembers(),MemberAlias, \
                            SERIAL_ENUM_MEMBER(MemberName,EnumName))
#define ADD_NAMED_ENUM_IN_MEMBER(MemberAlias,MemberName,CppContext,EnumName) \
    NCBI_NS_NCBI::AddMember(info->GetMembers(),MemberAlias, \
                  SERIAL_ENUM_IN_MEMBER(MemberName,CppContext,EnumName))
#define ADD_NAMED_REF_MEMBER(MemberAlias,MemberName,ClassName) \
    NCBI_NS_NCBI::AddMember(info->GetMembers(),MemberAlias, \
                            SERIAL_REF_MEMBER(MemberName,ClassName))

// ADD_*_MEMBER macros    
#define ADD_MEMBER(MemberName,TypeMacro,TypeMacroArgs) \
    ADD_NAMED_MEMBER(#MemberName,MemberName,TypeMacro,TypeMacroArgs)
#define ADD_STD_MEMBER(MemberName) \
    ADD_NAMED_STD_MEMBER(#MemberName,MemberName)
#define ADD_CLASS_MEMBER(MemberName) \
    ADD_NAMED_CLASS_MEMBER(#MemberName,MemberName)
#define ADD_ENUM_MEMBER(MemberName,EnumName) \
    ADD_NAMED_ENUM_MEMBER(#MemberName,MemberName,EnumName)
#define ADD_ENUM_IN_MEMBER(MemberName,CppContext,EnumName) \
    ADD_NAMED_ENUM_MEMBER(#MemberName,MemberName,CppContext,EnumName)
#define ADD_REF_MEMBER(MemberName,ClassName) \
    ADD_NAMED_REF_MEMBER(#MemberName,MemberName,ClassName)

// ADD_NAMED_*_CHOICE_VARIANT macros    
#define ADD_NAMED_CHOICE_VARIANT(MemberAlias,MemberName,TypeMacro,TypeMacroArgs) \
    ADD_NAMED_MEMBER(MemberAlias,MemberName,TypeMacro,TypeMacroArgs)
#define ADD_NAMED_STD_CHOICE_VARIANT(MemberAlias,MemberName) \
    ADD_NAMED_STD_MEMBER(MemberAlias,MemberName)
#define ADD_NAMED_ENUM_CHOICE_VARIANT(MemberAlias,MemberName,EnumName) \
    ADD_NAMED_ENUM_MEMBER(MemberAlias,MemberName,EnumName)
#define ADD_NAMED_ENUM_IN_CHOICE_VARIANT(MemberAlias,MemberName,CppContext,EnumName) \
    ADD_NAMED_ENUM_IN_MEMBER(MemberAlias,MemberName,CppContext,EnumName)
#define ADD_NAMED_PTR_CHOICE_VARIANT(MemberAlias,MemberName,TypeMacro,TypeMacroArgs) \
    NCBI_NS_NCBI::AddMember(info->GetMembers(),MemberAlias, \
                SERIAL_PTR_CHOICE_VARIANT(MemberName,TypeMacro,TypeMacroArgs))->SetPointer()
#define ADD_NAMED_REF_CHOICE_VARIANT(MemberAlias,MemberName,ClassName) \
    NCBI_NS_NCBI::AddMember(info->GetMembers(),MemberAlias, \
                SERIAL_REF_CHOICE_VARIANT(MemberName,ClassName))->SetObjectPointer()

// ADD_*_CHOICE_VARIANT macros
#define ADD_CHOICE_VARIANT(MemberName,TypeMacro,TypeMacroArgs) \
    ADD_NAMED_CHOICE_VARIANT(#MemberName,MemberName,TypeMacro,TypeMacroArgs)
#define ADD_STD_CHOICE_VARIANT(MemberName) \
    ADD_NAMED_STD_CHOICE_VARIANT(#MemberName,MemberName)
#define ADD_ENUM_CHOICE_VARIANT(MemberName,EnumName) \
    ADD_NAMED_ENUM_CHOICE_VARIANT(#MemberName,MemberName,EnumName)
#define ADD_ENUM_IN_CHOICE_VARIANT(MemberName,CppContext,EnumName) \
    ADD_NAMED_ENUM_IN_CHOICE_VARIANT(#MemberName,MemberName,CppContext,EnumName)
#define ADD_PTR_CHOICE_VARIANT(MemberName,TypeMacro,TypeMacroArgs) \
    ADD_NAMED_PTR_CHOICE_VARIANT(#MemberName,MemberName,TypeMacro,TypeMacroArgs)
#define ADD_REF_CHOICE_VARIANT(MemberName,ClassName) \
    ADD_NAMED_REF_CHOICE_VARIANT(#MemberName,MemberName,ClassName)

// type info definition macros
#define BEGIN_NAMED_CLASS_INFO(ClassAlias,ClassName) \
	BEGIN_TYPE_INFO(ClassName, \
        ClassName::GetTypeInfo, \
		NCBI_NS_NCBI::CClassTypeInfo, \
        NCBI_NS_NCBI::CClassInfoHelper<CClass>::CreateClassInfo(ClassAlias))
#define BEGIN_CLASS_INFO(ClassName) \
	BEGIN_NAMED_CLASS_INFO(#ClassName, ClassName)
#define BEGIN_NAMED_BASE_CLASS_INFO(ClassAlias,ClassName) \
	BEGIN_BASE_TYPE_INFO(ClassName, NCBI_NAME2(ClassName,_Base), \
		NCBI_NAME2(ClassName,_Base)::GetTypeInfo, \
        NCBI_NS_NCBI::CClassTypeInfo, \
        NCBI_NS_NCBI::CClassInfoHelper<CClass>::CreateClassInfo(ClassAlias))
#define BEGIN_BASE_CLASS_INFO(ClassName) \
	BEGIN_NAMED_BASE_CLASS_INFO(#ClassName, ClassName)

#define END_CLASS_INFO END_TYPE_INFO

#define BEGIN_NAMED_ABSTRACT_CLASS_INFO(ClassAlias,ClassName) \
	BEGIN_TYPE_INFO(ClassName, \
        ClassName::GetTypeInfo, \
		NCBI_NS_NCBI::CClassTypeInfo, \
        NCBI_NS_NCBI::CClassInfoHelper<CClass>::CreateAbstractClassInfo(ClassAlias))
#define BEGIN_ABSTRACT_CLASS_INFO(ClassName) \
	BEGIN_NAMED_ABSTRACT_CLASS_INFO(#ClassName, ClassName)
#define BEGIN_NAMED_ABSTRACT_BASE_CLASS_INFO(ClassAlias,ClassName) \
	BEGIN_BASE_TYPE_INFO(ClassName, NCBI_NAME2(ClassName,_Base), \
		NCBI_NAME2(ClassName,_Base)::GetTypeInfo, \
		NCBI_NS_NCBI::CClassTypeInfo, \
        NCBI_NS_NCBI::CClassInfoHelper<CClass>::CreateAbstractClassInfo(ClassAlias))

#define END_ABSTRACT_CLASS_INFO END_TYPE_INFO

#define BEGIN_NAMED_DERIVED_CLASS_INFO(ClassAlias,ClassName,ParentClassName) \
	BEGIN_NAMED_CLASS_INFO(ClassAlias,ClassName) \
    SET_PARENT_CLASS(ParentClassName);
#define BEGIN_DERIVED_CLASS_INFO(ClassName,ParentClassName) \
	BEGIN_NAMED_DERIVED_CLASS_INFO(#ClassName, ClassName, ParentClassName)

#define END_DERIVED_CLASS_INFO END_TYPE_INFO

#define BEGIN_NAMED_CHOICE_INFO(ClassAlias,ClassName) \
    BEGIN_TYPE_INFO(ClassName, \
		ClassName::GetTypeInfo, \
		NCBI_NS_NCBI::CChoiceTypeInfo, \
        NCBI_NS_NCBI::CClassInfoHelper<CClass>::CreateChoiceInfo(ClassAlias))
#define BEGIN_CHOICE_INFO(ClassName) \
    BEGIN_NAMED_CHOICE_INFO(#ClassName, ClassName)
#define BEGIN_NAMED_BASE_CHOICE_INFO(ClassAlias,ClassName) \
    BEGIN_BASE_TYPE_INFO(ClassName, NCBI_NAME2(ClassName,_Base), \
		NCBI_NAME2(ClassName,_Base)::GetTypeInfo, \
		NCBI_NS_NCBI::CChoiceTypeInfo, \
        NCBI_NS_NCBI::CClassInfoHelper<CClass>::CreateChoiceInfo(ClassAlias))
#define BEGIN_BASE_CHOICE_INFO(ClassName) \
    BEGIN_NAMED_BASE_CHOICE_INFO(#ClassName, ClassName)
#define END_CHOICE_INFO END_TYPE_INFO

// sub class definition
#define SET_PARENT_CLASS(ParentClassName) \
    info->SetParentClass(ParentClassName::GetTypeInfo())
#define ADD_NAMED_NULL_SUB_CLASS(ClassAlias) \
    info->AddSubClassNull(ClassAlias)
#define ADD_NAMED_SUB_CLASS(SubClassAlias, SubClassName) \
    info->AddSubClass(SubClassAlias, &SubClassName::GetTypeInfo)
#define ADD_SUB_CLASS(SubClassName) \
    ADD_NAMED_SUB_CLASS(#SubClassName, SubClassName)

// enum definition macros
#define BEGIN_NAMED_ENUM_INFO(EnumAlias, EnumName, IsInteger) \
const NCBI_NS_NCBI::CEnumeratedTypeValues* ENUM_METHOD_NAME(EnumName)(void) \
{ static NCBI_NS_NCBI::CEnumeratedTypeValues* enumInfo; if ( !enumInfo ) { \
    enumInfo = new NCBI_NS_NCBI::CEnumeratedTypeValues(EnumAlias, IsInteger); \
    EnumName enumValue;
#define BEGIN_NAMED_ENUM_IN_INFO(EnumAlias, CppContext, EnumName, IsInteger) \
const NCBI_NS_NCBI::CEnumeratedTypeValues* CppContext ENUM_METHOD_NAME(EnumName)(void) \
{ static NCBI_NS_NCBI::CEnumeratedTypeValues* enumInfo; if ( !enumInfo ) { \
    enumInfo = new NCBI_NS_NCBI::CEnumeratedTypeValues(EnumAlias, IsInteger); \
    EnumName enumValue;
#define BEGIN_ENUM_INFO(EnumName, IsInteger) \
    BEGIN_NAMED_ENUM_INFO(#EnumName, EnumName, IsInteger)
#define BEGIN_ENUM_IN_INFO(CppContext, EnumName, IsInteger) \
    BEGIN_NAMED_ENUM_IN_INFO(#EnumName, CppContext, EnumName, IsInteger)

#define ADD_ENUM_VALUE(EnumValueName, EnumValueValue) \
    enumInfo->AddValue(EnumValueName, enumValue = EnumValueValue)

#define END_ENUM_INFO } return enumInfo; }
#define END_ENUM_IN_INFO } return enumInfo; }

// member types
template<typename T>
inline
CMemberInfo* MemberInfo(const T* member, const CTypeRef typeRef)
{
	return new CRealMemberInfo(size_t(member), typeRef);
}

// add member methods
CMemberInfo*
AddMember(CMembersInfo& info, const char* name, const void* member,
          TTypeInfo typeInfo);

CMemberInfo*
AddMember(CMembersInfo& info, const char* name, CMemberInfo* memberInfo);

CMemberInfo*
AddMember(CMembersInfo& info, const char* name, const void* member,
          const CTypeRef& typeRef);

// one argument:
CMemberInfo*
AddMember(CMembersInfo& info, const char* name, const void* member,
          TTypeInfoGetter f);
// two arguments:
CMemberInfo*
AddMember(CMembersInfo& info, const char* name, const void* member,
          TTypeInfoGetter1 f, TTypeInfo t);
CMemberInfo*
AddMember(CMembersInfo& info, const char* name, const void* member,
          TTypeInfoGetter1 f, TTypeInfoGetter f1);
// three arguments:
CMemberInfo*
AddMember(CMembersInfo& info, const char* name, const void* member,
          TTypeInfoGetter1 f, TTypeInfoGetter1 f1, TTypeInfo t1);
CMemberInfo*
AddMember(CMembersInfo& info, const char* name, const void* member,
          TTypeInfoGetter1 f, TTypeInfoGetter1 f1, TTypeInfoGetter f11);
CMemberInfo*
AddMember(CMembersInfo& info, const char* name, const void* member,
          TTypeInfoGetter2 f, TTypeInfo t1, TTypeInfo t2);
CMemberInfo*
AddMember(CMembersInfo& info, const char* name, const void* member,
          TTypeInfoGetter2 f, TTypeInfoGetter f1, TTypeInfo t2);
CMemberInfo*
AddMember(CMembersInfo& info, const char* name, const void* member,
          TTypeInfoGetter2 f, TTypeInfo t1, TTypeInfoGetter f2);
CMemberInfo*
AddMember(CMembersInfo& info, const char* name, const void* member,
          TTypeInfoGetter2 f, TTypeInfoGetter f1, TTypeInfoGetter f2);

END_NCBI_SCOPE

#endif
