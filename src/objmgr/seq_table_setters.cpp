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
*   CSeq_table_Info -- parsed information about Seq-table and its columns
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/impl/seq_table_setters.hpp>
#include <serial/iterator.hpp>
#include <serial/objectinfo.hpp>
#include <objects/general/general__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seqfeat/seqfeat__.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/////////////////////////////////////////////////////////////////////////////
// CSeq_feat and CSeq_loc setters
/////////////////////////////////////////////////////////////////////////////


CSeqTableSetField::~CSeqTableSetField()
{
}


void CSeqTableSetField::Set(CSeq_loc& loc, int value) const
{
    NCBI_THROW_FMT(CAnnotException, eOtherError,
                   "Incompatible Seq-loc field value: "<<value);
}


void CSeqTableSetField::Set(CSeq_loc& loc, double value) const
{
    NCBI_THROW_FMT(CAnnotException, eOtherError,
                   "Incompatible Seq-loc field value: "<<value);
}


void CSeqTableSetField::Set(CSeq_loc& loc, const string& value) const
{
    NCBI_THROW_FMT(CAnnotException, eOtherError,
                   "Incompatible Seq-loc field value: "<<value);
}


void CSeqTableSetField::Set(CSeq_feat& feat, int value) const
{
    NCBI_THROW_FMT(CAnnotException, eOtherError,
                   "Incompatible Seq-feat field value: "<<value);
}


void CSeqTableSetField::Set(CSeq_feat& feat, double value) const
{
    NCBI_THROW_FMT(CAnnotException, eOtherError,
                   "Incompatible Seq-feat field value: "<<value);
}


void CSeqTableSetField::Set(CSeq_feat& feat, const string& value) const
{
    NCBI_THROW_FMT(CAnnotException, eOtherError,
                   "Incompatible Seq-feat field value: "<<value);
}


void CSeqTableSetField::Set(CSeq_feat& feat, const vector<char>& value) const
{
    NCBI_THROW_FMT(CAnnotException, eOtherError,
                   "Incompatible Seq-feat field value: vector<char>");
}


void CSeqTableSetComment::Set(CSeq_feat& feat, const string& value) const
{
    feat.SetComment(value);
}


void CSeqTableSetDataImpKey::Set(CSeq_feat& feat, const string& value) const
{
    feat.SetData().SetImp().SetKey(value);
}


void CSeqTableSetDataRegion::Set(CSeq_feat& feat, const string& value) const
{
    feat.SetData().SetRegion(value);
}


void CSeqTableSetLocFuzzFromLim::Set(CSeq_loc& loc, int value) const
{
    if ( loc.IsPnt() ) {
        loc.SetPnt().SetFuzz().SetLim(CInt_fuzz_Base::ELim(value));
    }
    else if ( loc.IsInt() ) {
        loc.SetInt().SetFuzz_from().SetLim(CInt_fuzz_Base::ELim(value));
    }
    else {
        NCBI_THROW_FMT(CAnnotException, eOtherError,
                       "Incompatible fuzz field");
    }
}


void CSeqTableSetLocFuzzFromLim::Set(CSeq_feat& feat, int value) const
{
    Set(feat.SetLocation(), value);
}


void CSeqTableSetLocFuzzToLim::Set(CSeq_loc& loc, int value) const
{
    if ( loc.IsInt() ) {
        loc.SetInt().SetFuzz_to().SetLim(CInt_fuzz_Base::ELim(value));
    }
    else {
        NCBI_THROW_FMT(CAnnotException, eOtherError,
                       "Incompatible fuzz field");
    }
}


void CSeqTableSetLocFuzzToLim::Set(CSeq_feat& feat, int value) const
{
    Set(feat.SetLocation(), value);
}


void CSeqTableSetProdFuzzFromLim::Set(CSeq_loc& loc, int value) const
{
    if ( loc.IsPnt() ) {
        loc.SetPnt().SetFuzz().SetLim(CInt_fuzz_Base::ELim(value));
    }
    else if ( loc.IsInt() ) {
        loc.SetInt().SetFuzz_from().SetLim(CInt_fuzz_Base::ELim(value));
    }
    else {
        NCBI_THROW_FMT(CAnnotException, eOtherError,
                       "Incompatible fuzz field");
    }
}


void CSeqTableSetProdFuzzFromLim::Set(CSeq_feat& feat, int value) const
{
    Set(feat.SetProduct(), value);
}


void CSeqTableSetProdFuzzToLim::Set(CSeq_loc& loc, int value) const
{
    if ( loc.IsInt() ) {
        loc.SetInt().SetFuzz_to().SetLim(CInt_fuzz_Base::ELim(value));
    }
    else {
        NCBI_THROW_FMT(CAnnotException, eOtherError,
                       "Incompatible fuzz field");
    }
}


void CSeqTableSetProdFuzzToLim::Set(CSeq_feat& feat, int value) const
{
    Set(feat.SetProduct(), value);
}


void CSeqTableSetQual::Set(CSeq_feat& feat, const string& value) const
{
    CRef<CGb_qual> qual(new CGb_qual);
    qual->SetQual(name);
    qual->SetVal(value);
    feat.SetQual().push_back(qual);
}


void CSeqTableSetExt::Set(CSeq_feat& feat, int value) const
{
    CRef<CUser_field> field(new CUser_field);
    field->SetLabel().SetStr(name);
    field->SetData().SetInt(value);
    feat.SetExt().SetData().push_back(field);
}


void CSeqTableSetExt::Set(CSeq_feat& feat, double value) const
{
    CRef<CUser_field> field(new CUser_field);
    field->SetLabel().SetStr(name);
    field->SetData().SetReal(value);
    feat.SetExt().SetData().push_back(field);
}


void CSeqTableSetExt::Set(CSeq_feat& feat, const string& value) const
{
    CRef<CUser_field> field(new CUser_field);
    field->SetLabel().SetStr(name);
    field->SetData().SetStr(value);
    feat.SetExt().SetData().push_back(field);
}


void CSeqTableSetExt::Set(CSeq_feat& feat, const vector<char>& value) const
{
    CRef<CUser_field> field(new CUser_field);
    field->SetLabel().SetStr(name);
    field->SetData().SetOs() = value;
    feat.SetExt().SetData().push_back(field);
}


void CSeqTableSetDbxref::Set(CSeq_feat& feat, int value) const
{
    CRef<CDbtag> dbtag(new CDbtag);
    dbtag->SetDb(name);
    dbtag->SetTag().SetId(value);
    feat.SetDbxref().push_back(dbtag);
}


void CSeqTableSetDbxref::Set(CSeq_feat& feat, const string& value) const
{
    CRef<CDbtag> dbtag(new CDbtag);
    dbtag->SetDb(name);
    dbtag->SetTag().SetStr(value);
    feat.SetDbxref().push_back(dbtag);
}


void CSeqTableSetExtType::Set(CSeq_feat& feat, int value) const
{
    feat.SetExt().SetType().SetId(value);
}


void CSeqTableSetExtType::Set(CSeq_feat& feat, const string& value) const
{
    feat.SetExt().SetType().SetStr(value);
}


/////////////////////////////////////////////////////////////////////////////
// CSeqTableSetAnyField
/////////////////////////////////////////////////////////////////////////////


CObjectInfo
CSeqTableNextObjectPointer::GetNextObject(const CObjectInfo& obj) const
{
    return obj.SetPointedObject();
}


CObjectInfo
CSeqTableNextObjectPtrElementNew::GetNextObject(const CObjectInfo& obj) const
{
    return obj.AddNewPointedElement();
}


CObjectInfo
CSeqTableNextObjectElementNew::GetNextObject(const CObjectInfo& obj) const
{
    return obj.AddNewElement();
}


CObjectInfo
CSeqTableNextObjectClassMember::GetNextObject(const CObjectInfo& obj) const
{
    return obj.SetClassMember(m_MemberIndex);
}


CObjectInfo
CSeqTableNextObjectChoiceVariant::GetNextObject(const CObjectInfo& obj) const
{
    return obj.SetChoiceVariant(m_VariantIndex);
}


CSeqTableSetAnyObjField::CSeqTableSetAnyObjField(CObjectTypeInfo type,
                                                 CTempString field)
    : m_SetFinalObject(false)
{
    for ( ;; ) {
        CConstRef<CSeqTableNextObject> next;
        switch ( type.GetTypeFamily() ) {
        default:
            ERR_POST("Incompatible field: "<<
                     type.GetTypeInfo()->GetName()<<" "<<field);
            return;
        case eTypeFamilyPointer:
            next = new CSeqTableNextObjectPointer();
            type = type.GetPointedType();
            break;
        case eTypeFamilyContainer:
            type = type.GetElementType();
            if ( type.GetTypeFamily() == eTypeFamilyPointer ) {
                next = new CSeqTableNextObjectPtrElementNew();
                type = type.GetPointedType();
            }
            else {
                next = new CSeqTableNextObjectElementNew();
            }
            break;
        case eTypeFamilyPrimitive:
            if ( !field.empty() ) {
                ERR_POST("Incompatible field: "<<
                         type.GetTypeInfo()->GetName()<<" "<<field);
                return;
            }
            m_SetFinalObject = true;
            return;
        case eTypeFamilyClass:
        case eTypeFamilyChoice:
            if ( field.empty() ) {
                // no fields to set in the class
                return;
            }
            else {
                size_t dot = field.find('.');
                CTempString field_name = field;
                CTempString next_field;
                if ( dot != CTempString::npos ) {
                    field_name = field.substr(0, dot);
                    next_field = field.substr(dot+1);
                }
                field = next_field;
                if ( type.GetTypeFamily() == eTypeFamilyClass ) {
                    TMemberIndex index = type.FindMemberIndex(field_name);
                    next = new CSeqTableNextObjectClassMember(index);
                    type = type.GetClassTypeInfo()->GetMemberInfo(index)
                        ->GetTypeInfo();
                }
                else {
                    TMemberIndex index = type.FindVariantIndex(field_name);
                    next = new CSeqTableNextObjectChoiceVariant(index);
                    type = type.GetChoiceTypeInfo()->GetVariantInfo(index)
                        ->GetTypeInfo();
                }
            }
            break;
        }
        m_Nexters.push_back(next);
    }
}


void CSeqTableSetAnyObjField::SetObjectField(CObjectInfo obj,
                                             int value) const
{
    ITERATE ( TNexters, it, m_Nexters ) {
        obj = (*it)->GetNextObject(obj);
    }
    if ( m_SetFinalObject ) {
        obj.GetPrimitiveTypeInfo()->SetValueInt(obj.GetObjectPtr(), value);
    }
}


void CSeqTableSetAnyObjField::SetObjectField(CObjectInfo obj,
                                             double value) const
{
    ITERATE ( TNexters, it, m_Nexters ) {
        obj = (*it)->GetNextObject(obj);
    }
    obj.GetPrimitiveTypeInfo()->SetValueDouble(obj.GetObjectPtr(), value);
}


void CSeqTableSetAnyObjField::SetObjectField(CObjectInfo obj,
                                             const string& value) const
{
    ITERATE ( TNexters, it, m_Nexters ) {
        obj = (*it)->GetNextObject(obj);
    }
    obj.GetPrimitiveTypeInfo()->SetValueString(obj.GetObjectPtr(), value);
}


void CSeqTableSetAnyObjField::SetObjectField(CObjectInfo obj,
                                             const vector<char>& value) const
{
    ITERATE ( TNexters, it, m_Nexters ) {
        obj = (*it)->GetNextObject(obj);
    }
    obj.GetPrimitiveTypeInfo()->SetValueOctetString(obj.GetObjectPtr(), value);
}


CSeqTableSetAnyLocField::CSeqTableSetAnyLocField(const CTempString& field)
    : CSeqTableSetAnyObjField(CSeq_loc::GetTypeInfo(), field)
{
}


void CSeqTableSetAnyLocField::Set(CSeq_loc& obj, int value) const
{
    SetObjectField(CObjectInfo(&obj, obj.GetTypeInfo()), value);
}


void CSeqTableSetAnyLocField::Set(CSeq_loc& obj, double value) const
{
    SetObjectField(CObjectInfo(&obj, obj.GetTypeInfo()), value);
}


void CSeqTableSetAnyLocField::Set(CSeq_loc& obj, const string& value) const
{
    SetObjectField(CObjectInfo(&obj, obj.GetTypeInfo()), value);
}


void CSeqTableSetAnyLocField::Set(CSeq_loc& obj,
                                  const vector<char>& value) const
{
    SetObjectField(CObjectInfo(&obj, obj.GetTypeInfo()), value);
}


CSeqTableSetAnyFeatField::CSeqTableSetAnyFeatField(const CTempString& field)
    : CSeqTableSetAnyObjField(CSeq_feat::GetTypeInfo(), field)
{
}


void CSeqTableSetAnyFeatField::Set(CSeq_feat& obj, int value) const
{
    SetObjectField(CObjectInfo(&obj, obj.GetTypeInfo()), value);
}


void CSeqTableSetAnyFeatField::Set(CSeq_feat& obj, double value) const
{
    SetObjectField(CObjectInfo(&obj, obj.GetTypeInfo()), value);
}


void CSeqTableSetAnyFeatField::Set(CSeq_feat& obj, const string& value) const
{
    SetObjectField(CObjectInfo(&obj, obj.GetTypeInfo()), value);
}


void CSeqTableSetAnyFeatField::Set(CSeq_feat& obj,
                                   const vector<char>& value) const
{
    SetObjectField(CObjectInfo(&obj, obj.GetTypeInfo()), value);
}


END_SCOPE(objects)
END_NCBI_SCOPE
