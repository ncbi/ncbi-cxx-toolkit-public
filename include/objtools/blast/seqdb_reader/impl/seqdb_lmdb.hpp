#ifndef OBJTOOLS_READERS_BLAST__SEQDB__SEQDB_LMDB_HPP
#define OBJTOOLS_READERS_BLAST__SEQDB__SEQDB_LMDB_HPP

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

/// @file seqdb_lmdb.hpp
/// Defines interface to interact with LMDB files

#include <util/lmdbxx/lmdb++.h>
#include <objtools/blast/seqdb_reader/seqdbcommon.hpp>
#include <corelib/ncbi_safe_static.hpp>

BEGIN_NCBI_SCOPE


class NCBI_XOBJREAD_EXPORT CSeqDBLMDB : public CObject
{
public:
    CSeqDBLMDB(const string & fname);
    virtual ~CSeqDBLMDB();
    CSeqDBLMDB& operator=(const CSeqDBLMDB&) = delete;
    CSeqDBLMDB(const CSeqDBLMDB&) = delete;

    /// Get OIDs for a vector of string accessions.
    /// Accessions may have ".version" appended.
    /// Returned vector of OIDs will have the same length as vector
    /// of accessions in one-to-one correspondence.
    /// Any accessions which are not found will be assigned OIDs
    /// of kSeqDBEntryNotFound (-1).
    /// @param accessions Vector of string accessions [in]
    /// @param oids Reference to vector of TOid to receive found OIDs [out]
    void GetOids(const vector<string>& accessions, vector<blastdb::TOid>& oids) const;

    /// Get OIDs for single string accession.
    /// String accession may have ".version" appended.
    /// If there are no matches, oids will be returned empty.
    /// If there are multiple matches, and allow_dup is true,
    /// @param accession String accession (with or without version suffix) [in]
    /// @param oids Reference to vector of TOid to receive found OIDs [out]
    /// @param allow_dup If true, return all OIDs which match (default false) [in]
    void GetOid(const string & accession, vector<blastdb::TOid>& oids,  const bool allow_dup = false) const;

    /// Return info for all volumes
    /// @param vol_names Reference to vector to receive volume names [out]
    /// @param vol_num_oids Reference to vector to receive number of OIDs
    /// in each volume [out]
    void GetVolumesInfo(vector<string> & vol_names, vector<blastdb::TOid> & vol_num_oids);

    /// Get Oids excluded from a vector of input accessions
    /// An oid only get exlcuded if all its seqids are found in the input list
    /// Note that the order or number of oids returned are independent of the input id list
    /// @param ids Accessions to exclude
    /// @param rv  Oids that are excluded
    void NegativeSeqIdsToOids(const vector<string>& ids, vector<blastdb::TOid>& rv) const;

    /// Get Oids for Tax Ids list, idenitcal Oids are merged.
    /// @param tax_ids  Input tax ids /Output tax ids found
    /// @param oids  Oids found for input tax ids
    void GetOidsForTaxIds(const set<TTaxId> & tax_ids, vector<blastdb::TOid>& oids, vector<TTaxId> & tax_ids_found) const;

    /// Get Oids to exclude for Tax ids
    /// @parm ids Input tax ids to exclude /Output tax ids found
    /// @param rv Oids to exclude based on input tax id list
    void NegativeTaxIdsToOids(const set<TTaxId>& ids, vector<blastdb::TOid>& rv, vector<TTaxId> & tax_ids_found) const;

    /// Get All Unique Tax Ids for db
    /// @parma tax_ids  Return all unique tax ids found in db
    void GetDBTaxIds(vector<TTaxId> & tax_ids) const;

    /// Get Tax Ids for oid list
    /// @param oids Input oid list
    /// @param tax_ids Output tax id list
    void GetTaxIdsForOids(const vector<blastdb::TOid> & oids, set<TTaxId> & tax_ids) const;

private:
    string  m_LMDBFile;
    string  m_Oid2SeqIdsFile;
    string  m_Oid2TaxIdsFile;
    string  m_TaxId2OidsFile;
    string  m_TaxId2OffsetsFile;
    mutable bool m_LMDBFileOpened;
};

/// Build the canonical LMDB file name for BLAST databases
/// @param basename Base name of the BLAST database [in]
/// @param is_protein whether the database contains proteins or not [in]
/// @return a file name
/// @throws std::invalid_argument in case of empty basename argument
NCBI_XOBJREAD_EXPORT
string BuildLMDBFileName(const string& basename, bool is_protein, bool use_index=false, unsigned int index=0);

enum ELMDBFileType
{
	eLMDB,
	eOid2SeqIds,
	eOid2TaxIds,
	eTaxId2Offsets,
	eTaxId2Oids,
	eLMDBFileTypeEnd
};

NCBI_XOBJREAD_EXPORT
string GetFileNameFromExistingLMDBFile(const string& lmdb_filename, ELMDBFileType file_type);

NCBI_XOBJREAD_EXPORT
void DeleteLMDBFiles(bool db_is_protein, const string & lmdb_filename);


/// Class for manageing LMDB env, each env should only be open once
class NCBI_XOBJREAD_EXPORT CBlastLMDBManager
{
public:
	static CBlastLMDBManager & GetInstance();
	lmdb::env & GetReadEnvVol(const string & fname, MDB_dbi & db_volname, MDB_dbi & db_volinfo);
	lmdb::env & GetReadEnvAcc(const string & fname, MDB_dbi & db_acc, bool* opened = 0);
	lmdb::env & GetReadEnvTax(const string & fname, MDB_dbi & db_tax, bool* opened = 0);
	lmdb::env & GetWriteEnv(const string & fname, Uint8 map_size);

	void CloseEnv(const string & fname);

private:
	class CBlastEnv
	{
	public:
		CBlastEnv(const string & fname, ELMDBFileType file_type, bool read_only = true, Uint8 map_size =0);
		lmdb::env & GetEnv() { return m_Env; }
		const string & GetFilename () const { return m_Filename; }
		~CBlastEnv();
		unsigned int AddReference(){ m_Count++; return m_Count;}
		unsigned int RemoveReference(){ m_Count--; return m_Count;}
		enum EDbiType {
				eDbiVolinof,
				eDbiVolname,
				eDbiAcc2oid,
				eDbiTaxid2offset,
				eDbiMax
		};
		MDB_dbi GetDbi(EDbiType dbi_type);
		void InitDbi(lmdb::env & env, ELMDBFileType file_type);
		void SetMapSize(Uint8 map_size);
		bool IsReadOnly() { return m_ReadOnly; }

	private:
		string m_Filename;
		ELMDBFileType m_FileType;
		lmdb::env m_Env;
		unsigned int m_Count;
		bool m_ReadOnly;
		vector<MDB_dbi> m_dbis;
	};

	CBlastEnv* GetBlastEnv(const string & fname, ELMDBFileType file_type, bool* opened = 0);
	CBlastLMDBManager(){}
	~CBlastLMDBManager();
	friend class CSafeStatic_Allocator<CBlastLMDBManager>;
	list <CBlastEnv * > m_EnvList;
	CFastMutex m_Mutex;
};

BEGIN_SCOPE(blastdb)
template <typename V> inline void NCBI_XOBJREAD_EXPORT SortAndUnique(vector<V> & a)
{
	std::sort(a.begin(), a.end());
	typename vector<V>::iterator itr = std::unique(a.begin(), a.end());
	a.resize(std::distance(a.begin(), itr));
}

const string volinfo_str = "volinfo";
const string volname_str = "volname";
const string acc2oid_str = "acc2oid";
const string taxid2offset_str = "taxid2offset";
END_SCOPE(blastdb)


END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_BLAST__SEQDB__SEQDB_LMDB_HPP
