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
    void CompareVar(CRef<CVariation> v1, CRef<CVariation> v2);
};


void CCorrectRefAlleleApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),"Correct Ref Allele in Variation objects");
    arg_desc->AddDefaultKey("i", "input", "Input file",CArgDescriptions::eInputFile, "-", CArgDescriptions::fPreOpen);
    arg_desc->AddDefaultKey("f", "fixed", "Fixed file",CArgDescriptions::eInputFile,"fixed",CArgDescriptions::fPreOpen);
    SetupArgDescriptions(arg_desc.release());
}

void CCorrectRefAlleleApp::Exit(void)
{
}


void CCorrectRefAlleleApp::CompareVar(CRef<CVariation> v1, CRef<CVariation> v2)
{
    if (v1->SetData().SetSet().SetVariations().size() !=
        v2->SetData().SetSet().SetVariations().size() )
        ERR_POST(Error << "Second level variation size does not match" << Endm);

    if (v1->SetData().SetSet().SetVariations().front()->SetPlacements().size() !=
        v2->SetData().SetSet().SetVariations().front()->SetPlacements().size())
        ERR_POST(Error << "Placement size does not match" << Endm);

    if (v1->SetData().SetSet().SetVariations().front()->SetPlacements().front()->SetSeq().SetSeq_data().SetIupacna().Set() !=
        v2->SetData().SetSet().SetVariations().front()->SetPlacements().front()->SetSeq().SetSeq_data().SetIupacna().Set())
        ERR_POST(Error << "Reference allele does not match" << Endm);

    CVariation::TData::TSet::TVariations::iterator vi2 = v2->SetData().SetSet().SetVariations().begin();
    for (CVariation::TData::TSet::TVariations::iterator vi1 = v1->SetData().SetSet().SetVariations().begin(); 
         vi1 != v1->SetData().SetSet().SetVariations().end() && vi2 != v2->SetData().SetSet().SetVariations().end();
         ++vi1, ++vi2)
    {
        if ((*vi1)->SetData().SetSet().SetVariations().size() !=
            (*vi2)->SetData().SetSet().SetVariations().size() )
            ERR_POST(Error << "Third level variation size does not match" << Endm);

        set<string> alleles1, alleles2;
        for (CVariation::TData::TSet::TVariations::iterator var2 = (*vi1)->SetData().SetSet().SetVariations().begin(); var2 != (*vi1)->SetData().SetSet().SetVariations().end(); ++var2)
            if ( (*var2)->IsSetData() && (*var2)->SetData().IsInstance() && (*var2)->SetData().SetInstance().IsSetDelta() && !(*var2)->SetData().SetInstance().SetDelta().empty()
                 && (*var2)->SetData().SetInstance().SetDelta().front()->IsSetSeq() && (*var2)->SetData().SetInstance().SetDelta().front()->SetSeq().IsLiteral()
                 && (*var2)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().IsSetSeq_data() 
                 && (*var2)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().IsIupacna())
            {
                string a = (*var2)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(); 
                alleles1.insert(a);
            }
        for (CVariation::TData::TSet::TVariations::iterator var2 = (*vi2)->SetData().SetSet().SetVariations().begin(); var2 != (*vi2)->SetData().SetSet().SetVariations().end(); ++var2)
            if ( (*var2)->IsSetData() && (*var2)->SetData().IsInstance() && (*var2)->SetData().SetInstance().IsSetDelta() && !(*var2)->SetData().SetInstance().SetDelta().empty()
                 && (*var2)->SetData().SetInstance().SetDelta().front()->IsSetSeq() && (*var2)->SetData().SetInstance().SetDelta().front()->SetSeq().IsLiteral()
                 && (*var2)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().IsSetSeq_data() 
                 && (*var2)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().IsIupacna())
            {
                string a = (*var2)->SetData().SetInstance().SetDelta().front()->SetSeq().SetLiteral().SetSeq_data().SetIupacna().Set(); 
                alleles2.insert(a);
            }
        if (!equal(alleles1.begin(), alleles1.end(), alleles2.begin()))
            ERR_POST(Error << "Alt alleles do not match" << Endm);
    }
}

int CCorrectRefAlleleApp::Run() 
{
    CArgs args = GetArgs();
    CNcbiIstream& istr = args["i"].AsInputFile();
    CNcbiIstream& fstr = args["f"].AsInputFile();

    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*object_manager));
    CGBDataLoader::RegisterInObjectManager(*object_manager, NULL, CObjectManager::eDefault, 2);
    scope->AddDefaults();

    AutoPtr<CDecompressIStream>	decomp_stream1(new CDecompressIStream(istr, CCompressStream::eGZipFile, CZipCompression::fAllowTransparentRead));
    AutoPtr<CObjectIStream> var_in1(CObjectIStream::Open(eSerial_AsnText, *decomp_stream1));

    AutoPtr<CDecompressIStream>	decomp_stream2(new CDecompressIStream(fstr, CCompressStream::eGZipFile, CZipCompression::fAllowTransparentRead));
    AutoPtr<CObjectIStream> var_in2(CObjectIStream::Open(eSerial_AsnText, *decomp_stream2));

    while (!var_in1->EndOfData() && !var_in2->EndOfData())
    {
        CRef<CVariation> v1(new CVariation);
        *var_in1 >> *v1;
        CVariationUtilities::CorrectRefAllele(v1,*scope);
        CRef<CVariation> v2(new CVariation);
        *var_in2 >> *v2;
        CompareVar(v1,v2);
    }
    if (!var_in1->EndOfData() || !var_in2->EndOfData())
        ERR_POST(Error << "File size does not match" << Endm);

    return 0;
}

int main(int argc, const char* argv[])
{
    CCorrectRefAlleleApp CorrectRefAlleleApp;
    return CorrectRefAlleleApp.AppMain(argc, argv);
}


