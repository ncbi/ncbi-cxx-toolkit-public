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
 *  Synchronization classes and utilities
 *
 */

#ifndef _SYNC_OBJ_HPP_
#define _SYNC_OBJ_HPP_

#include <atomic>
#include <thread>

#include <corelib/ncbithr.hpp>

#include "IdLogUtil/IdLogUtl.hpp"
#include "IdLogUtil/AppLog.hpp"
#include "IdCassScope.hpp"


BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

bool WaitCondVar(unsigned int timeoutmks, CFastMutex& Mux, CConditionVariable& Ev, const std::function<bool()>& IsDoneCB, const std::function<void(bool*)>& UpdateRsltCB);


#ifdef __linux__
class CFutex {
private:
	volatile atomic_int m_value;
public:
	typedef enum {
		wrTimeOut,   // value hasn't changed
		wrOk,        // value is changed and we waited
		wrOkFast     // value is changed but we didn't wait
	} WaitResult_t;
	void DoWake(int Waiters = INT_MAX);
public:
	CFutex(int start = 0) : m_value(start) {}
	CFutex& operator=(const CFutex&) = delete;
	CFutex(const CFutex&) = delete;
	bool CompareExchange(int Expected, int ReplaceWith, bool WakeOthers = true) {
		bool rv = m_value.compare_exchange_weak(Expected, ReplaceWith, std::memory_order_relaxed);
		if (rv && WakeOthers)
			DoWake();
		return rv;
	}
	int Value() {
		return m_value;
	}
	WaitResult_t WaitWhile(int Value, int TimeoutMks = -1);
	void Wake() {
		DoWake();
	}
	int Set(int Value, bool WakeOthers = true) {
		int rv = m_value.exchange(Value);
		if ((rv != Value) && WakeOthers)
			DoWake();
		return rv;
	}
	int Inc(bool WakeOthers = true) {
		int rv = atomic_fetch_add(&m_value, 1);
		if (WakeOthers)
			DoWake();
		return rv;
	}
	int Dec(int Value, bool WakeOthers = true) {
		int rv = atomic_fetch_sub(&m_value, 1);
		if (WakeOthers)
			DoWake();
		return rv;
	}
	int SemInc() {
		int rv = atomic_fetch_add(&m_value, 1);
		return rv;
	}
	int SemDec(int value) {
		int rv = atomic_fetch_sub(&m_value, 1);
		if (rv < 0)
			DoWake();
		return rv;
	}	
};
#endif

class SSignalHandler {
private:
	static volatile sig_atomic_t m_CtrlCPressed;
#ifdef __linux__
	static CFutex m_CtrlCPressedEvent;
#endif
	static unique_ptr<thread, function<void(thread*)> > m_WatchThread;
	static std::function<void()> m_OnCtrlCPressed;
	static volatile bool m_Quit;
public:
	static bool CtrlCPressed() {
		return m_CtrlCPressed != 0;
	}
	static void WatchCtrlCPressed(bool Enable, std::function<void()> OnCtrlCPressed = NULL);
};


END_IDBLOB_SCOPE

#endif
