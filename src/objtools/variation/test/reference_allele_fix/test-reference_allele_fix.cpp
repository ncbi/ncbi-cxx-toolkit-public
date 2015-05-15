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
* Author:  Igor Filippov
*
* File Description:
*   Simple program to test correction of reference allele in Variation object
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <sstream>
#include <iomanip>

#include <cstdlib>
#include <iostream>

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objectio.hpp>
#include <serial/streamiter.hpp>
#include <util/compress/stream_util.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objects/variation/Variation.hpp>
#include <objects/variation/VariantPlacement.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/misc/sequence_macros.hpp>

#include <util/line_reader.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/blastdb/bdbloader.hpp>
#include <connect/services/neticache_client.hpp>
#include <corelib/rwstream.hpp>

#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/seq_loc_util.hpp>

#include <objtools/variation/variation_utils.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
using namespace std;


class CReferenceAlleleFixApp : public CNcbiApplication 
{
private:
    virtual void Init(void);
    virtual int Run ();
    virtual void Exit(void);
    bool IsVariation(AutoPtr<CObjectIStream>& oinput_istream);
    void GetAlleles(CVariation_ref& vr, string& new_ref, set<string>& alleles);
    void CheckFeats(CObjectIStream& input_objstream, CScope& scope);
    void CheckVars( CObjectIStream& input_objstream, CScope& scope);
};


void CReferenceAlleleFixApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),"Correct Ref Allele in Variation objects");
    arg_desc->AddDefaultKey("i", "input", "Input file",CArgDescriptions::eInputFile, "-", CArgDescriptions::fPreOpen);
    SetupArgDescriptions(arg_desc.release());
}

void CReferenceAlleleFixApp::Exit(void)
{
}
    


void CReferenceAlleleFixApp::GetAlleles(CVariation_ref& vr, string& new_ref, set<string>& alleles)
{
    for (CVariation_ref::TData::TSet::TVariations::iterator inst = vr.SetData().SetSet().SetVariations().begin(); inst != vr.SetData().SetSet().SetVariations().end(); ++inst)
    {
        if ((*inst)->GetData().GetInstance().IsSetDelta() && !(*inst)->GetData().GetInstance().GetDelta().empty() && (*inst)->GetData().GetInstance().GetDelta().front()->IsSetSeq() 
            && (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().IsLiteral()
            && (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral().IsSetSeq_data() 
            && (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral().GetSeq_data().IsIupacna())
        {
            string a = (*inst)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set();
            if (
                 (*inst)->GetData().GetInstance().GetType() == CVariation_inst::eType_identity 
                //(*inst)->GetData().GetInstance().IsSetObservation() && ((*inst)->GetData().GetInstance().GetObservation() & CVariation_inst::eObservation_reference) == CVariation_inst::eObservation_reference
                )
                new_ref = a;
            else
                alleles.insert(a);
        }
    }
}

bool  CReferenceAlleleFixApp::IsVariation(AutoPtr<CObjectIStream>& oinput_istream)
{
    set<TTypeInfo> matches;
    set<TTypeInfo> candidates;
    //Potential File types
    candidates.insert(CType<CSeq_annot>().GetTypeInfo());
    candidates.insert(CType<CVariation>().GetTypeInfo());

    matches   = oinput_istream->GuessDataType(candidates);
    if(matches.size() != 1) 
    {
        string msg = "Found more than one match for this type of file.  Could not guess data type";
        NCBI_USER_THROW(msg);
    }

    return (*matches.begin())->GetName() == "Variation";
}

void CReferenceAlleleFixApp::CheckVars( CObjectIStream& input_objstream, 
    CScope& scope)
{
    CRef<CVariation> variation(new CVariation);
    input_objstream >> *variation;

    NON_CONST_ITERATE(CVariation::TData::TSet::TVariations, var_it, variation->SetData().SetSet().SetVariations()) {
        CVariation& var = **var_it;

        //1. Test GetType
        NcbiCout << "Check Variation type: " 
            << CVariationUtilities::GetVariationType(var) << NcbiEndl;

        //2. Get Ref Alt
        string ref;
        vector<string> alts;
        CVariationUtilities::GetVariationRefAlt(var, ref, alts);
        NcbiCout << "Ref/Alt : " << ref << " - " 
            << NStr::Join(alts,",") << NcbiEndl;


    }

    //3. Correct Ref Allele
    CVariationUtilities::CorrectRefAllele(*variation, scope);
    NcbiCout << MSerial_AsnText << *variation;



}

void CReferenceAlleleFixApp::CheckFeats( CObjectIStream& input_objstream, 
    CScope& scope)
{
    CRef<CSeq_annot> annot(new CSeq_annot);
    input_objstream >> *annot;
    NON_CONST_ITERATE(CSeq_annot::TData::TFtable, feat_it, annot->SetData().SetFtable()) {
        CSeq_feat& feat = **feat_it;
        const CVariation_ref& vref = feat.GetData().GetVariation();

        //1. Test IsReferenceCorrect
        //2. TUESDAY: Problem 1 - for insertions, this doesn't make sense
        //    as there is no 'reference', and correct ref is merely the range
        //    of the insertion, if fully or interval shifted.
        string asserted_ref(kEmptyStr), ref_at_location(kEmptyStr);
        bool isCorrect = CVariationUtilities::IsReferenceCorrect(feat,asserted_ref,ref_at_location ,scope);
        NcbiCout << "Is Reference Correct? : " << isCorrect << " . "
            << asserted_ref << " . " << ref_at_location << NcbiEndl;

        //2. Test GetType
        NcbiCout << "Check Variation type: " 
            << CVariationUtilities::GetVariationType(vref) << NcbiEndl;

        //3 Get Ref Alt
        string ref;
        vector<string> alts;
        CVariationUtilities::GetVariationRefAlt(vref, ref, alts);
        NcbiCout << "Ref/Alt : " << ref << " - " 
            << NStr::Join(alts,",") << NcbiEndl;

        //4. Correct Ref Allele - TUES: does this make sense for insertion?
        // it does for a deletion though!
        CVariationUtilities::CorrectRefAllele(feat, scope);
        NcbiCout << MSerial_AsnText << feat;
    }
}

int CReferenceAlleleFixApp::Run() 
{
    const CArgs& args = GetArgs();
    CNcbiIstream& input_istream = args["i"].AsInputFile();

    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*object_manager));
    CGBDataLoader::RegisterInObjectManager(*object_manager, NULL, CObjectManager::eDefault, 2);
    scope->AddDefaults();

    AutoPtr<CObjectIStream> input_objstream(CObjectIStream::Open(eSerial_AsnText, input_istream));
    bool is_variation = IsVariation(input_objstream);

    //Run through tests, dumping information to stdout
    //1. IsCorrectRef -- only used here?
    //2. CorrectRef
    //3. GetType
    //4. GetVarRefAlt
    //5. GetAlleleFromLoc

    if(is_variation)
        CheckVars(*input_objstream, *scope);
    else
        CheckFeats(*input_objstream, *scope);

    return 0;
}

int main(int argc, const char* argv[])
{
    CReferenceAlleleFixApp ReferenceAlleleFixApp;
    return ReferenceAlleleFixApp.AppMain(argc, argv);
}


