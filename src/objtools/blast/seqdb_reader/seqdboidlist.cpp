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
   		CSeqDB_BitSet si_bits(0, m_NumOIDs);
   		si_bits.AssignBitRange(0, m_NumOIDs, true);
       	x_ComputeFilters(volset, filters, lmdb_set, si_bits);
		m_AllBits->IntersectWith(si_bits, true);
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
            || (isBlastDBv5 && (mask.GetType() == CSeqDB_AliasMask::eSiList))) {
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

void
CSeqDBOIDList::x_ComputeFilters(const CSeqDBVolSet       & volset,
								const CSeqDB_FilterTree  & filters,
                           		const CSeqDBLMDBSet      & lmdb_set,
                           		CSeqDB_BitSet 			 & si_bit)
{
	vector<string> si_fnames;
	vector< vector<string> > si_fnames_vols;

	for(int i = 0; i < volset.GetNumVols(); i++) {
		const CSeqDBVolEntry & vol = *(volset.GetVolEntry(i));
	    const string & vn = vol.Vol()->GetVolName();
        CRef<CSeqDB_FilterTree> ft = filters.Specialize(vn);
       	ITERATE(CSeqDB_FilterTree::TFilters, itr, ft->GetFilters()){
        	if((*itr)->GetType() == CSeqDB_AliasMask::eSiList){
        		string si_name = (*itr)->GetPath().GetPathS();
        		unsigned int j=0;
        		for(; j < si_fnames.size(); j++) {
        			if(si_fnames[j] == si_name) {
        				si_fnames_vols[j].push_back(vn);
        				break;
        			}
        		}
        		if( si_fnames.size() == j) {
        			vector<string> p(1,vn);
        			si_fnames.push_back(si_name);
        			si_fnames_vols.push_back(p);
        		}
        		si_bit.AssignBitRange(vol.OIDStart(), vol.OIDEnd(), false);
        	}
        }
	}

	for(unsigned int k=0; k < si_fnames.size(); k++) {
		vector<const CSeqDBVolEntry * > excluded_vols;
		CRef<CSeqDBGiList> si_list(new CSeqDBFileGiList(si_fnames[k], CSeqDBFileGiList::eSiList));
		s_GetFilteredOidRange(volset, si_fnames_vols[k], excluded_vols, si_list);
		vector<string> accs;
		vector<blastdb::TOid> oids;
		si_list->GetSiList(accs);
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
			si_list->SetSiTranslation(i, oids[i]);
			si_bit.SetBit(oids[i]);
		}
	}
	return;
}


END_NCBI_SCOPE

