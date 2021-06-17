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


bool CTableFieldHandle_Base::x_ThrowUnsetValue(void) const
{
    NCBI_THROW(CAnnotException, eOtherError,
               "CTableFieldHandle::Get: value is not set");
}


END_SCOPE(objects)
END_NCBI_SCOPE
