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

/** @file blastn_app.cpp
 * BLASTN command line application
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/api/remote_blast.hpp>
#include <algo/blast/blastinput/blast_fasta_input.hpp>
#include <algo/blast/blastinput/blastn_args.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/blast/format/blast_format.hpp>
#include <util/profile/rtprofile.hpp>
#include "blast_app_util.hpp"
#include "blastn_node.hpp"

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
#endif

class CBlastnApp : public CNcbiApplication
{
public:
    /** @inheritDoc */
    CBlastnApp() {
        CRef<CVersion> version(new CVersion());
        version->SetVersionInfo(new CBlastVersion());
        SetFullVersion(version);
        m_StopWatch.Start();
        if (m_UsageReport.IsEnabled()) {
        	m_UsageReport.AddParam(CBlastUsageReport::eVersion, GetVersion().Print());
        }
    }

    ~CBlastnApp() {
    	m_UsageReport.AddParam(CBlastUsageReport::eRunTime, m_StopWatch.Elapsed());
    }
private:
    /** @inheritDoc */
    virtual void Init();
    /** @inheritDoc */
    virtual int Run();

    int x_RunMTBySplitDB();
    int x_RunMTBySplitQuery();

    /// This application's command line args
    CRef<CBlastnAppArgs> m_CmdLineArgs; 
    CBlastUsageReport m_UsageReport;
    CStopWatch m_StopWatch;
};

void CBlastnApp::Init()
{
    // formulate command line arguments

    m_CmdLineArgs.Reset(new CBlastnAppArgs());
    // read the command line
    HideStdArgs(fHideLogfile | fHideConffile | fHideFullVersion | fHideXmlHelp | fHideDryRun);
    SetupArgDescriptions(m_CmdLineArgs->SetCommandLine());
}

int CBlastnApp::Run(void)
{
	const CArgs& args = GetArgs();
	if (args.Exist(kArgMTMode) &&
		(args[kArgMTMode].AsInteger() == CMTArgs::eSplitByQueries) &&
		(args[kArgNumThreads].AsInteger() > 1)){
		return x_RunMTBySplitQuery();
	}
	else {
		return x_RunMTBySplitDB();
	}
}

int CBlastnApp::x_RunMTBySplitDB()
{
    BLAST_PROF_START( APP.MAIN );
    BLAST_PROF_START( APP.PRE );
    BLAST_PROF_ADD2( PROGRAM, blastn ) ;
    int status = BLAST_EXIT_SUCCESS;
    CBlastAppDiagHandler bah;
    int batch_num = 0;

    try {

        // Allow the fasta reader to complain on invalid sequence input
        SetDiagPostLevel(eDiag_Warning);
        SetDiagPostPrefix("blastn");
        SetDiagHandler(&bah, false);

        /*** Get the BLAST options ***/
        const CArgs& args = GetArgs();

        CRef<CBlastOptionsHandle> opts_hndl;
        if(RecoverSearchStrategy(args, m_CmdLineArgs)){
        	opts_hndl.Reset(&*m_CmdLineArgs->SetOptionsForSavedStrategy(args));
        }
        else {
        	opts_hndl.Reset(&*m_CmdLineArgs->SetOptions(args));
        }
        const CBlastOptions& opt = opts_hndl->GetOptions();

        /*** Initialize the database/subject ***/
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
        if(IsIStreamEmpty(m_CmdLineArgs->GetInputStream())) {
           	ERR_POST(Warning << "Query is Empty!");
           	return BLAST_EXIT_SUCCESS;
        }
        CBlastFastaInputSource fasta(m_CmdLineArgs->GetInputStream(), iconfig);
        CBlastInput input(&fasta);

        // Initialize the megablast database index now so we can know whether an indexed search will be run.
        // This is only important for the reference in the report, but would be done anyway.
        if (opt.GetUseIndex() && !m_CmdLineArgs->ExecuteRemotely()) {
            CRef<CBlastOptions> my_options(&(opts_hndl->SetOptions()));
            CSetupFactory::InitializeMegablastDbIndex(my_options);
        }
        /*** Get the formatting options ***/
        CRef<CFormattingArgs> fmt_args(m_CmdLineArgs->GetFormattingArgs());
        bool isArchiveFormat = fmt_args->ArchiveFormatRequested(args);
        if(!isArchiveFormat) {
        	bah.DoNotSaveMessages();
        }
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
                               fmt_args->GetCustomOutputFormatSpec(),
                               m_CmdLineArgs->GetTask() == "megablast",
                               opt.GetMBIndexLoaded(),
                               NULL, NULL,
                               GetCmdlineArgs(GetArguments()),
			       GetSubjectFile(args));
                               
        formatter.SetQueryRange(query_opts->GetRange());
        formatter.SetLineLength(fmt_args->GetLineLength());
        formatter.SetHitsSortOption(fmt_args->GetHitsSortOption());
        formatter.SetHspsSortOption(fmt_args->GetHspsSortOption());
        formatter.SetCustomDelimiter(fmt_args->GetCustomDelimiter());
        if(UseXInclude(*fmt_args, args[kArgOutput].AsString())) {
        	formatter.SetBaseFile(args[kArgOutput].AsString());
        }
        formatter.PrintProlog();

        /*** Process the input ***/
        CBatchSizeMixer mixer(SplitQuery_GetChunkSize(opt.GetProgram())-1000);
        int batch_size = m_CmdLineArgs->GetQueryBatchSize();
        if (batch_size) {
            input.SetBatchSize(batch_size);
	    BLAST_PROF_ADD( BATCH_SIZE, (int)batch_size );
        } else {
            Int8 total_len = formatter.GetDbTotalLength();
            if (total_len > 0) {
                /* the optimal hits per batch scales with total db size */
                mixer.SetTargetHits(total_len / 3000);
            }
            input.SetBatchSize(mixer.GetBatchSize());
	    BLAST_PROF_ADD( BATCH_SIZE, (int)mixer.GetBatchSize() );
        }
	BLAST_PROF_STOP( APP.PRE );
        for (; !input.End(); formatter.ResetScopeHistory(), QueryBatchCleanup() ) {
	    BLAST_PROF_START( APP.LOOP.PRE );
            CRef<CBlastQueryVector> query_batch(input.GetNextSeqBatch(*scope));
            CRef<IQueryFactory> queries(new CObjMgr_QueryFactory(*query_batch));

            SaveSearchStrategy(args, m_CmdLineArgs, queries, opts_hndl);

            CRef<CSearchResultSet> results;

	    BLAST_PROF_STOP( APP.LOOP.PRE );
            if (m_CmdLineArgs->ExecuteRemotely()) {
                CRef<CRemoteBlast> rmt_blast = 
                    InitializeRemoteBlast(queries, db_args, opts_hndl,
                          m_CmdLineArgs->ProduceDebugRemoteOutput(),
                          m_CmdLineArgs->GetClientId());
                results = rmt_blast->GetResultSet();
            } else {
	        BLAST_PROF_START( APP.LOOP.BLAST );
                CLocalBlast lcl_blast(queries, opts_hndl, db_adapter);
                lcl_blast.SetNumberOfThreads(m_CmdLineArgs->GetNumThreads());
		lcl_blast.SetBatchNumber( batch_num );
                results = lcl_blast.Run();
                if (!batch_size) 
                    input.SetBatchSize(mixer.GetBatchSize(lcl_blast.GetNumExtensions()));
	        BLAST_PROF_STOP( APP.LOOP.BLAST );
            }
	    BLAST_PROF_START( APP.LOOP.FMT );
            if (isArchiveFormat) {
                formatter.WriteArchive(*queries, *opts_hndl, *results, 0, bah.GetMessages());
                bah.ResetMessages();
            } else {
                BlastFormatter_PreFetchSequenceData(*results, scope,
                			                        fmt_args->GetFormattedOutputChoice());
                ITERATE(CSearchResultSet, result, *results) {
                    formatter.PrintOneResultSet(**result, query_batch);
                }
            }
	    BLAST_PROF_STOP( APP.LOOP.FMT );
	    batch_num++;
        }
        BLAST_PROF_START( APP.POST );
        formatter.PrintEpilog(opt);

        if (m_CmdLineArgs->ProduceDebugOutput()) {
            opts_hndl->GetOptions().DebugDumpText(NcbiCerr, "BLAST options", 1);
        }

        LogQueryInfo(m_UsageReport, input);
        formatter.LogBlastSearchInfo(m_UsageReport);
        BLAST_PROF_STOP( APP.POST );
    } CATCH_ALL(status)

    if(!bah.GetMessages().empty()) {
    	const CArgs & a = GetArgs();
    	PrintErrorArchive(a, bah.GetMessages());
    }

	m_UsageReport.AddParam(CBlastUsageReport::eNumThreads, (int) m_CmdLineArgs->GetNumThreads());
    m_UsageReport.AddParam(CBlastUsageReport::eExitStatus, status);
    BLAST_PROF_STOP( APP.MAIN );
    BLAST_PROF_ADD( THREADS , (int)m_CmdLineArgs->GetNumThreads() );
    BLAST_PROF_ADD( BATCHES , (int)batch_num );
    BLAST_PROF_ADD( EXIT_STATUS , (int)status );
    BLAST_PROF_REPORT ;
    return status;
}

int CBlastnApp::x_RunMTBySplitQuery()
{
    BLAST_PROF_START( APP.MAIN );
    BLAST_PROF_START( APP.PRE );
    BLAST_PROF_ADD2( PROGRAM, blastn ) ;
    int status = BLAST_EXIT_SUCCESS;
    CBlastAppDiagHandler bah;
    int batch_size = 1000000;

	char * mt_query_batch_env = getenv("BLAST_MT_QUERY_BATCH_SIZE");
	if (mt_query_batch_env) {
		batch_size = NStr::StringToInt(mt_query_batch_env);
	}
	cerr << "Batch Size: " << batch_size << endl;
    // Allow the fasta reader to complain on invalid sequence input
    SetDiagPostLevel(eDiag_Warning);
    SetDiagPostPrefix("blastn");
    SetDiagHandler(&bah, false);

	try {
    	const CArgs& args = GetArgs();
    	const int kMaxNumOfThreads = args[kArgNumThreads].AsInteger();
    	CRef<CBlastOptionsHandle> opts_hndl;
        if(RecoverSearchStrategy(args, m_CmdLineArgs)) {
        	opts_hndl.Reset(&*m_CmdLineArgs->SetOptionsForSavedStrategy(args));
        }
        else {
        	opts_hndl.Reset(&*m_CmdLineArgs->SetOptions(args));
        }
    	if(IsIStreamEmpty(m_CmdLineArgs->GetInputStream())){
       		ERR_POST(Warning << "Query is Empty!");
       		return BLAST_EXIT_SUCCESS;
    	}
    	CNcbiOstream & out_stream = m_CmdLineArgs->GetOutputStream();
		CBlastMasterNode master_node(out_stream, kMaxNumOfThreads);
   		int chunk_num = 0;

   		CBlastNodeInputReader input(m_CmdLineArgs->GetInputStream(), batch_size, 2000);
		while (master_node.Processing()) {
			if (!input.AtEOF()) {
			 	if (!master_node.IsFull()) {
					string qb;
					int q_index = 0;
					int num_q = input.GetQueryBatch(qb, q_index);
					if (num_q > 0) {
						CBlastNodeMailbox * mb(new CBlastNodeMailbox(chunk_num, master_node.GetBuzzer()));
						CBlastnNode * t(new CBlastnNode(chunk_num, GetArguments(), args, bah, qb, q_index, num_q, mb));
						master_node.RegisterNode(t, mb);
						chunk_num ++;
					}
				}
			}
			else {
				master_node.Shutdown();
			}
    	}

	} CATCH_ALL (status)

    if(!bah.GetMessages().empty()) {
    	const CArgs & a = GetArgs();
    	PrintErrorArchive(a, bah.GetMessages());
    }
	    BLAST_PROF_STOP( APP.MAIN );
    BLAST_PROF_ADD( THREADS , (int)m_CmdLineArgs->GetNumThreads() );
    BLAST_PROF_ADD( EXIT_STATUS , (int)status );
    BLAST_PROF_REPORT ;

    return status;
}

#ifndef SKIP_DOXYGEN_PROCESSING
int NcbiSys_main(int argc, ncbi::TXChar* argv[])
{
    return CBlastnApp().AppMain(argc, argv);
}
#endif /* SKIP_DOXYGEN_PROCESSING */
