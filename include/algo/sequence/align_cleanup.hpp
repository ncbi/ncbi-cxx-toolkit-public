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

///
/// Utility function to split an alignment into pairwise alignments
/// NB: this should go elsewhere
///

class NCBI_XALGOSEQ_EXPORT CAlignCleanup
{
public:
    CAlignCleanup(CScope& scope);

    typedef list< CConstRef<CSeq_align> > TConstAligns;
    typedef list< CRef<CSeq_align> >      TAligns;

    enum EMode {
        eAlignVec,
        eAnchoredAlign,

        eDefault = eAlignVec
    };
    void Cleanup(const TConstAligns& aligns_in,
                 TAligns&            aligns_out,
                 EMode               mode = eDefault);

    static void CreatePairwiseFromMultiple(const CSeq_align& multiple,
                                           TAligns&          pairwise);

private:
    CRef<CScope> m_Scope;

    void x_Cleanup_AlignVec(const TConstAligns& aligns_in,
                            TAligns&            aligns_out);
    void x_Cleanup_AnchoredAln(const TConstAligns& aligns_in,
                               TAligns&            aligns_out);
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // ALGO_SEQ___ALIGN_CLEANUP__HPP
