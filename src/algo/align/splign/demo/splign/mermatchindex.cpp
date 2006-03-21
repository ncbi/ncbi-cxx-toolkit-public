/* $Id$
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
 * Author:  Yuri Kapustin
 *
 * File Description:
 *                   
*/


#include <ncbi_pch.hpp>

#include "mermatchindex.hpp"

#include <algorithm>

BEGIN_NCBI_SCOPE

bool CMerMatcherIndex::s_IsLowComplexity(TKey key, TKey* pkey_rc)
{
    vector<Uint1> counts(4, 0);
    if(pkey_rc) {
        *pkey_rc = 0;
    }

    for(Uint1 k = 0; k < 16; ++k) {

        Uint4 idx = 0x00000003 & key;
        ++counts[idx];
        key >>= 2;
        if(pkey_rc) {
            CMerMatcherIndex::TKey idx_rc;
            switch(idx) {
            case 0: idx_rc = 3; break;
            case 1: idx_rc = 2; break;
            case 2: idx_rc = 1; break;
            case 3: idx_rc = 0; break;
            default: idx_rc = idx;
            }
            *pkey_rc <<= 2;
            *pkey_rc |= idx_rc;
        }
    }

    const Uint1 kMaxSingleBaseContent = 11;
    for(Uint1 k = 0; k < 4; ++k) {
        if(counts[k] >= kMaxSingleBaseContent) {
            return true;
        }
    }

    const Uint1 kMaxTwoBaseContent = 13;
    Uint1 ag = counts[0] + counts[1];
    Uint1 at = counts[0] + counts[2];
    Uint1 ac = counts[0] + counts[3];
    Uint1 gt = counts[1] + counts[2];
    Uint1 gc = counts[1] + counts[3];
    Uint1 tc = counts[2] + counts[3];

    if( ag > kMaxTwoBaseContent || at > kMaxTwoBaseContent ||
        ac > kMaxTwoBaseContent || gt > kMaxTwoBaseContent ||
        gc > kMaxTwoBaseContent || tc > kMaxTwoBaseContent ) 
    {
        return true;
    }

    return false;
}


CMerMatcherIndex::TKey CMerMatcherIndex::s_FlipKey(TKey key0) 
{
    TKey key = 0;
    
    const Uint4 mask = 0x000000FF;
    for(Uint1 i = 0; i < 4; ++i) {
        key  <<= 8;
        key  |=  key0 & mask;
        key0 >>= 8;
    }

    return key;
}


CMerMatcherIndex::CMerMatcherIndex(void):
    m_CurIdx(0),
    m_MaxNodeCount(0),
    m_CurVol(0),
    m_MaxVolSize(0)
{
}


CMerMatcherIndex::SMatch::SMatch(Uint4 oid, Uint2 offs, Uint2 strand):
    m_OID(oid),
    m_Offset(offs),
    m_Strand(strand)
{
    if(m_Strand > 1) {
        NCBI_THROW(CException, eUnknown, 
                   "CMerMatcherIndex::SMatch(): Incorrect strand passed.");
    }
}


// create from a seqdb range
size_t CMerMatcherIndex::Create(CSeqDBIter& db_iter, size_t max_mem, size_t min_len)
{
    if(!db_iter) {
        NCBI_THROW(CException, eUnknown, 
                   "CMerMatcherIndex::Create(): invalid db iterator passed.");
    }

    const Uint1 bytes_per_seg = (sizeof(CMerMatcherIndex::TMatch) 
                                + sizeof(CMerMatcherIndex::SNodeFlat));
    m_MaxNodeCount = 2*((m_MaxVolSize = max_mem) / bytes_per_seg + 1);

    m_Nodes.resize(m_MaxNodeCount);
    m_MatchVec.resize(m_MaxNodeCount);
    m_CurIdx = 0;

    size_t rv = 0;
    for(; db_iter; ++db_iter) {

        int len = db_iter.GetLength();
        if(len < int(min_len)) {
            continue;
        }
        int oid = db_iter.GetOID();
        const char* seqdata = db_iter.GetData();

        bool max_reached = false;
        const Uint4 curidx0 = m_CurIdx;
        for(int k = 0; k*4 + 16 <= len; k += 4) {
            
            const TKey* pkey = reinterpret_cast<const TKey*>(seqdata + k);
            TKey key = *pkey, key_rc;
            if(s_IsLowComplexity(key, &key_rc) == false) {
                if(!x_AddMatch(key, TMatch(oid, k/4, 1)) ||
                   !x_AddMatch(key_rc, TMatch(oid, k/4, 0)))
                {
                    max_reached = true;
                    break;
                }
            }
        }
        
        if(max_reached) {
            m_CurIdx = curidx0;
            break;
        }

        ++rv;
    }

    m_Nodes.resize(m_CurIdx);
    m_MatchVec.resize(m_CurIdx);

    // terminate prior nodes
    NON_CONST_ITERATE(TNodes, ii, m_Nodes) {
        if(ii->m_Left >= m_CurIdx) {
            ii->m_Left = 0;
        }
        if(ii->m_Right >= m_CurIdx) {
            ii->m_Right = 0;
        }
    }

    return rv;
}


bool CMerMatcherIndex::x_AddNode(TKey key, const TMatch& match)
{
    SNode& node = m_Nodes[m_CurIdx];
    node.m_Key = key;
    m_MatchVec[node.m_Data = m_CurIdx].push_back(match);
    ++m_CurIdx;
    m_CurVol += sizeof(SNodeFlat) + sizeof(TMatch);

    return m_CurVol <= m_MaxVolSize && m_CurIdx < m_MaxNodeCount;
}


bool CMerMatcherIndex::x_AddMatch(TKey key, const TMatch& match)
{
    if(m_CurIdx == 0) {
        x_AddNode(key, match);
    }
    else {

        size_t j = 0;
        while(true) {

            SNode& curnode = m_Nodes[j];
            if(key < curnode.m_Key) {
                if(curnode.m_Left == 0) {
                    curnode.m_Left = m_CurIdx;
                    return x_AddNode(key, match);
                }
                j = curnode.m_Left;
            }
            else if(key > curnode.m_Key) {
                if(curnode.m_Right == 0) {
                    curnode.m_Right = m_CurIdx;
                    return x_AddNode(key, match);
                }
                j = curnode.m_Right;
            }
            else {
                m_MatchVec[curnode.m_Data].push_back(match);
                m_CurVol += sizeof(TMatch);
                break;
            }
        }
    }

    return true;
}


void CMerMatcherIndex::x_DiveNode(CNcbiOstream& ostr, Uint4 node_idx, 
                                  Uint4& match_idx)
{
    SNode& node = m_Nodes[node_idx];
    
    if(match_idx != numeric_limits<Uint4>::max()) {
        
        // write matches
        TMatches& matches = m_MatchVec[node.m_Data];
        node.m_Data = match_idx;
        node.m_Count = matches.size();
        ITERATE(TMatches, ii, matches) {
            const TMatch& match = *ii;
            ostr.write(reinterpret_cast<const char*>(&match), sizeof(TMatch));
            ++match_idx;
        }
    }

    if(node.m_Left > 0) {
        x_DiveNode(ostr, node.m_Left, match_idx);
    }

    if(match_idx == numeric_limits<Uint4>::max()) {

        // write the node
        const SNodeFlat& nf = node;
        ostr.write(reinterpret_cast<const char*>(&nf), sizeof nf);        
    }

    if(node.m_Right > 0) {
        x_DiveNode(ostr, node.m_Right, match_idx);
    }
}


void CMerMatcherIndex::Dump(CNcbiOstream& ostr)
{
    //  save the total number of matches

    Uint4 mc_total = 0;
    ITERATE(TMatchVec, ii, m_MatchVec) {
        mc_total += ii->size();
    }
    
    ostr.write(reinterpret_cast<const char*>(&mc_total), sizeof mc_total);
    
    // traverse the node tree to coalesce and save matches

    Uint4 match_idx = 0;
    x_DiveNode(ostr, 0, match_idx);

    // save the total of nodes

    const Uint4 nodes_total = m_Nodes.size();
    ostr.write(reinterpret_cast<const char*>(&nodes_total), sizeof(nodes_total));

    // traverse and save nodes

    match_idx = numeric_limits<Uint4>::max();
    x_DiveNode(ostr, 0, match_idx);
}


bool CMerMatcherIndex::Load(CNcbiIstream& istr)
{
    Uint4 mc_total = 0;
    istr.read(reinterpret_cast<char*>(&mc_total), sizeof mc_total);
    if(istr.fail()) return false;
    m_FlatMatches.resize(mc_total);
    istr.read(reinterpret_cast<char*>(&m_FlatMatches.front()), 
              mc_total * sizeof(TMatch));
    if(istr.fail()) return false;

    Uint4 nodes_total = 0;
    istr.read(reinterpret_cast<char*>(&nodes_total), sizeof nodes_total);
    if(istr.fail()) return false;
    m_FlatNodes.resize(nodes_total);
    istr.read(reinterpret_cast<char*>(&m_FlatNodes.front()), 
              nodes_total * sizeof(SNodeFlat));

    return !istr.fail();
}


Uint4 CMerMatcherIndex::LookUp(TKey key) const
{
    SNodeFlat key_node;
    key_node.m_Key = key;
    TFlatNodes::const_iterator ie = m_FlatNodes.end(),
        ib = m_FlatNodes.begin(),
        ii = lower_bound(ib, ie, key_node);

    return (ii == ie || ii->m_Key != key)? 
        numeric_limits<Uint4>::max():
        Uint4(ii - ib);
}


END_NCBI_SCOPE

/* 
 * $Log$
 * Revision 1.4  2006/03/21 16:20:50  kapustin
 * Various changes, mainly adjust the code with  other libs
 *
 * Revision 1.3  2006/02/14 02:21:08  ucko
 * Use [] rather than .at() for compatibility with GCC 2.95.
 *
 * Revision 1.2  2006/02/13 20:06:10  kapustin
 * Fix log tags
 *
 *
 * Revision 1.1  2006/02/13 20:03:18  kapustin
 * Initial revision
 * 
 */
