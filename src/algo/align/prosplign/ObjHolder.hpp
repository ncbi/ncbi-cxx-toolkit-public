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
public:
    inline CObjHolder(int alloc_size = 1024) : m_AllocSize(alloc_size) {}
 //   inline int size() const { return (int)m_Holder.size(); }
    inline void Add(T *pt) { m_PointerHolder.push_back(pt); } 
    inline T *Get() { 
        if(m_PointerHolder.empty()) {
            //swap
            vector<vector<T> > vtmp;
            vtmp.swap(m_ObjHolder);
            m_ObjHolder.resize(vtmp.size() + 1);
            /*  does not work because of bug in Linux compiler
            typedef vector<vector<T> >::iterator VVIT;
            vector<vector<T> >::iterator vit = vtmp.begin();
            vector<vector<T> >::iterator oit = m_ObjHolder.begin();
            for(;vit != vtmp.end(); ++vit, ++oit) vit->swap(*oit);
            */
            int ii;
            for(ii=0;ii != (int)vtmp.size(); ++ii) vtmp[ii].swap(m_ObjHolder[ii]);
            /*end of workaround*/
            //deal with the new one
            m_ObjHolder.back().resize(m_AllocSize);
            /* does not work because of bug in Linux compiler, workaround below
            for(vector<T>::iterator it = m_ObjHolder.back().begin(); it!= m_ObjHolder.back().end(); ++it) {
                m_PointerHolder.push_back(&*it);
            }
            */
            for(ii=0;ii<(int)m_ObjHolder.back().size(); ++ii) m_PointerHolder.push_back(&(m_ObjHolder.back()[ii]));
            /*end of workaround*/
            //change alloc size
            if(m_AllocSize < m_MaxAllocSize) m_AllocSize *=2;
            if(m_AllocSize > m_MaxAllocSize) m_AllocSize = m_MaxAllocSize;
        }
        T *tmp = m_PointerHolder.back();
        m_PointerHolder.pop_back();
        return tmp;
    }
private:        
    vector<T *> m_PointerHolder;
    vector<vector<T> > m_ObjHolder;
    int m_AllocSize;
    static int m_MaxAllocSize;
};

template<class T> int CObjHolder<T>::m_MaxAllocSize = 1024*1024;

END_SCOPE(prosplign)
END_NCBI_SCOPE

#endif //OBJECT_HOLDER_H
