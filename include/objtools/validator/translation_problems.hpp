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
 *`
 * Author:  Colleen Bollin
 *
 * File Description:
 *   For detecting translation problems
 *   .......
 *
 */

#ifndef VALIDATOR___TRANSLATION_PROBLEMS__HPP
#define VALIDATOR___TRANSLATION_PROBLEMS__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_autoinit.hpp>

#include <objmgr/scope.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_feat;
class CBioseq_Handle;

BEGIN_SCOPE(validator)

typedef Char(&TSpliceSite)[2];

// =============================  Validate SeqFeat  ============================


class NCBI_VALIDATOR_EXPORT CCDSTranslationProblems {
public:
    CCDSTranslationProblems();
    ~CCDSTranslationProblems() {};

    void CalculateTranslationProblems
        (const CSeq_feat& feat,
        CBioseq_Handle loc_handle,
        CBioseq_Handle prot_handle,
        bool ignore_exceptions,
        bool far_fetch_cds,
        bool standalone_annot,
        bool single_seq,
        bool is_gpipe,
        bool is_genomic,
        bool is_refseq,
        bool is_nt_or_ng_or_nw,
        bool is_nc,
        bool has_accession,
        CScope* scope);

    typedef enum {
        eCDSTranslationProblem_FrameNotPartial = 1,
        eCDSTranslationProblem_FrameNotConsensus = 2,
        eCDSTranslationProblem_NoStop = 4,
        eCDSTranslationProblem_StopPartial = 8,
        eCDSTranslationProblem_PastStop = 16,
        eCDSTranslationProblem_ShouldStartPartial = 32,
        eCDSTranslationProblem_Mismatches = 64,
        eCDSTranslationProblem_BadStart = 128,
        eCDSTranslationProblem_TooManyX = 256,
        eCDSTranslationProblem_UnableToFetch = 512,
        eCDSTranslationProblem_NoProtein = 1024,
        eCDSTranslationProblem_ShouldBePartialButIsnt = 2048,
        eCDSTranslationProblem_ShouldNotBePartialButIs = 4096,
        eCDSTranslationProblem_UnnecessaryException = 8192,
        eCDSTranslationProblem_UnqualifiedException = 16384,
        eCDSTranslationProblem_ErroneousException = 32768,

    } ECDSTranslationProblem;

    size_t GetTranslationProblemFlags() const { return m_ProblemFlags; }

    typedef enum {
        eTranslExceptPhase = 0,
        eTranslExceptSuspicious,
        eTranslExceptUnnecessary,
        eTranslExceptUnexpected
    } ETranslExceptType;
    typedef struct {
        ETranslExceptType problem;
        unsigned char ex;
        size_t prot_pos;
    } STranslExceptProblem;
    typedef vector<STranslExceptProblem> TTranslExceptProblems;

    const TTranslExceptProblems& GetTranslExceptProblems() const
    {
        return m_TranslExceptProblems;
    }

    typedef struct {
        unsigned char prot_res;
        unsigned char transl_res;
        TSeqPos pos;
    } STranslationMismatch;
    typedef vector<STranslationMismatch> TTranslationMismatches;

    const TTranslationMismatches& GetTranslationMismatches() const { return m_Mismatches; }

    bool HasException() const { return m_HasException; }
    bool UnableToTranslate() const { return m_UnableToTranslate; }
    bool HasUnparsedTranslExcept() const { return m_UnparsedTranslExcept; }
    size_t GetInternalStopCodons() const { return m_InternalStopCodons; }
    size_t GetNumNonsenseIntrons() const { return m_NumNonsenseIntrons; }
    bool AltStart() const { return m_AltStart; }
    char GetTranslStartCharacter() const { return m_TranslStart; }
    int GetRaggedLength() const { return m_RaggedLength; }
    size_t GetProtLen() const { return m_ProtLen; }
    size_t GetTransLen() const { return m_TransLen; }
    size_t GetTranslTerminalX() const { return m_TranslTerminalX; }
    size_t GetProdTerminalX() const { return m_ProdTerminalX; }

    static vector<CRef<CSeq_loc> > GetNonsenseIntrons(const CSeq_feat& feat, CScope& scope);

private:

    size_t m_ProblemFlags;
    int    m_RaggedLength;
    bool   m_HasDashXStart;
    size_t m_ProtLen;
    size_t m_TransLen;
    TTranslationMismatches m_Mismatches;
    char   m_TranslStart;
    size_t m_InternalStopCodons;
    bool   m_UnableToTranslate;
    size_t m_TranslTerminalX;
    size_t m_ProdTerminalX;
    bool   m_UnparsedTranslExcept;
    bool   m_AltStart;
    size_t m_NumNonsenseIntrons;
    TTranslExceptProblems m_TranslExceptProblems;
    bool   m_HasException;

    void x_Reset();

    TTranslExceptProblems x_GetTranslExceptProblems(const CSeq_feat& feat, CBioseq_Handle loc_handle, CScope* scope, bool is_refseq);

    static size_t x_CountNonsenseIntrons(const CSeq_feat& feat, CScope* scope);
    static bool x_ProteinHasTooManyXs(const string& transl_prot);

    static bool x_IsThreeBaseNonsense(const CSeq_feat& feat,
        const CSeq_id& id,
        const CCdregion& cdr,
        TSeqPos start,
        TSeqPos stop,
        ENa_strand strand,
        CScope *scope);

    static void x_GetExceptionFlags
        (const string& except_text,
        bool& unclassified_except,
        bool& mismatch_except,
        bool& frameshift_except,
        bool& rearrange_except,
        bool& product_replaced,
        bool& mixed_population,
        bool& low_quality,
        bool& rna_editing,
        bool& transcript_or_proteomic);

    static size_t x_CheckCDSFrame(const CSeq_feat& feat, CScope* scope);

    static bool x_Is5AtEndSpliceSiteOrGap(const CSeq_loc& loc, CScope& scope);

    static size_t x_CountTerminalXs(const string& transl_prot, bool skip_stop);
    static size_t x_CountTerminalXs(const CSeqVector& prot_vec);

    void x_GetCdTransErrors(const CSeq_feat& feat, CBioseq_Handle product, bool show_stop, bool got_stop, CScope* scope);

    static TTranslationMismatches x_GetTranslationMismatches(const CSeq_feat& feat,
        const CSeqVector& prot_vec,
        const string& transl_prot,
        bool has_accession);

    static bool x_JustifiesException(const TTranslExceptProblems& problems);
    bool x_JustifiesException() const;

    static int x_CheckForRaggedEnd(const CSeq_feat& feat, CScope* scope);
    static int x_CheckForRaggedEnd(const CSeq_loc&, const CCdregion& cdr, CScope* scope);
};


typedef enum {
    eMRNAProblem_TransFail = 1,
    eMRNAProblem_UnableToFetch = 2,
    eMRNAProblem_TranscriptLenLess = 4,
    eMRNAProblem_PolyATail100 = 8,
    eMRNAProblem_PolyATail95 = 16,
    eMRNAProblem_TranscriptLenMore = 32,
    eMRNAProblem_Mismatch = 64,
    eMRNAProblem_UnnecessaryException = 128,
    eMRNAProblem_ErroneousException = 256,
    eMRNAProblem_ProductReplaced = 512
} EMRNAProblem;

size_t GetMRNATranslationProblems
(const CSeq_feat& feat,
size_t& mismatches,
bool ignore_exceptions,
CBioseq_Handle nuc,
CBioseq_Handle rna,
bool far_fetch,
bool is_gpipe,
bool is_genomic,
CScope* scope);

END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* VALIDATOR___TRANSLATION_PROBLEMS__HPP */
