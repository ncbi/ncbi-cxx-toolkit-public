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


#ifndef OBJECT_HOLDER_H
#define OBJECT_HOLDER_H

#include <corelib/ncbistl.hpp>
#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(prosplign)

template<class T>
class CObjHolder
{
    static const int m_MaxAllocSize = 1024*1024;
public:
    inline CObjHolder(int alloc_size = 1024) : m_PointerHolder(NULL), m_AllocSize(alloc_size) {}
    ~CObjHolder()
    {
        for(size_t i = 0; i < m_ObjHolder.size(); ++i)
            delete[] m_ObjHolder[i];
    }
    inline void Add(T*tail, T* head) { head->m_Prev = m_PointerHolder; m_PointerHolder = tail;} 
    inline T *Get() { 
        if(m_PointerHolder == NULL) {
            T* new_batch = new T[m_AllocSize];
            m_ObjHolder.push_back(new_batch);
            for (int i = 0; i<m_AllocSize; ++i) {
                new_batch->m_Prev = m_PointerHolder;
                m_PointerHolder = new_batch++;
            }
            //change alloc size
            if(m_AllocSize < m_MaxAllocSize) {
                m_AllocSize *=2;
                if(m_AllocSize > m_MaxAllocSize) m_AllocSize = m_MaxAllocSize;
            }
        }
        T *tmp = m_PointerHolder;
        m_PointerHolder = tmp->m_Prev;
        return tmp;
    }
private:        
    T * m_PointerHolder;
    vector<T*> m_ObjHolder;
    int m_AllocSize;
};


END_SCOPE(prosplign)
END_NCBI_SCOPE

#endif //OBJECT_HOLDER_H
