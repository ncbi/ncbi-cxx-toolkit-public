#ifndef ALIGN_CI__HPP
#define ALIGN_CI__HPP

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
#include <objmgr/seq_align_handle.hpp>

#include <objects/seqalign/Seq_align.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class NCBI_XOBJMGR_EXPORT CAlign_CI : public CAnnotTypes_CI
{
public:
    CAlign_CI(void);
    CAlign_CI(CScope& scope, const CSeq_loc& loc,
              const SAnnotSelector& sel);
    CAlign_CI(const CBioseq_Handle& bioseq, TSeqPos start, TSeqPos stop,
              const SAnnotSelector& sel);
    // Search all TSEs in all datasources
    CAlign_CI(CScope& scope, const CSeq_loc& loc,
              SAnnotSelector::EOverlapType overlap_type
              = SAnnotSelector::eOverlap_Intervals,
              SAnnotSelector::EResolveMethod resolve
              = SAnnotSelector::eResolve_TSE);
    // Search only in TSE, containing the bioseq
    CAlign_CI(const CBioseq_Handle& bioseq, TSeqPos start, TSeqPos stop,
              SAnnotSelector::EOverlapType overlap_type
              = SAnnotSelector::eOverlap_Intervals,
              SAnnotSelector::EResolveMethod resolve
              = SAnnotSelector::eResolve_TSE);

    // Iterate all features from the object regardless of their location
    CAlign_CI(const CSeq_annot_Handle& annot);
    CAlign_CI(const CSeq_annot_Handle& annot,
              const SAnnotSelector& sel);
    CAlign_CI(const CSeq_entry_Handle& entry);
    CAlign_CI(const CSeq_entry_Handle& entry,
              const SAnnotSelector& sel);

    virtual ~CAlign_CI(void);

    CAlign_CI& operator++ (void);
    CAlign_CI& operator-- (void);
    operator bool (void) const;

    // Mapped alignment, not the original one
    const CSeq_align& operator* (void) const;
    const CSeq_align* operator-> (void) const;

    const CSeq_align& GetOriginalSeq_align(void) const;
    // Original align handle
    CSeq_align_Handle GetSeq_align_Handle(void) const;

private:
    CAlign_CI operator++ (int);
    CAlign_CI operator-- (int);

    mutable CConstRef<CSeq_align> m_MappedAlign;
};


inline
CAlign_CI::CAlign_CI(void)
{
}


inline
CAlign_CI::CAlign_CI(CScope& scope, const CSeq_loc& loc,
                     const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Align, scope, loc, sel)
{
}


inline
CAlign_CI::CAlign_CI(const CBioseq_Handle& bioseq,
                     TSeqPos start, TSeqPos stop,
                     const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Align, bioseq, start, stop, sel)
{
}


inline
CAlign_CI::CAlign_CI(const CSeq_annot_Handle& annot)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Align, annot)
{
}


inline
CAlign_CI::CAlign_CI(const CSeq_annot_Handle& annot,
                     const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Align, annot, sel)
{
}


inline
CAlign_CI::CAlign_CI(const CSeq_entry_Handle& entry)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Align, entry)
{
}


inline
CAlign_CI::CAlign_CI(const CSeq_entry_Handle& entry,
                     const SAnnotSelector& sel)
    : CAnnotTypes_CI(CSeq_annot::C_Data::e_Align, entry, sel)
{
}


inline
CAlign_CI::operator bool (void) const
{
    return IsValid();
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.29  2004/05/04 18:08:47  grichenk
* Added CSeq_feat_Handle, CSeq_align_Handle and CSeq_graph_Handle
*
* Revision 1.28  2004/04/05 15:56:13  grichenk
* Redesigned CAnnotTypes_CI: moved all data and data collecting
* functions to CAnnotDataCollector. CAnnotTypes_CI is no more
* inherited from SAnnotSelector.
*
* Revision 1.27  2004/03/16 15:47:25  vasilche
* Added CBioseq_set_Handle and set of EditHandles
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
* Revision 1.23  2004/01/29 15:44:45  vasilche
* Fixed mapped align when it's not mapped.
*
* Revision 1.22  2004/01/23 16:14:45  grichenk
* Implemented alignment mapping
*
* Revision 1.21  2003/08/04 17:02:57  grichenk
* Added constructors to iterate all annotations from a
* seq-entry or seq-annot.
*
* Revision 1.20  2003/06/02 16:01:36  dicuccio
* Rearranged include/objects/ subtree.  This includes the following shifts:
*     - include/objects/alnmgr --> include/objtools/alnmgr
*     - include/objects/cddalignview --> include/objtools/cddalignview
*     - include/objects/flat --> include/objtools/flat
*     - include/objects/objmgr/ --> include/objmgr/
*     - include/objects/util/ --> include/objmgr/util/
*     - include/objects/validator --> include/objtools/validator
*
* Revision 1.19  2003/04/24 16:12:37  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.18  2003/03/21 14:50:51  vasilche
* Added constructors of CAlign_CI and CGraph_CI taking generic
* SAnnotSelector parameters.
*
* Revision 1.17  2003/03/05 20:56:42  vasilche
* SAnnotSelector now holds all parameters of annotation iterators.
*
* Revision 1.16  2003/02/24 18:57:20  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.15  2002/12/26 20:42:55  dicuccio
* Added Win32 export specifier.  Removed unimplemented (private) operator++(int)
*
* Revision 1.14  2002/12/24 15:42:44  grichenk
* CBioseqHandle argument to annotation iterators made const
*
* Revision 1.13  2002/12/06 15:35:57  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.12  2002/12/05 19:28:29  grichenk
* Prohibited postfix operator ++()
*
* Revision 1.11  2002/07/08 20:50:55  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.10  2002/05/06 03:30:35  vakatov
* OM/OM1 renaming
*
* Revision 1.9  2002/05/03 21:28:01  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.8  2002/04/30 14:30:41  grichenk
* Added eResolve_TSE flag in CAnnot_Types_CI, made it default
*
* Revision 1.7  2002/04/17 20:57:49  grichenk
* Added full type for "resolve" argument
*
* Revision 1.6  2002/04/05 21:26:16  grichenk
* Enabled iteration over annotations defined on segments of a
* delta-sequence.
*
* Revision 1.5  2002/03/05 16:08:11  grichenk
* Moved TSE-restriction to new constructors
*
* Revision 1.4  2002/03/04 15:07:46  grichenk
* Added "bioseq" argument to CAnnotTypes_CI constructor to iterate
* annotations from a single TSE.
*
* Revision 1.3  2002/02/21 19:26:59  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.2  2002/01/16 16:26:36  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:03:59  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#endif  // ALIGN_CI__HPP
