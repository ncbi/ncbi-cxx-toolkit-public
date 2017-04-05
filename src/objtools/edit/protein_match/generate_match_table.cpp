#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seqtable/SeqTable_column.hpp>
#include <objects/seqtable/Seq_table.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/edit/protein_match/prot_match_exception.hpp>
#include <objtools/edit/protein_match/generate_match_table.hpp>
#include <util/line_reader.hpp>

#include <serial/objistr.hpp>
#include <serial/streamiter.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CMatchTabulate::CMatchTabulate(CRef<CScope> db_scope) : 
    mMatchTable(Ref(new CSeq_table())),
    mMatchTableInitialized(false),
    m_DBScope(db_scope)
{
    mMatchTable->SetNum_rows(0);
}


CMatchTabulate::~CMatchTabulate() {}


bool CMatchTabulate::x_IsPerfectAlignment(const CSeq_align& align) const 
{
    size_t gap_count = 1;
    size_t num_mismatch = 1;
    if (align.IsSetScore()) {
        for (CRef<CScore> score : align.GetScore()) {

            if (score->IsSetId() &&
                score->GetId().IsStr()) {
        
                if (score->GetId().GetStr() == "num_mismatch") {
                    num_mismatch = score->GetValue().GetInt();
                    continue;
                }

                if (score->GetId().GetStr() == "gap_count") {
                    gap_count = score->GetValue().GetInt();
                    continue;
                }
            }
        }
    }

    if (!gap_count  && 
        !num_mismatch) {
        return true;
    }

    return false;
}



void CMatchTabulate::GenerateMatchTable(
    const map<string, list<string>>& local_prot_ids,
    const map<string, list<string>>& prot_accessions,
    const map<string, string>& nuc_id_replacements,
    CObjectIStream& align_istr,
    CObjectIStream& annot_istr)
{
    if (!mMatchTableInitialized) {
        x_InitMatchTable();
        mMatchTableInitialized=true;
    }

    map<string, bool> nuc_match;
    x_ProcessAlignments(align_istr, nuc_id_replacements, nuc_match);

    x_ProcessProteins(annot_istr, 
        nuc_match, 
        local_prot_ids,
        prot_accessions,
        nuc_id_replacements);
}


void CMatchTabulate::x_ProcessAlignments(
    CObjectIStream& istr,
    const map<string, string>& nuc_id_replacements,
    map<string, bool>& nuc_match) 
{
    string prev_accession = "";
    for (const CSeq_align& align :
            CObjectIStreamIterator<CSeq_align>(istr)) 
    {
        string accession;
        if (x_FetchAccession(align, accession)) {
            if (!NStr::IsBlank(accession)) {
                if (nuc_id_replacements.find(accession) != nuc_id_replacements.end()) {
                    accession = nuc_id_replacements.at(accession); 
                }

                if ((accession == prev_accession) ||
                    !x_IsPerfectAlignment(align)) {
                    nuc_match[accession] = false;
                    continue;
                }
                nuc_match[accession] = true;
            }
            prev_accession = accession;
        }
    }
}


void CMatchTabulate::x_ProcessProteins(
        CObjectIStream& istr,
        const map<string, bool>& nuc_accessions,
        const map<string, list<string>>& local_prot_ids,
        const map<string, list<string>>& prot_accessions,
        const map<string, string>& nuc_id_replacement_map)
{
    map<string, set<string>> new_protein_skip;
    map<string, set<string>> dead_protein_skip;
    map<string, list<SProtMatchInfo>> match_map;


    for (const CSeq_annot& comparison : 
            CObjectIStreamIterator<CSeq_annot>(istr)) {

        if (!x_IsCdsComparison(comparison)) {
            continue;
        }

        string nuc_acc = x_GetSubjectNucleotideAccession(comparison);
       
        if (nuc_id_replacement_map.find(nuc_acc) != nuc_id_replacement_map.end()) {
            nuc_acc = nuc_id_replacement_map.at(nuc_acc);    
        }

        // Match by similarity
        if (x_IsGoodGloballyReciprocalBest(comparison)) { 
            const CSeq_feat& subject = x_GetSubject(comparison);
            const string& acc = x_GetAccession(subject);
            dead_protein_skip[nuc_acc].insert(acc);
            const CSeq_feat& query = x_GetQuery(comparison);
            const string local_id = x_GetLocalID(query);
            if (!NStr::IsBlank(local_id)) {
                new_protein_skip[nuc_acc].insert(local_id); 
            }

            SProtMatchInfo match;
            match.nuc_accession     = nuc_acc;
            match.prot_accession    = acc;
            match.local_id          = local_id; 
            match.same              = (x_GetComparisonClass(comparison) == "perfect");
            match_map[nuc_acc].push_back(match);
        } 
        else { // Match by protein accession
            const CSeq_feat& query = x_GetQuery(comparison);
            const CSeq_feat& subject = x_GetSubject(comparison);

            const string query_acc = x_GetAccession(query);
            if (NStr::IsBlank(query_acc)) {
                continue;
            }

            const string subject_acc = x_GetAccession(subject);
            if (subject_acc != query_acc) {
                continue;
            }

            dead_protein_skip[nuc_acc].insert(subject_acc);
            const string local_id = x_GetLocalID(query);
            if (!NStr::IsBlank(local_id)) {
                new_protein_skip[nuc_acc].insert(local_id);
            }

            SProtMatchInfo match;
            match.nuc_accession = nuc_acc;
            match.prot_accession = subject_acc;
            match.local_id = local_id;
            match.same = false;
            match_map[nuc_acc].push_back(match);
        }
    }

    for (const auto& key_val : nuc_accessions) {
        x_AppendNucleotide(key_val); 
        const string nuc_accession = key_val.first;
        if (match_map.find(nuc_accession) != match_map.end()) { 
            for (const SProtMatchInfo& match : match_map.at(nuc_accession)) {
                x_AppendMatchedProtein(match);
            }
        }

        for (const string local_id : local_prot_ids.at(nuc_accession)) {
            if (new_protein_skip[nuc_accession].find(local_id) == new_protein_skip[nuc_accession].end()) {
                new_protein_skip[nuc_accession].insert(local_id);
                x_AppendNewProtein(nuc_accession, local_id);
            }
        }

        for (const string prot_accver : prot_accessions.at(nuc_accession)) {
            if (dead_protein_skip[nuc_accession].find(prot_accver) == dead_protein_skip[nuc_accession].end()) {
                dead_protein_skip[nuc_accession].insert(prot_accver);
                vector<string> accver_vec;
                NStr::Split(prot_accver, ".", accver_vec);
                x_AppendDeadProtein(nuc_accession, accver_vec[0]);
            }
        }
    }
    return;
}


// Check to see if Seq-annot has the name "Comparison" and is a feature table 
// containing a pair of CDS features
bool CMatchTabulate::x_IsCdsComparison(const CSeq_annot& seq_annot) const
{
    if (!x_IsComparison(seq_annot)) {
        return false;
    }
    const CSeq_feat& first = *seq_annot.GetData().GetFtable().front();
    const CSeq_feat& second = *seq_annot.GetData().GetFtable().back();

    if (first.IsSetData() &&
        first.GetData().IsCdregion()  &&
        second.IsSetData() &&
        second.GetData().IsCdregion()) {
        return true;
    }
    return false;
}


bool CMatchTabulate::x_IsGoodGloballyReciprocalBest(const CUser_object& user_obj) const
{
   if (!user_obj.IsSetType() ||
       !user_obj.IsSetData() ||
        user_obj.GetType().GetStr() != "Attributes") {
       return false;
   }

   ITERATE(CUser_object::TData, it, user_obj.GetData()) {

       const CUser_field& uf = **it;

       if (!uf.IsSetData() ||
           !uf.IsSetLabel() ||
           !uf.GetLabel().IsStr() ||
            uf.GetLabel().GetStr() != "good_globally_reciprocal_best") {
           continue;
       }

       if (uf.GetData().IsBool() &&
           uf.GetData().GetBool()) {
           return true;
       }
   }

   return false;
}


bool CMatchTabulate::x_IsGoodGloballyReciprocalBest(const CSeq_annot& seq_annot) const 
{
    if (seq_annot.IsSetDesc()) {
        ITERATE(CAnnot_descr::Tdata, desc_it, seq_annot.GetDesc().Get()) {
            if (!(*desc_it)->IsUser()) {
                continue;
            }

            const CUser_object& user = (*desc_it)->GetUser();
            if (!user.IsSetData() ||
                !user.IsSetType() ||
                 user.GetType().GetStr() != "Attributes") {
                continue;
            }

            if (x_IsGoodGloballyReciprocalBest(user)) {
                return true;
            }
        }
    }
    return false;
}


CAnnotdesc::TName CMatchTabulate::x_GetComparisonClass(const CSeq_annot& seq_annot) const 
{
    if (seq_annot.IsSetDesc() &&
        seq_annot.GetDesc().IsSet() &&
        seq_annot.GetDesc().Get().size() >= 1 &&
        seq_annot.GetDesc().Get().front()->IsName()) {
        return seq_annot.GetDesc().Get().front()->GetName();
    }    
    return "";
}


const CSeq_feat& CMatchTabulate::x_GetQuery(const CSeq_annot& compare_annot) const
{
    return *(compare_annot.GetData().GetFtable().front());
}


const CSeq_feat& CMatchTabulate::x_GetSubject(const CSeq_annot& compare_annot) const
{
    return *(compare_annot.GetData().GetFtable().back());
}


string CMatchTabulate::x_GetSubjectNucleotideAccession(const CSeq_annot& compare_annot)
{
    const bool withVersion = false;

    const CSeq_feat& feature = x_GetSubject(compare_annot);
    if (feature.GetData().IsCdregion()) {
        const CSeq_id* id_ptr = feature.GetLocation().GetId();
        if (id_ptr != nullptr) {
            if (id_ptr->IsGenbank() || id_ptr->IsOther()) {
                return id_ptr->GetSeqIdString(withVersion);
            }

            if (id_ptr->IsGi()) {
                return sequence::GetAccessionForGi(id_ptr->GetGi(), *m_DBScope, sequence::eWithoutAccessionVersion);
            }
        }
    }

    return "";
}


bool CMatchTabulate::x_FetchAccession(const CSeq_align& align,
    string& accession) 
{
    const bool withVersion = false;

    map<string, string> id_map;


    if (align.IsSetSegs() &&
        align.GetSegs().IsDenseg() &&
        align.GetSegs().GetDenseg().IsSetIds()) {
        for (CRef<CSeq_id> id : align.GetSegs().GetDenseg().GetIds()) {
            if (id->IsGenbank()) { 
                id_map["genbank"] = id->GetSeqIdString(withVersion);
                break;
            }

            if (id->IsOther()) {
                id_map["other"] = id->GetSeqIdString(withVersion);
            }

            // Could optimize this by only performing look up if no genbank id has been 
            // found after the loop over look ups is complete
            if (id->IsGi() && (id_map.find("genbank") == id_map.end())) {
                id_map["genbank"] = sequence::GetAccessionForGi(id->GetGi(), *m_DBScope, sequence::eWithoutAccessionVersion);
                break;
            }
        }
    }

    if (id_map.find("genbank") != id_map.end()) {
        accession = id_map["genbank"];
        return true;
    }

    if (id_map.find("other") != id_map.end()) {
        accession = id_map["other"];
        return true;
    }

    return false;
}


string CMatchTabulate::x_GetLocalID(const CSeq_feat& seq_feat) const 
{
    string result = "";
    
    if (!seq_feat.IsSetProduct()) {
        return result;
    }

    const CSeq_loc& product = seq_feat.GetProduct();

    if (product.IsWhole() &&
        product.GetWhole().IsLocal()) {
        return product.GetWhole().GetSeqIdString();
    }

    return "";
}


string CMatchTabulate::x_GetAccessionVersion(const CUser_object& user_obj) const 
{
   if (!user_obj.IsSetType() ||
       !user_obj.IsSetData() ||
        user_obj.GetType().GetStr() != "Comparison") {
       return "";
   }

   ITERATE(CUser_object::TData, it, user_obj.GetData()) {

       const CUser_field& uf = **it;

       if (!uf.IsSetData() ||
           !uf.IsSetLabel() ||
           !uf.GetLabel().IsStr() ||
            uf.GetLabel().GetStr() != "product_accver") {
           continue;
       }

       if (uf.GetData().IsStr()) {
           return uf.GetData().GetStr();
       }
   }

    return "";
}


string CMatchTabulate::x_GetAccessionVersion(const CSeq_feat& seq_feat) const 
{
    if (seq_feat.IsSetExts()) {
        ITERATE(CSeq_feat::TExts, it, seq_feat.GetExts()) {
            const CUser_object& usr_obj = **it;

                string acc_ver = x_GetAccessionVersion(usr_obj);
                if (!acc_ver.empty()) {
                    return acc_ver;
                }
        }
    }
    return "";
}


string CMatchTabulate::x_GetAccession(const CSeq_feat& seq_feat) const
{
    const string accver = x_GetAccessionVersion(seq_feat);

    if (!NStr::IsBlank(accver)) {
        vector<string> accver_vec;
        NStr::Split(accver, ".", accver_vec);
        if (accver_vec.size() > 0) {
            return accver_vec[0];
        }
    } 

    return "";
}


string CMatchTabulate::x_GetLocalID(const CUser_object& user_obj) const 
{
    if (!user_obj.IsSetType() ||
        !user_obj.IsSetData() ||
        user_obj.GetType().GetStr() != "Comparison") {
        return "";
    }

    ITERATE(CUser_object::TData, it, user_obj.GetData()) {
        
        const CUser_field& uf = **it;

        if (!uf.IsSetData() ||
            !uf.IsSetLabel() ||
            !uf.GetLabel().IsStr() ||
             uf.GetLabel().GetStr() != "product_localid") {
            continue;
        }

        if (uf.GetData().IsStr()) {
            return uf.GetData().GetStr();
        }
    }

    return "";
}


bool CMatchTabulate::x_HasCdsQuery(const CSeq_annot& seq_annot) const 
{
    if (!x_IsComparison(seq_annot)) {
        return false;
    }

    const CSeq_feat& query_feat = *seq_annot.GetData().GetFtable().front();
    if (query_feat.IsSetData() &&
        query_feat.GetData().IsCdregion()) {
        return true;
    }
    return false;
}


bool CMatchTabulate::x_HasCdsSubject(const CSeq_annot& seq_annot) const 
{
    if (!x_IsComparison(seq_annot)) {
        return false;
    }

    const CSeq_feat& subject_feat = *seq_annot.GetData().GetFtable().back();
    if (subject_feat.IsSetData() &&
        subject_feat.GetData().IsCdregion()) {
        return true;
    }
    return false;
}


bool CMatchTabulate::x_IsComparison(const CSeq_annot& seq_annot) const
{
   if (seq_annot.IsSetName() &&
       seq_annot.GetName() == "Comparison" &&
       seq_annot.IsFtable() &&
       seq_annot.GetData().GetFtable().size() == 2) {
       return true;
   }

   return false;
}


void CMatchTabulate::x_InitMatchTable() 
{
    x_AddColumn("NA_Accession");
    x_AddColumn("PROT_Accession");
    x_AddColumn("PROT_LocalID");
    x_AddColumn("Mol_type");
    x_AddColumn("Status");

    mMatchTable->SetNum_rows(0);
}

/*
void CMatchTabulate::x_AppendNucleotide(
    const SNucMatchInfo& nuc_match_info)
{
    x_AppendColumnValue("NA_Accession", nuc_match_info.accession);
    x_AppendColumnValue("PROT_Accession", "---");
    x_AppendColumnValue("PROT_LocalID", "---");
    x_AppendColumnValue("Mol_type", "NUC");
    x_AppendColumnValue("Status", nuc_match_info.status);

    mMatchTable->SetNum_rows(mMatchTable->GetNum_rows()+1);
}
*/

void CMatchTabulate::x_AppendNucleotide(
    const pair<string, bool>& match_info)
{
    const string status = match_info.second ? "Same" : "Changed";

    x_AppendColumnValue("NA_Accession", match_info.first);
    x_AppendColumnValue("PROT_Accession", "---");
    x_AppendColumnValue("PROT_LocalID", "---");
    x_AppendColumnValue("Mol_type", "NUC");
    x_AppendColumnValue("Status", status);
    mMatchTable->SetNum_rows(mMatchTable->GetNum_rows()+1);
}


void CMatchTabulate::x_AppendNewProtein(
    const string& nuc_accession,
    const string& local_id)
{
    x_AppendColumnValue("NA_Accession", nuc_accession);
    x_AppendColumnValue("PROT_Accession", "---");
    x_AppendColumnValue("PROT_LocalID", local_id);
    x_AppendColumnValue("Mol_type", "PROT");
    x_AppendColumnValue("Status", "New");
    mMatchTable->SetNum_rows(mMatchTable->GetNum_rows()+1);
}


void CMatchTabulate::x_AppendDeadProtein(
    const string& nuc_accession,
    const string& prot_accession)
{
    x_AppendColumnValue("NA_Accession", nuc_accession);
    x_AppendColumnValue("PROT_Accession", prot_accession);
    x_AppendColumnValue("PROT_LocalID", "---");
    x_AppendColumnValue("Mol_type", "PROT");
    x_AppendColumnValue("Status", "Dead");
    mMatchTable->SetNum_rows(mMatchTable->GetNum_rows()+1);
}

/*
void CMatchTabulate::x_AppendMatchedProtein(
    const string& nuc_accession,
    const string& prot_accession,
    const string& local_id,
    const string& comp_class)
{
    // No protein in database - skip
    if (NStr::IsBlank(nuc_accession) ||
        NStr::IsBlank(prot_accession)) {
        return;
    }

    const string status = (comp_class == "perfect") ? "Same" : "Changed";

    string local_id_string = local_id;
    if (NStr::IsBlank(local_id)){ 
        local_id_string = "---";
    }

    x_AppendColumnValue("NA_Accession", nuc_accession);
    x_AppendColumnValue("PROT_Accession", prot_accession);
    x_AppendColumnValue("PROT_LocalID", local_id_string);
    x_AppendColumnValue("Mol_type", "PROT");
    x_AppendColumnValue("Status", status);

    mMatchTable->SetNum_rows(mMatchTable->GetNum_rows()+1);
}
*/

void CMatchTabulate::x_AppendMatchedProtein(
    const SProtMatchInfo& match_info)
{
    // No protein in database - skip
    if (NStr::IsBlank(match_info.nuc_accession) ||
        NStr::IsBlank(match_info.prot_accession)) {
        return;
    }

    const string status = match_info.same ? "Same" : "Changed";

    string local_id = match_info.local_id;
    if (NStr::IsBlank(local_id)){ 
        local_id = "---";
    }

    x_AppendColumnValue("NA_Accession", match_info.nuc_accession);
    x_AppendColumnValue("PROT_Accession", match_info.prot_accession);
    x_AppendColumnValue("PROT_LocalID", local_id);
    x_AppendColumnValue("Mol_type", "PROT");
    x_AppendColumnValue("Status", status);

    mMatchTable->SetNum_rows(mMatchTable->GetNum_rows()+1);
}


void CMatchTabulate::x_AddColumn(const string& colName) 
{
    // Check that a column doesn't appear more than once in the table
    if (mColnameToIndex.find(colName) != mColnameToIndex.end()) {
        return;
    }

    CRef<CSeqTable_column> pColumn(new CSeqTable_column());
    pColumn->SetHeader().SetField_name(colName); // Not the title, for internal use
    pColumn->SetHeader().SetTitle(colName);
    pColumn->SetDefault().SetString("");
    mColnameToIndex[colName] = mMatchTable->GetColumns().size();
    mMatchTable->SetColumns().push_back(pColumn);
}


void CMatchTabulate::x_AppendColumnValue(
    const string& colName,
    const string& colVal)
{
    size_t index = mColnameToIndex[colName];
    CSeqTable_column& column = *mMatchTable->SetColumns().at(index);
    column.SetData().SetString().push_back(colVal);
}



void CMatchTabulate::WriteTable(const CSeq_table& table,
    CNcbiOstream& out)
{
    for (auto pColumn : table.GetColumns()) {
        string displayName = pColumn->GetHeader().GetTitle();
        out << displayName << "\t";
    }

    out << '\n';

    const auto numRows = table.GetNum_rows();

    for (auto row_index=0; row_index<numRows; ++row_index) { 
        for (auto pColumn : table.GetColumns()) {
            const string* pValue = pColumn->GetStringPtr(row_index);
            out << *pValue << "\t";
        }
        out << '\n';
    } // iterate over rows
    return;
}


void CMatchTabulate::WriteTable(
        CNcbiOstream& out) const
{
    WriteTable(*mMatchTable, out);
}


TSeqPos CMatchTabulate::GetNum_rows(void) const 
{
    if (!mMatchTable || mMatchTable.IsNull()) {
        return 0;
    }
    return mMatchTable->GetNum_rows();
}


void g_ReadSeqTable(CNcbiIstream& in, CSeq_table& table) 
{

    //CRef<CSeq_table> table = Ref(new CSeq_table());
    table.Reset();
    CRef<ILineReader> pLineReader = ILineReader::New(in);

    if ( pLineReader->AtEOF() ) {
        return;
    }

    pLineReader->ReadLine();
    string line = pLineReader->GetCurrentLine();
    list<string> colNames;
    NStr::Split(line, " \t", colNames, NStr::fSplit_Tokenize);

    for (auto colName : colNames) {
        CRef<CSeqTable_column> pColumn(new CSeqTable_column());
        pColumn->SetHeader().SetField_name(colName); // Not the title, for internal use
        pColumn->SetHeader().SetTitle(colName);
        pColumn->SetDefault().SetString("");
        table.SetColumns().push_back(pColumn);
    }


    TSeqPos num_rows = 0;
    while (!pLineReader->AtEOF()) {
        pLineReader->ReadLine();
        line = pLineReader->GetCurrentLine();
        list<string> colValues;
        NStr::Split(line, " \t", colValues, NStr::fSplit_Tokenize);
        auto it = colValues.begin();
        for (CRef<CSeqTable_column> pColumn : table.SetColumns()) { 
            pColumn->SetData().SetString().push_back(*it);
            ++it;
        }
        ++num_rows;
    }

    table.SetNum_rows(num_rows);
}


END_SCOPE(objects)
END_NCBI_SCOPE

