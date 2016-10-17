#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbistd.hpp>
#include <serial/objistr.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/object_manager.hpp>

#include <objmgr/scope.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_field.hpp>

#include <objects/seqtable/SeqTable_column.hpp>
#include <objects/seqtable/Seq_table.hpp>

#include "prot_match_exception.hpp"


BEGIN_NCBI_SCOPE


USING_SCOPE(objects);


struct SNucMatchInfo {
    string accession;
    string status;
};

class CProteinMatchApp : public CNcbiApplication
{

public:
    void Init();
    int Run();

    typedef list<CRef<CSeq_annot>> TMatches;

private:
    bool x_GenerateMatchTable(
        const SNucMatchInfo& nuc_match_info,
        const TMatches& protein_matches,
        const list<string>& new_protein_ids,
        const list<string>& dead_proteins_accessions);
 
    bool x_TryProcessAnnotFile(const CArgs& args, 
        TMatches& matches, 
        list<string>& new_proteins,
        list<string>& dead_proteins);

    bool x_TryProcessAlignmentFile(
        const CArgs& args,
        SNucMatchInfo& nuc_match_info);

    bool x_IsPerfectAlignment(const CSeq_align& align) const;
    bool x_FetchAccessionVersion(const CSeq_align& align, 
        string& accver);

    CObjectIStream* x_InitInputAnnotStream(const CArgs& args) const;
    CObjectIStream* x_InitInputStream(const string& filename,
    const ESerialDataFormat serial) const;
    bool x_IsComparison(const CSeq_annot& seq_annot) const;
    bool x_HasCdsQuery(const CSeq_annot& seq_annot) const;
    bool x_HasCdsSubject(const CSeq_annot& seq_annot) const;
    bool x_HasNovelSubject(const CSeq_annot& seq_annot) const;
    bool x_HasNovelQuery(const CSeq_annot& seq_annot) const;
    bool x_IsCdsComparison(const CSeq_annot& seq_annot) const;
    bool x_IsGoodGloballyReciprocalBest(const CSeq_annot& seq_annot) const;
    bool x_IsGoodGloballyReciprocalBest(const CUser_object& user_obj) const;
    bool x_IsProteinMatch(const CSeq_annot& seq_annot) const;

    const CSeq_feat& x_GetQuery(const CSeq_annot& compare_annot) const;
    const CSeq_feat& x_GetSubject(const CSeq_annot& compare_annot) const;


    CAnnotdesc::TName x_GetComparisonClass(const CSeq_annot& seq_annot) const;
    string x_GetAccessionVersion(const CSeq_feat& seq_feat) const;
    string x_GetAccessionVersion(const CUser_object& obj) const;

    string x_GetLocalID(const CUser_object& obj) const;
    string x_GetLocalID(const CSeq_feat& seq_feat) const;

    string x_GetProductGI(const CSeq_feat& seq_feat) const;

    void x_AddColumn(const string& colName); 
    void x_AppendColumnValue(const string& colName,
                            const string& colVal);

    void x_WriteTable(CNcbiOstream& out);

    CNcbiOstream* x_InitOutputStream(const CArgs& args);


    CRef<CSeq_table> mMatchTable;
    // This maps the column name/title to its index in the CSeq_table::TData vector
    map<string, size_t> mColnameToIndex;

};


bool CProteinMatchApp::x_IsComparison(const CSeq_annot& seq_annot) const
{
   if (seq_annot.IsSetName() &&
       seq_annot.GetName() == "Comparison" &&
       seq_annot.IsFtable() &&
       seq_annot.GetData().GetFtable().size() == 2) {
       return true;
   }

   return false;
}



bool CProteinMatchApp::x_HasCdsQuery(const CSeq_annot& seq_annot) const 
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


bool CProteinMatchApp::x_HasCdsSubject(const CSeq_annot& seq_annot) const 
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



// Check to see if Seq-annot has the name "Comparison" and is a feature table 
// containing a pair of CDS features
bool CProteinMatchApp::x_IsCdsComparison(const CSeq_annot& seq_annot) const
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




// Check to see if the match is the best global reciprocal match 
bool CProteinMatchApp::x_IsGoodGloballyReciprocalBest(const CUser_object& user_obj) const
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



// Check to see if the match is the best global reciprocal match 
bool CProteinMatchApp::x_IsGoodGloballyReciprocalBest(const CSeq_annot& seq_annot) const 
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


// Check to see if the subject is novel
bool CProteinMatchApp::x_HasNovelSubject(const CSeq_annot& seq_annot) const 
{
    const string comparison_class = x_GetComparisonClass(seq_annot);
    return (comparison_class == "s-novel");
}


// Check to see if query is novel 
bool CProteinMatchApp::x_HasNovelQuery(const CSeq_annot& seq_annot) const 
{
    const string comparison_class = x_GetComparisonClass(seq_annot);
    return (comparison_class == "q-novel");

}


CAnnotdesc::TName CProteinMatchApp::x_GetComparisonClass(const CSeq_annot& seq_annot) const 
{
    if (seq_annot.IsSetDesc() &&
        seq_annot.GetDesc().IsSet() &&
        seq_annot.GetDesc().Get().size() >= 1 &&
        seq_annot.GetDesc().Get().front()->IsName()) {
        return seq_annot.GetDesc().Get().front()->GetName();
    }    
    return "";
}


const CSeq_feat& CProteinMatchApp::x_GetQuery(const CSeq_annot& compare_annot) const
{
    return *(compare_annot.GetData().GetFtable().front());
}


const CSeq_feat& CProteinMatchApp::x_GetSubject(const CSeq_annot& compare_annot) const
{
    return *(compare_annot.GetData().GetFtable().back());
}


bool CProteinMatchApp::x_IsProteinMatch(const CSeq_annot& seq_annot) const
{
    return x_IsCdsComparison(seq_annot) && x_IsGoodGloballyReciprocalBest(seq_annot);
}

/*
string CProteinMatchApp::x_GetAccessionVersion(const CSeq_annot& seq_annot) const 
{
    string result = "";

    const CSeq_feat& first = *seq_annot.GetData().GetFtable().front();
    const CSeq_feat& second = *seq_annot.GetData().GetFtable().back();

    result = x_GetAccessionVersion(first);

    if (result.empty()) {
        result = x_GetAccessionVersion(second);
    }

    return result;
}
*/

string CProteinMatchApp::x_GetAccessionVersion(const CSeq_feat& seq_feat) const 
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

/*
string CProteinMatchApp::x_GetProductGI(const CSeq_annot& seq_annot) const
{
    string result = "";

    const CSeq_feat& first = *seq_annot.GetData().GetFtable().front();
    const CSeq_feat& second = *seq_annot.GetData().GetFtable().back();

    result = x_GetProductGI(first);

    if (result.empty()) {
        result = x_GetProductGI(second);
    }

    return result;
}
*/

string CProteinMatchApp::x_GetProductGI(const CSeq_feat& seq_feat) const 
{
    string result = "";

    if (!seq_feat.IsSetProduct()) {
        return result;
    }

    const CSeq_loc& product = seq_feat.GetProduct();


    if (product.IsWhole() &&
        product.GetWhole().IsGi()) {
        return product.GetWhole().GetSeqIdString();
    }

    return result;
}

/*
string CProteinMatchApp::x_GetLocalID(const CSeq_annot& seq_annot) const 
{
    string result = "";

    const CSeq_feat& first = *seq_annot.GetData().GetFtable().front();
    const CSeq_feat& second = *seq_annot.GetData().GetFtable().back();

    result = x_GetLocalID(first);

    if (result.empty()) {
        result = x_GetLocalID(second);
    }

    return result;
}
*/

string CProteinMatchApp::x_GetLocalID(const CSeq_feat& seq_feat) const 
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


string CProteinMatchApp::x_GetAccessionVersion(const CUser_object& user_obj) const 
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


string CProteinMatchApp::x_GetLocalID(const CUser_object& user_obj) const 
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


void CProteinMatchApp::Init()
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(
            GetArguments().GetProgramBasename(),
            "Generate protein match table from the Seq-annots produced by the compare_annots utility",
            false);

    arg_desc->AddKey("annot-input",
            "InputFile",
            "Path to annotation input file",
            CArgDescriptions::eInputFile);

    arg_desc->AddKey("aln-input",
            "InputFile",
            "Path to nucleotide alignment input file",
            CArgDescriptions::eInputFile);

    arg_desc->AddOptionalKey("o", "OutputFile",
            "Output table file name. Defaults to stdout",
            CArgDescriptions::eOutputFile);

    arg_desc->AddFlag("text-input",
            "Read text input");

    SetupArgDescriptions(arg_desc.release());


    mMatchTable = Ref(new CSeq_table());
    mMatchTable->SetNum_rows(0);

    return;
}


int CProteinMatchApp::Run()
{
    const CArgs& args = GetArgs();

    SNucMatchInfo nuc_match_info;
    if (!x_TryProcessAlignmentFile(args, nuc_match_info)) {
        return 1;
    }

    list<string> new_proteins_ids;
    list<string> dead_protein_accessions;

    TMatches protein_matches;
    // Attempt to read a list of Seq-annots encoding the 
    // the protein matches.
    if (!x_TryProcessAnnotFile(args, 
                               protein_matches, 
                               new_proteins_ids, 
                               dead_protein_accessions)) {
        return 2;
    }

    // Generate mMatchTable (of type CRef<CSeq_table>) from the list of Seq-annots. 
    x_GenerateMatchTable(nuc_match_info,
                         protein_matches, 
                         new_proteins_ids, 
                         dead_protein_accessions);

    CNcbiOstream* pOs = x_InitOutputStream(args);

    // Write the contents of mMatchTable
    x_WriteTable(*pOs);

    return 0;    
}


bool CProteinMatchApp::x_TryProcessAlignmentFile(
        const CArgs& args,
        SNucMatchInfo& nuc_match_info)
{
    // Defaults to binary input
    ESerialDataFormat serial = eSerial_AsnBinary;
    if (args["text-input"]) {
        serial = eSerial_AsnText;
    }

    const string filename = args["aln-input"].AsString();

    CObjectIStream* pObjIstream;
    pObjIstream = x_InitInputStream(filename, serial);


    CRef<CSeq_align> pSeqAlign(new CSeq_align());
    try {
        pObjIstream->Read(ObjectInfo(*pSeqAlign));
    }
    catch (CException&) {
        return false;
    }
    // Find accession and status

    nuc_match_info.status = x_IsPerfectAlignment(*pSeqAlign) ? "Same" : "Changed";
    
    string accver;
    if (!x_FetchAccessionVersion(*pSeqAlign, accver)) {
        return false;
    }
    vector<string> accver_vec;
    NStr::Split(accver, ".", accver_vec);

    nuc_match_info.accession = accver_vec[0];

    return true;
}



bool CProteinMatchApp::x_IsPerfectAlignment(const CSeq_align& align) const 
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


bool CProteinMatchApp::x_FetchAccessionVersion(const CSeq_align& align, 
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

               accver = sequence::GetAccessionForGi(id->GetGi(),
                                                    *gb_scope);
               return true;
           }
        }
    }

    return false;
}



bool CProteinMatchApp::x_TryProcessAnnotFile(
        const CArgs& args,
        TMatches& matches, 
        list<string>& new_proteins,  // contains local ids for new proteins
        list<string>& dead_proteins) // contains accessions for dead proteins
{
    dead_proteins.clear();
    new_proteins.clear();

    set<string> new_protein_skip;
    set<string> dead_protein_skip;

    unique_ptr<CObjectIStream> pObjIstream(x_InitInputAnnotStream(args));

    list<CRef<CSeq_annot>> seq_annots;
    while (!pObjIstream->EndOfData()) {
        CRef<CSeq_annot> pSeqAnnot(new CSeq_annot());
        try { 
            pObjIstream->Read(ObjectInfo(*pSeqAnnot));
        } 
        catch (CException&) {
            return false;
        }
        seq_annots.push_back(pSeqAnnot);
    }


    for (CRef<CSeq_annot> pSeqAnnot : seq_annots) {
        if (x_IsProteinMatch(*pSeqAnnot)) {
            matches.push_back(pSeqAnnot);
            const CSeq_feat& subject = *(pSeqAnnot->GetData().GetFtable().back());
            const string accver = x_GetAccessionVersion(subject);
            dead_protein_skip.insert(accver);
            const CSeq_feat& query = *(pSeqAnnot->GetData().GetFtable().front());
            const string localid = x_GetLocalID(query);
            new_protein_skip.insert(localid);
            continue;
        }


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
            const string localid = x_GetLocalID(query);
            if (!NStr::IsBlank(localid)) {
                new_protein_skip.insert(localid);
            }
        }
    }


    for (CRef<CSeq_annot> pSeqAnnot : seq_annots) {
        if (x_HasCdsSubject(*pSeqAnnot) && 
            x_HasNovelSubject(*pSeqAnnot)) {
            const CSeq_feat& subject = *(pSeqAnnot->GetData().GetFtable().back());
            const string accver = x_GetAccessionVersion(subject);
            if (dead_protein_skip.find(accver) == dead_protein_skip.end()) {
                dead_protein_skip.insert(accver);
                dead_proteins.push_back(accver);
            }
            continue;
        }

        if (x_HasCdsQuery(*pSeqAnnot) && 
            x_HasNovelQuery(*pSeqAnnot)) {
            const CSeq_feat& query = *(pSeqAnnot->GetData().GetFtable().front());
            const string localid = x_GetLocalID(query);
            if (new_protein_skip.find(localid) == new_protein_skip.end()) {
                new_protein_skip.insert(localid);
                new_proteins.push_back(localid);
            }
            continue;
        }
    }

    return true;
}


// Add a new (empty) column to the table
void CProteinMatchApp::x_AddColumn(const string& colName)
{
    // Check that a column doesn't appear more than once in the Table
    if (mColnameToIndex.find(colName) != mColnameToIndex.end()) {
        return;
    }

    CRef<CSeqTable_column> pColumn(new CSeqTable_column());
    pColumn->SetHeader().SetField_name(colName); // Not the title. for internal use
    pColumn->SetHeader().SetTitle(colName); // The title appearing in the table header
    pColumn->SetDefault().SetString("");
    mColnameToIndex[colName] = mMatchTable->GetColumns().size();
    mMatchTable->SetColumns().push_back(pColumn);
}


// Add data specified by colVal to a table column
void CProteinMatchApp::x_AppendColumnValue(
        const string& colName,
        const string& colVal)
{
    size_t index = mColnameToIndex[colName];
    CSeqTable_column& column = *mMatchTable->SetColumns().at(index);
    column.SetData().SetString().push_back(colVal);
}



bool CProteinMatchApp::x_GenerateMatchTable(
        const SNucMatchInfo& nuc_match_info,
        const TMatches& matches,
        const list<string>& new_proteins,
        const list<string>& dead_proteins) 
{
    x_AddColumn("NA_Accession");
    x_AddColumn("PROT_Accession");
    x_AddColumn("PROT_LocalID");
    x_AddColumn("Mol_type");
    x_AddColumn("Status");

    x_AppendColumnValue("NA_Accession", nuc_match_info.accession);
    x_AppendColumnValue("PROT_Accession", "---");
    x_AppendColumnValue("PROT_LocalID", "---");
    x_AppendColumnValue("Mol_type", "NUC");
    x_AppendColumnValue("Status", nuc_match_info.status);

    mMatchTable->SetNum_rows(1);


    // Iterate over match Seq-annots
    TMatches::const_iterator cit;

    for (cit=matches.begin(); cit != matches.end(); ++cit) {

        const CSeq_feat& query = x_GetQuery(**cit);
        const CSeq_feat& subject = x_GetSubject(**cit);

        string localID = x_GetLocalID(query);
        const string accver = x_GetAccessionVersion(subject);

        vector<string> accver_vec;
        NStr::Split(accver, ".", accver_vec);

        string status = (x_GetComparisonClass(**cit) == "perfect") ? "Same" : "Changed";

        if (localID.empty()) {
            localID = "---";
        }
        x_AppendColumnValue("NA_Accession", nuc_match_info.accession);
        x_AppendColumnValue("PROT_Accession", accver_vec[0]);
        x_AppendColumnValue("PROT_LocalID", localID);
        x_AppendColumnValue("Mol_type", "PROT");
        x_AppendColumnValue("Status", status);

        mMatchTable->SetNum_rows(mMatchTable->GetNum_rows()+1);
    }

    for (string localID : new_proteins) {
        x_AppendColumnValue("NA_Accession", nuc_match_info.accession);
        x_AppendColumnValue("PROT_Accession", "---");
        x_AppendColumnValue("PROT_LocalID", localID);
        x_AppendColumnValue("Mol_type", "PROT");
        x_AppendColumnValue("Status", "New");
        mMatchTable->SetNum_rows(mMatchTable->GetNum_rows()+1);
    }

    for (string accver : dead_proteins) {
        vector<string> accver_vec;
        NStr::Split(accver, ".", accver_vec);
        x_AppendColumnValue("NA_Accession", nuc_match_info.accession);
        x_AppendColumnValue("PROT_Accession", accver_vec[0]);
        x_AppendColumnValue("PROT_LocalID", "---");
        x_AppendColumnValue("Mol_type", "PROT");
        x_AppendColumnValue("Status", "Dead");
        mMatchTable->SetNum_rows(mMatchTable->GetNum_rows()+1);
    }

    return true;
}

// Write the match table. Rows are odered by local ID
void CProteinMatchApp::x_WriteTable(
        CNcbiOstream& out)
{
    // colNames contains the column names in the order in which they were added to the table
    vector<string> colNames(mColnameToIndex.size());

    for (map<string,size_t>::const_iterator cit = mColnameToIndex.begin();
         cit != mColnameToIndex.end();
         ++cit) {
        colNames[cit->second] = cit->first;
    }

    // First, write the column titles
    for (const auto& column_name :  colNames) {
        const CSeqTable_column& column = mMatchTable->GetColumn(column_name);
        string displayName = column.GetHeader().GetTitle();
        out << displayName << "\t";
    }
    out << '\n';

    const unsigned int numRows = mMatchTable->GetNum_rows();

   for (int row_index=0; row_index<numRows; ++row_index) { 
        for (const auto& column_name : colNames) {
            const CSeqTable_column& column = mMatchTable->GetColumn(column_name);
            const string* pValue = column.GetStringPtr(row_index);
            out << *pValue << "\t";
        }
        out << '\n';
    } // iterate over rows
    return;
}



CObjectIStream* CProteinMatchApp::x_InitInputStream(const string& filename,
    const ESerialDataFormat serial) const
{
    CNcbiIstream* pInStream = new CNcbiIfstream(filename.c_str(), ios::in | ios::binary);

    if (pInStream->fail()) {
        NCBI_THROW(CProteinMatchException, 
                   eInputError, 
                   "Could not create input stream for \"" + filename + "\"");
    }

    CObjectIStream* pObjIStream = CObjectIStream::Open(serial,
                                                       *pInStream,
                                                       eTakeOwnership);

    if (!pObjIStream) {
        NCBI_THROW(CProteinMatchException, 
                   eInputError, 
                   "Unable to open input file \"" + filename + "\"");
    }

    return pObjIStream;
}


CObjectIStream* CProteinMatchApp::x_InitInputAnnotStream(const CArgs& args) const
{
    // Defaults to binary input
    ESerialDataFormat serial = eSerial_AsnBinary;
    if (args["text-input"]) {
        serial = eSerial_AsnText;
    }
    string infile = args["annot-input"].AsString();

    return x_InitInputStream(infile, serial);
}


CNcbiOstream* CProteinMatchApp::x_InitOutputStream(const CArgs& args)
{
    if (!args["o"]) {
        return &cout;
    }

    try {
        return &args["o"].AsOutputFile();
    }
    catch (...) {
        NCBI_THROW(CProteinMatchException,
                   eOutputError,
                   "Unable to open output file \"" + args["o"].AsString() + "\"");
    }

    return 0;
}



END_NCBI_SCOPE
USING_NCBI_SCOPE;


int main(int argc, const char* argv[])
{
    return CProteinMatchApp().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}

