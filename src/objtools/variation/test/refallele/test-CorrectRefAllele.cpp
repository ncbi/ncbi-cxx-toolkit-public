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


class CCorrectRefAlleleApp : public CNcbiApplication 
{
private:
    virtual void Init(void);
    virtual int Run ();
    virtual void Exit(void);
    int CompareVar(CRef<CVariation> v1, CRef<CVariation> v2);
    int CompareVar(CRef<CSeq_annot> v1, CRef<CSeq_annot> v2);
    bool IsVariation(AutoPtr<CObjectIStream>& oistr);
    template<class T>
    int CorrectAndCompare(AutoPtr<CObjectIStream>& var_in1, AutoPtr<CObjectIStream>& var_in2, CRef<CScope> scope, bool verbose);
    void GetAlleles(CVariation_ref& vr, string& new_ref, set<string>& alleles);
};


void CCorrectRefAlleleApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),"Correct Ref Allele in Variation objects");
    arg_desc->AddDefaultKey("i", "input", "Input file",CArgDescriptions::eInputFile, "-", CArgDescriptions::fPreOpen);
    arg_desc->AddDefaultKey("f", "fixed", "Fixed file",CArgDescriptions::eInputFile,"fixed",CArgDescriptions::fPreOpen);
    arg_desc->AddFlag("v", "Verbose output",true);
    SetupArgDescriptions(arg_desc.release());
}

void CCorrectRefAlleleApp::Exit(void)
{
}


int CCorrectRefAlleleApp::CompareVar(CRef<CVariation> v1, CRef<CVariation> v2)
{
    int n = 0;
    if (v1->SetData().SetSet().SetVariations().size() !=
        v2->SetData().SetSet().SetVariations().size() )
    {
        ERR_POST(Error << "Second level variation size does not match" << Endm);
        n++;
    }

    if (v1->SetData().SetSet().SetVariations().front()->SetPlacements().size() !=
        v2->SetData().SetSet().SetVariations().front()->SetPlacements().size())
    {
        ERR_POST(Error << "Placement size does not match" << Endm);
        n++;
    }

    //if (v1->SetData().SetSet().SetVariations().front()->SetPlacements().front()->SetSeq().SetSeq_data().SetIupacna().Set() !=
    //   v2->SetData().SetSet().SetVariations().front()->SetPlacements().front()->SetSeq().SetSeq_data().SetIupacna().Set())
   

    CVariation::TData::TSet::TVariations::iterator vi2 = v2->SetData().SetSet().SetVariations().begin();
    for (CVariation::TData::TSet::TVariations::iterator vi1 = v1->SetData().SetSet().SetVariations().begin(); 
         vi1 != v1->SetData().SetSet().SetVariations().end() && vi2 != v2->SetData().SetSet().SetVariations().end();
         ++vi1, ++vi2)
    {
        if ((*vi1)->SetData().SetSet().SetVariations().size() !=
            (*vi2)->SetData().SetSet().SetVariations().size() )
        {
            ERR_POST(Error << "Third level variation size does not match" << Endm);
            n++;
        }

        set<string> alleles1, alleles2;
        string ref1,ref2;
        for (CVariation::TData::TSet::TVariations::iterator var2 = (*vi1)->SetData().SetSet().SetVariations().begin(); var2 != (*vi1)->SetData().SetSet().SetVariations().end(); ++var2)
            if ( (*var2)->IsSetData() && (*var2)->SetData().IsInstance() && (*var2)->SetData().SetInstance().IsSetDelta() && !(*var2)->SetData().SetInstance().SetDelta().empty()
                 && (*var2)->SetData().SetInstance().SetDelta().front()->IsSetSeq() && (*var2)->SetData().SetInstance().SetDelta().front()->SetSeq().IsLiteral()
                 && (*var2)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().IsSetSeq_data() 
                 && (*var2)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().IsIupacna())
            {
                string a = (*var2)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(); 
                if ((*var2)->GetData().GetInstance().IsSetObservation() && (int((*var2)->GetData().GetInstance().GetObservation()) & int(CVariation_inst::eObservation_reference)) == int(CVariation_inst::eObservation_reference))
                    ref1 = a;
                else
                    alleles1.insert(a);                    
            }
        for (CVariation::TData::TSet::TVariations::iterator var2 = (*vi2)->SetData().SetSet().SetVariations().begin(); var2 != (*vi2)->SetData().SetSet().SetVariations().end(); ++var2)
            if ( (*var2)->IsSetData() && (*var2)->SetData().IsInstance() && (*var2)->SetData().SetInstance().IsSetDelta() && !(*var2)->SetData().SetInstance().SetDelta().empty()
                 && (*var2)->SetData().SetInstance().SetDelta().front()->IsSetSeq() && (*var2)->SetData().SetInstance().SetDelta().front()->SetSeq().IsLiteral()
                 && (*var2)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().IsSetSeq_data() 
                 && (*var2)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().IsIupacna())
            {
                string a = (*var2)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(); 
                if ((*var2)->GetData().GetInstance().IsSetObservation() && (int((*var2)->GetData().GetInstance().GetObservation()) & int(CVariation_inst::eObservation_reference)) == int(CVariation_inst::eObservation_reference))
                    ref2 = a;
                else
                    alleles2.insert(a);                    
            }
        if (alleles1.size() != alleles2.size() || !equal(alleles1.begin(), alleles1.end(), alleles2.begin()))
        {
            ERR_POST(Error << "Alt alleles do not match" << Endm);
            n++;
        }
        if (ref1 != ref2)
        {
            ERR_POST(Error << "Reference allele does not match" << Endm);
            n++;
        }
    }
    return n;
}

void CCorrectRefAlleleApp::GetAlleles(CVariation_ref& vr, string& new_ref, set<string>& alleles)
{
    for (CVariation_ref::TData::TSet::TVariations::iterator inst = vr.SetData().SetSet().SetVariations().begin(); inst != vr.SetData().SetSet().SetVariations().end(); ++inst)
    {
        if ((*inst)->GetData().GetInstance().IsSetDelta() && !(*inst)->GetData().GetInstance().GetDelta().empty() && (*inst)->GetData().GetInstance().GetDelta().front()->IsSetSeq() 
            && (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().IsLiteral()
            && (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral().IsSetSeq_data() 
            && (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral().GetSeq_data().IsIupacna())
        {
            string a = (*inst)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set();
            if (((*inst)->GetData().GetInstance().IsSetObservation() && ((*inst)->GetData().GetInstance().GetObservation() & CVariation_inst::eObservation_reference) == CVariation_inst::eObservation_reference))
                new_ref = a;
            else
                alleles.insert(a);
        }
    }
}

int CCorrectRefAlleleApp::CompareVar(CRef<CSeq_annot> v1, CRef<CSeq_annot> v2)
{
    int n = 0;
    CSeq_annot::TData::TFtable::iterator feat1, feat2;
    for ( feat1 = v1->SetData().SetFtable().begin(), feat2 = v2->SetData().SetFtable().begin(); feat1 != v1->SetData().SetFtable().end() && feat2 != v2->SetData().SetFtable().end(); ++feat1,++feat2)
    {
        CVariation_ref& vr1 = (*feat1)->SetData().SetVariation();
        CVariation_ref& vr2 = (*feat2)->SetData().SetVariation();
        string new_ref1,new_ref2;
        set<string> alleles1,alleles2;
        GetAlleles(vr1,new_ref1,alleles1);
        GetAlleles(vr2,new_ref2,alleles2);
        if (new_ref1 != new_ref2)
        {
            ERR_POST(Error << "Reference allele does not match" << Endm);
            n++;
        }
        if (alleles1.size() != alleles2.size() || !equal(alleles1.begin(), alleles1.end(), alleles2.begin()))
        {
            ERR_POST(Error << "Alt alleles do not match" << Endm);
            n++;
        } 
    }
    if (feat1 != v1->SetData().SetFtable().end() || feat2 != v2->SetData().SetFtable().end())
    {
        ERR_POST(Error << "Number of Variations does not match" << Endm);
        n++;
    } 

    return n;
}

bool  CCorrectRefAlleleApp::IsVariation(AutoPtr<CObjectIStream>& oistr)
{
    set<TTypeInfo> matches;
    set<TTypeInfo> candidates;
    //Potential File types
    candidates.insert(CType<CSeq_annot>().GetTypeInfo());
    candidates.insert(CType<CVariation>().GetTypeInfo());

    matches   = oistr->GuessDataType(candidates);
    if(matches.size() != 1) 
    {
        string msg = "Found more than one match for this type of file.  Could not guess data type";
        NCBI_USER_THROW(msg);
    }

    return (*matches.begin())->GetName() == "Variation";
}

template<class T>
int CCorrectRefAlleleApp::CorrectAndCompare(AutoPtr<CObjectIStream>& var_in1, AutoPtr<CObjectIStream>& var_in2, CRef<CScope> scope, bool verbose)
{
    CRef<T> v1(new T);
    CRef<T> v2(new T);

    *var_in1 >> *v1;      
    if (verbose)
    {
        cerr << endl << "Input Variation" << endl;
        cerr <<  MSerial_AsnText << *v1;
    }
    CVariationUtilities::CorrectRefAllele(v1,*scope);
    *var_in2 >> *v2;
    if (verbose)
    {
        cerr << endl << "Desired Variation" << endl;
        cerr <<  MSerial_AsnText << *v2;
    }   
    if (verbose)
    {
        cerr << endl << "Output Variation" << endl;
        cerr <<  MSerial_AsnText << *v1;
    }   
    int n = CompareVar(v1,v2);
    return n;
}

int CCorrectRefAlleleApp::Run() 
{
    CArgs args = GetArgs();
    CNcbiIstream& istr = args["i"].AsInputFile();
    CNcbiIstream& fstr = args["f"].AsInputFile();
    bool verbose = args["v"].AsBoolean();

    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*object_manager));
    CGBDataLoader::RegisterInObjectManager(*object_manager, NULL, CObjectManager::eDefault, 2);
    scope->AddDefaults();

    AutoPtr<CDecompressIStream>	decomp_stream1(new CDecompressIStream(istr, CCompressStream::eGZipFile, CZipCompression::fAllowTransparentRead));
    AutoPtr<CObjectIStream> var_in1(CObjectIStream::Open(eSerial_AsnText, *decomp_stream1));

    AutoPtr<CDecompressIStream>	decomp_stream2(new CDecompressIStream(fstr, CCompressStream::eGZipFile, CZipCompression::fAllowTransparentRead));
    AutoPtr<CObjectIStream> var_in2(CObjectIStream::Open(eSerial_AsnText, *decomp_stream2));

    bool is_variation = IsVariation(var_in1);

    int n = 0;
    while (!var_in1->EndOfData() && !var_in2->EndOfData())
    {
        if (is_variation)
            n += CorrectAndCompare<CVariation>(var_in1, var_in2, scope,verbose);
        else
            n += CorrectAndCompare<CSeq_annot>(var_in1, var_in2, scope,verbose);
    }
    if (verbose)
        cerr << endl << "Number of inconsistencies: " << n << endl;
    if (!var_in1->EndOfData() || !var_in2->EndOfData())
        ERR_POST(Error << "File size does not match" << Endm);

    return 0;
}

int main(int argc, const char* argv[])
{
    CCorrectRefAlleleApp CorrectRefAlleleApp;
    return CorrectRefAlleleApp.AppMain(argc, argv);
}


