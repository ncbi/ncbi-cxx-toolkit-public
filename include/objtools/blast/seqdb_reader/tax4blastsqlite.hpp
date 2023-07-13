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
 */

#ifndef OBJTOOLS_BLAST_SEQDB_READER___TAX4BLAST_SQLITE__HPP
#define OBJTOOLS_BLAST_SEQDB_READER___TAX4BLAST_SQLITE__HPP

/// @file tax4blastsqlite.hpp
/// @author Christiam Camacho
/// Defines a class to retrieve taxonomic information useful in BLAST

#include <corelib/ncbistl.hpp>
#include <corelib/ncbiobj.hpp>
#include <objtools/blast/seqdb_reader/itax4blast.hpp>
#include <db/sqlite/sqlitewrapp.hpp>

BEGIN_NCBI_SCOPE

/// Clas to retrieve taxonomic information for filtering BLASTDBs
class NCBI_XOBJREAD_EXPORT CTaxonomy4BlastSQLite : public ITaxonomy4Blast
{
public:

    /// Default name for SQLite database (by default installed in $BLASTDB)
    static const string kDefaultName;

    /// Parametrized constructor, uses its arguments to initialize database
    /// @param dbname base name of the database file. If not provided, kDefaultName is used [in]
    /// @throw CSeqDBException if the database file is not found or appears invalid
    CTaxonomy4BlastSQLite(const string& dbname = kEmptyStr);

    /// Destructor
    virtual ~CTaxonomy4BlastSQLite();

    /// @inheritDoc
    virtual void GetLeafNodeTaxids(const int taxid, vector<int>& descendants);

    CTaxonomy4BlastSQLite(const CTaxonomy4BlastSQLite&) = delete;
    CTaxonomy4BlastSQLite& operator=(const CTaxonomy4BlastSQLite&) = delete;

private:
    /// Simple sanity check to validate the integrity of the database
    void x_SanityCheck();

    /// SQLite3 database file name
    string m_DbName;

    /// Most recent SQLite statement
    unique_ptr<CSQLITE_Statement> m_Statement;

    /// Connection to SQLite engine
    unique_ptr<CSQLITE_Connection> m_DbConn;
};

END_NCBI_SCOPE

#endif // OBJTOOLS_BLAST_SEQDB_READER___TAX4BLAST_SQLITE__HPP

