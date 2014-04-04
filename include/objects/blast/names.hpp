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
 * Author:  Kevin Bealer
 *
 * File Description:
 *   Define names and value types for options known to blast4.
 *
 */

/// @file names.hpp
/// Names used in blast4 network communications.
///
/// This file declares string objects corresponding to names used when
/// specifying options for blast4 network communications protocol.

#ifndef OBJECTS_BLAST_NAMES_HPP
#define OBJECTS_BLAST_NAMES_HPP

#include <utility>
#include <string>
#include <corelib/ncbistl.hpp>
#include <objects/blast/NCBI_Blast4_module.hpp>
#include <objects/blast/Blast4_value.hpp>
#include <objects/blast/Blast4_parameter.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

/// Index of remote BLAST options.
/// The blast4 server only supports a subset of these
enum EBlastOptIdx {
    eBlastOpt_Program = 100,
    eBlastOpt_WordThreshold,
    eBlastOpt_LookupTableType,
    eBlastOpt_WordSize,
    eBlastOpt_AlphabetSize,
    eBlastOpt_MBTemplateLength,
    eBlastOpt_MBTemplateType,
    eBlastOpt_FilterString,
    eBlastOpt_MaskAtHash,
    eBlastOpt_DustFiltering,
    eBlastOpt_DustFilteringLevel,
    eBlastOpt_DustFilteringWindow,
    eBlastOpt_DustFilteringLinker,
    eBlastOpt_SegFiltering,
    eBlastOpt_SegFilteringWindow,
    eBlastOpt_SegFilteringLocut,
    eBlastOpt_SegFilteringHicut,
    eBlastOpt_RepeatFiltering,
    eBlastOpt_RepeatFilteringDB,
    eBlastOpt_StrandOption,
    eBlastOpt_QueryGeneticCode,
    eBlastOpt_WindowSize,
    eBlastOpt_SeedContainerType,
    eBlastOpt_SeedExtensionMethod,
    eBlastOpt_XDropoff,
    eBlastOpt_GapXDropoff,
    eBlastOpt_GapXDropoffFinal,
    eBlastOpt_GapTrigger,
    eBlastOpt_GapExtnAlgorithm,
    eBlastOpt_HitlistSize,
    eBlastOpt_MaxNumHspPerSequence,
    eBlastOpt_CullingLimit,
    eBlastOpt_EvalueThreshold,
    eBlastOpt_CutoffScore,
    eBlastOpt_PercentIdentity,
    eBlastOpt_QueryCovHspPerc,
    eBlastOpt_SumStatisticsMode,
    eBlastOpt_LongestIntronLength,
    eBlastOpt_GappedMode,
    eBlastOpt_ComplexityAdjMode,
    eBlastOpt_MaskLevel,
    eBlastOpt_MatrixName,
    eBlastOpt_MatrixPath,
    eBlastOpt_MatchReward,
    eBlastOpt_MismatchPenalty,
    eBlastOpt_GapOpeningCost,
    eBlastOpt_GapExtensionCost,
    eBlastOpt_FrameShiftPenalty,
    eBlastOpt_OutOfFrameMode,
    eBlastOpt_DbLength,
    eBlastOpt_DbSeqNum,
    eBlastOpt_EffectiveSearchSpace,
    eBlastOpt_DbGeneticCode,
    eBlastOpt_PHIPattern,
    eBlastOpt_InclusionThreshold,
    eBlastOpt_PseudoCount,
    eBlastOpt_GapTracebackAlgorithm,
    eBlastOpt_CompositionBasedStats,
    eBlastOpt_SmithWatermanMode,
    eBlastOpt_UnifiedP,
    eBlastOpt_WindowMaskerDatabase,
    eBlastOpt_WindowMaskerTaxId,
    eBlastOpt_ForceMbIndex,         // corresponds to -use_index flag
    eBlastOpt_MbIndexName,          // corresponds to -index_name flag
    eBlastOpt_BestHitScoreEdge,
    eBlastOpt_BestHitOverhang,
    eBlastOpt_IgnoreMsaMaster,
    eBlastOpt_DomainInclusionThreshold, // options for DELTA-BLAST

    eBlastOpt_Culling,
    eBlastOpt_EntrezQuery,
    eBlastOpt_FinalDbSeq,
    eBlastOpt_FirstDbSeq,
    eBlastOpt_GiList,
    eBlastOpt_DbFilteringAlgorithmId,
    eBlastOpt_DbFilteringAlgorithmKey,
    eBlastOpt_SubjectMaskingType,
    eBlastOpt_HspRangeMax,
    eBlastOpt_LCaseMask,
    eBlastOpt_MatrixTable,
    eBlastOpt_NegativeGiList,
    eBlastOpt_RequiredEnd,
    eBlastOpt_RequiredStart,
    eBlastOpt_UseRealDbSize,
    eBlastOpt_Web_BlastSpecialPage,
    eBlastOpt_Web_EntrezQuery,
    eBlastOpt_Web_JobTitle,
    eBlastOpt_Web_NewWindow,
    eBlastOpt_Web_OrganismName,
    eBlastOpt_Web_RunPsiBlast,
    eBlastOpt_Web_ShortQueryAdjust,
    eBlastOpt_Web_StepNumber,
    eBlastOpt_Web_DBInput,
    eBlastOpt_Web_DBGroup,
    eBlastOpt_Web_DBSubgroupName,
    eBlastOpt_Web_DBSubgroup,
    eBlastOpt_Web_ExclModels,
    eBlastOpt_Web_ExclSeqUncult,
    eBlastOpt_MaxValue       // For testing/looping, not an actual parameter
};


/// Field properties for options in a Blast4 parameter list.
class NCBI_BLAST_EXPORT CBlast4Field {
public:
    /// Default constructor (for STL)
    CBlast4Field() : m_Name("-"), m_Type(CBlast4_value::e_not_set) {}
    
    /// Construct field with name and type.
    CBlast4Field(const std::string& nm, CBlast4_value::E_Choice ch)
        : m_Name(nm), m_Type(ch) {}

    static CBlast4Field& Get(EBlastOptIdx opt);
    static const string& GetName(EBlastOptIdx opt);
    
    /// Get field name (key).
    const string& GetName() const;
    
    /// Match field name and type to parameter.
    bool Match(const CBlast4_parameter& p) const;

    /// Get field type.
    CBlast4_value::E_Choice GetType() const;
    
    
    /// Verify parameter name and type, and get boolean value.
    bool GetBoolean(const CBlast4_parameter& p) const;
    
    /// Verify parameter name and type, and get big integer value.
    Int8 GetBig_integer(const CBlast4_parameter& p) const;
    
    /// Verify parameter name and type, and get cutoff value.
    CConstRef<CBlast4_cutoff> GetCutoff(const CBlast4_parameter& p) const;
    
    /// Verify parameter name and type, and get integer value.
    int GetInteger(const CBlast4_parameter& p) const;
    
    /// Verify parameter name and type, and get integer list value.
    list<int> GetIntegerList(const CBlast4_parameter& p) const;
    
    /// Verify parameter name and type, and get matrix (pssm) value.
    CConstRef<CPssmWithParameters> GetMatrix(const CBlast4_parameter& p) const;
    
    /// Verify parameter name and type, and get query mask value.
    CConstRef<CBlast4_mask> GetQueryMask(const CBlast4_parameter& p) const;
    
    /// Verify parameter name and type, and get real value.
    double GetReal(const CBlast4_parameter & p) const;
    
    /// Verify parameter name and type, and get strand_type value.
    EBlast4_strand_type GetStrandType(const CBlast4_parameter& p) const;
    
    /// Verify parameter name and type, and get string value.
    string GetString(const CBlast4_parameter& p) const;
    
private:
    /// Field name string as used in Blast4_parameter objects.
    string m_Name;
    
    /// Field value type as used in Blast4_parameter objects.
    CBlast4_value::E_Choice m_Type;
    
    /// Type for map of all blast4 field objects.
    typedef map<EBlastOptIdx, CBlast4Field> TFieldMap;
    
    /// Map of all blast4 field objects.
    static TFieldMap m_Fields;
};

/*****************************************************************************/
// String pairs used to support for get-search-info request

/// Used to retrieve information about the BLAST search
NCBI_BLAST_EXPORT extern  const char* kBlast4SearchInfoReqName_Search;
/// Used to retrieve information about the BLAST alignments
NCBI_BLAST_EXPORT extern  const char* kBlast4SearchInfoReqName_Alignment;

/// Used to retrieve the BLAST search status
NCBI_BLAST_EXPORT extern  const char* kBlast4SearchInfoReqValue_Status;
/// Used to retrieve the BLAST search title
NCBI_BLAST_EXPORT extern  const char* kBlast4SearchInfoReqValue_Title;
/// Used to retrieve the BLAST search subjects
NCBI_BLAST_EXPORT extern  const char* kBlast4SearchInfoReqValue_Subjects;

/// Used to retrieve the PSI-BLAST iteration number
NCBI_BLAST_EXPORT extern  const char* kBlast4SearchInfoReqValue_PsiIterationNum;

/// This function builds the reply name token in the get-search-info reply
/// objects, provided a pair of strings such as those defined above
/// (i.e.: kBlast4SearchInfoReq{Name,Value})
NCBI_BLAST_EXPORT
string Blast4SearchInfo_BuildReplyName(const string& name, const string& value);


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

#endif // OBJECTS_BLAST_NAMES_HPP
