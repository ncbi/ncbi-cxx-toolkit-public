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
 * Authors:  Christiam Camacho
 *
 */

/** @file vecscreen_app.cpp
 * VecScreen command line application
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
	"$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <algo/blast/format/vecscreen_run.hpp>
#include <algo/blast/blastinput/blast_fasta_input.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/blast/api/remote_blast.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include "../blast/blast_app_util.hpp"

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
#endif

class CVecScreenApp : public CNcbiApplication
{
public:
    /** @inheritDoc */
    CVecScreenApp() {
        CRef<CVersion> version(new CVersion());
        version->SetVersionInfo(new CVecScreenVersion());
        SetFullVersion(version);
    }
private:
    /** @inheritDoc */
    virtual void Init();
    /** @inheritDoc */
    virtual int Run();

    /// This application's command line args
};

void CVecScreenApp::Init()
{
    HideStdArgs(fHideLogfile | fHideConffile | fHideFullVersion | fHideXmlHelp | fHideDryRun);
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Vector screening tool, version " +
                              CVecScreenVersion().Print());
    arg_desc->SetCurrentGroup("Input query options");
    arg_desc->AddDefaultKey(kArgQuery, "input_file", 
                     "Input file name",
                     CArgDescriptions::eInputFile, kDfltArgQuery);

    arg_desc->SetCurrentGroup("BLAST database options");
    arg_desc->AddDefaultKey(kArgDb, "dbname", "BLAST database name", 
                            CArgDescriptions::eString, kDefaultVectorDb);

    arg_desc->SetCurrentGroup("Output configuration options");
    arg_desc->AddDefaultKey(kArgOutput, "output_file", "Output file name", 
                            CArgDescriptions::eOutputFile, "-");

    //arg_desc->AddDefaultKey("outfmt", "format",
    arg_desc->AddDefaultKey(kArgOutputFormat, "format",
            "VecScreen results options:\n"
            "  0 = Show alignments pairwise,\n"
            "  1 = Do not show alignments, just contaminated range offsets\n",
            CArgDescriptions::eInteger, 
            NStr::IntToString(kDfltArgOutputFormat));
    arg_desc->SetConstraint(kArgOutputFormat, 
       new CArgAllowValuesBetween(0, CVecscreenRun::CFormatter::eEndValue-1, true));
    // Produce Text output?
    arg_desc->AddFlag("text_output", "Produce text output?", true);

    arg_desc->SetCurrentGroup("");
    SetupArgDescriptions(arg_desc.release());
}

int CVecScreenApp::Run(void)
{
    int status = BLAST_EXIT_SUCCESS;

    try {

        // Allow the fasta reader to complain on invalid sequence input
        SetDiagPostLevel(eDiag_Warning);

        const bool kIsProtein(false);
        /*** Process the command line arguments ***/
        const CArgs& args = GetArgs();
        const string kDbName(args[kArgDb].AsString());
        CRef<CBlastOptionsHandle> opts_hndl(CBlastOptionsFactory::Create(eVecScreen));

        /*** Initialize the scope ***/
        SDataLoaderConfig dlconfig(kDbName, kIsProtein);
        dlconfig.OptimizeForWholeLargeSequenceRetrieval();
        CBlastInputSourceConfig iconfig(dlconfig);
        iconfig.SetQueryLocalIdMode();
        CRef<CScope> scope = CBlastScopeSource(dlconfig).NewScope();

        /*** Initialize the input stream ***/
        CBlastFastaInputSource fasta(args[kArgQuery].AsInputFile(), iconfig);
        CBlastInput input(&fasta, 1);

        /*** Get the formatting options ***/
        const CVecscreenRun::CFormatter::TOutputFormat kFmt = 
            args[kArgOutputFormat].AsInteger();
        const bool kHtmlOutput = !args["text_output"].AsBoolean();
        
        /*** Process the input ***/
        while ( !input.End() ) {

            CRef<CBlastQueryVector> query_batch(input.GetNextSeqBatch(*scope));
            _ASSERT(query_batch->Size() == 1);
            CRef<IQueryFactory> queries(new CObjMgr_QueryFactory(*query_batch));
            CVecscreenRun vs(CRef<CSeq_loc>(const_cast<CSeq_loc*>(&*query_batch->GetQuerySeqLoc(0))),
                             query_batch->GetScope(0), kDbName);
            CVecscreenRun::CFormatter vs_format(vs, *scope, kFmt, kHtmlOutput);
            vs_format.FormatResults(args[kArgOutput].AsOutputFile(), opts_hndl);
        }

    } CATCH_ALL(status)
    return status;
}

#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
    return CVecScreenApp().AppMain(argc, argv, 0, eDS_Default, "");
}
#endif /* SKIP_DOXYGEN_PROCESSING */
