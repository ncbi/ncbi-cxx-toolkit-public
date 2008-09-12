#ifndef ALGO_SEQ___ALIGN_CLEANUP__HPP
#define ALGO_SEQ___ALIGN_CLEANUP__HPP

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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <corelib/ncbistd.hpp>

#include <objmgr/scope.hpp>
#include <objects/seqalign/Seq_align.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/// class CAlignCleanup implements an alignment cleanup utility based on
/// the C++ alignment manager.  The primary goal is to remove redundancies
/// from any given set of alignment, and generate a cleaned up version of
/// these alignments
class NCBI_XALGOSEQ_EXPORT CAlignCleanup
{
public:
    CAlignCleanup(CScope& scope);

    typedef list< CConstRef<CSeq_align> > TConstAligns;
    typedef list< CRef<CSeq_align> >      TAligns;

    enum EMode {
        //< use the older (i.e., CAlnVec) alignment manager
        eAlignVec,

        //< use the newer (i.e., CAnchoredAln) alignment manager
        eAnchoredAlign,

        eDefault = eAnchoredAlign
    };
    void Cleanup(const TConstAligns& aligns_in,
                 TAligns&            aligns_out,
                 EMode               mode = eDefault);

    /// flags
    /// these primarity affect the CAlnVec implementation

    /// Sort input alignments by score before evaluating
    void SortInputsByScore(bool b)      { m_SortByScore = b; }

    /// Permit off-diagonal high-scoring items (particularly ones on
    /// the opposite strand)
    void AllowTranslocations(bool b)    { m_AllowTransloc = b; }

    /// Assume that the alignments contains alignments of a sequence to itself
    void PreserveRows(bool b)    { m_PreserveRows = b; }

    /// Fill any unaligned regions with explicit gaps
    void FillUnaligned(bool b)    { m_FillUnaligned = b; }

    static void CreatePairwiseFromMultiple(const CSeq_align& multiple,
                                           TAligns&          pairwise);

private:
    CRef<CScope> m_Scope;
    bool m_SortByScore;
    bool m_AllowTransloc;
    bool m_PreserveRows;
    bool m_FillUnaligned;

    void x_Cleanup_AlignVec(const TConstAligns& aligns_in,
                            TAligns&            aligns_out);
    void x_Cleanup_AnchoredAln(const TConstAligns& aligns_in,
                               TAligns&            aligns_out);
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // ALGO_SEQ___ALIGN_CLEANUP__HPP
