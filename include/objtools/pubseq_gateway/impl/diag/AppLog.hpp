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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 *  App logs classes
 *    LOG LEVELS:
 *     0: TOTAL SILENT (ONLY FATAL ERRORS ARE TO BE SHOWN
 *     1: ALL INTERMEDIATE ERRORS, #0 AND JOB NAME ARE TO BE SHOWN
 *     2: PERFORMANCE + #1
 *     3: VERBOSE
 *
 *    DEFAULT IS #2
 */



#ifndef _APPLOG_HPP_
#define _APPLOG_HPP_

#define __STDC_FORMAT_MACROS
#include <stdint.h>
#include <inttypes.h>
#include <cstdarg>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <unistd.h>
#include <stdio.h>

#include <algorithm>
#include <atomic>


#include <corelib/ncbistre.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbimtx.hpp>

#include "AppLogWrap.h"
#include "IdLogUtl.hpp"
#include "AutoBuf.hpp"

namespace IdLogUtil {
USING_NCBI_SCOPE;

//typedef CFastMutex CAppLogMux;
//typedef CFastMutexGuard CAppLogMuxGuard;
typedef CSpinGuard CAppLogMuxGuard;
typedef CSpinLock CAppLogMux;

#define MAX_LOG_LINE_LEN 4096
#define REQ_LOG_LEVEL 2

#define __VARLOG(N)															\
		do {																\
			va_list args;													\
			va_start(args, format);											\
			char msg[MAX_LOG_LINE_LEN];										\
			int n = vsnprintf(msg, MAX_LOG_LINE_LEN, format, args);			\
			va_end(args);													\
			if (n > 0) {													\
				if (n >= MAX_LOG_LINE_LEN) n = MAX_LOG_LINE_LEN - 1;		\
				msg[n] = 0;													\
				DoLog(N, msg, n);											\
			}																\
		} while(0)

#define __VARLOGERR(N)														\
		do {																\
			va_list args;													\
			va_start(args, format);											\
			char msg[MAX_LOG_LINE_LEN];										\
			int n = vsnprintf(msg, MAX_LOG_LINE_LEN, format, args);			\
			va_end(args);													\
			if (n > 0) {													\
				if (n >= MAX_LOG_LINE_LEN) n = MAX_LOG_LINE_LEN - 1;		\
				msg[n] = 0;													\
				if (N == 0)													\
					Err0Post(msg, n);										\
				else 														\
					Err1Post(msg, n);										\
			}																\
		} while(0)

#define __LOGN(N, a)														\
	do {																	\
		if (IdLogUtil::CAppLog::GetLogLevel() >= N || IdLogUtil::CAppLog::GetLogLevelFile() >= N)	\
			IdLogUtil::CAppLog::VarLog##N a;											\
	} while(0)

#define __LOGERR(N, a)														\
	do {																	\
		if (IdLogUtil::CAppLog::GetLogLevel() >= N || IdLogUtil::CAppLog::GetLogLevelFile() >= N)	\
			IdLogUtil::CAppLog::VarLog##N##Err a;										\
	} while(0)

#define __LOGERRNOFMT(N, a)													\
	do {																	\
		if (IdLogUtil::CAppLog::GetLogLevel() >= N || IdLogUtil::CAppLog::GetLogLevelFile() >= N)	\
			IdLogUtil::CAppLog::Log##N##Err a;											\
	} while(0)


int strdcpy(char* dest, int destlen, const char* src);
static const char* strval(const char* where, const char* what) {
	const char *rv = strstr(where, what);
	if (rv)
		rv += strlen(what);
	return rv;
}

class CAppLog {
private:
	DISALLOW_COPY_AND_ASSIGN(CAppLog);
	class CAppLogFileHandle {
	private:
		DISALLOW_COPY_AND_ASSIGN(CAppLogFileHandle);
		int m_logfile;
	public:
		CAppLogFileHandle() : m_logfile(-1) {}
		~CAppLogFileHandle() {
			Close();
		}
		void Close() {
			if (IsOpen()) {
				if (m_logfile != STDOUT_FILENO && m_logfile != STDERR_FILENO)
					close(m_logfile);
				m_logfile = -1;
			}
		}
		void Flush() {
			if (IsOpen())
				fsync(m_logfile);
		}
		void SetName(const string& filename) {
			Close();
			if (!filename.empty()) {
				if (strcasecmp(filename.c_str(), "<stdout>") == 0)
					m_logfile = STDOUT_FILENO;
				else if (strcasecmp(filename.c_str(), "<stderr>") == 0)
					m_logfile = STDERR_FILENO;
				else
					m_logfile = open(filename.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
			}
		}
		int Write(const void *buf, int len) {
			if (IsOpen() && len > 0) {
				int rv;
				rv = write(m_logfile, buf, len);
				if (rv < 0) {
					rv = errno;
					return 0;
				}
				else
					return rv;
			}
			else
				return 0;
		}
		int Write2(void *buf, int len, void *buf2, int len2) {
			if (len > 0) {
				if (len2 <= 0)
					return Write(buf, len);
				else if (IsOpen()) {
					int rv;
					iovec vec[2];
					vec[0].iov_base = buf;
					vec[0].iov_len = len;
					vec[1].iov_base = buf2;
					vec[1].iov_len = len2;
					rv = writev(m_logfile, vec, 2);
					if (rv < 0){
						rv = errno;
						return 0;
					}
					return rv;
				}
				else
					return 0;
			}
			else {
				return Write(buf2, len2);
			}
		}
		bool IsOpen() {
			return m_logfile != -1;
		}
	};
	
	static CAppLogMux m_LogMux;
	static CAppLogMux m_LogFileMux;
	static CAppLogMux m_LogFileBufMux;
	static CAutoBuf m_logfilebuf1;
	static CAutoBuf m_logfilebuf2;
	static bool m_cur_logbuf_1;
	static unsigned int m_loglevel; // output
	static unsigned int m_loglevelfile; // output
	static atomic_bool m_needflush;
	static bool m_BOL;
	static bool m_ttydetected;
	static bool m_istty;
	static int m_maxlen;
	static string m_logfilename;
	static CAppLogFileHandle m_logf;
	static bool m_IsNcbiAppLog;
	static bool m_FmtDt;
	static bool m_MirrorErr;

	static void DoLogLine(const char* str, int len = -1, bool eol = false, bool stay = false) {
		if (len < 0)
			len = str ? strlen(str) : 0;
//		if (!stay)
//			FlushBOL();
			
		if (str) {
			if (m_BOL || stay) {
				string pad;
				int v = m_BOL ? m_maxlen - len : 0;
				m_maxlen = max(m_maxlen, len);
				if (v > 0)
					pad.append(v, ' ');
				if (stay) {
					NcbiCout << str << pad << '\r' << std::flush;
					m_BOL = true;
				}
				else
					NcbiCout << str << pad;
			}
			else {
				NcbiCout << str;
			}
		}

		if (eol && !stay) {
			NcbiCout << NcbiEndl;
			m_BOL = false;
		}
	}
	static void DoLogLine(const string& str, bool eol = false, bool stay = false) {
		DoLogLine(str.c_str(), str.size(), eol, stay);
	}
	static void LogPost(const char* str, int len = -1) {
		CAppLogMuxGuard guard(m_LogMux);
		FlushBOL();
		DoLogLine(str, len, true);
	}
	static void LogPost(const string& str) {
		LogPost(str.c_str(), str.size());
	}
	static void DoLog(unsigned int level, const char* msg, int len = - 1, bool /*eol*/ = false) {
		if (level <= m_loglevelfile) {
			if (m_IsNcbiAppLog)
				LogPush(lkInfo, msg);
			else
				LogToFile(msg, len);
		}
		if (level <= m_loglevel && !m_IsNcbiAppLog) {
			LogPost(msg, len);
		}
	}
	static void UpdateNcbiLogLevel() {
		if (m_IsNcbiAppLog) {
			switch (m_loglevelfile) {
				case 0:
					LogSetSeverity(eAppLog_Error);
					break;
				case 1:
				case 2:
					LogSetSeverity(eAppLog_Warning);
					break;
                case 3:
                case 4:
					LogSetSeverity(eAppLog_Info);
                    break;
				default:
					LogSetSeverity(eAppLog_Trace);
					break;
			}
		}
	}
	

public:
	static void LogToFile(const char* msg, int len = -1, bool flush = false);
	static void LogToFile(const string& msg, bool flush = false) {
		LogToFile(msg.c_str(), msg.size(), flush);
	}
	static void Flush() {
		if (!m_IsNcbiAppLog)
			LogToFile(NULL, 0, true);
	}
	static void FlushBOL() {
		if (m_BOL) {
			NcbiCout << NcbiEndl;
			m_BOL = false;
			m_maxlen = 0;
		}
	}
	static void FormatTimeStamp(bool FmtDt) {
		m_FmtDt = FmtDt;
	}
	static void UseNcbiAppLog(string AppName) {
		m_logf.Close();
		//GetDiagContext().SetAppName(AppName);
		//CDiagContext::SetOldPostFormat(false);
		//GetDiagContext().PrintStart("Daemon app start");
		LogAppStart(AppName.c_str());
		//NcbiLog_InitMT(AppName);
		//NcbiLog_SetDestination(eNcbiLog_Cwd);
		//NcbiLog_AppStart(argv);
		//NcbiLog_AppRun();
		//NcbiLog_AppSetClient("UNKNOWN");
		//NcbiLog_AppSetSession("UNKNOWN");
		//NcbiLog_SetPostLevel(eNcbiLog_Info);
		m_IsNcbiAppLog = true;
	}
	static void MirrorErrorsToNcbiAppLog(bool mirrorerr) {
		m_MirrorErr = mirrorerr;
	}
	static void SetAppLogDestination(EAppLog_Destination dest) {
		LogSetDestination(dest);
	}
	static void Err0Post(const char* str, int len = -1) {
		if (m_IsNcbiAppLog)
			LogPush(lkError, str);
		else {
			if (m_MirrorErr && LogAppIsStarted()) {
				LogPush(lkError, str);
			}
			LogToFile(str, len, true);
		}
		CAppLogMuxGuard guard(m_LogMux);
		FlushBOL();
		ERR_POST(string(str));		
		Flush();
	}
	static void Err0Post(const string& str) {
		Err0Post(str.c_str(), str.size());
	}
	static void Err1Post(const char* str, int len = -1) {
		if (m_loglevelfile >= 1) {
			if (m_IsNcbiAppLog)
				LogPush(lkWarn, str);
			else {
				if (m_MirrorErr && LogAppIsStarted()) {
					LogPush(lkWarn, str);
				}
				LogToFile(str, len, true);
				Flush();
			}
		}
		if (m_loglevel >= 1) {
			CAppLogMuxGuard guard(m_LogMux);
			FlushBOL();
			ERR_POST(str);
		}		
	}
	static void Err1Post(const string& str) {
		if (m_loglevel >= 1)
			Err1Post(str.c_str(), str.size());
	}
/*
	static void Log1(const string& msg) {
		DoLog(1, msg);
	}
	static void Log2(const string& msg) {
		DoLog(2, msg);
	}
	static void Log3(const string& msg) {
		DoLog(3, msg);
	}
	static void Log4(const string& msg) {
		DoLog(4, msg);
	}
	static void Log5(const string& msg) {
		DoLog(5, msg);
	}
*/
	static void VarLog1(const char* format,...) __attribute__ ((format (printf, 1, 2)))	{
		__VARLOG(1);
	}
	static void VarLog2(const char* format,...) __attribute__ ((format (printf, 1, 2)))	{
		__VARLOG(2);
	}
	static void VarLog3(const char* format,...) __attribute__ ((format (printf, 1, 2)))	{
		__VARLOG(3);
	}
	static void VarLog4(const char* format,...) __attribute__ ((format (printf, 1, 2)))	{
		__VARLOG(4);
	}
	static void VarLog5(const char* format,...) __attribute__ ((format (printf, 1, 2)))	{
		__VARLOG(5);
	}
	static void VarLog0Err(const char* format,...) __attribute__ ((format (printf, 1, 2)))	{
		__VARLOGERR(0);
	}
	static void VarLog1Err(const char* format,...) __attribute__ ((format (printf, 1, 2)))	{
		__VARLOGERR(1);
	}
	static void RequestStart(const char* params) {
		if (m_IsNcbiAppLog) {
			LogPush(lkReqStart, params);
		}
		else {
			char str[MAX_LOG_LINE_LEN];
			str[0] = 0;
			int v = 0;
			v += strdcpy(str, sizeof(str), "request-start ");
			v += strdcpy(&str[v], sizeof(str) - v, params);
			DoLog(REQ_LOG_LEVEL, str, v);
		}
	}
	static void RequestEnd(const char* params) {
		if (m_IsNcbiAppLog) {
			LogPush(lkReqStop, params);
		}
		else {
			int status;
			int64_t bytes_rd;
			int64_t bytes_wr;
			const char* s = strval(params, "status=");
			status = s ? atol(s) : -1;
			s = strval(params, "bytes_read=");
			bytes_rd = s ? atoll(s) : 0;
			s = strval(params, "bytes_wrote=");
			bytes_wr = s ? atoll(s) : 0;
			
			__LOGN(2, ("request-stop, status: %d, Bytes RD: %" PRId64 ", Bytes WR: %" PRId64, status, bytes_rd, bytes_wr));
		}
	}
	static void PerfLogLine(unsigned int level, const string& msg, bool eol, bool stay) {
		if (level <= m_loglevelfile) {
			if (m_IsNcbiAppLog)
				LogPush(lkInfo, msg.c_str());
			else
				LogToFile(msg);
		}
		if (level <= m_loglevel) {
			CAppLogMuxGuard guard(m_LogMux);
			DoLogLine(msg.c_str(), msg.size(), eol, stay);
		}
	}
	static unsigned int GetLogLevel() {
		return m_loglevel;
	}
	static unsigned int GetLogLevelFile() {
		return m_loglevelfile;
	}
	static void SetLogLevel(unsigned int level) {
		m_loglevel = max(min(level, 5U), 0U);
		if (m_IsNcbiAppLog)
			UpdateNcbiLogLevel();
	}
	static void SetLogLevelFile(unsigned int level) {
		m_loglevelfile = max(min(level, 5U), 0U);
		if (m_IsNcbiAppLog)
			UpdateNcbiLogLevel();
	}
	static void SetLogFile(const string& name) {
		if (m_logfilename.compare(name) != 0) {
			if (!m_IsNcbiAppLog) {
				m_logf.SetName(name);
			}
			else {
				if (strcasecmp(name.c_str(), "<local>") == 0) {
					LogSetDestination(eAppLog_Cwd);
				}
				else if (strcasecmp(name.c_str(), "<stdout>") == 0) {
					LogSetDestination(eAppLog_Stdout);
				}
				else if (strcasecmp(name.c_str(), "<stderr>") == 0) {
					LogSetDestination(eAppLog_Stderr);
				}
				else if (strcasecmp(name.c_str(), "<disable>") == 0) {
					LogSetDestination(eAppLog_Disable);
				}
				else if (strcasecmp(name.c_str(), "<default>") == 0) {
					LogSetDestination(eAppLog_Default);
				}
				else {
					fprintf(stderr, "NCBI Log destination can't be set to %s. Set <local>, <stdout> or <stderr>", name.c_str());
					return;
				}
			}
			m_logfilename = name;
		}
	}
	static bool *BOL() {
		return &m_BOL;
	}
	static bool IsTTY();
	static string STime(bool fmt);
	static void STime(string& buf, bool fmt);
	static int STime(char* buf, int bufsz, bool fmt);
};

			

#define LOG5(a) __LOGN(5, a)
#define LOG4(a) __LOGN(4, a)
#define LOG3(a) __LOGN(3, a)
#define LOG2(a) __LOGN(2, a)
#define LOG1(a) __LOGN(1, a)
#define ERRLOG0(a) __LOGERR(0, a)
#define ERRLOG1(a) __LOGERR(1, a)
#define LOG_CPTR(a) (static_cast<const void *>(a))
#define LOG_SPTR(a) (static_cast<const void *>((a).get()))
#define ERRLOG0NOFMT(a) IdLogUtil::CAppLog::Err0Post a
#define ERRLOG1NOFMT(a) IdLogUtil::CAppLog::Err1Post a
#define REQS(a)											\
{														\
	if (IdLogUtil::CAppLog::GetLogLevelFile() >= REQ_LOG_LEVEL) {	\
		IdLogUtil::CAppLog::RequestStart a;						\
	}													\
} while(0)	

#define REQE(a)											\
{														\
	if (IdLogUtil::CAppLog::GetLogLevelFile() >= REQ_LOG_LEVEL) {	\
		IdLogUtil::CAppLog::RequestEnd a;							\
	}													\
} while(0)	

}

#endif
