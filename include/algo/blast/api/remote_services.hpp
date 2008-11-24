#ifndef ALGO_BLAST_API___REMOTE_SERVICES__HPP
#define ALGO_BLAST_API___REMOTE_SERVICES__HPP

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
 * Authors:  Christiam Camacho, Kevin Bealer
 *
 */

/// @file remote_services.hpp
/// Declares the CRemoteServices class.

#include <algo/blast/api/remote_blast.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// API for Remote Blast Services
///
/// Class to obtain information and data from the Remote BLAST service that is
/// not associated with a specific BLAST search

class NCBI_XBLAST_EXPORT CRemoteServices : public CObject
{
public:
    /// Default constructor
    CRemoteServices() {}

    /// Returns true if the BLAST database specified exists in the NCBI servers
    /// @param dbname BLAST database name [in]
    /// @param is_protein is this a protein database? [in]
    bool IsValidBlastDb(const string& dbname, bool is_protein);
    
    /// Retrieve detailed information for a given BLAST database
    ///
    /// @param blastdb object describing the database for which to get
    /// detailed information
    /// @return Detailed information for the requested BLAST database or an
    /// empty object is the requested database wasn't found
    CRef<objects::CBlast4_database_info>
    GetDatabaseInfo(CRef<objects::CBlast4_database> blastdb);

    /// Retrieve organism specific repeats databases
    vector< CRef<objects::CBlast4_database_info> >
    GetOrganismSpecificRepeatsDatabases();
    
private:

    /// Retrieve the BLAST databases available for searching
    void x_GetAvailableDatabases();

    /// Look for a database matching this method's argument and returned
    /// detailed information about it.
    /// @param blastdb database description
    /// @return detailed information about the database requested or an empty
    /// CRef<> if the database was not found
    CRef<objects::CBlast4_database_info>
    x_FindDbInfoFromAvailableDatabases(CRef<objects::CBlast4_database> blastdb);
    
    /// Prohibit copy construction.
    CRemoteServices(const CRemoteServices &);
    
    /// Prohibit assignment.
    CRemoteServices & operator=(const CRemoteServices &);
    
    
    // Data
    
    /// BLAST databases available to search
    objects::CBlast4_get_databases_reply::Tdata m_AvailableDatabases;
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API___REMOTE_SERVICES__HPP */
