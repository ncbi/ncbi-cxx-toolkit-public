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
* Author: Aleksey Grichenko
*
* File Description:
*   Object manager iterators
*
*/

#include <objects/objmgr/align_ci.hpp>

#include "annot_object.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)



CAlign_CI::CAlign_CI(CScope& scope,
                     const CSeq_loc& loc,
                     EResolveMethod resolve)
    : CAnnotTypes_CI(scope, loc,
          SAnnotSelector(CSeq_annot::C_Data::e_Align),
          resolve)
{
    return;
}


CAlign_CI::CAlign_CI(CBioseq_Handle& bioseq, TSeqPos start, TSeqPos stop,
                     EResolveMethod resolve)
    : CAnnotTypes_CI(bioseq, start, stop,
          SAnnotSelector(CSeq_annot::C_Data::e_Align),
          resolve)
{
    return;
}


CAlign_CI::~CAlign_CI(void)
{
    return;
}


const CSeq_align& CAlign_CI::operator* (void) const
{
    const CAnnotObject* annot = Get();
    _ASSERT(annot  &&  annot->IsAlign());
    return annot->GetAlign();
}


const CSeq_align* CAlign_CI::operator-> (void) const
{
    const CAnnotObject* annot = Get();
    _ASSERT(annot  &&  annot->IsAlign());
    return &annot->GetAlign();
}

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.10  2002/11/04 21:29:11  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.9  2002/07/08 20:51:00  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.8  2002/05/06 03:28:46  vakatov
* OM/OM1 renaming
*
* Revision 1.7  2002/05/03 21:28:08  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.6  2002/04/05 21:26:19  grichenk
* Enabled iteration over annotations defined on segments of a
* delta-sequence.
*
* Revision 1.5  2002/03/05 16:08:13  grichenk
* Moved TSE-restriction to new constructors
*
* Revision 1.4  2002/03/04 15:07:47  grichenk
* Added "bioseq" argument to CAnnotTypes_CI constructor to iterate
* annotations from a single TSE.
*
* Revision 1.3  2002/02/21 19:27:04  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.2  2002/01/16 16:25:56  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:15  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/
