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
#include <serial/asntypes.hpp>

struct valnode;

BEGIN_NCBI_SCOPE

class CObjectIStream;
class CObjectOStream;
class CMemberInfo;

// define type info getter for standard classes
template<typename T>
inline CTypeRef GetStdTypeRef(const T* object);
inline CTypeRef GetTypeRef(const void* object);
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
    return CTypeRef(CPointerTypeInfo::GetTypeInfo, GetTypeRef(p));
}

// define type info getter for standard classes
template<typename T>
inline
CTypeRef GetStdTypeRef(const T* )
{
    return CTypeRef(CStdTypeInfo<T>::GetTypeInfo);
}

// STL
template<typename Data>
inline
CTypeRef GetStlTypeRef(const list<Data>* )
{
    return CTypeRef(CStlClassInfoList<Data>::GetTypeInfo);
}

inline
CTypeRef GetStlTypeRef(const vector<char>* )
{
    return CTypeRef(CStlClassInfoCharVector::GetTypeInfo);
}

template<typename Data>
inline
CTypeRef GetStlTypeRef(const vector<Data>* )
{
    return CTypeRef(CStlClassInfoVector<Data>::GetTypeInfo);
}

template<typename Key, typename Value>
inline
CTypeRef GetStlTypeRef(const map<Key, Value>* )
{
    return CTypeRef(CStlClassInfoMap<Key, Value>::GetTypeInfo);
}

// ASN
inline
CTypeRef GetOctetStringTypeRef(const void* const* )
{
    return CTypeRef(COctetStringTypeInfo::GetTypeInfo);
}

template<typename T>
inline
CTypeRef GetSetTypeRef(const T* const* )
{
    const T* p = 0;
    return CTypeRef(CSetTypeInfo::GetTypeInfo, GetTypeRef(p));
}

template<typename T>
inline
CTypeRef GetSequenceTypeRef(const T* const* )
{
    const T* p = 0;
    return CTypeRef(CSequenceTypeInfo::GetTypeInfo, GetTypeRef(p));
}

template<typename T>
inline
CTypeRef GetSetOfTypeRef(const T* const* )
{
    const T* p = 0;
    return CTypeRef(CSetOfTypeInfo::GetTypeInfo, GetTypeRef(p));
}

template<typename T>
inline
CTypeRef GetSequenceOfTypeRef(const T* const* )
{
    const T* p = 0;
    return CTypeRef(CSequenceOfTypeInfo::GetTypeInfo, GetTypeRef(p));
}

inline
CTypeRef GetChoiceTypeRef(TTypeInfo (*func)(void))
{
    return CTypeRef(CChoiceTypeInfo::GetTypeInfo, CTypeRef(func));
}

template<typename T>
inline
CTypeRef GetOldAsnTypeRef(T* (*newProc)(void), T* (*freeProc)(T*),
                          T* (*readProc)(asnio*, asntype*),
                          unsigned char (*writeProc)(T*, asnio*, asntype*))
{
    return CTypeRef(COldAsnTypeInfo::GetTypeInfo(
        reinterpret_cast<COldAsnTypeInfo::TNewProc>(newProc),
        reinterpret_cast<COldAsnTypeInfo::TFreeProc>(freeProc),
        reinterpret_cast<COldAsnTypeInfo::TReadProc>(readProc),
        reinterpret_cast<COldAsnTypeInfo::TWriteProc>(writeProc)));
}

//
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

template<typename Key, typename Value>
inline
CTypeRef GetTypeRef(const map<Key, Value>* object)
{
    return GetStlTypeRef(object);
}

// define type info getter for user classes
template<class Class>
inline
CTypeRef GetTypeRef(const Class* )
{
    return CTypeRef(Class::GetTypeInfo);
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
CMemberInfo* OldAsnMemberInfo(const T* const* member,
                              T* (*newProc)(void), T* (*freeProc)(T*),
                              T* (*readProc)(asnio*, asntype*),
                              unsigned char (*writeProc)(T*, asnio*, asntype*))
{
    return MemberInfo(member, GetOldAsnTypeRef(newProc, freeProc,
                                               readProc, writeProc));
}

// type info declaration
#define NAME2(n1, n2) n1##n2
#define NAME3(n1, n2, n3) n1##n2##n3

#define ASN_TYPE_INFO_GETTER_NAME(name) NAME2(GetTypeInfo_struct_, name)
#define ASN_STRUCT_NAME(name) NAME2(struct_, name)

#define ASN_TYPE_INFO_GETTER_DECL(name) \
BEGIN_NCBI_SCOPE \
const CTypeInfo* ASN_TYPE_INFO_GETTER_NAME(name)(void); \
END_NCBI_SCOPE

#define ASN_TYPE_REF(name) \
struct ASN_STRUCT_NAME(name); \
ASN_TYPE_INFO_GETTER_DECL(name) \
BEGIN_NCBI_SCOPE \
inline CTypeRef GetTypeRef(const ASN_STRUCT_NAME(name)* ) \
{ return CTypeRef(ASN_TYPE_INFO_GETTER_NAME(name)); } \
END_NCBI_SCOPE

#define ASN_CHOICE_REF(name) \
ASN_TYPE_INFO_GETTER_DECL(name)

// type info definition
#define BEGIN_TYPE_INFO(Class, Method, Info, Args) \
const CTypeInfo* Method(void) \
{ \
    typedef Class CClass; \
    static Info* info = 0; \
    if ( info == 0 ) { \
        info = new Info Args;

#define END_TYPE_INFO \
    } \
    return info; \
}
 
#define BEGIN_CLASS_INFO(Class) \
BEGIN_TYPE_INFO(Class, Class::GetTypeInfo, CClassInfo<CClass>, ())
#define END_CLASS_INFO END_TYPE_INFO

#define BEGIN_DERIVED_CLASS_INFO(Class, BaseClass) \
BEGIN_TYPE_INFO(Class, Class::GetTypeInfo, CClassInfo<CClass>, ()) \
    info->AddMember(NcbiEmptyString, \
                    MemberInfo(static_cast<const BaseClass*> \
                               (static_cast<const CClass*>(0))));
#define END_DERIVED_CLASS_INFO END_TYPE_INFO

#define BEGIN_STRUCT_INFO2(Name, Class) \
BEGIN_TYPE_INFO(NAME2(struct_, Class), NAME2(GetTypeInfo_struct_, Class), \
                CStructInfo<CClass>, (Name))
#define BEGIN_STRUCT_INFO(Class) BEGIN_STRUCT_INFO2(#Class, Class)
#define END_STRUCT_INFO END_TYPE_INFO

#define BEGIN_CHOICE_INFO2(Name, Class) \
BEGIN_TYPE_INFO(valnode, NAME2(GetTypeInfo_struct_, Class), \
                CChoiceValNodeInfo, (Name))
#define BEGIN_CHOICE_INFO(Class) BEGIN_CHOICE_INFO2(#Class, Class)
#define END_CHOICE_INFO END_TYPE_INFO

// adding members
#define MEMBER(Member, Type) \
    MemberInfo(&static_cast<const CClass*>(0)->Member, Type)
#define ADD_MEMBER2(Name, Member, Type) \
    info->AddMember(Name, MEMBER(Member, Type))
#define ADD_MEMBER(Member, Type) ADD_MEMBER2(#Member, Member, Type)

#define CLASS_MEMBER(Member) \
	MemberInfo(&static_cast<const CClass*>(0)->Member)
#define ADD_CLASS_MEMBER2(Name, Member) \
	info->AddMember(Name, CLASS_MEMBER(Member))
#define ADD_CLASS_MEMBER(Member) ADD_CLASS_MEMBER2(#Member, Member)

#define PTR_CLASS_MEMBER(Member) \
	PtrMemberInfo(&static_cast<const CClass*>(0)->Member)
#define ADD_PTR_CLASS_MEMBER2(Name, Member) \
	info->AddMember(Name, PTR_CLASS_MEMBER(Member))
#define ADD_PTR_CLASS_MEMBER(Member) ADD_PTR_CLASS_MEMBER2(#Member, Member)

#define STL_CLASS_MEMBER(Member) \
	StlMemberInfo(&static_cast<const CClass*>(0)->Member)
#define ADD_STL_CLASS_MEMBER2(Name, Member) \
	info->AddMember(Name, STL_CLASS_MEMBER(Member))
#define ADD_STL_CLASS_MEMBER(Member) ADD_STL_CLASS_MEMBER2(#Member, Member)

#define ASN_MEMBER(Member, Type) \
	NAME2(Type, MemberInfo)(&static_cast<const CClass*>(0)->Member)
#define ADD_ASN_MEMBER2(Name, Member, Type) \
	info->AddMember(Name, ASN_MEMBER(Member, Type))
#define ADD_ASN_MEMBER(Member, Type) ADD_ASN_MEMBER2(#Member, Member, Type)

#define OLD_ASN_MEMBER(Member, Type) \
    OldAsnMemberInfo(&static_cast<const CClass*>(0)->Member, \
                     NAME2(Type, New), NAME2(Type, Free), \
                     NAME2(Type, AsnRead), NAME2(Type, AsnWrite))
#define ADD_OLD_ASN_MEMBER2(Name, Member, Type) \
	info->AddMember(Name, OLD_ASN_MEMBER(Member, Type))
#define ADD_OLD_ASN_MEMBER(Member, Type) \
    ADD_OLD_ASN_MEMBER2(#Member, Member, Type)

#define CHOICE_MEMBER(Member, Choices) \
    ChoiceMemberInfo(&static_cast<const CClass*>(0)->Member, \
                     NAME2(GetTypeInfo_struct_, Choices))
#define ADD_CHOICE_MEMBER2(Name, Member, Choices) \
    info->AddMember(Name, CHOICE_MEMBER(Member, Choices))
#define ADD_CHOICE_MEMBER(Member, Choices) \
    ADD_CHOICE_MEMBER2(#Member, Member, Choices)

#define ADD_CHOICE_STD_VARIANT(Name, Member) \
    info->AddVariant(#Name, GetTypeRef(&static_cast<const valnode*>(0)->data.NAME2(Member, value)))
#define ADD_CHOICE_VARIANT(Name, Type, Struct) \
    info->AddVariant(#Name, NAME3(Get, Type, TypeRef)(reinterpret_cast<const NAME2(struct_, Struct)* const*>(&static_cast<const valnode*>(0)->data.ptrvalue)))

#define ADD_SUB_CLASS2(Name, Class) \
    AddSubClass(CMemberId(Name), GetTypeRef(static_cast<const Class*>(0)))
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
