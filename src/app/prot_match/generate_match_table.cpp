#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbienv.hpp> // ??
#include <corelib/ncbistre.hpp> // ??
#include <serial/objistr.hpp>

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

class CProteinMatchApp : public CNcbiApplication
{

public:
    void Init();
    int Run();

    typedef list<CRef<CSeq_annot> > TMatches;

private:
    bool x_GenerateMatchTable(const TMatches& matches);
    bool x_TryProcessInputFile(const CArgs& args, TMatches& matches);
    CObjectIStream* x_InitInputStream(const CArgs& args) const;
    bool x_IsCdsComparison(const CSeq_annot& seq_annot) const;
    bool x_IsGoodGloballyReciprocalBest(const CSeq_annot& seq_annot) const;
    bool x_IsGoodGloballyReciprocalBest(const CUser_object& user_obj) const;
    bool x_IsProteinMatch(const CSeq_annot& seq_annot) const;
    CAnnotdesc::TName x_GetComparisonClass(const CSeq_annot& seq_annot) const;
    bool x_GetGGRBThreshold(const CSeq_annot& annot, double& threshold) const;
    bool x_GetGGRBThreshold(const CUser_object& user_object, double& threshold) const;
    string x_GetAccessionVersion(const CSeq_feat& seq_feat) const;
    string x_GetAccessionVersion(const CUser_object& obj) const;
    string x_GetAccessionVersion(const CSeq_annot& seq_annot) const;

    string x_GetLocalID(const CUser_object& obj) const;
    string x_GetLocalID(const CSeq_feat& seq_feat) const;
    string x_GetLocalID(const CSeq_annot& seq_annot) const;

    string x_GetProductGI(const CSeq_annot& seq_annot) const;
    string x_GetProductGI(const CSeq_feat& seq_feat) const;

    void x_AddColumn(const string& colName); 

    void x_AppendColumnValue(const string& colName,
                            const string& colVal);

    void x_WriteTable(CNcbiOstream& out);

    CNcbiOstream* x_InitOutputStream(const CArgs& args);


    CRef<CSeq_table> mMatchTable;
    map<string, size_t> mColnameToIndex;

};



bool CProteinMatchApp::x_IsCdsComparison(const CSeq_annot& seq_annot) const
{
    if (!seq_annot.IsFtable() ||
        seq_annot.GetData().GetFtable().size() != 2) {
        return false;
    }

    const CRef<CSeq_feat>& first = seq_annot.GetData().GetFtable().front();
    const CRef<CSeq_feat>& second = seq_annot.GetData().GetFtable().back();
   

    if (!first->IsSetData() ||
        !first->GetData().IsCdregion()  ||
        !second->IsSetData() ||
        !second->GetData().IsCdregion()) {
        return false;
    }
    return true;
}


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


bool CProteinMatchApp::x_GetGGRBThreshold(const CSeq_annot& seq_annot, double& threshold) const 
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

            if (x_GetGGRBThreshold(user, threshold)) {
                return true;
            }
        }
    }

    return false;
}



bool CProteinMatchApp::x_GetGGRBThreshold(const CUser_object& user_obj, double& threshold) const
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
            uf.GetLabel().GetStr() != "good_globally_reciprocal_best_threshold") {
           continue;
       }

       if (uf.GetData().IsReal()) {
           threshold = uf.GetData().GetReal();
           return true;
       }
   }

   return false;
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


bool CProteinMatchApp::x_IsProteinMatch(const CSeq_annot& seq_annot) const
{
    return x_IsCdsComparison(seq_annot) && x_IsGoodGloballyReciprocalBest(seq_annot);
}


string CProteinMatchApp::x_GetAccessionVersion(const CSeq_annot& seq_annot) const 
{
    string result = "";

    const CRef<CSeq_feat>& first = seq_annot.GetData().GetFtable().front();
    const CRef<CSeq_feat>& second = seq_annot.GetData().GetFtable().back();

    result = x_GetAccessionVersion(*first);

    if (result.empty()) {
        result = x_GetAccessionVersion(*second);
    }

    return result;
}



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

string CProteinMatchApp::x_GetProductGI(const CSeq_annot& seq_annot) const
{
    string result = "";

    const CRef<CSeq_feat>& first = seq_annot.GetData().GetFtable().front();
    const CRef<CSeq_feat>& second = seq_annot.GetData().GetFtable().back();

    result = x_GetProductGI(*first);

    if (result.empty()) {
        result = x_GetProductGI(*second);
    }

    return result;
}

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


string CProteinMatchApp::x_GetLocalID(const CSeq_annot& seq_annot) const 
{
    string result = "";

    const CRef<CSeq_feat>& first = seq_annot.GetData().GetFtable().front();
    const CRef<CSeq_feat>& second = seq_annot.GetData().GetFtable().back();

    result = x_GetLocalID(*first);

    if (result.empty()) {
        result = x_GetLocalID(*second);
    }

    return result;
}


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


// revisit x_GetLocalID
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

    arg_desc->AddKey("i",
            "InputFile",
            "Path to ASN.1 input file",
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

    TMatches matches;
    x_TryProcessInputFile(args, matches);
    x_GenerateMatchTable(matches);

    CNcbiOstream* pOs = x_InitOutputStream(args);

    x_WriteTable(*pOs);

    return 0;    
}



bool CProteinMatchApp::x_TryProcessInputFile(
        const CArgs& args,
        TMatches& matches) 
{
    auto_ptr<CObjectIStream> pObjIstream;
    pObjIstream.reset(x_InitInputStream(args));

    while (!pObjIstream->EndOfData()) {
        CRef<CSeq_annot> pSeqAnnot(new CSeq_annot());
        try { 
            pObjIstream->Read(ObjectInfo(*pSeqAnnot));
        } 
        catch (CException&) {
            return false;
        }
        if (x_IsProteinMatch(*pSeqAnnot)) {
            matches.push_back(pSeqAnnot);
        }
    }

    return true;
}



void CProteinMatchApp::x_AddColumn(const string& colName)
{
    CRef<CSeqTable_column> pColumn(new CSeqTable_column());
    pColumn->SetHeader().SetField_name(colName);
    pColumn->SetHeader().SetTitle(colName);
    pColumn->SetDefault().SetString("");
    mColnameToIndex[colName] = mMatchTable->GetColumns().size();
    mMatchTable->SetColumns().push_back(pColumn);
}



void CProteinMatchApp::x_AppendColumnValue(
        const string& colName,
        const string& colVal)
{
    size_t index = mColnameToIndex[colName];
    CSeqTable_column& column = *mMatchTable->SetColumns().at(index);
    column.SetData().SetString().push_back(colVal);
}



bool CProteinMatchApp::x_GenerateMatchTable(
        const TMatches& matches) 
{
    x_AddColumn("LocalID");
    x_AddColumn("AccVer");
    x_AddColumn("GI");
    x_AddColumn("CompClass");

    // Iterate over match Seq-annots
    TMatches::const_iterator cit;

    for (cit=matches.begin(); cit != matches.end(); ++cit) {
        string localID = x_GetLocalID(**cit);
        string gi = x_GetProductGI(**cit);
        string accver = x_GetAccessionVersion(**cit);
        string compClass = x_GetComparisonClass(**cit);

        x_AppendColumnValue("LocalID", localID);
        x_AppendColumnValue("AccVer", accver);
        x_AppendColumnValue("GI", gi);
        x_AppendColumnValue("CompClass", compClass);

        mMatchTable->SetNum_rows(mMatchTable->GetNum_rows()+1);
    }

    return true;
}


void CProteinMatchApp::x_WriteTable(
        CNcbiOstream& out)
{

    vector<string> colName(mColnameToIndex.size());

    for (map<string,size_t>::const_iterator cit = mColnameToIndex.begin();
         cit != mColnameToIndex.end();
         ++cit) {
        colName[cit->second] = cit->first;
    }

    for (vector<string>::const_iterator cit = colName.begin(); 
         cit != colName.end();
         ++cit) {
        const CSeqTable_column& column = mMatchTable->GetColumn(*cit);
        string displayName = column.GetHeader().GetTitle();
        out << displayName << "\t";
    }
    out << '\n';

    const unsigned int numRows = mMatchTable->GetNum_rows();
    map<string, unsigned int> RowNameToIndex;
    {
        const CSeqTable_column& column = mMatchTable->GetColumn("LocalID");
       
        for (unsigned int r=0; r<numRows; ++r) {
          const string* pValue = column.GetStringPtr(r);
          RowNameToIndex[*pValue] = r;
        }
    }


    for (map<string, unsigned int>::const_iterator row_it = RowNameToIndex.begin();
            row_it != RowNameToIndex.end();
            ++row_it) {

        unsigned int r = row_it->second;
    
        for (vector<string>::const_iterator cit = colName.begin();
             cit != colName.end();
             ++cit) {

            const CSeqTable_column& column = mMatchTable->GetColumn(*cit);

            const string* pValue = column.GetStringPtr(r);

            out << *pValue << "\t";
        }
        out << '\n';
    } // iterate over rows
    return;
}


CObjectIStream* CProteinMatchApp::x_InitInputStream(const CArgs& args) const
{
    ESerialDataFormat serial = eSerial_AsnBinary;
    if (args["text-input"]) {
        serial = eSerial_AsnText;
    }
    

    string infile = args["i"].AsString();

    CNcbiIstream* pInStream = new CNcbiIfstream(infile.c_str(), ios::in | ios::binary);

    if (pInStream->fail())
    {
        NCBI_THROW(CProteinMatchException, 
                   eInputError, 
                   "Could not create input stream for \"" + infile + "\"");
    }

    CObjectIStream* pObjIStream = CObjectIStream::Open(serial,
                                                       *pInStream,
                                                       eTakeOwnership);

    if (!pObjIStream) {
        NCBI_THROW(CProteinMatchException, 
                   eInputError, 
                   "Unable to open input file \"" + infile + "\"");
    }

    return pObjIStream;
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

