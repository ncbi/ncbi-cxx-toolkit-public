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
*    Handle to top level Seq-entry
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/tse_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/impl/scope_info.hpp>
#include <objmgr/impl/scope_impl.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CTSE_Handle::CTSE_Handle(void)
{
}


CTSE_Handle::CTSE_Handle(TObject& object)
    : m_Scope(object.GetScopeImpl().GetScope()),
      m_TSE(object)
{
}


CTSE_Handle::~CTSE_Handle(void)
{
}


CTSE_Handle& CTSE_Handle::operator=(const CTSE_Handle& tse)
{
    if ( &tse != this ) {
        Reset();
        m_Scope = tse.m_Scope;
        m_TSE = tse.m_TSE;
    }
    return *this;
}


void CTSE_Handle::Reset(void)
{
    m_TSE.Reset();
    m_Scope.Reset();
}


const CTSE_Info& CTSE_Handle::x_GetTSE_Info(void) const
{
    return *m_TSE.GetTSE_Lock();
}


CBlobIdKey CTSE_Handle::GetBlobId(void) const
{
    CBlobIdKey ret;
    if ( *this ) {
        const CTSE_Info& tse = x_GetTSE_Info();
        ret = CBlobIdKey(tse.GetDataSource().GetDataLoader(), tse.GetBlobId());
    }
    return ret;
}


bool CTSE_Handle::Blob_IsSuppressed(void) const
{
    return Blob_IsSuppressedTemp()  ||  Blob_IsSuppressedPerm();
}


bool CTSE_Handle::Blob_IsSuppressedTemp(void) const
{
    return x_GetTSE_Info().GetBlobState() &
        CBioseq_Handle::fState_suppress_temp;
}


bool CTSE_Handle::Blob_IsSuppressedPerm(void) const
{
    return (x_GetTSE_Info().GetBlobState() &
        CBioseq_Handle::fState_suppress_perm) != 0;
}


bool CTSE_Handle::Blob_IsDead(void) const
{
    return (x_GetTSE_Info().GetBlobState() &
        CBioseq_Handle::fState_dead) != 0;
}


CSeq_entry_Handle CTSE_Handle::GetTopLevelEntry(void) const
{
    return CSeq_entry_Handle(x_GetTSE_Info(), *this);
}


CBioseq_Handle CTSE_Handle::GetBioseqHandle(const CSeq_id_Handle& id) const
{
    return x_GetScopeImpl().GetBioseqHandleFromTSE(id, *this);
}


CBioseq_Handle CTSE_Handle::GetBioseqHandle(const CSeq_id& id) const
{
    return GetBioseqHandle(CSeq_id_Handle::GetHandle(id));
}


bool CTSE_Handle::AddUsedTSE(const CTSE_Handle& tse) const
{
    return x_GetScopeInfo().AddUsedTSE(tse.m_TSE);
}


END_SCOPE(objects)
END_NCBI_SCOPE
