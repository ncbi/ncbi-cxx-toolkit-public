#ifndef DBAPI_DRIVER_FTDS64_IMPL___RENAME_FTDS_DBLIB__H
#define DBAPI_DRIVER_FTDS64_IMPL___RENAME_FTDS_DBLIB__H

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
 * Author:  Denis Vakatov
 *
 * File Description:
 *   Macros to rename FreeTDS DBLIB symbols -- to avoid their clashing
 *   with the Sybase DBLIB ones
 *
 */

#if defined(NCBI_FTDS_RENAME_SYBDB)
#  define abort_xact abort_xact_ver64
#  define bcp_batch bcp_batch_ver64
#  define bcp_bind bcp_bind_ver64
#  define bcp_colfmt bcp_colfmt_ver64
#  define bcp_colfmt_ps bcp_colfmt_ps_ver64
#  define bcp_collen bcp_collen_ver64
#  define bcp_colptr bcp_colptr_ver64
#  define bcp_columns bcp_columns_ver64
#  define bcp_control bcp_control_ver64
#  define bcp_done bcp_done_ver64
#  define bcp_exec bcp_exec_ver64
#  define bcp_getl bcp_getl_ver64
#  define bcp_init bcp_init_ver64
#  define bcp_moretext bcp_moretext_ver64
#  define bcp_options bcp_options_ver64
#  define bcp_readfmt bcp_readfmt_ver64
#  define bcp_sendrow bcp_sendrow_ver64
#  define bcp_writefmt bcp_writefmt_ver64
#  define build_xact_string build_xact_string_ver64
#  define close_commit close_commit_ver64
#  define commit_xact commit_xact_ver64
#  define dbadata dbadata_ver64
#  define dbadlen dbadlen_ver64
#  define dbaltcolid dbaltcolid_ver64
#  define dbaltlen dbaltlen_ver64
#  define dbaltop dbaltop_ver64
#  define dbalttype dbalttype_ver64
#  define dbaltutype dbaltutype_ver64
#  define dbaltbind dbaltbind_ver64
#  define dbanullbind dbanullbind_ver64
#  define dbbind dbbind_ver64
#  define dbbylist dbbylist_ver64
#  define dbcancel dbcancel_ver64
#  define dbcanquery dbcanquery_ver64
#  define dbchange dbchange_ver64
#  define dbclose dbclose_ver64
#  define dbclrbuf dbclrbuf_ver64
#  define dbclropt dbclropt_ver64
#  define dbcmd dbcmd_ver64
#  define dbcmdrow dbcmdrow_ver64
#  define dbcollen dbcollen_ver64
#  define dbcolinfo dbcolinfo_ver64
#  define dbcolname dbcolname_ver64
#  define dbcolsource dbcolsource_ver64
#  define dbcoltype dbcoltype_ver64
#  define dbcoltypeinfo dbcoltypeinfo_ver64
#  define dbcolutype dbcolutype_ver64
#  define dbconvert dbconvert_ver64
#  define dbconvert_ps dbconvert_ps_ver64
#  define dbcount dbcount_ver64
#  define dbcurcmd dbcurcmd_ver64
#  define dbcurrow dbcurrow_ver64
#  define dbdata dbdata_ver64
#  define dbdatecmp dbdatecmp_ver64
#  define dbdatecrack dbdatecrack_ver64
#  define dbdatlen dbdatlen_ver64
#  define dbdead dbdead_ver64
#  define dberrhandle dberrhandle_ver64
#  define dbexit dbexit_ver64
#  define dbfcmd dbfcmd_ver64
#  define dbfirstrow dbfirstrow_ver64
#  define dbfreebuf dbfreebuf_ver64
#  define dbgetchar dbgetchar_ver64
#  define dbgetmaxprocs dbgetmaxprocs_ver64
#  define dbgetpacket dbgetpacket_ver64
#  define dbgetrow dbgetrow_ver64
#  define dbgetuserdata dbgetuserdata_ver64
#  define dbhasretstat dbhasretstat_ver64
#  define dbinit dbinit_ver64
#  define dbiordesc dbiordesc_ver64
#  define dbiowdesc dbiowdesc_ver64
#  define dbisavail dbisavail_ver64
#  define dbisopt dbisopt_ver64
#  define dblastrow dblastrow_ver64
#  define dblogin dblogin_ver64
#  define dbloginfree dbloginfree_ver64
#  define dbmny4add dbmny4add_ver64
#  define dbmny4cmp dbmny4cmp_ver64
#  define dbmny4copy dbmny4copy_ver64
#  define dbmny4divide dbmny4divide_ver64
#  define dbmny4minus dbmny4minus_ver64
#  define dbmny4mul dbmny4mul_ver64
#  define dbmny4sub dbmny4sub_ver64
#  define dbmny4zero dbmny4zero_ver64
#  define dbmnyadd dbmnyadd_ver64
#  define dbmnycmp dbmnycmp_ver64
#  define dbmnycopy dbmnycopy_ver64
#  define dbmnydec dbmnydec_ver64
#  define dbmnydivide dbmnydivide_ver64
#  define dbmnydown dbmnydown_ver64
#  define dbmnyinc dbmnyinc_ver64
#  define dbmnyinit dbmnyinit_ver64
#  define dbmnymaxneg dbmnymaxneg_ver64
#  define dbmnymaxpos dbmnymaxpos_ver64
#  define dbmnyminus dbmnyminus_ver64
#  define dbmnymul dbmnymul_ver64
#  define dbmnyndigit dbmnyndigit_ver64
#  define dbmnyscale dbmnyscale_ver64
#  define dbmnysub dbmnysub_ver64
#  define dbmnyzero dbmnyzero_ver64
#  define dbmonthname dbmonthname_ver64
#  define dbmorecmds dbmorecmds_ver64
#  define dbmoretext dbmoretext_ver64
#  define dbmsghandle dbmsghandle_ver64
#  define dbname dbname_ver64
#  define dbnextrow dbnextrow_ver64
#  define dbnullbind dbnullbind_ver64
#  define dbnumalts dbnumalts_ver64
#  define dbnumcols dbnumcols_ver64
#  define dbnumcompute dbnumcompute_ver64
#  define dbnumrets dbnumrets_ver64
/* #  define dbopen dbopen_ver64 */
#  define dbperror dbperror_ver64
#  define dbpoll dbpoll_ver64
#  define dbprhead dbprhead_ver64
#  define dbprrow dbprrow_ver64
#  define dbprtype dbprtype_ver64
#  define dbreadtext dbreadtext_ver64
#  define dbrecftos dbrecftos_ver64
#  define dbregexec dbregexec_ver64
#  define dbreginit dbreginit_ver64
#  define dbreglist dbreglist_ver64
#  define dbregparam dbregparam_ver64
#  define dbresults dbresults_ver64
#  define dbretdata dbretdata_ver64
#  define dbretlen dbretlen_ver64
#  define dbretname dbretname_ver64
#  define dbretstatus dbretstatus_ver64
#  define dbrettype dbrettype_ver64
#  define dbrows dbrows_ver64
#  define dbrowtype dbrowtype_ver64
#  define dbrpcinit dbrpcinit_ver64
#  define dbrpcparam dbrpcparam_ver64
#  define dbrpcsend dbrpcsend_ver64
#  define dbrpwclr dbrpwclr_ver64
#  define dbrpwset dbrpwset_ver64
#  define dbsafestr dbsafestr_ver64
#  define dbservcharset dbservcharset_ver64
#  define dbsetavail dbsetavail_ver64
#  define dbsetdefcharset dbsetdefcharset_ver64
#  define dbsetdeflang dbsetdeflang_ver64
#  define dbsetifile dbsetifile_ver64
#  define dbsetinterrupt dbsetinterrupt_ver64
#  define dbsetlbool dbsetlbool_ver64
#  define dbsetllong dbsetllong_ver64
#  define dbsetlname dbsetlname_ver64
#  define dbsetlogintime dbsetlogintime_ver64
#  define dbsetlshort dbsetlshort_ver64
#  define dbsetlversion dbsetlversion_ver64
#  define dbsetmaxprocs dbsetmaxprocs_ver64
#  define dbsetopt dbsetopt_ver64
#  define dbsetrow dbsetrow_ver64
#  define dbsettime dbsettime_ver64
#  define dbsetuserdata dbsetuserdata_ver64
#  define dbsetversion dbsetversion_ver64
#  define dbspid dbspid_ver64
#  define dbspr1row dbspr1row_ver64
#  define dbspr1rowlen dbspr1rowlen_ver64
#  define dbsprhead dbsprhead_ver64
#  define dbsprline dbsprline_ver64
#  define dbsqlexec dbsqlexec_ver64
#  define dbsqlok dbsqlok_ver64
#  define dbsqlsend dbsqlsend_ver64
#  define dbstrbuild dbstrbuild_ver64
#  define dbstrcpy dbstrcpy_ver64
#  define dbstrlen dbstrlen_ver64
#  define dbtablecolinfo dbtablecolinfo_ver64
#  define dbtds dbtds_ver64
#  define dbtxptr dbtxptr_ver64
#  define dbtxtimestamp dbtxtimestamp_ver64
#  define dbuse dbuse_ver64
#  define dbvarylen dbvarylen_ver64
#  define dbversion dbversion_ver64
#  define dbwillconvert dbwillconvert_ver64
#  define dbwritetext dbwritetext_ver64
#  define open_commit open_commit_ver64
#  define remove_xact remove_xact_ver64
#  define scan_xact scan_xact_ver64
#  define start_xact start_xact_ver64
#  define stat_xact stat_xact_ver64
#  define tdsdbopen tdsdbopen_ver64
#  define _dblib_client_msg _dblib_client_msg_ver64
#  define _dblib_err_handler _dblib_err_handler_ver64
#  define _dblib_handle_err_message _dblib_handle_err_message_ver64
#  define _dblib_handle_info_message _dblib_handle_info_message_ver64
#  define _dblib_msg_handler _dblib_msg_handler_ver64
#  define _dblib_setTDS_version _dblib_setTDS_version_ver64
#endif  /* NCBI_FTDS_RENAME_SYBDB */

#endif  /* DBAPI_DRIVER_FTDS64_IMPL___RENAME_FTDS_DBLIB__H */
