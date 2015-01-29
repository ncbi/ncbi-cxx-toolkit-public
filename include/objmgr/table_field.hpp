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

#include <objects/seqtable/SeqTable_column_info.hpp>
#include <objects/seqtable/SeqTable_column.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CSeq_feat;
class CSeq_annot;
class CSeq_annot_Info;
class CSeq_annot_Handle;
class CTable_annot_Info;
class CFeat_CI;


class NCBI_XOBJMGR_EXPORT CTableFieldHandle_Base
{
public:
    explicit
    CTableFieldHandle_Base(CSeqTable_column_info::TField_id field_id);
    explicit
    CTableFieldHandle_Base(const string& field_name);
    ~CTableFieldHandle_Base();

    int GetFieldId(void) const {
        return m_FieldId;
    }
    const string& GetFieldName(void) const {
        return m_FieldName;
    }

    bool IsSet(const CFeat_CI& feat_ci) const {
        return x_GetColumn(feat_ci).IsSet(x_GetRow(feat_ci));
    }
    bool IsSet(const CSeq_annot_Handle& annot, size_t row) const {
        return x_GetColumn(annot).IsSet(row);
    }

    template<class Value>
    bool TryGet(const CFeat_CI& feat_ci, Value& v) const {
        if ( const CSeqTable_column* column = x_FindColumn(feat_ci) ) {
            return column->TryGetValue(x_GetRow(feat_ci), v);
        }
        return false;
    }

    template<class Value>
    bool TryGet(const CSeq_annot_Handle& annot, size_t row, Value& v) const {
        if ( const CSeqTable_column* column = x_FindColumn(annot) ) {
            return column->TryGetValue(row, v);
        }
        return false;
    }

    const string* GetPtr(const CFeat_CI& feat_ci,
                         const string* dummy,
                         bool force = false) const;
    const vector<char>* GetPtr(const CFeat_CI& feat_ci,
                               const vector<char>* dummy,
                               bool force = false) const;
    const string* GetPtr(const CSeq_annot_Handle& annot,
                         size_t row,
                         const string* dummy,
                         bool force = false) const;
    const vector<char>* GetPtr(const CSeq_annot_Handle& annot,
                               size_t row,
                               const vector<char>* dummy,
                               bool force = false) const;
    
protected:
    bool x_ThrowUnsetValue(void) const;

    const CSeqTable_column* x_FindColumn(const CSeq_annot_Info& annot) const;
    const CSeqTable_column& x_GetColumn(const CSeq_annot_Info& annot) const;

    const CSeqTable_column* x_FindColumn(const CSeq_annot_Handle& annot) const {
        return x_FindColumn(annot.x_GetInfo());
    }
    const CSeqTable_column* x_FindColumn(const CFeat_CI& feat_ci) const {
        return x_FindColumn(feat_ci.Get().GetSeq_annot_Info());
    }

    const CSeqTable_column& x_GetColumn(const CSeq_annot_Handle& annot) const {
        return x_GetColumn(annot.x_GetInfo());
    }
    const CSeqTable_column& x_GetColumn(const CFeat_CI& feat_ci) const {
        return x_GetColumn(feat_ci.Get().GetSeq_annot_Info());
    }

    size_t x_GetRow(const CFeat_CI& feat_ci) const {
        return feat_ci.Get().GetAnnotIndex();
    }

    int m_FieldId;
    string m_FieldName;
    mutable CConstRef<CSeqTable_column> m_CachedFieldInfo;
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
    CTableFieldHandle(CSeqTable_column_info::TField_id field_id)
        : CTableFieldHandle_Base(field_id) {
    }
    CTableFieldHandle(const string& field_name)
        : CTableFieldHandle_Base(field_name) {
    }
    /*
    const TFieldType* GetPtr(const CFeat_CI& feat_ci) const {
        const TFieldType* dummy = 0;
        return CTableFieldHandle_Base::GetPtr(feat_ci, dummy, false);
    }
    */
    void Get(const CFeat_CI& feat_ci, TFieldType& v) const {
        if ( !TryGet(feat_ci, v) ) {
            x_ThrowUnsetValue();
        }
    }
    
    void Get(const CSeq_annot_Handle& annot, size_t row, TFieldType& v) const {
        if ( !TryGet(annot, row, v) ) {
            x_ThrowUnsetValue();
        }
    }

    TFieldType Get(const CFeat_CI& feat_ci) const {
        TFieldType value;
        Get(feat_ci, value);
        return value;
    }

    TFieldType Get(const CSeq_annot_Handle& annot, size_t row) const {
        TFieldType value;
        Get(annot, row, value);
        return value;
    }
};


/////////////////////////////////////////////////////////////////////////////
// CTable_annot_Info
/////////////////////////////////////////////////////////////////////////////


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // TABLE_FIELD__HPP
