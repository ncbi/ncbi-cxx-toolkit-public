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
#include <algorithm>

using namespace std;

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

void CBadResiduesException::x_ConvertBadIndexesToString(
        CNcbiOstream & out,
        const vector<TSeqPos> &badIndexes, 
        unsigned int maxRanges )
{
    // assert that badIndexes is sorted in ascending order
    _ASSERT(adjacent_find(badIndexes.begin(), badIndexes.end(), 
        std::greater<int>()) == badIndexes.end() );

    typedef pair<TSeqPos, TSeqPos> TRange;
    typedef vector<TRange> TRangeVec;

    TRangeVec rangesFound;

    ITERATE( vector<TSeqPos>, idx_iter, badIndexes ) {
        const TSeqPos idx = *idx_iter;

        // first one
        if( rangesFound.empty() ) {
            rangesFound.push_back(TRange(idx, idx));
            continue;
        }

        const TSeqPos last_idx = rangesFound.back().second;
        if( idx == (last_idx+1) ) {
            // extend previous range
            ++rangesFound.back().second;
        } else {
            // create new range
            rangesFound.push_back(TRange(idx, idx));
        }

        if( rangesFound.size() > maxRanges ) {
            break;
        }
    }

    // turn the ranges found into a string
    const char *prefix = "";
    for( unsigned int rng_idx = 0; 
        ( rng_idx < rangesFound.size() && rng_idx < maxRanges ); 
        ++rng_idx ) 
    {
        out << prefix;
        const TRange &range = rangesFound[rng_idx];
        out << range.first;
        if( range.first != range.second ) {
            out << "-" << range.second;
        }

        prefix = ", ";
    }
    if( rangesFound.size() > maxRanges ) {
        out << ", and more";
    }
}

END_SCOPE(objects)
END_NCBI_SCOPE
