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
* Revision 1.1  1999/06/30 16:04:46  vasilche
* Added support for old ASN.1 structures.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/asntypes.hpp>
#include <asn.h>

BEGIN_NCBI_SCOPE

CSequenceTypeInfo::CSequenceTypeInfo(const CTypeRef& typeRef)
    : m_DataType(typeRef)
{
}

size_t CSequenceTypeInfo::GetSize(void) const
{
    return sizeof(void*);
}

TObjectPtr CSequenceTypeInfo::Create(void) const
{
    return new TObjectPtr(GetDataTypeInfo()->Create());
}

bool CSequenceTypeInfo::Equals(TConstObjectPtr object1,
                               TConstObjectPtr object2) const
{
    TConstObjectPtr obj1 = Get(object1);
    TConstObjectPtr obj2 = Get(object2);
    if ( obj1 == 0 ) {
        return obj2 == 0;
    }
    else if ( obj2 == 0 ) {
        return false;
    }
    else {
        return GetDataTypeInfo()->Equals(obj1, obj2);
    }
}

void CSequenceTypeInfo::Assign(TObjectPtr dst, TConstObjectPtr src) const
{
    TConstObjectPtr srcObj = Get(src);
    TObjectPtr dstObj;
    if ( srcObj == 0 ) {
        dstObj = 0;
    }
    else {
        dstObj = calloc(GetDataTypeInfo()->GetSize(), 1);
        GetDataTypeInfo()->Assign(dstObj, srcObj);
    }
    Get(dst) = dstObj;
}

void CSequenceTypeInfo::CollectExternalObjects(COObjectList& list,
                                               TConstObjectPtr object) const
{
}

void CSequenceTypeInfo::WriteData(CObjectOStream& out,
                                  TConstObjectPtr obejct) const
{
}

void CSequenceTypeInfo::ReadData(CObjectIStream& in,
                                 TObjectPtr object) const
{
}

CSetTypeInfo::CSetTypeInfo(const CTypeRef& typeRef)
    : CSequenceTypeInfo(typeRef)
{
}

CSequenceOfTypeInfo::CSequenceOfTypeInfo(const CTypeRef& typeRef)
    : CSequenceTypeInfo(typeRef)
{
}

TObjectPtr CSequenceOfTypeInfo::Create(void) const
{
    return new TObjectPtr(0);
}

bool CSequenceOfTypeInfo::Equals(TConstObjectPtr object1,
                                 TConstObjectPtr object2) const
{
    TConstObjectPtr obj1 = Get(object1);
    TConstObjectPtr obj2 = Get(object2);
    while ( obj1 != 0 ) {
        if ( obj2 == 0 )
            return false;
        if ( !GetDataTypeInfo()->Equals(obj1, obj2) )
            return false;
        obj1 = Get(obj1);
        obj2 = Get(obj2);
    }
    return obj2 == 0;
}

void CSequenceOfTypeInfo::Assign(TObjectPtr dst, TConstObjectPtr src) const
{
}

void CSequenceOfTypeInfo::CollectExternalObjects(COObjectList& list,
                                                 TConstObjectPtr object) const
{
}

void CSequenceOfTypeInfo::WriteData(CObjectOStream& out,
                                    TConstObjectPtr obejct) const
{
}

void CSequenceOfTypeInfo::ReadData(CObjectIStream& in,
                                   TObjectPtr object) const
{
}

CSetOfTypeInfo::CSetOfTypeInfo(const CTypeRef& typeRef)
    : CSequenceOfTypeInfo(typeRef)
{
}

CChoiceTypeInfo::CChoiceTypeInfo(const CTypeRef& typeRef)
{
}

CChoiceTypeInfo::~CChoiceTypeInfo(void)
{
}

size_t CChoiceTypeInfo::GetSize(void) const
{
    return sizeof(void*);
}

TObjectPtr CChoiceTypeInfo::Create(void) const
{
    throw runtime_error("illegal operation");
}

bool CChoiceTypeInfo::Equals(TConstObjectPtr object1,
                                 TConstObjectPtr object2) const
{
    return false;
}

void CChoiceTypeInfo::Assign(TObjectPtr dst, TConstObjectPtr src) const
{
}

void CChoiceTypeInfo::CollectExternalObjects(COObjectList& list,
                                                 TConstObjectPtr object) const
{
}

void CChoiceTypeInfo::WriteData(CObjectOStream& out,
                                    TConstObjectPtr obejct) const
{
}

void CChoiceTypeInfo::ReadData(CObjectIStream& in,
                                   TObjectPtr object) const
{
}

CChoiceValNodeInfo::CChoiceValNodeInfo(void)
{
}

void CChoiceValNodeInfo::AddVariant(const CMemberId& id, const CTypeRef& typeRef)
{
    m_Variants.push_back(pair<CMemberId, CTypeRef>(id, typeRef));
}

void CChoiceValNodeInfo::AddVariant(const string& name, const CTypeRef& typeRef)
{
    AddVariant(CMemberId(name), typeRef);
}

size_t CChoiceValNodeInfo::GetSize(void) const
{
    return sizeof(valnode);
}

TObjectPtr CChoiceValNodeInfo::Create(void) const
{
    return calloc(sizeof(valnode), 1);
}

bool CChoiceValNodeInfo::Equals(TConstObjectPtr object1,
                                TConstObjectPtr object2) const
{
    const valnode* val1 = static_cast<const valnode*>(object1);
    const valnode* val2 = static_cast<const valnode*>(object2);
    int choice = val1->choice;
    if ( choice != val2->choice )
        return false;
    if ( choice > 0 && choice <= m_Variants.size() )
        return m_Variants[choice - 1].second.Get()->Equals(&val1->data, &val2->data);
    return choice == 0;
}

void CChoiceValNodeInfo::Assign(TObjectPtr dst, TConstObjectPtr src) const
{
    valnode* valDst = static_cast<valnode*>(dst);
    const valnode* valSrc = static_cast<const valnode*>(src);
    int choice = valSrc->choice;
    valDst->choice = choice;
    if ( choice > 0 && choice <= m_Variants.size() )
        m_Variants[choice - 1].second.Get()->Assign(&valDst->data, &valSrc->data);
    else
        valDst->data.ptrvalue = 0;
}

void CChoiceValNodeInfo::CollectExternalObjects(COObjectList& list,
                                                 TConstObjectPtr object) const
{
}

void CChoiceValNodeInfo::WriteData(CObjectOStream& out,
                                    TConstObjectPtr obejct) const
{
}

void CChoiceValNodeInfo::ReadData(CObjectIStream& in,
                                   TObjectPtr object) const
{
}

END_NCBI_SCOPE
