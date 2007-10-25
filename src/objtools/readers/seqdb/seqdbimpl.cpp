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

/// @file seqdbimpl.cpp
/// Implementation for the CSeqDBImpl class, the top implementation
/// layer for SeqDB.

#include <ncbi_pch.hpp>
#include "seqdbimpl.hpp"
#include <iostream>
#include <algorithm>

BEGIN_NCBI_SCOPE

CSeqDBImpl::CSeqDBImpl(const string       & db_name_list,
                       char                 prot_nucl,
                       int                  oid_begin,
                       int                  oid_end,
                       bool                 use_mmap,
                       CSeqDBGiList       * gi_list,
                       CSeqDBNegativeList * neg_list,
                       CSeqDBIdSet          idset)
    : m_AtlasHolder     (use_mmap, & m_FlushCB),
      m_Atlas           (m_AtlasHolder.Get()),
      m_DBNames         (db_name_list),
      m_Aliases         (m_Atlas, db_name_list, prot_nucl),
      m_VolSet          (m_Atlas,
                         m_Aliases.GetVolumeNames(),
                         prot_nucl,
                         gi_list,
                         neg_list),
      m_RestrictBegin   (oid_begin),
      m_RestrictEnd     (oid_end),
      m_NextChunkOID    (0),
      m_NumSeqs         (0),
      m_NumSeqsStats    (0),
      m_NumOIDs         (0),
      m_TotalLength     (0),
      m_TotalLengthStats(0),
      m_VolumeLength    (0),
      m_SeqType         (prot_nucl),
      m_OidListSetup    (false),
      m_UserGiList      (gi_list),
      m_NegativeList    (neg_list),
      m_IdSet           (idset),
      m_NeedTotalsScan  (false)
{
    INIT_CLASS_MARK();
    
    _ASSERT((! gi_list) || (! neg_list));
    
    m_Aliases.SetMasks(m_VolSet);
    m_VolSet.OptimizeGiLists();
    
    m_OidListSetup = ! (m_VolSet.HasFilter() || gi_list || neg_list);
    
    m_VolumeLength = x_GetVolumeLength();
    m_NumOIDs      = x_GetNumOIDs();
    
    SetIterationRange(0, m_NumOIDs);
    
    try {
        m_Atlas.Verify(false);
        m_TaxInfo = new CSeqDBTaxInfo(m_Atlas);
        m_Atlas.Verify(false);
    }
    catch(CSeqDBException &) {
    }
    
    // Don't setup the flush callback until the implementation data
    // structures are fully populated (otherwise flushing may try to
    // flush unconstructed memory leases).
    
    m_FlushCB.SetImpl(this);
    
    // If the alias files seem to provide correct data for the totals,
    // use it; otherwise scan the OID list and use approximate lengths
    // to compute the totals.  Presence of a user GI list implies that
    // the alias file cannot have correct values.
    
    try {
        if (gi_list || neg_list || m_Aliases.NeedTotalsScan(m_VolSet)) {
            m_NeedTotalsScan = true;
        }
        
        if ((! m_OidListSetup) && (oid_begin || oid_end)) {
            m_NeedTotalsScan = true;
        }
        
        if (m_NeedTotalsScan) {
            CSeqDBLockHold locked(m_Atlas);
            
            // This is a whole-database scan; it's always done in
            // approximate length mode.
            
            x_ScanTotals(true, & m_NumSeqs, & m_TotalLength, locked);
            m_Atlas.Verify(locked);
        } else {
            m_NumSeqs     = x_GetNumSeqs();
            m_TotalLength = x_GetTotalLength();
        }
        m_NumSeqsStats     = x_GetNumSeqsStats();
        m_TotalLengthStats = x_GetTotalLengthStats();
    }
    catch(CSeqDBException & e) {
        m_UserGiList.Reset();
        m_NegativeList.Reset();
        m_VolSet.UnLease();
        m_FlushCB.SetImpl(0);
        throw e;
    }
    
    SetIterationRange(oid_begin, oid_end);
    
    CHECK_MARKER();
}

CSeqDBImpl::CSeqDBImpl()
    : m_AtlasHolder     (false, & m_FlushCB),
      m_Atlas           (m_AtlasHolder.Get()),
      m_Aliases         (m_Atlas, "", '-'),
      m_RestrictBegin   (0),
      m_RestrictEnd     (0),
      m_NextChunkOID    (0),
      m_NumSeqs         (0),
      m_NumOIDs         (0),
      m_TotalLength     (0),
      m_VolumeLength    (0),
      m_SeqType         ('-'),
      m_OidListSetup    (true),
      m_NeedTotalsScan  (false)
{
    INIT_CLASS_MARK();
    
    try {
        m_TaxInfo = new CSeqDBTaxInfo(m_Atlas);
    }
    catch(CSeqDBException &) {
    }
    
    // Don't setup the flush callback until the implementation data
    // structures are fully populated (otherwise flushing may try to
    // flush unconstructed memory leases).
    
    m_FlushCB.SetImpl(this);
    
    CHECK_MARKER();
}

void CSeqDBImpl::SetIterationRange(int oid_begin, int oid_end)
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);
    m_Atlas.Lock(locked);
    
    m_RestrictBegin = (oid_begin < 0) ? 0 : oid_begin;
    m_RestrictEnd   = (oid_end   < 0) ? 0 : oid_end;
    
    if ((oid_begin == 0) && (oid_end == 0)) {
        m_RestrictEnd = m_VolSet.GetNumOIDs();
    } else {
        if ((oid_end == 0) || (m_RestrictEnd > m_VolSet.GetNumOIDs())) {
            m_RestrictEnd = m_VolSet.GetNumOIDs();
        }
        if (m_RestrictBegin > m_RestrictEnd) {
            m_RestrictBegin = m_RestrictEnd;
        }
    }
}

CSeqDBImpl::~CSeqDBImpl()
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);
    m_Atlas.Lock(locked);
    
    // Prevent GC from flushing volumes after they are torn down.
    
    m_FlushCB.SetImpl(0);
    
    m_TaxInfo.Reset();
    
    m_VolSet.UnLease();
    
    if (m_OIDList.NotEmpty()) {
        m_OIDList->UnLease();
    }
    BREAK_MARKER();
}

void CSeqDBImpl::x_GetOidList(CSeqDBLockHold & locked) const
{
    CHECK_MARKER();
    if (! m_OidListSetup) {
        m_Atlas.Lock(locked);
        
        if (m_OIDList.Empty()) {
            m_OIDList.Reset( new CSeqDBOIDList(m_Atlas,
                                               m_VolSet,
                                               m_UserGiList,
                                               m_NegativeList,
                                               locked) );
        }
        
        m_OidListSetup = true;
    }
}

bool CSeqDBImpl::CheckOrFindOID(int & next_oid) const
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);
    return x_CheckOrFindOID(next_oid, locked);
}

bool CSeqDBImpl::x_CheckOrFindOID(int & next_oid, CSeqDBLockHold & locked) const
{
    CHECK_MARKER();
    bool success = true;
    
    if (next_oid < m_RestrictBegin) {
        next_oid = m_RestrictBegin;
    }
    
    if (next_oid >= m_RestrictEnd) {
        success = false;
    }
    
    if (! m_OidListSetup) {
        x_GetOidList(locked);
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
CSeqDBImpl::GetNextOIDChunk(int         & begin_chunk, // out
                            int         & end_chunk,   // out
                            vector<int> & oid_list,    // out
                            int         * state_obj)   // in+out
{
    CHECK_MARKER();
    CFastMutexGuard guard(m_OIDLock);
    
    if (! state_obj) {
        state_obj = & m_NextChunkOID;
    }
    
    int max_oids = (int) oid_list.size();
    
    if (! max_oids) {
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "GetNextOIDChunk(): Illegal argument; OIDs requested = 0.");
    }
    
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
    
    if (! m_OidListSetup) {
        CSeqDBLockHold locked(m_Atlas);
        x_GetOidList(locked);
    }
    
    if (m_OIDList.Empty()) {
        begin_chunk = * state_obj;
        end_chunk   = m_RestrictEnd;
        
        if ((end_chunk - begin_chunk) > max_oids) {
            end_chunk = begin_chunk + int(max_oids);
        }
        
        *state_obj = end_chunk;
        
        return CSeqDB::eOidRange;
    }
    
    // Case 3: Ones and Zeros - The bitmap provides OIDs.
    
    int next_oid = *state_obj;
    int iter = 0;
    
    while(iter < max_oids) {
        if (next_oid >= m_RestrictEnd) {
            break;
        }
        // Find next ordinal id, and save it if it falls within iteration range.
        if (m_OIDList->CheckOrFindOID(next_oid) &&
            next_oid < m_RestrictEnd) {
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

void CSeqDBImpl::ResetInternalChunkBookmark()
{
    CHECK_MARKER();
    CFastMutexGuard guard(m_OIDLock);
    m_NextChunkOID = 0;
}

int CSeqDBImpl::GetSeqLength(int oid) const
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);
    m_Atlas.MentionOid(oid, m_NumOIDs, locked);
    
    int vol_oid = 0;
    
    if ('p' == m_SeqType) {
        if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
            return vol->GetSeqLengthProt(vol_oid, locked);
        }
    } else {
        if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
            return vol->GetSeqLengthExact(vol_oid, locked);
        }
    }
    
    NCBI_THROW(CSeqDBException,
               eArgErr,
               "OID not in valid range.");
}

int CSeqDBImpl::GetSeqLengthApprox(int oid) const
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);
    m_Atlas.MentionOid(oid, m_NumOIDs, locked);
    
    int vol_oid = 0;
    
    if ('p' == m_SeqType) {
        if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
            return vol->GetSeqLengthProt(vol_oid, locked);
        }
    } else {
        if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
            return vol->GetSeqLengthApprox(vol_oid, locked);
        }
    }
    
    NCBI_THROW(CSeqDBException,
               eArgErr,
               "OID not in valid range.");
}

void CSeqDBImpl::GetTaxIDs(int             oid,
                           map<int, int> & gi_to_taxid,
                           bool            persist) const
{
    CSeqDBLockHold locked(m_Atlas);
    m_Atlas.Lock(locked);
    m_Atlas.MentionOid(oid, m_NumOIDs, locked);
    
    if (! persist) {
        gi_to_taxid.clear();
    }
    
    CRef<CBlast_def_line_set> defline_set =
        x_GetHdr(oid, locked);
    
    if ((! defline_set.Empty()) && defline_set->CanGet()) {
        ITERATE(list< CRef<CBlast_def_line> >, defline, defline_set->Get()) {
            if (! (*defline)->CanGetSeqid()) {
                continue;
            }
            
            if (! (*defline)->IsSetTaxid()) {
                continue;
            }
            
            ITERATE(list< CRef<CSeq_id> >, seqid, (*defline)->GetSeqid()) {
                if (! (**seqid).IsGi()) {
                    continue;
                }
                
                gi_to_taxid[(**seqid).GetGi()] = (*defline)->GetTaxid();
            }
        }
    }
}

void CSeqDBImpl::GetTaxIDs(int           oid,
                           vector<int> & taxids,
                           bool          persist) const
{
    CSeqDBLockHold locked(m_Atlas);
    m_Atlas.MentionOid(oid, m_NumOIDs, locked);
    
    if (! persist) {
        taxids.clear();
    }
    
    CRef<CBlast_def_line_set> defline_set =
        x_GetHdr(oid, locked);
    
    if ((! defline_set.Empty()) && defline_set->CanGet()) {
        ITERATE(list< CRef<CBlast_def_line> >, defline, defline_set->Get()) {
            if (! (*defline)->IsSetTaxid()) {
                continue;
            }
            
            taxids.push_back((*defline)->GetTaxid());
        }
    }
}

CRef<CBioseq>
CSeqDBImpl::GetBioseq(int oid, int target_gi, bool seqdata) const
{
    CHECK_MARKER();
    
    CSeqDBLockHold locked(m_Atlas);
    m_Atlas.Lock(locked);
    m_Atlas.MentionOid(oid, m_NumOIDs, locked);
    
    int vol_oid = 0;
    
    if (! m_OidListSetup) {
        x_GetOidList(locked);
    }
    
    bool have_oidlist = m_OIDList.NotEmpty();
    int memb_bit = m_Aliases.GetMembBit(m_VolSet);
    
    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetBioseq(vol_oid,
                              have_oidlist,
                              memb_bit,
                              target_gi,
                              m_TaxInfo,
                              seqdata,
                              locked);
    }
    
    NCBI_THROW(CSeqDBException,
               eArgErr,
               "OID not in valid range.");
}

void CSeqDBImpl::RetSequence(const char ** buffer) const
{
    CHECK_MARKER();
    
    // This can return either an allocated object or a reference to
    // part of a memory mapped region.
    
    CSeqDBLockHold locked(m_Atlas);
    m_Atlas.Lock(locked);
    
    m_Atlas.RetRegion(*buffer);
    *buffer = 0;
}

int CSeqDBImpl::GetSequence(int oid, const char ** buffer) const
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);
    int vol_oid = 0;
    
    m_Atlas.Lock(locked);
    m_Atlas.MentionOid(oid, m_NumOIDs, locked);
    
    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetSequence(vol_oid, buffer, locked);
    }
    
    NCBI_THROW(CSeqDBException,
               eArgErr,
               "OID not in valid range.");
}

CRef<CSeq_data> CSeqDBImpl::GetSeqData(int     oid,
                                       TSeqPos begin,
                                       TSeqPos end) const
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);
    int vol_oid = 0;
    
    m_Atlas.Lock(locked);
    m_Atlas.MentionOid(oid, m_NumOIDs, locked);
    
    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetSeqData(vol_oid, begin, end, locked);
    }
    
    NCBI_THROW(CSeqDBException,
               eArgErr,
               "OID not in valid range.");
}

int CSeqDBImpl::GetAmbigSeq(int               oid,
                            char           ** buffer,
                            int               nucl_code,
                            SSeqDBSlice     * region,
                            ESeqDBAllocType   alloc_type) const
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);
    
    m_Atlas.Lock(locked);
    m_Atlas.MentionOid(oid, m_NumOIDs, locked);
    
    int vol_oid = 0;
    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetAmbigSeq(vol_oid,
                                buffer,
                                nucl_code,
                                alloc_type,
                                region,
                                locked);
    }
    
    NCBI_THROW(CSeqDBException,
               eArgErr,
               "OID not in valid range.");
}

list< CRef<CSeq_id> > CSeqDBImpl::GetSeqIDs(int oid) const
{
    CHECK_MARKER();
    int vol_oid = 0;
    
    CSeqDBLockHold locked(m_Atlas);
    m_Atlas.Lock(locked);
    m_Atlas.MentionOid(oid, m_NumOIDs, locked);
    
    if (! m_OidListSetup) {
        x_GetOidList(locked);
    }
    
    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        bool have_oidlist = m_OIDList.NotEmpty();
        int memb_bit      = m_Aliases.GetMembBit(m_VolSet);
        
        return vol->GetSeqIDs(vol_oid,
                              have_oidlist,
                              memb_bit,
                              locked);
    }
    
    NCBI_THROW(CSeqDBException,
               eArgErr,
               "OID not in valid range.");
}

int CSeqDBImpl::GetNumSeqs() const
{
    CHECK_MARKER();
    return m_NumSeqs;
}

int CSeqDBImpl::GetNumSeqsStats() const
{
    CHECK_MARKER();
    return m_NumSeqsStats;
}

int CSeqDBImpl::GetNumOIDs() const
{
    CHECK_MARKER();
    return m_NumOIDs;
}

Uint8 CSeqDBImpl::GetTotalLength() const
{
    CHECK_MARKER();
    return m_TotalLength;
}

Uint8 CSeqDBImpl::GetTotalLengthStats() const
{
    CHECK_MARKER();
    return m_TotalLengthStats;
}

Uint8 CSeqDBImpl::GetVolumeLength() const
{
    CHECK_MARKER();
    return m_VolumeLength;
}

int CSeqDBImpl::x_GetNumSeqs() const
{
    CHECK_MARKER();
    
    // GetNumSeqs should not overflow, even for alias files.
    
    Int8 rv = m_Aliases.GetNumSeqs(m_VolSet);
    _ASSERT((rv & 0x7FFFFFFF) == rv);
    
    return (int) rv;
}

int CSeqDBImpl::x_GetNumSeqsStats() const
{
    CHECK_MARKER();
    
    // GetNumSeqs should not overflow, even for alias files.
    
    Int8 rv = m_Aliases.GetNumSeqsStats(m_VolSet);
    _ASSERT((rv & 0x7FFFFFFF) == rv);
    
    return (int) rv;
}

int CSeqDBImpl::x_GetNumOIDs() const
{
    CHECK_MARKER();
    Int8 num_oids = m_VolSet.GetNumOIDs();
    
    // The aliases file may have more of these, because walking the
    // alias tree will count volumes each time they appear in the
    // volume set.  The volset number is the "good" one, because it
    // corresponds to the range of OIDs we accept in methods like
    // "GetSequence()".  If you give SeqDB an OID, the volset number
    // is the range for that oid.
    
    // However, at this layer, we need to use Int8, because the alias
    // number can overestimate so much that it wraps a signed int.
    
    _ASSERT(num_oids <= m_Aliases.GetNumOIDs(m_VolSet));
    _ASSERT((num_oids & 0x7FFFFFFF) == num_oids);
    
    return (int) num_oids;
}

Uint8 CSeqDBImpl::x_GetTotalLength() const
{
    CHECK_MARKER();
    return m_Aliases.GetTotalLength(m_VolSet);
}

Uint8 CSeqDBImpl::x_GetTotalLengthStats() const
{
    CHECK_MARKER();
    return m_Aliases.GetTotalLengthStats(m_VolSet);
}

Uint8 CSeqDBImpl::x_GetVolumeLength() const
{
    CHECK_MARKER();
    return m_VolSet.GetVolumeSetLength();
}

string CSeqDBImpl::GetTitle() const
{
    CHECK_MARKER();
    return x_FixString( m_Aliases.GetTitle(m_VolSet) );
}

char CSeqDBImpl::GetSeqType() const
{
    CHECK_MARKER();
    if (const CSeqDBVol * vol = m_VolSet.GetVol(0)) {
        return vol->GetSeqType();
    }
    return '-';
}

string CSeqDBImpl::GetDate() const
{
    CHECK_MARKER();
    
    CSeqDBLockHold locked(m_Atlas);
    m_Atlas.Lock(locked);
    
    if (! m_Date.empty()) {
        return m_Date;
    }
    
    // This is close enough to allow parsing but does not precisely
    // describe the format normally used for generated dates.
    
    string fmt = "b d, Y  H:m P";
    string date;
    
    for(int i = 0; i < m_VolSet.GetNumVols(); i++) {
        string d = x_FixString( m_VolSet.GetVol(i)->GetDate() );
        
        if (date.empty()) {
            date = d;
        } else if (d != date) {
            try {
                CTime t1(date, fmt);
                CTime t2(d, fmt);
                
                if (t2 > t1) {
                    date.swap(d);
                }
            }
            catch(CStringException &) {
                // Here I think it is better to pick any valid date
                // than to propagate a string exception.
            }
        }
    }
    
    m_Date = date;
    
    return date;
}

CRef<CBlast_def_line_set> CSeqDBImpl::GetHdr(int oid) const
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);
    
    return x_GetHdr(oid, locked);
}

CRef<CBlast_def_line_set>
CSeqDBImpl::x_GetHdr(int oid, CSeqDBLockHold & locked) const
{
    CHECK_MARKER();
    m_Atlas.Lock(locked);
    m_Atlas.MentionOid(oid, m_NumOIDs, locked);
    
    int vol_oid = 0;
    
    if (! m_OidListSetup) {
        x_GetOidList(locked);
    }
    
    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        bool have_oidlist = m_OIDList.NotEmpty();
        Uint4 memb_bit = m_Aliases.GetMembBit(m_VolSet);
        
        return vol->GetFilteredHeader(vol_oid,
                                      have_oidlist,
                                      memb_bit,
                                      locked);
    }
    
    NCBI_THROW(CSeqDBException,
               eArgErr,
               "OID not in valid range.");
}

int CSeqDBImpl::GetMaxLength() const
{
    CHECK_MARKER();
    int max_len = 0;
    
    for(int i = 0; i < m_VolSet.GetNumVols(); i++) {
        int new_max = m_VolSet.GetVol(i)->GetMaxLength();
        
        if (new_max > max_len)
            max_len = new_max;
    }
    
    return max_len;
}

const string & CSeqDBImpl::GetDBNameList() const
{
    CHECK_MARKER();
    return m_DBNames;
}

// This is a work-around for bad data in the database; probably the
// fault of formatdb.  The problem is that the database date field has
// an incorrect length - possibly a general problem with string
// handling in formatdb?  In any case, this method trims a string to
// the minimum of its length and the position of the first NULL.  This
// technique will not work if the date field is not null terminated,
// but apparently it usually or always is, in spite of the length bug.

string CSeqDBImpl::x_FixString(const string & s) const
{
    CHECK_MARKER();
    for(int i = 0; i < (int) s.size(); i++) {
        if (s[i] == char(0)) {
            return string(s,0,i);
        }
    }
    return s;
}

void CSeqDBImplFlush::operator()()
{
    if (m_Impl) {
        m_Impl->FlushSeqMemory();
    }
}

// Assumes atlas is locked

void CSeqDBImpl::FlushSeqMemory()
{
    m_VolSet.UnLease();
}

bool CSeqDBImpl::PigToOid(int pig, int & oid) const
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);
    
    for(int i = 0; i < m_VolSet.GetNumVols(); i++) {
        if (m_VolSet.GetVol(i)->PigToOid(pig, oid, locked)) {
            oid += m_VolSet.GetVolOIDStart(i);
            return true;
        }
    }
    
    return false;
}

bool CSeqDBImpl::OidToPig(int oid, int & pig) const
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);
    int vol_oid(0);
    
    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetPig(vol_oid, pig, locked);
    }
    
    NCBI_THROW(CSeqDBException,
               eArgErr,
               "OID not in valid range.");
}

bool CSeqDBImpl::TiToOid(Int8 ti, int & oid) const
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);
    
    for(int i = 0; i < m_VolSet.GetNumVols(); i++) {
        if (m_VolSet.GetVol(i)->TiToOid(ti, oid, locked)) {
            oid += m_VolSet.GetVolOIDStart(i);
            return true;
        }
    }
    
    return false;
}

bool CSeqDBImpl::GiToOid(int gi, int & oid) const
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);
    
    // This could be accellerated (a little) if a GI list is used.
    // However, this should be done (if at all) at the volume layer,
    // not in *Impl.  This volume may mask a particular GI that the
    // user gi list DOES NOT mask.  Or, the user GI list may assign
    // this GI an OID that belongs to a different volume, for example,
    // if the same GI appears in more than one volume.  In such cases,
    // the volume GI list (if one exists) is probably a better filter,
    // because it represents the restrictions of both the volume GI
    // list and the User GI list.  It's also smaller, and therefore
    // should be easier to binary search.
    
    // (The hypothetical optimization described above changes if there
    // are Seq-ids in the user provided list, because you can no
    // longer assume that the GI list is all-inclusive -- you would
    // also need to fall back on regular lookups.)
    
    for(int i = 0; i < m_VolSet.GetNumVols(); i++) {
        if (m_VolSet.GetVol(i)->GiToOid(gi, oid, locked)) {
            oid += m_VolSet.GetVolOIDStart(i);
            return true;
        }
    }
    
    return false;
}

bool CSeqDBImpl::OidToGi(int oid, int & gi) const
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);
    int vol_oid(0);
    
    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetGi(vol_oid, gi, locked);
    }
    
    NCBI_THROW(CSeqDBException,
               eArgErr,
               "OID not in valid range.");
}

void CSeqDBImpl::AccessionToOids(const string & acc, vector<int> & oids) const
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);
    
    oids.clear();
    
    vector<int> vol_oids;
    
    for(int vol_idx = 0; vol_idx < m_VolSet.GetNumVols(); vol_idx++) {
        // Append any additional OIDs from this volume's indices.
        m_VolSet.GetVol(vol_idx)->AccessionToOids(acc, vol_oids, locked);
        
        if (vol_oids.empty()) {
            continue;
        }
        
        int vol_start = m_VolSet.GetVolOIDStart(vol_idx);
        
        ITERATE(vector<int>, iter, vol_oids) {
            int oid1 = ((*iter) + vol_start);
            int oid2 = oid1;
            
            // Remove OIDs already found in OIDs.
            
            if (find(oids.begin(), oids.end(), oid1) != oids.end()) {
                continue;
            }
            
            // Filter out any oids not in the virtual oid bitmaps.
            
            if (x_CheckOrFindOID(oid2, locked) && (oid1 == oid2)) {
                oids.push_back(oid1);
            }
        }
        
        vol_oids.clear();
    }
}

void CSeqDBImpl::SeqidToOids(const CSeq_id & seqid_in, vector<int> & oids, bool multi) const
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);
    
    // The lower level functions modify the seqid - namely, changing
    // or clearing certain fields before printing it to a string.
    // Further analysis of data and exception flow might reveal that
    // the Seq_id will always be returned to the original state by
    // this operation... At the moment, safest route is to clone it.
    
    CSeq_id seqid;
    seqid.Assign(seqid_in);
    
    oids.clear();
    
    vector<int> vol_oids;
    
    for(int vol_idx = 0; vol_idx < m_VolSet.GetNumVols(); vol_idx++) {
        // Append any additional OIDs from this volume's indices.
        m_VolSet.GetVol(vol_idx)->SeqidToOids(seqid, vol_oids, locked);
        
        if (vol_oids.empty()) {
            continue;
        }
        
        int vol_start = m_VolSet.GetVolOIDStart(vol_idx);
        
        ITERATE(vector<int>, iter, vol_oids) {
            int oid1 = ((*iter) + vol_start);
            int oid2 = oid1;
            
            // Filter out any oids not in the virtual oid bitmaps.
            
            if (x_CheckOrFindOID(oid2, locked) && (oid1 == oid2)) {
                oids.push_back(oid1);
                
                if (! multi) {
                    return;
                }
            }
        }
        
        vol_oids.clear();
    }
}

int CSeqDBImpl::GetOidAtOffset(int first_seq, Uint8 residue) const
{
    CSeqDBLockHold locked(m_Atlas);
    m_Atlas.Lock(locked);
    
    CHECK_MARKER();
    if (first_seq >= m_NumOIDs) {
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "OID not in valid range.");
    }
    
    if (residue >= m_VolumeLength) {
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "Residue offset not in valid range.");
    }
    
    int vol_start(0);
    
    for(int vol_idx = 0; vol_idx < m_VolSet.GetNumVols(); vol_idx++) {
        const CSeqDBVol * volp = m_VolSet.GetVol(vol_idx);
        
        int vol_cnt = volp->GetNumOIDs();
        Uint8 vol_len = volp->GetVolumeLength();
        
        // Both limits fit this volume, delegate to volume code.
        
        if ((first_seq < vol_cnt) && (residue < vol_len)) {
            return vol_start + volp->GetOidAtOffset(first_seq, residue, locked);
        }
        
        // Adjust each limit.
        
        vol_start += vol_cnt;
        
        if (first_seq > vol_cnt) {
            first_seq -= vol_cnt;
        } else {
            first_seq = 0;
        }
        
        if (residue > vol_len) {
            residue -= vol_len;
        } else {
            residue = 0;
        }
    }
    
    NCBI_THROW(CSeqDBException,
               eArgErr,
               "Could not find valid split point oid.");
}

void
CSeqDBImpl::FindVolumePaths(const string   & dbname,
                            char             prot_nucl,
                            vector<string> & paths)
{
    CSeqDBAtlasHolder AH(true, 0);
    CSeqDBAtlas & atlas(AH.Get());
    
    // This constructor handles its own locking.
    CSeqDBAliasFile aliases(atlas, dbname, prot_nucl);
    paths = aliases.GetVolumeNames();
}

void
CSeqDBImpl::FindVolumePaths(vector<string> & paths) const
{
    CHECK_MARKER();
    paths = m_Aliases.GetVolumeNames();
}

void CSeqDBImpl::GetAliasFileValues(TAliasFileValues & afv)
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);
    m_Atlas.Lock(locked);

    m_Aliases.GetAliasFileValues(afv, m_VolSet);
}

void CSeqDBImpl::x_ScanTotals(bool             approx,
                              int            * numseq,
                              Uint8          * totlen,
                              CSeqDBLockHold & locked)
{
    int   oid_count(0);
    Uint8 base_count(0);
    
    const CSeqDBVol * volp = 0;
    
    m_Atlas.Lock(locked);
    
    for(int oid = 0; x_CheckOrFindOID(oid, locked); oid++) {
        ++ oid_count;
        
        int vol_oid = 0;
        
        volp = m_VolSet.FindVol(oid, vol_oid);
        
        _ASSERT(volp);
        
        if (totlen) {
            if ('p' == m_SeqType) {
                base_count += volp->GetSeqLengthProt(vol_oid, locked);
            } else {
                if (approx) {
                    base_count += volp->GetSeqLengthApprox(vol_oid, locked);
                } else {
                    base_count += volp->GetSeqLengthExact(vol_oid, locked);
                }
            }
        }
    }
    
    if (numseq) {
        *numseq = oid_count;
    }
    
    if (totlen) {
        *totlen = base_count;
    }
}

void CSeqDBImpl::GetTaxInfo(int taxid, SSeqDBTaxInfo & info)
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);
    
    if (m_TaxInfo.Empty()) {
        NCBI_THROW(CSeqDBException,
                   eFileErr,
                   "Taxonomic database was not found.");
    }
    
    CSeqDBTaxNames tnames;
    
    if (! m_TaxInfo->GetTaxNames(taxid, tnames, locked)) {
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "Specified taxid was not found.");
    }
    
    info.taxid           = taxid;
    info.scientific_name = tnames.GetSciName();
    info.common_name     = tnames.GetCommonName();
    info.blast_name      = tnames.GetBlastName();
    info.s_kingdom       = tnames.GetSKing();
}

void CSeqDBImpl::GetTotals(ESummaryType   sumtype,
                           int          * oid_count,
                           Uint8        * total_length,
                           bool           use_approx)
{
    CSeqDBLockHold locked(m_Atlas);
    
    if (oid_count) {
        *oid_count = 0;
    }
    
    if (total_length) {
        *total_length = 0;
    }
    
    switch(sumtype) {
    case CSeqDB::eUnfilteredAll:
        if (oid_count) {
            *oid_count = GetNumOIDs();
        }
        if (total_length) {
            *total_length = GetVolumeLength();
        }
        break;
        
    case CSeqDB::eFilteredAll:
        if (oid_count) {
            *oid_count = GetNumSeqs();
        }
        if (total_length) {
            *total_length = GetTotalLength();
        }
        break;
        
    case CSeqDB::eFilteredRange:
        x_ScanTotals(use_approx, oid_count, total_length, locked);
        break;
    }
}

void CSeqDBImpl::GetRawSeqAndAmbig(int           oid,
                                   const char ** buffer,
                                   int         * seq_length,
                                   int         * ambig_length) const
{
    CHECK_MARKER();
    
    CSeqDBLockHold locked(m_Atlas);
    m_Atlas.Lock(locked);
    m_Atlas.MentionOid(oid, m_NumOIDs, locked);
    
    int vol_oid = 0;
    
    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        vol->GetRawSeqAndAmbig(vol_oid,
                               buffer,
                               seq_length,
                               ambig_length,
                               locked);
        
        return;
    }
    
    NCBI_THROW(CSeqDBException, eArgErr, "OID not in valid range.");
}

/// Accumulate optional min, max, and count.
///
/// This generic template describes the accumulation of low, high, and
/// count values over any ordered type.
///
/// @param low_in    The low value for one volume. [in]
/// @param high_in   The high value for one volume. [in]
/// @param count_in  The ID count for one volume. [in]
/// @param low_out   If non-null, the low output value. [out]
/// @param high_out  If non-null, the high output value. [out]
/// @param count_out If non-null, the ID output value. [out]
template<class TId>
inline void s_AccumulateMinMaxCount(TId   low_in,
                                    TId   high_in,
                                    int   count_in,
                                    TId * low_out,
                                    TId * high_out,
                                    int * count_out,
                                    bool set_all)
{
    if (set_all) {
        if (low_out) {
            *low_out   = low_in;
        }
        
        *high_out  = high_in;
        *count_out = count_in;
    } else {
        if (low_out && (set_all || *low_out > low_in)) {
            *low_out = low_in;
        }
        if (high_out && (set_all || *high_out < high_in)) {
            *high_out = high_in;
        }
        if (count_out) {
            if (set_all) {
                *count_out = 0;
            }
            *count_out += count_in;
        }
    }
}

void CSeqDBImpl::GetGiBounds(int * low_id,
                             int * high_id,
                             int * count)
{
    CSeqDBLockHold locked(m_Atlas);
    
    bool found = false;
    
    for(int i = 0; i < m_VolSet.GetNumVols(); i++) {
        int vlow(0), vhigh(0), vcount(0);
        
        m_VolSet.GetVol(i)->GetGiBounds(vlow, vhigh, vcount, locked);
        
        if (vcount) {
            s_AccumulateMinMaxCount(vlow,
                                    vhigh,
                                    vcount,
                                    low_id,
                                    high_id,
                                    count,
                                    ! found);
            
            found = true;
        }
    }
    
    if (! found) {
        NCBI_THROW(CSeqDBException, eArgErr, "No GIs found.");
    }
}

void CSeqDBImpl::GetPigBounds(int * low_id,
                              int * high_id,
                              int * count)
{
    CSeqDBLockHold locked(m_Atlas);
    
    bool found = false;
    
    for(int i = 0; i < m_VolSet.GetNumVols(); i++) {
        int vlow(0), vhigh(0), vcount(0);
        
        m_VolSet.GetVol(i)->GetPigBounds(vlow, vhigh, vcount, locked);
        
        if (vcount) {
            s_AccumulateMinMaxCount(vlow,
                                    vhigh,
                                    vcount,
                                    low_id,
                                    high_id,
                                    count,
                                    ! found);
            
            found = true;
        }
    }
    
    if (! found) {
        NCBI_THROW(CSeqDBException, eArgErr, "No PIGs found.");
    }
}

void CSeqDBImpl::GetStringBounds(string * low_id,
                                 string * high_id,
                                 int    * count)
{
    CSeqDBLockHold locked(m_Atlas);
    
    bool found = false;
    
    for(int i = 0; i < m_VolSet.GetNumVols(); i++) {
        string vlow, vhigh;
        int vcount(0);
        
        m_VolSet.GetVol(i)->GetStringBounds(vlow, vhigh, vcount, locked);
        
        if (vcount) {
            s_AccumulateMinMaxCount(vlow,
                                    vhigh,
                                    vcount,
                                    low_id,
                                    high_id,
                                    count,
                                    ! found);
            
            found = true;
        }
    }
    
    if (! found) {
        NCBI_THROW(CSeqDBException, eArgErr, "No strings found.");
    }
}

void CSeqDBImpl::SetOffsetRanges(int                oid,
                                 const TRangeList & offset_ranges,
                                 bool               append_ranges,
                                 bool               cache_data)
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);
    int vol_oid = 0;
    
    m_Atlas.Lock(locked);
    
    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        vol->SetOffsetRanges(vol_oid,
                             offset_ranges,
                             append_ranges,
                             cache_data,
                             locked);
    } else {
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "OID not in valid range.");
    }
}

void CSeqDBImpl::SetDefaultMemoryBound(Uint8 bytes)
{
    CSeqDBAtlas::SetDefaultMemoryBound(bytes);
}

unsigned CSeqDBImpl::GetSequenceHash(int oid)
{
    char * datap(0);
    int base_len = GetAmbigSeq(oid,
                               & datap,
                               kSeqDBNuclNcbiNA8,
                               0,
                               (ESeqDBAllocType) 0);
    
    unsigned h = SeqDB_SequenceHash(datap, base_len);
    
    RetSequence(const_cast<const char**>(& datap));
    
    return h;
}

void CSeqDBImpl::HashToOids(unsigned hash, vector<int> & oids)
{
    // Find all OIDs in all volumes that match this hash.
    
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);
    
    oids.clear();
    
    vector<int> vol_oids;
    
    for(int vol_idx = 0; vol_idx < m_VolSet.GetNumVols(); vol_idx++) {
        // Append any additional OIDs from this volume's indices.
        m_VolSet.GetVol(vol_idx)->HashToOids(hash, vol_oids, locked);
        
        if (vol_oids.empty()) {
            continue;
        }
        
        int vol_start = m_VolSet.GetVolOIDStart(vol_idx);
        
        ITERATE(vector<int>, iter, vol_oids) {
            int oid1 = (*iter) + vol_start;
            int oid2 = oid1;
            
            // Filter out any oids not in the virtual oid bitmaps.
            
            if (x_CheckOrFindOID(oid2, locked) && (oid1 == oid2)) {
                oids.push_back(oid1);
            }
        }
        
        vol_oids.clear();
    }
}

CSeqDBIdSet CSeqDBImpl::GetIdSet()
{
    if (m_IdSet.Blank()) {
        if (! m_UserGiList.Empty()) {
            // Note: this returns a 'blank' IdSet list for positive
            // lists that specify filtering using CSeq-id objects.
            
            if (m_UserGiList->GetNumGis()) {
                vector<int> gis;
                m_UserGiList->GetGiList(gis);
                
                CSeqDBIdSet new_ids(gis, CSeqDBIdSet::eGi);
                m_IdSet = new_ids;
            } else if (m_UserGiList->GetNumTis()) {
                vector<Int8> tis;
                m_UserGiList->GetTiList(tis);
                
                CSeqDBIdSet new_ids(tis, CSeqDBIdSet::eTi);
                m_IdSet = new_ids;
            }
        } else if (! m_NegativeList.Empty()) {
            const vector<int> & ngis = m_NegativeList->GetGiList();
            const vector<Int8> & ntis = m_NegativeList->GetTiList();
            
            if (! ngis.empty()) {
                CSeqDBIdSet new_ids(ngis, CSeqDBIdSet::eGi, false);
                m_IdSet = new_ids;
            } else if (! ntis.empty()) {
                CSeqDBIdSet new_ids(ntis, CSeqDBIdSet::eTi, false);
                m_IdSet = new_ids;
            }
        }
    }
    
    return m_IdSet;
}

END_NCBI_SCOPE

