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
 *  defines MPMC (multiple producers multiple consumers) queue class
 *  inspired by another no-lock queue implementation 
 *  published at http://www.1024cores.net
 *
 */

#ifndef _MPMC_W_HPP_
#define _MPMC_W_HPP_

#include <atomic>
#include <cstddef>
#include <objtools/pubseq_gateway/impl/cassandra/SyncObj.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_exception.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

template<typename T, size_t SZ>
class mpmc_bounded_queue_w {
public:
    static constexpr const size_t kCpuCacheLineSz = 64;
    static constexpr const size_t kPadSz = (-sizeof(std::atomic<size_t>) % kCpuCacheLineSz);
	using cacheline_pad_t = char[kPadSz];
    static constexpr const size_t kPadDataSz = ((-sizeof(T) - sizeof(std::atomic<size_t>)) % kCpuCacheLineSz);
    using cacheline_pad_data_t = char[kPadDataSz];

	struct cell_t {
		std::atomic<size_t> m_sequence;
		cacheline_pad_data_t m_pad;
		T m_data;
	};

	cacheline_pad_t         m_pad0;
	cell_t                  m_buffer[SZ];
	cacheline_pad_t         m_pad1;
	std::atomic<size_t>     m_push_pos;
	cacheline_pad_t         m_pad2;
	std::atomic<size_t>     m_pop_pos;
	cacheline_pad_t         m_pad3;

	CFutex m_readypush_ev;
	CFutex m_readypop_ev;
    atomic<size_t>          m_size;

public:
	mpmc_bounded_queue_w() {
		static_assert ((SZ & (SZ - 1)) == 0, "SZ template parameter value must be power of two");
		static_assert (sizeof(intptr_t) == sizeof(size_t), "All of sudden size_t is of different size than intptr_t, you've to update sources");
		clear();
	}
	mpmc_bounded_queue_w(const mpmc_bounded_queue_w&) = delete;
	mpmc_bounded_queue_w(mpmc_bounded_queue_w&&) = delete;
	mpmc_bounded_queue_w& operator=(const mpmc_bounded_queue_w&) = delete;
	mpmc_bounded_queue_w& operator=(mpmc_bounded_queue_w&&) = delete;

	void clear() {
		for (size_t i = 0; i < SZ; ++i)
			m_buffer[i].m_sequence.store(i, std::memory_order_relaxed);
		m_push_pos.store(0, std::memory_order_relaxed);
		m_pop_pos.store(0, std::memory_order_relaxed);
	}

    // advisory only (e.g. for monitoring)
    size_t size() const {
        return m_size.load();
    }

    template<class TT>
	bool push(TT&& data) {
        static_assert(!is_const<typename remove_reference<TT>::type>::value, "argument for this method can not be const");
        static_assert(is_same<typename remove_reference<TT>::type, typename remove_reference<T>::type>::value, "type of the argument must match the template class type");
		cell_t* cell;
		size_t pos = m_push_pos.load(std::memory_order_relaxed);
		for (;;) {
			cell = &m_buffer[pos & (SZ - 1)];
			size_t seq = cell->m_sequence.load(std::memory_order_acquire);
			intptr_t diff = (intptr_t)seq - (intptr_t)pos;
			if (diff == 0) {
				if (m_push_pos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
					break;
			}
			else if (diff < 0)
				return false;
			else
				pos = m_push_pos.load(std::memory_order_relaxed);
		}
		cell->m_data = std::move(data); // there is a "move", not "forward" and it is on purpose of grabbing original data for non-RValue references too
		cell->m_sequence.store(pos + 1, std::memory_order_release);
        ++m_size;
		return true;
	}

    template<class TT>
	bool push_wait(TT&& data, int64_t timeoutmks) {
		bool rv = false;
		int prev_val = m_readypush_ev.Value();
		rv = push(forward<TT>(data));
		if (rv)
			m_readypop_ev.Inc();
		else if (timeoutmks > 0) {
			CFutex::EWaitResult wr = m_readypush_ev.WaitWhile(prev_val, timeoutmks);
			if (wr == CFutex::eWaitResultOk || wr == CFutex::eWaitResultOkFast)
				rv = push(data);
		}
		return rv;
	}

    template<class TT>
	void push_wait(TT&& data) {
		bool rslt = false;
        while (!rslt) {
            int prev_val = m_readypush_ev.Value();
            rslt = push(forward<TT>(data));
            if (rslt) {
                m_readypop_ev.Inc();
                break;
            }
            CFutex::EWaitResult wr = m_readypush_ev.WaitWhile(prev_val);
            if (wr == CFutex::eWaitResultOk || wr == CFutex::eWaitResultOkFast)
                rslt = push(forward<TT>(data));
        };
	}

	bool pop(T* data) {
		cell_t* cell;
		size_t pos = m_pop_pos.load(std::memory_order_relaxed);
		for (;;) {
			cell = &m_buffer[pos & (SZ - 1)];
			size_t seq = cell->m_sequence.load(std::memory_order_acquire);
			intptr_t diff = (intptr_t)seq - (intptr_t)(pos + 1);
			if (diff == 0) {
				if (m_pop_pos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
					break;
			}
			else if (diff < 0)
				return false;
			else
				pos = m_pop_pos.load(std::memory_order_relaxed);
		}
		*data = std::move(cell->m_data);
        --m_size;
		cell->m_sequence.store(pos + SZ, std::memory_order_release);
		return true;
	}

	bool pop_wait(T* data, int64_t timeoutmks) {
		bool rv = false;
		int prev_val = m_readypop_ev.Value();
		rv = pop(data);
		if (rv)
			m_readypush_ev.Inc();
		else if (timeoutmks > 0) {
			CFutex::EWaitResult wr = m_readypop_ev.WaitWhile(prev_val, timeoutmks);
			if (wr == CFutex::eWaitResultOk || wr == CFutex::eWaitResultOkFast)
				rv = pop(data);
		}
		return rv;
	}

	void pop_wait(T* data) {
		bool rslt = false;
        while (!rslt) {
            int prev_val = m_readypop_ev.Value();
            rslt = pop(data);
            if (rslt) {
                m_readypush_ev.Inc();
                break;
            }
            CFutex::EWaitResult wr = m_readypop_ev.WaitWhile(prev_val);
            if (wr == CFutex::eWaitResultOk || wr == CFutex::eWaitResultOkFast)
                rslt = pop(data);
        }
	}
}; 

END_IDBLOB_SCOPE

#endif
