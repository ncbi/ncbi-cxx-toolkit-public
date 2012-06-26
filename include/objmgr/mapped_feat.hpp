#ifndef MAPPED_FEAT__HPP
#define MAPPED_FEAT__HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   class CMappedFeat to represent feature with mapped locations
*
*/

#include <corelib/ncbistd.hpp>
#include <objmgr/seq_feat_handle.hpp>
#include <objmgr/impl/annot_collector.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqloc/Seq_loc.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_annot_Handle;

/** @addtogroup ObjectManagerIterators
 *
 * @{
 */


/////////////////////////////////////////////////////////////////////////////
///
///  CMappedFeat --
///
///  Mapped CSeq_feat class returned from the feature iterator

class NCBI_XOBJMGR_EXPORT CMappedFeat : public CSeq_feat_Handle
{
public:
    CMappedFeat(void);
    CMappedFeat(const CSeq_feat_Handle& feat);
    CMappedFeat(const CMappedFeat& feat);
    CMappedFeat& operator=(const CMappedFeat& feat);
    ~CMappedFeat(void);

    /// Get original feature with unmapped location/product
    const CSeq_feat& GetOriginalFeature(void) const;

    /// Get original feature handle
    const CSeq_feat_Handle& GetSeq_feat_Handle(void) const
        { return *this; }

    /// Fast way to check if mapped feature is different from the original one
    bool IsMapped(void) const
        { return m_MappingInfoPtr->IsMapped(); }

    /// Feature mapped to the master sequence.
    /// WARNING! The function is rather slow and should be used with care.
    const CSeq_feat& GetMappedFeature(void) const;

    bool IsSetPartial(void) const;
    bool GetPartial(void) const;

    const CSeq_loc& GetProduct(void) const;
    const CSeq_loc& GetLocation(void) const;

    /// Get current seq-feat
    CConstRef<CSeq_feat> GetSeq_feat(void) const;

    /// Get range for mapped seq-feat's location
    TRange GetRange(void) const;
    TRange GetTotalRange(void) const
        { return GetRange(); }

    
    CSeq_id_Handle GetLocationId(void) const;
    TRange GetLocationTotalRange(void) const
        { return GetRange(); }
    CSeq_id_Handle GetProductId(void) const;
    TRange GetProductTotalRange(void) const;

private:
    friend class CFeat_CI;
    friend class CAnnot_CI;

    typedef CAnnot_Collector::TAnnotSet TAnnotSet;
    typedef TAnnotSet::const_iterator   TIterator;

    CMappedFeat& Set(CAnnot_Collector& collector,
                     const TIterator& annot);
    void Reset(void);

    CConstRef<CSeq_loc> GetMappedLocation(void) const;

    // Pointer is used with annot collector to avoid copying of the
    // mapping info. The structure is copied only when the whole
    // mapped feat is copied.
    CAnnotMapping_Info*          m_MappingInfoPtr;
    CAnnotMapping_Info           m_MappingInfoObj;

    // CMappedFeat does not re-use objects
    mutable CCreatedFeat_Ref     m_MappedFeat;
};


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE


#endif // MAPPED_FEAT__HPP
