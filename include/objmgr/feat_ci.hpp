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
    const CFeat_id& GetId(void) const
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

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.52  2005/03/07 17:30:01  vasilche
* Added methods to get feature type and subtype
*
* Revision 1.51  2005/02/24 20:38:28  grichenk
* Optimized mapping info in CMappedFeat.
*
* Revision 1.50  2005/02/24 19:13:34  grichenk
* Redesigned CMappedFeat not to hold the whole annot collector.
*
* Revision 1.49  2005/01/24 17:09:36  vasilche
* Safe boolean operators.
*
* Revision 1.48  2005/01/06 16:41:30  grichenk
* Removed deprecated methods
*
* Revision 1.47  2004/10/29 17:47:12  grichenk
* Marked CMappedFeat::GetSeq_annot as deprecated
*
* Revision 1.46  2004/10/29 16:29:47  grichenk
* Prepared to remove deprecated methods, added new constructors.
*
* Revision 1.45  2004/10/08 14:18:34  grichenk
* Moved MakeMappedXXXX methods to CAnnotCollector,
* fixed mapped feature initialization bug.
*
* Revision 1.44  2004/10/07 13:59:35  vasilche
* Better name for member of CFeat_CI. Removed mutable.
*
* Revision 1.43  2004/09/30 23:43:32  kononenk
* Added doxygen formatting
*
* Revision 1.42  2004/09/07 14:10:53  grichenk
* Added IsMapped()
*
* Revision 1.41  2004/05/04 18:08:47  grichenk
* Added CSeq_feat_Handle, CSeq_align_Handle and CSeq_graph_Handle
*
* Revision 1.40  2004/04/07 13:20:17  grichenk
* Moved more data from iterators to CAnnot_Collector
*
* Revision 1.39  2004/04/05 15:56:13  grichenk
* Redesigned CAnnotTypes_CI: moved all data and data collecting
* functions to CAnnotDataCollector. CAnnotTypes_CI is no more
* inherited from SAnnotSelector.
*
* Revision 1.38  2004/03/16 15:47:26  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.37  2004/02/04 18:05:32  grichenk
* Added annotation filtering by set of types/subtypes.
* Renamed *Choice to *Type in SAnnotSelector.
*
* Revision 1.36  2004/01/28 20:54:34  vasilche
* Fixed mapping of annotations.
*
* Revision 1.35  2004/01/23 16:14:46  grichenk
* Implemented alignment mapping
*
* Revision 1.34  2003/11/07 16:01:26  vasilche
* Added CMappedFeat::GetSeq_annot().
*
* Revision 1.33  2003/08/27 14:28:50  vasilche
* Reduce amount of object allocations in feature iteration.
*
* Revision 1.32  2003/08/15 19:19:15  vasilche
* Fixed memory leak in string packing hooks.
* Fixed processing of 'partial' flag of features.
* Allow table packing of non-point SNP.
* Allow table packing of SNP with long alleles.
*
* Revision 1.31  2003/08/15 15:22:41  grichenk
* +CAnnot_CI
*
* Revision 1.30  2003/08/14 20:05:18  vasilche
* Simple SNP features are stored as table internally.
* They are recreated when needed using CFeat_CI.
*
* Revision 1.29  2003/08/04 17:02:57  grichenk
* Added constructors to iterate all annotations from a
* seq-entry or seq-annot.
*
* Revision 1.28  2003/07/22 21:48:02  vasilche
* Use typedef for member access.
*
* Revision 1.27  2003/06/02 16:01:36  dicuccio
* Rearranged include/objects/ subtree.  This includes the following shifts:
*     - include/objects/alnmgr --> include/objtools/alnmgr
*     - include/objects/cddalignview --> include/objtools/cddalignview
*     - include/objects/flat --> include/objtools/flat
*     - include/objects/objmgr/ --> include/objmgr/
*     - include/objects/util/ --> include/objmgr/util/
*     - include/objects/validator --> include/objtools/validator
*
* Revision 1.26  2003/04/24 16:12:37  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.25  2003/03/26 14:55:57  vasilche
* Removed redundant 'const' from methods returning 'const bool'.
*
* Revision 1.24  2003/03/05 20:56:42  vasilche
* SAnnotSelector now holds all parameters of annotation iterators.
*
* Revision 1.23  2003/02/25 14:24:19  dicuccio
* Added Win32 export specifier to CMappedFeat
*
* Revision 1.22  2003/02/24 18:57:20  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.21  2003/02/13 14:57:36  dicuccio
* Changed interface to match new 'dataool'-generated code (built-in types
* returned by value, not reference).
*
* Revision 1.20  2003/02/13 14:34:31  grichenk
* Renamed CAnnotObject -> CAnnotObject_Info
* + CSeq_annot_Info and CAnnotObject_Ref
* Moved some members of CAnnotObject to CSeq_annot_Info
* and CAnnotObject_Ref.
* Added feat/align/graph to CAnnotObject_Info map
* to CDataSource.
*
* Revision 1.19  2003/02/10 22:03:55  grichenk
* operator++(int) made private
*
* Revision 1.18  2003/02/10 15:50:44  grichenk
* + CMappedFeat, CFeat_CI resolves to CMappedFeat
*
* Revision 1.17  2002/12/26 20:50:18  dicuccio
* Added Win32 export specifier.  Removed unimplemented (private) operator++(int)
*
* Revision 1.16  2002/12/24 15:42:44  grichenk
* CBioseqHandle argument to annotation iterators made const
*
* Revision 1.15  2002/12/20 20:54:23  grichenk
* Added optional location/product switch to CFeat_CI
*
* Revision 1.14  2002/12/06 15:35:57  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.13  2002/12/05 19:28:30  grichenk
* Prohibited postfix operator ++()
*
* Revision 1.12  2002/07/08 20:50:56  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.11  2002/05/06 03:30:35  vakatov
* OM/OM1 renaming
*
* Revision 1.10  2002/05/03 21:28:01  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.9  2002/04/30 14:30:42  grichenk
* Added eResolve_TSE flag in CAnnot_Types_CI, made it default
*
* Revision 1.8  2002/04/17 20:57:50  grichenk
* Added full type for "resolve" argument
*
* Revision 1.7  2002/04/16 19:59:45  grichenk
* Updated comments
*
* Revision 1.6  2002/04/05 21:26:16  grichenk
* Enabled iteration over annotations defined on segments of a
* delta-sequence.
*
* Revision 1.5  2002/03/05 16:08:12  grichenk
* Moved TSE-restriction to new constructors
*
* Revision 1.4  2002/03/04 15:07:46  grichenk
* Added "bioseq" argument to CAnnotTypes_CI constructor to iterate
* annotations from a single TSE.
*
* Revision 1.3  2002/02/21 19:27:00  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.2  2002/01/16 16:26:35  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:04:02  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#endif  // FEAT_CI__HPP
