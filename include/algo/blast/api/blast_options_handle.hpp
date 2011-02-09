#ifndef ALGO_BLAST_API___BLAST_OPTIONS_HANDLE__HPP
#define ALGO_BLAST_API___BLAST_OPTIONS_HANDLE__HPP

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

/// @file blast_options_handle.hpp
/// Declares the CBlastOptionsHandle and CBlastOptionsFactory classes.

#include <algo/blast/api/blast_options.hpp>
#include <set>

/** @addtogroup AlgoBlast
 *
 * @{
 */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

// forward declarations
class CBlastOptionsHandle;

/** 
* Creates BlastOptionsHandle objects with default values for the 
* programs/tasks requested. This factory is provided as a convenience
* to create CBlastOptionsHandles which are configured with default values for 
* a given program/task and will NOT be modified before passing them to objects
* which will execute the BLAST algorithm. If you need to set options for the
* specific task at hand, please instantiate the appropriate CBlastOptionsHandle
* subclass.
*
* @sa @ref blast_opts_cookbook
*
* Example:
* @code
* ...
* CRef<CBlastOptionsHandle> opts(CBlastOptionsFactory::Create(eBlastn));
* CBl2Seq blaster(query, subject, *opts);
* TSeqAlignVector results = blaster.Run();
* ...
* opts.Reset(CBlastOptionsFactory::Create(eMegablast));
* blaster.SetOptionsHandle() = *opts;
* results = blaster.Run();
* ...
* opts.Reset(CBlastOptionsFactory::Create(eDiscMegablast));
* blaster.SetOptionsHandle() = *opts;
* results = blaster.Run();
* ...
* @endcode
*/

class NCBI_XBLAST_EXPORT CBlastOptionsFactory
{
public:
    /// Convenience define
    /// @sa CBlastOptions class
    typedef CBlastOptions::EAPILocality EAPILocality;
    
    /// Creates an options handle object configured with default options for 
    /// the requested program, throws an exception if an unsupported program 
    /// is requested
    /// @param program BLAST program [in]
    /// @param locality Local processing (default) or remote processing.
    /// @return requested options handle with default values set
    /// @throw CBlastException in case of an unhandled program type
    static CBlastOptionsHandle* 
        Create(EProgram program, 
               EAPILocality locality = CBlastOptions::eLocal);

    /// Creates an options handle object configured with default options for 
    /// the requested task, throws an exception if an unsupported task 
    /// is requested
    /// @param task BLAST task [in]
    /// @param locality Local processing (default) or remote processing.
    /// @return requested options handle with default values set
    /// @throw CBlastException in case of an unhandled program type
    /// @sa GetDocumentation
    static CBlastOptionsHandle*
        CreateTask(string task,
               EAPILocality locality = CBlastOptions::eLocal);

    /// Sets of tasks for the command line BLAST binaries
    enum ETaskSets {
        eNuclNucl,      ///< Nucleotide-nucleotide tasks
        eProtProt,      ///< Protein-protein tasks
        eAll            ///< Retrieve all available tasks
    };

    /// Retrieve the set of supported tasks
    static set<string> GetTasks(ETaskSets choice = eAll);

    /// Return the documentation for the provided task
    /// @param task_name Task name for which to provide documentation [in]
    static string GetDocumentation(const string& task_name);

private:
    /// Private c-tor
    CBlastOptionsFactory();
};

/// Handle to the options to the BLAST algorithm.
///
/// This abstract base class only defines those options that are truly 
/// "universal" BLAST options (they apply to all flavors of BLAST).
///
/// @sa @ref blast_opts_cookbook 
///     @ref blast_opts_cpp_design
///
/// @invariant Derived classes define options that are applicable only to 
/// those programs whose options they manipulate.

class NCBI_XBLAST_EXPORT CBlastOptionsHandle : public CObject
{
public:
    /// Convenience define
    /// @sa CBlastOptions class
    typedef CBlastOptions::EAPILocality EAPILocality;
    
    /// Default c-tor
    CBlastOptionsHandle(EAPILocality locality);
    
    /// Validate the options contained in this object
    /// @note This method is meant to be used before calling any code that
    /// processes the BLAST options classes
    bool Validate() const;

    /// Return the object which this object is a handle for. This method is
    /// intended to be used when one wants to INSPECT the values of options
    /// which are not exposed by the classes derived from 
    /// CBlastOptionsHandle.
    const CBlastOptions& GetOptions() const { return *m_Opts; }

    /// Returns a reference to the internal options class which this object is
    /// a handle for. Please note that using objects of type CBlastOptions
    /// directly allows one to set inconsistent combinations of options.
    ///
    /// @note Assumes user knows exactly how to set the individual options 
    /// correctly. Calling CBlastOptions::Validate on this object is STRONGLY
    /// recommended.
    CBlastOptions& SetOptions() { return *m_Opts; }
    
    /// Resets the state of the object to all default values.
    /// This is a template method (design pattern).
    virtual void SetDefaults();
    
    /// Returns true if this object needs default values set.
    void DoneDefaults() { m_Opts->DoneDefaults(); }

   /******************** Initial Word options **********************/

    /// Returns WindowSize
    int GetWindowSize() const { return m_Opts->GetWindowSize(); }
    /// Sets WindowSize
    /// @param ws WindowSize [in]
    void SetWindowSize(int ws) { m_Opts->SetWindowSize(ws); }
    int GetOffDiagonalRange() const { return m_Opts->GetOffDiagonalRange(); }
    void SetOffDiagonalRange(int r) { m_Opts->SetOffDiagonalRange(r); }
    
    /******************* Query setup options ************************/
    /// Clears the filtering options
    void ClearFilterOptions() { m_Opts->ClearFilterOptions(); }
    /// Returns FilterString
    char* GetFilterString() const;
    /// Sets FilterString
    /// @param f FilterString [in]
    void SetFilterString(const char* f, bool clear = true);

    /// Returns whether masking should only be done for lookup table creation.
    bool GetMaskAtHash() const { return m_Opts->GetMaskAtHash(); }
    /// Sets MaskAtHash
    /// @param m whether masking should only be done for lookup table [in]
    void SetMaskAtHash(bool m = true) { m_Opts->SetMaskAtHash(m); }
    /******************* Gapped extension options *******************/
    /// Returns GapXDropoff
    double GetGapXDropoff() const { return m_Opts->GetGapXDropoff(); }
    /// Sets GapXDropoff
    /// @param x GapXDropoff [in]
    void SetGapXDropoff(double x) { m_Opts->SetGapXDropoff(x); }

    /// Returns GapTrigger
    double GetGapTrigger() const { return m_Opts->GetGapTrigger(); }
    /// Sets GapTrigger
    /// @param g GapTrigger [in]
    void SetGapTrigger(double g) { m_Opts->SetGapTrigger(g); }

    /// Returns GapXDropoffFinal
    double GetGapXDropoffFinal() const { 
        return m_Opts->GetGapXDropoffFinal(); 
    }
    /// Sets GapXDropoffFinal
    /// @param x GapXDropoffFinal [in]
    void SetGapXDropoffFinal(double x) { m_Opts->SetGapXDropoffFinal(x); }

    /******************* Hit saving options *************************/
    /// Returns HitlistSize
    int GetHitlistSize() const { return m_Opts->GetHitlistSize(); }
    /// Sets HitlistSize
    /// @param s HitlistSize [in]
    void SetHitlistSize(int s) { m_Opts->SetHitlistSize(s); }

    /// Returns MaxNumHspPerSequence
    int GetMaxNumHspPerSequence() const { 
        return m_Opts->GetMaxNumHspPerSequence();
    }
    /// Sets MaxNumHspPerSequence
    /// @param m MaxNumHspPerSequence [in]
    void SetMaxNumHspPerSequence(int m) { m_Opts->SetMaxNumHspPerSequence(m); }

    /// Returns EvalueThreshold
    double GetEvalueThreshold() const { return m_Opts->GetEvalueThreshold(); }
    /// Sets EvalueThreshold
    /// @param eval EvalueThreshold [in]
    void SetEvalueThreshold(double eval) { m_Opts->SetEvalueThreshold(eval); } 
    /// Returns CutoffScore
    int GetCutoffScore() const { return m_Opts->GetCutoffScore(); }
    /// Sets CutoffScore
    /// @param s CutoffScore [in]
    void SetCutoffScore(int s) { m_Opts->SetCutoffScore(s); }

    /// Returns PercentIdentity
    double GetPercentIdentity() const { return m_Opts->GetPercentIdentity(); }
    /// Sets PercentIdentity
    /// @param p PercentIdentity [in]
    void SetPercentIdentity(double p) { m_Opts->SetPercentIdentity(p); }

    /// Returns MinDiagSeparation
    int GetMinDiagSeparation() const { return m_Opts->GetMinDiagSeparation(); }
    /// Sets MinDiagSeparation
    /// @param d MinDiagSeparation [in]
    void SetMinDiagSeparation(int d) { m_Opts->SetMinDiagSeparation(d); }

    /// Returns GappedMode
    bool GetGappedMode() const { return m_Opts->GetGappedMode(); }
    /// Sets GappedMode
    /// @param m GappedMode [in]
    void SetGappedMode(bool m = true) { m_Opts->SetGappedMode(m); }

    /// Returns Culling limit
    int GetCullingLimit() const { return m_Opts->GetCullingLimit(); }
    /// Sets Culling limit
    /// @param s CullingLimit [in]
    void SetCullingLimit(int s) { m_Opts->SetCullingLimit(s); }

    /// Returns MaskLevel -RMH-
    int GetMaskLevel() const { return m_Opts->GetMaskLevel(); }
    /// Sets MaskLevel -RMH-
    /// @param ml MaskLevel [in]
    void SetMaskLevel(int ml) { m_Opts->SetMaskLevel(ml); }

    /// Returns Complexity Adjustment Mode -RMH-
    bool GetComplexityAdjMode() const { return m_Opts->GetComplexityAdjMode(); }
    /// Sets ComplexityAdjMode -RMH-
    /// @param m ComplexityAdjMode [in]
    void SetComplexityAdjMode(bool m = true) { m_Opts->SetComplexityAdjMode(m); }

    /******************** Database (subject) options *******************/
    /// Returns DbLength
    Int8 GetDbLength() const { return m_Opts->GetDbLength(); }
    /// Sets DbLength
    /// @param len DbLength [in]
    void SetDbLength(Int8 len) { m_Opts->SetDbLength(len); }

    /// Returns DbSeqNum
    unsigned int GetDbSeqNum() const { return m_Opts->GetDbSeqNum(); }
    /// Sets DbSeqNum
    /// @param num DbSeqNum [in]
    void SetDbSeqNum(unsigned int num) { m_Opts->SetDbSeqNum(num); }

    /// Returns EffectiveSearchSpace
    Int8 GetEffectiveSearchSpace() const {
        return m_Opts->GetEffectiveSearchSpace();
    }
    /// Sets EffectiveSearchSpace
    /// @param eff EffectiveSearchSpace [in]
    void SetEffectiveSearchSpace(Int8 eff) {
        m_Opts->SetEffectiveSearchSpace(eff);
    }
    
protected: 
    /// Set the program and service name for remote blast.
    virtual void SetRemoteProgramAndService_Blast3() = 0;
    
    /// Data type this class controls access to
    CRef<CBlastOptions> m_Opts;
    
    /// Set to true when 'remote' options should ignore setters.
    bool m_DefaultsMode;
    
    // These methods make up the template method
    /// Sets LookupTableDefaults
    virtual void SetLookupTableDefaults() = 0;
    /// Sets QueryOptionDefaults
    virtual void SetQueryOptionDefaults() = 0;
    /// Sets InitialWordOptionsDefaults
    virtual void SetInitialWordOptionsDefaults() = 0;
    /// Sets GappedExtensionDefaults
    virtual void SetGappedExtensionDefaults() = 0;
    /// Sets ScoringOptionsDefaults
    virtual void SetScoringOptionsDefaults() = 0;
    /// Sets HitSavingOptionsDefaults
    virtual void SetHitSavingOptionsDefaults() = 0;
    /// Sets EffectiveLengthsOptionsDefaults
    virtual void SetEffectiveLengthsOptionsDefaults() = 0;
    /// Sets SubjectSequenceOptionsDefaults
    virtual void SetSubjectSequenceOptionsDefaults() = 0;
};

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */

#endif  /* ALGO_BLAST_API___BLAST_OPTIONS_HANDLE__HPP */
