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
 * Authors:  Ning Ma
 *
 */

/// @file seedtop.cpp
/// Implements the CSeedTop class.

#include <ncbi_pch.hpp>

#include <algo/blast/api/seedtop.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/blast/api/seqsrc_multiseq.hpp>
#include <algo/blast/api/seqinfosrc_seqvec.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/blast_seqinfosrc_aux.hpp>
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/phi_lookup.h>
#include "blast_setup.hpp"

/** @addtogroup AlgoBlast
 *
 * @{
 */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

CSeedTop::CSeedTop(const string & pattern)
         : m_Pattern(pattern)
{
    x_ParsePattern();
    x_MakeScoreBlk();
    x_MakeLookupTable();
}

void CSeedTop::x_ParsePattern()
{
    vector <string> units;
    NStr::TruncateSpacesInPlace(m_Pattern);
    NStr::Tokenize(m_Pattern, "-", units);
    ITERATE(vector<string>, unit, units){
        if (*unit != "") {
            m_Units.push_back(SPatternUnit(*unit));
        }
    }
}

void CSeedTop::x_MakeLookupTable()
{
    CLookupTableOptions lookup_options;
    LookupTableOptionsNew(m_Program, &lookup_options);
    lookup_options->phi_pattern = strdup(m_Pattern.c_str());
    // Lookup segments, scoreblk, and rps info arguments are irrelevant 
    // and passed as NULL.
    LookupTableWrapInit(NULL, lookup_options, NULL, NULL, 
                        m_ScoreBlk, &m_Lookup, NULL, NULL);
}

void CSeedTop::x_MakeScoreBlk()
{
    CBlastScoringOptions score_options;
    BlastScoringOptionsNew(m_Program, &score_options);
    CBlast_Message msg;
    CBlastQueryInfo query_info(BlastQueryInfoNew(m_Program, 1));
    BlastSetup_ScoreBlkInit(NULL, query_info, score_options, m_Program,
                            &m_ScoreBlk, 1.0, &msg, &BlastFindMatrixPath);
}

CSeedTop::TSeedTopResults CSeedTop::Run(CRef<CLocalDbAdapter> db)
{
    BlastOffsetPair* offset_pairs = (BlastOffsetPair*)
         calloc(GetOffsetArraySize(m_Lookup), sizeof(BlastOffsetPair));

    CRef<CSeq_id> sid;
    TSeqPos slen;
    TSeedTopResults retv;
    
    BlastSeqSrcGetSeqArg seq_arg;
    memset((void*) &seq_arg, 0, sizeof(seq_arg));
    seq_arg.encoding = eBlastEncodingProtein;

    BlastSeqSrc *seq_src = db->MakeSeqSrc();
    IBlastSeqInfoSrc *seq_info_src = db->MakeSeqInfoSrc();
    BlastSeqSrcIterator* itr = BlastSeqSrcIteratorNewEx
         (MAX(BlastSeqSrcGetNumSeqs(seq_src)/100, 1));

    while( (seq_arg.oid = BlastSeqSrcIteratorNext(seq_src, itr))
           != BLAST_SEQSRC_EOF) {
        if (seq_arg.oid == BLAST_SEQSRC_ERROR) break;
        if (BlastSeqSrcGetSequence(seq_src, &seq_arg) < 0) continue;

        Int4 start_offset = 0;
        GetSequenceLengthAndId(seq_info_src, seq_arg.oid, sid, &slen);

        while (start_offset < seq_arg.seq->length) {
            // Query block and array size arguments are not used when scanning 
            // subject for pattern hits, so pass NULL and 0 for respective 
            // arguments.
            Int4 hit_count = 
              PHIBlastScanSubject(m_Lookup, NULL, seq_arg.seq, &start_offset,
                                  offset_pairs, 0);

            if (hit_count == 0) break;

            for (int index = 0; index < hit_count; ++index) {
                vector<vector<int> > pos_list;
                vector<int> pos(m_Units.size());
                int start = offset_pairs[index].phi_offsets.s_start;
                int end = offset_pairs[index].phi_offsets.s_end + 1;
                x_GetPatternRanges(pos, 0, seq_arg.seq->sequence + start, end-start, pos_list);
                ITERATE(vector<vector<int> >, it_pos, pos_list) {
                    CSeq_loc::TRanges ranges;
                    int r_start(start);
                    int r_end(r_start);
                    int uid(0);
                    ITERATE(vector<int>, q, *it_pos) {
                        if (m_Units[uid].is_x) {
                            ranges.push_back(CRange<TSeqPos>(r_start, r_end-1));
                            r_start = r_end + *q;
                            r_end = r_start;
                        } else {
                            r_end += (*q);
                        }
                        ++uid;
                    }
                    ranges.push_back(CRange<TSeqPos>(r_start, r_end-1));
                    CRef<CSeq_loc> hit(new CSeq_loc(*sid, ranges));
                    retv.push_back(hit);
                } 
                // skip the next pos_list.size()-1 hits
                _ASSERT(index + pos_list.size() - 1 < hit_count);
                for (int i = 1; i< pos_list.size(); ++i) {
                    _ASSERT(offset_pairs[index + i].phi_offsets.s_start == start);
                    _ASSERT(offset_pairs[index + i].phi_offsets.s_end + 1 == end);
                }
                index += pos_list.size() - 1;
            }
        }
 
        BlastSeqSrcReleaseSequence(seq_src, &seq_arg);
    }

    BlastSequenceBlkFree(seq_arg.seq);
    itr = BlastSeqSrcIteratorFree(itr);
    sfree(offset_pairs);
    return retv;
}

CSeedTop::TSeedTopResults CSeedTop::Run(CBioseq_Handle & bhl)
{
    CConstRef<CSeq_id> sid = bhl.GetSeqId();
    CSeq_loc sl;
    sl.SetWhole();
    sl.SetId(*sid);
    SSeqLoc subject(sl, bhl.GetScope());
    TSeqLocVector subjects;
    subjects.push_back(subject);
    CRef<IQueryFactory> qf(new CObjMgr_QueryFactory(subjects));
    CRef<CBlastOptionsHandle> opt_handle 
                 (CBlastOptionsFactory::Create(eBlastp));
    CRef<CLocalDbAdapter> db(new CLocalDbAdapter(qf, opt_handle));
    return Run(db);
}

void
CSeedTop::x_GetPatternRanges(vector<int> &pos, Int4 off, Uint1 *seq, Int4 len, 
                             vector<vector<int> > &ranges)
{
    // Not enough sequence letters
    if (len + off + m_Units[off].at_least < m_Units.size() + 1) return;
    // at least test
    int rep;
    for (rep =0; rep < m_Units[off].at_least; ++rep) {
        if (!m_Units[off].test(NCBISTDAA_TO_AMINOACID[seq[rep]])) return;
    }
    // at most test
    while(off < m_Units.size() - 1) {
        pos[off] = rep;
        x_GetPatternRanges(pos, off+1, seq+rep, len-rep, ranges);
        ++rep;
        if (rep >= m_Units[off].at_most) return;
        if (len + off + 1 < m_Units.size() + rep) return;
        if (!m_Units[off].test(NCBISTDAA_TO_AMINOACID[seq[rep]])) return;
    }
    // the last unit of the pattern
    if (m_Units[off].at_most <= len) return;
    for (; rep < len; ++rep) {
        if (!m_Units[off].test(NCBISTDAA_TO_AMINOACID[seq[rep]])) return;
    }   
    pos[off] = rep;
    ranges.push_back(pos);
    return;
}

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */
