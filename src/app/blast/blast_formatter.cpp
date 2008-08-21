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
 * Author: Christiam Camacho
 *
 */

/** @file blast_formatter.cpp
 * Stand-alone command line formatter for BLAST.
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <serial/iterator.hpp>
#include <algo/blast/api/version.hpp>
#include <algo/blast/api/remote_blast.hpp>
#include <algo/blast/blastinput/blast_input_aux.hpp>
#include "blast_format.hpp"
#include "blast_app_util.hpp"

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
#endif

/// The application class
class CBlastFormatterApp : public CNcbiApplication
{
public:
    /** @inheritDoc */
    CBlastFormatterApp() {
        CRef<CVersion> version(new CVersion());
        version->SetVersionInfo(new CBlastVersion());
        SetFullVersion(version);
    }
private:
    /** @inheritDoc */
    virtual void Init();
    /** @inheritDoc */
    virtual int Run();
    
    /// Prints the BLAST formatted output
    int PrintFormattedOutput(void);

    /// Extracts the queries to be formatted
    /// @param query_is_protein Are the queries protein sequences? [in]
    CRef<CBlastQueryVector> x_ExtractQueries(bool query_is_protein);

    /// Build the query from a PSSM
    /// @param pssm PSSM to inspect [in]
    CRef<CBlastSearchQuery> 
    x_BuildQueryFromPssm(const CPssmWithParameters& pssm);

    /// Package a scope and Seq-loc into a SSeqLoc from a Bioseq
    /// @param bioseq Bioseq to inspect [in]
    /// @param scope Scope object to add the sequence data to [in|out]
    SSeqLoc x_QueryBioseqToSSeqLoc(const CBioseq& bioseq, CRef<CScope> scope);

    /// Our link to the NCBI BLAST service
    CRef<CRemoteBlast> m_RmtBlast;

    /// The source of CScope objects for queries
    CRef<CBlastScopeSource> m_QueryScopeSource;
};

void CBlastFormatterApp::Init()
{
    HideStdArgs(fHideLogfile | fHideConffile | fHideFullVersion | fHideXmlHelp | fHideDryRun);

    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // FIXME: when local formatting is allowed, remove 'remote' from the
    // description below
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(), 
                  "Stand-alone (remote) BLAST formatter client, version " 
                  + CBlastVersion().Print());

    arg_desc->SetCurrentGroup("Input options");
    arg_desc->AddKey("rid", "BLAST_RID", "BLAST Request ID (RID)", 
                     CArgDescriptions::eString);
    // add input file for seq-align here?

    CFormattingArgs fmt_args;
    fmt_args.SetArgumentDescriptions(*arg_desc);

    arg_desc->SetCurrentGroup("Output configuration options");
    arg_desc->AddDefaultKey("out", "output_file", "Output file name", 
                            CArgDescriptions::eOutputFile, "-");

    arg_desc->SetCurrentGroup("Miscellaneous options");
    arg_desc->AddFlag(kArgParseDeflines,
                 "Should the query and subject defline(s) be parsed?", true);
    arg_desc->SetCurrentGroup("");

    CDebugArgs debug_args;
    debug_args.SetArgumentDescriptions(*arg_desc);

    SetupArgDescriptions(arg_desc.release());
}

SSeqLoc
CBlastFormatterApp::x_QueryBioseqToSSeqLoc(const CBioseq& bioseq, 
                                           CRef<CScope> scope)
{
    static bool first_time = true;
    _ASSERT(scope);

    scope->AddBioseq(bioseq);
    if ( !HasRawSequenceData(bioseq) && first_time ) {
        _ASSERT(m_QueryScopeSource);
        m_QueryScopeSource->AddDataLoaders(scope);
        first_time = false;
    }
    CRef<CSeq_loc> seqloc(new CSeq_loc);
    seqloc->SetWhole().Assign(*bioseq.GetFirstId());
    return SSeqLoc(seqloc, scope);
}

CRef<CBlastSearchQuery>
CBlastFormatterApp::x_BuildQueryFromPssm(const CPssmWithParameters& pssm)
{
    if ( !pssm.HasQuery() ) {
        throw runtime_error("PSSM has no query");
    }
    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
    const CSeq_entry& seq_entry = pssm.GetQuery();
    if ( !seq_entry.IsSeq() ) {
        throw runtime_error("Cannot have multiple queries in a PSSM");
    }
    SSeqLoc ssl = x_QueryBioseqToSSeqLoc(seq_entry.GetSeq(), scope);
    CRef<CBlastSearchQuery> retval;
    retval.Reset(new CBlastSearchQuery(*ssl.seqloc, *ssl.scope));
    _ASSERT(ssl.scope.GetPointer() == scope.GetPointer());
    return retval;
}

CRef<CBlastQueryVector>
CBlastFormatterApp::x_ExtractQueries(bool query_is_protein)
{
    CRef<CBlast4_queries> b4_queries = m_RmtBlast->GetQueries();
    _ASSERT(b4_queries);
    const size_t kNumQueries = b4_queries->GetNumQueries();

    CRef<CBlastQueryVector> retval(new CBlastQueryVector);

    SDataLoaderConfig dlconfig(query_is_protein);
    dlconfig.OptimizeForWholeLargeSequenceRetrieval(false);
    m_QueryScopeSource.Reset(new CBlastScopeSource(dlconfig));

    if (b4_queries->IsPssm()) {
        retval->AddQuery(x_BuildQueryFromPssm(b4_queries->GetPssm()));
    } else if (b4_queries->IsSeq_loc_list()) {
        CRef<CScope> scope = m_QueryScopeSource->NewScope();
        ITERATE(CBlast4_queries::TSeq_loc_list, seqloc,
                b4_queries->GetSeq_loc_list()) {
            _ASSERT( !(*seqloc)->GetId()->IsLocal() );
            CRef<CBlastSearchQuery> query(new CBlastSearchQuery(**seqloc,
                                                                *scope));
            retval->AddQuery(query);
        }
    } else if (b4_queries->IsBioseq_set()) {
        CTypeConstIterator<CBioseq> itr(ConstBegin(b4_queries->GetBioseq_set(),
                                                   eDetectLoops));
        CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
        for (; itr; ++itr) {
            SSeqLoc ssl = x_QueryBioseqToSSeqLoc(*itr, scope);
            CRef<CBlastSearchQuery> query(new CBlastSearchQuery(*ssl.seqloc,
                                                                *ssl.scope));
            retval->AddQuery(query);
        }
    }

    _ASSERT(kNumQueries == retval->size());
    return retval;
}

int CBlastFormatterApp::PrintFormattedOutput(void)
{
    int retval = 0;
    const CArgs& args = GetArgs();
    const string& kRid = args["rid"].AsString();
    CNcbiOstream& out = args["out"].AsOutputFile();
    CFormattingArgs fmt_args;

    CRef<CBlastOptionsHandle> opts_handle = m_RmtBlast->GetSearchOptions();
    CBlastOptions& opts = opts_handle->SetOptions();
    fmt_args.ExtractAlgorithmOptions(args, opts);
    {{
        CDebugArgs debug_args;
        debug_args.ExtractAlgorithmOptions(args, opts);
        if (debug_args.ProduceDebugOutput()) {
            opts.DebugDumpText(NcbiCerr, "BLAST options", 1);
        }
    }}


    const EBlastProgramType p = opts.GetProgramType();
    CRef<CBlastQueryVector> queries = x_ExtractQueries(Blast_QueryIsProtein(p));
    CRef<CScope> scope = queries->GetScope(0);
    _ASSERT(queries);

    CRef<CBlast4_database> db = m_RmtBlast->GetDatabases();
    _ASSERT(db);
    vector<int> filtering_algorithms;
    {
        const list<int>& tmp = m_RmtBlast->GetDbFilteringAlgorithmIds();
        copy(tmp.begin(), tmp.end(), back_inserter(filtering_algorithms));
    }

    const string kTask = m_RmtBlast->GetTask();

    SDataLoaderConfig dlconfig(db->GetName(), db->IsProtein());
    dlconfig.OptimizeForWholeLargeSequenceRetrieval(false);
    CBlastScopeSource scope_source(dlconfig);
    CRef<CScope> subj_scope = scope_source.NewScope();
    scope->AddScope(*subj_scope,
                    CBlastDatabaseArgs::kSubjectsDataLoaderPriority);

    const CSearchDatabase search_db(db->GetName(), db->IsProtein()
                              ? CSearchDatabase::eBlastDbIsProtein
                              : CSearchDatabase::eBlastDbIsNucleotide);
    CLocalDbAdapter db_adapter(search_db);

    CBlastFormat formatter(opts, db_adapter,
                           fmt_args.GetFormattedOutputChoice(),
                           static_cast<bool>(args[kArgParseDeflines]),
                           out,
                           fmt_args.GetNumDescriptions(),
                           fmt_args.GetNumAlignments(),
                           *scope,
                           opts.GetMatrixName(),
                           fmt_args.ShowGis(),
                           fmt_args.DisplayHtmlOutput(),
                           opts.GetQueryGeneticCode(),
                           opts.GetDbGeneticCode(),
                           opts.GetSumStatisticsMode(),
                           !kRid.empty(),
                           filtering_algorithms,
                           fmt_args.GetCustomOutputFormatSpec(),
                           kTask == "megablast",
                           opts.GetMBIndexLoaded());
    CRef<CSearchResultSet> results = m_RmtBlast->GetResultSet();
    formatter.PrintProlog();
    ITERATE(CSearchResultSet, result, *results) {
        formatter.PrintOneResultSet(**result, queries);
    }
    formatter.PrintEpilog(opts);
    return retval;
}

#define EXIT_CODE__UNKNOWN_RID 1
#define EXIT_CODE__SEARCH_PENDING 2
#define EXIT_CODE__SEARCH_FAILED 3

int CBlastFormatterApp::Run(void)
{
    int status = 0;
    const CArgs& args = GetArgs();

    try {
        const string& kRid = args["rid"].AsString();

        m_RmtBlast.Reset(new CRemoteBlast(kRid));
        {{
            CDebugArgs debug_args;
            CBlastOptions dummy_options;
            debug_args.ExtractAlgorithmOptions(args, dummy_options);
            if (debug_args.ProduceDebugRemoteOutput()) {
                m_RmtBlast->SetVerbose();
            }
        }}

        switch (m_RmtBlast->CheckStatus()) {
        case CRemoteBlast::eStatus_Unknown:
            cerr << "Unknown RID '" << kRid << "'." << endl;
            status = EXIT_CODE__UNKNOWN_RID;
            break;

        case CRemoteBlast::eStatus_Done:
            status = PrintFormattedOutput();
            break;

        case CRemoteBlast::eStatus_Pending:
            cerr << "RID '" << kRid << "' is still pending." << endl;
            status = EXIT_CODE__SEARCH_PENDING;
            break;

        case CRemoteBlast::eStatus_Failed:
            cerr << "RID '" << kRid << "' has failed" << endl;
            status = EXIT_CODE__SEARCH_FAILED;
            break;
           
        default:
            abort();
        }

    } CATCH_ALL(status)
    return status;
}


#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
    return CBlastFormatterApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
#endif /* SKIP_DOXYGEN_PROCESSING */
