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

#ifndef _MASKED_RANGE_SET_HPP_
#define _MASKED_RANGE_SET_HPP_

#include <objtools/writers/writedb/build_db.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

/** @file masked_range_set.hpp
  FIXME
*/

// First version will be simple -- it will use universal Seq-loc
// logic.  If performance is a problem then it might be best to have
// two containers, one with a Seq-loc key and one with an integer key.

class CMaskedRangeSet : public IMaskDataSource {
    struct SSeqIdKey {
        SSeqIdKey(const CSeq_id & k)
            : id(& k)
        {
        }
        
        CConstRef<CSeq_id> id;
        
        bool operator < (const SSeqIdKey & other) const
        {
            return *id < *other.id;
        }
    };
    
public:
    void Insert(int              algo_id,
                const CSeq_id  & k,
                const CSeq_loc & v)
    {
        x_CombineLocs(x_Set(algo_id, k), v);
    }
    
    virtual TMaskedRanges &
    GetRanges(const list< CRef<CSeq_id> > & idlist);
    
private:
    void x_FindAndCombine(CConstRef<CSeq_loc> & L1, int algo_id, SSeqIdKey & key);
    
    static void x_CombineLocs(CConstRef<CSeq_loc> & L1, const CSeq_loc & L2);
    
    CConstRef<CSeq_loc> & x_Set(int algo_id, SSeqIdKey key);
    
    typedef map< SSeqIdKey, CConstRef<CSeq_loc> > TAlgoMap;
    
    TMaskedRanges m_Ranges;
    vector< TAlgoMap > m_Values;
};

#endif /* _MASKED_RANGE_SET_HPP_ */
