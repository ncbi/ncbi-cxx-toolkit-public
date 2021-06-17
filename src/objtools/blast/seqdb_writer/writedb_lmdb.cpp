/*  $Id:
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

/// @file writedb_lmdb.cpp
/// Implementation for the CWriteDB_LMDB and related classes.

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbifile.hpp>
#include <objtools/blast/seqdb_reader/impl/seqdb_lmdb.hpp>
#include <objtools/blast/seqdb_writer/writedb_lmdb.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <math.h>
#ifdef _OPENMP
#include <omp.h>
#endif

BEGIN_NCBI_SCOPE

#define DEFAULT_MAX_ENTRY_PER_TXN 40000
#define DEFAULT_MIN_SPLIT_SORT_SIZE 500000000
#define DEFAULT_MIN_SPLIT_CHUNK_SIZE 25000000


CWriteDB_LMDB::CWriteDB_LMDB(const string& dbname,  Uint8 map_size, Uint8 capacity): m_Db(dbname),
                             m_Env(CBlastLMDBManager::GetInstance().GetWriteEnv(dbname, map_size)),
                             m_ListCapacity(capacity),
                             m_MaxEntryPerTxn(DEFAULT_MAX_ENTRY_PER_TXN),
                             m_TotalIdsLength(0)
{
	m_list.reserve(m_ListCapacity);
	char* max_entry_str = getenv("MAX_LMDB_TXN_ENTRY");
	if (max_entry_str) {
		m_MaxEntryPerTxn = NStr::StringToInt(max_entry_str);
	    _TRACE("DEBUG: LMDB TXN Max Entry " << m_MaxEntryPerTxn);
    }
}

CWriteDB_LMDB::~CWriteDB_LMDB()
{
	x_CreateOidToSeqidsLookupFile();
	x_CommitTransaction();
    CBlastLMDBManager::GetInstance().CloseEnv(m_Db);
    CFile(m_Db+"-lock").Remove();

}

void CWriteDB_LMDB::InsertVolumesInfo(const vector<string> & vol_names, const vector<blastdb::TOid> & vol_num_oids)
{
	x_IncreaseEnvMapSize(vol_names, vol_num_oids);

    lmdb::txn txn = lmdb::txn::begin(m_Env);
    lmdb::dbi volinfo = lmdb::dbi::open(txn, blastdb::volinfo_str.c_str(), MDB_CREATE | MDB_INTEGERKEY);
    lmdb::dbi volname = lmdb::dbi::open(txn, blastdb::volname_str.c_str(), MDB_CREATE | MDB_INTEGERKEY);
	for (unsigned int i =0; i < vol_names.size(); i++) {
		const lmdb::val key{&i, sizeof(i)};
		lmdb::val value{vol_names[i].c_str(), strlen(vol_names[i].c_str())};
		bool rc =lmdb::dbi_put(txn, volname.handle(), key, value);
		if (!rc) {
			NCBI_THROW( CSeqDBException, eArgErr, "VolNames error ");
		}
		rc = volinfo.put(txn, i, vol_num_oids[i]);
		if (!rc) {
			NCBI_THROW( CSeqDBException, eArgErr, "VolInfo error ");
		}
	}
	txn.commit();
}

int CWriteDB_LMDB::InsertEntries(const list<CRef<CSeq_id>> & seqids, const blastdb::TOid oid)
{
    int count = 0;
    ITERATE(list<CRef<CSeq_id>>, itr, seqids) {
    	x_InsertEntry((*itr), oid);
    	count++;
    }

    return count;
}


int CWriteDB_LMDB::InsertEntries(const vector<CRef<CSeq_id>> & seqids, const blastdb::TOid oid)
{
    int count = 0;
    ITERATE(vector<CRef<CSeq_id>>, itr, seqids) {
       x_InsertEntry((*itr), oid);
    	count++;
    }
    return count;
}


void CWriteDB_LMDB::x_InsertEntry(const CRef<CSeq_id> &seqid, const blastdb::TOid oid)
{
    
    if(seqid->IsGi()) {
    	return;
    }

    x_Resize();

    if(seqid->IsPir() || seqid->IsPrf()) {
    	SKeyValuePair kv_pir;
    	kv_pir.id = seqid->AsFastaString();
    	kv_pir.oid = oid;
    	kv_pir.saveToOidList = true;
    	m_list.push_back(kv_pir);
    	return;
    }

    if(seqid->IsPdb()) {
    	SKeyValuePair kv_pdb_mol;
    	kv_pdb_mol.id = seqid->GetPdb().GetMol().Get();
    	kv_pdb_mol.oid = oid;
    	m_list.push_back(kv_pdb_mol);
    	// mol code should be case insensitive but c++ tooklit
    	// is not converting it all to uppercase now
    	string id_upper = kv_pdb_mol.id;
    	NStr::ToUpper(id_upper);
    	if(kv_pdb_mol.id != id_upper) {
    		SKeyValuePair kv_u;
       		kv_u.id = id_upper;
       		kv_u.oid = oid;
       		m_list.push_back(kv_u);
       	}
    	SKeyValuePair kv;
    	kv.id = seqid->GetSeqIdString(true);
    	kv.oid = oid;
    	kv.saveToOidList = true;
    	m_list.push_back(kv);
    	return;
    }

    if(seqid->GetTextseq_Id() != NULL) {
    	SKeyValuePair kv;
    	kv.id = seqid->GetSeqIdString(false);
    	kv.oid = oid;
    	string id_v = seqid->GetSeqIdString(true);
    	if( kv.id != id_v) {
    		m_list.push_back(kv);
    		SKeyValuePair kv_v;
    		kv_v.id = id_v;
    		kv_v.oid = oid;
    		kv_v.saveToOidList = true;
    		m_list.push_back(kv_v);
    	}
    	else {
    		kv.saveToOidList = true;
    		m_list.push_back(kv);
    	}
   	}
    else {
    	SKeyValuePair kv;
    	kv.id = seqid->GetSeqIdString(true);
    	kv.oid = oid;
    	kv.saveToOidList = true;
    	m_list.push_back(kv);
    	string id_upper = kv.id;
    	NStr::ToUpper(id_upper);
    	if(kv.id != id_upper) {
    		SKeyValuePair kv_u;
    		kv_u.id = id_upper;
    		kv_u.oid = oid;
    		m_list.push_back(kv_u);

    	}

    }
    return;
}

void CWriteDB_LMDB::x_IncreaseEnvMapSize(const vector<string> & vol_names, const vector<blastdb::TOid> & vol_num_oids)
{
	// 2 meta pages
	const size_t MIN_PAGES = 3;
	const size_t BRANCH_PAGES = 2;
	// Each entry has 8 byte overhead + size of (key + entry)
	size_t vol_name_size = (vol_names.front().size() + 24)* vol_names.size();
	size_t vol_info_size = 24* vol_names.size();

	MDB_env *env = m_Env.handle();
	MDB_stat stat;
	MDB_envinfo info;
	lmdb::env_stat(env, &stat);
	lmdb::env_info(env, &info);
	size_t page_size = stat.ms_psize;
	// For each page 16 byte header
	size_t page_max_size = page_size -16;
	size_t last_page_num = info.me_last_pgno;
	size_t max_num_pages = info.me_mapsize/page_size;
	size_t leaf_pages_needed = vol_name_size/page_max_size + vol_info_size/page_max_size + 2;
	size_t total_pages_needed = MIN_PAGES + BRANCH_PAGES + leaf_pages_needed;
	if( (total_pages_needed + last_page_num) > max_num_pages ) {
		size_t newMapSize = (total_pages_needed + last_page_num) * page_size;
		m_Env.set_mapsize(newMapSize);
		LOG_POST(Info << "Increased lmdb mapsize to " << newMapSize);
	}

}

void CWriteDB_LMDB::x_IncreaseEnvMapSize()
{
	size_t size = m_TotalIdsLength  + m_list.size() * 16;
	size_t avg_id_length = m_TotalIdsLength/m_list.size();
	MDB_env *env = m_Env.handle();
	MDB_stat stat;
	MDB_envinfo info;
	lmdb::env_stat(env, &stat);
	lmdb::env_info(env, &info);
	size_t page_size = stat.ms_psize;
	// 16 byte header for each page
	size_t page_max_size = page_size -16;
	size_t last_page_num = info.me_last_pgno;
	size_t max_num_pages = info.me_mapsize/page_size;
	size_t leaf_pages_needed = size/page_max_size + 1;
	size_t dup_pages = (leaf_pages_needed > 200) ? 14: 7;
	size_t branch_pages_needed = (avg_id_length + 16)* leaf_pages_needed/page_max_size + 1;
	size_t total_pages_needed = leaf_pages_needed + branch_pages_needed + dup_pages;
	if( (total_pages_needed + last_page_num) > max_num_pages) {
		size_t newMapSize = (total_pages_needed + last_page_num) * page_size;
		m_Env.set_mapsize(newMapSize);
		LOG_POST(Info << "Increased lmdb mapsize to " << newMapSize);
	}
}

void CWriteDB_LMDB::x_Split(vector<SKeyValuePair>::iterator  b, vector<SKeyValuePair>::iterator e, const unsigned int min_chunk_size)
{
#ifdef _OPENMP
	unsigned int chunk = (e -b);
	if(chunk < min_chunk_size) {
		sort (b, e, SKeyValuePair::cmp_key);
		return;
	}

	chunk = chunk /2;
	std::nth_element(b, b+chunk, e, SKeyValuePair::cmp_key);
	#pragma omp task
	x_Split(b, (b+chunk), min_chunk_size);
	#pragma omp task
	x_Split((b+chunk),e, min_chunk_size);
#else
	sort (b, e, SKeyValuePair::cmp_key);

#endif
}

void CWriteDB_LMDB::x_CommitTransaction()
{
	if(m_list.size() == 0) {
		return;
	}
#ifdef _OPENMP
	unsigned int min_split_size = DEFAULT_MIN_SPLIT_SORT_SIZE;
	unsigned int chunk_size = DEFAULT_MIN_SPLIT_CHUNK_SIZE;
	char* min_split_str = getenv("LMDB_MIN_SPLIT_SIZE");
	char* chunk_str = getenv("LMDB_SPLIT_CHUNK_SIZE");
	if (chunk_str) {
		chunk_size = NStr::StringToUInt(chunk_str);
	    _TRACE("DEBUG: LMDB_SPLIT_CHUNK_SIZE " << chunk_str);
	}
	if (min_split_str) {
		min_split_size = NStr::StringToUInt(min_split_str);
		_TRACE("DEBUG: LMDB LMDB_MIN_SPLIT_SIZE " << min_split_str);
	}
	if((m_list.size() < min_split_size) || (m_list.size() < 2*chunk_size)) {
		std::sort (m_list.begin(), m_list.end(), SKeyValuePair::cmp_key);
	}
	else {
		unsigned int num_threads = GetCpuCount();
		unsigned int num_chunks = pow(2, ceil((log(m_list.size())- log(chunk_size))/log(2)));
		if (num_chunks < num_threads) {
			num_threads = num_chunks;
		}

		omp_set_num_threads(num_threads);
		#pragma omp parallel
		#pragma omp single nowait
		x_Split(m_list.begin(), m_list.end(), chunk_size);
	}
#else
	std::sort (m_list.begin(), m_list.end(), SKeyValuePair::cmp_key);
#endif

	x_IncreaseEnvMapSize();

    unsigned int j=0;
    while (j < m_list.size()){
    	lmdb::txn txn = lmdb::txn::begin(m_Env);
    	lmdb::dbi dbi = lmdb::dbi::open(txn, blastdb::acc2oid_str.c_str(),
    			                        MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED);
    	unsigned int i = j;
    	j= i+m_MaxEntryPerTxn;
    	if(j > m_list.size()) {
    		j = m_list.size();
    	}
    	for(; i < j; i++){
    		if( i > 0) {
    			if ((m_list[i-1].id == m_list[i].id) &&
    				(m_list[i-1].oid == m_list[i].oid)){
    				continue;
    			}
    		}
    		blastdb::TOid & oid = m_list[i].oid;
    		string & id = m_list[i].id;
    		//cerr << m_list[i].id << endl;
			lmdb::val value{&oid, sizeof(oid)};
			lmdb::val key{id.c_str(), strlen(id.c_str())};
			bool rc = lmdb::dbi_put(txn, dbi.handle(), key, value, MDB_APPENDDUP);
			if (!rc) {
		 		NCBI_THROW( CSeqDBException, eArgErr, "acc2oid error for id " + id);
			}
		}
    	txn.commit();
    }
    return;
}

Uint4 s_WirteIds(CNcbiOfstream & os, vector<string> & ids)
{
	const unsigned char byte_max = 0xFF;
	Uint4 diff =0;
	sort(ids.begin(), ids.end());
	for(unsigned int j =0; j < ids.size(); j++) {
		Uint4 id_len = ids[j].size();
		if(id_len >= byte_max) {
			os.write((char *)&byte_max, 1);
			os.write((char *)&id_len, 4);
			diff += 5;
		}
		else {
			char l = byte_max & id_len;
			os.write(&l,1);
			diff += 1;
		}
		os.write(ids[j].c_str(), id_len);
		diff += id_len;
	}
	return diff;
}

void CWriteDB_LMDB::x_CreateOidToSeqidsLookupFile()
{
	if(m_list.size() == 0) {
		return;
	}
	Uint8 total_num_oids = m_list.back().oid + 1;
	string filename = GetFileNameFromExistingLMDBFile(m_Db, ELMDBFileType::eOid2SeqIds);
	Uint8 offset = 0;
	CNcbiOfstream os(filename.c_str(), IOS_BASE::out | IOS_BASE::binary);
	vector<Uint4> offsets(total_num_oids, 0);

	os.write((char *)&total_num_oids, 8);

	for(unsigned int i=0; i < total_num_oids; i++) {
		os.write((char *) &offset, 8);
	}
	os.flush();

	blastdb::TOid count = 0;
	vector<string> tmp_ids;
	for(unsigned int i = 0; i < m_list.size(); i++) {
		if(i > 0 && m_list[i].oid != m_list[i-1].oid ) {
			if((m_list[i].oid - m_list[i-1].oid) != 1) {
		 		NCBI_THROW( CSeqDBException, eArgErr, "Input id list not in ascending oid order");
			}
			offsets[count] = s_WirteIds(os, tmp_ids);
			count++;
			tmp_ids.clear();
		}
		m_TotalIdsLength +=m_list[i].id.size();
		if(!m_list[i].saveToOidList) {
			continue;
		}
		tmp_ids.push_back(m_list[i].id);

	}
	offsets[count] = s_WirteIds(os, tmp_ids);
	_ASSERT(count == m_list.back().oid);

	os.flush();
	os.seekp(8);
	for(unsigned int i = 0; i < total_num_oids; i++) {
		offset += offsets[i];
		os.write((char *) &offset, 8);
	}

	os.flush();
	os.close();
}

void CWriteDB_LMDB::x_Resize()
{
	if (m_list.size() + 1 > m_ListCapacity) {
    	 m_ListCapacity = m_ListCapacity * 2;
         m_list.reserve(m_ListCapacity);
     }
}

CWriteDB_TaxID::CWriteDB_TaxID(const string& dbname,  Uint8 map_size, Uint8 capacity): m_Db(dbname),
                               m_Env(CBlastLMDBManager::GetInstance().GetWriteEnv(dbname, map_size)),
                               m_ListCapacity(capacity), m_MaxEntryPerTxn(DEFAULT_MAX_ENTRY_PER_TXN)
{
	m_TaxId2OidList.reserve(m_ListCapacity);
	char* max_entry_str = getenv("MAX_LMDB_TXN_ENTRY");
	if (max_entry_str) {
		m_MaxEntryPerTxn = NStr::StringToInt(max_entry_str);
	    _TRACE("DEBUG: LMDB TXN Max Entry " << m_MaxEntryPerTxn);
    }
}

CWriteDB_TaxID::~CWriteDB_TaxID()
{
	x_CreateOidToTaxIdsLookupFile();
	x_CreateTaxIdToOidsLookupFile();
	x_CommitTransaction();
    CBlastLMDBManager::GetInstance().CloseEnv(m_Db);
    CFile(m_Db+"-lock").Remove();
}

int CWriteDB_TaxID::InsertEntries(const set<TTaxId> & tax_ids, const blastdb::TOid oid)
{
    int count = 0;
    if(tax_ids.size() == 0) {
    	x_Resize();
    	SKeyValuePair<blastdb::TOid>  kv(ZERO_TAX_ID, oid);
    	m_TaxId2OidList.push_back(kv);
    	return 1;
    }

    ITERATE(set<TTaxId>, itr, tax_ids) {
    	x_Resize();
    	SKeyValuePair<blastdb::TOid> kv(*itr, oid);
    	m_TaxId2OidList.push_back(kv);
    	count++;
    }

    return count;
}

void CWriteDB_TaxID::x_IncreaseEnvMapSize()
{
	const size_t MIN_PAGES = 4;
	MDB_env *env = m_Env.handle();
	MDB_stat stat;
	MDB_envinfo info;
	lmdb::env_stat(env, &stat);
	lmdb::env_info(env, &info);
	size_t size = m_TaxId2OffsetsList.size()*32;
	size_t page_size = stat.ms_psize;
	size_t page_max_size = stat.ms_psize - 16;
	size_t last_page_num = info.me_last_pgno;
	size_t max_num_pages = info.me_mapsize/page_size;
	size_t leaf_pages_needed = size/page_max_size + 1;
	size_t branch_pages_needed = 24 * leaf_pages_needed/page_max_size + 1;
	size_t total_pages_needed = leaf_pages_needed + branch_pages_needed + MIN_PAGES;
	if( (total_pages_needed + last_page_num) > max_num_pages) {
		size_t newMapSize = (total_pages_needed + last_page_num) * page_size;
		m_Env.set_mapsize(newMapSize);
		LOG_POST(Info << "Increased lmdb mapsize to " << newMapSize);
	}
}


void CWriteDB_TaxID::x_CommitTransaction()
{
	_ASSERT(m_TaxId2OffsetsList.size());
    sort (m_TaxId2OffsetsList.begin(), m_TaxId2OffsetsList.end(), SKeyValuePair<Uint8>::cmp_key);

    x_IncreaseEnvMapSize();

    unsigned int j=0;
    while (j < m_TaxId2OffsetsList.size()){
    	lmdb::txn txn = lmdb::txn::begin(m_Env);
    	lmdb::dbi dbi = lmdb::dbi::open(txn, blastdb::taxid2offset_str.c_str(),
    			                        MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED);
    	unsigned int i = j;
    	j= i+m_MaxEntryPerTxn;
    	if(j > m_TaxId2OffsetsList.size()) {
    		j = m_TaxId2OffsetsList.size();
    	}
    	for(; i < j; i++){
    		Uint8 & offset = m_TaxId2OffsetsList[i].value;
            TTaxId & tax_id = m_TaxId2OffsetsList[i].tax_id;
    		//cerr << m_list[i].id << endl;
			lmdb::val value{&offset, sizeof(offset)};
			lmdb::val key{&tax_id, sizeof(tax_id)};
			bool rc = lmdb::dbi_put(txn, dbi.handle(), key, value, MDB_APPENDDUP);
			if (!rc) {
		 		NCBI_THROW( CSeqDBException, eArgErr, "taxid2offset error for tax id " + NStr::NumericToString(tax_id));
			}
		}
    	txn.commit();
    }
    return;
}

Uint4 s_WirteTaxIds(CNcbiOfstream & os, vector<TTaxId> & tax_ids)
{
	for(unsigned int j =0; j < tax_ids.size(); j++) {
        Int4 tid = TAX_ID_TO(Int4, tax_ids[j]);
		os.write((char *)&tid, 4);
	}
	return tax_ids.size();
}

void CWriteDB_TaxID::x_CreateOidToTaxIdsLookupFile()
{
	if(m_TaxId2OidList.size() == 0) {
 		NCBI_THROW( CSeqDBException, eArgErr, "No tax info for any oid");
	}
	Uint8 total_num_oids = m_TaxId2OidList.back().value + 1;
	string filename = GetFileNameFromExistingLMDBFile(m_Db, ELMDBFileType::eOid2TaxIds);
	Uint8 offset = 0;
	CNcbiOfstream os(filename.c_str(), IOS_BASE::out | IOS_BASE::binary);
	vector<Uint4> offsets(total_num_oids, 0);

	os.write((char *)&total_num_oids, 8);

	for(unsigned int i=0; i < total_num_oids; i++) {
		os.write((char *) &offset, 8);
	}
	os.flush();

	blastdb::TOid count = 0;
	vector<TTaxId> tmp_tax_ids;
	for(unsigned int i = 0; i < m_TaxId2OidList.size(); i++) {
		if(i > 0 && m_TaxId2OidList[i].value != m_TaxId2OidList[i-1].value ) {
			if((m_TaxId2OidList[i].value - m_TaxId2OidList[i-1].value) != 1) {
		 		NCBI_THROW( CSeqDBException, eArgErr, "Input id list not in ascending oid order");
			}
			offsets[count] = s_WirteTaxIds(os, tmp_tax_ids);
			count++;
			tmp_tax_ids.clear();
		}
		tmp_tax_ids.push_back(m_TaxId2OidList[i].tax_id);

	}
	offsets[count] = s_WirteTaxIds(os, tmp_tax_ids);
	_ASSERT(count == m_TaxId2OidList.back().value);

	os.flush();
	os.seekp(8);
	for(unsigned int i = 0; i < total_num_oids; i++) {
		offset += offsets[i];
		os.write((char *) &offset, 8);
	}

	os.flush();
	os.close();
}

Uint4 s_WirteOids(CNcbiOfstream & os, vector<blastdb::TOid> & oids)
{
	blastdb::SortAndUnique <blastdb::TOid> (oids);
	Uint4 num_oids = oids.size();
	os.write((char *)&num_oids, 4);
	for(unsigned int j =0; j < num_oids; j++) {
		os.write((char *)&oids[j], 4);
	}
	return ((num_oids +1) * 4);
}

void CWriteDB_TaxID::x_CreateTaxIdToOidsLookupFile()
{
    sort (m_TaxId2OidList.begin(), m_TaxId2OidList.end(), SKeyValuePair<blastdb::TOid>::cmp_key);
	string filename = GetFileNameFromExistingLMDBFile(m_Db, ELMDBFileType::eTaxId2Oids);
	CNcbiOfstream os(filename.c_str(), IOS_BASE::out | IOS_BASE::binary);
	Uint8 offset =0;

	vector<blastdb::TOid> tmp_oids;
	for(unsigned int i = 0; i < m_TaxId2OidList.size(); i++) {
		if(i > 0 && m_TaxId2OidList[i].tax_id != m_TaxId2OidList[i-1].tax_id ) {
			SKeyValuePair<Uint8> p(m_TaxId2OidList[i-1].tax_id, offset);
			offset += s_WirteOids(os, tmp_oids);
			m_TaxId2OffsetsList.push_back(p);
			tmp_oids.clear();
		}
		tmp_oids.push_back(m_TaxId2OidList[i].value);

	}
   	SKeyValuePair<Uint8> kv(m_TaxId2OidList.back().tax_id, offset);
   	s_WirteOids(os, tmp_oids);
	m_TaxId2OffsetsList.push_back(kv);

	os.flush();
	os.close();
}


void CWriteDB_TaxID::x_Resize()
{
	if (m_TaxId2OidList.size() + 1 > m_ListCapacity) {
    	 m_ListCapacity = m_ListCapacity * 2;
         m_TaxId2OidList.reserve(m_ListCapacity);
     }
}


END_NCBI_SCOPE
