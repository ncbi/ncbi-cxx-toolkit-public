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

#define NCBI_USE_ERRCODE_X   ObjMgr_SeqTable

BEGIN_NCBI_SCOPE

NCBI_DEFINE_ERR_SUBCODE_X(12);

BEGIN_SCOPE(objects)


/////////////////////////////////////////////////////////////////////////////
// CSeqTableColumnInfo
/////////////////////////////////////////////////////////////////////////////

bool CSeqTableColumnInfo::IsSet(size_t row) const
{
    return m_Column->IsSet(row);
}


bool CSeqTableColumnInfo::x_ThrowUnsetValue(void) const
{
    NCBI_THROW(CAnnotException, eOtherError,
               "CSeqTableColumnInfo::GetValue: value is not set");
}


bool CSeqTableColumnInfo::GetString(size_t row, string& v, bool force) const
{
    const string* ptr = GetStringPtr(row, force);
    if ( !ptr ) {
        return false;
    }
    v = *ptr;
    return true;
}


bool CSeqTableColumnInfo::GetUtf8String(size_t row, CStringUTF8& v, bool force) const
{
    const CStringUTF8* ptr = GetUtf8StringPtr(row, force);
    if ( !ptr ) {
        return false;
    }
    v = *ptr;
    return true;
}


bool CSeqTableColumnInfo::GetBytes(size_t row, vector<char>& v, bool force) const
{
    const vector<char>* ptr = GetBytesPtr(row, force);
    if ( !ptr ) {
        return false;
    }
    v = *ptr;
    return true;
}


const string* CSeqTableColumnInfo::GetStringPtr(size_t row,
                                                bool force) const
{
    const string* ret = m_Column->GetStringPtr(row);
    if ( !ret && force ) {
        x_ThrowUnsetValue();
    }
    return ret;
}


const CStringUTF8* CSeqTableColumnInfo::GetUtf8StringPtr(size_t row,
                                                         bool force) const
{
    const CStringUTF8* ret = m_Column->GetUtf8StringPtr(row);
    if ( !ret && force ) {
        x_ThrowUnsetValue();
    }
    return ret;
}


const vector<char>* CSeqTableColumnInfo::GetBytesPtr(size_t row,
                                                     bool force) const
{
    const vector<char>* ret = m_Column->GetBytesPtr(row);
    if ( !ret && force ) {
        x_ThrowUnsetValue();
    }
    return ret;
}


CConstRef<CSeq_id> CSeqTableColumnInfo::GetSeq_id(size_t row,
                                                  bool force) const
{
    CConstRef<CSeq_id> ret = m_Column->GetSeq_id(row);
    if ( !ret && force ) {
        x_ThrowUnsetValue();
    }
    return ret;
}


CConstRef<CSeq_loc> CSeqTableColumnInfo::GetSeq_loc(size_t row,
                                                    bool force) const
{
    CConstRef<CSeq_loc> ret = m_Column->GetSeq_loc(row);
    if ( !ret && force ) {
        x_ThrowUnsetValue();
    }
    return ret;
}


void CSeqTableColumnInfo::UpdateSeq_loc(CSeq_loc& loc,
                                        const CSeqTable_single_data& data,
                                        const CSeqTableSetLocField& setter) const
{
    switch ( data.Which() ) {
    case CSeqTable_single_data::e_Int:
        setter.SetInt(loc, data.GetInt());
        return;
    case CSeqTable_single_data::e_Real:
        setter.SetReal(loc, data.GetReal());
        return;
    case CSeqTable_single_data::e_String:
        setter.SetString(loc, data.GetString());
        return;
    default:
        ERR_POST_X(1, "Bad field data type: "<<data.Which());
        return;
    }
}


void CSeqTableColumnInfo::UpdateSeq_feat(CSeq_feat& feat,
                                         const CSeqTable_single_data& data,
                                         const CSeqTableSetFeatField& setter) const
{
    switch ( data.Which() ) {
    case CSeqTable_single_data::e_Int:
        setter.SetInt(feat, data.GetInt());
        return;
    case CSeqTable_single_data::e_Real:
        setter.SetReal(feat, data.GetReal());
        return;
    case CSeqTable_single_data::e_String:
        setter.SetString(feat, data.GetString());
        return;
    case CSeqTable_single_data::e_Bytes:
        setter.SetBytes(feat, data.GetBytes());
        return;
    default:
        ERR_POST_X(2, "Bad field data type: "<<data.Which());
        return;
    }
}


bool CSeqTableColumnInfo::UpdateSeq_loc(CSeq_loc& loc,
                                        const CSeqTable_multi_data& data,
                                        size_t index,
                                        const CSeqTableSetLocField& setter) const
{
    switch ( data.Which() ) {
    case CSeqTable_multi_data::e_Int:
        if ( index < data.GetInt().size() ) {
            setter.SetInt(loc, data.GetInt()[index]);
            return true;
        }
        break;
    case CSeqTable_multi_data::e_Real:
        if ( index < data.GetReal().size() ) {
            setter.SetReal(loc, data.GetReal()[index]);
            return true;
        }
        break;
    case CSeqTable_multi_data::e_String:
        if ( index < data.GetString().size() ) {
            setter.SetString(loc, data.GetString()[index]);
            return true;
        }
        break;
    case CSeqTable_multi_data::e_Common_string:
    {{
        const CCommonString_table& common = data.GetCommon_string();
        const CCommonString_table::TIndexes& indexes = common.GetIndexes();
        if ( index < indexes.size() ) {
            const CCommonString_table::TStrings& strings = common.GetStrings();
            size_t string_index = indexes[index];
            if ( string_index < strings.size() ) {
                setter.SetString(loc, strings[string_index]);
                return true;
            }
            else {
                ERR_POST_X(3, "Bad common string index");
                return false;
            }
        }
        break;
    }}
    default:
        ERR_POST_X(4, "Bad field data type: "<<data.Which());
        return true;
    }
    return false;
}


bool CSeqTableColumnInfo::UpdateSeq_feat(CSeq_feat& feat,
                                         const CSeqTable_multi_data& data,
                                         size_t index,
                                         const CSeqTableSetFeatField& setter) const
{
    switch ( data.Which() ) {
    case CSeqTable_multi_data::e_Int:
        if ( index < data.GetInt().size() ) {
            setter.SetInt(feat, data.GetInt()[index]);
            return true;
        }
        break;
    case CSeqTable_multi_data::e_Real:
        if ( index < data.GetReal().size() ) {
            setter.SetReal(feat, data.GetReal()[index]);
            return true;
        }
        break;
    case CSeqTable_multi_data::e_String:
        if ( index < data.GetString().size() ) {
            setter.SetString(feat, data.GetString()[index]);
            return true;
        }
        break;
    case CSeqTable_multi_data::e_Bytes:
        if ( index < data.GetBytes().size() ) {
            setter.SetBytes(feat, *data.GetBytes()[index]);
            return true;
        }
        break;
    case CSeqTable_multi_data::e_Common_string:
    {{
        const CCommonString_table& common = data.GetCommon_string();
        const CCommonString_table::TIndexes& indexes = common.GetIndexes();
        if ( index < indexes.size() ) {
            const CCommonString_table::TStrings& strings = common.GetStrings();
            size_t string_index = indexes[index];
            if ( string_index < strings.size() ) {
                setter.SetString(feat, strings[string_index]);
                return true;
            }
            else {
                ERR_POST_X(5, "Bad common string index");
                return false;
            }
        }
        break;
    }}
    case CSeqTable_multi_data::e_Common_bytes:
    {{
        const CCommonBytes_table& common = data.GetCommon_bytes();
        const CCommonBytes_table::TIndexes& indexes = common.GetIndexes();
        if ( index < indexes.size() ) {
            const CCommonBytes_table::TBytes& bytes = common.GetBytes();
            size_t bytes_index = indexes[index];
            if ( bytes_index < bytes.size() ) {
                setter.SetBytes(feat, *bytes[bytes_index]);
                return true;
            }
            else {
                ERR_POST_X(6, "Bad common bytes index");
                return false;
            }
        }
        break;
    }}
    default:
        ERR_POST_X(7, "Bad field data type: "<<data.Which());
        return true;
    }
    return false;
}


void CSeqTableColumnInfo::UpdateSeq_loc(CSeq_loc& loc, size_t row,
                                        const CSeqTableSetLocField& setter) const
{
    size_t index = row;
    if ( m_Column->IsSetSparse() ) {
        index = m_Column->GetSparse().GetIndexAt(row);
        if ( index == CSeqTable_sparse_index::kSkipped ) {
            if ( m_Column->IsSetSparse_other() ) {
                UpdateSeq_loc(loc, m_Column->GetSparse_other(), setter);
            }
            return;
        }
    }

    if ( m_Column->IsSetData() &&
         UpdateSeq_loc(loc, m_Column->GetData(), index, setter) ) {
        return;
    }

    if ( m_Column->IsSetDefault() ) {
        UpdateSeq_loc(loc, m_Column->GetDefault(), setter);
    }
    else if ( !m_Column->IsSetData() ) {
        // no multi or single data -> no value, but we need to touch the field
        setter.SetInt(loc, 0);
    }
}


void CSeqTableColumnInfo::UpdateSeq_feat(CSeq_feat& feat, size_t row,
                                         const CSeqTableSetFeatField& setter) const
{
    size_t index = row;
    if ( m_Column->IsSetSparse() ) {
        index = m_Column->GetSparse().GetIndexAt(row);
        if ( index == CSeqTable_sparse_index::kSkipped ) {
            if ( m_Column->IsSetSparse_other() ) {
                UpdateSeq_feat(feat, m_Column->GetSparse_other(), setter);
            }
            return;
        }
    }

    if ( m_Column->IsSetData() &&
         UpdateSeq_feat(feat, m_Column->GetData(), index, setter) ) {
        return;
    }

    if ( m_Column->IsSetDefault() ) {
        UpdateSeq_feat(feat, m_Column->GetDefault(), setter);
    }
    else if ( !m_Column->IsSetData() ) {
        // no multi or single data -> no value, but we need to touch the field
        setter.SetInt(feat, 0);
    }
}


/////////////////////////////////////////////////////////////////////////////
// CSeqTableLocColumns
/////////////////////////////////////////////////////////////////////////////


CSeqTableLocColumns::CSeqTableLocColumns(const char* name,
                                         CSeqTable_column_info::EField_id base)
    : m_FieldName(name),
      m_BaseValue(base),
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
    field = CSeqTableColumnInfo(column);
    m_Is_set = true;
}


void CSeqTableLocColumns::AddExtraColumn(const CSeqTable_column& column,
                                         const CSeqTableSetLocField* setter)
{
    m_ExtraColumns.push_back(TColumnInfo(column, ConstRef(setter)));
    m_Is_set = true;
}


bool CSeqTableLocColumns::AddColumn(const CSeqTable_column& column)
{
    const CSeqTable_column_info& type = column.GetHeader();
    if ( type.IsSetField_id() ) {
        int field = type.GetField_id() - m_BaseValue +
            CSeqTable_column_info::eField_id_location;
        if ( field < CSeqTable_column_info::eField_id_location ||
             field >= CSeqTable_column_info::eField_id_product ) {
            return false;
        }
        switch ( field ) {
        case CSeqTable_column_info::eField_id_location:
            SetColumn(m_Loc, column);
            return true;
        case CSeqTable_column_info::eField_id_location_id:
            SetColumn(m_Id, column);
            return true;
        case CSeqTable_column_info::eField_id_location_gi:
            SetColumn(m_Gi, column);
            return true;
        case CSeqTable_column_info::eField_id_location_from:
            SetColumn(m_From, column);
            return true;
        case CSeqTable_column_info::eField_id_location_to:
            SetColumn(m_To, column);
            return true;
        case CSeqTable_column_info::eField_id_location_strand:
            SetColumn(m_Strand, column);
            return true;
        case CSeqTable_column_info::eField_id_location_fuzz_from_lim:
            AddExtraColumn(column, new CSeqTableSetLocFuzzFromLim());
            return true;
        case CSeqTable_column_info::eField_id_location_fuzz_to_lim:
            AddExtraColumn(column, new CSeqTableSetLocFuzzToLim());
            return true;
        default:
            break;
        }
    }
    if ( !type.IsSetField_name() ) {
        return false;
    }

    CTempString field(type.GetField_name());
    if ( field == m_FieldName ) {
        SetColumn(m_Loc, column);
        return true;
    }
    else if ( NStr::StartsWith(field, m_FieldName) &&
              field[m_FieldName.size()] == '.' ) {
        CTempString extra = field.substr(m_FieldName.size()+1);
        if ( extra == "id" || NStr::EndsWith(extra, ".id") ) {
            SetColumn(m_Id, column);
            return true;
        }
        else if ( extra == "gi" || NStr::EndsWith(extra, ".gi") ) {
            SetColumn(m_Gi, column);
            return true;
        }
        else if ( extra == "pnt.point" || extra == "int.from" ) {
            SetColumn(m_From, column);
            return true;
        }
        else if ( extra == "int.to" ) {
            SetColumn(m_To, column);
            return true;
        }
        else if ( extra == "strand" ||
                  NStr::EndsWith(extra, ".strand") ) {
            SetColumn(m_Strand, column);
            return true;
        }
        else if ( extra == "int.fuzz-from.lim" ||
                  extra == "pnt.fuzz.lim" ) {
            AddExtraColumn(column, new CSeqTableSetLocFuzzFromLim());
            return true;
        }
        else if ( extra == "int.fuzz-to.lim" ) {
            AddExtraColumn(column, new CSeqTableSetLocFuzzToLim());
            return true;
        }
    }
    return false;
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


CConstRef<CSeq_loc> CSeqTableLocColumns::GetLoc(size_t row) const
{
    _ASSERT(m_Loc);
    _ASSERT(!m_Loc->IsSetDefault());
    return m_Loc.GetSeq_loc(row);
}


CConstRef<CSeq_id> CSeqTableLocColumns::GetId(size_t row) const
{
    _ASSERT(!m_Loc);
    _ASSERT(m_Id);
    return m_Id.GetSeq_id(row);
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
            int gi;
            if ( m_Gi.GetInt(row, gi) ) {
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
    int from;
    if ( !m_From || !m_From.GetInt(row, from) ) {
        return CRange<TSeqPos>::GetWhole();
    }
    int to = from;
    if ( m_To ) {
        m_To.GetInt(row, to);
    }
    return CRange<TSeqPos>(from, to);
}


ENa_strand CSeqTableLocColumns::GetStrand(size_t row) const
{
    _ASSERT(!m_Loc);
    int strand = eNa_strand_unknown;
    if ( m_Strand ) {
        m_Strand.GetInt(row, strand);
    }
    return ENa_strand(strand);
}


void CSeqTableLocColumns::UpdateSeq_loc(size_t row,
                                        CRef<CSeq_loc>& seq_loc,
                                        CRef<CSeq_point>& seq_pnt,
                                        CRef<CSeq_interval>& seq_int) const
{
    _ASSERT(m_Is_set);
    if ( m_Loc ) {
        seq_loc = &const_cast<CSeq_loc&>(*GetLoc(row));
        return;
    }
    if ( !seq_loc ) {
        seq_loc = new CSeq_loc();
    }
    CSeq_loc& loc = *seq_loc;

    CConstRef<CSeq_id> id;
    int gi = 0;
    if ( m_Id ) {
        id = GetId(row);
    }
    else {
        _ASSERT(m_Gi);
        m_Gi.GetInt(row, gi);
    }

    int from = 0;
    if ( !m_From || !m_From.GetInt(row, from) ) {
        // whole
        if ( id ) {
            loc.SetWhole(const_cast<CSeq_id&>(*id));
        }
        else {
            loc.SetWhole().SetGi(gi);
        }
    }
    else {
        int strand = -1;
        if ( m_Strand ) {
            m_Strand.GetInt(row, strand);
        }

        int to = 0;
        if ( !m_To || !m_To.GetInt(row, to) ) {
            // point
            if ( !seq_pnt ) {
                seq_pnt = new CSeq_point();
            }
            CSeq_point& point = *seq_pnt;
            if ( id ) {
                point.SetId(const_cast<CSeq_id&>(*id));
            }
            else {
                point.SetId().SetGi(gi);
            }
            point.SetPoint(from);
            if ( strand >= 0 ) {
                point.SetStrand(ENa_strand(strand));
            }
            else {
                point.ResetStrand();
            }
            point.ResetFuzz();
            loc.SetPnt(point);
        }
        else {
            // interval
            if ( !seq_int ) {
                seq_int = new CSeq_interval();
            }
            CSeq_interval& interval = *seq_int;
            if ( id ) {
                interval.SetId(const_cast<CSeq_id&>(*id));
            }
            else {
                interval.SetId().SetGi(gi);
            }
            interval.SetFrom(from);
            interval.SetTo(to);
            if ( strand >= 0 ) {
                interval.SetStrand(ENa_strand(strand));
            }
            else {
                interval.ResetStrand();
            }
            interval.ResetFuzz_from();
            interval.ResetFuzz_to();
            loc.SetInt(interval);
        }
    }
    ITERATE ( TExtraColumns, it, m_ExtraColumns ) {
        it->first.UpdateSeq_loc(loc, row, *it->second);
    }
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
            if ( it->first.IsSet(row) ) {
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


bool CSeqTableInfo::IsGoodFeatTable(const CSeq_table& table)
{
    if ( !table.IsSetFeat_type() ||
         table.GetFeat_type() <= CSeqFeatData::e_not_set ||
         table.GetFeat_type() >= CSeqFeatData::e_MaxChoice ) {
        return false; // not a feature table
    }
    if ( table.IsSetFeat_subtype() &&
         (table.GetFeat_subtype() <= CSeqFeatData::eSubtype_bad ||
          table.GetFeat_subtype() >= CSeqFeatData::eSubtype_max) ) {
        return false; // bad subtype
    }
    return true;
}


CSeqTableInfo::CSeqTableInfo(const CSeq_table& feat_table, bool is_feat)
    : m_IsFeatTable(is_feat),
      m_Location("loc", CSeqTable_column_info::eField_id_location),
      m_Product("product", CSeqTable_column_info::eField_id_product)
{
    x_Initialize(feat_table);
}


CSeqTableInfo::CSeqTableInfo(const CSeq_table& feat_table)
    : m_IsFeatTable(IsGoodFeatTable(feat_table)),
      m_Location("loc", CSeqTable_column_info::eField_id_location),
      m_Product("product", CSeqTable_column_info::eField_id_product)
{
    x_Initialize(feat_table);
}


void CSeqTableInfo::x_Initialize(const CSeq_table& feat_table)
{
    ITERATE ( CSeq_table::TColumns, it, feat_table.GetColumns() ) {
        const CSeqTable_column& col = **it;
        const CSeqTable_column_info& type = col.GetHeader();
        if ( type.IsSetField_id() ) {
            int id = type.GetField_id();
            m_ColumnsById.insert(TColumnsById::value_type(id, col));
            if ( IsFeatTable() && !type.IsSetField_name() ) {
                string name = type.GetNameForId(id);
                if ( !name.empty() ) {
                    m_ColumnsByName.insert(TColumnsByName::value_type(name, col));
                }
            }
        }
        if ( type.IsSetField_name() ) {
            string name = type.GetField_name();
            m_ColumnsByName.insert(TColumnsByName::value_type(name, col));
            if ( IsFeatTable() && !type.IsSetField_id() ) {
                int id = type.GetIdForName(name);
                if ( id >= 0 ) {
                    m_ColumnsById.insert(TColumnsById::value_type(id, col));
                }
            }
        }
        if ( !IsFeatTable() ) {
            continue;
        }

        if ( m_Location.AddColumn(col) || m_Product.AddColumn(col) ) {
            continue;
        }
        CConstRef<CSeqTableSetFeatField> setter;
        if ( type.IsSetField_id() ) {
            int id = type.GetField_id();
            switch ( id ) {
            case CSeqTable_column_info::eField_id_partial:
                if ( m_Partial ) {
                    NCBI_THROW_FMT(CAnnotException, eOtherError,
                                   "Duplicate partial column");
                }
                m_Partial = CSeqTableColumnInfo(col);
                continue;
            case CSeqTable_column_info::eField_id_comment:
                setter = new CSeqTableSetComment();
                break;
            case CSeqTable_column_info::eField_id_data_imp_key:
                setter = new CSeqTableSetDataImpKey();
                break;
            case CSeqTable_column_info::eField_id_data_region:
                setter = new CSeqTableSetDataRegion();
                break;
            case CSeqTable_column_info::eField_id_ext_type:
                setter = new CSeqTableSetExtType();
                break;
            case CSeqTable_column_info::eField_id_ext:
                setter = new CSeqTableSetExt(type.GetField_name());
                break;
            case CSeqTable_column_info::eField_id_dbxref:
                setter = new CSeqTableSetDbxref(type.GetField_name());
                break;
            case CSeqTable_column_info::eField_id_qual:
                setter = new CSeqTableSetQual(type.GetField_name());
                break;
            default:
                if ( !type.IsSetField_name() ) {
                    ERR_POST_X(8, "SeqTable-column-info.field-id = "<<id);
                    continue;
                }
                break;
            }
        }
        else if ( !type.IsSetField_name() ) {
            ERR_POST_X(9, "SeqTable-column-info: "
                       "neither field-id nor field-name is set");
            continue;
        }
        if ( !setter && type.IsSetField_name() ) {
            CTempString name(type.GetField_name());
            if ( name.empty() ) {
                ERR_POST_X(10, "SeqTable-column-info.field-name is empty");
                continue;
            }
            else if ( name[0] == 'E' ) {
                setter = new CSeqTableSetExt(name);
            }
            else if ( name[0] == 'D' ) {
                setter = new CSeqTableSetDbxref(name);
            }
            else if ( name[0] == 'Q' ) {
                setter = new CSeqTableSetQual(name);
            }
            else if ( name == "partial" ) {
                if ( m_Partial ) {
                    NCBI_THROW_FMT(CAnnotException, eOtherError,
                                   "Duplicate partial column");
                }
                m_Partial = CSeqTableColumnInfo(col);
                continue;
            }
            if ( !setter ) {
                try {
                    setter = new CSeqTableSetAnyFeatField(name);
                }
                catch ( CAnnotException& /*exc*/ ) {
                    // ignore invalid column names
                }
            }
        }
        if ( setter ) {
            m_ExtraColumns.push_back(TColumnInfo(col, setter));
        }
    }

    if ( IsFeatTable() ) {
        m_Location.ParseDefaults();
        m_Product.ParseDefaults();
    }
}


CSeqTableInfo::~CSeqTableInfo()
{
}


bool CSeqTableInfo::IsPartial(size_t row) const
{
    bool val = false;
    return m_Partial && m_Partial.GetBool(row, val) && val;
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
    if ( m_Partial ) {
        bool val = false;
        if ( m_Partial.GetBool(row, val) ) {
            feat.SetPartial(val);
        }
    }
    ITERATE ( TExtraColumns, it, m_ExtraColumns ) {
        it->first.UpdateSeq_feat(feat, row, *it->second);
    }
}


const CSeqTableColumnInfo*
CSeqTableInfo::FindColumn(int field_id) const
{
    TColumnsById::const_iterator iter = m_ColumnsById.find(field_id);
    if ( iter == m_ColumnsById.end() ) {
        return 0;
    }
    return &iter->second;
}


const CSeqTableColumnInfo*
CSeqTableInfo::FindColumn(const string& field_name) const
{
    TColumnsByName::const_iterator iter = m_ColumnsByName.find(field_name);
    if ( iter == m_ColumnsByName.end() ) {
        return 0;
    }
    return &iter->second;
}


const CSeqTableColumnInfo&
CSeqTableInfo::GetColumn(int field_id) const
{
    const CSeqTableColumnInfo* column = FindColumn(field_id);
    if ( !column ) {
        NCBI_THROW_FMT(CAnnotException, eOtherError,
                       "CSeqTableInfo::GetColumn: "
                       "column "<<field_id<<" not found");
    }
    return *column;
}


const CSeqTableColumnInfo&
CSeqTableInfo::GetColumn(const string& field_name) const
{
    const CSeqTableColumnInfo* column = FindColumn(field_name);
    if ( !column ) {
        NCBI_THROW_FMT(CAnnotException, eOtherError,
                       "CSeqTableInfo::GetColumn: "
                       "column "<<field_name<<" not found");
    }
    return *column;
}


END_SCOPE(objects)
END_NCBI_SCOPE
