/* $Id$
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
 * Author:  Victor Sapojnikov
 *
 * File Description:
 *   Some Sybase dblib constants, types and structure members
 *   translated to Microsoft-compatible ones.
 *
 */

#ifndef DBAPI_DRIVER_MSDBLIB___INTERFACES_P__HPP
#define DBAPI_DRIVER_MSDBLIB___INTERFACES_P__HPP

#define MS_DBLIB_IN_USE 1
#define CDBLibContext CMSDBLibContext
#define CDBL_Connection CMSDBL_Connection 
#define CDBL_LangCmd CMSDBL_LangCmd
#define CDBL_RPCCmd CMSDBL_RPCCmd
#define CDBL_CursorCmd CMSDBL_CursorCmd 
#define CDBL_BCPInCmd CMSDBL_BCPInCmd
#define CDBL_SendDataCmd CMSDBL_SendDataCmd
#define CDBL_RowResult CMSDBL_RowResult
#define CDBL_ParamResult CMSDBL_ParamResult
#define CDBL_ComputeResult CMSDBL_ComputeResult
#define CDBL_StatusResult CMSDBL_StatusResult
#define CDBL_CursorResult CMSDBL_CursorResult
#define CDBL_BlobResult CMSDBL_BlobResult
#define CDBL_ITDescriptor CMSDBL_ITDescriptor

#define SYBINT1 SQLINT1
#define SYBINT2 SQLINT2
#define SYBINT4 SQLINT4
#define SYBIMAGE     SQLIMAGE
#define SYBTEXT      SQLTEXT
#define SYBBINARY    SQLBINARY
#define SYBDATETIME  SQLDATETIME
#define SYBDATETIME4 SQLDATETIM4
#define SYBNUMERIC   SQLNUMERIC
#define SYBCHAR      SQLCHAR
#define SYBBIT       SQLBIT
#define SYBDECIMAL   SQLDECIMAL
#define SYBREAL      SQLFLT4
#define SYBFLT8      SQLFLT8

#define SYBETIME     SQLETIME
#define SYBEFCON     SQLECONNFB
#define SYBECONN     SQLECONN

// DBSETLENCRYPT
#define DBDATETIME4  DBDATETIM4
#define DBTYPEINFO   DBTYPEDEFS

#define CS_SUCCEED 0
#define INT_TIMEOUT 1
#define CS_INT Int4

#define DBDATETIME4_days(x) ((x)->numdays)
#define DBDATETIME4_mins(x) ((x)->nummins)
#define DBNUMERIC_val(x) ((x)->val)
#define DBIORDESC(x) (-1)
#define DBIOWDESC(x) (-1)

#endif  /* DBAPI_DRIVER_DBLIB___INTERFACES_P__HPP */

