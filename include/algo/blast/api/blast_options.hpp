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
 * Class to encapsulate all NewBlast options
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
class CBlastGapAlignTest;
class CBlastDbTest;
class CBlastTraceBackTest; 
class CScoreBlkTest; 
class CRPSTest; 
class CBlastRedoAlignmentTest; 
class CBlastEngineTest;
class CBlastSetupTest;
class CPsimodulestestApplication;

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
    eBlastOpt_PrelimHitlistSize,
    eBlastOpt_MaxNumHspPerSequence,
    eBlastOpt_CullingMode,
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
    eBlastOpt_CompositionBasedStatsMode
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

    /******************* Hit saving options *************************/
    int GetHitlistSize() const;
    void SetHitlistSize(int s);

    int GetPrelimHitlistSize() const;
    void SetPrelimHitlistSize(int s);

    int GetMaxNumHspPerSequence() const;
    void SetMaxNumHspPerSequence(int m);

    bool GetCullingMode() const;
    void SetCullingMode(bool m = true);

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
    
    QuerySetUpOptions * GetQueryOpts() const
    {
        return m_QueryOpts;
    }
    
    LookupTableOptions * GetLutOpts() const
    {
        return m_LutOpts;
    }
    
    BlastInitialWordOptions * GetInitWordOpts() const
    {
        return m_InitWordOpts;
    }
    
    BlastExtensionOptions * GetExtnOpts() const
    {
        return m_ExtnOpts;
    }
    
    BlastHitSavingOptions * GetHitSaveOpts() const
    {
        return m_HitSaveOpts;
    }
    
    PSIBlastOptions * GetPSIBlastOpts() const
    {
        return m_PSIBlastOpts;
    }
    
    BlastDatabaseOptions * GetDbOpts() const
    {
        return m_DbOpts;
    }
    
    BlastScoringOptions * GetScoringOpts() const
    {
        return m_ScoringOpts;
    }
    
    BlastEffectiveLengthsOptions * GetEffLenOpts() const
    {
        return m_EffLenOpts;
    }
    
    bool operator==(const CBlastOptionsLocal& rhs) const;
    bool operator!=(const CBlastOptionsLocal& rhs) const;

protected:

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

private:
    /// Prohibit copy c-tor 
    CBlastOptionsLocal(const CBlastOptionsLocal& bo);
    /// Prohibit assignment operator
    CBlastOptionsLocal& operator=(const CBlastOptionsLocal& bo);

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


/// Encapsulates all blast input parameters
class NCBI_XBLAST_EXPORT CBlastOptions : public CObject
{
public:
    enum EAPILocality {
        eLocal,
        eRemote,
        eBoth
    };
    
    CBlastOptions(EAPILocality locality = eLocal);
    ~CBlastOptions();
    
    EAPILocality GetLocality(void)
    {
        if (! m_Remote) {
            return eLocal;
        }
        if (! m_Local) {
            return eRemote;
        }
        return eBoth;
    }
    
//     void SetProgramService(const char * program, const char * service)
//     {
//         if (m_Remote) {
//             m_Remote->SetProgramService(program, service);
//         }
//     }
    
    /// Validate the options
    bool Validate() const
    {
        bool local_okay  = m_Local  ? (m_Local ->Validate()) : true;
        
        return local_okay;
    }
    
    /// Accessors/Mutators for individual options

    EProgram GetProgram() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetProgram() not available.");
        }
        return m_Local->GetProgram();
    }

    EBlastProgramType GetProgramType() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetProgramType() not available.");
        }
        return m_Local->GetProgramType();
    }

    void SetProgram(EProgram p)
    {
        if (m_Local) {
            m_Local->SetProgram(p);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_Program, p);
        }
    }

    /******************* Lookup table options ***********************/
    int GetWordThreshold() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetWordThreshold() not available.");
        }
        return m_Local->GetWordThreshold();
    }
    void SetWordThreshold(int w)
    {
        if (m_Local) {
            m_Local->SetWordThreshold(w);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_WordThreshold, w);
        }
    }

    int GetLookupTableType() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetLookupTableType() not available.");
        }
        return m_Local->GetLookupTableType();
    }
    void SetLookupTableType(int type)
    {
        if (m_Local) {
            m_Local->SetLookupTableType(type);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_LookupTableType, type);
        }
    }

    int GetWordSize() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetWordSize() not available.");
        }
        return m_Local->GetWordSize();
    }
    void SetWordSize(int ws)
    {
        if (m_Local) {
            m_Local->SetWordSize(ws);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_WordSize, ws);
        }
    }

    /// Megablast only lookup table options
    unsigned char GetMBTemplateLength() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetMBTemplateLength() not available.");
        }
        return m_Local->GetMBTemplateLength();
    }
    void SetMBTemplateLength(unsigned char len)
    {
        if (m_Local) {
            m_Local->SetMBTemplateLength(len);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_MBTemplateLength, len);
        }
    }

    unsigned char GetMBTemplateType() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetMBTemplateType() not available.");
        }
        return m_Local->GetMBTemplateType();
    }
    void SetMBTemplateType(unsigned char type)
    {
        if (m_Local) {
            m_Local->SetMBTemplateType(type);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_MBTemplateType, type);
        }
    }

    int GetMBMaxPositions() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetMBMaxPositions() not available.");
        }
        return m_Local->GetMBMaxPositions();
    }
    void SetMBMaxPositions(int m)
    {
        if (m_Local) {
            m_Local->SetMBMaxPositions(m);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_MBMaxPositions, m);
        }
    }

    bool GetVariableWordSize() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetVariableWordSize() not available.");
        }
        return m_Local->GetVariableWordSize();
    }
    void SetVariableWordSize(bool val = true)
    {
        if (m_Local) {
            m_Local->SetVariableWordSize(val);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_VariableWordSize, val);
        }
    }

    bool GetFullByteScan() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetFullByteScan() not available.");
        }
        return m_Local->GetFullByteScan();
    }
    void SetFullByteScan(bool val = true)
    {
        if (m_Local) {
            m_Local->SetFullByteScan(val);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_FullByteScan, val);
        }
    }

    bool GetUsePssm() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetUsePssm() not available.");
        }
        return m_Local->GetUsePssm();
    }
    void SetUsePssm(bool m)
    {
        if (m_Local) {
            m_Local->SetUsePssm(m);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_UsePssm, m);
        }
    }

    /******************* Query setup options ************************/
    const char* GetFilterString() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetFilterString() not available.");
        }
        return m_Local->GetFilterString();
    }
    void SetFilterString(const char* f)
    {
        if (m_Local) {
            m_Local->SetFilterString(f);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_FilterString, f);
        }
    }

    bool GetMaskAtHash() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetMaskAtHash() not available.");
        }
        return m_Local->GetMaskAtHash();
    }
    void SetMaskAtHash(bool val = true)
    {
        if (m_Local) {
            m_Local->SetMaskAtHash(val);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_MaskAtHash, val);
        }
    }

    bool GetDustFiltering() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetDustFiltering() not available.");
        }
        return m_Local->GetDustFiltering();
    }
    void SetDustFiltering(bool val = true)
    {
        if (m_Local) {
            m_Local->SetDustFiltering(val);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_DustFiltering, val);
        }
    }

    int GetDustFilteringLevel() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetDustFilteringLevel() not available.");
        }
        return m_Local->GetDustFilteringLevel();
    }
    void SetDustFilteringLevel(int m)
    {
        if (m_Local) {
            m_Local->SetDustFilteringLevel(m);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_DustFilteringLevel, m);
        }
    }

    int GetDustFilteringWindow() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetDustFilteringWindow() not available.");
        }
        return m_Local->GetDustFilteringWindow();
    }
    void SetDustFilteringWindow(int m)
    {
        if (m_Local) {
            m_Local->SetDustFilteringWindow(m);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_DustFilteringWindow, m);
        }
    }

    int GetDustFilteringLinker() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetDustFilteringLinker() not available.");
        }
        return m_Local->GetDustFilteringLinker();
    }
    void SetDustFilteringLinker(int m)
    {
        if (m_Local) {
            m_Local->SetDustFilteringLinker(m);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_DustFilteringLinker, m);
        }
    }

    bool GetSegFiltering() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetSegFiltering() not available.");
        }
        return m_Local->GetSegFiltering();
    }
    void SetSegFiltering(bool val = true)
    {
        if (m_Local) {
            m_Local->SetSegFiltering(val);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_SegFiltering, val);
        }
    }

    int GetSegFilteringWindow() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetSegFilteringWindow() not available.");
        }
        return m_Local->GetSegFilteringWindow();
    }
    void SetSegFilteringWindow(int m)
    {
        if (m_Local) {
            m_Local->SetSegFilteringWindow(m);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_SegFilteringWindow, m);
        }
    }

    double GetSegFilteringLocut() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetSegFilteringLocut() not available.");
        }
        return m_Local->GetSegFilteringLocut();
    }
    void SetSegFilteringLocut(double m)
    {
        if (m_Local) {
            m_Local->SetSegFilteringLocut(m);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_SegFilteringLocut, m);
        }
    }

    double GetSegFilteringHicut() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetSegFilteringHicut() not available.");
        }
        return m_Local->GetSegFilteringHicut();
    }
    void SetSegFilteringHicut(double m)
    {
        if (m_Local) {
            m_Local->SetSegFilteringHicut(m);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_SegFilteringHicut, m);
        }
    }

    bool GetRepeatFiltering() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetRepeatFiltering() not available.");
        }
        return m_Local->GetRepeatFiltering();
    }
    void SetRepeatFiltering(bool val = true)
    {
        if (m_Local) {
            m_Local->SetRepeatFiltering(val);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_RepeatFiltering, val);
        }
    }

    const char* GetRepeatFilteringDB() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetRepeatFilteringDB() not available.");
        }
        return m_Local->GetRepeatFilteringDB();
    }
    void SetRepeatFilteringDB(const char* db)
    {
        if (m_Local) {
            m_Local->SetRepeatFilteringDB(db);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_RepeatFilteringDB, db);
        }
    }

    objects::ENa_strand GetStrandOption() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetStrandOption() not available.");
        }
        return m_Local->GetStrandOption();
    }
    void SetStrandOption(objects::ENa_strand s)
    {
        if (m_Local) {
            m_Local->SetStrandOption(s);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_StrandOption, s);
        }
    }

    int GetQueryGeneticCode() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetQueryGeneticCode() not available.");
        }
        return m_Local->GetQueryGeneticCode();
    }
    void SetQueryGeneticCode(int gc)
    {
        if (m_Local) {
            m_Local->SetQueryGeneticCode(gc);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_QueryGeneticCode, gc);
        }
    }

    /******************* Initial word options ***********************/
    int GetWindowSize() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetWindowSize() not available.");
        }
        return m_Local->GetWindowSize();
    }
    void SetWindowSize(int w)
    {
        if (m_Local) {
            m_Local->SetWindowSize(w);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_WindowSize, w);
        }
    }

    bool GetUngappedExtension() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetUngappedExtension() not available.");
        }
        return m_Local->GetUngappedExtension();
    }
    void SetUngappedExtension(bool val = true)
    {
        if (m_Local) {
            m_Local->SetUngappedExtension(val);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_UngappedExtension, val);
        }
    }

    double GetXDropoff() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetXDropoff() not available.");
        }
        return m_Local->GetXDropoff();
    }
    void SetXDropoff(double x)
    {
        if (m_Local) {
            m_Local->SetXDropoff(x);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_XDropoff, x);
        }
    }

    /******************* Gapped extension options *******************/
    double GetGapXDropoff() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetGapXDropoff() not available.");
        }
        return m_Local->GetGapXDropoff();
    }
    void SetGapXDropoff(double x)
    {
        if (m_Local) {
            m_Local->SetGapXDropoff(x);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_GapXDropoff, x);
        }
    }

    double GetGapXDropoffFinal() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetGapXDropoffFinal() not available.");
        }
        return m_Local->GetGapXDropoffFinal();
    }
    void SetGapXDropoffFinal(double x)
    {
        if (m_Local) {
            m_Local->SetGapXDropoffFinal(x);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_GapXDropoffFinal, x);
        }
    }

    double GetGapTrigger() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetGapTrigger() not available.");
        }
        return m_Local->GetGapTrigger();
    }
    void SetGapTrigger(double g)
    {
        if (m_Local) {
            m_Local->SetGapTrigger(g);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_GapTrigger, g);
        }
    }

    EBlastPrelimGapExt GetGapExtnAlgorithm() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetGapExtnAlgorithm() not available.");
        }
        return m_Local->GetGapExtnAlgorithm();
    }
    void SetGapExtnAlgorithm(EBlastPrelimGapExt a)
    {
        if (m_Local) {
            m_Local->SetGapExtnAlgorithm(a);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_GapExtnAlgorithm, a);
        }
    }

    EBlastTbackExt GetGapTracebackAlgorithm() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetGapTracebackAlgorithm() not available.");
        }
        return m_Local->GetGapTracebackAlgorithm();
    }

    void SetGapTracebackAlgorithm(EBlastTbackExt a)
    {
        if (m_Local) {
            m_Local->SetGapTracebackAlgorithm(a);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_GapTracebackAlgorithm, a);
        }
    }

    bool GetCompositionBasedStatsMode() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetCompositionBasedStatsMode() not available.");
        }
        return m_Local->GetCompositionBasedStatsMode();
    }

    void SetCompositionBasedStatsMode(bool m = true)
    {
        if (m_Local) {
            m_Local->SetCompositionBasedStatsMode(m);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_CompositionBasedStatsMode, m);
        }
    }
    /******************* Hit saving options *************************/
    int GetHitlistSize() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetHitlistSize() not available.");
        }
        return m_Local->GetHitlistSize();
    }
    void SetHitlistSize(int s)
    {
        if (m_Local) {
            m_Local->SetHitlistSize(s);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_HitlistSize, s);
        }
    }

    int GetPrelimHitlistSize() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetPrelimHitlistSize() not available.");
        }
        return m_Local->GetPrelimHitlistSize();
    }
    void SetPrelimHitlistSize(int s)
    {
        if (m_Local) {
            m_Local->SetPrelimHitlistSize(s);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_PrelimHitlistSize, s);
        }
    }

    int GetMaxNumHspPerSequence() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetMaxNumHspPerSequence() not available.");
        }
        return m_Local->GetMaxNumHspPerSequence();
    }
    void SetMaxNumHspPerSequence(int m)
    {
        if (m_Local) {
            m_Local->SetMaxNumHspPerSequence(m);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_MaxNumHspPerSequence, m);
        }
    }

    bool GetCullingMode() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetCullingMode() not available.");
        }
        return m_Local->GetCullingMode();
    }
    void SetCullingMode(bool m = true)
    {
        if (m_Local) {
            m_Local->SetCullingMode(m);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_CullingMode, m);
        }
    }

    /// Start of the region required to be part of the alignment
    int GetRequiredStart() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetRequiredStart() not available.");
        }
        return m_Local->GetRequiredStart();
    }
    void SetRequiredStart(int s)
    {
        if (m_Local) {
            m_Local->SetRequiredStart(s);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_RequiredStart, s);
        }
    }

    /// End of the region required to be part of the alignment
    int GetRequiredEnd() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetRequiredEnd() not available.");
        }
        return m_Local->GetRequiredEnd();
    }
    void SetRequiredEnd(int e)
    {
        if (m_Local) {
            m_Local->SetRequiredEnd(e);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_RequiredEnd, e);
        }
    }

    // Expect value cut-off threshold for an HSP, or a combined hit if sum
    // statistics is used
    double GetEvalueThreshold() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetEvalueThreshold() not available.");
        }
        return m_Local->GetEvalueThreshold();
    }
    void SetEvalueThreshold(double eval)
    {
        if (m_Local) {
            m_Local->SetEvalueThreshold(eval);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_EvalueThreshold, eval);
        }
    }

    // Raw score cutoff threshold
    int GetCutoffScore() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetCutoffScore() not available.");
        }
        return m_Local->GetCutoffScore();
    }
    void SetCutoffScore(int s)
    {
        if (m_Local) {
            m_Local->SetCutoffScore(s);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_CutoffScore, s);
        }
    }

    double GetPercentIdentity() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetPercentIdentity() not available.");
        }
        return m_Local->GetPercentIdentity();
    }
    void SetPercentIdentity(double p)
    {
        if (m_Local) {
            m_Local->SetPercentIdentity(p);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_PercentIdentity, p);
        }
    }

    /// Sum statistics options
    bool GetSumStatisticsMode() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetSumStatisticsMode() not available.");
        }
        return m_Local->GetSumStatisticsMode();
    }
    void SetSumStatisticsMode(bool m = true)
    {
        if (m_Local) {
            m_Local->SetSumStatisticsMode(m);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_SumStatisticsMode, m);
        }
    }

    int GetLongestIntronLength() const // for linking HSPs with uneven gaps
    {
        if (! m_Local) {
            x_Throwx("Error: GetLongestIntronLength() not available.");
        }
        return m_Local->GetLongestIntronLength();
    }
    void SetLongestIntronLength(int l) // for linking HSPs with uneven gaps
    {
        if (m_Local) {
            m_Local->SetLongestIntronLength(l);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_LongestIntronLength, l);
        }
    }


    /// Returns true if gapped BLAST is set, false otherwise
    bool GetGappedMode() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetGappedMode() not available.");
        }
        return m_Local->GetGappedMode();
    }
    void SetGappedMode(bool m = true)
    {
        if (m_Local) {
            m_Local->SetGappedMode(m);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_GappedMode, m);
        }
    }

    /************************ Scoring options ************************/
    const char* GetMatrixName() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetMatrixName() not available.");
        }
        return m_Local->GetMatrixName();
    }
    void SetMatrixName(const char* matrix)
    {
        if (m_Local) {
            m_Local->SetMatrixName(matrix);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_MatrixName, matrix);
        }
    }

    const char* GetMatrixPath() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetMatrixPath() not available.");
        }
        return m_Local->GetMatrixPath();
    }
    void SetMatrixPath(const char* path)
    {
        if (m_Local) {
            m_Local->SetMatrixPath(path);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_MatrixPath, path);
        }
    }

    int GetMatchReward() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetMatchReward() not available.");
        }
        return m_Local->GetMatchReward();
    }
    void SetMatchReward(int r)
    {
        if (m_Local) {
            m_Local->SetMatchReward(r);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_MatchReward, r);
        }
    }

    int GetMismatchPenalty() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetMismatchPenalty() not available.");
        }
        return m_Local->GetMismatchPenalty();
    }
    void SetMismatchPenalty(int p)
    {
        if (m_Local) {
            m_Local->SetMismatchPenalty(p);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_MismatchPenalty, p);
        }
    }

    int GetGapOpeningCost() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetGapOpeningCost() not available.");
        }
        return m_Local->GetGapOpeningCost();
    }
    void SetGapOpeningCost(int g)
    {
        if (m_Local) {
            m_Local->SetGapOpeningCost(g);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_GapOpeningCost, g);
        }
    }

    int GetGapExtensionCost() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetGapExtensionCost() not available.");
        }
        return m_Local->GetGapExtensionCost();
    }
    void SetGapExtensionCost(int e)
    {
        if (m_Local) {
            m_Local->SetGapExtensionCost(e);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_GapExtensionCost, e);
        }
    }

    int GetFrameShiftPenalty() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetFrameShiftPenalty() not available.");
        }
        return m_Local->GetFrameShiftPenalty();
    }
    void SetFrameShiftPenalty(int p)
    {
        if (m_Local) {
            m_Local->SetFrameShiftPenalty(p);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_FrameShiftPenalty, p);
        }
    }

    int GetDecline2AlignPenalty() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetDecline2AlignPenalty() not available.");
        }
        return m_Local->GetDecline2AlignPenalty();
    }
    void SetDecline2AlignPenalty(int p)
    {
        if (m_Local) {
            m_Local->SetDecline2AlignPenalty(p);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_Decline2AlignPenalty, p);
        }
    }

    bool GetOutOfFrameMode() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetOutOfFrameMode() not available.");
        }
        return m_Local->GetOutOfFrameMode();
    }
    void SetOutOfFrameMode(bool m = true)
    {
        if (m_Local) {
            m_Local->SetOutOfFrameMode(m);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_OutOfFrameMode, m);
        }
    }

    /******************** Effective Length options *******************/
    Int8 GetDbLength() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetDbLength() not available.");
        }
        return m_Local->GetDbLength();
    }
    void SetDbLength(Int8 l)
    {
        if (m_Local) {
            m_Local->SetDbLength(l);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_DbLength, l);
        }
    }

    unsigned int GetDbSeqNum() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetDbSeqNum() not available.");
        }
        return m_Local->GetDbSeqNum();
    }
    void SetDbSeqNum(unsigned int n)
    {
        if (m_Local) {
            m_Local->SetDbSeqNum(n);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_DbSeqNum, n);
        }
    }

    Int8 GetEffectiveSearchSpace() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetEffectiveSearchSpace() not available.");
        }
        return m_Local->GetEffectiveSearchSpace();
    }
    void SetEffectiveSearchSpace(Int8 eff)
    {
        if (m_Local) {
            m_Local->SetEffectiveSearchSpace(eff);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_EffectiveSearchSpace, eff);
        }
    }

    int GetDbGeneticCode() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetDbGeneticCode() not available.");
        }
        return m_Local->GetDbGeneticCode();
    }
    
    //const unsigned char* GetDbGeneticCodeStr() const
    //{
    //    if (! m_Local) {
    //        throw string("Error: GetDbGeneticCodeStr() not available.");
    //    }
    //    return m_Local->GetDbGeneticCodeStr();
    //}
    //void SetDbGeneticCodeStr(const unsigned char* gc_str);
    
    // Set both integer and string genetic code in one call
    void SetDbGeneticCode(int gc)
    {
        if (m_Local) {
            m_Local->SetDbGeneticCode(gc);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_DbGeneticCode, gc);
        }
    }

    /// @todo PSI-Blast options could go on their own subclass?
    const char* GetPHIPattern() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetPHIPattern() not available.");
        }
        return m_Local->GetPHIPattern();
    }
    void SetPHIPattern(const char* pattern, bool is_dna)
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
    double GetInclusionThreshold() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetInclusionThreshold() not available.");
        }
        return m_Local->GetInclusionThreshold();
    }
    void SetInclusionThreshold(double u)
    {
        if (m_Local) {
            m_Local->SetInclusionThreshold(u);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_InclusionThreshold, u);
        }
    }

    int GetPseudoCount() const
    {
        if (! m_Local) {
            x_Throwx("Error: GetPseudoCount() not available.");
        }
        return m_Local->GetPseudoCount();
    }
    void SetPseudoCount(int u)
    {
        if (m_Local) {
            m_Local->SetPseudoCount(u);
        }
        if (m_Remote) {
            m_Remote->SetValue(eBlastOpt_PseudoCount, u);
        }
    }
    
    
    /// Allows to dump a snapshot of the object
    void DebugDump(CDebugDumpContext ddc, unsigned int depth) const
    {
        if (m_Local) {
            m_Local->DebugDump(ddc, depth);
        }
    }
    
    void DoneDefaults() const
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
    typedef ncbi::objects::CBlast4_parameters TBlast4Opts;
    TBlast4Opts * GetBlast4AlgoOpts()
    {
        TBlast4Opts * result = 0;
        
        if (m_Remote) {
            result = m_Remote->GetBlast4AlgoOpts();
        }
        
        return result;
    }
    
    bool operator==(const CBlastOptions& rhs) const;
    bool operator!=(const CBlastOptions& rhs) const;
    
    friend class CBl2Seq;
    friend class CDbBlast;
    friend class CDbBlastTraceback;
    friend class CDbBlastPrelim;

    // Tabular formatting thread needs to calculate parameters structures
    // and hence needs access to individual options structures.
    friend class ::CBlastTabularFormatThread; 

    friend class ::CBlastDbTest;           // unit test class
    friend class ::CBlastGapAlignTest;     // unit test class
    friend class ::CBlastTraceBackTest;    // unit test class
    friend class ::CScoreBlkTest;          // unit test class
    friend class ::CRPSTest;               // unit test class
    friend class ::CBlastRedoAlignmentTest;// unit test class
    friend class ::CBlastSetupTest;        // unit test class
    friend class ::CPsimodulestestApplication;  // bulk test application class
    friend class ::CBlastEngineTest;       // unit test class

protected:
    QuerySetUpOptions * GetQueryOpts() const
    {
        return m_Local ? m_Local->GetQueryOpts() : 0;
    }
    
    LookupTableOptions * GetLutOpts() const
    {
        return m_Local ? m_Local->GetLutOpts() : 0;
    }
    
    BlastInitialWordOptions * GetInitWordOpts() const
    {
        return m_Local ? m_Local->GetInitWordOpts() : 0;
    }
    
    BlastExtensionOptions * GetExtnOpts() const
    {
        return m_Local ? m_Local->GetExtnOpts() : 0;
    }
    
    BlastHitSavingOptions * GetHitSaveOpts() const
    {
        return m_Local ? m_Local->GetHitSaveOpts() : 0;
    }
    
    PSIBlastOptions * GetPSIBlastOpts() const
    {
        return m_Local ? m_Local->GetPSIBlastOpts() : 0;
    }
    
    BlastDatabaseOptions * GetDbOpts() const
    {
        return m_Local ? m_Local->GetDbOpts() : 0;
    }
    
    BlastScoringOptions * GetScoringOpts() const
    {
        return m_Local ? m_Local->GetScoringOpts() : 0;
    }
    
    BlastEffectiveLengthsOptions * GetEffLenOpts() const
    {
        return m_Local ? m_Local->GetEffLenOpts() : 0;
    }
    
private:
    /// Prohibit copy c-tor 
    CBlastOptions(const CBlastOptions& bo);
    /// Prohibit assignment operator
    CBlastOptions& operator=(const CBlastOptions& bo);

    // Pointers to local and remote objects
    
    CBlastOptionsLocal  * m_Local;
    CBlastOptionsRemote * m_Remote;
    
    void x_Throwx(const string& msg ) const
    {
        NCBI_THROW(CBlastException, eInternal, msg);
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
CBlastOptionsLocal::GetPrelimHitlistSize() const
{
    return m_HitSaveOpts->prelim_hitlist_size;
}

inline void
CBlastOptionsLocal::SetPrelimHitlistSize(int s)
{
    m_HitSaveOpts->prelim_hitlist_size = s;
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

inline bool
CBlastOptionsLocal::GetCullingMode() const
{
    return m_HitSaveOpts->perform_culling ? true : false;
}

inline void
CBlastOptionsLocal::SetCullingMode(bool m)
{
    m_HitSaveOpts->perform_culling = m;
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
    m_HitSaveOpts->phi_align = TRUE;
}

#endif /* SKIP_DOXYGEN_PROCESSING */

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
* ===========================================================================
*
* $Log$
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
