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

BEGIN_NCBI_SCOPE

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
    m_Variants.AddMember(id);
    m_VariantTypes.push_back(typeRef);
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
        return GetVariantTypeInfo(index)->Equals(GetData(object1),
                                                 GetData(object2));
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
        GetVariantTypeInfo(index)->Assign(GetData(dst), GetData(src));
    }
}

void CChoiceTypeInfoBase::CollectExternalObjects(COObjectList& l,
                                                 TConstObjectPtr object) const
{
    TMemberIndex index = GetIndex(object);
    if ( index >= 0 && index < GetVariantsCount() )
        GetVariantTypeInfo(index)->CollectExternalObjects(l, GetData(object));
}

void CChoiceTypeInfoBase::WriteData(CObjectOStream& out,
                                    TConstObjectPtr object) const
{
    TMemberIndex index = GetIndex(object);
    if ( index >= 0 && index < GetVariantsCount() ) {
        CObjectOStream::Member m(out, m_Variants.GetCompleteMemberId(index));
        GetVariantTypeInfo(index)->WriteData(out, GetData(object));
    }
}

void CChoiceTypeInfoBase::ReadData(CObjectIStream& in,
                                   TObjectPtr object) const
{
    CObjectIStream::Member m(in);
    TMemberIndex index = m_Variants.FindMember(m);
    if ( index < 0 ) {
        THROW1_TRACE(runtime_error,
                     "illegal choice variant: " + m.Id().ToString());
    }
    m.Id().UpdateName(m_Variants.GetMemberId(index));
    SetIndex(object, index);
    GetVariantTypeInfo(index)->ReadData(in, GetData(object));
}

END_NCBI_SCOPE
