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
inline
CTypeRef GetTypeRef(void)
{
    NcbiCerr << "GetTypeRef(void)" << endl;
    return CTypeRef(typeid(void), CStdTypeInfo<void>::CreateTypeInfo);
}

template<typename T>
inline
CTypeRef GetStdTypeRef(const T& )
{
    NcbiCerr << "GetStdTypeRef(const T&) T: " << typeid(T).name() << endl;
    return CTypeRef(typeid(T), CStdTypeInfo<T>::CreateTypeInfo);
}

inline
CTypeRef GetTypeRef(const char& object)
{
    return GetStdTypeRef(object);
}

inline
CTypeRef GetTypeRef(const unsigned char& object)
{
    return GetStdTypeRef(object);
}

inline
CTypeRef GetTypeRef(const signed char& object)
{
    return GetStdTypeRef(object);
}

inline
CTypeRef GetTypeRef(const int& object)
{
    NcbiCerr << "GetTypeRef(const int&)" << endl;
    return GetStdTypeRef(object);
}

inline
CTypeRef GetTypeRef(const unsigned int& object)
{
    NcbiCerr << "GetTypeRef(const unsigned int&)" << endl;
    return GetStdTypeRef(object);
}

inline
CTypeRef GetTypeRef(const string& object)
{
    NcbiCerr << "GetTypeRef(const string&)" << endl;
    return GetStdTypeRef(object);
}

template<typename CLASS>
inline
CTypeRef GetTypeRef(const list<CLASS>& )
{
    NcbiCerr << "GetTypeRef(const list<CLASS>&) CLASS: " << typeid(CLASS).name() << endl;
    return CTypeRef(typeid(list<CLASS>),
                    CStlClassInfoList<CLASS>::CreateTypeInfo);
}

template<typename Data>
inline
CStlClassInfoList<Data>::CStlClassInfoList(void)
    : CParent(typeid(TObjectType), GetTypeRef(*static_cast<const Data*>(0)))
{
}

// define type info getter for user classes
template<typename CLASS>
inline
CTypeRef GetTypeRef(const CLASS& )
{
    NcbiCerr << "GetTypeRef(const CLASS&) CLASS: " << typeid(CLASS).name() << endl;
    return CTypeRef(typeid(CLASS), CLASS::CreateTypeInfo);
}

// define type info getter for pointers
template<typename CLASS>
inline
CTypeRef GetTypeRef(const CLASS* object)
{
    NcbiCerr << "GetTypeRef(const CLASS*) CLASS: " << typeid(CLASS).name() << endl;
    return CTypeRef(CTypeInfo::GetPointerTypeInfo(typeid(CLASS*),
                                                  GetTypeRef(*object)));
}

inline
CTypeInfo::TTypeInfo GetTypeInfo(void)
{
    return GetTypeRef().Get();
}

template<typename CLASS>
inline
CTypeInfo::TTypeInfo GetTypeInfo(const CLASS& object)
{
    return GetTypeRef(object).Get();
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

#include <serial/serial.inl>

END_NCBI_SCOPE

#endif  /* SERIAL__HPP */
