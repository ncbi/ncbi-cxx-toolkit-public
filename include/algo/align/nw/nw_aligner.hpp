#ifndef ALGO___NW_ALIGNER__HPP
#define ALGO___NW_ALIGNER__HPP

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

#include <vector>
#include <string>


/** @addtogroup AlgoAlignRoot
 *
 * @{
 */


BEGIN_NCBI_SCOPE


// Needleman Wunsch algorithm encapsulation
//

class NCBI_XALGOALIGN_EXPORT CNWAligner: public CObject
{
public:
    typedef int TScore;

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

    void SetWm  (TScore value)  { m_Wm  = value; }   // match (na)
    void SetWms (TScore value)  { m_Wms = value; }   // mismatch (na)
    void SetWg  (TScore value)  { m_Wg  = value; }   // gap opening
    void SetWs  (TScore value)  { m_Ws  = value; }   // gap extension

    // specify whether end gaps should be penalized
    void SetEndSpaceFree(bool Left1, bool Right1, bool Left2, bool Right2);

    // alignment pattern (guides)
    void SetPattern(const vector<size_t>& pattern);

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

    TScore        GetScore(void) const;
    
    // alignment transcript
    enum ETranscriptSymbol {
        eTS_None    =  0,   
        eTS_Delete  = 'D',
        eTS_Insert  = 'I',
        eTS_Match   = 'M',
        eTS_Replace = 'R',
        eTS_Intron  = 'Z'
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

protected:

    // Bonuses and penalties
    TScore   m_Wm;   // match bonus (eNucl)
    TScore   m_Wms;  // mismatch penalty (eNucl)
    TScore   m_Wg;   // gap opening penalty
    TScore   m_Ws;   // gap extension penalty

    // end-space free flags
    bool     m_esf_L1, m_esf_R1, m_esf_L2, m_esf_R2;

    // alphabet and score matrix
    const char*               m_abc;
    SNCBIFullScoreMatrix      m_ScoreMatrix;

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

    // Transcript, score and guiding hits
    TTranscript               m_Transcript;
    bool                      m_PositivesAsMatches;
    TScore                    m_score;
    vector<size_t>            m_guides;

    // multiple threads flag
    bool                      m_mt;
    size_t                    m_maxthreads;
 
    // facilitate guide pre- and  post-processing, if applicable
    virtual TScore x_Run   (void);

    // core dynamic programming
    struct SAlignInOut;
    virtual TScore x_Align (SAlignInOut* data);

    // overflow safe "infinity"
    enum { kInfMinus = kMin_Int / 2 };

    // backtrace
    void x_DoBackTrace(const unsigned char* backtrace_matrix,
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

    static bool PSpace(const SAlignInOut* p1, const SAlignInOut* p2) {
        return p1->m_space >= p2->m_space;
    }

private:    

    size_t m_space; // required dynprog dimension
};


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.41  2005/04/04 16:32:23  kapustin
 * Distinguish matches from mismatches in raw transcripts
 *
 * Revision 1.40  2005/03/16 15:48:26  jcherry
 * Allow use of std::string for specifying sequences
 *
 * Revision 1.39  2005/02/23 16:57:24  kapustin
 * +SetTranscript
 *
 * Revision 1.38  2004/12/06 22:11:24  kapustin
 * File header update
 *
 * Revision 1.37  2004/11/29 14:36:45  kapustin
 * CNWAligner::GetTranscript now returns TTranscript and direction can be specified. x_ScoreByTanscript renamed to ScoreFromTranscript with two additional parameters to specify starting coordinates.
 *
 * Revision 1.36  2004/08/31 20:37:29  papadopo
 * fix cut-and-paste-error
 *
 * Revision 1.35  2004/08/31 16:19:03  papadopo
 * 1. Make 'Run' method virtual
 * 2. Make SAlignInOut use sequence offsets instead of char pointers
 *
 * Revision 1.34  2004/06/29 20:46:04  kapustin
 * Support simultaneous segment computing
 *
 * Revision 1.33  2004/05/17 14:50:46  kapustin
 * Add/remove/rearrange some includes and object declarations
 *
 * Revision 1.32  2003/12/29 13:02:39  kapustin
 * Make x_GetElemSize() public and rename.
 * Return string from GetTranscriptString().
 *
 * Revision 1.31  2003/10/27 20:46:21  kapustin
 * Derive from CObject. Add more getters.
 *
 * Revision 1.30  2003/10/14 18:43:37  kapustin
 * Run() is not iteslf polymorphic
 *
 * Revision 1.29  2003/09/30 19:49:32  kapustin
 * Make use of standard score matrix interface
 *
 * Revision 1.28  2003/09/26 14:43:01  kapustin
 * Remove exception specifications
 *
 * Revision 1.27  2003/09/10 20:12:47  kapustin
 * Update Doxygen tags
 *
 * Revision 1.26  2003/09/02 22:30:34  kapustin
 * Move formatting functionality out of the class
 *
 * Revision 1.24  2003/06/26 20:39:53  kapustin
 * Rename formal parameters in SetEndSpaceFree() to avoid conflict with
 * macro under some configurations
 *
 * Revision 1.23  2003/06/17 17:20:28  kapustin
 * CNWAlignerException -> CAlgoAlignException
 *
 * Revision 1.22  2003/06/17 14:49:38  dicuccio
 * Fix-up after move to algo/align
 *
 * Revision 1.21  2003/06/02 14:04:25  kapustin
 * Progress indication-related updates
 *
 * Revision 1.20  2003/05/23 18:23:40  kapustin
 * Introduce a generic splice type. Make transcript symbol to be more
 * specific about type of the intron.
 *
 * Revision 1.19  2003/04/14 18:58:19  kapustin
 * x_Run() -> x_Align()
 *
 * Revision 1.18  2003/04/10 19:14:04  kapustin
 * Introduce guiding hits approach
 *
 * Revision 1.17  2003/04/10 19:04:30  siyan
 * Added doxygen support
 *
 * Revision 1.16  2003/04/02 20:52:24  kapustin
 * Make FormatAsText virtual. Pass output string as a parameter.
 *
 * Revision 1.15  2003/03/31 15:31:47  kapustin
 * Calculate score independently from transcript
 *
 * Revision 1.14  2003/03/18 15:12:29  kapustin
 * Declare virtual mem limit checking function. Allow separate specification
 * of free end gaps
 *
 * Revision 1.13  2003/03/12 21:11:03  kapustin
 * Add text buffer to progress callback info structure
 *
 * Revision 1.12  2003/03/05 20:12:22  kapustin
 * Simplify FormatAsText interface
 *
 * Revision 1.11  2003/02/26 21:30:32  gouriano
 * modify C++ exceptions thrown by this library
 *
 * Revision 1.10  2003/02/21 16:41:11  dicuccio
 * Added Win32 export specifier
 *
 * Revision 1.9  2003/02/11 16:06:13  kapustin
 * Add end-space free alignment support
 *
 * Revision 1.8  2003/02/04 23:04:38  kapustin
 * Add progress callback support
 *
 * Revision 1.7  2003/01/30 20:32:51  kapustin
 * Add EstiamteRunningTime()
 *
 * Revision 1.6  2003/01/28 12:36:52  kapustin
 * Format() --> FormatAsText(). Add FormatAsSeqAlign() and
 * support for sequence ids
 *
 * Revision 1.5  2003/01/24 16:48:36  kapustin
 * Support more output formats - type 2 and gapped FastA
 *
 * Revision 1.4  2003/01/21 12:36:56  kapustin
 * Specify negative infinity value
 *
 * Revision 1.3  2002/12/12 17:55:00  kapustin
 * Add support for spliced alignment
 *
 * Revision 1.2  2002/12/09 15:44:40  kapustin
 * exception forward declaration removed
 *
 * Revision 1.1  2002/12/06 17:40:12  ivanov
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* ALGO___NW_ALIGNER__HPP */
