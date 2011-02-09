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
 * Author:  Christiam Camacho
 *
 */

/** @file blast_options.hpp
 * Declares class to encapsulate all BLAST options
 */

#ifndef ALGO_BLAST_API___BLAST_OPTION__HPP
#define ALGO_BLAST_API___BLAST_OPTION__HPP

#include <objects/blast/Blast4_value.hpp>
#include <objects/blast/Blast4_parameter.hpp>
#include <objects/blast/Blast4_parameters.hpp>
#include <objects/blast/Blast4_queue_search_reques.hpp>

#include <algo/blast/api/blast_types.hpp>
#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/api/blast_exception.hpp>

#include <algo/blast/core/blast_options.h>
#include <algo/blast/composition_adjustment/composition_constants.h>

// Forward declarations of classes that need to be declared friend 
// (mostly unit test classes)
class CTracebackTestFixture; 
class CBlastSetupTestFixture;
class CUniformSearchTest; 
class CTracebackSearchTestFixture;

class CBlastTabularFormatThread;

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CSeq_loc;
END_SCOPE(objects)

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_SCOPE(blast)

//#ifndef SKIP_DOXYGEN_PROCESSING


// Forward declarations
class CBlastOptionsLocal;
class CBlastOptionsRemote;
class CBlastOptionsMemento;

/// Encapsulates ALL the BLAST algorithm's options. To ensure that the default
/// options are set properly, it is recommended that this object is not created
/// directly by the calling code, instead, it should be obtained from calling
/// CBlastOptionsHandle::[GS]etOptions().
/// @note This class provides accessors and mutators for all BLAST options 
/// without preventing the caller from setting inconsistent options.
/// @sa @ref blast_opts_cookbook
class NCBI_XBLAST_EXPORT CBlastOptions : public CObject
{
public:
    /// Enumerates the possible contexts in which objects of this type can be
    /// used
    enum EAPILocality {
        /// To be used for running BLAST locally. 
        /// @sa CBl2Seq, CLocalBlast
        eLocal,
        /// To be used when running BLAST remotely. 
        /// @sa CRemoteBlast
        eRemote,
        eBoth
    };
    
    /// Constructor which allows specification of local or remote version of
    /// the options (might change in the future)
    /// @param locality specification of whether this type will be used with a
    /// remote or local BLAST search database class
    CBlastOptions(EAPILocality locality = eLocal);

    /// Destructor
    ~CBlastOptions();

    /// Explicit deep copy of the Blast options object.
    /// @return Copy of this Blast options object.
    CRef<CBlastOptions> Clone() const;

    /// Return the locality used when the object was created
    EAPILocality GetLocality() const;
    
    /// Validate the options
    bool Validate() const;
    
    /// Accessors/Mutators for individual options

    /// Returns the task this object is best suited for
    EProgram GetProgram() const;
    /// Sets the task this object is best suited for
    void SetProgram(EProgram p);

    /// Returns the CORE BLAST notion of program type
    EBlastProgramType GetProgramType() const;

    /******************* Lookup table options ***********************/
    /// Returns WordThreshold
    double GetWordThreshold() const;
    /// Sets WordThreshold
    /// @param w WordThreshold [in]
    void SetWordThreshold(double w);

#ifndef SKIP_DOXYGEN_PROCESSING

    ELookupTableType GetLookupTableType() const;
    void SetLookupTableType(ELookupTableType type);

    int GetWordSize() const;
    void SetWordSize(int ws);

    /// Megablast only lookup table options
    unsigned char GetMBTemplateLength() const;
    void SetMBTemplateLength(unsigned char len);

    unsigned char GetMBTemplateType() const;
    void SetMBTemplateType(unsigned char type);

    /******************* Query setup options ************************/
    void ClearFilterOptions();
#endif /* SKIP_DOXYGEN_PROCESSING */

    /// Return the filtering string used
    /// @return copy of the filtering options string, caller must free() the
    /// return value
    /// @deprecated Do not use this method, instead use the various methods to
    /// retrieve filtering options
    /// @sa GetMaskAtHash, GetDustFiltering, GetDustFilteringLevel,
    /// GetDustFilteringWindow, GetDustFilteringWindow, GetSegFiltering,
    /// GetSegFilteringWindow, GetSegFilteringLocut, GetSegFilteringHicut
    /// GetRepeatFiltering, GetRepeatFilteringDB
    NCBI_DEPRECATED char* GetFilterString() const;
#ifndef SKIP_DOXYGEN_PROCESSING
    NCBI_DEPRECATED void SetFilterString(const char* f, bool clear = true);

    bool GetMaskAtHash() const;
    void SetMaskAtHash(bool val = true);

    bool GetDustFiltering() const;
    void SetDustFiltering(bool val = true);

    int GetDustFilteringLevel() const;
    void SetDustFilteringLevel(int m);

    int GetDustFilteringWindow() const;
    void SetDustFilteringWindow(int m);

    int GetDustFilteringLinker() const;
    void SetDustFilteringLinker(int m);

    bool GetSegFiltering() const;
    void SetSegFiltering(bool val = true);

    int GetSegFilteringWindow() const;
    void SetSegFilteringWindow(int m);

    double GetSegFilteringLocut() const;
    void SetSegFilteringLocut(double m);

    double GetSegFilteringHicut() const;
    void SetSegFilteringHicut(double m);
#endif /* SKIP_DOXYGEN_PROCESSING */

    /// Returns true if repeat filtering is on
    bool GetRepeatFiltering() const;
    /// Turns on repeat filtering using the default repeat database, namely
    /// kDefaultRepeatFilterDb 
    /// @note Either SetRepeatFiltering or SetRepeatFilteringDB should be
    /// called, if both are called, only the last one called will take effect
    void SetRepeatFiltering(bool val = true);

    /// Returns the name of the repeat filtering database to use
    const char* GetRepeatFilteringDB() const;
    /// Sets the repeat filtering database to use
    /// @note Either SetRepeatFiltering or SetRepeatFilteringDB should be
    /// called, if both are called, only the last one called will take effect
    void SetRepeatFilteringDB(const char* db);

    /// Returns the tax id used for the windowmasker database to use, if set
    /// via SetWindowMaskerTaxId (otherwise, returns 0)
    int GetWindowMaskerTaxId() const;

    /// Sets the tax id to select an appropriate windowmasker database
    /// Conversion algorithm from tax id to database name is specific to NCBI,
    /// will not work outside NCBI.
    /// @note this only runs on machines that have the WINDOW_MASKER_PATH
    /// configuration value set and have the the correct endianness. If
    /// windowmasker databases are not available, the filtering will fail
    /// silently
    void SetWindowMaskerTaxId(int taxid);

    /// Return the name of the windowmasker database to use
    const char* GetWindowMaskerDatabase() const;

    /// Sets the windowmasker database to use. This must be the name of a
    /// subdirectory of WINDOW_MASKER_PATH
    /// @note this only runs on machines that have the WINDOW_MASKER_PATH
    /// configuration value set and have the the correct endianness. If
    /// windowmasker databases are not available, the filtering will fail
    /// silently
    void SetWindowMaskerDatabase(const char* db);

#ifndef SKIP_DOXYGEN_PROCESSING
    objects::ENa_strand GetStrandOption() const;
    void SetStrandOption(objects::ENa_strand s);

    int GetQueryGeneticCode() const;
    void SetQueryGeneticCode(int gc);

    /******************* Initial word options ***********************/
    int GetWindowSize() const;
    void SetWindowSize(int w);

    int GetOffDiagonalRange() const;
    void SetOffDiagonalRange(int r);

    double GetXDropoff() const;
    void SetXDropoff(double x);

    /******************* Gapped extension options *******************/
    double GetGapXDropoff() const;
    void SetGapXDropoff(double x);

    double GetGapXDropoffFinal() const;
    void SetGapXDropoffFinal(double x);

    double GetGapTrigger() const;
    void SetGapTrigger(double g);

    EBlastPrelimGapExt GetGapExtnAlgorithm() const;
    void SetGapExtnAlgorithm(EBlastPrelimGapExt a);

    EBlastTbackExt GetGapTracebackAlgorithm() const;
    void SetGapTracebackAlgorithm(EBlastTbackExt a);

    ECompoAdjustModes GetCompositionBasedStats() const;
    void SetCompositionBasedStats(ECompoAdjustModes mode);

    bool GetSmithWatermanMode() const;
    void SetSmithWatermanMode(bool m = true);

    int GetUnifiedP() const;
    void SetUnifiedP(int u = 0);

    /******************* Hit saving options *************************/
    int GetHitlistSize() const;
    void SetHitlistSize(int s);

    int GetMaxNumHspPerSequence() const;
    void SetMaxNumHspPerSequence(int m);

    int GetCullingLimit() const;
    void SetCullingLimit(int s);

    double GetBestHitOverhang() const;
    void SetBestHitOverhang(double overhang);
    double GetBestHitScoreEdge() const;
    void SetBestHitScoreEdge(double score_edge);

    // Expect value cut-off threshold for an HSP, or a combined hit if sum
    // statistics is used
    double GetEvalueThreshold() const;
    void SetEvalueThreshold(double eval);

    // Raw score cutoff threshold
    int GetCutoffScore() const;
    void SetCutoffScore(int s);

    double GetPercentIdentity() const;
    void SetPercentIdentity(double p);

    int GetMinDiagSeparation() const;
    void SetMinDiagSeparation(int d);

    /// Sum statistics options
    bool GetSumStatisticsMode() const;
    void SetSumStatisticsMode(bool m = true);

    /// for linking HSPs with uneven gaps
    /// @todo fix this description
    int GetLongestIntronLength() const;
    /// for linking HSPs with uneven gaps
    /// @todo fix this description
    void SetLongestIntronLength(int l);

    /// Returns true if gapped BLAST is set, false otherwise
    bool GetGappedMode() const;
    void SetGappedMode(bool m = true);

    // Masklevel filtering option -RMH-
    int GetMaskLevel() const;
    void SetMaskLevel(int s);

    /// Returns true if cross_match-like complexity adjusted
    //  scoring is required, false otherwise. -RMH-
    bool GetComplexityAdjMode() const;
    void SetComplexityAdjMode(bool m = true);

    /************************ Scoring options ************************/
    const char* GetMatrixName() const;
    void SetMatrixName(const char* matrix);

    int GetMatchReward() const;
    void SetMatchReward(int r);

    int GetMismatchPenalty() const;
    void SetMismatchPenalty(int p);

    int GetGapOpeningCost() const;
    void SetGapOpeningCost(int g);

    int GetGapExtensionCost() const;
    void SetGapExtensionCost(int e);

    int GetFrameShiftPenalty() const;
    void SetFrameShiftPenalty(int p);

    bool GetOutOfFrameMode() const;
    void SetOutOfFrameMode(bool m = true);

    /******************** Effective Length options *******************/
    Int8 GetDbLength() const;
    void SetDbLength(Int8 l);

    unsigned int GetDbSeqNum() const;
    void SetDbSeqNum(unsigned int n);

    Int8 GetEffectiveSearchSpace() const;
    void SetEffectiveSearchSpace(Int8 eff);
    void SetEffectiveSearchSpace(const vector<Int8>& eff);

    int GetDbGeneticCode() const;
    
    // Set both integer and string genetic code in one call
    void SetDbGeneticCode(int gc);

    /// @todo PSI-Blast options could go on their own subclass?
    const char* GetPHIPattern() const;
    void SetPHIPattern(const char* pattern, bool is_dna);

    /******************** PSIBlast options *******************/
    double GetInclusionThreshold() const;
    void SetInclusionThreshold(double u);

    int GetPseudoCount() const;
    void SetPseudoCount(int u);
    
    /******************** Megablast Database Index *******************/
    bool GetUseIndex() const;
    bool GetForceIndex() const;
    bool GetMBIndexLoaded() const;
    const string GetIndexName() const;
    void SetMBIndexLoaded( bool index_loaded = true );
    void SetUseIndex( bool use_index = true, const string & index_name = "", bool force_index = false );

    /// Allows to dump a snapshot of the object
    /// @todo this doesn't do anything for locality eRemote
    void DebugDump(CDebugDumpContext ddc, unsigned int depth) const;
    
    void DoneDefaults() const;
    
    /// This returns a list of parameters for remote searches.
    typedef ncbi::objects::CBlast4_parameters TBlast4Opts;
    TBlast4Opts * GetBlast4AlgoOpts();
    
    bool operator==(const CBlastOptions& rhs) const;
    bool operator!=(const CBlastOptions& rhs) const;

#endif /* SKIP_DOXYGEN_PROCESSING */
    
    /// Set the program and service name for remote blast.
    void SetRemoteProgramAndService_Blast3(const string & p, const string & s)
    {
        m_ProgramName = p;
        m_ServiceName = s;
    }
    
    /// Get the program and service name for remote blast.
    virtual void GetRemoteProgramAndService_Blast3(string & p, string & s) const
    {
        _ASSERT(m_Remote);
        p = m_ProgramName;
        s = m_ServiceName;
    }

    /// Create a snapshot of the state of this object for internal use of its
    /// data structures (BLAST C++ APIs only)
    const CBlastOptionsMemento* CreateSnapshot() const;
    
    /// If this is true, remote options will ignore "Set" calls.
    void SetDefaultsMode(bool dmode);
    bool GetDefaultsMode() const;
    
private:
    /// Prohibit copy c-tor 
    CBlastOptions(const CBlastOptions& bo);
    /// Prohibit assignment operator
    CBlastOptions& operator=(const CBlastOptions& bo);

    // Pointers to local and remote objects
    
    CBlastOptionsLocal  * m_Local;
    CBlastOptionsRemote * m_Remote;
    
    /// Program Name for Blast3
    string m_ProgramName;
    
    /// Service Name for Blast3
    string m_ServiceName;
    
    /// Defaults mode (remote options will ignore Set ops).
    bool m_DefaultsMode;
    
    /// Auxiliary to throw CBlastExceptions
    /// @param msg message to pass in the exception [in]
    void x_Throwx(const string& msg) const;
    /// Returns QuerySetUpOptions for eLocal objects, NULL for eRemote
    /// @internal
    QuerySetUpOptions * GetQueryOpts() const;
    /// Returns LookupTableOptions for eLocal objects, NULL for eRemote
    LookupTableOptions * GetLutOpts() const;
    /// Returns BlastInitialWordOptions for eLocal objects, NULL for eRemote
    BlastInitialWordOptions * GetInitWordOpts() const;
    /// Returns BlastExtensionOptions for eLocal objects, NULL for eRemote
    BlastExtensionOptions * GetExtnOpts() const;
    /// Returns BlastHitSavingOptions for eLocal objects, NULL for eRemote
    BlastHitSavingOptions * GetHitSaveOpts() const;
    /// Returns PSIBlastOptions for eLocal objects, NULL for eRemote
    PSIBlastOptions * GetPSIBlastOpts() const;
    /// Returns BlastDatabaseOptions for eLocal objects, NULL for eRemote
    BlastDatabaseOptions * GetDbOpts() const;
    /// Returns BlastScoringOptions for eLocal objects, NULL for eRemote
    BlastScoringOptions * GetScoringOpts() const;
    /// Returns BlastEffectiveLengthsOptions for eLocal objects, NULL for 
    /// eRemote
    BlastEffectiveLengthsOptions * GetEffLenOpts() const;

    /// Perform a "deep copy" of Blast options
    /// @param opts Blast options object to copy from.
    void x_DoDeepCopy(const CBlastOptions& opts);

    /// This field is add
    CAutomaticGenCodeSingleton m_GenCodeSingletonVar;

    friend class CBl2Seq;
    friend class CDbBlast;
    friend class CDbBlastTraceback;
    friend class CDbBlastPrelim;
    friend class CEffectiveSearchSpacesMemento;
    
    // Tabular formatting thread needs to calculate parameters structures
    // and hence needs access to individual options structures.
    friend class ::CBlastTabularFormatThread; 

    /// @todo Strive to remove these classes
    friend class ::CTracebackTestFixture;    // unit test class
    friend class ::CBlastSetupTestFixture;        // unit test class
    friend class ::CUniformSearchTest;     // unit test class
    friend class ::CTracebackSearchTestFixture;   // unit test class
};

//#endif /* SKIP_DOXYGEN_PROCESSING */

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/**
  @page blast_opts_cookbook C++ BLAST Options Cookbook

  The purpose of the C++ BLAST options APIs is to provide convenient access to
  the various algorithm options for a variety of users of BLAST as well as a 
  means to validating the options, while isolating them from the details of 
  the CORE BLAST implementation. Please note that these objects are
  instantiated with the default options set and these defaults can be queried 
  via the corresponding accessor method(s).

  @section _basic_opts_usage Basic usage
  For users who only want to perform a single BLAST searches using default 
  options for a specific task (EProgram) \em without modifying the options, 
  one can let the BLAST search classes create 
  and validate the appropriate BLAST options object internally:
  
  @code
  using ncbi::blast;
  try {
      // Task is specified by the eBlastp argument
      CBl2Seq bl2seq(query, subject, eBlastp);
      TSeqAlignVector results = bl2seq.Run();
  } catch (const CBlastException& e) { 
      // Handle exception ... 
  }
  @endcode

  Using the approach above guarantees that the BLAST options will be valid.
  
  An alternative to this approach is to use the CBlastOptionsFactory to create
  a CBlastOptionsHandle object, which allows the caller to set options which
  are applicable to all variants of BLAST (e.g.: E-value threshold, effective
  search space, window size). Furthermore, this approach allows the caller to
  reuse the CBlastOptionsHandle object with multiple BLAST search objects:

  @code
  using ncbi::blast;
  CRef<CBlastOptionsHandle> opts_handle(CBlastOptionsFactory::Create(eBlastn));
  ...
  opts_handle.SetEvalueThreshold(1e-20);
  CBl2Seq bl2seq(query, subjects, opts_handle);
  ...
  opts_handle.SetEvalueThreshold(1e-10);
  CLocalBlast blast(query_factory, opts_handle, seq_src);
  @endcode

  @section _validating_opts Options validation
  The CBlastOptionsHandle classes offers a <tt>Validate</tt> method in
  its interface which is called by the BLAST search classes prior to
  performing the actual search, but users of the C++ BLAST options APIs might
  also want to invoke this method so that any exceptions thrown by the
  BLAST search classes can be guaranteed not originate from an incorrect
  setting of BLAST options. Please note that the <tt>Validate</tt> method 
  throws a CBlastException in case of failure.

  @section _intermediate_opts_usage Intermediate options usage
  For users who want to obtain default options, yet modify the most popular
  options, one should create instances of derived classes of the 
  CBlastOptionsHandle, because these should expose an interface that is 
  relevant to the task at hand (although not an exhaustive interface, for that
  see @ref _advanced_opts_usage):

  @code
  using ncbi::blast;
  CBlastNucleotideOptionsHandle opts_handle;
  opts_handle.SetTraditionalBlastnDefaults();
  opts_handle.SetStrandOption(objects::eNa_strand_plus);
  CBl2Seq bl2seq(query, subject, opts_handle);
  TSeqAlignVector results = bl2seq.Run();
  @endcode

  By using this interface, the likelihood of setting invalid options is
  reduced, but the validity of the options cannot be fully guaranteed.
  @note BLAST help desk and developers reserve the right to determine which 
  options are popular.

  @section _advanced_opts_usage Advanced options usage
  For users who want to have full control over setting the algorithm's options,
  or whose options of interest are not available in any of the classes in the
  CBlastOptionsHandle hierarchy, the <tt>GetOptions</tt> and
  <tt>SetOptions</tt> methods of the CBlastOptionsHandle hierarchy allow 
  access to the CBlastOptions class, the lowest level class in the C++ BLAST 
  options API which contains all options available to all variants of the 
  BLAST algorithm. No guarantees about the validity of the options are made 
  if this interface is used, therefore invoking <tt>Validate</tt> is 
  \em strongly recommended.

  @code
  using ncbi::blast;
  try {
      CBlastProteinOptionsHandle opts_handle;
      opts_handle.SetMatrixName("PAM30");
      opts_handle.SetGapOpeningCost(9);
      opts_handle.SetGapExtensionCost(1);
      opts_handle.SetOptions().SetCompositionBasedStats(eCompositionBasedStats);
      opts_handle.Validate();

      CBl2Seq bl2seq(query, subject, opts_handle);
      TSeqAlignVector results = bl2seq.Run();
  } catch (const CBlastException& e) {
      // Handle exception ...
  }
  @endcode

  @sa @ref blast_opts_cpp_design.

  @author Christiam Camacho <camacho@ncbi.nlm.nih.gov>
 */

/**
  @page blast_opts_cpp_design C++ BLAST Options Design

  @section _blast_opts_cpp_goals Design goals
  - Isolate C++ toolkit users from details of CORE BLAST
  - Allow easy setting of default options for common tasks for which BLAST is
    used
  - Expose in an interface only those options that are relevant to the task at
    hand
  - Provide a means of validating BLAST options
  - Allow 'power' users to have unrestricted access to all BLAST options
  - Design should be flexible to accomodate introduction/removal of options

  @section Components

  - CBlastOptionsFactory:
  This class offers a single static method to create CBlastOptionsHandle
  subclasses so that options that are applicable to all variants of BLAST can
  be inspected or modified. The actual type of the CBlastOptionsHandle returned
  by Create is determined by its EProgram argument. The return
  value of this function is guaranteed to have reasonable defaults set for the
  selected task.

  - CBlastOptionsHandle hierarchy:
  The intent of this class is to encapsulate options that are common to all
  variants of BLAST, from which more specific tasks can inherit the common
  options. The subclasses of CBlastOptionsHandle should present an interface
  that is more specific, i.e.: only contain options relevant to the task at 
  hand, although it might not be an exhaustive interface for all options 
  available for the task. Please note that the initialization of this class' 
  data members follows the template method design pattern, and this should be 
  followed by subclasses also.

  - CBlastOptions:
  This class contains all available BLAST options and it is provided to
  satisfy the design goal of allowing qualified users unrestricted access to
  all BLAST options. Because of this, it is very easy to set incorrect options,
  and hence it should be use sparingly. The use of its <tt>Validate</tt> 
  method is <em>strongly</em> recommended.

  @section _blast_opts_cpp_deficiencies Known deficiencies

  The current design in noticeably weak in fulfilling the last design goal, in
  that it uses an inheritance hierarchy of CBlastOptionsHandle classes to
  provide specific interfaces for tasks, but this approach is breaks when an
  option is applicable to a parent class and not its child.

  Furthermore, the EProgram enumeration is misnamed, as it should convey the
  notion of a task, similar to those exposed in the BLAST web page.

  @section _blast_opts_cpp_future Future plans
  A redesign of the C++ BLAST options API might be available in the future to
  overcome the deficiencies of the current APIs. Additional design goals
  include:
  - Consistent local/remote behavior
  - Provide distinction between algorithm options and application options
  - Provide well defined guarantees about the validity of BLAST options
  - <em>Easy to use correctly, difficult to use incorrectly</em>

  @author Christiam Camacho <camacho@ncbi.nlm.nih.gov>
 */

#endif  /* ALGO_BLAST_API___BLAST_OPTION__HPP */
