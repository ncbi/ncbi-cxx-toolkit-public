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

#include <ncbi_pch.hpp>
#include "seqdbimpl.hpp"
#include <iostream>

BEGIN_NCBI_SCOPE

CSeqDBImpl::CSeqDBImpl(const string & db_name_list,
                       char           prot_nucl,
                       Uint4          oid_begin,
                       Uint4          oid_end,
                       bool           use_mmap)
    : m_Atlas        (use_mmap),
      m_DBNames      (db_name_list),
      m_Aliases      (m_Atlas, db_name_list, prot_nucl, use_mmap),
      m_VolSet       (m_Atlas, m_Aliases.GetVolumeNames(), prot_nucl),
      m_RestrictBegin(oid_begin),
      m_RestrictEnd  (oid_end),
      m_NextChunkOID (0),
      m_NumSeqs      (0),
      m_TotalLength  (0)
{
    m_Aliases.SetMasks(m_VolSet);
    
    if ( m_VolSet.HasMask() ) {
        m_OIDList.Reset( new CSeqDBOIDList(m_Atlas, m_VolSet, use_mmap) );
    }
    
    if ((oid_begin == 0) && (oid_end == 0)) {
        m_RestrictEnd = m_VolSet.GetNumSeqs();
    } else {
        if (m_RestrictEnd > m_VolSet.GetNumSeqs()) {
            m_RestrictEnd = m_VolSet.GetNumSeqs();
        }
        if (m_RestrictBegin > m_RestrictEnd) {
            m_RestrictBegin = m_RestrictEnd;
        }
    }
    
    m_NumSeqs     = x_GetNumSeqs();
    m_TotalLength = x_GetTotalLength();
}

CSeqDBImpl::~CSeqDBImpl(void)
{
    CSeqDBLockHold locked(m_Atlas);
    m_Atlas.Lock(locked);
    
    m_VolSet.UnLease();
    
    if (m_OIDList.NotEmpty()) {
        m_OIDList->UnLease();
    }
}

bool CSeqDBImpl::CheckOrFindOID(Uint4 & next_oid) const
{
    bool success = true;
    
    if (next_oid < m_RestrictBegin) {
        next_oid = m_RestrictBegin;
    }
    
    if (next_oid >= m_RestrictEnd) {
        success = false;
    }
    
    if (success && m_OIDList.NotEmpty()) {
        success = m_OIDList->CheckOrFindOID(next_oid);
        
        if (next_oid > m_RestrictEnd) {
            success = false;
        }
    }
    
    return success;
}

CSeqDB::EOidListType
CSeqDBImpl::GetNextOIDChunk(TOID         & begin_chunk, // out
                            TOID         & end_chunk,   // out
                            vector<TOID> & oid_list,    // out
                            Uint4        * state_obj)   // in+out
{
    CFastMutexGuard guard(m_OIDLock);
    
    if (! state_obj) {
        state_obj = & m_NextChunkOID;
    }
    
    Uint4 max_oids = oid_list.size();
    
    // This has to be done before ">=end" check, to insure correctness
    // in empty-range cases.
    
    if (*state_obj < m_RestrictBegin) {
        *state_obj = m_RestrictBegin;
    }
    
    // Case 1: Iteration's End.
    
    if (*state_obj >= m_RestrictEnd) {
        begin_chunk = 0;
        end_chunk   = 0;
        
        return CSeqDB::eOidRange;
    }
    
    // Case 2: Return a range
    
    if (m_OIDList.Empty()) {
        begin_chunk = * state_obj;
        end_chunk   = m_RestrictEnd;
        
        if ((end_chunk - begin_chunk) > max_oids) {
            end_chunk = begin_chunk + max_oids;
        }
        
        *state_obj = end_chunk;
        
        return CSeqDB::eOidRange;
    }
    
    // Case 3: Ones and Zeros
    
    Uint4 next_oid = *state_obj;
    Uint4 iter = 0;
    
    while(iter < max_oids) {
        if (next_oid >= m_RestrictEnd) {
            break;
        }
        
        if (m_OIDList->CheckOrFindOID(next_oid)) {
            oid_list[iter++] = next_oid++;
        } else {
            next_oid = m_RestrictEnd;
            break;
        }
    }
    
    if (iter < max_oids) {
        oid_list.resize(iter);
    }
    
    *state_obj = next_oid;
    return CSeqDB::eOidList;
}

Uint4 CSeqDBImpl::GetSeqLength(Uint4 oid) const
{
    CSeqDBLockHold locked(m_Atlas);
    Uint4 vol_oid = 0;
    
    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetSeqLength(vol_oid, false, locked);
    }
    
    NCBI_THROW(CSeqDBException,
               eArgErr,
               "OID not in valid range.");
}

Uint4 CSeqDBImpl::GetSeqLengthApprox(Uint4 oid) const
{
    CSeqDBLockHold locked(m_Atlas);
    Uint4 vol_oid = 0;
    
    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetSeqLength(vol_oid, true, locked);
    }
    
    NCBI_THROW(CSeqDBException,
               eArgErr,
               "OID not in valid range.");
}

CRef<CBioseq>
CSeqDBImpl::GetBioseq(Uint4 oid,
                      bool  use_objmgr,
                      bool  insert_ctrlA) const
{
    CSeqDBLockHold locked(m_Atlas);
    Uint4 vol_oid = 0;
    
    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetBioseq(vol_oid, use_objmgr, insert_ctrlA, locked);
    }
    
    NCBI_THROW(CSeqDBException,
               eArgErr,
               "OID not in valid range.");
}

void CSeqDBImpl::RetSequence(const char ** buffer) const
{
    // This can return either an allocated object or a reference to
    // part of a memory mapped region.
    CSeqDBLockHold locked(m_Atlas);
    
    m_Atlas.RetRegion(*buffer, locked);
    *buffer = 0;
}

Uint4 CSeqDBImpl::GetSequence(Uint4 oid, const char ** buffer) const
{
    CSeqDBLockHold locked(m_Atlas);
    Uint4 vol_oid = 0;
    
    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetSequence(vol_oid, buffer, locked);
    }
    
    NCBI_THROW(CSeqDBException,
               eArgErr,
               "OID not in valid range.");
}

Uint4 CSeqDBImpl::GetAmbigSeq(Uint4           oid,
                              char         ** buffer,
                              Uint4           nucl_code,
                              ESeqDBAllocType alloc_type) const
{
    CSeqDBLockHold locked(m_Atlas);
    
    Uint4 vol_oid = 0;
    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetAmbigSeq(vol_oid, buffer, nucl_code, alloc_type, locked);
    }
    
    NCBI_THROW(CSeqDBException,
               eArgErr,
               "OID not in valid range.");
}

list< CRef<CSeq_id> > CSeqDBImpl::GetSeqIDs(Uint4 oid) const
{
    CSeqDBLockHold locked(m_Atlas);
    Uint4 vol_oid = 0;
    
    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetSeqIDs(vol_oid, locked);
    }
    
    NCBI_THROW(CSeqDBException,
               eArgErr,
               "OID not in valid range.");
}

Uint4 CSeqDBImpl::GetNumSeqs(void) const
{
    return m_NumSeqs;
}

Uint8 CSeqDBImpl::GetTotalLength(void) const
{
    return m_TotalLength;
}

Uint4 CSeqDBImpl::x_GetNumSeqs(void) const
{
    return m_Aliases.GetNumSeqs(m_VolSet);
}

Uint8 CSeqDBImpl::x_GetTotalLength(void) const
{
    return m_Aliases.GetTotalLength(m_VolSet);
}

string CSeqDBImpl::GetTitle(void) const
{
    return x_FixString( m_Aliases.GetTitle(m_VolSet) );
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
        return x_FixString( vol->GetDate() );
    }
    return string();
}

CRef<CBlast_def_line_set> CSeqDBImpl::GetHdr(Uint4 oid) const
{
    CSeqDBLockHold locked(m_Atlas);
    Uint4 vol_oid = 0;
    
    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetHdr(vol_oid, locked);
    }
    
    NCBI_THROW(CSeqDBException,
               eArgErr,
               "OID not in valid range.");
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

// This is a work-around for bad data in the database; probably the
// fault of formatdb.  The problem is that the database date field has
// an incorrect length - possibly a general problem with string
// handling in formatdb?  In any case, this method trims a string to
// the minimum of its length and the position of the first NULL.  This
// technique will not work if the date field is null terminated, but
// apparently it usually is, in spite of the length bug.

string CSeqDBImpl::x_FixString(const string & s) const
{
    for(Uint4 i = 0; i < s.size(); i++) {
        if (s[i] == char(0)) {
            return string(s,0,i);
        }
    }
    return s;
}

END_NCBI_SCOPE

