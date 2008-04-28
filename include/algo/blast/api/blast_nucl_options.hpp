#ifndef ALGO_BLAST_API___BLAST_NUCL_OPTIONS__HPP
#define ALGO_BLAST_API___BLAST_NUCL_OPTIONS__HPP

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

/// @file blast_nucl_options.hpp
/// Declares the CBlastNucleotideOptionsHandle class.

#include <algo/blast/api/blast_options_handle.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Handle to the nucleotide-nucleotide options to the BLAST algorithm.
///
/// Adapter class for nucleotide-nucleotide BLAST comparisons.
/// Exposes an interface to allow manipulation the options that are relevant to
/// this type of search.
/// 
/// NB: By default, traditional megablast defaults are used. If blastn defaults
/// are desired, please call the appropriate member function:
///
///    void SetTraditionalBlastnDefaults();
///    void SetTraditionalMegablastDefaults();

class NCBI_XBLAST_EXPORT CBlastNucleotideOptionsHandle : 
                                            public CBlastOptionsHandle
{
public:

    /// Creates object with default options set
    CBlastNucleotideOptionsHandle(EAPILocality locality = CBlastOptions::eLocal);

    /// Sets Defaults
    virtual void SetDefaults();

    /******************* Lookup table options ***********************/
    /// Returns LookupTableType
    ELookupTableType GetLookupTableType() const { return m_Opts->GetLookupTableType(); }
    /// Sets LookupTableType
    /// @param type LookupTableType [in]
    void SetLookupTableType(ELookupTableType type) 
    { 
        m_Opts->SetLookupTableType(type); 
    }

    /// Returns WordSize
    int GetWordSize() const { return m_Opts->GetWordSize(); }
    /// Sets WordSize
    /// @param ws WordSize [in]
    void SetWordSize(int ws) 
    { 
        m_Opts->SetWordSize(ws); 
    }

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

    /// Is dust filtering enabled?
    bool GetDustFiltering() const { return m_Opts->GetDustFiltering(); }
    /// Enable dust filtering.
    /// @param val enable dust filtering [in]
    void SetDustFiltering(bool val) { m_Opts->SetDustFiltering(val); }

    /// Get level parameter for dust
    int GetDustFilteringLevel() const { return m_Opts->GetDustFilteringLevel(); }
    /// Set level parameter for dust.  Acceptable values: 2 < level < 64
    /// @param level dust filtering parameter level [in]
    void SetDustFilteringLevel(int level) { m_Opts->SetDustFilteringLevel(level); }

    /// Get window parameter for dust
    int GetDustFilteringWindow() const { return m_Opts->GetDustFilteringWindow(); }
    /// Set window parameter for dust.  Acceptable values: 8 < windowsize < 64
    /// @param window dust filtering parameter window [in]
    void SetDustFilteringWindow(int window) { m_Opts->SetDustFilteringWindow(window); }

    /// Get linker parameter for dust
    int GetDustFilteringLinker() const { return m_Opts->GetDustFilteringLinker(); }
    /// Set linker parameter for dust.  Acceptable values: 1 < linker < 32
    /// @param linker dust filtering parameter linker [in]
    void SetDustFilteringLinker(int linker) { m_Opts->SetDustFilteringLinker(linker); }

    /// Is repeat filtering enabled?
    bool GetRepeatFiltering() const { return m_Opts->GetRepeatFiltering(); }
    /// Enable repeat filtering.
    /// @param val enable repeat filtering [in]
    void SetRepeatFiltering(bool val) { m_Opts->SetRepeatFiltering(val); }

    /// Get the repeat filtering database
    const char* GetRepeatFilteringDB() const { return m_Opts->GetRepeatFilteringDB(); }
    /// Enable repeat filtering.
    /// @param db repeat filtering database [in]
    void SetRepeatFilteringDB(const char* db) { m_Opts->SetRepeatFilteringDB(db); }
    
    /// Get the window masker taxid (or 0 if not set).
    int GetWindowMaskerTaxId() const { return m_Opts->GetWindowMaskerTaxId(); }
    
    /// Enable window masker and select a taxid (or 0 to disable).
    /// @param taxid Select Window Masker filtering database for this taxid [in]
    void SetWindowMaskerTaxId(int taxid) { m_Opts->SetWindowMaskerTaxId(taxid); }
    
    /// Get the window masker database name (or NULL if not set).
    const char* GetWindowMaskerDatabase() const
    {
        return m_Opts->GetWindowMaskerDatabase();
    }
    
    /// Enable window masker and select a database (or NULL to disable).
    /// @param taxid Select Window Masker filtering database by filename [in]
    void SetWindowMaskerDatabase(const char* db)
    {
        m_Opts->SetWindowMaskerDatabase(db);
    }
    
    /******************* Initial word options ***********************/

    /// Returns XDropoff
    double GetXDropoff() const { return m_Opts->GetXDropoff(); } 
    /// Sets XDropoff
    /// @param x XDropoff [in]
    void SetXDropoff(double x) { m_Opts->SetXDropoff(x); }

    /******************* Gapped extension options *******************/
    /// Returns GapExtnAlgorithm
    EBlastPrelimGapExt GetGapExtnAlgorithm() const { return m_Opts->GetGapExtnAlgorithm(); }

    /// Sets GapExtnAlgorithm
    /// @param algo GapExtnAlgorithm [in]
    void SetGapExtnAlgorithm(EBlastPrelimGapExt algo) {m_Opts->SetGapExtnAlgorithm(algo);}

    /// Returns GapTracebackAlgorithm
    EBlastTbackExt GetGapTracebackAlgorithm() const { return m_Opts->GetGapTracebackAlgorithm(); }

    /// Sets GapTracebackAlgorithm
    /// @param algo GapTracebackAlgorithm [in]
    void SetGapTracebackAlgorithm(EBlastTbackExt algo) {m_Opts->SetGapTracebackAlgorithm(algo); }

    /************************ Scoring options ************************/
    /// Returns MatchReward
    int GetMatchReward() const { return m_Opts->GetMatchReward(); }
    /// Sets MatchReward
    /// @param r MatchReward [in]
    void SetMatchReward(int r) { m_Opts->SetMatchReward(r); }

    /// Returns MismatchPenalty
    int GetMismatchPenalty() const { return m_Opts->GetMismatchPenalty(); }
    /// Sets MismatchPenalty
    /// @param p MismatchPenalty [in]
    void SetMismatchPenalty(int p) { m_Opts->SetMismatchPenalty(p); }

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

    /// Sets TraditionalBlastnDefaults
    void SetTraditionalBlastnDefaults();
    /// Sets TraditionalMegablastDefaults
    void SetTraditionalMegablastDefaults();
    
protected:
    /// Set the program and service name for remote blast.
    virtual void SetRemoteProgramAndService_Blast3()
    {
        m_Opts->SetRemoteProgramAndService_Blast3("blastn", "megablast");
    }
    
    /// Overrides LookupTableDefaults for nucleotide options
    virtual void SetLookupTableDefaults();
    /// Overrides MBLookupTableDefaults for nucleotide options
    virtual void SetMBLookupTableDefaults();
    /// Overrides QueryOptionDefaults for nucleotide options
    virtual void SetQueryOptionDefaults();
    /// Overrides InitialWordOptionsDefaults for nucleotide options
    virtual void SetInitialWordOptionsDefaults();
    /// Overrides MBInitialWordOptionsDefaults for nucleotide options
    virtual void SetMBInitialWordOptionsDefaults();
    /// Overrides GappedExtensionDefaults for nucleotide options
    virtual void SetGappedExtensionDefaults();
    /// Overrides MBGappedExtensionDefaults for nucleotide options
    virtual void SetMBGappedExtensionDefaults();
    /// Overrides ScoringOptionsDefaults for nucleotide options
    virtual void SetScoringOptionsDefaults();
    /// Overrides MBScoringOptionsDefaults for nucleotide options
    virtual void SetMBScoringOptionsDefaults();
    /// Overrides HitSavingOptionsDefaults for nucleotide options
    virtual void SetHitSavingOptionsDefaults();
    /// Overrides MBHitSavingOptionsDefaults for nucleotide options
    virtual void SetMBHitSavingOptionsDefaults();
    /// Overrides EffectiveLengthsOptionsDefaults for nucleotide options
    virtual void SetEffectiveLengthsOptionsDefaults();
    /// Overrides SubjectSequenceOptionsDefaults for nucleotide options
    virtual void SetSubjectSequenceOptionsDefaults();

private:
    /// Disallow copy constructor
    CBlastNucleotideOptionsHandle(const CBlastNucleotideOptionsHandle& rhs);
    /// Disallow assignment operator
    CBlastNucleotideOptionsHandle& operator=(const CBlastNucleotideOptionsHandle& rhs);
};

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */


#endif  /* ALGO_BLAST_API___BLAST_NUCL_OPTIONS__HPP */
