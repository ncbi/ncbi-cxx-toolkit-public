#ifndef OBJTOOLS_FORMAT___SAM_FORMATTER__HPP
#define OBJTOOLS_FORMAT___SAM_FORMATTER__HPP

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
 * Authors:  Aaron Ucko, Aleksey Grichenko
 *
 */

/// @file sam_formatter.hpp
/// Flat formatter for Sequence Alignment/Map (SAM).


#include <objtools/format/item_formatter.hpp>


/** @addtogroup Miscellaneous
 *
 * @{
 */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class NCBI_FORMAT_EXPORT CSAM_Formatter
{
public:
    enum EFlags {
        fSAM_AlignmentScore     = 1 << 0,  ///< Generate AS tags
        fSAM_ExpectationValue   = 1 << 1,  ///< Generate EV tags
        fSAM_NumNucDiff         = 1 << 2,  ///< Generate NM tags
        fSAM_PercentageIdentity = 1 << 3,  ///< Generate PI tags
        fSAM_BitScore           = 1 << 4,  ///< Generate BS tags

        //? Include all tags by default
        fSAM_Default            = fSAM_AlignmentScore     |
                                  fSAM_ExpectationValue   |
                                  fSAM_NumNucDiff         |
                                  fSAM_PercentageIdentity |
                                  fSAM_BitScore
    };
    typedef int TFlags;  ///< bitwise OR of EFlags

    CSAM_Formatter(CNcbiOstream& out,
                   CScope& scope,
                   TFlags flags = fSAM_Default);
    ~CSAM_Formatter(void) {}

    void SetOutputStream(CNcbiOstream& out) { m_Out = &out; }
    void SetScope(CScope& scope) { m_Scope.Reset(&scope); }
    void SetFlags (TFlags flags) { m_Flags = flags; }
    void SetFlag  (EFlags flag)  { m_Flags |= flag; }
    void UnsetFlag(EFlags flag)  { m_Flags &= ~flag; }

    CSAM_Formatter& Print(const CSeq_align&     aln,
                          const CSeq_id&        query_id);
    CSAM_Formatter& Print(const CSeq_align&     aln,
                          CSeq_align::TDim      query_row);
    CSAM_Formatter& Print(const CSeq_align_set& aln,
                          const CSeq_id&        query_id);
    CSAM_Formatter& Print(const CSeq_align_set& aln,
                          CSeq_align::TDim      query_row);

private:
    CNcbiOstream* m_Out;
    CRef<CScope>  m_Scope;
    TFlags        m_Flags;
};


END_SCOPE(objects)
END_NCBI_SCOPE


/* @} */

#endif  /* OBJTOOLS_FORMAT___SAM_FORMATTER__HPP */
