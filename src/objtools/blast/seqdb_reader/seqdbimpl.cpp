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
#include <sstream>
#include <algorithm>
#include <serial/enumvalues.hpp>
#include <serial/objistr.hpp>
#include <serial/objistrasnb.hpp>
#include <algo/blast/core/blast_seqsrc.h>

BEGIN_NCBI_SCOPE

CSeqDBImpl::CSeqDBImpl(const string       & db_name_list,
                       char                 prot_nucl,
                       int                  oid_begin,
                       int                  oid_end,
                       CSeqDBGiList       * gi_list,
                       CSeqDBNegativeList * neg_list,
                       CSeqDBIdSet          idset,
                       bool                 use_atlas_lock)
    : m_AtlasHolder     (NULL, use_atlas_lock),
      m_Atlas           (m_AtlasHolder.Get()),
      m_DBNames         (db_name_list),
      m_Aliases         (m_Atlas, db_name_list, prot_nucl),
      m_VolSet          (m_Atlas,
                         m_Aliases.GetVolumeNames(),
                         prot_nucl,
                         gi_list,
                         neg_list),
      m_LMDBSet         (m_VolSet),
      m_RestrictBegin   (oid_begin),
      m_RestrictEnd     (oid_end),
      m_NextChunkOID    (0),
      m_NumSeqs         (0),
      m_NumSeqsStats    (0),
      m_NumOIDs         (0),
      m_TotalLength     (0),
      m_ExactTotalLength(0),
      m_TotalLengthStats(0),
      m_VolumeLength    (0),
      m_MaxLength       (0),
      m_MinLength       (0),
      m_SeqType         (prot_nucl),
      m_OidListSetup    (false),
      m_UserGiList      (gi_list),
      m_NegativeList    (neg_list),
      m_IdSet           (idset),
      m_NeedTotalsScan  (false),
      m_UseGiMask       (m_Aliases.HasGiMask()),
      m_MaskDataColumn  (kUnknownTitle),
      m_NumThreads      (0)
{
    INIT_CLASS_MARK();

    if (m_UseGiMask) {
        vector <string> mask_list;
        m_Aliases.GetMaskList(mask_list);
        m_GiMask.Reset(new CSeqDBGiMask(m_Atlas, mask_list));
    }


    m_VolSet.OptimizeGiLists();

    m_OidListSetup = ! (m_Aliases.HasFilters() || gi_list || neg_list);

    m_VolumeLength = x_GetVolumeLength();
    m_NumOIDs      = x_GetNumOIDs();

    SetIterationRange(0, m_NumOIDs);


    // If the alias files seem to provide correct data for the totals,
    // use it; otherwise scan the OID list and use approximate lengths
    // to compute the totals.  Presence of a user GI list implies that
    // the alias file cannot have correct values.

    try {
        if (gi_list || neg_list || m_Aliases.NeedTotalsScan(m_VolSet)) {
            m_NeedTotalsScan = true;
            x_InitIdSet();
        }

        if ((! m_OidListSetup) && (oid_begin || oid_end)) {
            m_NeedTotalsScan = true;
        }

        if (m_NeedTotalsScan) {
            CSeqDBLockHold locked(m_Atlas);

            // This is a whole-database scan; it's always done in
            // approximate length mode.
            x_ScanTotals(true, & m_NumSeqs, & m_TotalLength,
                               & m_MaxLength, & m_MinLength, locked);

        } else {
            m_NumSeqs     = x_GetNumSeqs();
            m_TotalLength = x_GetTotalLength();
            m_MaxLength   = x_GetMaxLength();
            m_MinLength   = x_GetMinLength();

            // Do not bother scanning the db... it would be slow
            // FIXME: future implementation should probably have the
            // shortest length encoded in index file...
            if ( m_MinLength <= 0)  m_MinLength = BLAST_SEQSRC_MINLENGTH;
        }
        m_NumSeqsStats     = x_GetNumSeqsStats();
        m_TotalLengthStats = x_GetTotalLengthStats();

        LOG_POST(Info << "Num of Seqs: " << m_NumSeqs);
        LOG_POST(Info << "Total Length: " << m_TotalLength);
    }
    catch(CSeqDBException & e) {
        m_UserGiList.Reset();
        m_NegativeList.Reset();
        m_VolSet.UnLease();
        throw e;
    }

    SetIterationRange(oid_begin, oid_end);

    reusable_inpstr =  new CObjectIStreamAsnBinary ; 

    CHECK_MARKER();
}

CSeqDBImpl::CSeqDBImpl(bool use_atlas_lock)
    : m_AtlasHolder     (NULL, use_atlas_lock),
      m_Atlas           (m_AtlasHolder.Get()),
      m_Aliases         (m_Atlas, "", '-'),
      m_RestrictBegin   (0),
      m_RestrictEnd     (0),
      m_NextChunkOID    (0),
      m_NumSeqs         (0),
      m_NumOIDs         (0),
      m_TotalLength     (0),
      m_ExactTotalLength(0),
      m_VolumeLength    (0),
      m_SeqType         ('-'),
      m_OidListSetup    (true),
      m_NeedTotalsScan  (false),
      m_UseGiMask       (false),
      m_MaskDataColumn  (kUnknownTitle),
      m_NumThreads      (0)
{
    INIT_CLASS_MARK();

    reusable_inpstr =  new CObjectIStreamAsnBinary ; 
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
    if( reusable_inpstr ) {
	delete reusable_inpstr;
	reusable_inpstr = NULL;
    }
    else {
        cerr << "\n(=)\n";
    }
    SetNumberOfThreads(0);

    CSeqDBLockHold locked(m_Atlas);
    m_Atlas.Lock(locked);



    m_VolSet.UnLease();

    if (m_OIDList.NotEmpty()) {
        m_OIDList->UnLease();
    }
    BREAK_MARKER();
}

void CSeqDBImpl::x_GetOidList(CSeqDBLockHold & locked)
{
    CHECK_MARKER();
    m_Atlas.Lock(locked);
    if (! m_OidListSetup) {

        CRef<CSeqDB_FilterTree> ft = m_Aliases.GetFilterTree();
        if (m_OIDList.Empty()) {
            m_OIDList.Reset( new CSeqDBOIDList(m_Atlas,
                                               m_VolSet,
                                               *ft,
                                               m_UserGiList,
                                               m_NegativeList,
                                               locked,
                                               m_LMDBSet) );
        }

        m_OidListSetup = true;
        // Handle the case where FIRST_OID and LAST_OID is set on a top level
        // alias file and that's the only alias file present
        if (ft->HasFilter()) {
            const vector< CRef<CSeqDB_FilterTree> >& nodes = ft->GetNodes();
            if (nodes.size() == 1) {
                const CSeqDB_FilterTree::TFilters& filters = nodes.front()->GetFilters();
                if (filters.size() == 1 && filters.front()->GetType() == CSeqDB_AliasMask::eOidRange) {
                    const CSeqDB_AliasMask& alias_mask = *filters.front();
                    SetIterationRange(alias_mask.GetBegin(), alias_mask.GetEnd());
                }
            }
        }
        //DebugDumpText(cerr, "CSeqDBImpl after m_OIDList initialization", 10);
        //ft->Print();
    }
    m_Atlas.Unlock(locked);
}

bool CSeqDBImpl::CheckOrFindOID(int & next_oid)
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);
    return x_CheckOrFindOID(next_oid, locked);
}

bool CSeqDBImpl::x_CheckOrFindOID(int & next_oid, CSeqDBLockHold & locked)
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
                            int         oid_size,      // in
                            vector<int> & oid_list,    // out
                            int         * state_obj)   // in+out
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);

    int cacheID = (m_NumThreads) ? x_GetCacheID() : 0;

    if (! m_OidListSetup) {
        x_GetOidList(locked);
    }

   	m_Atlas.Lock(locked);
    if (! state_obj) {
        state_obj = & m_NextChunkOID;
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

    begin_chunk = * state_obj;

    // fill the cache for all sequence in mmaped slice
    if (m_NumThreads) {
        SSeqResBuffer * buffer = m_CachedSeqs[cacheID];
        x_FillSeqBuffer(buffer, begin_chunk);
        end_chunk = begin_chunk + static_cast<int>(buffer->results.size());
    } else {
        end_chunk = begin_chunk + oid_size;
    }

    if (end_chunk > m_RestrictEnd) {
        end_chunk = m_RestrictEnd;
    }
    *state_obj = end_chunk;

    // Case 2: Return a range

    if (m_OIDList.Empty()) {
        return CSeqDB::eOidRange;
    }


    // Case 3: Ones and Zeros - The bitmap provides OIDs.

    int next_oid = begin_chunk;
    if (m_NumThreads) {
        oid_list.clear();
        while(next_oid < end_chunk) {
            // Find next ordinal id, and save it if it falls within iteration range.
            if (m_OIDList->CheckOrFindOID(next_oid) &&
                next_oid < end_chunk) {
                oid_list.push_back(next_oid++);
            } else {
                next_oid = end_chunk;
                break;
            }
        }
    } else {
        int iter = 0;
        oid_list.resize(oid_size);
        while (iter < oid_size) {
            if (next_oid >= m_RestrictEnd) break;
            // Find next ordinal id, and save it if it falls within iteration range.
            if (m_OIDList->CheckOrFindOID(next_oid) &&
                next_oid < m_RestrictEnd) {
                oid_list[iter++] = next_oid++;
            } else {
                next_oid = m_RestrictEnd;
                break;
            }
        }
        if (iter < oid_size) {
            oid_list.resize(iter);
        }
        *state_obj = next_oid;
    }

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

    return x_GetSeqLength(oid);
}

int CSeqDBImpl::x_GetSeqLength(int oid) const
{
    int vol_oid = 0;

    if ('p' == m_SeqType) {
        if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
            return vol->GetSeqLengthProt(vol_oid);
        }
    } else {
        if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
            return vol->GetSeqLengthExact(vol_oid);
        }
    }

    NCBI_THROW(CSeqDBException, eArgErr, CSeqDB::kOidNotFound);
}

int CSeqDBImpl::GetSeqLengthApprox(int oid) const
{
    CHECK_MARKER();

    int vol_oid = 0;

    if ('p' == m_SeqType) {
        if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
            return vol->GetSeqLengthProt(vol_oid);
        }
    } else {
        if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
            return vol->GetSeqLengthApprox(vol_oid);
        }
    }

    NCBI_THROW(CSeqDBException, eArgErr, CSeqDB::kOidNotFound);
}

void CSeqDBImpl::GetTaxIDs(int             oid,
                           map<TGi, TTaxId> & gi_to_taxid,
                           bool            persist)
{
    CSeqDBLockHold locked(m_Atlas);
    m_Atlas.Lock(locked);
    //m_Atlas.MentionOid(oid, m_NumOIDs, locked);

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
                           vector<TTaxId> & taxids,
                           bool          persist)
{
    CSeqDBLockHold locked(m_Atlas);
    //m_Atlas.MentionOid(oid, m_NumOIDs, locked);

    if (! persist) {
        taxids.clear();
    }

    CRef<CBlast_def_line_set> defline_set =
        x_GetHdr(oid, locked);

    if ((! defline_set.Empty()) && defline_set->CanGet()) {
        ITERATE(list< CRef<CBlast_def_line> >, defline, defline_set->Get()) {
            if ((*defline)->IsSetTaxid()) {
                taxids.push_back((*defline)->GetTaxid());
            }
//            CBlast_def_line::TTaxIds taxid_set = (*defline)->GetTaxIds();
//            taxids.insert(taxids.end(), taxid_set.begin(), taxid_set.end());
        }
    }
}

void CSeqDBImpl::GetAllTaxIDs(int           oid,
                              set<TTaxId> & taxids)
{
    CSeqDBLockHold locked(m_Atlas);

    CRef<CBlast_def_line_set> defline_set =
        x_GetHdr(oid, locked);

    if ((! defline_set.Empty()) && defline_set->CanGet()) {
        ITERATE(list< CRef<CBlast_def_line> >, defline, defline_set->Get()) {
       		CBlast_def_line::TTaxIds taxid_set = (*defline)->GetTaxIds();
        	taxids.insert(taxid_set.begin(), taxid_set.end());
        }
    }
}

void CSeqDBImpl::GetLeafTaxIDs(
        int                  oid,
        map<TGi, set<TTaxId> >& gi_to_taxid_set,
        bool                 persist
)
{
    CSeqDBLockHold locked(m_Atlas);
    m_Atlas.Lock(locked);
    //m_Atlas.MentionOid(oid, m_NumOIDs, locked);

    if (! persist) {
        gi_to_taxid_set.clear();
    }

    CRef<CBlast_def_line_set> defline_set =
        x_GetHdr(oid, locked);

    if ((! defline_set.Empty()) && defline_set->CanGet()) {
        ITERATE(list< CRef<CBlast_def_line> >, defline, defline_set->Get()) {
            if (! (*defline)->CanGetSeqid()) {
                continue;
            }

            ITERATE(list< CRef<CSeq_id> >, seqid, (*defline)->GetSeqid()) {
                if (! (**seqid).IsGi()) {
                    continue;
                }

                CBlast_def_line::TTaxIds taxids = (*defline)->GetLeafTaxIds();
                gi_to_taxid_set[(**seqid).GetGi()].insert(
                        taxids.begin(), taxids.end()
                );
            }
        }
    }
}

void CSeqDBImpl::GetLeafTaxIDs(
        int          oid,
        vector<TTaxId>& taxids,
        bool         persist
)
{
    CSeqDBLockHold locked(m_Atlas);
    m_Atlas.Lock(locked);
    //m_Atlas.MentionOid(oid, m_NumOIDs, locked);

    if (! persist) {
        taxids.clear();
    }

    CRef<CBlast_def_line_set> defline_set = x_GetHdr(oid, locked);

    if ((! defline_set.Empty())  &&  defline_set->CanGet()) {
        ITERATE(
                list<CRef<CBlast_def_line> >,
                defline,
                defline_set->Get()
        ) {
            if ((*defline)->CanGetSeqid()) {
                ITERATE(
                        list<CRef<CSeq_id> >,
                        seqid,
                        (*defline)->GetSeqid()
                ) {
                    if ((**seqid).IsGi()) {
                        CBlast_def_line::TTaxIds leafTaxids =
                                (*defline)->GetLeafTaxIds();
                        taxids.insert(
                                taxids.end(),
                                leafTaxids.begin(),
                                leafTaxids.end()
                        );
                    }
                }
            }
        }
    }
}

CRef<CBioseq>
CSeqDBImpl::GetBioseq(int oid, TGi target_gi, const CSeq_id * target_seq_id, bool seqdata)
{
    CHECK_MARKER();

    CSeqDBLockHold locked(m_Atlas);
    m_Atlas.Lock(locked);
    //m_Atlas.MentionOid(oid, m_NumOIDs, locked);

    int vol_oid = 0;

    if (! m_OidListSetup) {
        x_GetOidList(locked);
    }
    m_Atlas.Unlock(locked);

    if (CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetBioseq(vol_oid,
                              target_gi,
                              target_seq_id,
                              seqdata,
                              locked);
    }

    NCBI_THROW(CSeqDBException, eArgErr, CSeqDB::kOidNotFound);
}

void CSeqDBImpl::RetSequence(const char ** buffer)
{
    CHECK_MARKER();

    if (m_NumThreads) {
        int cacheID = x_GetCacheID();
        (m_CachedSeqs[cacheID]->checked_out)--;
        *buffer = 0;
        return;
    }

    // This returns a reference to part of a memory mapped region.

    //m_Atlas.Lock(locked);

    //m_Atlas.RetRegion(*buffer);
    *buffer = 0;
}

void CSeqDBImpl::RetAmbigSeq(const char ** buffer) const
{
    CSeqDBAtlas::RetRegion(*buffer);//Keep this
    *buffer = 0;
}

void CSeqDBImpl::x_RetSeqBuffer(SSeqResBuffer * buffer) const
{
    // client must return sequence before getting a new one
    if (buffer->checked_out > 0) {
        NCBI_THROW(CSeqDBException, eArgErr, "Sequence not returned.");
    }

    buffer->checked_out = 0;
    buffer->results.clear();
}

int CSeqDBImpl::x_GetSeqBuffer(SSeqResBuffer * buffer, int oid,
                               const char ** seq) const
{
    // Search local cache for oid
    Uint4 index = oid - buffer->oid_start;
    if (index < buffer->results.size()) {
        (buffer->checked_out)++;
        *seq = buffer->results[index].address;
        return buffer->results[index].length;
    }

    x_FillSeqBuffer(buffer, oid);
    (buffer->checked_out)++;
    *seq = buffer->results[0].address;
    return buffer->results[0].length;
}

void CSeqDBImpl::x_FillSeqBuffer(SSeqResBuffer  *buffer,
                                 int             oid) const
{
    // clear the buffer first
    x_RetSeqBuffer(buffer);

    buffer->oid_start = oid;
    Int4 vol_oid = 0;

    // Get all sequences within the lease
    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        SSeqRes res;
        const char * seq;
        Int8 tot_length = m_Atlas.GetSliceSize() / (4*m_NumThreads) + 1;

        res.length = vol->GetSequence(vol_oid++, &seq);
        if (res.length < 0) return;
        // must return at least one sequence
        do {
            tot_length -= res.length;
            res.address = seq;
            buffer->results.push_back(res);
            res.length = vol->GetSequence(vol_oid++, &seq);
        } while (res.length >= 0 && tot_length >= res.length && vol_oid < m_RestrictEnd);

        return;
    }

    NCBI_THROW(CSeqDBException, eArgErr, CSeqDB::kOidNotFound);
}

int CSeqDBImpl::GetSequence(int oid, const char ** buffer)
{
    CHECK_MARKER();
    if (m_NumThreads) {
        int cacheID = x_GetCacheID();
        return x_GetSeqBuffer(m_CachedSeqs[cacheID], oid, buffer);
    }

    int vol_oid = 0;

    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetSequence(vol_oid, buffer);
    }

    NCBI_THROW(CSeqDBException, eArgErr, CSeqDB::kOidNotFound);
}

CRef<CSeq_data> CSeqDBImpl::GetSeqData(int     oid,
                                       TSeqPos begin,
                                       TSeqPos end) const
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);
    int vol_oid = 0;

    m_Atlas.Lock(locked);
    //m_Atlas.MentionOid(oid, m_NumOIDs, locked);

    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetSeqData(vol_oid, begin, end, locked);
    }

    NCBI_THROW(CSeqDBException, eArgErr, CSeqDB::kOidNotFound);
}

int CSeqDBImpl::GetAmbigSeq(int               oid,
                            char           ** buffer,
                            int               nucl_code,
                            SSeqDBSlice     * region,
                            ESeqDBAllocType   alloc_type,
                            CSeqDB::TSequenceRanges * masks) const
{
    CHECK_MARKER();

    int vol_oid = 0;
    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetAmbigSeq(vol_oid,
                                buffer,
                                nucl_code,
                                alloc_type,
                                region,
                                masks);
    }

    NCBI_THROW(CSeqDBException, eArgErr, CSeqDB::kOidNotFound);
}

int CSeqDBImpl::GetAmbigPartialSeq(int                oid,
                                   char            ** buffer,
                                   int                nucl_code,
                                   ESeqDBAllocType    alloc_type,
                                   CSeqDB::TSequenceRanges  * partial_ranges,
                                   CSeqDB::TSequenceRanges  * masks) const
{
	   CHECK_MARKER();
	    int vol_oid = 0;
	    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
	        return vol->GetAmbigPartialSeq(vol_oid,
	                                       buffer,
	                                       nucl_code,
	                                       alloc_type,
	                                       partial_ranges,
	                                       masks);
	    }

	    NCBI_THROW(CSeqDBException, eArgErr, CSeqDB::kOidNotFound);
}

list< CRef<CSeq_id> > CSeqDBImpl::GetSeqIDs(int oid)
{
    CHECK_MARKER();
    int vol_oid = 0;

    CSeqDBLockHold locked(m_Atlas);
    m_Atlas.Lock(locked);
    //m_Atlas.MentionOid(oid, m_NumOIDs, locked);

    if (! m_OidListSetup) {
        x_GetOidList(locked);
    }

    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
	if( ! reusable_inpstr   ) {
           reusable_inpstr =  new CObjectIStreamAsnBinary ; 
	}
        return vol->GetSeqIDs(vol_oid, reusable_inpstr);
    }

    NCBI_THROW(CSeqDBException, eArgErr, CSeqDB::kOidNotFound);
}

TGi CSeqDBImpl::GetSeqGI(int oid)
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);
    return x_GetSeqGI(oid, locked);
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

Uint8 CSeqDBImpl::GetExactTotalLength()
{
    CHECK_MARKER();
    if(m_ExactTotalLength)
    	return m_ExactTotalLength;

    if(m_NeedTotalsScan) {
    	 CSeqDBLockHold locked(m_Atlas);
    	 x_ScanTotals(false, &m_NumSeqs, &m_ExactTotalLength,
    			 	  &m_MaxLength, &m_MinLength, locked);

    }
    else {
    	m_ExactTotalLength = m_TotalLength;
    }
     return m_ExactTotalLength;
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

TGi CSeqDBImpl::x_GetSeqGI(int oid, CSeqDBLockHold & locked)
{
    CHECK_MARKER();

    m_Atlas.Lock(locked);

    if (! m_OidListSetup) {
        x_GetOidList(locked);
    }
    m_Atlas.Unlock(locked);

    int vol_oid = 0;
    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        // Try lookup *.nxg first
        TGi gi = vol->GetSeqGI(vol_oid, locked);
        if (gi>=ZERO_GI) return gi;
        // Fall back to parsing deflines
        list< CRef<CSeq_id> > ids =
            vol->GetSeqIDs(vol_oid);
        ITERATE(list< CRef<CSeq_id> >, id, ids) {
            if ((**id).IsGi()) {
                return (**id).GetGi();
            }
        }
        // No GI found
        return INVALID_GI;
    }

    NCBI_THROW(CSeqDBException, eArgErr, CSeqDB::kOidNotFound);
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

int CSeqDBImpl::x_GetMaxLength() const
{
    CHECK_MARKER();
    return m_VolSet.GetMaxLength();
}

int CSeqDBImpl::x_GetMinLength() const
{
    CHECK_MARKER();
    return m_Aliases.GetMinLength(m_VolSet);
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

    string date;

    for(int i = 0; i < m_VolSet.GetNumVols(); i++) {
        string d = x_FixString( m_VolSet.GetVol(i)->GetDate() );

        if (date.empty()) {
            date = d;
        } else if (d != date) {
            try {
                CTime t1(date, CSeqDB::kBlastDbDateFormat);
                CTime t2(d, CSeqDB::kBlastDbDateFormat);

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

CRef<CBlast_def_line_set> CSeqDBImpl::GetHdr(int oid)
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);

    return x_GetHdr(oid, locked);
}

CRef<CBlast_def_line_set>
CSeqDBImpl::x_GetHdr(int oid, CSeqDBLockHold & locked)
{
    CHECK_MARKER();
    m_Atlas.Lock(locked);
    //m_Atlas.MentionOid(oid, m_NumOIDs, locked);

    if (! m_OidListSetup) {
        x_GetOidList(locked);
    }
    m_Atlas.Unlock(locked);

    int vol_oid = 0;

    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetFilteredHeader(vol_oid, locked);
    }

    NCBI_THROW(CSeqDBException, eArgErr, CSeqDB::kOidNotFound);
}

int CSeqDBImpl::GetMaxLength() const
{
    CHECK_MARKER();
    return m_MaxLength;
}

int CSeqDBImpl::GetMinLength() const
{
    CHECK_MARKER();
    return m_MinLength;
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

// Assumes atlas is locked

void CSeqDBImpl::FlushSeqMemory()
{
    m_VolSet.UnLease();
}

bool CSeqDBImpl::PigToOid(int pig, int & oid) const
{
    CHECK_MARKER();

    for(int i = 0; i < m_VolSet.GetNumVols(); i++) {
        if (m_VolSet.GetVol(i)->PigToOid(pig, oid)) {
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

    NCBI_THROW(CSeqDBException, eArgErr, CSeqDB::kOidNotFound);
}

bool CSeqDBImpl::TiToOid(Int8 ti, int & oid)
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);

    if (! m_OidListSetup) {
        x_GetOidList(locked);
    }

    for(int i = 0; i < m_VolSet.GetNumVols(); i++) {
        if (m_VolSet.GetVol(i)->TiToOid(ti,
                                        oid,
                                        locked)) {
            oid += m_VolSet.GetVolOIDStart(i);
            return true;
        }
    }

    return false;
}

bool CSeqDBImpl::GiToOid(TGi gi, int & oid) const
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

template <typename T > void CSeqDBImpl::x_CheckOid(T & list, CSeqDBLockHold & locked)
{
	for (unsigned i =0; i < list.size(); i++) {
		blastdb::TOid oid1 = list[i].oid;
		if(oid1 == kSeqDBEntryNotFound) {
			continue;
		}
		blastdb::TOid oid2 = oid1;
		if (!(x_CheckOrFindOID(oid2, locked) && (oid1 == oid2))) {
		    	list[i].oid = kSeqDBEntryNotFound;
		}
	}

}

bool CSeqDBImpl::IdsToOids(CSeqDBGiList & id_list)
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);
    try {
    	for(int i = 0; i < m_VolSet.GetNumVols(); i++) {
    		m_VolSet.GetVol(i)->IdsToOids(id_list);
    	}

    	if (id_list.GetNumGis() > 0) {
    		x_CheckOid(id_list.GetGiList(), locked);
    	}
    	if (id_list.GetNumPigs() > 0) {
    		x_CheckOid(id_list.GetPigList(), locked);
    	}
    	if (id_list.GetNumTis() > 0) {
    		x_CheckOid(id_list.GetTiList(), locked);
    	}

    } catch (CException &) {
    	return false;
    }
    return true;
}



bool CSeqDBImpl::GiToOidwFilterCheck(TGi gi, int & oid)
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);

    for(int i = 0; i < m_VolSet.GetNumVols(); i++) {
    	oid =-1;
        if (m_VolSet.GetVol(i)->GiToOid(gi, oid, locked)) {
           	oid += m_VolSet.GetVolOIDStart(i);
            int oid0 = oid;
            if (CheckOrFindOID(oid) && (oid==oid0)) {
            	return true;
            }
        }
    }
    return false;
}

bool CSeqDBImpl::OidToGi(int oid, TGi & gi)
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);

    if (! m_OidListSetup) {
        x_GetOidList(locked);
    }

    int vol_oid(0);

    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetGi(vol_oid,  gi, locked);
    }

    NCBI_THROW(CSeqDBException, eArgErr, CSeqDB::kOidNotFound);
}

void CSeqDBImpl::AccessionToOids(const string & acc,
                                 vector<int>  & oids)
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);

    if (! m_OidListSetup) {
        x_GetOidList(locked);
    }

    oids.clear();
    if (m_LMDBSet.IsBlastDBVersion5()) {
    	vector<int>  tmp;
    	m_LMDBSet.AccessionToOids(acc, tmp);
   	    for(unsigned int i=0; i < tmp.size(); i++) {
   	    	int oid2 = tmp[i];
            if (x_CheckOrFindOID(oid2, locked) && (tmp[i] == oid2)) {
            	oids.push_back(tmp[i]);
            }
        }
    }
    else {
        vector<int> vol_oids;

        for(int vol_idx = 0; vol_idx < m_VolSet.GetNumVols(); vol_idx++) {
            // Append any additional OIDs from this volume's indices.
            m_VolSet.GetVol(vol_idx)->AccessionToOids(acc,
                                                      vol_oids,
                                                      locked);

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
}


void CSeqDBImpl::TaxIdsToOids(set<TTaxId>& tax_ids, vector<blastdb::TOid>& rv)
{
    CHECK_MARKER();
    rv.clear();
    vector<blastdb::TOid> oids;
    if (m_LMDBSet.IsBlastDBVersion5()) {
    	m_LMDBSet.TaxIdsToOids(tax_ids,oids);
    	CSeqDBLockHold locked(m_Atlas);
    	for(unsigned int i=0; i < oids.size(); i++) {
    		blastdb::TOid oid2 = oids[i];
    		if (x_CheckOrFindOID(oid2, locked) && (oids[i] == oid2)) {
    			rv.push_back(oids[i]);
    		}
    	}
    }
    else {
    	NCBI_THROW(CSeqDBException, eArgErr,
    			           "Taxonomy list is not supported in v4 BLAST db");
    }
    return;
}

void CSeqDBImpl::GetDBTaxIds(set<TTaxId> & tax_ids)
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);

    if (! m_OidListSetup) {
        x_GetOidList(locked);
    }
    tax_ids.clear();
    if (m_LMDBSet.IsBlastDBVersion5()) {
    	if(m_OIDList.NotEmpty()){
    		vector<blastdb::TOid>  oids;
    	    for(int oid = 0; CheckOrFindOID(oid); oid++) {
    	    	oids.push_back(oid);
    	    }
    		m_LMDBSet.GetTaxIdsForOids(oids, tax_ids);
    	}
    	else {
    		m_LMDBSet.GetDBTaxIds(tax_ids);
    	}
    }
    else {
    	NCBI_THROW(CSeqDBException, eArgErr,
    			           "Taxonomy list is not supported in v4 BLAST db");
    }
    return;
}

void CSeqDBImpl::GetTaxIdsForOids(const vector<blastdb::TOid> & oids, set<TTaxId> & tax_ids)
{
    if (m_LMDBSet.IsBlastDBVersion5()) {
   		m_LMDBSet.GetTaxIdsForOids(oids, tax_ids);
    }
    else {
    	NCBI_THROW(CSeqDBException, eArgErr,
    			           "Taxonomy list is not supported in v4 BLAST db");
    }

}

void CSeqDBImpl::GetAccessionsForOid(const blastdb::TOid  oid, vector<string> & accs)
{
    if (m_LMDBSet.IsBlastDBVersion5()) {
   		m_LMDBSet.GetAccessionsForOid(oid, accs);
    }
    else {
    	NCBI_THROW(CSeqDBException, eArgErr,
    			           "GetAccessions for oid not supported for v4 BLAST db");
    }

}

bool CSeqDBImpl::CheckDuplicateIDs(vector<string> & ids) const
{
	if (m_LMDBSet.IsBlastDBVersion5()) {
		return(m_LMDBSet.CheckDuplicateIDs(ids));
	}
	else {
	  	NCBI_THROW(CSeqDBException, eArgErr,
	   			           "Check duplicate id not supported for v4 BLAST db");
	}
}

void CSeqDBImpl::AccessionsToOids(const vector<string>& accs, vector<blastdb::TOid>& oids)
{
    CHECK_MARKER();
    oids.clear();
    oids.resize(accs.size());
    if (m_LMDBSet.IsBlastDBVersion5()) {
    	m_LMDBSet.AccessionsToOids(accs,oids);
    	CSeqDBLockHold locked(m_Atlas);
    	for(unsigned int i=0; i < oids.size(); i++) {
    		if(oids[i] == kSeqDBEntryNotFound) {
    			continue;
    		}
    		blastdb::TOid oid2 = oids[i];
    		if (!(x_CheckOrFindOID(oid2, locked) && (oids[i] == oid2))) {
    			oids[i] = kSeqDBEntryNotFound;
    		}
    	}
    }
    else {
    	for(unsigned int i=0; i < accs.size(); i++) {
    		vector<blastdb::TOid> tmp;
    		AccessionToOids(accs[i], tmp);
    		if(tmp.empty()) {
    			oids[i] = kSeqDBEntryNotFound;
    		}
    		else {
    			oids[i] = tmp[0];
    		}
    	}
    }
    return;
}


void CSeqDBImpl::SeqidToOids(const CSeq_id & seqid_in,
                             vector<int>   & oids,
                             bool            multi)
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);

    if (! m_OidListSetup) {
        x_GetOidList(locked);
    }

    oids.clear();

    bool is_BL_ORD_ID = false;
    if(seqid_in.Which() == CSeq_id::e_General)
    {
       const CDbtag & dbt = seqid_in.GetGeneral();
       if (dbt.CanGetDb()) {
            if (dbt.GetDb() == "BL_ORD_ID") {
                    is_BL_ORD_ID = true;
            }
       }
    }

    if (m_LMDBSet.IsBlastDBVersion5() && (!is_BL_ORD_ID)) {
    	if(IsStringId(seqid_in)) {
    		vector<int>  tmp;
    		if(seqid_in.IsPir() || seqid_in.IsPrf()) {
    			m_LMDBSet.AccessionToOids(seqid_in.AsFastaString(), tmp);
    		}
    		else {
    			m_LMDBSet.AccessionToOids(seqid_in.GetSeqIdString(true), tmp);
    		}
    		for(unsigned int i=0; i < tmp.size(); i++) {
    			int oid2 = tmp[i];
    			if (x_CheckOrFindOID(oid2, locked) && (tmp[i] == oid2)) {
    				oids.push_back(tmp[i]);
    			}
    		}
    		return;
    	}
    }

    vector<int> vol_oids;

    // The lower level functions modify the seqid - namely, changing
    // or clearing certain fields before printing it to a string.
    // Further analysis of data and exception flow might reveal that
    // the Seq_id will always be returned to the original state by
    // this operation... At the moment, safest route is to clone it.
    CSeq_id seqid;
    seqid.Assign(seqid_in);

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
                            vector<string> & paths,
                            vector<string> * alias_paths,
                            bool             recursive,
                            bool             expand_links)
{
    bool use_atlas_lock = true;
    CSeqDBAtlasHolder AH(NULL, use_atlas_lock);
    CSeqDBAtlas & atlas(AH.Get());

    // This constructor handles its own locking.
    CSeqDBAliasFile aliases(atlas, dbname, prot_nucl, expand_links);
    aliases.FindVolumePaths(paths, alias_paths, recursive);
}

void
CSeqDBImpl::FindVolumePaths(vector<string> & paths, bool recursive) const
{
    CHECK_MARKER();
    m_Aliases.FindVolumePaths(paths, NULL, recursive);
}

void
CSeqDBImpl::FindVolumePaths(vector<string> & paths, vector<string> & alias, bool recursive) const
{
    CHECK_MARKER();
    m_Aliases.FindVolumePaths(paths, &alias, recursive);
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
                              int            * maxlen,
                              int            * minlen,
                              CSeqDBLockHold & locked)
{
    int   oid_count(0);
    Uint8 base_count(0);
    int   max_count(0);
    int   min_count(INT4_MAX);

    const CSeqDBVol * volp = 0;

    for(int oid = 0; x_CheckOrFindOID(oid, locked); oid++) {
        ++ oid_count;

        int vol_oid = 0;

        volp = m_VolSet.FindVol(oid, vol_oid);

        _ASSERT(volp);

        if (totlen || maxlen || minlen) {
            int len;
            if ('p' == m_SeqType) {
                len = volp->GetSeqLengthProt(vol_oid);
            } else {
                if (approx) {
                    len = volp->GetSeqLengthApprox(vol_oid);
                } else {
                    len = volp->GetSeqLengthExact(vol_oid);
                }
            }
            max_count = max(len, max_count);
            min_count = min(len, min_count);
            base_count += len;
        }
    }

    if (numseq) {
        *numseq = oid_count;
    }

    if (totlen) {
        *totlen = base_count;
    }

    if (maxlen) {
        *maxlen = max_count;
    }

    if (minlen) {
        *minlen = min_count;
    }
}

void CSeqDBImpl::GetTaxInfo(TTaxId taxid, SSeqDBTaxInfo & info)
{
    if (! CSeqDBTaxInfo::GetTaxNames(taxid, info)) {
        CNcbiOstrstream oss;
        oss << "Taxid " << taxid << " not found";
        string msg = CNcbiOstrstreamToString(oss);
        NCBI_THROW(CSeqDBException, eArgErr, msg);
    }
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
        x_ScanTotals(use_approx, oid_count, total_length, NULL, NULL, locked);
        break;
    }
}

void CSeqDBImpl::GetRawSeqAndAmbig(int           oid,
                                   const char ** buffer,
                                   int         * seq_length,
                                   int         * ambig_length) const
{
    CHECK_MARKER();

    int vol_oid = 0;

    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        vol->GetRawSeqAndAmbig(vol_oid,
                               buffer,
                               seq_length,
                               ambig_length);

        return;
    }

    NCBI_THROW(CSeqDBException, eArgErr, CSeqDB::kOidNotFound);
}

/// Accumulate optional min, max, and count.
///
/// This generic template describes the accumulation of low, high, and
/// count values over any ordered type.  The low_in, high_in, and
/// count_in values are compared to the current values of low_out,
/// high_out, and count_out.  If the new value is lower than low_out,
/// it will replace the value in low_out, and similarly for high_out.
/// The count_out value just accumulates counts.  If any of the *_out
/// fields is NULL, that field will not be processed.  If the set_all
/// flag is true, the values in *_in are simply copied unchanged to
/// the corresponding *_out fields.
///
/// @param low_in    The low value for one volume. [in]
/// @param high_in   The high value for one volume. [in]
/// @param count_in  The ID count for one volume. [in]
/// @param low_out   If non-null, the low output value. [out]
/// @param high_out  If non-null, the high output value. [out]
/// @param count_out If non-null, the ID output value. [out]
/// @param set_all   If true, adopt values without testing. [in]
template<class TId>
inline void s_AccumulateMinMaxCount(TId   low_in,
                                    TId   high_in,
                                    int   count_in,
                                    TId * low_out,
                                    TId * high_out,
                                    int * count_out,
                                    bool  set_all)
{
    if (set_all) {
        if (low_out)
            *low_out = low_in;

        if (high_out)
            *high_out = high_in;

        if (count_out)
            *count_out = count_in;
    } else {
        if (low_out && (*low_out > low_in)) {
            *low_out = low_in;
        }
        if (high_out && (*high_out < high_in)) {
            *high_out = high_in;
        }
        if (count_out) {
            *count_out += count_in;
        }
    }
}

void CSeqDBImpl::GetGiBounds(TGi * low_id,
                             TGi * high_id,
                             int * count)
{
    CSeqDBLockHold locked(m_Atlas);

    bool found = false;

    for(int i = 0; i < m_VolSet.GetNumVols(); i++) {
        TGi vlow(ZERO_GI), vhigh(ZERO_GI);
        int vcount(0);

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
    bool found = false;

    for(int i = 0; i < m_VolSet.GetNumVols(); i++) {
        string vlow, vhigh;
        int vcount(0);

        m_VolSet.GetVol(i)->GetStringBounds(vlow, vhigh, vcount);

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
    int vol_oid = 0;

    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        vol->SetOffsetRanges(vol_oid,
                             offset_ranges,
                             append_ranges,
                             cache_data);
    } else {
        NCBI_THROW(CSeqDBException, eArgErr, CSeqDB::kOidNotFound);
    }
}

void CSeqDBImpl::FlushOffsetRangeCache()
{
    CHECK_MARKER();

    for(int vol_idx = 0; vol_idx < m_VolSet.GetNumVols(); vol_idx++) {
        CSeqDBVol* volp = m_VolSet.GetVolNonConst(vol_idx);
        volp->FlushOffsetRangeCache();
    }
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

    RetAmbigSeq(const_cast<const char**>(& datap));

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

void CSeqDBImpl::x_InitIdSet()
{
    if (m_IdSet.Blank()) {
        if (! m_UserGiList.Empty()) {
            // Note: this returns a 'blank' IdSet list for positive
            // lists that specify filtering using CSeq-id objects.

            if (m_UserGiList->GetNumGis()) {
                vector<TGi> gis;
                m_UserGiList->GetGiList(gis);

                CSeqDBIdSet new_ids(gis, CSeqDBIdSet::eGi);
                m_IdSet = new_ids;
            } else if (m_UserGiList->GetNumTis()) {
                vector<TTi> tis;
                m_UserGiList->GetTiList(tis);

                CSeqDBIdSet new_ids(tis, CSeqDBIdSet::eTi);
                m_IdSet = new_ids;
            }
        } else if (! m_NegativeList.Empty()) {
            const vector<TGi> & ngis = m_NegativeList->GetGiList();
            const vector<TTi> & ntis = m_NegativeList->GetTiList();
            const vector<string> & stis = m_NegativeList->GetSiList();

            if (! ngis.empty()) {
                CSeqDBIdSet new_ids(ngis, CSeqDBIdSet::eGi, false);
                m_IdSet = new_ids;
            } else if (! ntis.empty()) {
                CSeqDBIdSet new_ids(ntis, CSeqDBIdSet::eTi, false);
                m_IdSet = new_ids;
            } else if (!stis.empty()) {
                CSeqDBIdSet new_ids(stis, CSeqDBIdSet::eSi, false);
                m_IdSet = new_ids;
            }
        }
    }
}

CSeqDBIdSet CSeqDBImpl::GetIdSet()
{
	return m_IdSet;
}

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
void CSeqDBImpl::ListColumns(vector<string> & titles)
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);
    m_Atlas.Lock(locked);

    set<string> all;

    for(int vol_idx = 0; vol_idx < m_VolSet.GetNumVols(); vol_idx++) {
        m_VolSet.GetVolNonConst(vol_idx)->ListColumns(all, locked);
    }

    titles.assign(all.begin(), all.end());
}

int CSeqDBImpl::GetColumnId(const string & title)
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);

    return x_GetColumnId(title, locked);
}

int CSeqDBImpl::x_GetColumnId(const string   & title,
                              CSeqDBLockHold & locked)
{
    m_Atlas.Lock(locked);

    int col_id = SeqDB_MapFind(m_ColumnTitleMap, title, (int) kUnknownTitle);

    if (col_id == kUnknownTitle) {
        vector<int> vol_ids;

        bool found = false;

        for(int vol_idx = 0; vol_idx < m_VolSet.GetNumVols(); vol_idx++) {
            CSeqDBVol * volp = m_VolSet.GetVolNonConst(vol_idx);
            int id = volp->GetColumnId(title, locked);

            vol_ids.push_back(id);

            if (id >= 0) {
                found = true;
            }
        }

        if (found) {
            CRef<CSeqDB_ColumnEntry> obj(new CSeqDB_ColumnEntry(vol_ids));

            col_id = static_cast<int>(m_ColumnInfo.size());
            m_ColumnInfo.push_back(obj);
        } else {
            col_id = kColumnNotFound;
        }

        // Cache this lookup even if it failed (-1).

        m_ColumnTitleMap[title] = col_id;
    }

    return col_id;
}

const map<string,string> &
CSeqDBImpl::GetColumnMetaData(int column_id)
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);
    m_Atlas.Lock(locked);

    _ASSERT(column_id >= 0);
    _ASSERT(column_id < (int)m_ColumnInfo.size());
    CSeqDB_ColumnEntry & entry = *m_ColumnInfo[column_id];

    if (! entry.HaveMap()) {
        typedef map<string,string> TStringMap;

        for(int vol_idx = 0; vol_idx < m_VolSet.GetNumVols(); vol_idx++) {
            int vol_col_id = entry.GetVolumeIndex(vol_idx);

            if (vol_col_id < 0)
                continue;

            CSeqDBVol * volp = m_VolSet.GetVolNonConst(vol_idx);
            const TStringMap & volmap =
                volp->GetColumnMetaData(vol_col_id, locked);

            ITERATE(TStringMap, iter, volmap) {
                entry.SetMapValue(iter->first, iter->second);
            }
        }

        entry.SetHaveMap();
    }

    return entry.GetMap();
}

const map<string,string> &
CSeqDBImpl::GetColumnMetaData(int column_id, const string & volname)
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);
    m_Atlas.Lock(locked);

    _ASSERT(column_id >= 0);
    _ASSERT(column_id < (int)m_ColumnInfo.size());
    CSeqDB_ColumnEntry & entry = *m_ColumnInfo[column_id];

    for(int vol_idx = 0; vol_idx < m_VolSet.GetNumVols(); vol_idx++) {
        CSeqDBVol * volp = m_VolSet.GetVolNonConst(vol_idx);

        if (volname != volp->GetVolName()) {
            continue;
        }

        // Found it.

        int vol_col_id = entry.GetVolumeIndex(vol_idx);
        return volp->GetColumnMetaData(vol_col_id, locked);
    }

    NCBI_THROW(CSeqDBException,
               eArgErr,
               "This column ID was not found.");
}

void CSeqDBImpl::GetColumnBlob(int            col_id,
                               int            oid,
                               bool           keep,
                               CBlastDbBlob & blob)
{
    CHECK_MARKER();

    // This Clear() must be done outside of the Lock()ed section below
    // to avoid possible self-deadlock.  In general, do not clear or
    // allow the destruction of a blob that may have an attached
    // 'lifetime' object while the atlas lock is held.

    blob.Clear();

    CSeqDBLockHold locked(m_Atlas);
    m_Atlas.Lock(locked);

    _ASSERT(col_id >= 0);
    _ASSERT(col_id < (int)m_ColumnInfo.size());
    CSeqDB_ColumnEntry & entry = *m_ColumnInfo[col_id];

    // Find the volume for this OID.

    int vol_idx = -1, vol_oid = -1;

    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid, vol_idx)) {
        int vol_col_id = entry.GetVolumeIndex(vol_idx);

        if (vol_col_id >= 0) {
            const_cast<CSeqDBVol *>(vol)->
                GetColumnBlob(vol_col_id, vol_oid, blob, keep, locked);
        }
    }
}

int CSeqDBImpl::x_GetMaskDataColumn(CSeqDBLockHold & locked)
{
    m_Atlas.Lock(locked);

    if (m_MaskDataColumn == kUnknownTitle) {
        m_MaskDataColumn = x_GetColumnId("BlastDb/MaskData", locked);
    }

    _ASSERT(m_MaskDataColumn != kUnknownTitle);

    return m_MaskDataColumn;
}
#endif


template<class K, class C>
bool s_Contains(const C & c, const K & k)
{
    return c.find(k) != c.end();
}

static bool s_IsNumericId(const string &id) {
    Int4 nid(-1);
    return NStr::StringToNumeric(id, &nid, NStr::fConvErr_NoThrow, 10);
}

static const string* s_CheckUniqueValues(const map<string, string> & m)
{
    typedef map<string, string> TStringMap;

    set<string> seen;

    ITERATE(TStringMap, iter, m) {
        string v = iter->second;
        vector<string> items;
        NStr::Split(v, ":", items);

        if (items.size() == 4) {
            v = items[2];
        }

        if (s_Contains(seen, v)) {
            return & iter->second;
        }

        seen.insert(v);
    }

    return NULL;
}

CSeqDB_IdRemapper::CSeqDB_IdRemapper()
    : m_NextId(100), m_Empty(true), m_CacheRealAlgo(-1)
{
}

void CSeqDB_IdRemapper::GetIdList(vector<int> & algorithms)
{
    typedef map<int,string> TIdMap;

    ITERATE(TIdMap, iter, m_IdToDesc) {
        algorithms.push_back(iter->first);
    }
}

void CSeqDB_IdRemapper::AddMapping(int vol_id, int id, const string & desc)
{
    string real_desc = desc;
    vector<string> items;
    NStr::Split(desc, ":", items);
    if (items.size() == 4) {
        real_desc = items[2];
    }
    bool found_desc = s_Contains(m_DescToId, real_desc);
    bool found_id   = s_Contains(m_IdToDesc, id);

    int real_id = id;

    if (found_desc) {
        if ((! found_id) || (m_DescToId[real_desc] != id)) {
            // This description is mapped to a different ID.
            real_id = m_DescToId[real_desc];
        }
    } else {
        // New description.

        if (found_id) {
            // Pick a 'synthetic' ID for this description,
            // i.e. one that is not actually used by any of
            // the existing volumes so far.

            while(s_Contains(m_IdToDesc, m_NextId)) {
                m_NextId++;
            }

            real_id = m_NextId;
        }

        // Add the new description.

        m_IdToDesc[real_id] = desc;
        m_DescToId[real_desc] = real_id;
    }

    m_RealIdToVolumeId[vol_id][real_id] = id;
}

bool CSeqDB_IdRemapper::GetDesc(int algorithm_id, string & desc)
{
    if (! s_Contains(m_IdToDesc, algorithm_id)) {
       return false;
    }

    desc = m_IdToDesc[algorithm_id];
    return true;
}

int CSeqDB_IdRemapper::GetVolAlgo(int vol_idx, int algo_id)
{
    if (algo_id != m_CacheRealAlgo || vol_idx != m_CacheVolIndex) {
        m_CacheVolIndex = vol_idx;
        m_CacheRealAlgo = algo_id;
        m_CacheVolAlgo = RealToVol(vol_idx, algo_id);
    }
    return m_CacheVolAlgo;
}

int CSeqDB_IdRemapper::RealToVol(int vol_idx, int algo_id)
{
    if (! s_Contains(m_RealIdToVolumeId, vol_idx)) {
        NCBI_THROW(CSeqDBException, eArgErr,
                   "Cannot find volume in algorithm map.");
    }

    map<int,int> & trans = m_RealIdToVolumeId[vol_idx];

    if (! s_Contains(trans, algo_id)) {
        NCBI_THROW(CSeqDBException, eArgErr,
                   "Cannot find volume algorithm in algorithm map.");
    }

    return trans[algo_id];
}

int CSeqDB_IdRemapper::GetAlgoId(const string & id)
{
    if (! s_Contains(m_DescToId, id)) {
        NCBI_THROW(CSeqDBException, eArgErr,
                   "Cannot find string algorithm id in algorithm map.");
    }

    return m_DescToId[id];
}

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
void CSeqDBImpl::GetAvailableMaskAlgorithms(vector<int> & algorithms)
{
    if (m_UseGiMask) {
        m_GiMask->GetAvailableMaskAlgorithms(algorithms);
        return;
    }

    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);
    m_Atlas.Lock(locked);

    if (m_AlgorithmIds.Empty()) {
        x_BuildMaskAlgorithmList(locked);
    }

    algorithms.resize(0);
    m_AlgorithmIds.GetIdList(algorithms);
}

int CSeqDBImpl::GetMaskAlgorithmId(const string & algo)
{
    if (m_UseGiMask) {
        return m_GiMask->GetAlgorithmId(algo);
    }

    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);
    m_Atlas.Lock(locked);

    if (m_AlgorithmIds.Empty()) {
        x_BuildMaskAlgorithmList(locked);
    }

    return m_AlgorithmIds.GetAlgoId(algo);
}

string CSeqDBImpl::GetAvailableMaskAlgorithmDescriptions()
{
    vector<int> algorithms;
    GetAvailableMaskAlgorithms(algorithms);
    if (algorithms.empty()) {
        return kEmptyStr;
    }

    CNcbiOstrstream retval;
    retval << endl
           << "Available filtering algorithms applied to database sequences:"
           << endl << endl;

    retval << setw(13) << left << "Algorithm ID"
           << setw(40) << left << "Algorithm name"
           << setw(40) << left << "Algorithm options" << endl;
    ITERATE(vector<int>, algo_id, algorithms) {
        string algo, algo_opts, algo_name;
        GetMaskAlgorithmDetails(*algo_id, algo, algo_name, algo_opts);
        if (algo_opts.empty()) {
            algo_opts.assign("default options used");
        }
        if (s_IsNumericId(algo)) {
            retval << setw(13) << left << (*algo_id)
                   << setw(40) << left << algo_name
                   << setw(40) << left << algo_opts << endl;
        } else {
            retval << setw(13) << left << (*algo_id)
                   << setw(40) << left << algo
                   << setw(40) << left << algo_opts << endl;
        }
    }
    return CNcbiOstrstreamToString(retval);
}

static const string s_RestoreColon(const string &in) {
    const char l = 0x1;
    return NStr::Replace(in, string(l,1), ":");
}

static
void s_GetDetails(const string          & desc,
                  string                & program,
                  string                & program_name,
                  string                & algo_opts)
{
    static const CEnumeratedTypeValues* enum_type_vals = NULL;
    if (enum_type_vals == NULL) {
        enum_type_vals = GetTypeInfo_enum_EBlast_filter_program();
    }
    _ASSERT(enum_type_vals);

    vector<string> items;
    NStr::Split(desc, ":", items);

    if (items.size() == 2) {
        EBlast_filter_program
           pid = (EBlast_filter_program) NStr::StringToInt(items[0]);
        program.assign(items[0]);
        program_name.assign(enum_type_vals->FindName(pid, false));
        algo_opts.assign(s_RestoreColon(items[1]));
    } else if (items.size() == 4) {
        program.assign(s_RestoreColon(items[0]));
        program_name.assign(s_RestoreColon(items[2]));
        algo_opts.assign(s_RestoreColon(items[1]));
    } else {
        NCBI_THROW(CSeqDBException, eArgErr,
                   "Error in stored mask algorithm description data.");
    }
}

void CSeqDBImpl::GetMaskAlgorithmDetails(int                 algorithm_id,
                         string            & program,
                         string            & program_name,
                         string            & algo_opts)
{
    CHECK_MARKER();
    CSeqDBLockHold locked(m_Atlas);
    m_Atlas.Lock(locked);

    string s;
    bool found;

    if (m_UseGiMask) {
       // TODO:  to get description s
       s = m_GiMask->GetDesc(algorithm_id, locked);
       found = true;
    } else {
       if (m_AlgorithmIds.Empty()) {
           x_BuildMaskAlgorithmList(locked);
       }
       found = m_AlgorithmIds.GetDesc(algorithm_id, s);
    }

    if (found == false) {
        CNcbiOstrstream oss;
        oss << "Filtering algorithm ID " << algorithm_id
            << " is not supported." << endl;
        oss << GetAvailableMaskAlgorithmDescriptions();
        NCBI_THROW(CSeqDBException, eArgErr, CNcbiOstrstreamToString(oss));
    }

    s_GetDetails(s, program, program_name, algo_opts);
}

void CSeqDBImpl::x_BuildMaskAlgorithmList(CSeqDBLockHold & locked)
{
    m_Atlas.Lock(locked);

    if (! m_AlgorithmIds.Empty()) {
        return;
    }

    int col_id = x_GetMaskDataColumn(locked);

    if (col_id < 0) {
        // No mask data column exists, therefore, the algorithms list
        // is empty, and we are done.
        return;
    }

    CSeqDB_ColumnEntry & entry = *m_ColumnInfo[col_id];

    typedef map<string,string> TStringMap;

    // Results needed:
    // 1. Map global ids to desc.
    // 2. Map local id+vol -> global id

    for(int vol_idx = 0; vol_idx < m_VolSet.GetNumVols(); vol_idx++) {
        // Get volume column #.
        int vol_col_id = entry.GetVolumeIndex(vol_idx);

        if (vol_col_id < 0) {
            continue;
        }

        CSeqDBVol * volp = m_VolSet.GetVolNonConst(vol_idx);
        const TStringMap & volmap =
            volp->GetColumnMetaData(vol_col_id, locked);

        // Check for identical algorithm descriptions (should not happen.)

        const string * dup = s_CheckUniqueValues(volmap);

        if (dup != NULL) {
            ostringstream oss;
            oss << "Error: volume (" << volp->GetVolName()
                << ") mask data has duplicates value (" << *dup <<  ")";

            NCBI_THROW(CSeqDBException, eArgErr, oss.str());
        }

        ITERATE(TStringMap, iter, volmap) {
            int id1 = NStr::StringToInt(iter->first);
            const string & desc1 = iter->second;

            m_AlgorithmIds.AddMapping(vol_idx, id1, desc1);
        }
    }

    m_AlgorithmIds.SetNotEmpty();
}

struct SReadInt4 {
    enum { numeric_size = 4 };

    static int Read(CBlastDbBlob & blob)
    {
        return blob.ReadInt4();
    }

    static void Read(CBlastDbBlob & blob, int n,
                     CSeqDB::TSequenceRanges & ranges)
    {
        const void * src = (const void *) blob.ReadRaw(n*8);
        ranges.append(src, n);
    }
};

template<class TRead>
void s_ReadRanges(int                       vol_algo,
                  CSeqDB::TSequenceRanges & ranges,
                  CBlastDbBlob            & blob)
{
    int num_ranges = TRead::Read(blob);

    for(int rng = 0; rng < num_ranges; rng++) {
        int algo = TRead::Read(blob);
        int num_pairs = TRead::Read(blob);
        if (algo == vol_algo) {
            TRead::Read(blob, num_pairs, ranges);
            break;
        }
        int skip_amt = num_pairs * 2 * TRead::numeric_size;
        blob.SeekRead(blob.GetReadOffset() + skip_amt);
    }
}

void CSeqDBImpl::GetMaskData(int                       oid,
                             int                       algo_id,
                             CSeqDB::TSequenceRanges & ranges)
{
    CHECK_MARKER();

    // This reads the data written by CWriteDB_Impl::SetMaskData
    ranges.clear();

    CSeqDBLockHold locked(m_Atlas);
    m_Atlas.Lock(locked);

    if (m_UseGiMask) {
        m_GiMask->GetMaskData(algo_id, x_GetSeqGI(oid, locked), ranges, locked);
        return;
    }

    if (m_AlgorithmIds.Empty()) {
        x_BuildMaskAlgorithmList(locked);
    }

    int vol_oid = 0, vol_idx = -1;

    CSeqDBVol * vol = const_cast<CSeqDBVol*>
        (m_VolSet.FindVol(oid, vol_oid, vol_idx));

    if (! vol) {
        NCBI_THROW(CSeqDBException, eArgErr, CSeqDB::kOidNotFound);
    }

    // Get the data.

    CBlastDbBlob blob;
    vol->GetColumnBlob(x_GetMaskDataColumn(locked), vol_oid, blob, false, locked);

    if (blob.Size() != 0) {
        // If there actually is mask data, then we need to do the
        // algorithm translation.

        int vol_algo_id = -1;
	try {
        	vol_algo_id = m_AlgorithmIds.GetVolAlgo(vol_idx, algo_id);
	}  // indicates that masking algo not in this volume (should not be fatal)
    	catch(CSeqDBException & e) {
		return;
        }

        s_ReadRanges<SReadInt4>(vol_algo_id, ranges, blob);
    } 

    //int seq_length = 0;
}
#endif
/// Call this api in main thread before and after multi-threading
void CSeqDBImpl::SetNumberOfThreads(int num_threads, bool force_mt)
{
    CSeqDBLockHold locked(m_Atlas);
    m_Atlas.Lock(locked);

    if (num_threads < 1) {
        num_threads = 0;
    } else if (num_threads == 1) {
        num_threads = force_mt ? 1 : 0;
    }

    if (num_threads > m_NumThreads ) {

        for (int thread = m_NumThreads; thread < num_threads; ++thread) {
            m_CachedSeqs.push_back(new SSeqResBuffer());
        }

    } else if (num_threads < m_NumThreads) {

        for (int thread = num_threads; thread < m_NumThreads; ++thread) {
            SSeqResBuffer * buffer = m_CachedSeqs.back();
            x_RetSeqBuffer(buffer);
            m_CachedSeqs.pop_back();
            delete buffer;
        }
    }

    m_CacheID.clear();
    m_NextCacheID = 0;
    m_NumThreads = num_threads;
}



int CSeqDBImpl::x_SetCacheID(int threadID)
{
    std::unique_lock lock(m_CacheIDMutex);
    m_CacheID[threadID] = m_NextCacheID++;

    if (m_NextCacheID == m_NumThreads) {
        m_NextCacheID = -1;
    }
    return m_CacheID[threadID];
}

int CSeqDBImpl::x_GetCacheID()
{
    int threadID = CThread::GetSelf();

    if (m_NextCacheID < 0) {
       return m_CacheID[threadID];
    }

    {
    	std::shared_lock lock(m_CacheIDMutex);
        if (m_CacheID.find(threadID) != m_CacheID.end()) {
        	return m_CacheID[threadID];
        }
    }
    return x_SetCacheID(threadID);
}

void CSeqDBImpl::SetVolsMemBit(int mbit)
{
    int nvols = m_VolSet.GetNumVols();
    for (int vol = 0; vol < nvols; ++vol) {
        m_VolSet.GetVolNonConst(vol)->SetMemBit(mbit);
    }
}

void CSeqDBImpl::SetVolsOidMaskType(int oid_mask_type)
{
    int nvols = m_VolSet.GetNumVols();
    for (int vol = 0; vol < nvols; ++vol) {
        m_VolSet.GetVolNonConst(vol)->SetOidMaskType(oid_mask_type);
    }
}

void CSeqDBImpl::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
    ddc.SetFrame("CSeqDBImpl");
    CObject::DebugDump(ddc, depth);
    ddc.Log("m_DBNames", m_DBNames);
    ddc.Log("m_Aliases", &m_Aliases, depth);
    ddc.Log("m_OIDList", m_OIDList, depth);
    ddc.Log("m_RestrictBegin", m_RestrictBegin);
    ddc.Log("m_RestrictEnd", m_RestrictEnd);
    ddc.Log("m_NextChunkOID", m_NextChunkOID);
    ddc.Log("m_NumSeqs", m_NumSeqs);
    ddc.Log("m_NumSeqsStats", m_NumSeqsStats);
    ddc.Log("m_NumOIDs", m_NumOIDs);
    ddc.Log("m_TotalLength", m_TotalLength);
    ddc.Log("m_ExactTotalLength", m_ExactTotalLength);
    ddc.Log("m_TotalLengthStats", m_TotalLengthStats);
    ddc.Log("m_VolumeLength", m_VolumeLength);
    ddc.Log("m_MaxLength", m_MaxLength);
    ddc.Log("m_MinLength", m_MinLength);
    ddc.Log("m_SeqType", string(1, m_SeqType));
    ddc.Log("m_OidListSetup", m_OidListSetup);
    ddc.Log("m_NeedTotalsScan", m_NeedTotalsScan);
    ddc.Log("m_Date", m_Date);
    ddc.Log("m_UseGiMask", m_UseGiMask);
    ddc.Log("m_GiMask", m_GiMask);
    ddc.Log("m_NumThreads", m_NumThreads);
    ddc.Log("m_NextCacheID", m_NextCacheID);
}

EBlastDbVersion CSeqDBImpl::GetBlastDbVersion() const
{
	return (m_LMDBSet.IsBlastDBVersion5() ? eBDB_Version5 : eBDB_Version4);

}

int CSeqDBImpl::GetNumOfVols() const
{
  	return m_VolSet.GetNumVols();
}

void CSeqDBImpl::GetLMDBFileNames(vector<string> & lmdb_list) const
{
	m_LMDBSet.GetLMDBFileNames(lmdb_list);
}


void CSeqDBImpl::x_GetTaxIdsForSeqId(const CSeq_id & seq_id, int oid, CBlast_def_line::TTaxIds & taxid_set)
{

	CSeqDBLockHold locked(m_Atlas);
    CRef<CBlast_def_line_set> defline_set = x_GetHdr(oid, locked);

    if ((! defline_set.Empty()) && defline_set->CanGet()) {
        ITERATE(list< CRef<CBlast_def_line> >, defline, defline_set->Get()) {
            if (! (*defline)->CanGetSeqid()) {
                continue;
            }

            ITERATE(list< CRef<CSeq_id> >, df_seqid, (*defline)->GetSeqid()) {
            	if((*df_seqid)->Match(seq_id)) {
            		CBlast_def_line::TTaxIds df_taxids = (*defline)->GetTaxIds();
            		if(!df_taxids.empty()) {
            			taxid_set.insert(df_taxids.begin(), df_taxids.end());
            		}
            		return;
            	}
	        }
	    }
    }
}

void CSeqDBImpl::GetTaxIdsForSeqId(const CSeq_id & seq_id, vector<TTaxId> & taxids)
{
	vector<int>  oids;
	SeqidToOids(seq_id, oids, true);
	taxids.clear();
	CBlast_def_line::TTaxIds taxid_set;
	for (unsigned int i=0; i < oids.size(); i++) {
		x_GetTaxIdsForSeqId(seq_id, oids[i], taxid_set);
	}

	if (!taxid_set.empty()) {
		taxids.insert(taxids.begin(), taxid_set.begin(), taxid_set.end());
	}
}

END_NCBI_SCOPE

