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
 *  App performance and logs classes
 *
 */

#include <ncbi_pch.hpp>

#include <unistd.h>
#include <algorithm>
#include <sstream>
#include <cassert>

#include <corelib/ncbistre.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbiexpt.hpp>

#include <objtools/pubseq_gateway/impl/diag/AppLog.hpp>
#include <objtools/pubseq_gateway/impl/diag/AppPerf.hpp>

namespace IdLogUtil {

USING_NCBI_SCOPE;

#define PERF_UPD_FREQ 1000000

// 2ms:
#define PERF_RELAX_TIME_MKS 2000
#define PERF_RELAX_MAX_TIME_MKS 50000



bool CAppPerf::m_LogPerf = true;
CAppPerfMux CAppPerf::m_UpdMux;

CAppOp::~CAppOp() {
	try {
		if (Started() && !Stopped()) // due to exception
			CAppPerf::StopOp(this);
		if (m_stat) {
			delete m_stat;
			m_stat = nullptr;
		}
	} catch (const EError&) {
	// ignore b'ze we're in destructor
	}
}

void CAppOp::SetParent(CAppOp* op) {
    if (op != m_parent) {
        if (m_parent && Started() && !Stopped()) {
            CAppPerf::StopOp(this);
        }
        m_parent = op;
    }
}

const CAppOp::perf_t CAppOp::GetRdPerf() const {
	perf_t rv;
	CAppPerf::Mux().Lock();
	rv = m_rd_perf;
	CAppPerf::Mux().Unlock();
	return rv;
}

const CAppOp::perf_t CAppOp::GetWrPerf() const {
	perf_t rv;
	CAppPerf::Mux().Lock();
	rv = m_wr_perf;
	CAppPerf::Mux().Unlock();
	return rv;
}

const CAppOp::stat_t CAppOp::GetPerfStat() const {
	stat_t* rv;
	CAppPerf::Mux().Lock();
	rv = m_stat;
	CAppPerf::Mux().Unlock();
	if (!m_stat) {
		stat_t rv;
		return rv;
	}
	else
		return *rv;
}

void CAppOp::Reset() {
	CAppPerf::Mux().Lock();
	m_log = false;
	m_rowstart = 0;
	m_lastrowupdate = 0;
	m_tstart = 0;
	m_tstop = 0;
	m_parent = nullptr;
	if (m_stat)
		m_stat->Clear();
	m_rd_perf.Clear();
	m_wr_perf.Clear();
	m_relaxt = 0;
	CAppPerf::Mux().Unlock();
}

// in case if DB is heavily loaded, we'd better wait for 0.3sec and re-try again, than do this without a pause
// timeout grows from 7ms up to 0.9s exponentially
void CAppOp::Relax() {
    if (m_relaxt == 0)
        m_relaxt = PERF_RELAX_TIME_MKS;
    else if (m_relaxt < PERF_RELAX_MAX_TIME_MKS)
        m_relaxt = m_relaxt * 2;
    else
        m_relaxt = PERF_RELAX_MAX_TIME_MKS;
    usleep(m_relaxt);
}

void CAppOp::BeginRow() {
	//LOG5(("CAppOp::BeginRow: %p", this));
	m_rowstart = gettime();
}

// locked means we got there being locked by CAppPerf::Mux. 
//   F.e. this may happen in LogRowPerf -> UpdatePerformance -> LogRowPerf -> UpdatePerformance recursion
void CAppOp::UpdatePerformance(int64_t t, CAppOp* from, bool doflush, bool locked) {
	if (t <= 0)
		t = gettime();
	bool b;
	if (locked)
		b = true;
	else if (doflush) {
		CAppPerf::Mux().Lock();
		b = true;
	}
	else
		b = CAppPerf::Mux().TryLock();

	if (b) {
		try {
            if (m_tstart == 0)
                m_tstart = min(t, from->m_tstart); // auto-start

			//from->__log();
			
			if (from->m_rd_perf.m_curpos > 0)
				m_rd_perf.m_curpos = max(m_rd_perf.m_curpos, from->m_rd_perf.m_curpos);

			if (from->m_rd_perf.m_rowcount > 0) {
				m_rd_perf.m_rowcount += from->m_rd_perf.m_rowcount;
				m_rd_perf.m_curpos = max(m_rd_perf.m_curpos, m_rd_perf.m_rowcount);
			}
			
			if (from->m_rd_perf.m_bytecount > 0)
				m_rd_perf.m_bytecount += from->m_rd_perf.m_bytecount;

			if (from->m_rd_perf.m_tottime > 0)
				m_rd_perf.m_tottime += from->m_rd_perf.m_tottime;

			from->m_rd_perf.m_rowcount = 0;
			from->m_rd_perf.m_bytecount = 0;
			from->m_rd_perf.m_tottime = 0;

			if (from->m_wr_perf.m_curpos > 0)
				m_wr_perf.m_curpos = max(m_wr_perf.m_curpos, from->m_wr_perf.m_curpos);

			if (from->m_wr_perf.m_rowcount > 0) {
				m_wr_perf.m_rowcount += from->m_wr_perf.m_rowcount;
				m_wr_perf.m_curpos = max(m_wr_perf.m_curpos, m_wr_perf.m_rowcount);
			}
			
			if (from->m_wr_perf.m_bytecount > 0)
				m_wr_perf.m_bytecount += from->m_wr_perf.m_bytecount;

			if (from->m_wr_perf.m_tottime > 0)
				m_wr_perf.m_tottime += from->m_wr_perf.m_tottime;

			from->m_wr_perf.m_rowcount = 0;
			from->m_wr_perf.m_bytecount = 0;
			from->m_wr_perf.m_tottime = 0;

			m_rd_perf.m_tottime = min(m_rd_perf.m_tottime, t - m_tstart);
			m_wr_perf.m_tottime = min(m_wr_perf.m_tottime, t - m_tstart);


			//LOG5(("CAppOp::UpdatePerformance trd: 0x%lx, rdcnt=%ld, wrcnt=%ld", (long)pthread_self(), m_rd_perf.m_rowcount, m_wr_perf.m_rowcount));
			//__log();
			
			LogRowPerf(t, Stopped() || (from && from->Stopped()), true);
		}
		catch(...) {
			if (!locked)
				CAppPerf::Mux().Unlock();
			throw;
		}

		if (!locked)
			CAppPerf::Mux().Unlock();

	}
	else // force re-call UpdatePerformance with the next call
		from->m_lastrowupdate = 0;
}

void CAppOp::EndRow(bool Log, int64_t CurPos, int RowCount, int64_t ByteCount, bool IsRead) {
	int64_t t = gettime();

	if (m_rowstart == 0)
		RAISE_ERROR(eSeqFailed, "row has already been ended");
	if (IsRead) {
		m_rd_perf.m_curpos = CurPos;
		m_rd_perf.m_tottime += t - m_rowstart;
		if (RowCount >= 0)
			m_rd_perf.m_rowcount += RowCount;
		if (ByteCount >= 0)
			m_rd_perf.m_bytecount += ByteCount;
		//LOG5(("CAppOp::EndRow trd: 0x%lx, par=%p, rc=%d, rdcnt=%ld", (long)pthread_self(), m_parent, RowCount, m_rd_perf.m_rowcount));
	} else {
		m_wr_perf.m_curpos = CurPos;
		m_wr_perf.m_tottime += t - m_rowstart;
		if (RowCount >= 0)
			m_wr_perf.m_rowcount += RowCount;
		if (ByteCount >= 0)
			m_wr_perf.m_bytecount += ByteCount;
		//LOG5(("CAppOp::EndRow trd: 0x%lx, par=%p, rc=%d, wrcnt=%ld", (long)pthread_self(), m_parent, RowCount, m_wr_perf.m_rowcount));
	}
	m_rowstart = 0;
	if (Log)
		LogRowPerf(t, false, false);
}

void CAppOp::LogRowPerf(int64_t t, bool doflush, bool locked) {
	if (t <= 0)
		t = gettime();

	if (doflush || t - m_lastrowupdate > PERF_UPD_FREQ) {
		m_lastrowupdate = t;
		if (m_parent) {
			m_parent->UpdatePerformance(t, this, doflush, locked);
		}
		else {
			bool is_final = doflush && Stopped();

			if (!locked) {
				locked = false;
			}

			int64_t interval = 0;
			if (!m_stat)
				m_stat = new stat_t();
			if (m_stat->m_tsnapped == 0)
				m_stat->m_tsnapped = m_tstart;
			interval = t - m_stat->m_tsnapped;
			if (interval > 0 || is_final)
				m_stat->Calc(t - m_tstart, interval, m_rd_perf, m_wr_perf, is_final ? (m_tstop - m_tstart) : 0);
			m_stat->m_tsnapped = t;
			m_stat->m_rd_snap = m_rd_perf;
			m_stat->m_wr_snap = m_wr_perf;

			if (interval > 0 || is_final) {
				bool has_rd_data, has_wr_data;

				has_rd_data = (m_stat->m_avg_rd_kbsec != 0) || (m_stat->m_avg_rd_rowsec != 0);
				has_wr_data = (m_stat->m_avg_wr_kbsec != 0) || (m_stat->m_avg_wr_rowsec != 0);

				if (m_produce_output && (has_rd_data || has_wr_data || doflush)) {
					string wr_msg, rd_msg;

					if (has_rd_data) {
						if (m_rd_perf.m_curpos > 0)
							rd_msg = NStr::NumericToString(m_rd_perf.m_curpos);
						if (m_rd_perf.m_rowcount > 0) {
							if (!rd_msg.empty())
								rd_msg = rd_msg + ", ";
							if (is_final)
								rd_msg = rd_msg + CAppPerf::FormatRwSec(m_stat->m_avg_rd_rowsec);
							else
								rd_msg = rd_msg + CAppPerf::FormatRwSec(m_stat->m_curr_rd_rowsec) + " (" + CAppPerf::FormatRwSec(m_stat->m_avg_rd_rowsec) + ")";
						}
						if (m_stat->m_curr_rd_kbsec > 0 || m_stat->m_avg_rd_kbsec > 0) {
							if (!rd_msg.empty())
								rd_msg = rd_msg + ", ";
							if (is_final)
								rd_msg = rd_msg + CAppPerf::FormatKbSec(m_stat->m_avg_rd_kbsec);
							else
								rd_msg = rd_msg + CAppPerf::FormatKbSec(m_stat->m_curr_rd_kbsec) + " (" + CAppPerf::FormatKbSec(m_stat->m_avg_rd_kbsec) + ")";
						}
					}

					if (has_wr_data) {
						if (m_wr_perf.m_curpos > 0)
							wr_msg = NStr::NumericToString(m_wr_perf.m_curpos);
						if (m_wr_perf.m_rowcount > 0) {
							if (!wr_msg.empty())
								wr_msg = wr_msg + ", ";
							if (is_final)
								wr_msg = wr_msg + CAppPerf::FormatRwSec(m_stat->m_avg_wr_rowsec);
							else
								wr_msg = wr_msg + CAppPerf::FormatRwSec(m_stat->m_curr_wr_rowsec) + " (" + CAppPerf::FormatRwSec(m_stat->m_avg_wr_rowsec) + ")";
						}
						if (m_stat->m_curr_wr_kbsec > 0 || m_stat->m_avg_wr_kbsec > 0) {
							if (!wr_msg.empty())
								wr_msg = wr_msg + ", ";
							if (is_final)
								wr_msg = wr_msg + CAppPerf::FormatKbSec(m_stat->m_avg_wr_kbsec);
							else
								wr_msg = wr_msg + CAppPerf::FormatKbSec(m_stat->m_curr_wr_kbsec) + " (" + CAppPerf::FormatKbSec(m_stat->m_avg_wr_kbsec) + ")";
						}
					}

                    stringstream msg;
                    if (is_final)
                        msg << "Done: ";
                    else
                        msg << "Performance: ";
					if (!rd_msg.empty())
						msg << m_rd_cap << rd_msg;

					if (!wr_msg.empty()) {
						if (!rd_msg.empty())
							msg << ", ";
						msg << m_wr_cap << wr_msg;
					}

                    m_log = true;

                    if (! (doflush && Stopped()) && CAppLog::IsTTY()) {
                        CAppLog::PerfLogLine(2, msg.str(), false, true);
                    } else {
                        CAppLog::PerfLogLine(2, msg.str(), true, false);
                    }
				}
			}
		}
	}
}


// CAppPerf 

string CAppPerf::FormatKbSec(double val) {
	char buf[128];
	int len;
	if (val >= 10.0*1024*1024)
		len = snprintf(buf, sizeof(buf), "%2.1fGB/sec", val / 1024.0 / 1024.0);
	else if (val >= 1.0*1024*1024)
		len = snprintf(buf, sizeof(buf), "%1.2fGB/sec", val / 1024.0 / 1024.0);
	else if (val >= 100.0*1024)
		len = snprintf(buf, sizeof(buf), "%3.0fMB/sec", val / 1024.0);
	else if (val >= 10.0*1024)
		len = snprintf(buf, sizeof(buf), "%2.1fMB/sec", val / 1024.0);
	else if (val >= 1.0*1024)
		len = snprintf(buf, sizeof(buf), "%1.2fMB/sec", val / 1024.0);
	else if (val >= 100.0)
		len = snprintf(buf, sizeof(buf), "%3.0fKB/sec", val);
	else if (val >= 10.0)
		len = snprintf(buf, sizeof(buf), "%2.1fKB/sec", val);
	else if (val >= 1.0)
		len = snprintf(buf, sizeof(buf), "%1.2fKB/sec", val);
	else if (val >= 100.0/1024)
		len = snprintf(buf, sizeof(buf), "%3.0fB/sec", val * 1024.0);
	else if (val >= 10.0/1024)
		len = snprintf(buf, sizeof(buf), "%2.1fB/sec", val* 1024.0);
	else
		len = snprintf(buf, sizeof(buf), "%1.2fB/sec", val* 1024.0);
	if (len >= (int)sizeof(buf))
		len = sizeof(buf) - 1;
	return string(buf, len);
}

string CAppPerf::FormatRwSec(double val) {
	char buf[128];
	int len;
	if (val > 10.0*1000*1000*1000)
		len = snprintf(buf, sizeof(buf), "%2.1fGrw/sec", val / 1000.0 / 1000.0 / 1000.0);
	else if (val > 1.0*1000*1000*1000)
		len = snprintf(buf, sizeof(buf), "%1.2fGrw/sec", val / 1000.0 / 1000.0 / 1000.0);
	else if (val > 100.0*1000*1000)
		len = snprintf(buf, sizeof(buf), "%3.0fMrw/sec", val / 1000.0 / 1000.0);
	else if (val > 10.0*1000*1000)
		len = snprintf(buf, sizeof(buf), "%2.1fMrw/sec", val / 1000.0 / 1000.0);
	else if (val > 1.0*1000*1000)
		len = snprintf(buf, sizeof(buf), "%1.2fMrw/sec", val / 1000.0 / 1000.0);
	else if (val > 100.0*1000)
		len = snprintf(buf, sizeof(buf), "%3.0fKrw/sec", val / 1000.0);
	else if (val > 10.0*1000)
		len = snprintf(buf, sizeof(buf), "%2.1fKrw/sec", val / 1000.0);
	else if (val > 1.0*1000)
		len = snprintf(buf, sizeof(buf), "%1.2fKrw/sec", val / 1000.0);
	else if (val > 100.0)
		len = snprintf(buf, sizeof(buf), "%3.0frow/sec", val);
	else if (val > 10.0)
		len = snprintf(buf, sizeof(buf), "%2.1frow/sec", val);
	else
		len = snprintf(buf, sizeof(buf), "%1.2frow/sec", val);
	if (len >= (int)sizeof(buf))
		len = sizeof(buf) - 1;
	return string(buf, len);
}


};

