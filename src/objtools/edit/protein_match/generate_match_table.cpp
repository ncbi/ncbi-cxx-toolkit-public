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
#include <objects/seqtable/SeqTable_column.hpp>
#include <objects/seqtable/Seq_table.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/edit/protein_match/prot_match_exception.hpp>
#include <objtools/edit/protein_match/generate_match_table.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

bool CMatchTabulate::ProcessAnnots(const list<CRef<CSeq_annot>>& annot_list,
    TMatches& matches,
    list<string>& new_proteins,  // contains local ids for new proteins
    list<string>& dead_proteins) // contains accessions for dead proteins
{
    dead_proteins.clear();
    new_proteins.clear();

    set<string> new_protein_skip;
    set<string> dead_protein_skip;

    for (CRef<CSeq_annot> pSeqAnnot : annot_list) {
        // Match by algorithm
        if (x_IsProteinMatch(*pSeqAnnot)) {
            matches.push_back(pSeqAnnot);
            const CSeq_feat& subject = *(pSeqAnnot->GetData().GetFtable().back());
            const string& accver = x_GetAccessionVersion(subject);
            dead_protein_skip.insert(accver);

            const CSeq_feat& query = *(pSeqAnnot->GetData().GetFtable().front());
            const string local_id = x_GetLocalID(query);
            new_protein_skip.insert(local_id);
            continue;
        }

        // Match due to same accession
        if (x_IsComparison(*pSeqAnnot) &&
            x_HasCdsQuery(*pSeqAnnot) &&
            x_HasCdsSubject(*pSeqAnnot)) {

            const CSeq_feat& query = *(pSeqAnnot->GetData().GetFtable().front());
            const CSeq_feat& subject = *(pSeqAnnot->GetData().GetFtable().back());

            const string query_accver = x_GetAccessionVersion(query);
            if (NStr::IsBlank(query_accver)) {
                continue;
            }

            const string subject_accver = x_GetAccessionVersion(subject);
            if (subject_accver != query_accver) {
                continue;
            }

            matches.push_back(pSeqAnnot);
            dead_protein_skip.insert(subject_accver);

            const string local_id = x_GetLocalID(query);
            if (!NStr::IsBlank(local_id)) {
                new_protein_skip.insert(local_id);
            }
        }
    }

    return true;
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


bool CMatchTabulate::x_HasNovelSubject(const CSeq_annot& seq_annot) const 
{
    const string comparison_class = x_GetComparisonClass(seq_annot);
    return (comparison_class == "s-novel");
}


bool CMatchTabulate::x_HasNovelQuery(const CSeq_annot& seq_annot) const 
{
    const string comparison_class = x_GetComparisonClass(seq_annot);
    return (comparison_class == "q-novel");

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


bool CMatchTabulate::x_IsProteinMatch(const CSeq_annot& seq_annot) const
{
    return x_IsCdsComparison(seq_annot) && x_IsGoodGloballyReciprocalBest(seq_annot);
}


bool CMatchTabulate::x_FetchAccessionVersion(const CSeq_align& align,
    string& accver) 
{
    const bool withVersion = true;

    if (align.IsSetSegs() &&
        align.GetSegs().IsDenseg() &&
        align.GetSegs().GetDenseg().IsSetIds()) {
        for (CRef<CSeq_id> id : align.GetSegs().GetDenseg().GetIds()) {
            if (id->IsGenbank()) {
                accver = id->GetSeqIdString(withVersion);
                return true;
            }

            if (id->IsGi()) {
                CRef<CObjectManager> obj_mgr = CObjectManager::GetInstance();
                CGBDataLoader::RegisterInObjectManager(*obj_mgr);
                CRef<CScope> gb_scope = Ref(new CScope(*obj_mgr));
                gb_scope->AddDataLoader("GBLOADER");

                accver = sequence::GetAccessionForGi(id->GetGi(), *gb_scope);
                return true;
            }

        }
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
             uf.GetLabel().GetStr() != "produce_localid") {
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


END_SCOPE(objects)
END_NCBI_SCOPE

