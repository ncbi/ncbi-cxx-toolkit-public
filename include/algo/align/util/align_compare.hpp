#ifndef GPIPE_COMMON___ALIGN_COMPARE__HPP
#define GPIPE_COMMON___ALIGN_COMPARE__HPP

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
 * Authors:  Eyal Mozes
 *
 * File Description:
 *
 */

#include <objects/seq/seq_id_handle.hpp>

#include <algo/align/util/align_source.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


class NCBI_XALGOALIGN_EXPORT CAlignCompare
{
public:
    /////////////////////////////////////////////////////////////////////////////
    //
    // Retrieve sets of intervals for alignments
    //
    
    typedef map<TSeqRange, TSeqRange> TAlignmentSpans;

    enum EMode { e_Interval, e_Exon, e_Span };

    enum EMatchLevel {e_Equiv, e_Overlap, e_NoMatch};

    //////////////////////////////////////////////////////////////////////////////
    //
    // struct defining all information needed to store a single alignment
    //
    
    struct SAlignment
    {
        int              source_set;

        CSeq_id_Handle   query;
        CSeq_id_Handle   subject;
    
        ENa_strand query_strand;
        ENa_strand subject_strand;
    
        TSeqRange        query_range;
        TSeqRange        subject_range;
    
        CRef<CSeq_align> align;
        TAlignmentSpans  spans;

        EMatchLevel      match_level;

        SAlignment(int s, const CRef<CSeq_align> &al, EMode mode);
    };
    
    CAlignCompare(IAlignSource &set1,
                  IAlignSource &set2,
                  EMode mode = e_Interval,
                  bool strict = false)
    : m_Set1(set1)
    , m_Set2(set2)
    , m_Mode(mode)
    , m_Strict(strict)
    , m_CountSet1(0)
    , m_CountSet2(0)
    , m_CountEquivSet1(0)
    , m_CountEquivSet2(0)
    , m_CountOverlapSet1(0)
    , m_CountOverlapSet2(0)
    , m_CountOnlySet1(0)
    , m_CountOnlySet2(0)
    , m_CountEquivGroups(0)
    , m_CountOverlapGroups(0)
    {
    }

    bool EndOfData() const
    {
        return m_NextSet1Group.empty() && m_NextSet2Group.empty() &&
               m_Set1.EndOfData() && m_Set2.EndOfData();
    }

    vector<const SAlignment *> NextGroup();

    size_t CountSet1() const { return m_CountSet1; }
    size_t CountSet2() const { return m_CountSet2; }
    size_t CountEquivSet1() const { return m_CountEquivSet1; }
    size_t CountEquivSet2() const { return m_CountEquivSet2; }
    size_t CountOverlapSet1() const { return m_CountOverlapSet1; }
    size_t CountOverlapSet2() const { return m_CountOverlapSet2; }
    size_t CountOnlySet1() const { return m_CountOnlySet1; }
    size_t CountOnlySet2() const { return m_CountOnlySet2; }
    size_t CountEquivGroups() const { return m_CountEquivGroups; }
    size_t CountOverlapGroups() const { return m_CountOverlapGroups; }

private:
    IAlignSource &m_Set1;
    IAlignSource &m_Set2;
    EMode m_Mode;
    bool m_Strict;

    size_t m_CountSet1;
    size_t m_CountSet2;
    size_t m_CountEquivSet1;
    size_t m_CountEquivSet2;
    size_t m_CountOverlapSet1;
    size_t m_CountOverlapSet2;
    size_t m_CountOnlySet1;
    size_t m_CountOnlySet2;
    size_t m_CountEquivGroups;
    size_t m_CountOverlapGroups;

    list< AutoPtr<SAlignment> > m_CurrentSet1Group;
    list< AutoPtr<SAlignment> > m_CurrentSet2Group;
    list< AutoPtr<SAlignment> > m_NextSet1Group;
    list< AutoPtr<SAlignment> > m_NextSet2Group;

    /// Determine whether the next group of alignments should be taken from set 1 or 2.
    /// If the next group from both sets are on the same query and subject, return 3;
    /// otherwise return 1 or 2.
    int x_DetermineNextGroupSet();

    /// Get next alignment from the correct set
    AutoPtr<SAlignment> x_NextAlignment(int set)
    {
        ++(set == 1 ? m_CountSet1 : m_CountSet2);
        return new SAlignment(set, (set == 1 ? m_Set1 : m_Set2).GetNext(),
                              m_Mode);
    }

    void x_GetCurrentGroup(int set);
};

END_NCBI_SCOPE


#endif  // GPIPE_COMMON___ALIGN_COMPARE__HPP
