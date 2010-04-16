#ifndef ALGO_BLAST_API___SEARCH_STRATEGY__HPP
#define ALGO_BLAST_API___SEARCH_STRATEGY__HPP

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
 * Authors:  Tom Madden
 *
 */

/// @file search_strategy.hpp
/// Declares the CImportStrategy and CExportStrategy

#include <algo/blast/api/remote_blast.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/blast_options_builder.hpp>
#include <objects/blast/blast__.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    /// forward declaration of ASN.1 object containing PSSM (scoremat.asn)
    class CPssmWithParameters;
    class CBioseq_set;
    class CSeq_loc;
    class CSeq_id;
    class CSeq_align_set;
END_SCOPE(objects)

BEGIN_SCOPE(blast)

/// This is the "mutable" data for CImportStrategy.
struct CImportStrategyData {

    /// Has the struct been properly filled in?
    bool valid;

    /// BLAST options.
    CRef<blast::CBlastOptionsHandle> m_OptionsHandle;

    /// Filtering ID
    int m_FilteringID;

    /// Range of query.
    TSeqRange m_QueryRange;

    /// Task, such as megablast, blastn, blastp, etc.
    string m_Task;
};


/// Class to return parts of the CBlast4_request, or data associated with
/// a CBlast4_request, such as options.
class NCBI_XBLAST_EXPORT CImportStrategy : public CObject
{
public:
    /// Constructor, imports the CBlast4_request
    CImportStrategy(CRef<objects::CBlast4_request> request);

    /// Destructor
    ~CImportStrategy();

    /// Builds and returns the OptionsHandle
    CRef<blast::CBlastOptionsHandle> GetOptionsHandle() const;

    /// Fetches task, such as "megablast", "blastn", etc. 
    string GetTask() const;

    /// Fetches service, such as psiblast, plain, megablast
    string GetService() const;

    /// Fetches program, one of blastn, blastp, blastx, tblastn, tblastx
    string GetProgram() const;

    /// Returns ident field from a Blast4-request
    string GetCreatedBy() const;

    /// The start and stop on the query (if applicable)
    TSeqRange GetQueryRange() const;

    /// The DB filter ID.
    int GetDBFilteringID() const;

    /// The queries either as Bioseq, seqloc, or pssm.
    CRef<objects::CBlast4_queries> GetQueries();

    /// Returns the target sequences.  This is then a choice of a
    /// database (for searches over a blast database) or as a 
    /// list of Bioseqs (for bl2seq type searches).
    CRef<objects::CBlast4_subject> GetSubject();

    /// Options specific to blast searches (e.g, threshold, expect value).
    objects::CBlast4_parameters& GetAlgoOptions();

    /// Options for controlling program execution and database filtering.
    objects::CBlast4_parameters& GetProgramOptions();

private:

    void FetchData() const; /// Fills in CImportStrategyData;

    CImportStrategyData* m_Data;

    CRef<objects::CBlast4_request> m_Request;

    string m_Service;
};


END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_BLAST_API___SEARCH_STRATEGY__HPP */
