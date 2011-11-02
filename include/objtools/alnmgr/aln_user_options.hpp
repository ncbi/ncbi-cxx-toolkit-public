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


class CAlnUserOptions : public CObject
{
public:
    typedef CPairwiseAln::TPos TPos;

    enum EDirection {
        eBothDirections   = 0, ///< No filtering: use both direct and
                               ///  reverse sequences.

        eDirect           = 1, ///< Use only sequences whose strand is
                               ///  the same as that of the anchor

        eReverse          = 2,  ///< Use only sequences whose strand
                                ///  is opposite to that of the anchor

        eDefaultDirection = eBothDirections
    };
    EDirection m_Direction;

    enum EMergeAlgo {
        eMergeAllSeqs      = 0, ///< Merge all sequences [greedy algo]

        eQuerySeqMergeOnly = 1, ///< Only put the query seq on same
                                ///  row [input order is not
                                ///  significant]

        ePreserveRows      = 2, ///< Preserve all rows as they were in
                                ///  the input (e.g. self-align a
                                ///  sequence) (coresponds to separate
                                ///  alignments) [greedy algo]

        eDefaultMergeAlgo  = eMergeAllSeqs
    };
    EMergeAlgo m_MergeAlgo;

    enum EMergeFlags {
        fTruncateOverlaps   = 1 << 0, ///< Otherwise put on separate
                                      ///  rows

        fAllowMixedStrand   = 1 << 1, ///< Allow mixed strand on the
                                      ///  same row.  An experimental
                                      ///  feature for advance users.
                                      ///  Not supported for all
                                      ///  alignment types.

        fAllowTranslocation = 1 << 2, ///< Allow translocations on the
                                      ///  same row

        fSkipSortByScore    = 1 << 3, ///< In greedy algos, skip
                                      ///  sorting input alignments by
                                      ///  score thus allowing for
                                      ///  user-defined sort order.

        fUseAnchorAsAlnSeq  = 1 << 4, ///< (Not recommended!) Use the
                                      ///  anchor sequence as the
                                      ///  alignment sequence.
                                      ///  Otherwise (the default) a
                                      ///  pseudo sequence is created
                                      ///  whose coordinates are the
                                      ///  alignment coordinates.
                                      ///  WARNING: This will make all
                                      ///  CSparseAln::*AlnPos*
                                      ///  methods incosistent with
                                      ///  CAlnVec::*AlnPos*.

        fAnchorRowFirst     = 1 << 5  ///< Anchor row is stored in the first
                                      ///  pairwise alignment, not the last
                                      ///  one.

    };
    typedef int TMergeFlags;
    TMergeFlags  m_MergeFlags;

    enum EShowUnalignedOption {
        eHideUnaligned,
        eShowFlankingN, // show N residues on each side
        eShowAllUnaligned
    };

    bool    m_ClipAlignment;
    objects::CBioseq_Handle  m_ClipSeq;
    TPos m_ClipStart;
    TPos m_ClipEnd;

    bool m_ExtendAlignment;
    TPos m_Extension; 

    EShowUnalignedOption  m_UnalignedOption;
    TPos m_ShowUnalignedN;

    CAlnUserOptions()
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

    void SetAnchorId(const TAlnSeqIdIRef& anchor_id) {
        m_AnchorId = anchor_id;
    }


    const TAlnSeqIdIRef& GetAnchorId() const {
        return m_AnchorId;
    }


    void    SetMergeFlags(TMergeFlags flags, bool set)  {
        if(set) {
            m_MergeFlags |= flags;
        } else {
            m_MergeFlags &= ~flags;
        }
    }


    objects::CBioseq_Handle  m_Anchor; // if null then a multiple alignment shall be built    
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
