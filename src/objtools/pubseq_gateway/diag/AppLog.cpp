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
 *
 */

#include <ncbi_pch.hpp>

#include <unistd.h>
#include <cassert>
#include <sys/time.h>
#include <stdio.h>

#include <corelib/ncbithr.hpp>
#include <corelib/ncbistr.hpp>

#include <objtools/pubseq_gateway/impl/diag/IdLogUtl.hpp>
#include <objtools/pubseq_gateway/impl/diag/AppLog.hpp>


namespace IdLogUtil {
USING_NCBI_SCOPE;

unsigned int CAppLog::m_loglevel = 2;
unsigned int CAppLog::m_loglevelfile = 3;
bool CAppLog::m_BOL = false;
bool CAppLog::m_ttydetected = false;
bool CAppLog::m_istty = true;
int CAppLog::m_maxlen = 0;
string CAppLog::m_logfilename;
CAppLog::CAppLogFileHandle CAppLog::m_logf;
CAppLogMux CAppLog::m_LogMux;
CAppLogMux CAppLog::m_LogFileMux;
CAppLogMux CAppLog::m_LogFileBufMux;
CAutoBuf CAppLog::m_logfilebuf1(32*1024);
CAutoBuf CAppLog::m_logfilebuf2(32*1024);
bool CAppLog::m_cur_logbuf_1 = true;
atomic_bool CAppLog::m_needflush(false);
bool CAppLog::m_IsNcbiAppLog = false;
bool CAppLog::m_FmtDt = false;
bool CAppLog::m_MirrorErr = false;

bool CAppLog::IsTTY() {
	if (!m_ttydetected) {
		m_istty = isatty(fileno(stdout));
		m_ttydetected = true;
	}
	return m_istty;
}

void CAppLog::LogToFile(const char* msg, int len, bool flush) {
	if (m_logf.IsOpen()) {
		int rlen;
		char buf[MAX_LOG_LINE_LEN];
		if (msg && *msg) {
			char stime[100];
			STime(stime, sizeof(stime), m_FmtDt);
			rlen = snprintf(buf, MAX_LOG_LINE_LEN, "[%s %-3ld] %s", stime, (long)CThread::GetSelf(), msg);
			if (rlen >= MAX_LOG_LINE_LEN - 1)
				rlen = MAX_LOG_LINE_LEN - 2;
			buf[rlen] = '\n';
			rlen++;
			buf[rlen] = 0;
		}
		else
			rlen = 0;


		bool btrue = true;
		bool softflush = m_needflush.compare_exchange_strong(btrue, false);

		if (rlen > 0 || flush || softflush) {
			bool locked;

			if (flush || softflush) {
				m_LogFileMux.Lock();
				locked = true;
			}
			else {
				locked = m_LogFileMux.TryLock();
			}
			if (locked) {
				try {
					CAutoBuf* abuf = NULL;
					{
						CAppLogMuxGuard guard(m_LogFileBufMux);
						abuf = m_cur_logbuf_1 ? &m_logfilebuf1 : &m_logfilebuf2;
						m_cur_logbuf_1 = !m_cur_logbuf_1;
					}
					m_logf.Write2(abuf->Data(), abuf->Size(), buf, rlen);
					abuf->Reset();

					if (flush)
						m_logf.Flush();
				}
				catch(...) {
					m_LogFileMux.Unlock();
					throw;
				}
				m_LogFileMux.Unlock();
			}
			else {
				CAppLogMuxGuard guard(m_LogFileBufMux);
				CAutoBuf* abuf = m_cur_logbuf_1 ?  &m_logfilebuf1 : &m_logfilebuf2;
				unsigned char* dest = abuf->Reserve(rlen);
				memcpy(dest, buf, rlen);
				bool bEnough = abuf->Reserved() >= abuf->Size();
				abuf->Consume(rlen);
				if (bEnough && abuf->Reserved() < abuf->Size()) {
					m_needflush = true;
				}
			}
		}
	}

/*
	if (m_logf.IsOpen()) {
		const int BUF_RSRV=1000;
		int len;
		bool softflush = false;
		if (!msg.empty()) {
			char stime[100];
			int slen;
			bool bEnough;
			slen = STime(stime, sizeof(stime));

			CAppLogMuxGuard guard(m_LogFileBufMux);
			CAutoBuf* abuf = m_cur_logbuf_1 ?  &m_logfilebuf1 : &m_logfilebuf2;
			char* dest = reinterpret_cast<char*>(abuf->Reserve(BUF_RSRV));
			len = snprintf(dest, BUF_RSRV, "[%s %ld] %s\n", stime, (long)CThread::GetSelf(), msg.c_str());
			bEnough = (abuf->Reserved() >= abuf->Size());
			abuf->Consume(len);
//			if (bEnough && abuf->Reserved() < abuf->Size()) // filled by more than half -> better flush
//				softflush = true;
		}
		else
			len = 0;

		if (len > 0 || flush) {
			bool locked;

			if (flush || softflush) {
				m_LogFileMux.Lock();
				locked = true;
			}
			else {
				locked = m_LogFileMux.TryLock();
			}
			if (locked) {
				try {
					CAutoBuf* abuf = NULL;

					{
						CAppLogMuxGuard guard(m_LogFileBufMux);
						abuf = m_cur_logbuf_1 ? &m_logfilebuf1 : &m_logfilebuf2;
						m_cur_logbuf_1 = !m_cur_logbuf_1;
					}
					m_logf.Write(abuf->Data(), abuf->Size());
					abuf->Reset();

					if (flush)
						m_logf.Flush();
				}
				catch(...) {
					m_LogFileMux.Unlock();
					throw;
				}
				m_LogFileMux.Unlock();
			}
		}
	}
*/
/*
	m_LogFileMux.Lock();
	try {
		if (m_logstrm.is_open()) {
			m_logstrm << buf << msg << NcbiEndl;
			m_logstrm.flush();
		}
	}
	catch(...) {
		m_LogFileMux.Unlock();
		throw;
	}
	m_LogFileMux.Unlock();
*/
}

int CAppLog::STime(char* buf, int bufsz, bool fmt) {
	int rv;
	int64_t t = gettime();
	time_t tv_sec = t / 1000000L;
	unsigned int tv_usec = t % 1000000L;
	if (fmt) {
		char buff[100];
		struct tm tm;
		localtime_r(&tv_sec, &tm);
		strftime (buff, sizeof(buff), "%Y-%m-%d %H:%M:%S", &tm);
		rv = snprintf (buf, bufsz, "%s.%06u", buff, tv_usec);
	}
	else {
		rv = snprintf (buf, bufsz, "%06u.%06u", (unsigned int)tv_sec, tv_usec);
	}
	if (rv >= bufsz)
		rv = bufsz - 1;
	buf[rv] = 0;
	return rv;
}

void CAppLog::STime(string& buf, bool fmt) {
	char buff[100];
	STime(buff, sizeof(buff), fmt);
	buf.assign(buff);
}

string CAppLog::STime(bool fmt) {
	string rv;
	STime(rv, fmt);
    return rv;
}

int strdcpy(char* dest, int destlen, const char* src) {
	int rv = destlen;
	while (destlen > 0) {
		if ((*dest++ = *src++) == 0) break;
		destlen--;
	}
	if ((rv -= destlen) <= 0)
		rv = 0;
	else
		rv--; // term zero
	return rv;
}


};

