#ifndef SERIAL__HPP
#define SERIAL__HPP

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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.37  1999/10/25 19:07:13  vasilche
* Fixed coredump on non initialized choices.
* Fixed compilation warning.
*
* Revision 1.36  1999/10/19 13:21:59  vasilche
* Avoid macros names conflict.
*
* Revision 1.35  1999/10/18 20:11:16  vasilche
* Enum values now have long type.
* Fixed template generation for enums.
*
* Revision 1.34  1999/10/15 16:37:47  vasilche
* Added namespace specificators.
*
* Revision 1.33  1999/10/15 13:58:43  vasilche
* Added namespace specificators in defenition of all macros.
*
* Revision 1.32  1999/10/04 19:39:45  vasilche
* Fixed bug in CObjectOStreamBinary.
* Start using of BSRead/BSWrite.
* Added ASNCALL macro for prototypes of old ASN.1 functions.
*
* Revision 1.31  1999/10/04 16:22:10  vasilche
* Fixed bug with old ASN.1 structures.
*
* Revision 1.30  1999/09/24 19:01:17  vasilche
* Removed dependency on NCBI toolkit.
*
* Revision 1.29  1999/09/14 18:54:05  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.28  1999/09/08 20:31:18  vasilche
* Added BEGIN_CLASS_INFO3 macro
*
* Revision 1.27  1999/09/07 20:57:44  vasilche
* Forgot to add some files.
*
* Revision 1.26  1999/09/01 17:38:02  vasilche
* Fixed vector<char> implementation.
* Added explicit naming of class info.
* Moved IMPLICIT attribute from member info to class info.
*
* Revision 1.25  1999/08/31 17:50:04  vasilche
* Implemented several macros for specific data types.
* Added implicit members.
* Added multimap and set.
*
* Revision 1.24  1999/08/13 15:53:45  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.23  1999/07/21 14:20:01  vasilche
* Added serialization of bool.
*
* Revision 1.22  1999/07/20 18:22:56  vasilche
* Added interface to old ASN.1 routines.
* Added fixed choice of subclasses to use for pointers.
*
* Revision 1.21  1999/07/19 15:50:19  vasilche
* Added interface to old ASN.1 routines.
* Added naming of key/value in STL map.
*
* Revision 1.20  1999/07/15 16:54:43  vasilche
* Implemented vector<X> & vector<char> as special case.
*
* Revision 1.19  1999/07/14 18:58:04  vasilche
* Fixed ASN.1 types/field naming.
*
* Revision 1.18  1999/07/13 20:54:05  vasilche
* Fixed minor bugs.
*
* Revision 1.17  1999/07/13 20:18:07  vasilche
* Changed types naming.
*
* Revision 1.16  1999/07/09 16:32:54  vasilche
* Added OCTET STRING write/read.
*
* Revision 1.15  1999/07/07 18:18:32  vasilche
* Fixed some bugs found by MS VC++
*
* Revision 1.14  1999/06/30 18:54:54  vasilche
* Fixed some errors under MSVS
*
* Revision 1.13  1999/06/30 16:04:35  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.12  1999/06/24 14:44:43  vasilche
* Added binary ASN.1 output.
*
* Revision 1.11  1999/06/17 18:38:49  vasilche
* Fixed order of members in class.
* Added checks for overlapped members.
*
* Revision 1.10  1999/06/16 20:58:04  vasilche
* Added GetPtrTypeRef to avoid conflict in MSVS.
*
* Revision 1.9  1999/06/15 16:20:06  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.8  1999/06/11 19:15:49  vasilche
* Working binary serialization and deserialization of first test object.
*
* Revision 1.7  1999/06/09 19:58:31  vasilche
* Added specialized templates for compilation in MS VS
*
* Revision 1.6  1999/06/09 18:38:58  vasilche
* Modified templates to work on Sun.
*
* Revision 1.5  1999/06/07 20:42:58  vasilche
* Fixed compilation under MS VS
*
* Revision 1.4  1999/06/07 19:30:20  vasilche
* More bug fixes
*
* Revision 1.3  1999/06/04 20:51:37  vasilche
* First compilable version of serialization.
*
* Revision 1.2  1999/05/19 19:56:28  vasilche
* Commit just in case.
*
* Revision 1.1  1999/03/25 19:11:58  vasilche
* Beginning of serialization library.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/serialdef.hpp>
#include <serial/typeinfo.hpp>
#include <serial/member.hpp>
#include <serial/stdtypes.hpp>
#include <serial/stltypes.hpp>
#include <serial/ptrinfo.hpp>
#include <serial/autoptrinfo.hpp>
#include <serial/asntypes.hpp>
#include <serial/classinfo.hpp>
#include <serial/enumerated.hpp>

struct valnode;

BEGIN_NCBI_SCOPE

class CObjectIStream;
class CObjectOStream;
class CMemberInfo;

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

#define SERIAL_TYPE_STL_CHOICE_auto_ptr(Type,Args) NCBI_NS_STD::auto_ptr<SERIAL_TYPE(Type)Args>
#define SERIAL_REF_STL_CHOICE_auto_ptr(Type,Args) \
    &NCBI_NS_NCBI::CStlClassInfoChoiceAutoPtr<SERIAL_TYPE(Type)Args>::GetTypeInfo

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

// define type info getter for standard classes
template<typename T>
inline CTypeRef GetStdTypeRef(const T* object);
inline CTypeRef GetTypeRef(const void* object);
inline CTypeRef GetTypeRef(const bool* object);
inline CTypeRef GetTypeRef(const char* object);
inline CTypeRef GetTypeRef(const unsigned char* object);
inline CTypeRef GetTypeRef(const signed char* object);
inline CTypeRef GetTypeRef(const short* object);
inline CTypeRef GetTypeRef(const unsigned short* object);
inline CTypeRef GetTypeRef(const int* object);
inline CTypeRef GetTypeRef(const unsigned int* object);
inline CTypeRef GetTypeRef(const long* object);
inline CTypeRef GetTypeRef(const unsigned long* object);
inline CTypeRef GetTypeRef(const float* object);
inline CTypeRef GetTypeRef(const double* object);
inline CTypeRef GetTypeRef(const string* object);
template<typename T>
inline CTypeRef GetTypeRef(const T* const* object);
template<typename Data>
inline CTypeRef GetTypeRef(const list<Data>* object);
template<class Class>
inline CTypeRef GetTypeRef(const Class* object);




// define type info getter for pointers
template<typename T>
inline
CTypeRef GetPtrTypeRef(const T* const* object)
{
    const T* p = 0;
    return CTypeRef(&CPointerTypeInfo::GetTypeInfo, GetTypeRef(p));
}

// define type info getter for standard classes
template<typename T>
inline
CTypeRef GetStdTypeRef(const T* )
{
    return &CStdTypeInfo<T>::GetTypeInfo;
}

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

#if HAVE_NCBI_C
// ASN
inline
CTypeRef GetOctetStringTypeRef(const void* const* )
{
    return &COctetStringTypeInfo::GetTypeInfo;
}

template<typename T>
inline
CTypeRef GetSetTypeRef(const T* const* )
{
    const T* p = 0;
    return CTypeRef(&CAutoPointerTypeInfo::GetTypeInfo, GetTypeRef(p));
}

template<typename T>
inline
CTypeRef GetSequenceTypeRef(const T* const* )
{
    const T* p = 0;
    return CTypeRef(&CAutoPointerTypeInfo::GetTypeInfo, GetTypeRef(p));
}

template<typename T>
inline
CTypeRef GetSetOfTypeRef(const T* const* p)
{
    //    const T* p = 0;
    return CTypeRef(&CSetOfTypeInfo::GetTypeInfo, GetSetTypeRef(p));
}

template<typename T>
inline
CTypeRef GetSequenceOfTypeRef(const T* const* p)
{
    //    const T* p = 0;
    return CTypeRef(&CSequenceOfTypeInfo::GetTypeInfo, GetSetTypeRef(p));
}

inline
CTypeRef GetChoiceTypeRef(TTypeInfo (*func)(void))
{
    return CTypeRef(&CAutoPointerTypeInfo::GetTypeInfo, CTypeRef(func));
}

template<typename T>
inline
CTypeRef GetOldAsnTypeRef(const string& name,
                          T* (ASNCALL*newProc)(void),
						  T* (ASNCALL*freeProc)(T*),
                          T* (ASNCALL*readProc)(asnio*, asntype*),
                          unsigned char (ASNCALL*writeProc)(T*, asnio*, asntype*))
{
    return new COldAsnTypeInfo(name,
        reinterpret_cast<COldAsnTypeInfo::TNewProc>(newProc),
        reinterpret_cast<COldAsnTypeInfo::TFreeProc>(freeProc),
        reinterpret_cast<COldAsnTypeInfo::TReadProc>(readProc),
        reinterpret_cast<COldAsnTypeInfo::TWriteProc>(writeProc));
}
#endif

//
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

// define type info getter for user classes
template<class Class>
inline
CTypeRef GetTypeRef(const Class* )
{
    return &Class::GetTypeInfo;
}



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

// type info declaration
#define ASN_TYPE_INFO_GETTER_NAME(name) SERIAL_NAME2(GetTypeInfo_struct_, name)
#define ASN_STRUCT_NAME(name) SERIAL_NAME2(struct_, name)

#define ASN_TYPE_INFO_GETTER_DECL(name) \
BEGIN_NCBI_SCOPE \
const CTypeInfo* ASN_TYPE_INFO_GETTER_NAME(name)(void); \
END_NCBI_SCOPE

#define ASN_TYPE_REF(name) \
struct ASN_STRUCT_NAME(name); \
ASN_TYPE_INFO_GETTER_DECL(name) \
BEGIN_NCBI_SCOPE \
inline CTypeRef GetTypeRef(const ASN_STRUCT_NAME(name)* ) \
{ return ASN_TYPE_INFO_GETTER_NAME(name); } \
END_NCBI_SCOPE

#define ASN_CHOICE_REF(name) \
ASN_TYPE_INFO_GETTER_DECL(name)

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


// reader/writer
template<typename T>
inline
CObjectOStream& Write(CObjectOStream& out, const T& object)
{
    out.Write(&object, GetTypeRef(&object).Get());
    return out;
}

template<typename T>
inline
CObjectIStream& Read(CObjectIStream& in, T& object)
{
    in.Read(&object, GetTypeRef(&object).Get());
    return in;
}

template<typename T>
inline
CObjectOStream& operator<<(CObjectOStream& out, const T& object)
{
    return Write(out, object);
}

template<typename T>
inline
CObjectIStream& operator>>(CObjectIStream& in, T& object)
{
    return Read(in, object);
}

#include <serial/serial.inl>

END_NCBI_SCOPE

#endif  /* SERIAL__HPP */
