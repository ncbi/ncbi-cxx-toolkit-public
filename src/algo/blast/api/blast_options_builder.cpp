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
                     CBlastOptions::EAPILocality   locality)
    : m_Program        (program),
      m_Service        (service),
      m_PerformCulling (false),
      m_HspRangeMax    (0),
      m_QueryRange     (TSeqRange::GetEmpty()),
      m_Locality       (locality),
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
    // b. others, like vecscreen, disco?
    
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
        }
    } else if (p == "blastn") {
        if (s == "megablast") {
            p = "megablast";
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
        if (B4Param_BestHitScoreEdge.Match(p)) {
            bo.SetBestHitScoreEdge(v.GetReal());
        } else if (B4Param_BestHitOverhang.Match(p)) {
            bo.SetBestHitOverhang(v.GetReal());
        } else {
            found = false;
        }
        break;

    case 'C':
        if (B4Param_CompositionBasedStats.Match(p)) {
            ECompoAdjustModes adjmode = (ECompoAdjustModes) v.GetInteger();
            bo.SetCompositionBasedStats(adjmode);
        } else if (B4Param_Culling.Match(p)) {
            m_PerformCulling = v.GetBoolean();
        } else if (B4Param_CullingLimit.Match(p)) {
            bo.SetCullingLimit(v.GetInteger());
        } else if (B4Param_CutoffScore.Match(p)) {
            opts.SetCutoffScore(v.GetInteger());
        } else {
            found = false;
        }
        break;
        
    case 'D':
        if (B4Param_DbGeneticCode.Match(p)) {
            bo.SetDbGeneticCode(v.GetInteger());
        } else if (B4Param_DbLength.Match(p)) {
            opts.SetDbLength(v.GetBig_integer());
        } else if (B4Param_DustFiltering.Match(p)) {
            bo.SetDustFiltering(v.GetBoolean());
        } else if (B4Param_DustFilteringLevel.Match(p)) {
            bo.SetDustFilteringLevel(v.GetInteger());
        } else if (B4Param_DustFilteringWindow.Match(p)) {
            bo.SetDustFilteringWindow(v.GetInteger());
        } else if (B4Param_DustFilteringLinker.Match(p)) {
            bo.SetDustFilteringLinker(v.GetInteger());
        } else if (B4Param_DbFilteringAlgorithmId.Match(p)) {
            m_DbFilteringAlgorithmId = v.GetInteger();
        } else {
            found = false;
        }
        break;
        
    case 'E':
        if (B4Param_EffectiveSearchSpace.Match(p)) {
            opts.SetEffectiveSearchSpace(v.GetBig_integer());
        } else if (B4Param_EntrezQuery.Match(p)) {
            m_EntrezQuery = v.GetString();
        } else if (B4Param_EvalueThreshold.Match(p)
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
        if (B4Param_FilterString.Match(p)) {
            opts.SetFilterString(v.GetString().c_str(), true);  /* NCBI_FAKE_WARNING */
        } else if (B4Param_FinalDbSeq.Match(p)) {
            m_FinalDbSeq = v.GetInteger();
        } else if (B4Param_FirstDbSeq.Match(p)) {
            m_FirstDbSeq = v.GetInteger();
        } else if (B4Param_ForceMbIndex.Match(p)) {
            m_ForceMbIndex = v.GetBoolean();
        } else {
            found = false;
        }
        break;
        
    case 'G':
        if (B4Param_GapExtensionCost.Match(p)) {
            bo.SetGapExtensionCost(v.GetInteger());
        } else if (B4Param_GapOpeningCost.Match(p)) {
            bo.SetGapOpeningCost(v.GetInteger());
        } else if (B4Param_GiList.Match(p)) {
            m_GiList = v.GetInteger_list();
        } else if (B4Param_GapTracebackAlgorithm.Match(p)) {
            bo.SetGapTracebackAlgorithm((EBlastTbackExt) v.GetInteger());
        } else if (B4Param_GapTrigger.Match(p)) {
            bo.SetGapTrigger(v.GetReal());
        } else if (B4Param_GapXDropoff.Match(p)) {
            bo.SetGapXDropoff(v.GetReal());
        } else if (B4Param_GapXDropoffFinal.Match(p)) {
            bo.SetGapXDropoffFinal(v.GetReal());
        } else if (B4Param_GapExtnAlgorithm.Match(p)) {
            bo.SetGapExtnAlgorithm(static_cast<EBlastPrelimGapExt>
                                   (v.GetInteger()));
        } else {
            found = false;
        }
        break;
        
    case 'H':
        if (B4Param_HitlistSize.Match(p)) {
            opts.SetHitlistSize(v.GetInteger());
        } else if (B4Param_HspRangeMax.Match(p)) {
            m_HspRangeMax = v.GetInteger();
        } else {
            found = false;
        }
        break;

    case 'I':
        if (B4Param_InclusionThreshold.Match(p)) {
            bo.SetInclusionThreshold(v.GetReal());
        } else {
            found = false;
        }
        break;
        
    case 'L':
        if (B4Param_LCaseMask.Match(p))
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
        } else if (B4Param_LongestIntronLength.Match(p)) {
            bo.SetLongestIntronLength(v.GetInteger());
        } else {
            found = false;
        }
        break;
        
    case 'M':
        if (B4Param_MBTemplateLength.Match(p)) {
            bo.SetMBTemplateLength(v.GetInteger());
        } else if (B4Param_MBTemplateType.Match(p)) {
            bo.SetMBTemplateType(v.GetInteger());
        } else if (B4Param_MatchReward.Match(p)) {
            bo.SetMatchReward(v.GetInteger());
        } else if (B4Param_MatrixName.Match(p)) {
            bo.SetMatrixName(v.GetString().c_str());
        } else if (B4Param_MatrixTable.Match(p)) {
            // This is no longer used.
        } else if (B4Param_MismatchPenalty.Match(p)) {
            bo.SetMismatchPenalty(v.GetInteger());
        } else if (B4Param_MaskAtHash.Match(p)) {
            bo.SetMaskAtHash(v.GetBoolean());
        } else if (B4Param_MbIndexName.Match(p)) {
            m_MbIndexName = v.GetString();
        } else {
            found = false;
        }
        break;
        
    case 'O':
        if (B4Param_OutOfFrameMode.Match(p)) {
            bo.SetOutOfFrameMode(v.GetBoolean());
        } else {
            found = false;
        }
        break;

    case 'N':
        if (B4Param_NegativeGiList.Match(p)) {
            m_NegativeGiList = v.GetInteger_list();
        } else {
            found = false;
        }
        break;
        
    case 'P':
        if (B4Param_PHIPattern.Match(p)) {
            if (v.GetString() != "") {
                bool is_na = !! Blast_QueryIsNucleotide(bo.GetProgramType());
                bo.SetPHIPattern(v.GetString().c_str(), is_na);
            }
        } else if (B4Param_PercentIdentity.Match(p)) {
            opts.SetPercentIdentity(v.GetReal());
        } else if (B4Param_PseudoCountWeight.Match(p)) {
            bo.SetPseudoCount(v.GetInteger());
        } else {
            found = false;
        }
        break;
        
    case 'Q':
        if (B4Param_QueryGeneticCode.Match(p)) {
            bo.SetQueryGeneticCode(v.GetInteger());
        } else {
            found = false;
        }
        break;
        
    case 'R':
        if (B4Param_RepeatFiltering.Match(p)) {
            bo.SetRepeatFiltering(v.GetBoolean());
        } else if (B4Param_RepeatFilteringDB.Match(p)) {
            bo.SetRepeatFilteringDB(v.GetString().c_str());
        } else if (B4Param_RequiredStart.Match(p)) {
            m_QueryRange.SetFrom(v.GetInteger());
        } else if (B4Param_RequiredEnd.Match(p)) {
            m_QueryRange.SetToOpen(v.GetInteger());
        } else {
            found = false;
        }
        break;
        
    case 'S':
        if (B4Param_StrandOption.Match(p)) {
            // These encodings use the same values.
            ENa_strand strand = (ENa_strand) v.GetStrand_type();
            bo.SetStrandOption(strand);
        } else if (B4Param_SegFiltering.Match(p)) {
            bo.SetSegFiltering(v.GetBoolean());
        } else if (B4Param_SegFilteringWindow.Match(p)) {
            bo.SetSegFilteringWindow(v.GetInteger());
        } else if (B4Param_SegFilteringLocut.Match(p)) {
            bo.SetSegFilteringLocut(v.GetReal());
        } else if (B4Param_SegFilteringHicut.Match(p)) {
            bo.SetSegFilteringHicut(v.GetReal());
        } else if (B4Param_SumStatistics.Match(p)) {
            bo.SetSumStatisticsMode(v.GetBoolean());
        } else if (B4Param_SmithWatermanMode.Match(p)) {
            bo.SetSmithWatermanMode(v.GetBoolean());
        } else {
            found = false;
        }
        break;
        
    case 'U':
        if (B4Param_UngappedMode.Match(p)) {
            // Notes: (1) this is the inverse of the corresponding
            // blast4 concept (2) blast4 always returns this option
            // regardless of whether the value matches the default.
            
            opts.SetGappedMode(! v.GetBoolean());
        } else if (B4Param_UnifiedP.Match(p)) {
            bo.SetUnifiedP(v.GetInteger());
        } else {
            found = false;
        }
        break;
        
    case 'W':
        if (B4Param_WindowSize.Match(p)) {
            opts.SetWindowSize(v.GetInteger());
        } else if (B4Param_WordSize.Match(p)) {
            bo.SetWordSize(v.GetInteger());
        } else if (B4Param_WordThreshold.Match(p)) {
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

EProgram
CBlastOptionsBuilder::AdjustProgram(const TValueList * L,
                                    EProgram           program,
                                    const string     & program_string)
{
    bool problem = false;
    string name;

    if ( !L ) {
        return program;
    }
    
    ITERATE(TValueList, iter, *L) {
        CBlast4_parameter & p = const_cast<CBlast4_parameter&>(**iter);
        const CBlast4_value & v = p.GetValue();
        
        if (B4Param_MBTemplateLength.Match(p)) {
            if (v.GetInteger() != 0) {
                return eDiscMegablast;
            }
        } else if (B4Param_PHIPattern.Match(p)) {
            switch(program) {
            case ePHIBlastn:
            case eBlastn:
                return ePHIBlastn;
                
            case ePHIBlastp:
            case eBlastp:
                return ePHIBlastp;
                
            default:
                problem = true;
                break;
            }
        }
        
        if (problem) {
            name = (**iter).GetName();
            
            string msg = "Incorrect combination of option (";
            msg += name;
            msg += ") and program (";
            msg += program_string;
            msg += ")";
            
            NCBI_THROW(CRemoteBlastException,
                       eServiceNotAvailable,
                       msg);
        }
    }
    
    return program;
}

CRef<CBlastOptionsHandle> CBlastOptionsBuilder::
GetSearchOptions(const objects::CBlast4_parameters * aopts,
                 const objects::CBlast4_parameters * popts,
                 string *task_name)
{
    EProgram program = ComputeProgram(m_Program, m_Service);
    
    program = AdjustProgram((aopts == NULL ? 0 : &aopts->Get()), 
                            program, m_Program);
    
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
