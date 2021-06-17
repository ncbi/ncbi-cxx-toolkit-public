/*
*  $Id$
*
* =========================================================================
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
*  Government do not and cannt warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* =========================================================================
*
*  Author: Boris Kiryutin
*
* =========================================================================
*/


#ifndef INTRON_CHAIN_H
#define INTRON_CHAIN_H

#include <corelib/ncbistl.hpp>
#include <stdexcept> // runtime_error

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(prosplign)

// CIgapIntron represents intron or gap at the beg/end
//in intron chain
class CIgapIntron
{
public:
    CIgapIntron() : m_Beg(0), m_Len(0), ref_counter(1) {}

	void Init(int beg, int len)
	{
		m_Beg = beg;
		m_Len = len;
		ref_counter = 1;
	}

    int m_Beg;//coord of the beginning, 0 - based
    int m_Len;
    CIgapIntron* m_Prev;//previous in chain

private:
    int ref_counter;

	friend class CIgapIntronChain;
};

template <class C>
class CObjectPool {
public:
	CObjectPool(size_t chunk_size=10000) : m_ChunkSize(chunk_size), m_Free(NULL)
	{
	}
	~CObjectPool()
	{
#ifdef _DEBUG
		LOG_POST( Info << "pool size = " << m_Chunks.size()*m_ChunkSize );
		size_t free_obj_count = 0;
		while (m_Free) {
			++free_obj_count;
			m_Free = m_Free->m_Prev;
		}
		_ASSERT( free_obj_count == m_Chunks.size()*m_ChunkSize );
#endif
		ITERATE( typename vector<C*>, i, m_Chunks ) {
			delete[] (*i);
		}
	}

	C* GetObject()
	{
		if (m_Free == NULL)
			GetNewChunk();
		
		C* res = m_Free;
		m_Free = res->m_Prev;

		return res;
	}
	void PutObject(C* object)
	{
		object->m_Prev = m_Free;
		m_Free = object;
	}

private:
	void GetNewChunk()
	{
		C* next = new C[m_ChunkSize];
		m_Chunks.push_back(next);
		for (size_t i = 0; i < m_ChunkSize; ++i) {
			next->m_Prev = m_Free;
			m_Free = next;
			++next;
		}
	}

	size_t m_ChunkSize;
	vector<C*> m_Chunks;
	C* m_Free;

};

typedef CObjectPool<CIgapIntron> CIgapIntronPool;

class CIgapIntronChain
{
public:
    inline CIgapIntronChain() : m_Top(NULL), m_Pool(NULL) {}
    ~CIgapIntronChain() { Clear(); }
	void SetPool(CIgapIntronPool& pool) {
		m_Pool = &pool;
	}
    inline void Creat(int beg, int len) 
    {
        m_Top = m_Pool->GetObject();
		m_Top->Init(beg,len);
        m_Top->m_Prev = NULL;
    }
    inline void Expand(CIgapIntronChain& source, int beg, int len)
    {
        Copy(source);
        CIgapIntron* tmp = m_Top;
        m_Top = m_Pool->GetObject();
		m_Top->Init(beg,len);
        m_Top->m_Prev = tmp;
    }
    inline void Copy(CIgapIntronChain& source) {
        if (m_Top != source.m_Top) {
            m_Top = source.m_Top;
            ++m_Top->ref_counter;
        }
    }
    inline void Clear() {
        while (m_Top && --m_Top->ref_counter < 1) {
            CIgapIntron *prev = m_Top->m_Prev;
            m_Pool->PutObject(m_Top);
            m_Top = prev;
        }
        m_Top = NULL;
    }

    CIgapIntron* m_Top;//top of the chain, i.e. last intron

private:
	CIgapIntronPool* m_Pool;

private:
	CIgapIntronChain(const CIgapIntronChain& source);
	CIgapIntronChain& operator=(const CIgapIntronChain& source);
/*
    inline CIgapIntronChain& operator=(const CIgapIntronChain& source) 
    {
        if(source.m_Top) throw runtime_error("Copying of non-empty intron chain");
        Clear();
        return *this;
    }
    inline CIgapIntronChain(const CIgapIntronChain& source)
    {
        if(source.m_Top) throw runtime_error("Copying of non-empty intron chain");
        m_Top = source.m_Top;
    }
*/

};


END_SCOPE(prosplign)
END_NCBI_SCOPE

#endif //INTRON_CHAIN_H
