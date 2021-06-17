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
*/

#include <serial/impl/typeref.hpp>


/** @addtogroup GenClassSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CObjectOStream;
class CObjectIStream;

NCBI_XSERIAL_EXPORT
TTypeInfo CPointerTypeInfoGetTypeInfo(TTypeInfo type);

// define type info getter for classes
template<class Class>
inline TTypeInfoGetter GetTypeInfoGetter(const Class* object);


// define type info getter for pointers
template<typename T>
inline
CTypeRef GetPtrTypeRef(const T* const* object)
{
    const T* p = 0;
    return CTypeRef(&CPointerTypeInfoGetTypeInfo, GetTypeInfoGetter(p));
}

// define type info getter for user classes
template<class Class>
inline
TTypeInfoGetter GetTypeInfoGetter(const Class* )
{
    return &Class::GetTypeInfo;
}

template<typename T>
inline
TTypeInfoGetter GetTypeRef(const T* object)
{
    return GetTypeInfoGetter(object);
}

NCBI_XSERIAL_EXPORT
void Write(CObjectOStream& out, TConstObjectPtr object, const CTypeRef& type);

NCBI_XSERIAL_EXPORT
void Read(CObjectIStream& in, TObjectPtr object, const CTypeRef& type);

NCBI_XSERIAL_EXPORT
void Write(CObjectOStream& out, TConstObjectPtr object, TTypeInfo type);

NCBI_XSERIAL_EXPORT
void Read(CObjectIStream& in, TObjectPtr object, TTypeInfo type);

// reader/writer
template<typename T>
inline
CObjectOStream& Write(CObjectOStream& out, const T& object)
{
    Write(out, &object,object.GetThisTypeInfo());
    return out;
}

template<typename T>
inline
CObjectIStream& Read(CObjectIStream& in, T& object)
{
    Read(in,&object,object.GetThisTypeInfo());
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


/* @} */


END_NCBI_SCOPE

#endif  /* SERIAL__HPP */
