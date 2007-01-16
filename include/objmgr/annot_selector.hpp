#ifndef ANNOT_SELECTOR__HPP
#define ANNOT_SELECTOR__HPP

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
* Author: Aleksey Grichenko, Michael Kimelman, Eugene Vasilchenko
*
* File Description:
*   Annotations selector structure.
*
*/

#include <corelib/ncbi_limits.h>
#include <objmgr/annot_name.hpp>
#include <objmgr/annot_type_selector.hpp>
#include <objmgr/tse_handle.hpp>

#include <bitset>
#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/** @addtogroup ObjectManagerIterators
 *
 * @{
 */


class CSeq_entry;
class CSeq_annot;

class CTSE_Handle;
class CBioseq_Handle;
class CSeq_entry_Handle;
class CSeq_annot_Handle;
class CAnnotObject_Info;


/////////////////////////////////////////////////////////////////////////////
///
///  SAnnotSelector --
///
///  Structure to control retrieval of Seq-annots

struct NCBI_XOBJMGR_EXPORT SAnnotSelector : public SAnnotTypeSelector
{
    /// Flag to indicate location overlapping method
    enum EOverlapType {
        eOverlap_Intervals,  ///< default - overlapping of individual intervals
        eOverlap_TotalRange  ///< overlapping of total ranges only
    };
    /// Flag to indicate references resolution method
    enum EResolveMethod {
        eResolve_None,   ///< Do not search annotations on segments
        eResolve_TSE,    ///< default - search only on segments in the same TSE
        eResolve_All     ///< Search annotations for all referenced sequences
    };
    /// Flag to indicate sorting method
    enum ESortOrder {
        eSortOrder_None,    ///< do not sort annotations for faster retrieval
        eSortOrder_Normal,  ///< default - increasing start, decreasing length
        eSortOrder_Reverse  ///< decresing end, decreasing length
    };
    /// Flag to indicate handling of unresolved seq-ids
    enum EUnresolvedFlag {
        eIgnoreUnresolved, ///< Ignore unresolved ids (default)
        eSearchUnresolved, ///< Search annotations for unresolvable IDs
        eFailUnresolved    ///< Throw exception for unresolved ids
    };

    SAnnotSelector(TAnnotType annot = CSeq_annot::C_Data::e_not_set,
                   TFeatType  feat  = CSeqFeatData::e_not_set,
                   bool       feat_product = false);
    SAnnotSelector(TFeatType  feat,
                   bool       feat_product = false);
    SAnnotSelector(TFeatSubtype feat_subtype);

    SAnnotSelector(const SAnnotSelector& sel);
    SAnnotSelector& operator=(const SAnnotSelector& sel);
    ~SAnnotSelector(void);

    /// Set annotation type (feat, align, graph)
    SAnnotSelector& SetAnnotType(TAnnotType type)
        {
            if ( GetAnnotType() != type ) {
                x_ClearAnnotTypesSet();
                SAnnotTypeSelector::SetAnnotType(type);
            }
            return *this;
        }

    /// Set feature type (also set annotation type to feat)
    SAnnotSelector& SetFeatType(TFeatType type)
        {
            x_ClearAnnotTypesSet();
            SAnnotTypeSelector::SetFeatType(type);
            return *this;
        }

    /// Set feature subtype (also set annotation and feat type)
    SAnnotSelector& SetFeatSubtype(TFeatSubtype subtype)
        {
            x_ClearAnnotTypesSet();
            SAnnotTypeSelector::SetFeatSubtype(subtype);
            return *this;
        }

    /// Include annotation type in the search
    SAnnotSelector& IncludeAnnotType(TAnnotType type);
    /// Exclude annotation type from the search
    SAnnotSelector& ExcludeAnnotType(TAnnotType type);
    /// Include feature type in the search
    SAnnotSelector& IncludeFeatType(TFeatType type);
    /// Exclude feature type from the search
    SAnnotSelector& ExcludeFeatType(TFeatType type);
    /// Include feature subtype in the search
    SAnnotSelector& IncludeFeatSubtype(TFeatSubtype subtype);
    /// Exclude feature subtype from the search
    SAnnotSelector& ExcludeFeatSubtype(TFeatSubtype subtype);

    /// Check annot type (ignore subtypes).
    bool CheckAnnotType(TAnnotType type) const
        {
            return GetAnnotType() == type;
        }

    /// Set annot type, include all subtypes.
    SAnnotSelector& ForceAnnotType(TAnnotType type);

    /// Return true if at least one subtype of the type is included
    /// or selected type is not set (any).
    bool IncludedAnnotType(TAnnotType type) const;
    bool IncludedFeatType(TFeatType type) const;
    bool IncludedFeatSubtype(TFeatSubtype subtype) const;

    /// Check if type of the annotation matches the selector
    bool MatchType(const CAnnotObject_Info& annot_info) const;

    /// Return true if the features should be searched using their
    /// product rather than location.
    bool GetFeatProduct(void) const
        {
            return m_FeatProduct;
        }
    /// Set flag indicating if the features should be searched by
    /// their product rather than location.
    SAnnotSelector& SetByProduct(bool byProduct = true)
        {
            m_FeatProduct = byProduct;
            return *this;
        }

    /// Get the selected overlap type
    EOverlapType GetOverlapType(void) const
        {
            return m_OverlapType;
        }
    /// Set overlap type.
    ///   eOverlap_Intervals - default, overlapping of locations should
    ///       be checked using individual intervals.
    ///   eOverlap_TotalRange indicates that overlapping of locations
    ///       should be checked only by their total range.
    SAnnotSelector& SetOverlapType(EOverlapType overlap_type)
        {
            m_OverlapType = overlap_type;
            return *this;
        }
    /// Check overlapping of individual intervals
    SAnnotSelector& SetOverlapIntervals(void)
        {
            return SetOverlapType(eOverlap_Intervals);
        }
    /// Check overlapping only of total ranges
    SAnnotSelector& SetOverlapTotalRange(void)
        {
            return SetOverlapType(eOverlap_TotalRange);
        }

    /// Get the selected sort order
    ESortOrder GetSortOrder(void) const
        {
            return m_SortOrder;
        }
    /// Set sort order of annotations.
    ///   eSortOrder_None - do not sort annotations for faster retrieval.
    ///   eSortOrder_Normal - default. Sort by start (increasing), then by
    ///       length (decreasing).
    ///   eSortOrder_Reverse - sort by end (decresing), then length
    ///       (decreasing).
    SAnnotSelector& SetSortOrder(ESortOrder sort_order)
        {
            m_SortOrder = sort_order;
            return *this;
        }

    /// GetResolveMethod() returns current value of resolve_method.
    ///
    ///  @sa
    ///    SetResolveMethod(), SetResolveNone(), SetResolveTSE(),
    ///    SetResolveAll()
    EResolveMethod GetResolveMethod(void) const
        {
            return m_ResolveMethod;
        }
    /// SetResolveMethod() controls visibility of subsegments depending
    /// on whether it's packaged together with master sequence.
    ///   eResolve_None means to skip all subsegments completely.
    ///       It has the same effect as calling SetResolveDepth(0).
    ///   eResolve_TSE means to look on 'near' segments, packaged in the
    ///       Same TSE (top level Seq-entry).
    ///   eResolve_All lifts any restriction of segments by packaging.
    /// This option works in addition to 'resolve depth', 'adaptive depth',
    /// and 'exact depth'.
    ///
    ///  @sa
    ///    SetResolveNone(), SetResolveTSE(), SetResolveAll(),
    ///    SetResolveDepth(), SetExactDepth(), SetAdaptiveDepth(),
    ///    SetUnresolvedFlag().
    SAnnotSelector& SetResolveMethod(EResolveMethod resolve_method)
        {
            m_ResolveMethod = resolve_method;
            return *this;
        }
    /// SetResolveNone() is equivalent to SetResolveMethod(eResolve_None).
    ///  @sa
    ///    SetResolveMethod()
    SAnnotSelector& SetResolveNone(void)
        {
            return SetResolveMethod(eResolve_None);
        }
    /// SetResolveTSE() is equivalent to SetResolveMethod(eResolve_TSE).
    ///  @sa
    ///    SetResolveMethod()
    SAnnotSelector& SetResolveTSE(void)
        {
            return SetResolveMethod(eResolve_TSE);
        }
    /// SetResolveAll() is equivalent to SetResolveMethod(eResolve_All).
    ///  @sa
    ///    SetResolveMethod()
    SAnnotSelector& SetResolveAll(void)
        {
            return SetResolveMethod(eResolve_All);
        }

    /// GetResolveDepth() returns current limit of subsegment resolution
    /// in searching annotations.
    ///
    ///  @sa
    ///    SetResolveDepth()
    int GetResolveDepth(void) const
        {
            return m_ResolveDepth;
        }
    /// SetResolveDepth sets the limit of subsegment resolution
    /// in searching annotations.
    /// Zero means look for annotations directly pointing
    /// to the sequence. One means to look on direct segments
    /// of the sequence too.
    /// By default the limit is set to kIntMax, meaning no restriction.
    /// 
    ///  @sa
    ///    SetExactDepth(), SetAdaptiveDepth(), SetAdaptiveTrigger()
    SAnnotSelector& SetResolveDepth(int depth)
        {
            m_ResolveDepth = depth;
            return *this;
        }

    typedef vector<SAnnotTypeSelector> TAdaptiveTriggers;
    /// GetAdaptiveDepth() returns current value of 'adaptive depth' flag.
    ///
    ///  @sa
    ///    SetAdaptiveDepth()
    bool GetAdaptiveDepth(void) const
        {
            return m_AdaptiveDepth;
        }
    /// SetAdaptiveDepth() requests to restrict subsegment resolution
    /// depending on annotations found on lower level of segments.
    /// It's meaningful in cases when features on segments are also
    /// annotated on master sequence. Setting this flag will avoid
    /// duplicates, and speed up loading.
    /// Annotation iterator will look for annotations of special types,
    /// by default it's gene, mrna, and cds. If any annotation from those
    /// types is found the iterator will treat this as annotated sequence,
    /// and not look for more annotations on subsegments.
    /// This option works in addition to SetResolveDepth(), so subsegment
    /// resolution stops when either specified depth is reached, or if
    /// any adaptive trigger annotation is found.
    /// Adaptive depth flag has no effect if 'exact depth' is set.
    ///
    /// Note, that trigger annotations on one segment has no effect on
    /// adaptive resolution of another segment.
    /// So, for example, if
    /// Master sequence A has segments B1 and B2, while B1 has segments
    /// C11 and C12, and B2 has segments C21 and C22:
    ///  |--------------- A ----------------|
    ///  |------ B1 ------||------ B2 ------|
    ///  |- C11 -||- C12 -||- C21 -||- C22 -|
    /// Also, there are genes only on sequences B1, C11, C12, C21, and C22.
    /// For simplicity, there are no other adaptive trigger annotations.
    /// In this case annotation iterator in 'adaptive' mode will return
    /// genes and other annotations from sequences A, B1, B2, C21, and C22.
    /// It will skip searching on C11, and C12 because trigger feature will
    /// be found on B1.
    ///
    ///  @sa
    ///    SetResolveDepth(), SetExactDepth(), SetAdaptiveTrigger()
    SAnnotSelector& SetAdaptiveDepth(bool value = true)
        {
            m_AdaptiveDepth = value;
            return *this;
        }
    /// SetAdaptiveTrigger() allows to change default set of adaptive trigger
    /// annotations.
    /// Default set is: gene, mrna, cds.
    SAnnotSelector& SetAdaptiveTrigger(const SAnnotTypeSelector& sel);

    /// SetExactDepth() specifies that annotations will be searched
    /// on the segment level specified by SetResolveDepth() only.
    /// By default this flag is not set, and annotations iterators
    /// will also return annotations from above levels.
    /// This flag, when set, overrides 'adaptive depth' flag.
    ///
    /// Examples:
    ///   SetResolveDepth(0)
    ///       - only direct annotations on the sequence will be found.
    ///   SetResolveDepth(1), SetExactDepth(false) (default)
    ///       - find annotations on the sequence, and its direct segments.
    ///   SetResolveDepth(1), SetExactDepth(true)
    ///       - find annotations on the direct segments.
    ///
    ///  @sa
    ///    SetResolveDepth(), SetAdaptiveDepth(), SetAdaptiveTrigger()
    SAnnotSelector& SetExactDepth(bool value = true)
        {
            m_ExactDepth = value;
            return *this;
        }
    /// GetExactDepth() returns current value of 'exact depth' flag.
    ///
    ///  @sa
    ///    SetExactDepth()
    bool GetExactDepth(void) const
        {
            return m_ExactDepth;
        }

    /// Get maximum allowed number of annotations to find.
    size_t GetMaxSize(void) const
        {
            return m_MaxSize;
        }
    /// Set maximum number of annotations to find.
    /// Set to 0 for no limit (default).
    SAnnotSelector& SetMaxSize(size_t max_size)
        {
            m_MaxSize = max_size? max_size: kMax_UInt;
            return *this;
        }

    /// Check if the parent object of annotations is set. If set,
    /// only the annotations from the object (TSE, seq-entry or seq-annot)
    /// will be found.
    bool HasLimit(void)
        {
            return m_LimitObject.NotEmpty();
        }
    /// Remove restrictions on the parent object of annotations.
    SAnnotSelector& SetLimitNone(void);
    /// Limit annotations to those from the TSE only.
    SAnnotSelector& SetLimitTSE(const CTSE_Handle& limit);
    SAnnotSelector& SetLimitTSE(const CSeq_entry_Handle& limit);
    /// Limit annotations to those from the seq-entry only.
    SAnnotSelector& SetLimitSeqEntry(const CSeq_entry_Handle& limit);
    /// Limit annotations to those from the seq-annot only.
    SAnnotSelector& SetLimitSeqAnnot(const CSeq_annot_Handle& limit);

    /// Get current method of handling unresolved seq-ids
    EUnresolvedFlag GetUnresolvedFlag(void) const
        {
            return m_UnresolvedFlag;
        }
    /// Set method of handling unresolved seq-ids. A seq-id may be
    /// unresolvable due to EResolveMethod restrictions. E.g. a
    /// seq-id may be a far reference, while the annotations for this
    /// seq-id are stored in the master sequence TSE.
    ///   eIgnoreUnresolved - default, do not search for annotations
    ///       on unresolved seq-ids.
    ///   eSearchUnresolved - search for annotations on unresolved
    ///       seq-ids.
    ///   eFailUnresolved - throw CAnnotException exception if a seq-id
    ///       can not be resolved.
    ///  @sa
    ///    SetSearchExternal()
    SAnnotSelector& SetUnresolvedFlag(EUnresolvedFlag flag)
        {
            m_UnresolvedFlag = flag;
            return *this;
        }
    SAnnotSelector& SetIgnoreUnresolved(void)
        {
            m_UnresolvedFlag = eIgnoreUnresolved;
            return *this;
        }
    SAnnotSelector& SetSearchUnresolved(void)
        {
            m_UnresolvedFlag = eSearchUnresolved;
            return *this;
        }
    SAnnotSelector& SetFailUnresolved(void)
        {
            m_UnresolvedFlag = eFailUnresolved;
            return *this;
        }

    /// Set all flags for searching external annotations. "seq" or "tse"
    /// should contain the virtual segmented bioseq. The flags set are:
    ///   ResolveTSE - prevent loading external bioseqs
    ///   LimitTSE   - search only external annotations near the virtual bioseq
    ///   SearchUnresolved - search on unresolved IDs.
    SAnnotSelector& SetSearchExternal(const CTSE_Handle& tse);
    SAnnotSelector& SetSearchExternal(const CSeq_entry_Handle& tse);
    SAnnotSelector& SetSearchExternal(const CBioseq_Handle& seq);

    /// Exclude all external annotations from the search.
    /// Effective only when no limit is set.
    SAnnotSelector& SetExcludeExternal(bool exclude = true)
        {
            m_ExcludeExternal = exclude;
            return *this;
        }

    typedef vector<CAnnotName> TAnnotsNames;
    /// Select annotations from all Seq-annots
    SAnnotSelector& ResetAnnotsNames(void);
    /// Reset special processing of unnamed annots (added or excluded)
    SAnnotSelector& ResetUnnamedAnnots(void);
    /// Reset special processing of named annots (added or excluded)
    SAnnotSelector& ResetNamedAnnots(const CAnnotName& name);
    SAnnotSelector& ResetNamedAnnots(const char* name);
    /// Add unnamed annots to set of annots names to look for
    SAnnotSelector& AddUnnamedAnnots(void);
    /// Add named annot to set of annots names to look for
    SAnnotSelector& AddNamedAnnots(const CAnnotName& name);
    SAnnotSelector& AddNamedAnnots(const char* name);
    /// Add unnamed annots to set of annots names to exclude
    SAnnotSelector& ExcludeUnnamedAnnots(void);
    /// Add named annot to set of annots names to exclude
    SAnnotSelector& ExcludeNamedAnnots(const CAnnotName& name);
    SAnnotSelector& ExcludeNamedAnnots(const char* name);
    /// Look for all named Seq-annots
    SAnnotSelector& SetAllNamedAnnots(void);
    // Compatibility:
    /// Look for named annot.
    /// If name is empty ("") look for unnamed annots too.
    SAnnotSelector& SetDataSource(const string& name);

    // Access methods for iterator
    bool IsSetAnnotsNames(void) const
        {
            return
                !m_IncludeAnnotsNames.empty() ||
                !m_ExcludeAnnotsNames.empty();
        }
    bool IncludedAnnotName(const CAnnotName& name) const;
    bool ExcludedAnnotName(const CAnnotName& name) const;

    // Limit search with a set of TSEs
    SAnnotSelector& ExcludeTSE(const CTSE_Handle& tse);
    SAnnotSelector& ExcludeTSE(const CSeq_entry_Handle& tse);
    SAnnotSelector& ResetExcludedTSE(void);
    bool ExcludedTSE(const CTSE_Handle& tse) const;
    bool ExcludedTSE(const CSeq_entry_Handle& tse) const;

    // No locations mapping flag. Set to true by CAnnot_CI.
    bool GetNoMapping(void) const
        {
            return m_NoMapping;
        }
    SAnnotSelector& SetNoMapping(bool value = true)
        {
            m_NoMapping = value;
            return *this;
        }

    /// Try to avoid collecting multiple objects from the same seq-annot.
    /// Speeds up collecting seq-annots with SNP features.
    SAnnotSelector& SetCollectSeq_annots(bool value = true)
        {
            m_CollectSeq_annots = value;
            return *this;
        }

    /// Collect available annot types rather than annots.
    ///  @sa
    ///    CAnnotTypes_CI::GetAnnotTypes()
    SAnnotSelector& SetCollectTypes(bool value = true)
        {
            m_CollectTypes = value;
            return *this;
        }

protected:
    friend class CAnnot_Collector;

    void CheckLimitObjectType(void) const;

    static bool x_Has(const TAnnotsNames& names, const CAnnotName& name);
    static void x_Add(TAnnotsNames& names, const CAnnotName& name);
    static void x_Del(TAnnotsNames& names, const CAnnotName& name);
    void x_InitializeAnnotTypesSet(bool default_value);
    void x_ClearAnnotTypesSet(void);

    typedef bitset<CSeqFeatData::eSubtype_max+2> TAnnotTypesBitset;
    typedef vector<CTSE_Handle> TTSE_Limits;

    bool                  m_FeatProduct;  // "true" for searching products
    int                   m_ResolveDepth;
    EOverlapType          m_OverlapType;
    EResolveMethod        m_ResolveMethod;
    ESortOrder            m_SortOrder;

    enum ELimitObject {
        eLimit_None,
        eLimit_TSE_Info,        // CTSE_Info + m_LimitTSE
        eLimit_Seq_entry_Info,  // CSeq_entry_Info + m_LimitTSE
        eLimit_Seq_annot_Info   // CSeq_annot_Info + m_LimitTSE
    };
    mutable ELimitObject  m_LimitObjectType;
    EUnresolvedFlag       m_UnresolvedFlag;
    CConstRef<CObject>    m_LimitObject;
    CTSE_Handle           m_LimitTSE;
    size_t                m_MaxSize; //
    TAnnotsNames          m_IncludeAnnotsNames;
    TAnnotsNames          m_ExcludeAnnotsNames;
    bool                  m_NoMapping;
    bool                  m_AdaptiveDepth;
    bool                  m_ExactDepth;
    bool                  m_ExcludeExternal;
    bool                  m_CollectSeq_annots;
    bool                  m_CollectTypes;
    TAdaptiveTriggers     m_AdaptiveTriggers;
    TTSE_Limits           m_ExcludedTSE;
    TAnnotTypesBitset     m_AnnotTypesBitset;
};


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log: annot_selector.hpp,v $
* Revision 1.52  2006/06/21 20:09:45  vasilche
* Reset included annot subtypes only if annot type is changed.
*
* Revision 1.51  2006/06/19 20:22:34  grichenk
* Added comments. Removed SegmentSelect flag.
*
* Revision 1.50  2006/06/14 15:14:53  vasilche
* Added description of segment resolution controls.
*
* Revision 1.49  2006/02/14 15:47:41  grichenk
* Added methods for collecting types of annotations.
*
* Revision 1.48  2005/12/15 21:33:55  vasilche
* Added SetExactDepth() option.
* Fixed constness of getters.
*
* Revision 1.47  2005/04/11 17:51:38  grichenk
* Fixed m_CollectSeq_annots initialization.
* Avoid copying SAnnotSelector in CAnnotTypes_CI.
*
* Revision 1.46  2005/04/08 14:31:26  grichenk
* Changed TAnnotTypesSet from vector to bitset
*
* Revision 1.45  2005/03/17 17:52:27  grichenk
* Added flag to SAnnotSelector for skipping multiple SNPs from the same
* seq-annot. Optimized CAnnotCollector::GetAnnot().
*
* Revision 1.44  2005/03/14 18:19:02  grichenk
* Added SAnnotSelector(TFeatSubtype), fixed initialization of CFeat_CI and
* SAnnotSelector.
*
* Revision 1.43  2005/01/12 17:16:13  vasilche
* Avoid performance warning on MSVC.
*
* Revision 1.42  2005/01/06 16:41:30  grichenk
* Removed deprecated methods
*
* Revision 1.41  2005/01/04 16:50:34  vasilche
* Added SAnnotSelector::SetExcludeExternal().
*
* Revision 1.40  2004/12/22 15:56:04  vasilche
* Introduced CTSE_Handle.
*
* Revision 1.39  2004/12/13 17:02:24  grichenk
* Removed redundant constructors
*
* Revision 1.38  2004/11/22 16:04:06  grichenk
* Fixed/added doxygen comments
*
* Revision 1.37  2004/11/15 22:21:48  grichenk
* Doxygenized comments, fixed group names.
*
* Revision 1.36  2004/11/04 19:21:17  grichenk
* Marked non-handle versions of SetLimitXXXX as deprecated
*
* Revision 1.35  2004/10/29 16:29:47  grichenk
* Prepared to remove deprecated methods, added new constructors.
*
* Revision 1.34  2004/09/27 14:35:45  grichenk
* +Flag for handling unresolved IDs (search/ignore/fail)
* +Selector method for external annotations search
*
* Revision 1.33  2004/09/07 14:11:11  grichenk
* Added getters
*
* Revision 1.32  2004/08/25 17:11:25  grichenk
* Removed obsolete SAnnotSelector::SetCombineMethod()
*
* Revision 1.31  2004/08/05 18:23:48  vasilche
* CAnnotName and CAnnotTypeSelector are moved in separate headers. Added TSE_Lock field.
*
* Revision 1.30  2004/04/05 15:56:13  grichenk
* Redesigned CAnnotTypes_CI: moved all data and data collecting
* functions to CAnnotDataCollector. CAnnotTypes_CI is no more
* inherited from SAnnotSelector.
*
* Revision 1.29  2004/03/17 16:03:42  vasilche
* Removed Windows EOL
*
* Revision 1.28  2004/03/16 15:47:25  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.27  2004/02/26 14:41:40  grichenk
* Fixed types excluding in SAnnotSelector and multiple types search
* in CAnnotTypes_CI.
*
* Revision 1.26  2004/02/11 22:19:23  grichenk
* Fixed annot type initialization in iterators
*
* Revision 1.25  2004/02/05 19:53:39  grichenk
* Fixed type matching in SAnnotSelector. Added IncludeAnnotType().
*
* Revision 1.24  2004/02/04 18:05:31  grichenk
* Added annotation filtering by set of types/subtypes.
* Renamed *Choice to *Type in SAnnotSelector.
*
* Revision 1.23  2004/01/23 16:14:45  grichenk
* Implemented alignment mapping
*
* Revision 1.22  2003/11/13 19:12:51  grichenk
* Added possibility to exclude TSEs from annotations request.
*
* Revision 1.21  2003/10/09 20:20:59  vasilche
* Added possibility to include and exclude Seq-annot names to annot iterator.
* Fixed adaptive search. It looked only on selected set of annot names before.
*
* Revision 1.20  2003/10/07 13:43:22  vasilche
* Added proper handling of named Seq-annots.
* Added feature search from named Seq-annots.
* Added configurable adaptive annotation search (default: gene, cds, mrna).
* Fixed selection of blobs for loading from GenBank.
* Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
* Fixed leaked split chunks annotation stubs.
* Moved some classes definitions in separate *.cpp files.
*
* Revision 1.19  2003/10/01 15:08:48  vasilche
* Do not reset feature type if feature subtype is set to 'any'.
*
* Revision 1.18  2003/09/30 16:21:59  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
* Revision 1.17  2003/09/16 14:21:46  grichenk
* Added feature indexing and searching by subtype.
*
* Revision 1.16  2003/09/11 17:45:06  grichenk
* Added adaptive-depth option to annot-iterators.
*
* Revision 1.15  2003/08/22 14:58:55  grichenk
* Added NoMapping flag (to be used by CAnnot_CI for faster fetching).
* Added GetScope().
*
* Revision 1.14  2003/07/17 21:01:43  vasilche
* Fixed ambiguity on Windows
*
* Revision 1.13  2003/07/17 20:07:55  vasilche
* Reduced memory usage by feature indexes.
* SNP data is loaded separately through PUBSEQ_OS.
* String compression for SNP data.
*
* Revision 1.12  2003/06/25 20:56:29  grichenk
* Added max number of annotations to annot-selector, updated demo.
*
* Revision 1.11  2003/06/17 20:34:02  grichenk
* Added flag to ignore sorting
*
* Revision 1.10  2003/04/28 14:59:58  vasilche
* Added missing initialization.
*
* Revision 1.9  2003/04/24 16:12:37  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.8  2003/03/31 21:48:17  grichenk
* Added possibility to select feature subtype through SAnnotSelector.
*
* Revision 1.7  2003/03/18 14:46:35  grichenk
* Set limit object type to "none" if the object is null.
*
* Revision 1.6  2003/03/14 20:39:26  ucko
* return *this from SetIdResolvingXxx().
*
* Revision 1.5  2003/03/14 19:10:33  grichenk
* + SAnnotSelector::EIdResolving; fixed operator=() for several classes
*
* Revision 1.4  2003/03/10 16:55:16  vasilche
* Cleaned SAnnotSelector structure.
* Added shortcut when features are limited to one TSE.
*
* Revision 1.3  2003/03/05 20:56:42  vasilche
* SAnnotSelector now holds all parameters of annotation iterators.
*
* Revision 1.2  2003/02/25 14:48:06  vasilche
* Added Win32 export modifier to object manager classes.
*
* Revision 1.1  2003/02/24 19:36:19  vasilche
* Added missing annot_selector.hpp.
*
*
* ===========================================================================
*/

#endif  // ANNOT_SELECTOR__HPP
