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
 * Authors:  Mike DiCuccio
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
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <algo/align/util/score_builder.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);


/////////////////////////////////////////////////////////////////////////////
//  CDemoApplication::


class CDemoApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CDemoApplication::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddDefaultKey("i", "InputFile",
                            "Seq-align to evaluate",
                            CArgDescriptions::eInputFile,
                            "-");

    arg_desc->AddDefaultKey("o", "OutputFile",
                            "Seq-align report",
                            CArgDescriptions::eOutputFile,
                            "-");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



/////////////////////////////////////////////////////////////////////////////
//  Run test (printout arguments obtained from command-line)


int CDemoApplication::Run(void)
{
    // Get arguments
    const CArgs& args = GetArgs();

    CRef<CObjectManager> om(CObjectManager::GetInstance());
    CGBDataLoader::RegisterInObjectManager(*om);
    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    CSeq_align align;
    args["i"].AsInputFile() >> MSerial_AsnText >> align;

    CNcbiOstream& ostr = args["o"].AsOutputFile();
    CScoreBuilder sb;

    ostr << "alignment length: " << sb.GetAlignLength(align) << endl;
    ostr << "identities: " << sb.GetIdentityCount(*scope, align) << endl;
    ostr << "mismatches: " << sb.GetMismatchCount(*scope, align) << endl;

    {{
         int mismatches;// = 0;
         int identities;// = 0;
         sb.GetMismatchCount(*scope, align, identities, mismatches);
         ostr << "identities + mismatches: "
             << identities << " / " << mismatches << endl;
     }}

    ostr << "tiebreaker: " << sb.ComputeTieBreaker(align) << endl;

    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CDemoApplication::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CDemoApplication().AppMain(argc, argv);
}
