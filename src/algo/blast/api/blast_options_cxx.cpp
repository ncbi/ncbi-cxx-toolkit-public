#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

/* ===========================================================================
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

/// @file blast_options_cxx.cpp
/// Implements the CBlastOptions class, which encapsulates options structures
/// from algo/blast/core

#include <ncbi_pch.hpp>
#include <algo/blast/api/blast_options.hpp>
#include "blast_setup.hpp"
#include "blast_options_local_priv.hpp"
#include "blast_memento_priv.hpp"

#include <algo/blast/core/blast_extend.h>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/blast/Blast4_cutoff.hpp>
#include <objects/blast/names.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

#ifndef SKIP_DOXYGEN_PROCESSING

/// Index of remote BLAST options.
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
    eBlastOpt_SumStatisticsMode,
    eBlastOpt_LongestIntronLength,
    eBlastOpt_GappedMode,
    eBlastOpt_ComplexityAdjMode,  // -RMH-
    eBlastOpt_MaskLevel,  // -RMH-
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
    eBlastOpt_BestHitOverhang
};

/// Encapsulates all blast input parameters
class NCBI_XBLAST_EXPORT CBlastOptionsRemote : public CObject
{
public:
    CBlastOptionsRemote(void)
        : m_DefaultsMode(false)
    {
        m_ReqOpts.Reset(new objects::CBlast4_parameters);
    }
    
    ~CBlastOptionsRemote()
    {
    }

    /// Copy constructor
    CBlastOptionsRemote(const CBlastOptionsRemote& optsRemote)
        : m_DefaultsMode(false)
    {
        x_DoDeepCopy(optsRemote);
    }

    /// Assignment operator
    CBlastOptionsRemote& operator=(const CBlastOptionsRemote& optsRemote)
    {
        x_DoDeepCopy(optsRemote);
        return *this;
    }
    
    // the "new paradigm"
    typedef ncbi::objects::CBlast4_parameters TBlast4Opts;
    TBlast4Opts * GetBlast4AlgoOpts()
    {
        return m_ReqOpts;
    }
    
    typedef vector< CConstRef<objects::CSeq_loc> > TSeqLocVector;
    
    // SetValue(x,y) with different types:
    void SetValue(EBlastOptIdx opt, const EProgram            & x);
    void SetValue(EBlastOptIdx opt, const int                 & x);
    void SetValue(EBlastOptIdx opt, const double              & x);
    void SetValue(EBlastOptIdx opt, const char                * x);
    void SetValue(EBlastOptIdx opt, const TSeqLocVector       & x);
    void SetValue(EBlastOptIdx opt, const ESeedContainerType  & x);
    void SetValue(EBlastOptIdx opt, const bool                & x);
    void SetValue(EBlastOptIdx opt, const Int8                & x);
    
    // Pseudo-types:
    void SetValue(EBlastOptIdx opt, const short & x)
    {
        int x2 = x; SetValue(opt, x2);
    }
    
    void SetValue(EBlastOptIdx opt, const unsigned int & x)
    {
        int x2 = x; SetValue(opt, x2);
    }
    
    void SetValue(EBlastOptIdx opt, const unsigned char & x)
    {
        int x2 = x; SetValue(opt, x2);
    }
    
    void SetValue(EBlastOptIdx opt, const objects::ENa_strand & x)
    {
        int x2 = x; SetValue(opt, x2);
    }
    
    /// Remove any objects matching this Blast4 field object.
    /// 
    /// The given field object represents a Blast4 field to remove
    /// from the list of remote options.
    /// 
    /// @param opt Field object representing option to remove.
    void ResetValue(CBlast4Field & opt)
    {
        x_ResetValue(opt);
    }
    
    void SetDefaultsMode(bool dmode)
    {
        m_DefaultsMode = dmode;
    }
    
    bool GetDefaultsMode()
    {
        return m_DefaultsMode;
    }
    
    
private:
    //CRef<objects::CBlast4_queue_search_request> m_Req;
    CRef<objects::CBlast4_parameters> m_ReqOpts;
    
    bool m_DefaultsMode;
    
    /// Perform a "deep copy" of remote Blast options
    /// @param optsRemote remote Blast options object to copy from.
    void x_DoDeepCopy(const CBlastOptionsRemote& optsRemote)
    {
        if (&optsRemote != this)
        {
            m_ReqOpts.Reset(new objects::CBlast4_parameters);
            m_ReqOpts->Assign(*optsRemote.m_ReqOpts);
            m_DefaultsMode = optsRemote.m_DefaultsMode;
        }
    }

    template<class T>
    void x_SetParam(CBlast4Field & name, T & value)
    {
        x_SetOneParam(name, & value);
    }
    
    void x_SetOneParam(CBlast4Field & field, const int * x)
    {
        CRef<objects::CBlast4_value> v(new objects::CBlast4_value);
        v->SetInteger(*x);
        
        CRef<objects::CBlast4_parameter> p(new objects::CBlast4_parameter);
        p->SetName(field.GetName());
        p->SetValue(*v);
        
        x_AttachValue(p);
    }
    
    void x_SetOneParam(CBlast4Field & field, const char ** x)
    {
        CRef<objects::CBlast4_value> v(new objects::CBlast4_value);
        v->SetString().assign((x && (*x)) ? (*x) : "");
        
        CRef<objects::CBlast4_parameter> p(new objects::CBlast4_parameter);
        p->SetName(field.GetName());
        p->SetValue(*v);
        
        x_AttachValue(p);
    }
    
    void x_SetOneParam(CBlast4Field & field, const bool * x)
    {
        CRef<objects::CBlast4_value> v(new objects::CBlast4_value);
        v->SetBoolean(*x);
        
        CRef<objects::CBlast4_parameter> p(new objects::CBlast4_parameter);
        p->SetName(field.GetName());
        p->SetValue(*v);
        
        x_AttachValue(p);
    }
    
    void x_SetOneParam(CBlast4Field & field, CRef<objects::CBlast4_cutoff> * x)
    {
        CRef<objects::CBlast4_value> v(new objects::CBlast4_value);
        v->SetCutoff(**x);
        
        CRef<objects::CBlast4_parameter> p(new objects::CBlast4_parameter);
        p->SetName(field.GetName());
        p->SetValue(*v);
        
        x_AttachValue(p);
    }
    
    void x_SetOneParam(CBlast4Field & field, const double * x)
    {
        CRef<objects::CBlast4_value> v(new objects::CBlast4_value);
        v->SetReal(*x);
        
        CRef<objects::CBlast4_parameter> p(new objects::CBlast4_parameter);
        p->SetName(field.GetName());
        p->SetValue(*v);
        
        x_AttachValue(p);
    }
    
    void x_SetOneParam(CBlast4Field & field, const Int8 * x)
    {
        CRef<objects::CBlast4_value> v(new objects::CBlast4_value);
        v->SetBig_integer(*x);
        
        CRef<objects::CBlast4_parameter> p(new objects::CBlast4_parameter);
        p->SetName(field.GetName());
        p->SetValue(*v);
        
        x_AttachValue(p);
    }
    
    void x_SetOneParam(CBlast4Field & field, objects::EBlast4_strand_type * x)
    {
        CRef<objects::CBlast4_value> v(new objects::CBlast4_value);
        v->SetStrand_type(*x);
        
        CRef<objects::CBlast4_parameter> p(new objects::CBlast4_parameter);
        p->SetName(field.GetName());
        p->SetValue(*v);
        
        x_AttachValue(p);
    }
    
    void x_AttachValue(CRef<objects::CBlast4_parameter> p)
    {
        typedef objects::CBlast4_parameter TParam;
        
        NON_CONST_ITERATE(list< CRef<TParam> >, iter, m_ReqOpts->Set()) {
            if ((**iter).GetName() == p->GetName()) {
                (*iter) = p;
                return;
            }
        }
        
        m_ReqOpts->Set().push_back(p);
    }
    
    /// Remove values for a given Blast4 field.
    /// @param f Field to search for and remove.
    void x_ResetValue(CBlast4Field & f)
    {
        typedef list< CRef<objects::CBlast4_parameter> > TParamList;
        typedef TParamList::iterator TParamIter;
        
        const string & nm = f.GetName();
        TParamList & lst = m_ReqOpts->Set();
        TParamIter pos = lst.begin(), end = lst.end();
        
        while(pos != end) {
            TParamIter current = pos;
            pos++;
            
            if ((**current).GetName() == nm) {
                lst.erase(current);
            }
        }
    }
    
    void x_Throwx(const string& msg) const
    {
        NCBI_THROW(CBlastException, eInvalidOptions, msg);
    }
};


CBlastOptions::CBlastOptions(EAPILocality locality)
    : m_Local (0),
      m_Remote(0),
      m_DefaultsMode(false)
{
    if (locality == eRemote)
        locality = eBoth;
    
    if (locality != eRemote) {
        m_Local = new CBlastOptionsLocal();
    }
    if (locality != eLocal) {
        m_Remote = new CBlastOptionsRemote();
    }
}

CBlastOptions::~CBlastOptions()
{
    if (m_Local) {
        delete m_Local;
    }
    if (m_Remote) {
        delete m_Remote;
    }
}

CRef<CBlastOptions> CBlastOptions::Clone() const
{
    CRef<CBlastOptions> optsRef;
    optsRef.Reset(new CBlastOptions(GetLocality()));
    optsRef->x_DoDeepCopy(*this);
    return optsRef;
}

CBlastOptions::EAPILocality 
CBlastOptions::GetLocality(void) const
{
    if (! m_Remote) {
        return eLocal;
    }
    if (! m_Local) {
        return eRemote;
    }
    return eBoth;
}

// Note: only some of the options are supported for the remote case;
// An exception is thrown if the option is not available.

void CBlastOptionsRemote::SetValue(EBlastOptIdx opt, const EProgram & v)
{
    if (m_DefaultsMode) {
        return;
    }
    
    switch(opt) {
    case eBlastOpt_Program:
        return;
        
    default:
        break;
    }
    
    char errbuf[1024];
    
    sprintf(errbuf, "tried to set option (%d) and value (%d), line (%d).",
            int(opt), v, __LINE__);
    
    x_Throwx(string("err:") + errbuf);
}

void CBlastOptionsRemote::SetValue(EBlastOptIdx opt, const int & v)
{
    if (m_DefaultsMode) {
        return;
    }
    
    switch(opt) {
    case eBlastOpt_WordSize:
        x_SetParam(B4Param_WordSize, v);
        return;

    // Added for rmblastn and the new masklevel option. -RMH-
    case eBlastOpt_MaskLevel:
         x_SetParam(B4Param_MaskLevel,v);
         return;

    case eBlastOpt_LookupTableType: 
        // do nothing, should be specified by the task
        return;
        
    case eBlastOpt_StrandOption:
        {
            typedef objects::EBlast4_strand_type TSType;
            TSType strand;
            bool set_strand = true;
            
            switch(v) {
            case 1:
                strand = eBlast4_strand_type_forward_strand;
                break;
                
            case 2:
                strand = eBlast4_strand_type_reverse_strand;
                break;
                
            case 3:
                strand = eBlast4_strand_type_both_strands;
                break;
                
            default:
                set_strand = false;
            }
            
            if (set_strand) {
                x_SetParam(B4Param_StrandOption, strand);
                return;
            }
        }
        
    case eBlastOpt_WindowSize:
        x_SetParam(B4Param_WindowSize, v);
        return;

    case eBlastOpt_GapOpeningCost:
        x_SetParam(B4Param_GapOpeningCost, v);
        return;
        
    case eBlastOpt_GapExtensionCost:
        x_SetParam(B4Param_GapExtensionCost, v);
        return;
        
    case eBlastOpt_HitlistSize:
        x_SetParam(B4Param_HitlistSize, v);
        return;
        
    case eBlastOpt_CutoffScore:
        if (0) {
            typedef objects::CBlast4_cutoff TCutoff;
            CRef<TCutoff> cutoff(new TCutoff);
            cutoff->SetRaw_score(v);
            
            x_SetParam(B4Param_CutoffScore, cutoff);
        }
        return;
        
    case eBlastOpt_MatchReward:
        x_SetParam(B4Param_MatchReward, v);
        return;
        
    case eBlastOpt_MismatchPenalty:
        x_SetParam(B4Param_MismatchPenalty, v);
        return;

    case eBlastOpt_WordThreshold:
        x_SetParam(B4Param_WordThreshold, v);
        return;
        
    case eBlastOpt_PseudoCount:
        x_SetParam(B4Param_PseudoCountWeight, v);
        return;
        
    case eBlastOpt_CompositionBasedStats:
        if (v < eNumCompoAdjustModes) {
            x_SetParam(B4Param_CompositionBasedStats, v);
            return;
        }
        
    case eBlastOpt_MBTemplateLength:
        x_SetParam(B4Param_MBTemplateLength, v);
        return;
        
    case eBlastOpt_MBTemplateType:
        x_SetParam(B4Param_MBTemplateType, v);
        return;
        
    case eBlastOpt_GapExtnAlgorithm:
        x_SetParam(B4Param_GapExtnAlgorithm, v);
        return;
        
    case eBlastOpt_GapTracebackAlgorithm:
        x_SetParam(B4Param_GapTracebackAlgorithm, v);
        return;
        
    case eBlastOpt_SegFilteringWindow:
        x_SetParam(B4Param_SegFilteringWindow, v);
        return;

    case eBlastOpt_DustFilteringLevel:
        x_SetParam(B4Param_DustFilteringLevel, v);
        return;

    case eBlastOpt_DustFilteringWindow:
        x_SetParam(B4Param_DustFilteringWindow, v);
        return;

    case eBlastOpt_DustFilteringLinker:
        x_SetParam(B4Param_DustFilteringLinker, v);
        return;

    case eBlastOpt_CullingLimit:
        x_SetParam(B4Param_CullingLimit, v);
        return;

    case eBlastOpt_LongestIntronLength:
        x_SetParam(B4Param_LongestIntronLength, v);
        return;

    case eBlastOpt_QueryGeneticCode:
        x_SetParam(B4Param_QueryGeneticCode, v);
        return;

    case eBlastOpt_DbGeneticCode:
        x_SetParam(B4Param_DbGeneticCode, v);
        return;

    case eBlastOpt_UnifiedP:
        x_SetParam(B4Param_UnifiedP, v);
        return;

    case eBlastOpt_WindowMaskerTaxId:
        x_SetParam(B4Param_WindowMaskerTaxId, v);
        return;

    default:
        break;
    }
    
    char errbuf[1024];
    
    sprintf(errbuf, "tried to set option (%d) and value (%d), line (%d).",
            int(opt), v, __LINE__);
    
    x_Throwx(string("err:") + errbuf);
}

void CBlastOptionsRemote::SetValue(EBlastOptIdx opt, const double & v)
{
    if (m_DefaultsMode) {
        return;
    }
    
    switch(opt) {
    case eBlastOpt_EvalueThreshold:
        {
            typedef objects::CBlast4_cutoff TCutoff;
            CRef<TCutoff> cutoff(new TCutoff);
            cutoff->SetE_value(v);
            
            x_SetParam(B4Param_EvalueThreshold, cutoff);
        }
        return;
        
    case eBlastOpt_PercentIdentity:
        x_SetParam(B4Param_PercentIdentity, v);
        return;

    case eBlastOpt_InclusionThreshold:
        x_SetParam(B4Param_InclusionThreshold, v);
        return;

    case eBlastOpt_GapXDropoff:
        x_SetParam(B4Param_GapXDropoff, v);
        return;

    case eBlastOpt_GapXDropoffFinal:
        x_SetParam(B4Param_GapXDropoffFinal, v);
        return;
        
    case eBlastOpt_XDropoff:
        //x_SetParam(B4Param_XDropoff, v);
        return;
        
    case eBlastOpt_SegFilteringLocut:
        x_SetParam(B4Param_SegFilteringLocut, v);
        return;

    case eBlastOpt_SegFilteringHicut:
        x_SetParam(B4Param_SegFilteringHicut, v);
        return;

    case eBlastOpt_GapTrigger:
        x_SetParam(B4Param_GapTrigger, v);
        return;

    case eBlastOpt_BestHitScoreEdge:
        x_SetParam(B4Param_BestHitScoreEdge, v);
        return;

    case eBlastOpt_BestHitOverhang:
        x_SetParam(B4Param_BestHitOverhang, v);
        return;

    default:
        break;
    }
    
    char errbuf[1024];
    
    sprintf(errbuf, "tried to set option (%d) and value (%f), line (%d).",
            int(opt), v, __LINE__);
    
    x_Throwx(string("err:") + errbuf);
}

void CBlastOptionsRemote::SetValue(EBlastOptIdx opt, const char * v)
{
    if (m_DefaultsMode) {
        return;
    }
    
    switch(opt) {
    case eBlastOpt_FilterString:
        x_SetParam(B4Param_FilterString, v);
        return;
        
    case eBlastOpt_RepeatFilteringDB:
        x_SetParam(B4Param_RepeatFilteringDB, v);
        return;
        
    case eBlastOpt_MatrixName:
        x_SetParam(B4Param_MatrixName, v);
        return;
        
    case eBlastOpt_WindowMaskerDatabase:
        x_SetParam(B4Param_WindowMaskerDatabase, v);
        return;
        
    case eBlastOpt_PHIPattern:
        x_SetParam(B4Param_PHIPattern, v);
        return;

    case eBlastOpt_MbIndexName:
        x_SetParam(B4Param_MbIndexName, v);
        return;
        
    default:
        break;
    }
    
    char errbuf[1024];
    
    sprintf(errbuf, "tried to set option (%d) and value (%.20s), line (%d).",
            int(opt), v, __LINE__);
    
    x_Throwx(string("err:") + errbuf);
}

void CBlastOptionsRemote::SetValue(EBlastOptIdx opt, const TSeqLocVector & v)
{
    if (m_DefaultsMode) {
        return;
    }
    
    char errbuf[1024];
    
    sprintf(errbuf, "tried to set option (%d) and TSeqLocVector (size %zd), line (%d).",
            int(opt), v.size(), __LINE__);
    
    x_Throwx(string("err:") + errbuf);
}

void CBlastOptionsRemote::SetValue(EBlastOptIdx opt, const ESeedContainerType & v)
{
    if (m_DefaultsMode) {
        return;
    }
    
    char errbuf[1024];
    
    sprintf(errbuf, "tried to set option (%d) and value (%d), line (%d).",
            int(opt), v, __LINE__);
    
    x_Throwx(string("err:") + errbuf);
}

void CBlastOptionsRemote::SetValue(EBlastOptIdx opt, const bool & v)
{
    if (m_DefaultsMode) {
        return;
    }
    
    switch(opt) {
    case eBlastOpt_GappedMode:
        {
            bool ungapped = ! v;
            x_SetParam(B4Param_UngappedMode, ungapped); // inverted
            return;
        }

    // Added for rmblastn and the new complexity adjusted scoring -RMH-
    case eBlastOpt_ComplexityAdjMode:
        x_SetParam(B4Param_ComplexityAdjustMode, v);
        return;
        
    case eBlastOpt_OutOfFrameMode:
        x_SetParam(B4Param_OutOfFrameMode, v);
        return;

    case eBlastOpt_SegFiltering:
        x_SetParam(B4Param_SegFiltering, v);
        return;

    case eBlastOpt_DustFiltering:
        x_SetParam(B4Param_DustFiltering, v);
        return;

    case eBlastOpt_RepeatFiltering:
        x_SetParam(B4Param_RepeatFiltering, v);
        return;

    case eBlastOpt_MaskAtHash:
        x_SetParam(B4Param_MaskAtHash, v);
        return;
        
    case eBlastOpt_SumStatisticsMode:
        x_SetParam(B4Param_SumStatistics, v);
        return;

    case eBlastOpt_SmithWatermanMode:
        x_SetParam(B4Param_SmithWatermanMode, v);
        return;

    case eBlastOpt_ForceMbIndex:
        x_SetParam(B4Param_ForceMbIndex, v);
        return;

    default:
        break;
    }
    
    char errbuf[1024];
    
    sprintf(errbuf, "tried to set option (%d) and value (%s), line (%d).",
            int(opt), (v ? "true" : "false"), __LINE__);
    
    x_Throwx(string("err:") + errbuf);
}

void CBlastOptionsRemote::SetValue(EBlastOptIdx opt, const Int8 & v)
{
    if (m_DefaultsMode) {
        return;
    }
    
    switch(opt) {
    case eBlastOpt_EffectiveSearchSpace:
        x_SetParam(B4Param_EffectiveSearchSpace, v);
        return;

    case eBlastOpt_DbLength:
        x_SetParam(B4Param_DbLength, v);
        return;
        
    default:
        break;
    }
    
    char errbuf[1024];
    
    sprintf(errbuf, "tried to set option (%d) and value (%f), line (%d).",
            int(opt), double(v), __LINE__);
    
    x_Throwx(string("err:") + errbuf);
}

const CBlastOptionsMemento*
CBlastOptions::CreateSnapshot() const
{
    if ( !m_Local ) {
        NCBI_THROW(CBlastException, eInvalidArgument,
                   "Cannot create CBlastOptionsMemento without a local "
                   "CBlastOptions object");
    }
    return new CBlastOptionsMemento(m_Local);
}

bool
CBlastOptions::operator==(const CBlastOptions& rhs) const
{
    if (m_Local && rhs.m_Local) {
        return (*m_Local == *rhs.m_Local);
    } else {
        NCBI_THROW(CBlastException, eNotSupported, 
                   "Equality operator unsupported for arguments");
    }
}

bool
CBlastOptions::operator!=(const CBlastOptions& rhs) const
{
    return !(*this == rhs);
}

bool
CBlastOptions::Validate() const
{
    bool local_okay  = m_Local  ? (m_Local ->Validate()) : true;
    
    return local_okay;
}

EProgram
CBlastOptions::GetProgram() const
{
    if (! m_Local) {
        x_Throwx("Error: GetProgram() not available.");
    }
    return m_Local->GetProgram();
}

EBlastProgramType 
CBlastOptions::GetProgramType() const
{
    if (! m_Local) {
        x_Throwx("Error: GetProgramType() not available.");
    }
    return m_Local->GetProgramType();
}

void 
CBlastOptions::SetProgram(EProgram p)
{
    if (m_Local) {
        m_Local->SetProgram(p);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_Program, p);
    }
}

/******************* Lookup table options ***********************/
double 
CBlastOptions::GetWordThreshold() const
{
    if (! m_Local) {
        x_Throwx("Error: GetWordThreshold() not available.");
    }
    return m_Local->GetWordThreshold();
}

void 
CBlastOptions::SetWordThreshold(double w)
{
    if (m_Local) {
        m_Local->SetWordThreshold(w);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_WordThreshold, static_cast<int>(w));
    }
}

ELookupTableType
CBlastOptions::GetLookupTableType() const
{
    if (! m_Local) {
        x_Throwx("Error: GetLookupTableType() not available.");
    }
    return m_Local->GetLookupTableType();
}
void 
CBlastOptions::SetLookupTableType(ELookupTableType type)
{
    if (m_Local) {
        m_Local->SetLookupTableType(type);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_LookupTableType, type);
    }
}

int 
CBlastOptions::GetWordSize() const
{
    if (! m_Local) {
        x_Throwx("Error: GetWordSize() not available.");
    }
    return m_Local->GetWordSize();
}
void 
CBlastOptions::SetWordSize(int ws)
{
    if (m_Local) {
        m_Local->SetWordSize(ws);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_WordSize, ws);
    }
}

/// Megablast only lookup table options
unsigned char 
CBlastOptions::GetMBTemplateLength() const
{
    if (! m_Local) {
        x_Throwx("Error: GetMBTemplateLength() not available.");
    }
    return m_Local->GetMBTemplateLength();
}
void 
CBlastOptions::SetMBTemplateLength(unsigned char len)
{
    if (m_Local) {
        m_Local->SetMBTemplateLength(len);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_MBTemplateLength, len);
    }
}

unsigned char 
CBlastOptions::GetMBTemplateType() const
{
    if (! m_Local) {
        x_Throwx("Error: GetMBTemplateType() not available.");
    }
    return m_Local->GetMBTemplateType();
}
void 
CBlastOptions::SetMBTemplateType(unsigned char type)
{
    if (m_Local) {
        m_Local->SetMBTemplateType(type);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_MBTemplateType, type);
    }
}

/******************* Query setup options ************************/

void
CBlastOptions::ClearFilterOptions()
{
    SetDustFiltering(false);
    SetSegFiltering(false);
    SetRepeatFiltering(false);
    SetMaskAtHash(false);
    SetWindowMaskerTaxId(0);
    SetWindowMaskerDatabase(NULL);
    return;
}

char* 
CBlastOptions::GetFilterString() const
{
    if (! m_Local) {
        x_Throwx("Error: GetFilterString() not available.");
    }
    return m_Local->GetFilterString();/* NCBI_FAKE_WARNING */
}
void 
CBlastOptions::SetFilterString(const char* f, bool clear)
{
    // Clear if clear is true or filtering set to FALSE.
    if (clear == true || NStr::CompareNocase("F", f) == 0) {
        ClearFilterOptions();
    }
    
    if (m_Local) {
        m_Local->SetFilterString(f);/* NCBI_FAKE_WARNING */
    }
    
    if (m_Remote) {
        // When maintaining this code, please insure the following:
        // 
        // 1. This list of items is parallel to the list found
        //    below, in the "set" block.
        // 
        // 2. Both lists should also correspond to the list of
        //    options in names.hpp and names.cpp that are related
        //    to filtering options.
        // 
        // 3. Blast4's code in CCollectFilterOptions should also
        //    handle the set of options handled here.
        // 
        // 4. CRemoteBlast and CRemoteBlastService's handling of
        //    filtering options (CBlastOptionsBuilder) should
        //    include all of these elements.
        // 
        // 5. Libnet2blast should deal with all of these filtering
        //    options when it builds CBlastOptionsHandle objects.
        //
        // 6. Probably at least one or two other places that I forgot.
        
        m_Remote->SetValue(eBlastOpt_MaskAtHash, m_Local->GetMaskAtHash());
        
        bool do_dust(false), do_seg(false), do_rep(false);
        
        if (Blast_QueryIsProtein(GetProgramType()) ||
            Blast_QueryIsTranslated(GetProgramType())) {
            do_seg = m_Local->GetSegFiltering();
            m_Remote->SetValue(eBlastOpt_SegFiltering, do_seg);
        } else {
            m_Remote->ResetValue(B4Param_SegFiltering);
            m_Remote->ResetValue(B4Param_SegFilteringWindow);
            m_Remote->ResetValue(B4Param_SegFilteringLocut);
            m_Remote->ResetValue(B4Param_SegFilteringHicut);
        }
        
        if (Blast_QueryIsNucleotide(GetProgramType()) &&
            !Blast_QueryIsTranslated(GetProgramType())) {
            do_dust = m_Local->GetDustFiltering();
            do_rep  = m_Local->GetRepeatFiltering();
            
            m_Remote->SetValue(eBlastOpt_DustFiltering, do_dust);
            m_Remote->SetValue(eBlastOpt_RepeatFiltering, do_rep);
        } else {
            m_Remote->ResetValue(B4Param_DustFiltering);
            m_Remote->ResetValue(B4Param_DustFilteringLevel);
            m_Remote->ResetValue(B4Param_DustFilteringWindow);
            m_Remote->ResetValue(B4Param_DustFilteringLinker);
            
            m_Remote->ResetValue(B4Param_RepeatFiltering);
            m_Remote->ResetValue(B4Param_RepeatFilteringDB);
        }
        
        if (do_dust) {
            m_Remote->SetValue(eBlastOpt_DustFilteringLevel,
                               m_Local->GetDustFilteringLevel());
            m_Remote->SetValue(eBlastOpt_DustFilteringWindow,
                               m_Local->GetDustFilteringWindow());
            m_Remote->SetValue(eBlastOpt_DustFilteringLinker,
                               m_Local->GetDustFilteringLinker());
        }
        
        if (do_rep) {
            m_Remote->SetValue(eBlastOpt_RepeatFilteringDB,
                               m_Local->GetRepeatFilteringDB());
        }
        
        if (do_seg) {
            m_Remote->SetValue(eBlastOpt_SegFilteringWindow,
                               m_Local->GetSegFilteringWindow());
            m_Remote->SetValue(eBlastOpt_SegFilteringLocut,
                               m_Local->GetSegFilteringLocut());
            m_Remote->SetValue(eBlastOpt_SegFilteringHicut,
                               m_Local->GetSegFilteringHicut());
        }
    }
}

bool 
CBlastOptions::GetMaskAtHash() const
{
    if (! m_Local) {
        x_Throwx("Error: GetMaskAtHash() not available.");
    }
    return m_Local->GetMaskAtHash();
}

void 
CBlastOptions::SetMaskAtHash(bool val)
{
    if (m_Local) {
        m_Local->SetMaskAtHash(val);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_MaskAtHash, val);
    }
}

bool 
CBlastOptions::GetDustFiltering() const
{
    if (! m_Local) {
        x_Throwx("Error: GetDustFiltering() not available.");
    }
    return m_Local->GetDustFiltering();
}
void 
CBlastOptions::SetDustFiltering(bool val)
{
    if (m_Local) {
        m_Local->SetDustFiltering(val);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_DustFiltering, val);
    }
}

int 
CBlastOptions::GetDustFilteringLevel() const
{
    if (! m_Local) {
        x_Throwx("Error: GetDustFilteringLevel() not available.");
    }
    return m_Local->GetDustFilteringLevel();
}
void 
CBlastOptions::SetDustFilteringLevel(int m)
{
    if (m_Local) {
        m_Local->SetDustFilteringLevel(m);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_DustFilteringLevel, m);
    }
}

int 
CBlastOptions::GetDustFilteringWindow() const
{
    if (! m_Local) {
        x_Throwx("Error: GetDustFilteringWindow() not available.");
    }
    return m_Local->GetDustFilteringWindow();
}
void 
CBlastOptions::SetDustFilteringWindow(int m)
{
    if (m_Local) {
        m_Local->SetDustFilteringWindow(m);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_DustFilteringWindow, m);
    }
}

int 
CBlastOptions::GetDustFilteringLinker() const
{
    if (! m_Local) {
        x_Throwx("Error: GetDustFilteringLinker() not available.");
    }
    return m_Local->GetDustFilteringLinker();
}
void 
CBlastOptions::SetDustFilteringLinker(int m)
{
    if (m_Local) {
        m_Local->SetDustFilteringLinker(m);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_DustFilteringLinker, m);
    }
}

bool 
CBlastOptions::GetSegFiltering() const
{
    if (! m_Local) {
        x_Throwx("Error: GetSegFiltering() not available.");
    }
    return m_Local->GetSegFiltering();
}
void 
CBlastOptions::SetSegFiltering(bool val)
{
    if (m_Local) {
        m_Local->SetSegFiltering(val);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_SegFiltering, val);
    }
}

int 
CBlastOptions::GetSegFilteringWindow() const
{
    if (! m_Local) {
        x_Throwx("Error: GetSegFilteringWindow() not available.");
    }
    return m_Local->GetSegFilteringWindow();
}
void 
CBlastOptions::SetSegFilteringWindow(int m)
{
    if (m_Local) {
        m_Local->SetSegFilteringWindow(m);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_SegFilteringWindow, m);
    }
}

double 
CBlastOptions::GetSegFilteringLocut() const
{
    if (! m_Local) {
        x_Throwx("Error: GetSegFilteringLocut() not available.");
    }
    return m_Local->GetSegFilteringLocut();
}
void 
CBlastOptions::SetSegFilteringLocut(double m)
{
    if (m_Local) {
        m_Local->SetSegFilteringLocut(m);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_SegFilteringLocut, m);
    }
}

double 
CBlastOptions::GetSegFilteringHicut() const
{
    if (! m_Local) {
        x_Throwx("Error: GetSegFilteringHicut() not available.");
    }
    return m_Local->GetSegFilteringHicut();
}
void 
CBlastOptions::SetSegFilteringHicut(double m)
{
    if (m_Local) {
        m_Local->SetSegFilteringHicut(m);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_SegFilteringHicut, m);
    }
}

bool 
CBlastOptions::GetRepeatFiltering() const
{
    if (! m_Local) {
        x_Throwx("Error: GetRepeatFiltering() not available.");
    }
    return m_Local->GetRepeatFiltering();
}
void 
CBlastOptions::SetRepeatFiltering(bool val)
{
    if (m_Local) {
        m_Local->SetRepeatFiltering(val);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_RepeatFiltering, val);
    }
}

const char* 
CBlastOptions::GetRepeatFilteringDB() const
{
    if (! m_Local) {
        x_Throwx("Error: GetRepeatFilteringDB() not available.");
    }
    return m_Local->GetRepeatFilteringDB();
}
void 
CBlastOptions::SetRepeatFilteringDB(const char* db)
{
    if (m_Local) {
        m_Local->SetRepeatFilteringDB(db);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_RepeatFilteringDB, db);
    }
}

int
CBlastOptions::GetWindowMaskerTaxId() const
{
    if (! m_Local) {
        x_Throwx("Error: GetWindowMaskerTaxId() not available.");
    }
    return m_Local->GetWindowMaskerTaxId();
}

void
CBlastOptions::SetWindowMaskerTaxId(int value)
{
    if (m_Local) {
        m_Local->SetWindowMaskerTaxId(value);
    }
    if (m_Remote) {
        if (value) {
            m_Remote->SetValue(eBlastOpt_WindowMaskerTaxId, value);
        } else {
            m_Remote->ResetValue(B4Param_WindowMaskerTaxId);
        }
    }
}

const char *
CBlastOptions::GetWindowMaskerDatabase() const
{
    if (! m_Local) {
        x_Throwx("Error: GetWindowMaskerDatabase() not available.");
    }
    return m_Local->GetWindowMaskerDatabase();
}

void
CBlastOptions::SetWindowMaskerDatabase(const char * value)
{
    if (m_Local) {
        m_Local->SetWindowMaskerDatabase(value);
    }
    if (m_Remote) {
        if (value) {
            m_Remote->SetValue(eBlastOpt_WindowMaskerDatabase, value); 
        } else {
            m_Remote->ResetValue(B4Param_WindowMaskerDatabase); 
        }
    }
}

objects::ENa_strand 
CBlastOptions::GetStrandOption() const
{
    if (! m_Local) {
        x_Throwx("Error: GetStrandOption() not available.");
    }
    return m_Local->GetStrandOption();
}
void 
CBlastOptions::SetStrandOption(objects::ENa_strand s)
{
    if (m_Local) {
        m_Local->SetStrandOption(s);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_StrandOption, s);
    }
}

int 
CBlastOptions::GetQueryGeneticCode() const
{
    if (! m_Local) {
        x_Throwx("Error: GetQueryGeneticCode() not available.");
    }
    return m_Local->GetQueryGeneticCode();
}
void 
CBlastOptions::SetQueryGeneticCode(int gc)
{
    if (m_Local) {
        m_Local->SetQueryGeneticCode(gc);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_QueryGeneticCode, gc);
    }
}

/******************* Initial word options ***********************/
int 
CBlastOptions::GetWindowSize() const
{
    if (! m_Local) {
        x_Throwx("Error: GetWindowSize() not available.");
    }
    return m_Local->GetWindowSize();
}
void 
CBlastOptions::SetWindowSize(int w)
{
    if (m_Local) {
        m_Local->SetWindowSize(w);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_WindowSize, w);
    }
}

int 
CBlastOptions::GetOffDiagonalRange() const
{
    if (! m_Local) {
        x_Throwx("Error: GetOffDiagonalRange() not available.");
    }
    return m_Local->GetOffDiagonalRange();
}
void 
CBlastOptions::SetOffDiagonalRange(int w)
{
    if (m_Local) {
        m_Local->SetOffDiagonalRange(w);
    }
    // N/A for the time being
    //if (m_Remote) {
    //    m_Remote->SetValue(eBlastOpt_OffDiagonalRange, w);
    //}
}
double 
CBlastOptions::GetXDropoff() const
{
    if (! m_Local) {
        x_Throwx("Error: GetXDropoff() not available.");
    }
    return m_Local->GetXDropoff();
}
void 
CBlastOptions::SetXDropoff(double x)
{
    if (m_Local) {
        m_Local->SetXDropoff(x);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_XDropoff, x);
    }
}

/******************* Gapped extension options *******************/
double 
CBlastOptions::GetGapXDropoff() const
{
    if (! m_Local) {
        x_Throwx("Error: GetGapXDropoff() not available.");
    }
    return m_Local->GetGapXDropoff();
}
void 
CBlastOptions::SetGapXDropoff(double x)
{
    if (m_Local) {
        m_Local->SetGapXDropoff(x);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_GapXDropoff, x);
    }
}

double 
CBlastOptions::GetGapXDropoffFinal() const
{
    if (! m_Local) {
        x_Throwx("Error: GetGapXDropoffFinal() not available.");
    }
    return m_Local->GetGapXDropoffFinal();
}
void 
CBlastOptions::SetGapXDropoffFinal(double x)
{
    if (m_Local) {
        m_Local->SetGapXDropoffFinal(x);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_GapXDropoffFinal, x);
    }
}

double 
CBlastOptions::GetGapTrigger() const
{
    if (! m_Local) {
        x_Throwx("Error: GetGapTrigger() not available.");
    }
    return m_Local->GetGapTrigger();
}
void 
CBlastOptions::SetGapTrigger(double g)
{
    if (m_Local) {
        m_Local->SetGapTrigger(g);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_GapTrigger, g);
    }
}

EBlastPrelimGapExt 
CBlastOptions::GetGapExtnAlgorithm() const
{
    if (! m_Local) {
        x_Throwx("Error: GetGapExtnAlgorithm() not available.");
    }
    return m_Local->GetGapExtnAlgorithm();
}
void 
CBlastOptions::SetGapExtnAlgorithm(EBlastPrelimGapExt a)
{
    if (m_Local) {
        m_Local->SetGapExtnAlgorithm(a);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_GapExtnAlgorithm, a);
    }
}

EBlastTbackExt 
CBlastOptions::GetGapTracebackAlgorithm() const
{
    if (! m_Local) {
        x_Throwx("Error: GetGapTracebackAlgorithm() not available.");
    }
    return m_Local->GetGapTracebackAlgorithm();
}

void 
CBlastOptions::SetGapTracebackAlgorithm(EBlastTbackExt a)
{
    if (m_Local) {
        m_Local->SetGapTracebackAlgorithm(a);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_GapTracebackAlgorithm, a);
    }
}

ECompoAdjustModes 
CBlastOptions::GetCompositionBasedStats() const
{
    if (! m_Local) {
        x_Throwx("Error: GetCompositionBasedStats() not available.");
    }
    return m_Local->GetCompositionBasedStats();
}

void 
CBlastOptions::SetCompositionBasedStats(ECompoAdjustModes mode)
{
    if (m_Local) {
        m_Local->SetCompositionBasedStats(mode);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_CompositionBasedStats, mode);
    }
}

bool 
CBlastOptions::GetSmithWatermanMode() const
{
    if (! m_Local) {
        x_Throwx("Error: GetSmithWatermanMode() not available.");
    }
    return m_Local->GetSmithWatermanMode();
}

void 
CBlastOptions::SetSmithWatermanMode(bool m)
{
    if (m_Local) {
        m_Local->SetSmithWatermanMode(m);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_SmithWatermanMode, m);
    }
}

int 
CBlastOptions::GetUnifiedP() const
{
    if (! m_Local) {
        x_Throwx("Error: GetUnifiedP() not available.");
    }

    return m_Local->GetUnifiedP();
}

void 
CBlastOptions::SetUnifiedP(int u)
{
   if (m_Local) {
      m_Local->SetUnifiedP(u);
   }
   if (m_Remote) {
      m_Remote->SetValue(eBlastOpt_UnifiedP, u);
   }
}
 

/******************* Hit saving options *************************/
int 
CBlastOptions::GetHitlistSize() const
{
    if (! m_Local) {
        x_Throwx("Error: GetHitlistSize() not available.");
    }
    return m_Local->GetHitlistSize();
}
void 
CBlastOptions::SetHitlistSize(int s)
{
    if (m_Local) {
        m_Local->SetHitlistSize(s);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_HitlistSize, s);
    }
}

int 
CBlastOptions::GetMaxNumHspPerSequence() const
{
    if (! m_Local) {
        x_Throwx("Error: GetMaxNumHspPerSequence() not available.");
    }
    return m_Local->GetMaxNumHspPerSequence();
}
void 
CBlastOptions::SetMaxNumHspPerSequence(int m)
{
    if (m_Local) {
        m_Local->SetMaxNumHspPerSequence(m);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_MaxNumHspPerSequence, m);
    }
}

int 
CBlastOptions::GetCullingLimit() const
{
    if (! m_Local) {
        x_Throwx("Error: GetCullingMode() not available.");
    }
    return m_Local->GetCullingLimit();
}
void 
CBlastOptions::SetCullingLimit(int s)
{
    if (m_Local) {
        m_Local->SetCullingLimit(s);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_CullingLimit, s);
    }
}

double 
CBlastOptions::GetBestHitOverhang() const
{
    if (! m_Local) {
        x_Throwx("Error: GetBestHitOverhangMode() not available.");
    }
    return m_Local->GetBestHitOverhang();
}
void 
CBlastOptions::SetBestHitOverhang(double overhang)
{
    if (m_Local) {
        m_Local->SetBestHitOverhang(overhang);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_BestHitOverhang, overhang);
    }
}

double 
CBlastOptions::GetBestHitScoreEdge() const
{
    if (! m_Local) {
        x_Throwx("Error: GetBestHitScoreEdgeMode() not available.");
    }
    return m_Local->GetBestHitScoreEdge();
}
void 
CBlastOptions::SetBestHitScoreEdge(double score_edge)
{
    if (m_Local) {
        m_Local->SetBestHitScoreEdge(score_edge);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_BestHitScoreEdge, score_edge);
    }
}

double 
CBlastOptions::GetEvalueThreshold() const
{
    if (! m_Local) {
        x_Throwx("Error: GetEvalueThreshold() not available.");
    }
    return m_Local->GetEvalueThreshold();
}
void 
CBlastOptions::SetEvalueThreshold(double eval)
{
    if (m_Local) {
        m_Local->SetEvalueThreshold(eval);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_EvalueThreshold, eval);
    }
}

int 
CBlastOptions::GetCutoffScore() const
{
    if (! m_Local) {
        x_Throwx("Error: GetCutoffScore() not available.");
    }
    return m_Local->GetCutoffScore();
}
void 
CBlastOptions::SetCutoffScore(int s)
{
    if (m_Local) {
        m_Local->SetCutoffScore(s);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_CutoffScore, s);
    }
}

double 
CBlastOptions::GetPercentIdentity() const
{
    if (! m_Local) {
        x_Throwx("Error: GetPercentIdentity() not available.");
    }
    return m_Local->GetPercentIdentity();
}
void 
CBlastOptions::SetPercentIdentity(double p)
{
    if (m_Local) {
        m_Local->SetPercentIdentity(p);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_PercentIdentity, p);
    }
}

int 
CBlastOptions::GetMinDiagSeparation() const
{
    if (! m_Local) {
        x_Throwx("Error: GetMinDiagSeparation() not available.");
    }
    return m_Local->GetMinDiagSeparation();
}
void 
CBlastOptions::SetMinDiagSeparation(int d)
{
    if (! m_Local) {
        x_Throwx("Error: SetMinDiagSeparation() not available.");
    }
    m_Local->SetMinDiagSeparation(d);
}

bool 
CBlastOptions::GetSumStatisticsMode() const
{
    if (! m_Local) {
        x_Throwx("Error: GetSumStatisticsMode() not available.");
    }
    return m_Local->GetSumStatisticsMode();
}
void 
CBlastOptions::SetSumStatisticsMode(bool m)
{
    if (m_Local) {
        m_Local->SetSumStatisticsMode(m);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_SumStatisticsMode, m);
    }
}

int 
CBlastOptions::GetLongestIntronLength() const
{
    if (! m_Local) {
        x_Throwx("Error: GetLongestIntronLength() not available.");
    }
    return m_Local->GetLongestIntronLength();
}
void 
CBlastOptions::SetLongestIntronLength(int l)
{
    if (m_Local) {
        m_Local->SetLongestIntronLength(l);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_LongestIntronLength, l);
    }
}


bool 
CBlastOptions::GetGappedMode() const
{
    if (! m_Local) {
        x_Throwx("Error: GetGappedMode() not available.");
    }
    return m_Local->GetGappedMode();
}
void 
CBlastOptions::SetGappedMode(bool m)
{
    if (m_Local) {
        m_Local->SetGappedMode(m);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_GappedMode, m);
    }
}

// -RMH-
int
CBlastOptions::GetMaskLevel() const
{
    if (! m_Local) {
        x_Throwx("Error: GetMaskLevel() not available.");
    }
    return m_Local->GetMaskLevel();
}

// -RMH-
void
CBlastOptions::SetMaskLevel(int s)
{
    if (m_Local) {
        m_Local->SetMaskLevel(s);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_MaskLevel, s);
    }
}

// -RMH-
bool
CBlastOptions::GetComplexityAdjMode() const
{
    if (! m_Local) {
        x_Throwx("Error: GetComplexityAdjMode() not available.");
    }
    return m_Local->GetComplexityAdjMode();
}

// -RMH-
void
CBlastOptions::SetComplexityAdjMode(bool m)
{
    if (m_Local) {
        m_Local->SetComplexityAdjMode(m);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_ComplexityAdjMode, m);
    }
}

/************************ Scoring options ************************/
const char* 
CBlastOptions::GetMatrixName() const
{
    if (! m_Local) {
        x_Throwx("Error: GetMatrixName() not available.");
    }
    return m_Local->GetMatrixName();
}
void 
CBlastOptions::SetMatrixName(const char* matrix)
{
    if (m_Local) {
        m_Local->SetMatrixName(matrix);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_MatrixName, matrix);
    }
}

int 
CBlastOptions::GetMatchReward() const
{
    if (! m_Local) {
        x_Throwx("Error: GetMatchReward() not available.");
    }
    return m_Local->GetMatchReward();
}
void 
CBlastOptions::SetMatchReward(int r)
{
    if (m_Local) {
        m_Local->SetMatchReward(r);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_MatchReward, r);
    }
}

int 
CBlastOptions::GetMismatchPenalty() const
{
    if (! m_Local) {
        x_Throwx("Error: GetMismatchPenalty() not available.");
    }
    return m_Local->GetMismatchPenalty();
}
void 
CBlastOptions::SetMismatchPenalty(int p)
{
    if (m_Local) {
        m_Local->SetMismatchPenalty(p);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_MismatchPenalty, p);
    }
}

int 
CBlastOptions::GetGapOpeningCost() const
{
    if (! m_Local) {
        x_Throwx("Error: GetGapOpeningCost() not available.");
    }
    return m_Local->GetGapOpeningCost();
}
void 
CBlastOptions::SetGapOpeningCost(int g)
{
    if (m_Local) {
        m_Local->SetGapOpeningCost(g);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_GapOpeningCost, g);
    }
}

int 
CBlastOptions::GetGapExtensionCost() const
{
    if (! m_Local) {
        x_Throwx("Error: GetGapExtensionCost() not available.");
    }
    return m_Local->GetGapExtensionCost();
}
void 
CBlastOptions::SetGapExtensionCost(int e)
{
    if (m_Local) {
        m_Local->SetGapExtensionCost(e);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_GapExtensionCost, e);
    }
}

int 
CBlastOptions::GetFrameShiftPenalty() const
{
    if (! m_Local) {
        x_Throwx("Error: GetFrameShiftPenalty() not available.");
    }
    return m_Local->GetFrameShiftPenalty();
}
void 
CBlastOptions::SetFrameShiftPenalty(int p)
{
    if (m_Local) {
        m_Local->SetFrameShiftPenalty(p);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_FrameShiftPenalty, p);
    }
}

bool 
CBlastOptions::GetOutOfFrameMode() const
{
    if (! m_Local) {
        x_Throwx("Error: GetOutOfFrameMode() not available.");
    }
    return m_Local->GetOutOfFrameMode();
}
void 
CBlastOptions::SetOutOfFrameMode(bool m)
{
    if (m_Local) {
        m_Local->SetOutOfFrameMode(m);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_OutOfFrameMode, m);
    }
}

/******************** Effective Length options *******************/
Int8 
CBlastOptions::GetDbLength() const
{
    if (! m_Local) {
        x_Throwx("Error: GetDbLength() not available.");
    }
    return m_Local->GetDbLength();
}
void 
CBlastOptions::SetDbLength(Int8 l)
{
    if (m_Local) {
        m_Local->SetDbLength(l);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_DbLength, l);
    }
}

unsigned int 
CBlastOptions::GetDbSeqNum() const
{
    if (! m_Local) {
        x_Throwx("Error: GetDbSeqNum() not available.");
    }
    return m_Local->GetDbSeqNum();
}
void 
CBlastOptions::SetDbSeqNum(unsigned int n)
{
    if (m_Local) {
        m_Local->SetDbSeqNum(n);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_DbSeqNum, n);
    }
}

Int8 
CBlastOptions::GetEffectiveSearchSpace() const
{
    if (! m_Local) {
        x_Throwx("Error: GetEffectiveSearchSpace() not available.");
    }
    return m_Local->GetEffectiveSearchSpace();
}
void 
CBlastOptions::SetEffectiveSearchSpace(Int8 eff)
{
    if (m_Local) {
        m_Local->SetEffectiveSearchSpace(eff);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_EffectiveSearchSpace, eff);
    }
}
void 
CBlastOptions::SetEffectiveSearchSpace(const vector<Int8>& eff)
{
    if (m_Local) {
        m_Local->SetEffectiveSearchSpace(eff);
    }
    if (m_Remote) {
        _ASSERT( !eff.empty() );
        // This is the best we can do because remote BLAST only accepts one
        // value for the effective search space
        m_Remote->SetValue(eBlastOpt_EffectiveSearchSpace, eff.front());
    }
}

int 
CBlastOptions::GetDbGeneticCode() const
{
    if (! m_Local) {
        x_Throwx("Error: GetDbGeneticCode() not available.");
    }
    return m_Local->GetDbGeneticCode();
}

void 
CBlastOptions::SetDbGeneticCode(int gc)
{
    if (m_Local) {
        m_Local->SetDbGeneticCode(gc);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_DbGeneticCode, gc);
    }
}

const char* 
CBlastOptions::GetPHIPattern() const
{
    if (! m_Local) {
        x_Throwx("Error: GetPHIPattern() not available.");
    }
    return m_Local->GetPHIPattern();
}
void 
CBlastOptions::SetPHIPattern(const char* pattern, bool is_dna)
{
    if (m_Local) {
        m_Local->SetPHIPattern(pattern, is_dna);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_PHIPattern, pattern);
        
// For now I will assume this is handled when the data is passed to the
// code in blast4_options - i.e. that code will discriminate on the basis
// of the type of *OptionHandle that is passed in.
//
//             if (is_dna) {
//                 m_Remote->SetProgram("blastn");
//             } else {
//                 m_Remote->SetProgram("blastp");
//             }
//             
//             m_Remote->SetService("phi");
    }
}

/******************** PSIBlast options *******************/
double 
CBlastOptions::GetInclusionThreshold() const
{
    if (! m_Local) {
        x_Throwx("Error: GetInclusionThreshold() not available.");
    }
    return m_Local->GetInclusionThreshold();
}
void 
CBlastOptions::SetInclusionThreshold(double u)
{
    if (m_Local) {
        m_Local->SetInclusionThreshold(u);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_InclusionThreshold, u);
    }
}

int 
CBlastOptions::GetPseudoCount() const
{
    if (! m_Local) {
        x_Throwx("Error: GetPseudoCount() not available.");
    }
    return m_Local->GetPseudoCount();
}
void 
CBlastOptions::SetPseudoCount(int u)
{
    if (m_Local) {
        m_Local->SetPseudoCount(u);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_PseudoCount, u);
    }
}


/// Allows to dump a snapshot of the object
void 
CBlastOptions::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
    if (m_Local) {
        m_Local->DebugDump(ddc, depth);
    }
}

void 
CBlastOptions::DoneDefaults() const
{
    if (m_Remote) {
        m_Remote->SetDefaultsMode(false);
    }
}

//     typedef ncbi::objects::CBlast4_queue_search_request TBlast4Req;
//     CRef<TBlast4Req> GetBlast4Request() const
//     {
//         CRef<TBlast4Req> result;
    
//         if (m_Remote) {
//             result = m_Remote->GetBlast4Request();
//         }
    
//         return result;
//     }

// the "new paradigm"
CBlastOptions::TBlast4Opts * 
CBlastOptions::GetBlast4AlgoOpts()
{
    TBlast4Opts * result = 0;
    
    if (m_Remote) {
        result = m_Remote->GetBlast4AlgoOpts();
    }
    
    return result;
}

bool CBlastOptions::GetUseIndex() const 
{
    if (! m_Local) {
        x_Throwx("Error: GetUseIndex() not available.");
    }

    return m_Local->GetUseIndex();
}

bool CBlastOptions::GetForceIndex() const 
{
    if (! m_Local) {
        x_Throwx("Error: GetForceIndex() not available.");
    }

    return m_Local->GetForceIndex();
}

bool CBlastOptions::GetMBIndexLoaded() const
{
    if (! m_Local) {
        x_Throwx("Error: GetMBIndexLoaded() not available.");
    }

    return m_Local->GetMBIndexLoaded();
}

const string CBlastOptions::GetIndexName() const
{
    if (! m_Local) {
        x_Throwx("Error: GetIndexName() not available.");
    }

    return m_Local->GetIndexName();
}

void CBlastOptions::SetUseIndex( bool use_index, const string & index_name, bool force_index )
{
    if (m_Local) {
        m_Local->SetUseIndex( use_index, index_name, force_index );
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_ForceMbIndex, force_index);
        if ( !index_name.empty() ) {
            m_Remote->SetValue(eBlastOpt_MbIndexName, index_name.c_str());
        }
    }

}

void CBlastOptions::SetMBIndexLoaded( bool index_loaded )
{
    if (! m_Local) {
        x_Throwx("Error: SetMBIndexLoaded() not available.");
    }

    m_Local->SetMBIndexLoaded( index_loaded );
}

QuerySetUpOptions * 
CBlastOptions::GetQueryOpts() const
{
    return m_Local ? m_Local->GetQueryOpts() : 0;
}

LookupTableOptions * 
CBlastOptions::GetLutOpts() const
{
    return m_Local ? m_Local->GetLutOpts() : 0;
}

BlastInitialWordOptions * 
CBlastOptions::GetInitWordOpts() const
{
    return m_Local ? m_Local->GetInitWordOpts() : 0;
}

BlastExtensionOptions * 
CBlastOptions::GetExtnOpts() const
{
    return m_Local ? m_Local->GetExtnOpts() : 0;
}

BlastHitSavingOptions * 
CBlastOptions::GetHitSaveOpts() const
{
    return m_Local ? m_Local->GetHitSaveOpts() : 0;
}

PSIBlastOptions * 
CBlastOptions::GetPSIBlastOpts() const
{
    return m_Local ? m_Local->GetPSIBlastOpts() : 0;
}

BlastDatabaseOptions * 
CBlastOptions::GetDbOpts() const
{
    return m_Local ? m_Local->GetDbOpts() : 0;
}

BlastScoringOptions * 
CBlastOptions::GetScoringOpts() const
{
    return m_Local ? m_Local->GetScoringOpts() : 0;
}

BlastEffectiveLengthsOptions * 
CBlastOptions::GetEffLenOpts() const
{
    return m_Local ? m_Local->GetEffLenOpts() : 0;
}

void
CBlastOptions::x_Throwx(const string& msg) const
{
    NCBI_THROW(CBlastException, eInvalidOptions, msg);
}

void CBlastOptions::SetDefaultsMode(bool dmode)
{
    if (m_Remote) {
        m_Remote->SetDefaultsMode(dmode);
    }
}

bool CBlastOptions::GetDefaultsMode() const
{
    if (m_Remote) {
        return m_Remote->GetDefaultsMode();
    }
    else
        return false;
}

void CBlastOptions::x_DoDeepCopy(const CBlastOptions& opts)
{
    if (&opts != this)
    {
        // Clean up the old object
        if (m_Local)
        {
            delete m_Local;
            m_Local = 0;
        }
        if (m_Remote)
        {
            delete m_Remote;
            m_Remote = 0;
        }

        // Copy the contents of the new object
        if (opts.m_Remote)
        {
            m_Remote = new CBlastOptionsRemote(*opts.m_Remote);
        }
        if (opts.m_Local)
        {
            m_Local = new CBlastOptionsLocal(*opts.m_Local);
        }
        m_ProgramName = opts.m_ProgramName;
        m_ServiceName = opts.m_ServiceName;
        m_DefaultsMode = opts.m_DefaultsMode;
    }
}

#endif /* SKIP_DOXYGEN_PROCESSING */

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
