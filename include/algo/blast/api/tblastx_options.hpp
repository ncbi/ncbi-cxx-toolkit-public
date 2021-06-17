#ifndef ALGO_BLAST_API___TBLASTX_OPTIONS__HPP
#define ALGO_BLAST_API___TBLASTX_OPTIONS__HPP

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
 * Authors:  Christiam Camacho
 *
 */

/// @file tblastx_options.hpp
/// Declares the CTBlastxOptionsHandle class.


#include <algo/blast/api/blast_prot_options.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Handle to the translated nucleotide-translated nucleotide options to 
/// the BLAST algorithm.
///
/// Adapter class for translated nucleotide-translated nucleotide BLAST 
/// comparisons.
/// Exposes an interface to allow manipulation the options that are relevant to
/// this type of search.

class NCBI_XBLAST_EXPORT CTBlastxOptionsHandle : 
                                            public CBlastProteinOptionsHandle
{
public:

    /// Creates object with default options set
    CTBlastxOptionsHandle(EAPILocality locality);
    ~CTBlastxOptionsHandle() {}

    /******************* Query setup options ************************/
    /// Returns StrandOption
    objects::ENa_strand GetStrandOption() const { 
        return m_Opts->GetStrandOption();
    }
    /// Sets StrandOption
    /// @param strand StrandOption [in]
    void SetStrandOption(objects::ENa_strand strand) {
        m_Opts->SetStrandOption(strand);
    }

    /******************* Subject sequence options *******************/
    /// Returns DbGeneticCode
    int GetDbGeneticCode() const {
        return m_Opts->GetDbGeneticCode();
    }
    /// Sets DbGeneticCode
    /// @param gc DbGeneticCode [in]
    void SetDbGeneticCode(int gc) {
        m_Opts->SetDbGeneticCode(gc);
    }

    /// Returns QueryGeneticCode
    int GetQueryGeneticCode() const {
        return m_Opts->GetQueryGeneticCode();
    }
    /// Sets QueryGeneticCode
    /// @param gc QueryGeneticCode [in]
    void SetQueryGeneticCode(int gc) {
        m_Opts->SetQueryGeneticCode(gc);
    }
    
protected:
    /// Set the program and service name for remote blast.
    virtual void SetRemoteProgramAndService_Blast3()
    {
        m_Opts->SetRemoteProgramAndService_Blast3("tblastx", "plain");
    }
    
    /// Overrides LookupTableDefaults for tblastx options
    void SetLookupTableDefaults();
    /// Overrides QueryOptionDefaults for tblastx options
    void SetQueryOptionDefaults();
    /// Overrides GappedExtensionDefaults for tblastx options
    void SetGappedExtensionDefaults();
    /// Overrides ScoringOptionsDefaults for tblastx options
    void SetScoringOptionsDefaults();
    /// Overrides HitSavingOptionsDefaults for tblastx options
    void SetHitSavingOptionsDefaults();
    /// Overrides SubjectSequenceOptionsDefaults for tblastx options
    void SetSubjectSequenceOptionsDefaults();

private:
    /// Disallow copy constructor
    CTBlastxOptionsHandle(const CTBlastxOptionsHandle& rhs);
    /// Disallow assignment operator
    CTBlastxOptionsHandle& operator=(const CTBlastxOptionsHandle& rhs);
};

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */


#endif  /* ALGO_BLAST_API___TBLASTX_OPTIONS__HPP */
