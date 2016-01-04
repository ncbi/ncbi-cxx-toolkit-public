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
 * Authors:  Colleen Bollin
 */


#ifndef _LOC_EDIT_H_
#define _LOC_EDIT_H_

#include <corelib/ncbistd.hpp>

#include <objmgr/scope.hpp>

#include <objmgr/scope.hpp>
#include <objects/seqfeat/Seq_feat.hpp>


BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

NCBI_XOBJEDIT_EXPORT string PrintBestSeqId(const CSeq_id& sid, CScope& scope);
NCBI_XOBJEDIT_EXPORT string PrintSeqIntUseBestID(const CSeq_interval& seq_int, CScope& scope, bool range_only);
NCBI_XOBJEDIT_EXPORT string PrintPntAndPntsUseBestID(const CSeq_loc& seq_loc, CScope& scope, bool range_only = false);
NCBI_XOBJEDIT_EXPORT string SeqLocPrintUseBestID(const CSeq_loc& seq_loc, CScope& scope, bool range_only = false);

class NCBI_XOBJEDIT_EXPORT CLocationEditPolicy : public CObject
{
public:
    enum EPartialPolicy {
        ePartialPolicy_eNoChange = 0,
        ePartialPolicy_eSet,
        ePartialPolicy_eSetAtEnd,
        ePartialPolicy_eSetForBadEnd,
        ePartialPolicy_eSetForFrame,
        ePartialPolicy_eClear,
        ePartialPolicy_eClearNotAtEnd,
        ePartialPolicy_eClearForGoodEnd };

    enum EMergePolicy {
        eMergePolicy_NoChange = 0,
        eMergePolicy_Join,
        eMergePolicy_Order,
        eMergePolicy_SingleInterval };

    CLocationEditPolicy(EPartialPolicy partial5 = ePartialPolicy_eNoChange, 
                    EPartialPolicy partial3 = ePartialPolicy_eNoChange, 
                    bool extend_5 = true,
                    bool extend_3 = true,
                    EMergePolicy merge = eMergePolicy_NoChange
                    )
                    : m_PartialPolicy5(partial5),
                      m_PartialPolicy3(partial3),
                      m_Extend5(extend_5),
                      m_Extend3(extend_3),
                      m_MergePolicy(merge)
        {};

    ~CLocationEditPolicy() {};

    EPartialPolicy GetPartial5Policy() { return m_PartialPolicy5; };
    EPartialPolicy GetPartial3Policy() { return m_PartialPolicy3; };
    void SetPartial5Policy(EPartialPolicy policy) { m_PartialPolicy5 = policy; };
    void SetPartial3Policy(EPartialPolicy policy) { m_PartialPolicy3 = policy; };
    EMergePolicy GetMergePolicy() { return m_MergePolicy; };
    void SetMergePolicy(EMergePolicy policy) { m_MergePolicy = policy; };
    bool GetExtend5() { return m_Extend5; };
    bool GetExtend3() { return m_Extend3; };
    void SetExtend5(bool policy) { m_Extend5 = policy; };
    void SetExtend3(bool policy) { m_Extend3 = policy; };

    bool ApplyPolicyToFeature(CSeq_feat& feat, CScope& scope) const;
    bool Interpret5Policy(const CSeq_feat& orig_feat, CScope& scope, bool& do_set_5_partial, bool& do_clear_5_partial) const;
    bool Interpret3Policy(const CSeq_feat& orig_feat, CScope& scope, bool& do_set_3_partial, bool& do_clear_3_partial) const;
    static CRef<CSeq_loc> ConvertToJoin(const CSeq_loc& orig_loc, bool &changed);
    static CRef<CSeq_loc> ConvertToOrder(const CSeq_loc& orig_loc, bool &changed);
    static bool Extend5(CSeq_feat& feat, CScope& scope);
    static bool Extend3(CSeq_feat& feat, CScope& scope);
    static bool HasNulls(const CSeq_loc& orig_loc);

    static bool Is5AtEndOfSeq(const CSeq_loc& loc, CBioseq_Handle bsh);
    static bool Is3AtEndOfSeq(const CSeq_loc& loc, CBioseq_Handle bsh);

private:
    EPartialPolicy m_PartialPolicy5;
    EPartialPolicy m_PartialPolicy3;
    bool m_Extend5;
    bool m_Extend3;
    EMergePolicy m_MergePolicy;
};


NCBI_XOBJEDIT_EXPORT bool 
    ApplyPolicyToFeature(const CLocationEditPolicy& policy, 
                         const CSeq_feat& orig_feat, 
                         CScope& scope, 
                         bool adjust_gene, bool retranslate_cds);

NCBI_XOBJEDIT_EXPORT CRef<CSeq_loc> 
    SeqLocExtend(const CSeq_loc& loc, size_t pos, CScope* scope);

NCBI_XOBJEDIT_EXPORT void ReverseComplementLocation(CSeq_loc& loc, CScope& scope);

NCBI_XOBJEDIT_EXPORT void ReverseComplementFeature(CSeq_feat& feat, CScope& scope);

NCBI_XOBJEDIT_EXPORT void SeqLocAdjustForTrim(CSeq_loc& loc, 
                                                TSeqPos from, TSeqPos to,
                                                const CSeq_id* seqid,
                                                bool& bCompleteCut,
                                                TSeqPos& trim5,
                                                bool& bAdjusted);

NCBI_XOBJEDIT_EXPORT void SeqLocAdjustForInsert(CSeq_loc& loc, 
                                                TSeqPos from, TSeqPos to,
                                                const CSeq_id* seqid);

NCBI_XOBJEDIT_EXPORT void FeatureAdjustForTrim(CSeq_feat& feat, 
                                                TSeqPos from, TSeqPos to,
                                                const CSeq_id* seqid,
                                                bool& bCompleteCut,
                                                bool& bAdjusted);

NCBI_XOBJEDIT_EXPORT void FeatureAdjustForInsert(CSeq_feat& feat, 
                                                 TSeqPos from, TSeqPos to,
                                                 const CSeq_id* seqid);


typedef enum {
  eSplitLocOption_make_partial = 0x01,
  eSplitLocOption_split_in_exon = 0x02,
  eSplitLocOption_split_in_intron = 0x04
} ESplitLocOptions;

NCBI_XOBJEDIT_EXPORT 
    void SplitLocationForGap(CSeq_loc& loc1, CSeq_loc& loc2,
                             size_t start, size_t stop, 
                             const CSeq_id* seqid,
                             unsigned int options = eSplitLocOption_make_partial | eSplitLocOption_split_in_exon);

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif

