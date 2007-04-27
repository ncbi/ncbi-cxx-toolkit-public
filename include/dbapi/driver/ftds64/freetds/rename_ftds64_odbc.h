#ifndef DBAPI_DRIVER_FTDS64_FREETDS___RENAME_FTDS64_ODBC__H
#define DBAPI_DRIVER_FTDS64_FREETDS___RENAME_FTDS64_ODBC__H


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
 * Author:  Sergey Sikorskiy
 *
 * File Description:
 *
 */


#define CloseODBCPerfData       CloseODBCPerfData_ver64
#define CollectODBCPerfData     CollectODBCPerfData_ver64
#define CursorLibLockDbc        CursorLibLockDbc_ver64
#define CursorLibLockDesc       CursorLibLockDesc_ver64
#define CursorLibLockStmt       CursorLibLockStmt_ver64
#define CursorLibTransact       CursorLibTransact_ver64
#define DllBidEntryPoint        DllBidEntryPoint_ver64
#define GetODBCSharedData       GetODBCSharedData_ver64
#define LockHandle              LockHandle_ver64
#define MpHeapAlloc             MpHeapAlloc_ver64
#define MpHeapCompact           MpHeapCompact_ver64
#define MpHeapCreate            MpHeapCreate_ver64
#define MpHeapDestroy           MpHeapDestroy_ver64
#define MpHeapFree              MpHeapFree_ver64
#define MpHeapReAlloc           MpHeapReAlloc_ver64
#define MpHeapSize              MpHeapSize_ver64
#define MpHeapValidate          MpHeapValidate_ver64
#define ODBCGetTryWaitValue     ODBCGetTryWaitValue_ver64
#define ODBCInternalConnectW    ODBCInternalConnectW_ver64
#define ODBCQualifyFileDSNW     ODBCQualifyFileDSNW_ver64
#define ODBCSetTryWaitValue     ODBCSetTryWaitValue_ver64
#define OpenODBCPerfData        OpenODBCPerfData_ver64
#define PostComponentError      PostComponentError_ver64
#define PostODBCComponentError  PostODBCComponentError_ver64
#define PostODBCError           PostODBCError_ver64
#define SQLAllocConnect         SQLAllocConnect_ver64
#define SQLAllocEnv             SQLAllocEnv_ver64
#define SQLAllocHandle          SQLAllocHandle_ver64
#define SQLAllocHandleStd       SQLAllocHandleStd_ver64
#define SQLAllocStmt            SQLAllocStmt_ver64
#define SQLBindCol              SQLBindCol_ver64
#define SQLBindParam            SQLBindParam_ver64
#define SQLBindParameter        SQLBindParameter_ver64
#define SQLBrowseConnect        SQLBrowseConnect_ver64
#define SQLBrowseConnectA       SQLBrowseConnectA_ver64
#define SQLBrowseConnectW       SQLBrowseConnectW_ver64
#define SQLBulkOperations       SQLBulkOperations_ver64
#define SQLCancel               SQLCancel_ver64
#define SQLCloseCursor          SQLCloseCursor_ver64
#define SQLColAttribute         SQLColAttribute_ver64
#define SQLColAttributeA        SQLColAttributeA_ver64
#define SQLColAttributeW        SQLColAttributeW_ver64
#define SQLColAttributes        SQLColAttributes_ver64
#define SQLColAttributesA       SQLColAttributesA_ver64
#define SQLColAttributesW       SQLColAttributesW_ver64
#define SQLColumnPrivileges     SQLColumnPrivileges_ver64
#define SQLColumnPrivilegesA    SQLColumnPrivilegesA_ver64
#define SQLColumnPrivilegesW    SQLColumnPrivilegesW_ver64
#define SQLColumns              SQLColumns_ver64
#define SQLColumnsA             SQLColumnsA_ver64
#define SQLColumnsW             SQLColumnsW_ver64
#define SQLConnect              SQLConnect_ver64
#define SQLConnectA             SQLConnectA_ver64
#define SQLConnectW             SQLConnectW_ver64
#define SQLCopyDesc             SQLCopyDesc_ver64
#define SQLDataSources          SQLDataSources_ver64
#define SQLDataSourcesA         SQLDataSourcesA_ver64
#define SQLDataSourcesW         SQLDataSourcesW_ver64
#define SQLDescribeCol          SQLDescribeCol_ver64
#define SQLDescribeColA         SQLDescribeColA_ver64
#define SQLDescribeColW         SQLDescribeColW_ver64
#define SQLDescribeParam        SQLDescribeParam_ver64
#define SQLDisconnect           SQLDisconnect_ver64
#define SQLDriverConnect        SQLDriverConnect_ver64
#define SQLDriverConnectA       SQLDriverConnectA_ver64
#define SQLDriverConnectW       SQLDriverConnectW_ver64
#define SQLDrivers              SQLDrivers_ver64
#define SQLDriversA             SQLDriversA_ver64
#define SQLDriversW             SQLDriversW_ver64
#define SQLEndTran              SQLEndTran_ver64
#define SQLError                SQLError_ver64
#define SQLErrorA               SQLErrorA_ver64
#define SQLErrorW               SQLErrorW_ver64
#define SQLExecDirect           SQLExecDirect_ver64
#define SQLExecDirectA          SQLExecDirectA_ver64
#define SQLExecDirectW          SQLExecDirectW_ver64
#define SQLExecute              SQLExecute_ver64
#define SQLExtendedFetch        SQLExtendedFetch_ver64
#define SQLFetch                SQLFetch_ver64
#define SQLFetchScroll          SQLFetchScroll_ver64
#define SQLForeignKeys          SQLForeignKeys_ver64
#define SQLForeignKeysA         SQLForeignKeysA_ver64
#define SQLForeignKeysW         SQLForeignKeysW_ver64
#define SQLFreeConnect          SQLFreeConnect_ver64
#define SQLFreeEnv              SQLFreeEnv_ver64
#define SQLFreeHandle           SQLFreeHandle_ver64
#define SQLFreeStmt             SQLFreeStmt_ver64
#define SQLGetConnectAttr       SQLGetConnectAttr_ver64
#define SQLGetConnectAttrA      SQLGetConnectAttrA_ver64
#define SQLGetConnectAttrW      SQLGetConnectAttrW_ver64
#define SQLGetConnectOption     SQLGetConnectOption_ver64
#define SQLGetConnectOptionA    SQLGetConnectOptionA_ver64
#define SQLGetConnectOptionW    SQLGetConnectOptionW_ver64
#define SQLGetCursorName        SQLGetCursorName_ver64
#define SQLGetCursorNameA       SQLGetCursorNameA_ver64
#define SQLGetCursorNameW       SQLGetCursorNameW_ver64
#define SQLGetData              SQLGetData_ver64
#define SQLGetDescField         SQLGetDescField_ver64
#define SQLGetDescFieldA        SQLGetDescFieldA_ver64
#define SQLGetDescFieldW        SQLGetDescFieldW_ver64
#define SQLGetDescRec           SQLGetDescRec_ver64
#define SQLGetDescRecA          SQLGetDescRecA_ver64
#define SQLGetDescRecW          SQLGetDescRecW_ver64
#define SQLGetDiagField         SQLGetDiagField_ver64
#define SQLGetDiagFieldA        SQLGetDiagFieldA_ver64
#define SQLGetDiagFieldW        SQLGetDiagFieldW_ver64
#define SQLGetDiagRec           SQLGetDiagRec_ver64
#define SQLGetDiagRecA          SQLGetDiagRecA_ver64
#define SQLGetDiagRecW          SQLGetDiagRecW_ver64
#define SQLGetEnvAttr           SQLGetEnvAttr_ver64
#define SQLGetFunctions         SQLGetFunctions_ver64
#define SQLGetInfo              SQLGetInfo_ver64
#define SQLGetInfoA             SQLGetInfoA_ver64
#define SQLGetInfoW             SQLGetInfoW_ver64
#define SQLGetStmtAttr          SQLGetStmtAttr_ver64
#define SQLGetStmtAttrA         SQLGetStmtAttrA_ver64
#define SQLGetStmtAttrW         SQLGetStmtAttrW_ver64
#define SQLGetStmtOption        SQLGetStmtOption_ver64
#define SQLGetTypeInfo          SQLGetTypeInfo_ver64
#define SQLGetTypeInfoA         SQLGetTypeInfoA_ver64
#define SQLGetTypeInfoW         SQLGetTypeInfoW_ver64
#define SQLMoreResults          SQLMoreResults_ver64
#define SQLNativeSql            SQLNativeSql_ver64
#define SQLNativeSqlA           SQLNativeSqlA_ver64
#define SQLNativeSqlW           SQLNativeSqlW_ver64
#define SQLNumParams            SQLNumParams_ver64
#define SQLNumResultCols        SQLNumResultCols_ver64
#define SQLParamData            SQLParamData_ver64
#define SQLParamOptions         SQLParamOptions_ver64
#define SQLPrepare              SQLPrepare_ver64
#define SQLPrepareA             SQLPrepareA_ver64
#define SQLPrepareW             SQLPrepareW_ver64
#define SQLPrimaryKeys          SQLPrimaryKeys_ver64
#define SQLPrimaryKeysA         SQLPrimaryKeysA_ver64
#define SQLPrimaryKeysW         SQLPrimaryKeysW_ver64
#define SQLProcedureColumns     SQLProcedureColumns_ver64
#define SQLProcedureColumnsA    SQLProcedureColumnsA_ver64
#define SQLProcedureColumnsW    SQLProcedureColumnsW_ver64
#define SQLProcedures           SQLProcedures_ver64
#define SQLProceduresA          SQLProceduresA_ver64
#define SQLProceduresW          SQLProceduresW_ver64
#define SQLPutData              SQLPutData_ver64
#define SQLRowCount             SQLRowCount_ver64
#define SQLSetConnectAttr       SQLSetConnectAttr_ver64
#define SQLSetConnectAttrA      SQLSetConnectAttrA_ver64
#define SQLSetConnectAttrW      SQLSetConnectAttrW_ver64
#define SQLSetConnectOption     SQLSetConnectOption_ver64
#define SQLSetConnectOptionA    SQLSetConnectOptionA_ver64
#define SQLSetConnectOptionW    SQLSetConnectOptionW_ver64
#define SQLSetCursorName        SQLSetCursorName_ver64
#define SQLSetCursorNameA       SQLSetCursorNameA_ver64
#define SQLSetCursorNameW       SQLSetCursorNameW_ver64
#define SQLSetDescField         SQLSetDescField_ver64
#define SQLSetDescFieldA        SQLSetDescFieldA_ver64
#define SQLSetDescFieldW        SQLSetDescFieldW_ver64
#define SQLSetDescRec           SQLSetDescRec_ver64
#define SQLSetEnvAttr           SQLSetEnvAttr_ver64
#define SQLSetParam             SQLSetParam_ver64
#define SQLSetPos               SQLSetPos_ver64
#define SQLSetScrollOptions     SQLSetScrollOptions_ver64
#define SQLSetStmtAttr          SQLSetStmtAttr_ver64
#define SQLSetStmtAttrA         SQLSetStmtAttrA_ver64
#define SQLSetStmtAttrW         SQLSetStmtAttrW_ver64
#define SQLSetStmtOption        SQLSetStmtOption_ver64
#define SQLSpecialColumns       SQLSpecialColumns_ver64
#define SQLSpecialColumnsA      SQLSpecialColumnsA_ver64
#define SQLSpecialColumnsW      SQLSpecialColumnsW_ver64
#define SQLStatistics           SQLStatistics_ver64
#define SQLStatisticsA          SQLStatisticsA_ver64
#define SQLStatisticsW          SQLStatisticsW_ver64
#define SQLTablePrivileges      SQLTablePrivileges_ver64
#define SQLTablePrivilegesA     SQLTablePrivilegesA_ver64
#define SQLTablePrivilegesW     SQLTablePrivilegesW_ver64
#define SQLTables               SQLTables_ver64
#define SQLTablesA              SQLTablesA_ver64
#define SQLTablesW              SQLTablesW_ver64
#define SQLTransact             SQLTransact_ver64
#define SearchStatusCode        SearchStatusCode_ver64
#define VFreeErrors             VFreeErrors_ver64
#define VRetrieveDriverErrorsRowCol VRetrieveDriverErrorsRowCol_ver64
#define ValidateErrorQueue      ValidateErrorQueue_ver64


#endif  /* DBAPI_DRIVER_FTDS64_FREETDS___RENAME_FTDS64_ODBC__H */
