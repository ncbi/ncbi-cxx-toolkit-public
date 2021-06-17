/*
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
 * Author: Amelia Fong
 *
 */
#include <ncbi_pch.hpp>
#include "seqdblmdbset.hpp"


BEGIN_NCBI_SCOPE
CSeqDBLMDBEntry::CSeqDBLMDBEntry(const string & name, TOid start_oid, const vector<string> & vol_names)
        					: m_LMDBFName (name),
        					  m_OIDStart   (start_oid),
        					  m_OIDEnd     (0),
        					  m_isPartial  (false)
{
	m_LMDB.Reset(new CSeqDBLMDB(name));
	vector<string> tmp_name;
	vector<TOid>  tmp_num_oids;
	TOid total_num_oids_lmdb = 0;
	m_LMDB->GetVolumesInfo(tmp_name, tmp_num_oids);
	m_VolInfo.resize(tmp_name.size());
	vector<string>::const_iterator itr = vol_names.begin();
	if(vol_names.size()  > tmp_name.size()) {
		NCBI_THROW(CSeqDBException, eArgErr, "Input db vol size does not match lmdb vol size");
	}
	// The input vol_names should be sorted in the same order as
	for(unsigned int i=0; i < tmp_name.size(); i++) {
		SVolumeInfo & v = m_VolInfo[i];
		v.vol_name = tmp_name[i];
		total_num_oids_lmdb += tmp_num_oids[i];
		v.max_oid = total_num_oids_lmdb;
		if((itr != vol_names.end()) && (*itr == v.vol_name)) {
			v.skipped_oids = 0;
			m_OIDEnd  += tmp_num_oids[i];
			itr++;
		}
		else {
			v.skipped_oids = tmp_num_oids[i];
		}
	}

	if(m_OIDEnd == 0) {
		NCBI_THROW(CSeqDBException, eArgErr, "Input db vol does not match lmdb vol");
	}
	if(m_OIDEnd != total_num_oids_lmdb) {
		m_isPartial = true;
	}
	m_OIDEnd += m_OIDStart;
}

CSeqDBLMDBEntry::~CSeqDBLMDBEntry()
{
	m_LMDB.Reset();
}

void CSeqDBLMDBEntry::x_AdjustOidsOffset(vector<TOid> & oids) const
{
	if ((m_OIDStart > 0) && (!m_isPartial)) {
		for(unsigned int i=0; i < oids.size(); i++) {
			if (oids[i] != kSeqDBEntryNotFound) {
				oids[i] +=m_OIDStart;
			}
		}
	}

	if(m_isPartial) {
		for(unsigned int i=0; i < oids.size(); i++) {
			if (oids[i] != kSeqDBEntryNotFound) {
				TOid skipped_oids = 0;
				for(unsigned int j=0;j < m_VolInfo.size(); j++) {
					if(oids[i] >=  m_VolInfo[j].max_oid) {
						skipped_oids += m_VolInfo[j].skipped_oids;
					}
					else {
						if(m_VolInfo[j].skipped_oids > 0) {
							// ids found but in skipped vol
							oids[i] = kSeqDBEntryNotFound;
						}
						else {
							oids[i] = oids[i] + m_OIDStart - skipped_oids ;
						}
						break;
					}
				}
			}
		}
	}
}

void CSeqDBLMDBEntry::x_AdjustOidsOffset_TaxList(vector<TOid> & oids) const
{
	if ((m_OIDStart > 0) && (!m_isPartial)) {
		for(unsigned int i=0; i < oids.size(); i++) {
				oids[i] +=m_OIDStart;
		}
	}

	if(m_isPartial) {
		vector<TOid> tmp;
		for(unsigned int i=0; i < oids.size(); i++) {
			TOid skipped_oids = 0;
			for(unsigned int j=0;j < m_VolInfo.size(); j++) {
				if(oids[i] >=  m_VolInfo[j].max_oid) {
					skipped_oids += m_VolInfo[j].skipped_oids;
				}
				else {
					if(m_VolInfo[j].skipped_oids > 0) {
					}
					else {
						tmp.push_back(oids[i] + m_OIDStart - skipped_oids) ;
					}
					break;
				}
			}
		}
		oids.swap(tmp);
	}

}

void CSeqDBLMDBEntry::AccessionToOids(const string & acc, vector<TOid>  & oids) const
{
	m_LMDB->GetOid(acc,oids, true);
	x_AdjustOidsOffset(oids);

}

void CSeqDBLMDBEntry::AccessionsToOids(const vector<string>& accs, vector<TOid>& oids) const
{
	m_LMDB->GetOids(accs, oids);
	x_AdjustOidsOffset(oids);
}

void CSeqDBLMDBEntry::NegativeSeqIdsToOids(const vector<string>& ids, vector<blastdb::TOid>& rv) const
{
	m_LMDB->NegativeSeqIdsToOids(ids, rv);
	x_AdjustOidsOffset(rv);
}

void CSeqDBLMDBEntry::TaxIdsToOids(const set<TTaxId>& tax_ids, vector<blastdb::TOid>& rv, vector<TTaxId> & tax_ids_found) const
{
	m_LMDB->GetOidsForTaxIds(tax_ids, rv, tax_ids_found);
	x_AdjustOidsOffset_TaxList(rv);
}

void CSeqDBLMDBEntry::NegativeTaxIdsToOids(const set<TTaxId>& tax_ids, vector<blastdb::TOid>& rv, vector<TTaxId> & tax_ids_found) const
{
	m_LMDB->NegativeTaxIdsToOids(tax_ids, rv, tax_ids_found);
	x_AdjustOidsOffset_TaxList(rv);
}

void CSeqDBLMDBEntry::GetDBTaxIds(vector<TTaxId> & tax_ids) const
{
	m_LMDB->GetDBTaxIds(tax_ids);
}

void CSeqDBLMDBEntry::GetTaxIdsForOids(const vector<blastdb::TOid> & oids, set<TTaxId> & tax_ids) const
{
	if(m_isPartial) {
		vector<TOid> tmp;
		TOid skipped_oids = 0;
		unsigned int j=0;
		for(unsigned int i=0; i < oids.size(); i++) {
			while(j < m_VolInfo.size() &&
				  (m_VolInfo[j].skipped_oids > 0 || oids[i] + skipped_oids >= m_VolInfo[j].max_oid)){
				skipped_oids += m_VolInfo[j].skipped_oids;
				j++;
			}

			tmp.push_back(oids[i] + skipped_oids) ;
		}
	   m_LMDB->GetTaxIdsForOids(tmp, tax_ids);
	}
	else {
		m_LMDB->GetTaxIdsForOids(oids, tax_ids);
	}
}

CSeqDBLMDBSet::CSeqDBLMDBSet()
{
}

CSeqDBLMDBSet::~CSeqDBLMDBSet()
{
	m_LMDBEntrySet.clear();
}

CSeqDBLMDBSet::CSeqDBLMDBSet( const CSeqDBVolSet & volSet)
{
	string lmdb_full_name = kEmptyStr;
	vector<string> lmdb_vol_names;
	bool version5 = true;
	for(int i=0; i < volSet.GetNumVols(); i++) {
		const CSeqDBVol * vol = volSet.GetVol(i);
		string vol_lmdb_name = vol->GetLMDBFileName();
		if(vol_lmdb_name == kEmptyStr) {
			version5 = false;
			if ((!m_LMDBEntrySet.empty())|| (!lmdb_vol_names.empty())) {
				m_LMDBEntrySet.clear();
				NCBI_THROW(CSeqDBException, eVersionErr, "DB list contains both Version 4 and Version 5 dbs");
			}
			continue;
		}

		if(version5 == false) {
			m_LMDBEntrySet.clear();
			NCBI_THROW(CSeqDBException, eVersionErr, "DB list contains both Version 4 and Version 5 dbs");
		}

		CSeqDB_Path p(vol->GetVolName());
		string vol_name;
		p.FindBaseName().GetString(vol_name);
		string vol_lmdb_full_name;
		CSeqDB_Substring fn(vol_lmdb_name);
		SeqDB_CombinePath(p.FindDirName(),fn, NULL, vol_lmdb_full_name);
		if( 0 == i ) {
			lmdb_full_name = vol_lmdb_full_name;
		}

		if(vol_lmdb_full_name == lmdb_full_name) {
			lmdb_vol_names.push_back(vol_name);
			continue;
		}
		else {
			TOid start_oid = m_LMDBEntrySet.empty() ? 0 :m_LMDBEntrySet.back()->GetOIDEnd();
			CRef<CSeqDBLMDBEntry> entry(new CSeqDBLMDBEntry(lmdb_full_name,start_oid, lmdb_vol_names));
			m_LMDBEntrySet.push_back(entry);
			lmdb_vol_names.clear();
			if(entry->GetOIDEnd() < 0) {
				m_LMDBEntrySet.clear();
				NCBI_THROW(CSeqDBException, eFileErr, "Invalid db file : " + lmdb_full_name);
			}
			lmdb_full_name = vol_lmdb_full_name;
			lmdb_vol_names.push_back(vol_name);
		}
	}
	if( lmdb_full_name!=kEmptyStr)	 {
		TOid start_oid = m_LMDBEntrySet.empty() ? 0 :m_LMDBEntrySet.back()->GetOIDEnd();
		CRef<CSeqDBLMDBEntry> entry(new CSeqDBLMDBEntry(lmdb_full_name,start_oid, lmdb_vol_names));
		m_LMDBEntrySet.push_back(entry);
		lmdb_vol_names.clear();
		if(entry->GetOIDEnd() < 0) {
			m_LMDBEntrySet.clear();
			NCBI_THROW(CSeqDBException, eFileErr, "Invalid db file : " + lmdb_full_name);
		}
	}

}

void CSeqDBLMDBSet::AccessionToOids(const string & acc, vector<TOid>  & oids)const
{
	m_LMDBEntrySet[0]->AccessionToOids(acc, oids);
	vector <TOid> tmp;
	for(unsigned int i=1; i < m_LMDBEntrySet.size(); i++) {
		m_LMDBEntrySet[i]->AccessionToOids(acc, tmp);
		if(!tmp.empty()){
			oids.insert(oids.end(), tmp.begin(), tmp.end());
			tmp.clear();
		}
	}
}

void CSeqDBLMDBSet::AccessionsToOids(const vector<string>& accs, vector<TOid>& oids) const
{
	m_LMDBEntrySet[0]->AccessionsToOids(accs, oids);

	for(unsigned int i=1; i < m_LMDBEntrySet.size(); i++) {
		vector <TOid> tmp(accs.size(), 0);
		m_LMDBEntrySet[i]->AccessionsToOids(accs, tmp);
		// This implies we will return higher OID fro diplicate entry
		for(unsigned int j=0; j < oids.size(); j++) {
			if(tmp[j] != kSeqDBEntryNotFound) {
				oids[j] = tmp[j];
			}
		}
	}
}

void CSeqDBLMDBSet::NegativeSeqIdsToOids(const vector<string>& ids, vector<blastdb::TOid>& rv) const
{
	m_LMDBEntrySet[0]->NegativeSeqIdsToOids(ids, rv);
	for(unsigned int i=1; i < m_LMDBEntrySet.size(); i++) {
		vector <TOid> tmp(ids.size(), 0);
		m_LMDBEntrySet[i]->NegativeSeqIdsToOids(ids, tmp);
		rv.insert(rv.end(), tmp.begin(), tmp.end());
	}

}

void CSeqDBLMDBSet::TaxIdsToOids(set<TTaxId>& tax_ids, vector<blastdb::TOid>& rv) const
{
	vector<TTaxId> tax_ids_found;
	set<TTaxId> rv_tax_ids;
	m_LMDBEntrySet[0]->TaxIdsToOids(tax_ids, rv, tax_ids_found);
	rv_tax_ids.insert(tax_ids_found.begin(), tax_ids_found.end());
	for(unsigned int i=1; i < m_LMDBEntrySet.size(); i++) {
		vector <TOid> tmp;
		m_LMDBEntrySet[i]->TaxIdsToOids(tax_ids, tmp, tax_ids_found);
		rv.insert(rv.end(), tmp.begin(), tmp.end());
		if(rv_tax_ids.size() < tax_ids.size()) {
			rv_tax_ids.insert(tax_ids_found.begin(), tax_ids_found.end());
		}
	}
	if(rv.size() == 0) {
		NCBI_THROW(CSeqDBException, eTaxidErr, "Taxonomy ID(s) not found. This could be because the ID(s) provided are not at or below the species level. Please use get_species_taxids.sh to get taxids for nodes higher than species (see https://www.ncbi.nlm.nih.gov/books/NBK546209/).");
	}
	tax_ids.swap(rv_tax_ids);
}

void CSeqDBLMDBSet::NegativeTaxIdsToOids(set<TTaxId>& tax_ids, vector<blastdb::TOid>& rv) const
{
	vector<TTaxId> tax_ids_found;
	set<TTaxId> rv_tax_ids;
	m_LMDBEntrySet[0]->NegativeTaxIdsToOids(tax_ids, rv, tax_ids_found);
	rv_tax_ids.insert(tax_ids_found.begin(), tax_ids_found.end());
	for(unsigned int i=1; i < m_LMDBEntrySet.size(); i++) {
		vector <TOid> tmp;
		m_LMDBEntrySet[i]->NegativeTaxIdsToOids(tax_ids, tmp, tax_ids_found);
		rv.insert(rv.end(), tmp.begin(), tmp.end());
		if(rv_tax_ids.size() < tax_ids.size()) {
			rv_tax_ids.insert(tax_ids_found.begin(), tax_ids_found.end());
		}
	}
	if(rv.size() == 0) {
		NCBI_THROW(CSeqDBException, eTaxidErr, "Taxonomy ID(s) not found.Taxonomy ID(s) not found. This could be because the ID(s) provided are not at or below the species level. Please use get_species_taxids.sh to get taxids for nodes higher than species (see https://www.ncbi.nlm.nih.gov/books/NBK546209/).");
	}

	tax_ids.swap(rv_tax_ids);
}

void CSeqDBLMDBSet::GetDBTaxIds(set<TTaxId> & tax_ids) const
{
	vector<TTaxId> t;
	m_LMDBEntrySet[0]->GetDBTaxIds(t);
	tax_ids.insert(t.begin(), t.end());
	for(unsigned int i=1; i < m_LMDBEntrySet.size(); i++) {
		t.clear();
		m_LMDBEntrySet[i]->GetDBTaxIds(t);
		tax_ids.insert(t.begin(), t.end());
	}
}


void CSeqDBLMDBSet::GetTaxIdsForOids(const vector<blastdb::TOid> & oids, set<TTaxId> & tax_ids) const
{
	if (m_LMDBEntrySet.size() > 1) {
    	vector<TOid> t;
	    int j =0;
    	for(unsigned int i =0; i < oids.size(); i++){
		   	if (oids[i] >= m_LMDBEntrySet[j]->GetOIDEnd()){
		   		if (t.size() > 0){
		   			set<TTaxId> t_set;
		   			m_LMDBEntrySet[j]->GetTaxIdsForOids(t, t_set);
		   			t.clear();
		   			tax_ids.insert(t_set.begin(), t_set.end());
		   		}
		   		j++;
		   	}
		   	t.push_back(oids[i] - m_LMDBEntrySet[j]->GetOIDStart());
		}
    	if (t.size() > 0){
    		set<TTaxId> t_set;
    		m_LMDBEntrySet[j]->GetTaxIdsForOids(t, t_set);
    		tax_ids.insert(t_set.begin(), t_set.end());
    	}
	}
	else {
		m_LMDBEntrySet[0]->GetTaxIdsForOids(oids, tax_ids);
	}


}
END_NCBI_SCOPE
