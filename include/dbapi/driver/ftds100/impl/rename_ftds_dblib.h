#ifndef DBAPI_DRIVER_FTDS100_IMPL___RENAME_FTDS_DBLIB__H
#define DBAPI_DRIVER_FTDS100_IMPL___RENAME_FTDS_DBLIB__H

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

#include "../freetds/config.h"

#if defined(NCBI_FTDS_RENAME_SYBDB)
#  define bcp_batch         bcp_batch_ver100
#  define bcp_bind          bcp_bind_ver100
#  define bcp_colfmt        bcp_colfmt_ver100
#  define bcp_colfmt_ps     bcp_colfmt_ps_ver100
#  define bcp_collen        bcp_collen_ver100
#  define bcp_colptr        bcp_colptr_ver100
#  define bcp_columns       bcp_columns_ver100
#  define bcp_control       bcp_control_ver100
#  define bcp_done          bcp_done_ver100
#  define bcp_exec          bcp_exec_ver100
#  define bcp_getbatchsize  bcp_getbatchsize_ver100
#  define bcp_getl          bcp_getl_ver100
#  define bcp_init          bcp_init_ver100
#  define bcp_options       bcp_options_ver100
#  define bcp_readfmt       bcp_readfmt_ver100
#  define bcp_sendrow       bcp_sendrow_ver100
#  define copy_data_to_host_var copy_data_to_host_var_ver100
#  define dbadata           dbadata_ver100
#  define dbadlen           dbadlen_ver100
#  define dbaltcolid        dbaltcolid_ver100
#  define dbaltlen          dbaltlen_ver100
#  define dbaltop           dbaltop_ver100
#  define dbalttype         dbalttype_ver100
#  define dbaltutype        dbaltutype_ver100
#  define dbaltbind         dbaltbind_ver100
#  define dbanullbind       dbanullbind_ver100
#  define dbanydatecrack    dbanydatecrack_ver100
#  define dbbind            dbbind_ver100
#  define dbbylist          dbbylist_ver100
#  define dbcancel          dbcancel_ver100
#  define dbcanquery        dbcanquery_ver100
#  define dbchange          dbchange_ver100
#  define dbclose           dbclose_ver100
#  define dbclrbuf          dbclrbuf_ver100
#  define dbclropt          dbclropt_ver100
#  define dbcmd             dbcmd_ver100
#  define dbcmdrow          dbcmdrow_ver100
#  define dbcolinfo         dbcolinfo_ver100
#  define dbcollen          dbcollen_ver100
#  define dbcolname         dbcolname_ver100
#  define dbcolsource       dbcolsource_ver100
#  define dbcoltype         dbcoltype_ver100
#  define dbcoltypeinfo     dbcoltypeinfo_ver100
#  define dbcolutype        dbcolutype_ver100
#  define dbconvert         dbconvert_ver100
#  define dbconvert_ps      dbconvert_ps_ver100
#  define dbcount           dbcount_ver100
#  define dbcurcmd          dbcurcmd_ver100
#  define dbcurrow          dbcurrow_ver100
#  define dbdata            dbdata_ver100
#  define dbdatecmp         dbdatecmp_ver100
#  define dbdatecrack       dbdatecrack_ver100
#  define dbdatlen          dbdatlen_ver100
#  define dbdead            dbdead_ver100
#  define dberrhandle       dberrhandle_ver100
#  define dbexit            dbexit_ver100
#  define dbfcmd            dbfcmd_ver100
#  define dbfirstrow        dbfirstrow_ver100
#  define dbfreebuf         dbfreebuf_ver100
#  define dbgetchar         dbgetchar_ver100
#  define dbgetmaxprocs     dbgetmaxprocs_ver100
#  define dbgetnull         dbgetnull_ver100
#  define dbgetpacket       dbgetpacket_ver100
#  define dbgetrow          dbgetrow_ver100
#  define dbgettime         dbgettime_ver100
#  define dbgetuserdata     dbgetuserdata_ver100
#  define dbhasretstat      dbhasretstat_ver100
#  define dbinit            dbinit_ver100
#  define dbiordesc         dbiordesc_ver100
#  define dbiowdesc         dbiowdesc_ver100
#  define dbisavail         dbisavail_ver100
#  define dbiscount         dbiscount_ver100
#  define dbisopt           dbisopt_ver100
#  define dblastrow         dblastrow_ver100
#  define dblib_bound_type  dblib_bound_type_ver100
#  define dblogin           dblogin_ver100
#  define dbloginfree       dbloginfree_ver100
#  define dbmny4add         dbmny4add_ver100
#  define dbmny4cmp         dbmny4cmp_ver100
#  define dbmny4copy        dbmny4copy_ver100
#  define dbmny4minus       dbmny4minus_ver100
#  define dbmny4sub         dbmny4sub_ver100
#  define dbmny4zero        dbmny4zero_ver100
#  define dbmnycmp          dbmnycmp_ver100
#  define dbmnycopy         dbmnycopy_ver100
#  define dbmnydec          dbmnydec_ver100
#  define dbmnyinc          dbmnyinc_ver100
#  define dbmnymaxneg       dbmnymaxneg_ver100
#  define dbmnymaxpos       dbmnymaxpos_ver100
#  define dbmnyminus        dbmnyminus_ver100
#  define dbmnyzero         dbmnyzero_ver100
#  define dbmonthname       dbmonthname_ver100
#  define dbmorecmds        dbmorecmds_ver100
#  define dbmoretext        dbmoretext_ver100
#  define dbmsghandle       dbmsghandle_ver100
#  define dbname            dbname_ver100
#  define dbnextrow         dbnextrow_ver100
#  define dbnextrow_pivoted dbnextrow_pivoted_ver100
#  define dbnullbind        dbnullbind_ver100
#  define dbnumalts         dbnumalts_ver100
#  define dbnumcols         dbnumcols_ver100
#  define dbnumcompute      dbnumcompute_ver100
#  define dbnumrets         dbnumrets_ver100
#  ifdef HAVE_DBOPEN
#    define dbopen            dbopen_ver100
#  endif
#  define dbperror          dbperror_ver100
#  define dbpivot           dbpivot_ver100
#  define dbpivot_count     dbpivot_count_ver100
#  define dbpivot_lookup_name dbpivot_lookup_name_ver100
#  define dbpivot_max       dbpivot_max_ver100
#  define dbpivot_min       dbpivot_min_ver100
#  define dbpivot_sum       dbpivot_sum_ver100
#  define dbprcollen        dbprcollen_ver100
#  define dbprhead          dbprhead_ver100
#  define dbprrow           dbprrow_ver100
#  define dbprtype          dbprtype_ver100
#  define dbreadtext        dbreadtext_ver100
#  define dbrecftos         dbrecftos_ver100
#  define dbresults         dbresults_ver100
#  define dbretdata         dbretdata_ver100
#  define dbretlen          dbretlen_ver100
#  define dbretname         dbretname_ver100
#  define dbretstatus       dbretstatus_ver100
#  define dbrettype         dbrettype_ver100
#  define dbrows            dbrows_ver100
#  define dbrows_pivoted    dbrows_pivoted_ver100
#  define dbrowtype         dbrowtype_ver100
#  define dbrpcinit         dbrpcinit_ver100
#  define dbrpcparam        dbrpcparam_ver100
#  define dbrpcsend         dbrpcsend_ver100
#  define dbsafestr         dbsafestr_ver100
#  define dbservcharset     dbservcharset_ver100
#  define dbsetavail        dbsetavail_ver100
#  define dbsetifile        dbsetifile_ver100
#  define dbsetinterrupt    dbsetinterrupt_ver100
#  define dbsetlbool        dbsetlbool_ver100
#  define dbsetllong        dbsetllong_ver100
#  define dbsetlname        dbsetlname_ver100
#  define dbsetlogintime    dbsetlogintime_ver100
#  define dbsetlversion     dbsetlversion_ver100
#  define dbsetmaxprocs     dbsetmaxprocs_ver100
#  define dbsetnull         dbsetnull_ver100
#  define dbsetopt          dbsetopt_ver100
#  define dbsetrow          dbsetrow_ver100
#  define dbsettime         dbsettime_ver100
#  define dbsetuserdata     dbsetuserdata_ver100
#  define dbsetversion      dbsetversion_ver100
#  define dbspid            dbspid_ver100
#  define dbspr1row         dbspr1row_ver100
#  define dbspr1rowlen      dbspr1rowlen_ver100
#  define dbsprhead         dbsprhead_ver100
#  define dbsprline         dbsprline_ver100
#  define dbsqlexec         dbsqlexec_ver100
#  define dbsqlok           dbsqlok_ver100
#  define dbsqlsend         dbsqlsend_ver100
#  define dbstrbuild        dbstrbuild_ver100
#  define dbstrcpy          dbstrcpy_ver100
#  define dbstrlen          dbstrlen_ver100
#  define dbtablecolinfo    dbtablecolinfo_ver100
#  define dbtds             dbtds_ver100
#  define dbtxptr           dbtxptr_ver100
#  define dbtxtimestamp     dbtxtimestamp_ver100
#  define dbuse             dbuse_ver100
#  define dbvarylen         dbvarylen_ver100
#  define dbversion         dbversion_ver100
#  define dbwillconvert     dbwillconvert_ver100
#  define dbwritetext       dbwritetext_ver100
#  define tdsdbopen         tdsdbopen_ver100
#  define _dblib_check_and_handle_interrupt \
                                     _dblib_check_and_handle_interrupt_ver100
#  define _dblib_err_handler         _dblib_err_handler_ver100
#  define _dblib_handle_err_message  _dblib_handle_err_message_ver100
#  define _dblib_handle_info_message _dblib_handle_info_message_ver100
#  define _dblib_msg_handler         _dblib_msg_handler_ver100
#  define _dblib_setTDS_version      _dblib_setTDS_version_ver100
#endif  /* NCBI_FTDS_RENAME_SYBDB */

#endif  /* DBAPI_DRIVER_FTDS100_IMPL___RENAME_FTDS_DBLIB__H */
