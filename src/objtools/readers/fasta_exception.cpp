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
* Authors:  Michael Kornbluh, NCBI
*
* File Description:
*   Exceptions for CFastaReader.
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>

#include <corelib/ncbistr.hpp>
#include <objtools/readers/fasta_exception.hpp>
#include <corelib/ncbistre.hpp>
#include <functional>
#include <algorithm>
#include <objects/seqloc/Seq_id.hpp>

using namespace std;

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

void CBadResiduesException::ReportExtra(ostream& out) const
{
    if( empty() ) {
        out << "No Bad Residues";
        return;
    }

    out << "Bad Residues = ";
    if( m_BadResiduePositions.m_SeqId ) {
        out << m_BadResiduePositions.m_SeqId->GetSeqIdString(true);
    } else {
        out << "Seq-id ::= NULL";
    }
    out << ", positions: ";
    m_BadResiduePositions.ConvertBadIndexesToString( out );
}

void CBadResiduesException::SBadResiduePositions::AddBadIndexMap(const TBadIndexMap & additionalBadIndexMap)
{
    ITERATE(SBadResiduePositions::TBadIndexMap, new_line_iter, additionalBadIndexMap) {
        const int lineNum = new_line_iter->first;
        const vector<TSeqPos> & src_seqpos_vec = new_line_iter->second;

        if( src_seqpos_vec.empty() ) {
            continue;
        }

        vector<TSeqPos> & dest_seqpos_vec =
            m_BadIndexMap[lineNum];
        copy( src_seqpos_vec.begin(), src_seqpos_vec.end(),
              back_inserter(dest_seqpos_vec) );
    }
}

void CBadResiduesException::SBadResiduePositions::ConvertBadIndexesToString(
        CNcbiOstream & out,
        unsigned int maxRanges ) const
{
    const char *line_prefix = "";
    unsigned int iRangesFound = 0;
    ITERATE( SBadResiduePositions::TBadIndexMap, index_map_iter, m_BadIndexMap ) {
        const int lineNum = index_map_iter->first;
        const vector<TSeqPos> & badIndexesOnLine = index_map_iter->second;

        // assert that badIndexes is sorted in ascending order on every line
        _ASSERT(adjacent_find(badIndexesOnLine.begin(), badIndexesOnLine.end(),
            std::greater<int>()) == badIndexesOnLine.end() );

        typedef pair<TSeqPos, TSeqPos> TRange;
        typedef vector<TRange> TRangeVec;

        TRangeVec rangesFound;

        ITERATE( vector<TSeqPos>, idx_iter, badIndexesOnLine ) {
            const TSeqPos idx = *idx_iter;

            // first one
            if( rangesFound.empty() ) {
                rangesFound.push_back(TRange(idx, idx));
                ++iRangesFound;
                continue;
            }

            const TSeqPos last_idx = rangesFound.back().second;
            if( idx == (last_idx+1) ) {
                // extend previous range
                ++rangesFound.back().second;
                continue;
            }

            if( iRangesFound >= maxRanges ) {
                break;
            }

            // create new range
            rangesFound.push_back(TRange(idx, idx));
            ++iRangesFound;
        }

        // turn the ranges found on this line into a string
        out << line_prefix << "On line " << lineNum << ": ";
        line_prefix = ", ";

        const char *pos_prefix = "";
        for( unsigned int rng_idx = 0;
            ( rng_idx < rangesFound.size() );
            ++rng_idx )
        {
            out << pos_prefix;
            const TRange &range = rangesFound[rng_idx];
            out << (range.first + 1); // "+1" because 1-based for user
            if( range.first != range.second ) {
                out << "-" << (range.second + 1); // "+1" because 1-based for user
            }

            pos_prefix = ", ";
        }
        if (iRangesFound > maxRanges) {
            out << ", and more";
            return;
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
