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
 * Author:  Colleen Bollin
 *
 * File Description:
 *      Implementation of utility classes and functions for biosample_chk.
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>

#include <serial/enumvalues.hpp>
#include <serial/serialimpl.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objects/seqtable/SeqTable_multi_data.hpp>
#include <objects/seqtable/SeqTable_column_info.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/object_manager.hpp>
#include <objtools/edit/dblink_field.hpp>

#include <vector>
#include <algorithm>
#include <list>

// for biosample fetching
#include <objects/seq/Seq_descr.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <serial/objistrasn.hpp>

#include "util.hpp"
#include "struc_table_column.hpp"

#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <serial/objistrasn.hpp>
#include <serial/objistr.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CRef< CSeq_descr >
GetBiosampleData(string accession, bool use_dev_server)
{
    string host = use_dev_server ? "dev-api-int" : "api-int";
    string path = "/biosample/fetch/";
    string args = "accession=" + accession + "&format=asn1raw";
    CConn_HttpStream http_stream(host, path, args);
    auto_ptr<CObjectIStream> in_stream;
    in_stream.reset(new CObjectIStreamAsn(http_stream));
 
	CRef< CSeq_descr > response(new CSeq_descr());
    try {
	    *in_stream >> *response;
    } catch (...) {
        response.Reset(NULL);
    }
    return response;
}


vector<string> GetDBLinkIDs(const CUser_object& user, const string& field_name)
{
    vector<string> ids;

    if (!user.IsSetType() || !user.GetType().IsStr() || !NStr::EqualNocase(user.GetType().GetStr(), "DBLink")) {
        // Not DBLink object
        return ids;
    }
    try {
        const CUser_field& field = user.GetField(field_name);
        if (field.IsSetData() && field.GetData().IsStrs()) {
            ITERATE(CUser_field::TData::TStrs, it, field.GetData().GetStrs()) {
                ids.push_back(*it);
            }
        }
    } catch (...) {
        // no biosample ID field
    }        

    return ids;
}


vector<string> GetDBLinkIDs(const CSeqdesc& seqdesc, const string& field)
{
    vector<string> ids;

    if (seqdesc.IsUser()) {
        ids = GetDBLinkIDs(seqdesc.GetUser(), field);
    }
    return ids;
}


vector<string> GetBiosampleIDs(CBioseq_Handle bh)
{
    vector<string> ids;

    edit::CDBLinkField dblink_field(edit::CDBLinkField::eDBLinkFieldType_BioSample);
    vector<CConstRef<CObject> > objs = dblink_field.GetObjects(bh);
    ITERATE(vector<CConstRef<CObject> >, it, objs) {
        vector<string> new_ids = dblink_field.GetVals(**it);
        ITERATE(vector<string>, s, new_ids) {
            ids.push_back(*s);
        }
    }
    return ids;
}


vector<string> GetBioProjectIDs(CBioseq_Handle bh)
{
    vector<string> ids;

    CSeqdesc_CI desc_ci(bh, CSeqdesc::e_User);
    while (desc_ci) {
        vector<string> new_ids = GetDBLinkIDs(*desc_ci, "BioProject");
        ITERATE(vector<string>, s, new_ids) {
            ids.push_back(*s);
        }
        ++desc_ci;
    }
    return ids;
}


void CBiosampleFieldDiff::PrintHeader(ncbi::CNcbiOstream & stream, bool show_seq_id)
{
    stream << "#sample\tattribute";
    if (show_seq_id) {
        stream << "\tSequenceID";
    }
    stream << "\told_value\tnew_value" << endl;
}

    
void CBiosampleFieldDiff::Print(CNcbiOstream& stream, bool show_seq_id) const
{
    bool blank_sample = NStr::IsBlank(m_SampleVal);
    bool blank_src = NStr::IsBlank(m_SrcVal);
    if (blank_sample && blank_src) {
        return;
    }
    stream << m_BiosampleID << "\t";
    stream << m_FieldName << "\t";
    if (show_seq_id) {
        stream << m_SequenceID << "\t";
    }
    stream << (blank_sample ? "[[add]]" : m_SampleVal) << "\t";
    stream << (blank_src ? "[[delete]]" : m_SrcVal) << endl;
}


void CBiosampleFieldDiff::Print(ncbi::CNcbiOstream & stream, const CBiosampleFieldDiff& prev)
{
    if (!NStr::EqualNocase(m_BiosampleID, prev.m_BiosampleID)) {
        Print(stream);
    } else {
        stream << "\t";
        if (!NStr::EqualNocase(m_FieldName, prev.m_FieldName)) {
            stream << m_FieldName;
        }
        bool blank_sample = NStr::IsBlank(m_SampleVal) || CBioSource::IsStopWord(m_SampleVal);
        stream << "\t";
        stream << m_SequenceID << "\t";
        stream << (blank_sample ? "" : m_SampleVal) << "\t";
        stream << m_SrcVal << "\t";
        stream << endl;
    }
}


int CBiosampleFieldDiff::CompareAllButSequenceID(const CBiosampleFieldDiff& other)
{
    int cmp = NStr::CompareCase(m_BiosampleID, other.m_BiosampleID);
    if (cmp == 0) {
        cmp = NStr::CompareNocase(m_FieldName, other.m_FieldName);
        if (cmp == 0) {
            // "mixed" matches to anything
            if (!NStr::EqualNocase(m_SrcVal, "mixed") && !NStr::EqualNocase(other.m_SrcVal, "mixed")) {
                cmp = NStr::CompareNocase(m_SrcVal, other.m_SrcVal);
                // note - if BioSample ID is the same, sample_val should also be the same
            }
        }
    }

    return cmp;
}


int CBiosampleFieldDiff::Compare(const CBiosampleFieldDiff& other)
{
    int cmp = CompareAllButSequenceID(other);
    if (cmp == 0) {
        cmp = NStr::CompareCase(m_SequenceID, other.m_SequenceID);
    }

    return cmp;
}


bool s_CompareStructuredCommentFields (CRef<CStructuredCommentTableColumnBase> f1, CRef<CStructuredCommentTableColumnBase> f2)
{ 
    if (!f1) {
        return true;
    } else if (!f2) {
        return false;
    } 
    string name1 = f1->GetLabel();
    string name2 = f2->GetLabel();
    int cmp = NStr::Compare (name1, name2);
    if (cmp < 0) {
        return true;
    } else {
        return false;
    }        
}


TStructuredCommentTableColumnList GetAvailableFields(vector<CConstRef<CUser_object> > src)
{
    TStructuredCommentTableColumnList fields;

    ITERATE(vector<CConstRef<CUser_object> >, it, src) {
        TStructuredCommentTableColumnList src_fields = GetStructuredCommentFields(**it);
		fields.insert(fields.end(), src_fields.begin(), src_fields.end());
    }

    // no need to sort and unique if there are less than two fields
    if (fields.size() < 2) {
        return fields;
    }
    sort(fields.begin(), fields.end(), s_CompareStructuredCommentFields);

    TStructuredCommentTableColumnList::iterator f_prev = fields.begin();
    TStructuredCommentTableColumnList::iterator f_next = f_prev;
    f_next++;
    while (f_next != fields.end()) {
        if (NStr::Equal((*f_prev)->GetLabel(), (*f_next)->GetLabel())) {
            f_next = fields.erase(f_next);
        } else {
            ++f_prev;
            ++f_next;
        }
    }

    return fields;
}


TBiosampleFieldDiffList GetFieldDiffs(string sequence_id, string biosample_id, const CBioSource& src, const CBioSource& sample)
{
    TBiosampleFieldDiffList rval;

    TFieldDiffList src_diffs = src.GetBiosampleDiffs(sample);
    ITERATE(TFieldDiffList, it, src_diffs) {
        CRef<CBiosampleFieldDiff> diff(new CBiosampleFieldDiff(sequence_id, biosample_id, **it));
        rval.push_back(diff);
    }


    return rval;
}


bool s_ShouldIgnoreStructuredCommentFieldDiff (const string& label, const string& src_val, const string& sample_val)
{
    if (NStr::Equal(label, "StructuredCommentPrefix")
        || NStr::Equal(label, "StructuredCommentSuffix")) {
        return true;
    } else if (NStr::EqualNocase(src_val, sample_val)) {
        return true;
    } else {
        return false;
    }
}


TBiosampleFieldDiffList GetFieldDiffs(string sequence_id, string biosample_id, const CUser_object& src, const CUser_object& sample)
{
    TBiosampleFieldDiffList rval;

    vector<CConstRef<CUser_object> > src_list;
    CConstRef<CUser_object> s1(&src);
    src_list.push_back(s1);
    CConstRef<CUser_object> s2(&sample);
    src_list.push_back(s2);

    TStructuredCommentTableColumnList field_list = GetAvailableFields (src_list);

    ITERATE(TStructuredCommentTableColumnList, it, field_list) {
        if (NStr::Equal((*it)->GetLabel(), "StructuredCommentPrefix")
            || NStr::Equal((*it)->GetLabel(), "StructuredCommentSuffix")) {
            continue;
        }
        string src_val = (*it)->GetFromComment(src);
        string sample_val = (*it)->GetFromComment(sample);

        if (!s_ShouldIgnoreStructuredCommentFieldDiff((*it)->GetLabel(), src_val, sample_val)) {
            CRef<CBiosampleFieldDiff> diff(new CBiosampleFieldDiff(sequence_id, biosample_id, (*it)->GetLabel(), src_val, sample_val));
            rval.push_back(diff);
        }
    }

    return rval;
}


TBiosampleFieldDiffList GetFieldDiffs(string sequence_id, string biosample_id, CConstRef<CUser_object> src, CConstRef<CUser_object> sample)
{
    TBiosampleFieldDiffList rval;

    vector<CConstRef<CUser_object> > src_list;
    if (src) {
        src_list.push_back(src);
    }
    if (sample) {
        src_list.push_back(sample);
    }

    TStructuredCommentTableColumnList field_list = GetAvailableFields (src_list);

    ITERATE(TStructuredCommentTableColumnList, it, field_list) {
        string src_val = "";
        if (src) {
            src_val = (*it)->GetFromComment(*src);
        }
        string sample_val = "";
        if (sample) {
            (*it)->GetFromComment(*sample);
        }
        if (!s_ShouldIgnoreStructuredCommentFieldDiff((*it)->GetLabel(), src_val, sample_val)) {
            CRef<CBiosampleFieldDiff> diff(new CBiosampleFieldDiff(sequence_id, biosample_id, (*it)->GetLabel(), src_val, sample_val));
            rval.push_back(diff);
        }
    }

    return rval;
}


CRef<CSeqTable_column> FindSeqTableColumnByName (CRef<CSeq_table> values_table, string column_name)
{
    ITERATE (CSeq_table::TColumns, cit, values_table->GetColumns()) {
        if ((*cit)->IsSetHeader() && (*cit)->GetHeader().IsSetTitle()
            && NStr::Equal ((*cit)->GetHeader().GetTitle(), column_name)) {
            return *cit;
        }
    }
    CRef<CSeqTable_column> empty;
    return empty;
}


void AddValueToColumn (CRef<CSeqTable_column> column, string value, size_t row)
{
    while (column->SetData().SetString().size() < row + 1) {
        column->SetData().SetString().push_back ("");
    }
	column->SetData().SetString()[row] = value;
}


void AddValueToTable (CRef<CSeq_table> table, string column_name, string value, size_t row)
{
    // do we already have a column for this subtype?
    bool found = false;
    NON_CONST_ITERATE (CSeq_table::TColumns, cit, table->SetColumns()) {
        if ((*cit)->IsSetHeader() && (*cit)->GetHeader().IsSetTitle()
            && NStr::EqualNocase((*cit)->GetHeader().GetTitle(), column_name)) {
            AddValueToColumn((*cit), value, row);
            found = true;
            break;
        }
    }
    if (!found) {
        CRef<objects::CSeqTable_column> new_col(new objects::CSeqTable_column());
        new_col->SetHeader().SetTitle(column_name);
		AddValueToColumn(new_col, value, row);
        table->SetColumns().push_back(new_col);
    }
}


string GetValueFromColumn(const CSeqTable_column& column, size_t row)
{
    string val = "";

    if (column.IsSetData() && column.GetData().IsString() && column.GetData().GetString().size() > row) {
        val = column.GetData().GetString()[row];
    }
    return val;
}


string GetValueFromTable(const CSeq_table& table, string column_name, size_t row)
{
    string val = "";
    ITERATE (CSeq_table::TColumns, cit, table.GetColumns()) {
        if ((*cit)->IsSetHeader() && (*cit)->GetHeader().IsSetTitle()
            && NStr::EqualNocase((*cit)->GetHeader().GetTitle(), column_name)) {
            val = GetValueFromColumn((**cit), row);
            break;
        }
    }
    return val;
}


void HarmonizeAttributeName(string& attribute_name)
{
    NStr::ReplaceInPlace (attribute_name, " ", "");
    NStr::ReplaceInPlace (attribute_name, "_", "");
    NStr::ReplaceInPlace (attribute_name, "-", "");
}


bool AttributeNamesAreEquivalent (string name1, string name2)
{
    HarmonizeAttributeName(name1);
    HarmonizeAttributeName(name2);
    return NStr::EqualNocase(name1, name2);
}


END_SCOPE(objects)
END_NCBI_SCOPE
