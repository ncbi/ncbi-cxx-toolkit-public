#ifndef ALGO_BLAST_API___DELTABLAST_OPTIONS__HPP
#define ALGO_BLAST_API___DELTABLAST_OPTIONS__HPP

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
 * Authors:  Greg Boratyn
 *
 */

/// @file deltablast_options.hpp
/// Declares the CDeltaBlastOptionsHandle class.


#include <algo/blast/api/psiblast_options.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Handle to the protein-protein options to the BLAST algorithm.
///
/// Adapter class for protein-protein BLAST comparisons.
/// Exposes an interface to allow manipulation the options that are relevant to
/// this type of search.
class NCBI_XBLAST_EXPORT CDeltaBlastOptionsHandle : public CPSIBlastOptionsHandle
{
public:
    
    /// Creates object with default options set
    CDeltaBlastOptionsHandle(EAPILocality locality = CBlastOptions::eLocal);
    /// Destructor
    ~CDeltaBlastOptionsHandle() {}
    
    /******************* DELTA BLAST options ***********************/

    /// Get e-value threshold for including domains in Pssm calculation
    /// @return E-value cutoff for domains
    double GetDomainInclusionThreshold(void) const
    { return m_Opts->GetDomainInclusionThreshold(); }

    /// Set e-value threshold for including domains in Pssm calculation
    /// @param th E-value cutoff for domains [in]
    void SetDomainInclusionThreshold(double th)
    { m_Opts->SetDomainInclusionThreshold(th); }

    /// Get e-value threshold for including sequences in Pssm calculation
    /// @return E-value cutoff for sequences
    ///
    /// Same as GetInclusionThreshold().It was added for clear distinction
    /// between Psi and Delta Blast inclusion thresholds
    double GetPSIInclusionThreshold(void) const
    {return GetInclusionThreshold(); }

    /// Set e-value threshold for including sequences in Pssm calculation
    /// @param th E-value cutoff for sequences [in]
    ///
    /// Same as SetInclusionThreshold().It was added for clear distinction
    /// between Psi and Delta Blast inclusion thresholds
    void SetPSIInclusionThreshold(double th)
    { SetInclusionThreshold(th); }
    
protected:

    /// Set the program and service name for remote blast
    virtual void SetRemoteProgramAndService_Blast3()
    {
        m_Opts->SetRemoteProgramAndService_Blast3("blastp", "delta_blast");
    }

    /// Override the parent class' default for filtering query sequence (i.e.:
    /// no filtering applied to the query by default)
    virtual void SetQueryOptionDefaults();

    /// Override the parent class' defaults for gapped extension (i.e.:
    /// composition based statistics 1)
    virtual void SetGappedExtensionDefaults();
    
    /// Sets Delta Blast defaults
    void SetDeltaBlastDefaults();
    
private:
    /// Disallow copy constructor
    CDeltaBlastOptionsHandle(const CDeltaBlastOptionsHandle& rhs);
    /// Disallow assignment operator
    CDeltaBlastOptionsHandle& operator=(const CDeltaBlastOptionsHandle& rhs);
};

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */

#endif  /* ALGO_BLAST_API___DELTABLAST_OPTIONS__HPP */
