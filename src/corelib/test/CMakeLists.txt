#############################################################################
# $Id$
#############################################################################

NCBI_headers("")
NCBI_disable_pch()
NCBI_project_tags(test)

NCBI_add_library(test_dll pbacktest)
NCBI_add_app(
           test_tls_object 
           test_ncbitime_mt test_ncbitime test_ncbithr coretest 
           test_ncbiargs test_ncbiargs_sample test_ncbi_limits test_ncbistr 
           test_ncbidiag_mt test_ncbireg_mt test_ncbi_system test_ncbiutil 
           test_ncbifile test_ncbidll test_semaphore_mt test_ncbiexec 
           test_ncbiexpt test_ncbi_process test_ncbi_os_unix test_ncbi_tree 
           test_plugins test_ncbidiag_p test_ncbidiag_f_mt test_objstore 
           test_hash test_param_mt test_diag_parser test_fstream_pushback 
           test_stacktrace test_tempstr test_ncbi_config test_ncbicfg 
           test_weakref test_request_control test_expr test_sub_reg 
           test_resource_info test_interprocess_lock test_ncbithr_native 
           test_ncbi_rwstream test_condvar test_base64 test_trial_check 
           test_message_mt test_ncbicntr test_ncbi_url test_trial 
           test_uncaught_exception test_ncbi_fast test_boost_mt
)
