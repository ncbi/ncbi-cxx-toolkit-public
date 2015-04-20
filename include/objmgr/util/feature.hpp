#ifndef FEAT__HPP
#define FEAT__HPP

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
* Author:  Clifford Clausen
*
* File Description:
*   Feature utilities
*/

#include <corelib/ncbistd.hpp>
#include <objmgr/annot_selector.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objects/seqfeat/Gb_qual.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// Forward declarations
class CSeq_feat;
class CScope;
class CFeat_CI;
class CTSE_Handle;
class CFeat_id;
class CGene_ref;
class CGb_qual;

BEGIN_SCOPE(feature)


/** @addtogroup ObjUtilFeature
 *
 * @{
 */


/** @name GetLabel
 * Returns a label for a CSeq_feat. Label may be based on just the type of 
 * feature, just the content of the feature, or both. If scope is 0, then the
 * label will not include information from feature products.
 * @{
 */

enum FFeatLabelFlags {
    fFGL_Type       = 1 <<  1, ///< Always include the feature's type.
    fFGL_Content    = 1 <<  2, ///< Include its content if there is any.
    fFGL_Both       = fFGL_Type | fFGL_Content,
    fFGL_NoComments = 1 <<  3  ///< Leave out comments, even as fallbacks.
};
typedef int TFeatLabelFlags; ///< binary OR of FFeatLabelFlags

NCBI_XOBJUTIL_EXPORT
void GetLabel(const CSeq_feat& feat, string* label, TFeatLabelFlags flags,
              CScope* scope = 0);

/// For compatibility with legacy code.
enum ELabelType {
    eType,
    eContent,
    eBoth
};

NCBI_XOBJUTIL_EXPORT
NCBI_DEPRECATED
void GetLabel (const CSeq_feat&    feat, 
               string*             label, 
               ELabelType          label_type,
               CScope*             scope = 0);

/* @} */


class CFeatIdRemapper : public CObject
{
public:

    void Reset(void);
    size_t GetFeatIdsCount(void) const;

    int RemapId(int old_id, const CTSE_Handle& tse);
    bool RemapId(CFeat_id& id, const CTSE_Handle& tse);
    bool RemapId(CFeat_id& id, const CFeat_CI& feat_it);
    bool RemapIds(CSeq_feat& feat, const CTSE_Handle& tse);
    CRef<CSeq_feat> RemapIds(const CFeat_CI& feat_it);
    
private:
    typedef pair<int, CTSE_Handle> TFullId;
    typedef map<TFullId, int> TIdMap;
    TIdMap m_IdMap;
};


class NCBI_XOBJUTIL_EXPORT CFeatComparatorByLabel : public CObject,
                                                    public IFeatComparator
{
public:
    virtual bool Less(const CSeq_feat& f1,
                      const CSeq_feat& f2,
                      CScope* scope);
};


NCBI_XOBJUTIL_EXPORT
CMappedFeat MapSeq_feat(const CSeq_feat_Handle& feat,
                        const CBioseq_Handle& master_seq,
                        const CRange<TSeqPos>& range);
NCBI_XOBJUTIL_EXPORT
CMappedFeat MapSeq_feat(const CSeq_feat_Handle& feat,
                        const CSeq_id_Handle& master_id,
                        const CRange<TSeqPos>& range);
NCBI_XOBJUTIL_EXPORT
CMappedFeat MapSeq_feat(const CSeq_feat_Handle& feat,
                        const CBioseq_Handle& master_seq);
NCBI_XOBJUTIL_EXPORT
CMappedFeat MapSeq_feat(const CSeq_feat_Handle& feat,
                        const CSeq_id_Handle& master_id);


/// @name GetParentFeature
/// Returns a best parent feature for a CMappedFeat.
/// CSeq_feat_Handle is convertible to a CMappedFeat so it can be used too.
NCBI_XOBJUTIL_EXPORT
CMappedFeat GetParentFeature(const CMappedFeat& feat);

struct STypeLink;
class CFeatTreeIndex;

/////////////////////////////////////////////////////////////////////////////
/// CFeatTree
/// 
/// CFeatTree class builds a parent-child feature tree in a more efficient way
/// than repeatedly calling of GetParentFeature() for each feature.
/// The algorithm of a parent search is the same as the one used by
/// GetParentFeature().
///
/// The class CFeatTree works with a set of features which should be
/// specified via AddFeature() or AddFeatures() call.
/// The actual tree is built when the first time method GetParent()
/// or GetChildren() is called after adding new features.
/// Features can be added later, but the parent information is cached and will
/// not change if parents were found already. However, features with no parent
/// will be processed again in attempt to find parents from the newly added
/// features.
class NCBI_XOBJUTIL_EXPORT CFeatTree : public CObject
{
public:
    /// Construct empty tree.
    CFeatTree(void);
    /// Construct a tree with features collected by a CFeat_CI.
    explicit
    CFeatTree(CFeat_CI it);
    /// Construct a tree with features from a Seq-annot
    explicit
    CFeatTree(const CSeq_annot_Handle& sah);
    /// Construct a tree with features from a Seq-entry
    explicit
    CFeatTree(const CSeq_entry_Handle& seh);
    /// Destructor.
    ~CFeatTree(void);

    CFeatTree(const CFeatTree&);
    CFeatTree& operator=(const CFeatTree&);

    /// Mode of processing feature ids
    enum EFeatIdMode {
        eFeatId_ignore,
        eFeatId_by_type, // default
        eFeatId_always
    };
    EFeatIdMode GetFeatIdMode(void) const {
        return m_FeatIdMode;
    }
    void SetFeatIdMode(EFeatIdMode mode);

    /// Mode of processing feature ids
    enum EBestGeneFeatIdMode {
        eBestGeneFeatId_ignore,
        eBestGeneFeatId_always // default
    };
    EBestGeneFeatIdMode GetBestGeneFeatIdMode(void) const {
        return m_BestGeneFeatIdMode;
    }
    void SetBestGeneFeatIdMode(EBestGeneFeatIdMode mode);

    /// Mode for taking into account best gene
    ///   eGeneCheck_match will try to match a parent feature only
    ///   if they both match to the same gene
    enum EGeneCheckMode {
        eGeneCheck_ignore,
        eGeneCheck_match // default
    };
    EGeneCheckMode GetGeneCheckMode(void) const {
        return m_GeneCheckMode;
    }
    void SetGeneCheckMode(EGeneCheckMode mode);
    
    /// Mode of processing SNP strands
    enum ESNPStrandMode {
        eSNPStrand_same,
        eSNPStrand_both  // default
    };
    ESNPStrandMode GetSNPStrandMode(void) const {
        return m_SNPStrandMode;
    }
    void SetSNPStrandMode(ESNPStrandMode mode);
    
    /// Add all features collected by a CFeat_CI to the tree.
    void AddFeatures(CFeat_CI it);
    /// Add a single feature to the tree.
    void AddFeature(const CMappedFeat& feat);

    /// Add all features from bottom_type to top_type for a feature.
    void AddFeaturesFor(CScope& scope, const CSeq_loc& loc,
                        CSeqFeatData::ESubtype bottom_type,
                        CSeqFeatData::ESubtype top_type,
                        const SAnnotSelector* base_sel = 0,
                        bool skip_bottom = false);
    void AddFeaturesFor(const CMappedFeat& feat,
                        CSeqFeatData::ESubtype bottom_type,
                        CSeqFeatData::ESubtype top_type,
                        const SAnnotSelector* base_sel = 0);
    void AddFeaturesFor(const CMappedFeat& feat,
                        CSeqFeatData::ESubtype top_type,
                        const SAnnotSelector* base_sel = 0);
    /// Add all necessary features to get genes for a mRNA feature.
    void AddGenesForMrna(const CMappedFeat& mrna_feat,
                         const SAnnotSelector* base_sel = 0);
    /// Add all necessary features to get cdregions for a mRNA feature.
    void AddCdsForMrna(const CMappedFeat& mrna_feat,
                       const SAnnotSelector* base_sel = 0);
    /// Add all necessary features to get mRNAs for a gene feature.
    void AddMrnasForGene(const CMappedFeat& gene_feat,
                         const SAnnotSelector* base_sel = 0);
    /// Add all necessary features to get cdregions for a gene feature.
    void AddCdsForGene(const CMappedFeat& gene_feat,
                       const SAnnotSelector* base_sel = 0);
    /// Add all necessary features to get genes for a cdregion feature.
    void AddGenesForCds(const CMappedFeat& cds_feat,
                        const SAnnotSelector* base_sel = 0);
    /// Add all necessary features to get mRNAs for a cdregion feature.
    void AddMrnasForCds(const CMappedFeat& cds_feat,
                        const SAnnotSelector* base_sel = 0);
    /// Add all necessary features to get genes for an arbitrary feature.
    void AddGenesForFeat(const CMappedFeat& feat,
                         const SAnnotSelector* base_sel = 0);

    /// Find a corresponding CMappedFeat for a feature already added to a tree.
    /// Will throw an exception if the feature is not in the tree.
    const CMappedFeat& GetMappedFeat(const CSeq_feat_Handle& feat) const;
    /// Return nearest parent of a feature.
    /// Will throw an exception if the feature is not in the tree.
    /// Will return null CMappedFeat if the feature has no parent.
    CMappedFeat GetParent(const CMappedFeat& feat);
    /// Return parent of a feature of the specified type, it may be not
    /// the nearest parent, but a parent's parent, and so on.
    /// Will throw an exception if the feature is not in the tree.
    /// Will return null CMappedFeat if the feature has no parent of the type.
    CMappedFeat GetParent(const CMappedFeat& feat,
                          CSeqFeatData::E_Choice type);
    /// Return parent of a feature of the specified subtype, it may be not
    /// the nearest parent, but a parent's parent, and so on.
    /// Will throw an exception if the feature is not in the tree.
    /// Will return null CMappedFeat if the feature has no parent of the type.
    CMappedFeat GetParent(const CMappedFeat& feat,
                          CSeqFeatData::ESubtype subtype);
    /// Return all nearest children of a feature.
    /// Will throw an exception if the feature is not in the tree.
    /// Will return an empty vector if the feature has no children.
    /// If the feat argument is null then all the features without parent
    /// are returned.
    vector<CMappedFeat> GetChildren(const CMappedFeat& feat);
    /// Store all nearest children of a feature into a vector.
    /// Will throw an exception if the feature is not in the tree.
    /// The second argument will become empty if the feature has no children.
    /// If the feat argument is null then all the features without parent
    /// are returned.
    void GetChildrenTo(const CMappedFeat& feat, vector<CMappedFeat>& children);
    /// Return all root features (w/o parent).
    vector<CMappedFeat> GetRootFeatures(void) {
        return GetChildren(CMappedFeat());
    }
    /// Store all root features (w/o parent) into a vector.
    void GetRootFeaturesTo(vector<CMappedFeat>& children) {
        GetChildrenTo(CMappedFeat(), children);
    }

    enum EBestGeneType {
        eBestGene_TreeOnly,        // determined by feature position in tree
        eBestGene_AllowOverlapped, // get gene from tree, then by overlapping
        eBestGene_OverlappedOnly   // get gene by overlapping only
    };

    /// Return parent gene if exists or best overlapping gene
    CMappedFeat GetBestGene(const CMappedFeat& feat,
                            EBestGeneType lookup_type = eBestGene_TreeOnly);

public:
    class CFeatInfo {
    public:
        CFeatInfo(void);
        ~CFeatInfo(void);

        const CTSE_Handle& GetTSE(void) const;
        bool IsSetParent(void) const {
            return m_IsSetParent;
        }
        CSeqFeatData::ESubtype GetSubtype(void) const {
            return m_Feat.GetFeatSubtype();
        }
        bool IsGene(void) const {
            return GetSubtype() == CSeqFeatData::eSubtype_gene;
        }

        typedef vector<CFeatInfo*> TChildren;
        
        size_t m_AddIndex;
        CMappedFeat m_Feat;
        CRange<TSeqPos> m_MasterRange;
        bool m_CanMatchByQual;
        bool m_IsSetParent, m_IsSetChildren, m_MultiId;
        enum EIsLinkedToRoot NCBI_PACKED_ENUM_TYPE(Int1) {
            eIsLinkedToRoot_unknown,
            eIsLinkedToRoot_linked,
            eIsLinkedToRoot_linking
        } NCBI_PACKED_ENUM_END();
        EIsLinkedToRoot m_IsLinkedToRoot;
        CFeatInfo* m_Parent;
        CFeatInfo* m_Gene;
        TChildren m_Children;
    };
    typedef vector<CFeatInfo*> TFeatArray;

protected:
    typedef vector<CFeatInfo*> TChildren;

    void x_Init(void);

    CFeatInfo& x_GetInfo(const CSeq_feat_Handle& feat);
    CFeatInfo& x_GetInfo(const CMappedFeat& feat);
    CFeatInfo* x_FindInfo(const CSeq_feat_Handle& feat);

    void x_AssignParents(void);
    void x_AssignParentsByOverlap(TFeatArray& features,
                                  const STypeLink& link);
    bool x_AssignParentByRef(CFeatInfo& info);

    void x_AssignGenesByOverlap(TFeatArray& features);
    void x_AssignGenes(void);

    pair<int, CFeatInfo*>
    x_LookupParentByRef(CFeatInfo& info,
                        CSeqFeatData::ESubtype parent_type);
    void x_VerifyLinkedToRoot(CFeatInfo& info);

    void x_SetParent(CFeatInfo& info, CFeatInfo& parent);
    void x_SetNoParent(CFeatInfo& info);
    CFeatInfo* x_GetParent(CFeatInfo& info);
    const TChildren& x_GetChildren(CFeatInfo& info);
    void x_SetGeneRecursive(CFeatInfo& info, CFeatInfo& gene);

    typedef map<CSeq_feat_Handle, CFeatInfo> TInfoMap;
    typedef vector<CFeatInfo*> TInfoArray;

    size_t m_AssignedParents, m_AssignedGenes;
    TInfoMap m_InfoMap;
    TInfoArray m_InfoArray;
    CFeatInfo m_RootInfo;
    EFeatIdMode m_FeatIdMode;
    EBestGeneFeatIdMode m_BestGeneFeatIdMode;
    EGeneCheckMode m_GeneCheckMode;
    ESNPStrandMode m_SNPStrandMode;
    CRef<CFeatTreeIndex> m_Index;
};


/* @} */


/////////////////////////////////////////////////////////////////////////////
// Versions of functions with lookup using CFeatTree

NCBI_XOBJUTIL_EXPORT
CMappedFeat
GetBestGeneForMrna(const CMappedFeat& mrna_feat,
                   CFeatTree* feat_tree = 0,
                   const SAnnotSelector* base_sel = 0,
                   CFeatTree::EBestGeneType lookup_type = CFeatTree::eBestGene_TreeOnly);


NCBI_XOBJUTIL_EXPORT
CMappedFeat
GetBestGeneForCds(const CMappedFeat& cds_feat,
                  CFeatTree* feat_tree = 0,
                  const SAnnotSelector* base_sel = 0,
                  CFeatTree::EBestGeneType lookup_type = CFeatTree::eBestGene_TreeOnly);


NCBI_XOBJUTIL_EXPORT
CMappedFeat
GetBestMrnaForCds(const CMappedFeat& cds_feat,
                  CFeatTree* feat_tree = 0,
                  const SAnnotSelector* base_sel = 0);


NCBI_XOBJUTIL_EXPORT
CMappedFeat
GetBestCdsForMrna(const CMappedFeat& mrna_feat,
                  CFeatTree* feat_tree = 0,
                  const SAnnotSelector* base_sel = 0);


NCBI_XOBJUTIL_EXPORT
void GetMrnasForGene(const CMappedFeat& gene_feat,
                     list< CMappedFeat >& mrna_feats,
                     CFeatTree* feat_tree = 0,
                     const SAnnotSelector* base_sel = 0);


NCBI_XOBJUTIL_EXPORT
void GetCdssForGene(const CMappedFeat& gene_feat,
                    list< CMappedFeat >& cds_feats,
                    CFeatTree* feat_tree = 0,
                    const SAnnotSelector* base_sel = 0);


NCBI_XOBJUTIL_EXPORT
CMappedFeat
GetBestGeneForFeat(const CMappedFeat& feat,
                   CFeatTree* feat_tree = 0,
                   const SAnnotSelector* base_sel = 0,
                   CFeatTree::EBestGeneType lookup_type = CFeatTree::eBestGene_TreeOnly);


NCBI_XOBJUTIL_EXPORT
CMappedFeat
GetBestParentForFeat(const CMappedFeat& feat,
                     CSeqFeatData::ESubtype parent_subtype,
                     CFeatTree* feat_tree = 0,
                     const SAnnotSelector* base_sel = 0);


NCBI_XOBJUTIL_EXPORT
CMappedFeat
GetBestOverlappingFeat(const CMappedFeat& feat,
                       CSeqFeatData::ESubtype need_subtype,
                       sequence::EOverlapType overlap_type,
                       CFeatTree* feat_tree = 0,
                       const SAnnotSelector* base_sel = 0);


/// Create CSeq_loc_Mapper from a feature, check for special cases
/// like exceptions in CDS features. Return null on error.
NCBI_XOBJUTIL_EXPORT
CRef<CSeq_loc_Mapper>
CreateSeqLocMapperFromFeat(const CSeq_feat& feat,
                           CSeq_loc_Mapper::EFeatMapDirection dir,
                           CScope* scope = 0);


NCBI_XOBJUTIL_EXPORT
void ClearFeatureIds(const CSeq_entry_EditHandle& entry);
NCBI_XOBJUTIL_EXPORT
void ClearFeatureIds(const CSeq_annot_EditHandle& annot);
NCBI_XOBJUTIL_EXPORT
void ReassignFeatureIds(const CSeq_entry_EditHandle& entry);
NCBI_XOBJUTIL_EXPORT
void ReassignFeatureIds(const CSeq_annot_EditHandle& annot);

typedef enum {
    eLocationInFrame_InFrame = 0,
    eLocationInFrame_BadStart,
    eLocationInFrame_BadStop,
    eLocationInFrame_BadStartAndStop,
    eLocationInFrame_NotIn
} ELocationInFrame;

/// Determines whether location loc is in frame with coding region cds
/// @param cds coding region feature [in]
/// @param loc location to compare to coding region
/// @return enumerated return value indicates frame is good or either endpoint is bad
ELocationInFrame NCBI_XOBJUTIL_EXPORT IsLocationInFrame (const CSeq_feat_Handle& cds, const CSeq_loc& loc);


END_SCOPE(feature)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* FEAT__HPP */
