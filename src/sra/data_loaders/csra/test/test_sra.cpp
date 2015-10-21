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
 * Authors:  Denis Vakatov, Vladimir Ivanov
 *
 * File Description:
 *   Sample for the command-line arguments' processing ("ncbiargs.[ch]pp"):
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_vector.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <sra/data_loaders/csra/csraloader.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);


/////////////////////////////////////////////////////////////////////////////
//  CTestSraApplication::


class CTestSraApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CTestSraApplication::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "CArgDescriptions demo program");

    arg_desc->AddDefaultKey("i", "Input",
                            "File of accessions to process",
                            CArgDescriptions::eInputFile,
                            "-");
    arg_desc->AddDefaultKey("o", "OutputFile",
                            "File of data",
                            CArgDescriptions::eOutputFile,
                            "-");
    arg_desc->AddFlag("no-genbank", "Exclude GenBank loader");
    arg_desc->AddFlag("no-csra", "Exclude cSRA loader");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



/////////////////////////////////////////////////////////////////////////////
//  Run test (printout arguments obtained from command-line)


int CTestSraApplication::Run(void)
{
    // Get arguments
    CArgs args = GetArgs();

    CRef<CObjectManager> om(CObjectManager::GetInstance());
    if ( !args["no-csra"] ) {
        CCSRADataLoader::RegisterInObjectManager(*om, CObjectManager::eDefault,
                                                 0);
    }
    if ( !args["no-genbank"] ) {
        CGBDataLoader::RegisterInObjectManager(*om);
    }

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    CNcbiIstream& istr = args["i"].AsInputFile();
    CNcbiOstream& ostr = args["o"].AsOutputFile();

    string line;
    while (NcbiGetlineEOL(istr, line)) {
        NStr::TruncateSpacesInPlace(line);
        if (line.empty()  ||  line[0] == '#') {
            continue;
        }

        CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(line);
        CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
        if ( 1 ) {
            CSeqVector v(bsh, CBioseq_Handle::eCoding_Iupac);
            string data;
            v.GetSeqData(v.begin(), v.end(), data);
            ostr << idh << '\t' << data << '\n';
        }
    }



#if 0
    string sra_acc = args["sra-acc"].AsString();
    CVDBMgr mgr;
    CCSraDb sra_db(mgr, sra_acc);
    CCSraShortReadIterator iter(sra_db);

    size_t count = 0;
    for ( ;  iter  &&  count < 5;  ++iter, ++count) {
        CTempString s = iter.GetReadData();
        ostr << s << endl;
    }
#endif

    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CTestSraApplication::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CTestSraApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
