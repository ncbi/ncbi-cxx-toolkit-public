#ifndef ALGO_ALIGN_SPLIGN_DEMO_SPLIGN_MERMATCHIDX_HPP
#define ALGO_ALIGN_SPLIGN_DEMO_SPLIGN_MERMATCHIDX_HPP


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

#include <corelib/ncbistd.hpp>

#include <algo/align/splign/splign.hpp>

#include <objtools/readers/seqdb/seqdb.hpp>

BEGIN_NCBI_SCOPE

class CMerMatcherIndex
{
public:

    CMerMatcherIndex(void);

    // create using seq db iterator and target size
    size_t Create(CSeqDBIter& db_iter, size_t max_mem, size_t min_len);

    // load from a binary file
    bool Load(CNcbiIstream&);

    // write index to a file
    void Dump(CNcbiOstream&);

    typedef Uint4 TKey;
    typedef Uint4 TData;

    static bool s_IsLowComplexity(TKey key, TKey* key_rc = 0);
    static TKey s_FlipKey(CMerMatcherIndex::TKey key0) ;


    Uint4 LookUp(TKey key) const; // will return numeric_limits<Uint4>::max() 
                                  // if not found; index in m_Nodes otherwise

    typedef CSplign::THit    THit;
    typedef CRef<THit>       THitRef;
    typedef vector<THitRef>  THitRefs;

    struct SNodeFlat {

        SNodeFlat() {}

        TKey  m_Key;
        TData m_Data;
        Uint4 m_Count;

        // for debug purpose
        friend ostream& operator << (ostream& ostr, const SNodeFlat& m) {
            ostr << m.m_Key << '\t' << m.m_Data << '\t' << m.m_Count;
            return ostr;
        }

        bool operator < (const SNodeFlat& rhs) const {
            return m_Key < rhs.m_Key;
        }
    };

    struct SNode: public SNodeFlat {

        SNode(): m_Left(0), m_Right(0) { m_Count = 0; }

        Uint4 m_Left;
        Uint4 m_Right;
    };

    const SNodeFlat& GetNode(Uint4 idx) const {
        return m_FlatNodes[idx];
    }
    
    struct SMatch {

        SMatch(){};
        SMatch(Uint4 oid, Uint2 offs, Uint2 strand);

        Uint4 m_OID;
        Uint2 m_Offset;
        Uint2 m_Strand;
        
        // for debug purpose
        friend ostream& operator << (ostream& ostr, const SMatch& m) {
            ostr << m.m_OID << '\t' << m.m_Offset << '\t' << m.m_Strand;
            return ostr;
        }
    };

    typedef SMatch            TMatch;

    const TMatch& GetMatch(Uint4 idx) const {
        return m_FlatMatches[idx];
    }

protected:

    typedef list<TMatch>      TMatches;
    typedef vector<TMatches>  TMatchVec;
    TMatchVec       m_MatchVec;

    typedef vector<SNode>     TNodes;
    TNodes          m_Nodes;

    typedef vector<SNodeFlat> TFlatNodes;
    TFlatNodes      m_FlatNodes;

    Uint4           m_CurIdx;
    Uint4           m_MaxNodeCount;
    Uint4           m_CurVol;
    Uint4           m_MaxVolSize;

    typedef vector<TMatch>    TFlatMatches;
    TFlatMatches   m_FlatMatches;

    typedef map<int, CRef<CSeq_id> > TOidToSeqId;
    TOidToSeqId     m_Oid2SeqId;

    // indexing
    bool x_AddMatch(TKey key, const TMatch& match);
    bool x_AddNode(TKey key, const TMatch& match);

    // dumping
    void x_DiveNode(CNcbiOstream& ostr, Uint4 node_idx, Uint4& match_idx);

};

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

#endif
