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
* Author: Aleksey Grichenko, Michael Kimelman
*
* File Description:
*   Object manager iterators
*
*/

#include <objects/objmgr/annot_types_ci.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CMappedFeat
{
public:
    CMappedFeat(void)
        : m_Feat(0),
          m_MappedFeat(0),
          m_Partial(false),
          m_MappedLoc(0),
          m_MappedProd(0)
        { return; }
    CMappedFeat(const CMappedFeat& feat)
        : m_Feat(feat.m_Feat),
          m_MappedFeat(feat.m_MappedFeat),
          m_Partial(feat.m_Partial),
          m_MappedLoc(feat.m_MappedLoc),
          m_MappedProd(feat.m_MappedProd)
        { return; }
    CMappedFeat& operator= (const CMappedFeat& feat)
        {
            if (this != &feat) {
                m_Feat = feat.m_Feat;
                m_MappedFeat = feat.m_MappedFeat;
                m_Partial = feat.m_Partial;
                m_MappedLoc = feat.m_MappedLoc;
                m_MappedProd = feat.m_MappedProd;
            }
            return *this;
        }

    // Original feature with unmapped location/product
    const CSeq_feat& GetOriginalFeature(void) const
        {
            return *m_Feat;
        }
    // Feature mapped to the master sequence.
    // WARNING! The function is rather slow and should be used with care.
    const CSeq_feat& GetMappedFeature(void) const
        {
            if (!m_MappedFeat) {
                if (m_MappedLoc.GetPointer()  ||  m_MappedProd.GetPointer()) {
                    CSeq_feat* tmp = new CSeq_feat;
                    m_MappedFeat = tmp;
                    tmp->Assign(*m_Feat);
                    if (m_MappedLoc)
                        tmp->SetLocation().Assign(*m_MappedLoc);
                    if (m_MappedProd)
                        tmp->SetProduct().Assign(*m_MappedProd);
                }
                else {
                    m_MappedFeat = m_Feat;
                }
            }
            return *m_MappedFeat;
        }

    bool IsSetId(void) const
        { return m_Feat->IsSetId(); }
    const CFeat_id& GetId(void) const
        { return m_Feat->GetId(); }

    const CSeqFeatData& GetData(void) const
        { return m_Feat->GetData(); }

    bool IsSetPartial(void) const
        { return m_Feat->IsSetPartial()  ||  m_Partial; }
    const bool GetPartial(void) const
        { return m_Feat->GetPartial()  ||  m_Partial; }

    bool IsSetExcept(void) const
        { return m_Feat->IsSetExcept(); }
    const bool GetExcept(void) const
        { return m_Feat->GetExcept(); }

    bool IsSetComment(void) const
        { return m_Feat->IsSetComment(); }
    const string& GetComment(void) const
        { return m_Feat->GetComment(); }

    bool IsSetProduct(void) const
        { return m_Feat->IsSetProduct(); }
    const CSeq_loc& GetProduct(void) const
        //### Check mapped location
        { return m_MappedProd ? *m_MappedProd : m_Feat->GetProduct(); }

    const CSeq_loc& GetLocation(void) const
        { return m_MappedLoc ? *m_MappedLoc : m_Feat->GetLocation(); }

    bool IsSetQual(void) const
        { return m_Feat->IsSetQual(); }
    const list< CRef< CGb_qual > >& GetQual(void) const
        { return m_Feat->GetQual(); }

    bool IsSetTitle(void) const
        { return m_Feat->IsSetTitle(); }
    const string& GetTitle(void) const
        { return m_Feat->GetTitle(); }

    bool IsSetExt(void) const
        { return m_Feat->IsSetExt(); }
    const CUser_object& GetExt(void) const
        { return m_Feat->GetExt(); }

    bool IsSetCit(void) const
        { return m_Feat->IsSetCit(); }
    const CPub_set& GetCit(void) const
        { return m_Feat->GetCit(); }

    bool IsSetExp_ev(void) const
        { return m_Feat->IsSetExp_ev(); }
    CSeq_feat::EExp_ev GetExp_ev(void) const
        { return m_Feat->GetExp_ev(); }

    bool IsSetXref(void) const
        { return m_Feat->IsSetXref(); }
    const list< CRef< CSeqFeatXref > >& GetXref(void) const
        { return m_Feat->GetXref(); }

    bool IsSetDbxref(void) const
        { return m_Feat->IsSetDbxref(); }
    const list< CRef< CDbtag > >& GetDbxref(void) const
        { return m_Feat->GetDbxref(); }

    bool IsSetPseudo(void) const
        { return m_Feat->IsSetPseudo(); }
    const bool GetPseudo(void) const
        { return m_Feat->GetPseudo(); }

    bool IsSetExcept_text(void) const
        { return m_Feat->IsSetExcept_text(); }
    const string& GetExcept_text(void) const
        { return m_Feat->GetExcept_text(); }

private:
    friend class CFeat_CI;
    CMappedFeat& Set(const CAnnotObject_Ref& annot);

    CConstRef<CSeq_feat>         m_Feat;
    mutable CConstRef<CSeq_feat> m_MappedFeat;
    bool                         m_Partial;
    CConstRef<CSeq_loc>          m_MappedLoc;
    CConstRef<CSeq_loc>          m_MappedProd;
};



class NCBI_XOBJMGR_EXPORT CFeat_CI : public CAnnotTypes_CI
{
public:
    enum EFeat_Location {
        e_Location,
        e_Product
    };
    CFeat_CI(void);
    // Search all TSEs in all datasources. By default search sequence segments
    // (for constructed sequences) only if the referenced sequence is in the
    // same TSE as the master one. Use CFeat_CI::eResolve_All flag to search
    // features on all referenced sequences or CFeat_CI::eResolve_None to
    // disable references resolving.
    CFeat_CI(CScope& scope,
             const CSeq_loc& loc,
             SAnnotSelector::TFeatChoice feat_choice,
             CAnnot_CI::EOverlapType overlap_type = CAnnot_CI::eOverlap_Intervals,
             CAnnotTypes_CI::EResolveMethod resolve =
             CAnnotTypes_CI::eResolve_TSE,
             EFeat_Location loc_type = e_Location);
    // Search only in TSE, containing the bioseq. If both start & stop are 0,
    // the whole bioseq is searched. References are resolved depending on the
    // "resolve" flag (see above).
    // If "entry" is set, search only features from the seq-entry specified
    // (but no its sub-entries or parent entry).
    CFeat_CI(const CBioseq_Handle& bioseq,
             TSeqPos start, TSeqPos stop,
             SAnnotSelector::TFeatChoice feat_choice,
             CAnnot_CI::EOverlapType overlap_type = CAnnot_CI::eOverlap_Intervals,
             EResolveMethod resolve = eResolve_TSE,
             EFeat_Location loc_type = e_Location,
             const CSeq_entry* entry = 0);
    CFeat_CI(const CFeat_CI& iter);
    virtual ~CFeat_CI(void);
    CFeat_CI& operator= (const CFeat_CI& iter);

    CFeat_CI& operator++ (void);
    operator bool (void) const;
    const CMappedFeat& operator* (void) const;
    const CMappedFeat* operator-> (void) const;

private:
    CFeat_CI& operator++ (int);

    mutable CMappedFeat m_Feat; // current feature object returned by operator->()
};



inline
CFeat_CI::CFeat_CI(void)
{
    return;
}

inline
CFeat_CI::CFeat_CI(const CFeat_CI& iter)
    : CAnnotTypes_CI(iter)
{
    return;
}

inline
CFeat_CI::~CFeat_CI(void)
{
    return;
}

inline
CFeat_CI::CFeat_CI(const CBioseq_Handle& bioseq,
                   TSeqPos start, TSeqPos stop,
                   SAnnotSelector::TFeatChoice feat_choice,
                   CAnnot_CI::EOverlapType overlap_type,
                   EResolveMethod resolve,
                   EFeat_Location loc_type,
                   const CSeq_entry* entry)
    : CAnnotTypes_CI(bioseq, start, stop,
          SAnnotSelector(CSeq_annot::C_Data::e_Ftable,
                         feat_choice,
                         loc_type == e_Product),
          overlap_type, resolve, entry)
{
    return;
}

inline
CFeat_CI::CFeat_CI(CScope& scope,
                   const CSeq_loc& loc,
                   SAnnotSelector::TFeatChoice feat_choice,
                   CAnnot_CI::EOverlapType overlap_type,
                   EResolveMethod resolve,
                   EFeat_Location loc_type)
    : CAnnotTypes_CI(scope, loc,
          SAnnotSelector(CSeq_annot::C_Data::e_Ftable,
                         feat_choice,
                         loc_type == e_Product),
          overlap_type, resolve)
{
    return;
}

inline
CFeat_CI& CFeat_CI::operator= (const CFeat_CI& iter)
{
    CAnnotTypes_CI::operator=(iter);
    return *this;
}

inline
CFeat_CI& CFeat_CI::operator++ (void)
{
    Walk();
    return *this;
}

inline
CFeat_CI::operator bool (void) const
{
    return IsValid();
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
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
