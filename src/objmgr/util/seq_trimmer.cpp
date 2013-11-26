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
*   Trims the end of sequences, based on various criteria.
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>

#include <objmgr/util/sequence.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_map.hpp>
#include <objmgr/annot_ci.hpp>
#include <objmgr/seq_map.hpp>
#include <objmgr/seq_map_ci.hpp>
#include <objmgr/seq_vector.hpp>

#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_autoinit.hpp>
#include <corelib/ncbi_safe_static.hpp>

#include <objects/misc/sequence_macros.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

namespace {
    const static CSeqVector::TResidue kFirstCharInLookupTable = 'A';
    const static CSeqVector::TResidue kLastCharInLookupTable  = 'Z';

    // fill the given array with the given value
    // (just a simple wrapper over std::fill)
    template<typename TType, int Size>
    void s_FillArray(TType(& array)[Size], const TType & value) {
        fill( array, array + Size, value );
    }

    // ambig_lookup_table is a table for each letter of the alphabet.
    // input_table is an array of all the letters that should
    // be set to value.
    template<int TableSize, int InputSize>
    void s_SetAmbigLookupTableFromArray(
            bool (&ambig_lookup_table)[TableSize], 
            const CSeqVector::TResidue(&input_table)[InputSize],
            const bool & value )
    {
        ITERATE_0_IDX(input_idx, InputSize) {
            const CSeqVector::TResidue chInputChar = input_table[input_idx];
            _ASSERT( chInputChar >= kFirstCharInLookupTable &&
                chInputChar <= kLastCharInLookupTable );
            ambig_lookup_table[chInputChar - kFirstCharInLookupTable] = value;
        }
    }

    bool s_IsValidDirection(const TSignedSeqPos iSeqPos)
    {
        return (iSeqPos == 1 || iSeqPos == -1);
    }

    bool s_IsSupportedSegmentType(const CSeqMap_CI & segment )
    {
        switch( segment.GetType() ) {
        case CSeqMap::eSeqGap:
        case CSeqMap::eSeqData:
            return true;
        default:
            return false;
        }
    }

    bool s_IsEmptyRange(
        const TSignedSeqPos iStartPos,
        const TSignedSeqPos iEndPos,
        const TSignedSeqPos iTrimDirection) 
    {
        _ASSERT( s_IsValidDirection(iTrimDirection) );
        if( iTrimDirection < 0 ) {
            return (iStartPos < iEndPos);
        } else {
            return (iStartPos > iEndPos);
        }
    }

    struct PVecTrimRulesLessThan {
        bool operator()( const CSequenceAmbigTrimmer::STrimRule & lhs, 
            const CSequenceAmbigTrimmer::STrimRule & rhs ) const 
        {
            if( lhs.bases_to_check != rhs.bases_to_check ) {
                return lhs.bases_to_check < rhs.bases_to_check;
            }

            return lhs.max_bases_allowed_to_be_ambig < 
                   rhs.max_bases_allowed_to_be_ambig;
        }
    };

    struct PVecTrimRulesHaveSameNumberOfBases {
        bool operator()( const CSequenceAmbigTrimmer::STrimRule & lhs, 
            const CSequenceAmbigTrimmer::STrimRule & rhs ) const
        {
            return lhs.bases_to_check == rhs.bases_to_check;
        }
    };

    struct PVecTrimRuleAlwaysPasses {
        bool operator()( const CSequenceAmbigTrimmer::STrimRule & rule ) {
            return rule.bases_to_check == rule.max_bases_allowed_to_be_ambig;
        }
    };

    CSequenceAmbigTrimmer::TTrimRuleVec *s_DefaultRuleCreator(void)
    {
        auto_ptr<CSequenceAmbigTrimmer::TTrimRuleVec> pTrimRuleVec( new CSequenceAmbigTrimmer::TTrimRuleVec );
        CSequenceAmbigTrimmer::STrimRule arrTrimRules[] = {
            { 10, 5 },
            { 50, 15 }
        };
        ITERATE_0_IDX( rule_idx, ArraySize(arrTrimRules) ) {
            pTrimRuleVec->push_back(arrTrimRules[rule_idx]);
        }
        return pTrimRuleVec.release();
    }
}

// static
const CSequenceAmbigTrimmer::TTrimRuleVec & 
CSequenceAmbigTrimmer::GetDefaultTrimRules(void)
{
    static CSafeStatic<TTrimRuleVec> s_DefaultTrimRules( 
        s_DefaultRuleCreator, NULL );
    return s_DefaultTrimRules.Get();
}

CSequenceAmbigTrimmer::CSequenceAmbigTrimmer(
    EMeaningOfAmbig eMeaningOfAmbig,
    TFlags fFlags,
    const TTrimRuleVec & trimRuleVec,
    TSignedSeqPos uMinSeqLen )
    : m_eMeaningOfAmbig(eMeaningOfAmbig),
      m_fFlags(fFlags),
      m_vecTrimRules(trimRuleVec),
      m_uMinSeqLen(uMinSeqLen)
{
    x_NormalizeVecTrimRules( m_vecTrimRules );

    // set up ambig lookup tables:
    _ASSERT( ArraySize(m_arrNucAmbigLookupTable) == 
        (1 + kLastCharInLookupTable - kFirstCharInLookupTable) );
    _ASSERT( ArraySize(m_arrProtAmbigLookupTable) == 
        (1 + kLastCharInLookupTable - kFirstCharInLookupTable) );

    // most letters are unambiguous for amino acids
    s_FillArray(m_arrProtAmbigLookupTable, false);

    switch( m_eMeaningOfAmbig ) {
    case eMeaningOfAmbig_OnlyCompletelyUnknown:
        // here, only one letter is considered ambig
        s_FillArray(m_arrNucAmbigLookupTable, false);
        m_arrNucAmbigLookupTable['N'- kFirstCharInLookupTable] = true;
        m_arrProtAmbigLookupTable['X' - kFirstCharInLookupTable] = true;
        break;
    case eMeaningOfAmbig_AnyAmbig: {
        // anything not specific is considered ambiguous:

        s_FillArray(m_arrNucAmbigLookupTable, true);
        const static CSeqVector::TResidue arrNucCertainBases[] = {
            'A', 'C', 'G', 'T'};
        s_SetAmbigLookupTableFromArray(
            m_arrNucAmbigLookupTable, arrNucCertainBases, false);

        const static CSeqVector::TResidue arrProtAmbigBases[] = {
            'B', 'J', 'X', 'Z' };
        s_SetAmbigLookupTableFromArray(
            m_arrProtAmbigLookupTable, arrProtAmbigBases, true);
        break;
    }
    default:
        NCBI_USER_THROW_FMT("Unknown EMeaningOfAmbig: "
            << static_cast<int>(m_eMeaningOfAmbig) );
    }
}

CSequenceAmbigTrimmer::EResult 
CSequenceAmbigTrimmer::DoTrim( CBioseq_Handle &bioseq_handle)
{
    _ASSERT( bioseq_handle );

    const CSeqVector seqvec( bioseq_handle, CBioseq_Handle::eCoding_Iupac );

    // there's already no sequence, so nothing to trim
    const TSignedSeqPos bioseq_len = bioseq_handle.GetBioseqLength();
    if( bioseq_len < 1 ) {
        return eResult_NoTrimNeeded;
    }

    TSignedSeqPos leftmost_good_base = 0;
    TSignedSeqPos rightmost_good_base = (bioseq_len - 1);
    if( ! x_TestFlag(fFlags_DoNotTrimBeginning) ) {
        leftmost_good_base = x_FindWhereToTrim( 
            seqvec, leftmost_good_base, rightmost_good_base, 
            1 ); // 1 means "towards the right"
    }
    if( leftmost_good_base > rightmost_good_base ) {
        // trimming leaves nothing left
        return x_TrimToNothing( bioseq_handle );
    }
    
    if( ! x_TestFlag(fFlags_DoNotTrimEnd) ) {
        rightmost_good_base = 
            x_FindWhereToTrim( 
            seqvec, rightmost_good_base, leftmost_good_base, 
            -1 ); // -1 means "towards the left"
    }
    if( leftmost_good_base > rightmost_good_base ) {
        // trimming leaves nothing left
        return x_TrimToNothing( bioseq_handle );
    }

    // check if nothing to do
    if( (leftmost_good_base == 0) &&
        (rightmost_good_base == (bioseq_len - 1)) )
    {
        return eResult_NoTrimNeeded;
    }

    // do the actually slicing of the bioseq
    x_SliceBioseq( 
        leftmost_good_base, rightmost_good_base,
        bioseq_handle );

    return eResult_SuccessfullyTrimmed;
}

void 
CSequenceAmbigTrimmer::x_NormalizeVecTrimRules( 
        TTrimRuleVec & vecTrimRules )
{
    // we want rules that check fewer bases first.
    // then, we sort by number of ambig bases in the rule.
    sort( vecTrimRules.begin(), vecTrimRules.end(),
          PVecTrimRulesLessThan() );

    // For trim rules that represent the same number of bases,
    // we want only the strictest

    /// unique_copy only copies the first when multiple in a row
    /// are equal.
    TTrimRuleVec::iterator new_end_iter = 
        unique( 
            vecTrimRules.begin(), vecTrimRules.end(),
            PVecTrimRulesHaveSameNumberOfBases() );
    vecTrimRules.erase( new_end_iter, vecTrimRules.end() );

    // remove rules that will always pass because they don't
    // do anything.
    new_end_iter = remove_if(
        vecTrimRules.begin(), vecTrimRules.end(),
        PVecTrimRuleAlwaysPasses() );
    vecTrimRules.erase( new_end_iter, vecTrimRules.end() );

    // check if rules have any consistency issues
    CNcbiOstrstream problems_strm;
    ITERATE(TTrimRuleVec, trim_rule_it, vecTrimRules) {
        const STrimRule & trimRule = *trim_rule_it;
        if( trimRule.bases_to_check <= 0 ) {
            problems_strm << "A rule has a non-positive number of "
                "bases to check" << endl;
            continue;
        }
        if( trimRule.bases_to_check <= trimRule.max_bases_allowed_to_be_ambig )
        {
            problems_strm << "There is a rule where bases_to_check "
                << "(" << trimRule.bases_to_check << ") is less than or "
                "equal to max bases allowed (" 
                << trimRule.max_bases_allowed_to_be_ambig << ")" << endl;
            continue;
        }

        // if we're here, this rule is okay
    }

    const string sProblems = CNcbiOstrstreamToString(problems_strm);
    if( ! sProblems.empty()  ) {
         NCBI_USER_THROW_FMT( 
             "Cannot create CSequenceAmbigTrimmer due to issues with rules: " 
             << sProblems );
    }
}

CSequenceAmbigTrimmer::EResult
CSequenceAmbigTrimmer::x_TrimToNothing( CBioseq_Handle &bioseq_handle )
{
    // nothing to do if already empty
    if( bioseq_handle.GetBioseqLength() < 1 ) {
        return eResult_NoTrimNeeded;
    }

    // create new CSeq_inst since we're destroying the whole Bioseq 
    CRef<CSeq_inst> pNewSeqInst( SerialClone(bioseq_handle.GetInst()) );
    
    pNewSeqInst->SetRepr( CSeq_inst::eRepr_virtual );
    pNewSeqInst->SetLength(0);
    pNewSeqInst->ResetSeq_data();
    pNewSeqInst->ResetExt();

    CBioseq_EditHandle bioseq_eh = bioseq_handle.GetEditHandle();
    bioseq_eh.SetInst( *pNewSeqInst );

    return eResult_SuccessfullyTrimmed;
}

TSignedSeqPos 
CSequenceAmbigTrimmer::x_FindWhereToTrim(
    const  CSeqVector & seqvec,
    const TSignedSeqPos iStartPosInclusive_arg,
    const TSignedSeqPos iEndPosInclusive_arg,
    const TSignedSeqPos iTrimDirection )
{
    _ASSERT( s_IsValidDirection(iTrimDirection) );
    if( s_IsEmptyRange(
        iStartPosInclusive_arg, iEndPosInclusive_arg, iTrimDirection) ) 
    {
        return ( iTrimDirection > 0 
            ? numeric_limits<TSignedSeqPos>::max()
            : numeric_limits<TSignedSeqPos>::min() );
    }

    // there is a range in the middle of the bioseq from
    // start to end where we keep the sequence.
    // These are the inclusive bounds of that range:
    TSignedSeqPos uStartOfGoodBasesSoFar = iStartPosInclusive_arg;
    TSignedSeqPos uEndOfGoodBasesSoFar = iEndPosInclusive_arg;

    // if no rules given, there's nothing to initially do
    if( ! m_vecTrimRules.empty() ) {

        // holds the minimum number of bases that will be checked
        // out of all the rules.
        const TSignedSeqPos uFewestBasesCheckedInARule =
            m_vecTrimRules.front().bases_to_check;

        // while sequence hasn't been shrunken too far, we
        // apply the trimming rules we're given
        TSignedSeqPos iNumBasesLeft = 
            1 + abs(uEndOfGoodBasesSoFar - uStartOfGoodBasesSoFar );
        // so we can see if any rule was triggered
        TSignedSeqPos uOldBasesLeft = -1;
        while( iNumBasesLeft >= m_uMinSeqLen ) {
            uOldBasesLeft = iNumBasesLeft;

            // apply rules in order, restarting our rule-checking whenever
            // a rule matches.
            ITERATE(TTrimRuleVec, trim_rule_it, m_vecTrimRules) {
                const STrimRule & trimRule = *trim_rule_it;

                // skip the rule if the sequence is too small
                // (later rules are greater in number of
                // bases_to_check, so no point in even checking them
                // this go-round)
                if( trimRule.bases_to_check > iNumBasesLeft ) {
                    break;
                }

                // get the positions to check just for this rule
                const TSignedSeqPos iEndPosToCheckForThisRule =
                    uStartOfGoodBasesSoFar + 
                    (iTrimDirection * (trimRule.bases_to_check - 1));

                // get ambig count, etc. for the given range
                SAmbigCount ambig_count(iTrimDirection);
                x_CountAmbigInRange( ambig_count,
                    seqvec,
                    uStartOfGoodBasesSoFar,
                    iEndPosToCheckForThisRule,
                    iTrimDirection );

                // would this rule trigger?
                if( ambig_count.num_ambig_bases <= 
                    trimRule.max_bases_allowed_to_be_ambig ) 
                {
                    // rule did not trigger, so go to next rule
                    continue;
                }

                // at this point, the rule has been triggered

                if( s_IsEmptyRange(
                    ambig_count.pos_after_last_gap, 
                    iEndPosToCheckForThisRule, 
                    iTrimDirection) ) 
                {
                    // here, the entire region we checked is
                    // all ambiguous bases.
                    uStartOfGoodBasesSoFar += 
                        (iTrimDirection * trimRule.bases_to_check);

                    // optimization:

                    // are we at a tremendous gap feature?
                    // consider that gap's length instead of
                    // turning it into ambig bases that we slowly iterate over
                    // individually.
                    x_EdgeSeqMapGapAdjust( 
                        seqvec,
                        uStartOfGoodBasesSoFar, // this var will be adjusted
                        uEndOfGoodBasesSoFar,
                        iTrimDirection,
                        uFewestBasesCheckedInARule );
                } else {
                    // this part happens when there is at least one 
                    // non-ambiguous base in the region we checked.
                    uStartOfGoodBasesSoFar = 
                        ambig_count.pos_after_last_gap;
                }

                // when a rule triggers, we start over from the first rule
                break;
            } // end of iterating through the trimRules

            // calculate how many bases are left now
            if( s_IsEmptyRange(uStartOfGoodBasesSoFar, uEndOfGoodBasesSoFar, iTrimDirection) ) {
                iNumBasesLeft = 0;
            } else {
                iNumBasesLeft = 1 + abs(uEndOfGoodBasesSoFar - uStartOfGoodBasesSoFar );
            }
            if( iNumBasesLeft == uOldBasesLeft ) {
                // no rule triggered this iteration, 
                // so break to avoid an infinite loop
                break;
            }
        } // end of iterating while the remaining sequence is big enough
    } // end of "if(there are rules to process)"

    // always perform final edge trimming, regardless of 
    // m_uMinSeqLen.  There should only be a few left, so
    // this can be done with simple base iteration.
    x_EdgeSeqMapGapAdjust( 
        seqvec,
        uStartOfGoodBasesSoFar,
        uEndOfGoodBasesSoFar,
        iTrimDirection,
        1  // "1" means "no chunking"
        );

    return uStartOfGoodBasesSoFar;
}

void CSequenceAmbigTrimmer::x_EdgeSeqMapGapAdjust( 
    const CSeqVector & seqvec,
    TSignedSeqPos & in_out_uStartOfGoodBasesSoFar,
    const TSignedSeqPos uEndOfGoodBasesSoFar,
    const TSignedSeqPos iTrimDirection,
    const TSignedSeqPos uChunkSize )
{
    // check if we've already removed the whole thing 
    if( s_IsEmptyRange(
        in_out_uStartOfGoodBasesSoFar, uEndOfGoodBasesSoFar, iTrimDirection) )
    {
        return;
    }

    const TAmbigLookupTable * const pAmbigLookupTable = 
        ( seqvec.IsNucleotide() ? & m_arrNucAmbigLookupTable :
        seqvec.IsProtein() ? & m_arrProtAmbigLookupTable :
        NULL );
    if( ! pAmbigLookupTable ) {
        NCBI_USER_THROW(
            "Unable to determine molecule type of sequence");
    }

    TSignedSeqPos newStartOfGoodBases = in_out_uStartOfGoodBasesSoFar;
    while( ! s_IsEmptyRange(newStartOfGoodBases, uEndOfGoodBasesSoFar, iTrimDirection) &&
        (*pAmbigLookupTable)[ seqvec[newStartOfGoodBases] - kFirstCharInLookupTable] )
    {
        // find the end of this sequence of gaps
        CSeqMap_CI gap_seqmap_ci =
            seqvec.GetSeqMap().FindSegment(
            newStartOfGoodBases, &seqvec.GetScope() );
        if( gap_seqmap_ci.GetType() == CSeqMap::eSeqData ) {
            const TSignedSeqPos end_of_segment = 
                x_SegmentGetEndInclusive(gap_seqmap_ci, iTrimDirection);

            while( ! s_IsEmptyRange(newStartOfGoodBases, end_of_segment, iTrimDirection) &&
                ! s_IsEmptyRange(newStartOfGoodBases, uEndOfGoodBasesSoFar, iTrimDirection) &&
                (*pAmbigLookupTable)[ seqvec[newStartOfGoodBases] - kFirstCharInLookupTable] )
            {
                newStartOfGoodBases += iTrimDirection;
            }
        } else if( gap_seqmap_ci.GetType() == CSeqMap::eSeqGap ) {
            if(m_fFlags & fFlags_DoNotTrimSeqGap) {
                // do not trim past Seq-gaps here
                break;
            }
            newStartOfGoodBases = iTrimDirection + x_SegmentGetEndInclusive(gap_seqmap_ci, iTrimDirection);
        } else {
            // this is not a gap segment
            // (Note that this does NOT check for sequence data with ambiguous
            // bases in it).
            return;
        }
    }

    // if endOfGapPos is past uEndOfGoodBasesSoFar, then
    // stop it there
    TSignedSeqPos iNumBasesToRemove = 0;
    if( s_IsEmptyRange(newStartOfGoodBases, uEndOfGoodBasesSoFar, iTrimDirection) ) 
    {
        // we're removing all bases
        iNumBasesToRemove = 1 + abs(uEndOfGoodBasesSoFar - in_out_uStartOfGoodBasesSoFar);
    } else {
        iNumBasesToRemove = abs(newStartOfGoodBases - in_out_uStartOfGoodBasesSoFar);
    }

    // chunking:
    // iNumBasesToRemove must be a multiple of uChunkSize
    iNumBasesToRemove = (iNumBasesToRemove / uChunkSize) * uChunkSize;

    // adjust our output variable
    in_out_uStartOfGoodBasesSoFar += (iTrimDirection * iNumBasesToRemove);
}

void CSequenceAmbigTrimmer::x_CountAmbigInRange(
    SAmbigCount & out_result,
    const CSeqVector & seqvec,
    const TSignedSeqPos iStartPosInclusive_arg,
    const TSignedSeqPos iEndPosInclusive_arg,
    TSignedSeqPos iTrimDirection )
{
    if( s_IsEmptyRange(
        iStartPosInclusive_arg, iEndPosInclusive_arg, iTrimDirection) ) 
    {
        out_result = SAmbigCount(iTrimDirection);
        return;
    }

    const CSeqMap & seqmap = seqvec.GetSeqMap();
    CScope * const pScope = & seqvec.GetScope();

    CSeqMap_CI segment_ci = 
        seqmap.FindSegment(
            iStartPosInclusive_arg, pScope );

    const TAmbigLookupTable * const pAmbigLookupTable = 
        ( seqvec.IsNucleotide() ? & m_arrNucAmbigLookupTable :
          seqvec.IsProtein() ? & m_arrProtAmbigLookupTable :
          NULL );
    if( NULL == pAmbigLookupTable ) {
        NCBI_USER_THROW_FMT("Unexpected seqvector mol: " 
            << static_cast<int>(seqvec.GetSequenceType()) );
    }

    for( ; segment_ci && 
        ! s_IsEmptyRange( 
          x_SegmentGetBeginningInclusive(segment_ci, iTrimDirection),
          iEndPosInclusive_arg, iTrimDirection);
        x_SeqMapIterDoNext(segment_ci, iTrimDirection) )
    {
        // get type of segment at current_pos
        const CSeqMap::ESegmentType eSegmentType = segment_ci.GetType();

        // get the part of this segment that we're actually considering
        const TSignedSeqPos segmentStartPosInclusive = 
            x_SegmentGetBeginningInclusive(segment_ci, iTrimDirection);
        const TSignedSeqPos segmentEndPosInclusive = 
            x_SegmentGetEndInclusive(segment_ci, iTrimDirection);

        switch( eSegmentType ) {
        case CSeqMap::eSeqGap: {
            // the "min" is to catch the case of the gap segment going past
            // the end of the range we're looking at
            const TSignedSeqPos numBasesInGapInRange = 
                min(
                    1 + abs(segmentEndPosInclusive - segmentStartPosInclusive),
                    1 + abs(segmentStartPosInclusive - iEndPosInclusive_arg) );
            if(m_fFlags & fFlags_DoNotTrimSeqGap) {
                // if fFlags_DoNotTrimSeqGap is set, then we return 0 ambig
                // bases to make sure no rule is triggered
                out_result = SAmbigCount(iTrimDirection);
                return;
            }
            // gaps are always ambiguous no matter what our definition
            // of ambiguous is.
            out_result.num_ambig_bases += numBasesInGapInRange;
            out_result.pos_after_last_gap = ( (iTrimDirection > 0) 
                ? numeric_limits<TSignedSeqPos >::max()
                : numeric_limits<TSignedSeqPos >::min() );
            break;
        }
        case CSeqMap::eSeqData: {
            // count ambig in this chunk
            for( TSignedSeqPos pos = segmentStartPosInclusive;
                ! s_IsEmptyRange(pos, segmentEndPosInclusive, iTrimDirection) &&
                ! s_IsEmptyRange(pos, iEndPosInclusive_arg, iTrimDirection)
                ;
                pos += iTrimDirection) 
            {
                const CSeqVector::TResidue residue = seqvec[pos];
                if( residue < kFirstCharInLookupTable || residue > kLastCharInLookupTable ||
                    (*pAmbigLookupTable)[residue - kFirstCharInLookupTable]) 
                {
                    ++out_result.num_ambig_bases;
                    out_result.pos_after_last_gap = ( (iTrimDirection > 0) 
                        ? numeric_limits<TSignedSeqPos >::max()
                        : numeric_limits<TSignedSeqPos >::min() );
                } else if( s_IsEmptyRange(
                    out_result.pos_after_last_gap, iEndPosInclusive_arg, 
                    iTrimDirection) ) 
                {
                    out_result.pos_after_last_gap = pos;
                }
            }
            break;
        }
        default:
            NCBI_USER_THROW_FMT( "CSeqMap segments of type "
                << static_cast<int>(eSegmentType) 
                << " are not supported at this time");
        }
    }
}

/// This returns the (inclusive) position at the end of the
/// segment currently at iStartPosInclusive_arg.  As always,
/// the definition of "end" depends on iTrimDirection.
TSignedSeqPos CSequenceAmbigTrimmer::x_SegmentGetEndInclusive(
    const CSeqMap_CI & segment,
    const TSignedSeqPos iTrimDirection )
{
    _ASSERT( s_IsSupportedSegmentType(segment) );
    _ASSERT( s_IsValidDirection(iTrimDirection) );
    if( iTrimDirection == 1 ) {
        // the "-1" turns an exclusive end-position
        // into an inclusive one.
        return (segment.GetEndPosition() - 1);
    } else {
        _ASSERT( iTrimDirection == -1 );
        return segment.GetPosition();
    }
}


CSeqMap_CI & 
CSequenceAmbigTrimmer::x_SeqMapIterDoNext(
    CSeqMap_CI & in_out_segment_it,
    const TSignedSeqPos iTrimDirection )
{
    _ASSERT( s_IsValidDirection(iTrimDirection) );
    return ( iTrimDirection == 1 ? ++in_out_segment_it : --in_out_segment_it );
}

void
CSequenceAmbigTrimmer::x_SliceBioseq( 
    TSignedSeqPos leftmost_good_base, 
    TSignedSeqPos rightmost_good_base,
    CBioseq_Handle & bioseq_handle )
{
    CSeqVector seqvec( bioseq_handle );

    CAutoInitRef<CDelta_ext> pDeltaExt;

    const CSeqMap & seqmap = bioseq_handle.GetSeqMap();
    CSeqMap_CI seqmap_ci = seqmap.ResolvedRangeIterator( 
        &bioseq_handle.GetScope(),
        leftmost_good_base,
        1 + ( rightmost_good_base - leftmost_good_base ) );
    for( ; seqmap_ci; ++seqmap_ci ) {
        CSeqMap::ESegmentType eType = seqmap_ci.GetType();
        switch( eType ) {
        case CSeqMap::eSeqGap: {
            const TSeqPos uGapLength = seqmap_ci.GetLength();
            const bool bIsLengthKnown = ! seqmap_ci.IsUnknownLength();
            CConstRef<CSeq_literal> pOriginalGapSeqLiteral =
                seqmap_ci.GetRefGapLiteral();

            CAutoInitRef<CDelta_seq> pDeltaSeq;

            CAutoInitRef<CSeq_literal> pNewGapLiteral;
            if( pOriginalGapSeqLiteral ) {
                pNewGapLiteral->Assign(*pOriginalGapSeqLiteral);
            }
            if( ! bIsLengthKnown ) {
                pNewGapLiteral->SetFuzz().SetLim( CInt_fuzz::eLim_unk );
            }
            pNewGapLiteral->SetLength( uGapLength );

            pDeltaSeq->SetLiteral( *pNewGapLiteral );

            pDeltaExt->Set().push_back( Ref(&*pDeltaSeq) );
            break;
        }
        case CSeqMap::eSeqData: {
            string new_data;
            seqvec.GetPackedSeqData( 
                new_data, seqmap_ci.GetPosition(), 
                seqmap_ci.GetEndPosition() );

            CRef<CSeq_data> pSeqData( 
                new CSeq_data( new_data, seqvec.GetCoding() ) );

            CAutoInitRef<CDelta_seq> pDeltaSeq;
            pDeltaSeq->SetLiteral().SetLength( seqmap_ci.GetLength() );
            pDeltaSeq->SetLiteral().SetSeq_data( *pSeqData );
            
            pDeltaExt->Set().push_back( Ref(&*pDeltaSeq) );
            break;
        }
        default:
            NCBI_USER_THROW_FMT("CSequenceAmbigTrimmer does not yet support "
                "seqmap segments of type " << static_cast<int>(eType) );
            break;
        }
    }

    // use the new pDeltaExt

    // (we use const_case to defeat the type system to avoid the expense of
    // copying the Seq-inst.  We must be careful, though.
    CSeq_inst & seq_inst = const_cast<CSeq_inst &>(bioseq_handle.GetInst());
    seq_inst.ResetExt();
    seq_inst.ResetSeq_data();
    seq_inst.SetLength( 1 + ( rightmost_good_base - leftmost_good_base ) );
    if( pDeltaExt->Set().empty() ) {
        seq_inst.SetRepr( CSeq_inst::eRepr_virtual );
    } else if( pDeltaExt->Set().size() == 1 ) {
        CRef<CDelta_seq> pDeltaSeq = *pDeltaExt->Set().begin();
        CSeq_data & seq_data = pDeltaSeq->SetLiteral().SetSeq_data();
        seq_inst.SetSeq_data( seq_data );
    } else {
        seq_inst.SetExt().SetDelta( *pDeltaExt );
    }
    CBioseq_EditHandle bioseq_eh = bioseq_handle.GetEditHandle();
    bioseq_eh.SetInst( seq_inst );

    // at this time, annots aren't sliced, but that may be supported in the
    // future.
}

END_SCOPE(objects)
END_NCBI_SCOPE
