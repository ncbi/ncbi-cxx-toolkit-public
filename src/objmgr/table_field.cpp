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
#include <objmgr/impl/seq_table_info.hpp>
#include <objmgr/impl/seq_table_setters.hpp>
#include <objmgr/impl/annot_object_index.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objects/general/general__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <objects/seqtable/seqtable__.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/error_codes.hpp>

#include <objmgr/feat_ci.hpp>
#include <objmgr/table_field.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/////////////////////////////////////////////////////////////////////////////
// CTableFieldHandle_Base
/////////////////////////////////////////////////////////////////////////////


CTableFieldHandle_Base::CTableFieldHandle_Base(int field_id)
    : m_FieldId(field_id)
{
}


CTableFieldHandle_Base::CTableFieldHandle_Base(const string& field_name)
    : m_FieldId(CSeqTable_column_info::GetIdForName(field_name)),
      m_FieldName(field_name)
{
}


CTableFieldHandle_Base::~CTableFieldHandle_Base()
{
}


const CSeqTable_column*
CTableFieldHandle_Base::x_FindColumn(const CSeq_annot_Info& annot) const
{
    if ( &annot != m_CachedAnnotInfo ) {
        m_CachedAnnotInfo = &annot;
        const CSeqTableColumnInfo* column;
        if ( m_FieldId < 0 ) {
            column = annot.GetTableInfo().FindColumn(m_FieldName);
        }
        else {
            column = annot.GetTableInfo().FindColumn(m_FieldId);
        }
        if ( column ) {
            m_CachedFieldInfo = column->Get();
        }
        else {
            m_CachedFieldInfo = null;
        }
    }
    return m_CachedFieldInfo.GetPointerOrNull();
}


inline
const CSeqTable_column*
CTableFieldHandle_Base::x_FindColumn(const CSeq_annot_Handle& annot) const
{
    return x_FindColumn(annot.x_GetInfo());
}


inline
const CSeqTable_column*
CTableFieldHandle_Base::x_FindColumn(const CFeat_CI& feat_ci) const
{
    return x_FindColumn(feat_ci.Get().GetSeq_annot_Info());
}


const CSeqTable_column&
CTableFieldHandle_Base::x_GetColumn(const CSeq_annot_Info& annot) const
{
    const CSeqTable_column* column = x_FindColumn(annot);
    if ( !column ) {
        if ( m_FieldId < 0 ) {
            NCBI_THROW_FMT(CAnnotException, eOtherError,
                           "CTableFieldHandle: "
                           "column "<<m_FieldName<<" not found");
        }
        else {
            NCBI_THROW_FMT(CAnnotException, eOtherError,
                           "CTableFieldHandle: "
                           "column "<<m_FieldId<<" not found");
        }
    }
    return *column;
}


inline
const CSeqTable_column&
CTableFieldHandle_Base::x_GetColumn(const CSeq_annot_Handle& annot) const
{
    return x_GetColumn(annot.x_GetInfo());
}


inline
const CSeqTable_column&
CTableFieldHandle_Base::x_GetColumn(const CFeat_CI& feat_ci) const
{
    return x_GetColumn(feat_ci.Get().GetSeq_annot_Info());
}


inline
size_t CTableFieldHandle_Base::x_GetRow(const CFeat_CI& feat_ci) const
{
    return feat_ci.Get().GetAnnotIndex();
}


bool CTableFieldHandle_Base::IsSet(const CFeat_CI& feat_ci) const
{
    return x_GetColumn(feat_ci).IsSet(x_GetRow(feat_ci));
}


bool CTableFieldHandle_Base::IsSet(const CSeq_annot_Handle& annot,
                                   size_t row) const
{
    return x_GetColumn(annot).IsSet(row);
}


bool CTableFieldHandle_Base::TryGet(const CFeat_CI& feat_ci,
                                    bool& v) const
{
    if ( const CSeqTable_column* column = x_FindColumn(feat_ci) ) {
        return column->TryGetBool(x_GetRow(feat_ci), v);
    }
    return false;
}


void CTableFieldHandle_Base::Get(const CFeat_CI& feat_ci,
                                 bool& v) const
{
    if ( !TryGet(feat_ci, v) ) {
        x_ThrowUnsetValue();
    }
}


bool CTableFieldHandle_Base::TryGet(const CFeat_CI& feat_ci,
                                    int& v) const
{
    if ( const CSeqTable_column* column = x_FindColumn(feat_ci) ) {
        return column->TryGetInt(x_GetRow(feat_ci), v);
    }
    return false;
}


void CTableFieldHandle_Base::Get(const CFeat_CI& feat_ci,
                                 int& v) const
{
    if ( !TryGet(feat_ci, v) ) {
        x_ThrowUnsetValue();
    }
}


bool CTableFieldHandle_Base::TryGet(const CSeq_annot_Handle& annot,
                                    size_t row,
                                    bool& v) const
{
    if ( const CSeqTable_column* column = x_FindColumn(annot) ) {
        return column->TryGetBool(row, v);
    }
    return false;
}


void CTableFieldHandle_Base::Get(const CSeq_annot_Handle& annot,
                                 size_t row,
                                 bool& v) const
{
    if ( !TryGet(annot, row, v) ) {
        x_ThrowUnsetValue();
    }
}


bool CTableFieldHandle_Base::TryGet(const CSeq_annot_Handle& annot,
                                    size_t row,
                                    int& v) const
{
    if ( const CSeqTable_column* column = x_FindColumn(annot) ) {
        return column->TryGetInt(row, v);
    }
    return false;
}


void CTableFieldHandle_Base::Get(const CSeq_annot_Handle& annot,
                                 size_t row,
                                 int& v) const
{
    if ( !TryGet(annot, row, v) ) {
        x_ThrowUnsetValue();
    }
}


bool CTableFieldHandle_Base::TryGet(const CFeat_CI& feat_ci,
                                    double& v) const
{
    if ( const CSeqTable_column* column = x_FindColumn(feat_ci) ) {
        return column->TryGetReal(x_GetRow(feat_ci), v);
    }
    return false;
}


void CTableFieldHandle_Base::Get(const CFeat_CI& feat_ci,
                                 double& v) const
{
    if ( !TryGet(feat_ci, v) ) {
        x_ThrowUnsetValue();
    }
}


bool CTableFieldHandle_Base::TryGet(const CSeq_annot_Handle& annot,
                                    size_t row,
                                    double& v) const
{
    if ( const CSeqTable_column* column = x_FindColumn(annot) ) {
        return column->TryGetReal(row, v);
    }
    return false;
}


void CTableFieldHandle_Base::Get(const CSeq_annot_Handle& annot,
                                 size_t row,
                                 double& v) const
{
    if ( !TryGet(annot, row, v) ) {
        x_ThrowUnsetValue();
    }
}


const string*
CTableFieldHandle_Base::GetPtr(const CFeat_CI& feat_ci,
                               const string* /*dummy*/,
                               bool force) const
{
    const string* ret = 0;
    if ( const CSeqTable_column* column = x_FindColumn(feat_ci) ) {
        ret = column->GetStringPtr(x_GetRow(feat_ci));
    }
    if ( !ret && force ) {
        x_ThrowUnsetValue();
    }
    return ret;
}


bool CTableFieldHandle_Base::TryGet(const CFeat_CI& feat_ci,
                                    string& v) const
{
    const string* ptr = 0;
    ptr = GetPtr(feat_ci, ptr, false);
    if ( ptr ) {
        v = *ptr;
        return true;
    }
    else {
        return false;
    }
}


void CTableFieldHandle_Base::Get(const CFeat_CI& feat_ci,
                                 string& v) const
{
    const string* ptr = 0;
    v = *GetPtr(feat_ci, ptr, true);
}


const string*
CTableFieldHandle_Base::GetPtr(const CSeq_annot_Handle& annot,
                               size_t row,
                               const string* /*dummy*/,
                               bool force) const
{
    const string* ret = 0;
    if ( const CSeqTable_column* column = x_FindColumn(annot) ) {
        ret = column->GetStringPtr(row);
    }
    if ( !ret && force ) {
        x_ThrowUnsetValue();
    }
    return ret;
}


bool CTableFieldHandle_Base::TryGet(const CSeq_annot_Handle& annot,
                                    size_t row,
                                    string& v) const
{
    const string* ptr = 0;
    ptr = GetPtr(annot, row, ptr, false);
    if ( ptr ) {
        v = *ptr;
        return true;
    }
    else {
        return false;
    }
}


void CTableFieldHandle_Base::Get(const CSeq_annot_Handle& annot,
                                 size_t row,
                                 string& v) const
{
    const string* ptr = 0;
    v = *GetPtr(annot, row, ptr, true);
}


const vector<char>*
CTableFieldHandle_Base::GetPtr(const CFeat_CI& feat_ci,
                               const vector<char>* /*dummy*/,
                               bool force) const
{
    const vector<char>* ret = 0;
    if ( const CSeqTable_column* column = x_FindColumn(feat_ci) ) {
        ret = column->GetBytesPtr(x_GetRow(feat_ci));
    }
    if ( !ret && force ) {
        x_ThrowUnsetValue();
    }
    return ret;
}


bool CTableFieldHandle_Base::TryGet(const CFeat_CI& feat_ci,
                                    vector<char>& v) const
{
    const vector<char>* ptr = 0;
    ptr = GetPtr(feat_ci, ptr, false);
    if ( ptr ) {
        v = *ptr;
        return true;
    }
    else {
        return false;
    }
}


void CTableFieldHandle_Base::Get(const CFeat_CI& feat_ci,
                                 vector<char>& v) const
{
    const vector<char>* ptr = 0;
    v = *GetPtr(feat_ci, ptr, true);
}


const vector<char>*
CTableFieldHandle_Base::GetPtr(const CSeq_annot_Handle& annot,
                               size_t row,
                               const vector<char>* /*dummy*/,
                               bool force) const
{
    const vector<char>* ret = 0;
    if ( const CSeqTable_column* column = x_FindColumn(annot) ) {
        ret = column->GetBytesPtr(row);
    }
    if ( !ret && force ) {
        x_ThrowUnsetValue();
    }
    return ret;
}


bool CTableFieldHandle_Base::TryGet(const CSeq_annot_Handle& annot,
                                    size_t row,
                                    vector<char>& v) const
{
    const vector<char>* ptr = 0;
    ptr = GetPtr(annot, row, ptr, false);
    if ( ptr ) {
        v = *ptr;
        return true;
    }
    else {
        return false;
    }
}


void CTableFieldHandle_Base::Get(const CSeq_annot_Handle& annot,
                                 size_t row,
                                 vector<char>& v) const
{
    const vector<char>* ptr = 0;
    v = *GetPtr(annot, row, ptr, true);
}


bool CTableFieldHandle_Base::x_ThrowUnsetValue(void) const
{
    NCBI_THROW(CAnnotException, eOtherError,
               "CTableFieldHandle::Get: value is not set");
}


END_SCOPE(objects)
END_NCBI_SCOPE
