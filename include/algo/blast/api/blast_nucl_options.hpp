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
    ~CBlastNucleotideOptionsHandle() {}

    /// Sets Defaults
    virtual void SetDefaults();

    /******************* Lookup table options ***********************/
    /// Returns LookupTableType
    int GetLookupTableType() const { return m_Opts->GetLookupTableType(); }
    /// Sets LookupTableType
    /// @param type LookupTableType [in]
    void SetLookupTableType(int type) 
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

    /******************* Initial word options ***********************/

    /// Returns UngappedExtension
    bool GetUngappedExtension() const { return m_Opts->GetUngappedExtension();}
    /// Sets UngappedExtension
    /// @param val UngappedExtension [in]
    void SetUngappedExtension(bool val = true) { 
        m_Opts->SetUngappedExtension(val); 
    }

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

    /// Returns MatrixPath
    const char* GetMatrixPath() const { return m_Opts->GetMatrixPath(); }
    /// Sets MatrixPath
    /// @param path MatrixPath [in]
    void SetMatrixPath(const char* path) { m_Opts->SetMatrixPath(path); }

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
    
    EProgram GetFactorySetting()
    {
        return m_FactorySetting;
    }
    
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
    
    EProgram m_FactorySetting;
};

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.29  2006/01/23 16:37:45  papadopo
 * use {Set|Get}MinDiagSeparation to specify the number of diagonals to be used in HSP containment tests
 *
 * Revision 1.28  2005/12/22 14:02:57  papadopo
 * remove variable-wordsize-related code
 *
 * Revision 1.27  2005/08/01 12:55:01  madden
 * Change SetGapExtnAlgorithm and SetGapTracebackAlgorithm so they do not change gap costs
 *
 * Revision 1.26  2005/05/09 20:08:48  bealer
 * - Add program and service strings to CBlastOptions for remote blast.
 * - New CBlastOptionsHandle constructor for CRemoteBlast.
 * - Prohibit copy construction/assignment for CRemoteBlast.
 * - Code in each BlastOptionsHandle derived class to set program+service.
 *
 * Revision 1.25  2005/03/31 13:43:49  camacho
 * BLAST options API clean-up
 *
 * Revision 1.24  2005/03/10 13:20:22  madden
 * Moved [GS]etWindowSize and [GS]etEffectiveSearchSpace to CBlastOptionsHandle
 *
 * Revision 1.23  2005/03/02 16:45:24  camacho
 * Remove use_real_db_size
 *
 * Revision 1.22  2005/02/24 13:46:20  madden
 * Add setters and getters for filtering options
 *
 * Revision 1.21  2005/01/10 13:30:47  madden
 * Changes associated with removal of options for ScanStep, ContainerType, and ExtensionMethod as well as call to CalculateBestStride
 *
 * Revision 1.20  2004/12/28 13:36:17  madden
 * [GS]etWordSize is now an int rather than a short
 *
 * Revision 1.19  2004/08/30 16:53:23  dondosha
 * Added E in front of enum names ESeedExtensionMethod and ESeedContainerType
 *
 * Revision 1.18  2004/08/26 20:55:16  dondosha
 * Allow options dependency logic for local options, but not for remote
 *
 * Revision 1.17  2004/08/24 15:11:33  bealer
 * - Rollback blast option interaction code to fix bug.
 *
 * Revision 1.16  2004/08/03 21:52:16  dondosha
 * Minor correction in options dependency logic
 *
 * Revision 1.15  2004/08/03 20:20:30  dondosha
 * Added some option dependencies to setter methods
 *
 * Revision 1.14  2004/08/02 15:01:36  bealer
 * - Distinguish between blastn and megablast (for remote blast).
 *
 * Revision 1.13  2004/06/14 15:42:08  dondosha
 * Typo fix in comment
 *
 * Revision 1.12  2004/06/08 22:27:36  camacho
 * Add missing doxygen comments
 *
 * Revision 1.11  2004/05/18 12:48:24  madden
 * Add setter and getter for GapTracebackAlgorithm (EBlastTbackExt)
 *
 * Revision 1.10  2004/05/17 15:28:54  madden
 * Int algorithm_type replaced with enum EBlastPrelimGapExt
 *
 * Revision 1.9  2004/05/04 13:09:20  camacho
 * Made copy-ctor & assignment operator private
 *
 * Revision 1.8  2004/03/19 14:53:24  camacho
 * Move to doxygen group AlgoBlast
 *
 * Revision 1.7  2004/03/10 14:55:02  madden
 * Added methods for get/set matrix, matrix-path, gap-opening, gap-extension
 *
 * Revision 1.6  2004/02/10 19:48:07  dondosha
    /// Sets MBGappedExtensionDefaults
    /// @param x MBGappedExtensionDefaults [in]
 * Added SetMBGappedExtensionDefaults method
 *
 * Revision 1.5  2004/01/16 20:42:59  bealer
 * - Add locality flag for blast options handle classes.
 *
 * Revision 1.4  2003/12/15 23:41:35  dondosha
 * Added [gs]etters of database (subject) length and number of sequences to general options handle
 *
 * Revision 1.3  2003/12/10 18:00:54  camacho
 * Added comment explaining defaults
 *
 * Revision 1.2  2003/12/09 12:40:22  camacho
 * Added windows export specifiers
 *
 * Revision 1.1  2003/11/26 18:22:14  camacho
 * +Blast Option Handle classes
 *
 * ===========================================================================
 */

#endif  /* ALGO_BLAST_API___BLAST_NUCL_OPTIONS__HPP */
