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
* ===========================================================================
*/

/// @file blast_options_builder.cpp
/// Defines the CBlastOptionsBuilder class

#include <ncbi_pch.hpp>
#include <algo/blast/api/remote_blast.hpp>
#include <algo/blast/api/blast_options_builder.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

CBlastOptionsBuilder::
CBlastOptionsBuilder(const string                & program,
                     const string                & service,
                     CBlastOptions::EAPILocality   locality,
                     bool ignore_unsupported_options)
    : m_Program        (program),
      m_Service        (service),
      m_PerformCulling (false),
      m_HspRangeMax    (0),
      m_QueryRange     (TSeqRange::GetEmpty()),
      m_Locality       (locality),
      m_IgnoreUnsupportedOptions(ignore_unsupported_options),
      m_ForceMbIndex   (false)
{
}

EProgram
CBlastOptionsBuilder::ComputeProgram(const string & program,
                                     const string & service)
{
    string p = program;
    string s = service;
    
    NStr::ToLower(p);
    NStr::ToLower(s);
    
    // a. is there a program for phiblast?
    // b. others, like disco?
    
    bool found = false;
    
    if (p == "blastp") {
        if (s == "rpsblast") {
            p = "rpsblast";
            found = true;
        } else if (s == "psi") {
            p = "psiblast";
            found = true;
        } else if (s == "phi") {
            // phi is just treated as a blastp here
            found = true;
        } else if (s == "delta_blast") {
            p = "deltablast";
            found = true;
        }
    } else if (p == "blastn") {
        if (s == "megablast") {
            p = "megablast";
            found = true;
        }
        if (s == "vecscreen") {
            p = "vecscreen";
            found = true;
        }
        if (s == "sra") {
        	found = true;
        }
    } else if (p == "tblastn") {
        if (s == "rpsblast") {
            p = "rpstblastn";
            found = true;
        } else if (s == "psi") {
            p = "psitblastn";
            found = true;
        }
    } else if (p == "tblastx") {
        found = true;
    } else if (p == "blastx") {
        if (s == "rpsblast") {
            p = "rpstblastn";
            found = true;
        }
    }
    
    if (s != "plain" && (! found)) {
        string msg = "Unsupported combination of program (";
        msg += program;
        msg += ") and service (";
        msg += service;
        msg += ").";
        
        NCBI_THROW(CBlastException, eInvalidArgument, msg);
    }
    
    return ProgramNameToEnum(p);
}

void CBlastOptionsBuilder::
x_ProcessOneOption(CBlastOptionsHandle        & opts,
                   objects::CBlast4_parameter & p)
{
    const CBlast4_value & v = p.GetValue();
    
    // Note that this code does not attempt to detect or repair
    // inconsistencies; since this request has already been processed
    // by SplitD, the results are assumed to be correct, for now.
    // This will remain so unless options validation code becomes
    // available, in which case it could be used by this code.  This
    // could be considered as a potential "to-do" item.
    
    if (! p.CanGetName() || p.GetName().empty()) {
        NCBI_THROW(CBlastException,
                   eInvalidArgument,
                   "Option has no name.");
    }
    
    string nm = p.GetName();
    
    bool found = true;
    
    // This switch is not really necessary.  I wanted to break things
    // up for human consumption.  But as long as I'm doing that, I may
    // as well use a performance-friendly paragraph marker.
    
    CBlastOptions & bo = opts.SetOptions();

    switch(nm[0]) {
    case 'B':
        if (CBlast4Field::Get(eBlastOpt_BestHitScoreEdge).Match(p)) {
            bo.SetBestHitScoreEdge(v.GetReal());
        } else if (CBlast4Field::Get(eBlastOpt_BestHitOverhang).Match(p)) {
            bo.SetBestHitOverhang(v.GetReal());
        } else {
            found = false;
        }
        break;

    case 'C':
        if (CBlast4Field::Get(eBlastOpt_CompositionBasedStats).Match(p)) {
            ECompoAdjustModes adjmode = (ECompoAdjustModes) v.GetInteger();
            bo.SetCompositionBasedStats(adjmode);
        } else if (CBlast4Field::Get(eBlastOpt_Culling).Match(p)) {
            m_PerformCulling = v.GetBoolean();
        } else if (CBlast4Field::Get(eBlastOpt_CullingLimit).Match(p)) {
            bo.SetCullingLimit(v.GetInteger());
        } else if (CBlast4Field::Get(eBlastOpt_CutoffScore).Match(p)) {
            opts.SetCutoffScore(v.GetInteger());
        } else {
            found = false;
        }
        break;
        
    case 'D':
        if (CBlast4Field::Get(eBlastOpt_DbGeneticCode).Match(p)) {
            bo.SetDbGeneticCode(v.GetInteger());
        } else if (CBlast4Field::Get(eBlastOpt_DbLength).Match(p)) {
            opts.SetDbLength(v.GetBig_integer());
        } else if (CBlast4Field::Get(eBlastOpt_DustFiltering).Match(p)) {
            bo.SetDustFiltering(v.GetBoolean());
        } else if (CBlast4Field::Get(eBlastOpt_DustFilteringLevel).Match(p)) {
            bo.SetDustFilteringLevel(v.GetInteger());
        } else if (CBlast4Field::Get(eBlastOpt_DustFilteringWindow).Match(p)) {
            bo.SetDustFilteringWindow(v.GetInteger());
        } else if (CBlast4Field::Get(eBlastOpt_DustFilteringLinker).Match(p)) {
            bo.SetDustFilteringLinker(v.GetInteger());
        } else if (CBlast4Field::Get(eBlastOpt_DbFilteringAlgorithmId).Match(p)) {
            m_DbFilteringAlgorithmId = v.GetInteger();
        } else if (CBlast4Field::Get(eBlastOpt_DbFilteringAlgorithmKey).Match(p)) {
            m_DbFilteringAlgorithmKey = v.GetString();
        } else if (CBlast4Field::Get(eBlastOpt_DomainInclusionThreshold).Match(p)) {
            bo.SetDomainInclusionThreshold(v.GetReal());
        } else {
            found = false;
        }
        break;
        
    case 'E':
        if (CBlast4Field::Get(eBlastOpt_EffectiveSearchSpace).Match(p)) {
            opts.SetEffectiveSearchSpace(v.GetBig_integer());
        } else if (CBlast4Field::Get(eBlastOpt_EntrezQuery).Match(p)) {
            m_EntrezQuery = v.GetString();
        } else if (CBlast4Field::Get(eBlastOpt_EvalueThreshold).Match(p)
                   ||  p.GetName() == "EvalueThreshold") {
            if (v.IsReal()) {
                opts.SetEvalueThreshold(v.GetReal());
            } else if (v.IsCutoff() && v.GetCutoff().IsE_value()) {
                opts.SetEvalueThreshold(v.GetCutoff().GetE_value());
            } else {
                string msg = "EvalueThreshold has unsupported type.";
                NCBI_THROW(CBlastException, eInvalidArgument, msg);
            }
        } else {
            found = false;
        }
        break;
        
    case 'F':
        if (CBlast4Field::Get(eBlastOpt_FilterString).Match(p)) {
            opts.SetFilterString(v.GetString().c_str(), true);  /* NCBI_FAKE_WARNING */
        } else if (CBlast4Field::Get(eBlastOpt_FinalDbSeq).Match(p)) {
            m_FinalDbSeq = v.GetInteger();
        } else if (CBlast4Field::Get(eBlastOpt_FirstDbSeq).Match(p)) {
            m_FirstDbSeq = v.GetInteger();
        } else if (CBlast4Field::Get(eBlastOpt_ForceMbIndex).Match(p)) {
            m_ForceMbIndex = v.GetBoolean();
        } else {
            found = false;
        }
        break;
        
    case 'G':
        if (CBlast4Field::Get(eBlastOpt_GapExtensionCost).Match(p)) {
            bo.SetGapExtensionCost(v.GetInteger());
        } else if (CBlast4Field::Get(eBlastOpt_GapOpeningCost).Match(p)) {
            bo.SetGapOpeningCost(v.GetInteger());
        } else if (CBlast4Field::Get(eBlastOpt_GiList).Match(p)) {
            m_GiList = v.GetInteger_list();
        } else if (CBlast4Field::Get(eBlastOpt_GapTracebackAlgorithm).Match(p)) {
            bo.SetGapTracebackAlgorithm((EBlastTbackExt) v.GetInteger());
        } else if (CBlast4Field::Get(eBlastOpt_GapTrigger).Match(p)) {
            bo.SetGapTrigger(v.GetReal());
        } else if (CBlast4Field::Get(eBlastOpt_GapXDropoff).Match(p)) {
            bo.SetGapXDropoff(v.GetReal());
        } else if (CBlast4Field::Get(eBlastOpt_GapXDropoffFinal).Match(p)) {
            bo.SetGapXDropoffFinal(v.GetReal());
        } else if (CBlast4Field::Get(eBlastOpt_GapExtnAlgorithm).Match(p)) {
            bo.SetGapExtnAlgorithm(static_cast<EBlastPrelimGapExt>
                                   (v.GetInteger()));
        } else {
            found = false;
        }
        break;
        
    case 'H':
        if (CBlast4Field::Get(eBlastOpt_HitlistSize).Match(p)) {
            opts.SetHitlistSize(v.GetInteger());
        } else if (CBlast4Field::Get(eBlastOpt_HspRangeMax).Match(p)) {
            m_HspRangeMax = v.GetInteger();
        } else {
            found = false;
        }
        break;

    case 'I':
        if (CBlast4Field::Get(eBlastOpt_InclusionThreshold).Match(p)) {
            bo.SetInclusionThreshold(v.GetReal());
        } else if (CBlast4Field::Get(eBlastOpt_IgnoreMsaMaster).Match(p)) {
            bo.SetIgnoreMsaMaster(v.GetBoolean());
        } else {
            found = false;
        }
        break;
        
    case 'L':
        if (CBlast4Field::Get(eBlastOpt_LCaseMask).Match(p))
        {
            if (!m_IgnoreQueryMasks)
            {
                _ASSERT(v.IsQuery_mask());
                CRef<CBlast4_mask> refMask(new CBlast4_mask);
                refMask->Assign(v.GetQuery_mask());

                if (!m_QueryMasks.Have())
                {
                    TMaskList listEmpty;
                    m_QueryMasks = listEmpty;
                }
                m_QueryMasks.GetRef().push_back(refMask);
            }
        } else if (CBlast4Field::Get(eBlastOpt_LongestIntronLength).Match(p)) {
            bo.SetLongestIntronLength(v.GetInteger());
        } else {
            found = false;
        }
        break;
        
    case 'M':
        if (CBlast4Field::Get(eBlastOpt_MBTemplateLength).Match(p)) {
            bo.SetMBTemplateLength(v.GetInteger());
        } else if (CBlast4Field::Get(eBlastOpt_MBTemplateType).Match(p)) {
            bo.SetMBTemplateType(v.GetInteger());
        } else if (CBlast4Field::Get(eBlastOpt_MatchReward).Match(p)) {
            bo.SetMatchReward(v.GetInteger());
        } else if (CBlast4Field::Get(eBlastOpt_MatrixName).Match(p)) {
            bo.SetMatrixName(v.GetString().c_str());
        } else if (CBlast4Field::Get(eBlastOpt_MatrixTable).Match(p)) {
            // This is no longer used.
        } else if (CBlast4Field::Get(eBlastOpt_MismatchPenalty).Match(p)) {
            bo.SetMismatchPenalty(v.GetInteger());
        } else if (CBlast4Field::Get(eBlastOpt_MaskAtHash).Match(p)) {
            bo.SetMaskAtHash(v.GetBoolean());
        } else if (CBlast4Field::Get(eBlastOpt_MbIndexName).Match(p)) {
            m_MbIndexName = v.GetString();
        } else {
            found = false;
        }
        break;
        
    case 'O':
        if (CBlast4Field::Get(eBlastOpt_OutOfFrameMode).Match(p)) {
            bo.SetOutOfFrameMode(v.GetBoolean());
        } else {
            found = false;
        }
        break;

    case 'N':
        if (CBlast4Field::Get(eBlastOpt_NegativeGiList).Match(p)) {
            m_NegativeGiList = v.GetInteger_list();
        } else {
            found = false;
        }
        break;
        
    case 'P':
        if (CBlast4Field::Get(eBlastOpt_PHIPattern).Match(p)) {
            if (v.GetString() != "") {
                bool is_na = !! Blast_QueryIsNucleotide(bo.GetProgramType());
                bo.SetPHIPattern(v.GetString().c_str(), is_na);
            }
        } else if (CBlast4Field::Get(eBlastOpt_PercentIdentity).Match(p)) {
            opts.SetPercentIdentity(v.GetReal());
        } else if (CBlast4Field::Get(eBlastOpt_PseudoCount).Match(p)) {
            bo.SetPseudoCount(v.GetInteger());
        } else {
            found = false;
        }
        break;
        
    case 'Q':
        if (CBlast4Field::Get(eBlastOpt_QueryGeneticCode).Match(p)) {
            bo.SetQueryGeneticCode(v.GetInteger());
        } else {
            found = false;
        }
        break;
        
    case 'R':
        if (CBlast4Field::Get(eBlastOpt_RepeatFiltering).Match(p)) {
            bo.SetRepeatFiltering(v.GetBoolean());
        } else if (CBlast4Field::Get(eBlastOpt_RepeatFilteringDB).Match(p)) {
            bo.SetRepeatFilteringDB(v.GetString().c_str());
        } else if (CBlast4Field::Get(eBlastOpt_RequiredStart).Match(p)) {
            m_QueryRange.SetFrom(v.GetInteger());
        } else if (CBlast4Field::Get(eBlastOpt_RequiredEnd).Match(p)) {
            m_QueryRange.SetToOpen(v.GetInteger());
        } else {
            found = false;
        }
        break;
        
    case 'S':
        if (CBlast4Field::Get(eBlastOpt_StrandOption).Match(p)) {
            // These encodings use the same values.
            ENa_strand strand = (ENa_strand) v.GetStrand_type();
            bo.SetStrandOption(strand);
        } else if (CBlast4Field::Get(eBlastOpt_SegFiltering).Match(p)) {
            bo.SetSegFiltering(v.GetBoolean());
        } else if (CBlast4Field::Get(eBlastOpt_SegFilteringWindow).Match(p)) {
            bo.SetSegFilteringWindow(v.GetInteger());
        } else if (CBlast4Field::Get(eBlastOpt_SegFilteringLocut).Match(p)) {
            bo.SetSegFilteringLocut(v.GetReal());
        } else if (CBlast4Field::Get(eBlastOpt_SegFilteringHicut).Match(p)) {
            bo.SetSegFilteringHicut(v.GetReal());
        } else if (CBlast4Field::Get(eBlastOpt_SumStatisticsMode).Match(p)) {
            bo.SetSumStatisticsMode(v.GetBoolean());
        } else if (CBlast4Field::Get(eBlastOpt_SmithWatermanMode).Match(p)) {
            bo.SetSmithWatermanMode(v.GetBoolean());
        } else if (CBlast4Field::Get(eBlastOpt_SubjectMaskingType).Match(p)) {
            m_SubjectMaskingType = (ESubjectMaskingType) v.GetInteger();
        } else {
            found = false;
        }
        break;
        
    case 'U':
        if (CBlast4Field::Get(eBlastOpt_GappedMode).Match(p)) {
            // Notes: (1) this is the inverse of the corresponding
            // blast4 concept (2) blast4 always returns this option
            // regardless of whether the value matches the default.
            
            opts.SetGappedMode(! v.GetBoolean());
        } else if (CBlast4Field::Get(eBlastOpt_UnifiedP).Match(p)) {
            bo.SetUnifiedP(v.GetInteger());
        } else {
            found = false;
        }
        break;
        
    case 'W':
        if (CBlast4Field::Get(eBlastOpt_WindowMaskerTaxId).Match(p)) {
            opts.SetOptions().SetWindowMaskerTaxId(v.GetInteger());
        } else if (CBlast4Field::Get(eBlastOpt_WindowSize).Match(p)) {
            opts.SetWindowSize(v.GetInteger());
        } else if (CBlast4Field::Get(eBlastOpt_WordSize).Match(p)) {
            bo.SetWordSize(v.GetInteger());
        } else if (CBlast4Field::Get(eBlastOpt_WordThreshold).Match(p)) {
            bo.SetWordThreshold(v.GetInteger());
        } else {
            found = false;
        }
        break;
        
    default:
        found = false;
    }

    if (! found) {
        if (m_IgnoreUnsupportedOptions)
            return;

        string msg = "Internal: Error processing option [";
        msg += nm;
        msg += "] type [";
        msg += NStr::IntToString((int) v.Which());
        msg += "].";
        
        NCBI_THROW(CRemoteBlastException,
                   eServiceNotAvailable,
                   msg);
    }
}

void
CBlastOptionsBuilder::x_ProcessOptions(CBlastOptionsHandle & opts,
                                       const TValueList    * L)
{
    if ( !L ) {
        return;
    }

    ITERATE(TValueList, iter, *L) {
        CBlast4_parameter & p = *const_cast<CBlast4_parameter*>(& **iter);
        x_ProcessOneOption(opts, p);
    }
}

void CBlastOptionsBuilder::x_ApplyInteractions(CBlastOptionsHandle & boh)
{
    CBlastOptions & bo = boh.SetOptions();
    
    if (m_PerformCulling) {
        bo.SetCullingLimit(m_HspRangeMax);
    }
    if (m_ForceMbIndex) {
        bo.SetUseIndex(true, m_MbIndexName, m_ForceMbIndex);
    }
}

/// Finder class for matching CBlast4_parameter
struct SBlast4ParamFinder : public unary_function< CRef<CBlast4_parameter>, bool> {
    SBlast4ParamFinder(EBlastOptIdx opt_idx)
            : m_Target2Find(CBlast4Field::Get(opt_idx)) {}
    result_type operator()(const argument_type& rhs) {
        return rhs.NotEmpty() ? m_Target2Find.Match(*rhs) : false;
    }
private:
    CBlast4Field& m_Target2Find;
};

EProgram
CBlastOptionsBuilder::AdjustProgram(const TValueList * L,
                                    EProgram           program,
                                    const string     & program_string)
{
    if ( !L ) {
        return program;
    }

    // PHI-BLAST pattern trumps all other options
    if (find_if(L->begin(), L->end(), 
                SBlast4ParamFinder(eBlastOpt_PHIPattern)) != L->end()) {
        switch(program) {
        case ePHIBlastn:
        case eBlastn:
            _TRACE("Adjusting program to phiblastn");
            return ePHIBlastn;
            
        case ePHIBlastp:
        case eBlastp:
            _TRACE("Adjusting program to phiblastp");
            return ePHIBlastp;
            
        default:
            {
                string msg = "Incorrect combination of option (";
                msg += CBlast4Field::GetName(eBlastOpt_PHIPattern);
                msg += ") and program (";
                msg += program_string;
                msg += ")";
                
                NCBI_THROW(CRemoteBlastException,
                           eServiceNotAvailable,
                           msg);
            }
            break;
        }
    }
    
    ITERATE(TValueList, iter, *L) {
        CBlast4_parameter & p = const_cast<CBlast4_parameter&>(**iter);
        const CBlast4_value & v = p.GetValue();
        
        if (CBlast4Field::Get(eBlastOpt_MBTemplateLength).Match(p)) {
            if (v.GetInteger() != 0) {
                _TRACE("Adjusting program to discontiguous Megablast");
                return eDiscMegablast;
            }
        } else if (CBlast4Field::Get(eBlastOpt_Web_StepNumber).Match(p) ||
                   CBlast4Field::Get(eBlastOpt_Web_RunPsiBlast).Match(p) ||
                   CBlast4Field::Get(eBlastOpt_PseudoCount).Match(p) ||
                   CBlast4Field::Get(eBlastOpt_IgnoreMsaMaster).Match(p)
                   ) {
            // FIXME: should we handle DELTA-BLAST here too?
            _TRACE("Adjusting program to psiblast");
            return ePSIBlast;
        }
    }
    
    return program;
}

/// Convenience function to merge all lists into one object to facilitate
/// invoking AdjustProgram
static void
s_MergeCBlast4_parameters(const objects::CBlast4_parameters* aopts,
                          const objects::CBlast4_parameters* popts,
                          const objects::CBlast4_parameters* fopts,
                          objects::CBlast4_parameters& retval)
{
    retval.Set().clear();
    if (aopts) {
        retval.Set().insert(retval.Set().end(), aopts->Get().begin(), aopts->Get().end());
    }
    if (popts) {
        retval.Set().insert(retval.Set().end(), popts->Get().begin(), popts->Get().end());
    }
    if (fopts) {
        retval.Set().insert(retval.Set().end(), fopts->Get().begin(), fopts->Get().end());
    }
}

CRef<CBlastOptionsHandle> CBlastOptionsBuilder::
GetSearchOptions(const objects::CBlast4_parameters * aopts,
                 const objects::CBlast4_parameters * popts,
                 const objects::CBlast4_parameters* fopts,
                 string *task_name)
{
    EProgram program = ComputeProgram(m_Program, m_Service);
    objects::CBlast4_parameters all_params;
    s_MergeCBlast4_parameters(aopts, popts, fopts, all_params);
    program = AdjustProgram(&all_params.Get(), program, m_Program);
    
    // Using eLocal allows more of the options to be returned to the user.
    
    CRef<CBlastOptionsHandle>
        cboh(CBlastOptionsFactory::Create(program, m_Locality));
    
    if (task_name != NULL) 
        *task_name = EProgramToTaskName(program);
    
    m_IgnoreQueryMasks = false;
    x_ProcessOptions(*cboh, (aopts == NULL ? 0 : &aopts->Get()));

    m_IgnoreQueryMasks = m_QueryMasks.Have();
    x_ProcessOptions(*cboh, (popts == NULL ? 0 : &popts->Get()));
    
    x_ApplyInteractions(*cboh);
    
    return cboh;
}

bool CBlastOptionsBuilder::HaveEntrezQuery()
{
    return m_EntrezQuery.Have();
}

string CBlastOptionsBuilder::GetEntrezQuery()
{
    return m_EntrezQuery.Get();
}

bool CBlastOptionsBuilder::HaveFirstDbSeq()
{
    return m_FirstDbSeq.Have();
}

int CBlastOptionsBuilder::GetFirstDbSeq()
{
    return m_FirstDbSeq.Get();
}

bool CBlastOptionsBuilder::HaveFinalDbSeq()
{
    return m_FinalDbSeq.Have();
}

int CBlastOptionsBuilder::GetFinalDbSeq()
{
    return m_FinalDbSeq.Get();
}

bool CBlastOptionsBuilder::HaveGiList()
{
    return m_GiList.Have();
}

list<int> CBlastOptionsBuilder::GetGiList()
{
    return m_GiList.Get();
}

bool CBlastOptionsBuilder::HasDbFilteringAlgorithmId()
{
    return m_DbFilteringAlgorithmId.Have();
}

int CBlastOptionsBuilder::GetDbFilteringAlgorithmId()
{
    return m_DbFilteringAlgorithmId.Get();
}

bool CBlastOptionsBuilder::HasDbFilteringAlgorithmKey()
{
    return m_DbFilteringAlgorithmKey.Have();
}

string CBlastOptionsBuilder::GetDbFilteringAlgorithmKey()
{
    return m_DbFilteringAlgorithmKey.Get();
}

bool CBlastOptionsBuilder::HasSubjectMaskingType()
{
    return m_SubjectMaskingType.Have();
}

ESubjectMaskingType CBlastOptionsBuilder::GetSubjectMaskingType()
{
    return m_SubjectMaskingType.Get();
}


bool CBlastOptionsBuilder::HaveNegativeGiList()
{
    return m_NegativeGiList.Have();
}

list<int> CBlastOptionsBuilder::GetNegativeGiList()
{
    return m_NegativeGiList.Get();
}

bool CBlastOptionsBuilder::HaveQueryMasks()
{
    return m_QueryMasks.Have();
}

CBlastOptionsBuilder::TMaskList
    CBlastOptionsBuilder::GetQueryMasks()
{
    return m_QueryMasks.Get();
}

void CBlastOptionsBuilder::SetIgnoreUnsupportedOptions(bool ignore)
{
    m_IgnoreUnsupportedOptions = ignore;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
