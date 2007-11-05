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
// CSeq_feat and CSeq_loc field setters
/////////////////////////////////////////////////////////////////////////////

class CSeqTableSetField : public CObject
{
public:
    virtual ~CSeqTableSetField();

    virtual void Set(CSeq_loc& loc, int value) const;
    virtual void Set(CSeq_loc& loc, double value) const;
    virtual void Set(CSeq_loc& loc, const string& value) const;
    virtual void Set(CSeq_feat& feat, int value) const;
    virtual void Set(CSeq_feat& feat, double value) const;
    virtual void Set(CSeq_feat& feat, const string& value) const;
};


class CSeqTableSetComment : public CSeqTableSetField
{
public:
    virtual void Set(CSeq_feat& feat, const string& value) const;
};


class CSeqTableSetDataImpKey : public CSeqTableSetField
{
public:
    virtual void Set(CSeq_feat& feat, const string& value) const;
};


class CSeqTableSetDataRegion : public CSeqTableSetField
{
public:
    virtual void Set(CSeq_feat& feat, const string& value) const;
};


class CSeqTableSetLocFuzzFromLim : public CSeqTableSetField
{
public:
    virtual void Set(CSeq_loc& loc, int value) const;
    virtual void Set(CSeq_feat& feat, int value) const;
};


class CSeqTableSetLocFuzzToLim : public CSeqTableSetField
{
public:
    virtual void Set(CSeq_loc& loc, int value) const;
    virtual void Set(CSeq_feat& feat, int value) const;
};


class CSeqTableSetProdFuzzFromLim : public CSeqTableSetField
{
public:
    virtual void Set(CSeq_loc& loc, int value) const;
    virtual void Set(CSeq_feat& feat, int value) const;
};


class CSeqTableSetProdFuzzToLim : public CSeqTableSetField
{
public:
    virtual void Set(CSeq_loc& loc, int value) const;
    virtual void Set(CSeq_feat& feat, int value) const;
};


class CSeqTableSetQual : public CSeqTableSetField
{
public:
    CSeqTableSetQual(const CTempString& name)
        : name(name.substr(2))
        {
        }

    virtual void Set(CSeq_feat& feat, const string& value) const;

private:
    CTempString name;
};


class CSeqTableSetExt : public CSeqTableSetField
{
public:
    CSeqTableSetExt(const CTempString& name)
        : name(name.substr(2))
        {
        }

    virtual void Set(CSeq_feat& feat, int value) const;
    virtual void Set(CSeq_feat& feat, double value) const;
    virtual void Set(CSeq_feat& feat, const string& value) const;

private:
    CTempString name;
};


class CSeqTableSetDbxref : public CSeqTableSetField
{
public:
    CSeqTableSetDbxref(const CTempString& name)
        : name(name.substr(2))
        {
        }

    virtual void Set(CSeq_feat& feat, int value) const;
    virtual void Set(CSeq_feat& feat, const string& value) const;

private:
    CTempString name;
};


class CSeqTableSetExtType : public CSeqTableSetField
{
public:
    virtual void Set(CSeq_feat& feat, int value) const;
    virtual void Set(CSeq_feat& feat, const string& value) const;
};


/////////////////////////////////////////////////////////////////////////////
// information about Seq-table column
/////////////////////////////////////////////////////////////////////////////

class CSeqTableColumnInfo
{
public:
    CSeqTableColumnInfo(void)
        {
        }
    explicit CSeqTableColumnInfo(const CSeqTable_column& column,
                                 CConstRef<CSeqTableSetField> setter)
        : m_Column(&column), m_Setter(setter)
        {
        }
    ~CSeqTableColumnInfo()
        {
        }

    DECLARE_OPERATOR_BOOL_REF(m_Column);

    void SetColumn(const CSeqTable_column& column,
                   CConstRef<CSeqTableSetField> setter)
        {
            m_Column = &column;
            m_Setter = setter;
        }

    const CSeqTable_column* operator->(void) const
        {
            return m_Column.GetPointer();
        }

    size_t GetRowIndex(size_t row) const;
    bool IsSet(size_t row) const;
    bool GetBool(size_t row) const;
    int GetInt(size_t row, int def) const;
    const string& GetString(size_t row) const;
    const CSeq_id* GetSeq_id(size_t row) const;
    const CSeq_loc* GetSeq_loc(size_t row) const;

    void UpdateSeq_loc(CSeq_loc& loc, size_t row) const;
    void UpdateSeq_feat(CSeq_feat& feat, size_t row) const;

private:
    CConstRef<CSeqTable_column> m_Column;
    CConstRef<CSeqTableSetField> m_Setter;
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
                        const CSeqTableSetField* setter);

    void ParseDefaults(void);

    void UpdateSeq_loc(size_t row,
                       CRef<CSeq_loc>& seq_loc,
                       CRef<CSeq_point>& seq_pnt,
                       CRef<CSeq_interval>& seq_int) const;

    const CSeq_loc& GetLoc(size_t row) const;
    const CSeq_id& GetId(size_t row) const;
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

    CSeqTableColumnInfo m_Loc;
    CSeqTableColumnInfo m_Id;
    CSeqTableColumnInfo m_Gi;
    CSeqTableColumnInfo m_From;
    CSeqTableColumnInfo m_To;
    CSeqTableColumnInfo m_Strand;
    typedef vector<CSeqTableColumnInfo> TExtraColumns;
    TExtraColumns m_ExtraColumns;

    CSeq_id_Handle m_DefaultIdHandle;

private:
    CSeqTableLocColumns(const CSeqTableLocColumns&);
    void operator=(const CSeqTableLocColumns&);
};


class CSeqTableInfo : public CObject
{
public:
    explicit CSeqTableInfo(const CSeq_table& feat_table);
    ~CSeqTableInfo();

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
    
private:
    CSeqTableLocColumns m_Location;
    CSeqTableLocColumns m_Product;
    CSeqTableColumnInfo m_Partial;
    typedef vector<CSeqTableColumnInfo> TExtraColumns;
    TExtraColumns m_ExtraColumns;

private:
    CSeqTableInfo(const CSeqTableInfo&);
    void operator=(const CSeqTableInfo&);
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // SEQ_TABLE_INFO__HPP
