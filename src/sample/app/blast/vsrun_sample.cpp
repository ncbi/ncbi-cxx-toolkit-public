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
 * Author:  Tom Madden
 *
 * File Description:
 *   Demo of using the CVecscreenRun class.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>

// Objects includes
#include <objects/seqloc/Seq_loc.hpp>

// Object manager.
#include <objmgr/object_manager.hpp>

// Object Manager includes
#include <objtools/data_loaders/genbank/gbloader.hpp>

// BLAST includes
#include <algo/blast/format/vecscreen_run.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

/////////////////////////////////////////////////////////////////////////////
//  VSRunSampleApplication::


class VSRunSampleApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

};


/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void VSRunSampleApplication::Init(void)
{
    HideStdArgs(fHideLogfile | fHideConffile | fHideFullVersion | fHideXmlHelp | fHideVersion);

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
      "VSRun demo to run vecscreen");

    // Identifier for the query sequence
    arg_desc->AddKey("gi", "QuerySequenceID", 
                     "GI of the query sequence",
                     CArgDescriptions::eInteger);


    // Output file
    arg_desc->AddDefaultKey("out", "OutputFile", 
        "File name for writing the vecscreen results",
        CArgDescriptions::eOutputFile, "-", CArgDescriptions::fPreOpen);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



/////////////////////////////////////////////////////////////////////////////
//  Run demo


int VSRunSampleApplication::Run(void)
{
    int retval = 0;

        // Get arguments
    const CArgs& args = GetArgs();

    // C++ boiler plate to fetch the query.
    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*object_manager);
    CRef<CScope> scope(new CScope(*object_manager));
    // Add default loaders (GB loader in this demo) to the scope.
    scope->AddDefaults();

    CRef<CSeq_loc> query_loc(new CSeq_loc());
    query_loc->SetWhole().SetGi(GI_FROM(int, args["gi"].AsInteger()));

    CVecscreenRun vs_run(query_loc, scope);

    list<CVecscreenRun::SVecscreenSummary> vs_list = vs_run.GetList();

    list<CVecscreenRun::SVecscreenSummary>::iterator itr = vs_list.begin();

    for ( ; itr != vs_list.end(); ++itr)
    {
        cout << (*itr).seqid->AsFastaString() << " " << (*itr).range.GetFrom() << " " 
          << (*itr).range.GetTo() << " " << (*itr).match_type << endl;
    }

    return retval;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void VSRunSampleApplication::Exit(void)
{
    // Do your after-Run() cleanup here
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


#ifndef SKIP_DOXYGEN_PROCESSING
int NcbiSys_main(int argc, ncbi::TXChar* argv[])
{
    // Execute main application function
    return VSRunSampleApplication().AppMain(argc, argv);
}
#endif /* SKIP_DOXYGEN_PROCESSING */
