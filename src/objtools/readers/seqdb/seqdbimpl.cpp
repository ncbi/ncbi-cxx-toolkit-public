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

CSeqDBImpl::CSeqDBImpl(const string & dbpath, const string & db_name_list, char prot_nucl, bool use_mmap)
    : m_Aliases (dbpath, db_name_list, prot_nucl, use_mmap),
      m_VolSet  (m_MemPool, dbpath, m_Aliases.GetVolumeNames(), prot_nucl, use_mmap)
{
    m_Aliases.SetMasks(m_VolSet);
    
    if ( m_VolSet.HasMask() ) {
        m_OIDList.Reset( new CSeqDBOIDList(m_VolSet, use_mmap) );
    }
}

CSeqDBImpl::~CSeqDBImpl(void)
{
}

bool CSeqDBImpl::CheckOrFindOID(Uint4 & next_oid)
{
    if (m_OIDList.Empty()) {
        return next_oid < GetNumSeqs();
    }
    
    return m_OIDList->CheckOrFindOID(next_oid);
}

Int4 CSeqDBImpl::GetSeqLength(Uint4 oid)
{
    Uint4 vol_oid = 0;
    
    if (CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetSeqLength(vol_oid, false);
    }
    
    return -1;
}

Int4 CSeqDBImpl::GetSeqLengthApprox(Uint4 oid)
{
    Uint4 vol_oid = 0;
    
    if (CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetSeqLength(vol_oid, true);
    }
    
    return -1;
}

CRef<CBioseq>
CSeqDBImpl::GetBioseq(Int4 oid,
                      bool use_objmgr,
                      bool insert_ctrlA)
{
    Uint4 vol_oid = 0;
    
    if (CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetBioseq(vol_oid, use_objmgr, insert_ctrlA);
    }
    
    return CRef<CBioseq>();
}

void CSeqDBImpl::RetSequence(const char ** buffer)
{
    m_MemPool.Free((void*) *buffer);
}

Int4 CSeqDBImpl::GetSequence(Int4 oid, const char ** buffer)
{
    Uint4 vol_oid = 0;
    if (CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetSequence(vol_oid, buffer);
    }
    
    return -1;
}

Int4 CSeqDBImpl::GetAmbigSeq(Int4 oid, const char ** buffer, bool nucl_code)
{
    Uint4 vol_oid = 0;
    if (CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetAmbigSeq(vol_oid, buffer, nucl_code);
    }
    
    return -1;
}

list< CRef<CSeq_id> > CSeqDBImpl::GetSeqIDs(Uint4 oid)
{
    Uint4 vol_oid = 0;
    
    if (CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetSeqIDs(vol_oid);
    }
    
    return list< CRef<CSeq_id> >();
}

Uint4 CSeqDBImpl::GetNumSeqs(void)
{
    return m_Aliases.GetNumSeqs(m_VolSet);
}

Uint8 CSeqDBImpl::GetTotalLength(void)
{
    return m_Aliases.GetTotalLength(m_VolSet);
}

string CSeqDBImpl::GetTitle(void)
{
    return m_Aliases.GetTitle(m_VolSet);
}

char CSeqDBImpl::GetSeqType(void)
{
    if (CSeqDBVol * vol = m_VolSet.GetVol(0)) {
        return vol->GetSeqType();
    }
    return kSeqTypeUnkn;
}

string CSeqDBImpl::GetDate(void)
{
    if (CSeqDBVol * vol = m_VolSet.GetVol(0)) {
        return vol->GetDate();
    }
    return string();
}

CRef<CBlast_def_line_set> CSeqDBImpl::GetHdr(Uint4 oid)
{
    Uint4 vol_oid = 0;
    
    if (CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetHdr(vol_oid);
    }
    
    return CRef<CBlast_def_line_set>();
}

Uint4 CSeqDBImpl::GetMaxLength(void)
{
    Uint4 max_len = 0;
    
    for(Uint4 i = 0; i < m_VolSet.GetNumVols(); i++) {
        Uint4 new_max = m_VolSet.GetVol(i)->GetMaxLength();
        
        if (new_max > max_len)
            max_len = new_max;
    }
    
    return max_len;
}

END_NCBI_SCOPE

