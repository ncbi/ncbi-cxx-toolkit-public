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

#include <ncbi_pch.hpp>
#include <objtools/alnmgr/alnvec.hpp>

// Objects includes
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/IUPACna.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>

// Object Manager includes
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>

#include <util/tables/raw_scoremat.h>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

CAlnVec::CAlnVec(const CDense_seg& ds, CScope& scope) 
    : CAlnMap(ds),
      m_Scope(&scope),
      m_set_GapChar(false),
      m_set_EndChar(false)
{
}


CAlnVec::CAlnVec(const CDense_seg& ds, TNumrow anchor, CScope& scope)
    : CAlnMap(ds, anchor),
      m_Scope(&scope),
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
        CBioseq_Handle bioseq_handle = 
            GetScope().GetBioseqHandle(GetSeqId(row));
        if (bioseq_handle) {
            return m_BioseqHandlesCache[row] = bioseq_handle;
        } else {
            string errstr = string("CAlnVec::GetBioseqHandle(): ") 
                + "Seq-id cannot be resolved: "
                + GetSeqId(row).AsFastaString();
            
            NCBI_THROW(CAlnException, eInvalidSeqId, errstr);
        }
    }
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


string& CAlnVec::GetAlnSeqString(string& buffer,
                                 TNumrow row,
                                 const TSignedRange& aln_rng) const
{
    string buff;
    buffer.erase();

    CSeqVector& seq_vec      = x_GetSeqVector(row);
    TSeqPos     seq_vec_size = seq_vec.size();
    
    // get the chunks which are aligned to seq on anchor
    CRef<CAlnMap::CAlnChunkVec> chunk_vec = 
        GetAlnChunks(row, aln_rng, fSkipInserts | fSkipUnalignedGaps);
    
    // for each chunk
    for (int i=0; i<chunk_vec->size(); i++) {
        CConstRef<CAlnMap::CAlnChunk> chunk = (*chunk_vec)[i];
                
        if (chunk->GetType() & fSeq) {
            // add the sequence string
            if (IsPositiveStrand(row)) {
                seq_vec.GetSeqData(chunk->GetRange().GetFrom(),
                                   chunk->GetRange().GetTo() + 1,
                                   buff);
            } else {
                seq_vec.GetSeqData(seq_vec_size - chunk->GetRange().GetTo() - 1,
                                   seq_vec_size - chunk->GetRange().GetFrom(),
                                   buff);
            }
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
            delete[] ch_buff;
        }
    }
    return buffer;
}


string& CAlnVec::GetWholeAlnSeqString(TNumrow       row,
                                      string&       buffer,
                                      TSeqPosList * insert_aln_starts,
                                      TSeqPosList * insert_starts,
                                      TSeqPosList * insert_lens,
                                      unsigned int  scrn_width,
                                      TSeqPosList * scrn_lefts,
                                      TSeqPosList * scrn_rights) const
{
    TSeqPos       aln_pos = 0,
        len = 0,
        curr_pos = 0,
        anchor_pos = 0,
        scrn_pos = 0,
        prev_len = 0,
        ttl_len = 0;
    TSignedSeqPos start = -1,
        stop = -1,
        scrn_lft_seq_pos = -1,
        scrn_rgt_seq_pos = -1,
        prev_aln_pos = -1,
        prev_start = -1;
    TNumseg       seg;
    int           pos, nscrns, delta;
    
    TSeqPos aln_len = GetAlnStop() + 1;

    bool anchored = IsSetAnchor();
    bool plus     = IsPositiveStrand(row);
    int  width    = GetWidth(row);

    scrn_width *= width;

    const bool record_inserts = insert_starts && insert_lens;
    const bool record_coords  = scrn_width && scrn_lefts && scrn_rights;

    // allocate space for the row
    char* c_buff = new char[aln_len + 1];
    char* c_buff_ptr = c_buff;
    string buff;
    
    const TNumseg& left_seg = x_GetSeqLeftSeg(row);
    const TNumseg& right_seg = x_GetSeqRightSeg(row);

    // loop through all segments
    for (seg = 0, pos = row, aln_pos = 0, anchor_pos = m_Anchor;
         seg < m_NumSegs;
         ++seg, pos += m_NumRows, anchor_pos += m_NumRows) {
        
        const TSeqPos& seg_len = m_Lens[seg];

        start = m_Starts[pos];
        len = seg_len * width;

        if (anchored  &&  m_Starts[anchor_pos] < 0) {
            if (start >= 0) {
                // record the insert if requested
                if (record_inserts) {
                    if (prev_aln_pos == (aln_pos / width)  &&
                        start == (plus ? prev_start + prev_len :
                                  prev_start - len)) {
                        // consolidate the adjacent inserts
                        ttl_len += len;
                        insert_lens->pop_back();
                        insert_lens->push_back(ttl_len);
                        if (!plus) {
                            insert_starts->pop_back();
                            insert_starts->push_back(start);
                        }
                    } else {
                        prev_aln_pos = aln_pos / width;
                        ttl_len = len;
                        insert_starts->push_back(start);
                        insert_aln_starts->push_back(prev_aln_pos);
                        insert_lens->push_back(len);
                    }
                    prev_start = start;
                    prev_len = len;
		}
            }
        } else {
            if (start >= 0) {
                stop = start + len - 1;

                // add regular sequence to buffer
                GetSeqString(buff, row, start, stop);
                memcpy(c_buff_ptr, buff.c_str(), seg_len);
                c_buff_ptr += seg_len;
                
                // take care of coords if necessary
                if (record_coords) {
                    if (scrn_lft_seq_pos < 0) {
                        scrn_lft_seq_pos = plus ? start : stop;
                        if (scrn_rgt_seq_pos < 0) {
                            scrn_rgt_seq_pos = scrn_lft_seq_pos;
                        }
                    }
                    // previous scrns
                    nscrns = (aln_pos - scrn_pos) / scrn_width;
                    for (int i = 0; i < nscrns; i++) {
                        scrn_lefts->push_back(scrn_lft_seq_pos);
                        scrn_rights->push_back(scrn_rgt_seq_pos);
                        if (i == 0) {
                            scrn_lft_seq_pos = plus ? start : stop;
                        }
                        scrn_pos += scrn_width;
                    }
                    if (nscrns > 0) {
                        scrn_lft_seq_pos = plus ? start : stop;
                    }
                    // current scrns
                    nscrns = (aln_pos + len - scrn_pos) / scrn_width;
                    curr_pos = aln_pos;
                    for (int i = 0; i < nscrns; i++) {
                        delta = (plus ?
                                 scrn_width - (curr_pos - scrn_pos) :
                                 curr_pos - scrn_pos - scrn_width);
                        
                        scrn_lefts->push_back(scrn_lft_seq_pos);
                        if (plus ?
                            scrn_lft_seq_pos < start :
                            scrn_lft_seq_pos > stop) {
                            scrn_lft_seq_pos = (plus ? start : stop) +
                                delta;
                            scrn_rgt_seq_pos = scrn_lft_seq_pos +
                                (plus ? -1 : 1);
                        } else {
                            scrn_rgt_seq_pos = scrn_lft_seq_pos + (plus ? -1 : 1)
                                + delta;
                            scrn_lft_seq_pos += delta;
                        }
                        if (seg == left_seg  &&
                            scrn_lft_seq_pos == scrn_rgt_seq_pos) {
                            if (plus) {
                                scrn_rgt_seq_pos--;
                            } else {
                                scrn_rgt_seq_pos++;
                            }
                        }
                        scrn_rights->push_back(scrn_rgt_seq_pos);
                        curr_pos = scrn_pos += scrn_width;
                    }
                    if (aln_pos + len <= scrn_pos) {
                        scrn_lft_seq_pos = -1; // reset
                    }
                    scrn_rgt_seq_pos = plus ? stop : start;
                }
            } else {
                // add appropriate number of gap/end chars
                
                char* ch_buff = new char[seg_len + 1];
                char fill_ch;
                
                if (seg < left_seg  ||  seg > right_seg) {
                    fill_ch = GetEndChar();
                } else {
                    fill_ch = GetGapChar(row);
                }
                
                memset(ch_buff, fill_ch, seg_len);
                ch_buff[seg_len] = 0;
                memcpy(c_buff_ptr, ch_buff, seg_len);
                c_buff_ptr += seg_len;
                delete[] ch_buff;
            }
            aln_pos += len;
        }

    }
    
    // take care of the remaining coords if necessary
    if (record_coords) {
        // previous scrns
        TSeqPos pos_diff = aln_pos - scrn_pos;
        if (pos_diff > 0) {
            nscrns = pos_diff / scrn_width;
            if (pos_diff % scrn_width) {
                nscrns++;
            }
            for (int i = 0; i < nscrns; i++) {
                scrn_lefts->push_back(scrn_lft_seq_pos);
                scrn_rights->push_back(scrn_rgt_seq_pos);
                if (i == 0) {
                    scrn_lft_seq_pos = scrn_rgt_seq_pos;
                }
                scrn_pos += scrn_width;
            }
        }
    }
    c_buff[aln_len] = '\0';
    buffer = c_buff;
    delete [] c_buff;
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
CRef<CDense_seg> CAlnVec::CreateConsensus(int& consensus_row) const
{
    if ( !m_DS ) {
        return CRef<CDense_seg>();
    }

    int i;
    int j;

    // temporary storage for our consensus
    vector<string> consens(m_NumSegs);

    // determine what the number of segments required for a gapped consensus
    // segment is.  this must be rounded to be at least 50%.
    int gap_seg_thresh = m_NumRows - m_NumRows / 2;

    for (j = 0;  j < m_NumSegs;  ++j) {
        // evaluate for gap / no gap
        int gap_count = 0;
        for (i = 0;  i < m_NumRows;  ++i) {
            if (m_Starts[ j*m_NumRows + i ] == -1) {
                ++gap_count;
            }
        }

        // check to make sure that this seg is not a consensus
        // gap seg
        if ( gap_count <= gap_seg_thresh ) {
            // we will build a segment with enough bases to match
            consens[j].resize(m_Lens[j]);

            // retrieve all sequences for this segment
            vector<string> segs(m_NumRows);
            for (i = 0;  i < m_NumRows;  ++i) {
                if (m_Starts[ j*m_NumRows + i ] != -1) {
                    TSeqPos start = m_Starts[j*m_NumRows+i];
                    TSeqPos stop  = start + m_Lens[j];

                    if (IsPositiveStrand(i)) {
                        x_GetSeqVector(i).GetSeqData(start, stop, segs[i]);
                    } else {
                        CSeqVector &  seq_vec = x_GetSeqVector(i);
                        TSeqPos size = seq_vec.size();
                        seq_vec.GetSeqData(size - stop, size - start, segs[i]);
                    }
                    for (int c = 0;  c < segs[i].length();  ++c) {
                        segs[i][c] = FromIupac(segs[i][c]);
                    }
                }
            }

            typedef multimap<int, unsigned char, greater<int> > TRevMap;

            // 
            // evaluate for a consensus
            //
            for (i = 0;  i < m_Lens[j];  ++i) {
                // first, we record which bases occur and how often
                // this is computed in NCBI4na notation
                int base_count[4];
                base_count[0] = base_count[1] =
                    base_count[2] = base_count[3] = 0;
                for (int row = 0;  row < m_NumRows;  ++row) {
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
                    ((m_NumRows - gap_count) * 7 + 5) / 10;

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
    new_ds->SetDim(m_NumRows + 1);
    new_ds->SetNumseg(m_NumSegs);
    new_ds->SetLens() = m_Lens;
    new_ds->SetStarts().reserve(m_Starts.size() + m_NumSegs);
    if ( !m_Strands.empty() ) {
        new_ds->SetStrands().reserve(m_Strands.size() +
                                     m_NumSegs);
    }

    for (i = 0;  i < consens.size();  ++i) {
        // copy the old entries
        for (j = 0;  j < m_NumRows;  ++j) {
            int idx = i * m_NumRows + j;
            new_ds->SetStarts().push_back(m_Starts[idx]);
            if ( !m_Strands.empty() ) {
                new_ds->SetStrands().push_back(m_Strands[idx]);
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
        
        if ( !m_Strands.empty() ) {
            new_ds->SetStrands().push_back(eNa_strand_unknown);
        }

        total_bases += consens[i].length();
        data += consens[i];
    }

    // copy our IDs
    for (i = 0;  i < m_Ids.size();  ++i) {
        new_ds->SetIds().push_back(m_Ids[i]);
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
         na = CIUPACna(data);

         // once we've created the bioseq, we need to add it to the
         // scope
         CRef<CSeq_entry> entry(new CSeq_entry());
         entry->SetSeq(*bioseq);
         GetScope().AddTopLevelSeqEntry(*entry);
    }}

    consensus_row = new_ds->GetIds().size() - 1;
    return new_ds;
}


static SNCBIFullScoreMatrix s_FullScoreMatrix;

int CAlnVec::CalculateScore(const string& s1, const string& s2,
                            bool s1_is_prot, bool s2_is_prot)
{
    // check the lengths
    if (s1_is_prot == s2_is_prot  &&  s1.length() != s2.length()) {
        NCBI_THROW(CAlnException, eInvalidRequest,
                   "CAlnVec::CalculateScore(): "
                   "Strings should have equal lenghts.");
    } else if (s1.length() * (s1_is_prot ? 1 : 3) !=
               s1.length() * (s1_is_prot ? 1 : 3)) {
        NCBI_THROW(CAlnException, eInvalidRequest,
                   "CAlnVec::CalculateScore(): "
                   "Strings lengths do not match.");
    }        

    int score = 0;

    const char * res1 = s1.c_str();
    const char * res2 = s2.c_str();
    const char * end1 = res1 + s1.length();
    const char * end2 = res2 + s2.length();
    
    static bool s_FullScoreMatrixInitialized = false;
    if (s1_is_prot  &&  s2_is_prot) {
        if ( !s_FullScoreMatrixInitialized ) {
            s_FullScoreMatrixInitialized = true;
            NCBISM_Unpack(&NCBISM_Blosum62, &s_FullScoreMatrix);
        }
        
        // use BLOSUM62 matrix
        for ( ;  res1 != end1;  res1++, res2++) {
            score += s_FullScoreMatrix.s[*res1][*res2];
        }
    } else if ( !s1_is_prot  &&  !s2_is_prot ) {
        // use match score/mismatch penalty
        for ( ; res1 != end1;  res1++, res2++) {
            if (*res1 == *res2) {
                score += 1;
            } else {
                score -= 3;
            }
        }
    } else {
        string t;
        if (s1_is_prot) {
            TranslateNAToAA(s2, t);
            for ( ;  res1 != end1;  res1++, res2++) {
                score += s_FullScoreMatrix.s[*res1][*res2];
            }
        } else {
            TranslateNAToAA(s1, t);
            for ( ;  res2 != end2;  res1++, res2++) {
                score += s_FullScoreMatrix.s[*res1][*res2];
            }
        }
    }
    return score;
}


void CAlnVec::TranslateNAToAA(const string& na, string& aa)
{
    if (na.size() % 3) {
        NCBI_THROW(CAlnException, eTranslateFailure,
                   "CAlnVec::TranslateNAToAA(): "
                   "NA size expected to be divisible by 3");
    }

    const CTrans_table& tbl = CGen_code_table::GetTransTable(1);

    unsigned int i, j = 0, state = 0;

    aa.resize(na.size() / 3);

    string::const_iterator res = na.begin();
    while (res != na.end()) {
        for (i = 0; i < 3; i++, res++) {
            state = tbl.NextCodonState(state, *res);
        }
        aa[j++] = tbl.GetCodonResidue(state);
    }
}


int CAlnVec::CalculateScore(TNumrow row1, TNumrow row2) const
{
    TNumrow       numrows = m_NumRows;
    TNumrow       index1 = row1, index2 = row2;
    TSignedSeqPos start1, start2;
    string        buff1, buff2;
    bool          isAA1, isAA2;
    int           score = 0;
    TSeqPos       len;
    
    isAA1 = GetBioseqHandle(row1).GetBioseqCore()
        ->GetInst().GetMol() == CSeq_inst::eMol_aa;

    isAA2 = GetBioseqHandle(row2).GetBioseqCore()
        ->GetInst().GetMol() == CSeq_inst::eMol_aa;

    CSeqVector&   seq_vec1 = x_GetSeqVector(row1);
    TSeqPos       size1    = seq_vec1.size();
    CSeqVector &  seq_vec2 = x_GetSeqVector(row2);
    TSeqPos       size2    = seq_vec2.size();

    for (TNumseg seg = 0; seg < m_NumSegs; seg++) {
        start1 = m_Starts[index1];
        start2 = m_Starts[index2];

        if (start1 >=0  &&  start2 >= 0) {
            len = m_Lens[seg];

            if (IsPositiveStrand(row1)) {
                seq_vec1.GetSeqData(start1,
                                    start1 + len,
                                    buff1);
            } else {
                seq_vec1.GetSeqData(size1 - (start1 + len),
                                    size1 - start1,
                                    buff1);
            }
            if (IsPositiveStrand(row2)) {
                seq_vec2.GetSeqData(start2,
                                    start2 + len,
                                    buff2);
            } else {
                seq_vec2.GetSeqData(size2 - (start2 + len),
                                    size2 - start2,
                                    buff2);
            }
            score += CalculateScore(buff1, buff2, isAA1, isAA2);
        }

        index1 += numrows;
        index2 += numrows;
    }
    return score;
}


string& CAlnVec::GetColumnVector(string& buffer,
                                 TSeqPos aln_pos,
                                 TResidueCount * residue_count,
                                 bool gaps_in_count) const
{
    if (aln_pos > GetAlnStop()) {
        aln_pos = GetAlnStop(); // out-of-range adjustment
    }
    TNumseg seg   = GetSeg(aln_pos);
    TSeqPos delta = aln_pos - GetAlnStart(seg);
    TSeqPos len   = GetLen(seg);

    TSignedSeqPos pos;

    for (TNumrow row = 0; row < m_NumRows; row++) {
        pos = GetStart(row, seg);
        if (pos >= 0) {
            // it's a sequence residue

            bool plus = IsPositiveStrand(row);
            if (plus) {
                pos += delta;
            } else {
                pos += len - 1 - delta;
            }
            
            CSeqVector& seq_vec = x_GetSeqVector(row);
            if (GetWidth(row) == 3) {
                string na_buff, aa_buff;
                if (plus) {
                    seq_vec.GetSeqData(pos, pos + 3, na_buff);
                } else {
                    TSeqPos size = seq_vec.size();
                    seq_vec.GetSeqData(size - pos - 3, size - pos, na_buff);
                }
                TranslateNAToAA(na_buff, aa_buff);
                buffer[row] = aa_buff[0];
            } else {
                buffer[row] = seq_vec[plus ? pos : seq_vec.size() - pos - 1];
            }

            if (residue_count) {
                (*residue_count)[FromIupac(buffer[row])]++;
            }

        } else {
            // it's a gap or endchar
            
            if (GetEndChar() != (buffer[row] = GetGapChar(row))) {
                // need to check the where the segment is
                // only if endchar != gap
                // this saves a check if there're the same
                TSegTypeFlags type = GetSegType(row, seg);
                if (type & fNoSeqOnLeft  ||  type & fNoSeqOnRight) {
                    buffer[row] = GetEndChar();
                }
            }

            if (gaps_in_count) {
                (*residue_count)[FromIupac(buffer[row])]++;
            }
        }
    } // for row

    return buffer;
}

int CAlnVec::CalculatePercentIdentity(TSeqPos aln_pos) const
{
    string column;
    column.resize(m_NumRows);

    TResidueCount residue_cnt;
    residue_cnt.resize(16, 0);

    GetColumnVector(column, aln_pos, &residue_cnt);
    
    int max = 0, total = 0;
    ITERATE (TResidueCount, i_res, residue_cnt) {
        if (*i_res > max) {
            max = *i_res;
        }
        total += *i_res;
    }
    return 100 * max / total;
}


END_objects_SCOPE // namespace ncbi::objects::
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.59  2004/05/21 21:42:51  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.58  2004/03/30 16:39:09  todorov
* -redundant statement
*
* Revision 1.57  2003/12/22 19:14:12  todorov
* Only left constructors that accept CScope&
*
* Revision 1.56  2003/12/22 18:30:37  todorov
* ObjMgr is no longer created internally. Scope should be passed as a reference in the ctor
*
* Revision 1.55  2003/12/18 19:47:51  todorov
* + GetColumnVector & CalculatePercentIdentity
*
* Revision 1.54  2003/12/10 17:17:39  todorov
* CalcScore now const
*
* Revision 1.53  2003/10/21 14:41:35  grichenk
* Fixed type convertion
*
* Revision 1.52  2003/09/26 16:58:34  todorov
* Fixed the length of c_buff
*
* Revision 1.51  2003/09/23 21:29:34  todorov
* Rearranged variables & fixed a bug
*
* Revision 1.50  2003/09/23 18:37:24  todorov
* bug fix in GetWholeAlnSeqString
*
* Revision 1.49  2003/09/22 21:00:16  todorov
* Consolidated adjacent inserts in GetWholeAlnSeqString
*
* Revision 1.48  2003/09/22 19:03:30  todorov
* Use the new x_GetSeq{Left,Right}Seg methods
*
* Revision 1.47  2003/09/17 15:48:06  jianye
* Added missing [] when de-allocating c_buff
*
* Revision 1.46  2003/09/17 14:46:39  todorov
* Performance optimization: Use char * instead of string
*
* Revision 1.45  2003/09/12 16:16:50  todorov
* TSeqPos->TSignedSeqPos bug fix
*
* Revision 1.44  2003/08/29 18:18:58  dicuccio
* Changed CreateConsensus() API to return a new dense-seg instead of altering the
* current alignment manager
*
* Revision 1.43  2003/08/27 21:19:55  todorov
* using raw_scoremat.h
*
* Revision 1.42  2003/08/25 16:34:59  todorov
* exposed GetWidth
*
* Revision 1.41  2003/08/20 17:50:52  todorov
* resize + direct string access rather than appending
*
* Revision 1.40  2003/08/20 17:23:54  ucko
* TranslateNAToAA: append to strings with += rather than push_back
* (which MSVC lacks); fix a typo while I'm at it.
*
* Revision 1.39  2003/08/20 14:34:58  todorov
* Support for NA2AA Densegs
*
* Revision 1.38  2003/07/23 20:26:14  todorov
* fixed an unaligned pieces coords problem in GetWhole..
*
* Revision 1.37  2003/07/23 20:24:39  todorov
* +aln_starts for the inserts in GetWhole...
*
* Revision 1.36  2003/07/22 19:18:37  todorov
* fixed a 1st seg check in GetWhole...
*
* Revision 1.35  2003/07/21 21:29:39  todorov
* cleaned an expression
*
* Revision 1.34  2003/07/21 17:08:50  todorov
* fixed calc of remaining nscrns in GetWhole...
*
* Revision 1.33  2003/07/18 22:12:51  todorov
* Fixed an anchor bug in GetWholeAlnSeqString
*
* Revision 1.32  2003/07/17 22:45:56  todorov
* +GetWholeAlnSeqString
*
* Revision 1.31  2003/07/15 21:13:54  todorov
* rm bioseq_handle ref
*
* Revision 1.30  2003/07/15 20:54:01  todorov
* exception type fixed
*
* Revision 1.29  2003/07/15 20:46:09  todorov
* Exception if bioseq handle is null
*
* Revision 1.28  2003/06/05 19:03:12  todorov
* Added const refs to Dense-seg members as a speed optimization
*
* Revision 1.27  2003/06/02 16:06:40  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.26  2003/04/24 16:15:57  vasilche
* Added missing includes and forward class declarations.
*
* Revision 1.25  2003/04/15 14:21:27  vasilche
* Added missing include file.
*
* Revision 1.24  2003/03/29 07:07:31  todorov
* deallocation bug fixed
*
* Revision 1.23  2003/03/05 16:18:17  todorov
* + str len err check
*
* Revision 1.22  2003/02/11 21:32:44  todorov
* fMinGap optional merging algorithm
*
* Revision 1.21  2003/01/29 20:54:37  todorov
* CalculateScore speed optimization
*
* Revision 1.20  2003/01/27 22:48:41  todorov
* Changed CreateConsensus accordingly too
*
* Revision 1.19  2003/01/27 22:30:30  todorov
* Attune to seq_vector interface change
*
* Revision 1.18  2003/01/23 21:31:08  todorov
* Removed the original, inefficient GetXXXString methods
*
* Revision 1.17  2003/01/23 16:31:34  todorov
* Added calc score methods
*
* Revision 1.16  2003/01/17 19:25:04  ucko
* Clear buffer with erase(), as G++ 2.9x lacks string::clear.
*
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
