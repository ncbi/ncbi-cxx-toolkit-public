#ifndef ALGO_BLAST_API___BLASTX_OPTIONS__HPP
#define ALGO_BLAST_API___BLASTX_OPTIONS__HPP

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

/// @file blastx_options.hpp
/// Declares the CBlastxOptionsHandle class.


#include <algo/blast/api/blast_prot_options.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Handle to the translated nucleotide-protein options to the BLAST algorithm.
///
/// Adapter class for translated nucleotide-protein BLAST comparisons.
/// Exposes an interface to allow manipulation the options that are relevant to
/// this type of search.

class NCBI_XBLAST_EXPORT CBlastxOptionsHandle : 
                                            public CBlastProteinOptionsHandle
{
public:

    /// Creates object with default options set
    CBlastxOptionsHandle(EAPILocality locality = CBlastOptions::eLocal);

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

    /// Returns QueryGeneticCode
    int GetQueryGeneticCode() const { return m_Opts->GetQueryGeneticCode(); }
    /// Sets QueryGeneticCode
    /// @param gc QueryGeneticCode [in]
    void SetQueryGeneticCode(int gc) { m_Opts->SetQueryGeneticCode(gc); }

    /************************ Scoring options ************************/
    /// Returns OutOfFrameMode
    /// @todo is this needed or can we use a sentinel for the frame shift penalty?
    bool GetOutOfFrameMode() const { return m_Opts->GetOutOfFrameMode(); }
    /// Sets OutOfFrameMode
    /// @param m OutOfFrameMode [in]
    void SetOutOfFrameMode(bool m = true) { m_Opts->SetOutOfFrameMode(m); }

    /// Returns FrameShiftPenalty
    int GetFrameShiftPenalty() const { return m_Opts->GetFrameShiftPenalty(); }
    /// Sets FrameShiftPenalty
    /// @param p FrameShiftPenalty [in]
    void SetFrameShiftPenalty(int p) { m_Opts->SetFrameShiftPenalty(p); }

    /// Returns LongestIntronLength
    int GetLongestIntronLength() const { return m_Opts->GetLongestIntronLength(); }
    /// Sets LongestIntronLength
    /// @param l LongestIntronLength [in]
    void SetLongestIntronLength(int l) { m_Opts->SetLongestIntronLength(l); }


/******************* Gapped extension options *******************/

    /// Returns this mode, which mostly specifies whether composition of db
    /// sequence is taken into account when calculating expect values.
    ECompoAdjustModes GetCompositionBasedStats() const {
        return m_Opts->GetCompositionBasedStats();
    }

    /// Sets this mode, which mostly specifies whether composition of db
    /// sequence is taken into account when calculating expect values.
    /// @param mode composition-based statistics mode [in]
    void SetCompositionBasedStats(ECompoAdjustModes mode)  {
        m_Opts->SetCompositionBasedStats(mode);
    }

    /// Returns this mode, specifying that smith waterman rather than the normal blast heuristic
    /// should be used for final extensions.
    /// into account when calculating expect values.
    bool GetSmithWatermanMode() const { return m_Opts->GetSmithWatermanMode(); }

    /// Sets this mode, specifying that smith waterman rather than the normal blast heuristic
    /// should be used for final extensions.
    /// @param m use smith-waterman if true [in]
    void SetSmithWatermanMode(bool m = false)  { m_Opts->SetSmithWatermanMode(m); }

protected:
    /// Set the program and service name for remote blast.
    virtual void SetRemoteProgramAndService_Blast3()
    {
        m_Opts->SetRemoteProgramAndService_Blast3("blastx", "plain");
    }
    
    /// Overrides LookupTableDefaults for blastx options
    void SetLookupTableDefaults();
    /// Overrides QueryOptionDefaults for blastx options
    void SetQueryOptionDefaults();
    /// Overrides ScoringOptionsDefaults for blastx options
    void SetScoringOptionsDefaults();
    /// Overrides HitSavingOptionsDefaults for blastx options
    void SetHitSavingOptionsDefaults();
    /// Overrides SetGappedExtensionDefaults for blastx option
    void SetGappedExtensionDefaults();

private:
    /// Disallow copy constructor
    CBlastxOptionsHandle(const CBlastxOptionsHandle& rhs);
    /// Disallow assignment operator
    CBlastxOptionsHandle& operator=(const CBlastxOptionsHandle& rhs);
};

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */

#endif  /* ALGO_BLAST_API___BLASTX_OPTIONS__HPP */
