#ifndef ALGO_BLAST_API___PSIBLAST_OPTIONS__HPP
#define ALGO_BLAST_API___PSIBLAST_OPTIONS__HPP

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
 * Authors:  Kevin Bealer
 *
 */

/// @file psiblast_options.hpp
/// Declares the CPSIBlastOptionsHandle class.


#include <algo/blast/api/blast_prot_options.hpp>

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

class NCBI_XBLAST_EXPORT CPSIBlastOptionsHandle : public CBlastProteinOptionsHandle
{
public:
    
    /// Creates object with default options set
    CPSIBlastOptionsHandle(EAPILocality locality = CBlastOptions::eLocal);
    ~CPSIBlastOptionsHandle() {}
    
    /******************* PSI options ***********************/
    /// Returns InclusionThreshold
    double GetInclusionThreshold() const { return m_Opts->GetInclusionThreshold(); }
    /// Sets InclusionThreshold
    /// @param incthr InclusionThreshold [in]
    void SetInclusionThreshold(double incthr) { m_Opts->SetInclusionThreshold(incthr); }
    
    /// Returns PseudoCount
    int GetPseudoCount() const { return m_Opts->GetPseudoCount(); }
    /// Sets PseudoCount
    /// @param p PseudoCount [in]
    void SetPseudoCount(int p) { m_Opts->SetPseudoCount(p); }
    
protected:
    /// Set the program and service name for remote blast.
    virtual void SetRemoteProgramAndService_Blast3()
    {
        m_Opts->SetRemoteProgramAndService_Blast3("blastp", "psi");
    }
    
    /// Overrides LookupTableDefaults for PSI-BLAST
    void SetLookupTableDefaults();

    /// Sets PSIBlastDefaults
    void SetPSIBlastDefaults();
    
private:
    /// Disallow copy constructor
    CPSIBlastOptionsHandle(const CPSIBlastOptionsHandle& rhs);
    /// Disallow assignment operator
    CPSIBlastOptionsHandle& operator=(const CPSIBlastOptionsHandle& rhs);
};

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2005/05/09 20:08:48  bealer
 * - Add program and service strings to CBlastOptions for remote blast.
 * - New CBlastOptionsHandle constructor for CRemoteBlast.
 * - Prohibit copy construction/assignment for CRemoteBlast.
 * - Code in each BlastOptionsHandle derived class to set program+service.
 *
 * Revision 1.5  2005/03/10 13:17:27  madden
 * Changed type from short to int for [GS]etPseudoCount
 *
 * Revision 1.4  2004/12/20 20:10:55  camacho
 * + option to set composition based statistics
 * + option to use pssm in lookup table
 *
 * Revision 1.3  2004/06/08 23:11:58  camacho
 * fix to previous commit
 *
 * Revision 1.2  2004/06/08 22:41:04  camacho
 * Add missing doxygen comments
 *
 * Revision 1.1  2004/05/17 18:54:09  bealer
 * - Add PSI-Blast support.
 *
 *
 * ===========================================================================
 */

#endif  /* ALGO_BLAST_API___PSIBLAST_OPTIONS__HPP */
