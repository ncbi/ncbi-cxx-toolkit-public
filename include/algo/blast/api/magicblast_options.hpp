#ifndef ALGO_BLAST_API___MAGIC_BLAST_OPTIONS__HPP
#define ALGO_BLAST_API___MAGIC_BLAST_OPTIONS__HPP

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

/// @file blast_mapper_options.hpp
/// Declares the CMagicBlastOptionsHandle class.

#include <algo/blast/api/blast_options_handle.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Handle to the nucleotide mapping options to the BLAST algorithm.
///
/// Adapter class for nucleotide-nucleotide BLAST comparisons.
/// Exposes an interface to allow manipulation the options that are relevant to
/// this type of search.
/// 
///

class NCBI_XBLAST_EXPORT CMagicBlastOptionsHandle : 
                                         public CBlastOptionsHandle
{
public:

    /// Creates object with default options set
    CMagicBlastOptionsHandle(EAPILocality locality = CBlastOptions::eLocal);

    /// Create Options Handle from Existing CBlastOptions Object
    CMagicBlastOptionsHandle(CRef<CBlastOptions> opt);

    /// Sets Defaults
    virtual void SetDefaults();

    virtual void SetRNAToGenomeDefaults();
    virtual void SetRNAToRNADefaults();
    virtual void SetGenomeToGenomeDefaults();

    /******************* Lookup table options ***********************/

    /// Return db filtering option for the lookup table
    ///
    /// The filtering removes from lookup table words that are frequent in the
    /// database.
    /// @return If true, words that are frequent in the dabase will be removed
    bool GetLookupDbFilter() const { return m_Opts->GetLookupDbFilter(); }

    /// Set db filtering option for the lookup table
    ///
    /// The filtering removes from lookup table words that are frequent in the
    /// database.
    /// @param b If true, words that are frequent in the dabase will be removed
    /// from the lookup table
    void SetLookupDbFilter(bool b) { m_Opts->SetLookupDbFilter(b); }

    /// Get maximum word count for lookup table word masking by database
    /// frequency
    Uint1 GetMaxDbWordCount(void) const { return m_Opts->GetMaxDbWordCount(); }

    /// Set maximum word count for lookup table word masking by database
    /// frequency
    void SetMaxDbWordCount(Uint1 num) { m_Opts->SetMaxDbWordCount(num); }

    /// Return number of words skipped betweem collected ones when creating
    /// a lookup table
    int GetLookupTableStride() const { return m_Opts->GetLookupTableStride(); }

    /// Set lookup table stride: number of words skipped between collected ones
    /// when creating a lookup table
    /// @param s lookup table stride [in]
    void SetLookupTableStride(int s) { m_Opts->SetLookupTableStride(s); }


    /******************* Query setup options ************************/

    /// Return true if query quality filtering is on
    bool GetReadQualityFiltering() const
    { return m_Opts->GetReadQualityFiltering(); }

    /// Turn on/off query quaility filtering with default parameters
    void SetReadQualityFiltering(bool val = true)
    { m_Opts->SetReadQualityFiltering(val); }

    /// Get maximum fraction of ambiguous bases for query quality filtering
    double GetMaxFractionAmbiguous() const
    { return m_Opts->GetReadMaxFractionAmbiguous(); }

    /// Get maximum fraction of ambiguous bases for query quality filtering
    void SetMaxFractionAmbiguous(double val)
    { m_Opts->SetReadMaxFractionAmbiguous(val); }

    /// Get mininum dimer entropy for query quality filtering
    int GetMinDimerEntropy() const { return m_Opts->GetReadMinDimerEntropy(); }

    /// Set minimum dimer entropy for query quality filtering
    void SetMinDimerEntropy(int val) { m_Opts->SetReadMinDimerEntropy(val); }

    
    /******************* Initial word options ***********************/

    /// Return word size
    int GetWordSize() const { return m_Opts->GetWordSize(); }

    /// Set word size
    /// @param w word size [in]
    void SetWordSize(int w) { m_Opts->SetWordSize(w); }

    /******************* Gapped extension options *******************/

    /// Return x-dropoff parameter for gapped alignment
    int GetGapXDropoff() const { return m_Opts->GetGapXDropoff(); }

    /// Set x-dropoff parameter for gapped alignment
    /// @param d x-dropoff value [in]
    void SetGapXDropoff(int d) { m_Opts->SetGapXDropoff(d); }

    /************************ Scoring options ************************/

    /// Returns mismatch penalty
    int GetMismatchPenalty() const { return m_Opts->GetMismatchPenalty(); }

    /// Sets mismatch penalty
    /// @param p mismatch penalty [in]
    void SetMismatchPenalty(int p) { m_Opts->SetMismatchPenalty(p); }

    /// Return gap opening cost
    int GetGapOpeningCost() const { return m_Opts->GetGapOpeningCost(); }

    /// Set gap opening cost
    /// @param g gap opening cost [in]
    void SetGapOpeningCost(int g) { m_Opts->SetGapOpeningCost(g); }

    /// Return gap extension cost
    int GetGapExtensionCost() const { return m_Opts->GetGapExtensionCost(); }

    /// Sets gap extension cost
    /// @param g gap extension cost [in]
    void SetGapExtensionCost(int g) { m_Opts->SetGapExtensionCost(g); }

    /// Return a single alignment cutoff score
    int GetCutoffScore() const { return m_Opts->GetCutoffScore(); }

    /// Set a single alignment cutoff score
    /// @param s cutoff score [in]
    void SetCutoffScore(int s) { m_Opts->SetCutoffScore(s); }

    /// Get coefficients for alignment cutoff score as a linear function of
    /// query length: x[0] + x[1] * length
    vector<double> GetCutoffScoreCoeffs()
    { return m_Opts->GetCutoffScoreCoeffs(); }

    /// Set coefficients for alignment cutoff score as a linear function of
    /// query length: x[0] + x[1] * length
    /// @param c function coefficients [in]
    void SetCutoffScoreCoeffs(const vector<double>& c)
    { m_Opts->SetCutoffScoreCoeffs(c); }

    int GetMaxEditDistance() const { return m_Opts->GetMaxEditDistance(); }
    void SetMaxEditDistance(int e) { m_Opts->SetMaxEditDistance(e); }


    /************************ Mapping options ************************/

    /// Are the mapping reads assumed paired
    /// @returns True if reads are assumed paired, false otherwise
    bool GetPaired() const { return m_Opts->GetPaired(); }

    /// Set input reads as paired/not paired
    /// @param p If true, the input sequences are paired [in]
    void SetPaired(bool p) { m_Opts->SetPaired(p); }

    /// Return the splice/unsplice alignments switch value
    /// @return True if alignments are spliced, false otherwise
    bool GetSpliceAlignments() const { return m_Opts->GetSpliceAlignments(); }

    /// Set splicing alignments
    /// @param s if true alignments will be spliced [in]
    void SetSpliceAlignments(bool s) { m_Opts->SetSpliceAlignments(s); }

    /// Get max intron length
    /// @return Max intron length
    int GetLongestIntronLength(void) const
    { return m_Opts->GetLongestIntronLength(); }

    /// Set max intron length
    /// @param len Max inrton length
    void SetLongestIntronLength(int len) {m_Opts->SetLongestIntronLength(len);}

    
protected:
    /// Set the program and service name for remote blast.
    virtual void SetRemoteProgramAndService_Blast3()
    {
//        m_Opts->SetRemoteProgramAndService_Blast3("blastn", "megablast");
    }
    
    /// Overrides LookupTableDefaults for nucleotide options
    virtual void SetLookupTableDefaults();
    /// Overrides QueryOptionDefaults for nucleotide options
    virtual void SetQueryOptionDefaults();
    /// Overrides InitialWordOptionsDefaults for nucleotide options
    virtual void SetInitialWordOptionsDefaults();
    /// Overrides GappedExtensionDefaults for nucleotide options
    virtual void SetGappedExtensionDefaults();
    /// Overrides ScoringOptionsDefaults for nucleotide options
    virtual void SetScoringOptionsDefaults();
    /// Overrides HitSavingOptionsDefaults for nucleotide options
    virtual void SetHitSavingOptionsDefaults();
    /// Overrides EffectiveLengthsOptionsDefaults for nucleotide options
    virtual void SetEffectiveLengthsOptionsDefaults();
    /// Overrides SubjectSequenceOptionsDefaults for nucleotide options
    virtual void SetSubjectSequenceOptionsDefaults();

private:
    /// Disallow copy constructor
    CMagicBlastOptionsHandle(const CMagicBlastOptionsHandle& rhs);
    /// Disallow assignment operator
    CMagicBlastOptionsHandle& operator=(const CMagicBlastOptionsHandle& rhs);
};

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */


#endif  /* ALGO_BLAST_API___MAGIC_BLAST_OPTIONS__HPP */
