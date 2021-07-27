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
 * Author: Amelia Fong
 *
 */

/// @file blastn_vdb.cpp
/// BLASTN command line application that searches the vdb databases.
///
/// - Based on the standard BLASTN command line application located in
///   app/blast/blastn_app.cpp.
/// - Modifications include using the VDB Blast utils class to
///   initialize VDB-specific BlastSeqSrc and BlastSeqInfoSrc objects.
/// - Does not support remote searches

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/api/remote_blast.hpp>
#include <algo/blast/blastinput/blast_fasta_input.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/blast/format/blast_format.hpp>

#include <algo/blast/vdb/vdb2blast_util.hpp>
#include <algo/blast/vdb/vdbalias.hpp>
#include <algo/blast/vdb/vdbblast_local.hpp>
#include <algo/blast/vdb/blastn_vdb_args.hpp>

#include "blast_vdb_app_util.hpp"
#include "../blast/blast_app_util.hpp"

#include "CBlastVdbVersion.hpp" // CBlastVdbVersion

USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);

// ==========================================================================//

/// Exit code for successful completion of the Blast search.
#define BLAST_SRA_EXIT_SUCCESS          0
/// Exit code for a Blast input error (in query/options).
#define BLAST_SRA_INPUT_ERROR           1
/// Exit code for a Blast engine-related error.
#define BLAST_SRA_ENGINE_ERROR          2
/// Exit code for all other errors and unknown exceptions.
#define BLAST_SRA_UNKNOWN_ERROR         255

// ==========================================================================//

/// CVDBBlastnApp
///
/// Blastn application that searches the SRA databases.

class CVDBBlastnApp : public CNcbiApplication
{
public:
    /// Constructor, sets up the version info.
    CVDBBlastnApp();

    ~CVDBBlastnApp();

private:
    /// Initialize the application.
    virtual void Init(void);
    /// Run the application.
    virtual int  Run(void);
    /// Cleanup on application exit.
    virtual void Exit(void);

    void x_SetupLocalVDBSearch();
    void x_FillVDBInfo(CBlastFormatUtil::SDbInfo & vecDbInfo);

    CRef<CScope> x_GetScope(void);
    void x_GetFullVDBPaths(void);

private:
    string m_dbAllNames;
    CLocalVDBBlast::SLocalVDBStruct m_localVDBStruct;

    /// This application's command line arguments.
    CRef<CBlastnVdbAppArgs> m_CmdLineArgs;
    CBlastUsageReport m_UsageReport;
    CStopWatch m_StopWatch;
};

// ==========================================================================//
// Various helper functions

CVDBBlastnApp::~CVDBBlastnApp() {
   	m_UsageReport.AddParam(CBlastUsageReport::eRunTime, m_StopWatch.Elapsed());
}


void CVDBBlastnApp::x_SetupLocalVDBSearch()
{
    // Read the SRA-related input
    CArgs args = GetArgs();
    unsigned int num_threads = (unsigned int) (args[kArgNumThreads].AsInteger());
    CLocalVDBBlast::ESRASearchMode search_mode = (CLocalVDBBlast::ESRASearchMode) args[kArgSRASearchMode].AsInteger();
    CLocalVDBBlast::PreprocessDBs(m_localVDBStruct, m_dbAllNames, num_threads, search_mode);
}

void CVDBBlastnApp::x_FillVDBInfo(CBlastFormatUtil::SDbInfo & dbInfo )
{
    dbInfo.is_protein = false;
    dbInfo.name = m_dbAllNames;
    dbInfo.definition = dbInfo.name;
    dbInfo.total_length = m_localVDBStruct.total_length;
   	dbInfo.number_seqs = m_localVDBStruct.total_num_seqs;
}

// ==========================================================================//
// Initialization

CVDBBlastnApp::CVDBBlastnApp()
{
    CRef<CVersion> version(new CVersion());
    version->SetVersionInfo(new CBlastVdbVersion);
    SetFullVersion(version);
    m_StopWatch.Start();
    if (m_UsageReport.IsEnabled()) {
        m_UsageReport.AddParam(CBlastUsageReport::eVersion, GetVersion().Print());
    }

}

void CVDBBlastnApp::Init(void)
{
	   m_CmdLineArgs.Reset(new CBlastnVdbAppArgs());

	    // read the command line

	    HideStdArgs(fHideLogfile | fHideConffile | fHideFullVersion | fHideXmlHelp | fHideDryRun);
	    SetupArgDescriptions(m_CmdLineArgs->SetCommandLine());
	    SetEnvironment("CSRA_CLIP_BY_QUALITY", "true");
	    SetEnvironment("CSRA_PATH_IN_ID", "false");
	    //SetDiagTrace(eDT_Enable);

}

// ==========================================================================//
// Run demo

void CVDBBlastnApp::x_GetFullVDBPaths(void)
{
	  string dbs = m_CmdLineArgs->GetBlastDatabaseArgs()->GetDatabaseName();
	  vector<string> paths;
	  CVDBAliasUtil::FindVDBPaths(dbs, false, paths, NULL, NULL, true, true, false);
	  m_dbAllNames = NStr::Join(paths, " ");
}

int CVDBBlastnApp::Run(void)
{
    int status = BLAST_SRA_EXIT_SUCCESS;
    try
    {
        // Allow the fasta reader to complain on invalid sequence input
        SetDiagPostLevel(eDiag_Warning);
        SetDiagPostPrefix("blastn_vdb");

        // Get the arguments
        const CArgs& args = GetArgs();

        // Get and validate the Blast options
        CRef<CBlastOptionsHandle> optsHandle;
        if(RecoverSearchStrategy(args, m_CmdLineArgs)){
        	optsHandle.Reset(&*m_CmdLineArgs->SetOptionsForSavedStrategy(args));
        }
        else {
        	if(!(args.Exist(kArgDb) && args[kArgDb]))
        		 NCBI_THROW(CInputException, eInvalidInput,
        				 	"Must specify at least one SRA/WGS database");

           	optsHandle.Reset(&*m_CmdLineArgs->SetOptions(args));
        }
        const CBlastOptions& opt = optsHandle->GetOptions();

        // Get the query sequence(s)
        CRef<CQueryOptionsArgs> query_opts = 
            m_CmdLineArgs->GetQueryOptionsArgs();
        SDataLoaderConfig dlconfig(query_opts->QueryIsProtein());
        dlconfig.OptimizeForWholeLargeSequenceRetrieval();
        CBlastInputSourceConfig iconfig(dlconfig, query_opts->GetStrand(),
                                     query_opts->UseLowercaseMasks(),
                                     query_opts->GetParseDeflines(),
                                     query_opts->GetRange(),
                                     !m_CmdLineArgs->ExecuteRemotely());
        iconfig.SetQueryLocalIdMode();
        CBlastFastaInputSource fasta(m_CmdLineArgs->GetInputStream(), iconfig);
        CBlastInput input(&fasta);

        // Resolve all vdb paths first
        x_GetFullVDBPaths();

        // Initialize the object manager and the scope object
        CRef<CScope> scope = GetVDBScope(m_dbAllNames);

        CVDBBlastUtil::SetupVDBManager();
        // Setup for local vdb search
        x_SetupLocalVDBSearch();
        // Create the DBInfo entries for dbs being searched
        vector< CBlastFormatUtil::SDbInfo > vecDbInfo(1);
        x_FillVDBInfo(vecDbInfo[0]);

        // Get the formatting options and initialize the formatter
        CRef<CFormattingArgs> fmt_args(m_CmdLineArgs->GetFormattingArgs());
        CBlastFormat formatter(opt, vecDbInfo,
                               fmt_args->GetFormattedOutputChoice(),
                               query_opts->GetParseDeflines(),
                               m_CmdLineArgs->GetOutputStream(),
                               fmt_args->GetNumDescriptions(),
                               fmt_args->GetNumAlignments(),
                               *scope,
                               fmt_args->ShowGis(),
                               fmt_args->DisplayHtmlOutput(),
                               m_CmdLineArgs->ExecuteRemotely(),
                               fmt_args->GetCustomOutputFormatSpec(),
                               true,
                               GetCmdlineArgs(GetArguments()));

        // Begin Blast output
        formatter.SetQueryRange(query_opts->GetRange());
        formatter.SetLineLength(fmt_args->GetLineLength());
        if(UseXInclude(*fmt_args, args[kArgOutput].AsString())){
        		formatter.SetBaseFile(args[kArgOutput].AsString());
        }
        formatter.PrintProlog();

        CBatchSizeMixer mixer(SplitQuery_GetChunkSize(opt.GetProgram())-1000);
        int batch_size = m_CmdLineArgs->GetQueryBatchSize();
        if (batch_size) {
               input.SetBatchSize(batch_size);
        } else {
               Int8 total_len = formatter.GetDbTotalLength();
               if (total_len > 0) {
                        /* the optimal hits per batch scales with total db size */
            	   	    Int4 target_hits = (total_len/3000) <  2000000 ? 2000000: total_len/3000;
                        mixer.SetTargetHits(target_hits);
               }
               input.SetBatchSize(mixer.GetBatchSize());
        }

        // Process the input
        for (; !input.End(); formatter.ResetScopeHistory()) {
            CRef<CBlastQueryVector> query_batch(input.GetNextSeqBatch(*scope));
            CRef<IQueryFactory> queries(new CObjMgr_QueryFactory(*query_batch));
            SaveSearchStrategy(args, m_CmdLineArgs, queries, optsHandle);

            // Run local Blast
            CRef<CSearchResultSet> results;
            CLocalVDBBlast  local_vdb_blast(query_batch, optsHandle, m_localVDBStruct);
            results = local_vdb_blast.Run();
            if (!batch_size)
            	 input.SetBatchSize(mixer.GetBatchSize(local_vdb_blast.GetNumExtensions()));

            if (fmt_args->ArchiveFormatRequested(args)){
            	formatter.WriteArchive(*queries, *optsHandle, *results);
            }
            else {
            	//CScope::TBioseqHandles handles;
            	//SortAndFetchSeqData(*results, scope, handles);
            	// Output the results
            	ITERATE(CSearchResultSet, result, *results) {
            		formatter.PrintOneResultSet(**result, query_batch);
            	}
            }
        }
        // End Blast output
        formatter.PrintEpilog(opt);
        LogQueryInfo(m_UsageReport, input);
        formatter.LogBlastSearchInfo(m_UsageReport);

        // Optional debug output
        if (m_CmdLineArgs->ProduceDebugOutput()) {
            optsHandle->GetOptions().DebugDumpText(NcbiCerr, "BLAST options", 1);
        }
        CVDBBlastUtil::ReleaseVDBManager();
    }
    catch (const CInputException& e) {
        cerr << "BLAST query/options error: " << e.GetMsg() << endl;
        status = BLAST_SRA_INPUT_ERROR;
    }
    catch (const CArgException& e) {
        cerr << "Command line argument error: " << e.GetMsg() << endl;
        status = BLAST_SRA_INPUT_ERROR;
    }
    catch (const CBlastException& e) {
        cerr << "BLAST engine error: " << e.GetMsg() << endl;
        status = BLAST_SRA_ENGINE_ERROR;
    	// Temporary fix to avoid vdb core dump during cleanup SB-1170
    	abort();
    }
    catch (const CException& e) {
        cerr << "Error: " << e.GetMsg() << endl;
        status = BLAST_SRA_UNKNOWN_ERROR;
    }
    catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        status = BLAST_SRA_UNKNOWN_ERROR;
    }
    catch (...) {
        cerr << "Unknown exception occurred" << endl;
        status = BLAST_SRA_UNKNOWN_ERROR;
    }

    m_UsageReport.AddParam(CBlastUsageReport::eNumThreads, (int) m_CmdLineArgs->GetNumThreads());
    m_UsageReport.AddParam(CBlastUsageReport::eExitStatus, status);
    return status;
}

// ==========================================================================//
// Cleanup

void CVDBBlastnApp::Exit(void)
{
    SetDiagStream(0);
}

// ==========================================================================//
// Main

#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[])
{
    // Execute main application function
    return CVDBBlastnApp().AppMain(argc, argv, 0, eDS_Default, "");
}
#endif /* SKIP_DOXYGEN_PROCESSING */

// ==========================================================================//
