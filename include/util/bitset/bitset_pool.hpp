#ifndef UTIL___BV_POOL__HPP
#define UTIL___BV_POOL__HPP

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
 * Author:  Anatoliy Kuznetsov
 *    Bit-vector resource pool.
 */

#include <util/resource_pool.hpp>

BEGIN_NCBI_SCOPE


/** @addtogroup ResourcePool
 *
 * @{
 */

/// Resource pool for bit-vectors
/// Bit-vector creation-destruction can be expensive, use pool to
/// recycle used objects
///
template<class BV, class Lock=CNoLock>
class CBVResourcePool : public CResourcePool<BV, Lock>
{
public:
    typedef CResourcePool<BV, Lock>  TParent;

public:
    CBVResourcePool(bm::strategy strat=bm::BM_GAP) 
    : CResourcePool<BV, Lock>(),
      m_Strat(strat)
    {}

    /// Get bitvector 
    /// Method clears the existing vector, so it looks like new.
    /// (Do not call bvector<>::clear() on returned object)
    ///
    BV* Get()
    {
        BV* bv = this->GetIfAvailable();
        if (!bv) {
            bv = new BV(m_Strat, bm::gap_len_table_min<true>::_len);
        } else {
            bv->clear(true);
        }
        return bv;
    }
protected:
    bm::strategy          m_Strat; ///< bitset making strategy
};


/* @} */

END_NCBI_SCOPE


#endif  /* UTIL___BV_POOL__HPP */
