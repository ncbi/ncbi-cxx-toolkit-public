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
 *`
 * Author:  Jonathan Kans, Clifford Clausen, Aaron Ucko......
 *
 * File Description:
 *   Privae classes and definition for the validator
 *   .......
 *
 */

#ifndef VALIDATOR___VALIDERROR_ANNOT__HPP
#define VALIDATOR___VALIDERROR_ANNOT__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_autoinit.hpp>

#include <objmgr/scope.hpp>

#include <objtools/validator/validator.hpp>
#include <objtools/validator/validerror_imp.hpp>
#include <objtools/validator/validerror_base.hpp>
#include <objtools/validator/validerror_feat.hpp>
#include <objtools/validator/validerror_align.hpp>
#include <objtools/validator/validerror_graph.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_annot;
class CBioseq;
class CSeq_feat;
class CSeq_loc;

BEGIN_SCOPE(validator)

// ============================  Validate SeqAnnot  ============================


class CValidError_annot : private CValidError_base
{
public:
    CValidError_annot(CValidError_imp& imp);
    virtual ~CValidError_annot(void);

    void ValidateSeqAnnot(const CSeq_annot_Handle& annot);

    void ValidateSeqAnnot(const CSeq_annot& annot);
    void ValidateSeqAnnotContext(const CSeq_annot& annot, const CBioseq& seq);
    void ValidateSeqAnnotContext(const CSeq_annot& annot, const CBioseq_set& set);
    bool IsLocationUnindexed(const CSeq_loc& loc);
    void ReportLocationGI0(const CSeq_feat& f, const string& label);

private:
    CValidError_graph m_GraphValidator;
    CValidError_align m_AlignValidator;
    CValidError_feat m_FeatValidator;

};


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* VALIDATOR___VALIDERROR_ANNOT__HPP */
