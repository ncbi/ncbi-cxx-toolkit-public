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
* Author:  Kamen Todorov, NCBI
*
* File Description:
*   Access to the actual aligned residues
*
* ===========================================================================
*/

#include <objects/alnmgr/alnvec.hpp>

// Objects includes
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/IUPACna.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_loc.hpp>

// Object Manager includes
#include <objects/objmgr/gbloader.hpp>
#include <objects/objmgr/object_manager.hpp>
#include <objects/objmgr/reader_id1.hpp>
#include <objects/objmgr/scope.hpp>
#include <objects/objmgr/seq_vector.hpp>


BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

CAlnVec::CAlnVec(const CDense_seg& ds) 
    : CAlnMap(ds)
{
}


CAlnVec::CAlnVec(const CDense_seg& ds, TNumrow anchor)
    : CAlnMap(ds, anchor)
{
}


CAlnVec::CAlnVec(const CDense_seg& ds, CScope& scope) 
    : CAlnMap(ds), m_Scope(&scope)
{
}


CAlnVec::CAlnVec(const CDense_seg& ds, TNumrow anchor, CScope& scope)
    : CAlnMap(ds, anchor), m_Scope(&scope)
{
}


CAlnVec::~CAlnVec(void)
{
}


string CAlnVec::GetSeqString(TNumrow row, TSeqPos from, TSeqPos to) const
{
    string buff;
    x_GetSeqVector(row).GetSeqData(from, to + 1, buff);
    return buff;
}


string CAlnVec::GetSeqString(TNumrow row, TNumseg seg, int offset) const
{
    string buff;
    x_GetSeqVector(row).GetSeqData(GetStart(row, seg, offset),
                                   GetStop (row, seg, offset) + 1,
                                   buff);
    return buff;
}


string CAlnVec::GetSeqString(TNumrow row, const CRange<TSeqPos>& range) const
{
    string buff;
    x_GetSeqVector(row).GetSeqData(range.GetFrom(), range.GetTo() + 1, buff);
    return buff;
}


string CAlnVec::GetConsensusString(TSeqPos start, TSeqPos stop) const
{
    string s;
    x_GetConsensusSeqVector().GetSeqData(start, stop + 1, s);
    return s;
}

string CAlnVec::GetConsensusString(TNumseg seg) const
{
    string s;
    TSeqPos start = GetConsensusStart(seg);
    TSeqPos stop  = GetConsensusStop (seg);

    x_GetConsensusSeqVector().GetSeqData(start, stop + 1, s);
    return s;
}


const CBioseq_Handle& CAlnVec::GetBioseqHandle(TNumrow row) const
{
    TBioseqHandleCache::iterator i = m_BioseqHandlesCache.find(row);
    
    if (i != m_BioseqHandlesCache.end()) {
        return i->second;
    } else {
        return m_BioseqHandlesCache[row] = 
            GetScope().GetBioseqHandle(GetSeqId(row));
    }
}

CScope& CAlnVec::GetScope(void) const
{
    if (!m_Scope) {
        m_ObjMgr = new CObjectManager;
        
        m_ObjMgr->RegisterDataLoader
            (*new CGBDataLoader("ID", NULL /*new CId1Reader*/, 2),
             CObjectManager::eDefault);

        m_Scope = new CScope(*m_ObjMgr);
        m_Scope->AddDefaults();
    }
    return *m_Scope;
}

//
// SetAnchorConsensus()
// this functon resets the anchor to reflect the generated consensus sequence
//
void CAlnVec::SetAnchorConsensus(void)
{
    if ( !m_ConsensusBioseq ) {
        x_CreateConsensus();
    }

    x_SetAnchor(m_ConsensusStarts, m_DS->GetLens(),
                1, m_DS->GetNumseg(), 0);

    // we set the anchor to an invalid position beyond the end of the array
    // this is okay, as the anchor is not used to index into this array
    m_Anchor = m_DS->GetDim();
}


//
// x_CreateConsensus()
//
// compute a consensus sequence given a particular alignment
// the rules for a consensus are:
//   - a segment is consensus gap if > 50% of the sequences are gap at this
//     segment.  50% exactly is counted as sequence
//   - for a segment counted as sequence, for each position, the most
//     frequently occurring base is counted as consensus.  in the case of
//     a tie, the consensus is considered muddied, and the consensus is
//     so marked
//
void CAlnVec::x_CreateConsensus(void) const
{
    if ( !m_DS ) {
        return;
    }

    int i;
    int j;

    // temporary storage for our consensus
    vector<string> consens(m_DS->GetNumseg());

    // determine what the number of segments required for a gapped consensus
    // segment is.  this must be rounded to be at least 50%.
    int gap_seg_thresh = m_DS->GetDim() - m_DS->GetDim() / 2;

    for (j = 0;  j < m_DS->GetNumseg();  ++j) {
        // evaluate for gap / no gap
        int gap_count = 0;
        for (i = 0;  i < m_DS->GetDim();  ++i) {
            if (m_DS->GetStarts()[ j*m_DS->GetDim() + i ] == -1) {
                ++gap_count;
            }
        }

        // check to make sure that this seg is not a consensus
        // gap seg
        if ( gap_count <= gap_seg_thresh ) {
            // we will build a segment with enough bases to match
            consens[j].resize(m_DS->GetLens()[j]);

            // retrieve all sequences for this segment
            vector<string> segs(m_DS->GetDim());
            for (i = 0;  i < m_DS->GetDim();  ++i) {
                if (m_DS->GetStarts()[ j*m_DS->GetDim() + i ] != -1) {
                    TSeqPos start = m_DS->GetStarts()[j*m_DS->GetDim()+i];
                    TSeqPos stop  = start + m_DS->GetLens()[j];

                    CSeqVector& vec = x_GetSeqVector(i);
                    vec.GetSeqData(start, stop, segs[i]);

                    for (int c = 0;  c < segs[i].length();  ++c) {
                        segs[i][c] = FromIupac(segs[i][c]);
                    }
                }
            }

            typedef multimap<int, unsigned char, greater<int> > TRevMap;

            // 
            // evaluate for a consensus
            //
            for (i = 0;  i < m_DS->GetLens()[j];  ++i) {
                // first, we record which bases occur and how often
                // this is computed in NCBI4na notation
                vector<int> base_count(4, 0);
                for (int row = 0;  row < m_DS->GetDim();  ++row) {
                    if (segs[row] != "") {
                        for (int pos = 0;  pos < 4;  ++pos) {
                            if (segs[row][i] & (1<<pos)) {
                                ++base_count[ pos ];
                            }
                        }
                    }
                }

                // we create a sorted list (in descending order) of
                // frequencies of appearance to base
                // the frequency is "global" for this position: that is,
                // if 40% of the sequences are gapped, the highest frequency
                // any base can have is 0.6
                TRevMap rev_map;

                vector<int>::iterator iter;
                for (iter = base_count.begin();
                     iter != base_count.end();
                     ++iter) {
                    // this gets around a potentially tricky idiosyncrasy
                    // in some implementations of multimap.  depending on
                    // the library, the key may be const (or not)
                    TRevMap::value_type p (*iter,
                                           (1<<(iter-base_count.begin())));
                    rev_map.insert(p);
                }

                // the base threshold for being considered unique is at least
                // 70% of the available sequences
                int base_thresh =
                    ((m_DS->GetDim() - gap_count) * 7 + 5) / 10;

                // now, the first element here contains the best frequency
                // we scan for the appropriate bases
                if (rev_map.count(rev_map.begin()->first) == 1 &&
                    rev_map.begin()->first >= base_thresh) {
                    consens[j][i] = ToIupac(rev_map.begin()->second);
                } else {
                    // now we need to make some guesses based on IUPACna
                    // notation
                    int               count;
                    unsigned char     c    = 0x00;
                    int               freq = 0;
                    TRevMap::iterator curr = rev_map.begin();
                    TRevMap::iterator prev = rev_map.begin();
                    for (count = 0;
                         curr != rev_map.end() &&
                         (freq < base_thresh || prev->first == curr->first);
                         ++curr, ++count) {
                        prev = curr;
                        freq += curr->first;
                        c |= curr->second;
                    }

                    //
                    // catchall
                    //
                    if (count > 2) {
                        consens[j][i] = 'N';
                    } else {
                        consens[j][i] = ToIupac(c);
                    }
                }
            }
        }
    }

    //
    // now, convert to a CBioseq
    //
    string data;
    TSignedSeqPos total_bases = 0;
    m_ConsensusStarts.clear();
    m_ConsensusStarts.resize(consens.size(), -1);

    for (i = 0;  i < consens.size();  ++i) {
        if (consens[i].length() != 0) {
            m_ConsensusStarts[i] = total_bases;
        }

        total_bases += consens[i].length();
        data += consens[i];
    }

    {{
         CRef<CBioseq> bioseq = new CBioseq();

         // construct a local sequence ID for this sequence
         CRef<CSeq_id> id(new CSeq_id());
         bioseq->SetId().push_back(id);
         id->SetLocal().SetStr("consensus");

         // add a description for this sequence
         CSeq_descr& desc = bioseq->SetDescr();
         CRef<CSeqdesc> d(new CSeqdesc);
         desc.Set().push_back(d);
         d->SetComment("This is a generated consensus sequence");

         // the main one: Seq-inst
         CSeq_inst& inst = bioseq->SetInst();
         inst.SetRepr(CSeq_inst::eRepr_raw);
         inst.SetMol(CSeq_inst::eMol_na);
         inst.SetLength(data.length());

         CSeq_data& seq_data = inst.SetSeq_data();
         CIUPACna& na = seq_data.SetIupacna();
         na = data;

         // once we've created the bioseq, we need to add it to the
         // scope
         CRef<CSeq_entry> entry = new CSeq_entry();
         entry->SetSeq(*bioseq);

         GetScope().AddTopLevelSeqEntry(*entry);

         CSeq_id seq_id;
         seq_id.SetLocal().SetStr() = "consensus";
         m_ConsensusBioseq = GetScope().GetBioseqHandle(seq_id);
     }}

#if 0

    cerr << "final consensus: " << data.length() << " bases" << endl;
    cerr << data << endl;

    cerr << "consensus starts: ";
    for (i = 0;  i < m_ConsensusStarts.size();  ++i) {
        cerr << m_ConsensusStarts[i];
        if (i < m_ConsensusStarts.size()-1) {
            cerr << ", ";
        }
    }
    cerr << endl;

    cerr << "consensus lens: ";
    for (i = 0;  i < m_DS->GetLens().size();  ++i) {
        cerr << m_DS->GetLens()[i];
        if (i < m_DS->GetLens().size()-1) {
            cerr << ", ";
        }
    }
    cerr << endl;

#endif
}


END_objects_SCOPE // namespace ncbi::objects::
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.4  2002/09/19 17:40:16  todorov
* fixed m_Anchor setting in case of consensus
*
* Revision 1.3  2002/09/05 19:30:39  dicuccio
* - added ability to reference a consensus sequence for a given alignment
* - added caching for CSeqVector objects (big performance gain)
* - many small bugs fixed
*
* Revision 1.2  2002/08/29 18:40:51  dicuccio
* added caching mechanism for CSeqVector - this greatly improves speed in
* accessing sequence data.
*
* Revision 1.1  2002/08/23 14:43:52  ucko
* Add the new C++ alignment manager to the public tree (thanks, Kamen!)
*
*
* ===========================================================================
*/
