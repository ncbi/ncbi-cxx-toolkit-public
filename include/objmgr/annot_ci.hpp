#ifndef ANNOT_CI__HPP
#define ANNOT_CI__HPP

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

#include <objmgr/annot_types_ci.hpp>
#include <corelib/ncbistd.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/graph_ci.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class NCBI_XOBJMGR_EXPORT CAnnot_CI : public CAnnotTypes_CI
{
public:
    CAnnot_CI(void);
    CAnnot_CI(CScope& scope, const CSeq_loc& loc,
              SAnnotSelector sel);
    CAnnot_CI(const CBioseq_Handle& bioseq, TSeqPos start, TSeqPos stop,
              SAnnotSelector sel);
    // Search all TSEs in all datasources
    CAnnot_CI(CScope& scope, const CSeq_loc& loc,
              EOverlapType overlap_type = eOverlap_Intervals,
              EResolveMethod resolve = eResolve_TSE,
              const CSeq_entry* entry = 0);
    // Search only in TSE, containing the bioseq
    CAnnot_CI(const CBioseq_Handle& bioseq, TSeqPos start, TSeqPos stop,
              EOverlapType overlap_type = eOverlap_Intervals,
              EResolveMethod resolve = eResolve_TSE,
              const CSeq_entry* entry = 0);

    // Iterate all features from the object regardless of their location
    CAnnot_CI(const CSeq_annot_Handle& annot);
    CAnnot_CI(const CSeq_annot_Handle& annot,
              SAnnotSelector sel);
    CAnnot_CI(CScope& scope, const CSeq_entry& entry);
    CAnnot_CI(CScope& scope, const CSeq_entry& entry,
              SAnnotSelector sel);

    CAnnot_CI(const CAnnot_CI& iter);
    virtual ~CAnnot_CI(void);

    CAnnot_CI& operator= (const CAnnot_CI& iter);

    CAnnot_CI& operator++ (void);
    CAnnot_CI& operator-- (void);
    operator bool (void) const;

    bool IsFeat(void) const;
    bool IsAlign(void) const;
    bool IsGraph(void) const;

    CMappedFeat&       GetFeat(void) const;
    const CSeq_align&  GetAlign(void) const;
    CMappedGraph&      GetGraph(void) const;

private:
    CAnnot_CI operator++ (int);
    CAnnot_CI operator-- (int);

    mutable CMappedFeat m_MappedFeat;
    mutable CMappedGraph m_MappedGraph;
};


inline
CAnnot_CI::CAnnot_CI(void)
{
    return;
}

inline
CAnnot_CI::CAnnot_CI(const CAnnot_CI& iter)
    : CAnnotTypes_CI(iter)
{
    return;
}

inline
CAnnot_CI::CAnnot_CI(CScope& scope, const CSeq_loc& loc,
                     SAnnotSelector sel)
    : CAnnotTypes_CI(scope, loc,
                     sel.SetAnnotChoice(CSeq_annot::C_Data::e_not_set))
{
}


inline
CAnnot_CI::CAnnot_CI(const CBioseq_Handle& bioseq,
                     TSeqPos start, TSeqPos stop,
                     SAnnotSelector sel)
    : CAnnotTypes_CI(bioseq, start, stop,
                     sel.SetAnnotChoice(CSeq_annot::C_Data::e_not_set))
{
}


inline
CAnnot_CI::CAnnot_CI(const CSeq_annot_Handle& annot)
    : CAnnotTypes_CI(annot,
                     SAnnotSelector(CSeq_annot::C_Data::e_not_set))
{
}


inline
CAnnot_CI::CAnnot_CI(const CSeq_annot_Handle& annot,
                     SAnnotSelector sel)
    : CAnnotTypes_CI(annot,
                     sel.SetAnnotChoice(CSeq_annot::C_Data::e_not_set))
{
}


inline
CAnnot_CI::CAnnot_CI(CScope& scope, const CSeq_entry& entry)
    : CAnnotTypes_CI(scope, entry,
                     SAnnotSelector(CSeq_annot::C_Data::e_not_set))
{
}


inline
CAnnot_CI::CAnnot_CI(CScope& scope, const CSeq_entry& entry,
                     SAnnotSelector sel)
    : CAnnotTypes_CI(scope, entry,
                     sel.SetAnnotChoice(CSeq_annot::C_Data::e_not_set))
{
}


inline
CAnnot_CI& CAnnot_CI::operator= (const CAnnot_CI& iter)
{
    CAnnotTypes_CI::operator=(iter);
    m_MappedFeat = CMappedFeat();
    m_MappedGraph = CMappedGraph();
    return *this;
}

inline
CAnnot_CI& CAnnot_CI::operator++ (void)
{
    Next();
    return *this;
}

inline
CAnnot_CI& CAnnot_CI::operator-- (void)
{
    Prev();
    return *this;
}

inline
CAnnot_CI::operator bool (void) const
{
    return IsValid();
}

inline
bool CAnnot_CI::IsFeat(void) const
{
    return Get().IsFeat();
}

inline
bool CAnnot_CI::IsAlign(void) const
{
    return Get().IsAlign();
}

inline
bool CAnnot_CI::IsGraph(void) const
{
    return Get().IsGraph();
}

inline
CMappedFeat& CAnnot_CI::GetFeat(void) const
{
    _ASSERT(IsFeat());
    return m_MappedFeat.Set(Get());
}

inline
const CSeq_align& CAnnot_CI::GetAlign(void) const
{
    _ASSERT(IsAlign());
    return Get().GetAlign();
}

inline
CMappedGraph& CAnnot_CI::GetGraph(void) const
{
    _ASSERT(IsGraph());
    return m_MappedGraph.Set(Get());
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.20  2003/08/15 15:21:58  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#endif  // ANNOT_CI__HPP
