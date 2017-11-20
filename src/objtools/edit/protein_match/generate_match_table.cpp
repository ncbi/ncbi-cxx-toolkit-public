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
#include <objtools/edit/protein_match/prot_match_utils.hpp>
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

void CMatchTabulate::ReportWildDependents(
    const string& nuc_accession,
    const list<string>& wild_dependents) 
{
    if (!mMatchTableInitialized) {
        x_InitMatchTable();
        mMatchTableInitialized = true;
    }

    x_AppendNucleotide(nuc_accession, "Wild_dependent");
    for (const string& prot_accession : wild_dependents) {
        x_AppendProtein(nuc_accession, prot_accession, "Wild_dependent");
    }
}


void CMatchTabulate::OverwriteEntry(
    const SOverwriteIdInfo& match_info) 
{
    if (!mMatchTableInitialized) {
        x_InitMatchTable();
        mMatchTableInitialized = true;
    }

    const string& update_nuc_id = match_info.update_nuc_id;

    if (match_info.replaced_nuc_ids.empty()) {
        x_AppendNucleotide(update_nuc_id, "Same");
    }
    else {
        x_AppendNucleotide(update_nuc_id, match_info.replaced_nuc_ids);
    }


    for (const string& update_prot_id : match_info.update_prot_ids) {
        if (match_info.replaced_prot_id_map.find(update_prot_id) !=
            match_info.replaced_prot_id_map.end()) { 
            x_AppendUnchangedProtein(update_nuc_id, 
                    update_prot_id,
                    match_info.replaced_prot_id_map.at(update_prot_id));
        } 
        else {
            x_AppendUnchangedProtein(update_nuc_id, 
                    update_prot_id);
        }
    }

    // Look at database proteins for the replaced nucleotide sequences
    const list<string>& replaced_prot_ids = match_info.replaced_prot_ids;

    for (const string& replaced_nuc_id : match_info.replaced_nuc_ids) {
        if (match_info.DBEntryHasProteins(replaced_nuc_id)) {
            for (const string& db_prot_id : match_info.db_prot_ids.at(replaced_nuc_id)) {
                if (find(replaced_prot_ids.begin(), replaced_prot_ids.end(), db_prot_id) == 
                    replaced_prot_ids.end()) {
                    x_AppendDeadProtein(update_nuc_id, db_prot_id);
                } 
            }
        }
    }

    // Look at other database ids
    for (const string& db_nuc_id : match_info.db_nuc_ids) {
        if (!match_info.IsReplacedNucId(db_nuc_id)) {
            x_AppendNucleotide(db_nuc_id , "Dead");
            
            if (match_info.DBEntryHasProteins(db_nuc_id)) {
                for (const string& db_prot_id : match_info.db_prot_ids.at(db_nuc_id)) {
                    x_AppendDeadProtein(update_nuc_id, db_prot_id);
                }
            }
        } 
    }
}


void CMatchTabulate::GenerateMatchTable(
    const map<string, list<string>>& local_prot_ids,
    const map<string, list<string>>& prot_accessions,
    const list<string>& current_nuc_accessions,
    const map<string, string>& new_nuc_accessions,
    const list<CMatchIdInfo>& match_id_info,
    CObjectIStream& align_istr,
    CObjectIStream& annot_istr)
{
    if (!mMatchTableInitialized) {
        x_InitMatchTable();
        mMatchTableInitialized=true;
    }



    x_GenerateMatchTable(
            match_id_info,
            align_istr, 
            annot_istr);
/*
    map<string, bool> nuc_match;
    x_ProcessAlignments(align_istr, 
            nuc_match);

    x_ProcessProteins(annot_istr, 
        nuc_match, 
        local_prot_ids,
        prot_accessions,
        current_nuc_accessions,
        new_nuc_accessions,
        match_id_info
        );
        */
}


void CMatchTabulate::x_ProcessAlignments(
    CObjectIStream& istr,
    map<string, bool>& nucseq_changed) 
{
    string prev_accession = "";
    for (const CSeq_align& align :
            CObjectIStreamIterator<CSeq_align>(istr)) 
    {
        string accession;
        if (x_FetchAccession(align, accession)) {
            if (!NStr::IsBlank(accession)) {

                if ((accession == prev_accession) ||
                    !x_IsPerfectAlignment(align)) {
                    nucseq_changed[accession] = false;
                    continue;
                }
                nucseq_changed[accession] = true;
            }
            prev_accession = accession;
        }
    }
}


bool CMatchTabulate::x_GetMatch(const CSeq_annot& annot, 
                                string& nuc_accession,
                                CProtMatchInfo& match_info) 
{
    if (!x_IsCdsComparison(annot)) {
        return false;
    }

    nuc_accession = x_GetSubjectNucleotideAccession(annot);

    if (x_IsGoodGloballyReciprocalBest(annot)) {
        const CSeq_feat& subject = x_GetSubject(annot);
        const string db_prot_acc =  x_GetAccession(subject);

        const CSeq_feat& query = x_GetQuery(annot);
        const string update_prot_id = x_GetGeneralOrLocalID(query);
        match_info.SetAccession(db_prot_acc);
        match_info.SetOtherId(update_prot_id);
        match_info.SetExactMatch((x_GetComparisonClass(annot) == "perfect"));
        return true;
    }


    // Match by protein accession
    const CSeq_feat& query = x_GetQuery(annot);
    const CSeq_feat& subject = x_GetSubject(annot);

    const string db_prot_acc = x_GetAccession(query);
    const string update_prot_acc = x_GetAccession(subject);

    if (!NStr::IsBlank(update_prot_acc) && 
        (update_prot_acc == db_prot_acc)) {
        const string update_prot_id = x_GetGeneralOrLocalID(query);
        if (!NStr::IsBlank(update_prot_id)) {
            match_info.SetOtherId(update_prot_id);
        }
        match_info.SetAccession(db_prot_acc);
        match_info.SetExactMatch(false);
        return true;
    }

    return false;
}


void CMatchTabulate::x_ProcessAnnots(
    CObjectIStream& annot_istr,
    TProtMatchMap& match_map)
{
    for (const CSeq_annot& annot :
            CObjectIStreamIterator<CSeq_annot>(annot_istr)) {
        string nuc_accession;
        CProtMatchInfo match_info;
        if (x_GetMatch(annot, nuc_accession, match_info)) {
            match_map[nuc_accession].push_back(match_info);
        }
    }
}


void CMatchTabulate::x_ReportDeadDBEntry(
    const CMatchIdInfo& id_info) 
{
    const string& db_nuc_accession = id_info.GetDBNucId();
    x_AppendNucleotide(db_nuc_accession, "Dead");
    for (const string& db_prot_accession : id_info.GetDBProtIds()) {        
        x_AppendDeadProtein(db_nuc_accession, db_prot_accession);
    }
}


void CMatchTabulate::x_GenerateMatchTable(
    const list<CMatchIdInfo>& id_info_list,
    CObjectIStream& align_istr,
    CObjectIStream& annot_istr)
{
    map<string, bool> nucseq_same;
    x_ProcessAlignments(align_istr, nucseq_same); 

    TProtMatchMap prot_matches;
    x_ProcessAnnots(annot_istr, prot_matches);

    map<string, set<string>> matched_protein_accessions;
    map<string, set<string>> other_matched_protein_ids;

    for (const auto& key_val : nucseq_same) {
        const string& nuc_accession = key_val.first;        
        for (const CProtMatchInfo& prot_match : prot_matches[nuc_accession]) {
            matched_protein_accessions[nuc_accession].insert(prot_match.GetAccession());
            if (prot_match.IsSetOtherId()) {
                other_matched_protein_ids[nuc_accession].insert(prot_match.GetOtherId());
            }
        }
    }

    // loop over match id info
    for (const auto& id_info : id_info_list) {
        const string& update_nuc_accession = id_info.GetUpdateNucId();
        const string& db_nuc_accession = id_info.GetDBNucId();

        const bool no_alignment = (nucseq_same.find(db_nuc_accession) == nucseq_same.end());
    
        if (no_alignment) { // MSS-676
            x_ReportDeadDBEntry(id_info);
            continue;
        }
        // else we do have an alignment
        
        const string& nucseq_status = nucseq_same[db_nuc_accession] ? "Same" : "Changed";
        x_AppendNucleotide(update_nuc_accession, nucseq_status);
        if (prot_matches.find(db_nuc_accession) != prot_matches.end()) {
            for (const CProtMatchInfo& prot_match : prot_matches[db_nuc_accession]) {
                x_AppendMatchedProtein(update_nuc_accession, prot_match);
            }
        }

        for (const string& other_prot_id : id_info.GetUpdateOtherProtIds()) {
            if (other_matched_protein_ids[db_nuc_accession].find(other_prot_id) == 
                other_matched_protein_ids[db_nuc_accession].end()) {
                x_AppendNewProtein(update_nuc_accession, other_prot_id);
            }
        }

        for (const string& db_prot_accession : id_info.GetDBProtIds()) {
            if (matched_protein_accessions[db_nuc_accession].find(db_prot_accession) == 
                matched_protein_accessions[db_nuc_accession].end()) {
                x_AppendDeadProtein(update_nuc_accession, db_prot_accession);
            }
        }
    }
}



void CMatchTabulate::x_ProcessProteins(
        CObjectIStream& istr,
        const map<string, bool>& nuc_match,
        const map<string, list<string>>& local_prot_ids,
        const map<string, list<string>>& prot_accessions,
        const list<string>& current_nuc_accessions,
        const map<string, string>& new_nuc_accessions,
        const list<CMatchIdInfo>& match_id_info) // The name should be changed here
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

        // Match by similarity
        if (x_IsGoodGloballyReciprocalBest(comparison)) {

            const CSeq_feat& subject = x_GetSubject(comparison);
            const string& acc = x_GetAccession(subject);
            dead_protein_skip[nuc_acc].insert(acc);
            const CSeq_feat& query = x_GetQuery(comparison);
            const string local_id = x_GetGeneralOrLocalID(query);

           
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
            const string local_id = x_GetGeneralOrLocalID(query);
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

    for (const string& nuc_accession : current_nuc_accessions) {

        auto match_it = nuc_match.find(nuc_accession);
        if (match_it != nuc_match.end()) {
            const string displayed_nuc_accession = 
                (new_nuc_accessions.find(nuc_accession) == new_nuc_accessions.end()) ?
                nuc_accession :
                new_nuc_accessions.at(nuc_accession);


            const string& status = match_it->second ? "Same" : "Changed";
            x_AppendNucleotide(displayed_nuc_accession, status); 
            if (match_map.find(nuc_accession) != match_map.end()) { 
                for (const SProtMatchInfo& match : match_map.at(nuc_accession)) {
                    x_AppendMatchedProtein(displayed_nuc_accession, match);
                }
            }

            if (local_prot_ids.find(nuc_accession) != local_prot_ids.end()) {
                for (const string local_id : local_prot_ids.at(nuc_accession)) {
                    if (new_protein_skip[nuc_accession].find(local_id) == new_protein_skip[nuc_accession].end()) {
                      //  new_protein_skip[nuc_accession].insert(local_id);
                        x_AppendNewProtein(displayed_nuc_accession, local_id);
                    }
                }
            }

            if (prot_accessions.find(nuc_accession) != prot_accessions.end()) {
                for (const string prot_accver : prot_accessions.at(nuc_accession)) {
                    if (dead_protein_skip[nuc_accession].find(prot_accver) == dead_protein_skip[nuc_accession].end()) {
                      //  dead_protein_skip[nuc_accession].insert(prot_accver);
                        vector<string> accver_vec;
                        NStr::Split(prot_accver, ".", accver_vec);
                        x_AppendDeadProtein(displayed_nuc_accession, accver_vec[0]);
                    }
                }
            }
        }
        else { // no alignment for nucleotide sequence - report as dead (MSS-676)
            x_AppendNucleotide(nuc_accession, "Dead");
            if (prot_accessions.find(nuc_accession) != prot_accessions.end()) {
                for (const string prot_accver : prot_accessions.at(nuc_accession)) {
                    vector<string> accver_vec;
                    NStr::Split(prot_accver, ".", accver_vec);
                    x_AppendDeadProtein(nuc_accession, accver_vec[0]);
                }
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


bool CMatchTabulate::x_FetchLocalId(const CSeq_align& align,
    string& local_id) 
{
    const bool withVersion = false;
    if (align.IsSetSegs() &&
        align.GetSegs().IsDenseg() &&
        align.GetSegs().GetDenseg().IsSetIds()) {
        for (CRef<CSeq_id> id : align.GetSegs().GetDenseg().GetIds()) {
            if (id->IsLocal()) {
                local_id = id->GetSeqIdString(withVersion);
                return true;
            }
        }
    }
    return false;
}


bool CMatchTabulate::x_FetchAccession(const CSeq_align& align,
    string& accession) 
{
    const bool withVersion = false;
    if (align.IsSetSegs() &&
        align.GetSegs().IsDenseg() &&
        align.GetSegs().GetDenseg().IsSetIds()) {
        for (CRef<CSeq_id> id : align.GetSegs().GetDenseg().GetIds()) {
            if (id->IsGenbank() || id->IsOther()) { 
                accession = id->GetSeqIdString(withVersion);
                return true;
            }

            // Could optimize this by only performing look up if no genbank id has been 
            // found after the loop over look ups is complete
            if (id->IsGi()) {
                accession = sequence::GetAccessionForGi(id->GetGi(), *m_DBScope, sequence::eWithoutAccessionVersion);
                return true;
            }
        }
    }

    return false;
}


string CMatchTabulate::x_GetGeneralOrLocalID(const CSeq_feat& seq_feat) const 
{
    string result = "";
    
    if (!seq_feat.IsSetProduct()) {
        return result;
    }

    const CSeq_loc& product = seq_feat.GetProduct();

    if (product.IsWhole() &&
        (product.GetWhole().IsLocal() ||
         product.GetWhole().IsGeneral())) {
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
    x_AddColumn("Prot_Accession");
    x_AddColumn("Other_Prot_ID");
    x_AddColumn("Mol_type");
    x_AddColumn("Status");
    x_AddColumn("Replaces");

    mMatchTable->SetNum_rows(0);
}


void CMatchTabulate::x_AppendNucleotide(
    const string& accession, const string& status)
{
    x_AppendColumnValue("NA_Accession", accession);
    x_AppendColumnValue("Prot_Accession", "---");
    x_AppendColumnValue("Other_Prot_ID", "---");
    x_AppendColumnValue("Mol_type", "NUC");
    x_AppendColumnValue("Status", status);
    x_AppendColumnValue("Replaces", "---");
    mMatchTable->SetNum_rows(mMatchTable->GetNum_rows()+1);
}

void CMatchTabulate::x_AppendNucleotide(
    const string& accession,
    const list<string>& replaced_nuc_accessions)
{
    string replaces;
    for (const string& replaced_accession : replaced_nuc_accessions) {
        if (!replaces.empty()) {
            replaces += ";";
        }
        replaces += replaced_accession;
    }
    x_AppendColumnValue("NA_Accession", accession);
    x_AppendColumnValue("Prot_Accession", "---");
    x_AppendColumnValue("Other_Prot_ID", "---");
    x_AppendColumnValue("Mol_type", "NUC");
    x_AppendColumnValue("Status", "Same");

    if (NStr::IsBlank(replaces)) {
        replaces = "---";
    }
    x_AppendColumnValue("Replaces", replaces);
    mMatchTable->SetNum_rows(mMatchTable->GetNum_rows()+1);
}



void CMatchTabulate::x_AppendUnchangedProtein(
    const string& nuc_accession,
    const string& prot_accession)
{
    x_AppendColumnValue("NA_Accession", nuc_accession);
    x_AppendColumnValue("Prot_Accession", prot_accession);
    x_AppendColumnValue("Other_Prot_ID", "---");
    x_AppendColumnValue("Mol_type", "PROT");
    x_AppendColumnValue("Status", "Same");
    x_AppendColumnValue("Replaces", "---");
    mMatchTable->SetNum_rows(mMatchTable->GetNum_rows()+1);
}


void CMatchTabulate::x_AppendUnchangedProtein(
        const string& nuc_accession,
        const string& prot_accession,
        const list<string>& replaced_prot_accessions)
{   
    string replaces;
    for (const string& accession : replaced_prot_accessions) {
        if (!replaces.empty()) {
            replaces += ";";
        }
        replaces += accession;
    }

    x_AppendColumnValue("NA_Accession", nuc_accession);
    x_AppendColumnValue("Prot_Accession", prot_accession);
    x_AppendColumnValue("Other_Prot_ID", "---");
    x_AppendColumnValue("Mol_type", "PROT");
    x_AppendColumnValue("Status", "Same");
    if (NStr::IsBlank(replaces)) {
        replaces = "---";
    }
    x_AppendColumnValue("Replaces", replaces);
    mMatchTable->SetNum_rows(mMatchTable->GetNum_rows()+1);
    

}

void CMatchTabulate::x_AppendProtein(
    const string& nuc_accession,
    const string& prot_accession,
    const string& status) 
{
    x_AppendColumnValue("NA_Accession", nuc_accession);
    x_AppendColumnValue("Prot_Accession", prot_accession);
    x_AppendColumnValue("Other_Prot_ID", "---");
    x_AppendColumnValue("Mol_type", "PROT");
    x_AppendColumnValue("Status", status);
    x_AppendColumnValue("Replaces", "---");
    mMatchTable->SetNum_rows(mMatchTable->GetNum_rows()+1);
}


void CMatchTabulate::x_AppendNewProtein(
    const string& nuc_accession,
    const string& local_id)
{
    x_AppendColumnValue("NA_Accession", nuc_accession);
    x_AppendColumnValue("Prot_Accession", "---");
    x_AppendColumnValue("Other_Prot_ID", local_id);
    x_AppendColumnValue("Mol_type", "PROT");
    x_AppendColumnValue("Status", "New");
    x_AppendColumnValue("Replaces", "---");
    mMatchTable->SetNum_rows(mMatchTable->GetNum_rows()+1);
}


void CMatchTabulate::x_AppendDeadProtein(
    const string& nuc_accession,
    const string& prot_accession)
{
    x_AppendColumnValue("NA_Accession", nuc_accession);
    x_AppendColumnValue("Prot_Accession", prot_accession);
    x_AppendColumnValue("Other_Prot_ID", "---");
    x_AppendColumnValue("Mol_type", "PROT");
    x_AppendColumnValue("Status", "Dead");
    x_AppendColumnValue("Replaces", "---");
    mMatchTable->SetNum_rows(mMatchTable->GetNum_rows()+1);
}





void CMatchTabulate::x_AppendMatchedProtein(
    const string& nuc_accession,
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

    if (NStr::IsBlank(nuc_accession)) {
        x_AppendColumnValue("NA_Accession", match_info.nuc_accession);
    }
    else {
        x_AppendColumnValue("NA_Accession", nuc_accession);
    }
    x_AppendColumnValue("Prot_Accession", match_info.prot_accession);
    x_AppendColumnValue("Other_Prot_ID", local_id);
    x_AppendColumnValue("Mol_type", "PROT");
    x_AppendColumnValue("Status", status);
    x_AppendColumnValue("Replaces", "---");

    mMatchTable->SetNum_rows(mMatchTable->GetNum_rows()+1);
}


void CMatchTabulate::x_AppendMatchedProtein(
    const string& nuc_accession,
    const CProtMatchInfo& match_info)
{
    // No protein in database - skip
    if (NStr::IsBlank(nuc_accession) ||
        !match_info.IsSetAccession()) {
        return;
    }

    const string status = match_info.IsExactMatch() ? "Same" : "Changed";
    const string other_id = match_info.IsSetOtherId() ? 
                      match_info.GetOtherId()   :
                      "---";

    x_AppendColumnValue("NA_Accession", nuc_accession);
    x_AppendColumnValue("Prot_Accession", match_info.GetAccession());
    x_AppendColumnValue("Other_Prot_ID", other_id);
    x_AppendColumnValue("Mol_type", "PROT");
    x_AppendColumnValue("Status", status);
    x_AppendColumnValue("Replaces", "---");

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

