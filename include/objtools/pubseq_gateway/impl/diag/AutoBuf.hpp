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
 *  Interface class that implements memory buffer and cares of its deallocation
 *    in case of exceptions
 *
 */

#ifndef _AUTOBUF_H_
#define _AUTOBUF_H_

#include <string>
#include <cstdlib>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbistr.hpp>
#include "IdLogUtl.hpp"

namespace IdLogUtil {

USING_NCBI_SCOPE;


class CAutoBuf {
public:
	explicit CAutoBuf(unsigned int rsrv = 0) :
        m_buf(nullptr),
        m_len(0),
        m_limit(0) 
    {
		if (rsrv > 0)
			Reserve(rsrv);
	}
	CAutoBuf(const CAutoBuf& src) :
        m_buf(nullptr),
        m_len(0),
        m_limit(0) 
    {
		*this = src; // call assignment operator
	}
	CAutoBuf& operator=(const CAutoBuf& src) {
		Clear();
		if (src.m_limit) {
			m_limit = src.m_limit;
			m_buf = (unsigned char*)malloc(m_limit);
			if (!m_buf) {
				string msg = string("failed to allocate buffer (") + NStr::Int8ToString(m_limit) + ")";
				RAISE_ERROR(eMemory, msg);
			}
			memcpy(m_buf, src.m_buf, src.m_len);
			m_len = src.m_len;
		}
		return *this;
	}
	CAutoBuf(CAutoBuf&& src) :
        m_buf(src.m_buf),
        m_len(src.m_len),
        m_limit(src.m_limit) 
    {
		src.Detach();
	}
	~CAutoBuf() {
		Clear();
	}
	void Clear() {
		if (m_buf) {
			m_limit = 0;
			free(m_buf);
			m_buf = NULL;
		}
		m_len = 0;
	}
	unsigned char* Reserve(uint64_t len) {
		if (m_len > 0xffffffffffffffff - len) {
			string msg = string("requested Reserve() is too large (") + NStr::Int8ToString(len) + ")";
	    	RAISE_ERROR(eMemory, msg);
	    }
		if (m_limit - m_len < len) {
			uint64_t newlimit = m_limit;
			if (m_limit < 1024 * 1024)
				newlimit = m_limit * 3 / 2;
			if (newlimit - m_len < len)
				newlimit = (m_len + len);
			if (newlimit > 32 * 1024)
				newlimit = (newlimit + 0x7fff) & ~0x7fff;
			else if (newlimit > 256)
				newlimit = (newlimit + 0x1ff) & ~0x1ff;
			m_buf = (unsigned char*)realloc(m_buf, newlimit);
			if (!m_buf) {
				string msg = string("failed to allocate buffer (") + NStr::Int8ToString(newlimit) + ")";
				RAISE_ERROR(eMemory, msg);
			}
			m_limit = newlimit;
		}
		return &m_buf[m_len];
	}
	void Consume(uint64_t len) {
		if (len > m_limit || m_len > m_limit - len) {
			string msg = string("requested Consume() is too large (") + NStr::Int8ToString(len) + ")";
			RAISE_ERROR(eMemory, msg);
		}
		m_len += len;
	}
	void Unconsume(uint64_t len) {
		if (len > m_len) {
			string msg = string("requested Unconsume() is too large (") + NStr::Int8ToString(len) + ")";
			RAISE_ERROR(eMemory, msg);
		}
		m_len -= len;
	}
	void Reset(uint64_t MaxBlobSize = 1*1024*1024) {
		m_len = 0;
		if (m_limit > MaxBlobSize) {
			Clear();
		}
	}
	unsigned char* Data() const {
		return m_buf;
	}
	uint64_t Size() const {
		return m_len;
	}
	uint64_t Limit() const {
		return m_limit;
	}
	uint64_t Reserved() const {
		return m_limit - m_len;
	}
private:
	unsigned char* m_buf;
	uint64_t m_len;
	uint64_t m_limit;
	void Detach() {
		m_buf = nullptr;
		m_len = m_limit = 0;
	}
};

}

#endif
