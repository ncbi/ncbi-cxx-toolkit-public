#ifndef ALGO_ALIGN_NW_NW_ALIGNER__HPP
#define ALGO_ALIGN_NW_NW_ALIGNER__HPP

/* $Id$
* ===========================================================================
*
*                            public DOMAIN NOTICE                          
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
* Author:  Yuri Kapustin, Alexander Souvorov
*
* File Description:
*   CNWAligner class definition
*
*   CNWAligner encapsulates a generic global (Needleman-Wunsch)
*   alignment algorithm with affine gap penalty model.
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbi_limits.hpp>
#include <util/tables/raw_scoremat.h>
#include <objects/seqloc/Na_strand.hpp>

#include <vector>
#include <string>


/** @addtogroup AlgoAlignRoot
 *
 * @{
 */


BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CDense_seg;
    class CSeq_id;
END_SCOPE(objects)

// Needleman-Wunsch algorithm encapsulation
//

class NCBI_XALGOALIGN_EXPORT CNWAligner: public CObject
{
public:
    typedef int TScore;

    enum EGapPreference { eEarlier, eLater };

    // ctors
    CNWAligner(void);

    // Null scoremat pointer indicates IUPACna coding
    CNWAligner(const char* seq1, size_t len1,
               const char* seq2, size_t len2,
               const SNCBIPackedScoreMatrix* scoremat = 0);

    CNWAligner(const string& seq1,
               const string& seq2,
               const SNCBIPackedScoreMatrix* scoremat = 0);

    virtual ~CNWAligner(void) {}

    // Compute the alignment
    virtual TScore Run(void);

    // Setters
    virtual void SetSequences(const char* seq1, size_t len1,
                              const char* seq2, size_t len2,
                              bool verify = true);

    void SetSequences(const string& seq1,
                      const string& seq2,
                      bool verify = true);
  
    void SetScoreMatrix(const SNCBIPackedScoreMatrix* scoremat);

    void SetWm  (TScore value);                      // match (na)
    void SetWms (TScore value);                      // mismatch (na)
    void SetWg  (TScore value)  { m_Wg  = value; }   // gap opening
    void SetWs  (TScore value)  { m_Ws  = value; }   // gap extension

    // specify whether end gaps should be penalized
    //'true' - do not penalize, 'false' - penalize
    void SetEndSpaceFree(bool Left1, bool Right1, bool Left2, bool Right2);

    void SetSmithWaterman(bool SW);

    /// Control preference for where to place a gap if there is a choice;
    /// default is eEarlier, placing the gap as early as possible
    void SetGapPreference(EGapPreference p);

    // alignment pattern (guides)
    void SetPattern(const vector<size_t>& pattern);

    // max memory to use
    void SetSpaceLimit(const size_t& maxmem) { m_MaxMem = maxmem; }

    // progress reporting
    struct SProgressInfo
    {
        SProgressInfo(void): m_iter_done(0), m_iter_total(0), m_data(0) {}
        size_t m_iter_done;
        size_t m_iter_total;
        void*  m_data;
        char   m_text_buffer [1024];
    };

    // return true to cancel calculation
    typedef bool (*FProgressCallback) (SProgressInfo*);
    void SetProgressCallback ( FProgressCallback prg_callback, void* data );

    // Getters
    static TScore GetDefaultWm  (void) { return  1; }
    static TScore GetDefaultWms (void) { return -2; }
    static TScore GetDefaultWg  (void) { return -5; }
    static TScore GetDefaultWs  (void) { return -2; }

    TScore GetWm  (void) const { return m_Wm; }
    TScore GetWms (void) const { return m_Wms; }
    TScore GetWg  (void) const { return m_Wg; }
    TScore GetWs  (void) const { return m_Ws; }

    const char*   GetSeq1(void) const { return m_Seq1; }
    size_t        GetSeqLen1(void) const { return m_SeqLen1; }
    const char*   GetSeq2(void) const { return m_Seq2; }
    size_t        GetSeqLen2(void) const { return m_SeqLen2; }

    void          GetEndSpaceFree(bool* L1, bool* R1, bool* L2, bool* R2)
                      const;

    bool          IsSmithWatermen() const;

    EGapPreference GetGapPreference() const;

    TScore        GetScore(void) const;

    size_t        GetSpaceLimit(void) const {  return m_MaxMem; }
    static size_t GetDefaultSpaceLimit(void) {
        return 0xFFFFFFFF;
    }
    
    // alignment transcript
    enum ETranscriptSymbol {
        eTS_None         = 0   
        ,eTS_Delete       = 'D'
        ,eTS_Insert       = 'I'
        ,eTS_Match        = 'M'
        ,eTS_Replace      = 'R'
        ,eTS_Intron       = 'Z'
        ,eTS_SlackDelete // unaligned s-w term
        ,eTS_SlackInsert // -- " -- 
    };
    typedef vector<ETranscriptSymbol> TTranscript;

    // raw transcript
    TTranscript   GetTranscript(bool reversed = true) const;
    void          SetTranscript(const TTranscript& transcript);

    // transcript as a string
    string GetTranscriptString(void) const;

    // if set, all positively scoring diags will be
    // recorded as matches in the alignment transcript;
    // only real matches otherwise.
    void  SetPositivesAsMatches(bool positives_as_matches = true) {
        m_PositivesAsMatches = positives_as_matches;
    }
    bool  GetPositivesAsMatches(void) const {
        return m_PositivesAsMatches;
    }

    // transcript parsers
    size_t         GetLeftSeg(size_t* q0, size_t* q1,
                              size_t* s0, size_t* s1,
                              size_t min_size) const;
    size_t         GetRightSeg(size_t* q0, size_t* q1,
                               size_t* s0, size_t* s1,
                               size_t min_size) const;
    size_t         GetLongestSeg(size_t* q0, size_t* q1,
                                 size_t* s0, size_t* s1) const;
 
    // returns the size of a single backtrace matrix element
    virtual size_t GetElemSize(void) const {
        return 1;
    }

    // Compute score with the given transcript and sequences offsets.
    // if defaults are supplied for start1 and start2,
    // compute score using transcript only assuming nucleotide alignment.
    virtual TScore ScoreFromTranscript(const TTranscript& transcript,
                                       size_t start1 = kMax_UInt,
                                       size_t start2 = kMax_UInt ) const;

    void    EnableMultipleThreads(bool enable = true);

    // A naive pattern generator-use cautiously.
    // Do not use on sequences with repeats or error.
    size_t MakePattern(const size_t hit_size = 100, 
                       const size_t core_size = 28);

    // Create a Dense-seg representing the alignment, without ids set
    CRef<objects::CDense_seg> GetDense_seg(TSeqPos query_start,
                                           objects::ENa_strand query_strand,
                                           TSeqPos subj_start,
                                           objects::ENa_strand subj_strand)
                                           const;

    // Create a Dense-seg representing the alignment, with provided ids set
    CRef<objects::CDense_seg> GetDense_seg(TSeqPos query_start,
                                           objects::ENa_strand query_strand,
                                           const objects::CSeq_id& query_id,
                                           TSeqPos subj_start,
                                           objects::ENa_strand subj_strand,
                                           const objects::CSeq_id& subj_id)
                                           const;

protected:

    // Bonuses and penalties
    TScore   m_Wm;   // match bonus (eNucl)
    TScore   m_Wms;  // mismatch penalty (eNucl)
    TScore   m_Wg;   // gap opening penalty
    TScore   m_Ws;   // gap extension penalty

    // end-space free flags
    bool     m_esf_L1, m_esf_R1, m_esf_L2, m_esf_R2;
    bool     m_SmithWaterman;

    EGapPreference m_GapPreference;

    // alphabet and score matrix
    const char*               m_abc;
    SNCBIFullScoreMatrix      m_ScoreMatrix;
    bool                      m_ScoreMatrixInvalid;

    // progress callback
    FProgressCallback         m_prg_callback;

    // progress status
    mutable SProgressInfo     m_prg_info;

    // termination flag
    mutable  bool             m_terminate;

    // Source sequences
    const char*               m_Seq1;
    size_t                    m_SeqLen1;
    const char*               m_Seq2;
    size_t                    m_SeqLen2;
    size_t x_CheckSequence(const char* seq, size_t len) const;
    virtual bool x_CheckMemoryLimit(void);

    // naive pattern generation helpers (Rabin-Karp approach)
    unsigned char   x_CalcFingerPrint64( const char* beg,
                                         const char* end,
                                         size_t& err_index );
    const char*     x_FindFingerPrint64( const char* beg, 
                                         const char* end,
                                         unsigned char fingerprint,
                                         size_t size,
                                         size_t& err_index );

    // Transcript, score and guiding hits
    TTranscript               m_Transcript;
    bool                      m_PositivesAsMatches;
    TScore                    m_score;
    vector<size_t>            m_guides;

    // multiple threads flag
    bool                      m_mt;
    size_t                    m_maxthreads;

    // approximate max space to use
    size_t                   m_MaxMem;
 
    // facilitate guide pre- and  post-processing, if applicable
    virtual TScore x_Run   (void);

    // core dynamic programming
    struct SAlignInOut;
    virtual TScore x_Align (SAlignInOut* data);

    // a helper class assuming four bits per backtrace matrix cell
    class CBacktraceMatrix4 {
    public:

        CBacktraceMatrix4(size_t dim) {
            m_Buf = new Uint1 [dim / 2 + 1];
            m_Elem = 0;
            m_BestPos = 0;
        }

        ~CBacktraceMatrix4() { delete [] m_Buf; }

        void SetAt(size_t i, Uint1 v) {
            if(i & 1) {
                m_Buf[i >> 1] = m_Elem | (v << 4);
            }
            else {
                m_Elem = v;
            }
        }

        void SetBestPos(size_t k) {
            m_BestPos = k;
        }

        size_t BestPos() const {
            return m_BestPos;
        }

        void Purge(size_t i) {
            if(i & 1) {
                m_Buf[i >> 1] = m_Elem;
            }
        }

        Uint1 operator[] (size_t i) const {
            return 0x0F & ((m_Buf[i >> 1]) >> ((i & 1) << 2));
        }

    private:
        
        Uint1 * m_Buf;
        Uint1   m_Elem;
        size_t  m_BestPos;
    };

    void x_DoBackTrace(const CBacktraceMatrix4 & backtrace,
                       SAlignInOut* data);

    // retrieve transcript symbol for a one-character diag
    virtual ETranscriptSymbol x_GetDiagTS(size_t i1, size_t i2) const;

    friend class CNWAlignerThread_Align;
};


struct CNWAligner::SAlignInOut {

    SAlignInOut(): m_offset1(0), m_len1(0), 
                   m_offset2(0), m_len2(0), 
                   m_space(0) {}

    SAlignInOut(size_t offset1, size_t len1, bool esfL1, bool esfR1,
                size_t offset2, size_t len2, bool esfL2, bool esfR2):
        m_offset1(offset1), m_len1(len1), m_esf_L1(esfL1), m_esf_R1(esfR1),
        m_offset2(offset2), m_len2(len2), m_esf_L2(esfL2), m_esf_R2(esfR2)
    {
        m_space = m_len1*m_len2;
    }

    // [in] first sequence
    size_t      m_offset1;
    size_t      m_len1;
    bool        m_esf_L1, m_esf_R1;

    // [in] second sequence
    size_t      m_offset2;
    size_t      m_len2;
    bool        m_esf_L2, m_esf_R2;

    // [out]
    TTranscript m_transcript;

    size_t      GetSpace(void) const {
        return m_space;
    }

    void        FillEdgeGaps(size_t len) {
        m_transcript.insert(m_transcript.end(), len % (m_len2+1), eTS_Insert);
        m_transcript.insert(m_transcript.end(), len / (m_len2+1), eTS_Delete);
    }

    static bool PSpace(const SAlignInOut* p1, const SAlignInOut* p2) {
        return p1->m_space >= p2->m_space;
    }

private:    

    size_t m_space; // required dynprog dimension
};


namespace {

    const char g_nwaligner_nucleotides [] = "AGTCBDHKMNRSVWY";

    const CNWAligner::TScore kInfMinus = -(numeric_limits<CNWAligner::TScore>::
                                           max() / 2);
}

END_NCBI_SCOPE


/* @} */

#endif  /* ALGO___NW_ALIGNER__HPP */
