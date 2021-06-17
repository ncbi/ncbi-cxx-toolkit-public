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


#ifndef _APPPERF_HPP_
#define _APPPERF_HPP_

#include <inttypes.h>
#include <unordered_map>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbimtx.hpp>
#include "IdLogUtl.hpp"
#include "AppLog.hpp"

namespace IdLogUtil {

USING_NCBI_SCOPE;

typedef CSpinLock CAppPerfMux;
//typedef CFastMutex CAppPerfMux;


// 5sec average
#define TAVG  5

#define LOWPASS_FILTER_UPD(avg, curr, coff) \
	do {									\
		if (avg == 0)						\
			avg = (curr);					\
		else								\
			avg = (1 - (coff)) * avg + (coff) * (curr);	\
	} while(0)

class CAppOp {
public:
	struct perf_t {
		int64_t m_curpos;
		int64_t m_tottime;
		int64_t m_rowcount;
		int64_t m_bytecount;
		bool Empty() const {
			return (m_rowcount != 0 || m_bytecount != 0);
		}
		perf_t() : 
            m_curpos(0), 
            m_tottime(0), 
            m_rowcount(0), 
            m_bytecount(0) 
        {}
		void Clear() {
			memset(this, 0, sizeof(perf_t));
		}
		void __log() const {
			LOG4(("cp: %" PRId64 ", tm: %" PRId64 ", rc: %" PRId64 ", bc: %" PRId64, m_curpos, m_tottime, m_rowcount, m_bytecount));
		}
	};

	struct stat_t {
		int64_t m_tsnapped;
		perf_t m_rd_snap;
		perf_t m_wr_snap;
		double m_curr_rd_rowsec;
		double m_curr_rd_kbsec;
		double m_curr_wr_rowsec;
		double m_curr_wr_kbsec;
		double m_avg_rd_rowsec;
		double m_avg_rd_kbsec;
		double m_avg_wr_rowsec;
		double m_avg_wr_kbsec;
		stat_t() {
			Clear();
		}		
		void Clear() {
			memset(this, 0, sizeof(stat_t));
		}
		void Calc(int64_t op_interval, int64_t interval, const perf_t& new_rd_data, const perf_t& new_wr_data, int64_t final_op_time) {
			if (final_op_time) {
				m_avg_rd_rowsec = new_rd_data.m_rowcount * 1000000.0 / final_op_time;
				m_avg_rd_kbsec = new_rd_data.m_bytecount * 1000000.0 / final_op_time / 1024.0;
				m_avg_wr_rowsec = new_wr_data.m_rowcount * 1000000.0 / final_op_time;
				m_avg_wr_kbsec = new_wr_data.m_bytecount * 1000000.0 / final_op_time / 1024.0;
			}
			else if (interval > 0) {
				m_curr_rd_rowsec = (new_rd_data.m_rowcount - m_rd_snap.m_rowcount) * 1000000.0 / interval;
				m_curr_rd_kbsec =  (new_rd_data.m_bytecount - m_rd_snap.m_bytecount) * 1000000.0 / interval / 1024.0;
				m_curr_wr_rowsec = (new_wr_data.m_rowcount - m_wr_snap.m_rowcount) * 1000000.0 / interval;
				m_curr_wr_kbsec =  (new_wr_data.m_bytecount - m_wr_snap.m_bytecount) * 1000000.0 / interval / 1024.0;

				double tavg = (op_interval < TAVG * 1000000L) ? op_interval : TAVG * 1000000L;
				double T = interval * 1.0 / (interval + tavg); // 5sec average
				
				LOWPASS_FILTER_UPD(m_avg_rd_rowsec, m_curr_rd_rowsec, T);
				LOWPASS_FILTER_UPD(m_avg_rd_kbsec, m_curr_rd_kbsec, T);
				LOWPASS_FILTER_UPD(m_avg_wr_rowsec, m_curr_wr_rowsec, T);
				LOWPASS_FILTER_UPD(m_avg_wr_kbsec, m_curr_wr_kbsec, T);
			}
		}
	};

	CAppOp(CAppOp* parent = nullptr) : 
        m_log(false), 
        m_produce_output(true),
		m_rowstart(0), 
        m_lastrowupdate(0), 
        m_tstart(0), 
        m_tstop(0),
		m_parent(parent), 
        m_stat(nullptr),
		m_relaxt(0), 
        m_rd_cap("read: "), 
        m_wr_cap("write: ") {}
	CAppOp(const string& rd_cap, const string& wr_cap) : 
        CAppOp(nullptr) 
    {
        SetCaptions(rd_cap, wr_cap);
	}
	CAppOp(bool ProduceOutput, CAppOp* parent = nullptr): CAppOp(parent) {
		m_produce_output = ProduceOutput;
	}
    CAppOp(const CAppOp&) = delete;
    CAppOp& operator=(const CAppOp&) = delete;
	CAppOp(CAppOp&&) = default;
    CAppOp& operator=(CAppOp&&) = default;
	~CAppOp();
    void SetParent(CAppOp* op);
	void Reset();
    void SetCaptions(const string& rd_cap, const string& wr_cap) {
		m_rd_cap = rd_cap;
		m_wr_cap = wr_cap;
    }
	const perf_t GetRdPerf() const;
	const perf_t GetWrPerf() const;
	const stat_t GetPerfStat() const;
	void __log() const {
		LOG4(("%p: rd:", LOG_CPTR(this)));
		m_rd_perf.__log();
		LOG4(("%p: wr:", LOG_CPTR(this)));
		m_wr_perf.__log();
	}
private:	
	friend class CAppPerf;
	bool m_log; // anyting logged
	bool m_produce_output;

	int64_t m_rowstart;
	int64_t m_lastrowupdate;
	int64_t m_tstart;
	int64_t m_tstop;

	CAppOp* m_parent;	// used in multithreading case
	stat_t* m_stat;
	
	perf_t m_rd_perf;
	perf_t m_wr_perf;
	
	int m_relaxt;
	string m_rd_cap;
	string m_wr_cap;

	typedef unordered_map<CAppOp*, CAppOp> opchildren_t;

	int64_t GetTime() const {
		int64_t rv = -1;
		if (m_tstart == 0)
			RAISE_ERROR(eSeqFailed, "operation hasn't been started");
		else if (Stopped())
			rv = m_tstop - m_tstart;
		else
			rv = gettime() - m_tstart;
		return rv;
	}
	void start() {
		if (Started())
			stop();
		m_tstart = gettime();
		m_tstop = 0;
		m_rd_perf.Clear();
		m_wr_perf.Clear();
	}
	int64_t stop() {
		if (Started() && !Stopped()) {
			m_tstop = gettime();
			if (m_log || !Empty())
				LogRowPerf(m_tstop, true, false);
		}
		return GetTime();
	}
	bool Empty() const {
		return m_rd_perf.Empty() && m_wr_perf.Empty();
	}
	bool Started() const {
		return (m_tstart > 0) && (m_tstop == 0);
	}
	bool Stopped() const {
		return m_tstop > 0;
	}
	void BeginRow();
	void EndRow(bool Log, int64_t CurPos, int RowCount, int64_t ByteCount, bool IsRead);
	int64_t RowTime() {
		return m_tstart > 0 ? (gettime() - m_tstart) : 0;
	}
	void LogRowPerf(int64_t t, bool doflush, bool locked);
	void Relax();
	void Unrelax() {
		m_relaxt = 0;
	}
	void ClearPerf() {
		m_rd_perf.Clear();
		m_wr_perf.Clear();
	}
	void UpdatePerformance(int64_t t, CAppOp* from, bool doflush, bool locked);
};

class CAppPerf {
private:
	DISALLOW_COPY_AND_ASSIGN(CAppPerf);
	static bool m_LogPerf;
	static CAppPerfMux m_UpdMux;
public:
	static int64_t GetTime(CAppOp* op) {
		return m_LogPerf ? op->GetTime() : 0;
	}
	static bool Started(CAppOp* op) {
		if (m_LogPerf)
			return op->Started();
		else
			return false;
	}
	static void StartOp(CAppOp* op) {
		if (m_LogPerf)
			op->start();
	}
	static int64_t StopOp(CAppOp* op) {
		return m_LogPerf ? op->stop() : 0;
	}
	static void BeginRow(CAppOp* op) {
		if (m_LogPerf) {
			if (!op->Started())
				StartOp(op);
			op->BeginRow();
		}
	}
	static void EndRow(CAppOp* op, int64_t CurPos, int RowCount, int64_t ByteCount, bool IsRead) {
		if (m_LogPerf)
			op->EndRow(m_LogPerf, CurPos, RowCount, ByteCount, IsRead);
	}
	static int64_t RowTime(CAppOp* op) {
		return m_LogPerf ? op->RowTime() : 0;
	}
	static void Relax(CAppOp* op) {
		op->Relax();
	}
	static void Unrelax(CAppOp* op) {
		op->Unrelax();
	}
	static CAppPerfMux& Mux() {
		return m_UpdMux;
	}
	static void Reset(CAppOp* op) {
		op->Reset();
	}
	static string FormatKbSec(double val);
	static string FormatRwSec(double val);
};

}

#endif
