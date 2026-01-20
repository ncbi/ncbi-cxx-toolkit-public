#ifndef GBQUAL_CLEANUP__HPP
#define GBQUAL_CLEANUP__HPP

/*
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
 * Author:
 *
 * File Description:
 *   Basic and Extended Cleanup of CSeq_entries.
 *
 * ===========================================================================
 */

#include <stack>

#include <objtools/cleanup/cleanup_change.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CNewCleanup_imp;
class CScope;
class CGGb_qual;
class CSeq_feat;
class CGene_ref;
class CProt_ref;
class CCdregion;
class CCRNA_ref;
class CTrna_ext;

class NCBI_CLEANUP_EXPORT CGBQualCleanup
{
public:
    enum EAction {
        eAction_Nothing = 1,
        eAction_Erase
    };

    enum EGBQualOpt {
        eGBQualOpt_normal,
        eGBQualOpt_CDSMode
    };


    CGBQualCleanup(CScope& scope, CNewCleanup_imp& parent);

    virtual ~CGBQualCleanup() {}

    EAction Run(CGb_qual& gbq, CSeq_feat& seqfeat);

private:
    EAction x_GeneGBQualBC(CGene_ref& gene, const CGb_qual& gb_qual);
    EAction x_ProtGBQualBC(CProt_ref& prot, const CGb_qual& gb_qual, EGBQualOpt opt);
    EAction x_SeqFeatCDSGBQualBC(CSeq_feat& feat, CCdregion& cds, const CGb_qual& gb_qual);
    EAction x_HandleTrnaProductGBQual(CSeq_feat& feat, CRNA_ref& rna, const string& product);
    EAction x_SeqFeatRnaGBQualBC(CSeq_feat& feat, CRNA_ref& rna, CGb_qual& gb_qual);

    void x_ChangeMade(CCleanupChange::EChanges e);
    void x_SeqFeatTRNABC(CTrna_ext& tRNA);
    void x_AddToComment(CSeq_feat& feat, const string& comment);
    void x_CleanupECNumber(string& ec_num);


    void x_MendSatelliteQualifier(string& val);
    void x_CleanupAndRepairInference(string& inference);


    CNewCleanup_imp& m_Parent;
    CRef<CScope>     m_Scope;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif /* GBQUAL_CLEANUP__HPP */
