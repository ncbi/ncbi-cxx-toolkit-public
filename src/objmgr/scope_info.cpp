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


CBioseq_ScopeInfo::CBioseq_ScopeInfo(CScope* scope)
    : m_Scope(scope)
{
}


CBioseq_ScopeInfo::CBioseq_ScopeInfo(CScope* scope,
                                     const CConstRef<CBioseq_Info>& bioseq)
    : m_Scope(scope),
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


/////////////////////////////////////////////////////////////////////////////
// CSeq_id_ScopeInfo
/////////////////////////////////////////////////////////////////////////////

/*
CSeq_id_ScopeInfo::CSeq_id_ScopeInfo(void)
{
}


CSeq_id_ScopeInfo::CSeq_id_ScopeInfo(const CBioseq_Handle& bh)
    : m_Bioseq_Handle(bh)
{
}


CSeq_id_ScopeInfo::~CSeq_id_ScopeInfo(void)
{
}


CSeq_id_ScopeInfo::ClearCacheOnNewData(void)
{
    m_Synonyms.Reset();
    m_AnnotCache.Reset();
}
*/

SSeq_id_ScopeInfo::SSeq_id_ScopeInfo(void)
{
}

SSeq_id_ScopeInfo::~SSeq_id_ScopeInfo(void)
{
}

SSeq_id_ScopeInfo::SSeq_id_ScopeInfo(const SSeq_id_ScopeInfo& info)
: m_Bioseq_Info(info.m_Bioseq_Info), m_AnnotRef_Info(info.m_AnnotRef_Info)
{
}


const SSeq_id_ScopeInfo& SSeq_id_ScopeInfo::operator=(const SSeq_id_ScopeInfo& info)
{
    m_Bioseq_Info = info.m_Bioseq_Info;
    m_AnnotRef_Info = info.m_AnnotRef_Info;
    return *this;
}

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
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
