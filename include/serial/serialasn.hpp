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

#include <corelib/ncbistd.hpp>

#if HAVE_NCBI_C

#include <serial/serialimpl.hpp>

struct asnio;
struct asntype;

BEGIN_NCBI_SCOPE

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
    return CTypeRef(&CAutoPointerTypeInfoGetTypeInfo, GetAsnStructTypeInfo(p));
}

template<typename T>
inline
CTypeRef GetSequenceTypeRef(const T* const* )
{
    const T* p = 0;
    return CTypeRef(&CAutoPointerTypeInfoGetTypeInfo, GetAsnStructTypeInfo(p));
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
    return CTypeRef(&CAutoPointerTypeInfoGetTypeInfo, func);
}

template<typename T>
inline
CTypeRef GetOldAsnTypeRef(const string& name,
                          T* (ASNCALL*newProc)(void),
						  T* (ASNCALL*freeProc)(T*),
                          T* (ASNCALL*readProc)(asnio*, asntype*),
                          unsigned char (ASNCALL*writeProc)(T*, asnio*,
                                                            asntype*))
{
    return COldAsnTypeInfoGetTypeInfo(name,
                                      reinterpret_cast<TNewProc>(newProc),
                                      reinterpret_cast<TFreeProc>(freeProc),
                                      reinterpret_cast<TReadProc>(readProc),
                                      reinterpret_cast<TWriteProc>(writeProc));
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

// old ASN structires info
#define BEGIN_NAMED_ASN_STRUCT_INFO(AsnStructAlias, AsnStructName) \
    BEGIN_TYPE_INFO(NCBI_NAME2(struct_,AsnStructName), \
        ASN_STRUCT_METHOD_NAME(AsnStructName), \
        NCBI_NS_NCBI::CClassTypeInfo, \
        NCBI_NS_NCBI::CClassInfoHelper<CClass>::CreateAsnStructInfo(AsnStructAlias))
#define BEGIN_ASN_STRUCT_INFO(AsnStructName) \
    BEGIN_NAMED_ASN_STRUCT_INFO(#AsnStructName, AsnStructName)
#define END_ASN_STRUCT_INFO END_TYPE_INFO

#define BEGIN_NAMED_ASN_CHOICE_INFO(AsnChoiceAlias, AsnChoiceName) \
    BEGIN_TYPE_INFO(valnode, \
        ASN_STRUCT_METHOD_NAME(AsnChoiceName), \
        NCBI_NS_NCBI::CChoiceTypeInfo, \
        NCBI_NS_NCBI::CClassInfoHelper<CClass>::CreateAsnChoiceInfo(AsnChoiceAlias))
#define BEGIN_ASN_CHOICE_INFO(AsnChoiceName) \
    BEGIN_NAMED_ASN_CHOICE_INFO(#AsnChoiceName, AsnChoiceName)
#define END_ASN_CHOICE_INFO END_TYPE_INFO

// adding old ASN members
#define ASN_MEMBER(MemberName, AsnTypeKind) \
	NCBI_NAME2(AsnTypeKind,MemberInfo)(MEMBER_PTR(MemberName))
#define ADD_NAMED_ASN_MEMBER(MemberAlias, MemberName, AsnTypeKind) \
    NCBI_NS_NCBI::AddMember(info->GetMembers(),MemberAlias, \
        ASN_MEMBER(MemberName, AsnTypeKind))
#define ADD_ASN_MEMBER(MemberName, AsnTypeKind) \
    ADD_NAMED_ASN_MEMBER(#MemberName, MemberName, AsnTypeKind)

#define OLD_ASN_MEMBER(MemberName, AsnTypeAlias, AsnTypeName) \
    NCBI_NS_NCBI::OldAsnMemberInfo(MEMBER_PTR(MemberName), AsnTypeAlias, \
        &NCBI_NAME2(AsnTypeName,New), &NCBI_NAME2(AsnTypeName,Free), \
        &NCBI_NAME2(AsnTypeName,AsnRead), &NCBI_NAME2(AsnTypeName,AsnWrite))
#define ADD_NAMED_OLD_ASN_MEMBER(MemberAlias, MemberName, AsnTypeAlias, AsnTypeName) \
    NCBI_NS_NCBI::AddMember(info->GetMembers(),MemberAlias, \
        OLD_ASN_MEMBER(MemberName, AsnTypeAlias, AsnTypeName))
#define ADD_OLD_ASN_MEMBER(MemberName, AsnTypeName) \
    ADD_NAMED_OLD_ASN_MEMBER(#MemberName, MemberName, #AsnTypeName, AsnTypeName)

#define ASN_CHOICE_MEMBER(MemberName, AsnChoiceName) \
    NCBI_NS_NCBI::ChoiceMemberInfo(MEMBER_PTR(MemberName), \
                                   ASN_STRUCT_METHOD_NAME(AsnChoiceName))
#define ADD_NAMED_ASN_CHOICE_MEMBER(MemberAlias, MemberName, AsnChoiceName) \
    NCBI_NS_NCBI::AddMember(info->GetMembers(),MemberAlias, \
        ASN_CHOICE_MEMBER(MemberName, AsnChoiceName))
#define ADD_ASN_CHOICE_MEMBER(MemberName, AsnChoiceName) \
    ADD_NAMED_ASN_CHOICE_MEMBER(#MemberName, MemberName, AsnChoiceName)

#define ADD_NAMED_ASN_CHOICE_STD_VARIANT(VariantAlias, AsnTypeName) \
    NCBI_NS_NCBI::AddMember(info->GetMembers(),VariantAlias, \
        MEMBER_PTR(data.NCBI_NAME2(AsnTypeName,value)), \
        GetStdTypeInfoGetter(MEMBER_PTR(data.NCBI_NAME2(AsnTypeName,value))))
#define ADD_ASN_CHOICE_STD_VARIANT(VariantName, AsnTypeName) \
    ADD_NAMED_ASN_CHOICE_STD_VARIANT(#VariantName, AsnTypeName)
#define ADD_NAMED_ASN_CHOICE_VARIANT(VariantAlias, AsnTypeKind, AsnTypeName) \
    NCBI_NS_NCBI::AddMember(info->GetMembers(),VariantAlias, \
        MEMBER_PTR(data.ptrvalue), \
        NCBI_NAME3(Get,AsnTypeKind,TypeRef)(reinterpret_cast<const NCBI_NAME2 \
            (struct_,AsnTypeName)* const*>(MEMBER_PTR(data.ptrvalue))))
#define ADD_ASN_CHOICE_VARIANT(VariantName, AsnTypeKind, AsnTypeName) \
    ADD_NAMED_ASN_CHOICE_VARIANT(#VariantName, AsnTypeKind, AsnTypeName)

END_NCBI_SCOPE

#endif /* HAVE_NCBI_C */

#endif
