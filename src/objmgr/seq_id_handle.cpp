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
* Author: Aleksey Grichenko, Eugene Vasilchenko
*
* File Description:
*   Seq-id handle for Object Manager
*
*/

#include <objmgr/seq_id_handle.hpp>
#include <objmgr/seq_id_mapper.hpp>
#include <serial/typeinfo.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CSeq_id_Handle::
//


void CSeq_id_Handle::x_RemoveLastReference(void)
{
    CSeq_id_Mapper::GetSeq_id_Mapper().x_RemoveLastReference(m_Info);
}


bool CSeq_id_Handle::x_Match(const CSeq_id_Handle& handle) const
{
    // Different mappers -- handle can not be equal
    if ( !*this ) {
        return !handle;
    }
    if ( !handle )
        return false;
    return GetSeqId().Match(handle.GetSeqId());
}


bool CSeq_id_Handle::IsBetter(const CSeq_id_Handle& h) const
{
    return CSeq_id_Mapper::GetSeq_id_Mapper().x_IsBetter(*this, h);
}


string CSeq_id_Handle::AsString() const
{
    CNcbiOstrstream os;
    if ( *this ) {
        GetSeqId().WriteAsFasta(os);
    }
    else {
        os << "unknown";
    }
    return CNcbiOstrstreamToString(os);
}

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.13  2003/06/10 19:06:35  vasilche
* Simplified CSeq_id_Mapper and CSeq_id_Handle.
*
* Revision 1.11  2003/04/24 16:12:38  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.10  2003/03/14 19:10:41  grichenk
* + SAnnotSelector::EIdResolving; fixed operator=() for several classes
*
* Revision 1.9  2002/12/26 20:55:18  dicuccio
* Moved seq_id_mapper.hpp, tse_info.hpp, and bioseq_info.hpp -> include/ tree
*
* Revision 1.8  2002/12/26 16:39:24  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.7  2002/07/08 20:51:02  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.6  2002/05/29 21:21:13  gouriano
* added debug dump
*
* Revision 1.5  2002/05/06 03:28:47  vakatov
* OM/OM1 renaming
*
* Revision 1.4  2002/03/15 18:10:08  grichenk
* Removed CRef<CSeq_id> from CSeq_id_Handle, added
* key to seq-id map th CSeq_id_Mapper
*
* Revision 1.3  2002/02/21 19:27:06  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.2  2002/02/12 19:41:42  grichenk
* Seq-id handles lock/unlock moved to CSeq_id_Handle 'ctors.
*
* Revision 1.1  2002/01/23 21:57:22  grichenk
* Splitted id_handles.hpp
*
*
* ===========================================================================
*/
