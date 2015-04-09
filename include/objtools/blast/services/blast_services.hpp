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

/// @file blast_services.hpp
/// Declares the CBlastServices class.

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/blast/blast__.hpp>
#include <objects/blast/names.hpp>
#include <objects/scoremat/PssmWithParameters.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    /// forward declaration of ASN.1 object containing PSSM (scoremat.asn)
    class CBioseq_set;
    class CSeq_loc;
    class CSeq_id;
    class CSeq_align_set;
END_SCOPE(objects)

using namespace ncbi::objects;


/// RemoteServicesException
///

class NCBI_XOBJREAD_EXPORT CBlastServicesException : public CException {
public:
    /// Errors are classified into one of two types.
    enum EErrCode {
        /// Argument validation failed.
        eArgErr,

        /// Files were missing or contents were incorrect.
        eFileErr,

        /// Request failed
        eRequestErr,

        /// Memory allocation failed.
        eMemErr
    };

    /// Get a message describing the situation leading to the throw.
    virtual const char* GetErrCodeString() const
    {
        switch ( GetErrCode() ) {
        case eArgErr:  return "eArgErr";
        case eFileErr: return "eFileErr";
        case eRequestErr: return "eRequestErr";
        default:       return CException::GetErrCodeString();
        }
    }

    /// Include standard NCBI exception behavior.
    NCBI_EXCEPTION_DEFAULT(CBlastServicesException, CException);
};



/// API for Remote Blast Services
///
/// Class to obtain information and data from the Remote BLAST service that is
/// not associated with a specific BLAST search

class NCBI_XOBJREAD_EXPORT CBlastServices : public CObject
{
public:
    /// Default constructor
    CBlastServices() { m_Verbose = false; }

    /// Analogous to CRemoteBlast::SetVerbose
    void SetVerbose(bool value = true) { m_Verbose = value; }

    /// Returns true if the BLAST database specified exists in the NCBI servers
    /// @param dbname BLAST database name [in]
    /// @param is_protein is this a protein database? [in]
    bool IsValidBlastDb(const string& dbname, bool is_protein);
    
    /// Retrieve detailed information for one BLAST database
    /// If information about multiple databases is needed, use
    /// the other GetDatabaseInfo method.
    ///
    /// @param blastdb object describing the database for which to get
    /// detailed information
    /// @return Detailed information for the requested BLAST database or an
    /// empty object is the requested database wasn't found
    CRef<objects::CBlast4_database_info>
    GetDatabaseInfo(CRef<objects::CBlast4_database> blastdb);

    /// Retrieve detailed information for databases listed
    /// in the string.  If more than one database is supplied, it
    /// they should be separated by spaces (e.g., "nt wgs est").  
    ///
    /// @param dbname string listing the database(s)
    /// @param is_protein is a protein for true, otherwise dna
    /// @param found_all true if all databases were found.
    /// @return Detailed information for the requested BLAST databases or an
    /// empty vector if no databases were found.
    vector< CRef<objects::CBlast4_database_info> >
    GetDatabaseInfo(const string& dbname, bool is_protein, bool *found_all);
    /// Same as GetDatabaseInfo but  retrieving whole list of database
    vector< CRef<objects::CBlast4_database_info> >
    GetDatabaseInfoLegacy(const string& dbname, bool is_protein, bool *found_all);
    /// Retrieve organism specific repeats databases
    vector< CRef<objects::CBlast4_database_info> >
    GetOrganismSpecificRepeatsDatabases();

    /// Retrieve a list of NCBI taxonomy IDs for which there exists
    /// windowmasker masking data to support an alternative organism specific
    /// filtering
    objects::CBlast4_get_windowmasked_taxids_reply::Tdata
    GetTaxIdWithWindowMaskerSupport();

    /// Defines a std::vector of CRef<CSeq_id>
    typedef vector< CRef<objects::CSeq_id> > TSeqIdVector;
    /// Defines a std::vector of CRef<CBioseq>
    typedef vector< CRef<objects::CBioseq> > TBioseqVector;

   /// Get a set of Bioseqs without their sequence data given an input set of
    /// Seq-ids.
    ///
    /// @param seqids   A vector of Seq-ids for which Bioseqs are requested.
    /// @param database A list of databases from which to get the sequences.
    /// @param seqtype  The residue type, 'p' from protein, 'n' for nucleotide.
    /// @param bioseqs  The vector used to return the requested Bioseqs.
    /// @param errors   A null-separated list of errors.
    /// @param warnings A null-separated list of warnings.
    /// @param verbose  Produce verbose output. [in]
    /// @param target_only Filter the defline to include only the requested id. [in]
    /// @todo FIXME: Add retry logic in case of transient errors
    static void
    GetSequencesInfo(TSeqIdVector& seqids,      // in
                     const string& database,    // in
                     char seqtype,              // 'p' or 'n'
                     TBioseqVector& bioseqs,    // out
                     string& errors,            // out
                     string& warnings,          // out
                     bool verbose = false,      // in
                     bool target_only = false); // in

    /// Get a set of Bioseqs given an input set of Seq-ids. 
    ///
    /// This retrieves the Bioseqs corresponding to the given Seq-ids
    /// from the blast4 server.  Normally this will be much faster
    /// than consulting ID1 seperately for each sequence.  Sometimes
    /// there are multiple sequences for a given Seq-id.  In such
    /// cases, there are always 'non-ambiguous' ids available.  This
    /// interface does not currently address this issue, and will
    /// simply return the Bioseqs corresponding to one of the
    /// sequences.  Errors will be returned if the operation cannot be
    /// completed (or started).  In the case of a sequence that cannot
    /// be found, the error will indicate the index of (and Seq-id of)
    /// the missing sequence; processing will continue, and the
    /// sequences that can be found will be returned along with the
    /// error.
    ///
    /// @param seqids   A vector of Seq-ids for which Bioseqs are requested.
    /// @param database A list of databases from which to get the sequences.
    /// @param seqtype  The residue type, 'p' from protein, 'n' for nucleotide.
    /// @param bioseqs  The vector used to return the requested Bioseqs.
    /// @param errors   A null-separated list of errors.
    /// @param warnings A null-separated list of warnings.
    /// @param verbose  Produce verbose output. [in]
    /// @param target_only Filter the defline to include only the requested id. [in]
    /// @todo FIXME: Add retry logic in case of transient errors
    static void
    GetSequences(TSeqIdVector& seqids,      // in
                 const string& database,    // in
                 char seqtype,              // 'p' or 'n'
                 TBioseqVector& bioseqs,    // out
                 string& errors,            // out
                 string& warnings,          // out
                 bool verbose = false,      // in
                 bool target_only = false); // in
    /// Defines a std::vector of CRef<CSeq_interval>
    typedef vector< CRef<objects::CSeq_interval> > TSeqIntervalVector;
    /// Defines a std::vector of CRef<CSeq_data>
    typedef vector< CRef<objects::CSeq_data> > TSeqDataVector;

    /// This retrieves (partial) sequence data from the remote BLAST server.
    ///
    /// @param seqid
    ///     A vector of Seq-ids for which sequence data are requested. [in]
    /// @param database
    ///     A list of databases from which to get the sequences. [in]
    /// @param seqtype
    ///     The residue type, 'p' from protein, 'n' for nucleotide. [in]
    /// @param ids
    ///     The sequence IDs for those sequences which the seq data was
    //      obtained successfully [out]
    /// @param seq_data
    ///     Sequence data in CSeq_data format. [out]
    /// @param errors
    ///     An error message (if any). [out]
    /// @param warnings
    ///     A warning (if any). [out]
    /// @param verbose
    ///     Produce verbose output. [in]
    /// @todo FIXME: Add retry logic in case of transient errors
    static void
    GetSequenceParts(const TSeqIntervalVector   & seqids,    // in
                     const string               & database,  // in
                     char                         seqtype,   // 'p' or 'n'
                     TSeqIdVector               & ids,       // out
                     TSeqDataVector             & seq_data,  // out
                     string                     & errors,    // out
                     string                     & warnings,  // out
                     bool                         verbose = false);// in

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
    CBlastServices(const CBlastServices &);
    
    /// Prohibit assignment.
    CBlastServices & operator=(const CBlastServices &);
    
    
    // Data
    
    /// BLAST databases available to search
    objects::CBlast4_get_databases_reply::Tdata m_AvailableDatabases;
    /// Taxonomy IDs for which there's windowmasker masking data at NCBI
    objects::CBlast4_get_windowmasked_taxids_reply::Tdata m_WindowMaskedTaxIds;
    /// Display verbose output to stdout?
    bool m_Verbose;
};

END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API___REMOTE_SERVICES__HPP */
