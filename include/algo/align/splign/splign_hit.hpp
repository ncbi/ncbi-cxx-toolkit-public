#ifndef ALGO_ALIGN_SPLIGN_HIT__HPP
#define ALGO_ALIGN_SPLIGN_HIT__HPP

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
*   Temporary code - will be part of the hit filtering library
*
*/


#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
    class CSeq_align;
END_SCOPE(objects)


// single hit representation
class NCBI_XALGOALIGN_EXPORT CHit
{
public:
    string m_Query, m_Subj;       // query and subject strings
    double m_Idnty;               // percent of identity
    int m_Length;	          // length of alignment, as in source row
    int m_anLength[2];            // real length of the hit at q and s parts
    int m_MM;	                  // the number of mistmatches
    int m_Gaps;                   // the number of gap openings
    int m_an [4];                 // q[uery]s[tart], qf[inish], s[ubj]s, sf
    int	m_ai [4];                 // query min, query max, subj min, subj max
    double m_Value;               // e-Value
    double m_Score;               // bit score

    int m_GroupID;                // group id
    int m_MaxDistClustID;         // clust group id

    CHit();
    CHit(const string&);
    CHit(const objects::CSeq_align&);
    CHit(const CHit&,const CHit&);
    virtual ~CHit() {}

    void Move(unsigned char corner, int new_pos, bool prot2nucl);
    void UpdateAttributes();

    bool Inside(const CHit&) const;
    bool IsConsistent() const;

    bool IsStraight() const {
        return m_an[2] <= m_an[3];
    }

    bool operator < (const CHit& h) const {
        return m_Score < h.m_Score;
    }

    bool operator > (const CHit& h) const {
        return m_Score > h.m_Score;
    }
    
    bool operator == (const CHit& h) const {
        return m_Score == h.m_Score;
    }

    friend NCBI_XALGOALIGN_EXPORT
    ostream& operator << (ostream&, const CHit&);

    unsigned CalcSize() {
        return sizeof(CHit) + m_Query.size() + m_Subj.size();
    }

    static bool PGreaterScore_ptr (const CHit* ph1, const CHit* ph2) {
        if(!ph1) return ph2 == 0;
        if(!ph2) return ph1 == 0;
        return *ph1 > *ph2;
    };

    static bool PLessScore_ptr (const CHit* ph1, const CHit* ph2) {
        if(!ph1) return ph2 == 0;
        if(!ph2) return ph1 == 0;
        return *ph1 < *ph2;
    };

    static bool PPrecedeStrand (const CHit& h1, const CHit& h2) {
        return h1.IsStraight() < h2.IsStraight();
    }

    static bool PPrecedeStrand_ptr (const CHit* ph1, const CHit* ph2) {
        if(!ph1) return ph2 == 0;
        if(!ph2) return ph1 == 0;
        return ph1->IsStraight() < ph2->IsStraight();
    }

    static bool PSubj_Score_ptr (const CHit* ph1, const CHit* ph2) {

      if(ph1->m_ai[2] < ph2->m_ai[2]) {
        return true;
      }
      else if (ph1->m_ai[2] > ph2->m_ai[2]) {
        return false;
      }
      else {
        return ph1->m_Score > ph2->m_Score;
      }
    }

    static bool PSubjLow_QueryLow_ptr (const CHit* ph1, const CHit* ph2) {

      if(ph1->m_ai[2] < ph2->m_ai[2]) {
        return true;
      }
      else if (ph1->m_ai[2] > ph2->m_ai[2]) {
        return false;
      }
      else {
        return ph1->m_ai[0] < ph2->m_ai[0];
      }
    }

    static bool PSubjHigh_QueryHigh_ptr (const CHit* ph1, const CHit* ph2) {

      if(ph1->m_ai[3] < ph2->m_ai[3]) {
        return true;
      }
      else if (ph1->m_ai[3] > ph2->m_ai[3]) {
        return false;
      }
      else {
        return ph1->m_ai[1] < ph2->m_ai[1];
      }
    }

    static bool PQuerySubjSubjCoord (const CHit& h1, const CHit& h2) {

      int cmp = strcmp(h1.m_Query.c_str(), h2.m_Query.c_str());
      if(cmp < 0) return true;
      if(cmp > 0) return false;
      cmp = strcmp(h1.m_Subj.c_str(), h2.m_Subj.c_str());
      if(cmp < 0) return true;
      if(cmp > 0) return false;

      return h1.m_ai[2] < h2.m_ai[2];
    }

    static bool PPrecedeQ (const CHit& h1, const CHit& h2) {
        return h1.m_ai[0] < h2.m_ai[0];
    }

    static bool PPrecedeS (const CHit& h1, const CHit& h2) {
        return h1.m_ai[2] < h2.m_ai[2];
    }

    static bool PPrecedeQ_ptr (const CHit* ph1, const CHit* ph2) {
        return ph1->m_ai[0] < ph2->m_ai[0];
    }

    static bool PPrecedeS_ptr (const CHit* ph1, const CHit* ph2) {
        return ph1->m_ai[2] < ph2->m_ai[2];
    }

    static bool PPrecedeBySeqId (const CHit& h1, const CHit& h2) {
        int nQ = strcmp(h1.m_Query.c_str(), h2.m_Query.c_str());
        if(nQ < 0) return true;
        if(nQ > 0) return false;
        int nS = strcmp(h1.m_Subj.c_str(), h2.m_Subj.c_str());
        if(nS < 0) return true;
        if(nS > 0) return false;
        return false;
    }

    static bool PPrecedeByGroupId (const CHit& h1, const CHit& h2) {
        return h1.m_GroupID <= h2.m_GroupID;
    }

    static int GetHitDistance(const CHit& h1, const CHit& h2, int nWhere);

    void SetEnvelop();

    // function objects used by Max Dist SLC algorithm
    friend class CMaxDistClustPred;
    friend class CMaxDistClIdSet;
    friend class CMaxDistClIdGet;

private:
    void x_InitDefaults();
};


class CHitGroup
{
public:
    int m_nID;
    vector<int>         m_vHits;                  // hit indices
    double              m_dPriority;              // group priority
    const vector<CHit>* m_pHits;                  // hit array pointer 
    double              m_adMidPoint [2];         // group middle point, q & s

    bool operator< (const CHitGroup& rhs) const {
        return m_dPriority < rhs.m_dPriority;  }

    bool CheckSameOrder();

    CHitGroup(const vector<CHit>* ph):
        m_nID(0), m_dPriority(0), m_pHits(ph) {}

    // default constructor only supported for maps
    CHitGroup():
        m_nID(0), m_dPriority(0), m_pHits(0) {}
};


////////////////////////////////////////////
// auxiliary functional objects

struct hit_groupid_less: public binary_function<CHit, CHit, bool>
{
    bool operator() (const CHit& h1, const CHit& h2) const {
        return h1.m_GroupID < h2.m_GroupID;
    }
};


struct hit_score_less: public binary_function<CHit,double,bool>
{
    bool operator()(const CHit& h, const double& d) const {
        return h.m_Score < d ? true: false;
    }
};

struct hit_greater: public binary_function<CHit,CHit,bool>
{
    bool operator()(const CHit& hm, const CHit& h0) const {
        return hm > h0 ? true: false;
    }
};

struct hit_strand_is: public binary_function<CHit,bool,bool>
{
    bool operator()(const CHit& hm, const bool& b) const {
        return b == hm.IsStraight();
    }
};

struct hit_ptr_strand_is: public binary_function<CHit*, bool, bool>
{
    bool operator()(const CHit* hit, bool strand_plus) const {
        return strand_plus == hit->IsStraight();
    }
};

struct hit_same_order: public binary_function<CHit,CHit,bool>
{
    bool operator()(const CHit& hm, const CHit& h0) const;
};

struct hit_not_same_order: public hit_same_order
{
    bool operator()(const CHit& hm, const CHit& h0) const;
};

struct hit_out_of_group: public binary_function<CHit,int,bool>
{
    bool operator()(const CHit& h, int n) const {
        return h.m_MaxDistClustID != n;
    }
};

class CMaxDistClustPred: public binary_function<CHit, CHit, bool>
{
public:
    CMaxDistClustPred(int m1, int m2): m_MaxQ(m1), m_MaxS(m2) {}
    result_type operator() (const first_argument_type& h1,
			    const second_argument_type& h2) const {
        int nHitDistQ = CHit::GetHitDistance(h1, h2, 0);
        int nHitDistS = CHit::GetHitDistance(h1, h2, 1);
        bool b0 = ( 0 <= nHitDistQ && nHitDistQ <= m_MaxQ &&
                    0 <= nHitDistS && nHitDistS <= m_MaxS );
        return b0;
    }
private:
    int m_MaxQ, m_MaxS;
};

class CMaxDistClIdSet: public binary_function<CHit, int, void>
{
public:
    result_type operator() (first_argument_type& h,
			    const second_argument_type& i) const {
        h.m_MaxDistClustID = i;
    }
};

class CMaxDistClIdGet: public unary_function<CHit, int>
{
public:
    result_type operator() (const argument_type& h) const {
        return h.m_MaxDistClustID;
    }
};


END_NCBI_SCOPE


#endif
