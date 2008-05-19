#ifndef FEAT_CI__HPP
#define FEAT_CI__HPP

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
*   Object manager iterators
*
*/

#include <corelib/ncbistd.hpp>
#include <objmgr/annot_types_ci.hpp>
#include <objmgr/seq_annot_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objmgr/seq_feat_handle.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/** @addtogroup ObjectManagerIterators
 *
 * @{
 */


/////////////////////////////////////////////////////////////////////////////
///
///  CMappedFeat --
///
///  Mapped CSeq_feat class returned from the feature iterator

class NCBI_XOBJMGR_EXPORT CMappedFeat
{
public:
    CMappedFeat(void);
    CMappedFeat(const CMappedFeat& feat);
    CMappedFeat& operator=(const CMappedFeat& feat);
    ~CMappedFeat(void);

    /// Get original feature with unmapped location/product
    const CSeq_feat& GetOriginalFeature(void) const;

    /// Get original feature handle
    const CSeq_feat_Handle& GetSeq_feat_Handle(void) const
        { return m_OriginalFeat; }

    /// Get feature type
    CSeqFeatData::E_Choice GetFeatType(void) const
        { return m_OriginalFeat.GetFeatType(); }
    /// Get feature subtype
    CSeqFeatData::ESubtype GetFeatSubtype(void) const
        { return m_OriginalFeat.GetFeatSubtype(); }

    /// Fast way to check if mapped feature is different from the original one
    bool IsMapped(void) const
        { return m_MappingInfoPtr->IsMapped(); }

    /// Feature mapped to the master sequence.
    /// WARNING! The function is rather slow and should be used with care.
    const CSeq_feat& GetMappedFeature(void) const;

    bool IsSetId(void) const
        { return GetOriginalFeature().IsSetId(); }
    const CSeq_feat::TId& GetId(void) const
        { return GetOriginalFeature().GetId(); }

    const CSeqFeatData& GetData(void) const
        { return GetOriginalFeature().GetData(); }

    bool IsSetPartial(void) const
        { return m_MappingInfoPtr->IsPartial(); }
    bool GetPartial(void) const
        { return m_MappingInfoPtr->IsPartial(); }

    bool IsSetExcept(void) const
        { return GetOriginalFeature().IsSetExcept(); }
    bool GetExcept(void) const
        { return GetOriginalFeature().GetExcept(); }

    bool IsSetComment(void) const
        { return GetOriginalFeature().IsSetComment(); }
    const string& GetComment(void) const
        { return GetOriginalFeature().GetComment(); }

    bool IsSetProduct(void) const
        { return GetOriginalFeature().IsSetProduct(); }
    const CSeq_loc& GetProduct(void) const
        {
            return m_MappingInfoPtr->IsMappedProduct()?
                GetMappedLocation(): GetOriginalFeature().GetProduct();
        }

    const CSeq_loc& GetLocation(void) const
        {
            return m_MappingInfoPtr->IsMappedLocation()?
                GetMappedLocation(): GetOriginalFeature().GetLocation();
        }
    CRange<TSeqPos> GetTotalRange(void) const
        {
            return m_MappingInfoPtr->IsMappedLocation()?
                m_MappingInfoPtr->GetTotalRange():
                GetOriginalFeature().GetLocation().GetTotalRange();
        }

    bool IsSetQual(void) const
        { return GetOriginalFeature().IsSetQual(); }
    const CSeq_feat::TQual& GetQual(void) const
        { return GetOriginalFeature().GetQual(); }

    bool IsSetTitle(void) const
        { return GetOriginalFeature().IsSetTitle(); }
    const string& GetTitle(void) const
        { return GetOriginalFeature().GetTitle(); }

    bool IsSetExt(void) const
        { return GetOriginalFeature().IsSetExt(); }
    const CUser_object& GetExt(void) const
        { return GetOriginalFeature().GetExt(); }

    bool IsSetCit(void) const
        { return GetOriginalFeature().IsSetCit(); }
    const CPub_set& GetCit(void) const
        { return GetOriginalFeature().GetCit(); }

    bool IsSetExp_ev(void) const
        { return GetOriginalFeature().IsSetExp_ev(); }
    CSeq_feat::EExp_ev GetExp_ev(void) const
        { return GetOriginalFeature().GetExp_ev(); }

    bool IsSetXref(void) const
        { return GetOriginalFeature().IsSetXref(); }
    const CSeq_feat::TXref& GetXref(void) const
        { return GetOriginalFeature().GetXref(); }

    bool IsSetDbxref(void) const
        { return GetOriginalFeature().IsSetDbxref(); }
    const CSeq_feat::TDbxref& GetDbxref(void) const
        { return GetOriginalFeature().GetDbxref(); }

    bool IsSetPseudo(void) const
        { return GetOriginalFeature().IsSetPseudo(); }
    bool GetPseudo(void) const
        { return GetOriginalFeature().GetPseudo(); }

    bool IsSetExcept_text(void) const
        { return GetOriginalFeature().IsSetExcept_text(); }
    const string& GetExcept_text(void) const
        { return GetOriginalFeature().GetExcept_text(); }

    CSeq_annot_Handle GetAnnot(void) const;

private:
    friend class CFeat_CI;
    friend class CAnnot_CI;

    typedef CAnnot_Collector::TAnnotSet TAnnotSet;
    typedef TAnnotSet::const_iterator   TIterator;

    CMappedFeat& Set(CAnnot_Collector& collector,
                     const TIterator& annot);
    void Reset(void);

    const CSeq_loc& GetMappedLocation(void) const;

    CSeq_feat_Handle             m_OriginalFeat;
    // Pointer is used with annot collector to avoid copying of the
    // mapping info. The structure is copied only when the whole
    // mapped feat is copied.
    CAnnotMapping_Info*          m_MappingInfoPtr;
    CAnnotMapping_Info           m_MappingInfoObj;

    // CMappedFeat does not re-use objects
    mutable CCreatedFeat_Ref     m_MappedFeat;
    // Original feature is not locked by handle, lock here.
    mutable CConstRef<CSeq_feat> m_OriginalSeq_feat_Lock;
};


class CSeq_annot_Handle;


/////////////////////////////////////////////////////////////////////////////
///
///  CFeat_CI --
///
///  Enumerate CSeq_feat objects related to a bioseq, seq-loc,
///  or contained in a particular seq-entry or seq-annot 
///  regardless of the referenced locations.

class NCBI_XOBJMGR_EXPORT CFeat_CI : public CAnnotTypes_CI
{
public:
    CFeat_CI(void);

    /// Search features on the whole bioseq
    CFeat_CI(const CBioseq_Handle& bioseq);

    /// Search features on the whole bioseq
    ///
    /// @sa
    ///   SAnnotSelector
    CFeat_CI(const CBioseq_Handle& bioseq,
             const SAnnotSelector& sel);

    /// Search features on part of the bioseq
    CFeat_CI(const CBioseq_Handle& bioseq,
             const CRange<TSeqPos>& range,
             ENa_strand strand = eNa_strand_unknown);

    /// Search features on part of the bioseq
    CFeat_CI(const CBioseq_Handle& bioseq,
             const CRange<TSeqPos>& range,
             const SAnnotSelector& sel);

    /// Search features on part of the bioseq
    CFeat_CI(const CBioseq_Handle& bioseq,
             const CRange<TSeqPos>& range,
             ENa_strand strand,
             const SAnnotSelector& sel);

    /// Search features related to the location
    CFeat_CI(CScope& scope,
             const CSeq_loc& loc);

    /// Search features related to the location
    ///
    /// @sa
    ///   SAnnotSelector
    CFeat_CI(CScope& scope,
             const CSeq_loc& loc,
             const SAnnotSelector& sel);

    /// Iterate all features from the seq-annot regardless of their location
    CFeat_CI(const CSeq_annot_Handle& annot);

    /// Iterate all features from the seq-annot regardless of their location
    ///
    /// @sa
    ///   SAnnotSelector
    CFeat_CI(const CSeq_annot_Handle& annot,
             const SAnnotSelector& sel);

    /// Iterate all features from the seq-entry regardless of their location
    CFeat_CI(const CSeq_entry_Handle& entry);

    /// Iterate all features from the seq-entry regardless of their location
    ///
    /// @sa
    ///   SAnnotSelector
    CFeat_CI(const CSeq_entry_Handle& entry,
             const SAnnotSelector& sel);

    CFeat_CI(const CFeat_CI& iter);
    virtual ~CFeat_CI(void);
    CFeat_CI& operator= (const CFeat_CI& iter);

    /// Move to the next object in iterated sequence
    CFeat_CI& operator++(void);

    /// Move to the pervious object in iterated sequence
    CFeat_CI& operator--(void);

    /// Check if iterator points to an object
    DECLARE_OPERATOR_BOOL(IsValid());

    void Update(void);
    void Rewind(void);

    const CMappedFeat& operator* (void) const;
    const CMappedFeat* operator-> (void) const;

private:
    CFeat_CI& operator++ (int);
    CFeat_CI& operator-- (int);

    CMappedFeat m_MappedFeat;// current feature object returned by operator->()
};



inline
void CFeat_CI::Update(void)
{
    if ( IsValid() ) {
        m_MappedFeat.Set(GetCollector(), GetIterator());
    }
    else {
        m_MappedFeat.Reset();
    }
}


inline
CFeat_CI& CFeat_CI::operator++ (void)
{
    Next();
    Update();
    return *this;
}


inline
CFeat_CI& CFeat_CI::operator-- (void)
{
    Prev();
    Update();
    return *this;
}


inline
void CFeat_CI::Rewind(void)
{
    CAnnotTypes_CI::Rewind();
    Update();
}


inline
const CMappedFeat& CFeat_CI::operator* (void) const
{
    return m_MappedFeat;
}


inline
const CMappedFeat* CFeat_CI::operator-> (void) const
{
    return &m_MappedFeat;
}


inline
const CSeq_feat& CMappedFeat::GetOriginalFeature(void) const
{
    if ( !m_OriginalSeq_feat_Lock ) {
        m_OriginalSeq_feat_Lock = m_OriginalFeat.GetSeq_feat();
    }
    return *m_OriginalSeq_feat_Lock;
}


inline
const CSeq_loc& CMappedFeat::GetMappedLocation(void) const
{
    return *m_MappedFeat.MakeMappedLocation(*m_MappingInfoPtr);
}


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // FEAT_CI__HPP
