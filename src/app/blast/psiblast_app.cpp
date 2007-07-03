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

    SetupArgDescriptions(m_CmdLineArgs->SetCommandLine());
    HideStdArgs(fHideLogfile | fHideConffile);
}

/// Auxiliary function to create the PSSM for the next iteration
static CRef<CPssmWithParameters>
x_ComputePssmForNextIteration(CRef<CBlastQueryVector> query,
                              CConstRef<CSeq_align_set> sset,
                              CConstRef<CPSIBlastOptionsHandle> opts_handle,
                              CRef<CScope> scope)
{
    CBioseq_Handle bh = scope->GetBioseqHandle(*query->GetQuerySeqLoc(0));
    CBioseq_Handle::TBioseqCore bioseq = bh.GetBioseqCore();
    CPSIDiagnosticsRequest diags(PSIDiagnosticsRequestNew());
    diags->frequency_ratios = true;
    return PsiBlastComputePssmFromAlignment(*bioseq, sset, scope, *opts_handle,
                                            diags);
}

int CPsiBlastApp::Run(void)
{

    int status = 0;

    try {

        // Allow the fasta reader to complain on 
        // invalid sequence input
        SetDiagPostLevel(eDiag_Warning);

        const CArgs& args = GetArgs();

        CRef<CQueryOptionsArgs> query_opts = 
            m_CmdLineArgs->GetQueryOptionsArgs();

        CRef<CBlastOptionsHandle> handle(m_CmdLineArgs->SetOptions(args));
        CRef<CPSIBlastOptionsHandle> opts;
        opts.Reset(dynamic_cast<CPSIBlastOptionsHandle*>(&*handle));
        _ASSERT(opts.NotEmpty());
        const CBlastOptions& opt = opts->GetOptions();

        CBlastInputConfig iconfig(query_opts->GetStrand(),
                                     query_opts->UseLowercaseMasks(),
                                     query_opts->BelieveQueryDefline(),
                                     query_opts->GetRange());
        CBlastFastaInputSource fasta(*m_ObjMgr, m_CmdLineArgs->GetInputStream(),
                                     query_opts->QueryIsProtein(), iconfig);
        CBlastInput input(&fasta, m_CmdLineArgs->GetQueryBatchSize());

        // initialize the database

        CRef<CBlastDatabaseArgs> db_args(m_CmdLineArgs->GetBlastDatabaseArgs());
        CRef<CSeqDB> seqdb = GetSeqDB(db_args);
        CRef<CLocalDbAdapter> db(new CLocalDbAdapter(seqdb));

        const string loader_name = RegisterOMDataLoader(m_ObjMgr, seqdb);
        CRef<CScope> scope(fasta.GetScope());
        scope->AddDataLoader(loader_name); 

        // fill in the blast options, and add a few tweaks now
        // that auxiliary information is known

        // set up the output
        CRef<CFormattingArgs> fmt_args(m_CmdLineArgs->GetFormattingArgs());

        CNcbiOstream& out = m_CmdLineArgs->GetOutputStream();
        CBlastFormat format(opt,
                            db_args->GetDatabaseName(),
                            fmt_args->GetFormattedOutputChoice(),
                            db_args->IsProtein(),
                            query_opts->BelieveQueryDefline(),
                            out,
                            fmt_args->GetNumDescriptions(),
                            opt.GetMatrixName(),
                            fmt_args->ShowGis(),
                            fmt_args->DisplayHtmlOutput(),
                            opt.GetQueryGeneticCode(),
                            opt.GetDbGeneticCode(),
                            opt.GetSumStatisticsMode());

        format.PrintProlog();

        // run the search
        CRef<CBlastQueryVector> query(input.GetAllSeqs());

        CRef<CPssmWithParameters> pssm = m_CmdLineArgs->GetPssm();

        const size_t kNumIterations = m_CmdLineArgs->GetNumberOfIterations();
        CPsiBlastIterationState itr(kNumIterations);

        CRef<CPsiBlast> psiblast;
        if (query.NotEmpty()) {
            if (query->Empty() || query->Size() != 1) {
                NCBI_THROW(CBlastException, eInvalidArgument,
                           "PSI-BLAST only accepts one query");
            }
            CRef<IQueryFactory> query_factory(new CObjMgr_QueryFactory(*query));
            psiblast.Reset(new CPsiBlast(query_factory, db, opts));
        } else {
            _ASSERT(pssm.NotEmpty());
            psiblast.Reset(new CPsiBlast(pssm, db, opts));
        }
        psiblast->SetNumberOfThreads(m_CmdLineArgs->GetNumThreads());

        while (itr) {

            CRef<CSearchResultSet> results = psiblast->Run();
            CConstRef<CSeq_align_set> alignment = (*results)[0].GetSeqAlign();

            format.PrintOneAlignSet((*results)[0], *scope,
                                    itr.GetIterationNumber());

            CPsiBlastIterationState::TSeqIds ids;
            CPsiBlastIterationState::GetSeqIds(alignment, opts, ids);

            itr.Advance(ids);

            if (itr) {
                pssm = x_ComputePssmForNextIteration(query, alignment, opts,
                                                     scope);
                psiblast->SetPssm(pssm);
            }
        }

        // finish up
        format.PrintEpilog(opt);

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
    return CPsiBlastApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
#endif /* SKIP_DOXYGEN_PROCESSING */
