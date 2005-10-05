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
        NCBI_THROW(CBlastException, eInvalidOptions, msg);
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

    case eBlastOpt_SegFiltering:
        {
            const char* filter_string = v ? "T" : "F";
            x_SetParam("FilterString", filter_string);
            return;
        }
        
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
    NCBI_THROW(CBlastException, eInvalidOptions, msg);
}

#endif /* SKIP_DOXYGEN_PROCESSING */

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
* ===========================================================================
*
* $Log$
* Revision 1.72  2005/10/05 17:45:28  camacho
* + rcsid string
*
* Revision 1.71  2005/10/05 14:59:23  camacho
* Fix handling of eBlastOpt_SegFiltering in CBlastOptionsRemote
*
* Revision 1.70  2005/09/21 15:09:48  camacho
* Enable use of CBlastOptionsMemento
*
* Revision 1.69  2005/07/12 21:19:04  camacho
* Experimental: added CBlastOptionsMemento
*
* Revision 1.68  2005/07/07 16:32:11  camacho
* Revamping of BLAST exception classes and error codes
*
* Revision 1.67  2005/06/02 16:19:01  camacho
* Remove LookupTableOptions::use_pssm
*
* Revision 1.66  2005/05/26 14:34:09  dondosha
* Added PHI BLAST cases to GetProgramType method
*
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
