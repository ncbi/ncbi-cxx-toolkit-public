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

private:
    bool xGenerateMatchTable(const list<CRef<CSeq_annot> >& matches);
    bool xTryProcessInputFile(const CArgs& args, list<CRef<CSeq_annot> >& matches);
    CObjectIStream* xInitInputStream(const CArgs& args) const;
    bool xIsCdsComparison(const CSeq_annot& seq_annot) const;
    bool xIsGoodGloballyReciprocalBest(const CSeq_annot& seq_annot) const;
    bool xIsGoodGloballyReciprocalBest(const CUser_object& user_obj) const;
    bool xIsProteinMatch(const CSeq_annot& seq_annot) const;
    CAnnotdesc::TName xGetComparisonClass(const CSeq_annot& seq_annot) const;
    bool xGetGGRBThreshold(const CSeq_annot& annot, double& threshold) const;
    bool xGetGGRBThreshold(const CUser_object& user_object, double& threshold) const;
    string xGetAccessionVersion(const CSeq_feat& seq_feat) const;
    string xGetAccessionVersion(const CUser_object& obj) const;
    string xGetAccessionVersion(const CSeq_annot& seq_annot) const;

    string xGetLocalID(const CUser_object& obj) const;
    string xGetLocalID(const CSeq_feat& seq_feat) const;
    string xGetLocalID(const CSeq_annot& seq_annot) const;

    string xGetProductGI(const CSeq_annot& seq_annot) const;
    string xGetProductGI(const CSeq_feat& seq_feat) const;

    void xAddColumn(const string& colName); 

    void xAppendColumnValue(const string& colName,
                            const string& colVal);


    void xFormatTabDelimited(CNcbiOstream& out);

    CNcbiOstream* xInitOutputStream(const CArgs& args);


    CRef<CSeq_table> mMatchTable;
    map<string, size_t> mColnameToIndex;

};



bool CProteinMatchApp::xIsCdsComparison(const CSeq_annot& seq_annot) const
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


bool CProteinMatchApp::xIsGoodGloballyReciprocalBest(const CUser_object& user_obj) const
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



bool CProteinMatchApp::xIsGoodGloballyReciprocalBest(const CSeq_annot& seq_annot) const 
{

    if (seq_annot.IsSetDesc()) {
        // Iterate over annot descriptors. 
        // Find one that is user object
        // Find data label with "good_globally_reciprocal_best" 
        // and data == TRUE
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

            if (xIsGoodGloballyReciprocalBest(user)) {
                return true;
            }
        }
    }

    return false;
}


bool CProteinMatchApp::xGetGGRBThreshold(const CSeq_annot& seq_annot, double& threshold) const 
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

            if (xGetGGRBThreshold(user, threshold)) {
                return true;
            }
        }
    }

    return false;
}



bool CProteinMatchApp::xGetGGRBThreshold(const CUser_object& user_obj, double& threshold) const
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


CAnnotdesc::TName CProteinMatchApp::xGetComparisonClass(const CSeq_annot& seq_annot) const 
{
    if (seq_annot.IsSetDesc() &&
        seq_annot.GetDesc().IsSet() &&
        seq_annot.GetDesc().Get().size() >= 1 &&
        seq_annot.GetDesc().Get().front()->IsName()) {
        return seq_annot.GetDesc().Get().front()->GetName();
    }    
    return "";
}


bool CProteinMatchApp::xIsProteinMatch(const CSeq_annot& seq_annot) const
{
    return xIsCdsComparison(seq_annot) && xIsGoodGloballyReciprocalBest(seq_annot);
}


string CProteinMatchApp::xGetAccessionVersion(const CSeq_annot& seq_annot) const 
{
    string result = "";

    const CRef<CSeq_feat>& first = seq_annot.GetData().GetFtable().front();
    const CRef<CSeq_feat>& second = seq_annot.GetData().GetFtable().back();

    result = xGetAccessionVersion(*first);

    if (result.empty()) {
        result = xGetAccessionVersion(*second);
    }

    return result;
}



string CProteinMatchApp::xGetAccessionVersion(const CSeq_feat& seq_feat) const 
{
    if (seq_feat.IsSetExts()) {
        ITERATE(CSeq_feat::TExts, it, seq_feat.GetExts()) {
            const CUser_object& usr_obj = **it;

                string acc_ver = xGetAccessionVersion(usr_obj);
                if (!acc_ver.empty()) {
                    return acc_ver;
                }
        }
    }

    return "";
}

string CProteinMatchApp::xGetProductGI(const CSeq_annot& seq_annot) const
{
    string result = "";

    const CRef<CSeq_feat>& first = seq_annot.GetData().GetFtable().front();
    const CRef<CSeq_feat>& second = seq_annot.GetData().GetFtable().back();

    result = xGetProductGI(*first);

    if (result.empty()) {
        result = xGetProductGI(*second);
    }

    return result;
}

string CProteinMatchApp::xGetProductGI(const CSeq_feat& seq_feat) const 
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


string CProteinMatchApp::xGetLocalID(const CSeq_annot& seq_annot) const 
{
    string result = "";

    const CRef<CSeq_feat>& first = seq_annot.GetData().GetFtable().front();
    const CRef<CSeq_feat>& second = seq_annot.GetData().GetFtable().back();

    result = xGetLocalID(*first);

    if (result.empty()) {
        result = xGetLocalID(*second);
    }

    return result;
}


string CProteinMatchApp::xGetLocalID(const CSeq_feat& seq_feat) const 
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


// revisit xGetLocalID
string CProteinMatchApp::xGetAccessionVersion(const CUser_object& user_obj) const 
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


string CProteinMatchApp::xGetLocalID(const CUser_object& user_obj) const 
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

    list<CRef<CSeq_annot> > matches;
    xTryProcessInputFile(args, matches);

    xGenerateMatchTable(matches);


    CNcbiOstream* pOs = xInitOutputStream(args);

    xFormatTabDelimited(*pOs);

    return 0;    
}





bool CProteinMatchApp::xTryProcessInputFile(
        const CArgs& args,
        list<CRef<CSeq_annot> >& matches) 
{
    auto_ptr<CObjectIStream> pObjIstream;
    pObjIstream.reset(xInitInputStream(args));

    while (!pObjIstream->EndOfData()) {
        CRef<CSeq_annot> pSeqAnnot(new CSeq_annot());
        try { 
            pObjIstream->Read(ObjectInfo(*pSeqAnnot));
        } 
        catch (CException&) {
            return false;
        }
        if (xIsProteinMatch(*pSeqAnnot)) {
            matches.push_back(pSeqAnnot);
        }
    }

    return true;
}



void CProteinMatchApp::xAddColumn(const string& colName)
{
    CRef<CSeqTable_column> pColumn(new CSeqTable_column());
    pColumn->SetHeader().SetField_name(colName);
    pColumn->SetHeader().SetTitle(colName);
    pColumn->SetDefault().SetString("");
    mColnameToIndex[colName] = mMatchTable->GetColumns().size();
    mMatchTable->SetColumns().push_back(pColumn);
}



void CProteinMatchApp::xAppendColumnValue(
        const string& colName,
        const string& colVal)
{
    size_t index = mColnameToIndex[colName];
    CSeqTable_column& column = *mMatchTable->SetColumns().at(index);
    column.SetData().SetString().push_back(colVal);
}



bool CProteinMatchApp::xGenerateMatchTable(
        const list<CRef<CSeq_annot> >& matches) 
{
    xAddColumn("LocalID");
    xAddColumn("AccVer");
    xAddColumn("GI");
    xAddColumn("CompClass");

    // Iterate over match Seq-annots
    list<CRef<CSeq_annot> >::const_iterator cit;

    for (cit=matches.begin(); cit != matches.end(); ++cit) {
        string localID = xGetLocalID(**cit);
        string gi = xGetProductGI(**cit);
        string accver = xGetAccessionVersion(**cit);
        string compClass = xGetComparisonClass(**cit);

        xAppendColumnValue("LocalID", localID);
        xAppendColumnValue("AccVer", accver);
        xAppendColumnValue("GI", gi);
        xAppendColumnValue("CompClass", compClass);

        mMatchTable->SetNum_rows(mMatchTable->GetNum_rows()+1);
    }

    return true;
}


void CProteinMatchApp::xFormatTabDelimited(
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


CObjectIStream* CProteinMatchApp::xInitInputStream(const CArgs& args) const
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


CNcbiOstream* CProteinMatchApp::xInitOutputStream(const CArgs& args)
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

