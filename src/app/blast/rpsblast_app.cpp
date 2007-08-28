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
#include "blast_app_util.hpp"
#include "blast_format.hpp"

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
#endif

class CRPSBlastApp : public CNcbiApplication
{
public:
    CRPSBlastApp() {
        SetVersion(blast::Version);
    }
private:
    virtual void Init();
    virtual int Run();

    CRef<CObjectManager> m_ObjMgr;
    CRef<CRPSBlastAppArgs> m_CmdLineArgs;
};

void CRPSBlastApp::Init()
{
    // get the object manager instance
    m_ObjMgr = CObjectManager::GetInstance();
    if (!m_ObjMgr) {
        throw std::runtime_error("Could not initialize object manager");
    }

    // formulate command line arguments

    m_CmdLineArgs.Reset(new CRPSBlastAppArgs());

    // read the command line

    HideStdArgs(fHideLogfile | fHideConffile | fHideDryRun);
    SetupArgDescriptions(m_CmdLineArgs->SetCommandLine());
}

int CRPSBlastApp::Run(void)
{
    int status = 0;

    try {

        // Allow the fasta reader to complain on invalid sequence input
        SetDiagPostLevel(eDiag_Warning);

        /*** Get the BLAST options ***/
        const CArgs& args = GetArgs();
        RecoverSearchStrategy(args, m_CmdLineArgs);
        CRef<CBlastOptionsHandle> opts_hndl(&*m_CmdLineArgs->SetOptions(args));
        const CBlastOptions& opt = opts_hndl->GetOptions();

        /*** Get the query sequence(s) ***/
        CRef<CQueryOptionsArgs> query_opts = 
            m_CmdLineArgs->GetQueryOptionsArgs();
        SDataLoaderConfig dlconfig(query_opts->QueryIsProtein());
        CBlastInputConfig iconfig(dlconfig, query_opts->GetStrand(),
                                     query_opts->UseLowercaseMasks(),
                                     query_opts->BelieveQueryDefline(),
                                     query_opts->GetRange());
        CBlastFastaInputSource fasta(*m_ObjMgr, m_CmdLineArgs->GetInputStream(),
                                     iconfig);
        CBlastInput input(&fasta, m_CmdLineArgs->GetQueryBatchSize());

        /*** Initialize the database ***/
        CRef<CBlastDatabaseArgs> db_args(m_CmdLineArgs->GetBlastDatabaseArgs());
        CRef<CLocalDbAdapter> db_adapter;   /* needed for local searches */
        CRef<CSearchDatabase> search_db;    /* needed for remote searches and
                                               for exporting the search
                                               strategy */
        search_db = db_args->GetSearchDatabase();
        if ( !m_CmdLineArgs->ExecuteRemotely() ) {
            CRef<CSeqDB> seqdb = GetSeqDB(db_args);
            db_adapter.Reset(new CLocalDbAdapter(seqdb));

            const string loader_name = RegisterOMDataLoader(m_ObjMgr, seqdb);
            fasta.GetScope()->AddDataLoader(loader_name); 
        }

        /*** Get the formatting options ***/
        CRef<CFormattingArgs> fmt_args(m_CmdLineArgs->GetFormattingArgs());
        CBlastFormat formatter(opt,
                               db_args->GetDatabaseName(),
                               fmt_args->GetFormattedOutputChoice(),
                               db_args->IsProtein(),
                               query_opts->BelieveQueryDefline(),
                               m_CmdLineArgs->GetOutputStream(),
                               fmt_args->GetNumDescriptions(),
                               fmt_args->GetNumAlignments(),
                               opt.GetMatrixName(),
                               fmt_args->ShowGis(),
                               fmt_args->DisplayHtmlOutput(),
                               opt.GetQueryGeneticCode(),
                               opt.GetDbGeneticCode(),
                               opt.GetSumStatisticsMode());
        formatter.PrintProlog();

        /*** Process the input ***/
        while ( !fasta.End() ) {

            TSeqLocVector query_batch(input.GetNextSeqLocBatch());
            CRef<IQueryFactory> queries(new CObjMgr_QueryFactory(query_batch));

            SaveSearchStrategy(args, m_CmdLineArgs, queries, opts_hndl, 
                               search_db);

            CRef<CSearchResultSet> results;

            if (m_CmdLineArgs->ExecuteRemotely()) {
                CRemoteBlast rmt_blast(queries, opts_hndl, *search_db);
                if (m_CmdLineArgs->ProduceDebugRemoteOutput()) {
                    rmt_blast.SetVerbose();
                }
                results = rmt_blast.GetResultSet();
            } else {
                CLocalBlast lcl_blast(queries, opts_hndl, db_adapter);
                lcl_blast.SetNumberOfThreads(m_CmdLineArgs->GetNumThreads());
                results = lcl_blast.Run();
            }

            ITERATE(CSearchResultSet, result, *results) {
                formatter.PrintOneResultSet(**result, *fasta.GetScope());
            }

        }

        formatter.PrintEpilog(opt);

        if (m_CmdLineArgs->ProduceDebugOutput()) {
            opts_hndl->GetOptions().DebugDumpText(NcbiCerr, "BLAST options", 1);
        }

    } catch (const CBlastException& exptn) {
        cerr << exptn.what() << endl;
        status = exptn.GetErrCode();
    } catch (const exception& e) {
        cerr << e.what() << endl;
        status = -1;
    } catch (...) {
        cerr << "Unknown exception" << endl;
        status = -1;
    }
    return status;
}


#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
    return CRPSBlastApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
#endif /* SKIP_DOXYGEN_PROCESSING */
