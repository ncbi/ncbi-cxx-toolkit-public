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
 * Authors:  Greg Boratyn
 *
 */

/** @file deltablast_app.cpp
 * DELTA-BLAST command line application
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
        "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbiapp.hpp>
#include <algo/blast/api/deltablast.hpp>
#include <algo/blast/api/psiblast.hpp> // needed for psiblast iterations
#include <algo/blast/api/remote_blast.hpp>
#include <algo/blast/api/blast_results.hpp>
#include <algo/blast/blastinput/blast_fasta_input.hpp>
#include <algo/blast/blastinput/deltablast_args.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/blast/api/query_data.hpp>
#include <algo/blast/format/blast_format.hpp>
#include <objects/scoremat/Pssm.hpp> // needed for printing Pssm
#include <objects/scoremat/PssmIntermediateData.hpp> // needed for clearing
                                       // information content in ascii Pssm
#include <objects/seq/Seq_descr.hpp> // needed for adding qurey title to Pssm
#include "blast_app_util.hpp"

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(blast);
#endif

class CDeltaBlastApp : public CNcbiApplication
{
public:
    /** @inheritDoc */
    CDeltaBlastApp() {
        CRef<CVersion> version(new CVersion());
        version->SetVersionInfo(new CBlastVersion());
        SetFullVersion(version);
    }
private:
    /** @inheritDoc */
    virtual void Init();
    /** @inheritDoc */
    virtual int Run();


    /// Save Pssm to file
    void SavePssmToFile(CConstRef<CPssmWithParameters> pssm);

    /// Do PSI-BLAST iterations follwing DELTA-BLAST
    bool DoPsiBlastIterations(CRef<CBlastOptionsHandle> opts_hndl,
                              CRef<CBlastQueryVector> query,
                              CConstRef<CBioseq> query_bioseq,
                              CRef<blast::CSearchResultSet> results,
                              CRef<CBlastDatabaseArgs> db_args,
                              const CArgs& args,
                              CRef<CLocalDbAdapter> db_adapter,
                              CRef<CScope> scope,
                              CBlastFormat& formatter);

    /// Compute PSSM for next PSI-BLAST iteration
    CRef<CPssmWithParameters> ComputePssmForNextPsiBlastIteration(
                                 const CBioseq& bioseq,
                                 CConstRef<CSeq_align_set> sset,
                                 CConstRef<CPSIBlastOptionsHandle> opts_handle,
                                 CRef<CScope> scope,
                                 CRef<CBlastAncillaryData> ancillary_data);


    /// This application's command line args
    CRef<CDeltaBlastAppArgs> m_CmdLineArgs;

    CRef<CBlastAncillaryData> m_AncillaryData;
};

void CDeltaBlastApp::Init()
{
    // formulate command line arguments

    m_CmdLineArgs.Reset(new CDeltaBlastAppArgs());

    // read the command line

    HideStdArgs(fHideLogfile | fHideConffile | fHideFullVersion | fHideXmlHelp
                | fHideDryRun);
    SetupArgDescriptions(m_CmdLineArgs->SetCommandLine());
}

void
CDeltaBlastApp::SavePssmToFile(CConstRef<CPssmWithParameters> pssm)
{
    if (pssm.Empty()) {
        return;
    }

    if (m_CmdLineArgs->SaveCheckpoint()) {
        *m_CmdLineArgs->GetCheckpointStream() << MSerial_AsnText << *pssm;
    }

    if (m_CmdLineArgs->SaveAsciiPssm()) {
        if (m_AncillaryData.Empty() && pssm.NotEmpty()) {
            m_AncillaryData = ExtractPssmAncillaryData(*pssm);
        }

        CBlastFormatUtil::PrintAsciiPssm(*pssm, 
                                         m_AncillaryData,
                                         *m_CmdLineArgs->GetAsciiPssmStream());
    }
}


// Add query sequence title from scope to computed Pssm
static void s_AddSeqTitleToPssm(CRef<CPssmWithParameters> pssm,
                                CRef<CBlastQueryVector> query_batch,
                                CRef<CScope> scope)
{
    CConstRef<CSeq_id> query_id =
        query_batch->GetBlastSearchQuery(0)->GetQueryId();

    CBioseq_Handle bhandle = scope->GetBioseqHandle(*query_id);
    CConstRef<CBioseq> scope_bioseq = bhandle.GetCompleteBioseq();

    if (scope_bioseq->IsSetDescr()) {

        CBioseq& pssm_bioseq = pssm->SetQuery().SetSeq();
        ITERATE (CSeq_descr::Tdata, it, scope_bioseq->GetDescr().Get()) {
            pssm_bioseq.SetDescr().Set().push_back(*it);
        }
    }
}

// Add sequence data to pssm query
static void s_AddSeqDataToPssm(CRef<CPssmWithParameters> pssm,
                               CRef<CBlastQueryVector> query_batch,
                               CRef<CScope> scope)
{
    CConstRef<CSeq_id> query_id =
        query_batch->GetBlastSearchQuery(0)->GetQueryId();

    // first make sure that query id and pssm query id are the same
    if (!pssm->GetPssm().GetQuery().GetSeq().GetFirstId()->Match(*query_id)) {
        NCBI_THROW(CException, eInvalid, "Query and PSSM sequence ids do not "
                   "match");
    }
    
    CBioseq_Handle bhandle = scope->GetBioseqHandle(*query_id);
    CConstRef<CBioseq> scope_bioseq = bhandle.GetCompleteBioseq();

    // set sequence data only if query bioseq has them and pssm does not
    if (scope_bioseq->GetInst().IsSetSeq_data()
        && !pssm->GetPssm().GetQuery().GetSeq().GetInst().IsSetSeq_data()) {
        const CSeq_data& seq_data = scope_bioseq->GetInst().GetSeq_data();
        pssm->SetQuery().SetSeq().SetInst().SetSeq_data(
                                          const_cast<CSeq_data&>(seq_data));
    }
}

int CDeltaBlastApp::Run(void)
{
    int status = BLAST_EXIT_SUCCESS;

    try {

        // Allow the fasta reader to complain on invalid sequence input
        SetDiagPostLevel(eDiag_Warning);

        /*** Get the BLAST options ***/
        const CArgs& args = GetArgs();
        RecoverSearchStrategy(args, m_CmdLineArgs);
        CRef<CBlastOptionsHandle> opts_hndl(&*m_CmdLineArgs->SetOptions(args));
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
        iconfig.SetQueryLocalIdMode();
        if(IsIStreamEmpty(m_CmdLineArgs->GetInputStream()))
           	NCBI_THROW(CArgException, eNoValue, "Query is Empty!");
        CBlastFastaInputSource fasta(m_CmdLineArgs->GetInputStream(), iconfig);
        size_t query_batch_size = m_CmdLineArgs->GetQueryBatchSize();
        if (m_CmdLineArgs->GetNumberOfPsiBlastIterations() > 1
            || m_CmdLineArgs->ExecuteRemotely()) {

            query_batch_size = 1;
        }
        CBlastInput input(&fasta, query_batch_size);

        /*** Initialize the domain database ***/
        CRef<CLocalDbAdapter> domain_db_adapter(new CLocalDbAdapter(
                                    *m_CmdLineArgs->GetDomainDatabase()));
        _ASSERT(domain_db_adapter);
        CLocalDbAdapter* domain_db_ptr = NULL;

        // domain database does not need to be loaded into scope unless
        // domain search results are requested
        if (m_CmdLineArgs->GetShowDomainHits()) {
            CRef<CSeqDB> seqdb(new CSeqDB(domain_db_adapter->GetDatabaseName(),
                                          CSeqDB::eProtein));
            scope->AddDataLoader(RegisterOMDataLoader(seqdb),
                          CBlastDatabaseArgs::kSubjectsDataLoaderPriority - 1);

            domain_db_ptr = domain_db_adapter.GetNonNullPointer();
        }

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
                               fmt_args->GetCustomOutputFormatSpec(),
                               false, false, NULL,
                               domain_db_ptr);
        
        formatter.SetQueryRange(query_opts->GetRange());
        formatter.PrintProlog();

        /*** Process the input ***/
        for (; !input.End(); formatter.ResetScopeHistory()) {

            CRef<CBlastQueryVector> query_batch(input.GetNextSeqBatch(*scope));
            CRef<blast::IQueryFactory> queries(
                                     new CObjMgr_QueryFactory(*query_batch));

            SaveSearchStrategy(args, m_CmdLineArgs, queries, opts_hndl);

            CRef<blast::CSearchResultSet> results;
            CRef<blast::CSearchResultSet> domain_results;

            CRef<CDeltaBlast> deltablast;
            CRef<CPssmWithParameters> pssm;

            if (m_CmdLineArgs->ExecuteRemotely()) {

                // Remote BLAST

                CRef<CRemoteBlast> rmt_blast = 
                    InitializeRemoteBlast(queries, db_args, opts_hndl,
                          m_CmdLineArgs->ProduceDebugRemoteOutput(),
                          m_CmdLineArgs->GetClientId());
                results = rmt_blast->GetResultSet();
                pssm = rmt_blast->GetPSSM();
            } else {

                // Run locally

                CRef<CDeltaBlastOptionsHandle> delta_opts(
                        dynamic_cast<CDeltaBlastOptionsHandle*>(&*opts_hndl));

                deltablast.Reset(new CDeltaBlast(queries, db_adapter,
                                                 domain_db_adapter,
                                                 delta_opts));
                deltablast->SetNumberOfThreads(m_CmdLineArgs->GetNumThreads());
                results = deltablast->Run();
                domain_results = deltablast->GetDomainResults();
                pssm = deltablast->GetPssm();
            }

            // deltablast computed pssm does not have query title, so
            // it must be added if pssm is requested
            if (m_CmdLineArgs->SaveCheckpoint()
                || fmt_args->GetFormattedOutputChoice()
                == CFormattingArgs::eArchiveFormat) {

                s_AddSeqTitleToPssm(pssm, query_batch, scope);
            }

            // remote blast remves sequence data from pssm for known ids
            // the data must be added if pssm is requested after remote search
            if (m_CmdLineArgs->ExecuteRemotely()
                && (m_CmdLineArgs->SaveCheckpoint()
                    || m_CmdLineArgs->SaveAsciiPssm())) {

                s_AddSeqDataToPssm(pssm, query_batch, scope);
            }

            // only one PSI-BLAST iteration requested, then print results
            // (the first PIS-BLAST iteration is done by DELTA-BLAST)
            if (m_CmdLineArgs->GetNumberOfPsiBlastIterations() == 1) {

                SavePssmToFile(pssm);

                blast::CSearchResultSet::const_iterator domain_it;
                if (m_CmdLineArgs->GetShowDomainHits()) {
                    domain_it = domain_results->begin();
                }

                if (fmt_args->ArchiveFormatRequested(args)) {
                    formatter.WriteArchive(*queries, *opts_hndl, *results);
                } else {
                    BlastFormatter_PreFetchSequenceData(*results, scope);
                    if (m_CmdLineArgs->GetShowDomainHits()) {
                        BlastFormatter_PreFetchSequenceData(*results, scope);
                    }
                    ITERATE(blast::CSearchResultSet, result, *results) {
                        if (m_CmdLineArgs->GetShowDomainHits()) {
                            _ASSERT(domain_it != domain_results->end());
                            formatter.PrintOneResultSet(**domain_it,
                                  query_batch,
                                  numeric_limits<unsigned int>::max(),
                                  blast::CPsiBlastIterationState::TSeqIds(),
                                  true);
                            ++domain_it;
                        }
                        formatter.PrintOneResultSet(**result, query_batch);
                    }
                }
            }
            else {

                // if more than 1 iterations are requested, then
                // do PSI-BLAST iterations, this is not allowed for remote blast

                // print domain search results if requested
                // query_batch_size == 1 if number of iteratins > 1
                if (m_CmdLineArgs->GetShowDomainHits()) {
                    ITERATE (blast::CSearchResultSet, result,
                             *deltablast->GetDomainResults()) {

                        formatter.PrintOneResultSet(**result, query_batch,
                                    numeric_limits<unsigned int>::max(),
                                    blast::CPsiBlastIterationState::TSeqIds(),
                                    true);
                    }
                }

                // use pssm variable here, because it will contains query title
                CConstRef<CBioseq> query_bioseq(&pssm->GetQuery().GetSeq());

                bool retval = DoPsiBlastIterations(opts_hndl,
                                                   query_batch,
                                                   query_bioseq,
                                                   results,
                                                   db_args,
                                                   args,
                                                   db_adapter,
                                                   scope,
                                                   formatter);

                if (retval && !fmt_args->HasStructuredOutputFormat()
                    && fmt_args->GetFormattedOutputChoice()
                    != CFormattingArgs::eArchiveFormat) {

                    m_CmdLineArgs->GetOutputStream() << NcbiEndl
                                                     << "Search has CONVERGED!"
                                                     << NcbiEndl;
                    }
                    // Reset for next query sequence.
                    m_AncillaryData.Reset();
            }
        }

        formatter.PrintEpilog(opt);

        if (m_CmdLineArgs->ProduceDebugOutput()) {
            opts_hndl->GetOptions().DebugDumpText(NcbiCerr, "BLAST options", 1);
        }

    } CATCH_ALL(status)
    return status;
}

// This is a simplified version of CPsiBlastApp::DoIterations()
bool 
CDeltaBlastApp::DoPsiBlastIterations(CRef<CBlastOptionsHandle> opts_hndl,
                                     CRef<CBlastQueryVector> query,
                                     CConstRef<CBioseq> query_bioseq,
                                     CRef<blast::CSearchResultSet> results,
                                     CRef<CBlastDatabaseArgs> db_args,
                                     const CArgs& args,
                                     CRef<CLocalDbAdapter> db_adapter,
                                     CRef<CScope> scope,
                                     CBlastFormat& formatter)
{
    bool converged = false;

    const size_t kNumIterations = m_CmdLineArgs->GetNumberOfPsiBlastIterations();


    CPsiBlastIterationState itr(kNumIterations);
    CRef<CPSIBlastOptionsHandle> psi_opts;


    psi_opts.Reset(dynamic_cast<CPSIBlastOptionsHandle*>(&*opts_hndl));
    CRef<CPsiBlast> psiblast;

    CRef<IQueryFactory> query_factory(new CObjMgr_QueryFactory(*query));

    BlastFormatter_PreFetchSequenceData(*results, scope);
    if (CFormattingArgs::eArchiveFormat ==
        m_CmdLineArgs->GetFormattingArgs()->GetFormattedOutputChoice()) {
        formatter.WriteArchive(*query_factory, *opts_hndl, *results,
                               itr.GetIterationNumber());
    }
    else {
        ITERATE(blast::CSearchResultSet, result, *results) {
            formatter.PrintOneResultSet(**result, query,
                                        itr.GetIterationNumber(),
                                        itr.GetPreviouslyFoundSeqIds());
        }
    }
    // FIXME: what if there are no results!?!
        
    blast::CSearchResults& results_1st_query = (*results)[0];
    if ( !results_1st_query.HasAlignments() ) {
        return false;
    }

    CConstRef<CSeq_align_set> aln(results_1st_query.GetSeqAlign());
    CPsiBlastIterationState::TSeqIds ids;
    CPsiBlastIterationState::GetSeqIds(aln, psi_opts, ids);

    itr.Advance(ids);

    while (itr) {

        CRef<CPssmWithParameters>
            pssm = ComputePssmForNextPsiBlastIteration(*query_bioseq, aln,
                                       psi_opts,
                                       scope,
                                       results_1st_query.GetAncillaryData());

        if (psiblast.Empty()) {
            psiblast.Reset(new CPsiBlast(pssm, db_adapter, psi_opts));
        }
        else {
            psiblast->SetPssm(pssm);
        }

        SavePssmToFile(pssm);

        psiblast->SetNumberOfThreads(m_CmdLineArgs->GetNumThreads());
        results = psiblast->Run();

        BlastFormatter_PreFetchSequenceData(*results, scope);
        if (CFormattingArgs::eArchiveFormat ==
            m_CmdLineArgs->GetFormattingArgs()->GetFormattedOutputChoice()) {
            formatter.WriteArchive(*pssm, *opts_hndl, *results,
                                   itr.GetIterationNumber());
        }
        else {
            ITERATE(blast::CSearchResultSet, result, *results) {
                formatter.PrintOneResultSet(**result, query,
                                            itr.GetIterationNumber(),
                                            itr.GetPreviouslyFoundSeqIds());
            }
        }
        // FIXME: what if there are no results!?!

        blast::CSearchResults& results_1st_query = (*results)[0];
        if ( !results_1st_query.HasAlignments() ) {
            break;
        }

        CConstRef<CSeq_align_set> aln(results_1st_query.GetSeqAlign());
        CPsiBlastIterationState::TSeqIds ids;
        CPsiBlastIterationState::GetSeqIds(aln, psi_opts, ids);

        itr.Advance(ids);
    }
    if (itr.HasConverged()) {
        converged = true;
    }

    return converged;
}

CRef<CPssmWithParameters>
CDeltaBlastApp::ComputePssmForNextPsiBlastIteration(const CBioseq& bioseq,
                            CConstRef<CSeq_align_set> sset,
                            CConstRef<CPSIBlastOptionsHandle> opts_handle,
                            CRef<CScope> scope,
                            CRef<CBlastAncillaryData> ancillary_data)
{
    CPSIDiagnosticsRequest
        diags(PSIDiagnosticsRequestNewEx(m_CmdLineArgs->SaveAsciiPssm()));
    
    m_AncillaryData = ancillary_data;
    return PsiBlastComputePssmFromAlignment(bioseq, sset, scope, *opts_handle,
                                            m_AncillaryData, diags);
}


#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
    return CDeltaBlastApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
#endif /* SKIP_DOXYGEN_PROCESSING */


