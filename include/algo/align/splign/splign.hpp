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
        TSegments        m_segments;
        
        SAlignedCompartment(void): m_id(0), m_error(true)
        {}
        
        SAlignedCompartment(size_t id, bool err, const char* msg):
            m_id(id), m_error(err), m_msg(msg)
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
                                SAlignedCompartment* result,
                                const THitRefs* hitrefs_all = 0);

    // clear sequence vectors and scope - use with caution
    void ClearMem(void);

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

    typedef pair<size_t,size_t> TCDS;
    typedef map<string,TCDS> TStrIdToCDS;
    TStrIdToCDS m_CdsMap;

    // query sequence
    vector<char> m_mrna;
    bool         m_strand;
    size_t       m_polya_start;
    bool         m_nopolya;
    size_t       m_cds_start;
    size_t       m_cds_stop;

    // genomic sequence
    vector<char> m_genomic;

    // max space to look beyond end hits
    size_t       m_max_genomic_ext;

    // output per compartment
    TSegments    m_segments;
  
    // all compartments
    size_t       m_model_id;
    TResults     m_result;

    size_t       m_MaxCompsPerQuery;

    size_t       m_MinPatternHitLength;

    SAlignedCompartment x_RunOnCompartment( THitRefs* hitrefs,
                                            size_t range_left,
                                            size_t range_right,
                                            const THitRefs* hitrefs_all = 0);

    void   x_Run(const char* seq1, const char* seq2);
    size_t x_TestPolyA(void);
    void   x_SetPattern(THitRefs* hitrefs);
    void   x_ProcessTermSegm(TSegment** term_segs, Uint1 side) const;
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

/*
 * ===========================================================================
 *
 * $Log: splign.hpp,v $
 * Revision 1.39  2006/09/26 15:28:43  kapustin
 * Complete alignment information can now be passed to x_RunOnCompartment() for additional filtering of compartment hits
 *
 * Revision 1.38  2006/06/27 15:18:16  kapustin
 * +m_cds_*
 *
 * Revision 1.37  2006/05/22 16:00:08  kapustin
 * Pass max extent as an argument to x_GetGenomicExtent()
 *
 * Revision 1.36  2006/04/18 17:08:36  kapustin
 * Use member to hold min length for pattern hit
 *
 * Revision 1.35  2006/04/05 13:55:23  dicuccio
 * Added destructor, forbiddent copy constructor
 *
 * Revision 1.34  2006/03/21 16:17:33  kapustin
 * Support max singleton idty parameter
 *
 * Revision 1.33  2006/02/14 15:41:35  kapustin
 * +AlignSingleCompartment()
 *
 * Revision 1.32  2006/02/13 19:47:12  kapustin
 * +SetMaxCompsPerQuery()
 *
 * Revision 1.31  2005/12/01 18:31:40  kapustin
 * +CSplign::PreserveScope()
 *
 * Revision 1.30  2005/10/31 16:29:10  kapustin
 * Retrieve parameter defaults with static member methods
 *
 * Revision 1.29  2005/10/20 17:57:13  ivanov
 * + #include <objmgr/scope.hpp>
 *
 * Revision 1.28  2005/10/19 17:55:38  kapustin
 * Switch to using ObjMgr+LDS to load sequence data
 *
 * Revision 1.27  2005/09/12 16:22:31  kapustin
 * Move compartmentization to xalgoutil
 *
 * Revision 1.26  2005/09/06 17:52:29  kapustin
 * Add interface to max_extent member
 *
 * Revision 1.25  2005/08/29 14:13:42  kapustin
 * CSeqLoader::Load() +keep
 *
 * Revision 1.24  2005/08/02 15:55:48  kapustin
 * +x_GetGenomicExtent()
 *
 * Revision 1.23  2005/07/05 16:50:31  kapustin
 * Adjust compartmentization and term genomic extent. 
 * Introduce min overall identity required for compartments to align.
 *
 * Revision 1.22  2005/06/01 18:57:23  kapustin
 * +SAlignedCompartment::GetBox()
 *
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
