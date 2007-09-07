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
    CIgapIntron(int beg,int len) : m_Beg(beg), m_Len(len), ref_counter(1) {}
    int m_Beg;//coord of the beginning, 0 - based
    int m_Len;
    CIgapIntron* m_Prev;//previous in chain
    int ref_counter;
};

class CIgapIntronChain
{
public:
    inline CIgapIntronChain() : m_Top(NULL) {}
    ~CIgapIntronChain() { Clear(); }
    inline void Creat(int beg, int len) 
    {
        m_Top = new CIgapIntron(beg,len); //m_IntrHolder.Get();
        m_Top->m_Prev = NULL;
    }
    inline void Expand(CIgapIntronChain& source, int beg, int len)
    {
        Copy(source);
        CIgapIntron* tmp = m_Top;
        m_Top = new CIgapIntron(beg,len);
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
            delete m_Top;
            m_Top = prev;
        }
        m_Top = NULL;
    }

//just to make sure
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

    CIgapIntron* m_Top;//top of the chain, i.e. last intron
};


END_SCOPE(prosplign)
END_NCBI_SCOPE

#endif //INTRON_CHAIN_H
