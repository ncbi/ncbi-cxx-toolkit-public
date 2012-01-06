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

/// Field properties for options in a Blast4 parameter list.
class NCBI_BLAST_EXPORT CBlast4Field {
public:
    /// Default constructor (for STL)
    CBlast4Field();
    
    /// Construct field with name and type.
    CBlast4Field(const std::string& nm, CBlast4_value::E_Choice ch);
    
    /// Get field name (key).
    const string& GetName() const;
    
    /// Get field type.
    CBlast4_value::E_Choice GetType() const;
    
    /// Match field name and type to parameter.
    bool Match(const CBlast4_parameter& p) const;
    
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
    
    /// Check whether a field of the given name exists.
    static bool KnownField(const string& name);
    
private:
    /// Field name string as used in Blast4_parameter objects.
    string m_Name;
    
    /// Field value type as used in Blast4_parameter objects.
    CBlast4_value::E_Choice m_Type;
    
    /// Type for map of all blast4 field objects.
    typedef map<string, CBlast4Field> TFieldMap;
    
    /// Map of all blast4 field objects.
    static TFieldMap m_Fields;
};

NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_BestHitScoreEdge;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_BestHitOverhang;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_CompositionBasedStats;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_Culling;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_CullingLimit;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_CutoffScore;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_DbGeneticCode;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_DbLength;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_DustFiltering;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_DustFilteringLevel;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_DustFilteringLinker;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_DustFilteringWindow;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_EffectiveSearchSpace;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_EntrezQuery;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_EvalueThreshold;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_FilterString;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_FinalDbSeq;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_FirstDbSeq;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_GapExtensionCost;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_GapExtnAlgorithm;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_GapOpeningCost;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_GapTracebackAlgorithm;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_GiList;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_DbFilteringAlgorithmId;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_NegativeGiList;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_HitlistSize;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_HspRangeMax;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_InclusionThreshold;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_LCaseMask;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_MaskAtHash;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_MatchReward;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_MatrixName;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_MatrixTable;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_MismatchPenalty;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_PercentIdentity;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_PHIPattern;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_PseudoCountWeight;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_QueryGeneticCode;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_RepeatFiltering;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_RepeatFilteringDB;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_RequiredEnd;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_RequiredStart;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_StrandOption;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_MBTemplateLength;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_MBTemplateType;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_OutOfFrameMode;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_SegFiltering;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_SegFilteringWindow;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_SegFilteringLocut;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_SegFilteringHicut;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_UngappedMode;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_ComplexityAdjustMode;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_MaskLevel;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_UseRealDbSize;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_WindowSize;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_WordSize;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_WordThreshold;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_SumStatistics;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_LongestIntronLength;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_GapTrigger;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_GapXDropoff;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_GapXDropoffFinal;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_SmithWatermanMode;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_UnifiedP;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_WindowMaskerDatabase;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_WindowMaskerTaxId;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_ForceMbIndex;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_IgnoreMsaMaster;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_MbIndexName;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_DomainInclusionThreshold;

// List of web-related options
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_Web_BlastSpecialPage;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_Web_EntrezQuery;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_Web_JobTitle;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_Web_NewWindow;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_Web_OrganismName;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_Web_RunPsiBlast;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_Web_ShortQueryAdjust;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_Web_StepNumber;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_Web_DBInput;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_Web_DBGroup;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_Web_DBSubgroupName;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_Web_DBSubgroup;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_Web_ExclModels;
NCBI_BLAST_EXPORT extern  CBlast4Field B4Param_Web_ExclSeqUncult;

/*****************************************************************************/
// String pairs used to support for get-search-info request

/// Used to retrieve information about the BLAST search
NCBI_BLAST_EXPORT extern  string kBlast4SearchInfoReqName_Search;
/// Used to retrieve information about the BLAST alignments
NCBI_BLAST_EXPORT extern  string kBlast4SearchInfoReqName_Alignment;

/// Used to retrieve the BLAST search status
NCBI_BLAST_EXPORT extern  string kBlast4SearchInfoReqValue_Status;
/// Used to retrieve the BLAST search title
NCBI_BLAST_EXPORT extern  string kBlast4SearchInfoReqValue_Title;
/// Used to retrieve the BLAST search subjects
NCBI_BLAST_EXPORT extern  string kBlast4SearchInfoReqValue_Subjects;

/// Used to retrieve the PSI-BLAST iteration number
NCBI_BLAST_EXPORT extern  string kBlast4SearchInfoReqValue_PsiIterationNum;

/// This function builds the reply name token in the get-search-info reply
/// objects, provided a pair of strings such as those defined above
/// (i.e.: kBlast4SearchInfoReq{Name,Value})
NCBI_BLAST_EXPORT
string Blast4SearchInfo_BuildReplyName(const string& name, const string& value);


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

#endif // OBJECTS_BLAST_NAMES_HPP
