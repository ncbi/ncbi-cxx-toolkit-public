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
#include <serial/typeinfo.hpp>
#include <serial/stdtypes.hpp>
#include <serial/stltypes.hpp>
#include <serial/ptrinfo.hpp>

BEGIN_NCBI_SCOPE

class CObjectIStream;
class CObjectOStream;

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
    _TRACE("GetTypeRef(void)");
    return CTypeRef(typeid(void), CStdTypeInfo<void>::GetTypeInfo);
}

template<typename T>
inline
CTypeRef GetStdTypeRef(const T* )
{
    _TRACE("GetStdTypeRef(const T&) T: " << typeid(T).name());
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
CTypeRef GetTypeRef(const int* object)
{
    _TRACE("GetTypeRef(const int&)");
    return GetStdTypeRef(object);
}

inline
CTypeRef GetTypeRef(const unsigned int* object)
{
    _TRACE("GetTypeRef(const unsigned int&)");
    return GetStdTypeRef(object);
}

inline
CTypeRef GetTypeRef(const string* object)
{
    _TRACE("GetTypeRef(const string&)");
    return GetStdTypeRef(object);
}

// define type info getter for user classes
template<typename CLASS>
inline
CTypeRef GetTypeRef(const CLASS* )
{
    _TRACE("GetTypeRef(const CLASS&) CLASS: " << typeid(CLASS).name());
    return CTypeRef(typeid(CLASS), CLASS::GetTypeInfo);
}

template<typename Data>
inline
CTypeRef GetTypeRef(const list<Data>* )
{
    _TRACE("GetTypeRef(const list<Data>&) Data: " << typeid(Data).name());
    return CTypeRef(typeid(list<Data>),
                    CStlClassInfoList<Data>::GetTypeInfo);
}

template<typename Data>
inline
CTypeRef GetStlTypeRef(const list<Data>* )
{
    _TRACE("GetStlTypeRef(const list<Data>&) Data: " << typeid(Data).name());
    return CTypeRef(typeid(list<Data>),
                    CStlClassInfoList<Data>::GetTypeInfo);
}

// define type info getter for pointers
template<typename CLASS>
inline
CTypeRef GetTypeRef(const CLASS* const* object)
{
    _TRACE("GetTypeRef(const CLASS*) CLASS: " << typeid(CLASS).name());
    return CTypeRef(CTypeInfo::GetPointerTypeInfo(typeid(CLASS*),
                                                  GetTypeRef(object)));
}

template<typename Data>
inline
CStlClassInfoList<Data>::CStlClassInfoList(void)
    : CParent(typeid(TObjectType), GetTypeRef(static_cast<const Data*>(0)))
{
}

inline
CTypeInfo::TTypeInfo GetTypeInfo(void)
{
    return GetTypeRef(static_cast<const void*>(0)).Get();
}

template<typename CLASS>
inline
CTypeInfo::TTypeInfo GetTypeInfo(const CLASS& object)
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
CMemberInfo MemberInfo(const string& name, const T* member, const CTypeRef typeRef)
{
	return CMemberInfo(name, size_t(member), typeRef);
}

template<typename T>
inline
CMemberInfo MemberInfo(const string& name, const T* member)
{
	return MemberInfo(name, member, GetTypeRef(member));
}

template<typename T>
inline
CMemberInfo StlMemberInfo(const string& name, const T* member, const CTypeRef typeRef)
{
	return CMemberInfo(name, size_t(member), typeRef);
}

template<typename T>
inline
CMemberInfo StlMemberInfo(const string& name, const T* member)
{
	return StlMemberInfo(name, member, GetStlTypeRef(member));
}

#define ADD_CLASS_MEMBER(Member) \
	AddMember(MemberInfo(#Member, &static_cast<const CClass*>(0)->Member))
#define ADD_STL_CLASS_MEMBER(Member) \
	AddMember(StlMemberInfo(#Member, &static_cast<const CClass*>(0)->Member))

#include <serial/serial.inl>

END_NCBI_SCOPE

#endif  /* SERIAL__HPP */
