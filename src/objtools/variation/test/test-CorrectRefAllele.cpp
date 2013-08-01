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

#include <objtools/variation/CorrectRefAllele.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
using namespace std;


class CCorrectRefAlleleApp : public CNcbiApplication 
{
private:
    virtual void Init(void);
    virtual int Run ();
    virtual void Exit(void);
};


void CCorrectRefAlleleApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),"Correct Ref Allele in Variation objects");
    arg_desc->AddDefaultKey("i", "input", "Input file",CArgDescriptions::eInputFile, "-", CArgDescriptions::fPreOpen);
    arg_desc->AddDefaultKey("o", "output", "Output file",CArgDescriptions::eOutputFile,"-",CArgDescriptions::fPreOpen);
    SetupArgDescriptions(arg_desc.release());
}

void CCorrectRefAlleleApp::Exit(void)
{
}

int CCorrectRefAlleleApp::Run() 
{
    CArgs args = GetArgs();
    CNcbiIstream& istr = args["i"].AsInputFile();
    CNcbiOstream& ostr = args["o"].AsOutputFile();

    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*object_manager));
    CGBDataLoader::RegisterInObjectManager(*object_manager, NULL, CObjectManager::eDefault, 2);
    scope->AddDefaults();

    AutoPtr<CDecompressIStream>	decomp_stream(new CDecompressIStream(istr, CCompressStream::eGZipFile, CZipCompression::fAllowTransparentRead));
    AutoPtr<CObjectIStream> var_in(CObjectIStream::Open(eSerial_AsnText, *decomp_stream));

    while (!var_in->EndOfData())
    {
        CRef<CVariation> v(new CVariation);
        *var_in >> *v;
        CVariationUtilities::CorrectRefAllele(v,*scope);
        ostr <<MSerial_AsnText<< *v;
    }
    return 0;
}

int main(int argc, const char* argv[])
{
    CCorrectRefAlleleApp CorrectRefAlleleApp;
    return CorrectRefAlleleApp.AppMain(argc, argv);
}


