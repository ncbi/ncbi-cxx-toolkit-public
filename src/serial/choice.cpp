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
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.8  2000/03/14 14:42:29  vasilche
* Fixed error reporting.
*
* Revision 1.7  2000/03/07 14:06:21  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
* Added generation of reference counted objects.
*
* Revision 1.6  2000/02/17 20:02:42  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.5  2000/02/01 21:47:21  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Added buffering to CObjectIStreamAsn.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
*
* Revision 1.4  2000/01/10 19:46:38  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.3  2000/01/05 19:43:51  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
* Revision 1.2  1999/12/17 19:05:01  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.1  1999/09/24 18:20:07  vasilche
* Removed dependency on NCBI toolkit.
*
* 
* ===========================================================================
*/

#include <serial/choice.hpp>
#include <serial/objostr.hpp>
#include <serial/objistr.hpp>
#include <serial/memberid.hpp>
#include <serial/member.hpp>

BEGIN_NCBI_SCOPE

typedef CChoiceTypeInfoBase::TMemberIndex TMemberIndex;

CChoiceTypeInfoBase::CChoiceTypeInfoBase(const string& name)
    : CParent(name)
{
}

CChoiceTypeInfoBase::CChoiceTypeInfoBase(const char* name)
    : CParent(name)
{
}

CChoiceTypeInfoBase::~CChoiceTypeInfoBase(void)
{
}

void CChoiceTypeInfoBase::AddVariant(const CMemberId& id,
                                     const CTypeRef& typeRef)
{
    m_Members.AddMember(id, new CRealMemberInfo(0, typeRef.Get()));
}

void CChoiceTypeInfoBase::AddVariant(const string& name,
                                     const CTypeRef& typeRef)
{
    AddVariant(CMemberId(name), typeRef);
}

void CChoiceTypeInfoBase::AddVariant(const char* name,
                                     const CTypeRef& typeRef)
{
    AddVariant(CMemberId(name), typeRef);
}

TMemberIndex CChoiceTypeInfoBase::GetVariantsCount(void) const
{
    return m_Members.GetSize();
}

TTypeInfo CChoiceTypeInfoBase::GetVariantTypeInfo(TMemberIndex index) const
{
    return m_Members.GetMemberInfo(index)->GetTypeInfo();
}

bool CChoiceTypeInfoBase::IsDefault(TConstObjectPtr object) const
{
    return object == 0 || GetIndex(object) == -1;
}

bool CChoiceTypeInfoBase::Equals(TConstObjectPtr object1,
                                 TConstObjectPtr object2) const
{
    TMemberIndex index = GetIndex(object1);
    if ( index != GetIndex(object2) )
        return false;
    if ( index == -1 )
        return true;
    if ( index >= 0 && index < GetVariantsCount() ) {
        return GetVariantTypeInfo(index)->Equals(GetData(object1, index),
                                                 GetData(object2, index));
    }
    return index == -1;
}

void CChoiceTypeInfoBase::SetDefault(TObjectPtr dst) const
{
    SetIndex(dst, -1);
}

void CChoiceTypeInfoBase::Assign(TObjectPtr dst, TConstObjectPtr src) const
{
    TMemberIndex index = GetIndex(src);
    SetIndex(dst, index);
    if ( index >= 0 && index < GetVariantsCount() ) {
        GetVariantTypeInfo(index)->Assign(GetData(dst, index),
                                          GetData(src, index));
    }
}

void CChoiceTypeInfoBase::WriteData(CObjectOStream& out,
                                    TConstObjectPtr object) const
{
    TMemberIndex index = GetIndex(object);
    if ( index >= 0 && index < GetVariantsCount() ) {
        CObjectOStream::Member m(out, GetMembers(), index);
        GetVariantTypeInfo(index)->
            WriteData(out, GetData(object, index));
    }
}

void CChoiceTypeInfoBase::ReadData(CObjectIStream& in,
                                   TObjectPtr object) const
{
    CObjectIStream::Member m(in, GetMembers());
    TMemberIndex index = m.GetIndex();
    SetIndex(object, index);
    GetVariantTypeInfo(index)->
        ReadData(in, GetData(object, index));
    m.End();
}

void CChoiceTypeInfoBase::SkipData(CObjectIStream& in) const
{
    CObjectIStream::Member m(in, GetMembers());
    TMemberIndex index = m.GetIndex();
    GetVariantTypeInfo(index)->SkipData(in);
    m.End();
}

CGeneratedChoiceTypeInfo::CGeneratedChoiceTypeInfo(const char* name,
                                                   size_t size,
                                                   TCreateFunction cF,
                                                   TGetIndexFunction gIF,
                                                   TSetIndexFunction sIF)
    : CParent(name), m_Size(size),
      m_CreateFunction(cF), m_GetIndexFunction(gIF), m_SetIndexFunction(sIF)
{
}

CGeneratedChoiceTypeInfo::~CGeneratedChoiceTypeInfo(void)
{
}

size_t CGeneratedChoiceTypeInfo::GetSize(void) const
{
    return m_Size;
}

TObjectPtr CGeneratedChoiceTypeInfo::Create(void) const
{
    return m_CreateFunction();
}

TMemberIndex CGeneratedChoiceTypeInfo::GetIndex(TConstObjectPtr object) const
{
    return m_GetIndexFunction(object);
}

void CGeneratedChoiceTypeInfo::SetIndex(TObjectPtr object,
                                        TMemberIndex index) const
{
    m_SetIndexFunction(object, index);
}

TObjectPtr CGeneratedChoiceTypeInfo::x_GetData(TObjectPtr object,
                                               TMemberIndex index) const
{
    _ASSERT(object != 0);
    const CMemberInfo* info = GetMembers().GetMemberInfo(index);
    TObjectPtr memberPtr = info->GetMember(object);
    if ( info->IsPointer() ) {
        memberPtr = CType<TObjectPtr>::Get(memberPtr);
        _ASSERT(memberPtr != 0 );
    }
    return memberPtr;
}

END_NCBI_SCOPE
