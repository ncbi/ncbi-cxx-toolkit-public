#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbienv.hpp> // ??
#include <corelib/ncbistre.hpp> // ??
#include <corelib/ncbiexpt.hpp>
#include <serial/objistr.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_field.hpp>


BEGIN_NCBI_SCOPE


class CProteinMatchException : public CException {
public:
    enum EErrCode {
        eInputError
    };

    virtual const char* GetErrCodeString() const 
    {
        switch(GetErrCode()) {
            case eInputError:
                return "eInputError";
            default:
                return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CProteinMatchException, CException);
};


USING_SCOPE(objects);

class CProteinMatchApp : public CNcbiApplication
{

public:
    void Init();
    int Run();

private:
    bool xTryProcessInputFile(const CArgs& args, list<CRef<CSeq_annot> >& matches);
    CObjectIStream* xInitInputStream(const CArgs& args) const;
    bool xIsCdsComparison(const CSeq_annot& seq_annot) const;
    bool xIsGoodGloballyReciprocalBest(const CSeq_annot& seq_annot) const;
    bool xIsGoodGloballyReciprocalBest(const CUser_object& user_obj) const;
    bool xIsProteinMatch(const CSeq_annot& seq_annot) const;
    CAnnotdesc::TName xGetComparisonClass(const CSeq_annot& seq_annot) const;
    bool xGetGGRBThreshold(const CSeq_annot& annot, double& threshold) const;
    bool xGetGGRBThreshold(const CUser_object& user_object, double& threshold) const;
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

    return;
}


int CProteinMatchApp::Run()
{
    const CArgs& args = GetArgs();

    list<CRef<CSeq_annot> > matches;
    xTryProcessInputFile(args, matches);

    return 0;    
}


bool CProteinMatchApp::xTryProcessInputFile(
        const CArgs& args,
        list<CRef<CSeq_annot> >& matches) 
{
    auto_ptr<CObjectIStream> pObjIstream;
    pObjIstream.reset(xInitInputStream(args));

    ESerialDataFormat input_format = pObjIstream->GetDataFormat();


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



END_NCBI_SCOPE
USING_NCBI_SCOPE;


int main(int argc, const char* argv[])
{
    return CProteinMatchApp().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}

