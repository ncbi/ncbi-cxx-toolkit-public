#ifndef OBJTOOLS_READERS___FASTA_ALN_BUILDER__HPP
#define OBJTOOLS_READERS___FASTA_ALN_BUILDER__HPP

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
 * Authors:  Aaron Ucko
 *
 */

/// @file fasta_aln_builder.hpp
/// Helper class to build pairwise alignments, with double gaps
/// automatically spliced out.

#include <corelib/ncbiobj.hpp>

/** @addtogroup Miscellaneous
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDense_seg;
class CSeq_align;
class CSeq_id;

/// Helper class to build pairwise alignments, with double gaps
/// automatically spliced out.
class CFastaAlignmentBuilder : public CObject
{
public:
    CFastaAlignmentBuilder(CRef<CSeq_id> reference_id, CRef<CSeq_id> other_id);
    // ~CFastaAlignmentBuilder();
    void AddData(TSeqPos alignment_pos, TSignedSeqPos reference_pos,
                 TSignedSeqPos other_pos);
    CRef<CSeq_align> GetCompletedAlignment(void);

    /// special position values
    enum EConstants {
        kNoPos     = -1,
        kContinued = -2
    };

private:
    enum EState {
        eDoubleGap,
        eReferenceOnly,
        eOtherOnly,
        eBoth
    };

    void x_EnsurePos(TSignedSeqPos& pos, TSignedSeqPos last_pos,
                     TSeqPos alignment_pos);
    EState x_State(TSignedSeqPos reference_pos, TSignedSeqPos other_pos);

    CRef<CDense_seg> m_DS;
    TSeqPos          m_LastAlignmentPos;
    TSignedSeqPos    m_LastReferencePos;
    TSignedSeqPos    m_LastOtherPos;
    EState           m_LastState;
    EState           m_LastNonDGState;
};

END_SCOPE(objects)
END_NCBI_SCOPE

/* @} */

#endif  /* OBJTOOLS_READERS___FASTA_ALN_BUILDER__HPP */
