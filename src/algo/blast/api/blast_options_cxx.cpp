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
* File Description:
*   Class to encapsulate all NewBlast options
*
* ===========================================================================
*/

/// @file blast_options_cxx.cpp
/// Implements the CBlastOptions class, which encapsulates options structures
/// from algo/blast/core

#include <ncbi_pch.hpp>
#include <algo/blast/api/blast_options.hpp>
#include "blast_setup.hpp"

#include <algo/blast/core/blast_extend.h>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/blast/Blast4_cutoff.hpp>

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
    eBlastOpt_MBMaxPositions,
    eBlastOpt_UsePssm,
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
    eBlastOpt_VariableWordSize,
    eBlastOpt_FullByteScan,
    eBlastOpt_UngappedExtension,
    eBlastOpt_XDropoff,
    eBlastOpt_GapXDropoff,
    eBlastOpt_GapXDropoffFinal,
    eBlastOpt_GapTrigger,
    eBlastOpt_GapExtnAlgorithm,
    eBlastOpt_HitlistSize,
    eBlastOpt_MaxNumHspPerSequence,
    eBlastOpt_CullingLimit,
    eBlastOpt_RequiredStart,
    eBlastOpt_RequiredEnd,
    eBlastOpt_EvalueThreshold,
    eBlastOpt_CutoffScore,
    eBlastOpt_PercentIdentity,
    eBlastOpt_SumStatisticsMode,
    eBlastOpt_LongestIntronLength,
    eBlastOpt_GappedMode,
    eBlastOpt_MatrixName,
    eBlastOpt_MatrixPath,
    eBlastOpt_MatchReward,
    eBlastOpt_MismatchPenalty,
    eBlastOpt_GapOpeningCost,
    eBlastOpt_GapExtensionCost,
    eBlastOpt_FrameShiftPenalty,
    eBlastOpt_Decline2AlignPenalty,
    eBlastOpt_OutOfFrameMode,
    eBlastOpt_DbLength,
    eBlastOpt_DbSeqNum,
    eBlastOpt_EffectiveSearchSpace,
    eBlastOpt_DbGeneticCode,
    eBlastOpt_PHIPattern,
    eBlastOpt_InclusionThreshold,
    eBlastOpt_PseudoCount,
    eBlastOpt_GapTracebackAlgorithm,
    eBlastOpt_CompositionBasedStatsMode,
    eBlastOpt_SmithWatermanMode
};


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

    bool GetVariableWordSize() const;
    void SetVariableWordSize(bool val = true);

    bool GetFullByteScan() const;
    void SetFullByteScan(bool val = true);

    bool GetUsePssm() const;
    void SetUsePssm(bool m = true);

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


/// Encapsulates all blast input parameters
class NCBI_XBLAST_EXPORT CBlastOptionsRemote : public CObject
{
public:
    CBlastOptionsRemote(void)
        : m_DoneDefaults(false)
    {
        //m_Req.Reset(new objects::CBlast4_queue_search_request);
        m_ReqOpts.Reset(new objects::CBlast4_parameters);
    }
    
    ~CBlastOptionsRemote()
    {
    }
    
//     typedef ncbi::objects::CBlast4_queue_search_request TBlast4Req;
//     CRef<TBlast4Req> GetBlast4Request()
//     {
//         return m_Req;
//     }
    
    // the "new paradigm"
    typedef ncbi::objects::CBlast4_parameters TBlast4Opts;
    TBlast4Opts * GetBlast4AlgoOpts()
    {
        return m_ReqOpts;
    }
    
    typedef vector< CConstRef<objects::CSeq_loc> > TSeqLocVector;
    
//     // Basic mandatory functions
//     void SetProgram(const char * v)
//     {
//         m_Req->SetProgram(v);
//     }
    
//     void SetService(const char * v)
//     {
//         m_Req->SetService(v);
//     }
    
    // SetValue(x,y) with different types:
    void SetValue(EBlastOptIdx opt, const EProgram            & x);
    void SetValue(EBlastOptIdx opt, const int                 & x);
    void SetValue(EBlastOptIdx opt, const double              & x);
    void SetValue(EBlastOptIdx opt, const char                * x);
    void SetValue(EBlastOptIdx opt, const TSeqLocVector       & x);
    void SetValue(EBlastOptIdx opt, const ESeedContainerType   & x);
    void SetValue(EBlastOptIdx opt, const ESeedExtensionMethod & x);
    void SetValue(EBlastOptIdx opt, const bool                & x);
    void SetValue(EBlastOptIdx opt, const Int8                & x);
    
    // Pseudo-types:
    void SetValue(EBlastOptIdx opt, const short & x)
    { int x2 = x; SetValue(opt, x2); }
    
    void SetValue(EBlastOptIdx opt, const unsigned int & x)
    { int x2 = x; SetValue(opt, x2); }
    
    void SetValue(EBlastOptIdx opt, const unsigned char & x)
    { int x2 = x; SetValue(opt, x2); }
    
    void SetValue(EBlastOptIdx opt, const objects::ENa_strand & x)
    { int x2 = x; SetValue(opt, x2); }
    
    void DoneDefaults()
    {
        m_DoneDefaults = true;
    }
    
private:
    //CRef<objects::CBlast4_queue_search_request> m_Req;
    CRef<objects::CBlast4_parameters> m_ReqOpts;
    
    bool m_DoneDefaults;
    
//     void x_SetProgram(const char * program)
//     {
//         m_Req->SetProgram(program);
//     }
    
//     void x_SetService(const char * service)
//     {
//         m_Req->SetService(service);
//     }
    
    template<class T>
    void x_SetParam(const char * name, T & value)
    {
        x_SetOneParam(name, & value);
    }
    
    void x_SetOneParam(const char * name, const int * x)
    {
        CRef<objects::CBlast4_value> v(new objects::CBlast4_value);
        v->SetInteger(*x);
        
        CRef<objects::CBlast4_parameter> p(new objects::CBlast4_parameter);
        p->SetName(name);
        p->SetValue(*v);
        
        m_ReqOpts->Set().push_back(p);
    }
    
    void x_SetOneParam(const char * name, const char ** x)
    {
        CRef<objects::CBlast4_value> v(new objects::CBlast4_value);
        v->SetString().assign((x && (*x)) ? (*x) : "");
        
        CRef<objects::CBlast4_parameter> p(new objects::CBlast4_parameter);
        p->SetName(name);
        p->SetValue(*v);
        
        m_ReqOpts->Set().push_back(p);
    }
    
    void x_SetOneParam(const char * name, const bool * x)
    {
        CRef<objects::CBlast4_value> v(new objects::CBlast4_value);
        v->SetBoolean(*x);
        
        CRef<objects::CBlast4_parameter> p(new objects::CBlast4_parameter);
        p->SetName(name);
        p->SetValue(*v);
        
        m_ReqOpts->Set().push_back(p);
    }
    
    void x_SetOneParam(const char * name, CRef<objects::CBlast4_cutoff> * x)
    {
        CRef<objects::CBlast4_value> v(new objects::CBlast4_value);
        v->SetCutoff(**x);
        
        CRef<objects::CBlast4_parameter> p(new objects::CBlast4_parameter);
        p->SetName(name);
        p->SetValue(*v);
        
        m_ReqOpts->Set().push_back(p);
    }
    
    void x_SetOneParam(const char * name, const double * x)
    {
        CRef<objects::CBlast4_value> v(new objects::CBlast4_value);
        v->SetReal(*x);
        
        CRef<objects::CBlast4_parameter> p(new objects::CBlast4_parameter);
        p->SetName(name);
        p->SetValue(*v);
        
        m_ReqOpts->Set().push_back(p);
    }
    
    void x_SetOneParam(const char * name, const Int8 * x)
    {
        CRef<objects::CBlast4_value> v(new objects::CBlast4_value);
        v->SetBig_integer(*x);
        
        CRef<objects::CBlast4_parameter> p(new objects::CBlast4_parameter);
        p->SetName(name);
        p->SetValue(*v);
        
        m_ReqOpts->Set().push_back(p);
    }
    
    void x_SetOneParam(const char * name, objects::EBlast4_strand_type * x)
    {
        CRef<objects::CBlast4_value> v(new objects::CBlast4_value);
        v->SetStrand_type(*x);
        
        CRef<objects::CBlast4_parameter> p(new objects::CBlast4_parameter);
        p->SetName(name);
        p->SetValue(*v);
        
        m_ReqOpts->Set().push_back(p);
    }
    
    void x_Throwx(const string& msg) const
    {
        NCBI_THROW(CBlastException, eInternal, msg);
    }
};


CBlastOptions::CBlastOptions(EAPILocality locality)
    : m_Local (0),
      m_Remote(0)
{
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
    switch(opt) {
    case eBlastOpt_WordSize:
        x_SetParam("WordSize", v);
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
                x_SetParam("StrandOption", strand);
                return;
            }
        }
        
    case eBlastOpt_WindowSize:
        x_SetParam("WindowSize", v);
        return;
        
    case eBlastOpt_GapOpeningCost:
        x_SetParam("GapOpeningCost", v);
        return;
        
    case eBlastOpt_GapExtensionCost:
        x_SetParam("GapExtensionCost", v);
        return;
        
    case eBlastOpt_HitlistSize:
        x_SetParam("HitlistSize", v);
        return;
        
    case eBlastOpt_CutoffScore:
        if (0) {
            typedef objects::CBlast4_cutoff TCutoff;
            CRef<TCutoff> cutoff(new TCutoff);
            cutoff->SetRaw_score(v);
            
            x_SetParam("CutoffScore", cutoff);
        }
        return;
        
    case eBlastOpt_MatchReward:
        x_SetParam("MatchReward", v);
        return;
        
    case eBlastOpt_MismatchPenalty:
        x_SetParam("MismatchPenalty", v);
        return;

    case eBlastOpt_WordThreshold:
        x_SetParam("WordThreshold", v);
        return;
        
    case eBlastOpt_PseudoCount:
        x_SetParam("PseudoCountWeight", v);
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
    switch(opt) {
    case eBlastOpt_EvalueThreshold:
        {
            typedef objects::CBlast4_cutoff TCutoff;
            CRef<TCutoff> cutoff(new TCutoff);
            cutoff->SetE_value(v);
            
            x_SetParam("EvalueThreshold", cutoff);
        }
        return;
        
    case eBlastOpt_PercentIdentity:
        x_SetParam("PercentIdentity", v);
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
    switch(opt) {
    case eBlastOpt_FilterString:
        x_SetParam("FilterString", v);
        return;
        
    case eBlastOpt_MatrixName:
        x_SetParam("MatrixName", v);
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
    char errbuf[1024];
    
    sprintf(errbuf, "tried to set option (%d) and TSeqLocVector (size %zd), line (%d).",
            int(opt), v.size(), __LINE__);
    
    x_Throwx(string("err:") + errbuf);
}

void CBlastOptionsRemote::SetValue(EBlastOptIdx opt, const ESeedContainerType & v)
{
    char errbuf[1024];
    
    sprintf(errbuf, "tried to set option (%d) and value (%d), line (%d).",
            int(opt), v, __LINE__);
    
    x_Throwx(string("err:") + errbuf);
}

void CBlastOptionsRemote::SetValue(EBlastOptIdx opt, const ESeedExtensionMethod & v)
{
    char errbuf[1024];
    
    sprintf(errbuf, "tried to set option (%d) and value (%d), line (%d).",
            int(opt), v, __LINE__);
    
    x_Throwx(string("err:") + errbuf);
}

void CBlastOptionsRemote::SetValue(EBlastOptIdx opt, const bool & v)
{
    switch(opt) {
    case eBlastOpt_GappedMode:
        {
            bool ungapped = ! v;
            x_SetParam("UngappedMode", ungapped); // inverted
            return;
        }
        
    case eBlastOpt_OutOfFrameMode:
        x_SetParam("OutOfFrameMode", v);
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
    switch(opt) {
    case eBlastOpt_EffectiveSearchSpace:
        x_SetParam("EffectiveSearchSpace", v);
        return;
        
    default:
        break;
    }
    
    char errbuf[1024];
    
    sprintf(errbuf, "tried to set option (%d) and value (%f), line (%d).",
            int(opt), double(v), __LINE__);
    
    x_Throwx(string("err:") + errbuf);
}


CBlastOptionsLocal::CBlastOptionsLocal()
{
    QuerySetUpOptions* query_setup = NULL;
    BlastQuerySetUpOptionsNew(&query_setup);
    m_QueryOpts.Reset(query_setup);
    m_InitWordOpts.Reset((BlastInitialWordOptions*)calloc(1, sizeof(BlastInitialWordOptions)));
    m_LutOpts.Reset((LookupTableOptions*)calloc(1, sizeof(LookupTableOptions)));
    m_ExtnOpts.Reset((BlastExtensionOptions*)calloc(1, sizeof(BlastExtensionOptions)));
    m_HitSaveOpts.Reset((BlastHitSavingOptions*)calloc(1, sizeof(BlastHitSavingOptions)));
    m_ScoringOpts.Reset((BlastScoringOptions*)calloc(1, sizeof(BlastScoringOptions)));
    m_EffLenOpts.Reset((BlastEffectiveLengthsOptions*)calloc(1, sizeof(BlastEffectiveLengthsOptions)));
    m_DbOpts.Reset((BlastDatabaseOptions*)calloc(1, sizeof(BlastDatabaseOptions)));
    m_PSIBlastOpts.Reset((PSIBlastOptions*)calloc(1, sizeof(PSIBlastOptions)));
}

CBlastOptionsLocal::~CBlastOptionsLocal()
{
}

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
CBlastOptionsLocal::GetVariableWordSize() const
{
    return m_LutOpts->variable_wordsize ? true: false;
}

inline void
CBlastOptionsLocal::SetVariableWordSize(bool val)
{
    m_LutOpts->variable_wordsize = val;
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

inline bool
CBlastOptionsLocal::GetUsePssm() const
{
    return m_LutOpts->use_pssm ? true : false;
}

inline void
CBlastOptionsLocal::SetUsePssm(bool use_pssm)
{
    m_LutOpts->use_pssm = use_pssm;
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

inline int
CBlastOptionsLocal::GetRequiredStart() const
{
    return m_HitSaveOpts->required_start;
}

inline void
CBlastOptionsLocal::SetRequiredStart(int s)
{
    m_HitSaveOpts->required_start = s;
}

inline int
CBlastOptionsLocal::GetRequiredEnd() const
{
    return m_HitSaveOpts->required_end;
}

inline void
CBlastOptionsLocal::SetRequiredEnd(int e)
{
    m_HitSaveOpts->required_end = e;
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

#define GENCODE_STRLEN 64

void 
CBlastOptionsLocal::SetDbGeneticCode(int gc)
{

    m_DbOpts->genetic_code = gc;

    if (m_DbOpts->gen_code_string) 
        sfree(m_DbOpts->gen_code_string);

    m_DbOpts->gen_code_string = (Uint1*)
        BlastMemDup(FindGeneticCode(gc).get(), GENCODE_STRLEN);
}

EBlastProgramType 
CBlastOptionsLocal::GetProgramType() const
{
    switch (m_Program) {
    case eBlastn: case eMegablast: case eDiscMegablast:
        return eBlastTypeBlastn;
    case eBlastp: 
        return eBlastTypeBlastp;
    case eBlastx:
        return eBlastTypeBlastx;
    case eTblastn:
        return eBlastTypeTblastn;
    case eTblastx:
        return eBlastTypeTblastx;
    case eRPSBlast:
        return eBlastTypeRpsBlast;
    case eRPSTblastn:
        return eBlastTypeRpsTblastn;
    case ePSIBlast:
        return eBlastTypePsiBlast;
    default:
        return eBlastTypeUndefined;
    }
}

static void 
BlastMessageToException(Blast_Message** blmsg_ptr, const string& default_msg)
{
    if (!blmsg_ptr || *blmsg_ptr == NULL)
        return;

    Blast_Message* blmsg = *blmsg_ptr;
    string msg = blmsg ? blmsg->message : default_msg;

    *blmsg_ptr = Blast_MessageFree(blmsg);

    if (msg != NcbiEmptyString)
        NCBI_THROW(CBlastException, eBadParameter, msg.c_str());
}

bool
CBlastOptionsLocal::Validate() const
{
    Blast_Message* blmsg = NULL;
    string msg;
    EBlastProgramType program = GetProgramType();

    if (BlastScoringOptionsValidate(program, m_ScoringOpts, &blmsg)) {
        BlastMessageToException(&blmsg, "Scoring options validation failed");
    }

    if (LookupTableOptionsValidate(program, m_LutOpts, &blmsg)) {
        BlastMessageToException(&blmsg, "Lookup table options validation failed");
    }

    if (BlastInitialWordOptionsValidate(program, m_InitWordOpts, &blmsg)) {
        BlastMessageToException(&blmsg, "Word finder options validation failed");
    }

    if (BlastHitSavingOptionsValidate(program, m_HitSaveOpts, &blmsg)) {
        BlastMessageToException(&blmsg, "Hit saving options validation failed");
    }

    if (BlastExtensionOptionsValidate(program, m_ExtnOpts, &blmsg)) {
        BlastMessageToException(&blmsg, "Extension options validation failed");
    }

    return true;
}

void
CBlastOptionsLocal::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
    ddc.SetFrame("CBlastOptionsLocal");
    DebugDumpValue(ddc,"m_Program", m_Program);
    m_QueryOpts.DebugDump(ddc, depth);
    m_LutOpts.DebugDump(ddc, depth);
    m_InitWordOpts.DebugDump(ddc, depth);
    m_ExtnOpts.DebugDump(ddc, depth);
    m_HitSaveOpts.DebugDump(ddc, depth);
    m_PSIBlastOpts.DebugDump(ddc, depth);
    m_DbOpts.DebugDump(ddc, depth);
    m_ScoringOpts.DebugDump(ddc, depth);
    m_EffLenOpts.DebugDump(ddc, depth);
}

inline int
x_safe_strcmp(const char* a, const char* b)
{
    if (a != b) {
        if (a != NULL && b != NULL) {
            return strcmp(a,b);
        } else {
            return 1;
        }
    }
    return 0;
}

inline int
x_safe_memcmp(const void* a, const void* b, size_t size)
{
    if (a != b) {
        if (a != NULL && b != NULL) {
            return memcmp(a, b, size);
        } else {
            return 1;
        }
    }
    return 0;
}

bool
x_QuerySetupOptions_cmp(const QuerySetUpOptions* a, const QuerySetUpOptions* b)
{
    if (x_safe_strcmp(a->filter_string, b->filter_string) != 0) {
        return false;
    }
    if (a->strand_option != b->strand_option) return false;
    if (a->genetic_code != b->genetic_code) return false;
    return true;
}

bool
x_LookupTableOptions_cmp(const LookupTableOptions* a, 
                         const LookupTableOptions* b)
{
    if (a->threshold != b->threshold) return false;
    if (a->lut_type != b->lut_type) return false;
    if (a->word_size != b->word_size) return false;
    if (a->mb_template_length != b->mb_template_length) return false;
    if (a->mb_template_type != b->mb_template_type) return false;
    if (a->max_num_patterns != b->max_num_patterns) return false;
    if (a->use_pssm != b->use_pssm) return false;
    if (x_safe_strcmp(a->phi_pattern, b->phi_pattern) != 0) return false;
    if (a->variable_wordsize != b->variable_wordsize) return false;
    return true;
}

bool
x_BlastDatabaseOptions_cmp(const BlastDatabaseOptions* a,
                           const BlastDatabaseOptions* b)
{
    if (a->genetic_code != b->genetic_code) return false;
    if (x_safe_memcmp((void*)a->gen_code_string, 
                      (void*)b->gen_code_string, GENCODE_STRLEN) != 0)
        return false;
    return true;
}

bool
x_BlastScoringOptions_cmp(const BlastScoringOptions* a,
                          const BlastScoringOptions* b)
{
    if (x_safe_strcmp(a->matrix, b->matrix) != 0) return false;
    if (x_safe_strcmp(a->matrix_path, b->matrix_path) != 0) return false;
    if (a->reward != b->reward) return false;
    if (a->penalty != b->penalty) return false;
    if (a->gapped_calculation != b->gapped_calculation) return false;
    if (a->gap_open != b->gap_open) return false;
    if (a->gap_extend != b->gap_extend) return false;
    if (a->decline_align != b->decline_align) return false;
    if (a->is_ooframe != b->is_ooframe) return false;
    if (a->shift_pen != b->shift_pen) return false;
    return true;
}

bool
CBlastOptionsLocal::operator==(const CBlastOptionsLocal& rhs) const
{
    if (this == &rhs)
        return true;

    if (m_Program != rhs.m_Program)
        return false;

    if ( !x_QuerySetupOptions_cmp(m_QueryOpts, rhs.m_QueryOpts) )
        return false;

    if ( !x_LookupTableOptions_cmp(m_LutOpts, rhs.m_LutOpts) )
        return false;

    void *a, *b;

    a = static_cast<void*>( (BlastInitialWordOptions*) m_InitWordOpts);
    b = static_cast<void*>( (BlastInitialWordOptions*) rhs.m_InitWordOpts);
    if ( x_safe_memcmp(a, b, sizeof(BlastInitialWordOptions)) != 0 )
         return false;

    a = static_cast<void*>( (BlastExtensionOptions*) m_ExtnOpts);
    b = static_cast<void*>( (BlastExtensionOptions*) rhs.m_ExtnOpts);
    if ( x_safe_memcmp(a, b, sizeof(BlastExtensionOptions)) != 0 )
         return false;

    a = static_cast<void*>( (BlastHitSavingOptions*) m_HitSaveOpts);
    b = static_cast<void*>( (BlastHitSavingOptions*) rhs.m_HitSaveOpts);
    if ( x_safe_memcmp(a, b, sizeof(BlastHitSavingOptions)) != 0 )
         return false;

    a = static_cast<void*>( (PSIBlastOptions*) m_PSIBlastOpts);
    b = static_cast<void*>( (PSIBlastOptions*) rhs.m_PSIBlastOpts);
    if ( x_safe_memcmp(a, b, sizeof(PSIBlastOptions)) != 0 )
         return false;

    if ( !x_BlastDatabaseOptions_cmp(m_DbOpts, rhs.m_DbOpts) )
        return false;

    if ( !x_BlastScoringOptions_cmp(m_ScoringOpts, rhs.m_ScoringOpts) )
        return false;
    
    a = static_cast<void*>( (BlastEffectiveLengthsOptions*) m_EffLenOpts);
    b = static_cast<void*>( (BlastEffectiveLengthsOptions*) rhs.m_EffLenOpts);
    if ( x_safe_memcmp(a, b, sizeof(BlastEffectiveLengthsOptions)) != 0 )
         return false;
    
    return true;
}

bool
CBlastOptionsLocal::operator!=(const CBlastOptionsLocal& rhs) const
{
    return !(*this== rhs);
}

bool
CBlastOptions::operator==(const CBlastOptions& rhs) const
{
    if (m_Local && rhs.m_Local) {
        return (*m_Local == *rhs.m_Local);
    } else {
        NCBI_THROW(CBlastException, eInternal, 
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
int 
CBlastOptions::GetWordThreshold() const
{
    if (! m_Local) {
        x_Throwx("Error: GetWordThreshold() not available.");
    }
    return m_Local->GetWordThreshold();
}

void 
CBlastOptions::SetWordThreshold(int w)
{
    if (m_Local) {
        m_Local->SetWordThreshold(w);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_WordThreshold, w);
    }
}

int 
CBlastOptions::GetLookupTableType() const
{
    if (! m_Local) {
        x_Throwx("Error: GetLookupTableType() not available.");
    }
    return m_Local->GetLookupTableType();
}
void 
CBlastOptions::SetLookupTableType(int type)
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

int 
CBlastOptions::GetMBMaxPositions() const
{
    if (! m_Local) {
        x_Throwx("Error: GetMBMaxPositions() not available.");
    }
    return m_Local->GetMBMaxPositions();
}
void 
CBlastOptions::SetMBMaxPositions(int m)
{
    if (m_Local) {
        m_Local->SetMBMaxPositions(m);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_MBMaxPositions, m);
    }
}

bool 
CBlastOptions::GetVariableWordSize() const
{
    if (! m_Local) {
        x_Throwx("Error: GetVariableWordSize() not available.");
    }
    return m_Local->GetVariableWordSize();
}
void 
CBlastOptions::SetVariableWordSize(bool val)
{
    if (m_Local) {
        m_Local->SetVariableWordSize(val);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_VariableWordSize, val);
    }
}

bool 
CBlastOptions::GetFullByteScan() const
{
    if (! m_Local) {
        x_Throwx("Error: GetFullByteScan() not available.");
    }
    return m_Local->GetFullByteScan();
}
void 
CBlastOptions::SetFullByteScan(bool val)
{
    if (m_Local) {
        m_Local->SetFullByteScan(val);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_FullByteScan, val);
    }
}

bool 
CBlastOptions::GetUsePssm() const
{
    if (! m_Local) {
        x_Throwx("Error: GetUsePssm() not available.");
    }
    return m_Local->GetUsePssm();
}
void 
CBlastOptions::SetUsePssm(bool m)
{
    if (m_Local) {
        m_Local->SetUsePssm(m);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_UsePssm, m);
    }
}

/******************* Query setup options ************************/
const char* 
CBlastOptions::GetFilterString() const
{
    if (! m_Local) {
        x_Throwx("Error: GetFilterString() not available.");
    }
    return m_Local->GetFilterString();
}
void 
CBlastOptions::SetFilterString(const char* f)
{
    if (m_Local) {
        m_Local->SetFilterString(f);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_FilterString, f);
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

bool 
CBlastOptions::GetUngappedExtension() const
{
    if (! m_Local) {
        x_Throwx("Error: GetUngappedExtension() not available.");
    }
    return m_Local->GetUngappedExtension();
}
void 
CBlastOptions::SetUngappedExtension(bool val)
{
    if (m_Local) {
        m_Local->SetUngappedExtension(val);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_UngappedExtension, val);
    }
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

bool 
CBlastOptions::GetCompositionBasedStatsMode() const
{
    if (! m_Local) {
        x_Throwx("Error: GetCompositionBasedStatsMode() not available.");
    }
    return m_Local->GetCompositionBasedStatsMode();
}

void 
CBlastOptions::SetCompositionBasedStatsMode(bool m)
{
    if (m_Local) {
        m_Local->SetCompositionBasedStatsMode(m);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_CompositionBasedStatsMode, m);
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

int 
CBlastOptions::GetRequiredStart() const
{
    if (! m_Local) {
        x_Throwx("Error: GetRequiredStart() not available.");
    }
    return m_Local->GetRequiredStart();
}
void 
CBlastOptions::SetRequiredStart(int s)
{
    if (m_Local) {
        m_Local->SetRequiredStart(s);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_RequiredStart, s);
    }
}

int 
CBlastOptions::GetRequiredEnd() const
{
    if (! m_Local) {
        x_Throwx("Error: GetRequiredEnd() not available.");
    }
    return m_Local->GetRequiredEnd();
}
void 
CBlastOptions::SetRequiredEnd(int e)
{
    if (m_Local) {
        m_Local->SetRequiredEnd(e);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_RequiredEnd, e);
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

const char* 
CBlastOptions::GetMatrixPath() const
{
    if (! m_Local) {
        x_Throwx("Error: GetMatrixPath() not available.");
    }
    return m_Local->GetMatrixPath();
}
void 
CBlastOptions::SetMatrixPath(const char* path)
{
    if (m_Local) {
        m_Local->SetMatrixPath(path);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_MatrixPath, path);
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

int 
CBlastOptions::GetDecline2AlignPenalty() const
{
    if (! m_Local) {
        x_Throwx("Error: GetDecline2AlignPenalty() not available.");
    }
    return m_Local->GetDecline2AlignPenalty();
}
void 
CBlastOptions::SetDecline2AlignPenalty(int p)
{
    if (m_Local) {
        m_Local->SetDecline2AlignPenalty(p);
    }
    if (m_Remote) {
        m_Remote->SetValue(eBlastOpt_Decline2AlignPenalty, p);
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
        m_Remote->DoneDefaults();
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
    NCBI_THROW(CBlastException, eInternal, msg);
}

#endif /* SKIP_DOXYGEN_PROCESSING */

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
* ===========================================================================
*
* $Log$
* Revision 1.65  2005/05/24 14:08:40  madden
* Add [GS]etSmithWatermanMode
*
* Revision 1.64  2005/05/16 12:24:37  madden
* Remove references to [GS]etPrelimHitlistSize
*
* Revision 1.63  2005/04/27 19:58:15  dondosha
* BlastScoreBlk::effective_search_sp field no longer exists
*
* Revision 1.62  2005/04/27 14:40:28  papadopo
* modify member functions that set/get culling limits
*
* Revision 1.61  2005/03/31 13:45:35  camacho
* BLAST options API clean-up
*
* Revision 1.60  2005/03/02 16:45:36  camacho
* Remove use_real_db_size
*
* Revision 1.59  2005/02/24 13:46:54  madden
* Changes to use structured filteing options instead of string
*
* Revision 1.58  2005/01/26 18:13:16  camacho
* Fix compiler warning
*
* Revision 1.57  2005/01/10 13:36:06  madden
* Removed deleted options from x_LookupTableOptions_cmp
*
* Revision 1.56  2004/12/28 16:42:32  camacho
* Consistently use the RAII idiom for C structures using wrapper classes in CBlastOptions
*
* Revision 1.55  2004/11/09 20:07:35  camacho
* Fix CBlastOptionsLocal::GetProgramType for PSI-BLAST
*
* Revision 1.54  2004/10/04 18:24:46  bealer
* - Add Pseudo Count constant to remote blast switches.
*
* Revision 1.53  2004/09/08 18:32:31  camacho
* Doxygen fixes
*
* Revision 1.52  2004/08/30 16:53:43  dondosha
* Added E in front of enum names ESeedExtensionMethod and ESeedContainerType
*
* Revision 1.51  2004/08/26 20:54:10  dondosha
* Removed printouts; throw exception when setting of unavailable option is attempted for remote handle
*
* Revision 1.50  2004/08/18 18:14:13  camacho
* Remove GetProgramFromBlastProgramType, add ProgramNameToEnum
*
* Revision 1.49  2004/08/17 15:13:00  ivanov
* Moved GetProgramFromBlastProgramType() from blast_setup_cxx.cpp
* to blast_options_cxx.cpp
*
* Revision 1.48  2004/08/13 17:56:45  dondosha
* Memory leak fix in Validate method: throw exception after first failure
*
* Revision 1.47  2004/08/03 20:22:08  dondosha
* Added initial word options validation
*
* Revision 1.46  2004/07/06 15:48:40  dondosha
* Use EBlastProgramType enumeration type instead of EProgram when calling C code
*
* Revision 1.45  2004/06/08 15:20:22  dondosha
* Skip traceback option has been moved to the traceback extension method enum
*
* Revision 1.44  2004/05/21 21:41:02  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.43  2004/05/17 18:12:29  bealer
* - Add PSI-Blast support.
*
* Revision 1.42  2004/05/17 15:31:31  madden
* Removed unneeded blast_gapalign.h include
*
* Revision 1.41  2004/03/24 22:12:46  dondosha
* Fixed memory leaks
*
* Revision 1.40  2004/03/19 19:22:55  camacho
* Move to doxygen group AlgoBlast, add missing CVS logs at EOF
*
* Revision 1.39  2004/02/17 23:53:31  dondosha
* Added setting of preliminary hitlist size
*
* Revision 1.38  2004/02/04 22:33:36  bealer
* - Add 'noop' default cases to eliminate compiler warnings.
*
* Revision 1.37  2004/01/20 17:53:01  bealer
* - Add SkipTraceback option.
*
* Revision 1.36  2004/01/20 17:06:50  camacho
* Made operator== a member function
*
* Revision 1.35  2004/01/20 15:59:40  camacho
* Added missing implementations of overloaded operators
*
* Revision 1.34  2004/01/17 00:52:32  ucko
* Remove redundant default argument specification.
*
* Revision 1.33  2004/01/16 21:49:26  bealer
* - Add locality flag for Blast4 API
*
* Revision 1.32  2003/12/04 18:35:33  dondosha
* Correction in assigning the genetic code string option
*
* Revision 1.31  2003/12/03 16:41:16  dondosha
* Added SetDbGeneticCode implementation, to set both integer and string
*
* Revision 1.30  2003/11/26 19:37:59  camacho
* Fix windows problem with std::memcmp
*
* Revision 1.29  2003/11/26 18:36:45  camacho
* Renaming blast_option*pp -> blast_options*pp
*
* Revision 1.28  2003/11/26 18:23:59  camacho
* +Blast Option Handle classes
*
* Revision 1.27  2003/10/30 19:37:36  dondosha
* Removed extra stuff accidentally committed
*
* Revision 1.26  2003/10/30 19:34:53  dondosha
* Removed gapped_calculation from BlastHitSavingOptions structure
*
* Revision 1.25  2003/10/21 22:15:33  camacho
* Rearranging of C options structures, fix seed extension method
*
* Revision 1.24  2003/10/21 09:41:14  camacho
* Initialize variable_wordsize for eBlastn
*
* Revision 1.23  2003/10/17 18:21:53  dondosha
* Use separate variables for different initial word extension options
*
* Revision 1.22  2003/10/02 22:10:46  dondosha
* Corrections for one-strand translated searches
*
* Revision 1.21  2003/09/11 17:45:03  camacho
* Changed CBlastOption -> CBlastOptions
*
* Revision 1.20  2003/09/09 22:13:36  dondosha
* Added SetDbGeneticCodeAndStr method to set both integer and string genetic code in one call
*
* Revision 1.19  2003/09/09 12:57:15  camacho
* + internal setup functions, use smart pointers to handle memory mgmt
*
* Revision 1.18  2003/09/03 19:36:27  camacho
* Fix include path for blast_setup.hpp
*
* Revision 1.17  2003/09/03 18:45:34  camacho
* Fixed small memory leak, removed unneeded function
*
* Revision 1.16  2003/09/02 21:15:11  camacho
* Fix small memory leak
*
* Revision 1.15  2003/08/27 15:05:56  camacho
* Use symbolic name for alphabet sizes
*
* Revision 1.14  2003/08/21 19:32:08  dondosha
* Call SetDbGeneticCodeStr when creating a database gen. code string, to avoid code duplication
*
* Revision 1.13  2003/08/19 20:28:10  dondosha
* EProgram enum type is no longer part of CBlastOptions class
*
* Revision 1.12  2003/08/19 13:46:13  dicuccio
* Added 'USING_SCOPE(objects)' to .cpp files for ease of reading implementation.
*
* Revision 1.11  2003/08/18 20:58:57  camacho
* Added blast namespace, removed *__.hpp includes
*
* Revision 1.10  2003/08/14 19:07:32  dondosha
* Added BLASTGetEProgram function to convert from Uint1 to enum type
*
* Revision 1.9  2003/08/11 15:17:39  dondosha
* Added algo/blast/core to all #included headers
*
* Revision 1.8  2003/08/08 19:43:07  dicuccio
* Compilation fixes: #include file rearrangement; fixed use of 'list' and
* 'vector' as variable names; fixed missing ostrea<< for __int64
*
* Revision 1.7  2003/08/01 22:34:11  camacho
* Added accessors/mutators/defaults for matrix_path
*
* Revision 1.6  2003/07/31 19:45:33  camacho
* Eliminate Ptr notation
*
* Revision 1.5  2003/07/30 15:00:01  camacho
* Do not use Malloc/MemNew/MemFree
*
* Revision 1.4  2003/07/24 18:24:17  camacho
* Minor
*
* Revision 1.3  2003/07/23 21:29:37  camacho
* Update BlastDatabaseOptions
*
* Revision 1.2  2003/07/15 19:22:04  camacho
* Fix setting of scan step in blastn
*
* Revision 1.1  2003/07/10 18:34:19  camacho
* Initial revision
*
*
* ===========================================================================
*/
