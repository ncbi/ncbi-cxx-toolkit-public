#ifndef OBJECTS_OBJMGR___SEQMATCH_INFO__HPP
#define OBJECTS_OBJMGR___SEQMATCH_INFO__HPP

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
*   CSeqMatch_Info -- used internally by CScope and CDataSource
*
*/


#include <corelib/ncbiobj.hpp>
#include <objects/objmgr/seq_id_handle.hpp>
#include <objects/objmgr/tse_info.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CDataSource;


////////////////////////////////////////////////////////////////////
//
//  CSeqMatch_Info::
//
//    Information about a sequence, matching to a seq-id
//


class NCBI_XOBJMGR_EXPORT CSeqMatch_Info {
public:
    CSeqMatch_Info(void);
    CSeqMatch_Info(const CSeq_id_Handle& h, CTSE_Info& tse, CDataSource& ds);
    CSeqMatch_Info(const CSeqMatch_Info& info);
    
    CSeqMatch_Info& operator= (const CSeqMatch_Info& info);
    // Return true if "info" is better than "this"
    bool operator< (const CSeqMatch_Info& info) const;
    operator bool (void);
    bool operator! (void);

    CSeq_id_Handle    m_Handle;     // best id handle, matching the request
    CRef<CTSE_Info>   m_TSE;        // TSE, containing the best match
    CDataSource      *m_DataSource; // Data source, containing the match
};



END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.7  2002/12/26 20:41:22  dicuccio
* Added Win32 export specifier.  Converted predeclaration (CTSE_Info) into
* #include for tse_info.hpp.
*
* Revision 1.6  2002/12/26 16:39:22  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.5  2002/07/08 20:50:56  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.4  2002/06/04 17:18:32  kimelman
* memory cleanup :  new/delete/Cref rearrangements
*
* Revision 1.3  2002/05/06 03:30:36  vakatov
* OM/OM1 renaming
*
* Revision 1.2  2002/02/25 21:05:27  grichenk
* Removed seq-data references caching. Increased MT-safety. Fixed typos.
*
* Revision 1.1  2002/02/21 19:21:02  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#endif  /* OBJECTS_OBJMGR___SEQMATCH_INFO__HPP */
