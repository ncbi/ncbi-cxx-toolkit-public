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


const CSeqTable_column&
CTableFieldHandle_Base::x_GetColumn(const CFeat_CI& feat_ci) const
{
    const CSeq_annot_Info& annot = feat_ci.Get().GetSeq_annot_Info();
    if ( &annot != m_CachedAnnotInfo ) {
        m_CachedAnnotInfo = &annot;
        if ( m_FieldId < 0 ) {
            m_CachedFieldInfo =
                annot.GetTableInfo().GetColumn(m_FieldName).Get();
        }
        else {
            m_CachedFieldInfo =
                annot.GetTableInfo().GetColumn(m_FieldId).Get();
        }
    }
    return *m_CachedFieldInfo;
}


int CTableFieldHandle_Base::x_GetAnnotIndex(const CFeat_CI& feat_ci) const
{
    return feat_ci.Get().GetAnnotIndex();
}


bool CTableFieldHandle_Base::IsSet(const CFeat_CI& feat_ci) const
{
    return x_GetColumn(feat_ci).IsSet(x_GetAnnotIndex(feat_ci));
}


bool CTableFieldHandle_Base::TryGet(const CFeat_CI& feat_ci,
                                    int& v) const
{
    return x_GetColumn(feat_ci).TryGetInt(x_GetAnnotIndex(feat_ci), v);
}


void CTableFieldHandle_Base::Get(const CFeat_CI& feat_ci,
                                 int& v) const
{
    if ( !TryGet(feat_ci, v) ) {
        x_ThrowUnsetValue();
    }
}


bool CTableFieldHandle_Base::TryGet(const CFeat_CI& feat_ci,
                                    double& v) const
{
    return x_GetColumn(feat_ci).TryGetReal(x_GetAnnotIndex(feat_ci), v);
}


void CTableFieldHandle_Base::Get(const CFeat_CI& feat_ci,
                                 double& v) const
{
    if ( !TryGet(feat_ci, v) ) {
        x_ThrowUnsetValue();
    }
}


const string*
CTableFieldHandle_Base::GetPtr(const CFeat_CI& feat_ci,
                               const string* /*dummy*/,
                               bool force) const
{
    const string* ret =
        x_GetColumn(feat_ci).GetStringPtr(x_GetAnnotIndex(feat_ci));
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


const vector<char>*
CTableFieldHandle_Base::GetPtr(const CFeat_CI& feat_ci,
                               const vector<char>* /*dummy*/,
                               bool force) const
{
    const vector<char>* ret =
        x_GetColumn(feat_ci).GetBytesPtr(x_GetAnnotIndex(feat_ci));
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


bool CTableFieldHandle_Base::x_ThrowUnsetValue(void) const
{
    NCBI_THROW(CAnnotException, eOtherError,
               "CTableFieldHandle::Get: value is not set");
}


END_SCOPE(objects)
END_NCBI_SCOPE
