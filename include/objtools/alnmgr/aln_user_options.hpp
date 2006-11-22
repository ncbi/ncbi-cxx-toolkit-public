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


BEGIN_NCBI_SCOPE


class CAlnUserOptions : public CObject
{
public:

    enum EMergeOptions {
        eMergeAllSeqs      = 0, ///< Merge all sequences
        eQuerySeqMergeOnly = 1, ///< Only put the query seq on same row, 
        ePreserveRows      = 2, ///< Preserve all rows as they were in the input (e.g. self-align a sequence)
        eDefault           = eMergeAllSeqs
    };
    typedef int TMergeOption;
    TMergeOption m_MergeOption;

    enum EMergeFlags {
        fTruncateOverlaps = 0x0001, ///< Otherwise put on separate rows
        fAllowMixedStrand = 0x0002, ///< Allow mixed strand on the same row
    };
    typedef int TMergeFlags;
    TMergeFlags m_MergeFlags;


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


/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2006/11/22 00:46:16  todorov
* Fixed the flags and options.
*
* Revision 1.1  2006/11/17 05:34:35  todorov
* Initial revision.
*
* ===========================================================================
*/

#endif  // OBJTOOLS_ALNMGR___ALN_USER_OPTIONS__HPP
