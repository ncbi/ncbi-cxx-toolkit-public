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

/// @file seqdboidlist.cpp
/// Implementation for the CSeqDBOIDList class, an array of bits
/// describing a subset of the virtual oid space.
#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include "seqdboidlist.hpp"
#include "seqdbfilter.hpp"
#include <objtools/blast/seqdb_reader/impl/seqdbfile.hpp>
#include "seqdbgilistset.hpp"
#include <algorithm>

BEGIN_NCBI_SCOPE

CSeqDBOIDList::CSeqDBOIDList(CSeqDBAtlas              & atlas,
                             const CSeqDBVolSet       & volset,
                             CSeqDB_FilterTree        & filters,
                             CRef<CSeqDBGiList>       & gi_list,
                             CRef<CSeqDBNegativeList> & neg_list,
                             CSeqDBLockHold           & locked,
                             const CSeqDBLMDBSet      & lmdb_set)
    : m_Atlas   (atlas),      
      m_Lease   (atlas),      
      m_NumOIDs (0)
{
    _ASSERT(gi_list.NotEmpty() || neg_list.NotEmpty() || filters.HasFilter());
    x_Setup( volset, filters, gi_list, neg_list, locked, lmdb_set);
}

CSeqDBOIDList::~CSeqDBOIDList()
{
}

// The general rule I am following in these methods is to use byte
// computations except during actual looping.

void CSeqDBOIDList::x_Setup(const CSeqDBVolSet       & volset,
                            CSeqDB_FilterTree        & filters,
                            CRef<CSeqDBGiList>       & gi_list,
                            CRef<CSeqDBNegativeList> & neg_list,
                            CSeqDBLockHold           & locked,
							const CSeqDBLMDBSet      & lmdb_set)
{
    // First, get the memory space for the OID bitmap and clear it.
    
    // Pad memory space to word boundary, add 8 bytes for "insurance".  Some
    // of the algorithms here need to do bit shifting and OR half of a source
    // element into this destination element, and the other half into this
    // other destination element.  Rather than sprinkle this code with range
    // checks, padding is used.
    
    m_NumOIDs = volset.GetNumOIDs();
    
    m_AllBits.Reset(new CSeqDB_BitSet(0, m_NumOIDs));
    
    CSeqDBGiListSet gi_list_set(m_Atlas,
                                volset,
                                gi_list,
                                neg_list,
                                locked,
                                lmdb_set);
    // Then get the list of filenames and offsets to overlay onto it.

    for(int i = 0; i < volset.GetNumVols(); i++) {
        const CSeqDBVolEntry * v1 = volset.GetVolEntry(i);
        
        CRef<CSeqDB_BitSet> vol_bits =
            x_ComputeFilters(filters, *v1, gi_list_set, locked, lmdb_set.IsBlastDBVersion5());
        
        m_AllBits->UnionWith(*vol_bits, true);
    }
    
    if (lmdb_set.IsBlastDBVersion5()  && filters.HasFilter()) {
   		CSeqDB_BitSet f_bits(0, m_NumOIDs);
    	f_bits.AssignBitRange(0, m_NumOIDs, true);
    	if(x_ComputeFilters(volset, filters, lmdb_set, f_bits, gi_list, neg_list)) {
    		m_AllBits->IntersectWith(f_bits, true);
    	}
    }

    if (gi_list.NotEmpty()) {
        x_ApplyUserGiList(*gi_list);
    }
    if (neg_list.NotEmpty()) {
        x_ApplyNegativeList(*neg_list, lmdb_set.IsBlastDBVersion5());
    }
    
    while(m_NumOIDs && (! x_IsSet(m_NumOIDs - 1))) {
        -- m_NumOIDs;
    }
}

CRef<CSeqDB_BitSet>
CSeqDBOIDList::x_ComputeFilters(const CSeqDB_FilterTree & filters,
                                const CSeqDBVolEntry    & vol,
                                CSeqDBGiListSet         & gis,
                                CSeqDBLockHold          & locked,
                                bool					isBlastDBv5)

{
    const string & vn = vol.Vol()->GetVolName();
    CRef<CSeqDB_FilterTree> ft = filters.Specialize(vn);
    
    int vol_start = vol.OIDStart();
    int vol_end   = vol.OIDEnd();
    
    CRef<CSeqDB_BitSet> volume_map;
    
    // Step 1: Compute the bitmap representing the filtering done by
    // all subnodes.  This is a "union".
    
    int vols = ft->GetVolumes().size();
    
    _ASSERT(vols || ft->GetNodes().size());
    
    if (vols > 0) {
        // This filter tree is filtered by volume name, so all nodes
        // below this point can be ignored if this node contains a
        // volume.  This volume will be ORred with those nodes,
        // flushing them to all "1"s anyway (at least until this
        // node's filtering is applied.)
        
        // This loop really just verifies that specialization was done
        // properly in the case where there are multiple volume names
        // (which must be the same).
        
        for(int j = 1; j < vols; j++) {
            _ASSERT(ft->GetVolumes()[j] == ft->GetVolumes()[0]);
        }
        
        volume_map.Reset(new CSeqDB_BitSet(vol_start,
                                     vol_end,
                                     CSeqDB_BitSet::eAllSet));
    } else {
        // Since this node did not have a volume, we OR together all
        // of its subnodes.
        
        volume_map.Reset(new CSeqDB_BitSet(vol_start,
                                     vol_end,
                                     CSeqDB_BitSet::eAllClear));
        
        ITERATE(vector< CRef< CSeqDB_FilterTree > >, sub, ft->GetNodes()) {
            CRef<CSeqDB_BitSet> sub_bits =
                x_ComputeFilters(**sub, vol, gis, locked, isBlastDBv5);
            
            volume_map->UnionWith(*sub_bits, true);
        }
    }
    
    // Now we apply this level's filtering.  The first question is, is
    // it appropriate for a node to use multiple filtering mechanisms
    // (GI list, OID list, or OID range), either of the same or
    // different types?  The second question is how are multiply
    // filtered nodes interpreted?
    
    // The SeqDB unit tests assume that multiple filters at a given
    // level are ANDed together.  The unit tests assume this for the
    // case of combining OID masks and OID ranges, but in the absence
    // of another motivating example, I'll assume it means ANDing of
    // all such mechanisms.
    
    CRef<CSeqDB_BitSet> filter(new CSeqDB_BitSet(vol_start,
                                                 vol_end,
                                                 CSeqDB_BitSet::eAllSet));
    
    // First, apply any 'range' filters, because they can be combined
    // very efficiently.
    
    typedef CSeqDB_FilterTree::TFilters TFilters;

    ITERATE(TFilters, range, ft->GetFilters()) {
        const CSeqDB_AliasMask & mask = **range;
        
        if (mask.GetType() == CSeqDB_AliasMask::eOidRange) {
            CSeqDB_BitSet r2(mask.GetBegin(),
                                mask.GetEnd(),
                                CSeqDB_BitSet::eAllSet);
            filter->IntersectWith(r2, true);
        } else if (mask.GetType() == CSeqDB_AliasMask::eMemBit) {
            // TODO, adding vol-specific OR and AND
            vol.Vol()->SetMemBit(mask.GetMemBit());
            // No filter->IntersectWith here since
            // MEMBIT can not be done at OID level, therefore,
            // we delegate to seqdbvol (in x_GetFilteredHeader())
            // for further process.
        }
    }
    
    ITERATE(TFilters, filt, ft->GetFilters()) {
        const CSeqDB_AliasMask & mask = **filt;
        
        if ((mask.GetType() == CSeqDB_AliasMask::eOidRange)
            || (mask.GetType() == CSeqDB_AliasMask::eMemBit)
            || (isBlastDBv5 && (mask.GetType() == CSeqDB_AliasMask::eSiList))
            || (mask.GetType() == CSeqDB_AliasMask::eTaxIdList)) {
            continue;
        }
        
        CRef<CSeqDB_BitSet> f;
        CRef<CSeqDBGiList> idlist;
        switch(mask.GetType()) {
        case CSeqDB_AliasMask::eOidList:
            f = x_GetOidMask(mask.GetPath(), vol_start, vol_end);
            break;
            
        case CSeqDB_AliasMask::eSiList:
            idlist = gis.GetNodeIdList(mask.GetPath(),
                                       vol.Vol(),
                                       CSeqDBGiListSet::eSiList,
                                       locked);
            f = x_IdsToBitSet(*idlist, vol_start, vol_end);
            break;
            
        case CSeqDB_AliasMask::eTiList:
            idlist = gis.GetNodeIdList(mask.GetPath(),
                                       vol.Vol(),
                                       CSeqDBGiListSet::eTiList,
                                       locked);
            f = x_IdsToBitSet(*idlist, vol_start, vol_end);
            break;
            
        case CSeqDB_AliasMask::eGiList:
            idlist = gis.GetNodeIdList(mask.GetPath(),
                                       vol.Vol(),
                                       CSeqDBGiListSet::eGiList,
                                       locked);
            f = x_IdsToBitSet(*idlist, vol_start, vol_end);
            break;

        case CSeqDB_AliasMask::eOidRange:
        case CSeqDB_AliasMask::eMemBit:
        case CSeqDB_AliasMask::eTaxIdList:
     
            // these should have been handled in the previous loop.
            break;
        }
        
        filter->IntersectWith(*f, true);
    }
    
    volume_map->IntersectWith(*filter, true);
    
    return volume_map;
}

void CSeqDBOIDList::x_ApplyUserGiList(CSeqDBGiList   & gis)
                                      
{
    //m_Atlas.Lock(locked);
    
    if (gis.Empty()) {
        x_ClearBitRange(0, m_NumOIDs);
        m_NumOIDs = 0;
        return;
    }
    
    // This is the trivial way to 'sort' OIDs; build a bit vector
    // spanning the OID range, turn on the bit indexed by each
    // included OID, and then scan the vector sequentially.  This
    // technique also uniqifies the set, which is desireable here.
    
    
    int j = 0;
    
    if (gis.GetNumGis() || gis.GetNumSis() || gis.GetNumTis() || gis.GetNumPigs()){
    CRef<CSeqDB_BitSet> gilist_oids(new CSeqDB_BitSet(0, m_NumOIDs));
    if (gis.GetNumGis()) {
        for(j = 0; j < gis.GetNumGis(); j++) {
            int oid = gis.GetGiOid(j).oid;
            if ((oid != -1) && (oid < m_NumOIDs)) {
                gilist_oids->SetBit(oid);
            }
        }
    }
    
    if(gis.GetNumSis()) {
        for(j = 0; j < gis.GetNumSis(); j++) {
            int oid = gis.GetSiOid(j).oid;
            if ((oid != -1) && (oid < m_NumOIDs)) {
                gilist_oids->SetBit(oid);
            }
        }
    }
    
    if(gis.GetNumTis()) {
        for(j = 0; j < gis.GetNumTis(); j++) {
            int oid = gis.GetTiOid(j).oid;
            if ((oid != -1) && (oid < m_NumOIDs)) {
                gilist_oids->SetBit(oid);
            }
        }
    }
    
    if(gis.GetNumPigs()) {
        for(j = 0; j < gis.GetNumPigs(); j++) {
            int oid = gis.GetPigOid(j).oid;
            if ((oid != -1) && (oid < m_NumOIDs)) {
                gilist_oids->SetBit(oid);
            }
        }
    }
    m_AllBits->IntersectWith(*gilist_oids, true);
    }
    const vector<blastdb::TOid> & oids_tax = gis.GetOidsForTaxIdsList();
    if(oids_tax.size()) {
        CRef<CSeqDB_BitSet> taxlist_oids(new CSeqDB_BitSet(0, m_NumOIDs));
        for(unsigned int k = 0; k < oids_tax.size(); k++) {
            if (oids_tax[k] < m_NumOIDs) {
                taxlist_oids->SetBit(oids_tax[k]);
            }
        }
        m_AllBits->IntersectWith(*taxlist_oids, true);
    }
    
}

void CSeqDBOIDList::x_ApplyNegativeList(CSeqDBNegativeList & nlist, bool is_v5)
                                        
{
    // We require a normalized list in order to turn bits off.
	m_AllBits->Normalize();
    const vector<blastdb::TOid> & excluded_oids = nlist.GetExcludedOids();
	for(unsigned int i=0; i < excluded_oids.size(); i++) {
	    m_AllBits->ClearBit(excluded_oids[i]);
	}

	if((!is_v5 && nlist.GetNumSis() > 0) || nlist.GetNumGis() > 0 || nlist.GetNumTis() >  0) {
    
    // Intersect the user GI list with the OID bit map.
    
    // Iterate over the bitmap, clearing bits we find there but not in
    // the bool vector.  For very dense OID bit maps, it might be
    // faster to use two similarly implemented bitmaps and AND them
    // together word-by-word.
    
    int max = nlist.GetNumOids();
    
    // Clear any OIDs after the included range.
    
    if (max < m_NumOIDs) {
        CSeqDB_BitSet new_range(0, max, CSeqDB_BitSet::eAllSet);
        m_AllBits->IntersectWith(new_range, true);
    }
    
    // If a 'get next included oid' method was added to the negative
    // list, the following loop could be made a bit faster.
    
    for(int oid = 0; oid < max; oid++) {
        if (! nlist.GetOidStatus(oid)) {
            m_AllBits->ClearBit(oid);
        }
    }
	}


}

CRef<CSeqDB_BitSet>
CSeqDBOIDList::x_IdsToBitSet(const CSeqDBGiList & gilist,
                             int                  oid_start,
                             int                  oid_end)
{
    CRef<CSeqDB_BitSet> bits
        (new CSeqDB_BitSet(oid_start, oid_end, CSeqDB_BitSet::eNone));
    
    CSeqDB_BitSet & bitset = *bits;
    
    int num_gis = gilist.GetNumGis();
    int num_tis = gilist.GetNumTis();
    int num_sis = gilist.GetNumSis();
    int prev_oid = -1;
    
    for(int i = 0; i < num_gis; i++) {
        int oid = gilist.GetGiOid(i).oid;
        
        if (oid != prev_oid) {
            if ((oid >= oid_start) && (oid < oid_end)) {
                bitset.SetBit(oid);
            }
            prev_oid = oid;
        }
    }
    
    for(int i = 0; i < num_tis; i++) {
        int oid = gilist.GetTiOid(i).oid;
        
        if (oid != prev_oid) {
            if ((oid >= oid_start) && (oid < oid_end)) {
                bitset.SetBit(oid);
            }
            prev_oid = oid;
        }
    }
    
    for(int i = 0; i < num_sis; i++) {
        int oid = gilist.GetSiOid(i).oid;
        
        if (oid != prev_oid) {
            if ((oid >= oid_start) && (oid < oid_end)) {
                bitset.SetBit(oid);
            }
            prev_oid = oid;
        }
    }
    
    return bits;
}

void CSeqDBOIDList::x_ClearBitRange(int oid_start,
                                    int oid_end)
{
    m_AllBits->AssignBitRange(oid_start, oid_end, false);
}

CRef<CSeqDB_BitSet>
CSeqDBOIDList::x_GetOidMask(const CSeqDB_Path & fn,
                            int                 vol_start,
                            int                 vol_end)
                            
{
        
    // Open file and get pointers
    
    TCUC* bitmap = 0;
    TCUC* bitend = 0;
    
    CSeqDBRawFile volmask(m_Atlas);
    CSeqDBFileMemMap lease(m_Atlas);
    
    Uint4 num_oids = 0;
    
    {
        volmask.Open(fn);
        lease.Init(fn.GetPathS());
        volmask.ReadSwapped(lease, 0, & num_oids);
        
        // This is the index of the last oid, not the count of oids...
        num_oids++;
        
        size_t file_length = (size_t) volmask.GetFileLength();
        
        // Cast forces signed/unsigned conversion.
        
        volmask.GetFileDataPtr(lease, sizeof(Int4), file_length);
        
        bitmap = (TCUC*) lease.GetFileDataPtr(sizeof(Int4));
        
        bitend = bitmap + (((num_oids + 31) / 32) * 4);
    }
    CRef<CSeqDB_BitSet> bitset(new CSeqDB_BitSet(vol_start, vol_end, bitmap, bitend));
    
    
    // Disable any enabled bits occuring after the volume end point
    // [this should not normally occur.]
    
    for(size_t oid = vol_end; bitset->CheckOrFindBit(oid); oid++) {
        bitset->ClearBit(oid);
    }
    
    return bitset;
}

void 
CSeqDBOIDList::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
    ddc.SetFrame("CSeqDBOIDList");
    CObject::DebugDump(ddc, depth);
    ddc.Log("m_NumOIDs", m_NumOIDs);
    ddc.Log("m_AllBits", m_AllBits, depth);
}

void
s_GetFilteredOidRange(const CSeqDBVolSet & volset, const vector<string> &  vol_basenames,
		              vector<const CSeqDBVolEntry * >  & excluded_vols,
		              CRef<CSeqDBGiList> & si_list)
{
	unsigned int num_vol = volset.GetNumVols();
	vector<bool>  vol_included(num_vol, false);
	excluded_vols.clear();
	for(unsigned int i=0; i < num_vol; i++) {
		const CSeqDBVol * vol = volset.GetVol(i);
		if(std::find(vol_basenames.begin(), vol_basenames.end(), vol->GetVolName()) != vol_basenames.end()) {
			vol->AttachVolumeGiList(si_list);
			continue;
		}
		excluded_vols.push_back(volset.GetVolEntry(i));
	}
}

bool
s_IsOidInFilteredVol(blastdb::TOid oid, vector<const CSeqDBVolEntry * >  & excluded_vols)
{
	for(unsigned int i = 0; i < excluded_vols.size(); i++) {
		 const CSeqDBVolEntry & entry = *(excluded_vols[i]);
		 if ((entry.OIDStart() <= oid) && (entry.OIDEnd()   >  oid)) {
			 return true;
		 }
	}
	return false;
}

void s_AddFilterFile(string & name, const string & vn, vector<string> & fnames, vector<vector<string> > & fnames_vols)
{
	unsigned int j=0;
	for(; j < fnames.size(); j++) {
		if(fnames[j] == name) {
			fnames_vols[j].push_back(vn);
			break;
		}
	}
	if( fnames.size() == j) {
		vector<string> p(1,vn);
		fnames.push_back(name);
		fnames_vols.push_back(p);
	}
}

bool s_CompareSeqId(const string & id1, const string & id2)
{
	if (id1 == id2){
		return false;
	}
	CSeq_id seq_id1(id1, (CSeq_id::fParse_AnyRaw | CSeq_id::fParse_ValidLocal));
	CSeq_id seq_id2(id2, (CSeq_id::fParse_AnyRaw | CSeq_id::fParse_ValidLocal));
	if (seq_id1.Match(seq_id2)) {
		return false;
	}
	return (id1 < id2);
}

void s_ProcessSeqIdFilters(const vector<string>     & fnames,
						   vector<vector<string> >  & fnames_vols,
		                   CRef<CSeqDBGiList>		  user_list,
                           CRef<CSeqDBNegativeList>   neg_user_list,
                           const CSeqDBLMDBSet      & lmdb_set,
                           const CSeqDBVolSet       & volset,
                           CSeqDB_BitSet 			& filter_bit)
{
	if (fnames.size() == 0) {
		return;
	}
	vector<string> user_accs;
	if ((!user_list.Empty()) && (user_list->GetNumSis() > 0)) {
		user_list->GetSiList(user_accs);
		sort(user_accs.begin(), user_accs.end(), s_CompareSeqId);
	}
	vector<string> neg_user_accs;
	if ((!neg_user_list.Empty()) && (neg_user_list->GetNumSis() > 0)) {
		neg_user_accs = neg_user_list->GetSiList();
		sort(neg_user_accs.begin(), neg_user_accs.end());
	}

	for(unsigned int k=0; k < fnames.size(); k++) {
		vector<const CSeqDBVolEntry * > excluded_vols;
		vector<blastdb::TOid> oids;
		CRef<CSeqDBGiList> list(new CSeqDBFileGiList(fnames[k], CSeqDBFileGiList::eSiList));
		s_GetFilteredOidRange(volset, fnames_vols[k], excluded_vols, list);
		vector<string> accs;
		list->GetSiList(accs);
		if(accs.size() == 0){
				continue;
		}
		if((user_accs.size() > 0)  || (neg_user_accs.size() > 0)){
			sort(accs.begin(), accs.end(), s_CompareSeqId);
			if (user_accs.size() > 0) {
				vector<string> common;
				common.resize(accs.size());
				vector<string>::iterator itr = set_intersection(accs.begin(), accs.end(),
					                                            user_accs.begin(), user_accs.end(), common.begin(), s_CompareSeqId);
				common.resize(itr-common.begin());
				if(common.size() == 0){
					continue;
				}
				swap(accs, common);
			}
			if(neg_user_accs.size() > 0) {
				vector<string> difference;
				difference.resize(accs.size());
				vector<string>::iterator itr = set_difference(accs.begin(), accs.end(),
									                          neg_user_accs.begin(), neg_user_accs.end(), difference.begin(), s_CompareSeqId);
				difference.resize(itr-difference.begin());
				if(difference.size() == 0){
					continue;
				}
				swap(accs, difference);
			}
		}

		lmdb_set.AccessionsToOids(accs, oids);
		for(unsigned int i=0; i < accs.size(); i++) {
			if(oids[i] == kSeqDBEntryNotFound) {
				continue;
			}
			if(excluded_vols.size() != 0) {
				if (s_IsOidInFilteredVol(oids[i], excluded_vols)) {
					continue;
				}
			}
			filter_bit.SetBit(oids[i]);
		}
	}
}

void s_ProcessTaxIdFilters(const vector<string> &     fnames,
						   vector<vector<string> >  & fnames_vols,
		                   CRef<CSeqDBGiList>		  user_list,
                           CRef<CSeqDBNegativeList>   neg_user_list,
                           const CSeqDBLMDBSet      & lmdb_set,
                           const CSeqDBVolSet       & volset,
                           CSeqDB_BitSet 			& filter_bit)
{
	if (fnames.size() == 0) {
		return;
	}

	set<TTaxId> user_taxids;
	if(!user_list.Empty() && (user_list->GetNumTaxIds() > 0)) {
		user_taxids = user_list->GetTaxIdsList();
	}
	set<TTaxId> neg_user_taxids;
	if(!neg_user_list.Empty() && (neg_user_list->GetNumTaxIds() > 0)) {
		neg_user_taxids = neg_user_list->GetTaxIdsList();
	}

	for(unsigned int k=0; k < fnames.size(); k++) {
		vector<const CSeqDBVolEntry * > excluded_vols;
		vector<blastdb::TOid> oids;
		CRef<CSeqDBGiList> list(new CSeqDBFileGiList(fnames[k], CSeqDBFileGiList::eTaxIdList));
		s_GetFilteredOidRange(volset, fnames_vols[k], excluded_vols, list);
		set<TTaxId> taxids;
		taxids = list->GetTaxIdsList();
		if(taxids.size() == 0){
			continue;
		}
		if(user_taxids.size() > 0){
			vector<TTaxId> common;
			common.resize(taxids.size());
			vector<TTaxId>::iterator itr = set_intersection(taxids.begin(), taxids.end(),
					                                      user_taxids.begin(), user_taxids.end(), common.begin());
			common.resize(itr-common.begin());
			if( common.size() == 0) {
				continue;
			}
			taxids.clear();
			taxids.insert(common.begin(), common.end());
		}
		if(neg_user_taxids.size() > 0) {
			vector<TTaxId> difference;
			difference.resize(taxids.size());
			vector<TTaxId>::iterator itr = set_difference(taxids.begin(), taxids.end(),
								                        neg_user_taxids.begin(), neg_user_taxids.end(), difference.begin());
			difference.resize(itr-difference.begin());
			if(difference.size() == 0){
				continue;
			}
			taxids.clear();
			taxids.insert(difference.begin(), difference.end());
		}

		lmdb_set.TaxIdsToOids(taxids, oids);
		for(unsigned int i=0; i < oids.size(); i++) {
			if(excluded_vols.size() != 0) {
				if (s_IsOidInFilteredVol(oids[i], excluded_vols)) {
					continue;
				}
			}
			filter_bit.SetBit(oids[i]);
		}
	}
}

bool
CSeqDBOIDList::x_ComputeFilters(const CSeqDBVolSet       & volset,
		                        const CSeqDB_FilterTree  & filters,
   		                        const CSeqDBLMDBSet      & lmdb_set,
   		                        CSeqDB_BitSet 			 & filter_bit,
   		                        CRef<CSeqDBGiList>		   user_list,
   		                        CRef<CSeqDBNegativeList>   neg_user_list)
{
	vector<string> seqid_fnames;
	vector<string> taxid_fnames;
	vector< vector<string> > seqid_fnames_vols;
	vector< vector<string> > taxid_fnames_vols;

	for(int i = 0; i < volset.GetNumVols(); i++) {
		const CSeqDBVolEntry & vol = *(volset.GetVolEntry(i));
	    const string & vn = vol.Vol()->GetVolName();
        CRef<CSeqDB_FilterTree> ft = filters.Specialize(vn);
       	ITERATE(CSeqDB_FilterTree::TFilters, itr, ft->GetFilters()){
        	if(((*itr)->GetType() == CSeqDB_AliasMask::eSiList) ||
        	   ((*itr)->GetType() == CSeqDB_AliasMask::eTaxIdList)) {
        		string name = (*itr)->GetPath().GetPathS();
        		if((*itr)->GetType() == CSeqDB_AliasMask::eSiList) {
        			s_AddFilterFile(name, vn, seqid_fnames, seqid_fnames_vols);
        		}
        		else {
        			s_AddFilterFile(name, vn, taxid_fnames, taxid_fnames_vols);
        		}
        		filter_bit.AssignBitRange(vol.OIDStart(), vol.OIDEnd(), false);
        	}
        }
	}

	if (seqid_fnames.size() > 0) {
		s_ProcessSeqIdFilters(seqid_fnames, seqid_fnames_vols, user_list, neg_user_list,
	                          lmdb_set, volset, filter_bit);
	}
	if (taxid_fnames.size() > 0) {
		s_ProcessTaxIdFilters(taxid_fnames, taxid_fnames_vols, user_list, neg_user_list,
	                          lmdb_set, volset, filter_bit);
	}

	return ((seqid_fnames.size() + taxid_fnames.size()) > 0 ? true:false);
}


END_NCBI_SCOPE

