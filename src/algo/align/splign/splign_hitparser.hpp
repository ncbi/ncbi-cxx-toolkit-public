#ifndef ALGO_ALIGN_SPLIGN_HF_HITPARSER__HPP
#define ALGO_ALIGN_SPLIGN_HF_HITPARSER__HPP

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
*   Temporary code - will be included with the hit filtering library
*
*/


#include <algo/align/splign_hit.hpp>


BEGIN_NCBI_SCOPE


class CHitParser  
{
public:

    CHitParser();
    CHitParser(const vector<CHit>& hits, int& GroupID);
    virtual ~CHitParser();
    void Init(const vector<CHit>& hits, int& GroupID);

    enum EMode {
      eMode_Combine,
      eMode_GroupSelect,
      eMode_Normal };
 
    int Run(EMode);

    enum {
      eMethod_MaxScore,
      eMethod_MaxScore_GroupSelect
    }
    m_Method;
    
    enum { 
      eSplitMode_MaxScore,
      eSplitMode_Clear
    }
    m_SplitQuery, m_SplitSubject;


    static int CalcCoverage(vector<CHit>::const_iterator ib,
                            vector<CHit>::const_iterator ie,
                            char where);

    static int GetCollisionCount(int& query, int& subj, const vector<CHit>& hits);

    size_t     GetSeqLength(const string& accession);

    vector<CHit> m_Out;          // output vector

    bool         m_SameOrder;    // true if hits in alignment must go
                                 // in same order

    enum {
      eStrand_Plus = 0,
      eStrand_Minus,
      eStrand_Both,
      eStrand_Auto
    }
    m_Strand;

    // combine-proximity (max dist btw two hits suitable for combining)
    double       m_CombinationProximity_pre;
    double       m_CombinationProximity_post;

    int          m_MaxHitDistQ; // max dist btw hits on query
    int          m_MaxHitDistS; // max dist btw hits on subject
    bool         m_Prot2Nucl;   // assume protein to nucleotide alignment

    
    enum EGroupIdentificationMethod {
        eNone,
        eQueryCoverage,
        eSubjectCoverage
    };
    
    EGroupIdentificationMethod   m_group_identification_method;
    
    
    double       m_CovStep;             // min sensible coverage raise

    double       m_MinQueryCoverage;    // min query coverage filter threshold
    double       m_MinSubjCoverage;     // min subj coverage filter threshold

    bool         m_OutputAllGroups;     // only max score groups otherwise

    string       m_Query, m_Subj;  // accessions

private:

    int                     m_ange[4];      // global envelop
    int                     m_Origin[2];  // leftmost point in original coordinates
                                            // [0] - q, [1] - s
    int*                    m_GroupID;
    int                     m_ngid;
    
    map<string, size_t>     m_seqsizes;          //  sizes indexed by accession

    void     x_Set2Defaults();
    int      x_CheckParameters() const;
    double   x_CalculateProximity(const CHit&, const CHit&) const;

    void     x_GroupsIdentifyByCoverage(int Start, int Stop, int& GroupId,
                                      double dCovCurrent, char where);
    void     x_CalcGlobalEnvelop();
    void     x_Combine(double dProximity); // search for close hits and combine them
    int      x_RunMaxScore();  // max score
    bool     x_RunAltSplitMode(char, CHit&, CHit&);
    bool     x_DetectInclusion(const CHit& h1, const CHit& h2) const;

    // max score group selection
    int      x_RunMSGS(bool SelectGroupsOnly, int& GroupId);
    void     x_TransformCoordinates(bool Dir);
    void     x_IdentifyMaxDistGroups();
    void     x_FilterByMaxDist();
    void     x_SyncGroupsByMaxDist(int&);

    void     x_FilterByOrder(size_t offset = 0, bool use_chainfilter = true);
    size_t   x_RemoveEqual(); // removes equal hits from m_vOut

};


END_NCBI_SCOPE

#endif
