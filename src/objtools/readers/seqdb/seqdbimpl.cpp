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
 * Author:  Kevin Bealer
 *
 */

#include "seqdbimpl.hpp"
#include <iostream>

BEGIN_NCBI_SCOPE

CSeqDBImpl::CSeqDBImpl(const string & db_name_list, char prot_nucl, bool use_mmap)
    : m_DBNames      (db_name_list),
      m_Aliases      (db_name_list, prot_nucl, use_mmap),
      m_VolSet       (m_MemPool, m_Aliases.GetVolumeNames(), prot_nucl, use_mmap),
      m_RestrictFirst(0)
{
    m_Aliases.SetMasks(m_VolSet);
    
    if ( m_VolSet.HasMask() ) {
        m_OIDList.Reset( new CSeqDBOIDList(m_VolSet, use_mmap) );
    }
    
    m_RestrictLast = GetNumSeqs() - 1;
}

CSeqDBImpl::~CSeqDBImpl(void)
{
}

void CSeqDBImpl::SetOIDRange(Uint4 first, Uint4 last)
{
    m_RestrictFirst = first;
    m_RestrictLast  = last;
    
    if (m_RestrictLast >= GetNumSeqs()) {
        m_RestrictLast = GetNumSeqs() - 1;
    }
}

bool CSeqDBImpl::CheckOrFindOID(Uint4 & next_oid) const
{
    bool success = true;
    
    if (next_oid < m_RestrictFirst) {
        next_oid = m_RestrictFirst;
    }
    
    if (next_oid > m_RestrictLast) {
        success = false;
    }
    
    if (success && m_OIDList.NotEmpty()) {
        success = m_OIDList->CheckOrFindOID(next_oid);
        
        if (next_oid > m_RestrictLast) {
            success = false;
        }
    }
    
    return success;
}

Int4 CSeqDBImpl::GetSeqLength(Uint4 oid) const
{
    Uint4 vol_oid = 0;
    
    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetSeqLength(vol_oid, false);
    }
    
    return -1;
}

Int4 CSeqDBImpl::GetSeqLengthApprox(Uint4 oid) const
{
    Uint4 vol_oid = 0;
    
    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetSeqLength(vol_oid, true);
    }
    
    return -1;
}

CRef<CBioseq>
CSeqDBImpl::GetBioseq(Int4 oid,
                      bool use_objmgr,
                      bool insert_ctrlA) const
{
    Uint4 vol_oid = 0;
    
    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetBioseq(vol_oid, use_objmgr, insert_ctrlA);
    }
    
    return CRef<CBioseq>();
}

void CSeqDBImpl::RetSequence(const char ** buffer) const
{
    m_MemPool.Free((void*) *buffer);
}

Int4 CSeqDBImpl::GetSequence(Int4 oid, const char ** buffer) const
{
    Uint4 vol_oid = 0;
    
    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetSequence(vol_oid, buffer);
    }
    
    return -1;
}

Int4 CSeqDBImpl::GetAmbigSeq(Int4            oid,
                             char         ** buffer,
                             Uint4           nucl_code,
                             ESeqDBAllocType alloc_type) const
{
    Uint4 vol_oid = 0;
    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetAmbigSeq(vol_oid, buffer, nucl_code, alloc_type);
    }
    
    return -1;
}

list< CRef<CSeq_id> > CSeqDBImpl::GetSeqIDs(Uint4 oid) const
{
    Uint4 vol_oid = 0;
    
    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetSeqIDs(vol_oid);
    }
    
    return list< CRef<CSeq_id> >();
}

Uint4 CSeqDBImpl::GetNumSeqs(void) const
{
    return m_Aliases.GetNumSeqs(m_VolSet);
}

Uint8 CSeqDBImpl::GetTotalLength(void) const
{
    return m_Aliases.GetTotalLength(m_VolSet);
}

string CSeqDBImpl::GetTitle(void) const
{
    return m_Aliases.GetTitle(m_VolSet);
}

char CSeqDBImpl::GetSeqType(void) const
{
    if (const CSeqDBVol * vol = m_VolSet.GetVol(0)) {
        return vol->GetSeqType();
    }
    return kSeqTypeUnkn;
}

string CSeqDBImpl::GetDate(void) const
{
    if (const CSeqDBVol * vol = m_VolSet.GetVol(0)) {
        return vol->GetDate();
    }
    return string();
}

CRef<CBlast_def_line_set> CSeqDBImpl::GetHdr(Uint4 oid) const
{
    Uint4 vol_oid = 0;
    
    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetHdr(vol_oid);
    }
    
    return CRef<CBlast_def_line_set>();
}

Uint4 CSeqDBImpl::GetMaxLength(void) const
{
    Uint4 max_len = 0;
    
    for(Uint4 i = 0; i < m_VolSet.GetNumVols(); i++) {
        Uint4 new_max = m_VolSet.GetVol(i)->GetMaxLength();
        
        if (new_max > max_len)
            max_len = new_max;
    }
    
    return max_len;
}

const string & CSeqDBImpl::GetDBNameList(void) const
{
    return m_DBNames;
}

END_NCBI_SCOPE

