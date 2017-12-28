#undef tdsdump_log

#ifdef ENABLE_ODBC_WIDE
static SQLRETURN _SQLGetDiagRec(SQLSMALLINT handleType,
    SQLHANDLE handle,
    SQLSMALLINT numRecord,
    ODBC_CHAR * szSqlStat,
    SQLINTEGER * pfNativeError,
    ODBC_CHAR * szErrorMsg, SQLSMALLINT cbErrorMsgMax, SQLSMALLINT FAR* pcbErrorMsg, int wide);

SQLRETURN ODBC_PUBLIC ODBC_API SQLGetDiagRecW(
    SQLSMALLINT handleType,
    SQLHANDLE handle,
    SQLSMALLINT numRecord,
    SQLWCHAR * szSqlStat,
    SQLINTEGER * pfNativeError,
    SQLWCHAR * szErrorMsg, SQLSMALLINT cbErrorMsgMax, SQLSMALLINT FAR* pcbErrorMsg)
{
	TDSDUMP_LOG_FAST(TDS_DBG_FUNC, "SQLGetDiagRecW(%d, %p, %d, %p, %p, %p, %d, %p)\n",
			(int) handleType,
			handle,
			(int) numRecord,
			szSqlStat,
			pfNativeError,
			szErrorMsg, (int) cbErrorMsgMax, pcbErrorMsg);
	return _SQLGetDiagRec(handleType,
		handle,
		numRecord,
		(ODBC_CHAR*) szSqlStat,
		pfNativeError,
		(ODBC_CHAR*) szErrorMsg, cbErrorMsgMax, pcbErrorMsg, 1);
}
#endif

SQLRETURN ODBC_PUBLIC ODBC_API SQLGetDiagRec(
    SQLSMALLINT handleType,
    SQLHANDLE handle,
    SQLSMALLINT numRecord,
    SQLCHAR * szSqlStat,
    SQLINTEGER * pfNativeError,
    SQLCHAR * szErrorMsg, SQLSMALLINT cbErrorMsgMax, SQLSMALLINT FAR* pcbErrorMsg)
{
	TDSDUMP_LOG_FAST(TDS_DBG_FUNC, "SQLGetDiagRec(%d, %p, %d, %p, %p, %p, %d, %p)\n",
			(int) handleType,
			handle,
			(int) numRecord,
			szSqlStat,
			pfNativeError,
			szErrorMsg, (int) cbErrorMsgMax, pcbErrorMsg);
#ifdef ENABLE_ODBC_WIDE
	return _SQLGetDiagRec(handleType,
		handle,
		numRecord,
		(ODBC_CHAR*) szSqlStat,
		pfNativeError,
		(ODBC_CHAR*) szErrorMsg, cbErrorMsgMax, pcbErrorMsg, 0);
#else
	return _SQLGetDiagRec(handleType,
		handle,
		numRecord,
		szSqlStat,
		pfNativeError,
		szErrorMsg, cbErrorMsgMax, pcbErrorMsg);
#endif
}

#ifdef ENABLE_ODBC_WIDE
static SQLRETURN _SQLError(SQLHENV henv,
    SQLHDBC hdbc,
    SQLHSTMT hstmt,
    ODBC_CHAR * szSqlStat,
    SQLINTEGER * pfNativeError,
    ODBC_CHAR * szErrorMsg, SQLSMALLINT cbErrorMsgMax, SQLSMALLINT FAR* pcbErrorMsg, int wide);

SQLRETURN ODBC_PUBLIC ODBC_API SQLErrorW(
    SQLHENV henv,
    SQLHDBC hdbc,
    SQLHSTMT hstmt,
    SQLWCHAR * szSqlStat,
    SQLINTEGER * pfNativeError,
    SQLWCHAR * szErrorMsg, SQLSMALLINT cbErrorMsgMax, SQLSMALLINT FAR* pcbErrorMsg)
{
	TDSDUMP_LOG_FAST(TDS_DBG_FUNC, "SQLErrorW(%p, %p, %p, %p, %p, %p, %d, %p)\n",
			henv,
			hdbc,
			hstmt,
			szSqlStat,
			pfNativeError,
			szErrorMsg, (int) cbErrorMsgMax, pcbErrorMsg);
	return _SQLError(henv,
		hdbc,
		hstmt,
		(ODBC_CHAR*) szSqlStat,
		pfNativeError,
		(ODBC_CHAR*) szErrorMsg, cbErrorMsgMax, pcbErrorMsg, 1);
}
#endif

SQLRETURN ODBC_PUBLIC ODBC_API SQLError(
    SQLHENV henv,
    SQLHDBC hdbc,
    SQLHSTMT hstmt,
    SQLCHAR * szSqlStat,
    SQLINTEGER * pfNativeError,
    SQLCHAR * szErrorMsg, SQLSMALLINT cbErrorMsgMax, SQLSMALLINT FAR* pcbErrorMsg)
{
	TDSDUMP_LOG_FAST(TDS_DBG_FUNC, "SQLError(%p, %p, %p, %p, %p, %p, %d, %p)\n",
			henv,
			hdbc,
			hstmt,
			szSqlStat,
			pfNativeError,
			szErrorMsg, (int) cbErrorMsgMax, pcbErrorMsg);
#ifdef ENABLE_ODBC_WIDE
	return _SQLError(henv,
		hdbc,
		hstmt,
		(ODBC_CHAR*) szSqlStat,
		pfNativeError,
		(ODBC_CHAR*) szErrorMsg, cbErrorMsgMax, pcbErrorMsg, 0);
#else
	return _SQLError(henv,
		hdbc,
		hstmt,
		szSqlStat,
		pfNativeError,
		szErrorMsg, cbErrorMsgMax, pcbErrorMsg);
#endif
}

#ifdef ENABLE_ODBC_WIDE
static SQLRETURN _SQLGetDiagField(SQLSMALLINT handleType,
    SQLHANDLE handle,
    SQLSMALLINT numRecord,
    SQLSMALLINT diagIdentifier,
    SQLPOINTER buffer,
    SQLSMALLINT cbBuffer,
    SQLSMALLINT * pcbBuffer, int wide);

SQLRETURN ODBC_PUBLIC ODBC_API SQLGetDiagFieldW(
    SQLSMALLINT handleType,
    SQLHANDLE handle,
    SQLSMALLINT numRecord,
    SQLSMALLINT diagIdentifier,
    SQLPOINTER buffer,
    SQLSMALLINT cbBuffer,
    SQLSMALLINT * pcbBuffer)
{
	TDSDUMP_LOG_FAST(TDS_DBG_FUNC, "SQLGetDiagFieldW(%d, %p, %d, %d, %p, %d, %p)\n",
			(int) handleType,
			handle,
			(int) numRecord,
			(int) diagIdentifier,
			buffer,
			(int) cbBuffer,
			pcbBuffer);
	return _SQLGetDiagField(handleType,
		handle,
		numRecord,
		diagIdentifier,
		buffer,
		cbBuffer,
		pcbBuffer, 1);
}
#endif

SQLRETURN ODBC_PUBLIC ODBC_API SQLGetDiagField(
    SQLSMALLINT handleType,
    SQLHANDLE handle,
    SQLSMALLINT numRecord,
    SQLSMALLINT diagIdentifier,
    SQLPOINTER buffer,
    SQLSMALLINT cbBuffer,
    SQLSMALLINT * pcbBuffer)
{
	TDSDUMP_LOG_FAST(TDS_DBG_FUNC, "SQLGetDiagField(%d, %p, %d, %d, %p, %d, %p)\n",
			(int) handleType,
			handle,
			(int) numRecord,
			(int) diagIdentifier,
			buffer,
			(int) cbBuffer,
			pcbBuffer);
#ifdef ENABLE_ODBC_WIDE
	return _SQLGetDiagField(handleType,
		handle,
		numRecord,
		diagIdentifier,
		buffer,
		cbBuffer,
		pcbBuffer, 0);
#else
	return _SQLGetDiagField(handleType,
		handle,
		numRecord,
		diagIdentifier,
		buffer,
		cbBuffer,
		pcbBuffer);
#endif
}

#define tdsdump_log TDSDUMP_LOG_FAST
