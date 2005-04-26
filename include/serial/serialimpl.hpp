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
#include <corelib/ncbimtx.hpp>
#include <serial/typeinfo.hpp>
#include <serial/stdtypes.hpp>
#include <serial/stltypes.hpp>
#include <serial/ptrinfo.hpp>
#include <serial/enumerated.hpp>
#include <serial/classinfo.hpp>
#include <serial/choice.hpp>
#include <serial/choiceptr.hpp>
#include <serial/autoptrinfo.hpp>
#include <serial/serialbase.hpp>
#include <serial/exception.hpp>
#include <serial/classinfohelper.hpp>


/** @addtogroup GenClassSupport
 *
 * @{
 */


struct valnode;

BEGIN_NCBI_SCOPE

// forward declaration
class CMemberInfo;
class CClassTypeInfoBase;
class CClassTypeInfo;
class CChoiceTypeInfo;
class CDelayBufferData;

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
// so we'll add explicit implementations:

inline
TTypeInfoGetter GetStdTypeInfoGetter(char* const* )
{
    return &CStdTypeInfo<char*>::GetTypeInfo;
}

inline
TTypeInfoGetter GetStdTypeInfoGetter(const char* const* )
{
    return &CStdTypeInfo<const char*>::GetTypeInfo;
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
    &NCBI_NS_NCBI::CStdTypeInfo<string>::GetTypeInfoStringStore

#define SERIAL_TYPE_null() bool
#define SERIAL_REF_null() \
    &NCBI_NS_NCBI::CStdTypeInfo<bool>::GetTypeInfoNullBool

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
    CTypeRef(&NCBI_NS_NCBI::CStlClassInfo_multimap<SERIAL_TYPE(KeyTypeMacro)KeyTypeMacroArgs,SERIAL_TYPE(ValueTypeMacro)ValueTypeMacroArgs >::GetTypeInfo, SERIAL_REF(KeyTypeMacro)KeyTypeMacroArgs,SERIAL_REF(ValueTypeMacro)ValueTypeMacroArgs)

#define SERIAL_TYPE_STL_map(KeyTypeMacro,KeyTypeMacroArgs,ValueTypeMacro,ValueTypeMacroArgs) \
    NCBI_NS_STD::map<SERIAL_TYPE(KeyTypeMacro)KeyTypeMacroArgs,SERIAL_TYPE(ValueTypeMacro)ValueTypeMacroArgs >
#define SERIAL_REF_STL_map(KeyTypeMacro,KeyTypeMacroArgs,ValueTypeMacro,ValueTypeMacroArgs) \
    CTypeRef(&NCBI_NS_NCBI::CStlClassInfo_map<SERIAL_TYPE(KeyTypeMacro)KeyTypeMacroArgs,SERIAL_TYPE(ValueTypeMacro)ValueTypeMacroArgs >::GetTypeInfo, SERIAL_REF(KeyTypeMacro)KeyTypeMacroArgs,SERIAL_REF(ValueTypeMacro)ValueTypeMacroArgs)

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

#define SERIAL_TYPE_STL_vector_set(TypeMacro,TypeMacroArgs) \
    NCBI_NS_STD::vector<SERIAL_TYPE(TypeMacro)TypeMacroArgs >
#define SERIAL_REF_STL_vector_set(TypeMacro,TypeMacroArgs) \
    &NCBI_NS_NCBI::CStlClassInfo_vector<SERIAL_TYPE(TypeMacro)TypeMacroArgs >::GetSetTypeInfo, SERIAL_REF(TypeMacro)TypeMacroArgs

#define SERIAL_TYPE_STL_CHAR_vector(CharType) NCBI_NS_STD::vector<CharType>
#define SERIAL_REF_STL_CHAR_vector(CharType) \
    &NCBI_NS_NCBI::CStdTypeInfo< SERIAL_TYPE(STL_CHAR_vector)(CharType) >::GetTypeInfo

#define SERIAL_TYPE_STL_auto_ptr(TypeMacro,TypeMacroArgs) \
    NCBI_NS_STD::auto_ptr<SERIAL_TYPE(TypeMacro)TypeMacroArgs >
#define SERIAL_REF_STL_auto_ptr(TypeMacro,TypeMacroArgs) \
    &NCBI_NS_NCBI::CStlClassInfo_auto_ptr<SERIAL_TYPE(TypeMacro)TypeMacroArgs >::GetTypeInfo, SERIAL_REF(TypeMacro)TypeMacroArgs

#define SERIAL_TYPE_STL_AutoPtr(TypeMacro,TypeMacroArgs) \
    NCBI_NS_NCBI::AutoPtr<SERIAL_TYPE(TypeMacro)TypeMacroArgs >
#define SERIAL_REF_STL_AutoPtr(TypeMacro,TypeMacroArgs) \
    &NCBI_NS_NCBI::CAutoPtrTypeInfo<SERIAL_TYPE(TypeMacro)TypeMacroArgs >::GetTypeInfo, SERIAL_REF(TypeMacro)TypeMacroArgs

#define SERIAL_TYPE_STL_CRef(TypeMacro,TypeMacroArgs) \
    NCBI_NS_NCBI::CRef<SERIAL_TYPE(TypeMacro)TypeMacroArgs >
#define SERIAL_REF_STL_CRef(TypeMacro,TypeMacroArgs) \
    &NCBI_NS_NCBI::CRefTypeInfo<SERIAL_TYPE(TypeMacro)TypeMacroArgs >::GetTypeInfo, SERIAL_REF(TypeMacro)TypeMacroArgs

#define SERIAL_TYPE_STL_CConstRef(TypeMacro,TypeMacroArgs) \
    NCBI_NS_NCBI::CConstRef<SERIAL_TYPE(TypeMacro)TypeMacroArgs >
#define SERIAL_REF_STL_CConstRef(TypeMacro,TypeMacroArgs) \
    &NCBI_NS_NCBI::CConstRefTypeInfo<SERIAL_TYPE(TypeMacro)TypeMacroArgs >::GetTypeInfo, SERIAL_REF(TypeMacro)TypeMacroArgs

#define SERIAL_TYPE_CHOICE(TypeMacro,TypeMacroArgs) \
    SERIAL_TYPE(TypeMacro)TypeMacroArgs
#define SERIAL_REF_CHOICE(TypeMacro,TypeMacroArgs) \
    &NCBI_NS_NCBI::CChoicePointerTypeInfo::GetTypeInfo, \
    SERIAL_REF(TypeMacro)TypeMacroArgs

//#define SERIAL_TYPE_CHOICERef(ClassName) NCBI_NS_NCBI::CRef<ClassName>
//#define SERIAL_REF_CHOICERef(ClassName) &ClassName::GetChoiceRefTypeInfo
#define SERIAL_TYPE_CHOICERef(TypeMacro,TypeMacroArgs) \
    NCBI_NS_NCBI::CRef<SERIAL_TYPE(TypeMacro)TypeMacroArgs >
#define SERIAL_REF_CHOICERef(TypeMacro,TypeMacroArgs) \
    &NCBI_NS_NCBI::CChoicePointerTypeInfo::GetTypeInfo, \
    SERIAL_REF(TypeMacro)TypeMacroArgs


template<typename T>
struct Check
{
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
    static const void* ObjectPtrPtr(CSerialObject*const* member)
        {
            return member;
        }
private:
    Check(void);
    ~Check(void);
    Check(const Check<T>&);
    Check<T>& operator=(const Check<T>&);
};


// Functions preventing memory leaks due to undestroyed type info objects
NCBI_XSERIAL_EXPORT
void RegisterEnumTypeValuesObject(CEnumeratedTypeValues* object);

NCBI_XSERIAL_EXPORT
void RegisterTypeInfoObject(CTypeInfo* object);

template<typename T>
inline
TTypeInfo EnumTypeInfo(const T* member, const CEnumeratedTypeValues* enumInfo)
{
    return CreateEnumeratedTypeInfo(*member, enumInfo);
}

NCBI_XSERIAL_EXPORT SSystemMutex& GetTypeInfoMutex(void);

// internal macros for implementing BEGIN_*_INFO and ADD_*_MEMBER
#define DECLARE_BASE_OBJECT(ClassName) ClassName* base = 0
#define BASE_OBJECT() static_cast<const CClass_Base*>(base)
#define MEMBER_PTR(MemberName) &BASE_OBJECT()->MemberName
#define CLASS_PTR(ClassName) static_cast<const ClassName*>(BASE_OBJECT())

#define BEGIN_BASE_TYPE_INFO(ClassName,BaseClassName,Method,InfoType,Code) \
const NCBI_NS_NCBI::CTypeInfo* Method(void) \
{ \
    typedef ClassName CClass; \
    typedef BaseClassName CClass_Base; \
    static InfoType* volatile s_info = 0; \
    InfoType* info = s_info; \
    if ( !info ) { \
        NCBI_NS_NCBI::CMutexGuard GUARD(NCBI_NS_NCBI::GetTypeInfoMutex()); \
        info = s_info; \
        if ( !info ) { \
            DECLARE_BASE_OBJECT(CClass); \
            info = Code; \
            NCBI_NS_NCBI::RegisterTypeInfoObject(info);
#define BEGIN_TYPE_INFO(ClassName, Method, InfoType, Code) \
    BEGIN_BASE_TYPE_INFO(ClassName, ClassName, Method, InfoType, Code)
    
#define END_TYPE_INFO \
            s_info = info; \
        } \
    } \
    return info; \
}

// macros for specifying differents members
#define SERIAL_MEMBER(MemberName,TypeMacro,TypeMacroArgs) \
    NCBI_NS_NCBI::Check<SERIAL_TYPE(TypeMacro)TypeMacroArgs >::Ptr(MEMBER_PTR(MemberName)), SERIAL_REF(TypeMacro)TypeMacroArgs
#define SERIAL_BUF_MEMBER(MemberName,TypeMacro,TypeMacroArgs) \
    NCBI_NS_NCBI::Check<NCBI_NS_NCBI::CUnionBuffer<SERIAL_TYPE(TypeMacro)TypeMacroArgs > >::Ptr(MEMBER_PTR(MemberName)), SERIAL_REF(TypeMacro)TypeMacroArgs
#define SERIAL_STD_MEMBER(MemberName) \
    MEMBER_PTR(MemberName),NCBI_NS_NCBI::GetStdTypeInfoGetter(MEMBER_PTR(MemberName))
#define SERIAL_CLASS_MEMBER(MemberName) \
    MEMBER_PTR(MemberName),&MEMBER_PTR(MemberName).GetTypeInfo
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
#define SERIAL_BASE_CLASS(ClassName) \
    CLASS_PTR(ClassName),&(CLASS_PTR(ClassName)->GetTypeInfo)

// ADD_NAMED_*_MEMBER macros    
#define ADD_NAMED_NULL_MEMBER(MemberAlias,TypeMacro,TypeMacroArgs) \
    NCBI_NS_NCBI::AddMember(info,MemberAlias, \
                            0,SERIAL_REF(TypeMacro)TypeMacroArgs)
#define ADD_NAMED_MEMBER(MemberAlias,MemberName,TypeMacro,TypeMacroArgs) \
    NCBI_NS_NCBI::AddMember(info,MemberAlias, \
                            SERIAL_MEMBER(MemberName,TypeMacro,TypeMacroArgs))
#define ADD_NAMED_STD_MEMBER(MemberAlias,MemberName) \
    NCBI_NS_NCBI::AddMember(info,MemberAlias, \
                            SERIAL_STD_MEMBER(MemberName))
#define ADD_NAMED_CLASS_MEMBER(MemberAlias,MemberName) \
    NCBI_NS_NCBI::AddMember(info,MemberAlias, \
                            SERIAL_CLASS_MEMBER(MemberName))
#define ADD_NAMED_ENUM_MEMBER(MemberAlias,MemberName,EnumName) \
    NCBI_NS_NCBI::AddMember(info,MemberAlias, \
                            SERIAL_ENUM_MEMBER(MemberName,EnumName))
#define ADD_NAMED_ENUM_IN_MEMBER(MemberAlias,MemberName,CppContext,EnumName) \
    NCBI_NS_NCBI::AddMember(info,MemberAlias, \
                  SERIAL_ENUM_IN_MEMBER(MemberName,CppContext,EnumName))
#define ADD_NAMED_REF_MEMBER(MemberAlias,MemberName,ClassName) \
    NCBI_NS_NCBI::AddMember(info,MemberAlias, \
                            SERIAL_REF_MEMBER(MemberName,ClassName))
#define ADD_NAMED_BASE_CLASS(MemberAlias,ClassName) \
    NCBI_NS_NCBI::AddMember(info,MemberAlias, \
                            SERIAL_BASE_CLASS(ClassName))

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
#define ADD_NAMED_NULL_CHOICE_VARIANT(MemberAlias,TypeMacro,TypeMacroArgs) \
    NCBI_NS_NCBI::AddVariant(info,MemberAlias, \
        0,SERIAL_REF(TypeMacro)TypeMacroArgs)
#define ADD_NAMED_CHOICE_VARIANT(MemberAlias,MemberName,TypeMacro,TypeMacroArgs) \
    NCBI_NS_NCBI::AddVariant(info,MemberAlias, \
        SERIAL_MEMBER(MemberName,TypeMacro,TypeMacroArgs))
#define ADD_NAMED_BUF_CHOICE_VARIANT(MemberAlias,MemberName,TypeMacro,TypeMacroArgs) \
    NCBI_NS_NCBI::AddVariant(info,MemberAlias, \
        SERIAL_BUF_MEMBER(MemberName,TypeMacro,TypeMacroArgs))
#define ADD_NAMED_STD_CHOICE_VARIANT(MemberAlias,MemberName) \
    NCBI_NS_NCBI::AddVariant(info,MemberAlias, \
        SERIAL_STD_MEMBER(MemberName))
#define ADD_NAMED_ENUM_CHOICE_VARIANT(MemberAlias,MemberName,EnumName) \
    NCBI_NS_NCBI::AddVariant(info,MemberAlias, \
        SERIAL_ENUM_MEMBER(MemberName,EnumName))
#define ADD_NAMED_ENUM_IN_CHOICE_VARIANT(MemberAlias,MemberName,CppContext,EnumName) \
    NCBI_NS_NCBI::AddVariant(info,MemberAlias, \
        SERIAL_ENUM_IN_MEMBER(MemberName,CppContext,EnumName))
#define ADD_NAMED_PTR_CHOICE_VARIANT(MemberAlias,MemberName,TypeMacro,TypeMacroArgs) \
    NCBI_NS_NCBI::AddVariant(info,MemberAlias, \
        SERIAL_PTR_CHOICE_VARIANT(MemberName,TypeMacro,TypeMacroArgs))->SetPointer()
#define ADD_NAMED_REF_CHOICE_VARIANT(MemberAlias,MemberName,ClassName) \
    NCBI_NS_NCBI::AddVariant(info,MemberAlias, \
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

#define SET_CLASS_IMPLICIT() info->SetImplicit()
#define BEGIN_NAMED_IMPLICIT_CLASS_INFO(ClassAlias,ClassName) \
    BEGIN_NAMED_CLASS_INFO(ClassAlias,ClassName); SET_CLASS_IMPLICIT();
#define BEGIN_IMPLICIT_CLASS_INFO(ClassName) \
    BEGIN_CLASS_INFO(ClassName); SET_CLASS_IMPLICIT();
#define BEGIN_NAMED_BASE_IMPLICIT_CLASS_INFO(ClassAlias,ClassName) \
    BEGIN_NAMED_BASE_CLASS_INFO(ClassAlias,ClassName); SET_CLASS_IMPLICIT();
#define BEGIN_BASE_IMPLICIT_CLASS_INFO(ClassName) \
    BEGIN_BASE_CLASS_INFO(ClassName); SET_CLASS_IMPLICIT();

#define SET_CLASS_MODULE(ModuleName) \
    NCBI_NS_NCBI::SetModuleName(info, ModuleName)

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

#define SET_CHOICE_MODULE(ModuleName) \
    NCBI_NS_NCBI::SetModuleName(info, ModuleName)

#define SET_CHOICE_DELAYED() \
    info->SetSelectDelay(&NCBI_NS_NCBI::CClassInfoHelper<CClass>::SelectDelayBuffer)

#define END_CHOICE_INFO END_TYPE_INFO

// sub class definition
#define SET_PARENT_CLASS(ParentClassName) \
    info->SetParentClass(ParentClassName::GetTypeInfo())
#define ADD_NAMED_SUB_CLASS(SubClassAlias, SubClassName) \
    info->AddSubClass(SubClassAlias, &SubClassName::GetTypeInfo)
#define ADD_SUB_CLASS(SubClassName) \
    ADD_NAMED_SUB_CLASS(#SubClassName, SubClassName)
#define ADD_NAMED_NULL_SUB_CLASS(ClassAlias) \
    info->AddSubClassNull(ClassAlias)
#define ADD_NULL_SUB_CLASS(ClassAlias) \
    ADD_NAMED_NULL_SUB_CLASS("NULL")

// enum definition macros
#define BEGIN_ENUM_INFO_METHOD(MethodName, EnumAlias, EnumName, IsInteger) \
const NCBI_NS_NCBI::CEnumeratedTypeValues* MethodName(void) \
{ \
    static NCBI_NS_NCBI::CEnumeratedTypeValues* volatile s_enumInfo = 0; \
    NCBI_NS_NCBI::CEnumeratedTypeValues* enumInfo = s_enumInfo; \
    if ( !enumInfo ) { \
        NCBI_NS_NCBI::CMutexGuard GUARD(NCBI_NS_NCBI::GetTypeInfoMutex()); \
        enumInfo = s_enumInfo; \
        if ( !enumInfo ) { \
            enumInfo = new NCBI_NS_NCBI::CEnumeratedTypeValues(EnumAlias, IsInteger); \
            NCBI_NS_NCBI::RegisterEnumTypeValuesObject(enumInfo); \
            EnumName enumValue;
#define END_ENUM_INFO_METHOD \
            s_enumInfo = enumInfo; \
        } \
    } \
    return enumInfo; \
}

#define BEGIN_NAMED_ENUM_IN_INFO(EnumAlias, CppContext, EnumName, IsInteger) \
    BEGIN_ENUM_INFO_METHOD(CppContext ENUM_METHOD_NAME(EnumName), EnumAlias, EnumName, IsInteger)
#define BEGIN_NAMED_ENUM_INFO(EnumAlias, EnumName, IsInteger) \
    BEGIN_ENUM_INFO_METHOD(ENUM_METHOD_NAME(EnumName), EnumAlias, EnumName, IsInteger)

#define BEGIN_ENUM_IN_INFO(CppContext, EnumName, IsInteger) \
    BEGIN_NAMED_ENUM_IN_INFO(#EnumName, CppContext, EnumName, IsInteger)
#define BEGIN_ENUM_INFO(EnumName, IsInteger) \
    BEGIN_NAMED_ENUM_INFO(#EnumName, EnumName, IsInteger)

#define SET_ENUM_MODULE(ModuleName) \
    NCBI_NS_NCBI::SetModuleName(enumInfo, ModuleName)

#define ADD_ENUM_VALUE(EnumValueName, EnumValueValue) \
    enumInfo->AddValue(EnumValueName, enumValue = EnumValueValue)

#define END_ENUM_IN_INFO END_ENUM_INFO_METHOD
#define END_ENUM_INFO END_ENUM_INFO_METHOD

// alias definition macros
#define SERIAL_ALIAS(RefType) \
    NCBI_NAME2(SERIAL_REF_, RefType)
#define ALIASED_TYPE_INFO(RefType, RefCode) \
    NCBI_NAME2(RefType, RefCode)
#define BEGIN_ALIAS_INFO_METHOD(AliasName,ClassName,BaseClassName,SerialRef,Code) \
const NCBI_NS_NCBI::CTypeInfo* BaseClassName::GetTypeInfo(void) \
{ \
    static NCBI_NS_NCBI::CAliasTypeInfo* volatile s_info = 0; \
    NCBI_NS_NCBI::CAliasTypeInfo* info = s_info; \
    if ( !info ) { \
        NCBI_NS_NCBI::CMutexGuard GUARD(NCBI_NS_NCBI::GetTypeInfoMutex()); \
        info = s_info; \
        if ( !info ) { \
            typedef ClassName CClass; \
            typedef BaseClassName CClass_Base; \
            DECLARE_BASE_OBJECT(ClassName); \
            typedef NCBI_NS_NCBI::TTypeInfo (*TGetter)(void); \
            TGetter getter = SerialRef Code; \
            info = new NCBI_NS_NCBI::CAliasTypeInfo(AliasName, getter()); \
            NCBI_NS_NCBI::RegisterTypeInfoObject(info);
#define BEGIN_ALIAS_INFO(AliasName,ClassName,RefType,RefCode) \
    BEGIN_ALIAS_INFO_METHOD(AliasName, ClassName, \
    NCBI_NAME2(ClassName,_Base), \
    SERIAL_ALIAS(RefType), RefCode)
#define SET_STD_ALIAS_DATA_PTR \
    info->SetDataOffset(NCBI_NS_NCBI::TPointerOffsetType(GetDataPtr(BASE_OBJECT())))
#define SET_CLASS_ALIAS_DATA_PTR \
    info->SetDataOffset(NCBI_NS_NCBI::TPointerOffsetType(BASE_OBJECT())); \
    info->SetCreateFunction(NCBI_NS_NCBI::CClassInfoHelper<CClass>::Create)
#define END_ALIAS_INFO \
            s_info = info; \
        } \
    } \
    return info; \
}
#define SET_ALIAS_MODULE(ModuleName) \
    NCBI_NS_NCBI::SetModuleName(info, ModuleName)



NCBI_XSERIAL_EXPORT
void SetModuleName(CTypeInfo* info, const char* name);

NCBI_XSERIAL_EXPORT
void SetModuleName(CEnumeratedTypeValues* info, const char* name);

// internal methods
// add member
NCBI_XSERIAL_EXPORT
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfo t);
NCBI_XSERIAL_EXPORT
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter f);
NCBI_XSERIAL_EXPORT
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       const CTypeRef& r);
NCBI_XSERIAL_EXPORT
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f1,
                       TTypeInfo t);
NCBI_XSERIAL_EXPORT
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f1,
                       TTypeInfoGetter f);
NCBI_XSERIAL_EXPORT
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f1,
                       const CTypeRef& r);
NCBI_XSERIAL_EXPORT
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfo t);
NCBI_XSERIAL_EXPORT
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfoGetter f);
NCBI_XSERIAL_EXPORT
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       const CTypeRef& r);
NCBI_XSERIAL_EXPORT
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfo t);
NCBI_XSERIAL_EXPORT
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfoGetter f);
NCBI_XSERIAL_EXPORT
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       const CTypeRef& r);
NCBI_XSERIAL_EXPORT
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f4,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfo t);
NCBI_XSERIAL_EXPORT
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f4,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfoGetter f);
NCBI_XSERIAL_EXPORT
CMemberInfo* AddMember(CClassTypeInfoBase* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f4,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       const CTypeRef& r);
// add variant
NCBI_XSERIAL_EXPORT
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfo t);
NCBI_XSERIAL_EXPORT
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter f);
NCBI_XSERIAL_EXPORT
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       const CTypeRef& r);
NCBI_XSERIAL_EXPORT
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f1,
                       TTypeInfo t);
NCBI_XSERIAL_EXPORT
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f1,
                       TTypeInfoGetter f);
NCBI_XSERIAL_EXPORT
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f1,
                       const CTypeRef& r);
NCBI_XSERIAL_EXPORT
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfo t);
NCBI_XSERIAL_EXPORT
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfoGetter f);
NCBI_XSERIAL_EXPORT
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       const CTypeRef& r);
NCBI_XSERIAL_EXPORT
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfo t);
NCBI_XSERIAL_EXPORT
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfoGetter f);
NCBI_XSERIAL_EXPORT
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       const CTypeRef& r);
NCBI_XSERIAL_EXPORT
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f4,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfo t);
NCBI_XSERIAL_EXPORT
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f4,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       TTypeInfoGetter f);
NCBI_XSERIAL_EXPORT
CVariantInfo* AddVariant(CChoiceTypeInfo* info, const char* name,
                       const void* member,
                       TTypeInfoGetter1 f4,
                       TTypeInfoGetter1 f3,
                       TTypeInfoGetter1 f2,
                       TTypeInfoGetter1 f1,
                       const CTypeRef& r);
// end of internal methods

END_NCBI_SCOPE


/* @} */


#endif
