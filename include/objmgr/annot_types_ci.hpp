#ifndef ANNOT_TYPES_CI__HPP
#define ANNOT_TYPES_CI__HPP

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
* Revision 1.3  2002/02/07 21:27:33  grichenk
* Redesigned CDataSource indexing: seq-id handle -> TSE -> seq/annot
*
* Revision 1.2  2002/01/16 16:26:35  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:03:59  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/


#include <objects/objmgr1/bioseq_handle.hpp>
#include <objects/objmgr1/annot_ci.hpp>
#include <set>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CScope;
class CAnnot_CI;
class CTSE_Info;


// Base class for specific annotation iterators
class CAnnotTypes_CI
{
public:
    CAnnotTypes_CI(void);
    CAnnotTypes_CI(CScope& scope,
                   const CSeq_loc& loc,
                   SAnnotSelector selector);
    CAnnotTypes_CI(const CAnnotTypes_CI& it);
    virtual ~CAnnotTypes_CI(void);

    CAnnotTypes_CI& operator= (const CAnnotTypes_CI& it);

    typedef set< CRef<CTSE_Info> > TTSESet;

protected:
    // Check if a datasource and an annotation are selected.
    bool IsValid(void) const;
    // Move to the next valid position
    void Walk(void);
    // Return current annotation
    CAnnotObject* Get(void) const;

private:
    TTSESet                   m_Entries;
    TTSESet::const_iterator   m_CurrentTSE;
    SAnnotSelector            m_Selector;
    auto_ptr<CHandleRangeMap> m_Location;
    CAnnot_CI                 m_CurrentAnnot;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // ANNOT_TYPES_CI__HPP
