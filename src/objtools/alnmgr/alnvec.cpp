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
    : CAlnMap(ds),
      m_ConsensusSeq(-1),
      m_set_GapChar(false),
      m_set_EndChar(false)
{
}


CAlnVec::CAlnVec(const CDense_seg& ds, TNumrow anchor)
    : CAlnMap(ds, anchor),
      m_ConsensusSeq(-1),
      m_set_GapChar(false),
      m_set_EndChar(false)
{
}


CAlnVec::CAlnVec(const CDense_seg& ds, CScope& scope) 
    : CAlnMap(ds),
      m_Scope(&scope),
      m_ConsensusSeq(-1),
      m_set_GapChar(false),
      m_set_EndChar(false)
{
}


CAlnVec::CAlnVec(const CDense_seg& ds, TNumrow anchor, CScope& scope)
    : CAlnMap(ds, anchor),
      m_Scope(&scope),
      m_ConsensusSeq(-1),
      m_set_GapChar(false),
      m_set_EndChar(false)
{
}


CAlnVec::~CAlnVec(void)
{
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


CSeqVector& CAlnVec::x_GetSeqVector(TNumrow row) const
{
    TSeqVectorCache::iterator iter = m_SeqVectorCache.find(row);
    if (iter != m_SeqVectorCache.end()) {
        return *(iter->second);
    } else {
        CSeqVector vec = GetBioseqHandle(row).GetSeqVector
            (CBioseq_Handle::eCoding_Iupac,
             IsPositiveStrand(row) ? 
             CBioseq_Handle::eStrand_Plus :
             CBioseq_Handle::eStrand_Minus);
        CRef<CSeqVector> seq_vec(new CSeqVector(vec));
        return *(m_SeqVectorCache[row] = seq_vec);
    }
}


string CAlnVec::GetAlnSeqString(TNumrow row, const TSignedRange& aln_rng) const
{
    string str, str_buff;
    
    // get the chunks which are aligned to seq on anchor
    CRef<CAlnMap::CAlnChunkVec> chunk_vec = 
        GetAlnChunks(row, aln_rng, fSkipInserts | fSkipUnalignedGaps);
    
    // for each chunk
    for (int i=0; i<chunk_vec->size(); i++) {
        CConstRef<CAlnMap::CAlnChunk> chunk = (*chunk_vec)[i];
                
        if (chunk->GetType() & fSeq) {
            // add the sequence string
            x_GetSeqVector(row).GetSeqData(chunk->GetRange().GetFrom(),
                                           chunk->GetRange().GetTo()+1,
                                           str_buff);

            str += str_buff;
        } else {
            // add appropriate number of gap/end chars
            const int n = chunk->GetAlnRange().GetLength();
            char* ch_buff = new char[n+1];
            char fill_ch;
            if (chunk->GetType() & fNoSeqOnLeft  ||
                chunk->GetType() & fNoSeqOnRight) {
                fill_ch = GetEndChar();
            } else {
                fill_ch = GetGapChar(row);
            }
            memset(ch_buff, fill_ch, n);
            ch_buff[n] = 0;
            str += ch_buff;
            delete ch_buff;
        }
    }
    return str;
}


string& CAlnVec::GetAlnSeqString(string& buffer,
                                 TNumrow row,
                                 const TSignedRange& aln_rng) const
{
    string buff;
    buffer.clear();
    
    // get the chunks which are aligned to seq on anchor
    CRef<CAlnMap::CAlnChunkVec> chunk_vec = 
        GetAlnChunks(row, aln_rng, fSkipInserts | fSkipUnalignedGaps);
    
    // for each chunk
    for (int i=0; i<chunk_vec->size(); i++) {
        CConstRef<CAlnMap::CAlnChunk> chunk = (*chunk_vec)[i];
                
        if (chunk->GetType() & fSeq) {
            // add the sequence string
            x_GetSeqVector(row).GetSeqData(chunk->GetRange().GetFrom(),
                                           chunk->GetRange().GetTo()+1,
                                           buff);
            buffer += buff;
        } else {
            // add appropriate number of gap/end chars
            const int n = chunk->GetAlnRange().GetLength();
            char* ch_buff = new char[n+1];
            char fill_ch;
            if (chunk->GetType() & fNoSeqOnLeft  ||
                chunk->GetType() & fNoSeqOnRight) {
                fill_ch = GetEndChar();
            } else {
                fill_ch = GetGapChar(row);
            }
            memset(ch_buff, fill_ch, n);
            ch_buff[n] = 0;
            buffer += ch_buff;
            delete ch_buff;
        }
    }
    return buffer;
}


//
// CreateConsensus()
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
void CAlnVec::CreateConsensus(void)
{
    if ( !m_DS  ||  IsSetConsensus() ) {
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

                    x_GetSeqVector(i).GetSeqData(start, stop, segs[i]);

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
                int base_count[4];
                base_count[0] = base_count[1] =
                    base_count[2] = base_count[3] = 0;
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

                for (int k = 0;  k < 4;  ++k) {
                    // this gets around a potentially tricky idiosyncrasy
                    // in some implementations of multimap.  depending on
                    // the library, the key may be const (or not)
                    TRevMap::value_type p(base_count[k], (1<<k));
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
    // now, create a new CDense_seg
    // we create a new CBioseq for our data, add it to our scope, and
    // copy the contents of the CDense_seg
    //
    string data;
    TSignedSeqPos total_bases = 0;

    CRef<CDense_seg> new_ds(new CDense_seg());
    new_ds->SetDim(m_DS->GetDim() + 1);
    new_ds->SetNumseg(m_DS->GetNumseg());
    new_ds->SetLens() = m_DS->GetLens();
    new_ds->SetStarts().reserve(m_DS->GetStarts().size() + m_DS->GetNumseg());
    if ( !m_DS->GetStrands().empty() ) {
        new_ds->SetStrands().reserve(m_DS->GetStrands().size() +
                                     m_DS->GetNumseg());
    }

    for (i = 0;  i < consens.size();  ++i) {
        // copy the old entries
        for (j = 0;  j < m_DS->GetDim();  ++j) {
            int idx = i * m_DS->GetDim() + j;
            new_ds->SetStarts().push_back(m_DS->GetStarts()[idx]);
            if ( !m_DS->GetStrands().empty() ) {
                new_ds->SetStrands().push_back(m_DS->GetStrands()[idx]);
            }
        }

        // add our new entry
        // this places the consensus as the last sequence
        // it should preferably be the first, but this would mean adjusting
        // the bioseq handle and seqvector caches, and all row numbers would
        // shift
        if (consens[i].length() != 0) {
            new_ds->SetStarts().push_back(total_bases);
        } else {
            new_ds->SetStarts().push_back(-1);
        }
        
        if ( !m_DS->GetStrands().empty() ) {
            new_ds->SetStrands().push_back(eNa_strand_unknown);
        }

        total_bases += consens[i].length();
        data += consens[i];
    }

    // copy our IDs
    for (i = 0;  i < m_DS->GetIds().size();  ++i) {
        new_ds->SetIds().push_back(m_DS->GetIds()[i]);
    }

    // now, we construct a new Bioseq and add it to our scope
    // this bioseq must have a local ID; it will be named "consensus"
    // once this is in, the Denseg should resolve all IDs correctly
    {{
         CRef<CBioseq> bioseq(new CBioseq());

         // construct a local sequence ID for this sequence
         CRef<CSeq_id> id(new CSeq_id());
         bioseq->SetId().push_back(id);
         id->SetLocal().SetStr("consensus");

         new_ds->SetIds().push_back(id);

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
         CRef<CSeq_entry> entry(new CSeq_entry());
         entry->SetSeq(*bioseq);
         GetScope().AddTopLevelSeqEntry(*entry);
    }}

    // drop the old, bring in the new
    m_DS.Reset(new_ds.Release());

    // make sure that the consensus sequence row indicator is saved
    // by default, this is placed in the last row of the alignment
    m_ConsensusSeq = m_DS->GetDim() - 1;
    
#if 0

    cerr << "final consensus: " << data.length() << " bases" << endl;
    cerr << data << endl;

    cerr << "dense-seg:" << endl;
    for (i = 0;  i < m_DS->GetDim();  ++i) {
        if (i != m_DS->GetDim()-1) {
            cerr << m_DS->GetIds()[i]->GetGi() << ": ";
        }
        else {
            cerr << "consensus : ";
        }
        for (j = 0;  j < m_DS->GetNumseg();  ++j) {
            cerr << m_DS->GetStarts()[ j*m_DS->GetDim() + i ] << ", ";
        }
        cerr << endl;
    }

#endif
}

END_objects_SCOPE // namespace ncbi::objects::
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.15  2003/01/17 18:16:53  todorov
* Added a better-performing set of GetXXXString methods
*
* Revision 1.14  2003/01/16 20:46:17  todorov
* Added Gap/EndChar set flags
*
* Revision 1.13  2003/01/08 16:50:56  todorov
* Fixed TGetChunkFlags in GetAlnSeqString
*
* Revision 1.12  2002/11/04 21:29:08  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.11  2002/10/21 19:15:20  todorov
* added GetAlnSeqString
*
* Revision 1.10  2002/10/08 18:03:15  todorov
* added the default m_EndChar value
*
* Revision 1.9  2002/10/01 14:13:22  dicuccio
* Added handling of strandedness in creation of consensus sequence.
*
* Revision 1.8  2002/09/25 20:20:24  todorov
* x_GetSeqVector uses the strand info now
*
* Revision 1.7  2002/09/25 19:34:54  todorov
* "un-inlined" x_GetSeqVector
*
* Revision 1.6  2002/09/25 18:16:29  dicuccio
* Reworked computation of consensus sequence - this is now stored directly
* in the underlying CDense_seg
* Added exception class; currently used only on access of non-existent
* consensus.
*
* Revision 1.5  2002/09/19 18:24:15  todorov
* New function name for GetSegSeqString to avoid confusion
*
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
