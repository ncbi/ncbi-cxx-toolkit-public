/*  $Id:
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
 * Authors: Amelia Fong
 *
 */

/** @file rpstblastn_node.cpp
 * RPSTBLASTN MT command line application
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/api/remote_blast.hpp>
#include <algo/blast/blastinput/blast_fasta_input.hpp>
#include <algo/blast/blastinput/rpstblastn_args.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/blast/format/blast_format.hpp>
#include "blast_app_util.hpp"
#include "rpstblastn_node.hpp"
#include <algo/blast/api/rpsblast_local.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
#endif

CRPSTBlastnNode::CRPSTBlastnNode (int node_num, const CNcbiArguments & ncbi_args, const CArgs& args,
		                          CBlastAppDiagHandler & bah, const string & input,
                              	  int query_index, int num_queries,CBlastNodeMailbox * mailbox):
                              CBlastNode(node_num, ncbi_args, args, bah, query_index, num_queries, mailbox), m_Input(input)
{
	m_CmdLineArgs.Reset(new CRPSTBlastnNodeArgs(m_Input));
	SetState(eInitialized);
	SendMsg(CBlastNodeMsg::eRunRequest, (void*) this);
}

int CRPSTBlastnNode::GetBlastResults(CNcbiOstream & os)
{
	if(GetState() == eDone) {
		os << m_CmdLineArgs->GetOutputStrStream().rdbuf();
		return GetStatus();
	}
	return -1;
}

CRPSTBlastnNode::~CRPSTBlastnNode()
{
	m_CmdLineArgs.Reset();
}

void *
CRPSTBlastnNode::Main()
{
    int status = BLAST_EXIT_SUCCESS;
    CBlastAppDiagHandler & bah = GetDiagHandler();
	SetDiagPostPrefix(GetNodeIdStr().c_str());

    SetState(eRunning);
	try {
		const CArgs& args = GetArgs();
      	CRef<CBlastOptionsHandle> opts_hndl;
        if(RecoverSearchStrategy(args, m_CmdLineArgs)) {
        	opts_hndl.Reset(&*m_CmdLineArgs->SetOptionsForSavedStrategy(args));
        }
        else {
        	opts_hndl.Reset(&*m_CmdLineArgs->SetOptions(args));
        }

        CheckForFreqRatioFile(m_CmdLineArgs->GetBlastDatabaseArgs()->GetDatabaseName(),
        		               opts_hndl, true);
        const CBlastOptions& opt = opts_hndl->GetOptions();

        /*** Initialize the database ***/
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
        CBlastFastaInputSource fasta(m_CmdLineArgs->GetInputStream(), iconfig);
        CBlastInput input(&fasta, m_CmdLineArgs->GetQueryBatchSize());

        /*** Get the formatting options ***/
        CRef<CFormattingArgs> fmt_args(m_CmdLineArgs->GetFormattingArgs());
        bool isArchiveFormat = fmt_args->ArchiveFormatRequested(args);
        if(!isArchiveFormat) {
        	bah.DoNotSaveMessages();
        }
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
                               false, false, NULL, NULL,
                               GetCmdlineArgs(GetArguments()));

        formatter.SetQueryRange(query_opts->GetRange());
        formatter.SetLineLength(fmt_args->GetLineLength());
        if(UseXInclude(*fmt_args, args[kArgOutput].AsString())) {
        	formatter.SetBaseFile(args[kArgOutput].AsString());
        }
        formatter.PrintProlog();

        /*** Process the input ***/
        for (; !input.End(); formatter.ResetScopeHistory(), QueryBatchCleanup()) {

            CRef<CBlastQueryVector> query_batch(input.GetNextSeqBatch(*scope));
            CRef<IQueryFactory> queries(new CObjMgr_QueryFactory(*query_batch));

            SaveSearchStrategy(args, m_CmdLineArgs, queries, opts_hndl);

            CRef<CSearchResultSet> results;

            if (m_CmdLineArgs->ExecuteRemotely())
            {
                CRef<CRemoteBlast> rmt_blast =
                    InitializeRemoteBlast(queries, db_args, opts_hndl,
                          m_CmdLineArgs->ProduceDebugRemoteOutput(),
                          m_CmdLineArgs->GetClientId());
                results = rmt_blast->GetResultSet();
            }
            else
            {
            	CLocalRPSBlast  local_search (query_batch, db_args->GetDatabaseName(), opts_hndl, 1);
            	results = local_search.Run();
            }

            if (fmt_args->ArchiveFormatRequested(args)) {
                formatter.WriteArchive(*queries, *opts_hndl, *results, 0, bah.GetMessages());
                bah.ResetMessages();
            } else {
               BlastFormatter_PreFetchSequenceData(*results, scope,
            		                               fmt_args->GetFormattedOutputChoice());
                ITERATE(CSearchResultSet, result, *results) {
                    formatter.PrintOneResultSet(**result, query_batch);
                }
            }
        }

        formatter.PrintEpilog(opt);

        if (m_CmdLineArgs->ProduceDebugOutput()) {
            opts_hndl->GetOptions().DebugDumpText(NcbiCerr, "BLAST options", 1);
        }

        SetQueriesLength(input.GetTotalLengthProcessed());
    } CATCH_ALL(status)

	SetStatus(status);
	if (status == BLAST_EXIT_SUCCESS) {
		SetState(eDone);
		SendMsg(CBlastNodeMsg::ePostResult, (void *) this);

	}
	else {
		SetState(eError);
		SendMsg(CBlastNodeMsg::eErrorExit, (void *) this);

	}

    return NULL;
}
