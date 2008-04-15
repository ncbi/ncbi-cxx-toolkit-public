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

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include "blast_app_util.hpp"

#include <serial/serial.hpp>
#include <serial/objostr.hpp>

#include <objtools/data_loaders/blastdb/bdbloader.hpp>
#include <algo/blast/api/remote_blast.hpp>
#include <algo/blast/api/blast_options_builder.hpp>
#include <objtools/readers/seqdb/seqdbcommon.hpp>   // for CSeqDBException
#include <algo/blast/blastinput/blast_input.hpp>    // for CInputException
#include <algo/blast/blastinput/psiblast_args.hpp>
#include <algo/blast/blastinput/tblastn_args.hpp>
#include <algo/blast/blastinput/blast_scope_src.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
USING_SCOPE(blast);

CRef<blast::CRemoteBlast> 
InitializeRemoteBlast(CRef<blast::IQueryFactory> queries,
                      CRef<blast::CBlastDatabaseArgs> db_args,
                      CRef<blast::CBlastOptionsHandle> opts_hndl,
                      CRef<objects::CScope> scope,
                      bool verbose_output,
                      CRef<objects::CPssmWithParameters> pssm 
                        /* = CRef<objects::CPssmWithParameters>() */)
{
    _ASSERT(queries);
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
            retval.Reset(new CRemoteBlast(queries, opts_hndl,
                                         db_args->GetSubjects(scope)));
        }
    }
    if (verbose_output) {
        retval->SetVerbose();
    }
    return retval;
}

/** 
 * @brief Read a gi list by resolving the file name
 * 
 * @param filename name of file containing the gi list
 * 
 * @return CRef containing the gi list
 *
 * @throw CSeqDBException if the filename is not found
 */
static CRef<CSeqDBGiList> s_ReadGiList(const string& filename)
{
    CRef<CSeqDBGiList> retval;
    _ASSERT( !filename.empty() );

    string file2open(SeqDB_ResolveDbPath(filename));
    if (file2open.empty()) {
        NCBI_THROW(CSeqDBException, eFileErr, 
                   "File '" + filename + "' not found");
    } 
    retval.Reset(new CSeqDBFileGiList(file2open));
    _ASSERT(retval);
    return retval;
}

CRef<CSeqDB> GetSeqDB(CRef<blast::CBlastDatabaseArgs> db_args)
{
    CRef<CSeqDB> retval;
    const CSeqDB::ESeqType seq_type = db_args->IsProtein()
        ? CSeqDB::eProtein
        : CSeqDB::eNucleotide;

    string gi_list_restriction = db_args->GetGiListFileName();
    string negative_gi_list = db_args->GetNegativeGiListFileName();
    _ASSERT(gi_list_restriction.empty() || negative_gi_list.empty());

    if ( !gi_list_restriction.empty() ) {
        CRef<CSeqDBGiList> gi_list = s_ReadGiList(gi_list_restriction);
        _ASSERT(gi_list);
        retval.Reset(new CSeqDB(db_args->GetDatabaseName(), seq_type, gi_list));
    } else if ( !negative_gi_list.empty() ) {
        CRef<CSeqDBGiList> gi_list = s_ReadGiList(negative_gi_list);
        _ASSERT(gi_list);
        vector<int> gis;
        gi_list->GetGiList(gis);
        CSeqDBIdSet idset(gis, CSeqDBIdSet::eGi, false);
        retval.Reset(new CSeqDB(db_args->GetDatabaseName(), seq_type,
                                idset));
    } else {
        retval.Reset(new CSeqDB(db_args->GetDatabaseName(), seq_type));
    }
    _ASSERT(retval.NotEmpty());
    return retval;
}

string RegisterOMDataLoader(CRef<objects::CObjectManager> objmgr, 
                            CRef<CSeqDB> db_handle)
{
    // the blast formatter requires that the database coexist in
    // the same scope with the query sequences
    
    CBlastDbDataLoader::RegisterInObjectManager(*objmgr, db_handle,
                                                CObjectManager::eDefault);

    return CBlastDbDataLoader::GetLoaderNameFromArgs(db_handle);
}

/// Auxiliary function to obtain a Blast4-request, i.e.: a search strategy
static CRef<objects::CBlast4_request>
s_GetSearchStrategy(CRef<IQueryFactory> queries,
                  CRef<CBlastOptionsHandle> options_handle,
                  CRef<CSearchDatabase> search_db,
                  objects::CPssmWithParameters* pssm)
{
    _ASSERT(search_db);
    _ASSERT(options_handle);

    CRef<CRemoteBlast> rmt_blast;
    if (queries.NotEmpty()) {
        rmt_blast.Reset(new CRemoteBlast(queries, options_handle, *search_db));
    } else {
        _ASSERT(pssm);
        CRef<CPssmWithParameters> p(pssm);
        rmt_blast.Reset(new CRemoteBlast(p, options_handle, *search_db));
    }

    return rmt_blast->GetSearchStrategy();
}

/// Real implementation of search strategy extraction
/// @todo refactor this code so that it can be reused in other contexts
static void
s_ExportSearchStrategy(CNcbiOstream* out,
                     CRef<blast::IQueryFactory> queries,
                     CRef<blast::CBlastOptionsHandle> options_handle,
                     CRef<blast::CSearchDatabase> search_db,
                     objects::CPssmWithParameters* pssm)
{
    if ( !out ) {
        return;
    }

    CRef<CBlast4_request> req = 
        s_GetSearchStrategy(queries, options_handle, search_db, pssm);
    // N.B.: If writing XML, be sure to call SetEnforcedStdXml on the stream!
    *out << MSerial_AsnText << *req;
}

/// Real implementation of search strategy import
/// @todo refactor this code so that it can be reused in other contexts
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
    } catch (const CException& e) {
        ERR_POST(Fatal << "Fail to read search strategy: " << e.GetMsg());
    }

    if (b4req->CanGetBody() && !b4req->GetBody().IsQueue_search() ) {
        ERR_POST(Fatal << "Cannot recover search strategy: "
                 << "invalid request type");
    }

    const CBlast4_queue_search_request& req(b4req->GetBody().GetQueue_search());
    // N.B.: Make this both as we don't know whether the search will be remote
    // or not
    CBlastOptionsBuilder opts_builder(req.GetProgram(), req.GetService(),
                                      CBlastOptions::eBoth);

    // Create the BLAST options
    CRef<blast::CBlastOptionsHandle> opts_hndl;
    {{
        const CBlast4_parameters* algo_opts(0);
        const CBlast4_parameters* prog_opts(0);

        if (req.CanGetAlgorithm_options()) {
            algo_opts = &req.GetAlgorithm_options();
        } 
        if (req.CanGetProgram_options()) {
            prog_opts = &req.GetProgram_options();
        }

        string task;
        opts_hndl = opts_builder.GetSearchOptions(algo_opts, prog_opts, &task);
        cmdline_args->SetTask(task);
    }}
    _ASSERT(opts_hndl.NotEmpty());
    cmdline_args->SetOptionsHandle(opts_hndl);
    const EBlastProgramType prog = opts_hndl->GetOptions().GetProgramType();

    // Get the subject
    if (override_subject) {
        ERR_POST(Warning << "Overriding database/subject in saved strategy");
    } else {
        CRef<blast::CBlastDatabaseArgs> db_args;
        const CBlast4_subject& subj = req.GetSubject();

        db_args.Reset(new CBlastDatabaseArgs());

        if (subj.IsDatabase()) {

            const CSearchDatabase::EMoleculeType mol =
                Blast_SubjectIsProtein(prog)
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

            if (opts_builder.HaveGiList()) {
                CSearchDatabase::TGiList limit;
                const list<int>& gilist = opts_builder.GetGiList();
                copy(gilist.begin(), gilist.end(), back_inserter(limit));
                search_db->SetGiListLimitation(limit);
            }

            db_args->SetSearchDatabase(search_db);

        } else {
            throw runtime_error("Recovering from bl2seq is not implemented");
        }
        _ASSERT(db_args.NotEmpty());
        cmdline_args->SetBlastDatabaseArgs(db_args);
    }

    // Get the query, queries, or pssm
    if (override_query) {
        ERR_POST(Warning << "Overriding query in saved strategy");
    } else {
        const CBlast4_queries& queries = req.GetQueries();

        if (queries.IsPssm()) {

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

        } else {

            CRef<CTmpFile> tmpfile(new CTmpFile(CTmpFile::eNoRemove));

            // Stuff the query bioseq or seqloc list in the input stream of the
            // cmdline_args
            if (queries.IsSeq_loc_list()) {
                const CBlast4_queries::TSeq_loc_list& seqlocs =
                    queries.GetSeq_loc_list();
                CFastaOstream out(tmpfile->AsOutputFile
                                  (CTmpFile::eIfExists_Throw));
                out.SetFlag(CFastaOstream::eAssembleParts);
                
                CBlastScopeSource scope_src(!!Blast_QueryIsProtein(prog));
                CRef<CScope> scope(scope_src.NewScope());

                ITERATE(CBlast4_queries::TSeq_loc_list, itr, seqlocs) {
                    CBioseq_Handle bh = scope->GetBioseqHandle(**itr);
                    CConstRef<CBioseq> bioseq = bh.GetCompleteBioseq();
                    out.Write(*bioseq);
                }

            } else {
                _ASSERT(queries.IsBioseq_set());
                const CBlast4_queries::TBioseq_set& bioseqs =
                    queries.GetBioseq_set();
                CFastaOstream out(tmpfile->AsOutputFile
                                  (CTmpFile::eIfExists_Throw));
                out.SetFlag(CFastaOstream::eAssembleParts);

                ITERATE(CBioseq_set::TSeq_set, seq_entry, bioseqs.GetSeq_set()){
                    out.Write(**seq_entry);
                }
            }

            const string& fname = tmpfile->GetFileName();
            tmpfile.Reset(new CTmpFile(fname));
            cmdline_args->SetInputStream(tmpfile);
        }

        // Set the range restriction for the query, if applicable
        const TSeqRange query_range = opts_builder.GetRestrictedQueryRange();
        if (query_range != TSeqRange::GetEmpty()) {
            cmdline_args->GetQueryOptionsArgs()->SetRange(query_range);
        }
    }

}

void
RecoverSearchStrategy(const CArgs& args, blast::CBlastAppArgs* cmdline_args)
{
    CNcbiIstream* in = cmdline_args->GetImportSearchStrategyStream(args);
    s_ImportSearchStrategy(in, cmdline_args,
                           (args[kArgRemote].HasValue() &&
                            args[kArgRemote].AsBoolean()),
                           (args[kArgQuery].HasValue() && 
                            args[kArgQuery].AsString() != kDfltArgQuery),
                            CBlastDatabaseArgs::HasBeenSet(args));
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
                   CRef<blast::CSearchDatabase> search_db,
                   objects::CPssmWithParameters* pssm /* = NULL */)
{
    CNcbiOstream* out = cmdline_args->GetExportSearchStrategyStream(args);
    s_ExportSearchStrategy(out, queries, opts_hndl, search_db, pssm);
}

END_NCBI_SCOPE
