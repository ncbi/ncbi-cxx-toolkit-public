#ifndef OBJECTS_OBJMGR___BIOSEQ_INFO__HPP
#define OBJECTS_OBJMGR___BIOSEQ_INFO__HPP

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
*   Bioseq info for data source
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2002/02/07 21:25:05  grichenk
* Initial revision
*
*
* ===========================================================================
*/


#include <corelib/ncbiobj.hpp>

#include <set>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CBioseq_Info::
//
//    Structure to keep bioseq's parent seq-entry along with the list
//    of seq-id synonyms for the bioseq.
//


// forward declaration
class CSeq_entry;
class CSeq_id_Handle;

class CBioseq_Info : public CObject
{
public:
    typedef set<CSeq_id_Handle> TSynonyms;

    // 'ctors
    CBioseq_Info(void);
    CBioseq_Info(CSeq_entry& entry);
    CBioseq_Info(const CBioseq_Info& info);
    virtual ~CBioseq_Info(void);

    CBioseq_Info& operator= (const CBioseq_Info& info);

    bool operator== (const CBioseq_Info& info);
    bool operator!= (const CBioseq_Info& info);
    bool operator<  (const CBioseq_Info& info);

    CRef<CSeq_entry> m_Entry;
    TSynonyms        m_Synonyms;
};



/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJECTS_OBJMGR___BIOSEQ_INFO__HPP */
