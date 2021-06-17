#ifndef ALGO___LINK_REDUCE_HPP__
#define ALGO___LINK_REDUCE_HPP__

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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:  Link - reduce algorithm
 *
 */

#include <corelib/ncbistd.hpp>

#include <util/bitset/ncbi_bitset.hpp>
#include <util/bitset/bmserial.h>

#include <vector>

BEGIN_NCBI_SCOPE

/// Implementation of Many-to-One mapping with source set reduction.
/// Remapping algorithm relies on existence of pre-calculated (stored) links
/// 
/// <pre>
/// Description:
///    We have two bodies of IDs linked to each other:
///     Every element of Set1 corresponds to one element of Set2
///     Every element of Set2 corresponds to one or more elements of Set1
///    Set1 [Many] <-> Set2 [One]
///
/// Remapping algorithm iterates the input sequence (subset of Set1),
/// for each input id (src_id) it:
///   1. Looks for a correspondent Set2 id (id_set2)
///   2. Loads back link bit-vector (Set2->Set1) (bv_back)
///   3. Subtracts bv_back from the input sequence
///
///   Step 3 - reduction is to make sure every next step we reduce the input
///   as much as possible. 
///
///   This algorithm targets situations when a lot of elements in the input 
///   sequence belong to just a very few sessions. In this case the whole case
///   will quickly end, because every reduction step will close a lot inputs.
///
/// </pre>
///
/// Template parameters and conventions:
///
///  TBV - bit vector to use
///
///  TMapFunc1 - one function class to do step 1 mapping (Set1 -> Set2)
///  TMapFunc2 - class to load the back vector (step 2)
///  TMapFunc1 and TMapFunc2 are used to abstract from the storage format
///  
///  Maps input set id into Set2 id (returned)
///    unsigned TMapFunc1::Remap(unsigned src_id);
///
///  Load the back mapping BLOB (bit-vector). 
///  Returns true on success. false means the link is missing.
///     bool TMapFunc2::ReadBlob(unsigned id_set2, TBuffer& buf);
///   
///
template<class TBV, class TMapFunc1, class TMapFunc2>
class CLinkReduce
{
public:
    typedef  TBV                    TBitVector;
    typedef  vector<unsigned char>  TBuffer;
public:
    CLinkReduce(TMapFunc1& map_func1, TMapFunc2& map_func2);
    ~CLinkReduce();

    /// Run the remapping.
    ///
    /// @param src_set
    ///     Source set, subject for link remapping. 
    ///     All successfully remapped ids are getting cleaned from the set.
    ///
    /// @param dst_set
    ///     Destination set
    ///
    void Remap(TBV& src_set, TBV& dst_set);
private:
    CLinkReduce(const CLinkReduce&);
    CLinkReduce& operator=(const CLinkReduce&);
private:
    TMapFunc1&    m_MapFunc1;
    TMapFunc2&    m_MapFunc2;
    TBuffer       m_Buffer;
    bm::word_t*   m_STmpBlock;
};

///////////////////////////////////////////////////////////////////////////
//

template<class TBV, class TMapFunc1, class TMapFunc2 >
inline
CLinkReduce<TBV, TMapFunc1, TMapFunc2>::CLinkReduce(
                               TMapFunc1& map_func1, TMapFunc2& map_func2)
 : m_MapFunc1(map_func1),
   m_MapFunc2(map_func2),
{
}

template<class TBV, class TMapFunc1, class TMapFunc2>
inline
CLinkReduce<TBV, TMapFunc1, TMapFunc2>::~CLinkReduce()
{
    if (m_STmpBlock) {
        m_TmpBVec.free_tempblock(m_STmpBlock);
    }
}


template<class TBV, class TMapFunc1, class TMapFunc2>
inline
void CLinkReduce<TBV, TMapFunc1, TMapFunc2>::Remap(TBV& src_set, TBV& dst_set)
{
    // here i presume the sequence starts from 1 and 0 never happens
    // (if it ever happens we need a fix)
    //
    _ASSERT(src_set.test(0) == false);

    if (m_STmpBlock == 0) {
        m_STmpBlock = m_TmpBVec.allocate_tempblock();
    }

    unsigned src_id = 0;
    do {
        src_id = src_set.get_next(src_id);
        if (!src_id) {
            break;
        }
        
        unsigned target_id = m_MapFunc1.Remap(src_id);
        if (!target_id) {
            continue;
        }
        src_set.set(src_id, false);
        dst_set.set(target_id);

        // back reduction

        bool exists = m_MapFunc2.ReadBlob(target_id, m_Buffer);
        if (exists) { // reduction is possible
            bm::operation_deserializer<TBV>::deserialize(src_set,
                                                         &(m_Buffer[0]),
      	                                                 m_STmpBlock,
                                                         bm::set_SUB);
            
        }

    } while (1);
}



END_NCBI_SCOPE

#endif
