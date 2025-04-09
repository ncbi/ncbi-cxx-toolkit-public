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

/** @file blast_app_util.cpp
 *  Utility functions for BLAST command line applications
 */

#include <ncbi_pch.hpp>
#include "blast_app_util.hpp"

#include <serial/serial.hpp>
#include <serial/objostr.hpp>

#include <objtools/data_loaders/blastdb/bdbloader.hpp>
#include <algo/blast/api/remote_blast.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>     // for CObjMgr_QueryFactory
#include <algo/blast/api/blast_options_builder.hpp>
#include <algo/blast/api/search_strategy.hpp>
#include <algo/blast/blastinput/blast_input.hpp>    // for CInputException
#include <algo/blast/blastinput/psiblast_args.hpp>
#include <algo/blast/blastinput/tblastn_args.hpp>
#include <algo/blast/blastinput/blast_scope_src.hpp>
#include <objmgr/util/sequence.hpp>

#include <objects/scoremat/Pssm.hpp>
#include <serial/typeinfo.hpp>      // for CTypeInfo, needed by SerialClone
#include <objtools/data_loaders/blastdb/bdbloader_rmt.hpp>
#include <algo/blast/format/blast_format.hpp>
#include <objtools/align_format/format_flags.hpp>

#if defined(NCBI_OS_LINUX) && HAVE_MALLOC_H
#include <malloc.h>
#endif

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
USING_SCOPE(blast);

Int4 CBatchSizeMixer::GetBatchSize(Int4 hits) 
{
     if (hits >= 0) {
         double ratio = 1.0 * (hits+1) / m_BatchSize;
         m_Ratio = (m_Ratio < 0) ? ratio 
                 : k_MixIn * ratio + (1.0 - k_MixIn) * m_Ratio;
         m_BatchSize = (Int4) (1.0 * m_TargetHits / m_Ratio);
     } 
     if (m_BatchSize > k_MaxBatchSize) {
         m_BatchSize = k_MaxBatchSize;
         m_Ratio = -1.0;  
     } else if (m_BatchSize < k_MinBatchSize) {
         m_BatchSize = k_MinBatchSize;
         m_Ratio = -1.0; 
     }
     return m_BatchSize;
}

CRef<blast::CRemoteBlast> 
InitializeRemoteBlast(CRef<blast::IQueryFactory> queries,
                      CRef<blast::CBlastDatabaseArgs> db_args,
                      CRef<blast::CBlastOptionsHandle> opts_hndl,
                      bool verbose_output,
                      const string& client_id /* = kEmptyStr */,
                      CRef<objects::CPssmWithParameters> pssm 
                        /* = CRef<objects::CPssmWithParameters>() */)
{
    _ASSERT(queries || pssm);
    _ASSERT(db_args);
    _ASSERT(opts_hndl);

    CRef<CRemoteBlast> retval;

    CRef<CSearchDatabase> search_db = db_args->GetSearchDatabase();
    if (search_db.NotEmpty()) {
        if (pssm.NotEmpty()) {
            _ASSERT(queries.Empty());
            retval.Reset(new CRemoteBlast(pssm, opts_hndl, *search_db));
        } else {
            retval.Reset(new CRemoteBlast(queries, opts_hndl, *search_db));
        }
    } else {
        if (pssm.NotEmpty()) {
            NCBI_THROW(CInputException, eInvalidInput,
                       "Remote PSI-BL2SEQ is not supported");
        } else {
            // N.B.: there is NO scope needed in the GetSubjects call because
            // the subjects (if any) should have already been added in 
            // InitializeSubject 
            retval.Reset(new CRemoteBlast(queries, opts_hndl,
                                         db_args->GetSubjects()));
        }
    }
    if (verbose_output) {
        retval->SetVerbose();
    }
    if (client_id != kEmptyStr) {
        retval->SetClientId(client_id);
    }
    return retval;
}

blast::SDataLoaderConfig 
InitializeQueryDataLoaderConfiguration(bool query_is_protein, 
                                       CRef<blast::CLocalDbAdapter> db_adapter)
{
    SDataLoaderConfig retval(query_is_protein);
    retval.OptimizeForWholeLargeSequenceRetrieval();

    /* Load the BLAST database into the data loader configuration for the query
     * so that if the query sequence(s) are specified as seq-ids, these can be 
     * fetched from the BLAST database being searched */
    if (db_adapter->IsBlastDb() &&  /* this is a BLAST database search */
        retval.m_UseBlastDbs &&   /* the BLAST database data loader is requested */
        (query_is_protein == db_adapter->IsProtein())) { /* the same database type is used for both queries and subjects */
        // Make sure we don't add the same database more than once
        vector<string> default_dbs;
        NStr::Split(retval.m_BlastDbName, " ", default_dbs);
        if (default_dbs.size() &&
            (find(default_dbs.begin(), default_dbs.end(),
                 db_adapter->GetDatabaseName()) == default_dbs.end())) {
            CNcbiOstrstream oss;
            oss << db_adapter->GetDatabaseName() << " " << retval.m_BlastDbName;
            retval.m_BlastDbName = CNcbiOstrstreamToString(oss);
        }
    }
    if (retval.m_UseBlastDbs) {
        _TRACE("Initializing query data loader to '" << retval.m_BlastDbName 
               << "' (" << (query_is_protein ? "protein" : "nucleotide") 
               << " BLAST database)");
    }
    if (retval.m_UseGenbank) {
        _TRACE("Initializing query data loader to use GenBank data loader");
    }
    return retval;
}

void
InitializeSubject(CRef<blast::CBlastDatabaseArgs> db_args, 
                  CRef<blast::CBlastOptionsHandle> opts_hndl,
                  bool is_remote_search,
                  CRef<blast::CLocalDbAdapter>& db_adapter, 
                  CRef<objects::CScope>& scope)
{
	string dl = kEmptyStr;
    db_adapter.Reset();

    _ASSERT(db_args.NotEmpty());
    CRef<CSearchDatabase> search_db = db_args->GetSearchDatabase();

    // Initialize the scope... 
    if (is_remote_search) {
        const bool is_protein = 
            Blast_SubjectIsProtein(opts_hndl->GetOptions().GetProgramType())
			? true : false;
        CRef<CBlastScopeSource> scope_src;
        if (search_db.Empty()) {
        	SDataLoaderConfig config(is_protein);
        	scope_src.Reset(new CBlastScopeSource(config));
        }
        else {
        	SDataLoaderConfig config(search_db->GetDatabaseName(), is_protein);
        	scope_src.Reset(new CBlastScopeSource(config));
        }
        // configure scope to fetch sequences remotely for formatting
        if (scope.NotEmpty()) {
            scope_src->AddDataLoaders(scope);
        } else {
            scope = scope_src->NewScope();
        }
    } else {
        if (scope.Empty()) {
            scope.Reset(new CScope(*CObjectManager::GetInstance()));
        }
    }
    _ASSERT(scope.NotEmpty());

    // ... and then the subjects
    CRef<IQueryFactory> subjects;
    if ( (subjects = db_args->GetSubjects(scope)) ) {
        _ASSERT(search_db.Empty());
	char* bl2seq_legacy = getenv("BL2SEQ_LEGACY");
	if (bl2seq_legacy)
        	db_adapter.Reset(new CLocalDbAdapter(subjects, opts_hndl, false));
	else
        	db_adapter.Reset(new CLocalDbAdapter(subjects, opts_hndl, true));
    } else {
        _ASSERT(search_db.NotEmpty());
        try { 
            // Try to open the BLAST database even for remote searches, as if
            // it is available locally, it will be better to fetch the
            // sequence data for formatting from this (local) source
            CRef<CSeqDB> seqdb = search_db->GetSeqDb();
            db_adapter.Reset(new CLocalDbAdapter(*search_db));
            dl = RegisterOMDataLoader(seqdb);
            scope->AddDataLoader(dl);
        } catch (const CSeqDBException&) {
            // The BLAST database couldn't be found, report this for local
            // searches, but for remote searches go on.
        	dl = kEmptyStr;
            if (is_remote_search ) {
                db_adapter.Reset(new CLocalDbAdapter(*search_db));
            } else {
                throw;
            }
        }
    }

    /// Set the BLASTDB data loader as the default data loader (if applicable)
    if (search_db.NotEmpty()) {
        if ( dl != kEmptyStr) {
            // FIXME: will this work with multiple BLAST DBs?
            scope->AddDataLoader(dl,  CBlastDatabaseArgs::kSubjectsDataLoaderPriority);
            _TRACE("Setting " << dl << " priority to "
                   << (int)CBlastDatabaseArgs::kSubjectsDataLoaderPriority
                   << " for subjects");
        }
    }
    return;
}

string RegisterOMDataLoader(CRef<CSeqDB> db_handle)
{
    // the blast formatter requires that the database coexist in
    // the same scope with the query sequences
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CBlastDbDataLoader::RegisterInObjectManager(*om, db_handle, true,
                        CObjectManager::eDefault,
                        CBlastDatabaseArgs::kSubjectsDataLoaderPriority);
    CBlastDbDataLoader::SBlastDbParam param(db_handle);
    string retval(CBlastDbDataLoader::GetLoaderNameFromArgs(param));
    _TRACE("Registering " << retval << " at priority " <<
           (int)CBlastDatabaseArgs::kSubjectsDataLoaderPriority 
           << " for subjects");
    return retval;
}


static CRef<blast::CExportStrategy>
s_InitializeExportStrategy(CRef<blast::IQueryFactory> queries,
                      	 CRef<blast::CBlastDatabaseArgs> db_args,
                      	 CRef<blast::CBlastOptionsHandle> opts_hndl,
                      	 const string& client_id /* = kEmptyStr */,
                      	 CRef<objects::CPssmWithParameters> pssm
                         /* = CRef<objects::CPssmWithParameters>() */,
                         unsigned int num_iters
                         /* = 0 */)
{
    _ASSERT(queries || pssm);
    _ASSERT(db_args);
    _ASSERT(opts_hndl);

    CRef<CExportStrategy> retval;

    CRef<CSearchDatabase> search_db = db_args->GetSearchDatabase();
    if (search_db.NotEmpty())
    {
        if (pssm.NotEmpty())
        {
            _ASSERT(queries.Empty());
            if(num_iters != 0)
            	retval.Reset(new blast::CExportStrategy(pssm, opts_hndl, search_db, client_id, num_iters));
            else
            	retval.Reset(new blast::CExportStrategy(pssm, opts_hndl, search_db, client_id));
        }
        else
        {
            if(num_iters != 0)
            	retval.Reset(new blast::CExportStrategy(queries, opts_hndl, search_db, client_id, num_iters));
            else
            	retval.Reset(new blast::CExportStrategy(queries, opts_hndl, search_db, client_id));
        }
    }
    else
    {
        if (pssm.NotEmpty())
        {
            NCBI_THROW(CInputException, eInvalidInput,
                       "Remote PSI-BL2SEQ is not supported");
        }
        else
        {
            retval.Reset(new blast::CExportStrategy(queries, opts_hndl,
            								 db_args->GetSubjects(), client_id));
        }
    }

    return retval;
}


/// Real implementation of search strategy extraction
/// @todo refactor this code so that it can be reused in other contexts
static void
s_ExportSearchStrategy(CNcbiOstream* out,
                     CRef<blast::IQueryFactory> queries,
                     CRef<blast::CBlastOptionsHandle> options_handle,
                     CRef<blast::CBlastDatabaseArgs> db_args,
                     CRef<objects::CPssmWithParameters> pssm,
                       /* = CRef<objects::CPssmWithParameters>() */
                     unsigned int num_iters /* = 0 */)
{
    if ( !out )
        return;

    _ASSERT(db_args);
    _ASSERT(options_handle);

    try
    {
        CRef<CExportStrategy> export_strategy =
        			s_InitializeExportStrategy(queries, db_args, options_handle,
                                  	 	 	   kEmptyStr, pssm, num_iters);
        export_strategy->ExportSearchStrategy_ASN1(out);
    }
    catch (const CBlastException& e)
    {
        if (e.GetErrCode() == CBlastException::eNotSupported) {
            NCBI_THROW(CInputException, eInvalidInput, 
                       "Saving search strategies with gi lists is currently "
                       "not supported");
        }
        throw;
    }
}

/// Converts a list of Bioseqs into a TSeqLocVector. All Bioseqs are added to
/// the same CScope object
/// @param subjects Bioseqs to convert
static TSeqLocVector
s_ConvertBioseqs2TSeqLocVector(const CBlast4_subject::TSequences& subjects)
{
    TSeqLocVector retval;
    CRef<CScope> subj_scope(new CScope(*CObjectManager::GetInstance()));
    ITERATE(CBlast4_subject::TSequences, bioseq, subjects) {
        subj_scope->AddBioseq(**bioseq);
        CRef<CSeq_id> seqid = FindBestChoice((*bioseq)->GetId(),
                                             CSeq_id::BestRank);
        const TSeqPos length = (*bioseq)->GetInst().GetLength();
        CRef<CSeq_loc> sl(new CSeq_loc(*seqid, 0, length-1));
        retval.push_back(SSeqLoc(sl, subj_scope));
    }
    return retval;
}

/// Import PSSM into the command line arguments object
static void 
s_ImportPssm(const CBlast4_queries& queries,
             CRef<blast::CBlastOptionsHandle> opts_hndl,
             blast::CBlastAppArgs* cmdline_args)
{
    CRef<CPssmWithParameters> pssm
        (const_cast<CPssmWithParameters*>(&queries.GetPssm()));
    CPsiBlastAppArgs* psi_args = NULL;
    CTblastnAppArgs* tbn_args = NULL;

    if ( (psi_args = dynamic_cast<CPsiBlastAppArgs*>(cmdline_args)) ) {
        psi_args->SetInputPssm(pssm);
    } else if ( (tbn_args = 
                 dynamic_cast<CTblastnAppArgs*>(cmdline_args))) {
        tbn_args->SetInputPssm(pssm);
    } else {
        EBlastProgramType p = opts_hndl->GetOptions().GetProgramType();
        string msg("PSSM found in saved strategy, but not supported ");
        msg += "for " + Blast_ProgramNameFromType(p);
        NCBI_THROW(CBlastException, eNotSupported, msg);
    }
}

/// Import queries into command line arguments object
static void 
s_ImportQueries(const CBlast4_queries& queries,
                CRef<blast::CBlastOptionsHandle> opts_hndl,
                blast::CBlastAppArgs* cmdline_args)
{
    CRef<CTmpFile> tmpfile(new CTmpFile(CTmpFile::eNoRemove));

    // Stuff the query bioseq or seqloc list in the input stream of the
    // cmdline_args
    if (queries.IsSeq_loc_list()) {
        const CBlast4_queries::TSeq_loc_list& seqlocs =
            queries.GetSeq_loc_list();
        CFastaOstream out(tmpfile->AsOutputFile(CTmpFile::eIfExists_Throw));
        out.SetFlag(CFastaOstream::eAssembleParts);
        
        EBlastProgramType prog = opts_hndl->GetOptions().GetProgramType();
        SDataLoaderConfig dlconfig(!!Blast_QueryIsProtein(prog));
        dlconfig.OptimizeForWholeLargeSequenceRetrieval();
        CBlastScopeSource scope_src(dlconfig);
        CRef<CScope> scope(scope_src.NewScope());

        ITERATE(CBlast4_queries::TSeq_loc_list, itr, seqlocs) {
            if ((*itr)->GetId()) {
                CBioseq_Handle bh = scope->GetBioseqHandle(*(*itr)->GetId());
                out.Write(bh);
            }
        }
        scope.Reset();
        scope_src.RevokeBlastDbDataLoader();

    } else {
        _ASSERT(queries.IsBioseq_set());
        const CBlast4_queries::TBioseq_set& bioseqs =
            queries.GetBioseq_set();
        CFastaOstream out(tmpfile->AsOutputFile(CTmpFile::eIfExists_Throw));
        out.SetFlag(CFastaOstream::eAssembleParts);

        ITERATE(CBioseq_set::TSeq_set, seq_entry, bioseqs.GetSeq_set()){
            out.Write(**seq_entry);
        }
    }

    const string& fname = tmpfile->GetFileName();
    tmpfile.Reset(new CTmpFile(fname));
    cmdline_args->SetInputStream(tmpfile);
}

/// Import the database and return it in a CBlastDatabaseArgs object
static CRef<blast::CBlastDatabaseArgs>
s_ImportDatabase(const CBlast4_subject& subj, 
                 CBlastOptionsBuilder& opts_builder,
                 bool subject_is_protein,
                 bool is_remote_search)
{
    _ASSERT(subj.IsDatabase());
    CRef<CBlastDatabaseArgs> db_args(new CBlastDatabaseArgs);
    const CSearchDatabase::EMoleculeType mol = subject_is_protein
        ? CSearchDatabase::eBlastDbIsProtein
        : CSearchDatabase::eBlastDbIsNucleotide;
    const string dbname(subj.GetDatabase());
    CRef<CSearchDatabase> search_db(new CSearchDatabase(dbname, mol));

    if (opts_builder.HaveEntrezQuery()) {
        string limit(opts_builder.GetEntrezQuery());
        search_db->SetEntrezQueryLimitation(limit);
        if ( !is_remote_search ) {
            string msg("Entrez query '");
            msg += limit + string("' will not be processed locally.\n");
            msg += string("Please use the -remote option.");
            throw runtime_error(msg);
        }
    }

    if (opts_builder.HaveGiList() || opts_builder.HaveTaxidList()) {
        CSeqDBGiList *gilist = new CSeqDBGiList();
        if (opts_builder.HaveGiList()) {
        	ITERATE(list<TGi>, gi, opts_builder.GetGiList()) {
        		gilist->AddGi(*gi);
        	}
        }
        if (opts_builder.HaveTaxidList()) {
        	list<TTaxId>  list = opts_builder.GetTaxidList();
           	set<TTaxId> taxids(list.begin(), list.end());
        	gilist->AddTaxIds(taxids);
        }
        search_db->SetGiList(gilist);
    }

    if (opts_builder.HaveNegativeGiList() || opts_builder.HaveNegativeTaxidList()) {
    	CSeqDBGiList *gilist = new CSeqDBGiList();
        if (opts_builder.HaveNegativeGiList()) {
          	ITERATE(list<TGi>, gi, opts_builder.GetNegativeGiList()) {
      		gilist->AddGi(*gi);
       	    }
        }
        if (opts_builder.HaveNegativeTaxidList()) {
        	list<TTaxId>  list = opts_builder.GetNegativeTaxidList();
           	set<TTaxId> taxids(list.begin(), list.end());
           	gilist->AddTaxIds(taxids);
        }
        search_db->SetNegativeGiList(gilist);
    }

    if (opts_builder.HasDbFilteringAlgorithmKey()) {
        string algo_key = opts_builder.GetDbFilteringAlgorithmKey();
        ESubjectMaskingType mask_type= eSoftSubjMasking;
        if(opts_builder.HasSubjectMaskingType())
        	mask_type = opts_builder.GetSubjectMaskingType();
        search_db->SetFilteringAlgorithm(algo_key, mask_type);

    } else if (opts_builder.HasDbFilteringAlgorithmId()) {
        int algo_id = opts_builder.GetDbFilteringAlgorithmId();
        ESubjectMaskingType mask_type= eSoftSubjMasking;
        if(opts_builder.HasSubjectMaskingType())
        	mask_type = opts_builder.GetSubjectMaskingType();
        search_db->SetFilteringAlgorithm(algo_id, mask_type);
    }

    db_args->SetSearchDatabase(search_db);
    return db_args;
}

/// Import the subject sequences into a CBlastDatabaseArgs object
static CRef<blast::CBlastDatabaseArgs>
s_ImportSubjects(const CBlast4_subject& subj, bool subject_is_protein)
{
    _ASSERT(subj.IsSequences());
    CRef<CBlastDatabaseArgs> db_args(new CBlastDatabaseArgs);
    TSeqLocVector subjects = 
        s_ConvertBioseqs2TSeqLocVector(subj.GetSequences());
    CRef<CScope> subj_scope = subjects.front().scope;
    CRef<IQueryFactory> subject_factory(new CObjMgr_QueryFactory(subjects));
    db_args->SetSubjects(subject_factory, subj_scope, subject_is_protein);
    return db_args;
}

/// Imports search strategy, using CImportStrategy.
static void
s_ImportSearchStrategy(CNcbiIstream* in, 
                       blast::CBlastAppArgs* cmdline_args,
                       bool is_remote_search, 
                       bool override_query, 
                       bool override_subject)
{
    if ( !in ) {
        return;
    }

    CRef<CBlast4_request> b4req;
    try { 
        b4req = ExtractBlast4Request(*in);
    } catch (const CSerialException&) {
        NCBI_THROW(CInputException, eInvalidInput, 
                   "Failed to read search strategy");
    }

    CImportStrategy strategy(b4req);

    CRef<blast::CBlastOptionsHandle> opts_hndl = strategy.GetOptionsHandle();
    cmdline_args->SetOptionsHandle(opts_hndl);
    const EBlastProgramType prog = opts_hndl->GetOptions().GetProgramType();
    cmdline_args->SetTask(strategy.GetTask());
#if _DEBUG
    {
        char* program_string = 0;
        BlastNumber2Program(prog, &program_string);
        _TRACE("EBlastProgramType=" << program_string << " task=" << strategy.GetTask());
        sfree(program_string);
    }
#endif

    // Get the subject
    if (override_subject) {
        ERR_POST(Warning << "Overriding database/subject in saved strategy");
    } else {
        CRef<blast::CBlastDatabaseArgs> db_args;
        CRef<CBlast4_subject> subj = strategy.GetSubject();
        const bool subject_is_protein = Blast_SubjectIsProtein(prog) ? true : false;

        if (subj->IsDatabase()) {
            db_args = s_ImportDatabase(*subj, strategy.GetOptionsBuilder(),
                                       subject_is_protein, is_remote_search);
        } else {
            db_args = s_ImportSubjects(*subj, subject_is_protein);
        }
        _ASSERT(db_args.NotEmpty());
        cmdline_args->SetBlastDatabaseArgs(db_args);
    }

    // Get the query, queries, or pssm
    if (override_query) {
        ERR_POST(Warning << "Overriding query in saved strategy");
    } else {
        CRef<CBlast4_queries> queries = strategy.GetQueries();
        if (queries->IsPssm()) {
            s_ImportPssm(*queries, opts_hndl, cmdline_args);
        } else {
            s_ImportQueries(*queries, opts_hndl, cmdline_args);
        }
        // Set the range restriction for the query, if applicable
        const TSeqRange query_range = strategy.GetQueryRange();
        if (query_range != TSeqRange::GetEmpty()) {
            cmdline_args->GetQueryOptionsArgs()->SetRange(query_range);
        }
    }

    if ( CPsiBlastAppArgs* psi_args = dynamic_cast<CPsiBlastAppArgs*>(cmdline_args) )
    {
            psi_args->SetNumberOfIterations(strategy.GetPsiNumOfIterations());
    }
}

bool
RecoverSearchStrategy(const CArgs& args, blast::CBlastAppArgs* cmdline_args)
{
    CNcbiIstream* in = cmdline_args->GetImportSearchStrategyStream(args);
    if ( !in ) {
        return false;
    }
    const bool is_remote_search = 
        (args.Exist(kArgRemote) && args[kArgRemote].HasValue() && args[kArgRemote].AsBoolean());
    const bool override_query = (args[kArgQuery].HasValue() && 
                                 args[kArgQuery].AsString() != kDfltArgQuery);
    const bool override_subject = CBlastDatabaseArgs::HasBeenSet(args);

    if (CMbIndexArgs::HasBeenSet(args)) {
    	if (args[kArgUseIndex].AsBoolean() != kDfltArgUseIndex)
    		ERR_POST(Warning << "Overriding megablast BLAST DB indexed options in saved strategy");
    }

    s_ImportSearchStrategy(in, cmdline_args, is_remote_search, override_query,
                           override_subject);

    return true;
}

// Process search strategies
// FIXME: save program options,
// Save task if provided, no other options (only those in the cmd line) should
// be saved
void
SaveSearchStrategy(const CArgs& args,
                   blast::CBlastAppArgs* cmdline_args,
                   CRef<blast::IQueryFactory> queries,
                   CRef<blast::CBlastOptionsHandle> opts_hndl,
                   CRef<objects::CPssmWithParameters> pssm 
                     /* = CRef<objects::CPssmWithParameters>() */,
                    unsigned int num_iters /* =0 */)
{
    CNcbiOstream* out = cmdline_args->GetExportSearchStrategyStream(args);
    if ( !out ) {
        return;
    }

    s_ExportSearchStrategy(out, queries, opts_hndl, 
                           cmdline_args->GetBlastDatabaseArgs(), 
                           pssm, num_iters);
}

/// Extracts the subject sequence IDs and ranges from the BLAST results
/// @note if this ever needs to be refactored for popular developer
/// consumption, this function should operate on CSeq_align_set as opposed to
/// blast::CSearchResultSet
static void 
s_ExtractSeqidsAndRanges(const blast::CSearchResultSet& results,
                         CScope::TIds& ids, vector<TSeqRange>& ranges)
{
    static const CSeq_align::TDim kQueryRow = 0;
    static const CSeq_align::TDim kSubjRow = 1;
    ids.clear();
    ranges.clear();

    typedef map< CConstRef<CSeq_id>, 
                 vector<TSeqRange>, 
                 CConstRefCSeqId_LessThan
               > TSeqIdRanges;
    TSeqIdRanges id_ranges;

    ITERATE(blast::CSearchResultSet, result, results) {
        if ( !(*result)->HasAlignments() ) {
            continue;
        }
        ITERATE(CSeq_align_set::Tdata, aln, (*result)->GetSeqAlign()->Get()) {
            CConstRef<CSeq_id> subj(&(*aln)->GetSeq_id(kSubjRow));
            TSeqRange subj_range((*aln)->GetSeqRange(kSubjRow));
            if ((*aln)->GetSeqStrand(kQueryRow) == eNa_strand_minus &&
                (*aln)->GetSeqStrand(kSubjRow) == eNa_strand_plus) {
                TSeqRange r(subj_range);
                // flag the range as needed to be flipped once the sequence
                // length is known
                subj_range.SetFrom(r.GetToOpen());
                subj_range.SetToOpen(r.GetFrom());
            }
            id_ranges[subj].push_back(subj_range);
        }
    }

    ITERATE(TSeqIdRanges, itr, id_ranges) {
        ITERATE(vector<TSeqRange>, range, itr->second) {
            ids.push_back(CSeq_id_Handle::GetHandle(*itr->first));
            ranges.push_back(*range);
        }
    }
    _ASSERT(ids.size() == ranges.size());
}

/// Returns true if the remote BLAST DB data loader is being used
static bool
s_IsUsingRemoteBlastDbDataLoader()
{
    CObjectManager::TRegisteredNames data_loaders;
    CObjectManager::GetInstance()->GetRegisteredNames(data_loaders);
    ITERATE(CObjectManager::TRegisteredNames, name, data_loaders) {
        if (NStr::StartsWith(*name, objects::CRemoteBlastDbDataLoader::kNamePrefix)) {
            return true;
        }
    }
    return false;
}

static bool
s_IsPrefetchFormat(blast::CFormattingArgs::EOutputFormat format_type)
{
	if ((format_type == CFormattingArgs::eAsnText) ||
	    (format_type == CFormattingArgs::eAsnBinary) ||
	    (format_type == CFormattingArgs::eArchiveFormat)||
	    (format_type == CFormattingArgs::eJsonSeqalign)) {
		return false;
	}
	return true;
}

static bool
s_PreFetchSeqs(const blast::CSearchResultSet& results,
		       blast::CFormattingArgs::EOutputFormat format_type)
{
	{
		char * pre_fetch_limit_str = getenv("PRE_FETCH_SEQS_LIMIT");
		if (pre_fetch_limit_str) {
			int pre_fetch_limit = NStr::StringToInt(pre_fetch_limit_str);
			if(pre_fetch_limit == 0) {
				return false;
			}
			if(pre_fetch_limit == INT_MAX){
				return true;
			}
			int num_of_seqs = 0;
			for(unsigned int i=0; i < results.GetNumResults(); i++) {
				if(results[i].HasAlignments()) {
					num_of_seqs += results[i].GetSeqAlign()->Size();
				}
			}
			if(num_of_seqs > pre_fetch_limit) {
				return false;
			}
		}
	}

	return s_IsPrefetchFormat(format_type);
}

void BlastFormatter_PreFetchSequenceData(const blast::CSearchResultSet& results,
		                                 CRef<CScope> scope,
		                                 blast::CFormattingArgs::EOutputFormat format_type)
{
    _ASSERT(scope.NotEmpty());
    if (results.size() == 0) {
        return;
    }
    if(!s_PreFetchSeqs(results, format_type)){
    	return;
    }
    try {
       CScope::TIds ids;
       vector<TSeqRange> ranges;
       s_ExtractSeqidsAndRanges(results, ids, ranges);
       _TRACE("Prefetching " << ids.size() << " sequence lengths");
       LoadSequencesToScope(ids, ranges, scope);
	}
    catch(const CSeqDBException & e) {
        _TRACE(e.GetMsg());
    	throw;
    }
    catch (const CException& e) {
	   if(s_IsUsingRemoteBlastDbDataLoader()) {
          ERR_POST(Warning << "Error fetching sequence data from BLAST databases at NCBI, "
                     "please try again later");
	   }
	   else {
          ERR_POST(Warning << "Error pre-fetching sequence data");
	   }
   }

}

/// Auxiliary function to extract the ancillary data from the PSSM.
CRef<CBlastAncillaryData>
ExtractPssmAncillaryData(const CPssmWithParameters& pssm)
{
    _ASSERT(pssm.CanGetPssm());
    pair<double, double> lambda, k, h;
    lambda.first = pssm.GetPssm().GetLambdaUngapped();
    lambda.second = pssm.GetPssm().GetLambda();
    k.first = pssm.GetPssm().GetKappaUngapped();
    k.second = pssm.GetPssm().GetKappa();
    h.first = pssm.GetPssm().GetHUngapped();
    h.second = pssm.GetPssm().GetH();
    return CRef<CBlastAncillaryData>(new CBlastAncillaryData(lambda, k, h, 0,
                                                             true));
}

void
CheckForFreqRatioFile(const string& rps_dbname, CRef<CBlastOptionsHandle>  & opt_handle, bool isRpsblast)
{
    bool use_cbs = (opt_handle->GetOptions().GetCompositionBasedStats() == eNoCompositionBasedStats) ? false : true;
    if(use_cbs) {
        vector<string> db;
        NStr::Split(rps_dbname, " ", db);
        list<string> failed_db;
        for (unsigned int i=0; i < db.size(); i++) {
    	    string path;
    	    try {
                vector<string> dbpath;
       	        CSeqDB::FindVolumePaths(db[i], CSeqDB::eProtein, dbpath);
                path = *dbpath.begin();
            } catch (const CSeqDBException& e) {
                 NCBI_RETHROW(e, CBlastException, eRpsInit,
                              "Cannot retrieve path to RPS database");
            }

    	    CFile f(path + ".freq");
            if(!f.Exists()) {
            	failed_db.push_back(db[i]);
            }

        }
        if(!failed_db.empty()) {
        	opt_handle->SetOptions().SetCompositionBasedStats(eNoCompositionBasedStats);
        	string all_failed = NStr::Join(failed_db, ", ");
        	string prog_str = isRpsblast ? "RPSBLAST": "DELTABLAST";
        	string msg = all_failed + " contain(s) no freq ratios " \
                     	 + "needed for composition-based statistics.\n" \
                     	 + prog_str + " will be run without composition-based statistics.";
        	ERR_POST(Warning << msg);
        }

    }
    return;
}

bool
IsIStreamEmpty(CNcbiIstream & in)
{
#ifdef NCBI_OS_MSWIN
	char c;
	in.setf(ios::skipws);
	if (!(in >> c))
		return true;
	in.unget();
	return false;
#else
	char c;
	CNcbiStreampos orig_p = in.tellg();
	// Piped input
	if(orig_p < 0)
		return false;

	IOS_BASE::iostate orig_state = in.rdstate();
	IOS_BASE::fmtflags orig_flags = in.setf(ios::skipws);

	if(! (in >> c))
		return true;

	in.seekg(orig_p);
	in.flags(orig_flags);
	in.clear();
	in.setstate(orig_state);

	return false;
#endif
}

string
GetCmdlineArgs(const CNcbiArguments & a)
{
	string cmd = kEmptyStr;
	for(unsigned int i=0; i < a.Size(); i++) {
		cmd += a[i] + " ";
	}
	return cmd;
}

bool
UseXInclude(const CFormattingArgs & f, const string & s)
{
	CFormattingArgs::EOutputFormat fmt = f.GetFormattedOutputChoice();
	if((fmt ==  CFormattingArgs::eXml2) || (fmt ==  CFormattingArgs::eJson)) {
	   if (s == "-"){
		   string f_str = (fmt == CFormattingArgs::eXml2) ? "14.": "13.";
		   NCBI_THROW(CInputException, eEmptyUserInput,
		              "Please provide a file name for outfmt " + f_str);
	   }
	   return true;
	}
	return false;
}

string 
GetSubjectFile(const CArgs& args)
{
	string filename="";
	
	if (args.Exist(kArgSubject) && args[kArgSubject].HasValue())
		filename = args[kArgSubject].AsString();

	return filename;
}

void PrintErrorArchive(const CArgs & a, const list<CRef<CBlast4_error> > & msg)
{
	try {
		if(NStr::StringToInt(NStr::TruncateSpaces(a[align_format::kArgOutputFormat].AsString())) == CFormattingArgs::eArchiveFormat) {
			CRef<CBlast4_archive> archive (new CBlast4_archive);

			CBlast4_request & req = archive->SetRequest();
			CBlast4_get_request_info_request & info= req.SetBody().SetGet_request_info();
			info.SetRequest_id("Error");
			CBlast4_get_search_results_reply & results = archive->SetResults();
                        // Pacify unused varaible warning, the set above is used to populate mandatory field
			(void) results;
			archive->SetMessages() = msg;
			CBlastFormat::PrintArchive(archive, a[kArgOutput].AsOutputFile());
		}
	} catch (...) {}
}

void QueryBatchCleanup()
{
#if defined(NCBI_OS_LINUX) && HAVE_MALLOC_H
	malloc_trim(0);
#endif
    return;

}

void LogQueryInfo(CBlastUsageReport & report, const CBlastInput & q_info)
{
	report.AddParam(CBlastUsageReport::eTotalQueryLength, q_info.GetTotalLengthProcessed());
	report.AddParam(CBlastUsageReport::eNumQueries, q_info.GetNumSeqsProcessed());
}


void LogBlastOptions(blast::CBlastUsageReport & report, const CBlastOptions & opt)
{
	EBlastProgramType prog_type = opt.GetProgramType();
	report.AddParam(CBlastUsageReport::eProgram, Blast_ProgramNameFromType(prog_type));
	report.AddParam(CBlastUsageReport::eEvalueThreshold, opt.GetEvalueThreshold());
	report.AddParam(CBlastUsageReport::eHitListSize, opt.GetHitlistSize());
	if (!Blast_ProgramIsNucleotide(prog_type)) {
		report.AddParam(CBlastUsageReport::eCompBasedStats, opt.GetCompositionBasedStats());
	}
}

void LogCmdOptions(blast::CBlastUsageReport & report, const CBlastAppArgs & args)
{
	if (args.GetBlastDatabaseArgs().NotEmpty() &&
		args.GetBlastDatabaseArgs()->GetSearchDatabase().NotEmpty() &&
		args.GetBlastDatabaseArgs()->GetSearchDatabase()->GetSeqDb().NotEmpty()) {

		CRef<CSeqDB> db = args.GetBlastDatabaseArgs()->GetSearchDatabase()->GetSeqDb();
		string db_name = db->GetDBNameList();
		int off = db_name.find_last_of(CFile::GetPathSeparator());
	    if (off != -1) {
	    	db_name.erase(0, off+1);
		}
		report.AddParam(CBlastUsageReport::eDBName, db_name);
		report.AddParam(CBlastUsageReport::eDBLength, (Int8) db->GetTotalLength());
		report.AddParam(CBlastUsageReport::eDBNumSeqs, db->GetNumSeqs());
		report.AddParam(CBlastUsageReport::eDBDate, db->GetDate());
	}

	if(args.GetFormattingArgs().NotEmpty()){
		report.AddParam(CBlastUsageReport::eOutputFmt, args.GetFormattingArgs()->GetFormattedOutputChoice());
	}
}

int GetMTByQueriesBatchSize(EProgram p, int num_threads, const string & task)
{
	    int batch_size = 0;

		char * mt_query_batch_env = getenv("BLAST_MT_QUERY_BATCH_SIZE");
		if (mt_query_batch_env) {
			batch_size = NStr::StringToInt(mt_query_batch_env);
		}
		else {
			batch_size = GetQueryBatchSize(p);
		}
                if (task == "blastx-fast")
                {  // Set batch_size to 20004
                        batch_size *= 2;
                }
		return batch_size;
}

void MTByQueries_DBSize_Warning(const Int8 length_limit, bool is_db_protein)
{
	string warn = "This database is probably too large to benefit from -mt_mode=1. " \
				  "We suggest using -mt_mode=1 only if the database is less than " \
				  + NStr::Int8ToString(length_limit, NStr::fWithCommas);
	if (is_db_protein) {
		warn +=  + " residues ";
	}
	else {
		warn += " bases ";
	}
   	ERR_POST(Warning <<   warn + "or if the search is limited by an option such as -taxids, -taxidlist or -gilist.");
	return;
}

void CheckMTByQueries_QuerySize(EProgram prog, int batch_size)
{
	string warning = "This set of queries is too small to fully benefit from the -mt_mode=1 option. " \
			         "The total number of letters should be at least ";
	warning += NStr::IntToString(batch_size);
	EBlastProgramType p = EProgramToEBlastProgramType(prog);
	if (Blast_QueryIsProtein(p)) {
		warning += " residues";
	}
	else {
		warning += " bases";
	}
	warning += " per thread, and there should be at least one query of this length per thread.";
    ERR_POST(Warning << warning);
}

END_NCBI_SCOPE
