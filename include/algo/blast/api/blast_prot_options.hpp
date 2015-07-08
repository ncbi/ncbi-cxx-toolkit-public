#ifndef ALGO_BLAST_API___BLAST_PROT_OPTIONS__HPP
#define ALGO_BLAST_API___BLAST_PROT_OPTIONS__HPP

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

/// @file blast_prot_options.hpp
/// Declares the CBlastProteinOptionsHandle class.


#include <algo/blast/api/blast_options_handle.hpp>

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

class NCBI_XBLAST_EXPORT CBlastProteinOptionsHandle : 
                                            public CBlastOptionsHandle
{
public:
    
    /// Creates object with default options set
    CBlastProteinOptionsHandle(EAPILocality locality = CBlastOptions::eLocal);

    /// Create Options Handle from Existing CBlastOptions Object
    CBlastProteinOptionsHandle(CRef<CBlastOptions> opt):CBlastOptionsHandle(opt) {}

    /******************* Lookup table options ***********************/
    /// Returns WordThreshold
    double GetWordThreshold() const { return m_Opts->GetWordThreshold(); }
    /// Sets WordThreshold
    /// @param wt WordThreshold [in]
    void SetWordThreshold(double wt) { m_Opts->SetWordThreshold(wt); }

    /// Returns WordSize
    int GetWordSize() const { return m_Opts->GetWordSize(); }
    /// Sets WordSize
    /// @param ws WordSize [in]
    void SetWordSize(int ws) { m_Opts->SetWordSize(ws); }

    /******************* Initial word options ***********************/

    /// Returns XDropoff
    double GetXDropoff() const { return m_Opts->GetXDropoff(); } 
    /// Sets XDropoff
    /// @param x XDropoff [in]
    void SetXDropoff(double x) { m_Opts->SetXDropoff(x); }

    /******************* Query setup options ************************/
    /// Is SEG filtering enabled?
    bool GetSegFiltering() const { return m_Opts->GetSegFiltering(); }
    /// Enable SEG filtering.
    /// @param val enable SEG filtering [in]
    void SetSegFiltering(bool val) { m_Opts->SetSegFiltering(val); }

    /// Get window parameter for seg
    int GetSegFilteringWindow() const { return m_Opts->GetSegFilteringWindow(); }
    /// Set window parameter for seg.  Acceptable value are > 0.  
    /// @param window seg filtering parameter window [in]
    void SetSegFilteringWindow(int window) { m_Opts->SetSegFilteringWindow(window); }

    /// Get locut parameter for seg
    double GetSegFilteringLocut() const { return m_Opts->GetSegFilteringLocut(); }
    /// Set locut parameter for seg.  Acceptable values are greater than 0.
    /// @param locut seg filtering parameter locut [in]
    void SetSegFilteringLocut(double locut) { m_Opts->SetSegFilteringLocut(locut); }

    /// Get hicut parameter for seg
    double GetSegFilteringHicut() const { return m_Opts->GetSegFilteringHicut(); }
    /// Set hicut parameter for seg.  Acceptable values are greater than Locut 
    ///  (see SetSegFilteringLocut).
    /// @param hicut seg filtering parameter hicut [in]
    void SetSegFilteringHicut(double hicut) { m_Opts->SetSegFilteringHicut(hicut); }

    /************************ Scoring options ************************/
    /// Returns MatrixName
    const char* GetMatrixName() const { return m_Opts->GetMatrixName(); }
    /// Sets MatrixName
    /// @param matrix MatrixName [in]
    void SetMatrixName(const char* matrix) { m_Opts->SetMatrixName(matrix); }

    /// Returns GapOpeningCost
    int GetGapOpeningCost() const { return m_Opts->GetGapOpeningCost(); }
    /// Sets GapOpeningCost
    /// @param g GapOpeningCost [in]
    void SetGapOpeningCost(int g) { m_Opts->SetGapOpeningCost(g); }

    /// Returns GapExtensionCost
    int GetGapExtensionCost() const { return m_Opts->GetGapExtensionCost(); }
    /// Sets GapExtensionCost
    /// @param e GapExtensionCost [in]
    void SetGapExtensionCost(int e) { m_Opts->SetGapExtensionCost(e); }
    
protected:
    /// Set the program and service name for remote blast.
    virtual void SetRemoteProgramAndService_Blast3()
    {
        m_Opts->SetRemoteProgramAndService_Blast3("blastp", "plain");
    }
    
    /// Overrides LookupTableDefaults for protein options
    virtual void SetLookupTableDefaults();
    /// Overrides QueryOptionDefaults for protein options
    virtual void SetQueryOptionDefaults();
    /// Overrides InitialWordOptionsDefaults for protein options
    virtual void SetInitialWordOptionsDefaults();
    /// Overrides GappedExtensionDefaults for protein options
    virtual void SetGappedExtensionDefaults();
    /// Overrides ScoringOptionsDefaults for protein options
    virtual void SetScoringOptionsDefaults();
    /// Overrides HitSavingOptionsDefaults for protein options
    virtual void SetHitSavingOptionsDefaults();
    /// Overrides EffectiveLengthsOptionsDefaults for protein options
    virtual void SetEffectiveLengthsOptionsDefaults();
    /// Overrides SubjectSequenceOptionsDefaults for protein options
    virtual void SetSubjectSequenceOptionsDefaults(); 

private:
    /// Disallow copy constructor
    CBlastProteinOptionsHandle(const CBlastProteinOptionsHandle& rhs);
    /// Disallow assignment operator
    CBlastProteinOptionsHandle& operator=(const CBlastProteinOptionsHandle& rhs);
};

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */

#endif  /* ALGO_BLAST_API___BLAST_PROT_OPTIONS__HPP */
