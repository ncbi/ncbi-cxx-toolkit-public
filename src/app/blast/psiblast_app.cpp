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

/** @file psiblast.cpp
 * PSI-BLAST command line application
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
	"$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <objmgr/object_manager.hpp>
#include <algo/blast/api/blast_options.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/seqsrc_seqdb.hpp>
#include <algo/blast/blastinput/blast_fasta_input.hpp>
#include <algo/blast/blastinput/psiblast_args.hpp>
#include <algo/blast/api/remote_blast.hpp>
#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include "blast_app_util.hpp"
#include "blast_format.hpp"
#include <algo/blast/api/psiblast.hpp>
#include <algo/blast/api/psiblast_iteration.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
#endif

class CPsiBlastApp : public CNcbiApplication
{
public:
    CPsiBlastApp() {
        SetVersion(blast::Version);
    }
private:
    virtual void Init();
    virtual int Run();

    void SavePssmToCheckpoint(CRef<CPssmWithParameters> pssm, 
                              const CPsiBlastIterationState* itr = NULL) const;

    CRef<CObjectManager> m_ObjMgr;
    CRef<CPsiBlastAppArgs> m_CmdLineArgs;
};

void CPsiBlastApp::Init()
{
    // get the object manager instance
    m_ObjMgr = CObjectManager::GetInstance();
    if (!m_ObjMgr) {
        throw std::runtime_error("Could not initialize object manager");
    }

    // formulate command line arguments

    m_CmdLineArgs.Reset(new CPsiBlastAppArgs());

    // read the command line

    HideStdArgs(fHideLogfile | fHideConffile | fHideDryRun);
    SetupArgDescriptions(m_CmdLineArgs->SetCommandLine());
}

static CConstRef<CBioseq>
s_GetQueryBioseq(CConstRef<CBlastQueryVector> query, CRef<CScope> scope,
                 CRef<CPssmWithParameters> pssm)
{
    CConstRef<CBioseq> retval;

    if (pssm) {
        retval.Reset(&pssm->SetQuery().SetSeq());
    } else {
        _ASSERT(query.NotEmpty());
        CBioseq_Handle bh = scope->GetBioseqHandle(*query->GetQuerySeqLoc(0));
        retval.Reset(bh.GetBioseqCore());
    }
    return retval;
}

/// Auxiliary function to create the PSSM for the next iteration
static CRef<CPssmWithParameters>
x_ComputePssmForNextIteration(const CBioseq& bioseq,
                              CConstRef<CSeq_align_set> sset,
                              CConstRef<CPSIBlastOptionsHandle> opts_handle,
                              CRef<CScope> scope)
{
    CPSIDiagnosticsRequest diags(PSIDiagnosticsRequestNew());
    diags->frequency_ratios = true;
    return PsiBlastComputePssmFromAlignment(bioseq, sset, scope, *opts_handle,
                                            diags);
}

void
CPsiBlastApp::SavePssmToCheckpoint(CRef<CPssmWithParameters> pssm, 
                                   const CPsiBlastIterationState* itr) const
{
    // N.B.: this branch will be taken in case of remote PSI-BLAST
    if (itr == NULL) {
        // FIXME: make sure the proper pssm is being sent or do the semantics
        // change?
        if (m_CmdLineArgs->SaveCheckpoint() && pssm.NotEmpty()) {
            *m_CmdLineArgs->GetCheckpointStream() << MSerial_AsnText << *pssm;
        }
        return;
    }

    _ASSERT(itr);
    if (pssm.NotEmpty() && 
        itr->GetIterationNumber() > 1 && 
        m_CmdLineArgs->SaveCheckpoint()) {
        *m_CmdLineArgs->GetCheckpointStream() << MSerial_AsnText << *pssm;
    }
}

int CPsiBlastApp::Run(void)
{

    int status = 0;

    try {

        // Allow the fasta reader to complain on invalid sequence input
        SetDiagPostLevel(eDiag_Warning);

        const CArgs& args = GetArgs();
        RecoverSearchStrategy(args, m_CmdLineArgs);

        CRef<CQueryOptionsArgs> query_opts = 
            m_CmdLineArgs->GetQueryOptionsArgs();

        CRef<CBlastOptionsHandle> opts_hndl(m_CmdLineArgs->SetOptions(args));
        CRef<CPSIBlastOptionsHandle> opts;
        opts.Reset(dynamic_cast<CPSIBlastOptionsHandle*>(&*opts_hndl));
        _ASSERT(opts.NotEmpty());
        const CBlastOptions& opt = opts->GetOptions();
        const size_t kNumIterations = m_CmdLineArgs->GetNumberOfIterations();

        /*** Get the query sequence(s) or PSSM (these two options are mutually
         * exclusive) ***/
        CRef<CPssmWithParameters> pssm = m_CmdLineArgs->GetInputPssm();
        CRef<CBlastQueryVector> query;
        CRef<CBlastFastaInputSource> fasta;
        CRef<CBlastInput> input;
        const SDataLoaderConfig dlconfig(query_opts->QueryIsProtein());
        CRef<CScope> scope = CBlastScopeSource(dlconfig).NewScope();
        CRef<IQueryFactory> query_factory;  /* populated if no PSSM is given */

        if (pssm.Empty()) {
            CBlastInputSourceConfig iconfig(dlconfig, query_opts->GetStrand(),
                                         query_opts->UseLowercaseMasks(),
                                         query_opts->BelieveQueryDefline(),
                                         query_opts->GetRange());
            fasta.Reset(new CBlastFastaInputSource(
                                         m_CmdLineArgs->GetInputStream(),
                                         iconfig));
            input.Reset(new CBlastInput(&*fasta,
                                        m_CmdLineArgs->GetQueryBatchSize()));
            query = input->GetAllSeqs(*scope);
            if (query->Size() > 1 && kNumIterations > 1) {
                ERR_POST(Warning << "Multiple queries provided and multiple "
                         "iterations requested, only first query will be "
                         "iterated");
            }
            query_factory.Reset(new CObjMgr_QueryFactory(*query));
        } else {
            _ASSERT(pssm->GetQuery().IsSeq());  // single query only!
            query.Reset(new CBlastQueryVector());
            scope->AddTopLevelSeqEntry(pssm->SetQuery());

            CRef<CSeq_loc> sl(new CSeq_loc());
            sl->SetId(*pssm->GetQuery().GetSeq().GetFirstId());

            CRef<CBlastSearchQuery> q(new CBlastSearchQuery(*sl, *scope));
            query->AddQuery(q);
        }
        _ASSERT( query.NotEmpty() );


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
            scope->AddDataLoader(RegisterOMDataLoader(m_ObjMgr, seqdb));
        }

        /*** Get the formatting options ***/
        CRef<CFormattingArgs> fmt_args(m_CmdLineArgs->GetFormattingArgs());
        CNcbiOstream& out_stream = m_CmdLineArgs->GetOutputStream();
        CBlastFormat formatter(opt,
                               db_args->GetDatabaseName(),
                               fmt_args->GetFormattedOutputChoice(),
                               db_args->IsProtein(),
                               query_opts->BelieveQueryDefline(),
                               out_stream,
                               fmt_args->GetNumDescriptions(),
                               fmt_args->GetNumAlignments(),
                               *scope,
                               opt.GetMatrixName(),
                               fmt_args->ShowGis(),
                               fmt_args->DisplayHtmlOutput(),
                               opt.GetQueryGeneticCode(),
                               opt.GetDbGeneticCode(),
                               opt.GetSumStatisticsMode());

        formatter.PrintProlog();

        SaveSearchStrategy(args, m_CmdLineArgs, query_factory, opts_hndl, 
                           search_db, pssm.GetPointer());

        if (m_CmdLineArgs->ExecuteRemotely()) {

            CRef<CRemoteBlast> rmt_psiblast;
            if (query_factory.Empty()) {
                rmt_psiblast.Reset(new CRemoteBlast(pssm, opts_hndl, 
                                                    *search_db));
            } else {
                rmt_psiblast.Reset(new CRemoteBlast(query_factory, opts_hndl,
                                                    *search_db));
            }

            if (m_CmdLineArgs->ProduceDebugRemoteOutput()) {
                rmt_psiblast->SetVerbose();
            }

            CRef<CSearchResultSet> results = rmt_psiblast->GetResultSet();
            ITERATE(CSearchResultSet, result, *results) {
                formatter.PrintOneResultSet(**result, query);
            }

            SavePssmToCheckpoint(rmt_psiblast->GetPSSM());

        } else {

            CPsiBlastIterationState itr(kNumIterations);

            CRef<CPsiBlast> psiblast;
            if (query_factory.NotEmpty()) {
                psiblast.Reset(new CPsiBlast(query_factory, db_adapter, opts));
            } else {
                _ASSERT(pssm.NotEmpty());
                psiblast.Reset(new CPsiBlast(pssm, db_adapter, opts));
            }
            psiblast->SetNumberOfThreads(m_CmdLineArgs->GetNumThreads());

            while (itr) {

                SavePssmToCheckpoint(pssm, &itr);

                CRef<CSearchResultSet> results = psiblast->Run();
                ITERATE(CSearchResultSet, result, *results) {
                    formatter.PrintOneResultSet(**result, query,
                                                itr.GetIterationNumber());
                }

                if ( !(*results)[0].HasAlignments() ) {
                    break;
                }

                CConstRef<CSeq_align_set> aln((*results)[0].GetSeqAlign());
                CPsiBlastIterationState::TSeqIds ids;
                CPsiBlastIterationState::GetSeqIds(aln, opts, ids);

                itr.Advance(ids);

                if (itr) {
                    CConstRef<CBioseq> seq =
                        s_GetQueryBioseq(query, scope, pssm);
                    pssm = 
                        x_ComputePssmForNextIteration(*seq, aln, opts, scope);
                    psiblast->SetPssm(pssm);
                }
            }

            if (itr.HasConverged() && !fmt_args->HasStructuredOutputFormat()) {
                out_stream << NcbiEndl << "Search has CONVERGED!" << NcbiEndl;
            }
        }

        // finish up
        formatter.PrintEpilog(opt);

        if (m_CmdLineArgs->ProduceDebugOutput()) {
            opts_hndl->GetOptions().DebugDumpText(NcbiCerr, "BLAST options", 1);
        }

    } catch (const CBlastException& exptn) {
        cerr << "Error: " << exptn.GetMsg() << endl;
        status = exptn.GetErrCode();
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
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
    return CPsiBlastApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
#endif /* SKIP_DOXYGEN_PROCESSING */
