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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  2002/03/04 15:07:48  grichenk
* Added "bioseq" argument to CAnnotTypes_CI constructor to iterate
* annotations from a single TSE.
*
* Revision 1.3  2002/02/21 19:27:05  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.2  2002/01/16 16:25:55  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:19  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#include <objects/objmgr1/graph_ci.hpp>
#include "annot_object.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CGraph_CI::CGraph_CI(void)
{
    return;
}


CGraph_CI::CGraph_CI(CScope& scope,
                     const CSeq_loc& loc, CBioseq_Handle* bioseq)
    : CAnnotTypes_CI(scope, loc,
      SAnnotSelector(CSeq_annot::C_Data::e_Graph), bioseq)
{
    return;
}


CGraph_CI::CGraph_CI(const CGraph_CI& iter)
    : CAnnotTypes_CI(iter)
{
    return;
}


CGraph_CI::~CGraph_CI(void)
{
    return;
}


CGraph_CI& CGraph_CI::operator= (const CGraph_CI& iter)
{
    CAnnotTypes_CI::operator=(iter);
    return *this;
}


CGraph_CI& CGraph_CI::operator++ (void)
{
    Walk();
    return *this;
}


CGraph_CI& CGraph_CI::operator++ (int)
{
    Walk();
    return *this;
}


CGraph_CI::operator bool (void) const
{
    return IsValid();
}


const CSeq_graph& CGraph_CI::operator* (void) const
{
    CAnnotObject* annot = Get();
    _ASSERT(annot  &&  annot->IsGraph());
    return annot->GetGraph();
}


const CSeq_graph* CGraph_CI::operator-> (void) const
{
    CAnnotObject* annot = Get();
    _ASSERT(annot  &&  annot->IsGraph());
    return &annot->GetGraph();
}

END_SCOPE(objects)
END_NCBI_SCOPE
