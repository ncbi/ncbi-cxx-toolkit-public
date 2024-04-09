#ifndef DBAPI_DRIVER_FTDS14_IMPL___RENAME_FTDS_DBLIB__H
#define DBAPI_DRIVER_FTDS14_IMPL___RENAME_FTDS_DBLIB__H

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

#if NCBI_FTDS_RENAME_SYBDB
#  define bcp_batch             bcp_batch_ver14
#  define bcp_bind              bcp_bind_ver14
#  define bcp_colfmt            bcp_colfmt_ver14
#  define bcp_colfmt_ps         bcp_colfmt_ps_ver14
#  define bcp_collen            bcp_collen_ver14
#  define bcp_colptr            bcp_colptr_ver14
#  define bcp_columns           bcp_columns_ver14
#  define bcp_control           bcp_control_ver14
#  define bcp_done              bcp_done_ver14
#  define bcp_exec              bcp_exec_ver14
#  define bcp_getbatchsize      bcp_getbatchsize_ver14
#  define bcp_getl              bcp_getl_ver14
#  define bcp_init              bcp_init_ver14
#  define bcp_options           bcp_options_ver14
#  define bcp_readfmt           bcp_readfmt_ver14
#  define bcp_sendrow           bcp_sendrow_ver14
#  define copy_data_to_host_var copy_data_to_host_var_ver14
#  define dbacolname            dbacolname_ver14
#  define dbadata               dbadata_ver14
#  define dbadlen               dbadlen_ver14
#  define dbaltbind             dbaltbind_ver14
#  define dbaltcolid            dbaltcolid_ver14
#  define dbaltlen              dbaltlen_ver14
#  define dbaltop               dbaltop_ver14
#  define dbalttype             dbalttype_ver14
#  define dbaltutype            dbaltutype_ver14
#  define dbanullbind           dbanullbind_ver14
#  define dbanydatecrack        dbanydatecrack_ver14
#  define dbbind                dbbind_ver14
#  define dbbylist              dbbylist_ver14
#  define dbcancel              dbcancel_ver14
#  define dbcanquery            dbcanquery_ver14
#  define dbchange              dbchange_ver14
#  define dbclose               dbclose_ver14
#  define dbclrbuf              dbclrbuf_ver14
#  define dbclropt              dbclropt_ver14
#  define dbcmd                 dbcmd_ver14
#  define dbcmdrow              dbcmdrow_ver14
#  define dbcolinfo             dbcolinfo_ver14
#  define dbcollen              dbcollen_ver14
#  define dbcolname             dbcolname_ver14
#  define dbcolsource           dbcolsource_ver14
#  define dbcoltype             dbcoltype_ver14
#  define dbcoltypeinfo         dbcoltypeinfo_ver14
#  define dbcolutype            dbcolutype_ver14
#  define dbconvert             dbconvert_ver14
#  define dbconvert_ps          dbconvert_ps_ver14
#  define dbcount               dbcount_ver14
#  define dbcurcmd              dbcurcmd_ver14
#  define dbcurrow              dbcurrow_ver14
#  define dbdata                dbdata_ver14
#  define dbdatecmp             dbdatecmp_ver14
#  define dbdatecrack           dbdatecrack_ver14
#  define dbdatlen              dbdatlen_ver14
#  define dbdead                dbdead_ver14
#  define dberrhandle           dberrhandle_ver14
#  define dbexit                dbexit_ver14
#  define dbfcmd                dbfcmd_ver14
#  define dbfirstrow            dbfirstrow_ver14
#  define dbfreebuf             dbfreebuf_ver14
#  define dbgetchar             dbgetchar_ver14
#  define dbgetmaxprocs         dbgetmaxprocs_ver14
#  define dbgetnull             dbgetnull_ver14
#  define dbgetpacket           dbgetpacket_ver14
#  define dbgetrow              dbgetrow_ver14
#  define dbgettime             dbgettime_ver14
#  define dbgetuserdata         dbgetuserdata_ver14
#  define dbhasretstat          dbhasretstat_ver14
#  define dbinit                dbinit_ver14
#  define dbiordesc             dbiordesc_ver14
#  define dbiowdesc             dbiowdesc_ver14
#  define dbisavail             dbisavail_ver14
#  define dbiscount             dbiscount_ver14
#  define dbisopt               dbisopt_ver14
#  define dblastrow             dblastrow_ver14
#  define dblogin               dblogin_ver14
#  define dbloginfree           dbloginfree_ver14
#  define dbmny4add             dbmny4add_ver14
#  define dbmny4cmp             dbmny4cmp_ver14
#  define dbmny4copy            dbmny4copy_ver14
#  define dbmny4minus           dbmny4minus_ver14
#  define dbmny4sub             dbmny4sub_ver14
#  define dbmny4zero            dbmny4zero_ver14
#  define dbmnycmp              dbmnycmp_ver14
#  define dbmnycopy             dbmnycopy_ver14
#  define dbmnydec              dbmnydec_ver14
#  define dbmnyinc              dbmnyinc_ver14
#  define dbmnymaxneg           dbmnymaxneg_ver14
#  define dbmnymaxpos           dbmnymaxpos_ver14
#  define dbmnyminus            dbmnyminus_ver14
#  define dbmnyzero             dbmnyzero_ver14
#  define dbmonthname           dbmonthname_ver14
#  define dbmorecmds            dbmorecmds_ver14
#  define dbmoretext            dbmoretext_ver14
#  define dbmsghandle           dbmsghandle_ver14
#  define dbname                dbname_ver14
#  define dbnextrow             dbnextrow_ver14
#  define dbnextrow_pivoted     dbnextrow_pivoted_ver14
#  define dbnullbind            dbnullbind_ver14
#  define dbnumalts             dbnumalts_ver14
#  define dbnumcols             dbnumcols_ver14
#  define dbnumcompute          dbnumcompute_ver14
#  define dbnumrets             dbnumrets_ver14
#  if defined(HAVE_DBOPEN)
#    define dbopen              dbopen_ver14
#  endif
#  define dbperror              dbperror_ver14
#  define dbpivot               dbpivot_ver14
#  define dbpivot_count         dbpivot_count_ver14
#  define dbpivot_lookup_name   dbpivot_lookup_name_ver14
#  define dbpivot_max           dbpivot_max_ver14
#  define dbpivot_min           dbpivot_min_ver14
#  define dbpivot_sum           dbpivot_sum_ver14
#  define dbprcollen            dbprcollen_ver14
#  define dbprhead              dbprhead_ver14
#  define dbprrow               dbprrow_ver14
#  define dbprtype              dbprtype_ver14
#  define dbreadtext            dbreadtext_ver14
#  define dbrecftos             dbrecftos_ver14
#  define dbresults             dbresults_ver14
#  define dbretdata             dbretdata_ver14
#  define dbretlen              dbretlen_ver14
#  define dbretname             dbretname_ver14
#  define dbretstatus           dbretstatus_ver14
#  define dbrettype             dbrettype_ver14
#  define dbrows                dbrows_ver14
#  define dbrows_pivoted        dbrows_pivoted_ver14
#  define dbrowtype             dbrowtype_ver14
#  define dbrpcinit             dbrpcinit_ver14
#  define dbrpcparam            dbrpcparam_ver14
#  define dbrpcsend             dbrpcsend_ver14
#  define dbsafestr             dbsafestr_ver14
#  define dbservcharset         dbservcharset_ver14
#  define dbsetavail            dbsetavail_ver14
#  define dbsetifile            dbsetifile_ver14
#  define dbsetinterrupt        dbsetinterrupt_ver14
#  define dbsetlbool            dbsetlbool_ver14
#  define dbsetllong            dbsetllong_ver14
#  define dbsetlname            dbsetlname_ver14
#  define dbsetlogintime        dbsetlogintime_ver14
#  define dbsetlversion         dbsetlversion_ver14
#  define dbsetmaxprocs         dbsetmaxprocs_ver14
#  define dbsetnull             dbsetnull_ver14
#  define dbsetopt              dbsetopt_ver14
#  define dbsetrow              dbsetrow_ver14
#  define dbsettime             dbsettime_ver14
#  define dbsetuserdata         dbsetuserdata_ver14
#  define dbsetversion          dbsetversion_ver14
#  define dbspid                dbspid_ver14
#  define dbspr1row             dbspr1row_ver14
#  define dbspr1rowlen          dbspr1rowlen_ver14
#  define dbsprhead             dbsprhead_ver14
#  define dbsprline             dbsprline_ver14
#  define dbsqlexec             dbsqlexec_ver14
#  define dbsqlok               dbsqlok_ver14
#  define dbsqlsend             dbsqlsend_ver14
#  define dbstrbuild            dbstrbuild_ver14
#  define dbstrcpy              dbstrcpy_ver14
#  define dbstrlen              dbstrlen_ver14
#  define dbtablecolinfo        dbtablecolinfo_ver14
#  define dbtds                 dbtds_ver14
#  define dbtxptr               dbtxptr_ver14
#  define dbtxtimestamp         dbtxtimestamp_ver14
#  define dbuse                 dbuse_ver14
#  define dbvarylen             dbvarylen_ver14
#  define dbversion             dbversion_ver14
#  define dbwillconvert         dbwillconvert_ver14
#  define dbwritetext           dbwritetext_ver14
#  define tdsdbopen             tdsdbopen_ver14
#  define _dblib_check_and_handle_interrupt \
                                     _dblib_check_and_handle_interrupt_ver14
#  define _dblib_convert_err         _dblib_convert_err_ver14
#  define _dblib_err_handler         _dblib_err_handler_ver14
#  define _dblib_handle_err_message  _dblib_handle_err_message_ver14
#  define _dblib_handle_info_message _dblib_handle_info_message_ver14
#  define _dblib_msg_handler         _dblib_msg_handler_ver14
#  define _dblib_setTDS_version      _dblib_setTDS_version_ver14
#endif  /* NCBI_FTDS_RENAME_SYBDB */

#endif  /* DBAPI_DRIVER_FTDS14_IMPL___RENAME_FTDS_DBLIB__H */
