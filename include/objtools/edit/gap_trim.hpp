#ifndef EDIT___GAP_TRIM__HPP
#define EDIT___GAP_TRIM__HPP

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
 * Author:  Colleen Bollin
 *
 * File Description:
 *   Adjusting features for gaps
 *   .......
 *
 */

#include <corelib/ncbistd.hpp>

#include <objmgr/scope.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/general/Object_id.hpp>
#include <objmgr/seq_feat_handle.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CBioseq;
class CBioseq_set;
class CSeq_annot;
class CSeq_feat;



class CSeq_entry_Handle;
class CBioseq_Handle;
class CBioseq_set_Handle;
class CSeq_annot_Handle;
class CSeq_feat_Handle;

BEGIN_SCOPE(edit)

class NCBI_XOBJEDIT_EXPORT CFeatGapInfo : public CObject {
public:
    CFeatGapInfo() {};
    CFeatGapInfo(CSeq_feat_Handle sf);
    ~CFeatGapInfo() {};

    void CollectGaps(const CSeq_loc& feat_loc, CScope& scope);
    void CalculateRelevantIntervals(bool unknown_length, bool known_length, bool ns = false);
    bool HasKnown() const { return m_Known; };
    bool HasUnknown() const { return m_Unknown; };
    bool HasNs() const { return m_Ns; };

    bool Trimmable() const;
    bool Splittable() const;
    bool ShouldRemove() const;

    void Trim(CSeq_loc& loc, bool make_partial, CScope& scope);
    typedef vector< CRef<CSeq_loc> > TLocList;
    TLocList Split(const CSeq_loc& orig, bool in_intron, bool make_partial);

    vector<CRef<CSeq_feat> > AdjustForRelevantGapIntervals(bool make_partial, bool trim, bool split, bool in_intron, bool create_general_only = false);
    CSeq_feat_Handle GetFeature() const { return m_Feature; };

    static CRef<CBioseq> AdjustProteinSeq(const CBioseq& seq, const CSeq_feat& feat, const CSeq_feat& orig_cds, CScope& scope);

    bool IsRelatedByCrossRef(const CFeatGapInfo& other) const;

protected:
    typedef enum {
        eGapIntervalType_unknown = 0,
        eGapIntervalType_known,
        eGapIntervalType_n
    } EGapIntervalType;

    typedef pair<EGapIntervalType, pair<size_t, size_t> > TGapInterval;
    typedef vector<TGapInterval> TGapIntervalList;
    TGapIntervalList m_Gaps;

    typedef vector<pair<size_t, size_t> > TIntervalList;
    TIntervalList m_InsideGaps;
    TIntervalList m_LeftGaps;
    TIntervalList m_RightGaps;

    TSeqPos m_Start;
    TSeqPos m_Stop;

    bool m_Known;
    bool m_Unknown;
    bool m_Ns;

    CSeq_feat_Handle m_Feature;

    void x_AdjustOrigLabel(CSeq_feat& feat, size_t& id_offset, string& id_label, const string& qual);
    static void x_AdjustFrame(CCdregion& cdregion, TSeqPos frame_adjust);
    void x_AdjustCodebreaks(CSeq_feat& feat);
    void x_AdjustAnticodons(CSeq_feat& feat);
    bool x_UsableInterval(const TGapInterval& interval, bool unknown_length, bool known_length, bool ns);
};

typedef vector<CRef<CFeatGapInfo> > TGappedFeatList;
NCBI_XOBJEDIT_EXPORT
TGappedFeatList ListGappedFeatures(CFeat_CI& feat_it, CScope& scope);

NCBI_XOBJEDIT_EXPORT
void ProcessForTrimAndSplitUpdates(CSeq_feat_Handle cds, vector<CRef<CSeq_feat> > updates);

// for adjusting feature IDs after splitting

// for fixing a list of features from a split: pass in entire list, feature IDs will be changed
// for features after the first starting with the value of next_id, which will be increased
// after each use
NCBI_XOBJEDIT_EXPORT
void FixFeatureIdsForUpdates(vector<CRef<CSeq_feat> > updates, objects::CObject_id::TId& next_id);

// adjust feature ID for a single feature (will increase next_id if feature had an ID to be adjusted)
NCBI_XOBJEDIT_EXPORT
void FixFeatureIdsForUpdates(CSeq_feat& feat, CObject_id::TId& next_id);

// adjust two lists of features, that are the result of splitting two features that had cross-references
// to each other. Both features must have been split into the same number of pieces.
// The new cross-references pair the elements of the list in order.
NCBI_XOBJEDIT_EXPORT
void FixFeatureIdsForUpdatePair(vector<CRef<CSeq_feat> >& updates1, vector<CRef<CSeq_feat> >& updates2);


END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* EDIT___GAP_TRIM__HPP */
