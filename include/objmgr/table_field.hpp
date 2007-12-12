#ifndef TABLE_FIELD__HPP
#define TABLE_FIELD__HPP

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
*   Table Seq-annot object information
*
*/

#include <corelib/ncbiobj.hpp>

#include <objmgr/impl/seq_table_info.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CSeq_feat;
class CSeq_annot;
class CSeq_annot_Info;
class CTable_annot_Info;
class CFeat_CI;


class NCBI_XOBJMGR_EXPORT CTableFieldHandle_Base
{
public:
    CTableFieldHandle_Base(const string& name);
    ~CTableFieldHandle_Base();

    const string& GetFieldName(void) const {
        return m_FieldName;
    }

    bool IsSet(const CFeat_CI& feat_ci) const {
        return x_IsSet(x_GetAnnotIndex(feat_ci));
    }
    
protected:
    int x_GetAnnotIndex(const CFeat_CI& feat_ci) const;
    bool x_IsSet(size_t row) const;

    string m_FieldName;
    mutable CSeqTableColumnInfo m_CachedFieldInfo;
    mutable CConstRef<CSeq_annot_Info> m_CachedAnnotInfo;

private:
    CTableFieldHandle_Base(const CTableFieldHandle_Base&);
    void operator=(const CTableFieldHandle_Base&);
};

template<typename FieldType>
class CTableFieldHandle : public CTableFieldHandle_Base
{
public:
    typedef FieldType TFieldType;
    CTableFieldHandle(const string& name)
        : CTableFieldHandle_Base(name) {
    }
    /*
    TFieldType operator()(const CFeat_CI& feat_ci) const {
        const TFieldType* ptr = 0;
        int index = x_GetAnnotIndex(feat_ci);
        return this->m_CachedFieldInfo.GetValue(ptr, index);
    }
    */
};


/////////////////////////////////////////////////////////////////////////////
// CTable_annot_Info
/////////////////////////////////////////////////////////////////////////////

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // TABLE_FIELD__HPP
