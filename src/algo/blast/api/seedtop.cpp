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

CSeedTop::CSeedTop(const string & pattern,
                   CRef<CLocalDbAdapter> db)
         : m_Pattern(pattern),
           m_SeqSrc(db->MakeSeqSrc()),
           m_SeqInfoSrc(db->MakeSeqInfoSrc())
{}

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

vector< CConstRef <CSeq_loc> > 
CSeedTop::Run()
{
    x_MakeScoreBlk();
    x_MakeLookupTable();
    BlastOffsetPair* offset_pairs = (BlastOffsetPair*)
         calloc(GetOffsetArraySize(m_Lookup), sizeof(BlastOffsetPair));

    CRef<CSeq_id> sid;
    TSeqPos slen;
    vector< CConstRef <CSeq_loc> > retv;
    
    BlastSeqSrcGetSeqArg seq_arg;
    memset((void*) &seq_arg, 0, sizeof(seq_arg));
    seq_arg.encoding = eBlastEncodingProtein;

    BlastSeqSrcIterator* itr = BlastSeqSrcIteratorNewEx
         (MAX(BlastSeqSrcGetNumSeqs(m_SeqSrc)/100, 1));

    while( (seq_arg.oid = BlastSeqSrcIteratorNext(m_SeqSrc, itr))
           != BLAST_SEQSRC_EOF) {
        if (seq_arg.oid == BLAST_SEQSRC_ERROR) break;
        if (BlastSeqSrcGetSequence(m_SeqSrc, &seq_arg) < 0) continue;

        Int4 start_offset = 0;
        GetSequenceLengthAndId(m_SeqInfoSrc, seq_arg.oid, sid, &slen);

        while (start_offset < seq_arg.seq->length) {
            // Query block and array size arguments are not used when scanning 
            // subject for pattern hits, so pass NULL and 0 for respective 
            // arguments.
            Int4 hit_count = 
              PHIBlastScanSubject(m_Lookup, NULL, seq_arg.seq, &start_offset,
                                  offset_pairs, 0);

            if (hit_count == 0) break;

            for (int index = 0; index < hit_count; ++index) {
                CRef<CSeq_loc> hit(new CSeq_loc(*sid, offset_pairs[index].phi_offsets.s_start,
                                                  offset_pairs[index].phi_offsets.s_end));
                retv.push_back(hit);
            }
        }
 
        BlastSeqSrcReleaseSequence(m_SeqSrc, &seq_arg);
    }

    BlastSequenceBlkFree(seq_arg.seq);
    itr = BlastSeqSrcIteratorFree(itr);
    sfree(offset_pairs);
    return retv;
}

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */
