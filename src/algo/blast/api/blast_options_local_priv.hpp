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

/** @file blast_options_local_priv.hpp
 * Private header for local representation of BLAST options.
 */

#ifndef ALGO_BLAST_API___BLAST_OPTIONS_LOCAL_PRIV__HPP
#define ALGO_BLAST_API___BLAST_OPTIONS_LOCAL_PRIV__HPP

#include <objects/seqloc/Na_strand.hpp>
#include <algo/blast/api/blast_types.hpp>
#include <algo/blast/api/blast_aux.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

#ifndef SKIP_DOXYGEN_PROCESSING

// Forward declarations
class CBlastOptionsMemento;

/// Encapsulates all blast input parameters
class NCBI_XBLAST_EXPORT CBlastOptionsLocal : public CObject
{
public:
    CBlastOptionsLocal();
    ~CBlastOptionsLocal();

    /// Validate the options
    bool Validate() const;

    /// Accessors/Mutators for individual options
    
    EProgram GetProgram() const;
    EBlastProgramType GetProgramType() const;
    void SetProgram(EProgram p);

    /******************* Lookup table options ***********************/
    int GetWordThreshold() const;
    void SetWordThreshold(int w);

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

    bool GetFullByteScan() const;
    void SetFullByteScan(bool val = true);

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

    int GetLongestIntronLength() const; // for linking HSPs with uneven gaps
    void SetLongestIntronLength(int l); // for linking HSPs with uneven gaps

    /// Returns true if gapped BLAST is set, false otherwise
    bool GetGappedMode() const;
    void SetGappedMode(bool m = true);

    double GetGapTrigger() const;
    void SetGapTrigger(double g);

    /************************ Scoring options ************************/
    const char* GetMatrixName() const;
    void SetMatrixName(const char* matrix);

    const char* GetMatrixPath() const;
    void SetMatrixPath(const char* path);

    int GetMatchReward() const;
    void SetMatchReward(int r);         // r should be a positive integer

    int GetMismatchPenalty() const;
    void SetMismatchPenalty(int p);     // p should be a negative integer

    int GetGapOpeningCost() const;
    void SetGapOpeningCost(int g);      // g should be a positive integer

    int GetGapExtensionCost() const;
    void SetGapExtensionCost(int e);    // e should be a positive integer

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

   //const unsigned char* GetDbGeneticCodeStr() const;
   //void SetDbGeneticCodeStr(const unsigned char* gc_str);

    // Set both integer and string genetic code in one call
    void SetDbGeneticCode(int gc);

    /// @todo PSI-Blast options could go on their own subclass?
    const char* GetPHIPattern() const;
    void SetPHIPattern(const char* pattern, bool is_dna);

    /// Allows to dump a snapshot of the object
    void DebugDump(CDebugDumpContext ddc, unsigned int depth) const;
    
    /******************** PSIBlast options *******************/
    double GetInclusionThreshold() const;
    void SetInclusionThreshold(double incthr);
    
    int GetPseudoCount() const;
    void SetPseudoCount(int ps);
    
    bool operator==(const CBlastOptionsLocal& rhs) const;
    bool operator!=(const CBlastOptionsLocal& rhs) const;

private:

    /// Query sequence settings
    CQuerySetUpOptions            m_QueryOpts;

    /// Lookup table settings
    CLookupTableOptions           m_LutOpts;

    /// Word settings 
    CBlastInitialWordOptions      m_InitWordOpts;

    /// Hit extension settings
    CBlastExtensionOptions        m_ExtnOpts;

    /// Hit saving settings
    CBlastHitSavingOptions        m_HitSaveOpts;

    /// PSI-Blast settings
    CPSIBlastOptions              m_PSIBlastOpts;

    /// Blast database settings
    CBlastDatabaseOptions         m_DbOpts;

    /// Scoring options
    CBlastScoringOptions          m_ScoringOpts;

    /// Effective lengths options
    CBlastEffectiveLengthsOptions m_EffLenOpts;

    /// Blast program
    EProgram                             m_Program;

    /// Prohibit copy c-tor 
    CBlastOptionsLocal(const CBlastOptionsLocal& bo);
    /// Prohibit assignment operator
    CBlastOptionsLocal& operator=(const CBlastOptionsLocal& bo);

    friend class CBlastOptions;

    /// Friend class which allows extraction of this class' data members for
    /// internal use in the C++ API APIs
    friend class CBlastOptionsMemento;
    
    /// @internal
    QuerySetUpOptions * GetQueryOpts() const
    {
        return m_QueryOpts;
    }
    
    /// @internal
    LookupTableOptions * GetLutOpts() const
    {
        return m_LutOpts;
    }
    
    /// @internal
    BlastInitialWordOptions * GetInitWordOpts() const
    {
        return m_InitWordOpts;
    }
    
    /// @internal
    BlastExtensionOptions * GetExtnOpts() const
    {
        return m_ExtnOpts;
    }
    
    /// @internal
    BlastHitSavingOptions * GetHitSaveOpts() const
    {
        return m_HitSaveOpts;
    }
    
    /// @internal
    PSIBlastOptions * GetPSIBlastOpts() const
    {
        return m_PSIBlastOpts;
    }
    
    /// @internal
    BlastDatabaseOptions * GetDbOpts() const
    {
        return m_DbOpts;
    }
    
    /// @internal
    BlastScoringOptions * GetScoringOpts() const
    {
        return m_ScoringOpts;
    }
    
    /// @internal
    BlastEffectiveLengthsOptions * GetEffLenOpts() const
    {
        return m_EffLenOpts;
    }
};

inline EProgram
CBlastOptionsLocal::GetProgram() const
{
    return m_Program;
}

inline void
CBlastOptionsLocal::SetProgram(EProgram p)
{
    _ASSERT(p >= eBlastn && p < eBlastProgramMax);
    m_Program = p;
}

inline const char*
CBlastOptionsLocal::GetMatrixName() const
{
    return m_ScoringOpts->matrix;
}

inline void
CBlastOptionsLocal::SetMatrixName(const char* matrix)
{
    if (!matrix)
        return;

    sfree(m_ScoringOpts->matrix);
    m_ScoringOpts->matrix = strdup(matrix);
}

inline const char* 
CBlastOptionsLocal::GetMatrixPath() const
{
    return m_ScoringOpts->matrix_path;
}

inline void 
CBlastOptionsLocal::SetMatrixPath(const char* path)
{
    if (!path)
        return;

    sfree(m_ScoringOpts->matrix_path);
    m_ScoringOpts->matrix_path = strdup(path);
}

inline int
CBlastOptionsLocal::GetWordThreshold() const
{
    return m_LutOpts->threshold;
}

inline void
CBlastOptionsLocal::SetWordThreshold(int w)
{
    m_LutOpts->threshold = w;
}

inline int
CBlastOptionsLocal::GetLookupTableType() const
{
    return m_LutOpts->lut_type;
}

inline void
CBlastOptionsLocal::SetLookupTableType(int type)
{
    m_LutOpts->lut_type = type;
    if (type == MB_LOOKUP_TABLE) {
       m_LutOpts->max_positions = INT4_MAX;
       m_LutOpts->word_size = BLAST_WORDSIZE_MEGABLAST;
    } 
}

inline int
CBlastOptionsLocal::GetWordSize() const
{
    return m_LutOpts->word_size;
}

inline void
CBlastOptionsLocal::SetWordSize(int ws)
{
    m_LutOpts->word_size = ws;
}

inline unsigned char
CBlastOptionsLocal::GetMBTemplateLength() const
{
    return m_LutOpts->mb_template_length;
}

inline void
CBlastOptionsLocal::SetMBTemplateLength(unsigned char len)
{
    m_LutOpts->mb_template_length = len;
}

inline unsigned char
CBlastOptionsLocal::GetMBTemplateType() const
{
    return m_LutOpts->mb_template_type;
}

inline void
CBlastOptionsLocal::SetMBTemplateType(unsigned char type)
{
    m_LutOpts->mb_template_type = type;
}

inline int
CBlastOptionsLocal::GetMBMaxPositions() const
{
    return m_LutOpts->max_positions;
}

inline void
CBlastOptionsLocal::SetMBMaxPositions(int m)
{
    m_LutOpts->max_positions = m;
}

inline bool
CBlastOptionsLocal::GetFullByteScan() const
{
    return m_LutOpts->full_byte_scan ? true: false;
}

inline void
CBlastOptionsLocal::SetFullByteScan(bool val)
{
    m_LutOpts->full_byte_scan = val;
}

/******************* Query setup options ************************/
inline const char*
CBlastOptionsLocal::GetFilterString() const
{
    return m_QueryOpts->filter_string;
}
inline void
CBlastOptionsLocal::SetFilterString(const char* f)
{
    if (!f)
        return;

    sfree(m_QueryOpts->filter_string);
    m_QueryOpts->filter_string = strdup(f);

    m_QueryOpts->filtering_options = 
            SBlastFilterOptionsFree(m_QueryOpts->filtering_options);

    BlastFilteringOptionsFromString(GetProgramType(), f, 
            &(m_QueryOpts->filtering_options), NULL);

   // Repeat filtering is only allowed for blastn.
   if (GetProgramType() != eBlastTypeBlastn && 
            m_QueryOpts->filtering_options->repeatFilterOptions)
         m_QueryOpts->filtering_options->repeatFilterOptions =
            SRepeatFilterOptionsFree(m_QueryOpts->filtering_options->repeatFilterOptions);

   return;
}

inline bool
CBlastOptionsLocal::GetMaskAtHash() const
{
    if (m_QueryOpts->filtering_options->mask_at_hash)
       return true;
    else
       return false;
}
inline void
CBlastOptionsLocal::SetMaskAtHash(bool val)
{

   m_QueryOpts->filtering_options->mask_at_hash = val;

   return;
}

inline bool
CBlastOptionsLocal::GetDustFiltering() const
{
    if (m_QueryOpts->filtering_options->dustOptions)
       return true;
    else
       return false;
}
inline void
CBlastOptionsLocal::SetDustFiltering(bool val)
{

   if (m_QueryOpts->filtering_options->dustOptions)  // free previous structure so we provide defaults.
        m_QueryOpts->filtering_options->dustOptions = 
             SDustOptionsFree(m_QueryOpts->filtering_options->dustOptions);
     
   if (val == false)  // filtering should be turned off
       return;

   SDustOptionsNew(&(m_QueryOpts->filtering_options->dustOptions));

   return;
}

inline int
CBlastOptionsLocal::GetDustFilteringLevel() const
{
    if (m_QueryOpts->filtering_options->dustOptions == NULL)
       SDustOptionsNew(&(m_QueryOpts->filtering_options->dustOptions));

    return m_QueryOpts->filtering_options->dustOptions->level;
}
inline void
CBlastOptionsLocal::SetDustFilteringLevel(int level)
{
    if (m_QueryOpts->filtering_options->dustOptions == NULL)
       SDustOptionsNew(&(m_QueryOpts->filtering_options->dustOptions)); 
      
    m_QueryOpts->filtering_options->dustOptions->level = level;

    return;
}
inline int
CBlastOptionsLocal::GetDustFilteringWindow() const
{
    if (m_QueryOpts->filtering_options->dustOptions == NULL)
       SDustOptionsNew(&(m_QueryOpts->filtering_options->dustOptions));

    return m_QueryOpts->filtering_options->dustOptions->window;
}

inline void
CBlastOptionsLocal::SetDustFilteringWindow(int window)
{
    if (m_QueryOpts->filtering_options->dustOptions == NULL)
       SDustOptionsNew(&(m_QueryOpts->filtering_options->dustOptions)); 
      
    m_QueryOpts->filtering_options->dustOptions->window = window;

    return;
}
inline int
CBlastOptionsLocal::GetDustFilteringLinker() const
{
    if (m_QueryOpts->filtering_options->dustOptions == NULL)
       SDustOptionsNew(&(m_QueryOpts->filtering_options->dustOptions));

    return m_QueryOpts->filtering_options->dustOptions->linker;
}

inline void
CBlastOptionsLocal::SetDustFilteringLinker(int linker)
{
    if (m_QueryOpts->filtering_options->dustOptions == NULL)
       SDustOptionsNew(&(m_QueryOpts->filtering_options->dustOptions)); 
      
    m_QueryOpts->filtering_options->dustOptions->linker = linker;

    return;
}

inline bool
CBlastOptionsLocal::GetSegFiltering() const
{
    if (m_QueryOpts->filtering_options->segOptions)
      return true;
    else
      return false;
}

inline void
CBlastOptionsLocal::SetSegFiltering(bool val)
{

   if (m_QueryOpts->filtering_options->segOptions)  // free previous structure so we provide defaults.
        m_QueryOpts->filtering_options->segOptions = 
             SSegOptionsFree(m_QueryOpts->filtering_options->segOptions);
     
   if (val == false)  // filtering should be turned off
       return;

   SSegOptionsNew(&(m_QueryOpts->filtering_options->segOptions));

   return;
}

inline int
CBlastOptionsLocal::GetSegFilteringWindow() const
{
    if (m_QueryOpts->filtering_options->segOptions == NULL)
       SSegOptionsNew(&(m_QueryOpts->filtering_options->segOptions)); 
      
    return m_QueryOpts->filtering_options->segOptions->window;
}

inline void
CBlastOptionsLocal::SetSegFilteringWindow(int window)
{
    if (m_QueryOpts->filtering_options->segOptions == NULL)
       SSegOptionsNew(&(m_QueryOpts->filtering_options->segOptions)); 
      
    m_QueryOpts->filtering_options->segOptions->window = window;

    return;
}

inline double
CBlastOptionsLocal::GetSegFilteringLocut() const
{
    if (m_QueryOpts->filtering_options->segOptions == NULL)
       SSegOptionsNew(&(m_QueryOpts->filtering_options->segOptions)); 
      
    return m_QueryOpts->filtering_options->segOptions->locut;
}

inline void
CBlastOptionsLocal::SetSegFilteringLocut(double locut)
{
    if (m_QueryOpts->filtering_options->segOptions == NULL)
       SSegOptionsNew(&(m_QueryOpts->filtering_options->segOptions)); 
      
    m_QueryOpts->filtering_options->segOptions->locut = locut;

    return;
}

inline double
CBlastOptionsLocal::GetSegFilteringHicut() const
{
    if (m_QueryOpts->filtering_options->segOptions == NULL)
       SSegOptionsNew(&(m_QueryOpts->filtering_options->segOptions)); 
      
    return m_QueryOpts->filtering_options->segOptions->hicut;
}

inline void
CBlastOptionsLocal::SetSegFilteringHicut(double hicut)
{
    if (m_QueryOpts->filtering_options->segOptions == NULL)
       SSegOptionsNew(&(m_QueryOpts->filtering_options->segOptions)); 
      
    m_QueryOpts->filtering_options->segOptions->hicut = hicut;

    return;
}

inline bool
CBlastOptionsLocal::GetRepeatFiltering() const
{
    if (m_QueryOpts->filtering_options->repeatFilterOptions)
      return true;
    else
      return false;
}

inline void
CBlastOptionsLocal::SetRepeatFiltering(bool val)
{

   if (m_QueryOpts->filtering_options->repeatFilterOptions)  // free previous structure so we provide defaults.
        m_QueryOpts->filtering_options->repeatFilterOptions = 
             SRepeatFilterOptionsFree(m_QueryOpts->filtering_options->repeatFilterOptions);
     
   if (val == false)  // filtering should be turned off
       return;

   SRepeatFilterOptionsNew(&(m_QueryOpts->filtering_options->repeatFilterOptions));

   return;
}

inline const char*
CBlastOptionsLocal::GetRepeatFilteringDB() const
{
    if (m_QueryOpts->filtering_options->repeatFilterOptions == NULL)
      SRepeatFilterOptionsNew(&(m_QueryOpts->filtering_options->repeatFilterOptions));

    return m_QueryOpts->filtering_options->repeatFilterOptions->database;
}

inline void
CBlastOptionsLocal::SetRepeatFilteringDB(const char* db)
{

   if (!db)
      return;

   SRepeatFilterOptionsResetDB(&(m_QueryOpts->filtering_options->repeatFilterOptions), db);

   return;
}

inline objects::ENa_strand
CBlastOptionsLocal::GetStrandOption() const
{
    return (objects::ENa_strand) m_QueryOpts->strand_option;
}

inline void
CBlastOptionsLocal::SetStrandOption(objects::ENa_strand s)
{
    m_QueryOpts->strand_option = (unsigned char) s;
}

inline int
CBlastOptionsLocal::GetQueryGeneticCode() const
{
    return m_QueryOpts->genetic_code;
}

inline void
CBlastOptionsLocal::SetQueryGeneticCode(int gc)
{
    m_QueryOpts->genetic_code = gc;
}

/******************* Initial word options ***********************/
inline int
CBlastOptionsLocal::GetWindowSize() const
{
    return m_InitWordOpts->window_size;
}

inline void
CBlastOptionsLocal::SetWindowSize(int s)
{
    m_InitWordOpts->window_size = s;
}

inline bool
CBlastOptionsLocal::GetUngappedExtension() const
{
    return m_InitWordOpts->ungapped_extension ? true : false;
}

inline void 
CBlastOptionsLocal::SetUngappedExtension(bool val)
{
    m_InitWordOpts->ungapped_extension = val;
}

inline double
CBlastOptionsLocal::GetXDropoff() const
{
    return m_InitWordOpts->x_dropoff;
}

inline void
CBlastOptionsLocal::SetXDropoff(double x)
{
    m_InitWordOpts->x_dropoff = x;
}

inline double
CBlastOptionsLocal::GetGapTrigger() const
{
    return m_InitWordOpts->gap_trigger;
}

inline void
CBlastOptionsLocal::SetGapTrigger(double g)
{
    m_InitWordOpts->gap_trigger = g;
}

/******************* Gapped extension options *******************/
inline double
CBlastOptionsLocal::GetGapXDropoff() const
{
    return m_ExtnOpts->gap_x_dropoff;
}

inline void
CBlastOptionsLocal::SetGapXDropoff(double x)
{
    m_ExtnOpts->gap_x_dropoff = x;
}

inline double
CBlastOptionsLocal::GetGapXDropoffFinal() const
{
    return m_ExtnOpts->gap_x_dropoff_final;
}

inline void
CBlastOptionsLocal::SetGapXDropoffFinal(double x)
{
    m_ExtnOpts->gap_x_dropoff_final = x;
}

inline EBlastPrelimGapExt
CBlastOptionsLocal::GetGapExtnAlgorithm() const
{
    return m_ExtnOpts->ePrelimGapExt;
}

inline void
CBlastOptionsLocal::SetGapExtnAlgorithm(EBlastPrelimGapExt a)
{
    m_ExtnOpts->ePrelimGapExt = a;
}

inline EBlastTbackExt
CBlastOptionsLocal::GetGapTracebackAlgorithm() const
{
    return m_ExtnOpts->eTbackExt;
}

inline void
CBlastOptionsLocal::SetGapTracebackAlgorithm(EBlastTbackExt a)
{
    m_ExtnOpts->eTbackExt = a;
}

inline bool
CBlastOptionsLocal::GetCompositionBasedStatsMode() const
{
    return m_ExtnOpts->compositionBasedStats ? true : false;
}

inline void
CBlastOptionsLocal::SetCompositionBasedStatsMode(bool m)
{
    m_ExtnOpts->compositionBasedStats = m;
}

inline bool
CBlastOptionsLocal::GetSmithWatermanMode() const
{
    if (m_ExtnOpts->eTbackExt == eSmithWatermanTbck)
        return true;
    else
        return false;
}

inline void
CBlastOptionsLocal::SetSmithWatermanMode(bool m)
{
    if (m == true)
       m_ExtnOpts->eTbackExt = eSmithWatermanTbck;
    else
       m_ExtnOpts->eTbackExt = eDynProgTbck;
}

/******************* Hit saving options *************************/
inline int
CBlastOptionsLocal::GetHitlistSize() const
{
    return m_HitSaveOpts->hitlist_size;
}

inline void
CBlastOptionsLocal::SetHitlistSize(int s)
{
    m_HitSaveOpts->hitlist_size = s;
}

inline int
CBlastOptionsLocal::GetMaxNumHspPerSequence() const
{
    return m_HitSaveOpts->hsp_num_max;
}

inline void
CBlastOptionsLocal::SetMaxNumHspPerSequence(int m)
{
    m_HitSaveOpts->hsp_num_max = m;
}

inline int
CBlastOptionsLocal::GetCullingLimit() const
{
    return m_HitSaveOpts->culling_limit;
}

inline void
CBlastOptionsLocal::SetCullingLimit(int s)
{
    m_HitSaveOpts->culling_limit = s;
}

inline double
CBlastOptionsLocal::GetEvalueThreshold() const
{
    return m_HitSaveOpts->expect_value;
}

inline void
CBlastOptionsLocal::SetEvalueThreshold(double eval)
{
    m_HitSaveOpts->expect_value = eval;
}

inline int
CBlastOptionsLocal::GetCutoffScore() const
{
    return m_HitSaveOpts->cutoff_score;
}

inline void
CBlastOptionsLocal::SetCutoffScore(int s)
{
    m_HitSaveOpts->cutoff_score = s;
}

inline double
CBlastOptionsLocal::GetPercentIdentity() const
{
    return m_HitSaveOpts->percent_identity;
}

inline void
CBlastOptionsLocal::SetPercentIdentity(double p)
{
    m_HitSaveOpts->percent_identity = p;
}

inline int
CBlastOptionsLocal::GetMinDiagSeparation() const
{
    return m_HitSaveOpts->min_diag_separation;
}

inline void
CBlastOptionsLocal::SetMinDiagSeparation(int d)
{
    m_HitSaveOpts->min_diag_separation = d;
}

inline bool
CBlastOptionsLocal::GetSumStatisticsMode() const
{
    if (m_HitSaveOpts->do_sum_stats == eSumStatsTrue)
        return true;
    else 
        return false;
}

inline void
CBlastOptionsLocal::SetSumStatisticsMode(bool m)
{
    if (m)
        m_HitSaveOpts->do_sum_stats = eSumStatsTrue;
    else
        m_HitSaveOpts->do_sum_stats = eSumStatsFalse;
}

inline int
CBlastOptionsLocal::GetLongestIntronLength() const
{
    return m_HitSaveOpts->longest_intron;
}

inline void
CBlastOptionsLocal::SetLongestIntronLength(int l)
{
    m_HitSaveOpts->longest_intron = l;
}

inline bool
CBlastOptionsLocal::GetGappedMode() const
{
    return m_ScoringOpts->gapped_calculation ? true : false;
}

inline void
CBlastOptionsLocal::SetGappedMode(bool m)
{
    m_ScoringOpts->gapped_calculation = m;
}

/************************ Scoring options ************************/
inline int 
CBlastOptionsLocal::GetMatchReward() const
{
    return m_ScoringOpts->reward;
}

inline void 
CBlastOptionsLocal::SetMatchReward(int r)
{
    m_ScoringOpts->reward = r;
}

inline int 
CBlastOptionsLocal::GetMismatchPenalty() const
{
    return m_ScoringOpts->penalty;
}

inline void 
CBlastOptionsLocal::SetMismatchPenalty(int p)
{
    m_ScoringOpts->penalty = p;
}

inline int 
CBlastOptionsLocal::GetGapOpeningCost() const
{
    return m_ScoringOpts->gap_open;
}

inline void 
CBlastOptionsLocal::SetGapOpeningCost(int g)
{
    m_ScoringOpts->gap_open = g;
}

inline int 
CBlastOptionsLocal::GetGapExtensionCost() const
{
    return m_ScoringOpts->gap_extend;
}

inline void 
CBlastOptionsLocal::SetGapExtensionCost(int e)
{
    m_ScoringOpts->gap_extend = e;
}

inline int 
CBlastOptionsLocal::GetFrameShiftPenalty() const
{
    return m_ScoringOpts->shift_pen;
}

inline void 
CBlastOptionsLocal::SetFrameShiftPenalty(int p)
{
    m_ScoringOpts->shift_pen = p;
}

inline int 
CBlastOptionsLocal::GetDecline2AlignPenalty() const
{
    return m_ScoringOpts->decline_align;
}

inline void 
CBlastOptionsLocal::SetDecline2AlignPenalty(int p)
{
    m_ScoringOpts->decline_align = p;
}

inline bool 
CBlastOptionsLocal::GetOutOfFrameMode() const
{
    return m_ScoringOpts->is_ooframe ? true : false;
}

inline void 
CBlastOptionsLocal::SetOutOfFrameMode(bool m)
{
    m_ScoringOpts->is_ooframe = m;
}

/******************** Effective Length options *******************/
inline Int8 
CBlastOptionsLocal::GetDbLength() const
{
    return m_EffLenOpts->db_length;
}

inline void 
CBlastOptionsLocal::SetDbLength(Int8 l)
{
    m_EffLenOpts->db_length = l;
}

inline unsigned int 
CBlastOptionsLocal::GetDbSeqNum() const
{
    return (unsigned int) m_EffLenOpts->dbseq_num;
}

inline void 
CBlastOptionsLocal::SetDbSeqNum(unsigned int n)
{
    m_EffLenOpts->dbseq_num = (Int4) n;
}

inline Int8 
CBlastOptionsLocal::GetEffectiveSearchSpace() const
{
    return m_EffLenOpts->searchsp_eff;
}
 
inline void 
CBlastOptionsLocal::SetEffectiveSearchSpace(Int8 eff)
{
    m_EffLenOpts->searchsp_eff = eff;
}

inline int 
CBlastOptionsLocal::GetDbGeneticCode() const
{
    return m_DbOpts->genetic_code;
}

inline const char* 
CBlastOptionsLocal::GetPHIPattern() const
{
    return m_LutOpts->phi_pattern;
}

inline double
CBlastOptionsLocal::GetInclusionThreshold() const
{
    return m_PSIBlastOpts->inclusion_ethresh;
}

inline void
CBlastOptionsLocal::SetInclusionThreshold(double incthr)
{
    m_PSIBlastOpts->inclusion_ethresh = incthr;
}

inline int
CBlastOptionsLocal::GetPseudoCount() const
{
    return m_PSIBlastOpts->pseudo_count;
}

inline void
CBlastOptionsLocal::SetPseudoCount(int pc)
{
    m_PSIBlastOpts->pseudo_count = pc;
}

inline void 
CBlastOptionsLocal::SetPHIPattern(const char* pattern, bool is_dna)
{
    if (!pattern)
        return;

    if (is_dna)
       m_LutOpts->lut_type = PHI_NA_LOOKUP;
    else
       m_LutOpts->lut_type = PHI_AA_LOOKUP;

    m_LutOpts->phi_pattern = strdup(pattern);
}

#endif /* SKIP_DOXYGEN_PROCESSING */

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

#endif /* ALGO_BLAST_API___BLAST_OPTIONS_LOCAL_PRIV__HPP */
