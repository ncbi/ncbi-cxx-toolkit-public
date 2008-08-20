#ifndef DBAPI_DRIVER_FTDS___NCBI_FTDS_RENAME_SYBDB__H
#define DBAPI_DRIVER_FTDS___NCBI_FTDS_RENAME_SYBDB__H

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
#  define abort_xact NCBI_FTDS_abort_xact
#  define bcp_batch NCBI_FTDS_bcp_batch
#  define bcp_bind NCBI_FTDS_bcp_bind
#  define bcp_colfmt NCBI_FTDS_bcp_colfmt
#  define bcp_colfmt_ps NCBI_FTDS_bcp_colfmt_ps
#  define bcp_collen NCBI_FTDS_bcp_collen
#  define bcp_colptr NCBI_FTDS_bcp_colptr
#  define bcp_columns NCBI_FTDS_bcp_columns
#  define bcp_control NCBI_FTDS_bcp_control
#  define bcp_done NCBI_FTDS_bcp_done
#  define bcp_exec NCBI_FTDS_bcp_exec
#  define bcp_getl NCBI_FTDS_bcp_getl
#  define bcp_init NCBI_FTDS_bcp_init
#  define bcp_moretext NCBI_FTDS_bcp_moretext
#  define bcp_options NCBI_FTDS_bcp_options
#  define bcp_readfmt NCBI_FTDS_bcp_readfmt
#  define bcp_sendrow NCBI_FTDS_bcp_sendrow
#  define bcp_writefmt NCBI_FTDS_bcp_writefmt
#  define build_xact_string NCBI_FTDS_build_xact_string
#  define close_commit NCBI_FTDS_close_commit
#  define commit_xact NCBI_FTDS_commit_xact
#  define dbadata NCBI_FTDS_dbadata
#  define dbadlen NCBI_FTDS_dbadlen
#  define dbaltcolid NCBI_FTDS_dbaltcolid
#  define dbaltlen NCBI_FTDS_dbaltlen
#  define dbaltop NCBI_FTDS_dbaltop
#  define dbalttype NCBI_FTDS_dbalttype
#  define dbaltutype NCBI_FTDS_dbaltutype
#  define dbaltbind NCBI_FTDS_dbaltbind
#  define dbbind NCBI_FTDS_dbbind
#  define dbbylist NCBI_FTDS_dbbylist
#  define dbcancel NCBI_FTDS_dbcancel
#  define dbcanquery NCBI_FTDS_dbcanquery
#  define dbclose NCBI_FTDS_dbclose
#  define dbclrbuf NCBI_FTDS_dbclrbuf
#  define dbclropt NCBI_FTDS_dbclropt
#  define dbcmd NCBI_FTDS_dbcmd
#  define dbcmdrow NCBI_FTDS_dbcmdrow
#  define dbcollen NCBI_FTDS_dbcollen
#  define dbcolname NCBI_FTDS_dbcolname
#  define dbcolsource NCBI_FTDS_dbcolsource
#  define dbcoltype NCBI_FTDS_dbcoltype
#  define dbcoltypeinfo NCBI_FTDS_dbcoltypeinfo
#  define dbcolutype NCBI_FTDS_dbcolutype
#  define dbconvert NCBI_FTDS_dbconvert
#  define dbconvert_ps NCBI_FTDS_dbconvert_ps
#  define dbcount NCBI_FTDS_dbcount
#  define dbcurcmd NCBI_FTDS_dbcurcmd
#  define dbcurrow NCBI_FTDS_dbcurrow
#  define dbdata NCBI_FTDS_dbdata
#  define dbdatecmp NCBI_FTDS_dbdatecmp
#  define dbdatecrack NCBI_FTDS_dbdatecrack
#  define dbdatlen NCBI_FTDS_dbdatlen
#  define dbdead NCBI_FTDS_dbdead
#  define dberrhandle NCBI_FTDS_dberrhandle
#  define dbexit NCBI_FTDS_dbexit
#  define dbfcmd NCBI_FTDS_dbfcmd
#  define dbfirstrow NCBI_FTDS_dbfirstrow
#  define dbfreebuf NCBI_FTDS_dbfreebuf
#  define dbgetchar NCBI_FTDS_dbgetchar
#  define dbgetpacket NCBI_FTDS_dbgetpacket
#  define dbgetrow NCBI_FTDS_dbgetrow
#  define dbgetuserdata NCBI_FTDS_dbgetuserdata
#  define dbhasretstat NCBI_FTDS_dbhasretstat
#  define dbinit NCBI_FTDS_dbinit
#  define dbiordesc NCBI_FTDS_dbiordesc
#  define dbiowdesc NCBI_FTDS_dbiowdesc
#  define dbisavail NCBI_FTDS_dbisavail
#  define dbisopt NCBI_FTDS_dbisopt
#  define dblastrow NCBI_FTDS_dblastrow
#  define dblogin NCBI_FTDS_dblogin
#  define dbloginfree NCBI_FTDS_dbloginfree
#  define dbmny4add NCBI_FTDS_dbmny4add
#  define dbmny4cmp NCBI_FTDS_dbmny4cmp
#  define dbmny4divide NCBI_FTDS_dbmny4divide
#  define dbmny4minus NCBI_FTDS_dbmny4minus
#  define dbmny4mul NCBI_FTDS_dbmny4mul
#  define dbmny4sub NCBI_FTDS_dbmny4sub
#  define dbmny4zero NCBI_FTDS_dbmny4zero
#  define dbmnyadd NCBI_FTDS_dbmnyadd
#  define dbmnycmp NCBI_FTDS_dbmnycmp
#  define dbmnycopy NCBI_FTDS_dbmnycopy
#  define dbmnydec NCBI_FTDS_dbmnydec
#  define dbmnydivide NCBI_FTDS_dbmnydivide
#  define dbmnydown NCBI_FTDS_dbmnydown
#  define dbmnyinc NCBI_FTDS_dbmnyinc
#  define dbmnyinit NCBI_FTDS_dbmnyinit
#  define dbmnymaxneg NCBI_FTDS_dbmnymaxneg
#  define dbmnymaxpos NCBI_FTDS_dbmnymaxpos
#  define dbmnyminus NCBI_FTDS_dbmnyminus
#  define dbmnymul NCBI_FTDS_dbmnymul
#  define dbmnyndigit NCBI_FTDS_dbmnyndigit
#  define dbmnyscale NCBI_FTDS_dbmnyscale
#  define dbmnysub NCBI_FTDS_dbmnysub
#  define dbmnyzero NCBI_FTDS_dbmnyzero
#  define dbmonthname NCBI_FTDS_dbmonthname
#  define dbmorecmds NCBI_FTDS_dbmorecmds
#  define dbmoretext NCBI_FTDS_dbmoretext
#  define dbmsghandle NCBI_FTDS_dbmsghandle
#  define dbname NCBI_FTDS_dbname
#  define dbnextrow NCBI_FTDS_dbnextrow
#  define dbnullbind NCBI_FTDS_dbnullbind
#  define dbnumalts NCBI_FTDS_dbnumalts
#  define dbnumcols NCBI_FTDS_dbnumcols
#  define dbnumcompute NCBI_FTDS_dbnumcompute
#  define dbnumrets NCBI_FTDS_dbnumrets
/* #  define dbopen NCBI_FTDS_dbopen */
#  define dbpoll NCBI_FTDS_dbpoll
#  define dbprhead NCBI_FTDS_dbprhead
#  define dbprrow NCBI_FTDS_dbprrow
#  define dbprtype NCBI_FTDS_dbprtype
#  define dbreadtext NCBI_FTDS_dbreadtext
#  define dbrecftos NCBI_FTDS_dbrecftos
#  define dbregexec NCBI_FTDS_dbregexec
#  define dbreginit NCBI_FTDS_dbreginit
#  define dbreglist NCBI_FTDS_dbreglist
#  define dbregparam NCBI_FTDS_dbregparam
#  define dbresults NCBI_FTDS_dbresults
#  define dbretdata NCBI_FTDS_dbretdata
#  define dbretlen NCBI_FTDS_dbretlen
#  define dbretname NCBI_FTDS_dbretname
#  define dbretstatus NCBI_FTDS_dbretstatus
#  define dbrettype NCBI_FTDS_dbrettype
#  define dbrows NCBI_FTDS_dbrows
#  define dbrowtype NCBI_FTDS_dbrowtype
#  define dbrpcinit NCBI_FTDS_dbrpcinit
#  define dbrpcparam NCBI_FTDS_dbrpcparam
#  define dbrpcsend NCBI_FTDS_dbrpcsend
#  define dbrpwclr NCBI_FTDS_dbrpwclr
#  define dbrpwset NCBI_FTDS_dbrpwset
#  define dbsafestr NCBI_FTDS_dbsafestr
#  define dbsetavail NCBI_FTDS_dbsetavail
#  define dbsetdefcharset NCBI_FTDS_dbsetdefcharset
#  define dbsetdeflang NCBI_FTDS_dbsetdeflang
#  define dbsetifile NCBI_FTDS_dbsetifile
#  define dbsetinterrupt NCBI_FTDS_dbsetinterrupt
#  define dbsetlbool NCBI_FTDS_dbsetlbool
#  define dbsetllong NCBI_FTDS_dbsetllong
#  define dbsetlname NCBI_FTDS_dbsetlname
#  define dbsetlogintime NCBI_FTDS_dbsetlogintime
#  define dbsetlshort NCBI_FTDS_dbsetlshort
#  define dbsetmaxprocs NCBI_FTDS_dbsetmaxprocs
#  define dbsetopt NCBI_FTDS_dbsetopt
#  define dbsettime NCBI_FTDS_dbsettime
#  define dbsetuserdata NCBI_FTDS_dbsetuserdata
#  define dbsetversion NCBI_FTDS_dbsetversion
#  define dbspid NCBI_FTDS_dbspid
#  define dbspr1row NCBI_FTDS_dbspr1row
#  define dbspr1rowlen NCBI_FTDS_dbspr1rowlen
#  define dbsprhead NCBI_FTDS_dbsprhead
#  define dbsprline NCBI_FTDS_dbsprline
#  define dbsqlexec NCBI_FTDS_dbsqlexec
#  define dbsqlok NCBI_FTDS_dbsqlok
#  define dbsqlsend NCBI_FTDS_dbsqlsend
#  define dbstrcpy NCBI_FTDS_dbstrcpy
#  define dbstrlen NCBI_FTDS_dbstrlen
#  define dbtxptr NCBI_FTDS_dbtxptr
#  define dbtxtimestamp NCBI_FTDS_dbtxtimestamp
#  define dbuse NCBI_FTDS_dbuse
#  define dbvarylen NCBI_FTDS_dbvarylen
#  define dbversion NCBI_FTDS_dbversion
#  define dbwillconvert NCBI_FTDS_dbwillconvert
#  define dbwritetext NCBI_FTDS_dbwritetext
#  define open_commit NCBI_FTDS_open_commit
#  define remove_xact NCBI_FTDS_remove_xact
#  define scan_xact NCBI_FTDS_scan_xact
#  define start_xact NCBI_FTDS_start_xact
#  define stat_xact NCBI_FTDS_stat_xact
#  if defined(NCBI_OS_MSWIN)
#    define asprintf NCBI_FTDS_asprintf
#    define vasprintf NCBI_FTDS_vasprintf
#    define strtok_r NCBI_FTDS_strtok_r
#  endif
#endif  /* NCBI_FTDS_RENAME_SYBDB */

#endif  /* DBAPI_DRIVER_FTDS___NCBI_FTDS_RENAME_SYBDB__H */
