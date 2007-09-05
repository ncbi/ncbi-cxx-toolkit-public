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

#include <objmgr/scope.hpp>
#include <algo/align/nw/nw_formatter.hpp>
#include <algo/align/util/blast_tabular.hpp>


BEGIN_NCBI_SCOPE

class CBlastTabular;

BEGIN_SCOPE(objects)
    class CScope;
    class CSeq_id;
END_SCOPE(objects)


class NCBI_XALGOALIGN_EXPORT CSplign: public CObject
{
public:

    typedef CSplicedAligner TAligner;

    CSplign(void);
    ~CSplign();

    // setters and getters
    CRef<TAligner>&     SetAligner(void);
    CConstRef<TAligner> GetAligner(void) const;

    CRef<objects::CScope>  GetScope(void) const;
    CRef<objects::CScope>& SetScope(void);
    void   PreserveScope(bool preserve_scope = true);

    void   SetEndGapDetection(bool on);
    bool   GetEndGapDetection(void) const;

    void   SetPolyaDetection(bool on);
    bool   GetPolyaDetection(void) const;

    void   SetStrand(bool strand);
    bool   GetStrand(void) const;

    void   SetMaxGenomicExtent(size_t mge);
    static size_t s_GetDefaultMaxGenomicExtent(void);
    size_t GetMaxGenomicExtent(void) const;

    void   SetCompartmentPenalty(double penalty);
    static double s_GetDefaultCompartmentPenalty(void);
    double GetCompartmentPenalty(void) const;

    void   SetMinCompartmentIdentity(double idty);
    static double s_GetDefaultMinCompartmentIdty(void);
    double GetMinCompartmentIdentity(void) const;

    void   SetMinSingletonIdentity(double idty);
    double GetMinSingletonIdentity(void) const;

    void   SetMinExonIdentity(double idty);
    static double s_GetDefaultMinExonIdty(void);
    double GetMinExonIdentity(void) const;

    void   SetStartModelId(size_t model_id) {
        m_model_id = model_id - 1;
    }
    size_t GetNextModelId(void) const {
        return m_model_id + 1;
    }

    void SetMaxCompsPerQuery(size_t m);
    size_t GetMaxCompsPerQuery(void) const;
    
    typedef CNWFormatter::SSegment   TSegment;
    typedef vector<TSegment>         TSegments;

    // aligned compartment representation 
    struct NCBI_XALGOALIGN_EXPORT SAlignedCompartment {
        
        size_t           m_id;
        bool             m_error;
        string           m_msg;
        bool             m_QueryStrand, m_SubjStrand;
        size_t           m_cds_start, m_cds_stop;
        size_t           m_QueryLen;
        size_t           m_PolyA;
        TSegments        m_segments;
        
        SAlignedCompartment(void): m_id(0), m_error(true), 
                                   m_cds_start(0), m_cds_stop(0),
                                   m_QueryLen (0),
                                   m_PolyA(0)
        {}
        
        SAlignedCompartment(size_t id, bool err, const char* msg):
            m_id(id), m_error(err), m_msg(msg),
            m_cds_start(0), m_cds_stop(0),
            m_QueryLen(0),
            m_PolyA(0)
        {}
        
        // return overall identity (including gaps)
        double GetIdentity(void) const;

        // get aligned min/max on query and subject
        void GetBox(Uint4* box) const;
        
        // save to / read from NetCache buffer
        typedef vector<char> TNetCacheBuffer;
        void ToBuffer   (TNetCacheBuffer* buf) const;
        void FromBuffer (const TNetCacheBuffer& buf);
    };
    
    typedef CBlastTabular           THit;
    typedef CRef<THit>              THitRef;
    typedef vector<THitRef>         THitRefs;

    // identify compartments and align each of them
    void Run(THitRefs* hitrefs);
    typedef vector<SAlignedCompartment> TResults;
    
    // retrieve results computed with Run()
    const TResults& GetResult(void) const {
        return m_result;
    }

    // align as a single compartment within given genomic bounds
    bool AlignSingleCompartment(THitRefs* hitrefs,
                                size_t range_left, size_t range_right,
                                SAlignedCompartment* result);

    // clear sequence vectors and scope - use with caution
    void ClearMem(void);

    typedef pair<size_t,size_t>   TOrf;
    typedef pair<TOrf,TOrf>       TOrfPair;
    TOrfPair GetCds(const THit::TId & id, const vector<char> * seq_data = 0);

protected:

    // active ingredient :-)
    CRef<TAligner> m_aligner;

    // access to sequence data
    CRef<objects::CScope> m_Scope;
    bool                  m_CanResetHistory;

    // alignment pattern
    vector<size_t> m_pattern;

    // min exon idty - others will be marked as gaps
    double m_MinExonIdty;

    // compartment penalty as a per cent of the query (mRna) length
    double m_compartment_penalty;

    // min compartment idty - others will be skipped
    double m_MinCompartmentIdty;

    // min single compartment idty (single per subject per strand)
    double m_MinSingletonIdty;

    // mandatory end gap detection flag
    bool m_endgaps;

    // alignment map
    struct SAlnMapElem {
        size_t m_box [4];
        int    m_pattern_start, m_pattern_end;
    };
    vector<SAlnMapElem> m_alnmap;

    typedef map<string,TOrfPair>  TStrIdToOrfs;
    TStrIdToOrfs  m_OrfMap;

    // query sequence
    vector<char> m_mrna;
    bool         m_strand;
    size_t       m_polya_start;
    bool         m_nopolya;

    size_t       m_cds_start; // in antisense, these are computed based on a reverse-
    size_t       m_cds_stop;  // complimentary sequence, so start still less than stop

    // genomic sequence
    vector<char> m_genomic;

    // max space to look beyond end hits
    size_t       m_max_genomic_ext;

    // The limiting range as defined by the compartment hits,
    // if the max compartment hit identity is less than a cut-off.
    pair<size_t, size_t> m_BoundingRange;

    // output per compartment
    TSegments    m_segments;
  
    // all compartments
    size_t       m_model_id;
    TResults     m_result;

    size_t       m_MaxCompsPerQuery;

    size_t       m_MinPatternHitLength;

    SAlignedCompartment x_RunOnCompartment( THitRefs* hitrefs,
                                            size_t range_left,
                                            size_t range_right);

    void   x_Run(const char* seq1, const char* seq2);
    size_t x_TestPolyA(void);
    void   x_SetPattern(THitRefs* hitrefs);
    bool   x_ProcessTermSegm(TSegment** term_segs, Uint1 side) const;
    Uint4  x_GetGenomicExtent(const Uint4 query_extent, Uint4 max_ext = 0) const;

    void   x_LoadSequence(vector<char>* seq, 
                          const objects::CSeq_id& seqid,
                          THit::TCoord start,
                          THit::TCoord finish,
                          bool retain);

    /// forbidden
    CSplign(const CSplign&);
    CSplign& operator=(const CSplign&);
};


END_NCBI_SCOPE


#endif
