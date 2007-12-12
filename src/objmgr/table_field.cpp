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


CTableFieldHandle_Base::CTableFieldHandle_Base(const string& name)
    : m_FieldName(name)
{
}


CTableFieldHandle_Base::~CTableFieldHandle_Base()
{
}


int CTableFieldHandle_Base::x_GetAnnotIndex(const CFeat_CI& feat_ci) const
{
    return feat_ci.Get().GetAnnotIndex();
}


bool CTableFieldHandle_Base::x_IsSet(size_t row) const
{
    return m_CachedFieldInfo.IsSet(row);
}

/*
int get_table_int(const CFeat_CI& feat_ci, const string& name)
{
    return CTableFieldHandle<int>(name)(feat_ci);
}
*/

END_SCOPE(objects)
END_NCBI_SCOPE
