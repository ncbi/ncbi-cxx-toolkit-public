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
 * Author: Christiam Camacho
 *
 */

/// @file seqdb_lmdb.cpp
/// Implements interface to interact with LMDB files

#include <ncbi_pch.hpp>
#include <objtools/blast/seqdb_reader/impl/seqdb_lmdb.hpp>
#include <objtools/blast/seqdb_reader/impl/seqdbgeneral.hpp>
#include <corelib/ncbifile.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>

BEGIN_NCBI_SCOPE

#define SEQDB_LMDB_TIMING
#ifdef SEQDB_LMDB_TIMING
template<class T>
static string s_FormatNum(T value)
{
    CNcbiOstrstream oss;
    oss.imbue(std::locale(""));
    oss << std::fixed << value;
    return CNcbiOstrstreamToString(oss);
}

#define SPEED(time, nentries) s_FormatNum((size_t)((nentries)/(time)))
#endif /* SEQDB_LMDB_TIMING */

void CBlastLMDBManager::CBlastEnv::InitDbi(lmdb::env & env, ELMDBFileType file_type)
{
    auto txn = lmdb::txn::begin(env, nullptr, MDB_RDONLY);
	if(file_type == eLMDB) {
		try {
	    m_dbis[eDbiAcc2oid] = lmdb::dbi::open(txn, blastdb::acc2oid_str.c_str(), MDB_DUPSORT | MDB_DUPFIXED).handle();
		}
		catch (...){ /* It's ok not to have acc in a db */}
		m_dbis[eDbiVolname] = lmdb::dbi::open(txn, blastdb::volname_str.c_str(), MDB_INTEGERKEY).handle();
		m_dbis[eDbiVolinof] = lmdb::dbi::open(txn, blastdb::volinfo_str.c_str(), MDB_INTEGERKEY).handle();
	}
	else if (file_type == eTaxId2Offsets) {
    	m_dbis[eDbiTaxid2offset] = lmdb::dbi::open(txn, blastdb::taxid2offset_str.c_str()).handle();
	}
	else {
   		NCBI_THROW( CSeqDBException, eArgErr, "Invalid lmdb file type");
	}
	txn.commit();
	txn.reset();
	return;
}

CBlastLMDBManager::CBlastEnv::CBlastEnv(const string & fname, ELMDBFileType file_type, bool read_only, Uint8 map_size) :
		m_Filename(fname), m_FileType(file_type),m_Env(lmdb::env::create()), m_Count(1), m_ReadOnly(read_only)
{
	const MDB_dbi num_db(3);
	m_Env.set_max_dbs(num_db);
	m_dbis.resize(eDbiMax, UINT_MAX);
	if(m_ReadOnly) {
		CFile tf(fname);
		Uint8 readMapSize = (tf.GetLength()/10000 + 1) *10000;
		m_Env.set_mapsize(readMapSize);
		m_Env.open(m_Filename.c_str(), MDB_NOSUBDIR|MDB_NOLOCK|MDB_RDONLY, 0664);
        InitDbi(m_Env,file_type);
	}
	else {
		LOG_POST(Info <<"Initial Map Size: " << map_size);
		/// map_size 0 means use lmdb default
		if(map_size != 0) {
			m_Env.set_mapsize(map_size);
		}
		m_Env.open(m_Filename.c_str(), MDB_NOSUBDIR , 0664);
	}
}

CBlastLMDBManager::CBlastEnv::~CBlastEnv()
{
	for (unsigned int i=0; i < m_dbis.size(); i++){
		if (m_dbis[i] != UINT_MAX) {
			mdb_dbi_close(m_Env,m_dbis[i]);
		}
	}
	m_Env.close();
}

MDB_dbi CBlastLMDBManager::CBlastEnv::GetDbi(EDbiType dbi_type)
{
	if(m_dbis[dbi_type] == UINT_MAX) {
		string err = "DB contains no ";
		switch (dbi_type) {
		case eDbiVolinof:
		case eDbiVolname:
			err += "vol info.";
			break;
		case eDbiAcc2oid:
			err += "accession info.";
			break;
		case eDbiTaxid2offset:
			err += "tax id info";
			break;
		default:
			NCBI_THROW( CSeqDBException, eArgErr, "Invalid dbi type");
		}
		NCBI_THROW( CSeqDBException, eArgErr, err);

	}
	return m_dbis[dbi_type];
}

void CBlastLMDBManager::CBlastEnv::SetMapSize(Uint8 map_size)
{
	if(!m_ReadOnly) {
		m_Env.set_mapsize(map_size);
	}
}

CBlastLMDBManager & CBlastLMDBManager::GetInstance() {
	static CSafeStatic<CBlastLMDBManager> lmdb_manager;
	return lmdb_manager.Get();
}

lmdb::env & CBlastLMDBManager::GetReadEnvVol(const string & fname,  MDB_dbi & db_volname, MDB_dbi & db_volinfo)
{
	CBlastEnv* p = GetBlastEnv(fname, eLMDB);
	db_volinfo = p->GetDbi(CBlastEnv::eDbiVolinof);
	db_volname = p->GetDbi(CBlastEnv::eDbiVolname);
	return p->GetEnv();
}
lmdb::env & CBlastLMDBManager::GetReadEnvAcc(const string & fname, MDB_dbi & db_acc, bool* opened)
{
	CBlastEnv* p = GetBlastEnv(fname, eLMDB, opened);
	db_acc = p->GetDbi(CBlastEnv::eDbiAcc2oid);
	return p->GetEnv();
}
lmdb::env & CBlastLMDBManager::GetReadEnvTax(const string & fname, MDB_dbi & db_tax, bool* opened)
{
	CBlastEnv* p = GetBlastEnv(fname, eTaxId2Offsets, opened);
	db_tax = p->GetDbi(CBlastEnv::eDbiTaxid2offset);
	return p->GetEnv();
}


CBlastLMDBManager::CBlastEnv* CBlastLMDBManager::GetBlastEnv(const string & fname,
                                                             ELMDBFileType file_type,
                                                             bool* opened)
{
	CFastMutexGuard guard(m_Mutex);
	NON_CONST_ITERATE(list <CBlastEnv* >, itr, m_EnvList) {
		if((*itr)->GetFilename() == fname)  {
			(*itr)->AddReference();
            if ( opened && !*opened ) {
            	(*itr)->AddReference();
                *opened = true;
            }
			return (*itr);
		}
	}
	CBlastEnv * p (new CBlastEnv(fname, file_type));
	m_EnvList.push_back(p);
    if ( opened && !*opened ) {
        p->AddReference();
        *opened = true;
    }
	return p;
}

lmdb::env & CBlastLMDBManager::GetWriteEnv(const string & fname, Uint8 map_size)
{
	CFastMutexGuard guard(m_Mutex);
	NON_CONST_ITERATE(list <CBlastEnv* >, itr, m_EnvList) {
		if((*itr)->GetFilename() == fname)  {
			(*itr)->AddReference();
			return (*itr)->GetEnv();
		}
	}
	CBlastEnv * p (new CBlastEnv(fname, eLMDBFileTypeEnd, false, map_size));
	m_EnvList.push_back(p);
	return p->GetEnv();
}


void CBlastLMDBManager::CloseEnv(const string & fname)
{
	CFastMutexGuard guard(m_Mutex);
	NON_CONST_ITERATE(list <CBlastEnv* >, itr, m_EnvList) {
		if((*itr)->GetFilename() == fname)  {
			if((*itr)->RemoveReference() == 0) {
				delete *itr;
				itr = m_EnvList.erase(itr);
				break;
			}
		}
	}
}

CBlastLMDBManager::~CBlastLMDBManager()
{
	NON_CONST_ITERATE(list <CBlastEnv* >, itr, m_EnvList) {
		delete *itr;
	}
	m_EnvList.clear();
}

CSeqDBLMDB::CSeqDBLMDB(const string & fname)
    : m_LMDBFile(fname),
      m_Oid2SeqIdsFile(GetFileNameFromExistingLMDBFile(fname, ELMDBFileType::eOid2SeqIds)),
      m_Oid2TaxIdsFile(GetFileNameFromExistingLMDBFile(fname, ELMDBFileType::eOid2TaxIds)),
      m_TaxId2OidsFile(GetFileNameFromExistingLMDBFile(fname, ELMDBFileType::eTaxId2Oids)),
      m_TaxId2OffsetsFile(GetFileNameFromExistingLMDBFile(fname, ELMDBFileType::eTaxId2Offsets)),
      m_LMDBFileOpened(false)
{
}

CSeqDBLMDB::~CSeqDBLMDB()
{
    if ( m_LMDBFileOpened ) {
        CBlastLMDBManager::GetInstance().CloseEnv(m_LMDBFile);
        m_LMDBFileOpened = false;
    }
}

void 
CSeqDBLMDB::GetOid(const string & accession, vector<blastdb::TOid> & oids, const bool allow_dup) const
{
	try {
    oids.clear();
    {
    MDB_dbi dbi_handle;
	lmdb::env & env = CBlastLMDBManager::GetInstance().GetReadEnvAcc(m_LMDBFile, dbi_handle, &m_LMDBFileOpened);
    lmdb::dbi dbi(dbi_handle);
    auto txn = lmdb::txn::begin(env, nullptr, MDB_RDONLY);
    auto cursor = lmdb::cursor::open(txn, dbi);
        
    string acc = accession;
    lmdb::val data2find(acc);

    if (cursor.get(data2find, MDB_SET)) {            
        lmdb::val k, val;          
        cursor.get(k, val, MDB_GET_CURRENT);
        const char* d = val.data();
        oids.push_back(((d[3] << 24)&0xFF000000) | ((d[2] << 16) & 0xFF0000) | ((d[1] << 8) & 0xFF00) | (d[0]&0xFF));

        if(allow_dup) {
            while (cursor.get(k,val, MDB_NEXT_DUP)) {
                d = val.data();
                oids.push_back(((d[3] << 24)&0xFF000000) | ((d[2] << 16) & 0xFF0000) | ((d[1] << 8) & 0xFF00) | (d[0]&0xFF));
            }
        }
    }
    cursor.close();
    txn.reset();
    }
    CBlastLMDBManager::GetInstance().CloseEnv(m_LMDBFile);
    } catch (lmdb::error & e) {
   		string dbname;
       	CSeqDB_Path(m_LMDBFile).FindBaseName().GetString(dbname);
       	if(e.code() == MDB_NOTFOUND) {
    		NCBI_THROW( CSeqDBException, eArgErr, "Seqid list specified but no accession table is found in " + dbname);
       	}
       	else {
    		NCBI_THROW( CSeqDBException, eArgErr, "Accessions to Oids lookup error in " + dbname);
       	}
    }
}


void CSeqDBLMDB::GetVolumesInfo(vector<string> & vol_names, vector<blastdb::TOid> & vol_num_oids)
{
    MDB_dbi db_volname_handle;
    MDB_dbi db_volinfo_handle;
	lmdb::env & env = CBlastLMDBManager::GetInstance().GetReadEnvVol(m_LMDBFile, db_volname_handle, db_volinfo_handle);
	vol_names.clear();
	vol_num_oids.clear();
	{
    auto txn = lmdb::txn::begin(env, nullptr, MDB_RDONLY);
    lmdb::dbi db_volname(db_volname_handle);
    lmdb::dbi db_volinfo(db_volinfo_handle);
    MDB_stat volinfo_stat, volname_stat;
    lmdb::dbi_stat(txn, db_volinfo, &volinfo_stat);
    lmdb::dbi_stat(txn, db_volname, &volname_stat);
    if(volinfo_stat.ms_entries != volname_stat.ms_entries) {
   		NCBI_THROW( CSeqDBException, eArgErr, "Volinfo error ");
    }

    vol_names.resize(volinfo_stat.ms_entries);
    vol_num_oids.resize(volinfo_stat.ms_entries);

    auto cursor_volname = lmdb::cursor::open(txn, db_volname);
    auto cursor_volinfo = lmdb::cursor::open(txn, db_volinfo);
    for (unsigned int i=0; i < volinfo_stat.ms_entries; i++) {
    	lmdb::val data2find(&i, sizeof(Int4));
    	if (cursor_volname.get(data2find, MDB_SET)) {
    		{
    			lmdb::val k, val;
    			cursor_volname.get(k, val, MDB_GET_CURRENT);
    			vol_names[i].assign(val.data(), val.size());
    		}
    		if (cursor_volinfo.get(data2find, MDB_SET)) {
    			lmdb::val k, val;
    			cursor_volinfo.get(k, val, MDB_GET_CURRENT);
    			const char* d = val.data();
    			vol_num_oids[i] = (((d[3] << 24)&0xFF000000) | ((d[2] << 16) & 0xFF0000) | ((d[1] << 8) & 0xFF00) | (d[0]&0xFF));
    		}
    		else {
    			NCBI_THROW( CSeqDBException, eArgErr, "No volinfo for " + vol_names[i]);
    		}
    	 }
   	}
    cursor_volname.close();
    cursor_volinfo.close();
    txn.reset();
	}
    CBlastLMDBManager::GetInstance().CloseEnv(m_LMDBFile);
}

void
CSeqDBLMDB::GetOids(const vector<string>& accessions, vector<blastdb::TOid>& oids) const
{
    try {
    oids.clear();
    oids.resize(accessions.size(), kSeqDBEntryNotFound);

    MDB_dbi dbi_handle;
	lmdb::env & env = CBlastLMDBManager::GetInstance().GetReadEnvAcc(m_LMDBFile, dbi_handle, &m_LMDBFileOpened);
	{
    lmdb::dbi dbi(dbi_handle);
    auto txn = lmdb::txn::begin(env, nullptr, MDB_RDONLY);

    auto cursor = lmdb::cursor::open(txn, dbi);

    unsigned int i=0;
    for (i=0; i < accessions.size(); i++) {
    	string acc = accessions[i];
        lmdb::val data2find(acc);
        if (cursor.get(data2find, MDB_SET)) {
            lmdb::val k, val;
            cursor.get(k, val, MDB_GET_CURRENT);
            const char* d = val.data();
            oids[i] = (((d[3] << 24)&0xFF000000) | ((d[2] << 16) & 0xFF0000) | ((d[1] << 8) & 0xFF00) | (d[0]&0xFF));
        }
    }

    cursor.close();
    txn.reset();
	}
    CBlastLMDBManager::GetInstance().CloseEnv(m_LMDBFile);
    } catch (lmdb::error & e) {
   		string dbname;
       	CSeqDB_Path(m_LMDBFile).FindBaseName().GetString(dbname);
       	if(e.code() == MDB_NOTFOUND) {
    		NCBI_THROW( CSeqDBException, eArgErr, "Seqid list specified but no accession table is found in " + dbname);
       	}
       	else {
    		NCBI_THROW( CSeqDBException, eArgErr, "Accessions to Oids lookup error in " + dbname);
       	}
    }
}

struct SOidSeqIdPair
{
	SOidSeqIdPair(blastdb::TOid o, const string & i) : oid(o), id(i) {}
	blastdb::TOid oid;
	string id;
	static bool cmp_oid(const SOidSeqIdPair & v, const SOidSeqIdPair & k) {
		if(v.oid == k.oid) {
			return (v.id < k.id);
		}
		return (v.oid < k.oid );
	}
};

class CLookupSeqIds
{
public:
	CLookupSeqIds(CMemoryFile & file): m_IndexStart((Uint8*) file.GetPtr()), m_DataStart((char *) file.GetPtr()) {
		if(m_IndexStart == NULL){
			NCBI_THROW( CSeqDBException, eArgErr, "Failed to open oid-to-seqid lookup file");
		}

		Uint8 num_of_oids = *m_IndexStart;
		m_IndexStart ++;
		m_DataStart += (8 * (num_of_oids + 1));
	}

	inline void GetSeqIdListForOid(blastdb::TOid oid, vector<string> & idlist);
private:

	Uint8 * m_IndexStart;
	char * m_DataStart;
};

void CLookupSeqIds::GetSeqIdListForOid(blastdb::TOid oid, vector<string> & idlist)
{
	Uint8 * index_ptr = m_IndexStart + oid;
	Char * end = m_DataStart + (*index_ptr);
	index_ptr--;
	Char * begin = (oid == 0) ? m_DataStart:m_DataStart + (*index_ptr);
	while (begin < end) {
		unsigned char id_len = *begin;
		begin ++;
		if(id_len == 0xFF) {
			Uint4 long_id_len = *((Uint4 *) begin);
			begin +=4;
			string id;
			id.assign(begin, long_id_len);
			begin += long_id_len;
			idlist.push_back(id);
		}
		else {
			string id;
			id.assign(begin, id_len);
			begin += id_len;
			idlist.push_back(id);
		}
	}
}

bool s_CompareIdList(vector<string> & file_idlist, vector<string> &input_idlist)
{
	bool rv = false;
	vector<string>::iterator f_itr = file_idlist.begin();
	vector<string>::iterator i_itr = input_idlist.begin();
	while(f_itr != file_idlist.end() && i_itr != input_idlist.end()) {
		if(*i_itr == *f_itr) {
			i_itr++;
			f_itr++;
			continue;
		}
		else {
			CSeq_id seq_id(*i_itr, CSeq_id::fParse_RawText | CSeq_id::fParse_AnyLocal | CSeq_id::fParse_PartialOK);
			// Input id is PDB with just mol id
			if(seq_id.IsPdb() &&  !seq_id.GetPdb().IsSetChain_id()) {
				CSeq_id file_seqid(*f_itr, CSeq_id::fParse_RawText | CSeq_id::fParse_AnyLocal | CSeq_id::fParse_PartialOK);
				if (file_seqid.IsPdb() && file_seqid.GetPdb().GetMol().Get() == *i_itr) {
					f_itr++;
					string tmp_pdb = *i_itr;
					while ((f_itr != file_idlist.end()) && ((*f_itr).find_first_of(tmp_pdb) == 0)){
						f_itr++;
					}
					// Skip pdb id in input list but with chain id
					while ((i_itr != input_idlist.end()) && ((*i_itr).find_first_of(tmp_pdb) == 0)){
						i_itr++;
					}
					continue;
				}
			}
			else {
				CSeq_id file_seq_id(*f_itr, CSeq_id::fParse_RawText | CSeq_id::fParse_AnyLocal | CSeq_id::fParse_PartialOK);
				if( file_seq_id.GetSeqIdString(false) == *i_itr) {
					i_itr++;
					// Skip identical id in input list but with version
					if((i_itr != input_idlist.end()) && (file_seq_id.GetSeqIdString(true) == *i_itr)){
						i_itr++;
					}
					f_itr++;
					continue;
				}
			}
			break;
		}
	}
	if(f_itr == file_idlist.end()){
		rv=true;
	}

	file_idlist.clear();
	input_idlist.clear();
	return rv;
}

void
CSeqDBLMDB::NegativeSeqIdsToOids(const vector<string>& ids, vector<blastdb::TOid>& rv) const
{
	rv.clear();
	vector<blastdb::TOid> oids;
	GetOids(ids, oids);
	vector<SOidSeqIdPair> pairs;
	for (unsigned int i=0; i < ids.size(); i++) {
		if(oids[i] == kSeqDBEntryNotFound) {
			continue;
		}
		else {
			SOidSeqIdPair p(oids[i], ids[i]);
			pairs.push_back(p);
		}
	}

	if(pairs.size() == 0) {
		return;
	}

	sort (pairs.begin(), pairs.end(), SOidSeqIdPair::cmp_oid);

	CMemoryFile oid_file(m_Oid2SeqIdsFile);
	CLookupSeqIds lookup(oid_file);
	blastdb::TOid current_oid = 0;
	unsigned int i=0;
	while (i < pairs.size()) {
		vector<string> file_idlist;
		vector<string> input_idlist;
		current_oid = pairs[i].oid;
		lookup.GetSeqIdListForOid(current_oid, file_idlist);
		while ((i < pairs.size()) && (current_oid == pairs[i].oid)) {
			input_idlist.push_back(pairs[i].id);
			i++;
		}
		if(s_CompareIdList(file_idlist, input_idlist)) {
			rv.push_back(current_oid);
		}

	}

}

void CSeqDBLMDB::GetDBTaxIds(vector<TTaxId> & tax_ids) const
{

	tax_ids.clear();
    try {
   	MDB_dbi dbi_handle;
	lmdb::env & env = CBlastLMDBManager::GetInstance().GetReadEnvTax(m_TaxId2OffsetsFile, dbi_handle);
	{
		auto txn = lmdb::txn::begin(env, nullptr, MDB_RDONLY);
    	auto dbi(dbi_handle);
    	auto cursor = lmdb::cursor::open(txn, dbi);
    	lmdb::val key;
        while (cursor.get(key, MDB_NEXT)) {
        	TTaxId taxid = TAX_ID_FROM(Int4, *((Int4 *)key.data()));
        	tax_ids.push_back(taxid);
        }
        cursor.close();
        txn.reset();
	}
	}
    catch (lmdb::error & e) {
   		string dbname;
       	CSeqDB_Path(m_TaxId2OffsetsFile).FindBaseName().GetString(dbname);
       	if(e.code() == MDB_NOTFOUND) {
    		NCBI_THROW( CSeqDBException, eArgErr, "No taxonomy info found in " + dbname);
       	}
       	else {
    		NCBI_THROW( CSeqDBException, eArgErr, "Taxonomy Id to Oids lookup error in " + dbname);
       	}
    }
    CBlastLMDBManager::GetInstance().CloseEnv(m_TaxId2OffsetsFile);
}

void CSeqDBLMDB::GetOidsForTaxIds(const set<TTaxId> & tax_ids, vector<blastdb::TOid>& oids, vector<TTaxId> & tax_ids_found) const
{

    try {
    oids.clear();
    tax_ids_found.clear();
    vector<Uint8> offsets;
   	MDB_dbi dbi_handle;
	lmdb::env & env = CBlastLMDBManager::GetInstance().GetReadEnvTax(m_TaxId2OffsetsFile, dbi_handle);
	{
    auto txn = lmdb::txn::begin(env, nullptr, MDB_RDONLY);
   	lmdb::dbi dbi(dbi_handle);
    auto cursor = lmdb::cursor::open(txn, dbi);
    ITERATE(set<TTaxId>, itr, tax_ids) {
    	Int4 tax_id = TAX_ID_TO(Int4, *itr);
        lmdb::val data2find(tax_id);

        if (cursor.get(data2find, MDB_SET)) {
        	lmdb::val k, val;
            cursor.get(k, val, MDB_GET_CURRENT);
            const char* d = val.data();
            offsets.push_back((((Uint8) d[7] << 56) &0xFF00000000000000) | (((Uint8) d[6] << 48) & 0xFF000000000000) |
            		          (((Uint8) d[5] << 40) &0xFF0000000000)     | (((Uint8) d[4] << 32) & 0xFF00000000)     |
            		          (((Uint8) d[3] << 24) &0xFF000000)         | (((Uint8) d[2] << 16) & 0xFF0000)         |
            		          (((Uint8) d[1] << 8)  &0xFF00)             | ((Uint8)  d[0]&0xFF));
            while (cursor.get(k,val, MDB_NEXT_DUP)) {
            	d = val.data();
            	offsets.push_back((((Uint8) d[7] << 56) &0xFF00000000000000) | (((Uint8) d[6] << 48) & 0xFF000000000000) |
            					  (((Uint8) d[5] << 40) &0xFF0000000000)     | (((Uint8) d[4] << 32) & 0xFF00000000)     |
            		              (((Uint8) d[3] << 24) &0xFF000000)         | (((Uint8) d[2] << 16) & 0xFF0000)         |
            		              (((Uint8) d[1] << 8)  &0xFF00)             | ((Uint8)  d[0]&0xFF));
            }
            tax_ids_found.push_back(*itr);
        }
	}
    cursor.close();
    txn.reset();
	}
    CBlastLMDBManager::GetInstance().CloseEnv(m_TaxId2OffsetsFile);

    blastdb::SortAndUnique <Uint8> (offsets);

   	CMemoryFile oid_file(m_TaxId2OidsFile);
   	const char * start_ptr = (char *) oid_file.GetPtr();
   	for (unsigned int i=0; i < offsets.size(); i++) {
   		Uint4 * list_ptr = (Uint4 * ) (start_ptr + offsets[i]);
   		Uint4 num_of_oids = *list_ptr;
   		Uint4 count = 0 ;
   		list_ptr ++;
   		while(count < num_of_oids) {
   			oids.push_back(*list_ptr);
   			count++;
   			list_ptr++;
   		}
   	}

    blastdb::SortAndUnique <blastdb::TOid> (oids);

    } catch (lmdb::error & e) {
   		string dbname;
       	CSeqDB_Path(m_TaxId2OffsetsFile).FindBaseName().GetString(dbname);
       	if(e.code() == MDB_NOTFOUND) {
    		NCBI_THROW( CSeqDBException, eArgErr, "No taxonomy info found in " + dbname);
       	}
       	else {
    		NCBI_THROW( CSeqDBException, eArgErr, "Taxonomy Id to Oids lookup error in " + dbname);
       	}
    }
}


class CLookupTaxIds
{
public:
	CLookupTaxIds(CMemoryFile & file): m_IndexStart((Uint8*) file.GetPtr()), m_DataStart((Int4 *) file.GetPtr()) {
		if(m_IndexStart == NULL){
			NCBI_THROW( CSeqDBException, eArgErr, "Failed to open oid-to-taxids lookup file");
		}

		Uint8 num_of_oids = *m_IndexStart;
		m_IndexStart ++;
		m_DataStart += (2* (num_of_oids + 1));
	}

	inline void GetTaxIdListForOid(blastdb::TOid oid, vector<TTaxId> & taxid_list);
private:

	Uint8 * m_IndexStart;
	Int4 * m_DataStart;
};

void CLookupTaxIds::GetTaxIdListForOid(blastdb::TOid oid, vector<TTaxId> & taxid_list)
{
	taxid_list.clear();
	Uint8 * index_ptr = m_IndexStart + oid;
	Int4 * end = m_DataStart + (*index_ptr);
	index_ptr--;
	Int4 * begin = (oid == 0) ? m_DataStart:m_DataStart + (*index_ptr);
	while (begin < end) {
		taxid_list.push_back(TAX_ID_FROM(Int4, *begin));
		begin++;
	}
}

void
CSeqDBLMDB::NegativeTaxIdsToOids(const set<TTaxId>& tax_ids, vector<blastdb::TOid>& rv, vector<TTaxId> & tax_ids_found) const
{
	rv.clear();
	vector<blastdb::TOid> oids;
	GetOidsForTaxIds(tax_ids, oids, tax_ids_found);

	CMemoryFile oid_file(m_Oid2TaxIdsFile);
	set<TTaxId> tax_id_list(tax_ids.begin(), tax_ids.end());
	CLookupTaxIds lookup(oid_file);
	for(unsigned int i=0; i < oids.size(); i++) {
		vector<TTaxId>  file_list;
		lookup.GetTaxIdListForOid(oids[i], file_list);
		if(file_list.size() > tax_ids.size()) {
			continue;
		}
		else {
			unsigned int j = 0;
			for(; j < file_list.size(); j++) {
				if(tax_id_list.find(file_list[j]) == tax_id_list.end()) {
					break;
				}
			}
			if(j == file_list.size()) {
				rv.push_back(oids[i]);
			}
		}
	}
}

void CSeqDBLMDB::GetTaxIdsForOids(const vector<blastdb::TOid> & oids, set<TTaxId> & tax_ids) const
{
	CMemoryFile oid_file(m_Oid2TaxIdsFile);
	CLookupTaxIds lookup(oid_file);
	for(unsigned int i=0; i < oids.size(); i++) {
		vector<TTaxId>  taxid_list;
		lookup.GetTaxIdListForOid(oids[i], taxid_list);
		tax_ids.insert(taxid_list.begin(), taxid_list.end());
	}
}


string BuildLMDBFileName(const string& basename, bool is_protein, bool use_index, unsigned int index)
{
    if (basename.empty()) {
        throw invalid_argument("Basename is empty");
    }

    string vol_str=kEmptyStr;
    if(use_index) {
    	vol_str = (index > 9) ?".": ".0";
    	vol_str += NStr::UIntToString(index);
    }
    return basename + vol_str + (is_protein ? ".pdb" : ".ndb");
}

string GetFileNameFromExistingLMDBFile(const string& lmdb_filename, ELMDBFileType file_type)
{

	string filename (lmdb_filename, 0, lmdb_filename.size() - 2);
	switch (file_type) {
		case eLMDB :
			filename += "db";
		break;
		case eOid2SeqIds :
			filename +="os";
		break;
		case eOid2TaxIds :
			filename +="ot";
		break;
		case eTaxId2Offsets :
			filename += "tf";
		break;
		case eTaxId2Oids :
			filename += "to";
		break;
		default :
			NCBI_THROW( CSeqDBException, eArgErr, "Invalid LMDB file type");
		break;
	}
	return filename;
}

void DeleteLMDBFiles(bool db_is_protein, const string & filename)
{
	vector<string> extn;
	SeqDB_GetLMDBFileExtensions(db_is_protein, extn);
	ITERATE(vector<string>, itr, extn) {
		 CFile f(filename + "." + (*itr));
		 if (f.Exists()) {
			 f.Remove();
		 }
	}
}


END_NCBI_SCOPE

