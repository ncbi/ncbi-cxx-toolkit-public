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
*   Simple program to test correction of context
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


class CContextApp : public CNcbiApplication 
{
private:
    virtual void Init(void);
    virtual int Run ();
    virtual void Exit(void);
    int CompareVar(CRef<CVariation> v1, CRef<CVariation> v2);
    int CompareVar(CRef<CSeq_annot> v1, CRef<CSeq_annot> v2);
    int CompareLocations(const CSeq_loc& loc1, const CSeq_loc& loc2, int n);
    template<class T>
    void GetAlleles(T &variations, set<string>& alleles, string &ref, int *type = NULL);
    bool IsVariation(AutoPtr<CObjectIStream>& oistr);
    template<class T>
    int CorrectAndCompare(AutoPtr<CObjectIStream>& var_in1, AutoPtr<CObjectIStream>& var_in2, CVariationNormalization::ETargetContext context, CRef<CScope> scope, bool verbose);
    void CheckShiftable(AutoPtr<CObjectIStream>& var_in1, CRef<CScope> scope);
};


void CContextApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),"Correct Context in Variation objects");
    arg_desc->AddDefaultKey("i", "input", "Input file",CArgDescriptions::eInputFile, "-", CArgDescriptions::fPreOpen);
    arg_desc->AddOptionalKey("f", "fixed", "Fixed file",CArgDescriptions::eInputFile,CArgDescriptions::fPreOpen);
    arg_desc->AddFlag("v", "Verbose output",true);
    arg_desc->AddFlag("vcf", "VCF context",true);
    arg_desc->AddFlag("hgvs", "HGVS context",true);
    arg_desc->AddFlag("varloc", "VarLoc context",true);
    arg_desc->AddFlag("c", "Check is shiftable?",true);
    SetupArgDescriptions(arg_desc.release());
}

void CContextApp::Exit(void)
{
}

int CContextApp::CompareLocations(const CSeq_loc& loc1, const CSeq_loc& loc2, int n)
{
    if (loc1.IsPnt() && loc2.IsPnt())
    {
        if (loc1.GetPnt().GetPoint() != loc2.GetPnt().GetPoint())
        {
            ERR_POST(Error << "Position does not match" << Endm);
            n++;
        }
    }
    else if (loc1.IsInt() && loc2.IsInt())
    {
        if (loc1.GetInt().GetFrom() != loc2.GetInt().GetFrom() ||
            loc1.GetInt().GetTo() != loc2.GetInt().GetTo())

        {
            ERR_POST(Error << "Position does not match" << Endm);
            n++;
        }
    }
    else
    {
        ERR_POST(Error << "Type of placement does not match" << Endm);
        n++;
    }
    return n;
}

template<class T>
void  CContextApp::GetAlleles(T &variations, set<string>& alleles, string &ref, int *type)
{
    for (typename T::iterator var2 = variations.begin(); var2 != variations.end(); ++var2)
        if ( (*var2)->IsSetData() && (*var2)->SetData().IsInstance())
        {
            if ((*var2)->SetData().SetInstance().IsSetDelta() && !(*var2)->SetData().SetInstance().SetDelta().empty()
             && (*var2)->SetData().SetInstance().SetDelta().front()->IsSetSeq() && (*var2)->SetData().SetInstance().SetDelta().front()->SetSeq().IsLiteral()
             && (*var2)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().IsSetSeq_data() 
             && (*var2)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().IsIupacna())
            {
                string a = (*var2)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(); 
                
                if ((*var2)->GetData().GetInstance().IsSetObservation() && (int((*var2)->GetData().GetInstance().GetObservation()) & int(CVariation_inst::eObservation_reference)) == int(CVariation_inst::eObservation_reference))
                    ref = a;
                else
                    alleles.insert(a);
            }
            if (type && !((*var2)->GetData().GetInstance().IsSetObservation() && (int((*var2)->GetData().GetInstance().GetObservation()) & int(CVariation_inst::eObservation_reference)) == int(CVariation_inst::eObservation_reference)))
                *type = (*var2)->GetData().GetInstance().GetType();
        }
}

int CContextApp::CompareVar(CRef<CVariation> v1_orig, CRef<CVariation> v2_orig)
{
    int n = 0;
    CRef<CVariation> v1 = v1_orig;
    CRef<CVariation> v2 = v2_orig;
    bool same_level = true;
    if (!v1->IsSetPlacements())
    {
        v1 = v1_orig->SetData().SetSet().SetVariations().front();
        same_level = !same_level;
    }
    
    if (!v2->IsSetPlacements())
    {
        v2 = v2_orig->SetData().SetSet().SetVariations().front();
        same_level = !same_level;
    }

    if (!same_level)
    {
        ERR_POST(Error << "Placement level does not match" << Endm);
        n++;
    }
    
    if (v1->SetPlacements().size() != v2->SetPlacements().size())
    {
        ERR_POST(Error << "Placement size does not match" << Endm);
        n++;
    }
// checking only position on the first placement?
    n = CompareLocations(v1->SetPlacements().front()->GetLoc(),v2->SetPlacements().front()->GetLoc(),n);
   
    string ref1,ref2; 
    // if (v1->SetPlacements().front()->IsSetSeq() && v1->SetPlacements().front()->GetSeq().IsSetSeq_data() && v1->SetPlacements().front()->GetSeq().GetSeq_data().IsIupacna())
    //    ref1 = v1->SetPlacements().front()->SetSeq().SetSeq_data().SetIupacna().Set();
    //if (v2->SetPlacements().front()->IsSetSeq() && v2->SetPlacements().front()->GetSeq().IsSetSeq_data() && v2->SetPlacements().front()->GetSeq().GetSeq_data().IsIupacna())
    //     ref2 = v2->SetPlacements().front()->SetSeq().SetSeq_data().SetIupacna().Set();

    

    if (v1->SetData().SetSet().SetVariations().size() !=
        v2->SetData().SetSet().SetVariations().size() )
    {
        ERR_POST(Error << "Third level variation size does not match" << Endm);
        n++;
    }

    set<string> alleles1, alleles2;
    GetAlleles(v1->SetData().SetSet().SetVariations(), alleles1,ref1);
    GetAlleles(v2->SetData().SetSet().SetVariations(), alleles2,ref2);

    if (ref1 != ref2)
    {
        ERR_POST(Error << "Ref allele in VariantPlacement do not match" << Endm);
        n++;
    }

    if (alleles1.size() != alleles2.size() || !equal(alleles1.begin(), alleles1.end(), alleles2.begin()))
    {
        ERR_POST(Error << "Alt alleles do not match" << Endm);
        n++;
    }
    
    return n;
}

int CContextApp::CompareVar(CRef<CSeq_annot> v1, CRef<CSeq_annot> v2) 
{
    int n = 0;
    CSeq_annot::TData::TFtable::iterator feat1, feat2;
    for ( feat1 = v1->SetData().SetFtable().begin(), feat2 = v2->SetData().SetFtable().begin(); feat1 != v1->SetData().SetFtable().end() && feat2 != v2->SetData().SetFtable().end(); ++feat1,++feat2)
    {
        n = CompareLocations((*feat1)->GetLocation(),(*feat2)->GetLocation(),n);

        CVariation_ref& vr1 = (*feat1)->SetData().SetVariation();
        CVariation_ref& vr2 = (*feat2)->SetData().SetVariation();
        set<string> alleles1,alleles2;
        string ref1,ref2;
        GetAlleles(vr1.SetData().SetSet().SetVariations(),alleles1,ref1);
        GetAlleles(vr2.SetData().SetSet().SetVariations(),alleles2,ref2);
       
        if (ref1 != ref2)
        {
            ERR_POST(Error << "Ref allele in VariantPlacement do not match" << Endm);
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

bool  CContextApp::IsVariation(AutoPtr<CObjectIStream>& oistr)
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
int CContextApp::CorrectAndCompare(AutoPtr<CObjectIStream>& var_in1, AutoPtr<CObjectIStream>& var_in2, CVariationNormalization::ETargetContext context, CRef<CScope> scope, bool verbose)
{
    CRef<T> v1(new T);
    CRef<T> v2(new T);

    *var_in1 >> *v1;      
    if (verbose)
    {
        cerr << endl << "Input Variation" << endl;
        cerr <<  MSerial_AsnText << *v1;
    }
    CVariationNormalization::NormalizeVariation(v1,context,*scope);

    if (verbose)
    {
        cerr << endl << "Output Variation" << endl;
        cerr <<  MSerial_AsnText << *v1;
    }   


    if(!var_in2.get()) {
        cout <<  MSerial_AsnText << *v1;
        return 0;
    }


    *var_in2 >> *v2;
    if (verbose)
    {
        cerr << endl << "Desired Variation" << endl;
        cerr <<  MSerial_AsnText << *v2;
    }   
    int n = CompareVar(v1,v2); 
    return n;
}

void CContextApp::CheckShiftable(AutoPtr<CObjectIStream>& var_in1, CRef<CScope> scope)
{
    CRef<CSeq_annot> v1(new CSeq_annot);
    *var_in1 >> *v1;
    for (CSeq_annot::TData::TFtable::iterator feat1 = v1->SetData().SetFtable().begin(); feat1 != v1->SetData().SetFtable().end(); ++feat1)
    {
        CVariation_ref& vr1 = (*feat1)->SetData().SetVariation();
        set<string> alleles;
        string ref;
        int type;
        GetAlleles(vr1.SetData().SetSet().SetVariations(),alleles,ref,&type);
        if (ref.empty() && alleles.size() == 1)
            ref = *alleles.begin();
        bool r = CVariationNormalization::IsShiftable((*feat1)->GetLocation(),ref, *scope, type);
        if (r)
            cerr << "Yes" << endl;
        else
            cerr << "No" << endl;
    }

}

int CContextApp::Run() 
{

    CArgs args = GetArgs();
    CNcbiIstream& istr = args["i"].AsInputFile();
    bool verbose = args["v"].AsBoolean();
    bool context_vcf = args["vcf"].AsBoolean();
    bool context_hgvs = args["hgvs"].AsBoolean();
    bool context_varloc = args["varloc"].AsBoolean();
    bool check = args["c"].AsBoolean();

    AutoPtr<CNcbiIstream> fstr;
    if(args["f"])
        fstr.reset(new CNcbiIfstream(args["f"].AsString().c_str()));

    

    CVariationNormalization::ETargetContext context = CVariationNormalization::eDbSnp;
    if (context_vcf)
        context = CVariationNormalization::eVCF;
    else if (context_hgvs)
        context = CVariationNormalization::eHGVS;
    else if (context_varloc)
        context = CVariationNormalization::eVarLoc;


    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*object_manager));
    CGBDataLoader::RegisterInObjectManager(*object_manager, NULL, CObjectManager::eDefault, 2);
    scope->AddDefaults();

    AutoPtr<CDecompressIStream>	decomp_stream1(new CDecompressIStream(istr, CCompressStream::eGZipFile, CZipCompression::fAllowTransparentRead));
    AutoPtr<CObjectIStream> var_in1(CObjectIStream::Open(eSerial_AsnText, *decomp_stream1));

    if (check)
    {
        CheckShiftable(var_in1,scope);
        return 0;
    }

    AutoPtr<CDecompressIStream>	decomp_stream2;
    AutoPtr<CObjectIStream> var_in2;
    if( args["f"] ) {
        decomp_stream2.reset(new CDecompressIStream(*fstr, CCompressStream::eGZipFile, CZipCompression::fAllowTransparentRead));
        var_in2.reset(CObjectIStream::Open(eSerial_AsnText, *decomp_stream2));
    }

    bool is_variation = IsVariation(var_in1);

    int n = 0;
    while (!var_in1->EndOfData() )  // && !var_in2->EndOfData())
    {
        if( args["f"] && var_in2->EndOfData() ) 
        {
            ERR_POST(Error << "File size does not match - Fixed file is smaller" << Endm);
        }
        if (is_variation)
            n += CorrectAndCompare<CVariation>(var_in1, var_in2, context, scope,verbose);
        else
            n += CorrectAndCompare<CSeq_annot>(var_in1, var_in2, context, scope,verbose);
    }
    if (verbose)
        cerr << endl << "Number of inconsistencies: " << n << endl;
    
    if( args["f"] && !var_in2->EndOfData() ) 
        ERR_POST(Error << "File size does not match - Fixed file is larger" << Endm);

    return 0;
}

int main(int argc, const char* argv[])
{
    CContextApp ContextApp;
    return ContextApp.AppMain(argc, argv);
}


