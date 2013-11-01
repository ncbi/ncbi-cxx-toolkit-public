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
 * Authors:  Jason Papadopoulos
 *
 */

/** @file rpsblast_app.cpp
 * RPSBLAST command line application
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
	"$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/api/remote_blast.hpp>
#include <algo/blast/blastinput/blast_fasta_input.hpp>
#include <algo/blast/blastinput/rpsblast_args.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/blast/format/blast_format.hpp>
#include "blast_app_util.hpp"
#include <algo/blast/api/rpsblast_local.hpp>
#include <algo/blast/api/rps_aux.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
#endif

class CRPSBlastApp : public CNcbiApplication
{
public:
    /** @inheritDoc */
    CRPSBlastApp() {
        CRef<CVersion> version(new CVersion());
        version->SetVersionInfo(new CBlastVersion());
        SetFullVersion(version);
    }
private:
    /** @inheritDoc */
    virtual void Init();
    /** @inheritDoc */
    virtual int Run();

    /// This application's command line args
    CRef<CRPSBlastAppArgs> m_CmdLineArgs;
};

void CRPSBlastApp::Init()
{
    // formulate command line arguments

    m_CmdLineArgs.Reset(new CRPSBlastAppArgs());

    // read the command line

    HideStdArgs(fHideLogfile | fHideConffile | fHideFullVersion | fHideXmlHelp | fHideDryRun);
    SetupArgDescriptions(m_CmdLineArgs->SetCommandLine());
}

int CRPSBlastApp::Run(void)
{
    int status = BLAST_EXIT_SUCCESS;

    try {

        // Allow the fasta reader to complain on invalid sequence input
        SetDiagPostLevel(eDiag_Warning);

        /*** Get the BLAST options ***/
        const CArgs& args = GetArgs();
        CRef<CBlastOptionsHandle> opts_hndl;
        if(RecoverSearchStrategy(args, m_CmdLineArgs)) {
        	opts_hndl.Reset(&*m_CmdLineArgs->SetOptionsForSavedStrategy(args));
        }
        else {
        	opts_hndl.Reset(&*m_CmdLineArgs->SetOptions(args));
        }

        CheckForFreqRatioFile(m_CmdLineArgs->GetBlastDatabaseArgs()->GetDatabaseName(),
        		               opts_hndl, true);
        const CBlastOptions& opt = opts_hndl->GetOptions();

        /*** Initialize the database ***/
        CRef<CBlastDatabaseArgs> db_args(m_CmdLineArgs->GetBlastDatabaseArgs());
        CRef<CLocalDbAdapter> db_adapter;
        CRef<CScope> scope;
        InitializeSubject(db_args, opts_hndl, m_CmdLineArgs->ExecuteRemotely(),
                         db_adapter, scope);
        _ASSERT(db_adapter && scope);

        /*** Get the query sequence(s) ***/
        CRef<CQueryOptionsArgs> query_opts = 
            m_CmdLineArgs->GetQueryOptionsArgs();
        SDataLoaderConfig dlconfig =
            InitializeQueryDataLoaderConfiguration(query_opts->QueryIsProtein(),
                                                   db_adapter);
        CBlastInputSourceConfig iconfig(dlconfig, query_opts->GetStrand(),
                                     query_opts->UseLowercaseMasks(),
                                     query_opts->GetParseDeflines(),
                                     query_opts->GetRange());
        iconfig.SetQueryLocalIdMode();
        if(IsIStreamEmpty(m_CmdLineArgs->GetInputStream()))
           	NCBI_THROW(CArgException, eNoValue, "Query is Empty!");
        CBlastFastaInputSource fasta(m_CmdLineArgs->GetInputStream(), iconfig);
        CBlastInput input(&fasta, m_CmdLineArgs->GetQueryBatchSize());

        /*** Get the formatting options ***/
        CRef<CFormattingArgs> fmt_args(m_CmdLineArgs->GetFormattingArgs());
        CBlastFormat formatter(opt, *db_adapter,
                               fmt_args->GetFormattedOutputChoice(),
                               query_opts->GetParseDeflines(),
                               m_CmdLineArgs->GetOutputStream(),
                               fmt_args->GetNumDescriptions(),
                               fmt_args->GetNumAlignments(),
                               *scope,
                               opt.GetMatrixName(),
                               fmt_args->ShowGis(),
                               fmt_args->DisplayHtmlOutput(),
                               opt.GetQueryGeneticCode(),
                               opt.GetDbGeneticCode(),
                               opt.GetSumStatisticsMode(),
                               m_CmdLineArgs->ExecuteRemotely(),
                               db_adapter->GetFilteringAlgorithm(),
                               fmt_args->GetCustomOutputFormatSpec());

        formatter.SetQueryRange(query_opts->GetRange());
        formatter.PrintProlog();

        /*** Process the input ***/
        for (; !input.End(); formatter.ResetScopeHistory()) {

            CRef<CBlastQueryVector> query_batch(input.GetNextSeqBatch(*scope));
            CRef<IQueryFactory> queries(new CObjMgr_QueryFactory(*query_batch));

            SaveSearchStrategy(args, m_CmdLineArgs, queries, opts_hndl);

            CRef<CSearchResultSet> results;

            if (m_CmdLineArgs->ExecuteRemotely())
            {
                CRef<CRemoteBlast> rmt_blast = 
                    InitializeRemoteBlast(queries, db_args, opts_hndl,
                          m_CmdLineArgs->ProduceDebugRemoteOutput(),
                          m_CmdLineArgs->GetClientId());
                results = rmt_blast->GetResultSet();
            }
            else
            {
            	CLocalRPSBlast  local_search (query_batch, db_args->GetDatabaseName(), opts_hndl, args[kArgNumThreads].AsInteger() );
            	results = local_search.Run();
            }

            if (fmt_args->ArchiveFormatRequested(args)) {
                formatter.WriteArchive(*queries, *opts_hndl, *results);
            } else {
               BlastFormatter_PreFetchSequenceData(*results, scope);
                ITERATE(CSearchResultSet, result, *results) {
                    formatter.PrintOneResultSet(**result, query_batch);
                }
            }
        }

        formatter.PrintEpilog(opt);

        if (m_CmdLineArgs->ProduceDebugOutput()) {
            opts_hndl->GetOptions().DebugDumpText(NcbiCerr, "BLAST options", 1);
        }

    } CATCH_ALL(status)
    return status;
}

#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
	int status = CRPSBlastApp().AppMain(argc, argv, 0, eDS_Default, 0);

	return status;
}
#endif /* SKIP_DOXYGEN_PROCESSING */
