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
*/

#ifndef ALGO_BLAST_API___BLAST_OPTION__HPP
#define ALGO_BLAST_API___BLAST_OPTION__HPP

#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/api/blast_exception.hpp>

#include <algo/blast/core/blast_options.h>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CSeq_loc;
END_SCOPE(objects)

BEGIN_SCOPE(blast)


/// @todo Constants used to initialize default values (word size, evalue
/// threshold, ...) should be defined in this module when the C NewBlast is
/// deprecated
#define GENCODE_STRLEN 64

/// Encapsulates all blast input parameters
class NCBI_XBLAST_EXPORT CBlastOption : public CObject
{
public:

    /// Constructor
    CBlastOption(EProgram prog_name = eBlastp) THROWS((CBlastException));
    /// Destructor
    virtual ~CBlastOption();

    /// Validate the options
    bool Validate() const;


    /// Accessors/Mutators for individual options

    EProgram GetProgram() const;
    void SetProgram(EProgram p);

    /******************* Lookup table options ***********************/
    int GetWordThreshold() const;
    void SetWordThreshold(int w);

    int GetLookupTableType() const;
    void SetLookupTableType(int type);

    short GetWordSize() const;
    void SetWordSize(short ws);

    int GetAlphabetSize() const;
    void SetAlphabetSize(int s);

    unsigned char GetScanStep() const;
    void SetScanStep(unsigned char s);

    /// Megablast only lookup table options
    unsigned char GetMBTemplateLength() const;
    void SetMBTemplateLength(unsigned char len);

    unsigned char GetMBTemplateType() const;
    void SetMBTemplateType(unsigned char type);

    int GetMBMaxPositions() const;
    void SetMBMaxPositions(int m);

    /******************* Query setup options ************************/
    const char* GetFilterString() const;
    void SetFilterString(const char* f);

    vector< CConstRef<objects::CSeq_loc> >& GetLCaseMask() const;
    void SetLCaseMask(vector< CConstRef<objects::CSeq_loc> >& sl_vector);

    objects::ENa_strand GetStrandOption() const;
    void SetStrandOption(objects::ENa_strand s);

    int GetQueryGeneticCode() const;
    void SetQueryGeneticCode(int gc);

    /******************* Initial word options ***********************/
    int GetWindowSize() const;
    void SetWindowSize(int w);

    int GetExtendWordMethod() const;
    void SetExtendWordMethod(int ew);

    double GetXDropoff() const;
    void SetXDropoff(double x);

    /******************* Gapped extension options *******************/
    double GetGapXDropoff() const;
    void SetGapXDropoff(double x);

    double GetGapXDropoffFinal() const;
    void SetGapXDropoffFinal(double x);

    double GetGapTrigger() const;
    void SetGapTrigger(double g);

    int GetGapExtnAlgorithm() const;
    void SetGapExtnAlgorithm(int a);

    /******************* Hit saving options *************************/
    int GetHitlistSize() const;
    void SetHitlistSize(int s);

    int GetMaxNumHspPerSequence() const;
    void SetMaxNumHspPerSequence(int m);

    /// Maximum total number of HSPs to keep
    int GetTotalHspLimit() const;
    void SetTotalHspLimit(int l);

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
    void SetEvalueThrehold(double eval);

    double GetOriginalEvalue() const;
    //void SetOriginalEvalue(double e);

    // Raw score cutoff threshold
    int GetCutoffScore() const;
    void SetCutoffScore(int s);

    double GetPercentIdentity() const;
    void SetPercentIdentity(double p);

    /// Sum statistics options
    bool GetSumStatisticsMode() const;
    void SetSumStatisticsMode(bool m = true);

    double GetSingleHSPEvalueThreshold() const;
    void SetSingleHSPEvalueThreshold(double e);

    int GetSingleHSPCutoffScore() const;
    void SetSingleHSPCutoffScore(int s);

    int GetLongestIntronLength() const; // for tblastn w/ linking HSPs
    void SetLongestIntronLength(int l); // for tblastn w/ linking HSPs

    /// Returns true if gapped BLAST is set, false otherwise
    bool GetGappedMode() const;
    void SetGappedMode(bool m = true);

    bool GetNeighboringMode() const;
    void SetNeighboringMode(bool m = true);

    /************************ Scoring options ************************/
    const char* GetMatrixName() const;
    void SetMatrixName(const char* matrix);

    const char* GetMatrixPath() const;
    void SetMatrixPath(const char* path);

    int GetMatchReward() const;
    void SetMatchReward(int r);

    int GetMismatchPenalty() const;
    void SetMismatchPenalty(int p);

    int GetGapOpeningPenalty() const;
    void SetGapOpeningPenalty(int g);

    int GetGapExtensionPenalty() const;
    void SetGapExtensionPenalty(int e);

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

    bool GetUseRealDbSize() const;
    void SetUseRealDbSize(bool u = true);

    int GetDbGeneticCode() const;
    void SetDbGeneticCode(int gc);

    const unsigned char* GetDbGeneticCodeStr() const;
    void SetDbGeneticCodeStr(const unsigned char* gc_str);

    /// @todo PSI-Blast options could go on their own subclass?

    /// Allows to dump a snapshot of the object
    void DebugDump(CDebugDumpContext ddc, unsigned int depth) const;

    QuerySetUpOptions* GetQueryOpts() const { return m_QueryOpts; }
    LookupTableOptions* GetLookupTableOpts() const { return m_LutOpts; }
    BlastInitialWordOptions* GetInitWordOpts() const { return m_InitWordOpts;}
    BlastExtensionOptions* GetExtensionOpts() const { return m_ExtnOpts; }
    BlastHitSavingOptions* GetHitSavingOpts() const { return m_HitSaveOpts; }
    BlastScoringOptions* GetScoringOpts() const { return m_ScoringOpts; }
    BlastEffectiveLengthsOptions* GetEffLenOpts() const { return m_EffLenOpts;}
    BlastDatabaseOptions* GetDbOpts() const { return m_DbOpts; }

protected:

    void SetBlastp();
    void SetBlastn();
    void SetBlastx();
    void SetTblastn();
    void SetTblastx();
    void SetMegablast();

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
    CPSIBlastOptions              m_ProtOpts;

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
    CBlastOption(const CBlastOption& bo);
    /// Prohibit assignment operator
    CBlastOption& operator=(const CBlastOption& bo);
};

inline EProgram
CBlastOption::GetProgram() const
{
    return m_Program;
}

EProgram BLASTGetEProgram(Uint1 prog);

inline void
CBlastOption::SetProgram(EProgram p)
{
    _ASSERT(p >= eBlastn && p < eBlastUndef);
    m_Program = p;

    switch (p) {
    case eBlastn:       SetBlastn();        break;
    case eBlastp:       SetBlastp();        break;
    case eBlastx:       SetBlastx();        break;
    case eTblastn:      SetTblastn();       break;
    case eTblastx:      SetTblastx();       break;
    //case eMegablast:    SetMegablast();     break;
    default:
        NCBI_THROW(CBlastException, eBadParameter, "Invalid program");
    }
}

inline const char*
CBlastOption::GetMatrixName() const
{
    return m_ScoringOpts->matrix;
}

inline void
CBlastOption::SetMatrixName(const char* matrix)
{
    if (!matrix)
        return;

    sfree(m_ScoringOpts->matrix);
    m_ScoringOpts->matrix = strdup(matrix);
}

inline const char* 
CBlastOption::GetMatrixPath() const
{
    return m_ScoringOpts->matrix_path;
}

inline void 
CBlastOption::SetMatrixPath(const char* path)
{
    if (!path)
        return;

    sfree(m_ScoringOpts->matrix_path);
    m_ScoringOpts->matrix_path = strdup(path);
}

inline int
CBlastOption::GetWordThreshold() const
{
    return m_LutOpts->threshold;
}

inline void
CBlastOption::SetWordThreshold(int w)
{
    m_LutOpts->threshold = w;
}

inline int
CBlastOption::GetLookupTableType() const
{
    return m_LutOpts->lut_type;
}

inline void
CBlastOption::SetLookupTableType(int type)
{
    m_LutOpts->lut_type = type;
}

inline short
CBlastOption::GetWordSize() const
{
    return m_LutOpts->word_size;
}

inline void
CBlastOption::SetWordSize(short ws)
{
    m_LutOpts->word_size = ws;
}

inline int
CBlastOption::GetAlphabetSize() const
{
    return m_LutOpts->alphabet_size;
}

inline void
CBlastOption::SetAlphabetSize(int s)
{
    m_LutOpts->alphabet_size = s;
}

inline unsigned char
CBlastOption::GetScanStep() const
{
    return m_LutOpts->scan_step;
}

inline void
CBlastOption::SetScanStep(unsigned char s)
{
    m_LutOpts->scan_step = s;
}

inline unsigned char
CBlastOption::GetMBTemplateLength() const
{
    return m_LutOpts->mb_template_length;
}

inline void
CBlastOption::SetMBTemplateLength(unsigned char len)
{
    m_LutOpts->mb_template_length = len;
}

inline unsigned char
CBlastOption::GetMBTemplateType() const
{
    return m_LutOpts->mb_template_type;
}

inline void
CBlastOption::SetMBTemplateType(unsigned char type)
{
    m_LutOpts->mb_template_type = type;
}

inline int
CBlastOption::GetMBMaxPositions() const
{
    return m_LutOpts->max_positions;
}

inline void
CBlastOption::SetMBMaxPositions(int m)
{
    m_LutOpts->max_positions = m;
}

/******************* Query setup options ************************/
inline const char*
CBlastOption::GetFilterString() const
{
    return m_QueryOpts->filter_string;
}

inline void
CBlastOption::SetFilterString(const char* f)
{
    if (!f)
        return;

    sfree(m_QueryOpts->filter_string);
#if 0
    if (!StringICmp(f, "T")) {
        if (m_Program == eBlastn)
            m_QueryOpts->filter_string = strdup("D");
        else
            m_QueryOpts->filter_string = strdup("S");
    } else {
        m_QueryOpts->filter_string = strdup(f);
    }
#endif
    m_QueryOpts->filter_string = strdup(f);
}

inline objects::ENa_strand
CBlastOption::GetStrandOption() const
{
    return (objects::ENa_strand) m_QueryOpts->strand_option;
}

inline void
CBlastOption::SetStrandOption(objects::ENa_strand s)
{
    m_QueryOpts->strand_option = (unsigned char) s;
}

#if 0
vector< CConstRef<objects::CSeq_loc> >
x_BlastMask2CSeqLoc(BlastMask* mask, EProgram program)
{
    abort();
}

inline vector< CConstRef<objects::CSeq_loc> >& 
CBlastOption::GetLCaseMask() const
{
    // Convert BlastMaskPtrs to objects::CSeq_loc
    abort();
}
#endif


inline void 
CBlastOption::SetLCaseMask(vector< CConstRef<objects::CSeq_loc> >& sl_vector)
{
    BlastMask* mask = NULL,* curr = NULL,* tail = NULL;
    int index = 0;

    // Convert from objects::CSeq_loc's to BlastMask*'s
    // index corresponds to the query index in the case of query concatenation
    ITERATE(vector< CConstRef<objects::CSeq_loc> >, itr, sl_vector) {

        curr = CSeqLoc2BlastMask(*itr, index++);
        if (!mask) {
            mask = tail = curr;
        } else {
            tail->next = curr;
            tail = tail->next;
        }

    }

    m_QueryOpts->lcase_mask = mask;
}

inline int
CBlastOption::GetQueryGeneticCode() const
{
    return m_QueryOpts->genetic_code;
}

inline void
CBlastOption::SetQueryGeneticCode(int gc)
{
    m_QueryOpts->genetic_code = gc;
}

/******************* Initial word options ***********************/
inline int
CBlastOption::GetWindowSize() const
{
    return m_InitWordOpts->window_size;
}

inline void
CBlastOption::SetWindowSize(int s)
{
    m_InitWordOpts->window_size = s;
}

inline int
CBlastOption::GetExtendWordMethod() const
{
    return m_InitWordOpts->extend_word_method;
}

inline void
CBlastOption::SetExtendWordMethod(int ew)
{
    m_InitWordOpts->extend_word_method |= ew;
}

inline double
CBlastOption::GetXDropoff() const
{
    return m_InitWordOpts->x_dropoff;
}

inline void
CBlastOption::SetXDropoff(double x)
{
    m_InitWordOpts->x_dropoff = x;
}

/******************* Gapped extension options *******************/
inline double
CBlastOption::GetGapXDropoff() const
{
    return m_ExtnOpts->gap_x_dropoff;
}

inline void
CBlastOption::SetGapXDropoff(double x)
{
    m_ExtnOpts->gap_x_dropoff = x;
}

inline double
CBlastOption::GetGapXDropoffFinal() const
{
    return m_ExtnOpts->gap_x_dropoff_final;
}

inline void
CBlastOption::SetGapXDropoffFinal(double x)
{
    m_ExtnOpts->gap_x_dropoff_final = x;
}

inline double
CBlastOption::GetGapTrigger() const
{
    return m_ExtnOpts->gap_trigger;
}

inline void
CBlastOption::SetGapTrigger(double g)
{
    m_ExtnOpts->gap_trigger = g;
}

inline int
CBlastOption::GetGapExtnAlgorithm() const
{
    return m_ExtnOpts->algorithm_type;
}

inline void
CBlastOption::SetGapExtnAlgorithm(int a)
{
    m_ExtnOpts->algorithm_type = a;
}

/******************* Hit saving options *************************/
inline int
CBlastOption::GetHitlistSize() const
{
    return m_HitSaveOpts->hitlist_size;
}

inline void
CBlastOption::SetHitlistSize(int s)
{
    m_HitSaveOpts->hitlist_size = s;
}

inline int
CBlastOption::GetMaxNumHspPerSequence() const
{
    return m_HitSaveOpts->hsp_num_max;
}

inline void
CBlastOption::SetMaxNumHspPerSequence(int m)
{
    m_HitSaveOpts->hsp_num_max = m;
}

inline int
CBlastOption::GetTotalHspLimit() const
{
    return m_HitSaveOpts->total_hsp_limit;
}

inline void
CBlastOption::SetTotalHspLimit(int l)
{
    m_HitSaveOpts->total_hsp_limit = l;
}

inline bool
CBlastOption::GetCullingMode() const
{
    return m_HitSaveOpts->perform_culling ? true : false;
}

inline void
CBlastOption::SetCullingMode(bool m)
{
    m_HitSaveOpts->perform_culling = m;
}

inline int
CBlastOption::GetRequiredStart() const
{
    return m_HitSaveOpts->required_start;
}

inline void
CBlastOption::SetRequiredStart(int s)
{
    m_HitSaveOpts->required_start = s;
}

inline int
CBlastOption::GetRequiredEnd() const
{
    return m_HitSaveOpts->required_end;
}

inline void
CBlastOption::SetRequiredEnd(int e)
{
    m_HitSaveOpts->required_end = e;
}

inline double
CBlastOption::GetEvalueThreshold() const
{
    return m_HitSaveOpts->expect_value;
}

inline void
CBlastOption::SetEvalueThrehold(double eval)
{
    m_HitSaveOpts->expect_value = eval;
}

inline double
CBlastOption::GetOriginalEvalue() const
{
    return m_HitSaveOpts->original_expect_value;
}

#if 0
void
CBlastOption::SetOriginalEvalue(double e)
{
    m_HitSaveOpts->original_expect_value = e;
}
#endif

inline int
CBlastOption::GetCutoffScore() const
{
    return m_HitSaveOpts->cutoff_score;
}

inline void
CBlastOption::SetCutoffScore(int s)
{
    m_HitSaveOpts->cutoff_score = s;
}

inline double
CBlastOption::GetPercentIdentity() const
{
    return m_HitSaveOpts->percent_identity;
}

inline void
CBlastOption::SetPercentIdentity(double p)
{
    m_HitSaveOpts->percent_identity = p;
}

inline bool
CBlastOption::GetSumStatisticsMode() const
{
    return m_HitSaveOpts->do_sum_stats ? true : false;
}

inline void
CBlastOption::SetSumStatisticsMode(bool m)
{
    m_HitSaveOpts->do_sum_stats = m;
}

inline double
CBlastOption::GetSingleHSPEvalueThreshold() const
{
    return m_HitSaveOpts->single_hsp_evalue;
}

inline void
CBlastOption::SetSingleHSPEvalueThreshold(double e)
{
    m_HitSaveOpts->single_hsp_evalue = e;
}

inline int
CBlastOption::GetSingleHSPCutoffScore() const
{
    return m_HitSaveOpts->single_hsp_score;
}

inline void
CBlastOption::SetSingleHSPCutoffScore(int s)
{
    m_HitSaveOpts->single_hsp_score = s;
}

inline int
CBlastOption::GetLongestIntronLength() const
{
    return m_HitSaveOpts->longest_intron;
}

inline void
CBlastOption::SetLongestIntronLength(int l)
{
    m_HitSaveOpts->longest_intron = l;
}

inline bool
CBlastOption::GetGappedMode() const
{
    return m_HitSaveOpts->is_gapped ? true : false;
}

inline void
CBlastOption::SetGappedMode(bool m)
{
    m_HitSaveOpts->is_gapped = m;
    m_ScoringOpts->gapped_calculation = m;
}

inline bool
CBlastOption::GetNeighboringMode() const
{
    return m_HitSaveOpts->is_neighboring ? true : false;
}

inline void
CBlastOption::SetNeighboringMode(bool m)
{
    m_HitSaveOpts->is_neighboring = m;
}

/************************ Scoring options ************************/
inline int 
CBlastOption::GetMatchReward() const
{
    return m_ScoringOpts->reward;
}

inline void 
CBlastOption::SetMatchReward(int r)
{
    m_ScoringOpts->reward = r;
}

inline int 
CBlastOption::GetMismatchPenalty() const
{
    return m_ScoringOpts->penalty;
}

inline void 
CBlastOption::SetMismatchPenalty(int p)
{
    m_ScoringOpts->penalty = p;
}

inline int 
CBlastOption::GetGapOpeningPenalty() const
{
    return m_ScoringOpts->gap_open;
}

inline void 
CBlastOption::SetGapOpeningPenalty(int g)
{
    m_ScoringOpts->gap_open = g;
}

inline int 
CBlastOption::GetGapExtensionPenalty() const
{
    return m_ScoringOpts->gap_extend;
}

inline void 
CBlastOption::SetGapExtensionPenalty(int e)
{
    m_ScoringOpts->gap_extend = e;
}

inline int 
CBlastOption::GetFrameShiftPenalty() const
{
    return m_ScoringOpts->shift_pen;
}

inline void 
CBlastOption::SetFrameShiftPenalty(int p)
{
    m_ScoringOpts->shift_pen = p;
    if (p > 0)
        m_ScoringOpts->is_ooframe = TRUE;
}

inline int 
CBlastOption::GetDecline2AlignPenalty() const
{
    return m_ScoringOpts->decline_align;
}

inline void 
CBlastOption::SetDecline2AlignPenalty(int p)
{
    m_ScoringOpts->decline_align = p;
}

inline bool 
CBlastOption::GetOutOfFrameMode() const
{
    return m_ScoringOpts->is_ooframe ? true : false;
}

inline void 
CBlastOption::SetOutOfFrameMode(bool m)
{
#if 0
    if (m_Program != eBlastx && m == true)
        NCBI_THROW(CBlastException, eBadParameter, 
                "Out-of-Frame only allowed for blastx");
#endif
    m_ScoringOpts->is_ooframe = m;
}

/******************** Effective Length options *******************/
inline Int8 
CBlastOption::GetDbLength() const
{
    return m_EffLenOpts->db_length;
}

inline void 
CBlastOption::SetDbLength(Int8 l)
{
    m_EffLenOpts->db_length = l;
}

inline unsigned int 
CBlastOption::GetDbSeqNum() const
{
    return (unsigned int) m_EffLenOpts->dbseq_num;
}

inline void 
CBlastOption::SetDbSeqNum(unsigned int n)
{
    m_EffLenOpts->dbseq_num = (Int4) n;
}

inline Int8 
CBlastOption::GetEffectiveSearchSpace() const
{
    return m_EffLenOpts->searchsp_eff;
}
 
inline void 
CBlastOption::SetEffectiveSearchSpace(Int8 eff)
{
    m_EffLenOpts->searchsp_eff = eff;
}

inline bool 
CBlastOption::GetUseRealDbSize() const
{
    return m_EffLenOpts->use_real_db_size ? true : false;
}

inline void 
CBlastOption::SetUseRealDbSize(bool u)
{
    m_EffLenOpts->use_real_db_size = u;
}

inline int 
CBlastOption::GetDbGeneticCode() const
{
    return m_DbOpts->genetic_code;
}

inline void 
CBlastOption::SetDbGeneticCode(int gc)
{
    m_DbOpts->genetic_code = gc;
}

inline const unsigned char* 
CBlastOption::GetDbGeneticCodeStr() const
{
    return m_DbOpts->gen_code_string;
}

inline void 
CBlastOption::SetDbGeneticCodeStr(const unsigned char* gc_str)
{
    if (!gc_str)
        return;

    copy(gc_str, gc_str+GENCODE_STRLEN, m_DbOpts->gen_code_string);
}

END_SCOPE(blast)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.18  2003/08/19 22:11:16  dondosha
* Cosmetic changes
*
* Revision 1.17  2003/08/19 20:22:05  dondosha
* EProgram definition moved from CBlastOption clase to blast scope
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
