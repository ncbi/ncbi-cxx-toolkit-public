#ifndef GRAPH_CI__HPP
#define GRAPH_CI__HPP

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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.7  2002/04/17 20:57:50  grichenk
* Added full type for "resolve" argument
*
* Revision 1.6  2002/04/05 21:26:17  grichenk
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
* Revision 1.2  2002/01/16 16:26:36  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:04:02  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#include <objects/objmgr1/annot_types_ci.hpp>
#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CGraph_CI : public CAnnotTypes_CI
{
public:
    CGraph_CI(void);
    // Search all TSEs in all datasources
    CGraph_CI(CScope& scope, const CSeq_loc& loc,
              CAnnotTypes_CI::EResolveMethod resolve =
              CAnnotTypes_CI::eResolve_None);
    // Search only in TSE, containing the bioseq
    CGraph_CI(CBioseq_Handle& bioseq, int start, int stop,
              CAnnotTypes_CI::EResolveMethod resolve =
              CAnnotTypes_CI::eResolve_None);
    CGraph_CI(const CGraph_CI& iter);
    virtual ~CGraph_CI(void);
    CGraph_CI& operator= (const CGraph_CI& iter);

    CGraph_CI& operator++ (void);
    CGraph_CI& operator++ (int);
    operator bool (void) const;
    const CSeq_graph& operator* (void) const;
    const CSeq_graph* operator-> (void) const;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // GRAPH_CI__HPP
