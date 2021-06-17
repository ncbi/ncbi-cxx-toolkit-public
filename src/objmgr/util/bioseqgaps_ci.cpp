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
* Author:  Michael Kornbluh, NCBI
*
* File Description:
*   Iterates over the internal gaps of the given bioseq or seq-entry.
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>

#include <objmgr/util/sequence.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_map.hpp>
#include <objmgr/seq_map.hpp>
#include <objmgr/seq_map_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/bioseq_handle.hpp>

#include <corelib/ncbistd.hpp>

#include <objects/misc/sequence_macros.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CBioseqGaps_CI::CBioseqGaps_CI(
    const CSeq_entry_Handle & entry_h,
    const Params & params )
: m_bioseq_CI(entry_h, params.mol_filter, params.level_filter),
  m_Params(params)
{
    // check for weird corner cases where we can't
    // possibly find any gaps, so we 
    // immediately consider this iterator finished
    if( m_Params.max_num_gaps_per_seq < 1 ||
        m_Params.max_num_seqs < 1) 
    {
        m_bioseq_CI = CBioseq_CI();
    }

    switch( m_Params.mol_filter ) {
    case CSeq_inst::eMol_not_set:
    case CSeq_inst::eMol_na:
    case CSeq_inst::eMol_aa:
        // no problem
        break;
    default:
        NCBI_USER_THROW_FMT( "CBioseqGaps_CI only takes the not_set, na, and aa "
            "for mol_filter.  This value was given: " 
            << static_cast<int>(m_Params.mol_filter) );
        break;
    }

    if( m_bioseq_CI ) {
        x_Next();
    }
}

const CBioseqGaps_CI::SCurrentGapInfo & 
CBioseqGaps_CI::x_GetCurrent(void) const
{
    if( ! *this ) {
        NCBI_USER_THROW("CBioseqGaps_CI is out of range");
    }
    return m_infoOnCurrentGap;
}

void CBioseqGaps_CI::x_Next(void)
{
    if( ! *this ) {
        NCBI_USER_THROW("CBioseqGaps_CI is out of range");
    }

    // this is what we're looking at in the current m_Bioseq_CI
    // we don't adjust m_infoOnCurrentGap until the end.
    TSeqPos pos_to_try;
    if( ! m_infoOnCurrentGap.seq_id ) {
        // this is the first "x_Next()"
        _ASSERT( m_infoOnCurrentGap.length == 0 );
        _ASSERT( m_infoOnCurrentGap.num_seqs_seen_so_far == 0 );
        _ASSERT( m_infoOnCurrentGap.num_gaps_seen_so_far_on_this_seq == 0 );
        _ASSERT( m_infoOnCurrentGap.start_pos == 0 );
        pos_to_try = 0;
    } else {
        // the normal case: skip past the current run on N's
        _ASSERT( m_infoOnCurrentGap.seq_id );
        _ASSERT( m_infoOnCurrentGap.length > 0 );
        // start looking after the current gap
        pos_to_try = m_infoOnCurrentGap.start_pos + m_infoOnCurrentGap.length;

        // if we've reached the limi on number of gaps per seq, 
        // we skip to the next seq
        _ASSERT( m_Params.max_num_gaps_per_seq > 0  );
        if( m_infoOnCurrentGap.num_gaps_seen_so_far_on_this_seq >= 
            m_Params.max_num_gaps_per_seq )
        {
            x_NextBioseq();
            pos_to_try = 0;
        }
    }

    // we iterate through bioseqs until we find the next one
    // with a qualifying gap
    for( ; m_bioseq_CI; x_NextBioseq() ) 
    {
        const CBioseq_Handle & bioseq_to_try_h = *m_bioseq_CI;

        TSeqPos out_pos = kInvalidSeqPos;
        TSeqPos out_len = kInvalidSeqPos;
        while( eFindNext_Found ==
            x_FindNextGapOnBioseq( bioseq_to_try_h, 
                pos_to_try,
                out_pos,
                out_len ))
        {
            if( out_len > m_Params.max_gap_len_to_ignore ) {
                // we found our gap

                // is it on a different bioseq?
                if( bioseq_to_try_h.GetAccessSeq_id_Handle() == 
                    m_infoOnCurrentGap.seq_id )
                {
                    ++m_infoOnCurrentGap.num_gaps_seen_so_far_on_this_seq;
                } else {
                    m_infoOnCurrentGap.seq_id = 
                        bioseq_to_try_h.GetAccessSeq_id_Handle();
                    ++m_infoOnCurrentGap.num_seqs_seen_so_far;
                    m_infoOnCurrentGap.num_gaps_seen_so_far_on_this_seq = 1;
                }

                m_infoOnCurrentGap.start_pos = out_pos;
                m_infoOnCurrentGap.length    = out_len;

                // found the next gap, so return
                return;
            }
            // this gap is small enough to ignore, so
            // continue looking for the next one
            pos_to_try = out_pos + out_len;
        }

        // restart at the beginning for the next bioseq
        pos_to_try = 0;
    }

    // if we reached here, we've reached the end and there
    // are no more gaps
}

void 
CBioseqGaps_CI::x_NextBioseq(void)
{
    ++m_bioseq_CI;
    if( ! m_bioseq_CI ) {
        // we've reached the end
        return;
    }
    if( m_infoOnCurrentGap.num_seqs_seen_so_far >=
        m_Params.max_num_seqs ) 
    {
        m_bioseq_CI = CBioseq_CI();
        return;
    }
}

CBioseqGaps_CI::EFindNext
CBioseqGaps_CI::x_FindNextGapOnBioseq(
    const CBioseq_Handle & bioseq_h, 
    const TSeqPos pos_to_start_looking,
    TSeqPos & out_pos_of_gap,
    TSeqPos & out_len_of_gap ) const
{
    TSeqPos curr_pos = pos_to_start_looking;
    CSeqVector seqvec( bioseq_h, CBioseq_Handle::eCoding_Iupac );
    const CSeqMap & seqmap = seqvec.GetSeqMap();

    const TSeqPos bioseq_len = seqvec.size();
    const CSeqVector::TResidue gap_residue = seqvec.GetGapChar();

    // advance curr_pos until we're at a gap
    // (since we're mostly checking real bases, we don't need
    // to use CSeqMap or anything fancy like that)
    for( ; curr_pos < bioseq_len; ++curr_pos ) {
        const CSeqVector::TResidue residue = seqvec[curr_pos];
        if( residue == gap_residue ) {
            break;
        }
    }
    out_pos_of_gap = curr_pos;

    // we're on a gap (or past the end...), so we advance
    // curr_pos until it's past the end of the gap
    bool bDone = false;
    while( curr_pos < bioseq_len && ! bDone ) {
        CSeqMap_CI segment_ci = 
            seqmap.FindSegment( curr_pos, &bioseq_h.GetScope() );
        const TSeqPos pos_after_end_of_segment = segment_ci.GetEndPosition();
        const CSeqMap::ESegmentType eSegmentType = segment_ci.GetType();
        if( eSegmentType == CSeqMap::eSeqGap ) {
            // for efficiency, skip over the whole gap
            curr_pos = pos_after_end_of_segment;
        } else if( eSegmentType == CSeqMap::eSeqData ) {
            for( ; curr_pos < pos_after_end_of_segment; ++curr_pos) {
                if( seqvec[curr_pos] != gap_residue ) {
                    // break completely out of loop
                    out_len_of_gap = (curr_pos - out_pos_of_gap);
                    bDone = true;
                    break;
                }
            }
        } else {
            NCBI_USER_THROW_FMT( 
                "This segment type is not supported in CBioseqGaps_CI "
                "at this time: " << static_cast<int>(eSegmentType) );
            break;
        }
    }
    out_len_of_gap = (curr_pos - out_pos_of_gap);

    return ( out_len_of_gap > 0 ? eFindNext_Found : eFindNext_NotFound);
}

END_SCOPE(objects)
END_NCBI_SCOPE

