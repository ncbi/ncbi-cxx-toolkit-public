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

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbiatomic.hpp>
#include <objmgr/seq_id_handle.hpp>
#include <objmgr/seq_id_mapper.hpp>
#include <serial/typeinfo.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Info
//

#ifdef NCBI_SLOW_ATOMIC_SWAP
DEFINE_STATIC_FAST_MUTEX(sx_GetSeqIdMutex);
#endif


CConstRef<CSeq_id> CSeq_id_Info::GetGiSeqId(int gi) const
{
    CConstRef<CSeq_id> ret;
#if defined NCBI_SLOW_ATOMIC_SWAP
    CFastMutexGuard guard(sx_GetSeqIdMutex);
    ret = m_Seq_id;
    const_cast<CSeq_id_Info*>(this)->m_Seq_id.Reset();
    if ( !ret || !ret->ReferencedOnlyOnce() ) {
        ret.Reset(new CSeq_id);
    }
    const_cast<CSeq_id_Info*>(this)->m_Seq_id = ret;
#else
    const_cast<CSeq_id_Info*>(this)->m_Seq_id.AtomicReleaseTo(ret);
    if ( !ret || !ret->ReferencedOnlyOnce() ) {
        ret.Reset(new CSeq_id);
    }
    const_cast<CSeq_id_Info*>(this)->m_Seq_id.AtomicResetFrom(ret);
#endif
    const_cast<CSeq_id&>(*ret).SetGi(gi);
    return ret;
}


////////////////////////////////////////////////////////////////////
//
//  CSeq_id_Handle::
//


CConstRef<CSeq_id> CSeq_id_Handle::GetSeqId(void) const
{
    _ASSERT(m_Info);
    CConstRef<CSeq_id> ret;
    if ( m_Gi ) {
        ret = m_Info->GetGiSeqId(m_Gi);
    }
    else {
        ret = m_Info->GetSeqId();
    }
    return ret;
}


CConstRef<CSeq_id> CSeq_id_Handle::GetSeqIdOrNull(void) const
{
    CConstRef<CSeq_id> ret;
    if ( m_Gi ) {
        _ASSERT(m_Info);
        ret = m_Info->GetGiSeqId(m_Gi);
    }
    else if ( m_Info ) {
        ret = m_Info->GetSeqId();
    }
    return ret;
}


CSeq_id_Handle CSeq_id_Handle::GetHandle(const CSeq_id& id)
{
    return CSeq_id_Mapper::GetSeq_id_Mapper().GetHandle(id);
}


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
    return GetSeqId()->Match(*handle.GetSeqId());
}


bool CSeq_id_Handle::IsBetter(const CSeq_id_Handle& h) const
{
    return CSeq_id_Mapper::GetSeq_id_Mapper().x_IsBetter(*this, h);
}


bool CSeq_id_Handle::operator==(const CSeq_id& id) const
{
    if ( m_Gi ) {
        return id.Which() == CSeq_id::e_Gi && id.GetGi() == m_Gi;
    }
    return *this == CSeq_id_Mapper::GetSeq_id_Mapper().GetHandle(id);
}


string CSeq_id_Handle::AsString() const
{
    CNcbiOstrstream os;
    if ( m_Gi != 0 ) {
        os << "gi|" << m_Gi;
    }
    else if ( m_Info ) {
        m_Info->GetSeqId()->WriteAsFasta(os);
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
* Revision 1.14  2003/09/30 16:22:03  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
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
