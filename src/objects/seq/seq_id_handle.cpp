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

#include <ncbi_pch.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbiatomic.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objects/seq/seq_id_mapper.hpp>
#include <serial/typeinfo.hpp>
#include "seq_id_tree.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_Info
//

//#define NCBI_SLOW_ATOMIC_SWAP
#ifdef NCBI_SLOW_ATOMIC_SWAP
DEFINE_STATIC_FAST_MUTEX(sx_GetSeqIdMutex);
#endif


CSeq_id_Info::CSeq_id_Info(CSeq_id::E_Choice type,
                           CSeq_id_Mapper* mapper)
    : m_Seq_id_Type(type),
      m_Mapper(mapper)
{
    _ASSERT(mapper);
    m_LockCounter.Set(0);
}


CSeq_id_Info::CSeq_id_Info(const CConstRef<CSeq_id>& seq_id,
                           CSeq_id_Mapper* mapper)
    : m_Seq_id_Type(seq_id->Which()),
      m_Seq_id(seq_id),
      m_Mapper(mapper)
{
    _ASSERT(mapper);
    m_LockCounter.Set(0);
}


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


CSeq_id_Info::~CSeq_id_Info(void)
{
    _ASSERT(m_LockCounter.Get() == 0);
}


CSeq_id_Which_Tree& CSeq_id_Info::GetTree(void) const
{
    return GetMapper().x_GetTree(GetType());
}



void CSeq_id_Info::x_RemoveLastLock(void) const
{
    GetTree().DropInfo(this);
}


////////////////////////////////////////////////////////////////////
//
//  CSeq_id_Handle::
//


CConstRef<CSeq_id> CSeq_id_Handle::GetSeqId(void) const
{
    _ASSERT(m_Info);
    CConstRef<CSeq_id> ret;
    if ( IsGi() ) {
        ret = m_Info->GetGiSeqId(GetGi());
    }
    else {
        ret = m_Info->GetSeqId();
    }
    return ret;
}


CConstRef<CSeq_id> CSeq_id_Handle::GetSeqIdOrNull(void) const
{
    CConstRef<CSeq_id> ret;
    if ( IsGi() ) {
        _ASSERT(m_Info);
        ret = m_Info->GetGiSeqId(GetGi());
    }
    else if ( m_Info ) {
        ret = m_Info->GetSeqId();
    }
    return ret;
}


CSeq_id_Handle CSeq_id_Handle::GetGiHandle(int gi)
{
    return CSeq_id_Mapper::GetInstance()->GetGiHandle(gi);
}


CSeq_id_Handle CSeq_id_Handle::GetHandle(const CSeq_id& id)
{
    return CSeq_id_Mapper::GetInstance()->GetHandle(id);
}


bool CSeq_id_Handle::HaveMatchingHandles(void) const
{
    return GetMapper().HaveMatchingHandles(*this);
}


bool CSeq_id_Handle::HaveReverseMatch(void) const
{
    return GetMapper().HaveReverseMatch(*this);
}


void CSeq_id_Handle::GetMatchingHandles(TMatches& matches) const
{
    GetMapper().GetMatchingHandles(*this, matches);
}


void CSeq_id_Handle::GetReverseMatchingHandles(TMatches& matches) const
{
    GetMapper().GetReverseMatchingHandles(*this, matches);
}


bool CSeq_id_Handle::IsBetter(const CSeq_id_Handle& h) const
{
    return GetMapper().x_IsBetter(*this, h);
}


bool CSeq_id_Handle::MatchesTo(const CSeq_id_Handle& h) const
{
    return GetMapper().x_Match(*this, h);
}


bool CSeq_id_Handle::operator==(const CSeq_id& id) const
{
    if ( IsGi() ) {
        return id.IsGi() && id.GetGi() == GetGi();
    }
    return *this == GetMapper().GetHandle(id);
}


string CSeq_id_Handle::AsString() const
{
    CNcbiOstrstream os;
    if ( IsGi() ) {
        os << "gi|" << GetGi();
    }
    else if ( m_Info ) {
        m_Info->GetSeqId()->WriteAsFasta(os);
    }
    else {
        os << "unknown";
    }
    return CNcbiOstrstreamToString(os);
}


unsigned CSeq_id_Handle::GetHash(void) const
{
    unsigned hash = m_Gi;
    if ( !hash ) {
        hash = unsigned((unsigned long)(m_Info.GetPointerOrNull())>>3);
    }
    return hash;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.30  2005/06/20 17:28:20  vasilche
* Use CRef's locker for CSeq_id_Info locking.
* Removed obsolete registering functions.
* Changed sorting order of CSeq_id_Handle to put gi's first.
*
* Revision 1.29  2005/01/12 15:00:57  vasilche
* Fixed the way to get pointer in GetHash().
*
* Revision 1.28  2004/12/22 15:56:01  vasilche
* Added helper functions to avoid retrieval of CSeq_id_Mapper.
*
* Revision 1.27  2004/09/30 18:41:04  vasilche
* Added CSeq_id_Handle::GetMapper() and MatchesTo().
*
* Revision 1.26  2004/07/12 15:05:32  grichenk
* Moved seq-id mapper from xobjmgr to seq library
*
* Revision 1.25  2004/06/17 18:28:38  vasilche
* Fixed null pointer exception in GI CSeq_id_Handle.
*
* Revision 1.24  2004/06/16 19:21:56  grichenk
* Fixed locking of CSeq_id_Info
*
* Revision 1.23  2004/06/14 13:57:09  grichenk
* CSeq_id_Info locks CSeq_id_Mapper with a CRef
*
* Revision 1.22  2004/06/10 16:21:27  grichenk
* Changed CSeq_id_Mapper singleton type to pointer, GetSeq_id_Mapper
* returns CRef<> which is locked by CObjectManager.
*
* Revision 1.21  2004/05/21 21:42:13  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.20  2004/03/24 18:30:30  vasilche
* Fixed edit API.
* Every *_Info object has its own shallow copy of original object.
*
* Revision 1.19  2004/02/19 17:25:34  vasilche
* Use CRef<> to safely hold pointer to CSeq_id_Info.
* CSeq_id_Info holds pointer to owner CSeq_id_Which_Tree.
* Reduce number of calls to CSeq_id_Handle.GetSeqId().
*
* Revision 1.18  2004/01/22 20:10:41  vasilche
* 1. Splitted ID2 specs to two parts.
* ID2 now specifies only protocol.
* Specification of ID2 split data is moved to seqsplit ASN module.
* For now they are still reside in one resulting library as before - libid2.
* As the result split specific headers are now in objects/seqsplit.
* 2. Moved ID2 and ID1 specific code out of object manager.
* Protocol is processed by corresponding readers.
* ID2 split parsing is processed by ncbi_xreader library - used by all readers.
* 3. Updated OBJMGR_LIBS correspondingly.
*
* Revision 1.17  2004/01/07 20:42:01  grichenk
* Fixed matching of accession to accession.version
*
* Revision 1.16  2003/11/26 17:56:00  vasilche
* Implemented ID2 split in ID1 cache.
* Fixed loading of splitted annotations.
*
* Revision 1.15  2003/10/07 13:43:23  vasilche
* Added proper handling of named Seq-annots.
* Added feature search from named Seq-annots.
* Added configurable adaptive annotation search (default: gene, cds, mrna).
* Fixed selection of blobs for loading from GenBank.
* Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
* Fixed leaked split chunks annotation stubs.
* Moved some classes definitions in separate *.cpp files.
*
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
