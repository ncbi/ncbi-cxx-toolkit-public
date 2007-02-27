#ifndef ALGO_ALIGN_UTIL___SCORE_BUILDER__HPP
#define ALGO_ALIGN_UTIL___SCORE_BUILDER__HPP

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

#include <corelib/ncbiobj.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CScope;
class CSeq_align;


class NCBI_XALGOALIGN_EXPORT CScoreBuilder
{
public:

    enum EScoreType {
        //< typical blast 'score'
        eScore_Blast,

        //< blast 'bit_score' score
        eScore_Blast_BitScore,

        //< blast 'e_value' score
        eScore_Blast_EValue,

        //< count of ungapped identities as 'num_ident'
        eScore_IdentityCount,

        //< percent identity as 'pct_identity', range 0.0-100.0
        //< this will also create 'num_ident'
        eScore_PercentIdentity
    };

    /// @name Functions to add scores directly to Seq-aligns
    /// @{

    void AddScore(CScope& scope, CSeq_align& align,
                  EScoreType score);
    void AddScore(CScope& scope, list< CRef<CSeq_align> >& aligns,
                  EScoreType score);

    /// @}

    /// @name Functions to compute scores without adding
    /// @{

    int GetIdentityCount  (CScope& scope, const CSeq_align& align);
    int GetMismatchCount  (CScope& scope, const CSeq_align& align);
    int GetGapCount       (const CSeq_align& align);
    TSeqPos GetAlignLength(const CSeq_align& align);

    /// @}

};



END_SCOPE(objects)
END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2006/07/24 14:00:19  ucko
 * Include ncbiobj.hpp, not just ncbistd.hpp, for CRef<>.
 *
 * Revision 1.1  2006/07/24 13:16:45  dicuccio
 * Added CScoreBuilder, a general class to provide support for generating scores
 * for alignments
 *
 * ===========================================================================
 */

#endif  // ALGO_ALIGN_UTIL___SCORE_BUILDER__HPP
