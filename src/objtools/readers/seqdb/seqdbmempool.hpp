#ifndef OBJTOOLS_READERS_SEQDB__SEQDBMEMPOOL_HPP
#define OBJTOOLS_READERS_SEQDB__SEQDBMEMPOOL_HPP

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
 * Author:  Kevin Bealer
 *
 */

/// CSeqDBMemPool class
/// 
/// This object controls deletion of a set of memory objects.  Each
/// object is treated here as an array of char, although other layers
/// of the code may treat them differently.

#include <corelib/ncbimtx.hpp>
#include <set>

BEGIN_NCBI_SCOPE

class CSeqDBMemPool {
public:
    CSeqDBMemPool(void)
    {
    }
    
    ~CSeqDBMemPool()
    {
        CFastMutexGuard guard(m_Lock);
        
        //cout << "Destruct of mempool with " << dec << m_Pool.size() << " objects." << endl;
        
        for(set<char*>::iterator i = m_Pool.begin(); i != m_Pool.end(); i++) {
            //cout << "Destructor of mempool is deleting: " << hex << unsigned(*i) << endl;
            delete[] *i;
        }
        
        m_Pool.clear();
        
        //cout << "Destructing mempool" << dec << endl;
    }
    
    void * Alloc(Uint4 length)
    {
        CFastMutexGuard guard(m_Lock);
        
        if (! length) {
            length = 1;
        }
        
        char * newcp = new char[length];
        
        // should possibly throw here on failure?
        
        memset(newcp, 0, length);
        
        m_Pool.insert(newcp);
        
        //cout << "\nAlloc data: " << hex << unsigned(newcp) << endl;
        return newcp;
    }
    
    void Free(void * freeme)
    {
        CFastMutexGuard guard(m_Lock);
        
        // The first check we do is whether m_Pool is empty; in the
        // memory mapped case, this will often be true, making this
        // method nearly a no-op.
        
        if ((! m_Pool.empty()) && freeme) {
            char * cp = (char*) freeme;
            set<char*>::iterator i = m_Pool.find(cp);
            
            if (i != m_Pool.end()) {
                //cout << "\nSuccessful free: " << hex << unsigned(cp) << endl;
                delete[] cp;
                m_Pool.erase(i);
            } else {
                //cout << "\n!! Unsuccessful free: " << hex << unsigned(cp) << endl;
            }
        }
    }
    
private:
    set<char *> m_Pool;
    CFastMutex  m_Lock;
};

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBMEMPOOL_HPP


