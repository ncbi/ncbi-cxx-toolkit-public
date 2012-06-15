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
#include <corelib/version.hpp>

#include <objmgr/scope.hpp>

#include <algo/align/nw/nw_formatter.hpp>
#include <algo/align/util/blast_tabular.hpp>


BEGIN_NCBI_SCOPE

class CBlastTabular;

BEGIN_SCOPE(objects)
    class CScope;
    class CSeq_id;
    class CScore_set;
    class CSeq_align_set;
END_SCOPE(objects)


/// CSplign is the central library object for computing spliced
/// cDNA-to-genomic alignments.


class NCBI_XALGOALIGN_EXPORT CSplign: public CObject
{
public:

    typedef CSplicedAligner TAligner;

    CSplign(void);
    ~CSplign();

    /// Retrieve the library's version object

    static CRef<CVersion> s_GetVersion(void);

    /// Access the spliced aligner core object.

    CRef<TAligner>&     SetAligner(void);
    CConstRef<TAligner> GetAligner(void) const;
    static CRef<CSplicedAligner> s_CreateDefaultAligner(bool low_query_quality);

    /// Access the scope object that the library will use to retrieve the sequences

    CRef<objects::CScope>  GetScope(void) const;
    CRef<objects::CScope>& SetScope(void);

    /// Controls whether to clean the scope object's cache on a new sequence.
    ///
    /// @param preserve
    ///    When true, the sequences previsouly loaded into the scope will not
    ///    be deleted, which is feasible when working with a fixed number
    ///    of sequences e.g. in an interactive application. When false,
    ///    transcript sequences will always be cleared from the scope, and
    ///    genomic sequences will be cleared unless the requested sequence
    ///    is the same as the last one.

    void   PreserveScope(bool preserve = true);

    void   SetEndGapDetection(bool on);
    bool   GetEndGapDetection(void) const;

    void   SetPolyaDetection(bool on);
    bool   GetPolyaDetection(void) const;

    void   SetStrand(bool strand);
    bool   GetStrand(void) const;

    void   SetMaxGenomicExtent(size_t mge);
    static size_t s_GetDefaultMaxGenomicExtent(void);
    size_t GetMaxGenomicExtent(void) const;

    void   SetMaxIntron(size_t max_intron);
    size_t GetMaxIntron(void) const;

    void   SetCompartmentPenalty(double penalty);
    static double s_GetDefaultCompartmentPenalty(void);
    double GetCompartmentPenalty(void) const;

    void   SetMinCompartmentIdentity(double idty);
    static double s_GetDefaultMinCompartmentIdty(void);
    double GetMinCompartmentIdentity(void) const;

    void   SetMinSingletonIdentity(double idty);
    double GetMinSingletonIdentity(void) const;

    void   SetMinSingletonIdentityBps(size_t idty);
    size_t GetMinSingletonIdentityBps(void) const;

    void   SetMinExonIdentity(double idty);
    static double s_GetDefaultMinExonIdty(void);
    double GetMinExonIdentity(void) const;

    void   SetPolyaExtIdentity(double idty);
    static double s_GetDefaultPolyaExtIdty(void);
    double GetPolyaExtIdentity(void) const;

    void   SetMinPolyaLen(size_t len);
    static size_t s_GetDefaultMinPolyaLen(void);
    size_t GetMinPolyaLen(void) const;

   void   SetStartModelId(size_t model_id) {
        m_model_id = model_id - 1;
    }
    size_t GetNextModelId(void) const {
        return m_model_id + 1;
    }

    void   SetMaxCompsPerQuery(size_t m);
    size_t GetMaxCompsPerQuery(void) const;
    
    typedef CNWFormatter::SSegment   TSegment;
    typedef vector<TSegment>         TSegments;


    // aligned compartment representation 
    struct NCBI_XALGOALIGN_EXPORT SAlignedCompartment {
        
        size_t           m_Id;

        enum ECompartmentStatus {
            eStatus_Ok,
            eStatus_Empty,
            eStatus_Error
        };

        ECompartmentStatus m_Status;

        string           m_Msg;
        bool             m_QueryStrand, m_SubjStrand;
        size_t           m_Cds_start, m_Cds_stop;
        size_t           m_QueryLen;
        size_t           m_PolyA;
        float            m_Score;
        TSegments        m_Segments;
        
        SAlignedCompartment(void):
            m_Id(0),
            m_Status(eStatus_Empty),
            m_Cds_start(0), m_Cds_stop(0),
            m_QueryLen (0),
            m_PolyA(0),
            m_Score(0)
        {}
        
        SAlignedCompartment(size_t id, const char* msg):
            m_Id(id),
            m_Status(eStatus_Empty),
            m_Msg(msg),
            m_Cds_start(0), m_Cds_stop(0),
            m_QueryLen(0),
            m_PolyA(0),
            m_Score(0)
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

    // align single compartment within given genomic bounds
    bool AlignSingleCompartment(THitRefs* hitrefs,
                                size_t range_left, size_t range_right,
                                SAlignedCompartment* result);

    // align single ASN.1 compartment
    bool AlignSingleCompartment(CRef<objects::CSeq_align> compartment,
                                SAlignedCompartment* result);


    // clear sequence vectors and scope - use with caution
    void ClearMem(void);

    typedef pair<size_t,size_t>   TOrf;
    typedef pair<TOrf,TOrf>       TOrfPair;
    TOrfPair GetCds(const THit::TId & id, const vector<char> * seq_data = 0);

    static size_t s_TestPolyA(const char * seq, size_t dim, size_t cds_stop = 0);
    bool IsPolyA(const char * seq, size_t polya_start, size_t dim, size_t cds_stop = 0);

    // alignment statistics

    enum ECompartmentScores {
        eCS_Matches             = 6,
        eCS_OverallIdentity     = 10,
        eCS_InframeMatches      = 20,
        eCS_InframeIdentity     = 22,
        eCS_Splices             = 23,
        eCS_ConsensusSplices    = 24,
        eCS_ProductCoverage     = 27,
        eCS_ExonIdentity        = 28,
        eCS_CombinationIdentity = 32
    };

    enum EStatFlags {
        eSF_BasicNonCds = 1 << 0,
        eSF_BasicCds    = 1 << 1
    };

    typedef list<CRef<objects::CScore_set> > TScoreSets;

    /// Generate statistics based on splign-generated seq-align-set,
    /// with each seq-align corresponding to an aligned compartment.
    ///
    /// @param sas
    ///   [IN] Seq-align-set describing input alignments.
    /// @param output_stats
    ///   [OUT] A pointer to the object to be be filled in with computed stats.
    /// @param cds
    ///   [IN] Coding region start and stop to use when computing cds-related stats.
    ///   If both are null then no cds-related stats will be computed.
    /// @param flags
    ///   [IN] Bitwise OR of the eSF_* flags specifying types of statistics to include.
    /// @return
    ///   The number of elements written in output_stats.
    static size_t s_ComputeStats(
        CRef<objects::CSeq_align_set> sas,
        TScoreSets *                  output_stats,
        TOrf                          cds = TOrf(0, 0),
        EStatFlags                    flags = eSF_BasicNonCds);

    /// Generate statistics based on splign-generated seq-align corresponding
    /// to a single aligned compartment.
    ///
    /// @param sa
    ///   [IN] Seq-align describing one aligned compartment.
    /// @param embed_scoreset
    ///   [IN] Decorate the input seq-align with the scores.
    /// @param cds
    ///   [IN] Coding region start and stop to use when computing cds-related stats.
    ///   If both are null then no cds-related stats will be computed.
    /// @param flags
    ///   [IN] Bitwise OR of the eSF_* flags specifying types of statistics to include.
    /// @return
    ///   A reference to a score-set object with the computed statistics.
    static CRef<objects::CScore_set> s_ComputeStats(
        CRef<objects::CSeq_align> sa,
        bool                      embed_scoreset = true,
        TOrf                      cds = TOrf(0, 0),
        EStatFlags                flags = eSF_BasicNonCds);

protected:

    // the spliced alignment computing object
    CRef<TAligner>        m_aligner;

    // access to sequence data
    CRef<objects::CScope> m_Scope;
    bool                  m_CanResetHistory;

    // alignment pattern
    vector<size_t>        m_pattern;

    // min exon idty - others will be marked as gaps 
    double                m_MinExonIdty;

    // min idty to extend alignment into polya
    double                m_MinPolyaExtIdty; 

    // min polya length
    size_t                m_MinPolyaLen;

    // compartment penalty as a per cent of the query (mRna) length
    double                m_CompartmentPenalty;

    // min compartment idty - others will be skipped
    double                m_MinCompartmentIdty;

    // min single compartment idty (per subject per strand) as a fraction of
    // the query length and as an absolute value.
    // The final value for the parameter is computed
    // as min(m_MinSingletonIdty * query_length, m_MinSingletonIdtyBps)
    double                m_MinSingletonIdty;

    size_t                m_MinSingletonIdtyBps;


    // mandatory end gap detection flag
    bool                  m_endgaps;

    // alignment map
    struct SAlnMapElem {
        size_t m_box [4];
        int    m_pattern_start, m_pattern_end;
    };
    vector<SAlnMapElem>   m_alnmap;

    typedef map<string,TOrfPair>  TStrIdToOrfs;
    TStrIdToOrfs          m_OrfMap;

    // query sequence
    vector<char>          m_mrna;
    bool                  m_strand;
    size_t                m_polya_start;
    bool                  m_nopolya;

    // in antisense, these are computed based on a reverse-
    // complimentary sequence, so start still less than stop
    size_t                m_cds_start; 
    size_t                m_cds_stop;  

    // genomic sequence
    vector<char>          m_genomic;

    // max space to look beyond end hits
    size_t                m_max_genomic_ext;

    // max intron length
    size_t                m_MaxIntron;

    // The limiting range as defined by the compartment hits,
    // if the max compartment hit identity is less than a cut-off.
    pair<size_t, size_t>  m_BoundingRange;

    // output per compartment
    TSegments             m_segments;
  
    // all compartments
    size_t                m_model_id;
    TResults              m_result;

    size_t                m_MaxCompsPerQuery;
    size_t                m_MinPatternHitLength;



    SAlignedCompartment x_RunOnCompartment( THitRefs* hitrefs,
                                            size_t range_left,
                                            size_t range_right);

    float  x_Run(const char* seq1, const char* seq2);

    void   x_SplitQualifyingHits(THitRefs* phitrefs);
    void   x_SetPattern(THitRefs* hitrefs);
    bool   x_ProcessTermSegm(TSegment** term_segs, Uint1 side) const;
    Uint4  x_GetGenomicExtent(const Uint4 query_extent, Uint4 max_ext = 0) const;
    void   x_FinalizeAlignedCompartment(SAlignedCompartment & ac);

    void   x_LoadSequence(vector<char>* seq, 
                          const objects::CSeq_id& seqid,
                          THit::TCoord start,
                          THit::TCoord finish,
                          bool retain);

    static THitRef sx_NewHit(THit::TCoord q0, THit::TCoord q,
                             THit::TCoord s0, THit::TCoord s);

    /// forbidden
    CSplign(const CSplign&);
    CSplign& operator=(const CSplign&);
};


END_NCBI_SCOPE


#endif
