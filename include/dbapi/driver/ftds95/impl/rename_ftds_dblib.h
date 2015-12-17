#ifndef DBAPI_DRIVER_FTDS95_IMPL___RENAME_FTDS_DBLIB__H
#define DBAPI_DRIVER_FTDS95_IMPL___RENAME_FTDS_DBLIB__H

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
#  define bcp_batch         bcp_batch_ver95
#  define bcp_bind          bcp_bind_ver95
#  define bcp_colfmt        bcp_colfmt_ver95
#  define bcp_colfmt_ps     bcp_colfmt_ps_ver95
#  define bcp_collen        bcp_collen_ver95
#  define bcp_colptr        bcp_colptr_ver95
#  define bcp_columns       bcp_columns_ver95
#  define bcp_control       bcp_control_ver95
#  define bcp_done          bcp_done_ver95
#  define bcp_exec          bcp_exec_ver95
#  define bcp_getbatchsize  bcp_getbatchsize_ver95
#  define bcp_getl          bcp_getl_ver95
#  define bcp_init          bcp_init_ver95
#  define bcp_options       bcp_options_ver95
#  define bcp_readfmt       bcp_readfmt_ver95
#  define bcp_sendrow       bcp_sendrow_ver95
#  define copy_data_to_host_var copy_data_to_host_var_ver95
#  define dbadata           dbadata_ver95
#  define dbadlen           dbadlen_ver95
#  define dbaltcolid        dbaltcolid_ver95
#  define dbaltlen          dbaltlen_ver95
#  define dbaltop           dbaltop_ver95
#  define dbalttype         dbalttype_ver95
#  define dbaltutype        dbaltutype_ver95
#  define dbaltbind         dbaltbind_ver95
#  define dbanullbind       dbanullbind_ver95
#  define dbanydatecrack    dbanydatecrack_ver95
#  define dbbind            dbbind_ver95
#  define dbbylist          dbbylist_ver95
#  define dbcancel          dbcancel_ver95
#  define dbcanquery        dbcanquery_ver95
#  define dbchange          dbchange_ver95
#  define dbclose           dbclose_ver95
#  define dbclrbuf          dbclrbuf_ver95
#  define dbclropt          dbclropt_ver95
#  define dbcmd             dbcmd_ver95
#  define dbcmdrow          dbcmdrow_ver95
#  define dbcolinfo         dbcolinfo_ver95
#  define dbcollen          dbcollen_ver95
#  define dbcolname         dbcolname_ver95
#  define dbcolsource       dbcolsource_ver95
#  define dbcoltype         dbcoltype_ver95
#  define dbcoltypeinfo     dbcoltypeinfo_ver95
#  define dbcolutype        dbcolutype_ver95
#  define dbconvert         dbconvert_ver95
#  define dbconvert_ps      dbconvert_ps_ver95
#  define dbcount           dbcount_ver95
#  define dbcurcmd          dbcurcmd_ver95
#  define dbcurrow          dbcurrow_ver95
#  define dbdata            dbdata_ver95
#  define dbdatecmp         dbdatecmp_ver95
#  define dbdatecrack       dbdatecrack_ver95
#  define dbdatlen          dbdatlen_ver95
#  define dbdead            dbdead_ver95
#  define dberrhandle       dberrhandle_ver95
#  define dbexit            dbexit_ver95
#  define dbfcmd            dbfcmd_ver95
#  define dbfirstrow        dbfirstrow_ver95
#  define dbfreebuf         dbfreebuf_ver95
#  define dbgetchar         dbgetchar_ver95
#  define dbgetmaxprocs     dbgetmaxprocs_ver95
#  define dbgetnull         dbgetnull_ver95
#  define dbgetpacket       dbgetpacket_ver95
#  define dbgetrow          dbgetrow_ver95
#  define dbgettime         dbgettime_ver95
#  define dbgetuserdata     dbgetuserdata_ver95
#  define dbhasretstat      dbhasretstat_ver95
#  define dbinit            dbinit_ver95
#  define dbiordesc         dbiordesc_ver95
#  define dbiowdesc         dbiowdesc_ver95
#  define dbisavail         dbisavail_ver95
#  define dbiscount         dbiscount_ver95
#  define dbisopt           dbisopt_ver95
#  define dblastrow         dblastrow_ver95
#  define dblib_bound_type  dblib_bound_type_ver95
#  define dblogin           dblogin_ver95
#  define dbloginfree       dbloginfree_ver95
#  define dbmny4add         dbmny4add_ver95
#  define dbmny4cmp         dbmny4cmp_ver95
#  define dbmny4copy        dbmny4copy_ver95
#  define dbmny4minus       dbmny4minus_ver95
#  define dbmny4sub         dbmny4sub_ver95
#  define dbmny4zero        dbmny4zero_ver95
#  define dbmnycmp          dbmnycmp_ver95
#  define dbmnycopy         dbmnycopy_ver95
#  define dbmnydec          dbmnydec_ver95
#  define dbmnyinc          dbmnyinc_ver95
#  define dbmnymaxneg       dbmnymaxneg_ver95
#  define dbmnymaxpos       dbmnymaxpos_ver95
#  define dbmnyminus        dbmnyminus_ver95
#  define dbmnyzero         dbmnyzero_ver95
#  define dbmonthname       dbmonthname_ver95
#  define dbmorecmds        dbmorecmds_ver95
#  define dbmoretext        dbmoretext_ver95
#  define dbmsghandle       dbmsghandle_ver95
#  define dbname            dbname_ver95
#  define dbnextrow         dbnextrow_ver95
#  define dbnextrow_pivoted dbnextrow_pivoted_ver95
#  define dbnullbind        dbnullbind_ver95
#  define dbnumalts         dbnumalts_ver95
#  define dbnumcols         dbnumcols_ver95
#  define dbnumcompute      dbnumcompute_ver95
#  define dbnumrets         dbnumrets_ver95
/* #  define dbopen            dbopen_ver95 */
#  define dbperror          dbperror_ver95
#  define dbpivot           dbpivot_ver95
#  define dbpivot_count     dbpivot_count_ver95
#  define dbpivot_lookup_name dbpivot_lookup_name_ver95
#  define dbpivot_max       dbpivot_max_ver95
#  define dbpivot_min       dbpivot_min_ver95
#  define dbpivot_sum       dbpivot_sum_ver95
#  define dbprcollen        dbprcollen_ver95
#  define dbprhead          dbprhead_ver95
#  define dbprrow           dbprrow_ver95
#  define dbprtype          dbprtype_ver95
#  define dbreadtext        dbreadtext_ver95
#  define dbrecftos         dbrecftos_ver95
#  define dbresults         dbresults_ver95
#  define dbretdata         dbretdata_ver95
#  define dbretlen          dbretlen_ver95
#  define dbretname         dbretname_ver95
#  define dbretstatus       dbretstatus_ver95
#  define dbrettype         dbrettype_ver95
#  define dbrows            dbrows_ver95
#  define dbrows_pivoted    dbrows_pivoted_ver95
#  define dbrowtype         dbrowtype_ver95
#  define dbrpcinit         dbrpcinit_ver95
#  define dbrpcparam        dbrpcparam_ver95
#  define dbrpcsend         dbrpcsend_ver95
#  define dbsafestr         dbsafestr_ver95
#  define dbservcharset     dbservcharset_ver95
#  define dbsetavail        dbsetavail_ver95
#  define dbsetifile        dbsetifile_ver95
#  define dbsetinterrupt    dbsetinterrupt_ver95
#  define dbsetlbool        dbsetlbool_ver95
#  define dbsetllong        dbsetllong_ver95
#  define dbsetlname        dbsetlname_ver95
#  define dbsetlogintime    dbsetlogintime_ver95
#  define dbsetlversion     dbsetlversion_ver95
#  define dbsetmaxprocs     dbsetmaxprocs_ver95
#  define dbsetnull         dbsetnull_ver95
#  define dbsetopt          dbsetopt_ver95
#  define dbsetrow          dbsetrow_ver95
#  define dbsettime         dbsettime_ver95
#  define dbsetuserdata     dbsetuserdata_ver95
#  define dbsetversion      dbsetversion_ver95
#  define dbspid            dbspid_ver95
#  define dbspr1row         dbspr1row_ver95
#  define dbspr1rowlen      dbspr1rowlen_ver95
#  define dbsprhead         dbsprhead_ver95
#  define dbsprline         dbsprline_ver95
#  define dbsqlexec         dbsqlexec_ver95
#  define dbsqlok           dbsqlok_ver95
#  define dbsqlsend         dbsqlsend_ver95
#  define dbstrbuild        dbstrbuild_ver95
#  define dbstrcpy          dbstrcpy_ver95
#  define dbstrlen          dbstrlen_ver95
#  define dbtablecolinfo    dbtablecolinfo_ver95
#  define dbtds             dbtds_ver95
#  define dbtxptr           dbtxptr_ver95
#  define dbtxtimestamp     dbtxtimestamp_ver95
#  define dbuse             dbuse_ver95
#  define dbvarylen         dbvarylen_ver95
#  define dbversion         dbversion_ver95
#  define dbwillconvert     dbwillconvert_ver95
#  define dbwritetext       dbwritetext_ver95
#  define tdsdbopen         tdsdbopen_ver95
#  define _dblib_check_and_handle_interrupt \
                                     _dblib_check_and_handle_interrupt_ver95
#  define _dblib_err_handler         _dblib_err_handler_ver95
#  define _dblib_handle_err_message  _dblib_handle_err_message_ver95
#  define _dblib_handle_info_message _dblib_handle_info_message_ver95
#  define _dblib_msg_handler         _dblib_msg_handler_ver95
#  define _dblib_setTDS_version      _dblib_setTDS_version_ver95
#endif  /* NCBI_FTDS_RENAME_SYBDB */

#endif  /* DBAPI_DRIVER_FTDS95_IMPL___RENAME_FTDS_DBLIB__H */
