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
#include <objects/valid/Comment_rule.hpp>
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

#include <misc/xmlwrapp/xmlwrapp.hpp>
#include <misc/biosample_util/biosample_util.hpp>
#include <misc/biosample_util/struc_table_column.hpp>

#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <serial/objistrasn.hpp>
#include <serial/objistr.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(biosample_util)

using namespace xml;

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

EStatus GetBioSampleStatusFromNode(const node& item)
{
    attributes::const_iterator at = item.get_attributes().begin();
    while (at != item.get_attributes().end()) {
        if (NStr::Equal(at->get_name(), "status")) {
            string val = at->get_value();
            if (NStr::EqualNocase(val, "live")) {
                return eStatus_Live;
            } else if (NStr::EqualNocase(val, "hup")) {
                return eStatus_Hup;
            } else if (NStr::EqualNocase(val, "withdrawn")) {
                return eStatus_Withdrawn;
            } else if (NStr::EqualNocase(val, "suppressed")) {
                return eStatus_Suppressed;
            } else if (NStr::EqualNocase(val, "to_be_curated")) {
                return eStatus_ToBeCurated;
            } else if (NStr::EqualNocase(val, "replaced")) {
                return eStatus_Replaced;
            } else {
                return eStatus_Unknown;
            }
            break;
        }
        ++at;
    }
    return eStatus_Unknown;
}


TStatus ProcessBiosampleStatusNode(node& item)
{
    TStatus response;
    attributes::iterator at1 = item.get_attributes().begin();
    while (at1 != item.get_attributes().end() && NStr::IsBlank(response.first)) {
        if (NStr::Equal(at1->get_name(), "accession")) {
            response.first = at1->get_value();
        }
        ++at1;
    }
    node::iterator it = item.begin();
    while (it != item.end()) {
        if (NStr::Equal(it->get_name(), "Status")) {
            response.second = GetBioSampleStatusFromNode(*it);
            break;
        }
        ++it;
    }
    return response;
}

EStatus GetBiosampleStatus(string accession, bool use_dev_server)
{
    string host = use_dev_server ? "dev-api-int" : "api-int";
    string path = "/biosample/fetch/";
    string args = "accession=" + accession;
    CConn_HttpStream http_stream(host, path, args);
    xml::error_messages errors;
    document response(http_stream, &errors);
    
#if 0
    TStatus status = ProcessBiosampleStatusNode(response.get_root_node());
    return status.second;
#else 
    // get status from XML response
    node & root = response.get_root_node();
    node::iterator it = root.begin();
    while (it != root.end())
    {
        if (NStr::Equal(it->get_name(), "Status")) {
            return GetBioSampleStatusFromNode(*it);
        }
        ++it;
    }
#endif

    return eStatus_Unknown;
}

void ProcessBulkBioSample(TStatuses& status, string list, bool use_dev_server)
{
    string host = use_dev_server ? "dev-api-int" : "api-int";
    string path = "/biosample/fetch/";
    string args = "id=" + list + "&bulk=true";

    CConn_HttpStream http_stream(host, path, args);
    xml::error_messages errors;
    document response(http_stream, &errors);

    // get status from XML response
    node & root = response.get_root_node();
    node::iterator it = root.begin();
    while (it != root.end())
    {
        if (NStr::EqualNocase(it->get_name(), "BioSample")) {
            TStatus response = ProcessBiosampleStatusNode(*it);
            status[response.first] = response.second;
        }
        ++it;
    }
}

void GetBiosampleStatus(TStatuses& status, bool use_dev_server)
{
    size_t count = 0;
    string list = "";
    for (TStatuses::iterator it = status.begin(); it != status.end(); ++it) {
        list += "," + it->first;
        count++;
        if (count == 900) {
            ProcessBulkBioSample(status, list.substr(1), use_dev_server);
            list = "";
            count = 0;
        }
    }
    if (!NStr::IsBlank(list)) {
        ProcessBulkBioSample(status, list.substr(1), use_dev_server);
    }
}


string GetBiosampleStatusName(EStatus status)
{
    switch (status) {
    case eStatus_Unknown:
        return "Unknown";
        break;
    case eStatus_Live:
        return "Live";
        break;
    case eStatus_Hup:
        return "HUP";
        break;
    case eStatus_Withdrawn:
        return "Withdrawn";
        break;
    case eStatus_Suppressed:
        return "Suppressed";
        break;
    case eStatus_ToBeCurated:
        return "ToBeCurated";
        break;
    case eStatus_Replaced:
        return "Replaced";
        break;
    }
    return kEmptyStr;
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


bool DoDiffsContainConflicts(const TBiosampleFieldDiffList& diffs, CNcbiOstream* log)
{
    if (diffs.empty()) {
        return false;;
    }

    bool rval = false;
    bool printed_header = false;

    ITERATE (TBiosampleFieldDiffList, it, diffs) {
        string src_val = (*it)->GetSrcVal();
        if (!NStr::IsBlank(src_val)) {
            if (log) {
                if (!printed_header) {
                    *log << "Conflict found for " << (*it)->GetSequenceId() << " for " << (*it)->GetBioSample() << endl;
                    printed_header = true;
                }
                *log << "\t" << (*it)->GetFieldName() << ": BioSource contains \"" << src_val << "\", BioSample contains \"" << (*it)->GetSampleVal() << "\"" << endl;
            }
            rval = true;
        }
    }
    return rval;
}


bool s_IsReportableStructuredComment(const CSeqdesc& desc, const string& expected_prefix)
{
    if (!desc.IsUser()) {
        return false;
    }

    bool rval = false;

    const CUser_object& user = desc.GetUser();

    if (!user.IsSetType() || !user.GetType().IsStr()
    	|| !NStr::Equal(user.GetType().GetStr(), "StructuredComment")){
    	rval = false;
    } else {
        string prefix = CComment_rule::GetStructuredCommentPrefix(user);
        if (NStr::IsBlank (expected_prefix)) {
            if (!NStr::StartsWith(prefix, "##Genome-Assembly-Data", NStr::eNocase)
                && !NStr::StartsWith(prefix, "##Assembly-Data", NStr::eNocase)
                && !NStr::StartsWith(prefix, "##Genome-Annotation-Data", NStr::eNocase)) {
		        rval = true;
            }
        } else if (NStr::StartsWith(prefix, expected_prefix)) {
            rval = true;
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


void AddValueToTable (CSeq_table& table, string column_name, string value, size_t row)
{
    // do we already have a column for this subtype?
    bool found = false;
    NON_CONST_ITERATE (CSeq_table::TColumns, cit, table.SetColumns()) {
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
        table.SetColumns().push_back(new_col);
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


static bool s_IsCitSub (const CSeqdesc& desc)
{
    if (!desc.IsPub() || !desc.GetPub().IsSetPub()) {
        return false;
    }
    ITERATE(CPubdesc::TPub::Tdata, it, desc.GetPub().GetPub().Get()) {
        if ((*it)->IsSub()) {
            return true;
        }
    }

    return false;
}


const string kSequenceID = "Sequence ID";
const string kAffilInst = "Institution";
const string kAffilDept = "Department";
const string kBioProject = "BioProject";

// This function is for generating a table of biosample values for a bioseq
// that does not currently have a biosample ID
void AddBioseqToTable(CBioseq_Handle bh, CSeq_table& table, bool with_id,
                      bool include_comments, const string& expected_prefix)
{
    vector<string> biosample_ids = GetBiosampleIDs(bh);
    if (biosample_ids.size() > 0 && !with_id) {
        // do not collect if already has biosample ID
        string msg = GetBestBioseqLabel(bh) + " already has Biosample ID " + biosample_ids[0];
        NCBI_THROW(CException, eUnknown, msg );
    }
    vector<string> bioproject_ids = GetBioProjectIDs(bh);

    CSeqdesc_CI src_desc_ci(bh, CSeqdesc::e_Source);
    CSeqdesc_CI comm_desc_ci(bh, CSeqdesc::e_User);
    while (comm_desc_ci && !s_IsReportableStructuredComment(*comm_desc_ci, expected_prefix)) {
        ++comm_desc_ci;
    }

    CSeqdesc_CI pub_desc_ci(bh, CSeqdesc::e_Pub);
    while (pub_desc_ci && !s_IsCitSub(*pub_desc_ci)) {
        ++pub_desc_ci;
    }

    if (!src_desc_ci && !comm_desc_ci && bioproject_ids.size() == 0 && !pub_desc_ci) {
        return;
    }

    string sequence_id = GetBestBioseqLabel(bh);
    size_t row = table.GetNum_rows();
    AddValueToTable (table, kSequenceID, sequence_id, row);

    if (bioproject_ids.size() > 0) {
        string val = bioproject_ids[0];
        for (size_t i = 1; i < bioproject_ids.size(); i++) {
            val += ";";
            val += bioproject_ids[i];
        }
        AddValueToTable(table, kBioProject, val, row);
    }

    if (pub_desc_ci) {
        ITERATE(CPubdesc::TPub::Tdata, it, pub_desc_ci->GetPub().GetPub().Get()) {
            if ((*it)->IsSub() && (*it)->GetSub().IsSetAuthors() && (*it)->GetSub().GetAuthors().IsSetAffil()) {
                const CAffil& affil = (*it)->GetSub().GetAuthors().GetAffil();
                if (affil.IsStd()) {
                    if (affil.GetStd().IsSetAffil()) {
                        AddValueToTable(table, kAffilInst, affil.GetStd().GetAffil(), row);
                    }
                    if (affil.GetStd().IsSetDiv()) {
                        AddValueToTable(table, kAffilDept, affil.GetStd().GetDiv(), row);
                    }
                } else if (affil.IsStr()) {
                    AddValueToTable(table, kAffilInst, affil.GetStr(), row);
                }
                break;
            }
        }
    }

    if (src_desc_ci) {
        const CBioSource& src = src_desc_ci->GetSource();
        CBioSource::TNameValList src_vals = src.GetNameValPairs();
        ITERATE(CBioSource::TNameValList, it, src_vals) {
            AddValueToTable(table, it->first, it->second, row);
        }
    }

    if (include_comments) {
        while (comm_desc_ci) {
            const CUser_object& usr = comm_desc_ci->GetUser();
            TStructuredCommentTableColumnList comm_fields = GetStructuredCommentFields(usr);
            ITERATE(TStructuredCommentTableColumnList, it, comm_fields) {
                string label = (*it)->GetLabel();
                AddValueToTable(table, (*it)->GetLabel(), (*it)->GetFromComment(usr), row);
            }
            ++comm_desc_ci;
            while (comm_desc_ci && !s_IsReportableStructuredComment(*comm_desc_ci, expected_prefix)) {
                ++comm_desc_ci;
            }
        }
    }
    if (with_id && biosample_ids.size() > 0) {
        AddValueToTable(table, "BioSample ID", biosample_ids[0], row);
    }
    int num_rows = (int)row  + 1;
    table.SetNum_rows(num_rows);
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


bool ResolveSuppliedBioSampleAccession(const string& biosample_accession, vector<string>& biosample_ids)
{
    if (!NStr::IsBlank(biosample_accession)) {
        if (biosample_ids.size() == 0) {
            // use supplied BioSample accession
            biosample_ids.push_back(biosample_accession);
        } else {
           // make sure supplied BioSample accession is listed
            bool found = false;
            ITERATE(vector<string>, it, biosample_ids) {
                if (NStr::EqualNocase(*it, biosample_accession)) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                return false;
	    }
            biosample_ids.clear();
            biosample_ids.push_back(biosample_accession);
        }
    }
    return true;
}


string GetBestBioseqLabel(CBioseq_Handle bsh)
{
    string label = "";
 
    CConstRef<CSeq_id> id(NULL);
    vector<CRef <CSeq_id> > id_list;
    ITERATE(CBioseq_Handle::TId, it, bsh.GetId()) {
        CConstRef<CSeq_id> ir = (*it).GetSeqId();
        if (ir->IsGenbank()) {
            id = ir;
        }
        CRef<CSeq_id> ic(const_cast<CSeq_id *>(ir.GetPointer()));
        id_list.push_back(ic);
    }
    if (!id) {
        id = FindBestChoice(id_list, CSeq_id::BestRank);
    }
    if (id) {
        id->GetLabel(&label);
    }

    return label;
}


TBiosampleFieldDiffList 
GetBioseqDiffs(CBioseq_Handle bh,
               const string& biosample_accession, 
               size_t& num_processed, 
               vector<string>& unprocessed_ids,
               bool use_dev_server, 
               bool compare_structured_comments,
               const string& expected_prefix)
{
    TBiosampleFieldDiffList diffs;

    CSeqdesc_CI src_desc_ci(bh, CSeqdesc::e_Source);
    CSeqdesc_CI comm_desc_ci(bh, CSeqdesc::e_User);
    vector<string> user_labels;
    vector<CConstRef<CUser_object> > user_objs;    
    while (comm_desc_ci) {
        if (s_IsReportableStructuredComment(*comm_desc_ci, expected_prefix)) {
            const CUser_object& user = comm_desc_ci->GetUser();
            string prefix = CComment_rule::GetStructuredCommentPrefix(user);
            CConstRef<CUser_object> obj(&user);
            user_labels.push_back(prefix);
            user_objs.push_back(obj);
        }
        ++comm_desc_ci;
    }
    if (!src_desc_ci && user_labels.size() == 0) {
        return diffs;
    }

    vector<string> biosample_ids = GetBiosampleIDs(bh);

    if (!ResolveSuppliedBioSampleAccession(biosample_accession, biosample_ids)) {
        // error
        string msg = GetBestBioseqLabel(bh) + " has conflicting BioSample Accession " + biosample_ids[0];
        NCBI_THROW(CException, eUnknown, msg );
    }

    if (biosample_ids.size() == 0) {
        // for report mode, do not report if no biosample ID
        return diffs;
    }

    string sequence_id = GetBestBioseqLabel(bh);

    ITERATE(vector<string>, id, biosample_ids) {
        CRef<CSeq_descr> descr = GetBiosampleData(*id, use_dev_server);
        if (descr) {
            ITERATE(CSeq_descr::Tdata, it, descr->Get()) {
                if ((*it)->IsSource()) {
                    if (src_desc_ci) {
                        TBiosampleFieldDiffList these_diffs = GetFieldDiffs(sequence_id, 
                                                                            *id, 
                                                                            src_desc_ci->GetSource(), 
                                                                            (*it)->GetSource());
                        diffs.insert(diffs.end(), these_diffs.begin(), these_diffs.end());
                    }
                } else if ((*it)->IsUser() && s_IsReportableStructuredComment(**it, expected_prefix)) {
                    if (compare_structured_comments) {
                        CConstRef<CUser_object> sample(&(*it)->GetUser());
                        string this_prefix = CComment_rule::GetStructuredCommentPrefix((*it)->GetUser());
                        bool found = false;
                        vector<string>::iterator sit = user_labels.begin();
                        vector<CConstRef<CUser_object> >::iterator uit = user_objs.begin();
                        while (sit != user_labels.end() && uit != user_objs.end()) {
                            if (NStr::EqualNocase(*sit, this_prefix)) {
                                TBiosampleFieldDiffList these_diffs = GetFieldDiffs(sequence_id, *id, *uit, sample);
                                diffs.insert(diffs.end(), these_diffs.begin(), these_diffs.end());
                                found = true;
                            }
                            ++sit;
                            ++uit;
                        }
                        if (!found) {
                            TBiosampleFieldDiffList these_diffs = GetFieldDiffs(sequence_id, 
                                                                                *id,
                                                                                CConstRef<CUser_object>(NULL),
                                                                                sample);
                            diffs.insert(diffs.end(), these_diffs.begin(), these_diffs.end());                           
                        }
                    }
                }
            }
            num_processed++;
        } else {
            unprocessed_ids.push_back(*id);
        }
    }
    return diffs;
}


// section for creating XML

void AddContact(node::iterator& organization, CConstRef<CAuth_list> auth_list)
{
    string email = "";
    string street = "";
    string city = "";
    string sub = "";
    string country = "";
    string first = "";
    string last = "";
    bool add_address = false;

    CConstRef<CAffil> affil(NULL);
    if (auth_list && auth_list->IsSetAffil()) {
        affil = &(auth_list->GetAffil());
    }

    if (affil && affil->IsStd()) {
        const CAffil::TStd& std = affil->GetStd();
        string email = "";
        if (std.IsSetEmail()) {
            email = std.GetEmail();
        }
        if (std.IsSetStreet() && !NStr::IsBlank(std.GetStreet())
            && std.IsSetCity() && !NStr::IsBlank(std.GetCity())
            && std.IsSetSub() && !NStr::IsBlank(std.GetSub())
            && std.IsSetCountry() && !NStr::IsBlank(std.GetCountry())) {
            street = std.GetStreet();
            city = std.GetCity();
            sub = std.GetSub();
            country = std.GetCountry();
            add_address = true;
        }
    }

    if (auth_list && auth_list->IsSetNames() && auth_list->GetNames().IsStd()
        && auth_list->GetNames().GetStd().size()
        && auth_list->GetNames().GetStd().front()->IsSetName()
        && auth_list->GetNames().GetStd().front()->GetName().IsName()) {
        const CName_std& nstd = auth_list->GetNames().GetStd().front()->GetName().GetName();
        string first = "";
        string last = "";
        if (nstd.IsSetFirst()) {
            first = nstd.GetFirst();
        }
        if (nstd.IsSetLast()) {
            last = nstd.GetLast();
        }
    }

    if (NStr::IsBlank(email) || NStr::IsBlank(first) || NStr::IsBlank(last)) {
        // just don't add contact if no email address or name
        return;
    }
    node::iterator contact = organization->insert(node("Contact"));
    contact->get_attributes().insert("email", email.c_str());
    if (add_address) {
        node::iterator address = contact->insert(node("Address"));
        address->insert(node("Street", street.c_str()));
        address->insert(node("City", city.c_str()));
        address->insert(node("Sub", sub.c_str()));
        address->insert(node("Country", country.c_str()));
    }

    node::iterator name = contact->insert(node("Name"));
    name->insert(node("First", first.c_str()));
    name->insert(node("Last", last.c_str()));
}


void s_AddSamplePair(node& sample_attrs, string attribute_name, string val)
{
    sample_attrs.insert(node("Attribute", val.c_str()))
                    ->get_attributes().insert("attribute_name", attribute_name.c_str());

}

void AddBioSourceToAttributes(node& organism, node& sample_attrs, const CBioSource& src)
{
    if (src.IsSetSubtype()) {
        ITERATE(CBioSource::TSubtype, it, src.GetSubtype()) {
            if ((*it)->IsSetSubtype() && (*it)->IsSetName()) {
                CSubSource::TSubtype st = (*it)->GetSubtype();
                string attribute_name = "";
                if (st == CSubSource::eSubtype_other) {
                    attribute_name = "subsrc_note";
                } else {
                    attribute_name = CSubSource::GetSubtypeName((*it)->GetSubtype());
                }

                string val = (*it)->GetName();
                if (CSubSource::NeedsNoText(st) && NStr::IsBlank(val)) {
                    val = "true";
                }
                if (!CBioSource::ShouldIgnoreConflict(attribute_name, val, "")) {
                    s_AddSamplePair(sample_attrs, attribute_name, val);
                }
            }
        }
    }

    if (src.IsSetOrg()) {
        if (src.GetOrg().IsSetTaxname()) {
            organism.insert(node("OrganismName", src.GetOrg().GetTaxname().c_str()));
        }
        if (src.GetOrg().IsSetOrgMod()) {
            ITERATE(COrgName::TMod, it, src.GetOrg().GetOrgname().GetMod()) {
                if ((*it)->IsSetSubtype() && (*it)->IsSetSubname()) {
                    string attribute_name = "";
                    if ((*it)->GetSubtype() == COrgMod::eSubtype_other) {
                        attribute_name = "orgmod_note";
                    } else {
                        attribute_name = COrgMod::GetSubtypeName((*it)->GetSubtype());
                    }
                    if (!CBioSource::ShouldIgnoreConflict(attribute_name, (*it)->GetSubname(), "")) {
                        s_AddSamplePair(sample_attrs, attribute_name, (*it)->GetSubname());
                    }
                }
            }
        }
    }
}


static const string kStructuredCommentPrefix = "StructuredCommentPrefix";
static const string kStructuredCommentSuffix = "StructuredCommentSuffix";

void AddStructuredCommentToAttributes(node& sample_attrs, const CUser_object& usr)
{
    TStructuredCommentTableColumnList comm_fields = GetStructuredCommentFields(usr);
    ITERATE(TStructuredCommentTableColumnList, it, comm_fields) {
        string label = (*it)->GetLabel();
        if (NStr::EqualNocase(label, kStructuredCommentPrefix)
        || NStr::EqualNocase(label, kStructuredCommentSuffix)) {
            continue;
        }
        string val = (*it)->GetFromComment(usr);

        node::iterator a = sample_attrs.begin();
        bool found = false;
        while (a != sample_attrs.end() && !found) {
            if (NStr::Equal(a->get_name(), "Attribute")) {
                attributes::const_iterator at = a->get_attributes().begin();
                bool name_match = false;
                while (at != a->get_attributes().end() && !name_match) {
                    if (NStr::Equal(at->get_name(), "attribute_name")
                        && biosample_util::AttributeNamesAreEquivalent(at->get_value(), label)) {
                        name_match = true;
                    }
                    ++at;
                }
                if (name_match) {
                    if (NStr::Equal(a->get_content(), val)) {
                        found = true;
                    }
                }
            }
            ++a;
        }
        if (!found) {
            sample_attrs.insert(node("Attribute", val.c_str()))
                ->get_attributes().insert("attribute_name", label.c_str());
        }
    }
}


string OwnerFromAffil(const CAffil& affil)
{
    list<string> sbm_info;
    if (affil.IsStd()) {
        if (affil.GetStd().IsSetAffil()) {
            sbm_info.push_back(affil.GetStd().GetAffil());
        }
        if (affil.GetStd().IsSetDiv()
            && (!affil.GetStd().IsSetAffil()
                || !NStr::EqualNocase(affil.GetStd().GetDiv(), affil.GetStd().GetAffil()))) {
            sbm_info.push_back(affil.GetStd().GetDiv());
        }
    } else if (affil.IsStr()) {
        sbm_info.push_back(affil.GetStr());
    }

    return NStr::Join(sbm_info, ", ");
}


// This function is for generating a table of biosample values for a bioseq
// that does not currently have a biosample ID
void PrintBioseqXML(CBioseq_Handle bh,
                    const string& id_prefix,
                    CNcbiOstream* report_stream,
                    const string& bioproject_accession,
                    const string& default_owner,
                    const string& hup_date,
                    const string& comment,
                    bool first_seq_only,
                    bool report_structured_comments,
                    const string& expected_prefix)
{
    vector<string> biosample_ids = GetBiosampleIDs(bh);
    if (biosample_ids.size() > 0) {
        // do not collect if already has biosample ID
        string msg = GetBestBioseqLabel(bh) + " already has BioSample ID " + biosample_ids[0];
        NCBI_THROW(CException, eUnknown, msg );
    }
    vector<string> bioproject_ids = biosample_util::GetBioProjectIDs(bh);
    if (bioproject_ids.size() > 0) {
        if (!NStr::IsBlank(bioproject_accession)) {
            bool found = false;
            ITERATE(vector<string>, it, bioproject_ids) {
                if (NStr::EqualNocase(*it, bioproject_accession)) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                // error
                string msg = GetBestBioseqLabel(bh) +
                      " has conflicting BioProject ID " + bioproject_ids[0];
                NCBI_THROW(CException, eUnknown, msg );
            }
            bioproject_ids.clear();
            bioproject_ids.push_back(bioproject_accession);
        }
    } else if (!NStr::IsBlank(bioproject_accession)) {
        bioproject_ids.push_back(bioproject_accession);
    }

    CSeqdesc_CI src_desc_ci(bh, CSeqdesc::e_Source);
    CSeqdesc_CI comm_desc_ci(bh, CSeqdesc::e_User);
    while (comm_desc_ci && !s_IsReportableStructuredComment(*comm_desc_ci, expected_prefix)) {
        ++comm_desc_ci;
    }

    CConstRef<CAuth_list> auth_list(NULL);
    CSeqdesc_CI pub_desc_ci(bh, CSeqdesc::e_Pub);
    while (pub_desc_ci){
        if (pub_desc_ci->GetPub().IsSetPub()) {
            ITERATE(CPubdesc::TPub::Tdata, it, pub_desc_ci->GetPub().GetPub().Get()) {
                if ((*it)->IsSub() && (*it)->GetSub().IsSetAuthors()) {
                    auth_list = &((*it)->GetSub().GetAuthors());
                    break;
                } else if ((*it)->IsGen() && (*it)->GetGen().IsSetAuthors()) {
                    auth_list = &((*it)->GetGen().GetAuthors());
                }
            }
        }
        ++pub_desc_ci;
    }

    if (!src_desc_ci && !comm_desc_ci && bioproject_ids.size() == 0) {
        string msg = GetBestBioseqLabel(bh) + " has no BioSample information";
        NCBI_THROW(CException, eUnknown, msg );
    }

    string sequence_id = GetBestBioseqLabel(bh);
    CTime tNow(CTime::eCurrent);
    document doc("Submission");
    doc.set_encoding("UTF-8");
    doc.set_is_standalone(true);

    node & root = doc.get_root_node();

    node::iterator description = root.insert(node("Description"));
    string title = "Auto generated from GenBank Accession " + sequence_id;
    description->insert(node("Comment", title.c_str()));

    node::iterator node_iter = description->insert(node("Submitter"));

    CConstRef<CAffil> affil(NULL);
    if (auth_list && auth_list->IsSetAffil()) {
        affil = &(auth_list->GetAffil());
    }

    // Contact info
    node::iterator organization = description->insert(node("Organization"));
    {
        attributes & attrs = organization->get_attributes();
        attrs.insert("role", "owner");
        attrs.insert("type", "institute");
    }
    // same info for sample structure
    node owner("Owner");

    string owner_str = "";
    if (affil) {
        owner_str = OwnerFromAffil(*affil);
    }
    if (NStr::IsBlank(owner_str)) {
        owner_str = default_owner;
    }

    organization->insert(node("Name", owner_str.c_str()));
    owner.insert(node("Name", owner_str.c_str()));

    AddContact(organization, auth_list);

    if (!NStr::IsBlank(hup_date)) {
        node hup("Hold");
        hup.get_attributes().insert("release_date", hup_date.c_str());
        description->insert(hup);
    }

    node::iterator add_data = root.insert(node("Action"))->insert(node("AddData"));
    add_data->get_attributes().insert("target_db", "BioSample");

    // BioSample-specific xml
    node::iterator data = add_data->insert(node("Data"));
    data->get_attributes().insert("content_type", "XML");

    node::iterator sample = data->insert(node("XmlContent"))->insert(node("BioSample"));
    sample->get_attributes().insert("schema_version", "2.0");
    xml::ns ourns("xsi", "http://www.w3.org/2001/XMLSchema-instance");
    sample->add_namespace_definition(ourns, node::type_replace_if_exists);
    sample->get_attributes().insert("xsi:noNamespaceSchemaLocation", "http://www.ncbi.nlm.nih.gov/viewvc/v1/trunk/submit/public-docs/biosample/biosample.xsd?view=co");

    node::iterator ids = sample->insert(node("SampleId"));
    string sample_id = sequence_id;
    if (!NStr::IsBlank(id_prefix)) {
        if (first_seq_only) {
            sample_id = id_prefix;
        } else {
            sample_id = id_prefix + ":" + sequence_id;
        }
    }
    node_iter = ids->insert(node("SPUID", sample_id.c_str()));
    node_iter->get_attributes().insert( "spuid_namespace", "GenBank");

    node::iterator descr = sample->insert(node("Descriptor"));

    if (!NStr::IsBlank(comment)) {
        descr->insert(node("Description", comment.c_str()));
    }


    node::iterator organism = sample->insert(node("Organism"));

    // add unique bioproject links from series
    if (bioproject_ids.size() > 0) {
        node links("BioProject");
        ITERATE(vector<string>, it, bioproject_ids) {
            if (! it->empty()) {
                node_iter = links.insert(node("PrimaryId", it->c_str()));
                node_iter->get_attributes().insert("db", "BioProject");
            }
        }
        sample->insert(links);
    }

    sample->insert(node("Package", "Generic.1.0"));

    node sample_attrs("Attributes");
        if (src_desc_ci) {
                const CBioSource& src = src_desc_ci->GetSource();
        AddBioSourceToAttributes(*organism, sample_attrs, src);
    }

    if (report_structured_comments) {
        while (comm_desc_ci) {
            const CUser_object& usr = comm_desc_ci->GetUser();
            AddStructuredCommentToAttributes(sample_attrs, usr);
            ++comm_desc_ci;
            while (comm_desc_ci && !s_IsReportableStructuredComment(*comm_desc_ci, expected_prefix)) {
                ++comm_desc_ci;
            }
        }
    }
    sample->insert(sample_attrs);
    node::iterator identifier = add_data->insert(node("Identifier"));
    node_iter = identifier->insert(node("SPUID", sample_id.c_str()));
    node_iter->get_attributes().insert( "spuid_namespace", "GenBank");


    // write XML to file
    if (report_stream) {
        *report_stream << doc << endl;
    } else {
        string path = sequence_id;
        NStr::ReplaceInPlace(path, "|", "_");
        NStr::ReplaceInPlace(path, ".", "_");
        NStr::ReplaceInPlace(path, ":", "_");

        path = path + ".xml";
        CNcbiOstream* xml_out = new CNcbiOfstream(path.c_str());
        if (!xml_out)
        {
            NCBI_THROW(CException, eUnknown, "Unable to open " + path);
        }
        *xml_out << doc << endl;
    }
}


END_SCOPE(biosample_util)
END_SCOPE(objects)
END_NCBI_SCOPE
