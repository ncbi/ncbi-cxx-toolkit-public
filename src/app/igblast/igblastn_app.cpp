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
 * Authors:  Ning Ma, Jian Jee, Yury Merezhuk
 *
 */

/** @file xigblastn_app.cpp
 * IGBLASTN command line multithreaded application
 */


#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbi_signal.hpp>
#include <serial/objostr.hpp>

#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/api/remote_blast.hpp>
#include <algo/blast/blastinput/blast_fasta_input.hpp>
#include <algo/blast/blastinput/igblastn_args.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/blast/format/blast_format.hpp>
#include <algo/blast/blastinput/blast_input.hpp>
#include <algo/blast/blast_sra_input/blast_sra_input.hpp>
#include "../blast/blast_app_util.hpp"

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
#endif


//DEFINE_CLASS_STATIC_FAST_MUTEX(s_XIG_Lock);

#include "igblastn.hpp"

// this is need for static CFastMutex CIgBlastnApp::m_Mutex;

// WORKER .........................................
void* CIgBlastnApp::CIgWorker::Main(void)
{
    int current_batch_number = 0;

    try{
//===============================================================================================	
    thm_tid = CThread::GetSelf();
    { // increse running thread counter 
      CFastMutexGuard g1(  thm_Mutex_Output  ); //It does not matter for startup. but thread sanitizer will be happy
      thm_run_thread_count++;
      thm_any_started = true;
    }

    // main search cycle
    thm_sw.Start();
    //int local_counter = 0;
    bool done_fasta_input = false;
    while( !done_fasta_input) {
        //OK2 CRef<CIgBlastOptions> ig_opts(thm_ig_args->GetIgBlastOptions());


           CRef<CScope> scope;
	   CRef<CIgBlastArgs>    l_ig_args;
           CRef<CIgBlastOptions> l_ig_opts;
	   CRef<CLocalDbAdapter> l_blastdb;
	   CRef<CBlastOptionsHandle> l_opts_hndl;
           CRef<CBlastDatabaseArgs> l_db_args(thm_CmdLineArgs->GetBlastDatabaseArgs());
           bool db_is_remote = true;

           //OK2 thm_ig_args->AddIgSequenceScope(scope);
	   {
	      CFastMutexGuard g1(  thm_Mutex_Global  );
              scope.Reset(new CScope(*CObjectManager::GetInstance()));
              _ASSERT(scope);

              l_ig_args.Reset(thm_CmdLineArgs->GetIgBlastArgs());              

              l_ig_opts.Reset(l_ig_args->GetIgBlastOptions());
              l_opts_hndl.Reset(&*thm_CmdLineArgs->SetOptions(thm_args));
              if (l_db_args->GetDatabaseName() == kEmptyStr && 
                  l_db_args->GetSubjects().Empty()) {
                  l_blastdb.Reset(&(*(l_ig_opts->m_Db[0])));
                  db_is_remote = false;
              } else {
                  InitializeSubject(l_db_args, l_opts_hndl, 
                                    thm_CmdLineArgs->ExecuteRemotely(),
                                    l_blastdb, scope);
              }
              l_ig_args->AddIgSequenceScope(scope);
                
              
           }

          //CRef<CBlastQueryVector> query(m_input->GetNextSeqBatch(*scope));
          CRef<CBlastQueryVector> query;
	  {   // STEP #0 -- get access 
	      CFastMutexGuard g1(  thm_Mutex_Input  );
	      // STEP #1 -- if input finished, exit
	      if( thm_input->End() ) {
		  break; // other threads did all other chunks
	      }
	      // STEP #2.A -- next batch obtained from input
	      query.Reset( thm_input->GetNextSeqBatch(*scope) );
	      // STEP #2.B -- increase total number of batches in output
	      current_batch_number = thm_max_batch;
	     
	      thm_max_batch++;
	      // STEP #3   -- check if exit in next loop. NOT needed
	      done_fasta_input = thm_input->End(); // will exit if nothing left 
	  }

	  // STEP #4 -- RUN SEARCH
          CRef<CSearchResultSet> results;
          {  
              if (thm_CmdLineArgs->ExecuteRemotely() && db_is_remote) {
                  CIgBlast rmt_blast(query, 
                                     l_db_args->GetSearchDatabase(), 
                                     l_db_args->GetSubjects(),
                                     l_opts_hndl, l_ig_opts,
                                     NcbiEmptyString,
                                     scope);
                  results = rmt_blast.Run();
              } else {
                  // LOCAL BLAST ONLY FOR NOW
                  CIgBlast lcl_blast(query, l_blastdb, l_opts_hndl, l_ig_opts, scope);
                  //- CIgBlast lcl_blast(query, x_blastdb, l_opts_hndl, l_ig_opts, scope);
                  lcl_blast.SetNumberOfThreads( 1 /* one thread only */ );
                  results = lcl_blast.Run();
              }

          }

	  // STEP #5 -- STORE RESULTS IN MAIN STORAGE

	  CRef <CIgBlastContext> one_context( new CIgBlastContext );

	  one_context->m_batch_number = current_batch_number;
	  one_context->m_results = results ;
	  one_context->m_queries = query ;
	  one_context->m_scope   = scope ;

	  {
		// STEP #6.A -- NOTIFY FORMATTER FOR NEW RESULTS
		CFastMutexGuard g2( thm_Mutex_Output  );
	        thm_all_results[ current_batch_number ] = one_context;
	  }

	  {
		// STEP #6.B -- NOTIFY FORMATTER FOR NEW RESULTS
	  	//if( (local_counter++ % 2)  == 1 ) 
	  	{
		    CFastMutexGuard g2( thm_Mutex_Notify  );
		    thm_new_batch_done.Post();
	  	}
	  }

        } // done input

    }
    catch (const ncbi::CException& e) {
	string msg = e.ReportThis(eDPF_ErrCodeExplanation);
	const CStackTrace* stack_trace = e.GetStackTrace();
	if (stack_trace) {
	    CNcbiOstrstream os;
	    os << '\n';
	    stack_trace->Write(os);
	}
	fprintf(stderr,"WORKER: T%u BATCH # %d CEXCEPTION: %s\n",thm_tid,current_batch_number,msg.c_str());
    } catch (const std::exception& e) {
	fprintf(stderr,"WORKER: T%u BATCH # %d EXCEPTION: %s\n",thm_tid,current_batch_number,e.what());
	CSignal::Raise( CSignal::eSignal_SEGV );
    } catch (...) {
	fprintf(stderr,"WORKER: T%u BATCH # %d GENERAL EXCEPTION \n",thm_tid,current_batch_number);
	CSignal::Raise( CSignal::eSignal_SEGV );
    }

    return (void*) NULL;
}
void CIgBlastnApp::CIgWorker::OnExit(void){
      CFastMutexGuard g1(  thm_Mutex_Output  );
      thm_run_thread_count--;
}

// FORMATTER .......................................
void* CIgBlastnApp::CIgFormatter::Main(void)
{
    thm_tid = CThread::GetSelf();

    int waiting_batch_number=0;

    try{
    //bool workers_are_running = true;
    std::map< int , CRef<CIgBlastContext> >::iterator results_ctx_it;

    bool next_batch_ready = false;
    bool is_megablast = false;


    // INIT PART 
    CRef<CBlastDatabaseArgs> l_db_args(thm_CmdLineArgs->GetBlastDatabaseArgs());
        CRef<CFormattingArgs> l_fmt_args;
        CRef<CLocalDbAdapter> l_blastdb_full;
	CRef<CIgBlastArgs>    l_ig_args; 
	CRef<CIgBlastOptions> l_ig_opts;
	string                l_full_db_list;
        CRef<CLocalDbAdapter> l_blastdb;
        Int4 l_num_alignments = 0;
        CRef<CQueryOptionsArgs>       l_query_opts;

	{
	    CFastMutexGuard g1(  thm_Mutex_Global  ); // GENERAL?
	    if( thm_CmdLineArgs->GetTask() == "megablast" ) is_megablast = true;
	    l_ig_args.Reset(thm_CmdLineArgs->GetIgBlastArgs());
            l_ig_opts.Reset(l_ig_args->GetIgBlastOptions());
            l_fmt_args.Reset( thm_CmdLineArgs->GetFormattingArgs() );
            
  
            if (l_db_args->GetDatabaseName() == kEmptyStr && 
                l_db_args->GetSubjects().Empty()) {
                l_full_db_list =  l_ig_opts->m_Db[0]->GetDatabaseName() + " " +
                    l_ig_opts->m_Db[1]->GetDatabaseName() + " " +
                    l_ig_opts->m_Db[2]->GetDatabaseName() ;
            } else {
                 
                if (thm_CmdLineArgs->ExecuteRemotely()) {
                    l_full_db_list =  l_db_args->GetDatabaseName();
                } else {
                    l_full_db_list =  l_ig_opts->m_Db[0]->GetDatabaseName() + " " +
                    l_ig_opts->m_Db[1]->GetDatabaseName() + " " +
                        l_ig_opts->m_Db[2]->GetDatabaseName() + " " +
                        l_db_args->GetDatabaseName();
                }
            }
            

            CSearchDatabase l_sdb(l_full_db_list, CSearchDatabase::eBlastDbIsNucleotide );
            l_blastdb_full.Reset(new CLocalDbAdapter(l_sdb));
            l_blastdb.Reset(&(*(l_ig_opts->m_Db[0])));
	    if( !thm_CmdLineArgs->GetBlastDatabaseArgs()->GetDatabaseName().empty() ){
		l_num_alignments = l_fmt_args->GetNumAlignments();
	    }
            l_query_opts = thm_CmdLineArgs->GetQueryOptionsArgs();
	}
        CFormattingArgs::EOutputFormat l_fmt_output_choice = l_fmt_args->GetFormattedOutputChoice();

    	if(     l_fmt_output_choice == CFormattingArgs::eFlatQueryAnchoredIdentities ||
	    l_fmt_output_choice == CFormattingArgs::eFlatQueryAnchoredNoIdentities)
    	{
            if(l_blastdb_full->GetDatabaseName() != NcbiEmptyString){
		CFastMutexGuard g1(  thm_Mutex_Global  ); // GENERAL

                vector<CBlastFormatUtil::SDbInfo> db_info;
                CBlastFormatUtil::GetBlastDbInfo(db_info, l_full_db_list, l_ig_opts->m_IsProtein, -1, false);
                CBlastFormatUtil::PrintDbReport(db_info, 68, thm_CmdLineArgs->GetOutputStream(), true);
            }
    	}
    	

    //  HEADER -  END
   	const CBlastOptions& l_opt = thm_opts_hndl->GetOptions();  // MUTEX NEEDED?

        const char* matrix_name = l_opt.GetMatrixName();
        int query_gcode = l_opt.GetQueryGeneticCode();
        int db_gcode = l_opt.GetDbGeneticCode();
        bool sum_stats = l_opt.GetSumStatisticsMode();
        bool index_load = l_opt.GetMBIndexLoaded();

        int filter_algo = l_blastdb->GetFilteringAlgorithm();
     
        TSeqPos num_description = l_fmt_args->GetNumDescriptions();
        bool show_gi = l_fmt_args->ShowGis();
        bool show_html = l_fmt_args->DisplayHtmlOutput();
        string custom_format = l_fmt_args->GetCustomOutputFormatSpec();
        CFormattingArgs::EOutputFormat output_choice = l_fmt_args->GetFormattedOutputChoice();
        bool parse_defline = l_query_opts->GetParseDeflines();
        CNcbiOstream& out_stream = thm_CmdLineArgs->GetOutputStream();


    //
    //

        l_ig_opts->m_AirrField.push_back("sequence_id");
        l_ig_opts->m_AirrField.push_back("sequence");
        l_ig_opts->m_AirrField.push_back("locus");
        l_ig_opts->m_AirrField.push_back("stop_codon");
        l_ig_opts->m_AirrField.push_back("vj_in_frame");
        l_ig_opts->m_AirrField.push_back("v_frameshift");
        l_ig_opts->m_AirrField.push_back("productive");
        l_ig_opts->m_AirrField.push_back("rev_comp");
        l_ig_opts->m_AirrField.push_back("complete_vdj");
        l_ig_opts->m_AirrField.push_back("v_call");
        l_ig_opts->m_AirrField.push_back("d_call");
        l_ig_opts->m_AirrField.push_back("j_call");
        l_ig_opts->m_AirrField.push_back("sequence_alignment");
        l_ig_opts->m_AirrField.push_back("germline_alignment");
        l_ig_opts->m_AirrField.push_back("sequence_alignment_aa");
        l_ig_opts->m_AirrField.push_back("germline_alignment_aa");
        l_ig_opts->m_AirrField.push_back("v_alignment_start");
        l_ig_opts->m_AirrField.push_back("v_alignment_end");
        l_ig_opts->m_AirrField.push_back("d_alignment_start");
        l_ig_opts->m_AirrField.push_back("d_alignment_end");
        l_ig_opts->m_AirrField.push_back("j_alignment_start");
        l_ig_opts->m_AirrField.push_back("j_alignment_end");
        l_ig_opts->m_AirrField.push_back("v_sequence_alignment");
        l_ig_opts->m_AirrField.push_back("v_sequence_alignment_aa");
        l_ig_opts->m_AirrField.push_back("v_germline_alignment");
        l_ig_opts->m_AirrField.push_back("v_germline_alignment_aa");
        l_ig_opts->m_AirrField.push_back("d_sequence_alignment");
        l_ig_opts->m_AirrField.push_back("d_sequence_alignment_aa");
        l_ig_opts->m_AirrField.push_back("d_germline_alignment");
        l_ig_opts->m_AirrField.push_back("d_germline_alignment_aa");
        l_ig_opts->m_AirrField.push_back("j_sequence_alignment");
        l_ig_opts->m_AirrField.push_back("j_sequence_alignment_aa");
        l_ig_opts->m_AirrField.push_back("j_germline_alignment");
        l_ig_opts->m_AirrField.push_back("j_germline_alignment_aa");
        l_ig_opts->m_AirrField.push_back("fwr1");
        l_ig_opts->m_AirrField.push_back("fwr1_aa");
        l_ig_opts->m_AirrField.push_back("cdr1");
        l_ig_opts->m_AirrField.push_back("cdr1_aa");
        l_ig_opts->m_AirrField.push_back("fwr2");
        l_ig_opts->m_AirrField.push_back("fwr2_aa");
        l_ig_opts->m_AirrField.push_back("cdr2");
        l_ig_opts->m_AirrField.push_back("cdr2_aa");
        l_ig_opts->m_AirrField.push_back("fwr3");
        l_ig_opts->m_AirrField.push_back("fwr3_aa");
        l_ig_opts->m_AirrField.push_back("fwr4");
        l_ig_opts->m_AirrField.push_back("fwr4_aa");
        l_ig_opts->m_AirrField.push_back("cdr3");
        l_ig_opts->m_AirrField.push_back("cdr3_aa");

        l_ig_opts->m_AirrField.push_back("junction");
        l_ig_opts->m_AirrField.push_back("junction_length");
        l_ig_opts->m_AirrField.push_back("junction_aa");
        l_ig_opts->m_AirrField.push_back("junction_aa_length");
        l_ig_opts->m_AirrField.push_back("v_score");
        l_ig_opts->m_AirrField.push_back("d_score");
        l_ig_opts->m_AirrField.push_back("j_score");
        l_ig_opts->m_AirrField.push_back("v_cigar");
        l_ig_opts->m_AirrField.push_back("d_cigar");
        l_ig_opts->m_AirrField.push_back("j_cigar");
        l_ig_opts->m_AirrField.push_back("v_support");
        l_ig_opts->m_AirrField.push_back("d_support");
        l_ig_opts->m_AirrField.push_back("j_support");
        l_ig_opts->m_AirrField.push_back("v_identity");
        l_ig_opts->m_AirrField.push_back("d_identity");
        l_ig_opts->m_AirrField.push_back("j_identity");
        l_ig_opts->m_AirrField.push_back("v_sequence_start");
        l_ig_opts->m_AirrField.push_back("v_sequence_end");
        l_ig_opts->m_AirrField.push_back("v_germline_start");
        l_ig_opts->m_AirrField.push_back("v_germline_end");
        l_ig_opts->m_AirrField.push_back("d_sequence_start");
        l_ig_opts->m_AirrField.push_back("d_sequence_end");
        l_ig_opts->m_AirrField.push_back("d_germline_start");
        l_ig_opts->m_AirrField.push_back("d_germline_end");
        l_ig_opts->m_AirrField.push_back("j_sequence_start");
        l_ig_opts->m_AirrField.push_back("j_sequence_end");
        l_ig_opts->m_AirrField.push_back("j_germline_start");
        l_ig_opts->m_AirrField.push_back("j_germline_end");
        l_ig_opts->m_AirrField.push_back("fwr1_start");
        l_ig_opts->m_AirrField.push_back("fwr1_end");
        l_ig_opts->m_AirrField.push_back("cdr1_start");
        l_ig_opts->m_AirrField.push_back("cdr1_end");
        l_ig_opts->m_AirrField.push_back("fwr2_start");
        l_ig_opts->m_AirrField.push_back("fwr2_end");
        l_ig_opts->m_AirrField.push_back("cdr2_start");
        l_ig_opts->m_AirrField.push_back("cdr2_end");
        l_ig_opts->m_AirrField.push_back("fwr3_start");
        l_ig_opts->m_AirrField.push_back("fwr3_end");
        l_ig_opts->m_AirrField.push_back("fwr4_start");
        l_ig_opts->m_AirrField.push_back("fwr4_end");
        l_ig_opts->m_AirrField.push_back("cdr3_start");
        l_ig_opts->m_AirrField.push_back("cdr3_end");
        l_ig_opts->m_AirrField.push_back("np1");
        l_ig_opts->m_AirrField.push_back("np1_length");
        l_ig_opts->m_AirrField.push_back("np2");
        l_ig_opts->m_AirrField.push_back("np2_length");
      



        int align_index = 0;
        
    while( true ){
	CRef <CIgBlastContext> local_results_context;
	// STEP A try to get next ready batch from  IG_ResultsMap  &thm_all_results;  if possible
	{
	    CFastMutexGuard g1(  thm_Mutex_Output  );
	    next_batch_ready  = false;
	    results_ctx_it = thm_all_results.find( waiting_batch_number );
	    if( results_ctx_it != thm_all_results.end() ) {
		//.... some sanity/paranoid checking ..............
	        if( results_ctx_it->second.Empty() ){
		   fprintf(stderr,"MID-FORMAT:ERROR: INPROPER ITERATOR->SECOND: EMPTY. BATCH: %d\n",waiting_batch_number-1);
		   exit(0);
		}
	        if( results_ctx_it->first != results_ctx_it->second->m_batch_number ){
		   fprintf(stderr,"MID-FORMAT:ERROR: INPROPER ORDDER FOR BATCH: %d %d\n",results_ctx_it->first, results_ctx_it->second->m_batch_number);
		   exit(0);
	        }
		//........................................
		next_batch_ready = true;
		// map iterator should be save, but let's get data from it
		// batch is ready, store references object with reasult
		local_results_context.Reset(  results_ctx_it->second ); 
		thm_all_results.erase( results_ctx_it ) ; 
	    }
	    if( thm_any_started && !next_batch_ready && (thm_run_thread_count<=0) ) {
		//workers are NOT running, no new batches
		break; // exit run loop
	    }
	    //else {
	    //	workers_are_running = true; 
	    //}
	    //if( thm_run_thread_count > 0 ) workers_are_running = true; else workers_are_running = false;
	}
	// STEP B
	// IF NOT READY and 
	if( !next_batch_ready  )
	{
	    // OK, some threads are rinning, wait on semaphore
	    thm_new_batch_done.TryWait(1U);
	    continue; // TO STEP A
	}
	waiting_batch_number++; // got a batch 
	//============================================================

	//for( results_ctx_it = m_all_results.begin(); results_ctx_it != m_all_results.end(); results_ctx_it++)
	{
	    CRef<CScope> scope;
            CRef<CSearchResultSet> results;
	    CRef<CBlastQueryVector> query;

	    //scope.Reset( results_ctx_it->second->m_scope );
	    //results.Reset( results_ctx_it->second->m_results );
	    //query.Reset(  results_ctx_it->second->m_queries );


	    scope.Reset(   local_results_context->m_scope );
	    results.Reset( local_results_context->m_results );
	    query.Reset(   local_results_context->m_queries );

	    CRef <CBlastFormat> fmt;
            
            fmt.Reset( new  CBlastFormat(l_opt, *l_blastdb_full,
                                         output_choice,
                                         parse_defline,
                                         out_stream,
                                         num_description,
                                         l_num_alignments,
                                         *scope,
                                         matrix_name,
                                         show_gi,
                                         show_html,
                                         query_gcode,
                                         db_gcode,
                                         sum_stats,
                                         false,
                                         filter_algo,
                                         custom_format,
                                         is_megablast, //thm_CmdLineArgs->GetTask() == "megablast",
                                         index_load,
                                         &*l_ig_opts) );




            BlastFormatter_PreFetchSequenceData(*results, scope, l_fmt_output_choice );

        
            for( vector< CRef<CSearchResults> >::const_iterator result = results->begin(); result != results->end(); result ++) { //ITERATE(CSearchResultSet, result, *results) 
                CBlastFormat::SClone clone_info;
                SCloneNuc clone_nuc;
                CIgBlastResults &ig_result = *const_cast<CIgBlastResults *>
                        (dynamic_cast<const CIgBlastResults *>(&(**result)));
                
                fmt->PrintOneResultSet(ig_result, query, clone_info, true, (align_index == 0 ? true:false) ) ; 
                align_index ++;
                thm_total_input ++;
                // if ( clone_info.na != NcbiEmptyString && clone_info.aa != NcbiEmptyString)
                if ( !clone_info.na.empty() && !clone_info.aa.empty() ){

                    clone_nuc.na = clone_info.na;
                    clone_nuc.chain_type = clone_info.chain_type;
                    clone_nuc.v_gene = clone_info.v_gene;
                    clone_nuc.d_gene = clone_info.d_gene;
                    clone_nuc.j_gene = clone_info.j_gene;
               
                    SAaInfo* info = new SAaInfo;
                    info->count = 1;
                    info->seqid = clone_info.seqid;
                    info->all_seqid = clone_info.seqid;
                    info->min_identity = clone_info.identity;
                    info->max_identity = clone_info.identity;
                    info->total_identity = clone_info.identity;
                        
                    SAaStatus aa_status;
                    aa_status.aa = clone_info.aa;
                    aa_status.productive = clone_info.productive;
                    CloneInfo::iterator iter = thm_Clone.find(clone_nuc); 
                    if (iter != thm_Clone.end()) {
                        AaMap::iterator iter2 = iter->second->find(aa_status);
                        if (iter2 != (*iter->second).end()) {
                            if (info->min_identity < iter2->second->min_identity) {
                                iter2->second->min_identity = info->min_identity;
                            }
                            if (info->max_identity > iter2->second->max_identity) {
                                iter2->second->max_identity = info->max_identity;
                            }
                            iter2->second->total_identity += info->total_identity;
                            iter2->second->count  ++;
                            iter2->second->all_seqid = iter2->second->all_seqid + "," + info->seqid; 
                            delete info;
                        } else {
                            (*iter->second).insert(AaMap::value_type(aa_status, info));
                        }
                        
                    } else {
                        AaMap* aa_info = new AaMap; // new loc
                        (*aa_info)[aa_status] = info;
                        thm_Clone.insert(CloneInfo::value_type(clone_nuc, aa_info));
                    }
                }
            } // cycle by results

	}
    } // while end
    } // try end
    catch (const ncbi::CException& e) {
	string msg = e.ReportThis(eDPF_ErrCodeExplanation);
	const CStackTrace* stack_trace = e.GetStackTrace();
	if (stack_trace) {
	    CNcbiOstrstream os;
	    os << '\n';
	    stack_trace->Write(os);
	}
	fprintf(stderr,"WORKER: T%u BATCH # %d CEXCEPTION: %s\n",thm_tid,waiting_batch_number,msg.c_str());
	CSignal::Raise( CSignal::eSignal_SEGV );
    } catch (const std::exception& e) {
	fprintf(stderr,"WORKER: T%u BATCH # %d EXCEPTION: %s\n",thm_tid, waiting_batch_number,e.what());
	CSignal::Raise( CSignal::eSignal_SEGV );
    } catch (...) {
	fprintf(stderr,"WORKER: T%u BATCH # %d GENERAL EXCEPTION \n",thm_tid, waiting_batch_number);
	CSignal::Raise( CSignal::eSignal_SEGV );
    }


    return (void*) NULL; 
}
void CIgBlastnApp::CIgFormatter::OnExit(void){
}

// ================================================================================================
void CIgBlastnApp::Init_Worker_Threads( int thread_num )
{
    for( int thread_cnt = 0; thread_cnt < thread_num ; thread_cnt ++ ){
	    // 
	const CArgs& args = GetArgs(); 
	CRef< CIgWorker > one_worker( new CIgWorker(args,
		    				    m_CmdLineArgs,
		    				    m_Mutex_Input,
		    				    m_Mutex_Output,
		    				    m_Mutex_Notify,
		    				    m_Mutex_Global,
		    			            m_all_results,    //global: where to store results
						    m_max_batch,
						    m_ig_args,        //global:
						    m_opts_hndl,      //global
						    m_blastdb,        // global
						    m_input,          //global: where to get quiries to search
						    m_run_thread_count, // global:
						    m_new_batch_done,   // global: semaphore to notify
						    m_any_started ) ); // global: any worker thread started
	m_workers.push_back( one_worker );
    }
}
void CIgBlastnApp::Init_Formatter_Thread(void){
    // 
    CRef < CIgFormatter > one_formatter( new CIgFormatter(m_CmdLineArgs,
							  m_opts_hndl, 
							  m_Mutex_Input,
							  m_Mutex_Output,
							  m_Mutex_Notify,
							  m_Mutex_Global,
							  m_all_results,
							  m_total_input,
							  m_max_batch,
							  m_run_thread_count,
							  m_new_batch_done,
							  m_Clone,
							  m_any_started) );
    m_formatter.Reset( one_formatter ); 

}

void CIgBlastnApp::Run_Worker_Threads(bool is_detached){
    for( size_t ndx = 0; ndx < m_workers.size(); ndx++ ){
	m_workers[ndx]->Run();
	if( is_detached) m_workers[ndx]->Detach();
    }
}
void CIgBlastnApp::Join_Worker_Threads(void ){
    for( size_t ndx = 0; ndx < m_workers.size(); ndx++ ){
	m_workers[ndx]->Join();
    }
}
void CIgBlastnApp::Run_Formatter_Threads(void){
}

// ================================================================================================
void CIgBlastnApp::Init()
{
    // formulate command line arguments
    
    m_CmdLineArgs.Reset(new CIgBlastnAppArgs());

    // read the command line
    
    HideStdArgs(fHideLogfile | fHideConffile | fHideFullVersion | fHideXmlHelp | fHideDryRun);
    SetupArgDescriptions(m_CmdLineArgs->SetCommandLine());
    
    m_run_thread_count = 0;
    m_worker_thread_num =1;
    m_sra_src = NULL;
    
}
unsigned int  CIgBlastnApp::x_CountUserBatches(CBlastInputSourceConfig &iconfig, int batch_size )
{
    unsigned int  l_batch_count=0;
    CRef<CScope>  l_scope(new CScope(*CObjectManager::GetInstance()));
    CRef<CBlastFastaInputSource>            l_fasta;
    l_fasta.Reset( new CBlastFastaInputSource(m_CmdLineArgs->GetInputStream(), iconfig));
    CRef<CBlastInput> l_input;
    l_input.Reset( new CBlastInput(l_fasta.GetPointer(), batch_size));

    while( !l_input->End() ) {
	l_batch_count++;
	l_input->GetNextSeqBatch(*l_scope);
    }
    return  l_batch_count;
}

int CIgBlastnApp::Run(void)
{
    int status = BLAST_EXIT_SUCCESS;
    m_any_started = false; // no threads are running
    try {
	//################ Main thread initalization part - start ##############################################
        // Allow the fasta reader to complain on invalid sequence input
        SetDiagPostLevel(eDiag_Warning);

        /*** Get the BLAST options ***/
        const CArgs& args = GetArgs();
       
        m_opts_hndl.Reset(&*m_CmdLineArgs->SetOptions(args));

        //const CBlastOptions& opt = m_opts_hndl->GetOptions();
	m_worker_thread_num = m_CmdLineArgs->GetNumThreads();

        //do not allow multi-threading for remote blast
        if (m_CmdLineArgs->ExecuteRemotely()){
            m_worker_thread_num = 1;
        }
        /*** Get the query sequence(s) ***/
        m_query_opts = m_CmdLineArgs->GetQueryOptionsArgs();
        SDataLoaderConfig dlconfig(m_query_opts->QueryIsProtein());
        dlconfig.OptimizeForWholeLargeSequenceRetrieval();
        CBlastInputSourceConfig iconfig(dlconfig, m_query_opts->GetStrand(),
                                     m_query_opts->UseLowercaseMasks(),
                                     m_query_opts->GetParseDeflines(),
                                     m_query_opts->GetRange());
        iconfig.SetQueryLocalIdMode();
	//.......................................................
	// construct input source from file or SRA accession
	if( args[kArgSraAccession] && (args[kArgSraAccession].AsString() != "") ) {
	    // assuming SRA SUN accessions
	    vector < string >  all_sra_runs;
	    string sra_run_accessions = args[kArgSraAccession].AsString();
	    vector<string> run_arr;
	    string delim = ", ";
	    NStr::Split( sra_run_accessions, delim, run_arr );

	    for( int ndx=0; ndx < (int)run_arr.size(); ndx ++){
		string one_run = NStr::TruncateSpaces( run_arr[ndx]);
		all_sra_runs.push_back( one_run );
	    }
	    try
	    { 
		m_sra_src.Reset(new CSraInputSource( all_sra_runs ) ) ;
	    }
	    catch (const CException& e) {
		ERR_POST(e);
		cout << "Error: "<<e.what() << endl;
		cout <<"Error: invalid SRA accession(s): "<<sra_run_accessions<< endl;
		return -1;
	    }
	    catch(...){
		cout <<"Error: invalid SRA accessions: "<<sra_run_accessions<< endl;
		return -1;
	    }
	    if( m_sra_src.Empty() ){
		cout <<"Error: invalid SRA accession "<<sra_run_accessions<<endl;
		return -1;
	    }
	    //
	    m_input.Reset( new CBlastInput( m_sra_src, 10000));
	} // end if SRA run accession provided
	else 
	{   
	    m_fasta.Reset( new CBlastFastaInputSource(m_CmdLineArgs->GetInputStream(), iconfig));
	    m_input.Reset( new CBlastInput(m_fasta.GetPointer(), 10000));
	}

	

        /*** Initialize igblast database/subject and options ***/
        m_ig_args.Reset(m_CmdLineArgs->GetIgBlastArgs());
        m_ig_opts =  m_ig_args->GetIgBlastOptions();
        
        //this is just for formatter to print out correct penalty info since
        //igblast does not have standard penalty parameter
        m_opts_hndl->SetOptions().SetMismatchPenalty(m_ig_opts->m_V_penalty);


        /*** Initialize the database/subject ***/
        CRef<CBlastDatabaseArgs> db_args(m_CmdLineArgs->GetBlastDatabaseArgs());
        CRef<CFormattingArgs> fmt_args(m_CmdLineArgs->GetFormattingArgs());
        if (db_args->GetDatabaseName() == kEmptyStr && 
            db_args->GetSubjects().Empty()) {
            CSearchDatabase sdb(m_ig_opts->m_Db[0]->GetDatabaseName() + " " +
                                m_ig_opts->m_Db[1]->GetDatabaseName() + " " +
                                m_ig_opts->m_Db[2]->GetDatabaseName(), 
                                CSearchDatabase::eBlastDbIsNucleotide);
            m_blastdb_full.Reset(new CLocalDbAdapter(sdb));
            m_blastdb.Reset(&(*(m_ig_opts->m_Db[0])));
        } else {
            CRef<CScope> temp_scope(new CScope(*CObjectManager::GetInstance()));
            InitializeSubject(db_args, m_opts_hndl, m_CmdLineArgs->ExecuteRemotely(),
                              m_blastdb, temp_scope);
            if (m_CmdLineArgs->ExecuteRemotely()) {
                m_blastdb_full.Reset(&(*m_blastdb));
            } else {
                CSearchDatabase sdb(m_ig_opts->m_Db[0]->GetDatabaseName() + " " +
                                    m_ig_opts->m_Db[1]->GetDatabaseName() + " " +
                                    m_ig_opts->m_Db[2]->GetDatabaseName() + " " +
                                    m_blastdb->GetDatabaseName(), 
                                    CSearchDatabase::eBlastDbIsNucleotide);
                m_blastdb_full.Reset(new CLocalDbAdapter(sdb));
            }
        }
        _ASSERT(m_blastdb );


        /*** Get the formatting options ***/
	//################ Main thread:  initalization part - end ################################################
 
        //formatter.PrintProlog();
        m_total_input = 0;
	//################ Main thread:  run worker & format threads - start #####################################
	{
	    Init_Worker_Threads( m_worker_thread_num );
	    Init_Formatter_Thread();

 	    bool run_detach = false;

	    Run_Worker_Threads( false /* run_detach */  ); // 
	    m_formatter->Run();

	    // join workers first, formatter next
	    if( !run_detach ) { 
	    	Join_Worker_Threads();
	    }
	    m_formatter->Join();
	}
	//################ Main thread:  run worker & format threads - end  #####################################

        CNcbiOstream* outfile = NULL;
            
        if (args.Exist("clonotype_out") && args["clonotype_out"] && 
            args["clonotype_out"].AsString() != NcbiEmptyString) {
            outfile = &(args["clonotype_out"].AsOutputFile());
        } else if (fmt_args->GetFormattedOutputChoice() != CFormattingArgs::eAirrRearrangement){
            outfile = &m_CmdLineArgs->GetOutputStream();
        }

        if (outfile) {
        typedef vector<pair<const SCloneNuc*,  AaMap*> * > MapVec;
        MapVec map_vec; 
	// POST FORMAT
	{

        //sort by clone abundance
        ITERATE(CloneInfo, iter, m_Clone) {
            pair<const SCloneNuc*, AaMap*> *data = new pair<const SCloneNuc*, AaMap* > (&(iter->first), iter->second); 
            map_vec.push_back(data); 
        }
        stable_sort(map_vec.begin(), map_vec.end(), x_SortByCount);
        
        int total_elements = 0;
        int total_unique_clones = 0;
        ITERATE(MapVec, iter, map_vec) {
            ITERATE(AaMap, iter2, *((*iter)->second)){
                total_elements += iter2->second->count;
                total_unique_clones ++;
            }
        }


        *outfile << "\nTotal queries = " << m_total_input << endl;
        *outfile << "Total identifiable CDR3 = " << total_elements << endl;
        *outfile << "Total unique clonotypes = " << total_unique_clones << endl;
        *outfile << endl;
        
        if (!(m_ig_opts->m_IsProtein) && total_elements > 1) {
            *outfile << "\n" << "#Clonotype summary.  A particular clonotype includes any V(D)J rearrangements that have the same germline V(D)J gene segments, the same productive/non-productive status and the same CDR3 nucleotide as well as amino sequence (Those having the same CDR3 nucleotide but different amino acid sequence or productive/non-productive status due to frameshift in V or J gene are assigned to a different clonotype.  However, their clonotype identifers share the same prefix, for example, 6a, 6b).  Fields (tab-delimited) are clonotype identifier, representative query sequence name, count, frequency (%), CDR3 nucleotide sequence, CDR3 amino acid sequence, productive status, chain type, V gene, D gene, J gene\n" << endl;
            
            int count = 1; 
            string suffix = "abcdefghijklmnop";  //4x4 possibility = 16.  empty string included.
            
            ITERATE(MapVec, iter, map_vec) {
                if (count > args["num_clonotype"].AsInteger()) {
                    break;
                }

                int aa_count = 0;
                ITERATE(AaMap, iter2, *((*iter)->second)){
                    
                    double frequency = ((double) iter2->second->count)/total_elements*100;

                    string clone_name = NStr::IntToString(count);   
                    if ((*iter)->second->size() > 1) {
                        clone_name += suffix[aa_count];
                    }
                
                    *outfile << std::setprecision(3) << clone_name << "\t"
                        
                        <<iter2->second->seqid<<"\t"
                        <<iter2->second->count<<"\t"
                        <<frequency<<"\t"
                        <<(*iter)->first->na<<"\t"
                        <<iter2->first.aa<<"\t"
                        <<iter2->first.productive<<"\t"
                        <<(*iter)->first->chain_type<<"\t"
                        <<(*iter)->first->v_gene<<"\t"
                        <<(*iter)->first->d_gene<<"\t"
                        <<(*iter)->first->j_gene<<"\t"
                        <<endl;
                    aa_count ++;
                }
                count ++;
            }

            count = 1;
            *outfile << "\n#All query sequences grouped by clonotypes.  Fields (tab-delimited) are clonotype identifier, count, frequency (%), min similarity to top germline V gene (%), max similarity to top germline V gene (%), average similarity to top germline V gene (%), query sequence name (multiple names are separated by a comma if applicable)"<< endl << endl;
            ITERATE(MapVec, iter, map_vec) {
                if (count > args["num_clonotype"].AsInteger()) {
                    break;
                }
                int aa_count = 0;
                ITERATE(AaMap, iter2, *((*iter)->second)){
                    string clone_name = NStr::IntToString(count);   
                    if ((*iter)->second->size() > 1) {
                        clone_name += suffix[aa_count];
                    }
                   
                    double frequency = ((double) iter2->second->count)/total_elements*100;
                    *outfile << std::setprecision(3) 
                             << clone_name << "\t"
                             << iter2->second->count<<"\t"
                             << frequency << "\t"
                             << iter2->second->min_identity*100<<"\t"
                             << iter2->second->max_identity*100<<"\t"
                             << iter2->second->total_identity/iter2->second->count*100<<"\t"
                             << iter2->second->all_seqid<<"\t" << endl;
                    
                    aa_count ++;
                }
                count ++;
            }
        }

	} // post format 
       
	// deallocate

        ITERATE(MapVec, iter, map_vec) {
            delete *iter;
        }
     

        if (fmt_args->GetFormattedOutputChoice() == CFormattingArgs::eFlatQueryAnchoredIdentities ||
	    fmt_args->GetFormattedOutputChoice() == CFormattingArgs::eFlatQueryAnchoredNoIdentities) {
            CRef <CBlastFormat> print_fmt;
            CRef<CScope> print_scope;
            const CBlastOptions& opt = m_opts_hndl->GetOptions();
            print_scope.Reset(new CScope(*CObjectManager::GetInstance()));
            print_fmt.Reset( new  CBlastFormat(opt, *m_blastdb_full,
                                               fmt_args->GetFormattedOutputChoice(),
                                               true,
                                               m_CmdLineArgs->GetOutputStream(),
                                               fmt_args->GetNumDescriptions(),
                                               fmt_args->GetNumAlignments(),
                                               *print_scope,
                                               opt.GetMatrixName(),
                                               fmt_args->ShowGis(),
                                               fmt_args->DisplayHtmlOutput(),
                                               opt.GetQueryGeneticCode(),
                                               opt.GetDbGeneticCode(),
                                               opt.GetSumStatisticsMode(),
                                               false,
                                               m_blastdb->GetFilteringAlgorithm(),
                                               fmt_args->GetCustomOutputFormatSpec(),
                                               m_CmdLineArgs->GetTask() == "megablast",
                                               opt.GetMBIndexLoaded(),
                                               &*m_ig_opts));
            print_fmt->PrintEpilog(opt);
        } else if (fmt_args->GetFormattedOutputChoice() == CFormattingArgs::eTabularWithComments) {
            m_CmdLineArgs->GetOutputStream() << "# BLAST processed " << m_total_input << " queries" << endl;
        }
        }
        if (m_CmdLineArgs->ProduceDebugOutput()) {
            m_opts_hndl->GetOptions().DebugDumpText(NcbiCerr, "BLAST options", 1);
        }
        //deallocate
        ITERATE(CloneInfo, iter, m_Clone) {
            ITERATE(AaMap, iter2, *(iter->second)){
                delete iter2->second;
            }
            delete iter->second;
        }

    } CATCH_ALL(status)
    return status;
}

#ifndef SKIP_DOXYGEN_PROCESSING
int main(int argc, const char* argv[] /*, const char* envp[]*/)
{
    return CIgBlastnApp().AppMain(argc, argv, 0, eDS_Default, "");
}
#endif /* SKIP_DOXYGEN_PROCESSING */
