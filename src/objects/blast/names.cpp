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

#include <ncbi_pch.hpp>
#include <objects/blast/names.hpp>
#include <objects/blast/blast__.hpp>
#include <objects/scoremat/scoremat__.hpp>
#include <corelib/ncbimtx.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

CSafeStatic<CBlast4Field::TFieldMap> CBlast4Field::m_Fields;

CBlast4Field & CBlast4Field::Get(EBlastOptIdx opt)
{
	{
   	DEFINE_STATIC_MUTEX(mx);
   	CMutexGuard guard(mx);
    if (m_Fields->count(opt) == 0) {
        auto& m_ = m_Fields.Get();
        switch (opt) {
        //case eBlastOpt_Program:
        case eBlastOpt_WordThreshold:
            m_[opt] = CBlast4Field("WordThreshold",         CBlast4_value::e_Integer);
            break;
        //case eBlastOpt_LookupTableType:  // not found
        case eBlastOpt_WordSize:
            m_[opt] = CBlast4Field("WordSize",              CBlast4_value::e_Integer);
            break;
        //case eBlastOpt_AlphabetSize: //not found
        case eBlastOpt_MBTemplateLength:
            m_[opt] = CBlast4Field("MBTemplateLength",      CBlast4_value::e_Integer);
            break;
        case eBlastOpt_MBTemplateType:
            m_[opt] = CBlast4Field("MBTemplateType",        CBlast4_value::e_Integer);
            break;
        case eBlastOpt_FilterString:
            m_[opt] = CBlast4Field("FilterString",          CBlast4_value::e_String);
            break;
        case eBlastOpt_MaskAtHash:
            m_[opt] = CBlast4Field("MaskAtHash",            CBlast4_value::e_Boolean);
            break;
        case eBlastOpt_DustFiltering:
            m_[opt] = CBlast4Field("DustFiltering",         CBlast4_value::e_Boolean);
            break;
        case eBlastOpt_DustFilteringLevel:
            m_[opt] = CBlast4Field("DustFilteringLevel",    CBlast4_value::e_Integer);
            break;
        case eBlastOpt_DustFilteringWindow:
            m_[opt] = CBlast4Field("DustFilteringWindow",   CBlast4_value::e_Integer);
            break;
        case eBlastOpt_DustFilteringLinker:
            m_[opt] = CBlast4Field("DustFilteringLinker",   CBlast4_value::e_Integer);
            break;
        case eBlastOpt_SegFiltering:
            m_[opt] = CBlast4Field("SegFiltering",          CBlast4_value::e_Boolean);
            break;
        case eBlastOpt_SegFilteringWindow:
            m_[opt] = CBlast4Field("SegFilteringWindow",    CBlast4_value::e_Integer);
            break;
        case eBlastOpt_SegFilteringLocut:
            m_[opt] = CBlast4Field("SegFilteringLocut",     CBlast4_value::e_Real);
            break;
        case eBlastOpt_SegFilteringHicut:
            m_[opt] = CBlast4Field("SegFilteringHicut",     CBlast4_value::e_Real);
            break;
        case eBlastOpt_RepeatFiltering:
            m_[opt] = CBlast4Field("RepeatFiltering",       CBlast4_value::e_Boolean);
            break;
        case eBlastOpt_RepeatFilteringDB:
            m_[opt] = CBlast4Field("RepeatFilteringDB",     CBlast4_value::e_String);
            break;
        case eBlastOpt_StrandOption:
            m_[opt] = CBlast4Field("StrandOption",          CBlast4_value::e_Strand_type);
            break;
        case eBlastOpt_QueryGeneticCode:
            m_[opt] = CBlast4Field("QueryGeneticCode",      CBlast4_value::e_Integer);
            break;
        case eBlastOpt_WindowSize:
            m_[opt] = CBlast4Field("WindowSize",            CBlast4_value::e_Integer);
            break;
        //case eBlastOpt_SeedContainerType:    // not found
        //case eBlastOpt_SeedExtensionMethod:  // not found
        //case eBlastOpt_XDropoff: //not found
        case eBlastOpt_GapXDropoff:
            m_[opt] = CBlast4Field("GapXDropoff",           CBlast4_value::e_Real);
            break;
        case eBlastOpt_GapXDropoffFinal:
            m_[opt] = CBlast4Field("GapXDropoffFinal",      CBlast4_value::e_Real);
            break;
        case eBlastOpt_GapTrigger:
            m_[opt] = CBlast4Field("GapTrigger",            CBlast4_value::e_Real);
            break;
        case eBlastOpt_GapExtnAlgorithm:
            m_[opt] = CBlast4Field("GapExtnAlgorithm",      CBlast4_value::e_Integer);
            break;
        case eBlastOpt_HitlistSize:
            m_[opt] = CBlast4Field("HitlistSize",           CBlast4_value::e_Integer);
        //case eBlastOpt_MaxNumHspPerSequence:     // not found
            break;
        case eBlastOpt_CullingLimit:
            m_[opt] = CBlast4Field("Culling",               CBlast4_value::e_Integer);
            break;
        case eBlastOpt_EvalueThreshold:
            m_[opt] = CBlast4Field("EvalueThreshold",       CBlast4_value::e_Cutoff);
            break;
        case eBlastOpt_CutoffScore:
            m_[opt] = CBlast4Field("CutoffScore",           CBlast4_value::e_Cutoff);
            break;
        case eBlastOpt_PercentIdentity:
            m_[opt] = CBlast4Field("PercentIdentity",       CBlast4_value::e_Real);
            break;
        case eBlastOpt_QueryCovHspPerc:
            m_[opt] = CBlast4Field("QueryCovHspPerc",       CBlast4_value::e_Real);
            break;
        case eBlastOpt_MaxHspsPerSubject:
            m_[opt] = CBlast4Field("MaxHspsPerSubject",     CBlast4_value::e_Integer);
            break;
        case eBlastOpt_SumStatisticsMode:
            m_[opt] = CBlast4Field("SumStatistics",         CBlast4_value::e_Boolean);
            break;
        case eBlastOpt_LongestIntronLength:
            m_[opt] = CBlast4Field("LongestIntronLength",   CBlast4_value::e_Integer);
            break;
        case eBlastOpt_GappedMode:   // same as !ungapped mode
            m_[opt] = CBlast4Field("UngappedMode",          CBlast4_value::e_Boolean);
            break;
        case eBlastOpt_ComplexityAdjMode:  // -RMH-
            m_[opt] = CBlast4Field("ComplexityAdjustMode",          CBlast4_value::e_Boolean);
            break;
        case eBlastOpt_MaskLevel:  // -RMH-
            m_[opt] = CBlast4Field("MaskLevel",             CBlast4_value::e_Integer);
            break;
        case eBlastOpt_MatrixName:
            m_[opt] = CBlast4Field("MatrixName",            CBlast4_value::e_String);
            break;
        //case eBlastOpt_MatrixPath:       // not found
        case eBlastOpt_MatchReward:
            m_[opt] = CBlast4Field("MatchReward",           CBlast4_value::e_Integer);
            break;
        case eBlastOpt_MismatchPenalty:
            m_[opt] = CBlast4Field("MismatchPenalty",       CBlast4_value::e_Integer);
            break;
        case eBlastOpt_GapOpeningCost:
            m_[opt] = CBlast4Field("GapOpeningCost",        CBlast4_value::e_Integer);
            break;
        case eBlastOpt_GapExtensionCost:
            m_[opt] = CBlast4Field("GapExtensionCost",      CBlast4_value::e_Integer);
            break;
        //case eBlastOpt_FrameShiftPenalty:    // not found
        case eBlastOpt_OutOfFrameMode:
            m_[opt] = CBlast4Field("OutOfFrameMode",        CBlast4_value::e_Boolean);
            break;
        case eBlastOpt_DbLength:
            m_[opt] = CBlast4Field("DbLength",              CBlast4_value::e_Big_integer);
            break;
        //case eBlastOpt_DbSeqNum:     // not found
        case eBlastOpt_EffectiveSearchSpace:
            m_[opt] = CBlast4Field("EffectiveSearchSpace",  CBlast4_value::e_Big_integer);
            break;
        case eBlastOpt_DbGeneticCode:
            m_[opt] = CBlast4Field("DbGeneticCode",         CBlast4_value::e_Integer);
            break;
        case eBlastOpt_PHIPattern:
            m_[opt] = CBlast4Field("PHIPattern",            CBlast4_value::e_String);
            break;
        case eBlastOpt_InclusionThreshold:
            m_[opt] = CBlast4Field("InclusionThreshold",    CBlast4_value::e_Real);
            break;
        case eBlastOpt_PseudoCount:
            m_[opt] = CBlast4Field("PseudoCountWeight",     CBlast4_value::e_Integer);
            break;
        case eBlastOpt_GapTracebackAlgorithm:
            m_[opt] = CBlast4Field("GapTracebackAlgorithm", CBlast4_value::e_Integer);
            break;
        case eBlastOpt_CompositionBasedStats:
            m_[opt] = CBlast4Field("CompositionBasedStats", CBlast4_value::e_Integer);
            break;
        case eBlastOpt_SmithWatermanMode:
            m_[opt] = CBlast4Field("SmithWatermanMode",     CBlast4_value::e_Boolean);
            break;
        case eBlastOpt_UnifiedP:
            m_[opt] = CBlast4Field("UnifiedP",              CBlast4_value::e_Integer);
            break;
        case eBlastOpt_WindowMaskerDatabase:
            m_[opt] = CBlast4Field("WindowMaskerDatabase",  CBlast4_value::e_String);
            break;
        case eBlastOpt_WindowMaskerTaxId:
            m_[opt] = CBlast4Field("WindowMaskerTaxId",     CBlast4_value::e_Integer);
            break;
        case eBlastOpt_ForceMbIndex:         // corresponds to -use_index flag
            m_[opt] = CBlast4Field("ForceMbIndex",          CBlast4_value::e_Boolean);
            break;
        case eBlastOpt_MbIndexName:          // corresponds to -index_name flag
            m_[opt] = CBlast4Field("MbIndexName",           CBlast4_value::e_String);
            break;
        case eBlastOpt_BestHitScoreEdge:
            m_[opt] = CBlast4Field("BestHitScoreEdge",      CBlast4_value::e_Real);
            break;
        case eBlastOpt_BestHitOverhang:
            m_[opt] = CBlast4Field("BestHitOverhang",       CBlast4_value::e_Real);
            break;
        case eBlastOpt_SubjectBestHit:  // corresponds to -subject_besthit flag
            m_[opt] = CBlast4Field("UseSubjectBestHit",     CBlast4_value::e_Boolean);
            break;
        case eBlastOpt_IgnoreMsaMaster:
            m_[opt] = CBlast4Field("IgnoreMsaMaster",       CBlast4_value::e_Boolean);
            break;
        case eBlastOpt_DomainInclusionThreshold:  // options for DELTA-BLAST
            m_[opt] = CBlast4Field("DomainInclusionThreshold", CBlast4_value::e_Real);
            break;

        // Added to provide access to these options, they don't have a usage in
        case eBlastOpt_Culling:
            m_[opt] = CBlast4Field("Culling",               CBlast4_value::e_Boolean);
            break;
        case eBlastOpt_EntrezQuery:
            m_[opt] = CBlast4Field("EntrezQuery",           CBlast4_value::e_String);
            break;
        case eBlastOpt_FinalDbSeq:
            m_[opt] = CBlast4Field("FinalDbSeq",            CBlast4_value::e_Integer);
            break;
        case eBlastOpt_FirstDbSeq:
            m_[opt] = CBlast4Field("FirstDbSeq",            CBlast4_value::e_Integer);
            break;
        case eBlastOpt_GiList:
            m_[opt] = CBlast4Field("GiList",                CBlast4_value::e_Integer_list);
            break;
        case eBlastOpt_DbFilteringAlgorithmId:
            m_[opt] = CBlast4Field("DbFilteringAlgorithmId",CBlast4_value::e_Integer);
            break;
        case eBlastOpt_DbFilteringAlgorithmKey:
            m_[opt] = CBlast4Field("DbFilteringAlgorithmKey",CBlast4_value::e_String);
            break;
        case eBlastOpt_SubjectMaskingType:
            m_[opt] = CBlast4Field("SubjectMaskingType",    CBlast4_value::e_Integer);
            break;
        case eBlastOpt_HspRangeMax:
            m_[opt] = CBlast4Field("HspRangeMax",           CBlast4_value::e_Integer);
            break;
        case eBlastOpt_LCaseMask:
            m_[opt] = CBlast4Field("LCaseMask",             CBlast4_value::e_Query_mask);
            break;
        case eBlastOpt_MatrixTable:
            m_[opt] = CBlast4Field("MatrixTable",           CBlast4_value::e_Matrix);
            break;
        case eBlastOpt_NegativeGiList:
            m_[opt] = CBlast4Field("NegativeGiList",        CBlast4_value::e_Integer_list);
            break;
        case eBlastOpt_RequiredEnd:
            m_[opt] = CBlast4Field("RequiredEnd",           CBlast4_value::e_Integer);
            break;
        case eBlastOpt_RequiredStart:
            m_[opt] = CBlast4Field("RequiredStart",         CBlast4_value::e_Integer);
            break;
        case eBlastOpt_UseRealDbSize:
            m_[opt] = CBlast4Field("UseRealDbSize",         CBlast4_value::e_Boolean);
            break;
        case eBlastOpt_TaxidList:
            m_[opt] = CBlast4Field("TaxidList",             CBlast4_value::e_Integer_list);
            break;
        case eBlastOpt_NegativeTaxidList:
            m_[opt] = CBlast4Field("NegativeTaxidList",     CBlast4_value::e_Integer_list);
            break;

// List of web-related options
        case eBlastOpt_Web_BlastSpecialPage:
            m_[opt] = CBlast4Field("Web_BlastSpecialPage", CBlast4_value::e_String);
            break;
        case eBlastOpt_Web_EntrezQuery:
            m_[opt] = CBlast4Field("Web_EntrezQuery",      CBlast4_value::e_String);
            break;
        case eBlastOpt_Web_JobTitle:
            m_[opt] = CBlast4Field("Web_JobTitle",         CBlast4_value::e_String);
            break;
        case eBlastOpt_Web_NewWindow:
            m_[opt] = CBlast4Field("Web_NewWindow",        CBlast4_value::e_Boolean);
            break;
        case eBlastOpt_Web_OrganismName:
            m_[opt] = CBlast4Field("Web_OrganismName",     CBlast4_value::e_String);
            break;
        case eBlastOpt_Web_RunPsiBlast:
            m_[opt] = CBlast4Field("Web_RunPsiBlast",      CBlast4_value::e_Boolean);
            break;
        case eBlastOpt_Web_ShortQueryAdjust:
            m_[opt] = CBlast4Field("Web_ShortQueryAdjust", CBlast4_value::e_Boolean);
            break;
        case eBlastOpt_Web_StepNumber:
            m_[opt] = CBlast4Field("Web_StepNumber",       CBlast4_value::e_Integer);
            break;
        case eBlastOpt_Web_DBInput:
            m_[opt] = CBlast4Field("Web_DBInput", 	      CBlast4_value::e_Boolean);
            break;
        case eBlastOpt_Web_DBGroup:
            m_[opt] = CBlast4Field("Web_DBGroup",          CBlast4_value::e_String);
            break;
        case eBlastOpt_Web_DBSubgroupName:
            m_[opt] = CBlast4Field("Web_DBSubgroupName",   CBlast4_value::e_String);
            break;
        case eBlastOpt_Web_DBSubgroup:
            m_[opt] = CBlast4Field("Web_DBSubgroup",       CBlast4_value::e_String);
            break;
        case eBlastOpt_Web_ExclModels:
            m_[opt] = CBlast4Field("Web_ExclModels",       CBlast4_value::e_Boolean);
            break;
        case eBlastOpt_Web_ExclSeqUncult:
            m_[opt] = CBlast4Field("Web_SeqUncult",        CBlast4_value::e_Boolean);
            break;
        case eBlastOpt_Web_OrganismNameExcl:
            m_[opt] = CBlast4Field("Web_OrganismNameExcl",     CBlast4_value::e_String);
            break;            

        default:
            ERR_POST(Warning << "Undefined remote BLAST options used");
            m_[opt] = CBlast4Field("-",        CBlast4_value::e_not_set);
            break;
        }
    }
	}
    _ASSERT(m_Fields->count(opt) != 0);
    return (*m_Fields)[opt];
}

const string & CBlast4Field::GetName(EBlastOptIdx opt)
{
    return CBlast4Field::Get(opt).GetName();
}

const string & CBlast4Field::GetName() const
{
    return m_Name;
}

CBlast4_value::E_Choice CBlast4Field::GetType() const
{
    return m_Type;
}

bool CBlast4Field::Match(const CBlast4_parameter & p) const
{
    return (p.CanGetName()        &&
            p.CanGetValue()       &&
            p.GetValue().Which() == m_Type  &&
            p.GetName() == m_Name);
}

string CBlast4Field::GetString(const CBlast4_parameter & p) const
{
    _ASSERT(Match(p));
    return p.GetValue().GetString();
}

bool CBlast4Field::GetBoolean(const CBlast4_parameter & p) const
{
    _ASSERT(Match(p));
    return p.GetValue().GetBoolean();
}

Int8 CBlast4Field::GetBig_integer(const CBlast4_parameter & p) const
{
    _ASSERT(Match(p));
    return p.GetValue().GetBig_integer();
}

CConstRef<CBlast4_cutoff> 
CBlast4Field::GetCutoff(const CBlast4_parameter & p) const
{
    _ASSERT(Match(p));
    return CConstRef<CBlast4_cutoff>(& p.GetValue().GetCutoff());
}

int CBlast4Field::GetInteger(const CBlast4_parameter & p) const
{
    _ASSERT(Match(p));
    return p.GetValue().GetInteger();
}

list<int> CBlast4Field::GetIntegerList (const CBlast4_parameter & p) const
{
    _ASSERT(Match(p));
    return p.GetValue().GetInteger_list();
}

CConstRef<CPssmWithParameters> 
CBlast4Field::GetMatrix(const CBlast4_parameter & p) const
{
    _ASSERT(Match(p));
    return CConstRef<CPssmWithParameters>(& p.GetValue().GetMatrix());
}

CConstRef<CBlast4_mask> 
CBlast4Field::GetQueryMask(const CBlast4_parameter & p) const
{
    _ASSERT(Match(p));
    return CConstRef<CBlast4_mask>(& p.GetValue().GetQuery_mask());
}

double CBlast4Field::GetReal(const CBlast4_parameter & p) const
{
    _ASSERT(Match(p));
    return p.GetValue().GetReal();
}

EBlast4_strand_type 
CBlast4Field::GetStrandType(const CBlast4_parameter & p) const
{
    _ASSERT(Match(p));
    return p.GetValue().GetStrand_type();
}

const char* kBlast4SearchInfoReqName_Search("search");
const char* kBlast4SearchInfoReqName_Alignment("alignment");
const char* kBlast4SearchInfoReqValue_Status("status");
const char* kBlast4SearchInfoReqValue_Title("title");
const char* kBlast4SearchInfoReqValue_Subjects("subjects");
const char* kBlast4SearchInfoReqValue_PsiIterationNum("psi_iteration_number");

/// Auxiliary function to consistently build the Blast4-get-search-info-reply
/// names
string Blast4SearchInfo_BuildReplyName(const string& name, const string& value)
{
    return name + string("-") + value;
}

END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE
