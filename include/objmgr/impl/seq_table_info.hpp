#ifndef SEQ_TABLE_INFO__HPP
#define SEQ_TABLE_INFO__HPP

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
*   Seq-table information
*
*/

#include <corelib/ncbiobj.hpp>
#include <objmgr/impl/seq_table_setter.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <objects/seqtable/SeqTable_column.hpp>
#include <objects/seqtable/SeqTable_column_info.hpp>
#include <util/range.hpp>
#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataSource;
class CSeq_table;
class CSeq_loc;
class CSeq_interval;
class CSeq_point;
class CSeq_feat;
struct SAnnotObject_Key;
struct SAnnotObject_Index;

/////////////////////////////////////////////////////////////////////////////
// information about Seq-table column
/////////////////////////////////////////////////////////////////////////////

class CSeqTableColumnInfo
{
public:
    CSeqTableColumnInfo(void)
        {
        }
    CSeqTableColumnInfo(const CSeqTable_column& column)
        : m_Column(&column)
        {
        }
    ~CSeqTableColumnInfo()
        {
        }

    DECLARE_OPERATOR_BOOL_REF(m_Column);

    const CSeqTable_column* Get(void) const
        {
            return m_Column.GetPointer();
        }
    const CSeqTable_column* operator->(void) const
        {
            return m_Column.GetPointer();
        }

    bool IsSet(size_t row) const;

    bool GetBool(size_t row, bool& v, bool force = false) const
        {
            return m_Column->TryGetBool(row, v) ||
                (force && x_ThrowUnsetValue());
        }
    bool GetInt(size_t row, int& v, bool force = false) const
        {
            return m_Column->TryGetInt(row, v) ||
                (force && x_ThrowUnsetValue());
        }
    bool GetReal(size_t row, double& v, bool force = false) const
        {
            return m_Column->TryGetReal(row, v) ||
                (force && x_ThrowUnsetValue());
        }
    bool GetString(size_t row, string& v, bool force = false) const;
    bool GetUtf8String(size_t row, CStringUTF8& v, bool force = false) const;
    bool GetBytes(size_t row, vector<char>& v, bool force = false) const;
    const string* GetStringPtr(size_t row, bool force = false) const;
    const CStringUTF8* GetUtf8StringPtr(size_t row, bool force = false) const;
    const vector<char>* GetBytesPtr(size_t row, bool force = false) const;
    CConstRef<CSeq_id> GetSeq_id(size_t row, bool force = false) const;
    CConstRef<CSeq_loc> GetSeq_loc(size_t row, bool force = false) const;

    bool GetValue(size_t row, bool& value, bool force = false) const
        {
            return GetBool(row, value, force);
        }
    bool GetValue(size_t row, int& value, bool force = false) const
        {
            return GetInt(row, value, force);
        }
    bool GetValue(size_t row, double& value, bool force = false) const
        {
            return GetReal(row, value, force);
        }
    bool GetValue(size_t row, string& value, bool force = false) const
        {
            return GetString(row, value, force);
        }
    bool GetValue(size_t row, CStringUTF8& value, bool force = false) const
        {
            return GetUtf8String(row, value, force);
        }
    bool GetValue(size_t row, vector<char>& value, bool force = false) const
        {
            return GetBytes(row, value, force);
        }

    void UpdateSeq_loc(CSeq_loc& loc, size_t row,
                       const CSeqTableSetLocField& setter) const;
    void UpdateSeq_loc(CSeq_loc& loc,
                       const CSeqTable_single_data& data,
                       const CSeqTableSetLocField& setter) const;
    bool UpdateSeq_loc(CSeq_loc& loc,
                       const CSeqTable_multi_data& data,
                       size_t index,
                       const CSeqTableSetLocField& setter) const;
    void UpdateSeq_feat(CSeq_feat& feat, size_t row,
                        const CSeqTableSetFeatField& setter) const;
    void UpdateSeq_feat(CSeq_feat& feat,
                        const CSeqTable_single_data& data,
                        const CSeqTableSetFeatField& setter) const;
    bool UpdateSeq_feat(CSeq_feat& feat,
                        const CSeqTable_multi_data& data,
                        size_t index,
                        const CSeqTableSetFeatField& setter) const;

protected:
    // report unset value with exception
    bool x_ThrowUnsetValue(void) const;

private:
    CConstRef<CSeqTable_column> m_Column;
};

class CSeqTableLocColumns
{
public:
    CSeqTableLocColumns(const char* field_name,
                        CSeqTable_column_info::EField_id base_value);
    ~CSeqTableLocColumns();

    bool AddColumn(const CSeqTable_column& column);

    void SetColumn(CSeqTableColumnInfo& field,
                   const CSeqTable_column& column);
    void AddExtraColumn(const CSeqTable_column& column,
                        const CSeqTableSetLocField* setter);

    void ParseDefaults(void);

    void UpdateSeq_loc(size_t row,
                       CRef<CSeq_loc>& seq_loc,
                       CRef<CSeq_point>& seq_pnt,
                       CRef<CSeq_interval>& seq_int) const;

    CConstRef<CSeq_loc> GetLoc(size_t row) const;
    CConstRef<CSeq_id> GetId(size_t row) const;
    CSeq_id_Handle GetIdHandle(size_t row) const;
    CRange<TSeqPos> GetRange(size_t row) const;
    ENa_strand GetStrand(size_t row) const;

    bool IsSet(void) const {
        return m_Is_set;
    }
    bool IsRealLoc(void) const {
        return m_Is_real_loc;
    }

    void SetTableKeyAndIndex(size_t row,
                             SAnnotObject_Key& key,
                             SAnnotObject_Index& index) const;

private:
    CTempString m_FieldName;
    CSeqTable_column_info::EField_id m_BaseValue;

    bool m_Is_set, m_Is_real_loc;
    bool m_Is_simple, m_Is_probably_simple;
    bool m_Is_simple_point, m_Is_simple_interval, m_Is_simple_whole;

    typedef CSeqTableSetLocField TSetter;
    typedef pair<CSeqTableColumnInfo, CConstRef<TSetter> > TColumnInfo;
    typedef vector<TColumnInfo> TExtraColumns;

    CSeqTableColumnInfo m_Loc;
    CSeqTableColumnInfo m_Id;
    CSeqTableColumnInfo m_Gi;
    CSeqTableColumnInfo m_From;
    CSeqTableColumnInfo m_To;
    CSeqTableColumnInfo m_Strand;
    TExtraColumns m_ExtraColumns;

    CSeq_id_Handle m_DefaultIdHandle;

private:
    CSeqTableLocColumns(const CSeqTableLocColumns&);
    void operator=(const CSeqTableLocColumns&);
};


class CSeqTableInfo : public CObject
{
public:
    explicit CSeqTableInfo(const CSeq_table& feat_table, bool is_feat);
    explicit CSeqTableInfo(const CSeq_table& feat_table);
    ~CSeqTableInfo();

    bool IsFeatTable(void) const {
        return m_IsFeatTable;
    }

    void UpdateSeq_feat(size_t row,
                        CRef<CSeq_feat>& seq_feat,
                        CRef<CSeq_point>& seq_pnt,
                        CRef<CSeq_interval>& seq_int) const;

    const CSeqTableLocColumns& GetLocation(void) const {
        return m_Location;
    }
    const CSeqTableLocColumns& GetProduct(void) const {
        return m_Product;
    }
    bool IsPartial(size_t row) const;

    // returns null if column not found
    const CSeqTableColumnInfo* FindColumn(int field_id) const;
    const CSeqTableColumnInfo* FindColumn(const string& field_name) const;
    // throws an exception if column not found
    const CSeqTableColumnInfo& GetColumn(int field_id) const;
    const CSeqTableColumnInfo& GetColumn(const string& field_name) const;

    static bool IsGoodFeatTable(const CSeq_table& table);

private:
    typedef CSeqTableSetFeatField TSetter;
    typedef pair<CSeqTableColumnInfo, CConstRef<TSetter> > TColumnInfo;
    typedef vector<TColumnInfo> TExtraColumns;

    void x_Initialize(const CSeq_table& table);

    bool m_IsFeatTable;
    CSeqTableLocColumns m_Location;
    CSeqTableLocColumns m_Product;
    CSeqTableColumnInfo m_Partial;
    TExtraColumns m_ExtraColumns;

    typedef map<int, CSeqTableColumnInfo> TColumnsById;
    typedef map<string, CSeqTableColumnInfo> TColumnsByName;
    TColumnsById m_ColumnsById;
    TColumnsByName m_ColumnsByName;

private:
    CSeqTableInfo(const CSeqTableInfo&);
    void operator=(const CSeqTableInfo&);
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // SEQ_TABLE_INFO__HPP
