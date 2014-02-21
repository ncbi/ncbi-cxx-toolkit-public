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

#include <misc/xmlwrapp/xmlwrapp.hpp>

// standard includes
#include <string>
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
    int CompareVar(CRef<CVariation> v1, CRef<CVariation> v2, string &content, string &description);
    int CompareVar(CRef<CSeq_annot> v1, CRef<CSeq_annot> v2, string &content, string &description);
    int CompareLocations(const CSeq_loc& loc1, const CSeq_loc& loc2, int n, string &content);
    template<class T>
    void GetAlleles(T &variations, set<string>& alleles, string &ref);
    bool IsVariation(AutoPtr<CObjectIStream>& oistr, int &n, string &content);
    template<class T>
    int CorrectAndCompare(AutoPtr<CObjectIStream>& var_in1, AutoPtr<CObjectIStream>& var_in2, CVariationNormalization::ETargetContext context, CRef<CScope> scope, string &content, string &description);
};


void CContextApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),"Test Context in Variation objects");
    arg_desc->AddDefaultKey("i", "input", "Folder with input files", CArgDescriptions::eString, "annot/input");
    arg_desc->AddDefaultKey("t", "test", "Folder with test files", CArgDescriptions::eString, "annot/dbsnp");
    arg_desc->AddDefaultKey("o", "output", "Output file prefix",CArgDescriptions::eOutputFile,"-",CArgDescriptions::fPreOpen);
    arg_desc->AddFlag("vcf", "VCF context",true);
    arg_desc->AddFlag("hgvs", "HGVS context",true);
    arg_desc->AddFlag("varloc", "VarLoc context",true);
    SetupArgDescriptions(arg_desc.release());
}

void CContextApp::Exit(void)
{
}

int CContextApp::CompareLocations(const CSeq_loc& loc1, const CSeq_loc& loc2, int n, string &content)
{
    if (loc1.IsPnt() && loc2.IsPnt())
    {
        if (loc1.GetPnt().GetPoint() != loc2.GetPnt().GetPoint())
        {
            content += "Position does not match\n";
            n++;
        }
    }
    else if (loc1.IsInt() && loc2.IsInt())
    {
        if (loc1.GetInt().GetFrom() != loc2.GetInt().GetFrom() ||
            loc1.GetInt().GetTo() != loc2.GetInt().GetTo())

        {
            content += "Position does not match\n";
            n++;
        }
    }
    else
    {
        content += "Type of placement does not match\n";
        n++;
    }
    return n;
}

template<class T>
void  CContextApp::GetAlleles(T &variations, set<string>& alleles, string &ref)
{
    for (typename T::iterator var2 = variations.begin(); var2 != variations.end(); ++var2)
        if ( (*var2)->IsSetData() && (*var2)->SetData().IsInstance() && (*var2)->SetData().SetInstance().IsSetDelta() && !(*var2)->SetData().SetInstance().SetDelta().empty()
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
}

int CContextApp::CompareVar(CRef<CVariation> v1_orig, CRef<CVariation> v2_orig, string &content, string & description) 
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
        content += "Placement size does not match\n";
        n++;
    }
// checking only position on the first placement?
    n = CompareLocations(v1->SetPlacements().front()->GetLoc(),v2->SetPlacements().front()->GetLoc(),n, content);
   
    string ref1,ref2; 
    //if (v1->SetPlacements().front()->IsSetSeq() && v1->SetPlacements().front()->GetSeq().IsSetSeq_data() && v1->SetPlacements().front()->GetSeq().GetSeq_data().IsIupacna())
    //   ref1 = v1->SetPlacements().front()->SetSeq().SetSeq_data().SetIupacna().Set();
    //if (v2->SetPlacements().front()->IsSetSeq() && v2->SetPlacements().front()->GetSeq().IsSetSeq_data() && v2->SetPlacements().front()->GetSeq().GetSeq_data().IsIupacna())
    //   ref2 = v2->SetPlacements().front()->SetSeq().SetSeq_data().SetIupacna().Set();

 

    if (v1->SetData().SetSet().SetVariations().size() !=
        v2->SetData().SetSet().SetVariations().size() )
    {
        content += "Third level variation size does not match\n";
        n++;
    }

    set<string> alleles1, alleles2;
    GetAlleles(v1->SetData().SetSet().SetVariations(), alleles1, ref1);
    GetAlleles(v2->SetData().SetSet().SetVariations(), alleles2, ref2);
 
    if (ref1 != ref2)
    {
        content += "Ref allele in VariantPlacement do not match\n";
        n++;
    }
   
    if (alleles1.size() != alleles2.size() || !equal(alleles1.begin(), alleles1.end(), alleles2.begin()))
    {
        content += "Alt alleles do not match\n";
        n++;
    }
    
    string description1;
    if (v1->IsSetExt())
        for (CVariation::TExt::const_iterator ext = v1->GetExt().begin(); ext != v1->GetExt().end(); ++ext)
            if ((*ext)->IsSetType() && (*ext)->GetType().IsStr() && (*ext)->GetType().GetStr() == "Description" && (*ext)->IsSetData())
                for (CUser_object::TData::const_iterator field = (*ext)->GetData().begin(); field != (*ext)->GetData().end(); ++field)
                    if ((*field)->IsSetData() && (*field)->GetData().IsStr())
                    {
                        description1 += (*field)->GetData().GetStr();
                    }
    if (description1.empty())
        description1 = "No input description provided";

    string description2;
    if (v2->IsSetExt())
        for (CVariation::TExt::const_iterator ext = v2->GetExt().begin(); ext != v2->GetExt().end(); ++ext)
            if ((*ext)->IsSetType() && (*ext)->GetType().IsStr() && (*ext)->GetType().GetStr() == "Description" && (*ext)->IsSetData())
                for (CUser_object::TData::const_iterator field = (*ext)->GetData().begin(); field != (*ext)->GetData().end(); ++field)
                    if ((*field)->IsSetData() && (*field)->GetData().IsStr())
                    {
                        description2 += (*field)->GetData().GetStr();
                    }
    if (description2.empty())
        description2 = "No output description provided";

    description = description1 + " : " + description2;

    return n;
}

int CContextApp::CompareVar(CRef<CSeq_annot> v1, CRef<CSeq_annot> v2, string &content, string &description) 
{
    int n = 0;
    CSeq_annot::TData::TFtable::iterator feat1, feat2;
    for ( feat1 = v1->SetData().SetFtable().begin(), feat2 = v2->SetData().SetFtable().begin(); feat1 != v1->SetData().SetFtable().end() && feat2 != v2->SetData().SetFtable().end(); ++feat1,++feat2)
    {
        n = CompareLocations((*feat1)->GetLocation(),(*feat2)->GetLocation(),n, content);

        CVariation_ref& vr1 = (*feat1)->SetData().SetVariation();
        CVariation_ref& vr2 = (*feat2)->SetData().SetVariation();
        set<string> alleles1,alleles2;
        string ref1,ref2;
        GetAlleles(vr1.SetData().SetSet().SetVariations(),alleles1,ref1);
        GetAlleles(vr2.SetData().SetSet().SetVariations(),alleles2,ref2);
       
        if (ref1 != ref2)
        {
            content += "Ref allele in VariantPlacement do not match\n";
            n++;
        }
   
        if (alleles1.size() != alleles2.size() || !equal(alleles1.begin(), alleles1.end(), alleles2.begin()))
        {
            content += "Alt alleles do not match\n";
            n++;
        } 

        string description1;
        if ((*feat1)->IsSetExts())
            for (CSeq_feat::TExts::const_iterator ext = (*feat1)->GetExts().begin(); ext != (*feat1)->GetExts().end(); ++ext)
                if ((*ext)->IsSetType() && (*ext)->GetType().IsStr() && (*ext)->GetType().GetStr() == "Description" && (*ext)->IsSetData())
                    for (CUser_object::TData::const_iterator field = (*ext)->GetData().begin(); field != (*ext)->GetData().end(); ++field)
                        if ((*field)->IsSetData() && (*field)->GetData().IsStr())
                        {
                            description1 += (*field)->GetData().GetStr();
                        }
        if (description1.empty())
            description1 = "No input description provided";

        string description2;
        if ((*feat2)->IsSetExts())
            for (CSeq_feat::TExts::const_iterator ext = (*feat2)->GetExts().begin(); ext != (*feat2)->GetExts().end(); ++ext)
                if ((*ext)->IsSetType() && (*ext)->GetType().IsStr() && (*ext)->GetType().GetStr() == "Description" && (*ext)->IsSetData())
                    for (CUser_object::TData::const_iterator field = (*ext)->GetData().begin(); field != (*ext)->GetData().end(); ++field)
                        if ((*field)->IsSetData() && (*field)->GetData().IsStr())
                        {
                            description2 += (*field)->GetData().GetStr();
                        }
        if (description2.empty())
            description2 = "No output description provided";

        description = description1 + " : " + description2;
    }
    if (feat1 != v1->SetData().SetFtable().end() || feat2 != v2->SetData().SetFtable().end())
    {
        content += "Number of Variations does not match\n";
        n++;
    } 
    
    return n;
}

bool  CContextApp::IsVariation(AutoPtr<CObjectIStream>& oistr, int &n, string &content)
{
    set<TTypeInfo> matches;
    set<TTypeInfo> candidates;
    //Potential File types
    candidates.insert(CType<CSeq_annot>().GetTypeInfo());
    candidates.insert(CType<CVariation>().GetTypeInfo());

    matches   = oistr->GuessDataType(candidates);
    if(matches.size() != 1) 
    {
        n++;
        content += "Found more than one match for this type of file.  Could not guess data type";
        return false;
    }

    return (*matches.begin())->GetName() == "Variation";
}

template<class T>
int CContextApp::CorrectAndCompare(AutoPtr<CObjectIStream>& var_in1, AutoPtr<CObjectIStream>& var_in2, CVariationNormalization::ETargetContext context, CRef<CScope> scope, 
                                   string &content, string &description)
{
    CRef<T> v1(new T);
    CRef<T> v2(new T);

    *var_in1 >> *v1;      
    stringstream strstr;
    strstr << "Input Variation"<<endl;;
    strstr <<  MSerial_AsnText << *v1;

    CVariationNormalization::NormalizeVariation(*v1,context,*scope);

    strstr<< endl << "Output Variation" << endl;
    strstr <<  MSerial_AsnText << *v1;
        
    *var_in2 >> *v2;

    strstr<< endl << "Desired Variation" << endl;
    strstr <<  MSerial_AsnText << *v2;

    content += strstr.str();
       
    int n = CompareVar(v1,v2, content, description); 
    return n;
}

int CContextApp::Run() 
{

    CArgs args = GetArgs();
    string input = args["i"].AsString();
    string test = args["t"].AsString();
    CNcbiOstream& ostr = args["o"].AsOutputFile();
    bool context_vcf = args["vcf"].AsBoolean();
    bool context_hgvs = args["hgvs"].AsBoolean();
    bool context_varloc = args["varloc"].AsBoolean();

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

    xml::document xmldoc("testsuite");
    xml::node &root = xmldoc.get_root_node();
    int num_tests = 0;
    int num_failures = 0;

    ncbi::CDir dir(input);
    ncbi::CDir::TEntries entries = dir.GetEntries();
    for (ncbi::CDir::TEntries::iterator i=entries.begin(); i != entries.end(); ++i)
        if ((*i)->IsFile())
        {
            string name = (*i)->GetName();
            string path1 = (*i)->GetPath();
            string path2 = (*i)->MakePath(test,name);
            CFile file2(path2);
            if (file2.Exists())
            {
                num_tests++;
                xml::node::iterator it = root.insert(root.begin(), xml::node("testcase"));
                if (context_vcf)
                    it->get_attributes().insert("classname", "VCF");
                else if (context_hgvs)
                    it->get_attributes().insert("classname", "HGVS");
                else if (context_varloc)
                    it->get_attributes().insert("classname", "VarLoc");
                else
                    it->get_attributes().insert("classname", "DbSnp");            

                AutoPtr<CNcbiIstream> istr(new CNcbiIfstream(path1.c_str()));
                AutoPtr<CNcbiIstream> fstr(new CNcbiIfstream(path2.c_str()));
                AutoPtr<CDecompressIStream>	decomp_stream1(new CDecompressIStream(*istr, CCompressStream::eGZipFile, CZipCompression::fAllowTransparentRead));
                AutoPtr<CObjectIStream> var_in1(CObjectIStream::Open(eSerial_AsnText, *decomp_stream1));

                AutoPtr<CDecompressIStream>	decomp_stream2;
                AutoPtr<CObjectIStream> var_in2;
                decomp_stream2.reset(new CDecompressIStream(*fstr, CCompressStream::eGZipFile, CZipCompression::fAllowTransparentRead));
                var_in2.reset(CObjectIStream::Open(eSerial_AsnText, *decomp_stream2));
    
                int n = 0;
                string content;
                string description = file2.GetName()+" - ";

                bool is_variation = IsVariation(var_in1,n,content);
                if (n == 0)
                {
                    if (is_variation)
                        it->get_attributes().insert("type", "Variation");
                    else
                        it->get_attributes().insert("type", "Seq-annot");
                    
                    while (!var_in1->EndOfData() && !var_in2->EndOfData())
                    {
                        string var_description;
                        if (is_variation)
                            n += CorrectAndCompare<CVariation>(var_in1, var_in2, context, scope, content, var_description);
                        else
                            n += CorrectAndCompare<CSeq_annot>(var_in1, var_in2, context, scope, content, var_description);
                        description += var_description;
                    }
                }
                it->get_attributes().insert("name", description.c_str());

                if (n!=0 || !var_in1->EndOfData() || !var_in2->EndOfData())
                {
                    num_failures++;
                   it = it->insert(xml::node("failure"));
                   stringstream msg;
                   msg << "Number of inconsistencies: " << n;
                   it->get_attributes().insert("message", msg.str().c_str());

                   if (!var_in1->EndOfData())
                       content += "File size does not match - the input is larger\n";
                   if (!var_in2->EndOfData())
                       content += "File size does not match - the input is smaller\n";
                   it->set_content(content.c_str());
                }
            }
        }
    stringstream str_num_tests; 
    str_num_tests << num_tests;
    stringstream str_num_failures; 
    str_num_failures << num_failures;

    root.get_attributes().insert("failures",str_num_failures.str().c_str());
    root.get_attributes().insert("tests",str_num_tests.str().c_str());

    xmldoc.set_is_standalone(true);
    xmldoc.set_encoding("ISO-8859-1");
    ostr << xmldoc;
    return 0;
}

int main(int argc, const char* argv[])
{
    CContextApp ContextApp;
    return ContextApp.AppMain(argc, argv);
}

