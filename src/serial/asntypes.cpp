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
* Revision 1.5  1999/07/07 18:18:32  vasilche
* Fixed some bugs found by MS VC++
*
* Revision 1.4  1999/07/02 21:31:51  vasilche
* Implemented reading from ASN.1 binary format.
*
* Revision 1.3  1999/07/01 17:55:25  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.2  1999/06/30 18:54:58  vasilche
* Fixed some errors under MSVS
*
* Revision 1.1  1999/06/30 16:04:46  vasilche
* Added support for old ASN.1 structures.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/asntypes.hpp>
#include <serial/objostr.hpp>
#include <serial/objistr.hpp>
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
    TObjectPtr object = calloc(sizeof(TObjectPtr), 1);
    Get(object) = GetDataTypeInfo()->Create();
    return object;
}

bool CSequenceTypeInfo::Equals(TConstObjectPtr object1,
                               TConstObjectPtr object2) const
{
    TConstObjectPtr obj1 = Get(object1);
    TConstObjectPtr obj2 = Get(object2);
    if ( obj1 == 0 || obj2 == 0 )
        THROW1_TRACE(runtime_error, "null sequence pointer");

    return GetDataTypeInfo()->Equals(obj1, obj2);
}

void CSequenceTypeInfo::Assign(TObjectPtr dst, TConstObjectPtr src) const
{
    TConstObjectPtr srcObj = Get(src);
    if ( srcObj == 0 )
        THROW1_TRACE(runtime_error, "null sequence pointer");

    TObjectPtr dstObj = Get(dst);
    if ( dstObj == 0 ) {
        ERR_POST("null sequence pointer");
        dstObj = Get(dst) = GetDataTypeInfo()->Create();
    }
    GetDataTypeInfo()->Assign(dstObj, srcObj);
}

void CSequenceTypeInfo::CollectExternalObjects(COObjectList& l,
                                               TConstObjectPtr object) const
{
    TConstObjectPtr obj = Get(object);
    if ( obj == 0 )
        THROW1_TRACE(runtime_error, "null sequence pointer"); 
    GetDataTypeInfo()->CollectObjects(l, obj);
}

void CSequenceTypeInfo::WriteData(CObjectOStream& out,
                                  TConstObjectPtr object) const
{
    TConstObjectPtr obj = Get(object);
    if ( obj == 0 )
        THROW1_TRACE(runtime_error, "null sequence pointer"); 
    out.WriteExternalObject(obj, GetDataTypeInfo());
}

void CSequenceTypeInfo::ReadData(CObjectIStream& in,
                                 TObjectPtr object) const
{
    TObjectPtr obj = Get(object);
    if ( obj == 0 ) {
        ERR_POST("null sequence pointer"); 
        obj = Get(object) = GetDataTypeInfo()->Create();
    }
    in.ReadExternalObject(obj, GetDataTypeInfo());
}

CSetTypeInfo::CSetTypeInfo(const CTypeRef& typeRef)
    : CSequenceTypeInfo(typeRef)
{
}

CSequenceOfTypeInfo::CSequenceOfTypeInfo(const CTypeRef& typeRef)
    : CSequenceTypeInfo(typeRef)
{
}

bool CSequenceOfTypeInfo::RandomOrder(void) const
{
    return false;
}

TObjectPtr CSequenceOfTypeInfo::Create(void) const
{
    return calloc(sizeof(TObjectPtr), 1);
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
    while ( (src = Get(src)) != 0 ) {
        dst = Get(dst) = GetDataTypeInfo()->Create();
        GetDataTypeInfo()->Assign(dst, src);
    }
	Get(dst) = 0;
}

void CSequenceOfTypeInfo::CollectExternalObjects(COObjectList& l,
                                                 TConstObjectPtr object) const
{
    while ( (object = Get(object)) != 0 ) {
        GetDataTypeInfo()->CollectObjects(l, object);
    }
}

void CSequenceOfTypeInfo::WriteData(CObjectOStream& out,
                                    TConstObjectPtr object) const
{
    CObjectOStream::Block block(out, RandomOrder());
    while ( (object = Get(object)) != 0 ) {
        block.Next();
        out.WriteExternalObject(object, GetDataTypeInfo());
    }
}

void CSequenceOfTypeInfo::ReadData(CObjectIStream& in,
                                   TObjectPtr object) const
{
    CObjectIStream::Block block(in, RandomOrder());
    while ( block.Next() ) {
        TObjectPtr obj = Get(object);
        if ( obj == 0 ) {
            ERR_POST("null sequence pointer"); 
            obj = Get(object) = GetDataTypeInfo()->Create();
        }
        in.ReadExternalObject(obj, GetDataTypeInfo());
        object = obj;
    }
}

CSetOfTypeInfo::CSetOfTypeInfo(const CTypeRef& typeRef)
    : CSequenceOfTypeInfo(typeRef)
{
}

bool CSetOfTypeInfo::RandomOrder(void) const
{
    return true;
}


CChoiceTypeInfo::CChoiceTypeInfo(const CTypeRef& typeRef)
    : m_ChoiceTypeInfo(typeRef)
{
}

CChoiceTypeInfo::~CChoiceTypeInfo(void)
{
}

size_t CChoiceTypeInfo::GetSize(void) const
{
    return sizeof(valnode*);
}

TObjectPtr CChoiceTypeInfo::Create(void) const
{
    return calloc(sizeof(valnode*), 1);
}

bool CChoiceTypeInfo::Equals(TConstObjectPtr object1,
                             TConstObjectPtr object2) const
{
    TConstObjectPtr obj1 = Get(object1);
    TConstObjectPtr obj2 = Get(object2);
    if ( obj1 == 0 || obj2 == 0 )
        THROW1_TRACE(runtime_error, "null valnode pointer");

    return GetChoiceTypeInfo()->Equals(obj1, obj2);
}

void CChoiceTypeInfo::Assign(TObjectPtr dst, TConstObjectPtr src) const
{
    TConstObjectPtr srcObj = Get(src);
    if ( srcObj == 0 )
        THROW1_TRACE(runtime_error, "null valnode pointer");

    TObjectPtr dstObj = Get(dst);
    if ( dstObj == 0 ) {
        ERR_POST("null valnode pointer");
        dstObj = Get(dst) = GetChoiceTypeInfo()->Create();
    }

    GetChoiceTypeInfo()->Assign(dstObj, srcObj);
}

void CChoiceTypeInfo::CollectExternalObjects(COObjectList& l,
                                             TConstObjectPtr object) const
{
    TConstObjectPtr obj = Get(object);
    if ( obj == 0 )
        THROW1_TRACE(runtime_error, "null valnode pointer");

    GetChoiceTypeInfo()->CollectObjects(l, obj);
}

void CChoiceTypeInfo::WriteData(CObjectOStream& out,
                                TConstObjectPtr object) const
{
    TConstObjectPtr obj = Get(object);
    if ( obj == 0 )
        THROW1_TRACE(runtime_error, "null valnode pointer");

    out.WriteExternalObject(obj, GetChoiceTypeInfo());
}

void CChoiceTypeInfo::ReadData(CObjectIStream& in,
                               TObjectPtr object) const
{
    TObjectPtr obj = Get(object);
    if ( obj == 0 ) {
        ERR_POST("null valnode pointer");
        obj = Get(object) = GetChoiceTypeInfo()->Create();
    }

    in.ReadExternalObject(obj, GetChoiceTypeInfo());
}


CChoiceValNodeInfo::CChoiceValNodeInfo(void)
{
}

void CChoiceValNodeInfo::AddVariant(const CMemberId& id,
                                    const CTypeRef& typeRef)
{
    m_Variants.AddMember(id);
    m_VariantTypes.push_back(typeRef);
}

void CChoiceValNodeInfo::AddVariant(const string& name,
                                    const CTypeRef& typeRef)
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
    TMemberIndex choice = val1->choice;
    if ( choice != val2->choice )
        return false;
    TMemberIndex index = choice - 1;
    if ( index >= 0 && index < GetVariantsCount() ) {
        return GetVariantTypeInfo(index)->Equals(&val1->data, &val2->data);
    }
    return choice == 0;
}

void CChoiceValNodeInfo::Assign(TObjectPtr dst, TConstObjectPtr src) const
{
    valnode* valDst = static_cast<valnode*>(dst);
    const valnode* valSrc = static_cast<const valnode*>(src);
    TMemberIndex choice = valSrc->choice;
    valDst->choice = choice;
    TMemberIndex index = choice;
    if ( index >= 0 && index < GetVariantsCount() ) {
        GetVariantTypeInfo(index)->Assign(&valDst->data, &valSrc->data);
    }
    else {
        valDst->data.ptrvalue = 0;
    }
}

void CChoiceValNodeInfo::CollectExternalObjects(COObjectList& l,
                                                 TConstObjectPtr object) const
{
    const valnode* node = static_cast<const valnode*>(object);
    TMemberIndex index = node->choice - 1;
    if ( index < 0 || index >= GetVariantsCount() ) {
        THROW1_TRACE(runtime_error,
                     "illegal choice value: " +
                     NStr::IntToString(node->choice));
    }
    GetVariantTypeInfo(index)->CollectExternalObjects(l, &node->data);
}

void CChoiceValNodeInfo::WriteData(CObjectOStream& out,
                                    TConstObjectPtr object) const
{
    const valnode* node = static_cast<const valnode*>(object);
    TMemberIndex index = node->choice - 1;
    if ( index < 0 || index >= GetVariantsCount() ) {
        THROW1_TRACE(runtime_error,
                     "illegal choice value: " +
                     NStr::IntToString(node->choice));
    }
    CObjectOStream::Member m(out, m_Variants.GetCompleteMemberId(index));
    GetVariantTypeInfo(index)->WriteData(out, &node->data);
}

void CChoiceValNodeInfo::ReadData(CObjectIStream& in,
                                   TObjectPtr object) const
{
    CObjectIStream::Member id(in);
    TMemberIndex index = m_Variants.FindMember(id);
    if ( index < 0 ) {
        THROW1_TRACE(runtime_error,
                     "illegal choice variant: " + id.ToString());
    }
    valnode* node = static_cast<valnode*>(object);
    node->choice = index + 1;
    GetVariantTypeInfo(index)->ReadData(in, &node->data);
}

END_NCBI_SCOPE
