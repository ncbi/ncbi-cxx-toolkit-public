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
#include <objmgr/impl/annot_object_index.hpp>
#include <objects/general/general__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <objects/seqtable/seqtable__.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/error_codes.hpp>

#define NCBI_USE_ERRCODE_X   ObjMgr_SeqTable

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
    Set(feat, NStr::IntToString(value));
}


void CSeqTableSetField::Set(CSeq_feat& feat, double value) const
{
    Set(feat, NStr::DoubleToString(value));
}


void CSeqTableSetField::Set(CSeq_feat& feat, const string& value) const
{
    NCBI_THROW_FMT(CAnnotException, eOtherError,
                   "Incompatible Seq-feat field value: "<<value);
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


void CSeqTableSetExtType::Set(CSeq_feat& feat, int value) const
{
    feat.SetExt().SetType().SetId(value);
}


void CSeqTableSetExtType::Set(CSeq_feat& feat, const string& value) const
{
    feat.SetExt().SetType().SetStr(value);
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


/////////////////////////////////////////////////////////////////////////////
// CSeqTableColumnInfo
/////////////////////////////////////////////////////////////////////////////


size_t CSeqTableColumnInfo::GetRowIndex(size_t row) const
{
    if ( !m_Column->IsSetSparse() ) {
        return row;
    }
    const CSeqTable_sparse_index::TIndexes& idx =
        m_Column->GetSparse().GetIndexes();
    CSeqTable_sparse_index::TIndexes::const_iterator iter =
        lower_bound(idx.begin(), idx.end(), int(row));
    if ( iter != idx.end() && *iter == int(row) ) {
        return iter-idx.begin();
    }
    return size_t(-1); // maximum
}


bool CSeqTableColumnInfo::IsSet(size_t row) const
{
    if ( m_Column->IsSetData() ) {
        size_t index = GetRowIndex(row);
        const CSeqTable_multi_data& data = m_Column->GetData();
        switch ( data.Which() ) {
        case CSeqTable_multi_data::e_Int:
            if ( index < data.GetInt().size() ) {
                return true;
            }
            break;
        case CSeqTable_multi_data::e_Real:
            if ( index < data.GetReal().size() ) {
                return true;
            }
            break;
        case CSeqTable_multi_data::e_String:
            if ( index < data.GetString().size() ) {
                return true;
            }
            break;
        default:
            ERR_POST_X(1, "Bad field data type: "<<data.Which());
            break;
        }
    }
    return m_Column->IsSetDefault();
}


bool CSeqTableColumnInfo::GetBool(size_t row) const
{
    if ( m_Column->IsSetData() ) {
        size_t index = GetRowIndex(row);
        const CSeqTable_multi_data::TBit& arr = m_Column->GetData().GetBit();
        if ( index < arr.size()*8 ) {
            size_t byte = index / 8;
            size_t bit = index % 8;
            return ((arr[byte]<<bit)&0x80) != 0;
        }
    }
    return m_Column->IsSetDefault() && m_Column->GetDefault().GetBit();
}


int CSeqTableColumnInfo::GetInt(size_t row, int def) const
{
    if ( m_Column->IsSetData() ) {
        size_t index = GetRowIndex(row);
        const CSeqTable_multi_data::TInt& arr = m_Column->GetData().GetInt();
        if ( index < arr.size() ) {
            return arr[index];
        }
    }
    return m_Column->IsSetDefault()? m_Column->GetDefault().GetInt(): def;
}


const string& CSeqTableColumnInfo::GetString(size_t row) const
{
    if ( m_Column->IsSetData() ) {
        size_t index = GetRowIndex(row);
        const CSeqTable_multi_data::TString& arr =
            m_Column->GetData().GetString();
        if ( index < arr.size() ) {
            return arr[index];
        }
    }
    return m_Column->GetDefault().GetString();
}


const CSeq_id* CSeqTableColumnInfo::GetSeq_id(size_t row) const
{
    if ( m_Column->IsSetData() ) {
        size_t index = GetRowIndex(row);
        const CSeqTable_multi_data::TId& arr =
            m_Column->GetData().GetId();
        if ( index < arr.size() ) {
            return arr[index].GetPointer();
        }
    }
    if ( m_Column->IsSetDefault() ) {
        return &m_Column->GetDefault().GetId();
    }
    return 0;
}


const CSeq_loc* CSeqTableColumnInfo::GetSeq_loc(size_t row) const
{
    if ( m_Column->IsSetData() ) {
        size_t index = GetRowIndex(row);
        const CSeqTable_multi_data::TLoc& arr =
            m_Column->GetData().GetLoc();
        if ( index < arr.size() ) {
            return arr[index].GetPointer();
        }
    }
    if ( m_Column->IsSetDefault() ) {
        return &m_Column->GetDefault().GetLoc();
    }
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
// CSeqTableLocColumns
/////////////////////////////////////////////////////////////////////////////


static void sx_UpdateField(CSeq_loc& loc,
                           const CSeqTableColumnInfo& column,
                           size_t row);
static void sx_UpdateField(CSeq_feat& feat,
                           const CSeqTableColumnInfo& column,
                           size_t row);

static inline
void sx_UpdateFields(CSeq_loc& loc,
                     const vector<CSeqTableColumnInfo>& cols,
                     size_t row)
{
    ITERATE ( vector<CSeqTableColumnInfo>, it, cols ) {
        sx_UpdateField(loc, *it, row);
    }
}


static inline
void sx_UpdateFields(CSeq_feat& feat,
                     const vector<CSeqTableColumnInfo>& cols,
                     size_t row)
{
    ITERATE ( vector<CSeqTableColumnInfo>, it, cols ) {
        sx_UpdateField(feat, *it, row);
    }
}



CSeqTableLocColumns::CSeqTableLocColumns(
    const char* field_name,
    CSeqTable_column_info::C_Field::EField_int base_value
)
    : m_FieldName(field_name),
      m_BaseValue(base_value),
      m_Is_set(false),
      m_Is_real_loc(false),
      m_Is_simple(false),
      m_Is_probably_simple(false),
      m_Is_simple_point(false),
      m_Is_simple_interval(false),
      m_Is_simple_whole(false)
{
}


CSeqTableLocColumns::~CSeqTableLocColumns()
{
}


void CSeqTableLocColumns::SetColumn(CSeqTableColumnInfo& field,
                                    const CSeqTable_column& column)
{
    if ( field ) {
        NCBI_THROW_FMT(CAnnotException, eBadLocation,
                       "Duplicate "<<m_FieldName<<" column");
    }
    field.SetColumn(column, null);
    m_Is_set = true;
}


static CConstRef<CSeqTableSetField> sx_GetLocSetter(int field)
{
    return null;
}


static CConstRef<CSeqTableSetField> sx_GetLocSetter(const CTempString& field)
{
    return null;
}


static CConstRef<CSeqTableSetField> sx_GetFeatSetter(int field)
{
    return null;
}


static CConstRef<CSeqTableSetField> sx_GetFeatSetter(const CTempString& field)
{
    return null;
}


bool CSeqTableLocColumns::AddColumn(const CSeqTable_column& column)
{
    const CSeqTable_column_info& type = column.GetHeader();
    if ( type.GetField().IsInt() ) {
        int field = type.GetField().GetInt() - m_BaseValue +
            CSeqTable_column_info::C_Field::eField_int_location;
        if ( field >= CSeqTable_column_info::C_Field::eField_int_product ) {
            return false;
        }
        switch ( field ) {
        case CSeqTable_column_info::C_Field::eField_int_location:
            SetColumn(m_Loc, column);
            return true;
        case CSeqTable_column_info::C_Field::eField_int_location_id:
            SetColumn(m_Id, column);
            return true;
        case CSeqTable_column_info::C_Field::eField_int_location_gi:
            SetColumn(m_Gi, column);
            return true;
        case CSeqTable_column_info::C_Field::eField_int_location_from:
            SetColumn(m_From, column);
            return true;
        case CSeqTable_column_info::C_Field::eField_int_location_to:
            SetColumn(m_To, column);
            return true;
        case CSeqTable_column_info::C_Field::eField_int_location_strand:
            SetColumn(m_Strand, column);
            return true;
        default:
            m_ExtraColumns.push_back
                (CSeqTableColumnInfo(column, sx_GetLocSetter(field)));
            m_Is_set = true;
            return true;
        }
    }
    else {
        CTempString field(type.GetField().GetStr());
        if ( field == m_FieldName ) {
            SetColumn(m_Loc, column);
            return true;
        }
        else if ( NStr::StartsWith(field, m_FieldName) &&
                  field[m_FieldName.size()] == '.' ) {
            CTempString extra = field.substr(m_FieldName.size()+1);
            if ( extra == "id" || NStr::EndsWith(extra, ".id") ) {
                SetColumn(m_Id, column);
            }
            else if ( extra == "gi" || NStr::EndsWith(extra, ".gi") ) {
                SetColumn(m_Gi, column);
            }
            else if ( extra == "pnt.point" || extra == "int.from" ) {
                SetColumn(m_From, column);
            }
            else if ( extra == "int.to" ) {
                SetColumn(m_To, column);
            }
            else if ( extra == "strand" ||
                      NStr::EndsWith(extra, ".strand") ) {
                SetColumn(m_Strand, column);
            }
            else {
                m_ExtraColumns.push_back
                    (CSeqTableColumnInfo(column, sx_GetLocSetter(extra)));
                m_Is_set = true;
            }
            return true;
        }
        else {
            return false;
        }
    }
}


void CSeqTableLocColumns::ParseDefaults(void)
{
    if ( !m_Is_set ) {
        return;
    }
    if ( m_Loc ) {
        m_Is_real_loc = true;
        if ( m_Id || m_Gi || m_From || m_To || m_Strand ||
             !m_ExtraColumns.empty() ) {
            NCBI_THROW_FMT(CAnnotException, eBadLocation,
                           "Conflicting "<<m_FieldName<<" columns");
        }
        return;
    }

    if ( !m_Id && !m_Gi ) {
        NCBI_THROW_FMT(CAnnotException, eBadLocation,
                       "No "<<m_FieldName<<".id column");
    }
    if ( m_Id && m_Gi ) {
        NCBI_THROW_FMT(CAnnotException, eBadLocation,
                       "Conflicting "<<m_FieldName<<" columns");
    }
    if ( m_Id ) {
        if ( m_Id->IsSetDefault() ) {
            m_DefaultIdHandle =
                CSeq_id_Handle::GetHandle(m_Id->GetDefault().GetId());
        }
    }
    if ( m_Gi ) {
        if ( m_Gi->IsSetDefault() ) {
            m_DefaultIdHandle =
                CSeq_id_Handle::GetGiHandle(m_Gi->GetDefault().GetInt());
        }
    }

    if ( m_To ) {
        // interval
        if ( !m_From ) {
            NCBI_THROW_FMT(CAnnotException, eBadLocation,
                           "column "<<m_FieldName<<".to without "<<
                           m_FieldName<<".from");
        }
        m_Is_simple_interval = true;
    }
    else if ( m_From ) {
        // point
        m_Is_simple_point = true;
    }
    else {
        // whole
        if ( m_Strand || !m_ExtraColumns.empty() ) {
            NCBI_THROW_FMT(CAnnotException, eBadLocation,
                           "extra columns in whole "<<m_FieldName);
        }
        m_Is_simple_whole = true;
    }
    if ( m_ExtraColumns.empty() ) {
        m_Is_simple = true;
    }
    else {
        m_Is_probably_simple = true;
    }
}


const CSeq_loc& CSeqTableLocColumns::GetLoc(size_t row) const
{
    _ASSERT(m_Loc);
    _ASSERT(!m_Loc->IsSetDefault());
    return *m_Loc.GetSeq_loc(row);
}


const CSeq_id& CSeqTableLocColumns::GetId(size_t row) const
{
    _ASSERT(!m_Loc);
    _ASSERT(m_Id);
    return *m_Id.GetSeq_id(row);
}


CSeq_id_Handle CSeqTableLocColumns::GetIdHandle(size_t row) const
{
    _ASSERT(!m_Loc);
    if ( m_Id ) {
        _ASSERT(!m_Id->IsSetSparse());
        if ( m_Id->IsSetData() ) {
            const CSeq_id* id = m_Id.GetSeq_id(row);
            if ( id ) {
                return CSeq_id_Handle::GetHandle(*id);
            }
        }
    }
    else {
        _ASSERT(!m_Gi->IsSetSparse());
        if ( m_Gi->IsSetData() ) {
            int gi = m_Gi.GetInt(row, 0);
            if ( gi ) {
                return CSeq_id_Handle::GetGiHandle(gi);
            }
        }
    }
    return m_DefaultIdHandle;
}


CRange<TSeqPos> CSeqTableLocColumns::GetRange(size_t row) const
{
    _ASSERT(!m_Loc);
    _ASSERT(m_From);
    if ( !m_From ) {
        return CRange<TSeqPos>::GetWhole();
    }
    TSeqPos from = m_From.GetInt(row, kInvalidSeqPos);
    if ( from == kInvalidSeqPos ) {
        return CRange<TSeqPos>::GetWhole();
    }
    TSeqPos to = m_To? m_To.GetInt(row, from): from;
    return CRange<TSeqPos>(from, to);
}


ENa_strand CSeqTableLocColumns::GetStrand(size_t row) const
{
    _ASSERT(!m_Loc);
    if ( !m_Strand ) {
        return eNa_strand_unknown;
    }
    return ENa_strand(m_Strand.GetInt(row, eNa_strand_unknown));
}


void CSeqTableLocColumns::UpdateSeq_loc(size_t row,
                                        CRef<CSeq_loc>& seq_loc,
                                        CRef<CSeq_point>& seq_pnt,
                                        CRef<CSeq_interval>& seq_int) const
{
    _ASSERT(m_Is_set);
    if ( m_Loc ) {
        seq_loc = &const_cast<CSeq_loc&>(GetLoc(row));
        return;
    }
    if ( !seq_loc ) {
        seq_loc = new CSeq_loc();
    }
    CSeq_loc& loc = *seq_loc;

    CSeq_id* id = 0;
    int gi = 0;
    if ( m_Id ) {
        id = &const_cast<CSeq_id&>(GetId(row));
    }
    else {
        _ASSERT(m_Gi);
        gi = m_Gi.GetInt(row, 0);
    }

    TSeqPos from = !m_From? kInvalidSeqPos: m_From.GetInt(row, kInvalidSeqPos);
    if ( from == kInvalidSeqPos ) {
        // whole
        if ( id ) {
            loc.SetWhole(*id);
        }
        else {
            loc.SetWhole().SetGi(gi);
        }
    }
    else {
        TSeqPos to = !m_To? kInvalidSeqPos: m_To.GetInt(row, kInvalidSeqPos);

        int strand = !m_Strand? -1: m_Strand.GetInt(row, -1);
        if ( to == kInvalidSeqPos ) {
            // point
            if ( !seq_pnt ) {
                seq_pnt = new CSeq_point();
            }
            else {
                seq_pnt->Reset();
            }
            CSeq_point& point = *seq_pnt;
            if ( id ) {
                point.SetId(*id);
            }
            else {
                point.SetId().SetGi(gi);
            }
            point.SetPoint(from);
            if ( strand >= 0 ) {
                point.SetStrand(ENa_strand(strand));
            }
            loc.SetPnt(point);
        }
        else {
            // interval
            if ( !seq_int ) {
                seq_int = new CSeq_interval();
            }
            else {
                seq_int->Reset();
            }
            CSeq_interval& interval = *seq_int;
            if ( id ) {
                interval.SetId(*id);
            }
            else {
                interval.SetId().SetGi(gi);
            }
            interval.SetFrom(from);
            interval.SetTo(to);
            if ( strand >= 0 ) {
                interval.SetStrand(ENa_strand(strand));
            }
            loc.SetInt(interval);
        }
    }
    sx_UpdateFields(loc, m_ExtraColumns, row);
}


void CSeqTableLocColumns::SetTableKeyAndIndex(size_t row,
                                              SAnnotObject_Key& key,
                                              SAnnotObject_Index& index) const
{
    key.m_Handle = GetIdHandle(row);
    key.m_Range = GetRange(row);
    ENa_strand strand = GetStrand(row);
    index.m_Flags = 0;
    if ( strand == eNa_strand_unknown ) {
        index.m_Flags |= index.fStrand_both;
    }
    else {
        if ( IsForward(strand) ) {
            index.m_Flags |= index.fStrand_plus;
        }
        if ( IsReverse(strand) ) {
            index.m_Flags |= index.fStrand_minus;
        }
    }
    bool simple = m_Is_simple;
    if ( !simple && m_Is_probably_simple ) {
        simple = true;
        ITERATE ( TExtraColumns, it, m_ExtraColumns ) {
            if ( it->IsSet(row) ) {
                simple = false;
                break;
            }
        }
    }
    if ( simple ) {
        if ( m_Is_simple_interval ) {
            index.SetLocationIsInterval();
        }
        else if ( m_Is_simple_point ) {
            index.SetLocationIsPoint();
        }
        else {
            _ASSERT(m_Is_simple_whole);
            index.SetLocationIsWhole();
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
// CSeqTableInfo
/////////////////////////////////////////////////////////////////////////////

CSeqTableInfo::CSeqTableInfo(const CSeq_table& feat_table)
    : m_Location("loc", CSeqTable_column_info::C_Field::eField_int_location),
      m_Product("product", CSeqTable_column_info::C_Field::eField_int_product)
{
    ITERATE ( CSeq_table::TColumns, it, feat_table.GetColumns() ) {
        const CSeqTable_column& col = **it;
        if ( m_Location.AddColumn(col) || m_Product.AddColumn(col) ) {
            continue;
        }
        const CSeqTable_column_info& type = col.GetHeader();
        if ( type.GetField().IsInt() ) {
            int field = type.GetField().GetInt();
            if (field == CSeqTable_column_info::C_Field::eField_int_partial) {
                if ( m_Partial ) {
                    NCBI_THROW_FMT(CAnnotException, eOtherError,
                                   "Duplicate partial column");
                }
                m_Partial.SetColumn(col, null);
            }
            else {
                m_ExtraColumns.push_back
                    (CSeqTableColumnInfo(col, sx_GetFeatSetter(field)));
            }
        }
        else {
            CTempString field(type.GetField().GetStr());
            if ( field == "partial" ) {
                if ( m_Partial ) {
                    NCBI_THROW_FMT(CAnnotException, eOtherError,
                                   "Duplicate partial column");
                }
                m_Partial.SetColumn(col, null);
            }
            else {
                m_ExtraColumns.push_back
                    (CSeqTableColumnInfo(col, sx_GetFeatSetter(field)));
            }
        }
    }

    m_Location.ParseDefaults();
    m_Product.ParseDefaults();
}


CSeqTableInfo::~CSeqTableInfo()
{
}


void CSeqTableInfo::UpdateSeq_feat(size_t row,
                                   CRef<CSeq_feat>& seq_feat,
                                   CRef<CSeq_point>& seq_pnt,
                                   CRef<CSeq_interval>& seq_int) const
{
    if ( !seq_feat ) {
        seq_feat = new CSeq_feat;
    }
    else {
        seq_feat->Reset();
    }
    CSeq_feat& feat = *seq_feat;
    if ( m_Location.IsSet() ) {
        CRef<CSeq_loc> seq_loc;
        if ( feat.IsSetLocation() ) {
            seq_loc = &feat.SetLocation();
        }
        m_Location.UpdateSeq_loc(row, seq_loc, seq_pnt, seq_int);
        feat.SetLocation(*seq_loc);
    }
    if ( m_Product.IsSet() ) {
        CRef<CSeq_loc> seq_loc;
        CRef<CSeq_point> seq_pnt;
        CRef<CSeq_interval> seq_int;
        if ( feat.IsSetProduct() ) {
            seq_loc = &feat.SetProduct();
        }
        m_Product.UpdateSeq_loc(row, seq_loc, seq_pnt, seq_int);
        feat.SetProduct(*seq_loc);
    }
    if ( IsPartial(row) ) {
        feat.SetPartial(true);
    }
    sx_UpdateFields(feat, m_ExtraColumns, row);
}

/////////////////////////////////////////////////////////////////////////////


static
void sx_SetFeatField(CSeq_loc& loc, const CSeqTableSetField& field,
                     const CSeqTableColumnInfo& col, size_t row)
{
    if ( col->IsSetData() ) {
        size_t index = col.GetRowIndex(row);
        if ( index != size_t(-1) ) {
            const CSeqTable_multi_data& data = col->GetData();
            switch ( data.Which() ) {
            case CSeqTable_multi_data::e_Int:
                if ( index < data.GetInt().size() ) {
                    field.Set(loc, data.GetInt()[index]);
                    return;
                }
                break;
            case CSeqTable_multi_data::e_Real:
                if ( index < data.GetReal().size() ) {
                    field.Set(loc, data.GetReal()[index]);
                    return;
                }
                break;
            case CSeqTable_multi_data::e_String:
                if ( index < data.GetString().size() ) {
                    field.Set(loc, data.GetString()[index]);
                    return;
                }
                break;
            default:
                ERR_POST_X(2, "Bad field data type: "<<data.Which());
                return;
            }
        }
    }
    if ( col->IsSetDefault() ) {
        const CSeqTable_single_data& data = col->GetDefault();
        switch ( data.Which() ) {
        case CSeqTable_single_data::e_Int:
            field.Set(loc, data.GetInt());
            return;
        case CSeqTable_single_data::e_Real:
            field.Set(loc, data.GetReal());
            return;
        case CSeqTable_single_data::e_String:
            field.Set(loc, data.GetString());
            return;
        default:
            ERR_POST_X(3, "Bad field data type: "<<data.Which());
            return;
        }
    }
}


static
void sx_SetFeatField(CSeq_feat& feat, const CSeqTableSetField& field,
                     const CSeqTableColumnInfo& col, size_t row)
{
    if ( col->IsSetData() ) {
        size_t index = col.GetRowIndex(row);
        if ( index != size_t(-1) ) {
            const CSeqTable_multi_data& data = col->GetData();
            switch ( data.Which() ) {
            case CSeqTable_multi_data::e_Int:
                if ( index < data.GetInt().size() ) {
                    field.Set(feat, data.GetInt()[index]);
                    return;
                }
                break;
            case CSeqTable_multi_data::e_Real:
                if ( index < data.GetReal().size() ) {
                    field.Set(feat, data.GetReal()[index]);
                    return;
                }
                return;
            case CSeqTable_multi_data::e_String:
                if ( index < data.GetString().size() ) {
                    field.Set(feat, data.GetString()[index]);
                    return;
                }
                break;
            default:
                ERR_POST_X(4, "Bad field data type: "<<data.Which());
                return;
            }
        }
    }
    if ( col->IsSetDefault() ) {
        const CSeqTable_single_data& data = col->GetDefault();
        switch ( data.Which() ) {
        case CSeqTable_single_data::e_Int:
            field.Set(feat, data.GetInt());
            return;
        case CSeqTable_single_data::e_Real:
            field.Set(feat, data.GetReal());
            return;
        case CSeqTable_single_data::e_String:
            field.Set(feat, data.GetString());
            return;
        default:
            ERR_POST_X(5, "Bad field data type: "<<data.Which());
            return;
        }
    }
}


static
void sx_UpdateField(CSeq_loc& loc,
                    const CSeqTableColumnInfo& col,
                    size_t row)
{
    const CSeqTable_column_info& type = col->GetHeader();
    if ( type.GetField().IsInt() ) {
        int field = type.GetField().GetInt();
        switch ( field ) {
        case CSeqTable_column_info::C_Field::eField_int_location_fuzz_from_lim:
        case CSeqTable_column_info::C_Field::eField_int_product_fuzz_from_lim:
            sx_SetFeatField(loc, CSeqTableSetLocFuzzFromLim(), col, row);
            break;
        case CSeqTable_column_info::C_Field::eField_int_location_fuzz_to_lim:
        case CSeqTable_column_info::C_Field::eField_int_product_fuzz_to_lim:
            sx_SetFeatField(loc, CSeqTableSetLocFuzzToLim(), col, row);
            break;
        default:
            ERR_POST_X(6, "SeqTable-column-info.field.int = "<<field);
            break;
        }
    }
    else {
        CTempString field = type.GetField().GetStr();
        if ( NStr::StartsWith(field, "location.") ) {
            field = field.substr(9);
        }
        else if ( NStr::StartsWith(field, "product.") ) {
            field = field.substr(8);
        }
        else {
            ERR_POST_X(7, "SeqTable-column-info.field.str = "<<field);
        }
        ERR_POST_X(8, "SeqTable-column-info.field.str = "<<field);
    }
}


static
void sx_UpdateField(CSeq_feat& feat,
                    const CSeqTableColumnInfo& col,
                    size_t row)
{
    const CSeqTable_column_info& type = col->GetHeader();
    if ( type.GetField().IsInt() ) {
        int field = type.GetField().GetInt();
        switch ( field ) {
        case CSeqTable_column_info::C_Field::eField_int_location_fuzz_from_lim:
            sx_SetFeatField(feat, CSeqTableSetLocFuzzFromLim(), col, row);
            break;
        case CSeqTable_column_info::C_Field::eField_int_location_fuzz_to_lim:
            sx_SetFeatField(feat, CSeqTableSetLocFuzzToLim(), col, row);
            break;
        case CSeqTable_column_info::C_Field::eField_int_product_fuzz_from_lim:
            sx_SetFeatField(feat, CSeqTableSetProdFuzzFromLim(), col, row);
            break;
        case CSeqTable_column_info::C_Field::eField_int_product_fuzz_to_lim:
            sx_SetFeatField(feat, CSeqTableSetProdFuzzToLim(), col, row);
            break;
        case CSeqTable_column_info::C_Field::eField_int_comment:
            sx_SetFeatField(feat, CSeqTableSetComment(), col, row);
            break;
        case CSeqTable_column_info::C_Field::eField_int_data_imp_key:
            sx_SetFeatField(feat, CSeqTableSetDataImpKey(), col, row);
            break;
        case CSeqTable_column_info::C_Field::eField_int_data_region:
            sx_SetFeatField(feat, CSeqTableSetDataRegion(), col, row);
            break;
        case CSeqTable_column_info::C_Field::eField_int_ext_type:
            sx_SetFeatField(feat, CSeqTableSetExtType(), col, row);
            break;
        default:
            ERR_POST_X(8, "SeqTable-column-info.field.int = "<<field);
            break;
        }
    }
    else {
        CTempString field = type.GetField().GetStr();
        if ( field[0] == 'E' && field.size() > 1 && field[1] == '.' ) {
            sx_SetFeatField(feat, CSeqTableSetExt(field.substr(2)), col, row);
        }
        else if ( field[0] == 'D' && field.size() > 1 && field[1] == '.' ) {
            sx_SetFeatField(feat, CSeqTableSetDbxref(field.substr(2)), col, row);
        }
        else if ( field[0] == 'Q' && field.size() > 1 && field[1] == '.' ) {
            sx_SetFeatField(feat, CSeqTableSetQual(field.substr(2)), col, row);
        }
        else {
            ERR_POST_X(9, "SeqTable-column-info.field.str = "<<field);
        }
    }
}


bool CSeqTableInfo::IsPartial(size_t row) const
{
    return m_Partial && m_Partial.GetBool(row);
}


END_SCOPE(objects)
END_NCBI_SCOPE
