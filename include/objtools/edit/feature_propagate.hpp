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
 * Authors:  Colleen Bollin, Igor Filippov
 */

#ifndef _EDIT_FEATURE_PROPAGATE__HPP_
#define _EDIT_FEATURE_PROPAGATE__HPP_

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbi_message.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/sequence.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

class NCBI_XOBJEDIT_EXPORT CFeaturePropagator
{
public:
    NCBI_DEPRECATED CFeaturePropagator(CBioseq_Handle src, CBioseq_Handle target, const CSeq_align& align,
        bool stop_at_stop = true, bool cleanup_partials = true, bool merge_abutting = true,
        CMessageListener_Basic* pMessageListener = 0,
        CObject_id::TId* feat_id = nullptr);
    CFeaturePropagator(CBioseq_Handle src, CBioseq_Handle target, const CSeq_align& align,
                       bool stop_at_stop = true, bool cleanup_partials = true, bool merge_abutting = true, bool expand_over_gaps = true,
                       CMessageListener_Basic* pMessageListener = 0,
                       CObject_id::TId* feat_id = nullptr);
    ~CFeaturePropagator() {}

    CRef<CSeq_feat> Propagate(const objects::CSeq_feat& orig_feat);
    vector<CRef<CSeq_feat> > PropagateAll();
    vector<CRef<CSeq_feat> > PropagateAllReportFailures(
        vector<CConstRef<CSeq_feat> >&);

    CRef<CSeq_feat> ConstructProteinFeatureForPropagatedCodingRegion(const CSeq_feat& orig_cds, const CSeq_feat& new_cds);

    /// Propagates a feature list from the source sequence
    /// The propagated protein feature is stored right after the propagated cds
    vector<CRef<CSeq_feat>> PropagateFeatureList(const vector<CConstRef<CSeq_feat>>& orig_feats);

    typedef enum {
        eFeaturePropagationProblem_None = 0,
        eFeaturePropagationProblem_FeatureLocation,
        eFeaturePropagationProblem_CodeBreakLocation,
        eFeaturePropagationProblem_AnticodonLocation
    } EGapIntervalType;

private:
   // Prohibit copy constructor and assignment operator
    CFeaturePropagator(const CFeaturePropagator& value);
    CFeaturePropagator& operator=(const CFeaturePropagator& value);

    void x_PropagateCds(CSeq_feat& feat, const CSeq_id& targetId, bool origIsPartialStart);
    void x_CdsMapCodeBreaks(CSeq_feat& feat, const CSeq_id& targetId);
    void x_CdsStopAtStopCodon(CSeq_feat& cds);
    void x_CdsCleanupPartials(CSeq_feat& cds, bool origIsPartialStart);

    void x_PropagatetRNA(CSeq_feat& feat, const CSeq_id& targetId);

    CRef<CSeq_loc> x_MapLocation(const CSeq_loc& sourceLoc, const CSeq_id& targetId);
    TSignedSeqPos SeqPosToAlignPos(TSignedSeqPos pos, CDense_seg::TDim row, bool left, bool &partial5, bool &partial3);
    TSignedSeqPos AlignPosToSeqPos(TSignedSeqPos pos, CDense_seg::TDim row, bool left, bool &partial5, bool &partial3);
    CDense_seg::TDim  FindRow(const CSeq_align& align, CBioseq_Handle bsh);
    CRef<CSeq_loc>  CreateRowSeq_loc(const CSeq_align& align, CDense_seg::TDim  row);
    bool IsOrdered(const CSeq_loc &loc);
    CRef<CSeq_loc> MakeOrdered(const CSeq_loc &loc);

    CRef<CSeq_loc> x_TruncateToStopCodon(const CSeq_loc& loc, unsigned int truncLen);
    CRef<CSeq_loc> x_ExtendToStopCodon(CSeq_feat& feat);

    CBioseq_Handle m_Src;
    CBioseq_Handle m_Target;
    CConstRef<CSeq_align> m_Alignment;
    CScope& m_Scope;
    bool m_CdsStopAtStopCodon;
    bool m_CdsCleanupPartials;
    CMessageListener_Basic* m_MessageListener;
    CObject_id::TId* m_MaxFeatId = nullptr;
    map<CObject_id::TId, CObject_id::TId> m_FeatIdMap; // map old feat-id to propagated feat-id
    bool m_MergeAbutting;
    bool m_ExpandOverGaps;

    class CSynonymMapper : public ISynonymMapper
    {
    public:
        CSynonymMapper(CFeaturePropagator *e) : m_e(e) {}
        virtual ~CSynonymMapper(void) {}
        virtual CSeq_id_Handle GetBestSynonym(const CSeq_id& id) {return sequence::GetId(m_e->m_Scope.GetBioseqHandle(id), sequence::eGetId_Best);}
    private:
        CFeaturePropagator* m_e;
    };
    CSynonymMapper m_synonym_mapper;
};

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif
        // _EDIT_FEATURE_PROPAGATE__HPP_
