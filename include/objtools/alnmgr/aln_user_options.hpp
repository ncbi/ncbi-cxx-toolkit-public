#ifndef OBJTOOLS_ALNMGR___ALN_USER_OPTIONS__HPP
#define OBJTOOLS_ALNMGR___ALN_USER_OPTIONS__HPP
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
* Authors:  Kamen Todorov, NCBI
*
* File Description:
*   Alignment user options
*
* ===========================================================================
*/


#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <objtools/alnmgr/aln_user_options.hpp>
#include <objtools/alnmgr/pairwise_aln.hpp>


BEGIN_NCBI_SCOPE


/// Options for different alignment manager operations.
class CAlnUserOptions : public CObject
{
public:
    typedef CPairwiseAln::TPos TPos;

    /// Row direction flags.
    /// NOTE: in most cases directions are relative to the anchor row.
    enum EDirection {
        /// No filtering: use both direct and reverse sequences.
        eBothDirections   = 0,

        /// Use only sequences whose strand is the same as that of the anchor.
        eDirect           = 1,

        /// Use only sequences whose strand is opposite to that of the anchor.
        eReverse          = 2,

        /// By default don't filter by direction.
        eDefaultDirection = eBothDirections
    };
    EDirection m_Direction;

    /// Alignment merging algorithm.
    enum EMergeAlgo {
        /// Merge all sequences (greedy algo).
        eMergeAllSeqs      = 0,

        /// Only put the query seq on same row (input order is not significant).
        eQuerySeqMergeOnly = 1,

        /// Preserve all rows as they were in the input (e.g. self-align a
        /// sequence). Greedy algo.
        ePreserveRows      = 2,

        eDefaultMergeAlgo  = eMergeAllSeqs
    };
    EMergeAlgo m_MergeAlgo;

    /// Additional merge flags.
    enum EMergeFlags {
        /// Truncate overlapping ranges. If not set, the overlaps are put
        /// on separate rows.
        fTruncateOverlaps   = 1 << 0,

        /// Allow mixed strands on the same row. Experimental feature,
        /// not supported for all alignment types.
        fAllowMixedStrand   = 1 << 1,

        /// Allow translocations on the same row.
        fAllowTranslocation = 1 << 2,

        /// In greedy algos, skip sorting input alignments by score thus
        /// allowing for user-defined sort order.
        fSkipSortByScore    = 1 << 3,

        /// Use the anchor sequence as the alignment sequence. Otherwise
        /// (by default) a pseudo sequence is created whose coordinates are
        /// the alignment coordinates.
        /// NOTE: Setting this flag is not recommended. Using it will make
        /// all CSparseAln::*AlnPos* methods inconsistent with
        /// CAlnVec::*AlnPos* methods.
        fUseAnchorAsAlnSeq  = 1 << 4,

        /// Store anchor row in the first pairwise alignment (by default it's
        /// stored in the last one).
        fAnchorRowFirst     = 1 << 5,

        /// Do not collect and store insertions (gaps on the anchor).
        fIgnoreInsertions   = 1 << 6
    };
    typedef int TMergeFlags;
    TMergeFlags m_MergeFlags;

    bool m_ClipAlignment;
    objects::CBioseq_Handle m_ClipSeq;
    TPos m_ClipStart;
    TPos m_ClipEnd;

    bool m_ExtendAlignment;
    TPos m_Extension; 

    enum EShowUnalignedOption {
        eHideUnaligned,
        eShowFlankingN, // show N residues on each side
        eShowAllUnaligned
    };

    EShowUnalignedOption m_UnalignedOption;
    TPos m_ShowUnalignedN;

    CAlnUserOptions(void)
    :   m_Direction(eDefaultDirection),
        m_MergeAlgo(eDefaultMergeAlgo),
        m_MergeFlags(0),
        m_ClipAlignment(false),
        m_ClipStart(0), m_ClipEnd(1),
        m_ExtendAlignment(false),
        m_Extension(1),
        m_UnalignedOption(eHideUnaligned),
        m_ShowUnalignedN(10)
    {
    }

    /// Set anchor id.
    void SetAnchorId(const TAlnSeqIdIRef& anchor_id)
    {
        m_AnchorId = anchor_id;
    }

    /// Get anchor id.
    const TAlnSeqIdIRef& GetAnchorId(void) const
    {
        return m_AnchorId;
    }

    /// Set/clear merge flags.
    void SetMergeFlags(TMergeFlags flags, bool set)
    {
        if (set) {
            m_MergeFlags |= flags;
        }
        else {
            m_MergeFlags &= ~flags;
        }
    }

    /// Anchor bioseq - if null then a multiple alignment shall be built.
    objects::CBioseq_Handle m_Anchor;

private:
    TAlnSeqIdIRef m_AnchorId;

//     enum EAddFlags {
//         // Determine score of each aligned segment in the process of mixing
//         // (only makes sense if scope was provided at construction time)
//         fCalcScore            = 0x01,

//         // Force translation of nucleotide rows
//         // This will result in an output Dense-seg that has Widths,
//         // no matter if the whole alignment consists of nucleotides only.
//         fForceTranslation     = 0x02,

//         // Used for mapping sequence to itself
//         fPreserveRows         = 0x04
//     };

//     enum EMergeFlags {
//         fTruncateOverlaps     = 0x0001, // otherwise put on separate rows
//         fNegativeStrand       = 0x0002,
//         fGapJoin              = 0x0004, // join equal len segs gapped on refseq
//         fMinGap               = 0x0008, // minimize segs gapped on refseq
//         fRemoveLeadTrailGaps  = 0x0010, // Remove all leading or trailing gaps
//         fSortSeqsByScore      = 0x0020, // Better scoring seqs go towards the top
//         fSortInputByScore     = 0x0040, // Process better scoring input alignments first
//         fQuerySeqMergeOnly    = 0x0080, // Only put the query seq on same row,
//                                         // other seqs from diff densegs go to diff rows
//         fFillUnalignedRegions = 0x0100,
//         fAllowTranslocation   = 0x0200  // allow translocations when truncating overlaps
//     };

};


END_NCBI_SCOPE

#endif  // OBJTOOLS_ALNMGR___ALN_USER_OPTIONS__HPP
