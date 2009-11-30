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


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// Forward declarations
class CSeq_feat;
class CScope;
class CFeat_CI;
class CTSE_Handle;
class CFeat_id;
class CGene_ref;

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

enum ELabelType {
    eType,
    eContent,
    eBoth
};

NCBI_XOBJUTIL_EXPORT
void GetLabel (const CSeq_feat&    feat, 
               string*             label, 
               feature::ELabelType label_type,
               CScope*             scope = 0);


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


class NCBI_XOBJUTIL_EXPORT CFeatTree : public CObject
{
public:
    CFeatTree(void);
    CFeatTree(CFeat_CI it);
    
    void AddFeatures(CFeat_CI it);
    void AddFeature(const CMappedFeat& feat);

    const CMappedFeat& GetMappedFeat(const CSeq_feat_Handle& feat) const;
    CMappedFeat GetParent(const CMappedFeat& feat);
    CMappedFeat GetParent(const CMappedFeat& feat,
                          CSeqFeatData::E_Choice type);
    CMappedFeat GetParent(const CMappedFeat& feat,
                          CSeqFeatData::ESubtype subtype);
    vector<CMappedFeat> GetChildren(const CMappedFeat& feat);
    void GetChildrenTo(const CMappedFeat& feat, vector<CMappedFeat>& children);

public:
    class CFeatInfo : public CObject {
    public:
        CFeatInfo(const CMappedFeat& feat);
        ~CFeatInfo(void);

        const CTSE_Handle& GetTSE(void) const;
        bool IsSetParent(void) const {
            return m_IsSetParent;
        }

        typedef CRef<CFeatInfo> TInfoRef;
        typedef vector<TInfoRef> TChildren;
        
        CMappedFeat m_Feat;
        CRange<TSeqPos> m_MasterRange;
        bool m_IsSetParent, m_IsSetChildren;
        TInfoRef m_Parent;
        TChildren m_Children;
    };
    typedef vector<CFeatInfo*> TFeatArray;
    struct SFeatSet {
        CSeqFeatData::ESubtype m_FeatType, m_ParentType;
        bool m_NeedAll, m_CollectedAll;
        TFeatArray m_All, m_New;

        SFeatSet(void)
            : m_NeedAll(false),
              m_CollectedAll(false)
            {
            }
    };
    typedef vector<SFeatSet> TParentInfoMap;

protected:
    typedef CRef<CFeatInfo> TInfoRef;
    typedef vector<TInfoRef> TChildren;

    CFeatInfo& x_GetInfo(const CSeq_feat_Handle& feat);
    CFeatInfo& x_GetInfo(const CMappedFeat& feat);
    CFeatInfo* x_FindInfo(const CSeq_feat_Handle& feat);

    void x_AddFeatId(const CFeat_id& id, CFeatInfo& info);
    void x_AddGene(const CGene_ref& ref, CFeatInfo& info);

    void x_AssignParents(void);
    void x_AssignParentsByRef(TFeatArray& features,
                              CSeqFeatData::ESubtype parent_type);
    void x_AssignParentsByOverlap(TFeatArray& features,
                                  bool by_product,
                                  const TFeatArray& parents);
    void x_CollectNeeded(TParentInfoMap& pinfo_map);

    void x_SetParent(CFeatInfo& info, CFeatInfo& parent);
    void x_SetNoParent(CFeatInfo& info);
    const TInfoRef& x_GetParent(CFeatInfo& info);
    const TChildren& x_GetChildren(CFeatInfo& info);

    typedef map<CSeq_feat_Handle, TInfoRef> TInfoMap;
    typedef CRangeMultimap<TSeqPos> TRangeMap;

    size_t m_AssignedParents;
    TInfoMap m_InfoMap;
};


/* @} */


END_SCOPE(feature)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* FEAT__HPP */
