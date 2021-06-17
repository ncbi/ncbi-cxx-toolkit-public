#ifndef OBJTOOLS_READERS_SEQDB__SEQDBLMDBSET_HPP
#define OBJTOOLS_READERS_SEQDB__SEQDBLMDBSET_HPP

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
 * Author:  Amelia Fong
 *
 */

/// @file seqdbLMDBset.hpp
/// Manages a set of LMDB files.
///
/// Defines classes:
///     CSeqDBLMDBSet
///     CSeqDBLMDBEntry
///
/// Implemented for: UNIX, MS-Windows

#include <objtools/blast/seqdb_reader/impl/seqdb_lmdb.hpp>
#include <algo/blast/core/ncbi_std.h>
#include "seqdbvolset.hpp"

BEGIN_NCBI_SCOPE

/// Import definitions from the ncbi::objects namespace.
USING_SCOPE(objects);

/// CSeqDBLMDBEntry
///
/// This class controls access to the CSeqDBLMDB class.  It contains
/// data that is not relevant to the internal operation of am LMDB file,
/// but is associated with  operations over the volume LMDB
/// set as a whole, such as the starting OID of the LMDB file and masking
/// information (GI and OID lists).

class CSeqDBLMDBEntry : public CObject {
public:
	typedef blastdb::TOid TOid;
    /// Constructor
    ///
    /// This creates a object containing the specified volume object
    /// pointer.  Although this object owns the pointer, it uses a
    /// vector, so it does not keep an auto pointer or CRef<>.
    /// Instead, the destructor of the CSeqDBLMDBSet class deletes the
    /// volumes by calling Free() in a destructor.  Using indirect
    /// pointers (CRef<> for example) would require slightly more
    /// cycles in several performance critical paths.
    ///
    /// @param new_vol
    ///   A pointer to a volume.
    CSeqDBLMDBEntry(const string & name, TOid start_oid, const vector<string> & vol_names);

    ~CSeqDBLMDBEntry();


    /// Get the starting OID in this volume's range.
    ///
    /// This returns the first OID in this volume's OID range.
    ///
    /// @return The starting OID of the range
    int GetOIDStart() const { return m_OIDStart; }

    /// Get the ending OID in this volume's range.
    ///
    /// This returns the first OID past the end of this volume's OID
    /// range.
    ///
    /// @return
    ///   The ending OID of the range
    int GetOIDEnd() const { return m_OIDEnd; }

    string GetLMDBFileName() const { return m_LMDBFName; }

    void AccessionToOids(const string & acc, vector<TOid>  & oids) const;

    void AccessionsToOids(const vector<string>& accs, vector<TOid>& oids) const;

    void NegativeSeqIdsToOids(const vector<string>& ids, vector<blastdb::TOid>& rv) const;

    void TaxIdsToOids(const set<TTaxId>& tax_ids, vector<blastdb::TOid>& rv, vector<TTaxId> & tax_ids_found) const;

    void NegativeTaxIdsToOids(const set<TTaxId>& tax_ids, vector<blastdb::TOid>& rv, vector<TTaxId> & tax_ids_found) const;

    void GetDBTaxIds(vector<TTaxId> & tax_ids) const;

    void GetTaxIdsForOids(const vector<blastdb::TOid> & oids, set<TTaxId> & tax_ids) const;

private:
    void x_AdjustOidsOffset(vector<TOid> & oids) const;
    void x_AdjustOidsOffset_TaxList(vector<TOid> & oids) const;

    string 				m_LMDBFName;
    /// The underlying volume object
    CRef<CSeqDBLMDB>	m_LMDB;

    /// The start of the OID range.
    TOid             	m_OIDStart;

    /// The end of the OID range.
    TOid             	m_OIDEnd;

    struct SVolumeInfo {
    	TOid skipped_oids;
    	TOid max_oid;
    	string vol_name;
    };
    vector<SVolumeInfo> m_VolInfo;
    bool m_isPartial;

};


/// CSeqDBLMDBSet
///
/// This class stores a set of CSeqDBVol objects and defines an
/// interface to control usage of them.  Several methods are provided
/// to create the set of volumes, or to get the required volumes by
/// different criteria.  Also, certain methods perform operations over
/// the set of volumes.  The CSeqDBLMDBEntry class, defined internally
/// to this one, provides some of this abstraction.
class CSeqDBLMDBSet {
public:
	typedef blastdb::TOid TOid;
    /// Standard Constructor
    ///
    ///  An obejct to manage the LMDB file set associted with the
	///  input seq db volumes
    CSeqDBLMDBSet( const CSeqDBVolSet & m_VolSet);

    /// Default Constructor
    ///
    /// An empty volume set will be created; this is in support of the
    /// CSeqDBExpert class's default constructor.
    CSeqDBLMDBSet();

    /// Destructor
    ///
    /// The destructor will release all resources still held, but some
    /// of the resources will probably already be cleaned up via a
    /// call to the UnLease method.
    ~CSeqDBLMDBSet();

    void AccessionToOids(const string & acc, vector<TOid>  & oids) const;

    void AccessionsToOids(const vector<string>& accs, vector<TOid>& oids) const;

    bool IsBlastDBVersion5() const { return (m_LMDBEntrySet.empty()? false:true); }

    void NegativeSeqIdsToOids(const vector<string>& ids, vector<blastdb::TOid>& rv) const;

    void TaxIdsToOids(set<TTaxId>& tax_ids, vector<blastdb::TOid>& rv) const;

    void NegativeTaxIdsToOids(set<TTaxId>& tax_ids, vector<blastdb::TOid>& rv) const;

    void GetDBTaxIds(set<TTaxId> & tax_ids) const;

    void GetTaxIdsForOids(const vector<blastdb::TOid> & oids, set<TTaxId> & tax_ids) const;

private:
    vector<CRef<CSeqDBLMDBEntry> >  m_LMDBEntrySet;

};

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBLMDBSET_HPP

