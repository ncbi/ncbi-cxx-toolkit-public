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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2002/01/11 19:04:02  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/


#include <objects/objmgr1/annot_types_ci.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CFeat_CI : public CAnnotTypes_CI
{
public:
    CFeat_CI(void);
    CFeat_CI(TDataSources::iterator      source,
             TDataSources*               sources,
             const CSeq_loc&             loc,
             SAnnotSelector::TFeatChoice feat_choice);
    CFeat_CI(const CFeat_CI& iter);
    virtual ~CFeat_CI(void);
    CFeat_CI& operator= (const CFeat_CI& iter);

    CFeat_CI& operator++ (void);
    CFeat_CI& operator++ (int);
    operator bool (void) const;
    const CSeq_feat& operator* (void) const;
    const CSeq_feat* operator-> (void) const;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // FEAT_CI__HPP
