#ifndef ALGO_BLAST_API___SUBJ_RANGES_SET__HPP
#define ALGO_BLAST_API___SUBJ_RANGES_SET__HPP

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

/// @file subj_ranges_set.hpp
/// Declares classes to maintain lists of subject offset ranges in sequence
/// data for targetted retrieval during the traceback stage.

#include <algo/blast/api/seqsrc_seqdb.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Set of ranges of subject sequences to fetch during the traceback stage.
class NCBI_XBLAST_EXPORT CSubjectRanges : public CObject {
public:
    /// Convenience typedef
    typedef CSeqDB::TRangeList TRangeList;

    /// Constructor
    CSubjectRanges() {}
    
    /// Add and merge range.
    ///
    /// The new range is added to the existing set; any overlapping
    /// ranges are merged.
    ///
    /// @param query_oid OID of the query sequence.
    /// @param begin Starting offset of the sequence.
    /// @param begin Ending offset of the sequence.
    /// @param min_gap Minimum gap between ranges to avoid merge.
    void AddRange(int query_oid, int begin, int end, int min_gap);
    
    /// Returns true if the ranges associated with this sequence are aligned to
    /// multiple query sequences. This is needed to determine whether CSeqDB
    /// should cache the fetched subject regions or not.
    bool IsUsedByMultipleQueries() const { return m_QueryOIDs.size() > 1; }
    
    /// Returns the set of ranges accumulated thus far.
    const TRangeList & GetRanges() const { return m_Offsets; }
    
private:
    /// Absorb an existing range into the range we intend to add.
    ///
    /// @param lhs Absorbed region
    /// @param dest New region to be added.
    void x_Absorb(const pair<int, int> & src,
                  pair<int, int>       & dest) const
    {
        // begin moves left
        if (dest.first > src.first) {
            dest.first = src.first;
        }
        // end moves right
        if (dest.second < src.second) {
            dest.second = src.second;
        }
    }
    
    /// Query OIDs that have matches to this subject.
    set<int> m_QueryOIDs;
    
    /// Set of offsets for this subject OID.
    TRangeList m_Offsets;
};

/// Set of ranges of subject sequence offsets to fetch during the traceback
/// stage. This is applicable only to nucleotide sequences and improves
/// performance during the traceback stage when dealing with large subject
/// sequences.
class NCBI_XBLAST_EXPORT CSubjectRangesSet : public CObject {
public:
    /// Default number of letters to expand for each HSP after all merging has
    /// taken place.
    static const int kHspExpandSize = 1024;

    /// Default minimum gap size to avoid merge
    static const int kMinGap = 1024;
    
    /// Construct a set of sequence ranges.
    ///
    /// Create a new sequence map set, specifying that expand_hsp
    /// letters should be fetched on either side of every
    /// region of a database sequence. The min_gap value is
    /// applied to the results of extension, and joins any
    /// ranges where the intervening gap is smaller than the
    /// specified value.
    ///
    /// These numbers have not (yet) been 'tuned' to find an
    /// optimal value for performance; it is expected that few
    /// searches will hit the 'fence' at the current default values.
    ///
    /// It may be unsafe to specify either of these values at
    /// very small numbers (less than 10 or so).
    ///
    /// @param expand_hsp Expand each range by at least this much.
    /// @param min_gap Join ranges if gap is smaller than this.
    CSubjectRangesSet(int expand_hsp = kHspExpandSize, int min_gap = kMinGap)
        : m_ExpandHSP(expand_hsp), m_MinGap(min_gap)
    {}

    /// Add new offset range for subject HSP.
    /// @param q_oid Query OID or index.
    /// @param s_oid Subject OID.
    /// @param begin Start offset in subject HSP.
    /// @param begin End offset in subject HSP.
    void AddRange(int q_oid, int s_oid, int begin, int end);
    
    /// Remove a given subject OID from the set.
    ///
    /// Any ranges for the given subject oid are cleared, causing the
    /// entire offset range to be included.  The normal use of this
    /// method is to remove ranges specified for query sequences that
    /// are also found in the subject database.  If the OID is not
    /// found, there is no effect.
    ///
    /// @param s_oid Subject OID.
    void RemoveSubject(int s_oid);
    
    /// Apply existing ranges to a database.
    /// @param seqdb SeqDB object to modify.
    void ApplyRanges(CSeqDB& db) const;
    
private:
    /// Prevent copy constructor.
    CSubjectRangesSet(const CSubjectRangesSet & other);
    /// Prevent assignment operator.
    CSubjectRangesSet& operator=(const CSubjectRangesSet & rhs);
    
    /// Add, expand, and merge new range.
    void x_ExpandHspRange(int & begin, int & end);
    
    /// Translate subject offsets from protein to DNA coordinates.
    /// @param begin Starting offset.
    /// @param end Ending offset of desired sequence range.
    /// @param length Total length of sequence.
    /// @param negative Specify true for negative frames.
    void x_TranslateOffsets(int & begin, int & end);
    
    /// List of OIDs and sequence ranges.
    typedef map<int, CRef<CSubjectRanges> > TSubjOid2RangesMap;
    
    /// Set of query ids and ranges for an OID.
    TSubjOid2RangesMap m_SubjRanges;
    
    /// Expansion amount for each HSP range.
    int m_ExpandHSP;
    
    /// Minimum gap between sequences to avoid merging.
    int m_MinGap;
};

END_SCOPE(blast)
END_NCBI_SCOPE

#endif /* ALGO_BLAST_API___SUBJ_RANGES_SET__HPP */
