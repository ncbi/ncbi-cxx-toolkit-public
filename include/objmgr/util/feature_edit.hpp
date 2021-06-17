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
 *  and reliability of the software and data,  the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties,  express or implied,  including
 *  warranties of performance,  merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author: Justin Foley, NCBI
 *
 * File Description:
 *  Feature trimming code
 */

#ifndef _FEATURE_EDIT_HPP_
#define _FEATURE_EDIT_HPP_

#include <corelib/ncbistd.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/Code_break.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(sequence)

//  ----------------------------------------------------------------------------
class NCBI_XOBJEDIT_EXPORT CFeatTrim 
//  ----------------------------------------------------------------------------
{
public:
    static CRef<CSeq_feat> Apply(
        const CSeq_feat& feat,
        const CRange<TSeqPos>& range);

    static CRef<CSeq_loc> Apply(
        const CSeq_loc& loc,
        const CRange<TSeqPos>& range);

    static CRef<CCode_break> Apply(
        const CCode_break& code_break,
        const CRange<TSeqPos>& range);

    static CRef<CTrna_ext> Apply(
        const CTrna_ext& trna_ext,
        const CRange<TSeqPos>& range);

    static CCdregion::EFrame GetCdsFrame(
            const CSeq_feat& cds_feature,
            const CRange<TSeqPos>& range);


private:
    static void x_TrimLocation(TSeqPos from, TSeqPos to, 
        bool set_partial,
        CRef<CSeq_loc>& loc);
    static TSeqPos x_GetStartOffset(const CSeq_feat& feat,
        TSeqPos from, TSeqPos to);
    static TSeqPos x_GetFrame(const CCdregion& cdregion);
    static CCdregion::EFrame x_GetNewFrame(TSeqPos offset, const CCdregion& region);

    static void x_UpdateFrame(TSeqPos offset, CCdregion& cdregion);
    static void x_TrimTrnaExt(TSeqPos from, TSeqPos to, CTrna_ext& ext);
    static void x_TrimCodeBreak(TSeqPos from, TSeqPos to, CCode_break& cod_break);
};

END_SCOPE(sequence)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif

