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


#include <ncbi_pch.hpp>
#include "splign_hitparser.hpp"
#include <algo/align/align_exception.hpp>

#include <corelib/ncbi_limits.hpp>
#include <corelib/ncbistre.hpp>

#include <algorithm>
#include <numeric>


BEGIN_NCBI_SCOPE

// auxiliary structure utilized in calculation of collisions and other routines
struct SNode	{
    int x; // coordinate
    bool type; // true = open, false = close
    vector<CHit>::const_iterator pHit; // owner
    SNode(int n, bool b, vector<CHit>::const_iterator ph):
        x(n), type(b), pHit(ph) {};
    bool operator>(const SNode& rhs) const { return x > rhs.x; }
    bool operator==(const SNode& rhs) const { return x == rhs.x; }
    bool operator<(const SNode& rhs) const { return x < rhs.x; }
};

//
// general single-linkage clustering (SLC) algorithm 
//

struct SNodePair
{
    SNodePair(int i, int j): n1(i), n2(j) {}
    int n1, n2;
};


template<class T, class PClustCrit, class CClustID_Set, class CClustID_Get>
void DoSingleLinkageClustering (
   vector<T>& vt,          // vector of objects to be clustered
   const PClustCrit& pr2,        // clustering criterion (two-argument predicate)
   const CClustID_Set& CID_Set,  // function that assigns ClustID to objects
   const CClustID_Get& CID_Get   // function that retrieves ClustID from objects
)

{
    vector<SNodePair> vnodes;    // auxiliary vector
    size_t nSize = vt.size(), j = 0, i;

    for( j = 0; j < nSize; ++j ) {

        CID_Set( vt[j], j );
        for( i = 0; i < j; ++i ) {

            if( pr2(vt[i], vt[j] ) )
                vnodes.push_back( SNodePair( i, j ) );
         }
    }
 
    vector<SNodePair>::const_iterator kk, kend;
    for( kk = vnodes.begin(), kend = vnodes.end(); kk != kend; ++kk ) {

        int n1 = CID_Get( vt[kk->n1] ), n2 = CID_Get( vt[kk->n2] );
        if( n1 != n2 ) {
            NON_CONST_ITERATE (typename vector<T>, jj, vt) {
                if( CID_Get( *jj ) == n1 )
                    CID_Set( *jj, n2 );
            }
        }
    }
}


CHitParser::CHitParser()
{
    x_Set2Defaults();
}

CHitParser::CHitParser(const vector<CHit>& vh, int& nGroupID)
{
    Init(vh, nGroupID);
}

void CHitParser::Init(const vector<CHit>& vh, int& nGroupID)
{
    m_Out = vh;
    x_RemoveEqual();
    x_Set2Defaults();
    nGroupID++;
    m_GroupID = &nGroupID;
}

void CHitParser::x_Set2Defaults()
{
    m_SameOrder = true;
    m_Strand = eStrand_Auto;
    m_MaxHitDistQ = kMax_Int;
    m_MaxHitDistS = kMax_Int;
    m_Method = eMethod_MaxScore;
    m_SplitQuery   = eSplitMode_MaxScore;
    m_SplitSubject = eSplitMode_MaxScore;
    m_Prot2Nucl = false;

    m_CombinationProximity_pre = m_CombinationProximity_post = -1.;

    m_group_identification_method = eNone;
    m_CovStep = 90.;

    m_MinQueryCoverage = 0.;
    m_MinSubjCoverage = 0.;

    m_OutputAllGroups = false;

    m_Query = m_Subj = "";

    m_GroupID = 0;
    
    x_CalcGlobalEnvelop();
}

void CHitParser::x_CalcGlobalEnvelop()
{
    const size_t dim = m_Out.size();
    if(dim == 0) {
        m_ange[0] = m_ange[1] = m_ange[2] = m_ange[3] = 0;
        return;
    }
    m_ange[0] = m_ange[2] = kMax_Int;
    m_ange[1] = m_ange[3] = kMin_Int;
    vector<CHit>::iterator ii, iend;
    for(ii = m_Out.begin(), iend = m_Out.end(); ii != iend; ii++)
    {
        if(m_ange[0] > ii->m_ai[0]) m_ange[0] = ii->m_ai[0];
        if(m_ange[2] > ii->m_ai[2]) m_ange[2] = ii->m_ai[2];
        if(m_ange[1] < ii->m_ai[1]) m_ange[1] = ii->m_ai[1];
        if(m_ange[3] < ii->m_ai[3]) m_ange[3] = ii->m_ai[3];
    }
}

CHitParser::~CHitParser()
{
}


bool  CHitParser::x_DetectInclusion(const CHit& h1, const CHit& h2) const
{
  if( h2.m_ai[0] <= h1.m_ai[0] && h1.m_ai[1] <= h2.m_ai[1]) {
    return true;
  }
  if( h2.m_ai[2] <= h1.m_ai[2] && h1.m_ai[3] <= h2.m_ai[3]) {
    return true;
  }
  if( h1.m_ai[0] <= h2.m_ai[0] && h2.m_ai[1] <= h1.m_ai[1]) {
    return true;
  }
  if( h1.m_ai[2] <= h2.m_ai[2] && h2.m_ai[3] <= h1.m_ai[3]) {
    return true;
  }
  return false;
}


// implements half splitmode
bool CHitParser::x_RunAltSplitMode(char cSide, CHit& hit1, CHit& hit2)
{
    int nb = cSide*2;

    // detect intersection
    if(hit1.m_ai[nb] <= hit2.m_ai[nb] && hit2.m_ai[nb] <= hit1.m_ai[nb+1])
    {
        int n1 = hit1.m_ai[nb + 1] + 1;
        int n2 = hit2.m_ai[nb] - 1;
        if(m_Prot2Nucl)
        {  // frame must be preserved
            int nShift = n1 - hit2.m_ai[nb];
            while(nShift % 3) { nShift++; n1++; }
            nShift = hit1.m_ai[nb + 1] - n2;
            while(nShift % 3) { nShift++; n2--; }
        }
        hit1.Move(nb + 1, n2, m_Prot2Nucl);
        hit2.Move(nb, n1, m_Prot2Nucl);
        hit1.UpdateAttributes();
        hit2.UpdateAttributes();
    }
    else
    if(hit2.m_ai[nb] <= hit1.m_ai[nb] && hit1.m_ai[nb] <= hit2.m_ai[nb+1])
    {
        int n1 = hit1.m_ai[nb] - 1;
        int n2 = hit2.m_ai[nb + 1] + 1;
        if(m_Prot2Nucl)
        {  // frame must be preserved
            int nShift = hit2.m_ai[nb + 1] - n1;
            while(nShift % 3) { nShift++; n1--; }
            nShift = n2 - hit1.m_ai[nb];
            while(nShift % 3) { nShift++; n2++; }
        }
        hit1.Move(nb, n2, m_Prot2Nucl);
        hit2.Move(nb + 1, n1, m_Prot2Nucl);
        hit1.UpdateAttributes();
        hit2.UpdateAttributes();
    }

    return true;
}

int GetIntersectionSize (int a0, int b0, int c0, int d0) {
    int a = min(a0, b0), b = max(a0, b0), c = min(c0, d0), d = max(c0, d0);
    int an1[] = {a, b};
    int an2[] = {c, d};
    int* apn[] = {an1, an2};
    if(c <= a) {
        apn[0] = an2; apn[1] = an1;
    }
    int n = (apn[0][1] - apn[1][0]) + 1;
    return n > 0? n: 0;
}


// accumulate coverage on query or subject
class AcmCvg: public binary_function<int, const CHit*, int>
{
public:
    AcmCvg(char where): m_where(where), m_nFinish(-1) {
        m_i1 = m_where == 'q'? 0: 2;
        m_i2 = m_where == 'q'? 1: 3;
    }
    int operator() (int iVal, const CHit* ph)
    {
        const CHit& h = *ph;
        if(h.m_ai[m_i1] > m_nFinish)
            return iVal + (m_nFinish = h.m_ai[m_i2]) - h.m_ai[m_i1] + 1;
        else
        {
            int nFinish0 = m_nFinish;
            return (h.m_ai[m_i2] > nFinish0)?
                (iVal + (m_nFinish = h.m_ai[m_i2]) - nFinish0): iVal;
        }
    }
private:
    char m_where;
    int  m_nFinish;
    unsigned char m_i1, m_i2;
};


int CHitParser::CalcCoverage(vector<CHit>::const_iterator ib,
                             vector<CHit>::const_iterator ie,
                             char where)
{
    vector<const CHit*> vh (ie - ib, (const CHit*)0);
    for(int i = 0; i < ie - ib; ++i)  vh[i] = &(*ib) + i;
    if(where == 'q') {
        stable_sort(vh.begin(), vh.end(), CHit::PPrecedeQ_ptr);
    }
    else {
        stable_sort(vh.begin(), vh.end(), CHit::PPrecedeS_ptr);
    }
    int s = accumulate(vh.begin(), vh.end(), 0, AcmCvg(where));
    return s;
}

//////// Maximum Score method:
// 1. Sort the vector in desc order starting from the current hit
// 2. Consider max score hit (the current)
// 3. Process the other hits, so the current hit remains intact
//    and overlapping free
// 4. Go to step 1.
int CHitParser::x_RunMaxScore()
{
    // special pre-processing for prot2nucl mode:
    // remove those hits that intersect on the genomic
    // side having different reading frame
    if(m_Prot2Nucl && false)  // -- currently disabled
    {
        list<vector<CHit>::iterator > l0;
        vector<CHit>::iterator ivh;
        for(ivh = m_Out.begin(); ivh != m_Out.end(); ivh++)
            l0.push_back(ivh);
        list<vector<CHit>::iterator >::iterator ii;
        bool bDel = false;
        for(ii = l0.begin(); ii != l0.end(); )
        {
            list<vector<CHit>::iterator >::iterator jj = ii;
            for(++jj; jj != l0.end(); jj++)
            {
                int& a = (*ii)->m_ai[2];
                int& b = (*ii)->m_ai[3];                
                int& c = (*jj)->m_ai[2];
                int& d = (*jj)->m_ai[3];
                bool bSameFrame = a%3 == c%3;
                if( !bSameFrame && GetIntersectionSize(a, b, c, d) > 0)
                {
                    l0.erase(jj);
                    bDel = true;
                    break;
                }
            }
            if(bDel)
            {
                list<vector<CHit>::iterator >::iterator jj0 = ii; jj0++;
                l0.erase(ii);
                ii = jj0;
                bDel = false;
            }
            else
                ii++;
        }
        vector<CHit> vh1;
        for(ii = l0.begin(); ii != l0.end(); ii++)
            vh1.push_back(**ii);
        m_Out = vh1;
    }

    size_t nQuerySize = 0;
    if(m_MinQueryCoverage > 0.) {
        NCBI_THROW( CAlgoAlignException,
                    eInternal,
                    "Query coverage filtering not supported" );
    }

    size_t nSubjSize = 0;
    if(m_MinSubjCoverage > 0.) {
        NCBI_THROW( CAlgoAlignException,
                    eInternal,
                    "Subj coverage filtering not supported" );
    }

    // main processing
    int i0 = 0;
    bool bRestart = true;
    while(bRestart) {
        bRestart = false;
        for(i0 = 0; i0 < int(m_Out.size())-1; ++i0) {

            vector<CHit>::iterator ii = m_Out.begin() + i0;
            // Sort by score (& priority) starting from the current hit
            sort(ii, m_Out.end(), hit_greater());
            // Consider max score hit (the current)
            CHit& hit = *ii;
            // Process the other hits, so the current hit
            // remains intact and overlapping free
            int j0 = 0;
            for(j0 = int(m_Out.size())-1; j0 > i0; --j0) {

                vector<CHit>::iterator jj = m_Out.begin() + j0;

		if(m_SplitQuery == eSplitMode_Clear
		   || m_SplitSubject == eSplitMode_Clear) {
		  if(x_DetectInclusion(hit, *jj)) {
		    m_Out.erase(jj);
		    continue;
		  }
		}

                if(m_SplitQuery == eSplitMode_Clear) {
                    if(x_RunAltSplitMode(0, hit, *jj) == false) {
                        m_Out.erase(jj);
                        continue;
                    }
                }
                if(m_SplitSubject == eSplitMode_Clear) {
                    if(x_RunAltSplitMode(1, hit, *jj) == false) {
                        m_Out.erase(jj);
                        continue;
                    }
                }

                // We have two possibilities:
                // (1) there is at least one end of *jj inside the current hit;
                // (2) when *jj "embraces" the current hit;

                // (1) - proceed it in cyclic manner
                bool b [4];
                bool bFirstLoop = true;
                bool bContinue = false;
                do {
                    if(!bFirstLoop) {
                        size_t i = 0;
                        for(i = 0; i < 4; ++i) {
                            if(b[i]) {
                                switch(i) {

                                case 0: {
                                    jj->Move(0,
                                             hit.m_ai[1] + 1,
                                             m_Prot2Nucl);
                                    for(size_t d = 2; d < 4; ++d) {
                                        b[d] = hit.m_ai[2] <= jj->m_ai[d] &&
                                               jj->m_ai[d] <= hit.m_ai[3];
                                    }
                                }
                                break;

                                case 1: {
                                    jj->Move(1,
                                             hit.m_ai[0] - 1,
                                             m_Prot2Nucl);
                                    for(size_t d = 2; d < 4; ++d) {
                                        b[d] = hit.m_ai[2] <= jj->m_ai[d] &&
                                               jj->m_ai[d] <= hit.m_ai[3];
                                    }
                                }
                                break;

                                case 2: {
                                    jj->Move(2,
                                             hit.m_ai[3] + 1,
                                             m_Prot2Nucl);
                                    for(size_t d = 0; d < 2; ++d) {
                                        b[d] = hit.m_ai[0] <= jj->m_ai[d] &&
                                               jj->m_ai[d] <= hit.m_ai[1];
                                    }
                                }
                                break;

                                case 3: {
                                    jj->Move(3,
                                             hit.m_ai[2] - 1,
                                             m_Prot2Nucl);
                                    for(size_t d = 0; d < 2; ++d) {
                                        b[d] = hit.m_ai[0] <= jj->m_ai[d] &&
                                               jj->m_ai[d] <= hit.m_ai[1];
                                    }
                                }
                                break;
                                }
                            }
                            
                            // check if *jj still exists
                            if(!jj->IsConsistent())
                            {
                                m_Out.erase(jj);
                                bContinue = true;
                                break;
                            }
                        }
                        if(bContinue) break;
                    }
                    else
                        bFirstLoop = false;

                    size_t i = 0;
                    for(i = 0; i < 2; ++i) {
                        b[i] = hit.m_ai[0] <= jj->m_ai[i] &&
                            jj->m_ai[i] <= hit.m_ai[1];
                    }
                    for(i = 2; i < 4; ++i) {
                        b[i] = hit.m_ai[2] <= jj->m_ai[i] &&
                            jj->m_ai[i] <= hit.m_ai[3];
                    }
                    if(b[0] && b[1] || b[2] && b[3]) {
                        m_Out.erase(jj);
                        bContinue = true;
                        break;
                    }
                }
                while(b[0] || b[1] || b[2] || b[3]);
            
                if(bContinue)
                    continue;    // the hit has dissapeared

                // (2)
                double ad[] = {
                    hit.m_ai[0] - jj->m_ai[0], jj->m_ai[1] - hit.m_ai[1],
                    hit.m_ai[2] - jj->m_ai[2], jj->m_ai[3] - hit.m_ai[3] };
                bool ab[] = {ad[0] > 0, ad[1] > 0, ad[2] > 0, ad[3] > 0 };
                bool bNewHit = false;
                CHit hit2;
                if(ab[0] && ab[1] || ab[2] && ab[3]) { // split *jj
                    int n1, n2, n1x, n2x;
                    if(jj->IsStraight()) {
                        if(!(ab[2] && ab[3])) {
                            n1 = 1; n2 = 0;
                            n1x = hit.m_ai[0] - 1;
                            n2x = hit.m_ai[1] + 1;
                        }
                        else if(!(ab[0] && ab[1])) {
                            n1 = 3; n2 = 2;
                            n1x = hit.m_ai[2] - 1;
                            n2x = hit.m_ai[3] + 1;
                        }
                        else {
                            n1 = (ad[0] < ad[2])? 1: 3;
                            n2 = (ad[1] < ad[3])? 0: 2;
                            n1x = (ad[0] < ad[2])?
                                hit.m_ai[0] - 1: hit.m_ai[2] - 1;
                            n2x = (ad[1] < ad[3])?
                                hit.m_ai[1] + 1: hit.m_ai[3] + 1;
                        }
                    }
                    else {
                        if(!(ab[2] && ab[3])) {
                            n1 = 1; n2 = 0;
                            n1x = hit.m_ai[0] - 1; n2x = hit.m_ai[1] + 1;
                        }
                        else if(!(ab[0] && ab[1])) {
                            n1 = 2; n2 = 3;
                            n1x = hit.m_ai[3] + 1; n2x = hit.m_ai[2] - 1;
                        }
                        else {
                            n1 = (ad[0] < ad[3])? 1: 2;
                            n2 = (ad[1] < ad[2])? 0: 3;
                            n1x = (ad[0] < ad[3])?
                                hit.m_ai[0] - 1: hit.m_ai[3] + 1;
                            n2x = (ad[1] < ad[2])?
                                hit.m_ai[1] + 1: hit.m_ai[2] - 1;
                        }
                    }
                    hit2 = *jj;
                    bNewHit = true;
                    jj->Move(n1, n1x, m_Prot2Nucl);
                    if(!jj->IsConsistent()) {
                        m_Out.erase(jj);
                    }
                    else { // some trapezoidal hits may still overlap
                        bRestart = true;
                    }
                    hit2.Move(n2, n2x, m_Prot2Nucl);
                    if(hit2.IsConsistent()) {
                        // some trapezoidal hits may still overlap
                        hit2.UpdateAttributes();
                        bRestart = true;
                    }
                    else {
                        bNewHit = false;
                    }
                }

                jj->UpdateAttributes();
                if(bNewHit) {
                    m_Out.push_back(hit2);
                }
                if(bRestart) break;
            }

            if(m_SameOrder) {
                x_FilterByOrder(i0, false);
            }

            // step 4: re-assess groups and continue the cycle
            if(bRestart) break;
        }
    }

    if(m_MinQueryCoverage > 0.) {
        // calculate query coverage
        double dQCov = CalcCoverage(m_Out.begin(), m_Out.end(), 'q');
        if(dQCov < m_MinQueryCoverage* nQuerySize)
            m_Out.clear();
    }

    if(m_MinSubjCoverage > 0.) {
        // calculate subj coverage
        double dSCov = CalcCoverage(m_Out.begin(), m_Out.end(), 's');
        if(dSCov < m_MinSubjCoverage* nSubjSize)
            m_Out.clear();
    }
    return 0;
}


void CHitParser::x_FilterByOrder(size_t offset, bool use_chainfilter)
{
    size_t dim = m_Out.size();
    if(offset >= dim) return;
    vector<CHit>::iterator ii_beg = m_Out.begin() + offset;

    if(use_chainfilter) {  // groupwise chain filtering
      /*
        sort(ii_beg, m_Out.end(), hit_groupid_less());
        int nGroupID = m_Out[0].m_GroupID;
        size_t start = 0, fin = 0, target = 0;
        while(fin < dim) {
            for(; fin < dim && nGroupID == m_Out[fin].m_GroupID; ++fin);
            start = fin;
            nGroupID = m_Out[fin].m_GroupID;
            vector<CHit> vh (ii_beg + start, ii_beg + fin);
            CChainFilter cf (vh);
            cf.Run();
            copy(vh.begin(), vh.end(), ii_beg + target);
            target += vh.size();
        }
        m_Out.resize(target);
      */
    }
    else { // greedy filtering
        vector<CHit>::iterator ii2 = ii_beg, iend2;
        //for(iend2 = m_Out.end(); ii2 < iend2; ++ii2) {
        //    iend2 = remove_if(ii2 + 1, iend2,
        //                     bind1st(hit_not_same_order(), *ii2));
        //}
        iend2 = remove_if(ii2 + 1, m_Out.end(),
                             bind1st(hit_not_same_order(), *ii2));
        m_Out.erase(iend2, m_Out.end());
    }
}


// Calculates proximity btw any two hits.
// Return value can range from 0 (similar) to 1 (different)
double CHitParser::x_CalculateProximity(const CHit& h1, const CHit& h2) const
{
    //    if(m_SameOrder)
    //    {
    //        hit_not_same_order hnso;
    //        if(hnso(h1,h2)) return 1.;
    //    }
    double adm [] = { m_ange[1] - m_ange[0] + 1, m_ange[3] - m_ange[2] + 1};
    double dm = max(adm[0], adm[1]);
    double ad [2];
    for(int i = 0; i < 4; i += 2) {
        int i0 = i, i1 = i + 1;
        if(h1.m_ai[i1] <= h2.m_ai[i0])  // no intersection
            ad[i/2] = double(h2.m_ai[i0] - h1.m_ai[i1] - 1) / dm;// adm[i/2];
        else
        if(h2.m_ai[i1] <= h1.m_ai[i0])  // no intersection
            ad[i/2] = double(h1.m_ai[i0] - h2.m_ai[i1] - 1) / dm;//adm[i/2];
        else // inclusion
        if(h1.m_ai[i0] <= h2.m_ai[i0] && h2.m_ai[i1] <= h1.m_ai[i1] ||
             h2.m_ai[i0] <= h1.m_ai[i0] && h1.m_ai[i1] <= h2.m_ai[i1] )
            ad[i/2] = 1.0;
        else // intersection
        if(h2.m_ai[i0] <= h1.m_ai[i0] && h1.m_ai[i0] <= h2.m_ai[i1])
            ad[i/2] = double(h2.m_ai[i1] - h1.m_ai[i0] + 1) /
                (h1.m_ai[i1] - h2.m_ai[i0] + 1);
        else
            ad[i/2] = double(h1.m_ai[i1] - h2.m_ai[i0] + 1) /
                (h2.m_ai[i1] - h1.m_ai[i0] + 1);
    }
    return max(ad[0],ad[1]);
}


int CHitParser::x_RunMSGS(bool bSelectGroupsOnly, int& nGroupId)
{
    if( m_group_identification_method != eNone || bSelectGroupsOnly )
    {
        if(m_group_identification_method == eQueryCoverage) {
            x_GroupsIdentifyByCoverage(0, m_Out.size(), nGroupId, 0, 'q');
        }
        else {
            x_GroupsIdentifyByCoverage(0, m_Out.size(), nGroupId, 0, 's');
        }
        x_SyncGroupsByMaxDist(nGroupId);
    }

    if(m_OutputAllGroups && !bSelectGroupsOnly && m_Out.size())
    {
        // make copies to m_Out here
        // new subj string must reflect both number and strand
        // like [s4], [i8]

        // first we count the number of groups
        size_t nGroupCount = 1;
        {{
        sort(m_Out.begin(), m_Out.end(), hit_groupid_less());
        vector<CHit>::iterator ibegin = m_Out.begin(), iend = m_Out.end(),
            ii = ibegin;
        int ngid = ii->m_GroupID;
        for(++ii; ii != iend; ++ii)
        {
            if(ii->m_GroupID != ngid)
            {
                ngid = ii->m_GroupID;
                ++nGroupCount;
            }
        }
        }}

        {{
            // determine strands of the groups
            // 1 = straight, 0 = inverse, -1 = both / undefined
            vector<char> vStrands (nGroupCount, -1);
            vector<CHit>::iterator ibegin = m_Out.begin(),
                ii = ibegin, iend = m_Out.end();
            for(size_t ig = 0; ig < nGroupCount; ++ig)
            {
                vStrands[ig] = ii->IsStraight()? 1: 0;
                int ngid = ii->m_GroupID;
                for(; ii != iend; ii++)
                {
                    if(ii->m_GroupID != ngid) break;
                    char cs = ii->IsStraight()? 1: 0;
                    if(cs != vStrands[ig]) vStrands[ig] = -1;
                }
            }
            
            // now separate the groups
            ii = ibegin;
            string strSubject0 = ii->m_Subj;
            int ngc_s = 0, ngc_i = 0;
            for(size_t ig = 0; ig < nGroupCount; ig++)
            {
                int ngid = ii->m_GroupID;
		CNcbiOstrstream oss; // new subject
                char c0 = (vStrands[ig] == -1)? 'x':
                    (vStrands[ig] == 0)? 'm': 'p';
                int ngc = (c0 == 'i')? ++ngc_i: ++ngc_s;
		oss << strSubject0 << "_[" << c0 << ngc << ']';
		const string new_subj = CNcbiOstrstreamToString(oss);
                for(; ii != iend; ++ii)
                {
                    if(ii->m_GroupID != ngid) break;
                    ii->m_Subj = new_subj;
                }
            }
        }}

        return 0; // we don't resolve ambiguities in this mode
    }

    if(bSelectGroupsOnly) return 0;

    vector<CHit> vh (m_Out);
    vector<CHit> vh_out;

    double max_group_score = 0;
    size_t i = 0, dim = vh.size();
    while(i < dim) {
        int groupid0 = vh[i].m_GroupID;
        int istart = i;
        for(; i < dim && groupid0 == vh[i].m_GroupID; ++i);
        int ifin = i;
        m_Out.resize(ifin - istart);
        copy(vh.begin() + istart, vh.begin() + ifin, m_Out.begin());
        x_RunMaxScore();
        double group_score = 0;
        size_t dim_group = m_Out.size();
        for(size_t k = 0; k < dim_group; ++k) group_score += m_Out[k].m_Score;
        
        if(m_OutputAllGroups) {
            copy(m_Out.begin(), m_Out.end(), back_inserter(vh_out));
        }
        else if(group_score > max_group_score) {
            max_group_score = group_score;
            vh_out.clear();
            copy(m_Out.begin(), m_Out.end(), back_inserter(vh_out));
        }
    }

    m_Out = vh_out;

    return 0;
}


void CHitParser::x_GroupsIdentifyByCoverage(int iStart, int iStop, int& iGroupId,
                                          double dTotalCoverage, char where)
{
    bool bTopLevelCall = (iStart == 0) && (size_t(iStop) == m_Out.size());

    if(bTopLevelCall) { // top-level call
        x_CalcGlobalEnvelop();
        if(where == 'q') {
            stable_sort(m_Out.begin() + iStart, m_Out.begin() + iStop,
                        CHit::PPrecedeS);
        }
        else {
            stable_sort(m_Out.begin() + iStart, m_Out.begin() + iStop,
                        CHit::PPrecedeQ);
        }
        dTotalCoverage = CalcCoverage(m_Out.begin() + iStart,
                                      m_Out.begin() + iStop, where);
    }

    // assign new groupid
    {{
        ++iGroupId;
        for(int i = iStart; i < iStop; ++i)
            m_Out[i].m_GroupID = iGroupId;
    }}


    if(iStop - iStart <= 1) return;

    // choose the best location
    double dMax = dTotalCoverage;
    int iMax = iStop;
    unsigned char idx1, idx2;
    if(where == 'q') {
        idx1 = 2;
        idx2 = 3;
    }
    else {
        idx1 = 0;
        idx2 = 1;
    }
    // find all gaps, sort them by size, then 
    // proceed from larger to lower
    typedef multimap<int, int> mm_t;
    mm_t mm_gapsize2index;
    for(int i = iStart; i < iStop - 1; i++)
    {
        int gap = m_Out[i+1].m_ai[idx1] - m_Out[i].m_ai[idx2];
        if(gap > 0) {
            mm_gapsize2index.insert(mm_t::value_type(gap, i));
        }
    }

    mm_t::reverse_iterator mmirb = mm_gapsize2index.rbegin();
    mm_t::reverse_iterator mmire = mm_gapsize2index.rend();
    mm_t::reverse_iterator mmir;
    bool match = false;
    for(mmir = mmirb; mmir != mmire; ++mmir) {
        int i = mmir->second;
        int n1 = CalcCoverage(m_Out.begin() + iStart,
                              m_Out.begin() + i + 1, where);
        int n2 = CalcCoverage(m_Out.begin() + i + 1,
                              m_Out.begin() + iStop, where);
        double d = (n1 + n2);
        match = (d - dTotalCoverage) / dTotalCoverage  >= m_CovStep;
        if(match) {
             dMax = d;
             iMax = i;
             break;
         }
    }

    if(match)
    {
        x_GroupsIdentifyByCoverage(iStart, iMax + 1, iGroupId,
				   dTotalCoverage, where);
        x_GroupsIdentifyByCoverage(iMax + 1, iStop, iGroupId,
				   dTotalCoverage, where);
    }
}


size_t CHitParser::x_RemoveEqual()
{
    typedef map<size_t, vector<size_t> > map_t;
    map_t m;  // hit checksum to hits

    vector<size_t> vhd;  // hits to delete
    size_t dim = m_Out.size();
    for(size_t i = 0; i < dim ; ++i) {
        const CHit& h = m_Out[i];
        size_t checksum = ( h.m_an[0] + h.m_an[1] + h.m_an[2] + h.m_an[3] )
                          % 1000;
        map_t::iterator mi = m.find(checksum);
        if(mi == m.end()) {
            m[checksum].push_back(i);
        }
        else {
            vector<size_t>& vph = mi->second;
            size_t dim_hits = vph.size(), k = 0;
            for(; k < dim_hits; ++k) {
                const CHit& h2 = m_Out[vph[k]];
                if( h.m_an[0] == h2.m_an[0] && h.m_an[1] == h2.m_an[1] &&
                    h.m_an[2] == h2.m_an[2] && h.m_an[3] == h2.m_an[3]    ) {

                    vhd.push_back(i);
                    break;
                }
            }
            if(k == dim_hits) {
                    m[checksum].push_back(i);
            }
        }
    }

    size_t del_count = 0;
    if(vhd.size()) {
        sort(vhd.begin(), vhd.end());
        size_t dim_vhd = vhd.size();
        size_t vhd_i = 0;
        for(size_t k = vhd[vhd_i]; k < dim - del_count; ++k) {
            bool b = false;
            if(vhd_i < dim_vhd && k == vhd[vhd_i] - del_count) {
                ++del_count;
                ++vhd_i;
                b = true;
            }
            size_t k1 = k + del_count;
            if(k1 < dim) {
                m_Out[k] = m_Out[k1];
            }
            else  break;
            if(b) --k;
        }
        m_Out.resize(dim - del_count);
    }

    return del_count;
}


int CHitParser::Run(EMode erm)
{
    int nCode = x_CheckParameters();
    if(nCode) return nCode;

    vector<CHit> vhInvStrand;      // used only if strand mode = auto
    int& nGroupId = *m_GroupID;  // auxiliary variable
    size_t dim = m_Out.size();
    if(dim)
    {
        x_TransformCoordinates(true);
        x_Combine(m_CombinationProximity_pre);

        {{

        // filter by strand or separate strands, if applicable
        if(m_Strand == eStrand_Plus || m_Strand == eStrand_Minus ||
           m_Strand == eStrand_Auto)
        {
            vector<CHit>::iterator ii;
            for(ii = m_Out.end()-1; ii >= m_Out.begin(); ii--)
            {
                bool bStraight = ii->IsStraight();
                switch(m_Strand)
                {
                    case eStrand_Plus:
                        if (bStraight) continue; else m_Out.erase(ii);
                        break;
                    case eStrand_Minus:
                        if (!bStraight) continue; else m_Out.erase(ii);
                        break;
                    case eStrand_Auto: //  separate strands before processing
                        if (bStraight) continue;
                        else
                        {
                            vhInvStrand.push_back(*ii);
                            m_Out.erase(ii);
                        }
                        break;
                }
            }
        }

        x_CalcGlobalEnvelop();

        switch(erm)
        {
            case eMode_Combine: // already combined
                break;
            case eMode_GroupSelect:
                x_RunMSGS(true, nGroupId);
                break;
            case eMode_Normal:
                x_IdentifyMaxDistGroups();
                switch(m_Method)
                {
                    case eMethod_MaxScore:
                        x_RunMaxScore(); break;
                    case eMethod_MaxScore_GroupSelect:
                        x_RunMSGS(false, nGroupId); break;
                    default: return 1; //  not supported
                }
                if(!m_OutputAllGroups) {
                    // we need to identify again because "connecting"
                    // hits might might have perished
                    x_IdentifyMaxDistGroups();  
                    x_FilterByMaxDist();
                }
                break;
        }


        // if strand mode is auto then we run it again for inverse strand
        // and then compare the strands
        if(m_Strand == 3)
        {
            vector<CHit> vh0 = m_Out;
            m_Out = vhInvStrand;
            x_CalcGlobalEnvelop();
            switch(erm)
            {
                case eMode_Combine:
                    break;
                case eMode_GroupSelect:
                    x_RunMSGS(true, nGroupId);
                    break;
                case eMode_Normal:
                    x_IdentifyMaxDistGroups();
                    switch(m_Method)
                    {
                        case eMethod_MaxScore:
                            x_RunMaxScore(); break;
                        case eMethod_MaxScore_GroupSelect:
                            x_RunMSGS(false, nGroupId); break;
                        default: return 1; //  not supported
                    }
                    x_IdentifyMaxDistGroups();
                    x_FilterByMaxDist();
                    break;
            }


            if(erm == eMode_Normal && !m_OutputAllGroups)
            { // leave only the best strand
                double dScoreStraight = 0., dScoreInverse = 0.;
                vector<CHit>::iterator ii = vh0.begin(), iend = vh0.end();
                for(; ii != iend; ii++) dScoreStraight += ii->m_Score;
                ii = m_Out.begin(); iend = m_Out.end();
                for(; ii != iend; ii++) dScoreInverse += ii->m_Score;
                if(dScoreStraight > dScoreInverse) m_Out = vh0;
            }
            else
            { // merge the strands since we don't actually want to
              // eliminate ambiguities here
              copy(vh0.begin(), vh0.end(), back_inserter(m_Out));
            }
        }

        x_Combine(m_CombinationProximity_post);

        }}

        x_TransformCoordinates(false);
    }

    return nCode;
}


int CHitParser::x_CheckParameters() const
{
    switch (m_Method)
    {
        case eMethod_MaxScore:
        case eMethod_MaxScore_GroupSelect:
        break;
        default: return 1; // unsupported
    };
    switch (m_Strand)
    {
        case 0: case 1: case 3: break;
            // same order and different strands are incompatible
        case 2: if(m_SameOrder) return 3;
                        else break;
        default: return 2; // unsupported
    };
    return 0;
}


// counts the number of collisions
// pvh0== 0 => count at m_Out
int CHitParser::GetCollisionCount (int& nq, int& ns, const vector<CHit>& vh)
{
    nq = ns = 0;
    vector<SNode> vi_q, vi_s;
    vector<CHit>::const_iterator phi0 = vh.begin(),
      phi1 = vh.end(), phi = phi0;
    for(phi = phi0; phi != phi1; ++phi)
    {
        SNode Nodes [] = {
            SNode(phi->m_ai[0],true,phi), SNode(phi->m_ai[1],false,phi),
            SNode(phi->m_ai[2],true,phi), SNode(phi->m_ai[3],false,phi)
        };
        vi_q.push_back(Nodes[0]); vi_q.push_back(Nodes[1]);
        vi_s.push_back(Nodes[2]); vi_s.push_back(Nodes[3]);
    }
    sort(vi_q.begin(),vi_q.end());
    sort(vi_s.begin(),vi_s.end());
    int nLevel = 0;
    vector<SNode>::iterator ii;
    for(ii = vi_q.begin(); ii != vi_q.end(); ii++)
        if(ii->type) { if(++nLevel == 2) nq++; } else nLevel--;
    nLevel = 0;
    vector<SNode>::iterator jj;
    for(jj = vi_s.begin(); jj != vi_s.end(); jj++)
        if(jj->type) { if(++nLevel == 2) ns++; } else nLevel--;
    return nq + ns;
}


void CHitParser::x_TransformCoordinates(bool bDir)
{
    if(bDir)
    {
        // transform hits
        m_Origin[0] = m_ange[0];
        m_Origin[1] = m_ange[2];
        vector<CHit>::iterator ii, iend;
        for(ii = m_Out.begin(), iend = m_Out.end(); ii != iend; ii++)
        {
            for(int k=0; k<4; k++)
            {
                ii->m_an[k] -= m_Origin[k/2];
                ii->m_ai[k] -= m_Origin[k/2];
            }
        }
    }
    else
    {
        vector<CHit>::iterator ii, iend;
        vector<vector<CHit>::iterator> vhi_del;
        for(ii = m_Out.begin(), iend = m_Out.end(); ii != iend; ii++)
        {
            for(int k=0; k<4; k++)
            {
                ii->m_an[k] += m_Origin[k/2];
                ii->m_ai[k] += m_Origin[k/2];
            }
            if(ii->m_ai[1] == ii->m_ai[0] || ii->m_ai[3] == ii->m_ai[2])
                vhi_del.push_back(ii);
        }
        // delete the slack
        vector<vector<CHit>::iterator>::reverse_iterator i1, i1e;
        for(i1 = vhi_del.rbegin(), i1e = vhi_del.rend(); i1 != i1e; i1++)
            m_Out.erase(*i1);
    }
    x_CalcGlobalEnvelop();
}


void CHitParser::x_FilterByMaxDist()
{
    //  sum and compare the scores
    map<int,double> mgr2sc;
    int nSize = m_Out.size();
    for(int j = 0; j < nSize; j++)
    {
        const int nClustID = CMaxDistClIdGet()(m_Out[j]);
        mgr2sc[nClustID] += m_Out[j].m_Score;
    }
    double dmax = -1e38;
    map<int,double>::iterator im, im1;
    for(im1 = im = mgr2sc.begin(); im != mgr2sc.end(); im++)
    {
        if(im->second > dmax)
        {
            dmax = im->second;
            im1 = im;
        }
    }

    // leave only the top-score max dist group
    vector<CHit>::iterator ie = remove_if(
        m_Out.begin(), m_Out.end(), bind2nd(hit_out_of_group(),im1->first));
    m_Out.erase(ie,m_Out.end());
}


void CHitParser::x_IdentifyMaxDistGroups()
{
    DoSingleLinkageClustering ( m_Out,
        CMaxDistClustPred(m_MaxHitDistQ, m_MaxHitDistS),
        CMaxDistClIdSet(),
        CMaxDistClIdGet() );
}


void CHitParser::x_SyncGroupsByMaxDist(int& nGroupID)
{
    if(m_Out.size() == 0) return;

    // order by GroupID
    stable_sort(m_Out.begin(), m_Out.end(), hit_groupid_less());

    // renumber the groups to make sure that
    // we have same nClustID throughout the group
    vector<CHit>::iterator ibegin = m_Out.begin(), iend = m_Out.end(),
        ii = ibegin;
    int ngid = ii->m_GroupID, nmdcid = ii->m_MaxDistClustID;
    ii->m_GroupID = ++nGroupID;
    for(++ii; ii != iend; ++ii)
    {
        if(ngid == ii->m_GroupID && nmdcid != ii->m_MaxDistClustID)
        {
            ++nGroupID;
            nmdcid = ii->m_MaxDistClustID;
        }
        if(ngid != ii->m_GroupID)
        {
            ++nGroupID;
            ngid = ii->m_GroupID;
        }
        ii->m_GroupID = nGroupID;
    }
}


size_t CHitParser::GetSeqLength(const string& accession)
{
    return 0;

    /*
  using namespace objects;

  map<string, size_t>::const_iterator i1 = m_seqsizes.find(accession),
    i2 = m_seqsizes.end();
  if(i1 != i2) {
    size_t s = i1->second;
    return s;
  }
  
  CObjectManager om;
  om.RegisterDataLoader ( *new CGBDataLoader("ID", new CId1Reader, 2),
			  CObjectManager::eDefault );
  CScope scope(om);
  scope.AddDefaults();
    
  CSeq_id seq_id (accession);
  
  CBioseq_Handle bioseq_handle = scope.GetBioseqHandle(seq_id);
  
  if ( bioseq_handle ) {
    const CBioseq& bioseq = bioseq_handle.GetBioseq();
    const CSeq_inst& inst = bioseq.GetInst();
    if(inst.IsSetLength()) {
      size_t s = inst.GetLength();
      m_seqsizes[accession] = s;
      return s;
    }
  }
  return 0;

    */
}


bool CHitGroup::CheckSameOrder()
{
    if(m_vHits.size() == 0) return true;
    vector<int>::const_iterator ivh0 = m_vHits.begin(),
        ivh1 = m_vHits.end(), ii, jj;

    hit_not_same_order hnso;
    for(ii = ivh0; ii < ivh1; ++ii) {
        for(jj = ivh0; jj < ii; ++jj) {
            if(hnso( (*m_pHits)[*ii], (*m_pHits)[*jj] ))
                return false;
        }
    }

    return true;
}


void CHitParser::x_Combine(double dProximity)
{
    if(dProximity < 0 || m_Out.size() == 0) {
        return;
    }

    x_CalcGlobalEnvelop();

    size_t total_hits = m_Out.size();
    vector<CHit*> vh (total_hits);
    CHit* phit = &(m_Out[0]);
    for(size_t i = 0; i < total_hits; ++i) {
        vh[i] = phit++;
    }
    typedef vector<CHit*>::iterator TVHitPtrI;

    // separate strands
    sort(vh.begin(), vh.end(),  CHit::PPrecedeStrand_ptr);
    TVHitPtrI strand_minus_start = 
        find_if(vh.begin(), vh.end(), bind2nd(hit_ptr_strand_is(), true));

    typedef pair<TVHitPtrI, TVHitPtrI> THitRange;
    THitRange strands [2];
    strands[0].first = vh.begin();
    strands[0].second = strands[1].first = strand_minus_start;
    strands[1].second = strands[0].first + total_hits;
 
    for(size_t strand = 0; strand < 2; ++strand) {
        TVHitPtrI start  = strands[strand].first;
        TVHitPtrI finish = strands[strand].second;
        
        size_t dim = finish - start;
        if(dim < 2) continue;
        size_t count;
        do {
            count = 0;
            stable_sort(start, finish, CHit::PGreaterScore_ptr);

            for(TVHitPtrI i1 = start; i1 < finish - 1; ++i1) {
                if(*i1 == 0) continue;
                for(TVHitPtrI j1 = i1 + 1; j1 < finish; ++j1) {
                    if(*j1 == 0) continue;
                    double d = x_CalculateProximity(**i1, **j1);
                    if(d <= dProximity) {
                        CHit hc (**i1, **j1);
                        **i1 = hc;
                        *j1 = 0;
                        ++count;
                    }
                }
            }
        }
        while(count);
    }

    // flush deletions, compress pointers vector, fill original hit vector
    TVHitPtrI iv = remove(vh.begin(), vh.end(), (CHit*) 0);
    vh.erase(iv, vh.end());
    TVHitPtrI ib = vh.begin(), ie = vh.end();
    sort(ib, ie, CHit::PGreaterScore_ptr);
    vector<CHit> vout (vh.size());
    for(iv = ib; iv != ie; ++iv) {
        vout[iv - ib] = **iv;
    }
    m_Out = vout;

    return;
}

END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.7  2004/05/24 16:13:57  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.6  2004/05/18 21:43:40  kapustin
* Code cleanup
*
* Revision 1.5  2004/05/03 21:53:57  kapustin
* Eliminate OM-dependant code
*
* Revision 1.4  2004/04/26 16:52:44  ucko
* Add an explicit "typename" annotation required by GCC 3.4, and adjust
* the code to take advantage of the ITERATE macro.
*
* Revision 1.3  2004/04/23 14:37:44  kapustin
* *** empty log message ***
*
* 
* ===========================================================================
*/
