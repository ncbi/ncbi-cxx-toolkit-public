#ifndef UTIL___BITSET_ALLOC__HPP
#define UTIL___BITSET_ALLOC__HPP

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
 * Authors:  Anatoliy Kuznetsov
 *
 */

/// @file ncbi_bitset_alloc.hpp
/// Bitset allocator 

#include <corelib/ncbi_safe_static.hpp>

#include <util/bitset/ncbi_bitset.hpp>
#include <util/bitset/bmalloc.h>
#include <util/resource_pool.hpp>

BEGIN_NCBI_SCOPE


/// Thread-safe pool block allocator for NCBI bitsets
///
/// Class uses static pool: be careful about multiple DLL instantiations
///
template<class BA = bm::block_allocator,
         class Lock = CNoLock>
class CBV_PoolBlockAlloc
{
public:
    typedef BA  allocator_type;

public:
    static bm::word_t* allocate(size_t n, const void *p)
    {
        typename TBacketPool::TResourcePool* rp
            = x_Instance().GetResourcePool(n);
        bm::word_t* block = rp->GetIfAvailable();
        if (!block) {
            block = allocator_type::allocate(n, p);
        }
        return block;
    }

    static void deallocate(bm::word_t* p, size_t n)
    {
        typename TBacketPool::TResourcePool* rp
            = x_Instance().GetResourcePool(n);
        rp->Put(p);
    }

protected:
    struct CBlockFactory
    {
    /// Dummy (should be never called)
    static bm::word_t* Create() { _ASSERT(0); return 0; }
    /// Delete forwards this to SGI style STL allocator
    static void Delete(bm::word_t* block) 
        { allocator_type::deallocate(block, 0); }
    };

    typedef CResourcePool<bm::word_t, Lock, CBlockFactory> TResourcePool;
    typedef CBucketPool<bm::word_t, Lock, TResourcePool>   TBacketPool;

private:
    static TBacketPool& x_Instance() 
    {
        static CSafeStaticPtr<TBacketPool>  backet_pool;
        return backet_pool.Get();
    }
};


END_NCBI_SCOPE

#endif /* UTIL___BITSET_ALLOC__HPP */
