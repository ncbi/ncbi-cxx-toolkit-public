#ifndef ALGO_ALIGN_SPLIGN__HPP
#define ALGO_ALIGN_SPLIGN__HPP

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
*   CSplign class definition
*
*/

#include <corelib/ncbistd.hpp>
#include <algo/align/nw_spliced_aligner.hpp>
#include <algo/align/splign_hit.hpp>

BEGIN_NCBI_SCOPE


// Abstract base for splign sequence accessors
class NCBI_XALGOALIGN_EXPORT CSplignSeqAccessor: public CObject
{
public:

    // start and finish are zero-based;
    // must return full sequence when start == 0 and finish == kMax_UInt;
    // sequence characters are expected to be in upper case.

    virtual void Load(const string& id, vector<char> *seq,
                      size_t start, size_t finish) = 0;
};


class NCBI_XALGOALIGN_EXPORT CSplign: public CObject
{
public:

    CSplign(void);

    // setters and getters

    void   SetAligner( CRef<CSplicedAligner>& aligner);
    const  CRef<CSplicedAligner>& GetAligner(void) const;

    void   SetSeqAccessor(CRef<CSplignSeqAccessor>& sa);
    const  CRef<CSplignSeqAccessor>& GetSeqAccessor(void) const;

    void   SetEndGapDetection(bool on);
    bool   GetEndGapDetection(void) const;

    void   SetPolyaDetection(bool on);
    bool   GetPolyaDetection(void) const;

    void   SetStrand(bool strand);
    bool   GetStrand(void) const;

    void   SetMaxGenomicExtension(size_t ext);
    size_t GetMaxGenomicExtension(void) const;

    void   SetMinQueryCoverage(double cov);
    double GetMinQueryCoverage(void) const;

    void   SetCompartmentPenalty(double penalty);
    double GetCompartmentPenalty(void) const; 

    void   SetMinExonIdentity(double idty);
    double GetMinExonIdentity(void) const;

    void   SetStartModelId(size_t model_id) {
        m_model_id = model_id - 1;
    }

    void Run(vector<CHit>* hits);

    // a segment can represent an exon or an unaligning piece of mRna (a gap)
    struct SSegment {
        bool   m_exon; // false if gap
        double m_idty;
        size_t m_len;
        size_t m_box [4];
        string m_annot;   // short description like AG<exon>GT
        string m_details; // transcript for exons, '-' for gaps

        void ImproveFromLeft( const char* seq1, const char* seq2 );
        void ImproveFromRight( const char* seq1, const char* seq2 );
        void RestoreIdentity(void);
        const char* GetDonor(void) const;    // raw pointers to parts of annot
        const char* GetAcceptor(void) const; // or zero if less than 2 chars
    };

    // aligned compartment representation 
    struct SAlignedCompartment {

        SAlignedCompartment(void): m_id(0), m_error(true) {}
        SAlignedCompartment(size_t id, bool err, const char* msg):
            m_id(id), m_error(err), m_msg(msg) {}
    
        size_t           m_id;
        string           m_query, m_subj;
        vector<SSegment> m_segments;
        size_t           m_mrnasize;
        bool             m_QueryStrand, m_SubjStrand;
        size_t           m_qmin, m_smin, m_smax;
        bool             m_error;
        string           m_msg;
    };
  
    const vector<SAlignedCompartment>& GetResult(void) const {
        return m_result;
    }

protected:

    // active ingridient :-)
    CRef<CSplicedAligner> m_aligner;

    // access to sequence data
    CRef<CSplignSeqAccessor> m_sa;

    // alignment pattern
    vector<size_t> m_pattern;

    // min exon idty - others will be marked as gaps
    double m_minidty;

    // compartment penalty as a per cent of the query (mRna) length
    double m_compartment_penalty;

    // mandatory end gap detection flag
    bool m_endgaps;

    // min query hit coverage
    double m_min_query_coverage;

    // alignment map
    struct SAlnMapElem {
        size_t m_box [4];
        int    m_pattern_start, m_pattern_end;
    };
    vector<SAlnMapElem> m_alnmap;

    // query sequence
    vector<char> m_mrna;
    bool         m_strand;
    size_t       m_polya_start;
    bool         m_nopolya;

    // genomic sequence
    vector<char> m_genomic;

    // max space to look beyond end hits
    size_t       m_max_genomic_ext;

    // output per compartment
    vector<SSegment> m_segments;
  
    // all compartments
    size_t       m_model_id;
    vector<SAlignedCompartment> m_result;

    SAlignedCompartment x_RunOnCompartment( vector<CHit>* hits,
                                            size_t range_left,
                                            size_t range_right );
    void   x_Run(const char* seq1, const char* seq2);
    size_t x_TestPolyA(void);
    void   x_SetPattern(vector<CHit>* hits);
};

END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.7  2004/04/27 17:19:43  kapustin
 * Valuble comments added
 *
 * Revision 1.6  2004/04/26 15:38:25  kapustin
 * Add model_id as a class member
 *
 * Revision 1.5  2004/04/23 14:36:24  kapustin
 * Initial revision
 *
 *
 * ===========================================================================
 */

#endif
