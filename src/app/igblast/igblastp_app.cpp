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
 * Authors:  Ning Ma
 *
 */

/** @file igblastp_app.cpp
 * IGBLASTP command line application
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
#include <algo/blast/blastinput/igblastp_args.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/blast/format/blast_format.hpp>
#include "../blast/blast_app_util.hpp"

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
#endif

class CIgBlastpApp : public CNcbiApplication
{
public:
    /** @inheritDoc */
    CIgBlastpApp() {
        CRef<CVersion> version(new CVersion());
        version->SetVersionInfo(new CIgBlastVersion());
        SetFullVersion(version);
    }
private:
    /** @inheritDoc */
    virtual void Init();
    /** @inheritDoc */
    virtual int Run();

    /// This application's command line args
    CRef<CIgBlastpAppArgs> m_CmdLineArgs; 
};

void CIgBlastpApp::Init()
{
    // formulate command line arguments

    m_CmdLineArgs.Reset(new CIgBlastpAppArgs());

    // read the command line

    HideStdArgs(fHideLogfile | fHideConffile | fHideFullVersion | fHideXmlHelp | fHideDryRun);
    SetupArgDescriptions(m_CmdLineArgs->SetCommandLine());
}

int CIgBlastpApp::Run(void)
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
        const CBlastOptions& opt = opts_hndl->GetOptions();

        /*** Get the query sequence(s) ***/
        CRef<CQueryOptionsArgs> query_opts = 
            m_CmdLineArgs->GetQueryOptionsArgs();
        SDataLoaderConfig dlconfig(query_opts->QueryIsProtein());
        dlconfig.OptimizeForWholeLargeSequenceRetrieval();
        CBlastInputSourceConfig iconfig(dlconfig, query_opts->GetStrand(),
                                     query_opts->UseLowercaseMasks(),
                                     query_opts->GetParseDeflines(),
                                     query_opts->GetRange());
        iconfig.SetQueryLocalIdMode();
        CBlastFastaInputSource fasta(m_CmdLineArgs->GetInputStream(), iconfig);
        CBlastInput input(&fasta, m_CmdLineArgs->GetQueryBatchSize());

        /*** Initialize igblast database/subject and options ***/
        CRef<CIgBlastArgs> ig_args(m_CmdLineArgs->GetIgBlastArgs());
        CRef<CIgBlastOptions> ig_opts(ig_args->GetIgBlastOptions());

        /*** Initialize the database/subject ***/
        bool db_is_remote = true;
        CRef<CScope> scope;
        CRef<CLocalDbAdapter> blastdb;
        CRef<CLocalDbAdapter> blastdb_full;
        CRef<CBlastDatabaseArgs> db_args(m_CmdLineArgs->GetBlastDatabaseArgs());
        if (db_args->GetDatabaseName() == kEmptyStr && 
            db_args->GetSubjects().Empty()) {
            blastdb.Reset(&(*(ig_opts->m_Db[0])));
            scope.Reset(new CScope(*CObjectManager::GetInstance()));
            db_is_remote = false;
            blastdb_full.Reset(&(*blastdb));
        } else {
            InitializeSubject(db_args, opts_hndl, m_CmdLineArgs->ExecuteRemotely(),
                              blastdb, scope);
            if (m_CmdLineArgs->ExecuteRemotely()) {
                blastdb_full.Reset(&(*blastdb));
            } else {
                CSearchDatabase sdb(ig_opts->m_Db[0]->GetDatabaseName() + " " +
                       blastdb->GetDatabaseName(),
                       CSearchDatabase::eBlastDbIsProtein);
                blastdb_full.Reset(new CLocalDbAdapter(sdb));
            }
        }
        _ASSERT(blastdb && scope);

        // TODO: whose priority is higher?
        ig_args->AddIgSequenceScope(scope);

        /*** Get the formatting options ***/
        CRef<CFormattingArgs> fmt_args(m_CmdLineArgs->GetFormattingArgs());
        Int4 num_alignments = (db_args->GetDatabaseName() == kEmptyStr) ? 
                               0 : fmt_args->GetNumAlignments();
        CBlastFormat formatter(opt, *blastdb_full,
                               fmt_args->GetFormattedOutputChoice(),
                               query_opts->GetParseDeflines(),
                               m_CmdLineArgs->GetOutputStream(),
                               fmt_args->GetNumDescriptions(),
                               num_alignments,
                               *scope,
                               opt.GetMatrixName(),
                               fmt_args->ShowGis(),
                               fmt_args->DisplayHtmlOutput(),
                               opt.GetQueryGeneticCode(),
                               opt.GetDbGeneticCode(),
                               opt.GetSumStatisticsMode(),
                               false,
                               blastdb->GetFilteringAlgorithm(),
                               fmt_args->GetCustomOutputFormatSpec(),
                               false,
                               false,
                               &*ig_opts);
                               
        
        formatter.PrintProlog();
        
        /*** Process the input ***/
        for (; !input.End(); formatter.ResetScopeHistory()) {

            CRef<CBlastQueryVector> query(input.GetNextSeqBatch(*scope));

            //SaveSearchStrategy(args, m_CmdLineArgs, queries, opts_hndl);
            CRef<CSearchResultSet> results;

            if (m_CmdLineArgs->ExecuteRemotely() && db_is_remote) {
                CIgBlast rmt_blast(query, 
                                   db_args->GetSearchDatabase(), 
                                   db_args->GetSubjects(),
                                   opts_hndl, ig_opts,
                                   NcbiEmptyString);
                //TODO:          m_CmdLineArgs->ProduceDebugRemoteOutput(),
                //TODO:          m_CmdLineArgs->GetClientId());
                results = rmt_blast.Run();
            } else {
                CIgBlast lcl_blast(query, blastdb, opts_hndl, ig_opts);
                lcl_blast.SetNumberOfThreads(m_CmdLineArgs->GetNumThreads());
                results = lcl_blast.Run();
            }

            /* TODO should we support archive format?
            if (fmt_args->ArchiveFormatRequested(args)) {
                CRef<IQueryFactory> qf(new CObjMgr_QueryFactory(*query));
                formatter.WriteArchive(*qf, *opts_hndl, *results);
            } else {
            */
            BlastFormatter_PreFetchSequenceData(*results, scope);
            ITERATE(CSearchResultSet, result, *results) {
                CIgBlastResults &ig_result = *const_cast<CIgBlastResults *>
                        (dynamic_cast<const CIgBlastResults *>(&(**result)));
                formatter.PrintOneResultSet(ig_result, query);
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
    return CIgBlastpApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
#endif /* SKIP_DOXYGEN_PROCESSING */
