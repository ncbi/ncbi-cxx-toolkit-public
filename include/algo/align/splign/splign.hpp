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
#include <algo/align/nw/nw_spliced_aligner.hpp>
#include <algo/align/splign/splign_hit.hpp>

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

    typedef CSplicedAligner TAligner;

    CSplign(void);

    // setters and getters
    CRef<TAligner>&     SetAligner(void);
    CConstRef<TAligner> GetAligner(void) const;

    typedef CSplignSeqAccessor TSeqAccessor;
    CRef<TSeqAccessor>&      SetSeqAccessor(void);
    CConstRef<TSeqAccessor>  GetSeqAccessor(void) const;

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
    size_t GetNextModelId(void) const {
        return m_model_id + 1;
    }

    typedef vector<CHit> THits;

    void Run(THits* hits);
  
    // a segment can represent an exon or an unaligning piece of mRna (a gap)
    struct NCBI_XALGOALIGN_EXPORT SSegment {
        
    public:
        
        bool   m_exon; // false if gap
        double m_idty;
        size_t m_len;
        size_t m_box [4];
        string m_annot;   // short description like AG<exon>GT
        string m_details; // transcript for exons, '-' for gaps
        CNWAligner::TScore m_score;
        
        void ImproveFromLeft( const char* seq1, const char* seq2,
                              CConstRef<CSplicedAligner> aligner);
        void ImproveFromRight( const char* seq1, const char* seq2,
                               CConstRef<CSplicedAligner> aligner);
        
        void Update(CConstRef<CSplicedAligner> aligner); // recompute members
        const char* GetDonor(void) const;    // raw pointers to parts of annot
        const char* GetAcceptor(void) const; // or zero if less than 2 chars

        static bool s_IsConsensusSplice(const char* donor,
                                        const char* acceptor);
        
        // NetCache-related serialization
        typedef vector<char> TNetCacheBuffer;
        void ToBuffer   (TNetCacheBuffer* buf) const;
        void FromBuffer (const TNetCacheBuffer& buf);
    };
    
    typedef vector<SSegment> TSegments;

    // aligned compartment representation 
    struct NCBI_XALGOALIGN_EXPORT SAlignedCompartment {
        
        size_t           m_id;
        bool             m_error;
        string           m_msg;
        bool             m_QueryStrand, m_SubjStrand;
        TSegments        m_segments;
        
        SAlignedCompartment(void): m_id(0), m_error(true)
        {}
        
        SAlignedCompartment(size_t id, bool err, const char* msg):
            m_id(id), m_error(err), m_msg(msg)
        {}
        
        // return overall identity (including gaps)
        double GetIdentity(void) const;
        
        // save to / read from NetCache buffer
        typedef vector<char> TNetCacheBuffer;
        void ToBuffer   (TNetCacheBuffer* buf) const;
        void FromBuffer (const TNetCacheBuffer& buf);
    };
    
    typedef vector<SAlignedCompartment> TResults;
    
    const TResults& GetResult(void) const {
        return m_result;
    }

protected:

    // active ingredient :-)
    CRef<TAligner> m_aligner;

    // access to sequence data
    CRef<TSeqAccessor> m_sa;

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
    TSegments    m_segments;
  
    // all compartments
    size_t       m_model_id;
    TResults     m_result;

    SAlignedCompartment x_RunOnCompartment( THits* hits,
                                            size_t range_left,
                                            size_t range_right );
    void   x_Run(const char* seq1, const char* seq2);
    size_t x_TestPolyA(void);
    void   x_SetPattern(THits* hits);
    void   x_ProcessTermSegm(SSegment** term_segs, Uint1 side) const;
};


END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.21  2005/05/10 18:02:24  kapustin
 * + x_ProcessTermSegm()
 *
 * Revision 1.20  2005/03/28 17:38:05  jcherry
 * Added export specifiers for nested structs
 *
 * Revision 1.19  2005/01/26 21:32:31  kapustin
 * +CSplign::SSegment::s_IsConsensusSplice
 *
 * Revision 1.18  2004/12/16 23:03:47  kapustin
 * Fix #include
 *
 * Revision 1.17  2004/12/01 14:54:38  kapustin
 * typedef public std types
 *
 * Revision 1.16  2004/11/29 21:09:12  kapustin
 * Move out-of-class struct definitions back within CSplign for more 
 * compatibility
 *
 * Revision 1.15  2004/11/29 14:36:45  kapustin
 * CNWAligner::GetTranscript now returns TTranscript and direction can be 
 * specified. x_ScoreByTanscript renamed to ScoreFromTranscript with two 
 * additional parameters to specify starting coordinates.
 *
 * Revision 1.14  2004/06/29 20:48:18  kapustin
 * Use CRef to access CObject-derived members
 *
 * Revision 1.13  2004/06/23 18:55:34  kapustin
 * GetLastModel() --> GetNextModel()
 *
 * Revision 1.12  2004/06/21 17:45:05  kapustin
 * Add CSplign::GetLastModelId()
 *
 * Revision 1.11  2004/06/03 19:27:04  kapustin
 * Add CSplign::GetIdentity()
 *
 * Revision 1.10  2004/05/04 15:23:44  ucko
 * Split splign code out of xalgoalign into new xalgosplign.
 *
 * Revision 1.9  2004/05/03 15:22:18  johnson
 * added typedefs for public stl types
 *
 * Revision 1.8  2004/04/30 15:00:32  kapustin
 * Support ASN formatting
 *
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
