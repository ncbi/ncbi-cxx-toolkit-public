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
template<typename CLASS>
inline CTypeRef GetTypeRef(const CLASS* object);
template<typename Data>
inline CTypeRef GetTypeRef(const list<Data>* object);
template<typename T>
inline CTypeRef GetTypeRef(const T* const* object);




// define type info getter for standard classes
inline
CTypeRef GetTypeRef(const void*)
{
    return CTypeRef(typeid(void), CStdTypeInfo<void>::GetTypeInfo);
}

template<typename T>
inline
CTypeRef GetStdTypeRef(const T* )
{
    return CTypeRef(typeid(T), CStdTypeInfo<T>::GetTypeInfo);
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
CTypeRef GetTypeRef(unsigned char* const* object)
{
    return GetStdTypeRef(object);
}

inline
CTypeRef GetTypeRef(signed char* const* object)
{
    return GetStdTypeRef(object);
}

inline
CTypeRef GetTypeRef(const char* const* object)
{
    return GetStdTypeRef(object);
}

inline
CTypeRef GetTypeRef(const unsigned char* const* object)
{
    return GetStdTypeRef(object);
}

inline
CTypeRef GetTypeRef(const signed char* const* object)
{
    return GetStdTypeRef(object);
}

// define type info getter for user classes
template<typename CLASS>
inline
CTypeRef GetTypeRef(const CLASS* )
{
    return CTypeRef(typeid(CLASS), CLASS::GetTypeInfo);
}

template<typename Data>
inline
CTypeRef GetTypeRef(const list<Data>* )
{
    return CTypeRef(typeid(list<Data>),
                    CStlClassInfoList<Data>::GetTypeInfo);
}

template<typename Data>
inline
CTypeRef GetStlTypeRef(const list<Data>* )
{
    return CTypeRef(typeid(list<Data>),
                    CStlClassInfoList<Data>::GetTypeInfo);
}

// define type info getter for pointers
template<typename T>
inline
CTypeRef GetPtrTypeRef(const T* const* object, CTypeRef typeRef)
{
    typename T* p = 0;
    return CTypeRef(CTypeInfo::GetPointerTypeInfo(typeid(p), typeRef));
}

template<typename T>
inline
CTypeRef GetPtrTypeRef(const T* const* object)
{
    typename T* p = 0;
    return GetPtrTypeRef(object, GetTypeRef(p));
}

template<typename T>
inline
CTypeRef GetSetTypeRef(const T* const* )
{
    const T* object = 0;
    return CTypeRef(new CSetTypeInfo(GetTypeRef(object)));
}

template<typename T>
inline
CTypeRef GetSequenceTypeRef(const T* const* )
{
    const T* object = 0;
    return CTypeRef(new CSequenceTypeInfo(GetTypeRef(object)));
}

template<typename T>
inline
CTypeRef GetSetOfTypeRef(const T* const* )
{
    const T* object = 0;
    return CTypeRef(new CSetOfTypeInfo(GetTypeRef(object)));
}

template<typename T>
inline
CTypeRef GetSequenceOfTypeRef(const T* const* )
{
    const T* object = 0;
    return CTypeRef(new CSequenceOfTypeInfo(GetTypeRef(object)));
}

inline
CTypeRef GetChoiceTypeRef(TTypeInfo (*func)(void))
{
    return CTypeRef(new CChoiceTypeInfo(CTypeRef(typeid(void), func)));
}



template<typename Data>
inline
CStlClassInfoList<Data>::CStlClassInfoList(void)
    : CParent(typeid(TObjectType), GetTypeRef(static_cast<const Data*>(0)))
{
}



inline
TTypeInfo GetTypeInfo(void)
{
    return GetTypeRef(static_cast<const void*>(0)).Get();
}

template<typename CLASS>
inline
TTypeInfo GetTypeInfo(const CLASS& object)
{
    return GetTypeRef(&object).Get();
}




template<class CLASS>
inline
CObjectIStream& Read(CObjectIStream& in, CLASS& object)
{
    in.Read(&object, GetTypeInfo(object));
    return in;
}

template<class CLASS>
inline
CObjectOStream& Write(CObjectOStream& out, const CLASS& object)
{
    out.Write(&object, GetTypeInfo(object));
    return out;
}

template<class CLASS>
inline
CObjectOStream& operator<<(CObjectOStream& out, const CLASS& object)
{
    return Write(out, object);
}

template<class CLASS>
inline
CObjectIStream& operator>>(CObjectIStream& in, CLASS& object)
{
    return Read(in, object);
}


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
CMemberInfo* ChoiceMemberInfo(const valnode* const* member, TTypeInfo (*func)(void))
{
	return MemberInfo(member, GetChoiceTypeRef(func));
}

#define CLASS_MEMBER(Member) \
	MemberInfo(&static_cast<const CClass*>(0)->Member)
#define ADD_CLASS_MEMBER(Member) \
	AddMember(#Member, CLASS_MEMBER(Member))

#define PTR_CLASS_MEMBER(Member) \
	PtrMemberInfo(&static_cast<const CClass*>(0)->Member)
#define ADD_PTR_CLASS_MEMBER(Member) \
	AddMember(#Member, PTR_CLASS_MEMBER(Member))

#define STL_CLASS_MEMBER(Member) \
	StlMemberInfo(&static_cast<const CClass*>(0)->Member)
#define ADD_STL_CLASS_MEMBER(Member) \
	AddMember(#Member, STL_CLASS_MEMBER(Member))

#define ASN_MEMBER(Member, Type) \
	NAME2(Type, MemberInfo)(&static_cast<const CClass*>(0)->Member)
#define ADD_ASN_MEMBER(Member, Type) \
	AddMember(#Member, ASN_MEMBER(Member, Type))

#define SET_MEMBER(Member) \
	SetMemberInfo(&static_cast<const CClass*>(0)->Member)
#define ADD_SET_MEMBER(Member) \
	AddMember(#Member, SET_MEMBER(Member))

#define SEQUENCE_MEMBER(Member) \
	SequenceMemberInfo(&static_cast<const CClass*>(0)->Member)
#define ADD_SEQUENCE_MEMBER(Member) \
	AddMember(#Member, SEQUENCE_MEMBER(Member))

#define SET_OF_MEMBER(Member) \
	SetOfMemberInfo(&static_cast<const CClass*>(0)->Member)
#define ADD_SET_OF_MEMBER(Member) \
	AddMember(#Member, SET_OF_MEMBER(Member))

#define SEQUENCE_OF_MEMBER(Member) \
	SequenceOfMemberInfo(&static_cast<const CClass*>(0)->Member)
#define ADD_SEQUENCE_OF_MEMBER(Member) \
	AddMember(#Member, SEQUENCE_OF_MEMBER(Member))

#define CHOICE_MEMBER(Member, Choices) \
    ChoiceMemberInfo(&static_cast<const CClass*>(0)->Member, NAME2(GetTypeInfo_struct_, Choices))
#define ADD_CHOICE_MEMBER(Member, Choices) \
    AddMember(#Member, CHOICE_MEMBER(Member, Choices))
#define ADD_CHOICE_STD_VARIANT(Name, Member) \
    AddVariant(#Name, GetTypeRef(&static_cast<const valnode*>(0)->data.NAME2(Member, value)))
#define ADD_CHOICE_VARIANT(Name, Type, Struct) \
    AddVariant(#Name, NAME3(Get, Type, TypeRef)(reinterpret_cast<const NAME2(struct_, Struct)* const*>(&static_cast<const valnode*>(0)->data.ptrvalue)))

#include <serial/serial.inl>

END_NCBI_SCOPE

#endif  /* SERIAL__HPP */
