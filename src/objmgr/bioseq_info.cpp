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
* Revision 1.2  2002/02/21 19:27:05  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.1  2002/02/07 21:25:05  grichenk
* Initial revision
*
*
* ===========================================================================
*/


#include "bioseq_info.hpp"

//#include <corelib/ncbistd.hpp>

//#include <objects/seqset/Seq_entry.hpp>

//#include <objects/objmgr1/seq_id_handle.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CBioseq_Info::
//
//    Structure to keep bioseq's parent seq-entry along with the list
//    of seq-id synonyms for the bioseq.
//


CBioseq_Info::CBioseq_Info(void)
    : m_Entry(0)
{
    return;
}


CBioseq_Info::CBioseq_Info(CSeq_entry& entry)
    : m_Entry(&entry)
{
    return;
}


CBioseq_Info::CBioseq_Info(const CBioseq_Info& info)
{
    if ( &info != this )
        *this = info;
}


CBioseq_Info::~CBioseq_Info(void)
{
    return;
}


CBioseq_Info& CBioseq_Info::operator= (const CBioseq_Info& info)
{
    m_Entry.Reset(info.m_Entry);
    iterate ( TSynonyms, it, info.m_Synonyms ) {
        m_Synonyms.insert(*it);
    }
    return *this;
}


END_SCOPE(objects)
END_NCBI_SCOPE
