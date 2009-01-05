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
 * Author:  Kevin Bealer
 *
 */

/// @file subj_ranges_set.cpp
/// Defines classes to maintain lists of subject offset ranges in sequence
/// data for targetted retrieval during the traceback stage.

#include <ncbi_pch.hpp>
#include <algo/blast/api/subj_ranges_set.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

// The code attempts to fetch sequence data that is near 'used'
// areas, and to join segments that are near each other.  Each
// sub-range of a sequence will be expanded on each side by at
// least CSubjectRangesSet::m_ExpandHSP letters (or to the beginning or end of
// the sequence). If more than one offset range is specified for a subject
// OID, adjacent ranges will be merged if the distance
// between them is less than CSubjectRangesSet::m_MinGap.

void CSubjectRanges::AddRange(int query_oid,
                              int begin,
                              int end,
                              int min_gap)
{
    m_QueryOIDs.insert(query_oid);
    
    bool done = false;
    
    // Loop until done - in the process we absorb any competing
    // elements, and then insert the combined range.  There is a
    // special case where the new element fits into an existing one
    // where we can just exit.
    
    pair<int,int> range(begin, end);
    pair<int,int> range2(end+1, end+2); // first 'uninteresting' range.
    
    while(! done) {
        TRangeList::iterator lhs = m_Offsets.lower_bound(range);
        TRangeList::iterator rhs = m_Offsets.upper_bound(range2);
        
        // Before starting, we need to 'back up' the start range
        // iterator in case it overlaps the range we want.  If this is
        // not done, a range with an earlier start than us, but
        // overlapping data, could be missed.
        
        if (lhs != m_Offsets.begin()) {
            -- lhs;
        }
        
        done = true;
        
        // if true, need to redo the lhs/rhs
        bool recompute = false;
        
        while((! recompute) && (lhs != rhs)) {
            if (lhs->first  <= (end   + min_gap) &&
                lhs->second >= (begin - min_gap)) {
                
                if (lhs->first <= begin && lhs->second >= end) {
                    // special case: nothing to do.
                    return;
                }
                
                // Absorb this range into begin/end, and remove the
                // element that we are absorbing; which means lhs/rhs
                // should be recomputed.
                
                x_Absorb(*lhs, range);
                m_Offsets.erase(lhs);
                
                begin = range.first;
                end = range.second;
                
                recompute = true;
                done = false;
            } else {
                // Ranges do not match, try the next one.
                ++ lhs;
            }
        }
    }
    
    // Add the range.
    
    m_Offsets.insert(range);
}

void CSubjectRangesSet::AddRange(int q_oid, int s_oid, int begin, int end)
{
    CRef<CSubjectRanges> & R = m_SubjRanges[s_oid];
    
    if (R.Empty()) {
        R.Reset(new CSubjectRanges);
    }
    
    if (m_ExpandHSP) {
        x_ExpandHspRange(begin, end);
    }
    
    R->AddRange(q_oid, begin, end, m_MinGap);
}

void CSubjectRangesSet::RemoveSubject(int s_oid)
{
    m_SubjRanges.erase(s_oid);
}

void CSubjectRangesSet::x_ExpandHspRange(int & begin, int & end)
{
    // Expand by at least min_exp (letters).  It may be a good idea to
    // expand each area by a factor of the total length as well, but
    // the total length of an area is not known until all merging is
    // done; each individual HSP is potentially part of one or more
    // alignments that each include any number and combination of
    // HSPs; this will not be known until after the traceback is
    // completed.
    
    begin = (begin > m_ExpandHSP) ? (begin - m_ExpandHSP) : 0;
    
    // end must be adjusted to a max of subject length at data
    // fetch time.
    
    end += m_ExpandHSP;
}

void CSubjectRangesSet::ApplyRanges(CSeqDBExpert & seqdb) const
{
    static const bool kKeepExistingRanges = true;
    ITERATE(TSubjOid2RangesMap, subj, m_SubjRanges) {
        int subject_oid = subj->first;
        const CSubjectRanges & subj_list = *(subj->second);
        bool cache_data = subj_list.IsUsedByMultipleQueries();
        seqdb.SetOffsetRanges(subject_oid,
                              subj_list.GetRanges(),
                              kKeepExistingRanges,
                              cache_data);
    }
}

END_SCOPE(blast)
END_NCBI_SCOPE
