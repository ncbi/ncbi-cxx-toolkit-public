#ifndef SERIALASN__HPP
#define SERIALASN__HPP

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

#include <serial/serial.hpp>
#include <serial/typeref.hpp>

struct asnio;
struct asntype;

BEGIN_NCBI_SCOPE

#if HAVE_NCBI_C

TTypeInfo COctetStringTypeInfoGetTypeInfo(void);
TTypeInfo CAutoPointerTypeInfoGetTypeInfo(TTypeInfo type);
TTypeInfo CSetOfTypeInfoGetTypeInfo(TTypeInfo type);
TTypeInfo CSequenceOfTypeInfoGetTypeInfo(TTypeInfo type);

#ifdef HAVE_WINDOWS_H
# define ASNCALL __stdcall
#else
# define ASNCALL
#endif

typedef TObjectPtr (ASNCALL*TNewProc)(void);
typedef TObjectPtr (ASNCALL*TFreeProc)(TObjectPtr);
typedef TObjectPtr (ASNCALL*TReadProc)(asnio*, asntype*);
typedef unsigned char (ASNCALL*TWriteProc)(TObjectPtr, asnio*, asntype*);

TTypeInfo COldAsnTypeInfoGetTypeInfo(const string& name,
                                     TNewProc newProc,
                                     TFreeProc freeProc,
                                     TReadProc readProc,
                                     TWriteProc writeProc);

// ASN
inline
CTypeRef GetOctetStringTypeRef(const void* const* )
{
    return &COctetStringTypeInfoGetTypeInfo;
}

template<typename T>
inline
CTypeRef GetSetTypeRef(const T* const* )
{
    const T* p = 0;
    return CTypeRef(&CAutoPointerTypeInfoGetTypeInfo, GetTypeRef(p));
}

template<typename T>
inline
CTypeRef GetSequenceTypeRef(const T* const* )
{
    const T* p = 0;
    return CTypeRef(&CAutoPointerTypeInfoGetTypeInfo, GetTypeRef(p));
}

template<typename T>
inline
CTypeRef GetSetOfTypeRef(const T* const* p)
{
    //    const T* p = 0;
    return CTypeRef(&CSetOfTypeInfoGetTypeInfo, GetSetTypeRef(p));
}

template<typename T>
inline
CTypeRef GetSequenceOfTypeRef(const T* const* p)
{
    //    const T* p = 0;
    return CTypeRef(&CSequenceOfTypeInfoGetTypeInfo, GetSetTypeRef(p));
}

inline
CTypeRef GetChoiceTypeRef(TTypeInfo (*func)(void))
{
    return CTypeRef(&CAutoPointerTypeInfoGetTypeInfo, CTypeRef(func));
}

template<typename T>
inline
CTypeRef GetOldAsnTypeRef(const string& name,
                          T* (ASNCALL*newProc)(void),
						  T* (ASNCALL*freeProc)(T*),
                          T* (ASNCALL*readProc)(asnio*, asntype*),
                          unsigned char (ASNCALL*writeProc)(T*, asnio*, asntype*))
{
    return COldAsnTypeInfoGetTypeInfo(name,
                                      reinterpret_cast<TNewProc>(newProc),
                                      reinterpret_cast<TFreeProc>(freeProc),
                                      reinterpret_cast<TReadProc>(readProc),
                                      reinterpret_cast<TWriteProc>(writeProc));
}
#endif

// type info declaration
#define SERIALASN_NAME2(n1, n2) n1##n2
#define ASN_TYPE_INFO_GETTER_NAME(name) \
    SERIALASN_NAME2(GetTypeInfo_struct_, name)
#define ASN_STRUCT_NAME(name) SERIALASN_NAME2(struct_, name)

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

END_NCBI_SCOPE

#endif
