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

#ifndef VALIDATOR___VALIDERROR_BIOSEQSET__HPP
#define VALIDATOR___VALIDERROR_BIOSEQSET__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_autoinit.hpp>

#include <objmgr/scope.hpp>

#include <objtools/validator/validerror_imp.hpp>
#include <objtools/validator/validerror_base.hpp>
#include <objtools/validator/validerror_feat.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;

BEGIN_SCOPE(validator)

class CValidError_base;
class CValidError_annot;

// =============================================================================
//                            Caching classes
// =============================================================================

// for convenience
typedef CValidator::CCache CCache;


// =============================================================================
//                            Validation classes
// =============================================================================
class CValidError_desc;
class CValidError_descr;
class CValidError_bioseq;


// ===========================  Validate Bioseq_set  ===========================


class CValidError_bioseqset : private CValidError_base
{
public:
    CValidError_bioseqset(CValidError_imp& imp);
    ~CValidError_bioseqset() override;

    void ValidateBioseqSet(const CBioseq_set& seqset);

private:

    void ValidateNucProtSet(const CBioseq_set& seqset, int nuccnt, int protcnt, int segcnt);
    void ValidateSegSet(const CBioseq_set& seqset, int segcnt);
    void ValidatePartsSet(const CBioseq_set& seqset);
    void ValidateGenbankSet(const CBioseq_set& seqset);
    void ValidateSetTitle(const CBioseq_set& seqset);
    void ValidateSetElements(const CBioseq_set& seqset);
    void ValidatePopSet(const CBioseq_set& seqset);
    void ValidatePhyMutEcoWgsSet(const CBioseq_set& seqset);
    void ValidateGenProdSet(const CBioseq_set& seqset);
    void CheckForInconsistentBiomols(const CBioseq_set& seqset);
    void SetShouldNotHaveMolInfo(const CBioseq_set& seqset);
    void CheckForImproperlyNestedSets(const CBioseq_set& seqset);
    void ShouldHaveNoDblink(const CBioseq_set& seqset);

    bool IsMrnaProductInGPS(const CBioseq& seq);
    bool IsCDSProductInGPS(const CBioseq& seq, const CBioseq_set& gps);

    //internal validators
    CValidError_annot m_AnnotValidator;
    CValidError_descr m_DescrValidator;
    CValidError_bioseq m_BioseqValidator;
};





END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* VALIDATOR___VALIDERROR_BIOSEQSET__HPP */
