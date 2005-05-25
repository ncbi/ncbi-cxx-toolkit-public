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

#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/api/blast_exception.hpp>

#include <algo/blast/core/blast_options.h>

// Forward declarations of classes that need to be declared friend 
// (mostly unit test classes)
class CBlastTraceBackTest; 
class CBlastSetupTest;

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

/// Encapsulates ALL the BLAST algorithm's options.
/// @note This class provides accessors and mutators for all BLAST options 
/// without preventing the caller from setting inconsistent options.
class NCBI_XBLAST_EXPORT CBlastOptions : public CObject
{
public:
    /// Enumerates the possible contexts in which objects of this type can be
    /// used
    enum EAPILocality {
        /// To be used for running BLAST locally. 
        /// @sa CBl2Seq, CDbBlast
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
    int GetWordThreshold() const;
    /// Sets WordThreshold
    /// @param w WordThreshold [in]
    void SetWordThreshold(int w);

#ifndef SKIP_DOXYGEN_PROCESSING

    int GetLookupTableType() const;
    void SetLookupTableType(int type);

    int GetWordSize() const;
    void SetWordSize(int ws);

    /// Megablast only lookup table options
    unsigned char GetMBTemplateLength() const;
    void SetMBTemplateLength(unsigned char len);

    unsigned char GetMBTemplateType() const;
    void SetMBTemplateType(unsigned char type);

    int GetMBMaxPositions() const;
    void SetMBMaxPositions(int m);

    bool GetVariableWordSize() const;
    void SetVariableWordSize(bool val = true);

    bool GetFullByteScan() const;
    void SetFullByteScan(bool val = true);

    bool GetUsePssm() const;
    void SetUsePssm(bool m);

    /******************* Query setup options ************************/
    const char* GetFilterString() const;
    void SetFilterString(const char* f);

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

    bool GetRepeatFiltering() const;
    void SetRepeatFiltering(bool val = true);

    const char* GetRepeatFilteringDB() const;
    void SetRepeatFilteringDB(const char* db);

    objects::ENa_strand GetStrandOption() const;
    void SetStrandOption(objects::ENa_strand s);

    int GetQueryGeneticCode() const;
    void SetQueryGeneticCode(int gc);

    /******************* Initial word options ***********************/
    int GetWindowSize() const;
    void SetWindowSize(int w);

    bool GetUngappedExtension() const;
    void SetUngappedExtension(bool val = true);

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

    bool GetCompositionBasedStatsMode() const;
    void SetCompositionBasedStatsMode(bool m = true);

    bool GetSmithWatermanMode() const;
    void SetSmithWatermanMode(bool m = true);

    /******************* Hit saving options *************************/
    int GetHitlistSize() const;
    void SetHitlistSize(int s);

    int GetMaxNumHspPerSequence() const;
    void SetMaxNumHspPerSequence(int m);

    int GetCullingLimit() const;
    void SetCullingLimit(int s);

    /// Start of the region required to be part of the alignment
    int GetRequiredStart() const;
    void SetRequiredStart(int s);

    /// End of the region required to be part of the alignment
    int GetRequiredEnd() const;
    void SetRequiredEnd(int e);

    // Expect value cut-off threshold for an HSP, or a combined hit if sum
    // statistics is used
    double GetEvalueThreshold() const;
    void SetEvalueThreshold(double eval);

    // Raw score cutoff threshold
    int GetCutoffScore() const;
    void SetCutoffScore(int s);

    double GetPercentIdentity() const;
    void SetPercentIdentity(double p);

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

    /************************ Scoring options ************************/
    const char* GetMatrixName() const;
    void SetMatrixName(const char* matrix);

    const char* GetMatrixPath() const;
    void SetMatrixPath(const char* path);

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

    int GetDecline2AlignPenalty() const;
    void SetDecline2AlignPenalty(int p);

    bool GetOutOfFrameMode() const;
    void SetOutOfFrameMode(bool m = true);

    /******************** Effective Length options *******************/
    Int8 GetDbLength() const;
    void SetDbLength(Int8 l);

    unsigned int GetDbSeqNum() const;
    void SetDbSeqNum(unsigned int n);

    Int8 GetEffectiveSearchSpace() const;
    void SetEffectiveSearchSpace(Int8 eff);

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
    
    /// Auxiliary to throw CBlastExceptions
    /// @param msg message to pass in the exception [in]
    void x_Throwx(const string& msg) const;
    /// Returns QuerySetUpOptions for eLocal objects, NULL for eRemote
    /// @internal
    QuerySetUpOptions * GetQueryOpts() const;
    /// @internal Returns LookupTableOptions for eLocal objects, NULL for
    /// eRemote
    LookupTableOptions * GetLutOpts() const;
    /// @internal Returns BlastInitialWordOptions for eLocal objects, NULL for
    /// eRemote
    BlastInitialWordOptions * GetInitWordOpts() const;
    /// @internal Returns BlastExtensionOptions for eLocal objects, NULL for
    /// eRemote
    BlastExtensionOptions * GetExtnOpts() const;
    /// @internal Returns BlastHitSavingOptions for eLocal objects, NULL for
    /// eRemote
    BlastHitSavingOptions * GetHitSaveOpts() const;
    /// @internal Returns PSIBlastOptions for eLocal objects, NULL for
    /// eRemote
    PSIBlastOptions * GetPSIBlastOpts() const;
    /// @internal Returns BlastDatabaseOptions for eLocal objects, NULL for
    /// eRemote
    BlastDatabaseOptions * GetDbOpts() const;
    /// @internal Returns BlastScoringOptions for eLocal objects, NULL for
    /// eRemote
    BlastScoringOptions * GetScoringOpts() const;
    /// @internal Returns BlastEffectiveLengthsOptions for eLocal objects, 
    /// NULL for eRemote
    BlastEffectiveLengthsOptions * GetEffLenOpts() const;

    friend class CBl2Seq;
    friend class CDbBlast;
    friend class CDbBlastTraceback;
    friend class CDbBlastPrelim;

    // Tabular formatting thread needs to calculate parameters structures
    // and hence needs access to individual options structures.
    friend class ::CBlastTabularFormatThread; 

    /// @todo Strive to remove these classes
    friend class ::CBlastTraceBackTest;    // unit test class
    friend class ::CBlastSetupTest;        // unit test class
};

//#endif /* SKIP_DOXYGEN_PROCESSING */

END_SCOPE(blast)
END_NCBI_SCOPE

/**
  @page blast_opts_cookbook C++ BLAST Options Cookbook

  The purpose of the C++ BLAST options APIs is to provide convenient access to
  the various algorithm options for a variety of users of BLAST as well as a 
  means to validating the options, while isolating them from the details of 
  the CORE BLAST implementation.

  @section _basic_opts_usage Basic usage
  For users who only want to perform a single BLAST searches using default 
  options for a specific task (EProgram) \em without modifying the options, 
  one can let the @ref blast_search_classes create 
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
  CDbBlast blast(query, seq_src, opts_handle);
  @endcode

  @section _validating_opts Options validation
  The CBlastOptionsHandle classes offers a <tt>Validate</tt> method in
  its interface which is called by the @ref blast_search_classes prior to
  performing the actual search, but users of the C++ BLAST options APIs might
  also want to invoke this method so that any exceptions thrown by the @ref
  blast_search_classes can be guaranteed not originate from an incorrect
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
      opts_handle.SetOptions().SetCompositionBasedStatsMode();
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
  by Create is determined by its EProgram argument. The returned
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
/* @} */

/*
* ===========================================================================
*
* $Log$
* Revision 1.102  2005/05/25 13:09:02  camacho
* doxygen fixes
*
* Revision 1.101  2005/05/24 14:04:03  madden
* Add [GS]etSmithWatermanMode
*
* Revision 1.100  2005/05/24 12:56:57  camacho
* Doxygen fixes
*
* Revision 1.99  2005/05/16 12:25:54  madden
* Remove references to [GS]etPrelimHitlistSize
*
* Revision 1.98  2005/05/09 20:08:48  bealer
* - Add program and service strings to CBlastOptions for remote blast.
* - New CBlastOptionsHandle constructor for CRemoteBlast.
* - Prohibit copy construction/assignment for CRemoteBlast.
* - Code in each BlastOptionsHandle derived class to set program+service.
*
* Revision 1.97  2005/04/28 19:37:49  camacho
* Added design docs and cookbook
*
* Revision 1.96  2005/04/27 14:41:53  papadopo
* 1. Remove friendship for CRPSTest
* 2. Modify options to set/get HSP culling limit
*
* Revision 1.95  2005/04/25 14:47:48  madden
* Remove CScoreBlkTest as friend
*
* Revision 1.94  2005/04/11 16:56:10  dondosha
* Removed unused forward declaration of CBlastEngineTest class
*
* Revision 1.93  2005/04/11 14:29:04  madden
* Remove CBlastGapAlignTest as friend class
*
* Revision 1.92  2005/04/06 19:44:32  dondosha
* CBlastEngineTest unit test class does not need friendship any more
*
* Revision 1.91  2005/03/31 20:45:49  camacho
* Remove friend declaration for CBlastRedoAlignmentTest
*
* Revision 1.90  2005/03/31 13:43:49  camacho
* BLAST options API clean-up
*
* Revision 1.89  2005/03/22 19:00:20  madden
* NULL out repeatFilterOptions upon free to prevent double free
*
* Revision 1.88  2005/03/10 14:21:55  madden
* Do not set repeatFilterOptions for non-blastn jobs when parsing filter_string
*
* Revision 1.87  2005/03/10 13:17:27  madden
* Changed type from short to int for [GS]etPseudoCount
*
* Revision 1.86  2005/03/02 16:45:24  camacho
* Remove use_real_db_size
*
* Revision 1.85  2005/03/02 13:56:49  madden
* Rename Filtering option funcitons to standard naming convention
*
* Revision 1.84  2005/02/24 13:46:20  madden
* Add setters and getters for filtering options
*
* Revision 1.83  2005/01/11 17:49:37  dondosha
* Removed total HSP limit option
*
* Revision 1.82  2005/01/10 13:29:25  madden
* Remove [GS]etAlphabetSize as well as [GS]etScanStep
* Remove [GS]etSeedContainerType as well as [GS]etSeedExtensionMethod
* Add [GS]etFullByteScan
*
* Revision 1.81  2004/12/28 16:42:19  camacho
* Consistently use the RAII idiom for C structures using wrapper classes in CBlastOptions
*
* Revision 1.80  2004/12/28 13:36:17  madden
* [GS]etWordSize is now an int rather than a short
*
* Revision 1.79  2004/12/21 17:14:35  dondosha
* Added CBlastEngineTest friend class for new unit tests
*
* Revision 1.78  2004/12/21 15:12:19  dondosha
* Added friend class for blastsetup unit test
*
* Revision 1.77  2004/12/20 20:10:55  camacho
* + option to set composition based statistics
* + option to use pssm in lookup table
*
* Revision 1.76  2004/11/02 18:25:49  madden
* Move gap_trigger to m_InitWordOpts
*
* Revision 1.75  2004/09/08 18:32:23  camacho
* Doxygen fixes
*
* Revision 1.74  2004/09/07 17:59:12  dondosha
* CDbBlast class changed to support multi-threaded search
*
* Revision 1.73  2004/08/30 16:53:23  dondosha
* Added E in front of enum names ESeedExtensionMethod and ESeedContainerType
*
* Revision 1.72  2004/08/26 20:54:56  dondosha
* Removed a debugging printout in x_Throwx method
*
* Revision 1.71  2004/08/18 18:13:42  camacho
* Remove GetProgramFromBlastProgramType, add ProgramNameToEnum
*
* Revision 1.70  2004/08/16 19:46:05  dondosha
* Reflect in comments that uneven gap linking works now not just for tblastn
*
* Revision 1.69  2004/07/08 20:20:02  gorelenk
* Added missed export spec. to GetProgramFromBlastProgramType
*
* Revision 1.68  2004/07/06 15:45:47  dondosha
* Added GetProgramType method
*
* Revision 1.67  2004/06/15 18:45:46  dondosha
* Added friend classes CDbBlastTraceback and CBlastTabularFormatThread
*
* Revision 1.66  2004/06/08 21:16:52  bealer
* - Remote EAPILocality from CBlastOptionsLocal
*
* Revision 1.65  2004/06/08 17:56:14  dondosha
* Removed neighboring mode declarations, getters and setters
*
* Revision 1.64  2004/06/08 15:18:47  dondosha
* Skip traceback option has been moved to the traceback extension method enum
*
* Revision 1.63  2004/06/08 14:58:57  dondosha
* Removed is_neighboring option; let application set min_hit_length and percent_identity options instead
*
* Revision 1.62  2004/06/07 15:45:53  dondosha
* do_sum_stats hit saving option is now an enum
*
* Revision 1.61  2004/05/19 14:52:00  camacho
* 1. Added doxygen tags to enable doxygen processing of algo/blast/core
* 2. Standardized copyright, CVS $Id string, $Log and rcsid formatting and i
*    location
* 3. Added use of @todo doxygen keyword
*
* Revision 1.60  2004/05/18 13:20:35  madden
* Add CBlastRedoAlignmentTest as friend class
*
* Revision 1.59  2004/05/18 12:48:24  madden
* Add setter and getter for GapTracebackAlgorithm (EBlastTbackExt)
*
* Revision 1.58  2004/05/17 18:07:19  bealer
* - Add PSI Blast support.
*
* Revision 1.57  2004/05/17 15:28:24  madden
* Int algorithm_type replaced with enum EBlastPrelimGapExt
*
* Revision 1.56  2004/04/07 15:11:33  papadopo
* add RPS unit test as friend class
*
* Revision 1.55  2004/03/30 15:47:37  madden
* Add CScoreBlkTest as friend class
*
* Revision 1.54  2004/03/19 18:56:04  camacho
* Move to doxygen AlgoBlast group
*
* Revision 1.53  2004/03/09 18:41:06  dondosha
* Removed single hsp cutoff evalue and score, since these are calculated, and not set by user
*
* Revision 1.52  2004/02/27 19:44:38  madden
* Add  CBlastTraceBackTest (unit test) as friend class
*
* Revision 1.51  2004/02/24 23:22:59  bealer
* - Fix glitch in Validate().
*
* Revision 1.50  2004/02/24 22:42:11  bealer
* - Remove undefined methods from CBlastOptionsRemote.
*
* Revision 1.49  2004/02/24 13:16:35  dondosha
* Commented out argument names in unimplemented function, to eliminate compiler warnings
*
* Revision 1.48  2004/02/20 19:54:26  camacho
* Correct friendship declarations for unit test classes
*
* Revision 1.47  2004/02/17 23:52:08  dondosha
* Added methods to get/set preliminary hitlist size
*
* Revision 1.46  2004/02/10 19:46:19  dondosha
* Added friend class CBlastDbTest
*
* Revision 1.45  2004/01/20 17:54:50  bealer
* - Add SkipTraceback option.
*
* Revision 1.44  2004/01/20 17:06:39  camacho
* Made operator== a member function
*
* Revision 1.43  2004/01/20 16:42:16  camacho
* Do not use runtime_error, use NCBI Exception classes
*
* Revision 1.42  2004/01/17 23:03:41  dondosha
* There is no LCaseMask option any more, so [SG]EtLCaseMask methods removed
*
* Revision 1.41  2004/01/17 02:38:23  ucko
* Initialize variables with = rather than () when possible to avoid
* confusing MSVC's parser.
*
* Revision 1.40  2004/01/17 00:52:18  ucko
* Remove excess comma at the end of EBlastOptIdx (problematic on WorkShop)
*
* Revision 1.39  2004/01/17 00:15:06  ucko
* Substitute Int8 for non-portable "long long"
*
* Revision 1.38  2004/01/16 21:40:01  bealer
* - Add Blast4 API support to the CBlastOptions class.
*
* Revision 1.37  2004/01/13 14:54:54  dondosha
* Grant friendship to class CBlastGapAlignTest for gapped alignment unit test
*
* Revision 1.36  2003/12/17 21:09:33  camacho
* Add comments to reward/mismatch; gap open/extension costs
*
* Revision 1.35  2003/12/03 16:34:09  dondosha
* Added setter for skip_traceback option; changed SetDbGeneticCode so it fills both integer and string
*
* Revision 1.34  2003/11/26 18:22:14  camacho
* +Blast Option Handle classes
*
* Revision 1.33  2003/11/12 18:41:02  camacho
* Remove side effects from mutators
*
* Revision 1.32  2003/11/04 17:13:01  dondosha
* Set boolean is_ooframe option when needed
*
* Revision 1.31  2003/10/30 19:37:01  dondosha
* Removed gapped_calculation from BlastHitSavingOptions structure
*
* Revision 1.30  2003/10/21 22:15:33  camacho
* Rearranging of C options structures, fix seed extension method
*
* Revision 1.29  2003/10/21 17:31:06  camacho
* Renaming of gap open/extension accessors/mutators
*
* Revision 1.28  2003/10/21 15:36:25  camacho
* Remove unnecessary side effect when setting frame shift penalty
*
* Revision 1.27  2003/10/17 18:43:14  dondosha
* Use separate variables for different initial word extension options
*
* Revision 1.26  2003/10/07 17:27:38  dondosha
* Lower case mask removed from options, added to the SSeqLoc structure
*
* Revision 1.25  2003/09/26 15:42:42  dondosha
* Added second argument to SetExtendWordMethod, so bit can be set or unset
*
* Revision 1.24  2003/09/25 15:25:22  dondosha
* Set phi_align in hit saving options for PHI BLAST
*
* Revision 1.23  2003/09/11 17:44:39  camacho
* Changed CBlastOption -> CBlastOptions
*
* Revision 1.22  2003/09/09 22:07:57  dondosha
* Added accessor functions for PHI pattern
*
* Revision 1.21  2003/09/03 19:35:51  camacho
* Removed unneeded prototype
*
* Revision 1.20  2003/08/28 22:32:53  camacho
* Correct typo
*
* Revision 1.19  2003/08/21 19:30:17  dondosha
* Free previous value of gen_code_string and allocate memory for new one in SetDbGeneticCodeStr
*
* Revision 1.18  2003/08/19 22:11:16  dondosha
* Cosmetic changes
*
* Revision 1.17  2003/08/19 20:22:05  dondosha
* EProgram definition moved from CBlastOptions clase to blast scope
*
* Revision 1.16  2003/08/19 13:45:21  dicuccio
* Removed 'USING_SCOPE(objects)'.  Changed #include guards to be standards
* compliant.  Added 'objects::' where necessary.
*
* Revision 1.15  2003/08/18 20:58:56  camacho
* Added blast namespace, removed *__.hpp includes
*
* Revision 1.14  2003/08/14 19:06:51  dondosha
* Added BLASTGetEProgram function to convert from Uint1 to enum type
*
* Revision 1.13  2003/08/11 19:55:04  camacho
* Early commit to support query concatenation and the use of multiple scopes.
* Compiles, but still needs work.
*
* Revision 1.12  2003/08/11 15:23:23  dondosha
* Renamed conversion functions between BlastMask and CSeqLoc; added algo/blast/core to headers from core BLAST library
*
* Revision 1.11  2003/08/11 13:58:51  dicuccio
* Added export specifiers.  Fixed problem with unimplemented private copy ctor
* (truly make unimplemented)
*
* Revision 1.10  2003/08/08 19:42:14  dicuccio
* Compilation fixes: #include file relocation; fixed use of 'list' and 'vector'
* as variable names
*
* Revision 1.9  2003/08/01 22:34:11  camacho
* Added accessors/mutators/defaults for matrix_path
*
* Revision 1.8  2003/07/31 19:45:33  camacho
* Eliminate Ptr notation
*
* Revision 1.7  2003/07/30 19:56:19  coulouri
* remove matrixname
*
* Revision 1.6  2003/07/30 15:00:01  camacho
* Do not use Malloc/MemNew/MemFree
*
* Revision 1.5  2003/07/30 13:55:09  coulouri
* use strdup()
*
* Revision 1.4  2003/07/23 21:29:37  camacho
* Update BlastDatabaseOptions
*
* Revision 1.3  2003/07/16 19:51:12  camacho
* Removed logic of default setting from mutator member functions
*
* Revision 1.2  2003/07/14 22:17:17  camacho
* Convert CSeq_loc to BlastMaskPtr
*
* Revision 1.1  2003/07/10 18:34:19  camacho
* Initial revision
*
*
* ===========================================================================
*/

#endif  /* ALGO_BLAST_API___BLAST_OPTION__HPP */
