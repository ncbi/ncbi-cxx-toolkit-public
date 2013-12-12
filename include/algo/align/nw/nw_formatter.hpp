#ifndef ALGO_ALIGN__NW_FORMAT__HPP
#define ALGO_ALIGN__NW_FORMAT__HPP

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
* Author:  Yuri Kapustin
*
* File Description:
*   Library's formatting functionality.
*/

/** @addtogroup AlgoAlignFormat
 *
 * @{
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <algo/align/nw/nw_spliced_aligner.hpp>

#include <deque>

BEGIN_NCBI_SCOPE


BEGIN_SCOPE(objects)
    class CSeq_align;
END_SCOPE(objects)


class NCBI_XALGOALIGN_EXPORT CNWFormatter: public CObject
{
public:

    CNWFormatter(const CNWAligner& aligner);
    
    // supported text formats
    enum ETextFormatType {
        eFormatType1,
        eFormatType2,
        eFormatAsn,
        eFormatFastA,
        eFormatExonTable,  // spliced alignments
        eFormatExonTableEx //
    };

    // seq-align format flags
    enum ESeqAlignFormatFlags {
        eSAFF_None = 0,
        eSAFF_DynProgScore = 1,
        eSAFF_Identity = 2
    };

    // setters

    void SetSeqIds(CConstRef<objects::CSeq_id> id1, 
                   CConstRef<objects::CSeq_id> id2);

    // formatters

    void AsText(string* output, ETextFormatType type,
                size_t line_width = 100) const;

    CRef<objects::CSeq_align> AsSeqAlign (
        TSeqPos query_start, objects::ENa_strand query_strand,
        TSeqPos subj_start,  objects::ENa_strand subj_strand,
        ESeqAlignFormatFlags flags = eSAFF_None) const;


    // SSegment is a structural unit of a spliced alignment. It represents
    // either an exon or an unaligned segment.
    struct NCBI_XALGOALIGN_EXPORT SSegment {
        
    public:

        bool   m_exon;    // true == exon; false == unaligned
        double m_idty;    // ranges from 0.0 to 1.0
        size_t m_len;     // lenths of the alignment, not of an interval
        size_t m_box [4]; // query([0],[1]) and subj([2],[3]) coordinates
        string m_annot;   // text description like AG<exon>GT
        string m_details; // transcript for exons, '-' for gaps

        float  m_score;   // dynprog score (normalized)
        

        //old style:

        void ImproveFromLeft(const char* seq1, const char* seq2,
                             CConstRef<CSplicedAligner> aligner);
        void ImproveFromRight(const char* seq1, const char* seq2,
                              CConstRef<CSplicedAligner> aligner);
                    
        //trimming, new style:
        void ImproveFromLeft1(const char* seq1, const char* seq2,
                             CConstRef<CSplicedAligner> aligner);
        void ImproveFromRight1(const char* seq1, const char* seq2,
                              CConstRef<CSplicedAligner> aligner);

        size_t GapLength(); //count total gap length                              
        bool IsLowComplexityExon(const char *rna_seq);
        
        //check if 100% extension is possible, returns the length of possible extension
        int CanExtendRight(const vector<char>& mrna, const vector<char>& genomic) const;
        int CanExtendLeft(const vector<char>& mrna, const vector<char>& genomic) const;

        //do extend, 100% identity in extension is implied
        void ExtendRight(const vector<char>& mrna, const vector<char>& genomic, int ext_len, const CNWAligner* aligner);
        void ExtendLeft(const vector<char>& mrna, const vector<char>& genomic, int ext_len, const CNWAligner* aligner);

        void Update(const CNWAligner* aligner); // recompute members
        const char* GetDonor(void) const;       // raw pointers to parts of annot
        const char* GetAcceptor(void) const;    // or zero if less than 2 chars
        void SetToGap();//set segment to a gap

        static bool s_IsConsensusSplice(const char* donor, const char* acceptor,
                                        bool semi_as_cons = false);
        
        // NetCache-related serialization
        typedef vector<char> TNetCacheBuffer;
        void ToBuffer   (TNetCacheBuffer* buf) const;
        void FromBuffer (const TNetCacheBuffer& buf);
    };

    // partition a spliced alignment into SSegment's
    void MakeSegments(deque<SSegment>* psegments) const;

private:

    const CNWAligner*                 m_aligner;
    CConstRef<objects::CSeq_id>       m_Seq1Id, m_Seq2Id;

    size_t x_ApplyTranscript(vector<char>* seq1_transformed,
                             vector<char>* seq2_transformed) const;    
};


END_NCBI_SCOPE

/* @} */

#endif  /* ALGO_ALIGN__NW_FORMAT__HPP */
