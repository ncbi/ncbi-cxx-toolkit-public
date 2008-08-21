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

/** @file psiblast_app.cpp
 * PSI-BLAST command line application
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
	"$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
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
    /** @inheritDoc */
    CPsiBlastApp() {
        CRef<CVersion> version(new CVersion());
        version->SetVersionInfo(new CBlastVersion());
        SetFullVersion(version);
    }
private:
    /** @inheritDoc */
    virtual void Init();
    /** @inheritDoc */
    virtual int Run();

    /// Save the PSSM to a check point file and/or as an ASCII PSSM
    /// @param pssm PSSM to save [in]
    /// @param itr Iteration object, NULL in case of remote search [in]
    void SavePssmToFile(CRef<CPssmWithParameters> pssm, 
                        const CPsiBlastIterationState* itr = NULL) const;

    CRef<CPssmWithParameters>
    ComputePssmForNextIteration(const CBioseq& bioseq,
                                CConstRef<CSeq_align_set> sset,
                                CConstRef<CPSIBlastOptionsHandle> opts_handle,
                                CRef<CScope> scope,
                                CRef<CBlastAncillaryData> ancillary_data);

    /// This application's command line args
    CRef<CPsiBlastAppArgs> m_CmdLineArgs;
    /// Ancillary results for the previously executed PSI-BLAST iteration
    CConstRef<CBlastAncillaryData> m_AncillaryData;
};

void CPsiBlastApp::Init()
{
    // formulate command line arguments

    m_CmdLineArgs.Reset(new CPsiBlastAppArgs());

    // read the command line

    HideStdArgs(fHideLogfile | fHideConffile | fHideFullVersion | fHideXmlHelp | fHideDryRun);
    SetupArgDescriptions(m_CmdLineArgs->SetCommandLine());
}

/** 
 * @brief Extract the bioseq which represents the query from either a PSSM or a
 * CBlastQueryVector
 * 
 * @param query container for query sequence(s) [in]
 * @param scope Scope from which to retrieve the query if not in the PSSM [in]
 * @param pssm if NON-NULL, the query will be extracted from this object [in]
 */
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
        _ASSERT(bh);
        retval.Reset(bh.GetBioseqCore());
    }
    return retval;
}

/// Auxiliary function to create the PSSM for the next iteration
CRef<CPssmWithParameters>
CPsiBlastApp::ComputePssmForNextIteration(const CBioseq& bioseq,
                              CConstRef<CSeq_align_set> sset,
                              CConstRef<CPSIBlastOptionsHandle> opts_handle,
                              CRef<CScope> scope,
                              CRef<CBlastAncillaryData> ancillary_data)
{
    CPSIDiagnosticsRequest diags(PSIDiagnosticsRequestNew());
    diags->frequency_ratios = true;
    if (m_CmdLineArgs->SaveAsciiPssm()) {
        diags->information_content = true;
        diags->weighted_residue_frequencies = true;
        diags->gapless_column_weights = true;
        diags->sigma = diags->interval_sizes = diags->num_matching_seqs = true;
    }
    m_AncillaryData = ancillary_data;
    return PsiBlastComputePssmFromAlignment(bioseq, sset, scope, *opts_handle,
                                            m_AncillaryData, diags);
}

void
CPsiBlastApp::SavePssmToFile(CRef<CPssmWithParameters> pssm, 
                             const CPsiBlastIterationState* itr) const
{
    if (pssm.Empty()) {
        return;
    }

    if (m_CmdLineArgs->SaveCheckpoint() &&
        (itr == NULL || // this is true in the case of remote PSI-BLAST
         itr->GetIterationNumber() > 1)) {
        *m_CmdLineArgs->GetCheckpointStream() << MSerial_AsnText << *pssm;
    }

    if (m_CmdLineArgs->SaveAsciiPssm() &&
        (itr == NULL || // this is true in the case of remote PSI-BLAST
         itr->GetIterationNumber() >= 1)) {
        CBlastFormatUtil::PrintAsciiPssm(*pssm, 
                                         m_AncillaryData,
                                         *m_CmdLineArgs->GetAsciiPssmStream());
    }
}


int CPsiBlastApp::Run(void)
{
    int status = BLAST_EXIT_SUCCESS;

    try {

        // Allow the fasta reader to complain on invalid sequence input
        SetDiagPostLevel(eDiag_Warning);

        const CArgs& args = GetArgs();
        RecoverSearchStrategy(args, m_CmdLineArgs);

        CRef<CQueryOptionsArgs> query_opts = 
            m_CmdLineArgs->GetQueryOptionsArgs();

        CRef<CBlastOptionsHandle> opts_hndl(m_CmdLineArgs->SetOptions(args));
        const CBlastOptions& opt = opts_hndl->GetOptions();
        const size_t kNumIterations = m_CmdLineArgs->GetNumberOfIterations();

        /*** Get the query sequence(s) or PSSM (these two options are mutually
         * exclusive) ***/
        CRef<CPssmWithParameters> pssm = m_CmdLineArgs->GetInputPssm();
        CRef<CBlastQueryVector> query;
        CRef<CBlastFastaInputSource> fasta;
        CRef<CBlastInput> input;
        SDataLoaderConfig dlconfig(query_opts->QueryIsProtein());
        dlconfig.OptimizeForWholeLargeSequenceRetrieval();
        CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
        CRef<IQueryFactory> query_factory;  /* populated if no PSSM is given */

        if (pssm.Empty()) {
            CBlastInputSourceConfig iconfig(dlconfig, query_opts->GetStrand(),
                                         query_opts->UseLowercaseMasks(),
                                         query_opts->GetParseDeflines(),
                                         query_opts->GetRange(),
                                         !m_CmdLineArgs->ExecuteRemotely());
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
            sl->SetWhole().Assign(*pssm->GetQuery().GetSeq().GetFirstId());

            CRef<CBlastSearchQuery> q(new CBlastSearchQuery(*sl, *scope));
            query->AddQuery(q);
        }
        _ASSERT( query.NotEmpty() );


        /*** Initialize the database ***/
        CRef<CBlastDatabaseArgs> db_args(m_CmdLineArgs->GetBlastDatabaseArgs());
        CRef<CLocalDbAdapter> db_adapter;
        InitializeSubject(db_args, opts_hndl, m_CmdLineArgs->ExecuteRemotely(),
                         db_adapter, scope);
        _ASSERT(db_adapter && scope);

        /*** Get the formatting options ***/
        CRef<CFormattingArgs> fmt_args(m_CmdLineArgs->GetFormattingArgs());
        CNcbiOstream& out_stream = m_CmdLineArgs->GetOutputStream();
        CBlastFormat formatter(opt, *db_adapter,
                               fmt_args->GetFormattedOutputChoice(),
                               query_opts->GetParseDeflines(),
                               out_stream,
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
                               db_adapter->GetFilteringAlgorithms(),
                               fmt_args->GetCustomOutputFormatSpec());
        formatter.PrintProlog();

        SaveSearchStrategy(args, m_CmdLineArgs, query_factory, opts_hndl, pssm);

        if (m_CmdLineArgs->ExecuteRemotely()) {

            CRef<CRemoteBlast> rmt_psiblast = 
                InitializeRemoteBlast(query_factory, db_args, opts_hndl,
                      m_CmdLineArgs->ProduceDebugRemoteOutput(), pssm);
            // FIXME: determine if errors ocurred, if so, return appropriate
            // exit code

            CRef<CSearchResultSet> results = rmt_psiblast->GetResultSet();
            ITERATE(CSearchResultSet, result, *results) {
                formatter.PrintOneResultSet(**result, query);
            }

            SavePssmToFile(rmt_psiblast->GetPSSM());

        } else {

            CPsiBlastIterationState itr(kNumIterations);

            CRef<CPHIBlastProtOptionsHandle> phi_opts;
            CRef<CPSIBlastOptionsHandle> psi_opts;
            int run_token = 0; // 1 means phi-blast, 2 means psi-blast, 3 means both
            if (opt.GetPHIPattern() != NULL)
               run_token += 1;
            if (kNumIterations != 1 || opt.GetPHIPattern() == NULL)
               run_token += 2;

            if (run_token & 1)
            {
               phi_opts.Reset(dynamic_cast<CPHIBlastProtOptionsHandle*>(&*opts_hndl));
               if (run_token & 2)
               {
                  // m_CmdLineArgs->ForcePSIBlast();
                  ((CArgs&)args).Remove(kArgPHIPatternFile);
                  CRef<CBlastOptionsHandle> opts_hndl_2(m_CmdLineArgs->SetOptions(args));
                  CBlastOptions& options = opts_hndl_2->SetOptions();
                  options.SetPHIPattern(NULL, false);
                  options.SetLookupTableType(eAaLookupTable);
                  psi_opts.Reset(dynamic_cast<CPSIBlastOptionsHandle*>(&*opts_hndl_2));
               }
            }
            else if (run_token == 2)  // This branch means only psi-blast, NO phi-blast.
                psi_opts.Reset(dynamic_cast<CPSIBlastOptionsHandle*>(&*opts_hndl));

            CRef<CPsiBlast> psiblast;
            if (run_token & 2)
            {
                _ASSERT(psi_opts.NotEmpty());
                if (query_factory.NotEmpty()) {
                    psiblast.Reset(new CPsiBlast(query_factory, db_adapter, psi_opts));
                } else {
                    _ASSERT(pssm.NotEmpty());
                    psiblast.Reset(new CPsiBlast(pssm, db_adapter, psi_opts));
                }
            	psiblast->SetNumberOfThreads(m_CmdLineArgs->GetNumThreads());
            }

            while (itr) {

                SavePssmToFile(pssm, &itr);

                CRef<CSearchResultSet> results;
                if (run_token & 1 && itr.GetIterationNumber() == 1)
                {
                   CLocalBlast lcl_blast(query_factory, CRef<CBlastOptionsHandle>(phi_opts), db_adapter);
                   lcl_blast.SetNumberOfThreads(m_CmdLineArgs->GetNumThreads());
                   results = lcl_blast.Run();
                   formatter.PrintPhiResult(*results, query, itr.GetIterationNumber(),  itr.GetPreviouslyFoundSeqIds());
                }
                else
                {
                   results = psiblast->Run();
                   ITERATE(CSearchResultSet, result, *results) {
                       formatter.PrintOneResultSet(**result, query,
                                                itr.GetIterationNumber(),
                                                itr.GetPreviouslyFoundSeqIds());
                   }
                }
                // FIXME: what if there are no results!?!

                CSearchResults& results_1st_query = (*results)[0];
                if ( !results_1st_query.HasAlignments() ) {
                    break;
                }

                if (run_token & 2)
                {
                	CConstRef<CSeq_align_set> aln(results_1st_query.GetSeqAlign());
                	CPsiBlastIterationState::TSeqIds ids;
                	CPsiBlastIterationState::GetSeqIds(aln, psi_opts, ids);

                	itr.Advance(ids);

                	if (itr) {
                    		CConstRef<CBioseq> seq =
                       		 s_GetQueryBioseq(query, scope, pssm);
                    		pssm = 
                       		 ComputePssmForNextIteration(*seq, aln, psi_opts, scope,
                                          results_1st_query.GetAncillaryData());
                    		psiblast->SetPssm(pssm);
                	}
                }
                else
                  break;
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

    } CATCH_ALL(status)
    return status;
}

#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
    return CPsiBlastApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
#endif /* SKIP_DOXYGEN_PROCESSING */
