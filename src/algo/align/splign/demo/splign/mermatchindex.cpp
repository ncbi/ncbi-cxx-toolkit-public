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
    m_CurIdxMV(0),
    m_CurIdxNodes(0)
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
void CMerMatcherIndex::Create(CSeqDB& seqdb, int oid_begin, int oid_end)
{
    if(oid_begin >= oid_end) {
        NCBI_THROW(CException, eUnknown, "Incorrect oid range");
    }

    double s = 0;
    for(int oid = oid_begin; oid < oid_end; ++oid) {
        s += seqdb.GetSeqLengthApprox(oid);
    }
    const Uint4 Nmax = Uint4(2 * s / 16);

    m_Nodes.resize(Nmax);
    m_CurIdxNodes = 0;
    m_MatchVec.resize(Nmax);
    m_CurIdxMV = 0;

    for(int oid = oid_begin; oid < oid_end; ++oid) {

        const unsigned char* seqdata = 0;
        int len = seqdb.GetSequence(oid, (const char**)(&seqdata));
        for(int k = 0; k*4 + 16 <= len; k += 4) {

            const TKey* pkey = reinterpret_cast<const TKey*>(seqdata + k);
            TKey key = *pkey, key_rc;
            if(s_IsLowComplexity(key, &key_rc) == false) {
                x_AddMatch(key, TMatch(oid, k/4, 1));
                x_AddMatch(key_rc, TMatch(oid, k/4, 0));
            }
        }
        seqdb.RetSequence((const char**)(&seqdata));
    }

    m_Nodes.resize(m_CurIdxNodes);
    m_MatchVec.resize(m_CurIdxMV);
}


void CMerMatcherIndex::x_AddNode(TKey key, const TMatch& match)
{
    SNode& node = m_Nodes.at(m_CurIdxNodes++);
    node.m_Key = key;
    m_MatchVec.at(node.m_Data = m_CurIdxMV++).push_back(match);
}


void CMerMatcherIndex::x_AddMatch(TKey key, const TMatch& match)
{
    const size_t dim = m_Nodes.size();
    if(dim == 0) {
        x_AddNode(key, match);
    }
    else {

        size_t j = 0;
        while(true) {

            SNode& curnode = m_Nodes[j];
            if(key < curnode.m_Key) {
                if(curnode.m_Left == 0) {
                    curnode.m_Left = m_CurIdxNodes;
                    x_AddNode(key, match);
                    break;
                }
                j = curnode.m_Left;
            }
            else if(key > curnode.m_Key) {
                if(curnode.m_Right == 0) {
                    curnode.m_Right = m_CurIdxNodes;
                    x_AddNode(key, match);                    
                    break;
                }
                j = curnode.m_Right;
            }
            else {
                m_MatchVec[curnode.m_Data].push_back(match);
                break;
            }
        }
    }
}


void CMerMatcherIndex::x_DiveNode(CNcbiOstream& ostr, Uint4 node_idx, 
                                  Uint4& match_idx)
{
    SNode& node = m_Nodes[node_idx];
    TMatches& matches = m_MatchVec[node.m_Data];
    node.m_Data = match_idx;
    node.m_Count = matches.size();
    ITERATE(TMatches, ii, matches) {
        const TMatch& match = *ii;
        ostr.write(reinterpret_cast<const char*>(&match), sizeof(TMatch));
        ++match_idx;
    }
    //matches.clear();
    
    if(node.m_Left > 0) {
        x_DiveNode(ostr, node.m_Left, match_idx);
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
    ostr.write(reinterpret_cast<const char*>(&m_Nodes.front()), 
               nodes_total * sizeof(SNode));
}


bool CMerMatcherIndex::Load(CNcbiIstream& istr)
{
    Uint4 mc_total = 0;
    istr.read(reinterpret_cast<char*>(&mc_total), sizeof mc_total);
    if(istr.fail()) return false;
    m_PlainMatches.resize(mc_total);
    istr.read(reinterpret_cast<char*>(&m_PlainMatches.front()), 
              mc_total * sizeof(TMatch));
    if(istr.fail()) return false;

    Uint4 nodes_total = 0;
    istr.read(reinterpret_cast<char*>(&nodes_total), sizeof nodes_total);
    if(istr.fail()) return false;
    m_Nodes.resize(nodes_total);
    istr.read(reinterpret_cast<char*>(&m_Nodes.front()), nodes_total * sizeof(SNode));

    return !istr.fail();
}


Uint4 CMerMatcherIndex::LookUp(TKey key)
{
    Uint4 j = 0;
    do {
        const SNode& node = m_Nodes[j];
        if(key < node.m_Key) j = node.m_Left;
        else if(key > node.m_Key) j = node.m_Right;
        else
            return j;
    } while(j > 0);

    return numeric_limits<Uint4>::max();
}


void CMerMatcherIndex::GetHits(TKey key, 
                               CSeqDB& seqdb,
                               CRef<CSeq_id> seqid_subj,
                               Uint4 subj_min,
                               THitRefs* phitrefs)
{
    Uint4 node_idx = LookUp(key);
    if(node_idx != numeric_limits<Uint4>::max()) {
        
        const SNode& node = m_Nodes[node_idx];
        for(Uint4 i = node.m_Data, iend = node.m_Data + node.m_Count; i < iend; ++i) {
           
            const TMatch& match = m_PlainMatches[i];
            TOidToSeqId::const_iterator im = m_Oid2SeqId.find(match.m_OID);
            CRef<CSeq_id> seqid_query;
            if(im == m_Oid2SeqId.end()) {

                seqid_query = seqdb.GetSeqIDs(match.m_OID).back();
                m_Oid2SeqId[match.m_OID] = seqid_query;
            }
            else {
                seqid_query = im->second;
            }

            THitRef hitref (new THit);
            hitref->SetQueryId(seqid_query);
            hitref->SetSubjId(seqid_subj);
            if(match.m_Strand) {
                hitref->SetSubjStart(subj_min);
                hitref->SetSubjStop(subj_min + 15);
            }
            else {
                hitref->SetSubjStart(subj_min + 15);
                hitref->SetSubjStop(subj_min);
            }
            const Uint4 query_min = match.m_Offset << 4;
            hitref->SetQueryStart(query_min);
            hitref->SetQueryStop(query_min + 15);

            hitref->SetLength(16);
            hitref->SetMismatches(0);
            hitref->SetGaps(0);
            hitref->SetEValue(0);
            hitref->SetIdentity(1.0);
            hitref->SetScore(16.0);
            phitrefs->push_back(hitref);
        }
    }
}

END_NCBI_SCOPE

/* 
 * $Log$
 * Revision 1.2  2006/02/13 20:06:10  kapustin
 * Fix log tags
 *
 *
 * Revision 1.1  2006/02/13 20:03:18  kapustin
 * Initial revision
 * 
 */
