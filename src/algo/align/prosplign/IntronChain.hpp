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

#include "ObjHolder.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(prosplign)

// CIgapIntron represents intron or gap at the beg/end
//in intron chain
template<class Owner>
class CIgapIntron
{
public:
    int m_Beg;//coord of the beginning, 0 - based
    int m_Len;
    CIgapIntron *m_Prev;//previous in chain
    Owner *m_Owner;//for memory managing. NULL means returned to 'object heap'
};


template<class Owner>
class CIgapIntronChain
{
public:
    inline CIgapIntronChain(Owner *owner) : m_Owner(owner) { m_Top = NULL; }
    ~CIgapIntronChain() { Clear(); }
    inline void Creat
(int beg, int len) 
    {
        m_Top = m_IntrHolder.Get();
        m_Top->m_Beg = beg;
        m_Top->m_Len = len;
        m_Top->m_Owner = m_Owner;
        m_Top->m_Prev = NULL;
    }
//retake ownership 
    inline void Expand(CIgapIntronChain<Owner>& source, int beg, int len)
    {
        Copy(source);
        CIgapIntron<Owner> *tmp = m_Top;
        Creat(beg, len);
        m_Top->m_Prev = tmp;
    }    
    inline void Copy(CIgapIntronChain<Owner>& source) {
        m_Top = source.m_Top;
        CIgapIntron<Owner> *cur = m_Top;
        while(cur && cur->m_Owner != m_Owner) {
            cur->m_Owner = m_Owner;
            cur = cur->m_Prev;
        }
    }
//do not retake ownership
    inline void SoftCopy(CIgapIntronChain<Owner>& source) {
        m_Top = source.m_Top;
    }
    inline void SoftExpand(CIgapIntronChain<Owner>& source, int beg, int len)
    {
        Creat(beg, len);
        m_Top->m_Prev = source.m_Top;
    }    

    inline void RegainOwner(void) {
        CIgapIntron<Owner> *cur = m_Top;
        while(cur) {
            cur->m_Owner = m_Owner;
            cur = cur->m_Prev;
        }
    }

    inline void Clear() {
        CIgapIntron<Owner> *cur = m_Top;
        while(cur && cur->m_Owner == m_Owner) {
            cur->m_Owner = NULL;
            m_IntrHolder.Add(cur);
            cur = cur->m_Prev;
        }
        m_Top = NULL;
    }

//just to make sure
    inline CIgapIntronChain& operator=(const CIgapIntronChain& source) 
    {
        if(source.m_Top) throw runtime_error("Copying of non-empty intron chain");
        m_Owner = source.m_Owner;
        m_Top = source.m_Top;
        return *this;
    }
    inline CIgapIntronChain(const CIgapIntronChain& source)
    {
        if(source.m_Top) throw runtime_error("Copiing of non-empty intron chain");
        m_Owner = source.m_Owner;
        m_Top = source.m_Top;
    }
public:
    CIgapIntron<Owner> *m_Top;//top of the chain, i.e. last intron
private:
//    inline CIgapIntronChain& operator=(CIgapIntronChain& source) { Copy(source); return *this;}
    Owner *m_Owner;

    static CObjHolder<CIgapIntron<Owner> > m_IntrHolder;
};

template<class Owner> CObjHolder<CIgapIntron<Owner> > CIgapIntronChain<Owner>::m_IntrHolder;

END_SCOPE(prosplign)
END_NCBI_SCOPE

#endif //INTRON_CHAIN_H
