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
* Authors:
*           Eugene Vasilchenko
*
* File Description:
*           Structures used by CScope
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/impl/scope_info.hpp>

#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/synonyms.hpp>
#include <objmgr/impl/data_source.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////
// CSynonymsSet
/////////////////////////////////////////////////////////////////////////////

CSynonymsSet::CSynonymsSet(void)
{
}


CSynonymsSet::~CSynonymsSet(void)
{
}


CSeq_id_Handle CSynonymsSet::GetSeq_id_Handle(const const_iterator& iter)
{
    return (*iter)->first;
}


CBioseq_Handle CSynonymsSet::GetBioseqHandle(const const_iterator& iter)
{
    return CBioseq_Handle((*iter)->first,
                          (*iter)->second.m_Bioseq_Info.GetPointer());
}


bool CSynonymsSet::ContainsSynonym(const CSeq_id_Handle& id) const
{
   ITERATE ( TIdSet, iter, m_IdSet ) {
        if ( (*iter)->first == id ) {
            return true;
        }
    }
    return false;
}


void CSynonymsSet::AddSynonym(const value_type& syn)
{
    _ASSERT(!ContainsSynonym(syn->first));
    m_IdSet.push_back(syn);
}


/////////////////////////////////////////////////////////////////////////////
// CDataSource_ScopeInfo
/////////////////////////////////////////////////////////////////////////////

CDataSource_ScopeInfo::CDataSource_ScopeInfo(CDataSource& ds)
    : m_DataSource(&ds)
{
}


CDataSource_ScopeInfo::~CDataSource_ScopeInfo(void)
{
}


const CDataSource_ScopeInfo::TTSE_LockSet&
CDataSource_ScopeInfo::GetTSESet(void) const
{
    return m_TSE_LockSet;
}


CDataLoader* CDataSource_ScopeInfo::GetDataLoader(void)
{
    return GetDataSource().GetDataLoader();
}


void CDataSource_ScopeInfo::AddTSE(const CTSE_Info& tse)
{
    m_TSE_LockSet.insert(TTSE_Lock(&tse));
}


void CDataSource_ScopeInfo::Reset(void)
{
    m_TSE_LockSet.clear();
}


/////////////////////////////////////////////////////////////////////////////
// CBioseq_ScopeInfo
/////////////////////////////////////////////////////////////////////////////


CBioseq_ScopeInfo::~CBioseq_ScopeInfo(void)
{
}


CBioseq_ScopeInfo::CBioseq_ScopeInfo(TScopeInfo* scope_info)
    : m_ScopeInfo(scope_info)
{
}


CBioseq_ScopeInfo::CBioseq_ScopeInfo(TScopeInfo* scope_info,
                                     const CConstRef<CBioseq_Info>& bioseq)
    : m_ScopeInfo(scope_info),
      m_Bioseq_Info(bioseq),
      m_TSE_Lock(&bioseq->GetTSE_Info())
{
}


const CTSE_Info& CBioseq_ScopeInfo::GetTSE_Info(void) const
{
    return GetBioseq_Info().GetTSE_Info();
}


CDataSource& CBioseq_ScopeInfo::GetDataSource(void) const
{
    return GetBioseq_Info().GetDataSource();
}


CScope& CBioseq_ScopeInfo::GetScope(void) const
{
    CheckScope();
    return *m_ScopeInfo->second.m_Scope;
}


/////////////////////////////////////////////////////////////////////////////
// SSeq_id_ScopeInfo
/////////////////////////////////////////////////////////////////////////////

SSeq_id_ScopeInfo::SSeq_id_ScopeInfo(CScope* scope)
    : m_Scope(scope)
{
}

SSeq_id_ScopeInfo::~SSeq_id_ScopeInfo(void)
{
}

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.7  2004/05/21 21:42:12  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.6  2003/11/17 16:03:13  grichenk
* Throw exception in CBioseq_Handle if the parent scope has been reset
*
* Revision 1.5  2003/09/30 16:22:03  vasilche
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
* Revision 1.4  2003/06/19 19:48:16  vasilche
* Removed unnecessary copy constructor of SSeq_id_ScopeInfo.
*
* Revision 1.3  2003/06/19 19:31:23  vasilche
* Added missing CBioseq_ScopeInfo destructor.
*
* Revision 1.2  2003/06/19 19:08:55  vasilche
* Added explicit constructor/destructor.
*
* Revision 1.1  2003/06/19 18:23:46  vasilche
* Added several CXxx_ScopeInfo classes for CScope related information.
* CBioseq_Handle now uses reference to CBioseq_ScopeInfo.
* Some fine tuning of locking in CScope.
*
*
* ===========================================================================
*/
