#ifndef OBJECTS_OBJMGR___TSE_INFO__HPP
#define OBJECTS_OBJMGR___TSE_INFO__HPP

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
*   TSE info -- entry for data source seq-id to TSE map
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2002/02/07 21:25:06  grichenk
* Initial revision
*
*
* ===========================================================================
*/


#include <corelib/ncbiobj.hpp>

#include <objects/objmgr1/seq_id_handle.hpp>
//#include <objects/objmgr1/annot_ci.hpp>
#include "bioseq_info.hpp"

#include <map>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CTSE_Info::
//
//    General information and indexes for top level seq-entries
//


// forward declaration
class CSeq_entry;
class CBioseq;
class CAnnotObject;


class CTSE_Info : public CObject
{
public:
    // 'ctors
    CTSE_Info(void);
    virtual ~CTSE_Info(void);
//### Add copy constructor and operator!!!

    typedef map<CSeq_id_Handle, CRef<CBioseq_Info> >                 TBioseqMap;
    typedef CRange<int>                                              TRange;
    typedef CRangeMultimap<CRef<CAnnotObject>,TRange::position_type> TRangeMap;
    typedef map<CSeq_id_Handle, TRangeMap>                           TAnnotMap;

    // Reference to the TSE
    CRef<CSeq_entry> m_TSE;
    // Dead seq-entry flag
    bool m_Dead;
    TBioseqMap m_BioseqMap;
    TAnnotMap  m_AnnotMap;
};



/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJECTS_OBJMGR___TSE_INFO__HPP */
