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

#ifndef __IGBLASTN_HPP__
#define __IGBLASTN_HPP__


#include "igblast_types.hpp"



class CIgBlastnApp : public CNcbiApplication
{
public:
    class CIgBlastContext : public CObject {
	public:
	    int                     m_batch_number;
	    CRef<CSearchResultSet>  m_results;
	    CRef<CBlastQueryVector> m_queries;
	    CRef<CScope>            m_scope;

    };

    typedef std::map< int , CRef<CIgBlastContext> > IG_ResultsMap ;

    class CiGCommonBlock : public CObject {
	public:

    };

    // START: WORKER THREAD DECLARATION ===========================
    // USES:
    //        m_Mutex            --  for access controll
    //        m_run_thread_count -- +1 on start -1 on exit. used by formatter thread exit condition
    //        m_max_batch        -- +1 if next batch was succesfuly taken from input
    //        m_input            -- to get data to search
    //        m_all_results      -- to store results
    //        m_new_batch_done   -- to notify formater thread fo new data available
    class  CIgWorker : public CThread{
	public:
	    CIgWorker(    const CArgs&  args,
		          CRef<CIgBlastnAppArgs> cmd_line_args,
		    	  CFastMutex &mx_input, 
		    	  CFastMutex &mx_output, 
		    	  CFastMutex &mx_notify, 
		    	  CFastMutex &mx_global, 
			  IG_ResultsMap &resmap, 
			  int &maxbatch,  
			  CRef<CIgBlastArgs> igargs,  
			  CRef<CBlastOptionsHandle> opts_hndl,  
			  CRef<CLocalDbAdapter> blastdb, 
			  CRef<CBlastInput> user_input, 
			  int &th_count, 
			  CSemaphore &batct_notify,
			  bool &any_started) :
		thm_args( args ),
		thm_CmdLineArgs( cmd_line_args ),
		thm_Mutex_Input(mx_input), 
		thm_Mutex_Output(mx_output), 
		thm_Mutex_Notify(mx_notify), 
		thm_Mutex_Global(mx_global), 
		thm_all_results(resmap),
		thm_max_batch(maxbatch),
		thm_ig_args( igargs ),
		thm_opts_hndl( opts_hndl ),
		thm_blastdb( blastdb ), 
		thm_input(user_input),
		thm_run_thread_count(th_count),
		thm_new_batch_done(batct_notify),
		thm_any_started( any_started )
	    { }
	    virtual void* Main(void);
	    virtual void OnExit(void);

	private:
	    const CArgs&              thm_args;
	    CRef<CIgBlastnAppArgs>    thm_CmdLineArgs;
    	    CFastMutex                &thm_Mutex_Input;
    	    CFastMutex                &thm_Mutex_Output;
    	    CFastMutex                &thm_Mutex_Notify;
    	    CFastMutex                &thm_Mutex_Global;
	    IG_ResultsMap             &thm_all_results;
	    int                       &thm_max_batch; // indicate # but taken from m_input
    	    CRef<CIgBlastArgs>        thm_ig_args;
            CRef<CBlastOptionsHandle> thm_opts_hndl;
	    CRef<CLocalDbAdapter>     thm_blastdb;
	    CRef<CBlastInput>         thm_input;
	    int                       &thm_run_thread_count;
	    CSemaphore                &thm_new_batch_done;
	    int                       thm_tid;
    	    CStopWatch                thm_sw;
    	    bool                      &thm_any_started;
    };
    // END:   WORKER THREAD DECLARATION ===========================
    // START: FORMATTER THREAD DECLARATION ========================
    // USES:
    //        m_Mutex            --  for access controll
    //        m_run_thread_count -- +1 on start -1 on exit. used by formatter thread exit condition
    //        m_max_batch        -- +1 if next batch was succesfuly taken from input
    //        m_input            -- NOT USED
    //        m_all_results      -- to read/process results
    //        m_new_batch_done   -- to listen for notify for  new data available
    //        m_Clone            -- to store collected clone info
    //
    class CIgFormatter : public CThread{
	public:
	    CIgFormatter(CRef<CIgBlastnAppArgs> cmd_line_args,
		         CRef<CBlastOptionsHandle> opts_hndl,
		         CFastMutex &mx_input,
		         CFastMutex &mx_output,
		         CFastMutex &mx_notify,
		         CFastMutex &mx_global,
			 IG_ResultsMap &resmap,
    	                 int &total_input,
			 int &maxbatch,
			 int &th_count,
			 CSemaphore &batct_notify,
			 CloneInfo &cinfo,
			 bool &any_started) :
		thm_CmdLineArgs( cmd_line_args ),
		thm_opts_hndl( opts_hndl ),
		thm_Mutex_Input(mx_input), 
		thm_Mutex_Output(mx_output), 
		thm_Mutex_Notify(mx_notify), 
		thm_Mutex_Global(mx_global), 
		thm_all_results(resmap),
		thm_max_batch(maxbatch),
		thm_total_input(total_input),
		thm_run_thread_count(th_count),
		thm_new_batch_done(batct_notify),
		thm_Clone( cinfo),
		thm_any_started(any_started)
	    { }

	    virtual void* Main(void);
	    virtual void OnExit(void);

	private:    
	    CRef<CIgBlastnAppArgs>    thm_CmdLineArgs;
            CRef<CBlastOptionsHandle> thm_opts_hndl;
    	    CFastMutex        &thm_Mutex_Input;
    	    CFastMutex        &thm_Mutex_Output;
    	    CFastMutex        &thm_Mutex_Notify;
    	    CFastMutex        &thm_Mutex_Global;
	    IG_ResultsMap     &thm_all_results;
	    int               &thm_max_batch; // indicate # but taken from m_input
	    int       	      &thm_total_input;
	    int               &thm_run_thread_count;
	    CSemaphore        &thm_new_batch_done;
    	    CloneInfo         &thm_Clone;
	    int               thm_tid;
    	    CStopWatch        thm_sw;
    	    bool              &thm_any_started;
    };
    // END:   FORMATTER THREAD DECLARATION ========================
    
    /** @inheritDoc */
    CIgBlastnApp() : m_new_batch_done(0, UINT_MAX>>1) 
    {
        CRef<CVersion> version(new CVersion());
        version->SetVersionInfo(new CIgBlastVersion());
        SetFullVersion(version);
	m_max_batch = 0;
	m_run_thread_count = 0; // number of thread running
	m_global_done = false;
    }

private:
    /** @inheritDoc */
    virtual void Init();
    /** @inheritDoc */
    virtual int Run();
    // run worker threads
    void Init_Worker_Threads( int thread_num );
    void Init_Formatter_Thread(void);
    void Join_Worker_Threads(void);
    void Run_Worker_Threads(bool is_detached );
    void Run_Formatter_Threads(void);
    unsigned int  x_CountUserBatches(CBlastInputSourceConfig &iconfig, int batch_size=10000 );

    /// This application's command line args
    CRef<CBlastFastaInputSource>            m_fasta;
    CRef<CLocalDbAdapter>		    m_blastdb_full;
    // START THREAD-COMMON-DATA-BLOCK=========================
    CRef<CIgBlastnAppArgs>                  m_CmdLineArgs; 
    CRef<CLocalDbAdapter>		    m_blastdb;
    CRef<CBlastOptionsHandle>		    m_opts_hndl;
    //OLD OK static CFastMutex              m_Mutex;
    CFastMutex                              m_Mutex_Input;
    CFastMutex                              m_Mutex_Output;
    CFastMutex                              m_Mutex_Notify;
    CFastMutex                              m_Mutex_Global;
    IG_ResultsMap                           m_all_results; 
    int                                     m_max_batch; // indicate number pf batches taken from m_input
    CRef<CIgBlastArgs>                      m_ig_args;
    CRef<CBlastInput>                       m_input;
    int                                     m_run_thread_count;
    CSemaphore                              m_new_batch_done;
    CloneInfo                               m_Clone;
    bool                                    m_global_done;
    int 				    m_total_input;
    bool                                    m_any_started;  // Formatter thread only
    // END THREAD-COMMON-DATA-BLOCK =========================
    CRef<CQueryOptionsArgs>                 m_query_opts;
    CRef<CIgBlastOptions>                   m_ig_opts;
    vector< CRef < CIgWorker >  >           m_workers;
    CRef < CIgFormatter  >                  m_formatter;
    size_t				    m_worker_thread_num;
    CRef <CSraInputSource>                  m_sra_src;
    CBlastUsageReport m_UsageReport;
};

#endif
