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
* Author: Aleksey Grichenko, Michael Kimelman
*
* File Description:
*   Object manager iterators
*
*/

#include <objects/objmgr/annot_types_ci.hpp>
#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CAlign_CI : public CAnnotTypes_CI
{
public:
    CAlign_CI(void);
    // Search all TSEs in all datasources
    CAlign_CI(CScope& scope, const CSeq_loc& loc,
              CAnnot_CI::EOverlapType overlap_type =
              CAnnot_CI::eOverlap_Intervals,
              CAnnotTypes_CI::EResolveMethod resolve =
              CAnnotTypes_CI::eResolve_TSE);
    // Search only in TSE, containing the bioseq
    CAlign_CI(const CBioseq_Handle& bioseq, TSeqPos start, TSeqPos stop,
              CAnnot_CI::EOverlapType overlap_type =
              CAnnot_CI::eOverlap_Intervals,
              CAnnotTypes_CI::EResolveMethod resolve =
              CAnnotTypes_CI::eResolve_TSE,
              const CSeq_entry* entry = 0);
    CAlign_CI(const CAlign_CI& iter);
    virtual ~CAlign_CI(void);

    CAlign_CI& operator= (const CAlign_CI& iter);

    CAlign_CI& operator++ (void);
    operator bool (void) const;
    const CSeq_align& operator* (void) const;
    const CSeq_align* operator-> (void) const;
private:
    CAlign_CI operator++ (int);
};


inline
CAlign_CI::CAlign_CI(void)
{
    return;
}

inline
CAlign_CI::CAlign_CI(const CAlign_CI& iter)
    : CAnnotTypes_CI(iter)
{
    return;
}

inline
CAlign_CI& CAlign_CI::operator= (const CAlign_CI& iter)
{
    CAnnotTypes_CI::operator=(iter);
    return *this;
}

inline
CAlign_CI& CAlign_CI::operator++ (void)
{
    Walk();
    return *this;
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
