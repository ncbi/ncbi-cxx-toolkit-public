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

#include <serial/typeinfo.hpp>
#include <serial/member.hpp>
#include <serial/serialasn.hpp>
#include <serial/stdtypes.hpp>
#include <serial/stltypes.hpp>
#include <serial/ptrinfo.hpp>
#include <serial/autoptrinfo.hpp>
#include <serial/asntypes.hpp>
#include <serial/classinfo.hpp>
#include <serial/enumerated.hpp>

struct valnode;

BEGIN_NCBI_SCOPE

class CMemberInfo;

//
// define type info getter for standard classes
template<typename T>
inline
CTypeRef GetStdTypeRef(const T* )
{
    return &CStdTypeInfo<T>::GetTypeInfo;
}

inline
CTypeRef GetStdTypeRef(char* const* )
{
    return &CStdTypeInfo<char*>::GetTypeInfo;
}

inline
CTypeRef GetStdTypeRef(const char* const* )
{
    return &CStdTypeInfo<const char*>::GetTypeInfo;
}

inline
CTypeRef GetTypeRef(const bool* object)
{
    return GetStdTypeRef(object);
}

inline
CTypeRef GetTypeRef(const char* object)
{
    return GetStdTypeRef(object);
}

inline
CTypeRef GetTypeRef(const unsigned char* object)
{
    return GetStdTypeRef(object);
}

inline
CTypeRef GetTypeRef(const signed char* object)
{
    return GetStdTypeRef(object);
}

inline
CTypeRef GetTypeRef(const short* object)
{
    return GetStdTypeRef(object);
}

inline
CTypeRef GetTypeRef(const unsigned short* object)
{
    return GetStdTypeRef(object);
}

inline
CTypeRef GetTypeRef(const int* object)
{
    return GetStdTypeRef(object);
}

inline
CTypeRef GetTypeRef(const unsigned int* object)
{
    return GetStdTypeRef(object);
}

inline
CTypeRef GetTypeRef(const long* object)
{
    return GetStdTypeRef(object);
}

inline
CTypeRef GetTypeRef(const unsigned long* object)
{
    return GetStdTypeRef(object);
}

inline
CTypeRef GetTypeRef(const float* object)
{
    return GetStdTypeRef(object);
}

inline
CTypeRef GetTypeRef(const double* object)
{
    return GetStdTypeRef(object);
}

inline
CTypeRef GetTypeRef(const string* object)
{
    return GetStdTypeRef(object);
}

inline
CTypeRef GetTypeRef(char* const* object)
{
    return GetStdTypeRef(object);
}

inline
CTypeRef GetTypeRef(const char* const* object)
{
    return GetStdTypeRef(object);
}

#if 0
// STL
template<typename Data>
inline
CTypeRef GetStlTypeRef(const list<Data>* )
{
    return &CStlClassInfoList<Data>::GetTypeInfo;
}

inline
CTypeRef GetStlTypeRef(const vector<char>* )
{
    return &CStlClassInfoCharVector<char>::GetTypeInfo;
}

inline
CTypeRef GetStlTypeRef(const vector<unsigned char>* )
{
    return &CStlClassInfoCharVector<unsigned char>::GetTypeInfo;
}

inline
CTypeRef GetStlTypeRef(const vector<signed char>* )
{
    return &CStlClassInfoCharVector<signed char>::GetTypeInfo;
}

template<typename Data>
inline
CTypeRef GetStlTypeRef(const vector<Data>* )
{
    return &CStlClassInfoVector<Data>::GetTypeInfo;
}

template<typename Data>
inline
CTypeRef GetStlTypeRef(const set<Data>* )
{
    return &CStlClassInfoSet<Data>::GetTypeInfo;
}

template<typename Key, typename Value>
inline
CTypeRef GetStlTypeRef(const map<Key, Value>* )
{
    return &CStlClassInfoMap<Key, Value>::GetTypeInfo;
}

template<typename Key, typename Value>
inline
CTypeRef GetStlTypeRef(const multimap<Key, Value>* )
{
    return &CStlClassInfoMultiMap<Key, Value>::GetTypeInfo;
}

template<typename Data>
inline
CTypeRef GetTypeRef(const list<Data>* object)
{
    return GetStlTypeRef(object);
}

inline
CTypeRef GetTypeRef(const vector<char>* object)
{
    return GetStlTypeRef(object);
}

template<typename Data>
inline
CTypeRef GetTypeRef(const vector<Data>* object)
{
    return GetStlTypeRef(object);
}

template<typename Data>
inline
CTypeRef GetTypeRef(const set<Data>* object)
{
    return GetStlTypeRef(object);
}

template<typename Key, typename Value>
inline
CTypeRef GetTypeRef(const map<Key, Value>* object)
{
    return GetStlTypeRef(object);
}

template<typename Key, typename Value>
inline
CTypeRef GetTypeRef(const multimap<Key, Value>* object)
{
    return GetStlTypeRef(object);
}
#endif

#define SERIAL_NAME2(n1, n2) n1##n2
#define SERIAL_NAME3(n1, n2, n3) n1##n2##n3

#define SERIAL_TYPE(Name) SERIAL_NAME2(SERIAL_TYPE_,Name)
#define SERIAL_REF(Name) SERIAL_NAME2(SERIAL_REF_,Name)

#define SERIAL_TYPE_CLASS(Name) Name
#define SERIAL_REF_CLASS(Name) &Name::GetTypeInfo

#define SERIAL_TYPE_STD(Name) Name
#define SERIAL_REF_STD(Name) &NCBI_NS_NCBI::CStdTypeInfo<Name>::GetTypeInfo

#define SERIAL_TYPE_ENUM(Type, Name) Type
#define SERIAL_REF_ENUM(Type, Name) \
    NCBI_NS_NCBI::CreateEnumeratedTypeInfo(Type(0), SERIAL_NAME2(GetEnumInfo_,Name)())

#define SERIAL_TYPE_POINTER(Type,Args) SERIAL_TYPE(Type)Args*
#define SERIAL_REF_POINTER(Type,Args) \
    NCBI_NS_NCBI::CTypeRef(&NCBI_NS_NCBI::CPointerTypeInfo::GetTypeInfo,SERIAL_REF(Type)Args)

#define SERIAL_TYPE_STL_multiset(Type,Args) \
    NCBI_NS_STD::multiset<SERIAL_TYPE(Type)Args>
#define SERIAL_REF_STL_multiset(Type,Args) \
    new NCBI_NS_NCBI::CStlClassInfoMultiSet<SERIAL_TYPE(Type)Args>(SERIAL_REF(Type)Args)

#define SERIAL_TYPE_STL_set(Type,Args) NCBI_NS_STD::set<SERIAL_TYPE(Type)Args>
#define SERIAL_REF_STL_set(Type,Args) \
    new NCBI_NS_NCBI::CStlClassInfoSet<SERIAL_TYPE(Type)Args>(SERIAL_REF(Type)Args)

#define SERIAL_TYPE_STL_multimap(KeyType,KeyArgs,ValueType,ValueArgs) \
    NCBI_NS_STD::multimap<SERIAL_TYPE(KeyType)KeyArgs,SERIAL_TYPE(ValueType)ValueArgs>
#define SERIAL_REF_STL_multimap(KeyType,KeyArgs,ValueType,ValueArgs) \
    new NCBI_NS_NCBI::CStlClassInfoMultiMap<SERIAL_TYPE(KeyType)KeyArgs,SERIAL_TYPE(ValueType)ValueArgs>(SERIAL_REF(KeyType)KeyArgs,SERIAL_REF(ValueType)ValueArgs)

#define SERIAL_TYPE_STL_map(KeyType,KeyArgs,ValueType,ValueArgs) \
    NCBI_NS_STD::map<SERIAL_TYPE(KeyType)KeyArgs,SERIAL_TYPE(ValueType)ValueArgs>
#define SERIAL_REF_STL_map(KeyType,KeyArgs,ValueType,ValueArgs) \
    new NCBI_NS_NCBI::CStlClassInfoMap<SERIAL_TYPE(KeyType)KeyArgs,SERIAL_TYPE(ValueType)ValueArgs>(SERIAL_REF(KeyType)KeyArgs,SERIAL_REF(ValueType)ValueArgs)

#define SERIAL_TYPE_STL_list(Type,Args) NCBI_NS_STD::list<SERIAL_TYPE(Type)Args>
#define SERIAL_REF_STL_list(Type,Args) \
    new NCBI_NS_NCBI::CStlClassInfoList<SERIAL_TYPE(Type)Args>(SERIAL_REF(Type)Args)

#define SERIAL_TYPE_STL_vector(Type,Args) NCBI_NS_STD::vector<SERIAL_TYPE(Type)Args>
#define SERIAL_REF_STL_vector(Type,Args) \
    new NCBI_NS_NCBI::CStlClassInfoVector<SERIAL_TYPE(Type)Args>(SERIAL_REF(Type)Args)

#define SERIAL_TYPE_STL_CHAR_vector(Type) NCBI_NS_STD::vector<Type>
#define SERIAL_REF_STL_CHAR_vector(Type) \
    &NCBI_NS_NCBI::CStlClassInfoCharVector<Type>::GetTypeInfo

#define SERIAL_TYPE_STL_auto_ptr(Type,Args) NCBI_NS_STD::auto_ptr<SERIAL_TYPE(Type)Args>
#define SERIAL_REF_STL_auto_ptr(Type,Args) \
    &NCBI_NS_NCBI::CStlClassInfoAutoPtr<SERIAL_TYPE(Type)Args>::GetTypeInfo

#define SERIAL_TYPE_STL_AutoPtr(Type,Args) NCBI_NS_NCBI::AutoPtr<SERIAL_TYPE(Type)Args>
#define SERIAL_REF_STL_AutoPtr(Type,Args) \
    &NCBI_NS_NCBI::CAutoPtrTypeInfo<SERIAL_TYPE(Type)Args>::GetTypeInfo

#define SERIAL_REF_CHOICE_POINTER(Type,Args) \
    NCBI_NS_NCBI::CTypeRef(&NCBI_NS_NCBI::CChoicePointerTypeInfo::GetTypeInfo,SERIAL_REF(Type)Args)

#define SERIAL_REF_CHOICE_STL_auto_ptr(Type,Args) \
    &NCBI_NS_NCBI::CChoiceStlClassInfoAutoPtr<SERIAL_TYPE(Type)Args>::GetTypeInfo

#define SERIAL_REF_CHOICE_STL_AutoPtr(Type,Args) \
    &NCBI_NS_NCBI::CChoiceAutoPtrTypeInfo<SERIAL_TYPE(Type)Args>::GetTypeInfo

#define SERIAL_TYPE_CHOICE(Type,Args) SERIAL_TYPE(Type)Args
#define SERIAL_REF_CHOICE(Type,Args) SERIAL_REF(SERIAL_NAME2(CHOICE_,Type))Args

template<typename T>
struct Check
{
    static CMemberInfo* Member(const T* member, const CTypeRef& type)
        {
            return new CRealMemberInfo(size_t(member), type);
        }
};

template<typename T>
inline
CMemberInfo* Member(const T* member)
{
    return new CRealMemberInfo(size_t(member), GetTypeRef(member));
}

template<typename T>
inline
CMemberInfo* EnumMember(const T* member, const CEnumeratedTypeValues* enumInfo)
{
    return new CRealMemberInfo(size_t(member),
                               CreateEnumeratedTypeInfo(*member, enumInfo));
}

#define BASE_OBJECT(Class) const Class* const kObject = 0
#define MEMBER_PTR(Name) &kObject->Name
#define CLASS_PTR(Class) static_cast<const Class*>(kObject)

#define M(Name,Type,Args) \
    NCBI_NS_NCBI::Check<SERIAL_TYPE(Type)Args>::Member(MEMBER_PTR(Name),SERIAL_REF(Type)Args)
#define STD_M(Name) NCBI_NS_NCBI::Member(MEMBER_PTR(Name))
#define ENUM_M(Name,Type) \
    NCBI_NS_NCBI::EnumMember(MEMBER_PTR(Name), SERIAL_NAME2(GetEnumInfo_, Type)())
    
#define ADD_N_M(Name,Mem,Type,Args) info->AddMember(Name,M(Mem,Type,Args))
#define ADD_N_STD_M(Name,Mem) info->AddMember(Name,STD_M(Mem))
#define ADD_N_ENUM_M(Name,Mem,Type) info->AddMember(Name,ENUM_M(Mem,Type))
#define ADD_M(Name,Type,Args) ADD_N_M(#Name,Name,Type,Args)
#define ADD_STD_M(Name) ADD_N_STD_M(#Name,Name)
#define ADD_ENUM_M(Name,Type) ADD_N_ENUM_M(#Name,Name,Type)

// member types
template<typename T>
inline
CMemberInfo* MemberInfo(const T* member, const CTypeRef typeRef)
{
	return new CRealMemberInfo(size_t(member), typeRef);
}

template<typename T>
inline
CMemberInfo* MemberInfo(const T* member)
{
	return MemberInfo(member, GetTypeRef(member));
}

template<typename T>
inline
CMemberInfo* PtrMemberInfo(const T* member, CTypeRef typeRef)
{
	return MemberInfo(member, GetPtrTypeRef(member, typeRef));
}

template<typename T>
inline
CMemberInfo* PtrMemberInfo(const T* const* member)
{
	return MemberInfo(member, GetPtrTypeRef(member));
}

template<typename T>
inline
CMemberInfo* StlMemberInfo(const T* member)
{
	return MemberInfo(member, GetStlTypeRef(member));
}

#if HAVE_NCBI_C
template<typename T>
inline
CMemberInfo* SetMemberInfo(const T* const* member)
{
	return MemberInfo(member, GetSetTypeRef(member));
}

template<typename T>
inline
CMemberInfo* SequenceMemberInfo(const T* const* member)
{
	return MemberInfo(member, GetSequenceTypeRef(member));
}

template<typename T>
inline
CMemberInfo* SetOfMemberInfo(const T* const* member)
{
	return MemberInfo(member, GetSetOfTypeRef(member));
}

template<typename T>
inline
CMemberInfo* SequenceOfMemberInfo(const T* const* member)
{
	return MemberInfo(member, GetSequenceOfTypeRef(member));
}

inline
CMemberInfo* OctetStringMemberInfo(const void* const* member)
{
	return MemberInfo(member, GetOctetStringTypeRef(member));
}

inline
CMemberInfo* ChoiceMemberInfo(const valnode* const* member,
                              TTypeInfo (*func)(void))
{
	return MemberInfo(member, GetChoiceTypeRef(func));
}

template<typename T>
inline
CMemberInfo* OldAsnMemberInfo(const T* const* member, const string& name,
                              T* (ASNCALL*newProc)(void),
							  T* (ASNCALL*freeProc)(T*),
                              T* (ASNCALL*readProc)(asnio*, asntype*),
                              unsigned char (ASNCALL*writeProc)(T*, asnio*, asntype*))
{
    return MemberInfo(member,
                      GetOldAsnTypeRef(name, newProc, freeProc,
                                       readProc, writeProc));
}
#endif

// type info definition
#define BEGIN_TYPE_INFO(Class, Method, Info, Args) \
const NCBI_NS_NCBI::CTypeInfo* Method(void) \
{ \
    typedef Class CClass; \
    static Info* info = 0; \
    if ( info == 0 ) { \
        BASE_OBJECT(CClass); \
        info = new Info Args;

#define END_TYPE_INFO \
    } \
    return info; \
}
 
#define BEGIN_CLASS_INFO3(Name, Class, OwnerClass) \
BEGIN_TYPE_INFO(Class, OwnerClass::GetTypeInfo, NCBI_NS_NCBI::CClassInfo<CClass>, (Name))
#define BEGIN_CLASS_INFO2(Name, Class) \
BEGIN_CLASS_INFO3(Name, Class, Class)
#define BEGIN_CLASS_INFO(Class) \
BEGIN_TYPE_INFO(Class, Class::GetTypeInfo, NCBI_NS_NCBI::CClassInfo<CClass>, ())

#define END_CLASS_INFO END_TYPE_INFO

#define BEGIN_ABSTRACT_CLASS_INFO3(Name, Class, OwnerClass) \
BEGIN_TYPE_INFO(Class, OwnerClass::GetTypeInfo, NCBI_NS_NCBI::CAbstractClassInfo<CClass>, (Name))
#define BEGIN_ABSTRACT_CLASS_INFO2(Name, Class) \
BEGIN_ABSTRACT_CLASS_INFO3(Name, Class, Class)
#define BEGIN_ABSTRACT_CLASS_INFO(Class) \
BEGIN_TYPE_INFO(Class, Class::GetTypeInfo, NCBI_NS_NCBI::CAbstractClassInfo<CClass>, ())

#define SET_PARENT_CLASS(BaseClass) \
    info->AddMember(NCBI_NS_NCBI::NcbiEmptyString, NCBI_NS_NCBI::MemberInfo(CLASS_PTR(BaseClass)))

#define BEGIN_DERIVED_CLASS_INFO(Class, BaseClass) \
BEGIN_TYPE_INFO(Class, Class::GetTypeInfo, NCBI_NS_NCBI::CClassInfo<CClass>, ()) \
    SET_PARENT_CLASS(BaseClass);
#define END_DERIVED_CLASS_INFO END_TYPE_INFO

#define BEGIN_DERIVED_CLASS_INFO2(Name, Class, BaseClass) \
BEGIN_TYPE_INFO(Class, Class::GetTypeInfo, NCBI_NS_NCBI::CClassInfo<CClass>, (Name)) \
    SET_PARENT_CLASS(BaseClass);
#define END_DERIVED_CLASS_INFO END_TYPE_INFO

#define BEGIN_STRUCT_INFO2(Name, Class) \
BEGIN_TYPE_INFO(SERIAL_NAME2(struct_, Class), SERIAL_NAME2(GetTypeInfo_struct_, Class), \
                NCBI_NS_NCBI::CStructInfo<CClass>, (Name))
#define BEGIN_STRUCT_INFO(Class) BEGIN_STRUCT_INFO2(#Class, Class)
#define END_STRUCT_INFO END_TYPE_INFO

#define BEGIN_CHOICE_INFO2(Name, Class) \
BEGIN_TYPE_INFO(valnode, SERIAL_NAME2(GetTypeInfo_struct_, Class), \
                NCBI_NS_NCBI::CChoiceTypeInfo, (Name))
#define BEGIN_CHOICE_INFO(Class) BEGIN_CHOICE_INFO2(#Class, Class)
#define END_CHOICE_INFO END_TYPE_INFO

#define BEGIN_ENUM_INFO(Method, Enum, IsInteger) \
const NCBI_NS_NCBI::CEnumeratedTypeValues* Method(void) \
{ static NCBI_NS_NCBI::CEnumeratedTypeValues* enumInfo = 0; if ( !enumInfo ) { \
    enumInfo = new NCBI_NS_NCBI::CEnumeratedTypeValues(#Enum, IsInteger); \
    Enum enumValue;

#define ADD_ENUM_VALUE(Name, Value) enumInfo->AddValue(Name, enumValue = Value)

#define END_ENUM_INFO } return enumInfo; }

// adding members
#define MEMBER(Member, Type) \
    NCBI_NS_NCBI::MemberInfo(MEMBER_PTR(Member), Type)
#define ADD_MEMBER2(Name, Member, Type) \
    info->AddMember(Name, MEMBER(Member, Type))
#define ADD_MEMBER(Member, Type) ADD_MEMBER2(#Member, Member, Type)

#define CLASS_MEMBER(Member) \
	NCBI_NS_NCBI::MemberInfo(MEMBER_PTR(Member))
#define ADD_CLASS_MEMBER2(Name, Member) \
	info->AddMember(Name, CLASS_MEMBER(Member))
#define ADD_CLASS_MEMBER(Member) ADD_CLASS_MEMBER2(#Member, Member)

#define PTR_CLASS_MEMBER(Member) \
	NCBI_NS_NCBI::PtrMemberInfo(MEMBER_PTR(Member))
#define ADD_PTR_CLASS_MEMBER2(Name, Member) \
	info->AddMember(Name, PTR_CLASS_MEMBER(Member))
#define ADD_PTR_CLASS_MEMBER(Member) ADD_PTR_CLASS_MEMBER2(#Member, Member)

#define STL_CLASS_MEMBER(Member) \
	NCBI_NS_NCBI::StlMemberInfo(MEMBER_PTR(Member))
#define ADD_STL_CLASS_MEMBER2(Name, Member) \
	info->AddMember(Name, STL_CLASS_MEMBER(Member))
#define ADD_STL_CLASS_MEMBER(Member) ADD_STL_CLASS_MEMBER2(#Member, Member)

#define ASN_MEMBER(Member, Type) \
	SERIAL_NAME2(Type, MemberInfo)(MEMBER_PTR(Member))
#define ADD_ASN_MEMBER2(Name, Member, Type) \
	info->AddMember(Name, ASN_MEMBER(Member, Type))
#define ADD_ASN_MEMBER(Member, Type) ADD_ASN_MEMBER2(#Member, Member, Type)

#define OLD_ASN_MEMBER(Member, Name, Type) \
    NCBI_NS_NCBI::OldAsnMemberInfo(MEMBER_PTR(Member), Name, \
                     &SERIAL_NAME2(Type, New), &SERIAL_NAME2(Type, Free), \
                     &SERIAL_NAME2(Type, AsnRead), &SERIAL_NAME2(Type, AsnWrite))
#define ADD_OLD_ASN_MEMBER2(Name, Member, TypeName, Type) \
	info->AddMember(Name, OLD_ASN_MEMBER(Member, TypeName, Type))
#define ADD_OLD_ASN_MEMBER(Member, Type) \
    ADD_OLD_ASN_MEMBER2(#Member, Member, #Type, Type)

#define CHOICE_MEMBER(Member, Choices) \
    NCBI_NS_NCBI::ChoiceMemberInfo(MEMBER_PTR(Member), \
                     SERIAL_NAME2(GetTypeInfo_struct_, Choices))
#define ADD_CHOICE_MEMBER2(Name, Member, Choices) \
    info->AddMember(Name, CHOICE_MEMBER(Member, Choices))
#define ADD_CHOICE_MEMBER(Member, Choices) \
    ADD_CHOICE_MEMBER2(#Member, Member, Choices)

#define ADD_CHOICE_STD_VARIANT(Name, Member) \
    info->AddVariant(#Name, GetTypeRef(MEMBER_PTR(data.SERIAL_NAME2(Member, value))))
#define ADD_CHOICE_VARIANT(Name, Type, Struct) \
    info->AddVariant(#Name, SERIAL_NAME3(Get, Type, TypeRef)(reinterpret_cast<const SERIAL_NAME2(struct_, Struct)* const*>(MEMBER_PTR(data.ptrvalue))))

#define ADD_SUB_CLASS2(Name, SubClass) \
    info->AddSubClass(NCBI_NS_NCBI::CMemberId(Name), NCBI_NS_NCBI::GetTypeRef(CLASS_PTR(SubClass)))
#define ADD_SUB_CLASS(Class) \
    ADD_SUB_CLASS2(#Class, Class)

END_NCBI_SCOPE

#endif
