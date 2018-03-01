#ifndef OBJTOOLS_WRITERS_WRITEDB__WRITEDB_LMDB_HPP
#define OBJTOOLS_WRITERS_WRITEDB__WRITEDB_LMDB_HPP

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

/// @file writedb_lmdb.hpp
/// Defines lmdb implementation of string-key database.
///
/// Defines classes:
///     CWriteDB_LMDB
///
/// Implemented for: UNIX, MS-Windows

#include <objtools/blast/seqdb_reader/impl/seqdb_lmdb.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>

#include <memory>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

BEGIN_NCBI_SCOPE


/// This class supports creation of a string accession to integer OID
/// lmdb database

class NCBI_XOBJREAD_EXPORT CWriteDB_LMDB : public CObject
{

public:

    /// Constructor for LMDB write access
    /// @param dbname Database name
    CWriteDB_LMDB(const string& dbname, Uint8 map_size = 1000000000000, Uint8 capacity = 500000);

    // Destructor
    ~CWriteDB_LMDB();

    /// Create volume table
    /// This api should only be called once to create vol info for all vols in the db
    /// @param vol_names      name of the vol        (vector index corresponds to the vol num)
    /// @param vol_num_oids   num of oids in the vol (vector index corresponds to the vol num)
    void InsertVolumesInfo(const vector<string> & vol_names, const vector<blastdb::TOid> & vol_num_oids);

    /// Add entries in bulk as fetched from CSeqDB::GetSeqIDs.
    /// This api needs to be called in sequential order of OIDs
    /// This api should only be called once for each OID
    /// @param oid OID
    /// @param seqids list<CRef<CSeq_id> > from CSeqDB::GetSeqIDs
    /// @return number of rows added to database
    /// @see InsertEntry
    int InsertEntries(const list<CRef<CSeq_id>> & seqids, const blastdb::TOid oid);

    /// Add entries in bulk as fetched from CSeqDB::GetSeqIDs.
    /// This api needs to be called in sequential order of OIDs
    /// This api should only be called once for each OID
    /// @param oid OID
    /// @param seqids vector<CRef<CSeq_id> > from CSeqDB::GetSeqIDs
    /// @return number of rows added to database
    /// @see InsertEntry
    int InsertEntries(const vector<CRef<CSeq_id>> & seqids, const blastdb::TOid oid);

private:
    void x_CommitTransaction();
    void x_InsertEntry(const CRef<CSeq_id> &seqid, const blastdb::TOid oid);
    void x_CreateOidToSeqidsLookupFile();
    void x_Resize();

    string m_Db;
    lmdb::env  &m_Env;
    Uint8 m_ListCapacity;
    unsigned int m_MaxEntryPerTxn;
    struct SKeyValuePair {
    	string id;
    	blastdb::TOid oid;
    	bool saveToOidList;
    	SKeyValuePair() : id(kEmptyStr), oid(kSeqDBEntryNotFound), saveToOidList(false) {}
    	static bool cmp_key(const SKeyValuePair & v, const SKeyValuePair & k) {
   			if(v.id == k.id) {
				blastdb::TOid mask = 0xFF;
   				for(unsigned int i=0; i < sizeof(blastdb::TOid); i++) {
   					if( (v.oid & mask) != (k.oid & mask)) {
   						return (v.oid & mask) < (k.oid & mask);
   					}
   					mask =  mask << 8;
   				}
   			}
    	    return v.id < k.id;
    	}
    };
    vector<SKeyValuePair> m_list;
    void x_Split(vector<SKeyValuePair>::iterator  b, vector<SKeyValuePair>::iterator e, const unsigned int min_chunk_size);
};


/// This class supports creation of tax id list lookup files

class NCBI_XOBJREAD_EXPORT CWriteDB_TaxID : public CObject
{

public:

    /// Constructor for LMDB write access
    /// @param dbname Database name
    CWriteDB_TaxID(const string& dbname, Uint8 map_size = 1000000000000, Uint8 capacity = 500000);

    // Destructor
    ~CWriteDB_TaxID();


    /// Add tax id entries in bulk for each oid
    /// This api needs to be called in sequential order of OIDs
    /// This api should only be called once for each OID
    /// @param oid OID
    /// @param tax_ids list for oid
    /// @return number of rows added to database
    /// @see InsertEntry
    int InsertEntries(const set<Int4> & tax_ids, const blastdb::TOid oid);

private:
    void x_CommitTransaction();
    void x_CreateOidToTaxIdsLookupFile();
    void x_CreateTaxIdToOidsLookupFile();
    void x_Resize();

    string m_Db;
    lmdb::env  &m_Env;
    Uint8 m_ListCapacity;
    unsigned int m_MaxEntryPerTxn;
    template <class valueType>
    struct SKeyValuePair {
    	Int4 tax_id;
    	valueType value;
    	SKeyValuePair(int t, valueType v) : tax_id(t), value(v) {}
    	static bool cmp_key(const SKeyValuePair & v, const SKeyValuePair & k) {
   			if(v.tax_id == k.tax_id) {
   				return v.value < k.value;
   			}
    	    return v.tax_id < k.tax_id;
    	}
    };
    vector<SKeyValuePair<blastdb::TOid> > m_TaxId2OidList;
    vector<SKeyValuePair<Uint8> > m_TaxId2OffsetsList;
};


END_NCBI_SCOPE

#endif
